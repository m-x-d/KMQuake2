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

// ui_options_sound.c -- the sound options menu

#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"

static menuframework_s	s_options_sound_menu;
static menuseparator_s	s_options_sound_header;
static menuslider_s		s_options_sound_sfxvolume_slider;
static menuslider_s		s_options_sound_musicvolume_slider;
static menulist_s		s_options_sound_quality_list;
static menuaction_s		s_options_sound_defaults_action;
static menuaction_s		s_options_sound_back_action;

#pragma region ======================= Menu item callbacks

//mxd
static void RestartSound()
{
	Menu_DrawTextBox(168, 192, 36, 3);
	SCR_DrawString(188, 192 + MENU_FONT_SIZE * 1, ALIGN_CENTER, S_COLOR_ALT"Restarting the sound system.", 255);
	SCR_DrawString(188, 192 + MENU_FONT_SIZE * 2, ALIGN_CENTER, S_COLOR_ALT"This could take up to a minute,", 255);
	SCR_DrawString(188, 192 + MENU_FONT_SIZE * 3, ALIGN_CENTER, S_COLOR_ALT"so please be patient.", 255);

	// The text box won't show up unless we do a buffer swap
	GLimp_EndFrame();
	CL_Snd_Restart_f();
}

static void UpdateVolumeFunc(void *unused)
{
	Cvar_SetValue("s_volume", s_options_sound_sfxvolume_slider.curvalue / 10);
}

static void UpdateMusicVolumeFunc(void *unused)
{
	Cvar_SetValue("s_musicvolume", s_options_sound_musicvolume_slider.curvalue / 10);
}

static void UpdateSoundQualityFunc(void *unused)
{
	//Knightmare- added DMP's 44/48 KHz sound support
	//** DMP check the newly added sound quality menu options
	switch (s_options_sound_quality_list.curvalue)
	{
		case 1:
			Cvar_SetValue("s_khz", 22);
			Cvar_SetValue("s_loadas8bit", false);
			break;

		case 2:
			Cvar_SetValue("s_khz", 44);
			Cvar_SetValue("s_loadas8bit", false);
			break;

		case 3:
			Cvar_SetValue("s_khz", 48);
			Cvar_SetValue("s_loadas8bit", false);
			break;

		case 0:
		default:
			Cvar_SetValue("s_khz", 11);
			Cvar_SetValue("s_loadas8bit", true);
			break;
	}
	//** DMP end sound menu changes

	RestartSound(); //mxd
}

static void SoundSetMenuItemValues(void)
{
	s_options_sound_sfxvolume_slider.curvalue = Cvar_VariableValue("s_volume") * 10;
	s_options_sound_musicvolume_slider.curvalue = Cvar_VariableValue("s_musicvolume") * 10;
	
	//**  DMP convert setting into index for option display text
	switch(Cvar_VariableInteger("s_khz"))
	{
		case 48: s_options_sound_quality_list.curvalue = 3;  break;
		case 44: s_options_sound_quality_list.curvalue = 2;  break;
		case 22: s_options_sound_quality_list.curvalue = 1;  break;
		default: s_options_sound_quality_list.curvalue = 0;  break;
	}
}

static void SoundResetDefaultsFunc(void *unused)
{
	Cvar_SetToDefault("s_volume");
	Cvar_SetToDefault("s_musicvolume"); //mxd
	Cvar_SetToDefault("ogg_loopcount"); //mxd. Was cd_loopcount
	Cvar_SetToDefault("s_khz");
	Cvar_SetToDefault("s_loadas8bit");

	SoundSetMenuItemValues();
	RestartSound(); //mxd
}

#pragma endregion

void Options_Sound_MenuInit(void)
{
	static const char *quality_items[] =
	{
		"Low (11KHz / 8-bit)",			//** DMP - changed text
		"Normal (22KHz / 16-bit)",		//** DMP - changed text
		"High (44KHz / 16-bit)",		//** DMP - added 44 Khz menu item
		"Highest (48KHz / 16-bit)",		//** DMP - added 48 Khz menu item
		0
	};

	int y = MENU_LINE_SIZE * 3;

	s_options_sound_menu.x = SCREEN_WIDTH * 0.5f;
	s_options_sound_menu.y = DEFAULT_MENU_Y; //mxd. Was SCREEN_HEIGHT * 0.5f - 58;
	s_options_sound_menu.nitems = 0;

	s_options_sound_header.generic.type		= MTYPE_SEPARATOR;
	s_options_sound_header.generic.name		= "Sound";
	s_options_sound_header.generic.x		= MENU_FONT_SIZE / 2 * strlen(s_options_sound_header.generic.name);
	s_options_sound_header.generic.y		= 0;

	s_options_sound_sfxvolume_slider.generic.type		= MTYPE_SLIDER;
	s_options_sound_sfxvolume_slider.generic.x			= 0;
	s_options_sound_sfxvolume_slider.generic.y			= y;
	s_options_sound_sfxvolume_slider.generic.name		= "Effects volume";
	s_options_sound_sfxvolume_slider.generic.callback	= UpdateVolumeFunc;
	s_options_sound_sfxvolume_slider.minvalue			= 0;
	s_options_sound_sfxvolume_slider.maxvalue			= 10;
	s_options_sound_sfxvolume_slider.numdecimals		= 1; //mxd
	s_options_sound_sfxvolume_slider.generic.statusbar	= "Volume of sound effects";
	s_options_sound_sfxvolume_slider.cvar				= Cvar_FindVar("s_volume"); //mxd

	s_options_sound_musicvolume_slider.generic.type			= MTYPE_SLIDER;
	s_options_sound_musicvolume_slider.generic.x			= 0;
	s_options_sound_musicvolume_slider.generic.y			= y += MENU_LINE_SIZE;
	s_options_sound_musicvolume_slider.generic.name			= "Music volume";
	s_options_sound_musicvolume_slider.generic.callback		= UpdateMusicVolumeFunc;
	s_options_sound_musicvolume_slider.minvalue				= 0;
	s_options_sound_musicvolume_slider.maxvalue				= 10;
	s_options_sound_musicvolume_slider.numdecimals			= 1; //mxd
	s_options_sound_musicvolume_slider.generic.statusbar	= "Volume of ogg vorbis music";
	s_options_sound_musicvolume_slider.cvar					= Cvar_FindVar("s_musicvolume"); //mxd

	s_options_sound_quality_list.generic.type				= MTYPE_SPINCONTROL;
	s_options_sound_quality_list.generic.x					= 0;
	s_options_sound_quality_list.generic.y					= y += MENU_LINE_SIZE;
	s_options_sound_quality_list.generic.name				= "Sound quality";
	s_options_sound_quality_list.generic.callback			= UpdateSoundQualityFunc;
	s_options_sound_quality_list.itemnames					= quality_items;
	s_options_sound_quality_list.generic.statusbar			= "Changes quality of sound effects";

	s_options_sound_defaults_action.generic.type		= MTYPE_ACTION;
	s_options_sound_defaults_action.generic.flags		= QMF_LEFT_JUSTIFY; //mxd
	s_options_sound_defaults_action.generic.name		= "Reset to defaults";
	s_options_sound_defaults_action.generic.x			= -MENU_FONT_SIZE * strlen(s_options_sound_defaults_action.generic.name); //mxd
	s_options_sound_defaults_action.generic.y			= y += MENU_LINE_SIZE * 2;
	s_options_sound_defaults_action.generic.callback	= SoundResetDefaultsFunc;
	s_options_sound_defaults_action.generic.statusbar	= "Resets all sound settings to internal defaults";

	s_options_sound_back_action.generic.type			= MTYPE_ACTION;
	s_options_sound_back_action.generic.flags			= QMF_LEFT_JUSTIFY; //mxd
	s_options_sound_back_action.generic.name			= (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_OPTIONS); //mxd
	s_options_sound_back_action.generic.x				= UI_CenteredX(&s_options_sound_back_action.generic, s_options_sound_menu.x); //mxd. Was MENU_FONT_SIZE;
	s_options_sound_back_action.generic.y				= y += MENU_LINE_SIZE * 2;
	s_options_sound_back_action.generic.callback		= UI_BackMenu;

	Menu_AddItem(&s_options_sound_menu, (void *)&s_options_sound_header);
	Menu_AddItem(&s_options_sound_menu, (void *)&s_options_sound_sfxvolume_slider);
	Menu_AddItem(&s_options_sound_menu, (void *)&s_options_sound_musicvolume_slider);
	Menu_AddItem(&s_options_sound_menu, (void *)&s_options_sound_quality_list);
	Menu_AddItem(&s_options_sound_menu, (void *)&s_options_sound_defaults_action);
	Menu_AddItem(&s_options_sound_menu, (void *)&s_options_sound_back_action);

	SoundSetMenuItemValues();
}

void Options_Sound_MenuDraw(void)
{
	Menu_DrawBanner("m_banner_options");

	Menu_AdjustCursor(&s_options_sound_menu, 1);
	Menu_Draw(&s_options_sound_menu);
}

const char *Options_Sound_MenuKey(int key)
{
	return Default_MenuKey(&s_options_sound_menu, key);
}

void M_Menu_Options_Sound_f(void)
{
	Options_Sound_MenuInit();
	UI_PushMenu(Options_Sound_MenuDraw, Options_Sound_MenuKey);
}