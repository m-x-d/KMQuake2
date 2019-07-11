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
// cl_input_sdl2.c -- SDL2 mouse and gamepad code

#include "client.h"
#include "../win32/sdlquake.h" //mxd

// The last time input events were processed. Used throughout the client.
int sys_frame_time;

#pragma region ======================= Mouse control

#define MULTI_CLICK_TIME	750 //mxd. Double-click if time in ms. between clicks is less than this.

extern cursor_t cursor;

cvar_t *m_acceleration;
cvar_t *m_filter;
cvar_t *autosensitivity;

static qboolean mlooking;

static int mouse_x;
static int mouse_y;

static qboolean	mouseactive; // False when not focus app

static void MLookDown_f(void)
{
	mlooking = true;
}

static void MLookUp_f(void)
{
	mlooking = false;
	if (!freelook->integer && lookspring->integer)
		IN_CenterView();
}

// Called when the window gains focus or changes in some way
static void ActivateMouse(void)
{
	if (mouseactive)
		return;

	mouseactive = true;

	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, (m_acceleration->integer ? "0" : "1"));
	GLimp_GrabInput(true);
	SDL_ShowCursor(0);
}

// Called when the window loses focus
static void DeactivateMouse(void)
{
	if (!mouseactive)
		return;

	mouseactive = false;

	GLimp_GrabInput(false);
	SDL_ShowCursor(1);
}

extern void UI_RefreshCursorMenu(void);
extern void UI_RefreshCursorLink(void);

static void StartupMouse(void)
{
	// Knightmare- added Psychospaz's menu mouse support
	UI_RefreshCursorMenu();
	UI_RefreshCursorLink();

	//mxd. Center cursor in screen
	cursor.x = viddef.width / 2;
	cursor.y = viddef.height / 2;

	cursor.mouseaction = false;
}

//mxd. Mouse buttons processing. Rewritten for SDL2
static void MouseButtonEvent(byte sdl_button, qboolean down)
{
	int menu_button_index = -1;
	int key;

	// Convert SDL2 mouse buttons to Q2
	switch (sdl_button)
	{
		case SDL_BUTTON_LEFT:
			key = K_MOUSE1;
			menu_button_index = 0;
			break;

		case SDL_BUTTON_RIGHT:
			key = K_MOUSE2;
			menu_button_index = 1;
			break;

		case SDL_BUTTON_MIDDLE:
			key = K_MOUSE3;
			break;

		case SDL_BUTTON_X1:
			key = K_MOUSE4;
			break;

		case SDL_BUTTON_X2:
			key = K_MOUSE5;
			break;

		default:
			return;
	}

	// Set menu cursor buttons
	if (cls.key_dest == key_menu && menu_button_index != -1)
	{
		cursor.buttonused[menu_button_index] = false;
		
		if (down)
		{
			// Mouse press down
			if (sys_frame_time - cursor.buttontime[menu_button_index] < MULTI_CLICK_TIME)
				cursor.buttonclicks[menu_button_index] = min(2, cursor.buttonclicks[menu_button_index] + 1);
			else
				cursor.buttonclicks[menu_button_index] = 1;

			cursor.buttontime[menu_button_index] = sys_frame_time;
			cursor.buttondown[menu_button_index] = true;
			cursor.mouseaction = true;
		}
		else
		{
			// Mouse let go
			cursor.buttondown[menu_button_index] = false;
			cursor.mouseaction = true;
		}
	}

	// Perform button actions
	//mxd. Moved below "set menu cursor buttons" block, because if Key_Event changes cls.key_dest to key_menu, the above block will immediately trigger mouse click action,
	// which will select "Options" menu after clicking during ID logo cinematic, because it happens to be at the center of the screen
	Key_Event(key, down);
}

static void MouseMove(usercmd_t *cmd)
{
	static int old_mouse_x = 0; //mxd. Made local
	static int old_mouse_y = 0;
	
	if (!autosensitivity)
		autosensitivity = Cvar_Get("autosensitivity", "1", CVAR_ARCHIVE);

	if (!mouseactive)
		return;

	// Interpolate mouse movement?
	if (m_filter->integer)
	{
		mouse_x = (mouse_x + old_mouse_x) * 0.5f;
		mouse_y = (mouse_y + old_mouse_y) * 0.5f;

		old_mouse_x = mouse_x;
		old_mouse_y = mouse_y;
	}

	// Now to set the menu cursor
	if (cls.key_dest == key_menu)
	{
		cursor.oldx = cursor.x;
		cursor.oldy = cursor.y;

		cursor.x += mouse_x * menu_sensitivity->value;
		cursor.y += mouse_y * menu_sensitivity->value;

		if (cursor.x != cursor.oldx || cursor.y != cursor.oldy)
			cursor.mouseaction = true;

		cursor.x = clamp(cursor.x, 0, viddef.width);
		cursor.y = clamp(cursor.y, 0, viddef.height);
	}
	else
	{
		cursor.oldx = 0;
		cursor.oldy = 0;

		//psychospaz - zooming in preserves sensitivity
		if (autosensitivity->value && cl.base_fov < 90)
		{
			mouse_x *= sensitivity->value * (cl.base_fov / 90.0f);
			mouse_y *= sensitivity->value * (cl.base_fov / 90.0f);
		}
		else
		{
			mouse_x *= sensitivity->value;
			mouse_y *= sensitivity->value;
		}

		// Add mouse X/Y movement to cmd
		if ((in_strafe.state & KEYSTATE_DOWN) || (lookstrafe->value && mlooking))
			cmd->sidemove += m_side->value * mouse_x;
		else
			cl.viewangles[YAW] -= m_yaw->value * mouse_x;

		if ((mlooking || freelook->value) && !(in_strafe.state & KEYSTATE_DOWN))
			cl.viewangles[PITCH] += m_pitch->value * mouse_y;
		else
			cmd->forwardmove -= m_forward->value * mouse_y;
	}

	// Mouse movement is relative, so reset it
	mouse_x = 0;
	mouse_y = 0;
}

#pragma endregion

#pragma region ======================= Joystick control

//TODO

#pragma endregion

#pragma region ======================= Input processing

cvar_t *v_centermove;
cvar_t *v_centerspeed;

// Translate SDL keycodes into Quake 2 interal representation.
static int TranslateSDLtoQ2Key(uint keysym)
{
	int key = 0;

	// These must be translated
	switch (keysym)
	{
		case SDLK_PAGEUP:		key = K_PGUP; break;
		case SDLK_KP_9:			key = K_KP_PGUP; break;
		case SDLK_PAGEDOWN:		key = K_PGDN; break;
		case SDLK_KP_3:			key = K_KP_PGDN; break;
		case SDLK_KP_7:			key = K_KP_HOME; break;
		case SDLK_HOME:			key = K_HOME; break;
		case SDLK_KP_1:			key = K_KP_END; break;
		case SDLK_END:			key = K_END; break;
		case SDLK_KP_4:			key = K_KP_LEFTARROW; break;
		case SDLK_LEFT:			key = K_LEFTARROW; break;
		case SDLK_KP_6:			key = K_KP_RIGHTARROW; break;
		case SDLK_RIGHT:		key = K_RIGHTARROW; break;
		case SDLK_KP_2:			key = K_KP_DOWNARROW; break;
		case SDLK_DOWN:			key = K_DOWNARROW; break;
		case SDLK_KP_8:			key = K_KP_UPARROW; break;
		case SDLK_UP:			key = K_UPARROW; break;
		case SDLK_ESCAPE:		key = K_ESCAPE; break;
		case SDLK_KP_ENTER:		key = K_KP_ENTER; break;
		case SDLK_RETURN:		key = K_ENTER; break;
		case SDLK_TAB:			key = K_TAB; break;
		case SDLK_F1:			key = K_F1; break;
		case SDLK_F2:			key = K_F2; break;
		case SDLK_F3:			key = K_F3; break;
		case SDLK_F4:			key = K_F4; break;
		case SDLK_F5:			key = K_F5; break;
		case SDLK_F6:			key = K_F6; break;
		case SDLK_F7:			key = K_F7; break;
		case SDLK_F8:			key = K_F8; break;
		case SDLK_F9:			key = K_F9; break;
		case SDLK_F10:			key = K_F10; break;
		case SDLK_F11:			key = K_F11; break;
		case SDLK_F12:			key = K_F12; break;
		case SDLK_F13:			key = K_F13; break;
		case SDLK_F14:			key = K_F14; break;
		case SDLK_F15:			key = K_F15; break;
		case SDLK_BACKSPACE:	key = K_BACKSPACE; break;
		case SDLK_KP_PERIOD:	key = K_KP_DEL; break;
		case SDLK_DELETE:		key = K_DEL; break;
		case SDLK_PAUSE:		key = K_PAUSE; break;
		case SDLK_LSHIFT:		key = K_SHIFT; break;
		case SDLK_RSHIFT:		key = K_SHIFT; break;
		case SDLK_LCTRL:		key = K_CTRL; break;
		case SDLK_RCTRL:		key = K_CTRL; break;
		case SDLK_LGUI:			key = K_COMMAND; break;
		case SDLK_RGUI:			key = K_COMMAND; break;
		case SDLK_RALT:			key = K_ALT; break;
		case SDLK_LALT:			key = K_ALT; break;
		case SDLK_KP_5:			key = K_KP_5; break;
		case SDLK_INSERT:		key = K_INS; break;
		case SDLK_KP_0:			key = K_KP_INS; break;
		case SDLK_KP_MULTIPLY:	key = K_KP_MULT; break;
		case SDLK_KP_PLUS:		key = K_KP_PLUS; break;
		case SDLK_KP_MINUS:		key = K_KP_MINUS; break;
		case SDLK_KP_DIVIDE:	key = K_KP_SLASH; break;
		case SDLK_MODE:			key = K_MODE; break;
		case SDLK_APPLICATION:	key = K_COMPOSE; break;
		case SDLK_HELP:			key = K_HELP; break;
		case SDLK_PRINTSCREEN:	key = K_PRINT; break;
		case SDLK_SYSREQ:		key = K_SYSREQ; break;
		case SDLK_MENU:			key = K_MENU; break;
		case SDLK_POWER:		key = K_POWER; break;
		case SDLK_UNDO:			key = K_UNDO; break;
		case SDLK_SCROLLLOCK:	key = K_SCROLLOCK; break;
		case SDLK_NUMLOCKCLEAR:	key = K_KP_NUMLOCK; break;
		case SDLK_CAPSLOCK:		key = K_CAPSLOCK; break;

		default: break;
	}

	return key;
}

void IN_Init(void)
{
	Com_Printf("\n------- Input initialization -------\n");

	mouse_x = 0;
	mouse_y = 0;

	// Mouse variables
	autosensitivity	= Cvar_Get("autosensitivity",	"1", CVAR_ARCHIVE);
	m_acceleration	= Cvar_Get("m_acceleration",	"1", CVAR_ARCHIVE);
	m_filter		= Cvar_Get("m_filter",			"0", CVAR_ARCHIVE);

	// Centering
	v_centermove	= Cvar_Get("v_centermove", "0.15", 0);
	v_centerspeed	= Cvar_Get("v_centerspeed", "500", 0);

	Cmd_AddCommand("+mlook", MLookDown_f);
	Cmd_AddCommand("-mlook", MLookUp_f);

	StartupMouse();
}

void IN_Shutdown(void)
{
	DeactivateMouse();

	Cmd_RemoveCommand("+mlook");
	Cmd_RemoveCommand("-mlook");
}

// Called when the main window gains or loses focus.
// The window may have been destroyed and recreated between a deactivate and an activate.
void IN_Activate(qboolean active)
{
	mouseactive = !active; // Force a new window check or turn off
}

// Called every frame, even if not generating commands
void IN_Update(void)
{
	// We need to save the frame time so other subsystems know the exact time of the last input events.
	sys_frame_time = Sys_Milliseconds();

	// Get and process input events
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_MOUSEWHEEL:
				Key_Event((event.wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), true);
				Key_Event((event.wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), false);
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				MouseButtonEvent(event.button.button, event.type == SDL_MOUSEBUTTONDOWN);
				break;
				
			case SDL_MOUSEMOTION:
				if (cls.key_dest == key_menu || (cls.key_dest == key_game && !cl_paused->integer))
				{
					mouse_x += event.motion.xrel;
					mouse_y += event.motion.yrel;
				}
				break;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
			{
				const qboolean down = (event.type == SDL_KEYDOWN);

				// Workaround for AZERTY-keyboards, which don't have 1, 2, ..., 9, 0 in first row:
				// always map those physical keys (scancodes) to those keycodes anyway
				// see also https://bugzilla.libsdl.org/show_bug.cgi?id=3188
				const SDL_Scancode sc = event.key.keysym.scancode;

				if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0)
				{
					// Note that the SDL_SCANCODEs are SDL_SCANCODE_1, _2, ..., _9, SDL_SCANCODE_0
					// while in ASCII it's '0', '1', ..., '9' => handle 0 and 1-9 separately
					// (quake2 uses the ASCII values for those keys)
					int key = '0'; // Implicitly handles SDL_SCANCODE_0

					if (sc <= SDL_SCANCODE_9)
						key = '1' + (sc - SDL_SCANCODE_1);

					Key_Event(key, down);
				}
				else
				{
					if (event.key.keysym.sym >= SDLK_SPACE && event.key.keysym.sym < SDLK_DELETE)
						Key_Event(event.key.keysym.sym, down);
					else
						Key_Event(TranslateSDLtoQ2Key(event.key.keysym.sym), down);
				}
			} break;

			case SDL_WINDOWEVENT:
				GLimp_WindowEvent(&event); //mxd. Handle on GLimp side
				break;

			case SDL_QUIT:
				Com_Quit();
				break;
		}
	}

	//Knightmare- added Psychospaz's mouse menu support //mxd. Don't show cursor during map loading
	if ((!cl.refresh_prepped && cls.key_dest != key_menu && !cls.disable_screen) || cls.consoleActive) // Mouse used in menus...
	{
		// Temporarily deactivate if not in fullscreen
		if (Cvar_VariableInteger("vid_fullscreen") == 0)
		{
			DeactivateMouse();
			return;
		}
	}

	ActivateMouse();
}

extern void UI_Think_MouseCursor(void);

void IN_Move(usercmd_t *cmd)
{
	if (!activeapp)
		return;

	MouseMove(cmd);

	// Knightmare- added Psychospaz's mouse support
	if (cls.key_dest == key_menu && !cls.consoleActive)
		UI_Think_MouseCursor();
}

//mxd //TODO: test this!
char* IN_GetClipboardData()
{
	char *data = NULL;

	if (SDL_HasClipboardText())
	{
		char *cliptext = SDL_GetClipboardText();
		
		const int len = (strlen(cliptext) + 1) * sizeof(char);
		data = malloc(len);
		Q_strncpyz(data, cliptext, len);

		SDL_free(cliptext);
	}

	return data;
}

#pragma endregion