/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// snd_stream.c -- Ogg Vorbis stuff

#include "client.h"
#include "snd_loc.h"
#include "snd_ogg.h"

#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#include "../include/stb_vorbis/stb_vorbis.h"

// Added from Q2E
typedef struct
{
	char introName[MAX_QPATH];
	char loopName[MAX_QPATH];
	char ambientName[MAX_QPATH];
	qboolean looping;
	qboolean ambient_looping;
	stb_vorbis *ogg_file; //mxd
	byte *ogg_data; //mxd. Pointer to loaded data, so it can be freed
	int startframe; //mxd. For resuming tracks after game load
} bgTrack_t;

typedef enum
{
	PLAY,
	PAUSE,
	STOP
} ogg_status_t;

#define MAX_OGGLIST	512

static bgTrack_t s_bgTrack;
static channel_t *s_streamingChannel;

static qboolean ogg_started = false; // Initialization flag
static ogg_status_t ogg_status; // Status indicator

static char **ogg_filelist; // List of Ogg Vorbis files
static int ogg_numfiles; // Number of Ogg Vorbis files
static int ogg_loopcounter;

//mxd. Fade-in effect after setting music track frame
static float ogg_fadeinvolume = 1.0f;

static cvar_t *ogg_loopcount;
static cvar_t *ogg_ambient_track;

#pragma region ======================= Ogg vorbis playback

static qboolean S_OpenBackgroundTrack(const char *name, bgTrack_t *track)
{
	byte *data = NULL;

	fileHandle_t fh;
	int datalength = FS_FOpenFile(name, &fh, FS_READ);

	if (datalength > 0)
	{
		data = malloc(datalength);
		FS_Read(data, datalength, fh);
		FS_FCloseFile(fh);
	}
	else
	{
		//mxd. Music tracks in Q2 GOG are outside of Q2 filesystem, so try loading those directly...
		FILE *f = fopen(name, "rb");
		if (f)
		{
			fseek(f, 0, SEEK_END);
			datalength = ftell(f);
			fseek(f, 0, SEEK_SET);

			if (datalength > 0)
			{
				data = malloc(datalength);
				fread(data, 1, datalength, f);
			}

			fclose(f);
		}
	}

	if (datalength < 1) //mxd. FS_FOpenFile can return either 0 (zero-length file) or -1 (no such file).
	{
		Com_Printf(S_COLOR_YELLOW"%s: couldn't find '%s'\n", __func__, name);
		return false;
	}

	int stb_err = 0;
	track->ogg_file = stb_vorbis_open_memory(data, datalength, &stb_err, NULL);
	track->ogg_data = data;

	if (stb_err != 0)
	{
		Com_Printf(S_COLOR_YELLOW"%s: '%s' is not a valid Ogg Vorbis file (error %i).\n", __func__, name, stb_err);
		return false;
	}

	if (track->ogg_file->channels != 1 && track->ogg_file->channels != 2)
	{
		Com_Printf(S_COLOR_YELLOW"S_OpenBackgroundTrack: only mono and stereo OGG files supported (%s)\n", name);
		return false;
	}

	//mxd. Seek to target frame? Info: doing this during playback results in a bit of sound crackle right after calling stb_vorbis_seek...
	if (track->startframe > 0)
	{
		stb_vorbis_seek(track->ogg_file, (uint)track->startframe);
		ogg_fadeinvolume = 0.0f;
		track->startframe = 0;
	}

	return true;
}

static void S_CloseBackgroundTrack(bgTrack_t *track)
{
	if (track->ogg_file)
	{
		stb_vorbis_close(track->ogg_file);
		track->ogg_file = NULL;

		free(track->ogg_data);
		track->ogg_data = NULL;

		track->startframe = 0; //mxd
	}
}

static void S_StreamBackgroundTrack()
{
	if (!s_bgTrack.ogg_file || !s_musicvolume->value || !s_streamingChannel)
		return;

	//mxd. Get music volume, add fade-in effect
	const float volume = s_musicvolume->value * ogg_fadeinvolume;
	if (ogg_fadeinvolume < 1.0f)
		ogg_fadeinvolume += 0.02f;

	while (paintedtime + MAX_RAW_SAMPLES - 2048 > s_rawend)
	{
		short samples[4096] = { 0 };
		struct stb_vorbis *file = s_bgTrack.ogg_file;
		const int read_samples = stb_vorbis_get_samples_short_interleaved(file, file->channels, samples, sizeof(samples) / file->channels);

		if (read_samples > 0)
		{
			S_RawSamples(read_samples, file->sample_rate, file->channels, file->channels, (byte *)samples, volume);
		}
		else
		{
			// End of file
			if (!s_bgTrack.looping)
			{
				// Close the intro track
				S_CloseBackgroundTrack(&s_bgTrack);

				// Open the loop track
				if (!S_OpenBackgroundTrack(s_bgTrack.loopName, &s_bgTrack))
				{
					Com_Printf(S_COLOR_YELLOW"%s: failed to switch to loop track '%s'.\n", __func__, s_bgTrack.loopName); //mxd
					S_StopBackgroundTrack();

					return;
				}

				s_bgTrack.looping = true;
			}
			// Check if it's time to switch to the ambient track //mxd. Also check that ambientName contains data
			else if (s_bgTrack.ambientName[0] && ++ogg_loopcounter >= ogg_loopcount->integer && (!cl.configstrings[CS_MAXCLIENTS][0] || !strcmp(cl.configstrings[CS_MAXCLIENTS], "1")))
			{
				// Close the loop track
				S_CloseBackgroundTrack(&s_bgTrack);

				if (!S_OpenBackgroundTrack(s_bgTrack.ambientName, &s_bgTrack) && !S_OpenBackgroundTrack(s_bgTrack.loopName, &s_bgTrack))
				{
					Com_Printf(S_COLOR_YELLOW"%s: failed to switch to amblient track '%s' or loop track '%s'.\n", __func__, s_bgTrack.ambientName, s_bgTrack.loopName); //mxd
					S_StopBackgroundTrack();

					return;
				}

				s_bgTrack.ambient_looping = true;
			}
			else
			{
				// Restart the track
				stb_vorbis_seek_start(file);
			}
		}
	}
}

// Streams background track
void S_UpdateBackgroundTrack()
{
	// Stop music if paused
	if (ogg_status == PLAY)
		S_StreamBackgroundTrack();
}

void S_StartBackgroundTrack(const char *introTrack, const char *loopTrack, int startframe) //mxd. +frame
{
	if (!ogg_started) // Was sound_started
		return;

	// Stop any playing tracks
	S_StopBackgroundTrack();

	// Start it up
	Q_strncpyz(s_bgTrack.introName, introTrack, sizeof(s_bgTrack.introName));
	Q_strncpyz(s_bgTrack.loopName, loopTrack, sizeof(s_bgTrack.loopName));
	s_bgTrack.startframe = startframe; //mxd

	//mxd. No, we don't want to play "music/.ogg"
	if (ogg_ambient_track->string[0])
		Q_strncpyz(s_bgTrack.ambientName, va("music/%s.ogg", ogg_ambient_track->string), sizeof(s_bgTrack.ambientName));
	else
		s_bgTrack.ambientName[0] = '\0';

	// Set a loop counter so that this track will change to the ambient track later
	ogg_loopcounter = 0;

	//mxd. Don't try to switch to loop track if both are the same
	s_bgTrack.looping = !Q_strcasecmp(introTrack, loopTrack);

	S_StartStreaming();

	// Open the intro track
	if (!S_OpenBackgroundTrack(s_bgTrack.introName, &s_bgTrack))
	{
		S_StopBackgroundTrack();
		return;
	}

	ogg_status = PLAY;

	S_StreamBackgroundTrack();
}

void S_StopBackgroundTrack()
{
	if (!ogg_started) // Was sound_started
		return;

	S_StopStreaming();
	S_CloseBackgroundTrack(&s_bgTrack);

	ogg_status = STOP;

	memset(&s_bgTrack, 0, sizeof(bgTrack_t));
}

void S_StartStreaming()
{
	if (!ogg_started || s_streamingChannel) // Was sound_started || already started
		return;

	s_streamingChannel = S_PickChannel(0, 0);
	if (!s_streamingChannel)
		return;

	s_streamingChannel->streaming = true;
}

void S_StopStreaming()
{
	if (!ogg_started || !s_streamingChannel) // Was sound_started || already stopped
		return;

	s_streamingChannel->streaming = false;
	s_streamingChannel = NULL;
}

//mxd
int S_GetBackgroundTrackFrame()
{
	if (!ogg_started || s_bgTrack.ogg_file == NULL)
		return -1;

	return stb_vorbis_get_sample_offset(s_bgTrack.ogg_file);
}

#pragma endregion

#pragma region ======================= Init / shutdown / restart / list files

// Initialize the Ogg Vorbis subsystem
// Based on code by QuDos
void S_OGG_Init()
{
	static qboolean ogg_first_init = true; //mxd. Made local
	
	if (ogg_started)
		return;

	// Cvars
	ogg_loopcount = Cvar_Get("ogg_loopcount", "5", CVAR_ARCHIVE);
	ogg_ambient_track = Cvar_Get("ogg_ambient_track", "", CVAR_ARCHIVE); //mxd. Default value was "track11"

	// Console commands
	Cmd_AddCommand("ogg", S_OGG_ParseCmd);

	// Build list of files
	Com_Printf("Searching for Ogg Vorbis files...\n");
	ogg_numfiles = 0;
	S_OGG_LoadFileList();
	Com_Printf("%d Ogg Vorbis files found.\n", ogg_numfiles);

	// Initialize variables
	if (ogg_first_init)
	{
		ogg_status = STOP;
		ogg_first_init = false;
	}

	ogg_started = true;
}

// Shutdown the Ogg Vorbis subsystem
// Based on code by QuDos
void S_OGG_Shutdown()
{
	if (!ogg_started)
		return;

	S_StopBackgroundTrack();

	// Free the list of files
	FS_FreeFileList(ogg_filelist, MAX_OGGLIST); //mxd. Free unconditionally

	// Remove console commands
	Cmd_RemoveCommand("ogg");

	ogg_started = false;
}

// Reinitialize the Ogg Vorbis subsystem
// Based on code by QuDos
void S_OGG_Restart()
{
	char introname[MAX_QPATH]; //mxd
	Q_strncpyz(introname, s_bgTrack.introName, sizeof(introname));
	char loopname[MAX_QPATH];
	Q_strncpyz(loopname, s_bgTrack.loopName, sizeof(loopname));

	S_OGG_Shutdown();
	S_OGG_Init();

	if (introname[0] || loopname[0]) //mxd
		S_StartBackgroundTrack(introname, loopname, 0);
}

//mxd
static void LoadDirectoryFileList(const char *path)
{
	char findname[MAX_OSPATH];
	int numfiles = 0;
	
	// Get file list
	Com_sprintf(findname, sizeof(findname), "%s/music/*.ogg", path);
	char **list = FS_ListFiles(findname, &numfiles, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);

	// Add valid Ogg Vorbis file to the list
	for (int i = 0; i < numfiles && ogg_numfiles < MAX_OGGLIST; i++)
	{
		if (!list[i])
			continue;

		char *p = list[i];

		if (!strstr(p, ".ogg"))
			continue;

		if (!FS_ItemInList(p, ogg_numfiles, ogg_filelist)) // Check if already in list
		{
			ogg_filelist[ogg_numfiles] = malloc(strlen(p) + 1);
			sprintf(ogg_filelist[ogg_numfiles], "%s", p);
			ogg_numfiles++;
		}
	}

	if (numfiles) // Free the file list
		FS_FreeFileList(list, numfiles);
}

// Load list of Ogg Vorbis files in music/
// Based on code by QuDos
void S_OGG_LoadFileList()
{
	ogg_filelist = malloc(sizeof(char *) * MAX_OGGLIST);
	memset(ogg_filelist, 0, sizeof(char *) * MAX_OGGLIST);

	// Check search paths
	char *path = NULL;
	while ((path = FS_NextPath(path)) != NULL)
		LoadDirectoryFileList(path);

	//mxd. The GOG version of Quake2 has the music tracks in Quake2/music/TrackXX.ogg, so let's check for those as well...
	LoadDirectoryFileList(fs_basedir->string);

	// Check paks
	int numfiles = 0;
	char **list = FS_ListPak("music/", &numfiles);
	if (list)
	{
		// Add valid Ogg Vorbis file to the list
		for (int i = 0; i < numfiles && ogg_numfiles < MAX_OGGLIST; i++)
		{
			if (!list[i])
				continue;

			char *p = list[i];

			if (!strstr(p, ".ogg"))
				continue;

			if (!FS_ItemInList(p, ogg_numfiles, ogg_filelist)) // Check if already in list
			{
				ogg_filelist[ogg_numfiles] = malloc(strlen(p) + 1);
				sprintf(ogg_filelist[ogg_numfiles], "%s", p);
				ogg_numfiles++;
			}
		}
	}

	if (numfiles)
		FS_FreeFileList(list, numfiles);
}

#pragma endregion

#pragma region ======================= Console commands

// Based on code by QuDos
static void S_OGG_PlayCmd()
{
	if (Cmd_Argc() < 3)
	{
		Com_Printf("Usage: ogg play <track>\n");
		return;
	}

	char name[MAX_QPATH];
	Com_sprintf(name, sizeof(name), "music/%s.ogg", Cmd_Argv(2));
	S_StartBackgroundTrack(name, name, 0);
}

// Based on code by QuDos
static void S_OGG_StatusCmd()
{
	char *trackName;

	if (s_bgTrack.ambient_looping)
		trackName = s_bgTrack.ambientName;
	else if (s_bgTrack.looping)
		trackName = s_bgTrack.loopName;
	else
		trackName = s_bgTrack.introName;

	switch (ogg_status)
	{
		case PLAY:
		{
			const float curtime = stb_vorbis_get_sample_offset(s_bgTrack.ogg_file) / (float)s_bgTrack.ogg_file->sample_rate;
			const float totaltime = stb_vorbis_stream_length_in_samples(s_bgTrack.ogg_file) / (float)s_bgTrack.ogg_file->sample_rate;
			Com_Printf("Playing file '%s' (%0.2f / %0.2f seconds).\n", trackName, curtime, totaltime);
		} break;

		case PAUSE:
		{
			const float curtime = stb_vorbis_get_sample_offset(s_bgTrack.ogg_file) / (float)s_bgTrack.ogg_file->sample_rate;
			const float totaltime = stb_vorbis_stream_length_in_samples(s_bgTrack.ogg_file) / (float)s_bgTrack.ogg_file->sample_rate;
			Com_Printf("Paused file '%s' (%0.2f / %0.2f seconds).\n", trackName, curtime, totaltime);
		} break;

		case STOP:
			Com_Printf("Stopped.\n");
			break;
	}
}

// List Ogg Vorbis files
// Based on code by QuDos
static void S_OGG_ListCmd()
{
	if (ogg_numfiles <= 0)
	{
		Com_Printf(S_COLOR_GREEN"No Ogg Vorbis files to list.\n");
		return;
	}

	for (int i = 0; i < ogg_numfiles; i++)
		Com_Printf("%3i: %s\n", i + 1, ogg_filelist[i]);

	Com_Printf(S_COLOR_GREEN"Total: %i Ogg Vorbis files.\n", ogg_numfiles);
}

// Parses OGG commands
// Based on code by QuDos
void S_OGG_ParseCmd()
{
	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: ogg { play | pause | resume | stop | status | list }\n");
		return;
	}

	char *command = Cmd_Argv(1);

	if (Q_strcasecmp(command, "play") == 0)
	{
		S_OGG_PlayCmd();
	}
	else if (Q_strcasecmp(command, "pause") == 0)
	{
		if (ogg_status == PLAY)
			ogg_status = PAUSE;
	}
	else if (Q_strcasecmp(command, "resume") == 0)
	{
		if (ogg_status == PAUSE)
			ogg_status = PLAY;
	}
	else if (Q_strcasecmp(command, "stop") == 0)
	{
		S_StopBackgroundTrack();
	}
	else if (Q_strcasecmp(command, "status") == 0)
	{
		S_OGG_StatusCmd();
	}
	else if (Q_strcasecmp(command, "list") == 0)
	{
		S_OGG_ListCmd();
	}
	else
	{
		Com_Printf("Usage: ogg { play | pause | resume | stop | status | list }\n");
	}
}

#pragma endregion