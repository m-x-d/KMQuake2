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

// ui_subsystem.c -- menu support/handling functions

#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"

#define MAX_MENU_DEPTH	8

cvar_t *ui_cursor_scale;

qboolean m_entersound; // Play after drawing a frame, so caching won't disrupt the sound

void(*m_drawfunc) (void);
const char *(*m_keyfunc) (int key);

typedef struct
{
	void (*draw) (void);
	const char *(*key) (int k);
} menulayer_t;

menulayer_t	m_layers[MAX_MENU_DEPTH];
int m_menudepth;

// From Q2max. Technically, this doesn't ADD anything anywhere, just adjusts the coordinates...
void UI_AddButton(buttonmenuobject_t *thisObj, int index, float x, float y, float w, float h)
{
	float x1 = x;
	float y1 = y;
	float w1 = w;
	float h1 = h;

	SCR_AdjustFrom640(&x1, &y1, &w1, &h1, ALIGN_CENTER);

	thisObj->min[0] = x1;
	thisObj->max[0] = x1 + w1;
	thisObj->min[1] = y1;
	thisObj->max[1] = y1 + h1;

	thisObj->index = index;
}

// From Q2max
void UI_AddMainButton(mainmenuobject_t *thisObj, int index, int x, int y, char *name)
{
	int w, h;
	R_DrawGetPicSize(&w, &h, name);
	
	float x1 = x;
	float y1 = y;
	float w1 = w;
	float h1 = h;

	SCR_AdjustFrom640(&x1, &y1, &w1, &h1, ALIGN_CENTER);

	thisObj->min[0] = x1;
	thisObj->max[0] = x1 + w1;
	thisObj->min[1] = y1;
	thisObj->max[1] = y1 + h1;

	switch (index)
	{
		case 0:
			thisObj->OpenMenu = M_Menu_Game_f;
			break;
		case 1:
			thisObj->OpenMenu = M_Menu_Multiplayer_f;
			break;
		case 2:
			thisObj->OpenMenu = M_Menu_Options_f;
			break;
		case 3:
			thisObj->OpenMenu = M_Menu_Video_f;
			break;
		case 4:
			thisObj->OpenMenu = M_Menu_Quit_f;
			break;
	}
}

// From Q2max
void UI_RefreshCursorButtons(void)
{
	cursor.buttonused[MOUSEBUTTON1] = true;
	cursor.buttondown[MOUSEBUTTON1] = false;
	cursor.buttonclicks[MOUSEBUTTON1] = 0;
	cursor.buttonused[MOUSEBUTTON2] = true;
	cursor.buttondown[MOUSEBUTTON2] = false;
	cursor.buttonclicks[MOUSEBUTTON2] = 0;
}

void UI_PushMenu(void(*draw) (void), const char *(*key) (int k))
{
	if (Cvar_VariableValue("maxclients") == 1 && Com_ServerState() && !cls.consoleActive) // Knightmare added
		Cvar_Set("paused", "1");

	// Knightmare- if just opened menu, and ingame and not DM, grab screen first
	if (cls.key_dest != key_menu && !Cvar_VariableValue("deathmatch") && Com_ServerState() == 2) //ss_game
		//&& !cl.cinematictime && Com_ServerState())
		R_GrabScreen();

	// If this menu is already present, drop back to that level to avoid stacking menus by hotkeys
	int depth;
	for (depth = 0; depth < m_menudepth; depth++)
		if (m_layers[depth].draw == draw && m_layers[depth].key == key)
			m_menudepth = depth;

	if (depth == m_menudepth)
	{
		if (m_menudepth >= MAX_MENU_DEPTH)
			Com_Error(ERR_FATAL, "UI_PushMenu: MAX_MENU_DEPTH");

		m_layers[m_menudepth].draw = m_drawfunc;
		m_layers[m_menudepth].key = m_keyfunc;
		m_menudepth++;
	}

	m_drawfunc = draw;
	m_keyfunc = key;

	m_entersound = true;

	// Knightmare- added Psychospaz's mouse support
	UI_RefreshCursorLink();
	UI_RefreshCursorButtons();

	cls.key_dest = key_menu;
}

void UI_ForceMenuOff(void)
{
	// Knightmare- added Psychospaz's mouse support
	UI_RefreshCursorLink();

	m_drawfunc = 0;
	m_keyfunc = 0;
	cls.key_dest = key_game;
	m_menudepth = 0;

	Key_ClearStates();

	if (!cls.consoleActive && Cvar_VariableValue("maxclients") == 1 && Com_ServerState()) // Knightmare added
		Cvar_Set("paused", "0");
}

void UI_PopMenu(void)
{
	S_StartLocalSound(menu_out_sound);
	if (m_menudepth < 1)
		Com_Error(ERR_FATAL, "UI_PopMenu: depth < 1");

	m_menudepth--;

	m_drawfunc = m_layers[m_menudepth].draw;
	m_keyfunc = m_layers[m_menudepth].key;

	// Knightmare- added Psychospaz's mouse support
	UI_RefreshCursorLink();
	UI_RefreshCursorButtons();

	if (!m_menudepth)
		UI_ForceMenuOff();
}

void UI_BackMenu(void *unused)
{
	UI_PopMenu();
}

const char *Default_MenuKey(menuframework_s *m, int key)
{
	if (m)
	{
		menucommon_s *item = Menu_ItemAtCursor(m);
		if (item && item->type == MTYPE_FIELD && Field_Key((menufield_s *)item, key))
			return NULL;
	}

	switch (key)
	{
		case K_ESCAPE:
			UI_PopMenu();
			return menu_out_sound;

		case K_KP_UPARROW:
		case K_UPARROW:
			if (m)
			{
				m->cursor--;
				// Knightmare- added Psychospaz's mouse support
				UI_RefreshCursorLink();

				Menu_AdjustCursor(m, -1);
				return menu_move_sound;
			}
			break;

		case K_TAB:
		case K_KP_DOWNARROW:
		case K_DOWNARROW:
			if (m)
			{
				m->cursor++;
				// Knightmare- added Psychospaz's mouse support
				UI_RefreshCursorLink();

				Menu_AdjustCursor(m, 1);
				return menu_move_sound;
			}
			break;

		case K_KP_LEFTARROW:
		case K_LEFTARROW:
			if (m)
			{
				Menu_SlideItem(m, -1);
				return menu_move_sound;
			}
			break;

		case K_KP_RIGHTARROW:
		case K_RIGHTARROW:
			if (m)
			{
				Menu_SlideItem(m, 1);
				return menu_move_sound;
			}
			break;

		/*case K_MOUSE1:
		case K_MOUSE2:
		case K_MOUSE3:
		//Knightmare 12/22/2001
		case K_MOUSE4:
		case K_MOUSE5:*/
		//end Knightmare
		case K_JOY1:
		case K_JOY2:
		case K_JOY3:
		case K_JOY4:
		case K_AUX1:
		case K_AUX2:
		case K_AUX3:
		case K_AUX4:
		case K_AUX5:
		case K_AUX6:
		case K_AUX7:
		case K_AUX8:
		case K_AUX9:
		case K_AUX10:
		case K_AUX11:
		case K_AUX12:
		case K_AUX13:
		case K_AUX14:
		case K_AUX15:
		case K_AUX16:
		case K_AUX17:
		case K_AUX18:
		case K_AUX19:
		case K_AUX20:
		case K_AUX21:
		case K_AUX22:
		case K_AUX23:
		case K_AUX24:
		case K_AUX25:
		case K_AUX26:
		case K_AUX27:
		case K_AUX28:
		case K_AUX29:
		case K_AUX30:
		case K_AUX31:
		case K_AUX32:
		case K_KP_ENTER:
		case K_ENTER:
			if (m)
				Menu_SelectItem(m);
			return menu_move_sound;
	}

	return NULL;
}

void UI_Precache(void)
{
	char scratch[80];

	// General images
	R_DrawFindPic(LOADSCREEN_NAME);
	R_DrawFindPic(UI_BACKGROUND_NAME);
	R_DrawFindPic(UI_NOSCREEN_NAME);

	// Loadscreen images
	R_DrawFindPic("/pics/loading.pcx");
	R_DrawFindPic("/pics/loading_bar.pcx");
	R_DrawFindPic("/pics/downloading.pcx");
	R_DrawFindPic("/pics/downloading_bar.pcx");
	R_DrawFindPic("/pics/loading_led1.pcx");

	// Cursors
//	R_DrawFindPic (UI_MOUSECURSOR_MAIN_PIC);
//	R_DrawFindPic (UI_MOUSECURSOR_HOVER_PIC);
//	R_DrawFindPic (UI_MOUSECURSOR_CLICK_PIC);
//	R_DrawFindPic (UI_MOUSECURSOR_OVER_PIC);
//	R_DrawFindPic (UI_MOUSECURSOR_TEXT_PIC);
	R_DrawFindPic(UI_MOUSECURSOR_PIC);

	for (int i = 0; i < NUM_MAINMENU_CURSOR_FRAMES; i++)
	{
		Com_sprintf(scratch, sizeof(scratch), "/pics/m_cursor%d.pcx", i);
		R_DrawFindPic(scratch);
	}

	// Main menu items
	R_DrawFindPic("/pics/m_main_game.pcx");
	R_DrawFindPic("/pics/m_main_game_sel.pcx");
	R_DrawFindPic("/pics/m_main_multiplayer.pcx");
	R_DrawFindPic("/pics/m_main_multiplayer_sel.pcx");
	R_DrawFindPic("/pics/m_main_options.pcx");
	R_DrawFindPic("/pics/m_main_options_sel.pcx");
	R_DrawFindPic("/pics/m_main_video.pcx");
	R_DrawFindPic("/pics/m_main_video_sel.pcx");
	//	R_DrawFindPic ("/pics/m_main_mods.pcx");
	//	R_DrawFindPic ("/pics/m_main_mods_sel.pcx");
	R_DrawFindPic("/pics/m_main_quit.pcx");
	R_DrawFindPic("/pics/m_main_quit_sel.pcx");
	R_DrawFindPic("/pics/m_main_plaque.pcx");
	R_DrawFindPic("/pics/m_main_logo.pcx");
	R_RegisterModel("models/ui/quad_cursor.md2");

	// Menu banners
	R_DrawFindPic("/pics/m_banner_game.pcx");
	R_DrawFindPic("/pics/m_banner_load_game.pcx");
	R_DrawFindPic("/pics/m_banner_save_game.pcx");
	R_DrawFindPic("/pics/m_banner_multiplayer.pcx");
	R_DrawFindPic("/pics/m_banner_join_server.pcx");
	R_DrawFindPic("/pics/m_banner_addressbook.pcx");
	R_DrawFindPic("/pics/m_banner_start_server.pcx");
	R_DrawFindPic("/pics/m_banner_plauer_setup.pcx"); // typo for image name is id's fault
	R_DrawFindPic("/pics/m_banner_options.pcx");
	R_DrawFindPic("/pics/m_banner_customize.pcx");
	R_DrawFindPic("/pics/m_banner_video.pcx");
	//	R_DrawFindPic ("/pics/m_banner_mods.pcx");
	R_DrawFindPic("/pics/quit.pcx");
	//	R_DrawFindPic ("/pics/areyousure.pcx");
	//	R_DrawFindPic ("/pics/yn.pcx");

	// GUI elements
	R_DrawFindPic("/gfx/ui/listbox_background.pcx");
	R_DrawFindPic("/gfx/ui/arrows/arrow_left.pcx");
	R_DrawFindPic("/gfx/ui/arrows/arrow_left_d.pcx");
	R_DrawFindPic("/gfx/ui/arrows/arrow_right.pcx");
	R_DrawFindPic("/gfx/ui/arrows/arrow_right_d.pcx");
}

void UI_Init(void)
{
	// Init this cvar here so M_Print can use it
	if (!alt_text_color)
		alt_text_color = Cvar_Get("alt_text_color", "2", CVAR_ARCHIVE);

	ui_cursor_scale = Cvar_Get("ui_cursor_scale", "0.4", 0);

	UI_LoadMapList(); // Load map list
	UI_InitSavegameData(); // Load savegame data

	UI_Precache(); // Precache images

	Cmd_AddCommand("menu_main", M_Menu_Main_f);
	Cmd_AddCommand("menu_game", M_Menu_Game_f);
		Cmd_AddCommand("menu_loadgame", M_Menu_LoadGame_f);
		Cmd_AddCommand("menu_savegame", M_Menu_SaveGame_f);
		Cmd_AddCommand("menu_credits", M_Menu_Credits_f );
	Cmd_AddCommand("menu_multiplayer", M_Menu_Multiplayer_f);
		Cmd_AddCommand("menu_joinserver", M_Menu_JoinServer_f);
			Cmd_AddCommand("menu_addressbook", M_Menu_AddressBook_f);
		Cmd_AddCommand("menu_startserver", M_Menu_StartServer_f);
			Cmd_AddCommand("menu_dmoptions", M_Menu_DMOptions_f);
		Cmd_AddCommand("menu_playerconfig", M_Menu_PlayerConfig_f);
		Cmd_AddCommand("menu_downloadoptions", M_Menu_DownloadOptions_f);
	Cmd_AddCommand("menu_video", M_Menu_Video_f);
		Cmd_AddCommand("menu_video_advanced", M_Menu_Video_Advanced_f);
	Cmd_AddCommand("menu_options", M_Menu_Options_f);
		Cmd_AddCommand("menu_sound", M_Menu_Options_Sound_f);
		Cmd_AddCommand("menu_controls", M_Menu_Options_Controls_f);
			Cmd_AddCommand("menu_keys", M_Menu_Keys_f);
		Cmd_AddCommand("menu_screen", M_Menu_Options_Screen_f);
		Cmd_AddCommand("menu_effects", M_Menu_Options_Effects_f);
		Cmd_AddCommand("menu_interface", M_Menu_Options_Interface_f);
	Cmd_AddCommand("menu_quit", M_Menu_Quit_f);
}

void UI_Draw(void)
{
	if (cls.key_dest != key_menu)
		return;

	// Dim everything behind it down
	if (cl.cinematictime > 0 || cls.state == ca_disconnected)
	{
		if (menu_alpha->value > 0 && R_DrawFindPic(UI_BACKGROUND_NAME))
		{
			R_DrawStretchPic(0, 0, viddef.width, viddef.height, UI_BACKGROUND_NAME, 1.0f);

			//mxd. Let's apply menu_alpha here as well
			R_DrawFill(0, 0, viddef.width, viddef.height, 0, 0, 0, (int)((1.0f - menu_alpha->value) * 255));
		}
		else
		{
			R_DrawFill(0, 0, viddef.width, viddef.height, 0, 0, 0, 255);
		}
	}
	// Ingame menu uses alpha
	else if (R_DrawFindPic(UI_BACKGROUND_NAME))
	{
		R_DrawStretchPic(0, 0, viddef.width, viddef.height, UI_BACKGROUND_NAME, menu_alpha->value);
	}
	else
	{
		R_DrawFill(0, 0, viddef.width, viddef.height, 0, 0, 0, (int)(menu_alpha->value * 255));
	}

	// Knigthmare- added Psychospaz's mouse support
	UI_RefreshCursorMenu();

	m_drawfunc();

	// Delay playing the enter sound until after the menu has been drawn, to avoid delay while caching images
	if (m_entersound)
	{
		S_StartLocalSound(menu_in_sound);
		m_entersound = false;
	}

	// Knigthmare- added Psychospaz's mouse support
	UI_Draw_Cursor();
}

void UI_Keydown(int key)
{
	if (m_keyfunc)
	{
		const char *s = m_keyfunc(key);
		if (s)
			S_StartLocalSound((char *)s);
	}
}

//mxd. Returns horizontally centered position. Expects name to be set.
int UI_CenteredX(const menucommon_s *generic, const int menux)
{
	const int offset = (generic->type == MTYPE_ACTION ? LCOLUMN_OFFSET : 0);
	return (SCREEN_WIDTH / 2) - menux - (MENU_FONT_SIZE / 2 * strlen(generic->name)) - offset;
}

//mxd
int UI_MenuDepth()
{
	return m_menudepth;
}

//mxd. Moved from cl_main.c
float ClampCvar(const float min, const float max, const float value) //TODO: mxd. Replace with clamp()?
{
	if (value < min) return min;
	if (value > max) return max;
	return value;
}