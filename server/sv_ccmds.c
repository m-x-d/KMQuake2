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
#include "../client/ref.h"

#pragma region ======================= OPERATOR CONSOLE COMMANDS
// These commands can only be entered from stdin or by a remote operator datagram

// Specify a list of master servers
void SV_SetMaster_f(void)
{
	// Only dedicated servers send heartbeats
	if (!dedicated->value)
	{
		Com_Printf("Only dedicated servers use masters.\n");
		return;
	}

	// Make sure the server is listed public
	Cvar_Set("public", "1"); // Vic's fix

	for (int i = 1; i < MAX_MASTERS; i++)
		memset(&master_adr[i], 0, sizeof(master_adr[i]));

	int slot = 1; // Slot 0 will always contain the id master
	for (int i = 1; i < Cmd_Argc(); i++)
	{
		if (slot == MAX_MASTERS)
			break;

		if (!NET_StringToAdr(Cmd_Argv(i), &master_adr[i]))
		{
			Com_Printf("Bad address: %s\n", Cmd_Argv(i));
			continue;
		}

		if (master_adr[slot].port == 0)
			master_adr[slot].port = BigShort(PORT_MASTER);

		Com_Printf("Master server at %s\n", NET_AdrToString(master_adr[slot]));
		
		Com_Printf("Sending a ping.\n");
		Netchan_OutOfBandPrint(NS_SERVER, master_adr[slot], "ping");

		slot++;
	}

	svs.last_heartbeat = -9999999;
}

// Sets sv_client and sv_player to the player with idnum Cmd_Argv(1)
qboolean SV_SetPlayer(void)
{
	if (Cmd_Argc() < 2)
		return false;

	char *s = Cmd_Argv(1);

	// Numeric values are just slot numbers
	if (s[0] >= '0' && s[0] <= '9')
	{
		const int idnum = atoi(Cmd_Argv(1));
		if (idnum < 0 || idnum >= maxclients->value)
		{
			Com_Printf("Bad client slot: %i\n", idnum);
			return false;
		}

		sv_client = &svs.clients[idnum];
		sv_player = sv_client->edict;
		if (!sv_client->state)
		{
			Com_Printf("Client %i is not active\n", idnum);
			return false;
		}

		return true;
	}

	// Check for a name match
	client_t *cl = svs.clients;
	for (int i = 0; i < maxclients->value; i++, cl++)
	{
		if (!cl->state)
			continue;

		if (!strcmp(cl->name, s))
		{
			sv_client = cl;
			sv_player = sv_client->edict;
			return true;
		}
	}

	Com_Printf("Userid %s is not on the server\n", s);
	return false;
}

#pragma endregion

#pragma region ======================= SAVEGAME FILES

void R_GrabScreen(void); // Knightmare- screenshots for savegames
void R_ScaledScreenshot(char *filename); // Knightmare- screenshots for savegames
void R_FreePic(char *name); // Knightmare- unregisters an image

// Delete save/<XXX>/
void SV_WipeSavegame(char *savename)
{
	char name[MAX_OSPATH];

	Com_DPrintf("SV_WipeSaveGame(%s)\n", savename);

	Com_sprintf(name, sizeof(name), "%s/save/%s/server.ssv", FS_Gamedir(), savename);
	remove(name);
	Com_sprintf(name, sizeof(name), "%s/save/%s/game.ssv", FS_Gamedir(), savename);
	remove(name);

	// Knightmare- delete screenshot
	Com_sprintf(name, sizeof(name), "%s/save/%s/shot.jpg", FS_Gamedir(), savename);
	remove(name);

	Com_sprintf(name, sizeof(name), "%s/save/%s/*.sav", FS_Gamedir(), savename);
	char *s = Sys_FindFirst(name, 0, 0);
	while (s)
	{
		remove(s);
		s = Sys_FindNext(0, 0);
	}
	Sys_FindClose();

	Com_sprintf(name, sizeof(name), "%s/save/%s/*.sv2", FS_Gamedir(), savename);
	s = Sys_FindFirst(name, 0, 0);
	while (s)
	{
		remove (s);
		s = Sys_FindNext(0, 0);
	}
	Sys_FindClose();
}

void SV_CopySaveGame(char *src, char *dst)
{
	Com_DPrintf("SV_CopySaveGame(%s, %s)\n", src, dst);
	SV_WipeSavegame(dst);

	// Copy the savegame over
	char name[MAX_OSPATH];
	char name2[MAX_OSPATH];
	Com_sprintf(name,  sizeof(name),  "%s/save/%s/server.ssv", FS_Gamedir(), src);
	Com_sprintf(name2, sizeof(name2), "%s/save/%s/server.ssv", FS_Gamedir(), dst);
	FS_CreatePath(name2);
	FS_CopyFile(name, name2);

	Com_sprintf(name,  sizeof(name),  "%s/save/%s/game.ssv", FS_Gamedir(), src);
	Com_sprintf(name2, sizeof(name2), "%s/save/%s/game.ssv", FS_Gamedir(), dst);
	FS_CopyFile(name, name2);

	// Knightmare- copy screenshot
	if (strcmp(dst, "kmq2save0")) // No screenshot for start of level autosaves
	{
		Com_sprintf(name,  sizeof(name),  "%s/save/%s/shot.jpg", FS_Gamedir(), src);
		Com_sprintf(name2, sizeof(name2), "%s/save/%s/shot.jpg", FS_Gamedir(), dst);
		FS_CopyFile(name, name2);
	}

	Com_sprintf(name, sizeof(name), "%s/save/%s/", FS_Gamedir(), src);
	const int len = strlen(name);
	Com_sprintf(name, sizeof(name), "%s/save/%s/*.sav", FS_Gamedir(), src);
	char *found = Sys_FindFirst(name, 0, 0);
	while (found)
	{
		Q_strncpyz(name + len, found + len, sizeof(name) - len);
		Com_sprintf(name2, sizeof(name2), "%s/save/%s/%s", FS_Gamedir(), dst, found + len);
		FS_CopyFile(name, name2);

		// Change sav to sv2
		int l = strlen(name);
		Q_strncpyz(name + l - 3, "sv2", sizeof(name) - l + 3);
		l = strlen(name2);
		Q_strncpyz(name2 + l - 3, "sv2", sizeof(name2) - l + 3);
		FS_CopyFile(name, name2);

		found = Sys_FindNext(0, 0);
	}

	Sys_FindClose();
}

void SV_WriteLevelFile(void)
{
	Com_DPrintf("SV_WriteLevelFile()\n");

	char name[MAX_OSPATH];
	Com_sprintf(name, sizeof(name), "%s/save/current/%s.sv2", FS_Gamedir(), sv.name);
	FILE *f = fopen(name, "wb");
	if (!f)
	{
		Com_Printf("Failed to open %s\n", name);
		return;
	}

	fwrite(sv.configstrings, sizeof(sv.configstrings), 1, f);
	CM_WritePortalState(f);
	fclose(f);

	Com_sprintf(name, sizeof(name), "%s/save/current/%s.sav", FS_Gamedir(), sv.name);
	ge->WriteLevel(name);
}

extern void CM_ReadPortalState(fileHandle_t f);

void SV_ReadLevelFile()
{
	Com_DPrintf("SV_ReadLevelFile()\n");

	char name[MAX_OSPATH];
	fileHandle_t f;
	Com_sprintf(name, sizeof(name), "save/current/%s.sv2", sv.name);
	FS_FOpenFile(name, &f, FS_READ);
	if (!f)
	{
		Com_Printf("Failed to open %s\n", name);
		return;
	}

	FS_Read(sv.configstrings, sizeof(sv.configstrings), f);
	CM_ReadPortalState(f);
	FS_FCloseFile(f);

	Com_sprintf(name, sizeof(name), "%s/save/current/%s.sav", FS_Gamedir(), sv.name);
	ge->ReadLevel(name);
}

void SV_WriteServerFile(qboolean autosave)
{
	char fileName[MAX_OSPATH];
	char varName[128];
	char string[128];
	char comment[32];
	
	Com_DPrintf("SV_WriteServerFile(%s)\n", autosave ? "true" : "false");

	Com_sprintf(fileName, sizeof(fileName), "%s/save/current/server.ssv", FS_Gamedir());
	FILE *f = fopen(fileName, "wb");
	if (!f)
	{
		Com_Printf("Couldn't write %s\n", fileName);
		return;
	}

	// Write the comment field
	memset(comment, 0, sizeof(comment));
	if (!autosave)
	{
		time_t aclock;
		time(&aclock);
		struct tm *newtime = localtime(&aclock);
		Com_sprintf(comment, sizeof(comment), "%2i:%i%i %2i/%2i  ", newtime->tm_hour, newtime->tm_min / 10, newtime->tm_min % 10, newtime->tm_mon + 1, newtime->tm_mday);
		strncat(comment, sv.configstrings[CS_NAME], sizeof(comment) - 1 - strlen(comment));
	}
	else
	{
		// Autosaved
		Com_sprintf(comment, sizeof(comment), "ENTERING %s", sv.configstrings[CS_NAME]);
	}
	fwrite(comment, 1, sizeof(comment), f);

	// Write the mapcmd
	fwrite(svs.mapcmd, 1, sizeof(svs.mapcmd), f);

	// Write all CVAR_LATCH cvars. These will be things like coop, skill, deathmatch, etc
	for (cvar_t *var = cvar_vars; var; var = var->next)
	{
		if (!(var->flags & CVAR_LATCH) || var->flags & CVAR_SAVE_IGNORE) // latched vars that are not saved (game, etc)
			continue;

		if (strlen(var->name) >= sizeof(varName) - 1 || strlen(var->string) >= sizeof(string) - 1)
		{
			Com_Printf("Cvar too long: %s = %s\n", var->name, var->string);
			continue;
		}

		memset(varName, 0, sizeof(varName));
		memset(string, 0, sizeof(string));
		Q_strncpyz(varName, var->name, sizeof(varName));
		Q_strncpyz(string, var->string, sizeof(string));
		fwrite(varName, 1, sizeof(varName), f);
		fwrite(string, 1, sizeof(string), f);
	}

	fclose(f);

	// Write game state
	Com_sprintf(fileName, sizeof(fileName), "%s/save/current/game.ssv", FS_Gamedir());
	ge->WriteGame(fileName, autosave);
}

void SV_WriteScreenshot(void)
{
	if (dedicated->integer) // can't do this in dedicated mode
		return;

	Com_DPrintf("SV_WriteScreenshot()\n");

	char name[MAX_OSPATH];
	Com_sprintf(name, sizeof(name), "%s/save/current/shot.jpg", FS_Gamedir());
	R_ScaledScreenshot(name);
}

void SV_ReadServerFile(void)
{
	fileHandle_t f;
	char fileName[MAX_OSPATH];
	char comment[32];
	char mapcmd[MAX_TOKEN_CHARS];

	Com_DPrintf("SV_ReadServerFile()\n");

	Com_sprintf(fileName, sizeof(fileName), "save/current/server.ssv");
	FS_FOpenFile(fileName, &f, FS_READ);
	if (!f)
	{
		Com_Printf("%s: couldn't read '%s'\n", __func__, fileName);
		return;
	}

	// Read the comment field
	FS_Read(comment, sizeof(comment), f);

	// Read the mapcmd
	FS_Read(mapcmd, sizeof(mapcmd), f);

	// Read all CVAR_LATCH cvars. These will be things like coop, skill, deathmatch, etc
	while (true)
	{
		char name[128] = { 0 };
		if (!FS_Read(name, sizeof(name), f)) //mxd. Was FS_FRead. Why?
			break;

		char value[128] = { 0 };
		FS_Read(value, sizeof(value), f);

		// Skip first cvar slot in KMQ2 0.21 and later saves (embedded extra save info)
		if (!strncmp(name, "KMQ2SSV", 7))
			continue;

		// Don't load game, basegame, engine name/version, or sys_* cvars from savegames
		if (strcmp(name, "game") && strncmp(name, "basegame", 8)
			&& strncmp(name, "sv_engine", 9) && strncmp(name, "cl_engine", 9)
			&& strncmp(name, "sys_", 4))
		{
			Com_DPrintf("Set '%s' = '%s'\n", name, value);
			Cvar_ForceSet(name, value);
		}
	}

	FS_FCloseFile(f);

	// Start a new game fresh with new cvars
	SV_InitGame();

	Q_strncpyz(svs.mapcmd, mapcmd, sizeof(svs.mapcmd));

	// Read game state
	Com_sprintf(fileName, sizeof(fileName), "%s/save/current/game.ssv", FS_Gamedir());
	ge->ReadGame(fileName);
}

#pragma endregion

#pragma region ======================= DEMOMAP / GAMEMAP / MAP CONSOLE COMMANDS

// Puts the server in demo mode on a specific map/cinematic
void SV_DemoMap_f(void)
{
	//mxd. Usage
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: demomap <map>\n");
		return;
	}
	
	// Knightmare- force off DM, CTF mode
	Cvar_SetValue("ttctf", 0);
	Cvar_SetValue("ctf", 0);
	Cvar_SetValue("deathmatch", 0);

	SV_Map(true, Cmd_Argv(1), false);
}

// Saves the state of the map just being exited and goes to a new map.

// If the initial character of the map string is '*', the next map is
// in a new unit, so the current savegame directory is cleared of map files.

// Example: *inter.cin+jail

// Clears the archived maps, plays the inter.cin cinematic, then goes to map jail.bsp.
void SV_GameMap_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: gamemap <map>\n");
		return;
	}

	Com_DPrintf("SV_GameMap(%s)\n", Cmd_Argv(1));

	FS_CreatePath(va("%s/save/current/", FS_Gamedir()));

	// Check for clearing the current savegame
	char *map = Cmd_Argv(1);
	if (map[0] == '*')
	{
		// Wipe all the *.sav files
		SV_WipeSavegame("current");
	}
	else
	{
		// Save the map just exited
		if (sv.state == ss_game)
		{
			// Clear all the client inuse flags before saving so that when the level is re-entered, 
			// the clients will spawn at spawn points instead of occupying body shells
			qboolean *savedInuse = malloc(maxclients->value * sizeof(qboolean));
			client_t *cl = svs.clients;
			for (int i = 0; i < maxclients->value; i++, cl++)
			{
				savedInuse[i] = cl->edict->inuse;
				cl->edict->inuse = false;
			}

			SV_WriteLevelFile();

			// We must restore these for clients to transfer over correctly
			cl = svs.clients;
			for (int i = 0; i < maxclients->value; i++, cl++)
				cl->edict->inuse = savedInuse[i];

			free(savedInuse);
		}
	}

	// Start up the next map
	SV_Map(false, Cmd_Argv(1), false);

	// Archive server state
	strncpy(svs.mapcmd, Cmd_Argv(1), sizeof(svs.mapcmd) - 1);

	// Copy off the level to the autosave slot
	// Knightmare- don't do this in deathmatch or for cinematics
	char *ext = (char *)COM_FileExtension(map); //mxd
	if (!dedicated->integer && !Cvar_VariableValue("deathmatch") && Q_strcasecmp(ext, "cin") && Q_strcasecmp(ext, "roq") && Q_strcasecmp(ext, "pcx"))
	{
		SV_WriteServerFile(true);
		SV_CopySaveGame("current", "kmq2save0");
	}
}

// Goes directly to a given map without any savegame archiving.
void SV_Map_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: map <mapname>\n");
		return;
	}

	// If not a pcx, demo, or cinematic, check to make sure the level exists
	char *map = Cmd_Argv(1);
	if (!strchr(map, '.')) //mxd. strstr -> strchr
	{
		char expanded[MAX_QPATH];
		Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);

		if (!FS_FileExists(expanded)) //mxd. FS_LoadFile -> FS_FileExists
		{
			Com_Printf("Can't find '%s'\n", expanded);
			return;
		}
	}

	sv.state = ss_dead; // Don't save current level when changing
	SV_WipeSavegame("current");
	SV_GameMap_f();
}

#pragma endregion

#pragma region ======================= SAVEGAMES

extern char *load_saveshot;

void SV_Loadgame_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: load <directory>\n");
		return;
	}

	Com_Printf("Loading game...\n");

	char *dir = Cmd_Argv(1);
	if (strstr(dir, "..") || strchr(dir, '/') || strchr(dir, '\\')) //mxd. strstr -> strchr
	{
		Com_Printf("Invalid savedir path: '%s'.\n", dir);
		return; //mxd
	}

	// Make sure the server.ssv file exists
	char name[MAX_OSPATH];
	Com_sprintf(name, sizeof(name), "%s/save/%s/server.ssv", FS_Gamedir(), Cmd_Argv(1));
	FILE *f = fopen(name, "rb");
	if (!f)
	{
		Com_Printf("No such savegame: %s\n", name);
		return;
	}
	fclose(f);

	// Knightmare- set saveshot name
	if (!dedicated->value && (!strcmp(Cmd_Argv(1), "quick") || !strcmp(Cmd_Argv(1), "quik")))
	{
		static char sv_loadshotname[MAX_QPATH];
		Com_sprintf(sv_loadshotname, sizeof(sv_loadshotname), "/save/%s/shot.jpg", Cmd_Argv(1));
		R_FreePic(sv_loadshotname + 1); //mxd. Gross hacks. Skip leading slash because R_DrawFindPic will also skip it before calling R_FindImage

		load_saveshot = sv_loadshotname;
	}

	SV_CopySaveGame(Cmd_Argv(1), "current");
	SV_ReadServerFile();

	// Go to the map
	sv.state = ss_dead; // Don't save current level when changing
	SV_Map(false, svs.mapcmd, true);
}

extern char fs_gamedir[MAX_OSPATH];
extern int S_GetBackgroundTrackFrame(); //mxd

void SV_Savegame_f(void)
{
	if (sv.state != ss_game)
	{
		Com_Printf("You must be in a game to save.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: savegame <directory>\n");
		return;
	}

	if (Cvar_VariableValue("deathmatch"))
	{
		Com_Printf("Can't save in a deathmatch\n");
		return;
	}

	if (!strcmp(Cmd_Argv(1), "current"))
	{
		Com_Printf("Can't save to 'current'\n");
		return;
	}

	if (maxclients->value == 1 && svs.clients[0].edict->client->ps.stats[STAT_HEALTH] <= 0)
	{
		Com_Printf("Can't save game while dead!\n");
		return;
	}

	char *dir = Cmd_Argv(1);
	if (strstr(dir, "..") || strchr(dir, '/') || strchr(dir, '\\')) //mxd. strstr -> strchr
	{
		Com_Printf("Invalid savedir path: '%s'.\n", dir);
		return; //mxd
	}

	// Knightmare- grab screen for quicksave
	if (!dedicated->value && (!strcmp(Cmd_Argv(1), "quick") || !strcmp(Cmd_Argv(1), "quik")))
		R_GrabScreen();

	//mxd. Store soundtrack position...
	Cvar_FullSet("musictrackframe", va("%i", S_GetBackgroundTrackFrame()), CVAR_LATCH);

	Com_Printf(S_COLOR_CYAN"Saving game \"%s\"...\n", dir);

	// Archive current level, including all client edicts.
	// When the level is reloaded, they will be shells awaiting a connecting client.
	SV_WriteLevelFile();

	// Save server state
	SV_WriteServerFile(false);

	// Take screenshot
	SV_WriteScreenshot();

	// Copy it off
	SV_CopySaveGame("current", dir);

	Com_Printf(S_COLOR_CYAN"Done.\n");
}

#pragma endregion

#pragma region ======================= SERVER CONSOLE COMMANDS

// Kick a user off of the server
void SV_Kick_f(void)
{
	if (!svs.initialized)
	{
		Com_Printf("No server running.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: kick <userid>\n");
		return;
	}

	if (!SV_SetPlayer())
		return;

	SV_BroadcastPrintf(PRINT_HIGH, "%s was kicked\n", sv_client->name);

	// Print directly, because the dropped client won't get the SV_BroadcastPrintf message
	SV_ClientPrintf(sv_client, PRINT_HIGH, "You were kicked from the game\n");
	SV_DropClient(sv_client);
	sv_client->lastmessage = svs.realtime; // In case there is a funny zombie
}

void SV_Status_f()
{
	if (!svs.clients)
	{
		Com_Printf("No server running.\n");
		return;
	}

	Com_Printf("map              : %s\n", sv.name);

	Com_Printf("num score ping name            lastmsg address               qport \n");
	Com_Printf("--- ----- ---- --------------- ------- --------------------- ------\n");

	client_t *cl = svs.clients;
	for (int i = 0; i < maxclients->value; i++, cl++)
	{
		if (!cl->state)
			continue;

		Com_Printf("%3i ", i);
		Com_Printf("%5i ", cl->edict->client->ps.stats[STAT_FRAGS]);

		if (cl->state == cs_connected)
			Com_Printf("CNCT ");
		else if (cl->state == cs_zombie)
			Com_Printf("ZMBI ");
		else
			Com_Printf("%4i ", min(cl->ping, 9999));

		Com_Printf("%s", cl->name);
		int len = 16 - strlen(cl->name);
		for (int j = 0; j < len; j++)
			Com_Printf(" ");

		Com_Printf("%7i ", svs.realtime - cl->lastmessage);

		char *s = NET_AdrToString(cl->netchan.remote_address);
		Com_Printf("%s", s);
		len = 22 - strlen(s);
		for (int j = 0; j < len; j++)
			Com_Printf(" ");
		
		Com_Printf("%5i", cl->netchan.qport);

		Com_Printf("\n");
	}

	Com_Printf("\n");
}

void SV_ConSay_f(void)
{
	if (Cmd_Argc() < 2)
		return;

	char text[1024];
	Q_strncpyz(text, "console: ", sizeof(text));
	char *p = Cmd_Args();

	if (*p == '"')
	{
		p++;
		p[strlen(p) - 1] = 0;
	}

	Q_strncatz(text, p, sizeof(text));

	client_t *client = svs.clients;
	for (int j = 0; j < maxclients->value; j++, client++)
		if (client->state == cs_spawned)
			SV_ClientPrintf(client, PRINT_CHAT, "%s\n", text);
}

void SV_Heartbeat_f(void)
{
	svs.last_heartbeat = -9999999;
}

// Examine or change the serverinfo string
void SV_Serverinfo_f (void)
{
	Com_Printf("Server info settings:\n");
	Info_Print(Cvar_Serverinfo());
}

// Examine all a users info strings
void SV_DumpUser_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: info <userid>\n");
		return;
	}

	if (!SV_SetPlayer())
		return;

	Com_Printf("userinfo\n");
	Com_Printf("--------\n");
	Info_Print(sv_client->userinfo);
}

void SV_StartMod(char *mod)
{
	// killserver, start mod, unbind keys, exec configs, and start demos
	Cbuf_AddText("killserver\n");
	Cbuf_AddText(va("game %s\n", mod));
	Cbuf_AddText("unbindall\n");
	Cbuf_AddText("exec default.cfg\n");
	Cbuf_AddText("exec kmq2config.cfg\n");
	Cbuf_AddText("exec autoexec.cfg\n");
	Cbuf_AddText("d1\n");
}

// Switch to a different mod
void SV_ChangeGame_f(void)
{
	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: changegame <gamedir> : change game directory\n");
		return;
	}

	SV_StartMod(Cmd_Argv(1));
}

// Begins server demo recording.
// Every entity and every message will be recorded, but no playerinfo will be stored. Primarily for demo merging.
void SV_ServerRecord_f(void)
{
	char name[MAX_OSPATH];
	byte buf_data[32768];
	sizebuf_t buf;

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: serverrecord <demoname>\n");
		return;
	}

	if (svs.demofile)
	{
		Com_Printf("Already recording.\n");
		return;
	}

	if (sv.state != ss_game)
	{
		Com_Printf("You must be in a level to record.\n");
		return;
	}

	// Open the demo file
	Com_sprintf(name, sizeof(name), "%s/demos/%s.dm2", FS_Gamedir(), Cmd_Argv(1));

	Com_Printf("recording to \"%s\".\n", name);
	FS_CreatePath(name);
	svs.demofile = fopen(name, "wb");
	if (!svs.demofile)
	{
		Com_Printf("ERROR: couldn't open demo file.\n");
		return;
	}

	// Setup a buffer to catch all multicasts
	SZ_Init(&svs.demo_multicast, svs.demo_multicast_buf, sizeof(svs.demo_multicast_buf));

	// Write a single giant fake message with all the startup info
	SZ_Init(&buf, buf_data, sizeof(buf_data));

	// serverdata needs to go over for all types of servers to make sure the protocol is right, and to set the gamedir

	// Send the serverdata
	MSG_WriteByte(&buf, svc_serverdata);
	MSG_WriteLong(&buf, PROTOCOL_VERSION);
	MSG_WriteLong(&buf, svs.spawncount);
	MSG_WriteByte(&buf, 2); // 2 means server demo. Demos are always attract loops
	MSG_WriteString(&buf, Cvar_VariableString("gamedir"));
	MSG_WriteShort(&buf, -1);
	MSG_WriteString(&buf, sv.configstrings[CS_NAME]); // Send full levelname

	for (int i = 0 ; i < MAX_CONFIGSTRINGS; i++)
	{
		if (sv.configstrings[i][0])
		{
			MSG_WriteByte(&buf, svc_configstring);
			MSG_WriteShort(&buf, i);
			MSG_WriteString(&buf, sv.configstrings[i]);
		}
	}

	// Write it to the demo file
	Com_DPrintf("Signon message length: %i\n", buf.cursize);
	fwrite(&buf.cursize, 4, 1, svs.demofile);
	fwrite(buf.data, buf.cursize, 1, svs.demofile);

	// The rest of the demo file will be individual frames
}

// Ends server demo recording
void SV_ServerStop_f(void)
{
	if (!svs.demofile)
	{
		Com_Printf("Not doing a serverrecord.\n");
		return;
	}

	fclose(svs.demofile);
	svs.demofile = NULL;
	Com_Printf("Recording completed.\n");
}

// Kick everyone off, possibly in preparation for a new game
void SV_KillServer_f(void)
{
	if (svs.initialized)
	{
		SV_Shutdown("Server was killed.\n", false);
		NET_Config(false); // Close network sockets
	}
}

// Lets the game dll handle a command
void SV_ServerCommand_f(void)
{
	if (ge)
		ge->ServerCommand();
	else
		Com_Printf("No game loaded.\n");
}

#pragma endregion

void SV_InitOperatorCommands()
{
	Cmd_AddCommand("heartbeat", SV_Heartbeat_f);
	Cmd_AddCommand("kick", SV_Kick_f);
	Cmd_AddCommand("status", SV_Status_f);
	Cmd_AddCommand("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand("dumpuser", SV_DumpUser_f);

	Cmd_AddCommand("changegame", SV_ChangeGame_f); // Knightmare added

	Cmd_AddCommand("map", SV_Map_f);
	Cmd_AddCommand("demomap", SV_DemoMap_f);
	Cmd_AddCommand("gamemap", SV_GameMap_f);
	Cmd_AddCommand("setmaster", SV_SetMaster_f);

	if (dedicated->value)
		Cmd_AddCommand("say", SV_ConSay_f);

	Cmd_AddCommand("serverrecord", SV_ServerRecord_f);
	Cmd_AddCommand("serverstop", SV_ServerStop_f);

	Cmd_AddCommand("save", SV_Savegame_f);
	Cmd_AddCommand("load", SV_Loadgame_f);

	Cmd_AddCommand("killserver", SV_KillServer_f);

	Cmd_AddCommand("sv", SV_ServerCommand_f);
}