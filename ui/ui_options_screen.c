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

// ui_options_screen.c -- the screen options menu

#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"

static menuframework_s	s_options_screen_menu;
static menuseparator_s	s_options_screen_header;
static menulist_s		s_options_screen_crosshair_box;
static menuslider_s		s_options_screen_crosshairscale_slider;
static menuslider_s		s_options_screen_crosshairalpha_slider;
static menuslider_s		s_options_screen_crosshairpulse_slider;
static menuslider_s		s_options_screen_hudscale_slider;
static menuslider_s		s_options_screen_hudalpha_slider;
static menulist_s		s_options_screen_hudsqueezedigits_box;
static menulist_s		s_options_screen_objectives_box; //mxd
static menulist_s		s_options_screen_fps_box;
static menuaction_s		s_options_screen_defaults_action;
static menuaction_s		s_options_screen_back_action;

//mxd. Crosshair frame size and position...
static const int crosshair_frame_size = CROSSHAIR_SIZE * CROSSHAIR_SCALE_MAX + 4;
static int crosshair_frame_x;
static int crosshair_frame_y;

#pragma region ======================= Menu item callbacks

// Psychospaz's changeable size crosshair
static void CrosshairSizeFunc(void *unused)
{
	Cvar_SetValue("crosshair_scale", s_options_screen_crosshairscale_slider.curvalue * 0.25f);
}

static void CrosshairAlphaFunc(void *unused)
{
	Cvar_SetValue("crosshair_alpha", s_options_screen_crosshairalpha_slider.curvalue * 0.05f);
}

static void CrosshairPulseFunc(void *unused)
{
	Cvar_SetValue("crosshair_pulse", s_options_screen_crosshairpulse_slider.curvalue * 0.05f);
}

// hud scaling option
static void HudScaleFunc(void *unused)
{
	Cvar_SetValue("hud_scale", s_options_screen_hudscale_slider.curvalue - 1);
}

// hud trans option
static void HudAlphaFunc(void *unused)
{
	Cvar_SetValue("hud_alpha", (s_options_screen_hudalpha_slider.curvalue - 1) / 10);
}

// hud squeeze digits option
static void HudSqueezeDigitsFunc(void *unused)
{
	Cvar_SetValue("hud_squeezedigits", s_options_screen_hudsqueezedigits_box.curvalue);
}

// FPS counter option
static void FPSFunc(void *unused)
{
	Cvar_SetValue("cl_drawfps", s_options_screen_fps_box.curvalue);
}

//mxd. Printobjectives option
static void PrintObjectivesFunc(void *unused)
{
	Cvar_SetValue("printobjectives", s_options_screen_objectives_box.curvalue);
}

#pragma endregion

#pragma region ======================= Crosshair loading

#define MAX_CROSSHAIRS 100
char **crosshair_names;
int	numcrosshairs;

static void CrosshairFunc(void *unused)
{
	if (s_options_screen_crosshair_box.curvalue == 0)
		Cvar_SetValue("crosshair", 0);
	else
		Cvar_SetValue("crosshair", atoi(crosshair_names[s_options_screen_crosshair_box.curvalue] + 2));
}

void SetCrosshairCursor(void)
{
	s_options_screen_crosshair_box.curvalue = 0;

	if (numcrosshairs > 1)
	{
		for (int i = 0; crosshair_names[i]; i++)
		{
			if (!Q_strcasecmp(va("ch%i", (int)Cvar_VariableValue("crosshair")), crosshair_names[i]))
			{
				s_options_screen_crosshair_box.curvalue = i;
				return;
			}
		}
	}
}

void SortCrosshairs(char **list, int len)
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

			if (atoi(list[j] + 2) > atoi(list[j + 1] + 2))
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

char **SetCrosshairNames(void)
{
	char *curCrosshair;
	char *p;
	char findname[1024];
	int  ncrosshairs = 0;
	char **crosshairfiles;
	char *path = NULL;

	const int listsize = sizeof(char *) * MAX_CROSSHAIRS + 1; //mxd
	char** list = malloc(listsize);
	memset(list, 0, listsize);

	list[0] = "none"; //was "default"
	int ncrosshairnames = 1;

	path = FS_NextPath(path);
	while (path) 
	{
		Com_sprintf(findname, sizeof(findname), "%s/pics/ch*.*", path);
		crosshairfiles = FS_ListFiles(findname, &ncrosshairs, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);

		for (int i = 0; i < ncrosshairs && ncrosshairnames < MAX_CROSSHAIRS; i++)
		{
			if (!crosshairfiles || !crosshairfiles[i])
				continue;

			p = strstr(crosshairfiles[i], "/pics/");
			p++;

			p = strchr(p, '/'); //mxd. strstr -> strchr
			p++;

			if (!strstr(p, ".tga") && !strstr(p, ".png") && !strstr(p, ".jpg") && !strstr(p, ".pcx"))
				continue;

			// Filename must be chxxx
			if (Q_strncasecmp(p, "ch", 2)) 
				continue;

			const int namelen = strlen(p);
			if (namelen < 7 || namelen > 9)
				continue;

			if (!isdigit(p[2]))
				continue;

			if (namelen >= 8 && !isdigit(p[3]))
				continue;

			// ch100 is only valid 5-char name
			if (namelen == 9 && (p[2] != '1' || p[3] != '0' || p[4] != '0'))
				continue;

			const int num = strlen(p) - 4;
			p[num] = 0;

			curCrosshair = p;

			if (!FS_ItemInList(curCrosshair, ncrosshairnames, list))
			{
				FS_InsertInList(list, curCrosshair, ncrosshairnames, 1);	// i = 1 so none stays first!
				ncrosshairnames++;
			}
			
			// Set back so whole string get deleted.
			p[num] = '.';
		}

		if (ncrosshairs)
			FS_FreeFileList(crosshairfiles, ncrosshairs);
		
		path = FS_NextPath(path);
	}

	// Check pak after
	crosshairfiles = FS_ListPak("pics/", &ncrosshairs);
	if (crosshairfiles)
	{
		for (int i = 0; i < ncrosshairs && ncrosshairnames < MAX_CROSSHAIRS; i++)
		{
			if (!crosshairfiles[i])
				continue;

			p = strchr(crosshairfiles[i], '/'); //mxd. strstr -> strchr
			p++;

			if (!strstr(p, ".tga") && !strstr(p, ".png") && !strstr(p, ".jpg") && !strstr(p, ".pcx"))
				continue;

			// Filename must be chxxx
			if (Q_strncasecmp(p, "ch", 2))
				continue;

			const int namelen = strlen(p); //mxd. Was strlen(strdup(p));
			if (namelen < 7 || namelen > 9)
				continue;

			if (!isdigit(p[2]))
				continue;

			if (namelen >= 8 && !isdigit(p[3]))
				continue;

			// ch100 is only valid 5-char name
			if (namelen == 9 && (p[2] != '1' || p[3] != '0' || p[4] != '0'))
				continue;

			const int num = strlen(p) - 4;
			p[num] = 0; //NULL;

			curCrosshair = p;

			if (!FS_ItemInList(curCrosshair, ncrosshairnames, list))
			{
				FS_InsertInList(list, curCrosshair, ncrosshairnames, 1); // i = 1 so none stays first!
				ncrosshairnames++;
			}
			
			// Set back so whole string get deleted.
			p[num] = '.';
		}
	}

	// Sort the list
	SortCrosshairs(list, ncrosshairnames);

	if (ncrosshairs)
		FS_FreeFileList(crosshairfiles, ncrosshairs);

	numcrosshairs = ncrosshairnames;

	return list;
}

#pragma endregion

static void ScreenSetMenuItemValues(void)
{
	Cvar_SetValue("crosshair", ClampCvar(0, 100, Cvar_VariableValue("crosshair")));
	SetCrosshairCursor();

	Cvar_SetValue("crosshair_scale", ClampCvar(0.25, 3, Cvar_VariableValue("crosshair_scale")));
	s_options_screen_crosshairscale_slider.curvalue = Cvar_VariableValue("crosshair_scale") * 4;

	Cvar_SetValue("crosshair_alpha", ClampCvar(0.05, 1, Cvar_VariableValue("crosshair_alpha")));
	s_options_screen_crosshairalpha_slider.curvalue = Cvar_VariableValue("crosshair_alpha") * 20;

	Cvar_SetValue("crosshair_pulse", ClampCvar(0, 0.5, Cvar_VariableValue("crosshair_pulse")));
	s_options_screen_crosshairpulse_slider.curvalue = Cvar_VariableValue("crosshair_pulse") * 20;

	Cvar_SetValue("hud_scale", ClampCvar(0, 7, Cvar_VariableValue("hud_scale")));
	s_options_screen_hudscale_slider.curvalue = Cvar_VariableValue("hud_scale") + 1;

	Cvar_SetValue("hud_alpha", ClampCvar(0, 1, Cvar_VariableValue("hud_alpha")));
	s_options_screen_hudalpha_slider.curvalue = Cvar_VariableValue("hud_alpha") * 10 + 1;

	Cvar_SetValue("hud_squeezedigits", ClampCvar(0, 1, Cvar_VariableValue("hud_squeezedigits")));
	s_options_screen_hudsqueezedigits_box.curvalue = Cvar_VariableValue("hud_squeezedigits");

	Cvar_SetValue("printobjectives", ClampCvar(0, 1, Cvar_VariableValue("printobjectives"))); //mxd
	s_options_screen_objectives_box.curvalue = Cvar_VariableValue("printobjectives");

	Cvar_SetValue("cl_drawfps", ClampCvar(0, 1, Cvar_VariableValue("cl_drawfps")));
	s_options_screen_fps_box.curvalue = Cvar_VariableValue("cl_drawfps");
}

static void ScreenResetDefaultsFunc(void *unused)
{
	Cvar_SetToDefault("crosshair");
	Cvar_SetToDefault("crosshair_scale");
	Cvar_SetToDefault("crosshair_alpha");
	Cvar_SetToDefault("crosshair_pulse");
	Cvar_SetToDefault("hud_scale");
	Cvar_SetToDefault("hud_alpha");
	Cvar_SetToDefault("hud_squeezedigits");
	Cvar_SetToDefault("cl_drawfps");

	ScreenSetMenuItemValues();
}

void Options_Screen_MenuInit(void)
{
	int y = MENU_LINE_SIZE * 3;

	s_options_screen_menu.x = SCREEN_WIDTH * 0.5f;
	s_options_screen_menu.y = DEFAULT_MENU_Y; //mxd. Was SCREEN_HEIGHT * 0.5f - 58;
	s_options_screen_menu.nitems = 0;

	s_options_screen_header.generic.type	= MTYPE_SEPARATOR;
	s_options_screen_header.generic.name	= "Screen";
	s_options_screen_header.generic.x		= MENU_FONT_SIZE / 2 * strlen(s_options_screen_header.generic.name);
	s_options_screen_header.generic.y		= 0;

	crosshair_names = SetCrosshairNames();
	s_options_screen_crosshair_box.generic.type				= MTYPE_SPINCONTROL;
	s_options_screen_crosshair_box.generic.x				= 0;
	s_options_screen_crosshair_box.generic.y				= y;
	s_options_screen_crosshair_box.generic.name				= "Crosshair";
	s_options_screen_crosshair_box.generic.callback			= CrosshairFunc;
	s_options_screen_crosshair_box.itemnames				= crosshair_names;
	s_options_screen_crosshair_box.generic.statusbar		= "Changes crosshair";

	// Psychospaz's changeable size crosshair
	s_options_screen_crosshairscale_slider.generic.type		= MTYPE_SLIDER;
	s_options_screen_crosshairscale_slider.generic.x		= 0;
	s_options_screen_crosshairscale_slider.generic.y		= y += MENU_LINE_SIZE;
	s_options_screen_crosshairscale_slider.generic.name		= "Crosshair scale";
	s_options_screen_crosshairscale_slider.generic.callback = CrosshairSizeFunc;
	s_options_screen_crosshairscale_slider.minvalue			= 1;
	s_options_screen_crosshairscale_slider.maxvalue			= 8;
	s_options_screen_crosshairscale_slider.numdecimals		= 2; //mxd
	s_options_screen_crosshairscale_slider.generic.statusbar = "Changes size of crosshair";
	s_options_screen_crosshairscale_slider.cvar				= crosshair_scale; //mxd

	s_options_screen_crosshairalpha_slider.generic.type		= MTYPE_SLIDER;
	s_options_screen_crosshairalpha_slider.generic.x		= 0;
	s_options_screen_crosshairalpha_slider.generic.y		= y += MENU_LINE_SIZE;
	s_options_screen_crosshairalpha_slider.generic.name		= "Crosshair alpha";
	s_options_screen_crosshairalpha_slider.generic.callback = CrosshairAlphaFunc;
	s_options_screen_crosshairalpha_slider.minvalue			= 1;
	s_options_screen_crosshairalpha_slider.maxvalue			= 20;
	s_options_screen_crosshairalpha_slider.numdecimals		= 2; //mxd
	s_options_screen_crosshairalpha_slider.generic.statusbar = "Changes opacity of crosshair";
	s_options_screen_crosshairalpha_slider.cvar				= crosshair_alpha; //mxd

	s_options_screen_crosshairpulse_slider.generic.type		= MTYPE_SLIDER;
	s_options_screen_crosshairpulse_slider.generic.x		= 0;
	s_options_screen_crosshairpulse_slider.generic.y		= y += MENU_LINE_SIZE;
	s_options_screen_crosshairpulse_slider.generic.name		= "Crosshair pulse";
	s_options_screen_crosshairpulse_slider.generic.callback = CrosshairPulseFunc;
	s_options_screen_crosshairpulse_slider.minvalue			= 0;
	s_options_screen_crosshairpulse_slider.maxvalue			= 10;
	s_options_screen_crosshairpulse_slider.numdecimals		= 2; //mxd
	s_options_screen_crosshairpulse_slider.generic.statusbar = "Changes pulse amplitude of crosshair";
	s_options_screen_crosshairpulse_slider.cvar				= crosshair_pulse; //mxd

	// hud scaling option
	s_options_screen_hudscale_slider.generic.type			= MTYPE_SLIDER;
	s_options_screen_hudscale_slider.generic.x				= 0;
	s_options_screen_hudscale_slider.generic.y				= y += 2 * MENU_LINE_SIZE;
	s_options_screen_hudscale_slider.generic.name			= "Status bar scale";
	s_options_screen_hudscale_slider.generic.callback		= HudScaleFunc;
	s_options_screen_hudscale_slider.minvalue				= 1;
	s_options_screen_hudscale_slider.maxvalue				= 7;
	s_options_screen_hudscale_slider.generic.statusbar		= "Changes size of HUD elements";
	s_options_screen_hudscale_slider.cvar					= hud_scale; //mxd

	// hud trans option
	s_options_screen_hudalpha_slider.generic.type			= MTYPE_SLIDER;
	s_options_screen_hudalpha_slider.generic.x				= 0;
	s_options_screen_hudalpha_slider.generic.y				= y += MENU_LINE_SIZE;
	s_options_screen_hudalpha_slider.generic.name			= "Status bar transparency";
	s_options_screen_hudalpha_slider.generic.callback		= HudAlphaFunc;
	s_options_screen_hudalpha_slider.minvalue				= 1;
	s_options_screen_hudalpha_slider.maxvalue				= 11;
	s_options_screen_hudalpha_slider.numdecimals			= 1; //mxd
	s_options_screen_hudalpha_slider.generic.statusbar		= "Changes opacity of HUD elements";
	s_options_screen_hudalpha_slider.cvar					= hud_alpha; //mxd

	// hud squeeze digits option
	s_options_screen_hudsqueezedigits_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_screen_hudsqueezedigits_box.generic.x			= 0;
	s_options_screen_hudsqueezedigits_box.generic.y			= y += MENU_LINE_SIZE;
	s_options_screen_hudsqueezedigits_box.generic.name		= "Status bar digit squeezing";
	s_options_screen_hudsqueezedigits_box.generic.callback	= HudSqueezeDigitsFunc;
	s_options_screen_hudsqueezedigits_box.itemnames			= yesno_names;
	s_options_screen_hudsqueezedigits_box.generic.statusbar	= "Enables showing of longer numbers on HUD";

	//mxd. Print objectives option
	s_options_screen_objectives_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_screen_objectives_box.generic.x			= 0;
	s_options_screen_objectives_box.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_options_screen_objectives_box.generic.name		= "Display new objectives";
	s_options_screen_objectives_box.generic.callback	= PrintObjectivesFunc;
	s_options_screen_objectives_box.itemnames			= yesno_names;
	s_options_screen_objectives_box.generic.statusbar	= "Print new objectives on screen instead of blinking the Help icon";

	s_options_screen_fps_box.generic.type				= MTYPE_SPINCONTROL;
	s_options_screen_fps_box.generic.x					= 0;
	s_options_screen_fps_box.generic.y					= y += MENU_LINE_SIZE;
	s_options_screen_fps_box.generic.name				= "FPS counter";
	s_options_screen_fps_box.generic.callback			= FPSFunc;
	s_options_screen_fps_box.itemnames					= yesno_names;
	s_options_screen_fps_box.generic.statusbar			= "Enables FPS counter";

	s_options_screen_defaults_action.generic.type		= MTYPE_ACTION;
	s_options_screen_defaults_action.generic.flags		= QMF_LEFT_JUSTIFY; //mxd
	s_options_screen_defaults_action.generic.name		= "Reset to defaults";
	s_options_screen_defaults_action.generic.x			= -MENU_FONT_SIZE * strlen(s_options_screen_defaults_action.generic.name); //mxd. Was MENU_FONT_SIZE;
	s_options_screen_defaults_action.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_options_screen_defaults_action.generic.callback	= ScreenResetDefaultsFunc;
	s_options_screen_defaults_action.generic.statusbar	= "Resets all screen settings to internal defaults";

	s_options_screen_back_action.generic.type			= MTYPE_ACTION;
	s_options_screen_back_action.generic.flags			= QMF_LEFT_JUSTIFY; //mxd
	s_options_screen_back_action.generic.name			= (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_OPTIONS); //mxd
	s_options_screen_back_action.generic.x				= UI_CenteredX(&s_options_screen_back_action.generic, s_options_screen_menu.x); //mxd. Was MENU_FONT_SIZE;
	s_options_screen_back_action.generic.y				= y += 2 * MENU_LINE_SIZE;
	s_options_screen_back_action.generic.callback		= UI_BackMenu;

	//mxd. Crosshair preview position
	crosshair_frame_x = s_options_screen_menu.x + MENU_FONT_SIZE * 18;
	crosshair_frame_y = s_options_screen_menu.y + s_options_screen_crosshairalpha_slider.generic.y - 2 - crosshair_frame_size / 2;

	Menu_AddItem(&s_options_screen_menu, (void *)&s_options_screen_header);
	Menu_AddItem(&s_options_screen_menu, (void *)&s_options_screen_crosshair_box);
	Menu_AddItem(&s_options_screen_menu, (void *)&s_options_screen_crosshairscale_slider);
	Menu_AddItem(&s_options_screen_menu, (void *)&s_options_screen_crosshairalpha_slider);
	Menu_AddItem(&s_options_screen_menu, (void *)&s_options_screen_crosshairpulse_slider);
	Menu_AddItem(&s_options_screen_menu, (void *)&s_options_screen_hudscale_slider);
	Menu_AddItem(&s_options_screen_menu, (void *)&s_options_screen_hudalpha_slider);
	Menu_AddItem(&s_options_screen_menu, (void *)&s_options_screen_hudsqueezedigits_box);
	Menu_AddItem(&s_options_screen_menu, (void *)&s_options_screen_objectives_box); //mxd
	Menu_AddItem(&s_options_screen_menu, (void *)&s_options_screen_fps_box);
	Menu_AddItem(&s_options_screen_menu, (void *)&s_options_screen_defaults_action);
	Menu_AddItem(&s_options_screen_menu, (void *)&s_options_screen_back_action);

	ScreenSetMenuItemValues();
}

void MenuCrosshair_MouseClick(void)
{
	buttonmenuobject_t crosshairbutton;

	UI_AddButton(&crosshairbutton, 0, crosshair_frame_x, crosshair_frame_y, crosshair_frame_size, crosshair_frame_size);

	if (   cursor.x >= crosshairbutton.min[0] && cursor.x <= crosshairbutton.max[0]
		&& cursor.y >= crosshairbutton.min[1] && cursor.y <= crosshairbutton.max[1])
	{
		char *sound = NULL;
		
		if (!cursor.buttonused[MOUSEBUTTON1] && cursor.buttonclicks[MOUSEBUTTON1] == 1)
		{
			s_options_screen_crosshair_box.curvalue++;
			if (s_options_screen_crosshair_box.curvalue > numcrosshairs - 1)
				s_options_screen_crosshair_box.curvalue = 0; // Wrap around
			CrosshairFunc(NULL);

			cursor.buttonused[MOUSEBUTTON1] = true;
			cursor.buttonclicks[MOUSEBUTTON1] = 0;
			sound = menu_move_sound;
		}
		else if (!cursor.buttonused[MOUSEBUTTON2] && cursor.buttonclicks[MOUSEBUTTON2] == 1)
		{
			s_options_screen_crosshair_box.curvalue--;
			if (s_options_screen_crosshair_box.curvalue < 0)
				s_options_screen_crosshair_box.curvalue = numcrosshairs - 1; // Wrap around
			CrosshairFunc(NULL);

			cursor.buttonused[MOUSEBUTTON2] = true;
			cursor.buttonclicks[MOUSEBUTTON2] = 0;
			sound = menu_move_sound;
		}

		if (sound)
			S_StartLocalSound(sound);

		//mxd. Mark "crosshair" spin control as selected item
		s_options_screen_menu.cursor = 1;
	}
}

void DrawMenuCrosshair(void)
{
	SCR_DrawFill(crosshair_frame_x + 0, crosshair_frame_y + 0, crosshair_frame_size - 0, crosshair_frame_size - 0, ALIGN_CENTER, 60, 60, 60, 255);
	SCR_DrawFill(crosshair_frame_x + 1, crosshair_frame_y + 1, crosshair_frame_size - 2, crosshair_frame_size - 2, ALIGN_CENTER, 0, 0, 0, 255);

	if (s_options_screen_crosshair_box.curvalue < 1)
		return;

	//mxd. Let's use crosshair_scale, crosshair_alpha and crosshair_pulse cvars when drawing the preview.
	const float scaledsize = crosshair_scale->value * CROSSHAIR_SIZE;
	const float pulsealpha = crosshair_alpha->value * crosshair_pulse->value;
	float alpha = crosshair_alpha->value - pulsealpha + pulsealpha * sinf(anglemod(cl.time * 0.005f));
	alpha = clamp(alpha, 0.0f, 1.0f);

	const float offset = (crosshair_frame_size - scaledsize) * 0.5f;

	SCR_DrawPic(crosshair_frame_x + offset, crosshair_frame_y + offset, scaledsize, scaledsize, ALIGN_CENTER, crosshair_names[s_options_screen_crosshair_box.curvalue], alpha);
}

void Options_Screen_MenuDraw(void)
{
	Menu_DrawBanner("m_banner_options");

	Menu_AdjustCursor(&s_options_screen_menu, 1);
	Menu_Draw(&s_options_screen_menu);
	DrawMenuCrosshair();
}

const char *Options_Screen_MenuKey(int key)
{
	return Default_MenuKey(&s_options_screen_menu, key);
}

void M_Menu_Options_Screen_f(void)
{
	Options_Screen_MenuInit();
	UI_PushMenu(Options_Screen_MenuDraw, Options_Screen_MenuKey);
}