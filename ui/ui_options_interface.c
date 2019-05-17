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

// ui_options_interface.c -- the interface options menu

#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"

static menuframework_s	s_options_interface_menu;
static menuseparator_s	s_options_interface_header;
static menuslider_s		s_options_interface_conalpha_slider;
//static menuslider_s		s_options_interface_conheight_slider;
static menuslider_s		s_options_interface_menumouse_slider;
static menuslider_s		s_options_interface_menualpha_slider;
static menulist_s		s_options_interface_font_box;
static menuslider_s		s_options_interface_fontsize_slider;
static menulist_s		s_options_interface_alt_text_color_box;
static menulist_s		s_options_interface_simple_loadscreen_box;
static menulist_s		s_options_interface_newconback_box;
static menuaction_s		s_options_interface_defaults_action;
static menuaction_s		s_options_interface_back_action;

#pragma region ======================= Menu item callbacks

static void MouseMenuFunc(void *unused)
{
	Cvar_SetValue("menu_sensitivity", s_options_interface_menumouse_slider.curvalue / 4.0f);
}

// Menu alpha option
static void MenuAlphaFunc(void *unused)
{
	Cvar_SetValue("menu_alpha", s_options_interface_menualpha_slider.curvalue / 20.0f);
}

static void AltTextColorFunc(void *unused)
{
	Cvar_SetValue("alt_text_color", s_options_interface_alt_text_color_box.curvalue);
}

// Psychospaz's transparent console
static void ConAlphaFunc(void *unused)
{
	Cvar_SetValue("con_alpha", s_options_interface_conalpha_slider.curvalue * 0.05f);
}

// variable console height
/*static void ConHeightFunc( void *unused )
{
	Cvar_SetValue( "con_height", 0.25 + (s_options_interface_conheight_slider.curvalue * 0.05) );
}*/

static void SimpleLoadscreenFunc(void *unused)
{
	Cvar_SetValue("scr_simple_loadscreen", s_options_interface_simple_loadscreen_box.curvalue);
}

static void NewConbackFunc(void *unused)
{
	Cvar_SetValue("con_newconback", s_options_interface_newconback_box.curvalue);
}

#pragma endregion 

#pragma region ======================= Font loading

cvar_t *con_font;
#define MAX_FONTS 32
char **font_names;
int numfonts;

static void FontSizeFunc(void *unused)
{
	Cvar_SetValue("con_font_size", s_options_interface_fontsize_slider.curvalue * 2);
}

static void FontFunc(void *unused)
{
	Cvar_Set("con_font", font_names[s_options_interface_font_box.curvalue]);
}

void SetFontCursor(void)
{
	s_options_interface_font_box.curvalue = 0;

	if (!con_font)
		con_font = Cvar_Get("con_font", "default", CVAR_ARCHIVE);

	if (numfonts > 1)
	{
		for (int i = 0; font_names[i]; i++)
		{
			if (!Q_strcasecmp(con_font->string, font_names[i]))
			{
				s_options_interface_font_box.curvalue = i;
				return;
			}
		}
	}
}

void InsertFont(char **list, char *insert, int len)
{
	if (!list)
		return;

	//i = 1 so default stays first!
	for (int i = 1; i < len; i++)
	{
		if (!list[i])
			break;

		if (strcmp(list[i], insert))
		{
			for (int j = len; j > i; j--)
				list[j] = list[j - 1];

			list[i] = strdup(insert);
			return;
		}
	}

	list[len] = strdup(insert);
}

char **SetFontNames(void)
{
	char *curFont;
	char *p;
	char findname[1024];
	char *path = NULL;

	char** list = malloc(sizeof(char *) * MAX_FONTS);
	memset(list, 0, sizeof(char *) * MAX_FONTS);

	list[0] = strdup("default");

	int nfonts = 0;
	int nfontnames = 1;

	path = FS_NextPath(path);
	while (path) 
	{
		Com_sprintf(findname, sizeof(findname), "%s/fonts/*.*", path);
		char **fontfiles = FS_ListFiles(findname, &nfonts, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);

		for (int i = 0; i < nfonts && nfontnames < MAX_FONTS; i++)
		{
			if (!fontfiles || !fontfiles[i]) // Knightmare added array base check
				continue;

			p = strstr(fontfiles[i], "/fonts/");
			p++;

			p = strchr(p, '/'); //mxd. strstr -> strchr
			p++;

			if (!strstr(p, ".tga") && !strstr(p, ".png") && !strstr(p, ".jpg") && !strstr(p, ".pcx"))
				continue;

			const int num = strlen(p) - 4;
			p[num] = 0;

			curFont = p;

			if (!FS_ItemInList(curFont, nfontnames, list))
			{
				InsertFont(list, strdup(curFont), nfontnames);
				nfontnames++;
			}
			
			// Set back so whole string get deleted.
			p[num] = '.';
		}

		if (nfonts)
			FS_FreeFileList(fontfiles, nfonts);
		
		path = FS_NextPath(path);
	}

	// Check pak after
	char **fontfiles = FS_ListPak("fonts/", &nfonts);
	if (fontfiles)
	{
		for (int i = 0; i < nfonts && nfontnames < MAX_FONTS; i++)
		{
			if (!fontfiles[i]) // Knightmare added array base check
				continue;

			p = strchr(fontfiles[i], '/'); //mxd. strstr -> strchr
			p++;

			if (!strstr(p, ".tga") && !strstr(p, ".png") && !strstr(p, ".jpg") && !strstr(p, ".pcx"))
				continue;

			const int num = strlen(p) - 4;
			p[num] = 0;

			curFont = p;

			if (!FS_ItemInList(curFont, nfontnames, list))
			{
				InsertFont(list, strdup(curFont), nfontnames);
				nfontnames++;
			}
			
			// Set back so whole string get deleted.
			p[num] = '.';
		}
	}

	if (nfonts)
		FS_FreeFileList(fontfiles, nfonts);

	numfonts = nfontnames;

	return list;
}

#pragma endregion 

static void InterfaceSetMenuItemValues(void)
{
	SetFontCursor();

	s_options_interface_menumouse_slider.curvalue = Cvar_VariableValue("menu_sensitivity") * 4;
	s_options_interface_menualpha_slider.curvalue = Cvar_VariableValue("menu_alpha") * 20;
	s_options_interface_fontsize_slider.curvalue =  Cvar_VariableValue("con_font_size") * 0.5f;

	Cvar_SetValue("alt_text_color", ClampCvar(0, 9, Cvar_VariableValue("alt_text_color")));
	s_options_interface_alt_text_color_box.curvalue = Cvar_VariableValue("alt_text_color");

	Cvar_SetValue("con_alpha", ClampCvar(0, 1, Cvar_VariableValue("con_alpha")));
	s_options_interface_conalpha_slider.curvalue = Cvar_VariableValue("con_alpha") * 20;

	//Cvar_SetValue( "con_height", ClampCvar( 0.25, 0.75, Cvar_VariableValue("con_height") ) );
	//s_options_interface_conheight_slider.curvalue		= 20 * (Cvar_VariableValue("con_height") - 0.25);

	Cvar_SetValue("scr_simple_loadscreen", ClampCvar(0, 1, Cvar_VariableValue("scr_simple_loadscreen")));
	s_options_interface_simple_loadscreen_box.curvalue = Cvar_VariableValue("scr_simple_loadscreen");

	Cvar_SetValue("con_newconback", ClampCvar(0, 1, Cvar_VariableValue("con_newconback")));
	s_options_interface_newconback_box.curvalue = Cvar_VariableValue("con_newconback");
}

static void InterfaceResetDefaultsFunc(void *unused)
{
	Cvar_SetToDefault("menu_sensitivity");
	Cvar_SetToDefault("menu_alpha");
	Cvar_SetToDefault("con_font");
	Cvar_SetToDefault("con_font_size");
	Cvar_SetToDefault("alt_text_color");
	Cvar_SetToDefault("con_alpha");
	//	Cvar_SetToDefault ("con_height");	
	Cvar_SetToDefault("scr_simple_loadscreen");
	Cvar_SetToDefault("con_newconback");

	InterfaceSetMenuItemValues();
}

void Options_Interface_MenuInit(void)
{
	static const char *textcolor_names[] = { "Gray", "Red", "Green", "Yellow", "Blue", "Cyan", "Magenta", "White", "Black", "Orange", 0 };

	int y = MENU_LINE_SIZE * 3;

	s_options_interface_menu.x = SCREEN_WIDTH * 0.5f;
	s_options_interface_menu.y = DEFAULT_MENU_Y; //mxd. Was SCREEN_HEIGHT * 0.5f - 58;
	s_options_interface_menu.nitems = 0;

	s_options_interface_header.generic.type		= MTYPE_SEPARATOR;
	s_options_interface_header.generic.name		= "Interface";
	s_options_interface_header.generic.x		= MENU_FONT_SIZE / 2 * strlen(s_options_interface_header.generic.name);
	s_options_interface_header.generic.y		= 0;

	// Knightmare- Psychospaz's menu mouse support
	s_options_interface_menumouse_slider.generic.type		= MTYPE_SLIDER;
	s_options_interface_menumouse_slider.generic.x			= 0;
	s_options_interface_menumouse_slider.generic.y			= y;
	s_options_interface_menumouse_slider.generic.name		= "Mouse cursor speed";
	s_options_interface_menumouse_slider.generic.callback	= MouseMenuFunc;
	s_options_interface_menumouse_slider.minvalue			= 1;
	s_options_interface_menumouse_slider.maxvalue			= 8;
	s_options_interface_menumouse_slider.numdecimals		= 2; //mxd
	s_options_interface_menumouse_slider.generic.statusbar	= "Changes mouse sensitivity in menus";
	s_options_interface_menumouse_slider.cvar				= menu_sensitivity; //mxd

	s_options_interface_menualpha_slider.generic.type		= MTYPE_SLIDER;
	s_options_interface_menualpha_slider.generic.x			= 0;
	s_options_interface_menualpha_slider.generic.y			= y += MENU_LINE_SIZE;
	s_options_interface_menualpha_slider.generic.name		= "Menu background transparency";
	s_options_interface_menualpha_slider.generic.callback	= MenuAlphaFunc;
	s_options_interface_menualpha_slider.minvalue			= 0;
	s_options_interface_menualpha_slider.maxvalue			= 20;
	s_options_interface_menualpha_slider.numdecimals		= 2; //mxd
	s_options_interface_menualpha_slider.generic.statusbar	= "Changes opacity of menu background";
	s_options_interface_menualpha_slider.cvar				= menu_alpha; //mxd

	font_names = SetFontNames();
	s_options_interface_font_box.generic.type				= MTYPE_SPINCONTROL;
	s_options_interface_font_box.generic.x					= 0;
	s_options_interface_font_box.generic.y					= y += 2 * MENU_LINE_SIZE;
	s_options_interface_font_box.generic.name				= "Font";
	s_options_interface_font_box.generic.callback			= FontFunc;
	s_options_interface_font_box.itemnames					= font_names;
	s_options_interface_font_box.generic.statusbar			= "Changes console and menu text font";

	s_options_interface_fontsize_slider.generic.type		= MTYPE_SLIDER;
	s_options_interface_fontsize_slider.generic.x			= 0;
	s_options_interface_fontsize_slider.generic.y			= y += MENU_LINE_SIZE;
	s_options_interface_fontsize_slider.generic.name		= "Console font size";
	s_options_interface_fontsize_slider.generic.callback	= FontSizeFunc;
	s_options_interface_fontsize_slider.minvalue			= 3;
	s_options_interface_fontsize_slider.maxvalue			= 8;
	s_options_interface_fontsize_slider.generic.statusbar	= "Changes the size of console text";
	s_options_interface_fontsize_slider.cvar				= con_font_size; //mxd

	s_options_interface_alt_text_color_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_interface_alt_text_color_box.generic.x		= 0;
	s_options_interface_alt_text_color_box.generic.y		= y += MENU_LINE_SIZE;
	s_options_interface_alt_text_color_box.generic.name		= "Alternative text color";
	s_options_interface_alt_text_color_box.generic.callback	= AltTextColorFunc;
	s_options_interface_alt_text_color_box.itemnames		= textcolor_names;
	s_options_interface_alt_text_color_box.generic.statusbar= "Changes the color of highlighted text";

	s_options_interface_conalpha_slider.generic.type		= MTYPE_SLIDER;
	s_options_interface_conalpha_slider.generic.x			= 0;
	s_options_interface_conalpha_slider.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_options_interface_conalpha_slider.generic.name		= "Console transparency";
	s_options_interface_conalpha_slider.generic.callback	= ConAlphaFunc;
	s_options_interface_conalpha_slider.minvalue			= 0;
	s_options_interface_conalpha_slider.maxvalue			= 20;
	s_options_interface_conalpha_slider.numdecimals			= 2; //mxd
	s_options_interface_conalpha_slider.generic.statusbar	= "Changes opacity of console background";
	s_options_interface_conalpha_slider.cvar				= con_alpha; //mxd

	/*
	s_options_interface_conheight_slider.generic.type	= MTYPE_SLIDER;
	s_options_interface_conheight_slider.generic.x		= 0;
	s_options_interface_conheight_slider.generic.y		= y+=MENU_LINE_SIZE;
	s_options_interface_conheight_slider.generic.name	= "console height";
	s_options_interface_conheight_slider.generic.callback = ConHeightFunc;
	s_options_interface_conheight_slider.minvalue		= 0;
	s_options_interface_conheight_slider.maxvalue		= 10;
	*/

	s_options_interface_simple_loadscreen_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_interface_simple_loadscreen_box.generic.x			= 0;
	s_options_interface_simple_loadscreen_box.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_options_interface_simple_loadscreen_box.generic.name		= "Simple load screens";
	s_options_interface_simple_loadscreen_box.generic.callback	= SimpleLoadscreenFunc;
	s_options_interface_simple_loadscreen_box.itemnames			= yesno_names;
	s_options_interface_simple_loadscreen_box.generic.statusbar	= "Toggles simple map loading screens";

	s_options_interface_newconback_box.generic.type			= MTYPE_SPINCONTROL;
	s_options_interface_newconback_box.generic.x			= 0;
	s_options_interface_newconback_box.generic.y			= y += MENU_LINE_SIZE;
	s_options_interface_newconback_box.generic.name			= "New console background";
	s_options_interface_newconback_box.generic.callback		= NewConbackFunc;
	s_options_interface_newconback_box.itemnames			= yesno_names;
	s_options_interface_newconback_box.generic.statusbar	= "Enables Q3-style console background";

	s_options_interface_defaults_action.generic.type		= MTYPE_ACTION;
	s_options_interface_defaults_action.generic.flags		= QMF_LEFT_JUSTIFY; //mxd
	s_options_interface_defaults_action.generic.name		= "Reset to defaults";
	s_options_interface_defaults_action.generic.x			= -MENU_FONT_SIZE * strlen(s_options_interface_defaults_action.generic.name); //mxd;
	s_options_interface_defaults_action.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_options_interface_defaults_action.generic.callback	= InterfaceResetDefaultsFunc;
	s_options_interface_defaults_action.generic.statusbar	= "Resets all interface settings to internal defaults";

	s_options_interface_back_action.generic.type		= MTYPE_ACTION;
	s_options_interface_back_action.generic.flags		= QMF_LEFT_JUSTIFY; //mxd
	s_options_interface_back_action.generic.name		= (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_OPTIONS); //mxd
	s_options_interface_back_action.generic.x			= UI_CenteredX(&s_options_interface_back_action.generic, s_options_interface_menu.x); //mxd. Was MENU_FONT_SIZE;
	s_options_interface_back_action.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_options_interface_back_action.generic.callback	= UI_BackMenu;

	Menu_AddItem(&s_options_interface_menu, (void *)&s_options_interface_header);
	Menu_AddItem(&s_options_interface_menu, (void *)&s_options_interface_menumouse_slider);
	Menu_AddItem(&s_options_interface_menu, (void *)&s_options_interface_menualpha_slider);
	Menu_AddItem(&s_options_interface_menu, (void *)&s_options_interface_font_box);
	Menu_AddItem(&s_options_interface_menu, (void *)&s_options_interface_fontsize_slider);
	Menu_AddItem(&s_options_interface_menu, (void *)&s_options_interface_alt_text_color_box);
	Menu_AddItem(&s_options_interface_menu, (void *)&s_options_interface_conalpha_slider);
	//Menu_AddItem( &s_options_interface_menu, ( void * ) &s_options_interface_conheight_slider );
	Menu_AddItem(&s_options_interface_menu, (void *)&s_options_interface_simple_loadscreen_box);
	Menu_AddItem(&s_options_interface_menu, (void *)&s_options_interface_newconback_box);
	Menu_AddItem(&s_options_interface_menu, (void *)&s_options_interface_defaults_action);
	Menu_AddItem(&s_options_interface_menu, (void *)&s_options_interface_back_action);

	InterfaceSetMenuItemValues();
}

void Options_Interface_MenuDraw(void)
{
	Menu_DrawBanner("m_banner_options");
	Menu_AdjustCursor(&s_options_interface_menu, 1);
	Menu_Draw(&s_options_interface_menu);
}

const char *Options_Interface_MenuKey(int key)
{
	return Default_MenuKey(&s_options_interface_menu, key);
}

void M_Menu_Options_Interface_f(void)
{
	Options_Interface_MenuInit();
	UI_PushMenu(Options_Interface_MenuDraw, Options_Interface_MenuKey);
}