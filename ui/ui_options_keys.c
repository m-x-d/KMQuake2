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

// ui_options_keys.c -- the key binding menu

#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"

char *bindnames[][2] =
{
	{"+forward",	"Forward"},
	{"+back",		"Back"},
	{"+moveleft",	"Move left"},
	{"+moveright",	"Move right"},
	{"+moveup",		"Jump"},
	{"+movedown",	"Crouch"},
	{"+speed", 		"Run (hold)"},

	{"+attack",		"Attack"},
	{"+attack2",	"Secondary attack"},
	{"weapnext",	"Next weapon"},
	{"weapprev",	"Previous weapon"},

	{"+use",		"Activate"},

	{"inven",		"Inventory"},
	{"invnext",		"Next item"},
	{"invprev",		"Previous item"},
	{"invuse",		"Use item"},
	{"invdrop",		"Drop item"},
	
	{"+left",		"Turn left"},
	{"+right",		"Turn right"},
	{"+strafe",		"Sidestep (hold)"},

	{"+lookup",		"Look up"},
	{"+lookdown",	"Look down"},
	{"centerview",	"Center view"},

	{"+mlook",		"Mouse look"},
	{"+klook",		"Keyboard look"},
	
	{"cmd help",	"Help computer"},
	{0, 0}
};

static menuframework_s s_keys_menu;
static menuaction_s s_keys_binds[64];
static menuaction_s s_keys_back_action;

static menuaction_s *bind_target = NULL; //mxd

static void M_UnbindCommand(char *command)
{
	for (int i = 0; i < K_LAST; i++)
	{
		char* binding = keybindings[i];

		if (binding && !Q_stricmp(binding, command)) //mxd. strncmp -> Q_stricmp
			Key_SetBinding(i, "");
	}
}

static void M_FindKeysForCommand(char *command, int *twokeys)
{
	int count = 0;

	twokeys[0] = -1;
	twokeys[1] = -1;

	for (int i = 0; i < K_LAST; i++)
	{
		char* binding = keybindings[i];

		if (binding && !Q_stricmp(binding, command)) //mxd. strncmp -> Q_stricmp
		{
			twokeys[count] = i;
			count++;

			if (count == 2)
				break;
		}
	}
}

//mxd
static void KeyStatusBarFunc(void *unused)
{
	if (bind_target) // Awaiting key press?
		Menu_DrawStatusBar("Press a key or a mouse button for this action");
	else // Default hint
		Menu_DrawStatusBar("Enter or LMB to change, Backspace or Delete to clear");
}

//mxd. Clear bind_target before closing the menu.
static void KeyBackMenuFunc(void *self)
{
	bind_target = NULL;
	UI_BackMenu(self);
}

// Selection cursor draw for keybind items
static void KeyCursorDrawFunc(menuaction_s *self)
{
	//mxd. If awaiting input, draw selection for bind_target instead of hovered item
	if(bind_target)
	{
		SCR_DrawChar(s_keys_menu.x + bind_target->generic.x, s_keys_menu.y + bind_target->generic.y, ALIGN_CENTER, '=', 255, 255, 255, 255, false, true);
	}
	else
	{
		const int num = 12 + ((Sys_Milliseconds() / 250) & 1); // Alternate between char 12 (empty Q2 font char) and char 13 ('>' Q2 font char)
		SCR_DrawChar(s_keys_menu.x + self->generic.x, s_keys_menu.y + self->generic.y, ALIGN_CENTER, num, 255, 255, 255, 255, false, true);
	}
}

static void DrawKeyBindingFunc(void *self)
{
	menuaction_s *a = (menuaction_s *)self;

	int keys[2];
	M_FindKeysForCommand(bindnames[a->generic.localdata[0]][0], keys);

	char* name;
	if (keys[0] == -1) // No keys bound
		name = "???";
	else if (keys[1] == -1) // Single key is bound 
		name = Key_KeynumToString(keys[0]);
	else // Both keys are bound
		name = va("%s or %s", Key_KeynumToString(keys[0]), Key_KeynumToString(keys[1]));

	Menu_DrawString(a->generic.x + a->generic.parent->x + 16,
					a->generic.y + a->generic.parent->y, name, 255);
}

static void KeyBindingFunc(void *self)
{
	bind_target = (menuaction_s *)self;
}

void AddBindOption(int index, char* list[][2])
{
	s_keys_binds[index].generic.type			= MTYPE_ACTION;
	s_keys_binds[index].generic.flags			= QMF_GRAYED;
	s_keys_binds[index].generic.x				= 0;
	s_keys_binds[index].generic.y				= index * MENU_LINE_SIZE;
	s_keys_binds[index].generic.ownerdraw		= DrawKeyBindingFunc;
	s_keys_binds[index].generic.localdata[0]	= index;
	s_keys_binds[index].generic.name			= list[s_keys_binds[index].generic.localdata[0]][1];
	s_keys_binds[index].generic.callback		= KeyBindingFunc;
	s_keys_binds[index].generic.cursordraw		= KeyCursorDrawFunc; //mxd
	s_keys_binds[index].generic.statusbarfunc	= KeyStatusBarFunc; //mxd
}

static void Keys_MenuInit(void)
{
	s_keys_menu.x = SCREEN_WIDTH * 0.5f;
	s_keys_menu.y = DEFAULT_MENU_Y; //mxd. Was SCREEN_HEIGHT * 0.5f - 72;
	s_keys_menu.nitems = 0;

	// Count number of binds
	int maxbinds = 0;
	while (bindnames[maxbinds][1])
		maxbinds++;

	for (int i = 0; i < maxbinds; i++)
		AddBindOption(i, bindnames);

	s_keys_back_action.generic.type			= MTYPE_ACTION;
	s_keys_back_action.generic.name			= (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_CONTROLS); //mxd
	s_keys_back_action.generic.flags		= QMF_LEFT_JUSTIFY;
	s_keys_back_action.generic.x			= UI_CenteredX(&s_keys_back_action.generic, s_keys_menu.x); //mxd. Was 0;
	s_keys_back_action.generic.y			= (maxbinds + 1) * MENU_LINE_SIZE;
	s_keys_back_action.generic.callback		= KeyBackMenuFunc; //mxd. Was UI_BackMenu;

	for (int i = 0; i < maxbinds; i++)
		Menu_AddItem(&s_keys_menu, (void *)&s_keys_binds[i]);

	Menu_AddItem(&s_keys_menu, (void *)&s_keys_back_action);
}

static void Keys_MenuDraw(void)
{
	Menu_DrawBanner("m_banner_customize"); // Knightmare added
	Menu_AdjustCursor(&s_keys_menu, 1);
	Menu_Draw(&s_keys_menu);
}

static const char *Keys_MenuKey(int key)
{
	menuaction_s *item = (menuaction_s *)Menu_ItemAtCursor(&s_keys_menu);
	
	// Pressing mouse1 to pick a new bind won't force bind/unbind itself - spaz
	if (bind_target && item != &s_keys_back_action && !(cursor.buttonused[MOUSEBUTTON1] && key == K_MOUSE1))
	{
		if (key != K_ESCAPE && key != '`')
		{
			//mxd. If both keys are already bound, unbind them first...
			int keys[2];
			M_FindKeysForCommand(bindnames[bind_target->generic.localdata[0]][0], keys);

			if (keys[1] != -1)
				M_UnbindCommand(bindnames[bind_target->generic.localdata[0]][0]);
			
			// Bind command to key
			char cmd[1024];
			Com_sprintf(cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n", Key_KeynumToString(key), bindnames[bind_target->generic.localdata[0]][0]);
			Cbuf_InsertText(cmd);
		}
		
		// Knightmare- added Psychospaz's mouse support
		// Don't let selecting with mouse buttons screw everything up
		UI_RefreshCursorButtons();
		if (key == K_MOUSE1)
			cursor.buttonclicks[MOUSEBUTTON1] = -1;

		//mxd. Clear bind target
		bind_target = NULL;

		return menu_out_sound;
	}

	switch (key)
	{
		case K_KP_ENTER:
		case K_ENTER:
			if (item == &s_keys_back_action) // Back action hack
			{
				UI_BackMenu(item);
				return NULL;
			}
			KeyBindingFunc(item);
			return menu_in_sound;

		case K_BACKSPACE: // Delete bindings
		case K_DEL:
		case K_KP_DEL:
			M_UnbindCommand(bindnames[item->generic.localdata[0]][0]);
			return menu_out_sound;

		default:
			return Default_MenuKey(&s_keys_menu, key);
	}
}

void M_Menu_Keys_f(void)
{
	Keys_MenuInit();
	UI_PushMenu(Keys_MenuDraw, Keys_MenuKey);
}