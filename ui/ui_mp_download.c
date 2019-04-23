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

// ui_download.c -- the autodownload options menu 

#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"

static menuframework_s s_downloadoptions_menu;

static menuseparator_s	s_download_title;
static menulist_s	s_allow_download_box;

#ifdef USE_CURL	// HTTP downloading from R1Q2
static menulist_s	s_allow_http_download_box;
#endif	// USE_CURL

static menulist_s	s_allow_download_maps_box;
static menulist_s	s_allow_download_textures_24bit_box;	// option to allow downloading 24-bit textures
static menulist_s	s_allow_download_models_box;
static menulist_s	s_allow_download_players_box;
static menulist_s	s_allow_download_sounds_box;

static menuaction_s	s_download_back_action;

static void DownloadCallback(void *self)
{
	menulist_s *f = (menulist_s *)self;

	if (f == &s_allow_download_box)
		Cvar_SetValue("allow_download", f->curvalue);
#ifdef USE_CURL	// HTTP downloading from R1Q2
	else if (f == &s_allow_http_download_box)
		Cvar_SetValue("cl_http_downloads", f->curvalue);
#endif	// USE_CURL
	else if (f == &s_allow_download_maps_box)
		Cvar_SetValue("allow_download_maps", f->curvalue);
	else if (f == &s_allow_download_textures_24bit_box) // Knightmare- option to allow downloading 24-bit textures
		Cvar_SetValue("allow_download_textures_24bit", f->curvalue);
	else if (f == &s_allow_download_models_box)
		Cvar_SetValue("allow_download_models", f->curvalue);
	else if (f == &s_allow_download_players_box)
		Cvar_SetValue("allow_download_players", f->curvalue);
	else if (f == &s_allow_download_sounds_box)
		Cvar_SetValue("allow_download_sounds", f->curvalue);
}

void DownloadOptions_MenuInit(void)
{
	int y = 0;

	s_downloadoptions_menu.x = SCREEN_WIDTH * 0.5f;
	s_downloadoptions_menu.y = DEFAULT_MENU_Y; //mxd
	s_downloadoptions_menu.nitems = 0;

	s_download_title.generic.type	= MTYPE_SEPARATOR;
	s_download_title.generic.flags	= QMF_LEFT_JUSTIFY; //mxd
	s_download_title.generic.name	= "Download Options";
	s_download_title.generic.x		= UI_CenteredX(&s_download_title.generic, s_downloadoptions_menu.x); //mxd. Draw centered. Was 48
	s_download_title.generic.y		= y;

	s_allow_download_box.generic.type		= MTYPE_SPINCONTROL;
	s_allow_download_box.generic.x			= 0;
	s_allow_download_box.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_allow_download_box.generic.name		= "Allow downloading";
	s_allow_download_box.generic.callback	= DownloadCallback;
	s_allow_download_box.itemnames			= yesno_names;
	s_allow_download_box.curvalue			= (Cvar_VariableValue("allow_download") != 0);

#ifdef USE_CURL	// HTTP downloading from R1Q2
	s_allow_http_download_box.generic.type		= MTYPE_SPINCONTROL;
	s_allow_http_download_box.generic.x			= 0;
	s_allow_http_download_box.generic.y			= y += MENU_LINE_SIZE;
	s_allow_http_download_box.generic.name		= "HTTP downloading";
	s_allow_http_download_box.generic.callback	= DownloadCallback;
	s_allow_http_download_box.itemnames			= yesno_names;
	s_allow_http_download_box.curvalue			= (Cvar_VariableValue("cl_http_downloads") != 0);
#endif	// USE_CURL

	s_allow_download_maps_box.generic.type		= MTYPE_SPINCONTROL;
	s_allow_download_maps_box.generic.x			= 0;
	s_allow_download_maps_box.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_allow_download_maps_box.generic.name		= "Maps and textures";
	s_allow_download_maps_box.generic.callback	= DownloadCallback;
	s_allow_download_maps_box.itemnames			= yesno_names;
	s_allow_download_maps_box.curvalue			= (Cvar_VariableValue("allow_download_maps") != 0);

	// Knightmare- option to allow downloading 24-bit textures
	s_allow_download_textures_24bit_box.generic.type		= MTYPE_SPINCONTROL;
	s_allow_download_textures_24bit_box.generic.x			= 0;
	s_allow_download_textures_24bit_box.generic.y			= y += MENU_LINE_SIZE;
	s_allow_download_textures_24bit_box.generic.name		= "Truecolor textures";
	s_allow_download_textures_24bit_box.generic.callback	= DownloadCallback;
	s_allow_download_textures_24bit_box.generic.statusbar	= "Enable to allow downloading of JPG / PNG / TGA textures";
	s_allow_download_textures_24bit_box.itemnames			= yesno_names;
	s_allow_download_textures_24bit_box.curvalue			= (Cvar_VariableValue("allow_download_textures_24bit") != 0);

	s_allow_download_players_box.generic.type		= MTYPE_SPINCONTROL;
	s_allow_download_players_box.generic.x			= 0;
	s_allow_download_players_box.generic.y			= y += MENU_LINE_SIZE;
	s_allow_download_players_box.generic.name		= "Player models and skins";
	s_allow_download_players_box.generic.callback	= DownloadCallback;
	s_allow_download_players_box.itemnames			= yesno_names;
	s_allow_download_players_box.curvalue			= (Cvar_VariableValue("allow_download_players") != 0);

	s_allow_download_models_box.generic.type		= MTYPE_SPINCONTROL;
	s_allow_download_models_box.generic.x			= 0;
	s_allow_download_models_box.generic.y			= y += MENU_LINE_SIZE;
	s_allow_download_models_box.generic.name		= "Models";
	s_allow_download_models_box.generic.callback	= DownloadCallback;
	s_allow_download_models_box.itemnames			= yesno_names;
	s_allow_download_models_box.curvalue			= (Cvar_VariableValue("allow_download_models") != 0);

	s_allow_download_sounds_box.generic.type		= MTYPE_SPINCONTROL;
	s_allow_download_sounds_box.generic.x			= 0;
	s_allow_download_sounds_box.generic.y			= y += MENU_LINE_SIZE;
	s_allow_download_sounds_box.generic.name		= "Sounds";
	s_allow_download_sounds_box.generic.callback	= DownloadCallback;
	s_allow_download_sounds_box.itemnames			= yesno_names;
	s_allow_download_sounds_box.curvalue			= (Cvar_VariableValue("allow_download_sounds") != 0);

	s_download_back_action.generic.type			= MTYPE_ACTION;
	s_download_back_action.generic.name			= (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_MULTIPLAYER); //mxd
	s_download_back_action.generic.flags		= QMF_LEFT_JUSTIFY;
	s_download_back_action.generic.x			= UI_CenteredX(&s_download_back_action.generic, s_downloadoptions_menu.x); //mxd. Draw centered. Was -7 * MENU_FONT_SIZE;
	s_download_back_action.generic.y			= y += 3 * MENU_LINE_SIZE;
	s_download_back_action.generic.callback		= UI_BackMenu;

	Menu_AddItem(&s_downloadoptions_menu, &s_download_title);
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_box);

#ifdef USE_CURL	// HTTP downloading from R1Q2
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_http_download_box);
#endif	// USE_CURL

	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_maps_box);
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_textures_24bit_box); // Option to allow downloading 24-bit textures
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_players_box);
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_models_box);
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_sounds_box);

	Menu_AddItem(&s_downloadoptions_menu, &s_download_back_action);
}

void DownloadOptions_MenuDraw(void)
{
	Menu_DrawBanner("m_banner_multiplayer");
	Menu_AdjustCursor(&s_downloadoptions_menu, 1); //mxd
	Menu_Draw(&s_downloadoptions_menu);
}

const char *DownloadOptions_MenuKey(int key)
{
	return Default_MenuKey(&s_downloadoptions_menu, key);
}

void M_Menu_DownloadOptions_f(void)
{
	DownloadOptions_MenuInit();
	UI_PushMenu(DownloadOptions_MenuDraw, DownloadOptions_MenuKey);
}