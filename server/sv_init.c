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

#include "server.h"

server_static_t	svs; // Persistant server info
server_t sv; // Local server

static int SV_FindIndex(char *name, const int start, const int max)
{
	int index;
	
	if (!name || !name[0])
		return 0;

	for (index = 1; index < max && sv.configstrings[start + index][0]; index++)
		if (!strcmp(sv.configstrings[start + index], name))
			return index;

	// Knightmare 12/23/2001
	// Output a more useful error message to tell user what overflowed
	// And don't bomb out, either - instead, return last possible index
	if (index == max)
	{
		if (start == CS_MODELS)
			Com_Printf(S_COLOR_YELLOW"Warning: Index overflow for models\n");
		else if (start == CS_SOUNDS)
			Com_Printf(S_COLOR_YELLOW"Warning: Index overflow for sounds\n");
		else if (start == CS_IMAGES)
			Com_Printf(S_COLOR_YELLOW"Warning: Index overflow for images\n");

		return max - 1; // Return the last possible index
	}
	// end Knightmare

	strncpy(sv.configstrings[start + index], name, sizeof(sv.configstrings[index]));

	if (sv.state != ss_loading)
	{
		// Send the update to everyone
		SZ_Clear(&sv.multicast);
		MSG_WriteChar(&sv.multicast, svc_configstring);
		MSG_WriteShort(&sv.multicast, start + index);
		MSG_WriteString(&sv.multicast, name);
		SV_Multicast(vec3_origin, MULTICAST_ALL_R);
	}

	return index;
}


int SV_ModelIndex(char *name)
{
	return SV_FindIndex(name, CS_MODELS, MAX_MODELS);
}

int SV_SoundIndex(char *name)
{
	return SV_FindIndex(name, CS_SOUNDS, MAX_SOUNDS);
}

int SV_ImageIndex(char *name)
{
	return SV_FindIndex(name, CS_IMAGES, MAX_IMAGES);
}

// Entity baselines are used to compress the update messages to the clients. Only the fields that differ from the baseline will be transmitted.
static void SV_CreateBaseline()
{
	for (int entnum = 1; entnum < ge->num_edicts; entnum++)
	{
		edict_t *svent = EDICT_NUM(entnum);
		if (!svent->inuse)
			continue;

		if (!svent->s.modelindex && !svent->s.sound && !svent->s.effects)
			continue;

		svent->s.number = entnum;

		// Take current state as baseline
		VectorCopy(svent->s.origin, svent->s.old_origin);
		sv.baselines[entnum] = svent->s;
	}
}

static void SV_CheckForSavegame()
{
	if (sv_noreload->integer || Cvar_VariableInteger("deathmatch"))
		return;

	char name[MAX_OSPATH];
	Com_sprintf(name, sizeof(name), "%s/save/current/%s.sav", FS_Gamedir(), sv.name);
	FILE *f = fopen(name, "rb");
	if (!f)
		return; // No savegame

	fclose(f);

	SV_ClearWorld();

	// Get configstrings and areaportals
	SV_ReadLevelFile();

	if (!sv.loadgame)
	{
		// Coming back to a level after being in a different level, so run it for ten seconds.

		// rlava2 was sending too many lightstyles, and overflowing the reliable data.
		// Temporarily changing the server state to loading prevents these from being passed down.
		const server_state_t previousState = sv.state; // PGM
		sv.state = ss_loading; // PGM
		
		for (int i = 0; i < 100; i++)
			ge->RunFrame();

		sv.state = previousState; // PGM
	}
}

// Change the server to a new map, taking all connected clients along with it.
static void SV_SpawnServer(char *server, char *spawnpoint, const server_state_t serverstate, const qboolean attractloop, const qboolean loadgame)
{
	uint checksum;

	if (attractloop)
		Cvar_Set("paused", "0");

	Com_Printf("------- Server Initialization -------\n");

	Com_DPrintf("SpawnServer: %s\n", server);
	if (sv.demofile)
		FS_FCloseFile(sv.demofile);

	svs.spawncount++; // Any partially connected client will be restarted
	sv.state = ss_dead;
	Com_SetServerState(sv.state);

	// Wipe the entire per-level structure
	memset(&sv, 0, sizeof(sv));
	svs.realtime = 0;
	sv.loadgame = loadgame;
	sv.attractloop = attractloop;

	// Save name for levels that don't set message
	Q_strncpyz(sv.configstrings[CS_NAME], server, sizeof(sv.configstrings[0]));
	if (Cvar_VariableValue("deathmatch"))
	{
		Com_sprintf(sv.configstrings[CS_AIRACCEL], sizeof(sv.configstrings[0]), "%g", sv_airaccelerate->value);
		pm_airaccelerate = sv_airaccelerate->value;
	}
	else
	{
		Q_strncpyz(sv.configstrings[CS_AIRACCEL], "0", sizeof(sv.configstrings[0]));
		pm_airaccelerate = 0;
	}

	SZ_Init(&sv.multicast, sv.multicast_buf, sizeof(sv.multicast_buf));

	Q_strncpyz(sv.name, server, sizeof(sv.name));

	// Leave slots at start for clients only
	for (int i = 0; i < maxclients->value; i++)
	{
		// Needs to reconnect
		if (svs.clients[i].state > cs_connected)
			svs.clients[i].state = cs_connected;

		svs.clients[i].lastframe = -1;
	}

	sv.time = 1000;
	
	Q_strncpyz(sv.name, server, sizeof(sv.name));
	Q_strncpyz(sv.configstrings[CS_NAME], server, sizeof(sv.configstrings[0]));

	if (serverstate != ss_game)
	{
		sv.models[1] = CM_LoadMap("", false, &checksum); // No real map
	}
	else
	{
		Com_sprintf(sv.configstrings[CS_MODELS + 1], sizeof(sv.configstrings[0]), "maps/%s.bsp", server);
	
		// Resolve CS_PAKFILE, hack by Jay Dolan
		if(FS_FileExists(sv.configstrings[CS_MODELS + 1])) //mxd. If the file doesn't exist, trigger "Couldn't load [map]" message in CM_LoadMap, instead of "FS_GetFileByHandle: out of range" in FS_GetFileByHandle...
		{
			fileHandle_t f;
			FS_FOpenFile(sv.configstrings[CS_MODELS + 1], &f, FS_READ);
			Q_strncpyz(sv.configstrings[CS_PAKFILE], (last_pk3_name[0] ? last_pk3_name : ""), sizeof(sv.configstrings[0])); //mxd. Address of array 'last_pk3_name' will always evaluate to 'true'
			FS_FCloseFile(f);
		}
	
		sv.models[1] = CM_LoadMap(sv.configstrings[CS_MODELS + 1], false, &checksum);
	}

	Com_sprintf(sv.configstrings[CS_MAPCHECKSUM], sizeof(sv.configstrings[0]), "%i", checksum);

	// Clear physics interaction links
	SV_ClearWorld();
	
	for (int i = 1; i < CM_NumInlineModels(); i++)
	{
		Com_sprintf(sv.configstrings[CS_MODELS + 1 + i], sizeof(sv.configstrings[0]), "*%i", i);
		sv.models[i + 1] = CM_InlineModel(sv.configstrings[CS_MODELS + 1 + i]);
	}

	// Spawn the rest of the entities on the map

	// Precache and static commands can be issued during map initialization
	sv.state = ss_loading;
	Com_SetServerState(sv.state);

	// Load and spawn all other entities
	ge->SpawnEntities(sv.name, CM_EntityString(), spawnpoint);

	// Run two frames to allow everything to settle
	ge->RunFrame();
	ge->RunFrame();

	// All precaches are complete
	sv.state = serverstate;
	Com_SetServerState(sv.state);
	
	// Create a baseline for more efficient communications
	SV_CreateBaseline();

	// Check for a savegame
	SV_CheckForSavegame();

	// Set serverinfo variable
	Cvar_FullSet("mapname", sv.name, CVAR_SERVERINFO | CVAR_NOSET);

	Com_Printf("-------------------------------------\n");
}

// A brand new game has been started
void SV_InitGame()
{
	char idmaster[32];

	if (svs.initialized)
	{
		// Cause any connected clients to reconnect
		SV_Shutdown("Server restarted\n", true);
	}
	else
	{
		// Make sure the client is down
		CL_Drop();
		SCR_BeginLoadingPlaque(NULL);
	}

	// Get any latched variable changes (maxclients, etc)
	Cvar_GetLatchedVars();

	svs.initialized = true;

	if (Cvar_VariableValue("coop") && Cvar_VariableValue("deathmatch"))
	{
		Com_Printf("Deathmatch and Coop both set, disabling Coop\n");
		Cvar_FullSet("coop", "0",  CVAR_SERVERINFO | CVAR_LATCH);
	}

	// Dedicated servers can't be single player and are usually DM, so unless they explicity set coop, force it to deathmatch
	if (dedicated->integer && !Cvar_VariableInteger("coop"))
		Cvar_FullSet("deathmatch", "1",  CVAR_SERVERINFO | CVAR_LATCH);

	// Init clients
	if (Cvar_VariableInteger("deathmatch"))
	{
		if (maxclients->value <= 1)
			Cvar_FullSet("maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH);
		else if (maxclients->value > MAX_CLIENTS)
			Cvar_FullSet("maxclients", va("%i", MAX_CLIENTS), CVAR_SERVERINFO | CVAR_LATCH);
	}
	else if (Cvar_VariableInteger("coop"))
	{
		if (maxclients->value <= 1 || maxclients->value > 4)
			Cvar_FullSet("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
	}
	else // Non-deathmatch, non-coop is singleplayer
	{
		Cvar_FullSet("maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH);
	}

	svs.spawncount = rand();
	svs.clients = Z_Malloc(sizeof(client_t) * maxclients->value);
	svs.num_client_entities = maxclients->value * UPDATE_BACKUP * 64;
	svs.client_entities = Z_Malloc(sizeof(entity_state_t) * svs.num_client_entities);

	// Init network stuff
	NET_Config(maxclients->value > 1);

	// Heartbeats will always be sent to the id master
	svs.last_heartbeat = -99999; // Send immediately
	Com_sprintf(idmaster, sizeof(idmaster), "192.246.40.37:%i", PORT_MASTER);
	NET_StringToAdr(idmaster, &master_adr[0]);

	// Init game
	SV_InitGameProgs();
	for (int i = 0; i < maxclients->value; i++)
	{
		edict_t *ent = EDICT_NUM(i + 1);
		ent->s.number = i + 1;
		svs.clients[i].edict = ent;
		memset(&svs.clients[i].lastcmd, 0, sizeof(svs.clients[i].lastcmd));
	}
}

// The full syntax is:
//		map [*]<map>$<startspot>+<nextserver>
// command from the console or progs.
// Map can also be a .cin, .pcx, or .dm2 file
// Nextserver is used to allow a cinematic to play, then proceed to another level:
//		map tram.cin+jail_e3
void SV_Map(const qboolean attractloop, const char *levelstring, const qboolean loadgame)
{
	char level[MAX_QPATH];
	char spawnpoint[MAX_QPATH];

	sv.loadgame = loadgame;
	sv.attractloop = attractloop;

	if (sv.state == ss_dead && !sv.loadgame)
		SV_InitGame(); // The game is just starting

	//mxd. Reset soundtrack position when changing maps...
	if (!sv.loadgame)
		Cvar_FullSet("musictrackframe", "-1", CVAR_LATCH);

	// r1ch fix: buffer overflow
	if (levelstring[0] == '*')
		Q_strncpyz(level, levelstring + 1, sizeof(level));
	else
		Q_strncpyz(level, levelstring, sizeof(level));

	// If there is a + in the map, set nextserver to the remainder
	char *ch = strchr(level, '+'); //mxd. strstr -> strchr
	if (ch)
	{
		*ch = 0;
		Cvar_Set("nextserver", va("gamemap \"%s\"", ch + 1));
	}
	else
	{
		Cvar_Set("nextserver", "");
	}

	//ZOID special hack for end game screen in coop mode
	if (Cvar_VariableInteger("coop") && !Q_stricmp(level, "victory.pcx"))
		Cvar_Set("nextserver", "gamemap \"*base1\"");

	// If there is a $, use the remainder as a spawnpoint
	ch = strchr(level, '$'); //mxd. strstr -> strchr
	if (ch)
	{
		*ch = 0;
		Q_strncpyz(spawnpoint, ch + 1, sizeof(spawnpoint));
	}
	else
	{
		spawnpoint[0] = 0;
	}

	if (!dedicated->integer)
		SCR_BeginLoadingPlaque(level); // For local system

	SV_BroadcastCommand("changing\n");

	const char *ext = COM_FileExtension(level); //mxd

	if (!strcmp(ext, "cin") || !strcmp(ext, "roq"))
	{
		SV_SpawnServer(level, spawnpoint, ss_cinematic, attractloop, loadgame);
	}
	else if (!strcmp(ext, "dm2"))
	{
		SV_SpawnServer(level, spawnpoint, ss_demo, attractloop, loadgame);
	}
	else if (!strcmp(ext, "pcx"))
	{
		SV_SpawnServer(level, spawnpoint, ss_pic, attractloop, loadgame);
	}
	else
	{
		SV_SendClientMessages();
		SV_SpawnServer(level, spawnpoint, ss_game, attractloop, loadgame);
		Cbuf_CopyToDefer();
	}

	SV_BroadcastCommand("reconnect\n");
}