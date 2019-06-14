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

// ui_game_saveload.c -- the single save/load menus

#include <ctype.h>
#include <sys/stat.h>
#include "../client/client.h"
#include "ui_local.h"

#define	MAX_SAVEGAMES	21 // was 15

static menuframework_s	s_loadgame_menu;
static menuaction_s		s_loadgame_actions[MAX_SAVEGAMES];
static menuaction_s		s_loadgame_back_action;

static menuframework_s	s_savegame_menu;
static menuaction_s		s_savegame_actions[MAX_SAVEGAMES];
static menuaction_s		s_savegame_back_action;


#pragma region ======================= SAVESHOT HANDLING

static char		m_savestrings[MAX_SAVEGAMES][32];
static qboolean	m_savevalid[MAX_SAVEGAMES];
static time_t	m_savetimestamps[MAX_SAVEGAMES];
static qboolean	m_savechanged[MAX_SAVEGAMES];
static qboolean	m_saveshotvalid[MAX_SAVEGAMES + 1];

static char		m_mapname[MAX_QPATH];

static void Load_Savestrings(qboolean update)
{
	fileHandle_t f;
	char name[MAX_OSPATH];
	char mapname[MAX_TOKEN_CHARS];
	char *ch;
	struct stat st;

	for (int i = 0; i < MAX_SAVEGAMES; i++)
	{
		Com_sprintf(name, sizeof(name), "%s/save/kmq2save%i/server.ssv", FS_Gamedir(), i);

		const time_t old_timestamp = m_savetimestamps[i];
		stat(name, &st);
		m_savetimestamps[i] = st.st_mtime;

		// Doesn't need to be refreshed
		if (update && m_savetimestamps[i] == old_timestamp)
		{
			m_savechanged[i] = false;
			continue;
		}

		FILE* fp = fopen(name, "rb");
		if (!fp)
		{
			Q_strncpyz(m_savestrings[i], "<EMPTY>", sizeof(m_savestrings[i]));
			m_savevalid[i] = false;
			m_savetimestamps[i] = 0;
		}
		else
		{
			fclose(fp);
			Com_sprintf(name, sizeof(name), "save/kmq2save%i/server.ssv", i);
			FS_FOpenFile(name, &f, FS_READ);
			
			if (!f)
			{
				Q_strncpyz(m_savestrings[i], "<EMPTY>", sizeof(m_savestrings[i]));
				m_savevalid[i] = false;
				m_savetimestamps[i] = 0;
			}
			else
			{
				FS_Read(m_savestrings[i], sizeof(m_savestrings[i]), f);

				if (i == 0)
				{
					// Grab mapname
					FS_Read(mapname, sizeof(mapname), f);
					if (mapname[0] == '*') // Skip * marker
						Com_sprintf(m_mapname, sizeof(m_mapname), mapname + 1);
					else
						Com_sprintf(m_mapname, sizeof(m_mapname), mapname);

					if ((ch = strchr(m_mapname, '$')))
						*ch = 0; // Terminate string at $ marker
				}

				FS_FCloseFile(f);
				m_savevalid[i] = true;
			}
		}

		m_savechanged[i] = (m_savetimestamps[i] != old_timestamp);
	}
}

static void ValidateSaveshots(void)
{
	char shotname[MAX_QPATH];

	for (int i = 0; i < MAX_SAVEGAMES; i++)
	{
		if (!m_savechanged[i]) // Doesn't need to be reloaded
			continue;

		if (m_savevalid[i])
		{
			if (i == 0)
			{
				Com_sprintf(shotname, sizeof(shotname), "/levelshots/%s.pcx", m_mapname);
			}
			else
			{
				// Free previously loaded shots
				Com_sprintf(shotname, sizeof(shotname), "/save/kmq2save%i/shot.jpg", i);
				R_FreePic(shotname + 1); //mxd. Gross hacks. Skip leading slash because R_DrawFindPic will also skip it before calling R_FindImage
			}

			m_saveshotvalid[i] = (R_DrawFindPic(shotname) != NULL);
		}
		else
		{
			m_saveshotvalid[i] = false;
		}
	}
}

static void UI_UpdateSavegameData(void)
{
	Load_Savestrings(true);
	ValidateSaveshots(); // Register saveshots
}

void UI_InitSavegameData(void)
{
	for (int i = 0; i < MAX_SAVEGAMES; i++)
	{
		m_savetimestamps[i] = 0;
		m_savechanged[i] = true;
	}

	Load_Savestrings(false);
	ValidateSaveshots(); // Register saveshots

	// Register null saveshot, this is only done once
	m_saveshotvalid[MAX_SAVEGAMES] = (R_DrawFindPic("/gfx/ui/noscreen.pcx") != NULL);
}

static void DrawSaveshot(qboolean loadmenu)
{
	// Get save/load game slot index
	int slotindex;

	if (loadmenu)
		slotindex = s_loadgame_actions[s_loadgame_menu.cursor].generic.localdata[0];
	else
		slotindex = s_savegame_actions[s_savegame_menu.cursor].generic.localdata[0];

	//mxd. Get preview image path, if any
	char shotname[MAX_QPATH];
	if (loadmenu && slotindex == 0 && m_savevalid[slotindex] && m_saveshotvalid[slotindex]) // 1st item in the Load menu is "ENTERING map XXX", which can't have a saveshot
		Com_sprintf(shotname, sizeof(shotname), "/levelshots/%s.pcx", m_mapname);
	else if (m_savevalid[slotindex] && m_saveshotvalid[slotindex])
		Com_sprintf(shotname, sizeof(shotname), "/save/kmq2save%i/shot.jpg", slotindex);
	else if (m_saveshotvalid[MAX_SAVEGAMES])
		Com_sprintf(shotname, sizeof(shotname), "/gfx/ui/noscreen.pcx");
	else
		shotname[0] = 0;

	const int x = SCREEN_WIDTH / 2 + 44;
	const int y = DEFAULT_MENU_Y + 2;

	//mxd. Default to 4x3 preview
	int preview_width = 240;
	int preview_height = 180;

	//mxd. If we have an image, check aspect ratio...
	if(shotname[0])
	{
		int w, h;
		R_DrawGetPicSize(&w, &h, shotname);
		if (w != h) // Draw KMQ's square levelshots using 4x3 aspect...
		{
			if((float)w / h > (float)viddef.width / viddef.height) // Widescreen image and 4x3 resolution
				preview_height = preview_width * h / w;
			else // 4x3 image or widescreen image and resolution
				preview_width = preview_height * w / h;
		}

		SCR_DrawFill(x, y, preview_width + 4, preview_height + 4, ALIGN_CENTER, 60, 60, 60, 255); // Gray
		SCR_DrawPic(x + 2, y + 2, preview_width, preview_height, ALIGN_CENTER, shotname, 1.0f);
	}
	else
	{
		SCR_DrawFill(x, y, preview_width + 4, preview_height + 4, ALIGN_CENTER, 60, 60, 60, 255); // Gray
		SCR_DrawFill(x + 2, y + 2, preview_width, preview_height, ALIGN_CENTER, 0, 0, 0, 255); // Black
	}
}

#pragma endregion 

#pragma region ======================= LOADGAME MENU

extern char *load_saveshot;
static char loadshotname[MAX_QPATH];

static void LoadGameCallback(void *self)
{
	menuaction_s *a = (menuaction_s *)self;

	// Set saveshot name here
	if (a->generic.localdata[0] > 0 && m_saveshotvalid[a->generic.localdata[0]]) //mxd. Skip kmq2save0, because it's the "ENTERING Map Name" item, which doesn't have a saveshot.
	{
		Com_sprintf(loadshotname, sizeof(loadshotname), "/save/kmq2save%i/shot.jpg", a->generic.localdata[0]);
		load_saveshot = loadshotname;
	}
	else
	{
		load_saveshot = NULL;
	}

	if (m_savevalid[a->generic.localdata[0]])
	{
		Cbuf_AddText(va("load kmq2save%i\n", a->generic.localdata[0]));
		UI_ForceMenuOff();
	}
}

static void LoadGame_MenuInit(void)
{
	UI_UpdateSavegameData();

	s_loadgame_menu.x = SCREEN_WIDTH * 0.5f - 240;
	s_loadgame_menu.y = DEFAULT_MENU_Y; //mxd. Was SCREEN_HEIGHT * 0.5 - 58;
	s_loadgame_menu.nitems = 0;

	for (int i = 0; i < MAX_SAVEGAMES; i++)
	{
		s_loadgame_actions[i].generic.name = m_savestrings[i];
		s_loadgame_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_loadgame_actions[i].generic.localdata[0] = i;
		s_loadgame_actions[i].generic.callback = LoadGameCallback;

		s_loadgame_actions[i].generic.x = 0;
		s_loadgame_actions[i].generic.y = i * MENU_LINE_SIZE;

		if (i > 0) // Separate from autosave
			s_loadgame_actions[i].generic.y += 10;

		s_loadgame_actions[i].generic.type = MTYPE_ACTION;

		Menu_AddItem(&s_loadgame_menu, &s_loadgame_actions[i]);
	}

	s_loadgame_back_action.generic.type = MTYPE_ACTION;
	s_loadgame_back_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_loadgame_back_action.generic.x = 0;
	s_loadgame_back_action.generic.y = (MAX_SAVEGAMES + 3) * MENU_LINE_SIZE;
	s_loadgame_back_action.generic.name = (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_GAME); //mxd
	s_loadgame_back_action.generic.callback = UI_BackMenu;

	Menu_AddItem(&s_loadgame_menu, &s_loadgame_back_action);
}

static void LoadGame_MenuDraw(void)
{
	Menu_DrawBanner("m_banner_load_game");
	Menu_Draw(&s_loadgame_menu);

	if(s_loadgame_menu.cursor < MAX_SAVEGAMES) //mxd. Don't try to draw a saveshot for the "back" button!
		DrawSaveshot(true);
}

static const char *LoadGame_MenuKey(int key)
{
	if (key == K_ESCAPE || key == K_ENTER)
		s_savegame_menu.cursor = max(s_loadgame_menu.cursor - 1, 0);

	return Default_MenuKey(&s_loadgame_menu, key);
}

void M_Menu_LoadGame_f(void)
{
	LoadGame_MenuInit();
	UI_PushMenu(LoadGame_MenuDraw, LoadGame_MenuKey);
}

#pragma endregion 

#pragma region ======================= SAVEGAME MENU

static void SaveGameCallback(void *self)
{
	menuaction_s *a = (menuaction_s *)self;

	Cbuf_AddText(va("save kmq2save%i\n", a->generic.localdata[0]));
	UI_ForceMenuOff();
}

static void SaveGame_MenuDraw(void)
{
	Menu_DrawBanner("m_banner_save_game");
	Menu_AdjustCursor(&s_savegame_menu, 1);
	Menu_Draw(&s_savegame_menu);

	if (s_savegame_menu.cursor < MAX_SAVEGAMES - 1) //mxd. Don't try to draw a saveshot for the "back" button!
		DrawSaveshot(false);
}

static void SaveGame_MenuInit(void)
{
	UI_UpdateSavegameData();

	s_savegame_menu.x = SCREEN_WIDTH * 0.5f - 240;
	s_savegame_menu.y = DEFAULT_MENU_Y; //mxd. Was SCREEN_HEIGHT * 0.5 - 58;
	s_savegame_menu.nitems = 0;

	// Don't include the autosave slot
	for (int i = 0; i < MAX_SAVEGAMES - 1; i++)
	{
		s_savegame_actions[i].generic.name = m_savestrings[i + 1];
		s_savegame_actions[i].generic.localdata[0] = i + 1;
		s_savegame_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_savegame_actions[i].generic.callback = SaveGameCallback;

		s_savegame_actions[i].generic.x = 0;
		s_savegame_actions[i].generic.y = i * MENU_LINE_SIZE;

		s_savegame_actions[i].generic.type = MTYPE_ACTION;

		Menu_AddItem(&s_savegame_menu, &s_savegame_actions[i]);
	}

	s_savegame_back_action.generic.type = MTYPE_ACTION;
	s_savegame_back_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_savegame_back_action.generic.x = 0;
	s_savegame_back_action.generic.y = (MAX_SAVEGAMES + 1) * MENU_LINE_SIZE;
	s_savegame_back_action.generic.name = (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_GAME); //mxd
	s_savegame_back_action.generic.callback = UI_BackMenu;

	Menu_AddItem(&s_savegame_menu, &s_savegame_back_action);
}

static const char *SaveGame_MenuKey(int key)
{
	if (key == K_ENTER || key == K_ESCAPE)
		s_loadgame_menu.cursor = max(s_savegame_menu.cursor - 1, 0);

	return Default_MenuKey(&s_savegame_menu, key);
}

void M_Menu_SaveGame_f(void)
{
	if (!Com_ServerState())
		return; // Not playing a game

	SaveGame_MenuInit();
	UI_PushMenu(SaveGame_MenuDraw, SaveGame_MenuKey);
}

#pragma endregion 