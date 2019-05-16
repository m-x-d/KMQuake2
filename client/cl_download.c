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

// cl_download.c  -- client autodownload code
// moved from cl_main.c and cl_parse.c

#include "client.h"

extern cvar_t *allow_download;
extern cvar_t *allow_download_players;
extern cvar_t *allow_download_models;
extern cvar_t *allow_download_sounds;
extern cvar_t *allow_download_maps;
extern cvar_t *allow_download_textures_24bit; // Knightmare- whether to allow downloading 24-bit textures

int precache_check; // For autodownload of precache items
int precache_spawncount;
int precache_model_skin;
int precache_pak;	// Knightmare added

byte *precache_model; // Used for skin checking in alias models

#define PLAYER_MULT 5

// ENV_CNT is map load, ENV_CNT+1 is first env map
#define ENV_CNT (CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
#define TEXTURE_CNT (ENV_CNT + 13)

// Knightmare- old configstrings for version 34 client compatibility
#define OLD_ENV_CNT (OLD_CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
#define OLD_TEXTURE_CNT (OLD_ENV_CNT + 13)

#pragma region ======================= CL_RequestNextDownload

void CL_InitFailedDownloadList(void);

// From qcommon/cmodel.c
extern int numtexinfo;
extern mapsurface_t map_surfaces[];

void CL_RequestNextDownload(void)
{
	unsigned	map_checksum; // For detecting cheater maps
	char		fn[MAX_OSPATH];
	char		*skinname;
	int			cs_sounds, cs_playerskins, cs_images;
	int			max_models, max_sounds, max_images;
	int			env_cnt, texture_cnt;

	if (cls.state != ca_connected)
		return;

	// Clear failed download list
	if (precache_check == CS_MODELS)
		CL_InitFailedDownloadList();

	// Knightmare- hack for connected to server using old protocol
	// Changed config strings require different parsing
	if (LegacyProtocol())
	{
		cs_sounds		= OLD_CS_SOUNDS;
		cs_playerskins	= OLD_CS_PLAYERSKINS;
		cs_images		= OLD_CS_IMAGES;
		max_models		= OLD_MAX_MODELS;
		max_sounds		= OLD_MAX_SOUNDS;
		max_images		= OLD_MAX_IMAGES;
		env_cnt			= OLD_ENV_CNT;
		texture_cnt		= OLD_TEXTURE_CNT;
	}
	else
	{
		cs_sounds		= CS_SOUNDS;
		cs_playerskins	= CS_PLAYERSKINS;
		cs_images		= CS_IMAGES;
		max_models		= MAX_MODELS;
		max_sounds		= MAX_SOUNDS;
		max_images		= MAX_IMAGES;
		env_cnt			= ENV_CNT;
		texture_cnt		= TEXTURE_CNT;
	}

	// Skip to loading map if downloading disabled or on local server
	if ((Com_ServerState() || !allow_download->value) && precache_check < env_cnt)
		precache_check = env_cnt;

	// Try downloading pk3 file for current map from server, hack by Jay Dolan
	if (!LegacyProtocol() && precache_check == CS_MODELS && precache_pak == 0 )
	{
		precache_pak++;
		if (strlen(cl.configstrings[CS_PAKFILE]) && !CL_CheckOrDownloadFile(cl.configstrings[CS_PAKFILE]))
			return; // Started a download
	}

	// ZOID
	if (precache_check == CS_MODELS)
	{ 
		// Confirm map
		precache_check = CS_MODELS + 2; // 0 isn't used
		if (allow_download_maps->value && !CL_CheckOrDownloadFile(cl.configstrings[CS_MODELS + 1]))
			return; // Started a download
	}

	if (precache_check >= CS_MODELS && precache_check < CS_MODELS + max_models)
	{
		if (allow_download_models->value)
		{
			while (precache_check < CS_MODELS + max_models && cl.configstrings[precache_check][0])
			{
				if (cl.configstrings[precache_check][0] == '*' ||
					cl.configstrings[precache_check][0] == '#')
				{
					precache_check++;
					continue;
				}

				if (precache_model_skin == 0)
				{
					if (!CL_CheckOrDownloadFile(cl.configstrings[precache_check]))
					{
						precache_model_skin = 1;
						return; // Started a download
					}

					precache_model_skin = 1;
				}

#ifdef USE_CURL	// HTTP downloading from R1Q2
				// Pending downloads (models), let's wait here before we can check skins.
				if (CL_PendingHTTPDownloads())
					return;
#endif

				// Checking for skins in the model
				if (!precache_model)
				{
					FS_LoadFile(cl.configstrings[precache_check], (void **)&precache_model);
					if (!precache_model)
					{
						precache_model_skin = 0;
						precache_check++;

						continue; // Couldn't load it
					}

					//mxd. Rewritten for clarity
					qboolean skipmodel = true;
					const int modelid = *(unsigned *)precache_model;
					if (modelid == IDALIASHEADER) // md2 model?
					{
						dmdl_t *md2header = (dmdl_t *)precache_model;
						skipmodel = (md2header->version != ALIAS_VERSION);
					}
					else if(modelid == IDMD3HEADER) // md3 model?
					{
						dmd3_t *md3header = (dmd3_t *)precache_model;
						skipmodel = (md3header->version != MD3_ALIAS_VERSION);
					}
					else if(modelid == IDSPRITEHEADER) // sprite?
					{
						dsprite_t *spriteheader = (dsprite_t *)precache_model;
						skipmodel = (spriteheader->version != SPRITE_VERSION);
					}

					if(skipmodel)
					{
						// Not a recognized model or sprite
						FS_FreeFile(precache_model);
						precache_model = 0;
						precache_model_skin = 0;
						precache_check++;

						continue; // Couldn't load it
					}
				}

				if (*(unsigned *)precache_model == IDALIASHEADER) // md2
				{
					dmdl_t *md2header = (dmdl_t *)precache_model;
					while (precache_model_skin - 1 < md2header->num_skins)
					{
						skinname = (char *)precache_model + md2header->ofs_skins + (precache_model_skin - 1) * MAX_SKINNAME;

						// r1ch: spam warning for models that are broken
						if (strchr(skinname, '\\'))
							Com_Printf("Warning, model %s with incorrectly linked skin: %s\n", cl.configstrings[precache_check], skinname);
						else if (strlen(skinname) > MAX_SKINNAME - 1)
							Com_Error(ERR_DROP, "Model %s has too long a skin path: %s", cl.configstrings[precache_check], skinname);

						if (!CL_CheckOrDownloadFile(skinname))
						{
							precache_model_skin++;
							return; // Started a download
						}

						precache_model_skin++;
					}
				}
				else if (*(unsigned *)precache_model == IDMD3HEADER) // md3
				{
					dmd3_t *md3header = (dmd3_t *)precache_model;
					while (precache_model_skin - 1 < md3header->num_skins)
					{
						dmd3mesh_t *md3mesh = (dmd3mesh_t *)((byte *)md3header + md3header->ofs_meshes);
						for (int i = 0; i < md3header->num_meshes; i++)
						{
							if (precache_model_skin - 1 >= md3header->num_skins)
								break;

							skinname = (char *)precache_model + md3mesh->ofs_skins + (precache_model_skin - 1) * MD3_MAX_PATH;

							// r1ch: spam warning for models that are broken
							if (strchr(skinname, '\\'))
								Com_Printf("Warning, model %s with incorrectly linked skin: %s\n", cl.configstrings[precache_check], skinname);
							else if (strlen(skinname) > MD3_MAX_PATH - 1)
								Com_Error(ERR_DROP, "Model %s has too long a skin path: %s", cl.configstrings[precache_check], skinname);

							if (!CL_CheckOrDownloadFile(skinname))
							{
								precache_model_skin++;
								return; // Started a download
							}

							precache_model_skin++;

							md3mesh = (dmd3mesh_t *)((byte *)md3mesh + md3mesh->meshsize);
						}
					}
				}
				else // sprite
				{
					dsprite_t *spriteheader = (dsprite_t *)precache_model;
					while (precache_model_skin - 1 < spriteheader->numframes)
					{
						skinname = spriteheader->frames[(precache_model_skin - 1)].name;

						// r1ch: spam warning for models that are broken
						if (strchr(skinname, '\\'))
							Com_Printf("Warning, sprite %s with incorrectly linked skin: %s\n", cl.configstrings[precache_check], skinname);
						//else if (strlen(skinname) > MAX_SKINNAME - 1) //mxd. Never triggered, because dsprframe_t.name[64]
							//Com_Error(ERR_DROP, "Sprite %s has too long a skin path: %s", cl.configstrings[precache_check], skinname);

						if (!CL_CheckOrDownloadFile(skinname))
						{
							precache_model_skin++;
							return; // Started a download
						}

						precache_model_skin++;
					}
				}

				if (precache_model)
				{
					FS_FreeFile(precache_model);
					precache_model = 0;
				}

				precache_model_skin = 0;
				precache_check++;
			}
		}

		precache_check = cs_sounds;
	}

	if (precache_check >= cs_sounds && precache_check < cs_sounds + max_sounds)
	{ 
		if (allow_download_sounds->value)
		{
			if (precache_check == cs_sounds)
				precache_check++; // Zero is blank

			while (precache_check < cs_sounds + max_sounds && cl.configstrings[precache_check][0])
			{
				if (cl.configstrings[precache_check][0] == '*')
				{
					precache_check++;
					continue;
				}

				Com_sprintf(fn, sizeof(fn), "sound/%s", cl.configstrings[precache_check++]);
				if (!CL_CheckOrDownloadFile(fn))
					return; // Started a download
			}
		}

		precache_check = cs_images;
	}

	if (precache_check >= cs_images && precache_check < cs_images + max_images)
	{
		if (precache_check == cs_images)
			precache_check++; // Zero is blank

		while (precache_check < cs_images + max_images && cl.configstrings[precache_check][0])
		{	
			Com_sprintf(fn, sizeof(fn), "pics/%s.pcx", cl.configstrings[precache_check++]);
			if (!CL_CheckOrDownloadFile(fn))
				return; // Started a download
		}

		precache_check = cs_playerskins;
	}

	// Skins are special, since a player has three things to download:
	// model, weapon model and skin, so precache_check is now *3
	if (precache_check >= cs_playerskins && precache_check < cs_playerskins + MAX_CLIENTS * PLAYER_MULT)
	{
		if (allow_download_players->value)
		{
			while (precache_check < cs_playerskins + MAX_CLIENTS * PLAYER_MULT)
			{
				char model[MAX_QPATH], skin[MAX_QPATH], *p;

				const int playerindex = (precache_check - cs_playerskins) / PLAYER_MULT;

				// from R1Q2- skip invalid player skins data
				if (playerindex >= cl.maxclients)
				{
					precache_check = env_cnt;
					continue;
				}

				if (!cl.configstrings[cs_playerskins + playerindex][0])
				{
					precache_check = cs_playerskins + (playerindex + 1) * PLAYER_MULT;
					continue;
				}

				if ((p = strchr(cl.configstrings[cs_playerskins + playerindex], '\\')) != NULL)
					p++;
				else
					p = cl.configstrings[cs_playerskins + playerindex];

				Q_strncpyz(model, p, sizeof(model));
				p = strchr(model, '/');
				if (!p)
					p = strchr(model, '\\');

				if (p)
				{
					*p++ = 0;
					Q_strncpyz(skin, p, sizeof(skin));
				}
				else
				{
					*skin = 0;
				}

				const int downloadtype = (precache_check - cs_playerskins) % PLAYER_MULT;

				switch (downloadtype)
				{
				case 0: // Model
					Com_sprintf(fn, sizeof(fn), "players/%s/tris.md2", model);
					if (!CL_CheckOrDownloadFile(fn))
					{
						precache_check = cs_playerskins + playerindex * PLAYER_MULT + 1;
						return; // Started a download
					}
					/*FALL THROUGH*/

				case 1: // Weapon model
					Com_sprintf(fn, sizeof(fn), "players/%s/weapon.md2", model);
					if (!CL_CheckOrDownloadFile(fn))
					{
						precache_check = cs_playerskins + playerindex * PLAYER_MULT + 2;
						return; // Started a download
					}
					/*FALL THROUGH*/

				case 2: // Weapon skin
					Com_sprintf(fn, sizeof(fn), "players/%s/weapon.pcx", model);
					if (!CL_CheckOrDownloadFile(fn))
					{
						precache_check = cs_playerskins + playerindex * PLAYER_MULT + 3;
						return; // Started a download
					}
					/*FALL THROUGH*/

				case 3: // Skin
					Com_sprintf(fn, sizeof(fn), "players/%s/%s.pcx", model, skin);
					if (!CL_CheckOrDownloadFile(fn))
					{
						precache_check = cs_playerskins + playerindex * PLAYER_MULT + 4;
						return; // Started a download
					}
					/*FALL THROUGH*/

				case 4: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/%s_i.pcx", model, skin);
					if (!CL_CheckOrDownloadFile(fn))
					{
						precache_check = cs_playerskins + playerindex * PLAYER_MULT + 5;
						return; // Started a download
					}
					// Move on to next model
					precache_check = cs_playerskins + (playerindex + 1) * PLAYER_MULT;
					break;
				}
			}
		}

		// Precache phase completed
		precache_check = env_cnt;
	}

#ifdef USE_CURL	// HTTP downloading from R1Q2
	// Pending downloads (possibly the map), let's wait here.
	if (CL_PendingHTTPDownloads())
		return;
#endif	// USE_CURL

	if (precache_check == env_cnt)
	{
		if (Com_ServerState()) // If on local server, skip checking textures
			precache_check = texture_cnt + 999;
		else
			precache_check = env_cnt + 1;

		CM_LoadMap(cl.configstrings[CS_MODELS + 1], true, &map_checksum);

		if (map_checksum != (unsigned)atoi(cl.configstrings[CS_MAPCHECKSUM]))
		{
			Com_Error(ERR_DROP, "Local map version differs from server: '%i' != '%s'\n", map_checksum, cl.configstrings[CS_MAPCHECKSUM]);
			return;
		}
	}

	if (precache_check > env_cnt && precache_check < texture_cnt)
	{
		if (allow_download->value && allow_download_maps->value)
		{
			while (precache_check < texture_cnt)
			{
				static const char *env_suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

				const int n = precache_check++ - env_cnt - 1;
				char* format = (n & 1 ? "env/%s%s.pcx" : "env/%s%s.tga"); //mxd
				Com_sprintf(fn, sizeof(fn), format, cl.configstrings[CS_SKY], env_suf[n / 2]);
				
				if (!CL_CheckOrDownloadFile(fn))
					return; // Started a download
			}
		}

		precache_check = texture_cnt;
	}

	// Keeps track of already checked items in download texture blocks between CL_RequestNextDownload() calls.
	static int precache_tex = 0; //mxd. Made local

	if (precache_check == texture_cnt)
	{
		precache_check = texture_cnt + 1;
		precache_tex = 0;
	}

	// Confirm existance of .wal textures, download any that don't exist
	if (precache_check == texture_cnt + 1)
	{
		if (allow_download->value && allow_download_maps->value)
		{
			while (precache_tex < numtexinfo)
			{
				Com_sprintf(fn, sizeof(fn), "textures/%s.wal", map_surfaces[precache_tex++].rname);
				if (!CL_CheckOrDownloadFile(fn))
					return; // Started a download
			}
		}

		precache_check = texture_cnt + 2;
		precache_tex = 0;
	}

	// Confirm existance of .tga textures, try to download any that don't exist
	if (precache_check == texture_cnt + 2)
	{
		if (allow_download->value && allow_download_maps->value && allow_download_textures_24bit->value)
		{
			while (precache_tex < numtexinfo)
			{
				Com_sprintf(fn, sizeof(fn), "textures/%s.tga", map_surfaces[precache_tex++].rname);
				if (!CL_CheckOrDownloadFile(fn))
					return; // Started a download
			}
		}

		precache_check = texture_cnt + 3;
		precache_tex = 0; //mxd. Was missing in KMQ2
	}

	// Confirm existance of .png textures, try to download any that don't exist
	if (precache_check == texture_cnt + 3)
	{
		if (allow_download->value && allow_download_maps->value && allow_download_textures_24bit->value)
		{
			while (precache_tex < numtexinfo)
			{
				Com_sprintf(fn, sizeof(fn), "textures/%s.png", map_surfaces[precache_tex++].rname);
				if (!CL_CheckOrDownloadFile(fn))
					return; // Started a download
			}
		}

		precache_check = texture_cnt + 4;
		precache_tex = 0;
	}

	// Confirm existance of .jpg textures, try to download any that don't exist
	if (precache_check == texture_cnt + 4)
	{
		if (allow_download->value && allow_download_maps->value && allow_download_textures_24bit->value)
		{
			while (precache_tex < numtexinfo)
			{
				Com_sprintf(fn, sizeof(fn), "textures/%s.jpg", map_surfaces[precache_tex++].rname);
				if (!CL_CheckOrDownloadFile(fn))
					return; // Started a download
			}
		}

		precache_check = texture_cnt + 999;
		precache_tex = 0;
	}
//ZOID

#ifdef USE_CURL	// HTTP downloading from R1Q2
	// Pending downloads (possibly textures), let's wait here.
	if (CL_PendingHTTPDownloads())
		return;
#endif	// USE_CURL

	CL_RegisterSounds();
	CL_PrepRefresh();

	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
	MSG_WriteString(&cls.netchan.message, va("begin %i\n", precache_spawncount));
	cls.forcePacket = true;
}

#pragma endregion

#pragma region ======================= Failed downloads list

// Knightmare- store the names of last downloads that failed
#define NUM_FAILED_DOWNLOADS 1024 //mxd. Was 64
char lastfaileddownload[NUM_FAILED_DOWNLOADS][MAX_OSPATH];
static unsigned failedDlListIndex;

void CL_InitFailedDownloadList(void)
{
	for (int i = 0; i < NUM_FAILED_DOWNLOADS; i++)
		Com_sprintf(lastfaileddownload[i], sizeof(lastfaileddownload[i]), "\0");

	failedDlListIndex = 0;
}

qboolean CL_CheckDownloadFailed(char *name) //TODO: use hashing, like in R_CheckImgFailed
{
	for (int i = 0; i < NUM_FAILED_DOWNLOADS; i++)
		if (strlen(lastfaileddownload[i]) && !strcmp(name, lastfaileddownload[i]))
			return true; // We already tried downloading this, server didn't have it

	return false;
}

void CL_AddToFailedDownloadList(char *name)
{
	// Check if this name is already in the table
	for (int i = 0; i < NUM_FAILED_DOWNLOADS; i++)
		if (strlen(lastfaileddownload[i]) && !strcmp(name, lastfaileddownload[i]))
			return;

	// If it isn't already in the table, then we need to add it
	Com_sprintf(lastfaileddownload[failedDlListIndex++], sizeof(lastfaileddownload[failedDlListIndex]), "%s", name);

	// Wrap around to start of list
	if (failedDlListIndex >= NUM_FAILED_DOWNLOADS)
		failedDlListIndex = 0;
}

#pragma endregion

void CL_DownloadFileName(char *dest, int destlen, char *fn)
{
	char *dir = (strncmp(fn, "players", 7) == 0 ? BASEDIRNAME : FS_Gamedir()); //mxd
	Com_sprintf(dest, destlen, "%s/%s", dir, fn);
}

// Returns true if the file exists, otherwise it attempts to start a download from the server.
qboolean CL_CheckOrDownloadFile(char *filename)
{
	FILE *fp;
	char name[MAX_OSPATH];
	char s[128];

	if (strstr(filename, ".."))
	{
		Com_Printf("Refusing to download a file with relative path: \"%s\"\n", filename);
		return true;
	}

	if (FS_FileExists(filename)) //mxd. FS_LoadFile -> FS_FileExists
		return true; // It exists, no need to download

	// Don't try again to download a file that just failed
	if (CL_CheckDownloadFailed(filename))
		return true;

	// Don't download a .png texture which already has a .tga counterpart
	const int len = strlen(filename);
	Q_strncpyz(s, filename, sizeof(s));
	if (strstr(s, "textures/") && !strcmp(s + len - 4, ".png")) // Look if we have a .png texture 
	{
		// Replace extension
		s[len - 3] = 't';
		s[len - 2] = 'g';
		s[len - 1] = 'a';

		if (FS_FileExists(s)) // Check for .tga counterpart //mxd. FS_LoadFile -> FS_FileExists
			return true;
	}

	// Don't download a .jpg texture which already has a .tga or .png counterpart
	if (strstr(s, "textures/") && !strcmp(s + len - 4, ".jpg")) // Look if we have a .jpg texture 
	{ 
		// Replace extension 
		s[len - 3] = 't';
		s[len - 2] = 'g';
		s[len - 1] = 'a';

		if (FS_FileExists(s)) // Check for .tga counterpart //mxd. FS_LoadFile -> FS_FileExists
			return true;

		// Replace extension 
		s[len - 3] = 'p';
		s[len - 2] = 'n';
		s[len - 1] = 'g';

		if (FS_FileExists(s)) // Check for .png counterpart //mxd. FS_LoadFile -> FS_FileExists
			return true;
	}

#ifdef USE_CURL	// HTTP downloading from R1Q2
	if (CL_QueueHTTPDownload(filename))
	{
		// We return true so that the precache check keeps feeding us more files.
		// Since we have multiple HTTP connections we want to minimize latency
		// and be constantly sending requests, not one at a time.

		return true;
	}
#endif	// USE_CURL

	Q_strncpyz(cls.downloadname, filename, sizeof(cls.downloadname));

	// Download to a temp name, and only rename to the real name when done, so if interrupted a runt file won't be left
	COM_StripExtension(cls.downloadname, cls.downloadtempname);
	Q_strncatz(cls.downloadtempname, ".tmp", sizeof(cls.downloadtempname));

	//ZOID. Check to see if we already have a tmp for this file, if so, try to resume / open the file if not opened yet
	CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

	fp = fopen(name, "r+b");
	if (fp) // It exists
	{
		fseek(fp, 0, SEEK_END);
		const int flen = ftell(fp);

		cls.download = fp;

		// Give the server an offset to start the download
		Com_Printf("Resuming %s\n", cls.downloadname);
		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, va("download %s %i", cls.downloadname, flen));
	}
	else
	{
		Com_Printf("Downloading %s\n", cls.downloadname);
		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, va("download %s", cls.downloadname));
	}

	cls.downloadnumber++;
	cls.forcePacket = true;

	return false;
}

// Request a download from the server
void CL_Download_f(void)
{
	char filename[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: download <filename>\n");
		return;
	}

	Com_sprintf(filename, sizeof(filename), "%s", Cmd_Argv(1));

	if (strstr(filename, ".."))
	{
		Com_Printf("Refusing to download a file with relative path.\n");
		return;
	}

	if (FS_FileExists(filename)) //mxd. FS_LoadFile -> FS_FileExists
	{
		// It exists, no need to download
		Com_Printf("File already exists.\n");
		return;
	}

	Q_strncpyz(cls.downloadname, filename, sizeof(cls.downloadname));
	Com_Printf("Downloading %s\n", cls.downloadname);

	// Download to a temp name, and only rename to the real name when done, so if interrupted a runt file wont be left
	COM_StripExtension(cls.downloadname, cls.downloadtempname);
	Q_strncatz(cls.downloadtempname, ".tmp", sizeof(cls.downloadtempname));

	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
	MSG_WriteString(&cls.netchan.message, va("download %s", cls.downloadname));

	cls.downloadnumber++;
}

//=============================================================================

// A download message has been received from the server
void CL_ParseDownload(void)
{
	// Read the data
	const int size = MSG_ReadShort(&net_message);
	const int percent = MSG_ReadByte(&net_message);
	if (size == -1)
	{
		Com_Printf("Server does not have this file.\n");

		if (cls.downloadname[0]) // Knightmare- save name of failed download //mxd. Address of array 'cls.downloadname' will always evaluate to 'true'
			CL_AddToFailedDownloadList(cls.downloadname);

		if (cls.download)
		{
			// If here, we tried to resume a file but the server said no
			fclose(cls.download);
			cls.download = NULL;
		}

		CL_RequestNextDownload();
		return;
	}

	// Open the file if not opened yet
	if (!cls.download)
	{
		char name[MAX_OSPATH];

		CL_Download_Reset_KBps_counter(); // Knightmare- for KB/s counter
		CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);
		FS_CreatePath(name);

		cls.download = fopen(name, "wb");
		if (!cls.download)
		{
			net_message.readcount += size;
			Com_Printf("Failed to open %s\n", cls.downloadtempname);
			CL_RequestNextDownload();

			return;
		}
	}

	fwrite(net_message.data + net_message.readcount, 1, size, cls.download);
	net_message.readcount += size;

	if (percent != 100)
	{
		// Request next block
		// change display routines by zoid
		CL_Download_Calculate_KBps(size, 0); // Knightmare- for KB/s counter
		cls.downloadpercent = percent;

		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		SZ_Print(&cls.netchan.message, "nextdl");
		cls.forcePacket = true;
	}
	else
	{
		char oldn[MAX_OSPATH];
		char newn[MAX_OSPATH];

		fclose(cls.download);

		// Rename the temp file to it's final name
		CL_DownloadFileName(oldn, sizeof(oldn), cls.downloadtempname);
		CL_DownloadFileName(newn, sizeof(newn), cls.downloadname);
		const int r = rename(oldn, newn);
		if (r)
			Com_Printf("Failed to rename downloaded file '%s'.\n", oldn);

		cls.download = NULL;
		cls.downloadpercent = 0;

		// Add new pk3s to search paths, hack by Jay Dolan
		if (strstr(newn, ".pk3"))
			FS_AddPK3File(newn);

		// Get another file if needed
		CL_RequestNextDownload();
	}
}

#pragma region ======================= Download speed counter

typedef struct
{
	int		prevTime;
	int		bytesRead;
	int		byteCount;
	float	timeCount;
	float	prevTimeCount;
	float	startTime;
} dlSpeedInfo_t;

static dlSpeedInfo_t dlSpeedInfo;


void CL_Download_Reset_KBps_counter(void)
{
	dlSpeedInfo.timeCount = 0;
	dlSpeedInfo.prevTime = 0;
	dlSpeedInfo.prevTimeCount = 0;
	dlSpeedInfo.bytesRead = 0;
	dlSpeedInfo.byteCount = 0;
	dlSpeedInfo.startTime = (float)cls.realtime;
	cls.downloadrate = 0;
}

void CL_Download_Calculate_KBps(int byteDistance, int totalSize)
{
	const float	timeDistance = (float)(cls.realtime - dlSpeedInfo.prevTime);
	const float	totalTime = (dlSpeedInfo.timeCount - dlSpeedInfo.startTime) / 1000.0f;

	dlSpeedInfo.timeCount += timeDistance;
	dlSpeedInfo.byteCount += byteDistance;
	dlSpeedInfo.bytesRead += byteDistance;

	if (totalTime >= 1.0f)
	{
		cls.downloadrate = dlSpeedInfo.byteCount / 1024.0f;
		Com_DPrintf("Rate: %4.2fKB/s, Downloaded %4.2fKB of %4.2fKB\n", cls.downloadrate, dlSpeedInfo.bytesRead / 1024.0f, totalSize / 1024.0f);
		dlSpeedInfo.byteCount = 0;
		dlSpeedInfo.startTime = (float)cls.realtime;
	}

	dlSpeedInfo.prevTime = cls.realtime;
}

#pragma endregion