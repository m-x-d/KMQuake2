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
// snd_main.c -- main control for any streaming sound output device

#include "client.h"
#include "snd_loc.h"
#include "snd_ogg.h"

#pragma region ======================= Internal sound data & structures

static int s_registration_sequence;

channel_t channels[MAX_CHANNELS];
int s_numchannels; //mxd

qboolean sound_started = false;

sound_t sound;

vec3_t listener_origin;
vec3_t listener_right;

static qboolean s_registering;

int paintedtime; // Sample PAIRS

// During registration it is possible to have more sounds than could actually be referenced during gameplay,
// because we don't want to free anything until we are sure we won't need it.
#define MAX_SFX		(MAX_SOUNDS * 2)
static sfx_t known_sfx[MAX_SFX];
static int num_sfx;

#define MAX_PLAYSOUNDS	128
static playsound_t s_playsounds[MAX_PLAYSOUNDS];
static playsound_t s_freeplays;
playsound_t s_pendingplays;

cvar_t *s_volume;
cvar_t *s_testsound;
cvar_t *s_loadas8bit;
cvar_t *s_khz;
cvar_t *s_show;
cvar_t *s_mixahead;
cvar_t *s_musicvolume;
cvar_t *s_sndbits; //mxd
cvar_t *s_sndchannels; //mxd

int s_rawend;
portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];

#pragma endregion

#pragma region ======================= Console commands 

static void S_Play_f(void)
{
	if (Cmd_Argc() == 1) //mxd. Usage
	{
		Com_Printf("Usage: play <filename1> [filename2] [filename3] ...\n");
		return;
	}

	for (int i = 1; i < Cmd_Argc(); i++)
	{
		char name[MAX_OSPATH];
		if (!strrchr(Cmd_Argv(i), '.'))
		{
			Q_strncpyz(name, Cmd_Argv(i), sizeof(name));
			Q_strncatz(name, ".wav", sizeof(name));
		}
		else
		{
			Q_strncpyz(name, Cmd_Argv(i), sizeof(name));
		}

		sfx_t *sfx = S_RegisterSound(name);
		S_StartSound(NULL, cl.playernum + 1, 0, sfx, 1.0f, 1.0f, 0.0f);
	}
}

//mxd
typedef struct
{
	char *name;
	size_t size;
	int bits;
	qboolean looping;
	qboolean placeholder;
	qboolean notloaded;
} soundinfo_t;

//mxd
static int S_SortSoundinfos(const soundinfo_t *first, const soundinfo_t *second)
{
	return Q_stricmp(first->name, second->name);
}

static void S_SoundList_f()
{
	//mxd. Avoid memset(NULL);
	if (num_sfx == 0)
	{
		Com_Printf(S_COLOR_GREEN"No sounds loaded.\n");
		return;
	}

	//mxd. Collect sound infos first...
	const uint infossize = sizeof(soundinfo_t) * num_sfx;
	soundinfo_t *infos = malloc(infossize);
	memset(infos, 0, infossize);
	int numinfos = 0;
	int bytestotal = 0;

	sfx_t *sfx = known_sfx;
	for (int i = 0; i < num_sfx; i++, sfx++)
	{
		if (!sfx->registration_sequence)
			continue;

		infos[numinfos].name = sfx->name;

		sfxcache_t *sc = sfx->cache;
		if (sc)
		{
			const int size = sc->length * sc->width * (sc->stereo + 1);
			bytestotal += size;

			infos[numinfos].looping = (sc->loopstart >= 0);
			infos[numinfos].bits = sc->width * 8;
			infos[numinfos].size = size;
		}
		else
		{
			if (sfx->name[0] == '*')
				infos[numinfos].placeholder = true;
			else
				infos[numinfos].notloaded = true;
		}

		numinfos++;
	}

	if (numinfos == 0)
	{
		Com_Printf(S_COLOR_GREEN"No sounds loaded.\n");
		free(infos);
		return;
	}

	//mxd. Sort infos by name
	qsort(infos, numinfos, sizeof(soundinfo_t), (int(*)(const void *, const void *))S_SortSoundinfos);

	// Print results
	Com_Printf(S_COLOR_GREEN"Loaded sounds:\n");

	for (int i = 0; i < numinfos; i++)
	{
		if (infos[i].placeholder)
		{
			Com_Printf(S_COLOR_YELLOW"---- : ------- : placeholder : %s\n", infos[i].name);
		}
		else if (infos[i].notloaded)
		{
			Com_Printf(S_COLOR_YELLOW"---- : ------- :  not loaded : %s\n", infos[i].name);
		}
		else
		{
			const char *looping = infos[i].looping ? "LOOP" : "----";
			Com_Printf("%s : %2i bits : %7.2f Kb. : %s\n", looping, infos[i].bits, infos[i].size / 1024.0f, infos[i].name); // Print size in Kb.
		}
	}

	Com_Printf(S_COLOR_GREEN"Total: %i sounds (%0.2f Mb.).\n", numinfos, bytestotal / (1024.0f * 1024.0f)); // Print size in Mb.

	//mxd. Free memory
	free(infos);
}

static void S_SoundInfo_f(void)
{
	if (!sound_started)
	{
		Com_Printf(S_COLOR_GREEN"Sound system not started\n");
		return;
	}

	Com_Printf("%5d stereo\n", sound.channels - 1);
	Com_Printf("%5d samples\n", sound.samples);
	Com_Printf("%5d samplepos\n", sound.samplepos);
	Com_Printf("%5d samplebits\n", sound.samplebits);
	Com_Printf("%5d submission_chunk\n", sound.submission_chunk);
	Com_Printf("%5d speed\n", sound.speed);
	Com_Printf("0x%x sound buffer\n", sound.buffer);
}

#pragma endregion 

#pragma region ======================= Init / shutdown

void S_Init(void)
{
	Com_Printf("\n------- Sound Initialization -------\n");

	cvar_t *cv = Cvar_Get("s_initsound", "1", 0);
	if (!cv->integer)
	{
		Com_Printf("Not initializing.\n");
		Com_Printf("------------------------------------\n\n");
		return;
	}

	s_volume = Cvar_Get("s_volume", "1.0", CVAR_ARCHIVE);
	s_khz = Cvar_Get("s_khz", "44", CVAR_ARCHIVE); //mxd. Was 22
	s_loadas8bit = Cvar_Get("s_loadas8bit", "0", CVAR_ARCHIVE);
	s_mixahead = Cvar_Get("s_mixahead", "0.2", CVAR_ARCHIVE);
	s_show = Cvar_Get("s_show", "0", 0);
	s_testsound = Cvar_Get("s_testsound", "0", 0);
	s_musicvolume = Cvar_Get("s_musicvolume", "1.0", CVAR_ARCHIVE); // Q2E
	s_sndbits = Cvar_Get("s_sndbits", "16", CVAR_ARCHIVE); //mxd
	s_sndchannels = Cvar_Get("s_sndchannels", "2", CVAR_ARCHIVE); //mxd

	Cmd_AddCommand("play", S_Play_f);
	Cmd_AddCommand("stopsound", S_StopAllSounds);
	Cmd_AddCommand("soundlist", S_SoundList_f);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);
	Cmd_AddCommand("ogg_restart", S_OGG_Restart);

	if (!SDL_BackendInit())
		return;

	sound_started = true;
	num_sfx = 0;
	paintedtime = 0;

	S_StopAllSounds();
	S_OGG_Init();

	Com_Printf("------------------------------------\n\n");
}

// Shutdown the sound system and it's backend
void S_Shutdown(void)
{
	if (!sound_started)
		return;

	S_StopAllSounds(); //mxd
	S_OGG_Shutdown();
	SDL_BackendShutdown();

	sound_started = false;
	s_numchannels = 0; //mxd

	Cmd_RemoveCommand("play");
	Cmd_RemoveCommand("stopsound");
	Cmd_RemoveCommand("soundlist");
	Cmd_RemoveCommand("soundinfo");
	Cmd_RemoveCommand("ogg_restart");

	// Free all sounds
	sfx_t *sfx = known_sfx;
	for (int i = 0; i < num_sfx; i++, sfx++)
	{
		if (!sfx->name[0])
			continue;

		if (sfx->cache)
			Z_Free(sfx->cache);

		if (sfx->truename) //mxd
			Z_Free(sfx->truename);
	}

	memset(known_sfx, 0, sizeof(known_sfx)); //mxd
	num_sfx = 0;
}

#pragma endregion

#pragma region ======================= Sound loading

sfxcache_t *S_LoadSound(sfx_t *s)
{
	char namebuffer[MAX_QPATH];
	byte *data;
	char *name;

	if (s->name[0] == '*')
		return NULL;

	// See if still in memory
	sfxcache_t *sc = s->cache;
	if (sc)
		return sc;

	// Load it in
	if (s->truename)
		name = s->truename;
	else
		name = s->name;

	if (name[0] == '#')
		Q_strncpyz(namebuffer, &name[1], sizeof(namebuffer));
	else
		Com_sprintf(namebuffer, sizeof(namebuffer), "sound/%s", name);

	const int size = FS_LoadFile(namebuffer, (void **)&data);

	if (!data)
	{
		Com_Printf(S_COLOR_YELLOW"%s: couldn't load sound '%s'.\n", __func__, namebuffer);
		return NULL;
	}

	wavinfo_t info = S_GetWavInfo(s->name, data, size);
	if (info.channels < 1 || info.channels > 2)	//CDawg changed
	{
		Com_Printf(S_COLOR_YELLOW"%s: sound '%s' has unsupported number of channels (%i, supported: 1 or 2).\n", __func__, namebuffer, info.channels);
		FS_FreeFile(data);

		return NULL;
	}

	//mxd. Force loopstart if it's a music file
	if (!strncmp(namebuffer, "music/", 6) && info.loopstart == -1)
		info.loopstart = 0;

	if (!SDL_Cache(s, &info, data + info.dataofs))
	{
		//mxd. SDL_Cache is supposed to log the error.
		FS_FreeFile(data);
		return NULL;
	}

	FS_FreeFile(data);
	return sc;
}

sfx_t *S_FindName(char *name, qboolean create)
{
	if (!name)
	{
		Com_Error(ERR_FATAL, "S_FindName: NULL\n");
		return NULL; //mxd. Silence PVS warning.
	}

	if (!name[0])
	{
		Com_Error(ERR_FATAL, "S_FindName: empty name\n");
		return NULL; //mxd. Silence PVS warning.
	}

	if (strlen(name) >= MAX_QPATH)
	{
		Com_Error(ERR_FATAL, "Sound name too long: '%s' (%i / %i chars)", name, strlen(name), MAX_QPATH - 1);
		return NULL; //mxd. Silence PVS warning.
	}

	// See if already loaded
	for (int i = 0; i < num_sfx; i++)
		if (!strcmp(known_sfx[i].name, name))
			return &known_sfx[i];

	if (!create)
		return NULL;

	// Find a free sfx
	int sfxindex;
	for (sfxindex = 0; sfxindex < num_sfx; sfxindex++)
		if (!known_sfx[sfxindex].name[0])
			break;

	if (sfxindex == num_sfx)
	{
		if (num_sfx == MAX_SFX)
		{
			Com_Error(ERR_FATAL, "S_FindName: out of sfx_t");
			return NULL; //mxd
		}

		num_sfx++;
	}
	
	sfx_t *sfx = &known_sfx[sfxindex];
	memset(sfx, 0, sizeof(*sfx));
	Q_strncpyz(sfx->name, name, sizeof(sfx->name));
	sfx->registration_sequence = s_registration_sequence;
	
	return sfx;
}

static sfx_t *S_AliasName(char *aliasname, char *truename)
{
	int i;

	char *s = Z_Malloc(MAX_QPATH);
	Q_strncpyz(s, truename, MAX_QPATH);

	// Find a free sfx
	for (i = 0; i < num_sfx; i++)
		if (!known_sfx[i].name[0])
			break;

	if (i == num_sfx)
	{
		if (num_sfx == MAX_SFX)
			Com_Error(ERR_FATAL, "S_FindName: out of sfx_t");

		num_sfx++;
	}
	
	sfx_t *sfx = &known_sfx[i];
	memset(sfx, 0, sizeof(*sfx));
	Q_strncpyz(sfx->name, aliasname, sizeof(sfx->name));
	sfx->registration_sequence = s_registration_sequence;
	sfx->truename = s;

	return sfx;
}

void S_BeginRegistration(void)
{
	s_registration_sequence++;
	s_registering = true;
}

sfx_t *S_RegisterSound(char *name)
{
	if (!sound_started)
		return NULL;

	sfx_t *sfx = S_FindName(name, true);
	sfx->registration_sequence = s_registration_sequence;

	if (!s_registering)
		S_LoadSound(sfx);

	return sfx;
}

void S_EndRegistration(void)
{
	// Free any sounds not from this registration sequence
	sfx_t *sfx = known_sfx;
	for (int i = 0; i < num_sfx; i++, sfx++)
	{
		if (!sfx->name[0])
			continue;

		if (sfx->registration_sequence != s_registration_sequence)
		{
			// Don't need this sound
			if (sfx->cache)	// It is possible to have a leftover from a server that didn't finish loading
				Z_Free(sfx->cache);

			if (sfx->truename) //mxd
				Z_Free(sfx->truename);

			memset(sfx, 0, sizeof(*sfx));
		}
	}

	// Load everything in
	sfx = known_sfx;
	for (int i = 0; i < num_sfx; i++, sfx++)
		if (sfx->name[0])
			S_LoadSound(sfx);

	s_registering = false;
}

#pragma endregion

channel_t *S_PickChannel(int entnum, int entchannel)
{
	if (entchannel < 0)
		Com_Error(ERR_DROP, "S_PickChannel: entchannel < 0");

	// Check for replacement sound, or find the best one to replace
	int first_to_die = -1;
	int life_left = 0x7fffffff;

	for (int ch_idx = 0; ch_idx < MAX_CHANNELS; ch_idx++)
	{
		// Don't let game sounds override streaming sounds
		if (channels[ch_idx].streaming) // Q2E
			continue;

		if (entchannel != 0 && channels[ch_idx].entnum == entnum && channels[ch_idx].entchannel == entchannel) // Channel 0 never overrides
		{
			// Always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// Don't let monster sounds override player sounds
		if (channels[ch_idx].entnum == cl.playernum + 1 && entnum != cl.playernum + 1 && channels[ch_idx].sfx)
			continue;

		if (channels[ch_idx].end - paintedtime < life_left)
		{
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
	}

	if (first_to_die == -1)
		return NULL;

	channel_t *ch = &channels[first_to_die];
	memset(ch, 0, sizeof(*ch));

	return ch;
}

#pragma region ======================= Playsounds handling

static playsound_t *S_AllocPlaysound(void)
{
	playsound_t *ps = s_freeplays.next;
	if (ps == &s_freeplays)
		return NULL; // No free playsounds, this results in stuttering an cracking

	// Unlink from freelist
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;
	
	return ps;
}

static void S_FreePlaysound(playsound_t *ps)
{
	// Unlink from channel
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	// Add to free list
	ps->next = s_freeplays.next;
	s_freeplays.next->prev = ps;
	ps->prev = &s_freeplays;
	s_freeplays.next = ps;
}

// Take the next playsound and begin it on the channel.
// This is never called directly by S_Play*, but only by the update loop.
void S_IssuePlaysound(playsound_t *ps)
{
	if (!ps) //mxd
		return;
	
	if (s_show->integer)
		Com_Printf("Issue %i\n", ps->begin);

	// Pick a channel to play on
	channel_t *ch = S_PickChannel(ps->entnum, ps->entchannel);
	if (!ch || !ps->sfx->name[0]) //mxd. Added ps->sfx check...
	{
		S_FreePlaysound(ps);
		return;
	}

	sfxcache_t *sc = S_LoadSound(ps->sfx);
	if (!sc) //mxd
	{
		Com_Printf(S_COLOR_YELLOW"%s: couldn't load '%s'\n", __func__, ps->sfx->name);
		S_FreePlaysound(ps);
		return;
	}

	// Spatialize
	if (ps->attenuation == ATTN_STATIC)
		ch->dist_mult = ps->attenuation * 0.001f;
	else
		ch->dist_mult = ps->attenuation * 0.0005f;

	ch->master_vol = ps->volume;
	ch->entnum = ps->entnum;
	ch->entchannel = ps->entchannel;
	ch->sfx = ps->sfx;
	VectorCopy(ps->origin, ch->origin);
	ch->fixed_origin = ps->fixed_origin;

	if (sound_started)
	{
		ch->master_vol = (int)ps->volume;
		SDL_Spatialize(ch);
	}

	ch->pos = 0;
	ch->end = paintedtime + sc->length;

	// Free the playsound
	S_FreePlaysound(ps);
}

static struct sfx_s *S_RegisterSexedSound(entity_state_t *ent, char *base)
{
	// Determine what model the client is using
	char model[MAX_QPATH] = { 0 };

	// Knightmare- BIG UGLY HACK for old connected to server using old protocol
	// Changed config strings require different parsing
	const int cs_playerskins = (LegacyProtocol() ? OLD_CS_PLAYERSKINS : CS_PLAYERSKINS); //mxd
	const int n = cs_playerskins + ent->number - 1;

	if (cl.configstrings[n][0])
	{
		char *p = strchr(cl.configstrings[n], '\\');
		if (p)
		{
			p += 1;
			Q_strncpyz(model, p, sizeof(model));
			p = strchr(model, '/');
			if (p)
				*p = 0;
		}
	}

	// If we can't figure it out, they're male
	if (!model[0])
		Q_strncpyz(model, "male", sizeof(model));

	// See if we already know of the model specific sound
	char sexedFilename[MAX_QPATH];
	Com_sprintf(sexedFilename, sizeof(sexedFilename), "#players/%s/%s", model, base + 1);
	struct sfx_s *sfx = S_FindName(sexedFilename, false);

	if (!sfx)
	{
		// No, so see if it exists
		fileHandle_t f;
		FS_FOpenFile(&sexedFilename[1], &f, FS_READ);
		if (f)
		{
			// Yes, close the file and register it
			FS_FCloseFile(f);
			sfx = S_RegisterSound(sexedFilename);
		}
		else
		{
			// No, revert to the male sound in the pak0.pak
			char maleFilename[MAX_QPATH];
			Com_sprintf(maleFilename, sizeof(maleFilename), "player/%s/%s", "male", base + 1);
			sfx = S_AliasName(sexedFilename, maleFilename);
		}
	}

	return sfx;
}

#pragma endregion

#pragma region ======================= Sound start / stop / clear

// Validates the parms and ques the sound up.
// If pos is NULL, the sound will be dynamically sourced from the entity.
// Entchannel 0 will never override a playing sound.
void S_StartSound(const vec3_t origin, int entnum, int entchannel, sfx_t *sfx, float fvol, float attenuation, float timeofs)
{
	static int s_beginofs = 0; //mxd. Made local

	if (!sound_started || !sfx)
		return;

	if (sfx->name[0] == '*')
		sfx = S_RegisterSexedSound(&cl_entities[entnum].current, sfx->name);

	// Make sure the sound is loaded
	sfxcache_t *sc = S_LoadSound(sfx);
	if (!sc)
		return;

	// Make the playsound_t
	playsound_t *ps = S_AllocPlaysound();
	if (!ps)
		return;

	if (origin)
	{
		VectorCopy(origin, ps->origin);
		ps->fixed_origin = true;
	}
	else
	{
		ps->fixed_origin = false;
	}

	ps->entnum = entnum;
	ps->entchannel = entchannel;
	ps->attenuation = attenuation;
	ps->volume = fvol * 255;
	ps->sfx = sfx;

	// Drift s_beginofs
	int start = cl.frame.servertime * 0.001f * sound.speed + s_beginofs;
	if (start < paintedtime)
	{
		start = paintedtime;
		s_beginofs = start - (cl.frame.servertime * 0.001f * sound.speed);
	}
	else if (start > paintedtime + 0.3f * sound.speed)
	{
		start = paintedtime + 0.1f * sound.speed;
		s_beginofs = start - (cl.frame.servertime * 0.001f * sound.speed);
	}
	else
	{
		s_beginofs -= 10;
	}

	if (!timeofs)
		ps->begin = paintedtime;
	else
		ps->begin = start + timeofs * sound.speed;

	// Sort into the pending sound list
	playsound_t *sort = s_pendingplays.next;
	while (sort != &s_pendingplays && sort->begin < ps->begin)
		sort = sort->next;

	ps->next = sort;
	ps->prev = sort->prev;

	ps->next->prev = ps;
	ps->prev->next = ps;
}

void S_StartLocalSound(char *sound)
{
	if (!sound_started)
		return;
		
	sfx_t *sfx = S_RegisterSound(sound);
	if (!sfx)
	{
		Com_Printf(S_COLOR_YELLOW"%s: can't cache '%s'\n", __func__, sound);
		return;
	}

	S_StartSound(NULL, cl.playernum + 1, 0, sfx, 1, 1, 0);
}

void S_StopAllSounds(void)
{
	if (!sound_started)
		return;

	// Clear all the playsounds
	memset(s_playsounds, 0, sizeof(s_playsounds));
	s_freeplays.next = s_freeplays.prev = &s_freeplays;
	s_pendingplays.next = s_pendingplays.prev = &s_pendingplays;

	for (int i = 0; i < MAX_PLAYSOUNDS; i++)
	{
		s_playsounds[i].prev = &s_freeplays;
		s_playsounds[i].next = s_freeplays.next;
		s_playsounds[i].prev->next = &s_playsounds[i];
		s_playsounds[i].next->prev = &s_playsounds[i];
	}

	// Clear all the channels
	memset(channels, 0, sizeof(channels));

	// Stop background track
	S_StopBackgroundTrack(); // Q2E

	SDL_ClearBuffer(); //mxd
}

#pragma endregion

#pragma region ======================= Cinematic streaming

// Cinematic streaming and voice over network.
// This could be used for chat over network, but that would be terrible slow.
void S_RawSamples(const int samples, const int rate, const int width, const int channels, const byte *data, const float volume)
{
	if (sound_started)
	{
		s_rawend = max(paintedtime, s_rawend);
		SDL_RawSamples(samples, rate, width, channels, data, volume);
	}
}

#pragma endregion

#pragma region ======================= Per-frame update

// Called once per frame from CL_Frame() / CL_Frame_Async()
void S_Update(const vec3_t origin, const vec3_t forward, const vec3_t right, const vec3_t up)
{
	if (!sound_started)
		return;

	// If the loading plaque is up, clear everything out to make sure we aren't looping a dirty dma buffer while loading
	if (cls.disable_screen)
	{
		SDL_ClearBuffer();
		return;
	}

	VectorCopy(origin, listener_origin);
	VectorCopy(right, listener_right);

	// Mix some sound
	SDL_Update();
}

#pragma endregion