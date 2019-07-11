/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2000-2002 Mr. Hyde and Mad Dog

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

#include "g_local.h"

game_locals_t	game;
level_locals_t	level;
game_import_t	gi;
game_export_t	globals;
spawn_temp_t	st;

int sm_meat_index;
int snd_fry;
int meansOfDeath;

edict_t *g_edicts;

cvar_t *deathmatch;
cvar_t *coop;
cvar_t *dmflags;
cvar_t *skill;
cvar_t *fraglimit;
cvar_t *timelimit;
//ZOID
cvar_t *capturelimit;
cvar_t *instantweap;
//ZOID
cvar_t *password;
cvar_t *spectator_password;
cvar_t *needpass;
cvar_t *maxclients;
cvar_t *maxspectators;
cvar_t *maxentities;
cvar_t *g_select_empty;
cvar_t *dedicated;

cvar_t *filterban;

cvar_t *sv_maxvelocity;
cvar_t *sv_gravity;

cvar_t *sv_rollspeed;
cvar_t *sv_rollangle;
cvar_t *gun_x;
cvar_t *gun_y;
cvar_t *gun_z;

cvar_t *run_pitch;
cvar_t *run_roll;
cvar_t *bob_up;
cvar_t *bob_pitch;
cvar_t *bob_roll;

cvar_t *sv_cheats;

cvar_t *flood_msgs;
cvar_t *flood_persecond;
cvar_t *flood_waitdelay;

cvar_t *sv_maplist;

cvar_t *actorchicken;
cvar_t *actorjump;
cvar_t *actorscram;
cvar_t *alert_sounds;
cvar_t *allow_download;
cvar_t *allow_fog; // Set to 0 for no fog

// Set to 0 to bypass target_changelevel clear inventory flag
// because some user maps have this erroneously set
cvar_t *allow_clear_inventory;

cvar_t *bounce_bounce;
cvar_t *bounce_minv;
cvar_t *ogg_loopcount; //mxd. Was cd_loopcount
cvar_t *cl_gun;
cvar_t *cl_thirdperson; // Knightmare added
cvar_t *corpse_fade;
cvar_t *corpse_fadetime;
cvar_t *crosshair;
cvar_t *developer;
cvar_t *footstep_sounds;
cvar_t *info_fov;
cvar_t *gl_clear;
cvar_t *gl_driver; //mxd. No longer used
cvar_t *info_hand;
cvar_t *jetpack_weenie;
cvar_t *joy_pitchsensitivity;
cvar_t *joy_yawsensitivity;
cvar_t *jump_kick;
cvar_t *lazarus_cd_loop;
cvar_t *lazarus_cl_gun;
cvar_t *lazarus_crosshair;
cvar_t *lazarus_gl_clear;
cvar_t *lazarus_joyp;
cvar_t *lazarus_joyy;
cvar_t *lazarus_pitch;
cvar_t *lazarus_yaw;
cvar_t *lights;
cvar_t *lightsmin;
cvar_t *m_pitch;
cvar_t *m_yaw;
cvar_t *monsterjump;
cvar_t *readout;
cvar_t *rocket_strafe;
cvar_t *rotate_distance;
cvar_t *shift_distance;
cvar_t *sv_maxgibs;
cvar_t *turn_rider;
cvar_t *vid_ref;
cvar_t *zoomrate;
cvar_t *zoomsnap;
cvar_t *printobjectives; //mxd

cvar_t *sv_stopspeed; //PGM (this was a define in g_phys.c)
cvar_t *sv_step_fraction; // Knightmare- this was a define in p_view.c

cvar_t *blaster_color; // Knightmare added

//===================================================================

static game_import_t RealFunc;
int max_modelindex;
int max_soundindex;

static int Debug_Modelindex(char *name)
{
	const int modelnum = RealFunc.modelindex(name);
	if (modelnum > max_modelindex)
	{
		gi.dprintf("Model %03d %s\n", modelnum, name);
		max_modelindex = modelnum;
	}

	return modelnum;
}

static int Debug_Soundindex(char *name)
{
	const int soundnum = RealFunc.soundindex(name);
	if (soundnum > max_soundindex)
	{
		gi.dprintf("Sound %03d %s\n", soundnum, name);
		max_soundindex = soundnum;
	}

	return soundnum;
}

extern void SpawnEntities(char *mapname, char *entities, char *spawnpoint);
extern void ClientDisconnect(edict_t *ent);
extern void ClientBegin(edict_t *ent);
extern void ClientCommand(edict_t *ent);
extern void WriteGame(char *filename, qboolean autosave);
extern void ReadGame(char *filename);
extern void WriteLevel(char *filename);
extern void ReadLevel(char *filename);
extern void InitGame(void);
extern void ShutdownGame(void); //mxd. Moved to g_save.c
void G_RunFrame(void);

// Returns a pointer to the structure with all entry points and global variables
game_export_t *GetGameAPI(game_import_t *import)
{
	gi = *import;

	globals.apiversion = GAME_API_VERSION;
	globals.Init = InitGame;
	globals.Shutdown = ShutdownGame;
	globals.SpawnEntities = SpawnEntities;

	globals.WriteGame = WriteGame;
	globals.ReadGame = ReadGame;
	globals.WriteLevel = WriteLevel;
	globals.ReadLevel = ReadLevel;

	globals.ClientThink = ClientThink;
	globals.ClientConnect = ClientConnect;
	globals.ClientUserinfoChanged = ClientUserinfoChanged;
	globals.ClientDisconnect = ClientDisconnect;
	globals.ClientBegin = ClientBegin;
	globals.ClientCommand = ClientCommand;

	globals.RunFrame = G_RunFrame;

	globals.ServerCommand = ServerCommand;

	globals.edict_size = sizeof(edict_t);

	gl_driver = gi.cvar("gl_driver", "", 0);
	vid_ref = gi.cvar("vid_ref", "", 0);

	Fog_Init();

	developer = gi.cvar("developer", "0", CVAR_SERVERINFO);
	readout = gi.cvar("readout", "0", CVAR_SERVERINFO);
	if (readout->integer)
	{
		max_modelindex = 0;
		max_soundindex = 0;
		RealFunc.modelindex = gi.modelindex;
		RealFunc.soundindex = gi.soundindex;
		gi.modelindex = Debug_Modelindex;
		gi.soundindex = Debug_Soundindex;
	}

	return &globals;
}

#ifndef GAME_HARD_LINKED

// This is only here so the functions in q_shared.c and q_shwin.c can link
void Sys_Error(char *error, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	gi.error(ERR_FATAL, "%s", text);
}

void Com_Printf(char *msg, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	gi.dprintf("%s", text);
}

#endif

static void ClientEndServerFrames(void)
{
	// Calculate the player views now that all pushing and damage has been added
	for (int i = 0; i < maxclients->integer; i++)
	{
		edict_t *ent = g_edicts + 1 + i;

		if (ent->inuse && ent->client)
			ClientEndServerFrame(ent);
	}

	// Reflection stuff - modified from psychospaz' original code
	if (level.num_reflectors)
	{
		edict_t *ent = &g_edicts[0];
		for (int i = 0; i < globals.num_edicts; i++, ent++) // Pointers, not as slow as you think
		{
			if (!ent->inuse || !ent->s.modelindex)
				continue;

			if (ent->flags & FL_REFLECT)
				continue;

			if (!ent->client && (ent->svflags & SVF_NOCLIENT))
				continue;

			if (ent->client && !ent->client->chasetoggle && (ent->svflags & SVF_NOCLIENT))
				continue;

			if (ent->svflags & SVF_MONSTER && ent->solid != SOLID_BBOX)
				continue;

			if (ent->solid == SOLID_BSP && ent->movetype != MOVETYPE_PUSHABLE)
				continue;

			if (ent->client && (ent->client->resp.spectator || ent->health <= 0 || ent->deadflag == DEAD_DEAD))
				continue;
			
			AddReflection(ent);	
		}
	}
}

// Returns the created target changelevel
static edict_t *CreateTargetChangeLevel(char *map)
{
	edict_t *ent = G_Spawn();
	ent->classname = "target_changelevel";
	Com_sprintf(level.nextmap, sizeof(level.nextmap), "%s", map);
	ent->map = level.nextmap;

	return ent;
}

// The timelimit or fraglimit has been exceeded
void EndDMLevel(void)
{
	static const char *delimeter = " ,\n\r";

	// Stay on same level flag
	if (dmflags->integer & DF_SAME_LEVEL)
	{
		BeginIntermission(CreateTargetChangeLevel(level.mapname));
		return;
	}

	// See if it's in the map list
	if (*sv_maplist->string)
	{
		char *maplist = strdup(sv_maplist->string);
		char *firstmapname = NULL;
		char *mapname = strtok(maplist, delimeter);

		while (mapname != NULL)
		{
			if (Q_stricmp(mapname, level.mapname) == 0)
			{
				// It's in the list, go to the next one
				mapname = strtok(NULL, delimeter);
				if (mapname == NULL)
				{
					// End of list, go to first one
					if (firstmapname == NULL) // There isn't a first one, same level
						BeginIntermission(CreateTargetChangeLevel(level.mapname));
					else
						BeginIntermission(CreateTargetChangeLevel(firstmapname));
				}
				else
				{
					BeginIntermission(CreateTargetChangeLevel(mapname));
				}

				free(maplist);
				return;
			}

			if (!firstmapname)
				firstmapname = mapname;

			mapname = strtok(NULL, delimeter);
		}

		free(maplist);
	}

	// Go to a specific map
	if (level.nextmap[0])
	{
		BeginIntermission(CreateTargetChangeLevel(level.nextmap));
		return;
	}

	// Search for a changelevel
	edict_t *ent = G_Find(NULL, FOFS(classname), "target_changelevel");
	if (ent)
	{
		BeginIntermission(ent);
		return;
	}

	// The map designer didn't include a changelevel, so create a fake ent that goes back to the same level.
	BeginIntermission(CreateTargetChangeLevel(level.mapname));
}

static void CheckNeedPass(void)
{
	// If password or spectator_password has changed, update needpass as needed
	if (password->modified || spectator_password->modified) 
	{
		password->modified = false;
		spectator_password->modified = false;

		int need = 0;

		if (*password->string && Q_stricmp(password->string, "none"))
			need |= 1;

		if (*spectator_password->string && Q_stricmp(spectator_password->string, "none"))
			need |= 2;

		gi.cvar_set("needpass", va("%d", need));
	}
}

static void CheckDMRules(void)
{
	if (level.intermissiontime || !deathmatch->integer)
		return;

	if (timelimit->value)
	{
		if (level.time >= timelimit->value * 60)
		{
			safe_bprintf(PRINT_HIGH, "Timelimit hit.\n");
			EndDMLevel();

			return;
		}
	}

	if (fraglimit->integer)
	{
		for (int i = 0; i < maxclients->integer; i++)
		{
			gclient_t *cl = game.clients + i;
			if (!g_edicts[i + 1].inuse)
				continue;

			if (cl->resp.score >= fraglimit->integer)
			{
				safe_bprintf(PRINT_HIGH, "Fraglimit hit.\n");
				EndDMLevel();

				return;
			}
		}
	}
}

static void ExitLevel(void)
{
	char command[256];
	Com_sprintf(command, sizeof(command), "gamemap \"%s\"\n", level.changemap);
	gi.AddCommandString(command);

	level.changemap = NULL;
	level.exitintermission = 0;
	level.intermissiontime = 0;
	ClientEndServerFrames();

	// Clear some things before going to next level
	for (int i = 0; i < maxclients->integer; i++)
	{
		edict_t *ent = g_edicts + 1 + i;
		if (ent->inuse && ent->health > ent->client->pers.max_health)
			ent->health = ent->client->pers.max_health;
	}

	// mxd added
	gibsthisframe = 0;
	lastgibframe = 0;
}

extern void CheckNumTechs(void);

// Advances the world by 0.1 seconds
static void G_RunFrame(void)
{
	// Knightmare- dm pause
	if (paused && deathmatch->integer)
		return;

	if (level.freeze)
	{
		level.freezeframes++;
		if (level.freezeframes >= sk_stasis_time->value * 10)
			level.freeze = false;
	}
	else
	{
		level.framenum++;
	}

	level.time = level.framenum * FRAMETIME;

	// Choose a client for monsters to target this frame
	AI_SetSightClient();

	// Exit intermissions
	if (level.exitintermission)
	{
		ExitLevel();
		return;
	}

	if (use_techs->integer || (ctf->integer && !(dmflags->integer & DF_CTF_NO_TECH)))
		CheckNumTechs();

	// Treat each object in turn. Even the world gets a chance to think
	edict_t *ent = &g_edicts[0];
	for (int i = 0; i < globals.num_edicts; i++, ent++)
	{
		if (!ent->inuse)
			continue;

		level.current_entity = ent;

		VectorCopy(ent->s.origin, ent->s.old_origin);

		// If the ground entity moved, make sure we are still on it
		if (ent->groundentity && ent->groundentity->linkcount != ent->groundentity_linkcount)
		{
			ent->groundentity = NULL;
			if (!(ent->flags & (FL_SWIM | FL_FLY)) && (ent->svflags & SVF_MONSTER))
				M_CheckGround(ent);
		}

		if (i > 0 && i <= maxclients->integer)
		{
			ClientBeginServerFrame(ent);

			// ACEBOT_ADD
			if (!ent->is_bot) // Bots need G_RunEntity called
				continue;
		}

		G_RunEntity(ent);
	}

	// See if it is time to end a deathmatch
	CheckDMRules();

	// See if needpass needs updated
	CheckNeedPass();

	// Build the playerstate_t structures for all players
	ClientEndServerFrames();
}