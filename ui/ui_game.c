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

// ui_game.c -- the single player menu and credits

#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"


static int m_game_cursor;

static menuframework_s	s_game_menu;
static menuaction_s		s_easy_game_action;
static menuaction_s		s_medium_game_action;
static menuaction_s		s_hard_game_action;
static menuaction_s		s_nitemare_game_action;
static menuaction_s		s_load_game_action;
static menuaction_s		s_save_game_action;
static menuaction_s		s_credits_action;
static menuseparator_s	s_blankline;
static menuaction_s		s_game_back_action;


static void StartGame(void)
{
	// Disable updates and start the cinematic going
	cl.servercount = -1;
	UI_ForceMenuOff();
	Cvar_SetValue("deathmatch", 0);
	Cvar_SetValue("coop", 0);
	Cvar_SetValue("gamerules", 0); //PGM

	if (cls.state != ca_disconnected) // Don't force loadscreen if disconnected
		Cbuf_AddText("loading ; killserver ; wait\n");

	Cbuf_AddText("newgame\n");
	cls.key_dest = key_game;
}

#pragma region ======================= Menu item callbacks

static void EasyGameFunc(void *data)
{
	Cvar_ForceSet("skill", "0");
	StartGame();
}

static void MediumGameFunc(void *data)
{
	Cvar_ForceSet("skill", "1");
	StartGame();
}

static void HardGameFunc(void *data)
{
	Cvar_ForceSet("skill", "2");
	StartGame();
}

static void NitemareGameFunc(void *data)
{
	Cvar_ForceSet("skill", "3");
	StartGame();
}

static void LoadGameFunc(void *unused)
{
	M_Menu_LoadGame_f();
}

static void SaveGameFunc(void *unused)
{
	M_Menu_SaveGame_f();
}

static void CreditsFunc(void *unused)
{
	M_Menu_Credits_f();
}

#pragma endregion 

void Game_MenuInit(void)
{
	const int x = -MENU_FONT_SIZE * 3; //mxd
	int y = 0;

	s_game_menu.x = SCREEN_WIDTH * 0.5f;
	s_game_menu.nitems = 0;

	s_easy_game_action.generic.type		= MTYPE_ACTION;
	s_easy_game_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_easy_game_action.generic.x		= x;
	s_easy_game_action.generic.y		= y; // 0
	s_easy_game_action.generic.name		= "Easy";
	s_easy_game_action.generic.callback = EasyGameFunc;

	s_medium_game_action.generic.type	= MTYPE_ACTION;
	s_medium_game_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_medium_game_action.generic.x		= x;
	s_medium_game_action.generic.y		= y += MENU_LINE_SIZE;
	s_medium_game_action.generic.name	= "Medium";
	s_medium_game_action.generic.callback = MediumGameFunc;

	s_hard_game_action.generic.type		= MTYPE_ACTION;
	s_hard_game_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_hard_game_action.generic.x		= x;
	s_hard_game_action.generic.y		= y += MENU_LINE_SIZE;
	s_hard_game_action.generic.name		= "Hard";
	s_hard_game_action.generic.callback	= HardGameFunc;

	s_nitemare_game_action.generic.type		= MTYPE_ACTION;
	s_nitemare_game_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_nitemare_game_action.generic.x		= x;
	s_nitemare_game_action.generic.y		= y += MENU_LINE_SIZE;
	s_nitemare_game_action.generic.name		= "Nightmare";
	s_nitemare_game_action.generic.callback	= NitemareGameFunc;

	s_blankline.generic.type = MTYPE_SEPARATOR;

	s_load_game_action.generic.type		= MTYPE_ACTION;
	s_load_game_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_load_game_action.generic.x		= x;
	s_load_game_action.generic.y		= y += 2 * MENU_LINE_SIZE;
	s_load_game_action.generic.name		= "Load game";
	s_load_game_action.generic.callback	= LoadGameFunc;

	s_save_game_action.generic.type		= MTYPE_ACTION;
	s_save_game_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_save_game_action.generic.x		= x;
	s_save_game_action.generic.y		= y += MENU_LINE_SIZE;
	s_save_game_action.generic.name		= "Save game";
	s_save_game_action.generic.callback	= SaveGameFunc;

	s_credits_action.generic.type		= MTYPE_ACTION;
	s_credits_action.generic.flags		= QMF_LEFT_JUSTIFY;
	s_credits_action.generic.x			= x;
	s_credits_action.generic.y			= y += MENU_LINE_SIZE;
	s_credits_action.generic.name		= "Credits";
	s_credits_action.generic.callback	= CreditsFunc;

	s_game_back_action.generic.type		= MTYPE_ACTION;
	s_game_back_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_game_back_action.generic.name		= (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_MAIN); //mxd
	s_game_back_action.generic.x		= UI_CenteredX(&s_game_back_action.generic, s_game_menu.x); //mxd. Was 0
	s_game_back_action.generic.y		= y += 2 * MENU_LINE_SIZE;
	s_game_back_action.generic.callback = UI_BackMenu;

	Menu_AddItem(&s_game_menu, (void *)&s_easy_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_medium_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_hard_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_nitemare_game_action);

	Menu_AddItem(&s_game_menu, (void *)&s_blankline);

	Menu_AddItem(&s_game_menu, (void *)&s_load_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_save_game_action);

	Menu_AddItem(&s_game_menu, (void *)&s_blankline);
	Menu_AddItem(&s_game_menu, (void *)&s_credits_action);

	Menu_AddItem(&s_game_menu, (void *)&s_game_back_action);

	Menu_Center(&s_game_menu);
}

void Game_MenuDraw(void)
{
	Menu_DrawBanner("m_banner_game");
	Menu_AdjustCursor(&s_game_menu, 1);
	Menu_Draw(&s_game_menu);
}

const char *Game_MenuKey(int key)
{
	return Default_MenuKey(&s_game_menu, key);
}

void M_Menu_Game_f(void)
{
	Game_MenuInit();
	UI_PushMenu(Game_MenuDraw, Game_MenuKey);
	m_game_cursor = 1;
}