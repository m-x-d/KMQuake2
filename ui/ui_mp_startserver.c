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

// ui_startserver.c -- the start server menu 

#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"

static menuframework_s s_startserver_menu;

static menuaction_s	s_startserver_start_action;
static menuaction_s	s_startserver_dmoptions_action;
static menufield_s	s_timelimit_field;
static menufield_s	s_fraglimit_field;
static menufield_s	s_maxclients_field;
static menufield_s	s_hostname_field;
static menulist_s	s_startmap_list;
menulist_s			s_rules_box;
static menuaction_s	s_startserver_back_action;

#define M_UNSET		0
#define M_MISSING	1
#define M_FOUND		2

static byte *ui_svr_mapshotvalid; // levelshot validity table

typedef enum
{
	MAP_DM,
	MAP_COOP,
	MAP_CTF,
	MAP_3TCTF,
	NUM_MAPTYPES
} maptype_t;

#define MAP_TAG	4 //mxd. Special Rogue maptype requires special rogue handling...

typedef struct
{
	int index;
	char *tokens;
} gametype_names_t;

static gametype_names_t gametype_names[] =
{
	{ MAP_DM, "dm ffa team teamdm" },
	{ MAP_COOP, "coop" },
	{ MAP_CTF, "ctf" },
	{ MAP_3TCTF, "3tctf" },
};

#define MAX_ARENAS		1024
#define MAX_ARENAS_TEXT	8192

static maptype_t ui_svr_maptype;
static int	ui_svr_nummaps;
static char	**ui_svr_mapnames;
static int	ui_svr_listfile_nummaps;
static char	**ui_svr_listfile_mapnames;
static int	ui_svr_arena_nummaps[NUM_MAPTYPES];
static char	**ui_svr_arena_mapnames[NUM_MAPTYPES];

#pragma region ======================= Persistent settings

//mxd. Saving changes to cvars when closing the menu seems to mess server state
#define HOSTNAME_FIELD_LENGTH 12

static int ps_timelimit;
static int ps_fraglimit;
static int ps_maxclients;
static int ps_currentrule;
static char ps_hostname[HOSTNAME_FIELD_LENGTH];

// Called from StartServer_MenuInit
static void InitializePersistentSettings()
{
	static qboolean ps_initialized;

	if (!ps_initialized)
	{
		ps_timelimit = Cvar_VariableInteger("timelimit");
		ps_fraglimit = Cvar_VariableInteger("fraglimit");

		ps_maxclients = Cvar_VariableInteger("maxclients");
		if (ps_maxclients == 1)
			ps_maxclients = 8;

		Q_strncpyz(ps_hostname, Cvar_VariableString("hostname"), sizeof(ps_hostname));

		// Determine current game mode from assortment of flags...
		if (Cvar_VariableInteger("ttctf"))
			ps_currentrule = MAP_3TCTF;
		else if (Cvar_VariableInteger("ctf"))
			ps_currentrule = MAP_CTF;
		else if (FS_RoguePath() && Cvar_VariableInteger("gamerules") == 2)
			ps_currentrule = MAP_TAG;
		else if (Cvar_VariableInteger("coop"))
			ps_currentrule = MAP_COOP;
		else
			ps_currentrule = MAP_DM;

		ps_initialized = true;
	}
}

// Called when closing the menu or starting the server
static void SavePersistentSettings()
{
	ps_maxclients = atoi(s_maxclients_field.buffer);
	ps_timelimit = atoi(s_timelimit_field.buffer);
	ps_fraglimit = atoi(s_fraglimit_field.buffer);
	ps_currentrule = s_rules_box.curvalue;
	Q_strncpyz(ps_hostname, s_hostname_field.buffer, sizeof(ps_hostname));
}

#pragma endregion

#pragma region ======================= .arena files support

static qboolean UI_ParseArenaFromFile(char *filename, char *shortname, char *longname, char *gametypes, size_t bufSize)
{
	fileHandle_t f;
	char buf[MAX_ARENAS_TEXT];

	const int len = FS_FOpenFile(filename, &f, FS_READ);
	if (!f)
	{
		Com_Printf(S_COLOR_RED "UI_ParseArenaFromFile: file not found: %s\n", filename);

		return false;
	}

	if (len >= MAX_ARENAS_TEXT)
	{
		Com_Printf(S_COLOR_RED "UI_ParseArenaFromFile: file too large: %s is %i, max allowed is %i", filename, len, MAX_ARENAS_TEXT);
		FS_FCloseFile (f);

		return false;
	}

	FS_Read(buf, len, f);
	buf[len] = 0;
	FS_FCloseFile(f);

	char* s = buf;

	// Get the opening curly brace
	char* token = COM_Parse(&s);
	if (!token)
	{
		Com_Printf("UI_ParseArenaFromFile: unexpected EOF\n");
		return false;
	}

	if (token[0] != '{')
	{
		Com_Printf("UI_ParseArenaFromFile: found %s when expecting {\n", token);
		return false;
	}

	// Go through all the parms
	while (s < buf + len)
	{
		char* dest = NULL;
		token = COM_Parse(&s);

		if (token && token[0] == '}')
			break;

		if (!token || !s)
		{
			Com_Printf("UI_ParseArenaFromFile: EOF without closing brace\n");
			break;
		}

		if (!Q_strcasecmp(token, "map"))
			dest = shortname;
		else if (!Q_strcasecmp(token, "longname"))
			dest = longname;
		else if (!Q_strcasecmp(token, "type"))
			dest = gametypes;

		if (dest)
		{
			token = COM_Parse(&s);

			if (!token)
			{
				Com_Printf("UI_ParseArenaFromFile: unexpected EOF\n");
				return false;
			}

			if (token[0] == '}')
			{
				Com_Printf("UI_ParseArenaFromFile: closing brace without data\n");
				break;
			}

			if (!s)
			{
				Com_Printf("UI_ParseArenaFromFile: EOF without closing brace\n");
				break;
			}

			Q_strncpyz(dest, token, bufSize);
		}
	}

	if (!shortname || !strlen(shortname))
	{
		Com_Printf(S_COLOR_RED "UI_ParseArenaFromFile: %s: map field not found\n", filename);
		return false;
	}

	if (!strlen(longname))
		strcpy(longname, shortname);

	return true;
}

static void UI_SortArenas(char **list, int len)
{
	if (!list || len < 2)
		return;

	for (int i = len - 1; i > 0; i--)
	{
		qboolean moved = false;

		for (int j = 0; j < i; j++)
		{
			if (!list[j])
				break;

			char* s1 = strchr(list[j], '\n') + 1;
			char* s2 = strchr(list[j + 1], '\n') + 1;

			if (Q_stricmp(s1, s2) > 0)
			{
				char* temp = list[j];
				list[j] = list[j + 1];
				list[j + 1] = temp;

				moved = true;
			}
		}

		if (!moved)
			break; // Done sorting
	}
}

static void UI_LoadArenas(void)
{
	int narenas = 0;
	int narenanames = 0;
	qboolean type_supported[NUM_MAPTYPES];

	// Free existing lists and malloc new ones
	for (int i = 0; i < NUM_MAPTYPES; i++)
	{
		if (ui_svr_arena_mapnames[i])
			FS_FreeFileList(ui_svr_arena_mapnames[i], ui_svr_arena_nummaps[i]);

		ui_svr_arena_nummaps[i] = 0;
		ui_svr_arena_mapnames[i] = malloc(sizeof(char *) * MAX_ARENAS);
		memset(ui_svr_arena_mapnames[i], 0, sizeof(char *) * MAX_ARENAS);
	}

	char **tmplist = malloc(sizeof(char *) * MAX_ARENAS);
	memset(tmplist, 0, sizeof(char *) * MAX_ARENAS);

	// Check in paks for .arena files
	char** arenafiles = FS_ListPak("scripts/", &narenas);
	if (arenafiles)
	{
		for (int i = 0; i < narenas && narenanames < MAX_ARENAS; i++)
		{
			if (!arenafiles[i])
				continue;

			char *p = arenafiles[i];

			if (!strstr(p, ".arena"))
				continue;

			if (!FS_ItemInList(p, narenanames, tmplist)) // Check if already in list
			{
				char shortname[MAX_TOKEN_CHARS];
				char longname[MAX_TOKEN_CHARS];
				char gametypes[MAX_TOKEN_CHARS];
				char scratch[200];

				if (UI_ParseArenaFromFile(p, shortname, longname, gametypes, MAX_TOKEN_CHARS))
				{
					Com_sprintf(scratch, sizeof(scratch), "%s\n%s", longname, shortname);
					
					for (int j = 0; j < NUM_MAPTYPES; j++)
						type_supported[j] = false;

					char *s = gametypes;
					char *tok = strdup(COM_Parse(&s));

					while (s != NULL)
					{
						for (int j = 0; j < NUM_MAPTYPES; j++)
						{
							char *s2 = gametype_names[j].tokens;
							char *tok2 = COM_Parse(&s2);

							while (s2 != NULL)
							{
								if (!Q_strcasecmp(tok, tok2))
									type_supported[j] = true;
								tok2 = COM_Parse(&s2);
							}
						}

						free(tok);
						tok = strdup(COM_Parse(&s));
					}

					free(tok);

					const size_t scratchlen = strlen(scratch) + 1; //mxd. V814 Decreased performance. The 'strlen' function was called multiple times inside the body of a loop.
					for (int j = 0; j < NUM_MAPTYPES; j++)
					{
						if (type_supported[j])
						{
							ui_svr_arena_mapnames[j][ui_svr_arena_nummaps[j]] = malloc(scratchlen);
							Q_strncpyz(ui_svr_arena_mapnames[j][ui_svr_arena_nummaps[j]], scratch, scratchlen);
							ui_svr_arena_nummaps[j]++;
						}
					}

					narenanames++;
					FS_InsertInList(tmplist, strdup(p), narenanames, 0); // Add to list
				}
			}
		}
	}

	if (narenas)
		FS_FreeFileList(arenafiles, narenas);

	FS_FreeFileList(tmplist, MAX_ARENAS); //mxd. Free unconditionally

	for (int i = 0; i < NUM_MAPTYPES; i++)
		UI_SortArenas(ui_svr_arena_mapnames[i], ui_svr_arena_nummaps[i]);
}

#pragma endregion

#pragma region ======================= Map list handling

void UI_LoadMapList(void)
{
	// Free existing list
	if (ui_svr_listfile_mapnames)
		FS_FreeFileList(ui_svr_listfile_mapnames, ui_svr_listfile_nummaps);
	ui_svr_listfile_nummaps = 0;

	// Load the list of map names
	char mapsname[1024];
	Com_sprintf(mapsname, sizeof(mapsname), "%s/maps.lst", FS_Gamedir());

	char *buffer;
	const int length = FS_LoadFile("maps.lst", (void **)&buffer);
	if (length == -1)
		Com_Error(ERR_DROP, "%s: couldn't find maps.lst\n", __func__);

	char* s = buffer;

	for(int i = 0; i < length; i++)
		if (s[i] == '\r')
			ui_svr_listfile_nummaps++;

	if (ui_svr_listfile_nummaps == 0)
	{
		// Hack in a default map list
		ui_svr_listfile_nummaps = 1;
		buffer = "base1 \"Outer Base\"\n";
	}

	ui_svr_listfile_mapnames = malloc(sizeof(char *) * (ui_svr_listfile_nummaps + 1));
	memset(ui_svr_listfile_mapnames, 0, sizeof(char *) * (ui_svr_listfile_nummaps + 1));

	s = buffer;

	for (int i = 0; i < ui_svr_listfile_nummaps; i++)
	{
		char shortname[MAX_TOKEN_CHARS];
		char longname[MAX_TOKEN_CHARS];
		char scratch[200];

		Q_strncpyz(shortname, COM_Parse(&s), sizeof(shortname));
		Q_strncpyz(longname, COM_Parse(&s), sizeof(longname));
		Com_sprintf(scratch, sizeof(scratch), "%s\n%s", longname, shortname);

		ui_svr_listfile_mapnames[i] = malloc(strlen(scratch) + 1);
		Q_strncpyz(ui_svr_listfile_mapnames[i], scratch, strlen(scratch) + 1);
	}

	ui_svr_listfile_mapnames[ui_svr_listfile_nummaps] = 0;

	FS_FreeFile(buffer);

	UI_LoadArenas();
	ui_svr_maptype = MAP_DM; // Initial maptype
}

static void UI_BuildMapList(maptype_t maptype)
{
	free(ui_svr_mapnames);

	ui_svr_nummaps = ui_svr_listfile_nummaps + ui_svr_arena_nummaps[maptype];
	ui_svr_mapnames = malloc(sizeof(char *) * (ui_svr_nummaps + 1));
	memset(ui_svr_mapnames, 0, sizeof(char *) * (ui_svr_nummaps + 1));

	for (int i = 0; i < ui_svr_nummaps; i++)
	{
		if (i < ui_svr_listfile_nummaps)
			ui_svr_mapnames[i] = ui_svr_listfile_mapnames[i];
		else
			ui_svr_mapnames[i] = ui_svr_arena_mapnames[maptype][i - ui_svr_listfile_nummaps];
	}

	ui_svr_mapnames[ui_svr_nummaps] = 0;
	ui_svr_maptype = maptype;

	if (s_startmap_list.curvalue >= ui_svr_nummaps) // paranoia
		s_startmap_list.curvalue = 0;
}

static void UI_RefreshMapList(maptype_t maptype)
{
	if (maptype == ui_svr_maptype) // No change
		return;

	// Reset startmap if it's in the part of the list that changed
	if (s_startmap_list.curvalue >= ui_svr_listfile_nummaps)
		s_startmap_list.curvalue = 0;

	UI_BuildMapList(maptype);
	s_startmap_list.itemnames = ui_svr_mapnames;

	int numitemnames = 0;
	while (s_startmap_list.itemnames[numitemnames])
		numitemnames++;

	s_startmap_list.numitemnames = numitemnames;

	// Levelshot found table
	free(ui_svr_mapshotvalid);

	ui_svr_mapshotvalid = malloc(sizeof(byte) * (ui_svr_nummaps + 1));
	memset(ui_svr_mapshotvalid, 0, sizeof(byte) * (ui_svr_nummaps + 1));
	
	// Register null levelshot
	if (ui_svr_mapshotvalid[ui_svr_nummaps] == M_UNSET)
	{
		if (R_DrawFindPic("/gfx/ui/noscreen.pcx"))
			ui_svr_mapshotvalid[ui_svr_nummaps] = M_FOUND;
		else
			ui_svr_mapshotvalid[ui_svr_nummaps] = M_MISSING;
	}
}

#pragma endregion 

#pragma region ======================= Menu item callbacks

static void DMOptionsFunc(void *self)
{
	if (s_rules_box.curvalue != 1)
		M_Menu_DMOptions_f();
}

static void RulesChangeFunc(void *self)
{
	s_maxclients_field.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.statusbar = NULL;
	
	if (s_rules_box.curvalue == MAP_DM)
	{
		if (atoi(s_maxclients_field.buffer) < 8) // Set default of 8
		{
			Q_strncpyz(s_maxclients_field.buffer, "8", sizeof(s_maxclients_field.buffer));
			s_maxclients_field.cursor = strlen(s_maxclients_field.buffer); //mxd. Re-position the cursor
		}

		UI_RefreshMapList(MAP_DM);
	}
	else if (s_rules_box.curvalue == MAP_COOP)
	{
		s_maxclients_field.generic.statusbar = "4 maximum for cooperative";

		if (atoi(s_maxclients_field.buffer) > 4)
		{
			Q_strncpyz(s_maxclients_field.buffer, "4", sizeof(s_maxclients_field.buffer));
			s_maxclients_field.cursor = strlen(s_maxclients_field.buffer); //mxd. Re-position the cursor
		}

		s_startserver_dmoptions_action.generic.statusbar = "N/A for cooperative";
		UI_RefreshMapList(MAP_COOP);
	}
	else if (s_rules_box.curvalue == MAP_CTF)
	{
		if (atoi(s_maxclients_field.buffer) < 12) // Set default of 12
		{
			Q_strncpyz(s_maxclients_field.buffer, "12", sizeof(s_maxclients_field.buffer));
			s_maxclients_field.cursor = strlen(s_maxclients_field.buffer); //mxd. Re-position the cursor
		}

		UI_RefreshMapList(MAP_CTF);
	}
	else if (s_rules_box.curvalue == MAP_3TCTF)
	{
		if (atoi(s_maxclients_field.buffer) < 18) // Set default of 18
		{
			Q_strncpyz(s_maxclients_field.buffer, "18", sizeof(s_maxclients_field.buffer));
			s_maxclients_field.cursor = strlen(s_maxclients_field.buffer); //mxd. Re-position the cursor
		}

		UI_RefreshMapList(MAP_3TCTF);
	}
	// ROGUE GAMES
	else if (FS_RoguePath() && s_rules_box.curvalue == MAP_TAG)
	{
		if (atoi(s_maxclients_field.buffer) < 8) // Set default of 8
		{
			Q_strncpyz(s_maxclients_field.buffer, "8", sizeof(s_maxclients_field.buffer));
			s_maxclients_field.cursor = strlen(s_maxclients_field.buffer); //mxd. Re-position the cursor
		}

		UI_RefreshMapList(MAP_DM);
	}
}

static void StartServerActionFunc(void *self)
{
	char startmap[MAX_PATH]; //mxd. Was 1024. Why?..
	Q_strncpyz(startmap, strchr(ui_svr_mapnames[s_startmap_list.curvalue], '\n') + 1, sizeof(startmap));

	const int maxclients = atoi(s_maxclients_field.buffer);
	const int timelimit  = atoi(s_timelimit_field.buffer);
	const int fraglimit  = atoi(s_fraglimit_field.buffer);

	Cvar_SetInteger("maxclients", max(0, maxclients));
	Cvar_SetInteger("timelimit", max(0, timelimit));
	Cvar_SetInteger("fraglimit", max(0, fraglimit));
	Cvar_Set("hostname", s_hostname_field.buffer);

	Cvar_SetInteger("deathmatch", s_rules_box.curvalue != MAP_COOP);
	Cvar_SetInteger("coop", s_rules_box.curvalue == MAP_COOP);
	Cvar_SetInteger("ctf", s_rules_box.curvalue == MAP_CTF);
	Cvar_SetInteger("ttctf", s_rules_box.curvalue == MAP_3TCTF);
	Cvar_SetInteger("gamerules", FS_RoguePath() ? ((s_rules_box.curvalue == MAP_TAG) ? MAP_CTF : MAP_DM) : MAP_DM);

	SavePersistentSettings(); //mxd

	char* spot = NULL;
	if (s_rules_box.curvalue == MAP_COOP) // PGM
	{
		if(Q_stricmp(startmap, "bunk1") == 0 || Q_stricmp(startmap, "mintro") == 0 || Q_stricmp(startmap, "fact1") == 0)
			spot = "start";
		else if(Q_stricmp(startmap, "power1") == 0)
			spot = "pstart";
		else if(Q_stricmp(startmap, "biggun") == 0)
			spot = "bstart";
		else if(Q_stricmp(startmap, "hangar1") == 0 || Q_stricmp(startmap, "city1") == 0)
			spot = "unitstart";
		else if(Q_stricmp(startmap, "boss1") == 0)
			spot = "bosstart";
	}

	if (spot)
	{
		if (Com_ServerState())
			Cbuf_AddText("disconnect\n");

		Cbuf_AddText(va("gamemap \"*%s$%s\"\n", startmap, spot));
	}
	else
	{
		Cbuf_AddText(va("map %s\n", startmap));
	}

	UI_ForceMenuOff();
}

//mxd
static void StartServerBackFunc(void *unused)
{
	SavePersistentSettings();
	UI_BackMenu(unused);
}

#pragma endregion

static void StartServer_MenuInit(void)
{
	static const char *dm_coop_names[]		 = { "Deathmatch", "Cooperative", "CTF", "3Team CTF", 0 };
	static const char *dm_coop_names_rogue[] = { "Deathmatch", "Cooperative", "CTF", "3Team CTF", "Tag", 0 };

	int y = MENU_LINE_SIZE * 2; //mxd. Was 0
	
	UI_BuildMapList(ui_svr_maptype); // was MAP_DM

	// Levelshot found table
	free(ui_svr_mapshotvalid);

	ui_svr_mapshotvalid = malloc(sizeof(byte) * (ui_svr_nummaps + 1));
	memset(ui_svr_mapshotvalid, 0, sizeof(byte) * (ui_svr_nummaps + 1));
	
	// Register null levelshot
	if (ui_svr_mapshotvalid[ui_svr_nummaps] == M_UNSET)
	{
		if (R_DrawFindPic("/gfx/ui/noscreen.pcx"))
			ui_svr_mapshotvalid[ui_svr_nummaps] = M_FOUND;
		else
			ui_svr_mapshotvalid[ui_svr_nummaps] = M_MISSING;
	}

	//mxd. Initialize persistent settings
	InitializePersistentSettings();

	// Initialize the menu stuff
	s_startserver_menu.x = SCREEN_WIDTH * 0.5f - 140;
	s_startserver_menu.y = DEFAULT_MENU_Y; //mxd. Was unset
	s_startserver_menu.nitems = 0;

	s_startmap_list.generic.type	= MTYPE_SPINCONTROL;
	s_startmap_list.generic.x		= 0;
	s_startmap_list.generic.y		= y;
	s_startmap_list.generic.name	= "Initial map";
	s_startmap_list.itemnames		= ui_svr_mapnames;

	s_rules_box.generic.type	= MTYPE_SPINCONTROL;
	s_rules_box.generic.x		= 0;
	s_rules_box.generic.y		= y += 3 * MENU_LINE_SIZE;
	s_rules_box.generic.name	= "Rules";
	s_rules_box.itemnames		= (FS_RoguePath() ? dm_coop_names_rogue : dm_coop_names); //PGM - rogue games only available with rogue DLL.
	s_rules_box.curvalue		= ps_currentrule; //mxd
	s_rules_box.generic.callback = RulesChangeFunc;

	s_timelimit_field.generic.type		= MTYPE_FIELD;
	s_timelimit_field.generic.name		= "Time limit";
	s_timelimit_field.generic.flags		= QMF_NUMBERSONLY;
	s_timelimit_field.generic.x			= 0;
	s_timelimit_field.generic.y			= y += 2 * MENU_FONT_SIZE;
	s_timelimit_field.generic.statusbar	= "0 - no limit";
	s_timelimit_field.length			= 4;
	s_timelimit_field.visible_length	= 5; //mxd. Add space to draw cursor
	Q_snprintfz(s_timelimit_field.buffer, sizeof(s_timelimit_field.buffer), "%i", ps_timelimit); //mxd
	s_timelimit_field.cursor			= strlen(s_timelimit_field.buffer);

	s_fraglimit_field.generic.type		= MTYPE_FIELD;
	s_fraglimit_field.generic.name		= "Frag limit";
	s_fraglimit_field.generic.flags		= QMF_NUMBERSONLY;
	s_fraglimit_field.generic.x			= 0;
	s_fraglimit_field.generic.y			= y += 2.25f * MENU_FONT_SIZE;
	s_fraglimit_field.generic.statusbar	= "0 - no limit";
	s_fraglimit_field.length			= 4;
	s_fraglimit_field.visible_length	= 5; //mxd. Add space to draw cursor
	Q_snprintfz(s_fraglimit_field.buffer, sizeof(s_fraglimit_field.buffer), "%i", ps_fraglimit); //mxd
	s_fraglimit_field.cursor			= strlen(s_fraglimit_field.buffer);

	// maxclients determines the maximum number of players that can join the game.
	// If maxclients is only "1" then we should default the menu
	// option to 8 players, otherwise use whatever it's current value is. 
	// Clamping will be done when the server is actually started.
	s_maxclients_field.generic.type			= MTYPE_FIELD;
	s_maxclients_field.generic.name			= "Maximum players";
	s_maxclients_field.generic.flags		= QMF_NUMBERSONLY;
	s_maxclients_field.generic.x			= 0;
	s_maxclients_field.generic.y			= y += 2.25f * MENU_FONT_SIZE;
	s_maxclients_field.generic.statusbar	= NULL;
	s_maxclients_field.length				= 3;
	s_maxclients_field.visible_length		= 4; //mxd. Add space to draw cursor
	Q_snprintfz(s_maxclients_field.buffer, sizeof(s_maxclients_field.buffer), "%i", ps_maxclients); //mxd
	s_maxclients_field.cursor				= strlen(s_maxclients_field.buffer);

	s_hostname_field.generic.type			= MTYPE_FIELD;
	s_hostname_field.generic.name			= "Hostname";
	s_hostname_field.generic.flags			= 0;
	s_hostname_field.generic.x				= 0;
	s_hostname_field.generic.y				= y += 2.25f * MENU_FONT_SIZE;
	s_hostname_field.generic.statusbar		= NULL;
	s_hostname_field.length					= HOSTNAME_FIELD_LENGTH;
	s_hostname_field.visible_length			= HOSTNAME_FIELD_LENGTH + 1; //mxd. Add space to draw cursor
	Q_strncpyz(s_hostname_field.buffer, ps_hostname, sizeof(s_hostname_field.buffer)); //mxd
	s_hostname_field.cursor					= strlen(s_hostname_field.buffer);

	s_startserver_dmoptions_action.generic.type			= MTYPE_ACTION;
	s_startserver_dmoptions_action.generic.name			= "Deathmatch flags";
	s_startserver_dmoptions_action.generic.flags		= QMF_LEFT_JUSTIFY;
	s_startserver_dmoptions_action.generic.x			= -MENU_FONT_SIZE * strlen(s_startserver_dmoptions_action.generic.name); //mxd. Was3 * MENU_FONT_SIZE;
	s_startserver_dmoptions_action.generic.y			= y += 4 * MENU_FONT_SIZE;
	s_startserver_dmoptions_action.generic.statusbar	= NULL;
	s_startserver_dmoptions_action.generic.callback		= DMOptionsFunc;

	s_startserver_start_action.generic.type		= MTYPE_ACTION;
	s_startserver_start_action.generic.name		= "Begin";
	s_startserver_start_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_startserver_start_action.generic.x		= -MENU_FONT_SIZE * strlen(s_startserver_start_action.generic.name); //mxd. Was 3 * MENU_FONT_SIZE;
	s_startserver_start_action.generic.y		= y += 2 * MENU_LINE_SIZE;
	s_startserver_start_action.generic.callback	= StartServerActionFunc;

	s_startserver_back_action.generic.type		= MTYPE_ACTION;
	s_startserver_back_action.generic.name		= (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_MULTIPLAYER); //mxd
	s_startserver_back_action.generic.flags		= QMF_LEFT_JUSTIFY;
	s_startserver_back_action.generic.x			= UI_CenteredX(&s_startserver_back_action.generic, s_startserver_menu.x); //mxd. Draw centered
	s_startserver_back_action.generic.y			= y += 6 * MENU_LINE_SIZE;
	s_startserver_back_action.generic.callback	= StartServerBackFunc; //mxd. Let's accept changes. Was UI_BackMenu;

	Menu_AddItem(&s_startserver_menu, &s_startmap_list);
	Menu_AddItem(&s_startserver_menu, &s_rules_box);
	Menu_AddItem(&s_startserver_menu, &s_timelimit_field);
	Menu_AddItem(&s_startserver_menu, &s_fraglimit_field);
	Menu_AddItem(&s_startserver_menu, &s_maxclients_field);
	Menu_AddItem(&s_startserver_menu, &s_hostname_field);
	Menu_AddItem(&s_startserver_menu, &s_startserver_dmoptions_action);
	Menu_AddItem(&s_startserver_menu, &s_startserver_start_action);
	Menu_AddItem(&s_startserver_menu, &s_startserver_back_action);

	Menu_Center(&s_startserver_menu);

	// Call this now to set proper inital state
	RulesChangeFunc(NULL);
}

static void DrawStartSeverLevelshot(void)
{
	char startmap[MAX_QPATH];
	char mapshotname[MAX_QPATH];
	const int i = s_startmap_list.curvalue;
	const int x = SCREEN_WIDTH / 2 + 44;
	const int y = DEFAULT_MENU_Y + MENU_LINE_SIZE; //mxd
	Q_strncpyz(startmap, strchr(ui_svr_mapnames[i], '\n') + 1, sizeof(startmap));

	SCR_DrawFill(x, y, 244, 184, ALIGN_CENTER, 60, 60, 60, 255);

	if (ui_svr_mapshotvalid[i] == M_UNSET)
	{
		// Init levelshot
		Com_sprintf(mapshotname, sizeof(mapshotname), "/levelshots/%s.pcx", startmap);
		if (R_DrawFindPic(mapshotname))
			ui_svr_mapshotvalid[i] = M_FOUND;
		else
			ui_svr_mapshotvalid[i] = M_MISSING;
	}

	if (ui_svr_mapshotvalid[i] == M_FOUND)
	{
		Com_sprintf(mapshotname, sizeof(mapshotname), "/levelshots/%s.pcx", startmap);
		SCR_DrawPic(x + 2, y + 2, 240, 180, ALIGN_CENTER, mapshotname, 1.0f);
	}
	else if (ui_svr_mapshotvalid[ui_svr_nummaps] == M_FOUND)
	{
		SCR_DrawPic(x + 2, y + 2, 240, 180, ALIGN_CENTER, "/gfx/ui/noscreen.pcx", 1.0f);
	}
	else
	{
		SCR_DrawFill(x + 2, y + 2, 240, 180, ALIGN_CENTER, 0, 0, 0, 255);
	}
}

static void StartServer_MenuDraw(void)
{
	Menu_DrawBanner("m_banner_start_server"); // Knightmare added
	Menu_Draw(&s_startserver_menu);
	DrawStartSeverLevelshot(); // Added levelshots
}

static const char *StartServer_MenuKey(int key)
{
	if (key == K_ESCAPE) //mxd
		SavePersistentSettings();
	
	return Default_MenuKey(&s_startserver_menu, key);
}

void M_Menu_StartServer_f(void)
{
	StartServer_MenuInit();
	UI_PushMenu(StartServer_MenuDraw, StartServer_MenuKey);
}