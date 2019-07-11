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

#include "client.h"

// Key up events are sent even if in console mode

int anykeydown;

int key_waiting;
char *keybindings[K_LAST];
qboolean consolekeys[K_LAST];	// If true, can't be rebound while in console
qboolean menubound[K_LAST];		// If true, can't be rebound while in menu
int keyshift[K_LAST];		// Key to map to if shift held down in console
int key_repeats[K_LAST];	// If > 1, it's autorepeating
qboolean keydown[K_LAST];

#pragma region ======================= KEYNAMES

typedef struct
{
	char *name;
	int keynum;
} keyname_t;

// Translates internal key representations into human readable strings.
keyname_t keynames[] =
{
	{"TAB", K_TAB},
	{ "ENTER", K_ENTER },
	{ "ESCAPE", K_ESCAPE },
	{ "SPACE", K_SPACE },
	{ "BACKSPACE", K_BACKSPACE },

	{ "COMMAND", K_COMMAND },
	{ "CAPSLOCK", K_CAPSLOCK },
	{ "POWER", K_POWER },
	{ "PAUSE", K_PAUSE },

	{ "UPARROW", K_UPARROW },
	{ "DOWNARROW", K_DOWNARROW },
	{ "LEFTARROW", K_LEFTARROW },
	{ "RIGHTARROW", K_RIGHTARROW },

	{ "ALT", K_ALT },
	{ "CTRL", K_CTRL },
	{ "SHIFT", K_SHIFT },

	{ "F1", K_F1 },
	{ "F2", K_F2 },
	{ "F3", K_F3 },
	{ "F4", K_F4 },
	{ "F5", K_F5 },
	{ "F6", K_F6 },
	{ "F7", K_F7 },
	{ "F8", K_F8 },
	{ "F9", K_F9 },
	{ "F10", K_F10 },
	{ "F11", K_F11 },
	{ "F12", K_F12 },

	{ "INS", K_INS },
	{ "DEL", K_DEL },
	{ "PGDN", K_PGDN },
	{ "PGUP", K_PGUP },
	{ "HOME", K_HOME },
	{ "END", K_END },

	{ "MOUSE1", K_MOUSE1 },
	{ "MOUSE2", K_MOUSE2 },
	{ "MOUSE3", K_MOUSE3 },
	{ "MOUSE4", K_MOUSE4 },
	{ "MOUSE5", K_MOUSE5 },

	{ "JOY1", K_JOY1 },
	{ "JOY2", K_JOY2 },
	{ "JOY3", K_JOY3 },
	{ "JOY4", K_JOY4 },
	{ "JOY5", K_JOY5 },
	{ "JOY6", K_JOY6 },
	{ "JOY7", K_JOY7 },
	{ "JOY8", K_JOY8 },
	{ "JOY9", K_JOY9 },
	{ "JOY10", K_JOY10 },
	{ "JOY11", K_JOY11 },
	{ "JOY12", K_JOY12 },
	{ "JOY13", K_JOY13 },
	{ "JOY14", K_JOY14 },
	{ "JOY15", K_JOY15 },
	{ "JOY16", K_JOY16 },
	{ "JOY17", K_JOY17 },
	{ "JOY18", K_JOY18 },
	{ "JOY19", K_JOY19 },
	{ "JOY20", K_JOY20 },
	{ "JOY21", K_JOY21 },
	{ "JOY22", K_JOY22 },
	{ "JOY23", K_JOY23 },
	{ "JOY24", K_JOY24 },
	{ "JOY25", K_JOY25 },
	{ "JOY26", K_JOY26 },
	{ "JOY27", K_JOY27 },
	{ "JOY28", K_JOY28 },
	{ "JOY29", K_JOY29 },
	{ "JOY30", K_JOY30 },
	{ "JOY31", K_JOY31 },
	{ "JOY32", K_JOY32 },

	{ "HAT_UP", K_HAT_UP },
	{ "HAT_RIGHT", K_HAT_RIGHT },
	{ "HAT_DOWN", K_HAT_DOWN },
	{ "HAT_LEFT", K_HAT_LEFT },

	{ "TRIG_LEFT", K_TRIG_LEFT },
	{ "TRIG_RIGHT", K_TRIG_RIGHT },

	{ "JOY_BACK", K_JOY_BACK },

	{ "AUX1", K_AUX1 },
	{ "AUX2", K_AUX2 },
	{ "AUX3", K_AUX3 },
	{ "AUX4", K_AUX4 },
	{ "AUX5", K_AUX5 },
	{ "AUX6", K_AUX6 },
	{ "AUX7", K_AUX7 },
	{ "AUX8", K_AUX8 },
	{ "AUX9", K_AUX9 },
	{ "AUX10", K_AUX10 },
	{ "AUX11", K_AUX11 },
	{ "AUX12", K_AUX12 },
	{ "AUX13", K_AUX13 },
	{ "AUX14", K_AUX14 },
	{ "AUX15", K_AUX15 },
	{ "AUX16", K_AUX16 },
	{ "AUX17", K_AUX17 },
	{ "AUX18", K_AUX18 },
	{ "AUX19", K_AUX19 },
	{ "AUX20", K_AUX20 },
	{ "AUX21", K_AUX21 },
	{ "AUX22", K_AUX22 },
	{ "AUX23", K_AUX23 },
	{ "AUX24", K_AUX24 },
	{ "AUX25", K_AUX25 },
	{ "AUX26", K_AUX26 },
	{ "AUX27", K_AUX27 },
	{ "AUX28", K_AUX28 },
	{ "AUX29", K_AUX29 },
	{ "AUX30", K_AUX30 },
	{ "AUX31", K_AUX31 },
	{ "AUX32", K_AUX32 },

	{ "KP_HOME", K_KP_HOME },
	{ "KP_UPARROW", K_KP_UPARROW },
	{ "KP_PGUP", K_KP_PGUP },
	{ "KP_LEFTARROW", K_KP_LEFTARROW },
	{ "KP_5", K_KP_5 },
	{ "KP_RIGHTARROW", K_KP_RIGHTARROW },
	{ "KP_END", K_KP_END },
	{ "KP_DOWNARROW", K_KP_DOWNARROW },
	{ "KP_PGDN", K_KP_PGDN },
	{ "KP_ENTER", K_KP_ENTER },
	{ "KP_INS", K_KP_INS },
	{ "KP_DEL", K_KP_DEL },
	{ "KP_SLASH", K_KP_SLASH },
	{ "KP_MINUS", K_KP_MINUS },
	{ "KP_PLUS", K_KP_PLUS },
	{ "KP_MULT", K_KP_MULT },

	{ "MWHEELUP", K_MWHEELUP },
	{ "MWHEELDOWN", K_MWHEELDOWN },

	{ "PAUSE", K_PAUSE },

	{ "SEMICOLON", ';' }, // Because a raw semicolon seperates commands

	{ NULL, 0 }
};

#pragma endregion

#pragma region ======================= Key message processing

qboolean chat_team;
char chat_buffer[MAXCMDLINE];
int chat_bufferlen = 0;
int chat_backedit = 0;

void Key_Message(int key)
{
	if (key == K_ENTER || key == K_KP_ENTER)
	{
		if (chat_team)
			Cbuf_AddText("say_team \"");
		else
			Cbuf_AddText("say \"");

		Cbuf_AddText(chat_buffer);
		Cbuf_AddText("\"\n");

		cls.key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		chat_backedit = 0;
	}
	else if (key == K_ESCAPE)
	{
		cls.key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		chat_backedit = 0;
	}
	else if (key == K_BACKSPACE)
	{
		if (!chat_bufferlen)
			return;

		if (chat_backedit)
		{
			if (chat_bufferlen - chat_backedit == 0)
				return;

			for (int i = chat_bufferlen - chat_backedit - 1; i < chat_bufferlen; i++)
				chat_buffer[i] = chat_buffer[i + 1];
		}

		chat_bufferlen--;
		chat_buffer[chat_bufferlen] = 0;
	}
	else if (key == K_DEL)
	{
		if (chat_bufferlen && chat_backedit)
		{
			for (int i = chat_bufferlen - chat_backedit; i < chat_bufferlen; i++)
				chat_buffer[i] = chat_buffer[i + 1];

			chat_backedit--;
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
	}
	else if (key == K_LEFTARROW)
	{
		if (chat_bufferlen)
		{
			chat_backedit++;
			chat_backedit = clamp(chat_backedit, 0, chat_bufferlen); //mxd
		}
	}
	else if (key == K_RIGHTARROW)
	{
		if (chat_bufferlen)
		{
			chat_backedit--;
			chat_backedit = clamp(chat_backedit, 0, chat_bufferlen); //mxd
		}
	}
	else if (key < 32 || key > 127 || chat_bufferlen == sizeof(chat_buffer) - 1)
	{
		// Non printable / all full
	}
	else
	{
		// Insert character...
		if (chat_backedit)
		{
			for (int i = chat_bufferlen; i > chat_bufferlen - chat_backedit; i--)
				chat_buffer[i] = chat_buffer[i - 1];

			chat_buffer[chat_bufferlen - chat_backedit] = key;
			chat_bufferlen++;
		}
		else
		{
			chat_buffer[chat_bufferlen++] = key;
		}

		chat_buffer[chat_bufferlen] = 0;
	}
}

#pragma endregion 

// Returns a key number to be used to index keybindings[] by looking at the given string.
// Single ascii characters return themselves, while the K_* names are matched up.
int Key_StringToKeynum(char *str)
{
	if (!str || !str[0])
		return -1;

	if (!str[1])
		return str[0];

	for (keyname_t *kn = keynames; kn->name; kn++)
		if (!Q_strcasecmp(str, kn->name))
			return kn->keynum;

	return -1;
}

// Returns a string (either a single ascii char, or a K_* name) for the given keynum.
// FIXME: handle quote special (general escape sequence?)
char *Key_KeynumToString(int keynum)
{
	static char tinystr[2];
	
	if (keynum == -1)
		return "<KEY NOT FOUND>";

	if (keynum > 32 && keynum < 127)
	{
		// Printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;

		return tinystr;
	}
	
	for (keyname_t *kn = keynames; kn->name; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}

void Key_SetBinding(int keynum, char *binding)
{
	if (keynum == -1)
		return;

	// Free old bindings
	if (keybindings[keynum])
	{
		Z_Free(keybindings[keynum]);
		keybindings[keynum] = NULL;
	}

	//mxd. Don't bind to empty string
	if (!binding[0])
		return;

	// Allocate memory for new binding
	const int len = strlen(binding);
	char *new = Z_Malloc(len + 1);
	Q_strncpyz(new, binding, len + 1);
	new[len] = 0;
	keybindings[keynum] = new;
}

void Key_Unbind_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: unbind <key> : remove commands from a key\n");
		return;
	}

	const int b = Key_StringToKeynum(Cmd_Argv(1));
	if (b == -1)
	{
		Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Key_SetBinding(b, "");
}

void Key_Unbindall_f(void)
{
	for (int i = 0; i < K_LAST; i++)
		if (keybindings[i])
			Key_SetBinding(i, "");
}

void Key_Bind_f(void)
{
	const int c = Cmd_Argc();
	if (c < 2)
	{
		Com_Printf("Usage: bind <key> [command] : attach a command to a key\n");
		return;
	}

	const int b = Key_StringToKeynum(Cmd_Argv(1));
	if (b == -1)
	{
		Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2)
	{
		if (keybindings[b])
			Com_Printf("\"%s\" = \"%s\"\n", Cmd_Argv(1), keybindings[b]);
		else
			Com_Printf("\"%s\" is not bound\n", Cmd_Argv(1));

		return;
	}
	
	// Copy the rest of the command line
	char cmd[1024];
	cmd[0] = 0; // Start out with a null string
	for (int i = 2; i < c; i++)
	{
		Q_strncatz(cmd, Cmd_Argv(i), sizeof(cmd));
		if (i != (c - 1))
			Q_strncatz(cmd, " ", sizeof(cmd));
	}

	Key_SetBinding(b, cmd);
}

// Writes lines containing "bind key value"
void Key_WriteBindings(FILE *f)
{
	for (int i = 0; i < K_LAST; i++)
		if (keybindings[i] && keybindings[i][0])
			fprintf(f, "bind %s \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
}

void Key_Bindlist_f(void)
{
	Com_Printf(S_COLOR_GREEN"Assigned keybinds:\n"); //mxd
	
	int numbinds = 0; //mxd
	for (int i = 0; i < K_LAST; i++)
	{
		if (keybindings[i] && keybindings[i][0])
		{
			Com_Printf("%s \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
			numbinds++; //mxd
		}
	}

	Com_Printf(S_COLOR_GREEN"Total: %i keybinds.\n", numbinds); //mxd
}

void Key_Init(void)
{
	// Init ascii characters in console mode
	for (int i = 32; i < 128; i++)
		consolekeys[i] = true;

	consolekeys[K_ENTER] = true;
	consolekeys[K_KP_ENTER] = true;
	consolekeys[K_TAB] = true;
	consolekeys[K_LEFTARROW] = true;
	consolekeys[K_KP_LEFTARROW] = true;
	consolekeys[K_RIGHTARROW] = true;
	consolekeys[K_KP_RIGHTARROW] = true;
	consolekeys[K_UPARROW] = true;
	consolekeys[K_KP_UPARROW] = true;
	consolekeys[K_DOWNARROW] = true;
	consolekeys[K_KP_DOWNARROW] = true;
	consolekeys[K_BACKSPACE] = true;
	consolekeys[K_HOME] = true;
	consolekeys[K_KP_HOME] = true;
	consolekeys[K_END] = true;
	consolekeys[K_KP_END] = true;
	consolekeys[K_PGUP] = true;
	consolekeys[K_KP_PGUP] = true;
	consolekeys[K_PGDN] = true;
	consolekeys[K_KP_PGDN] = true;
	consolekeys[K_KP_MULT] = true;

	consolekeys[K_SHIFT] = true;
	consolekeys[K_INS] = true;
	consolekeys[K_KP_INS] = true;
	consolekeys[K_DEL] = true; //mxd
	consolekeys[K_KP_DEL] = true;
	consolekeys[K_KP_SLASH] = true;
	consolekeys[K_KP_PLUS] = true;
	consolekeys[K_KP_MINUS] = true;
	consolekeys[K_KP_5] = true;

	consolekeys[K_MWHEELUP] = true;
	consolekeys[K_MWHEELDOWN] = true;
	consolekeys[K_MOUSE4] = true; //mxd
	consolekeys[K_MOUSE5] = true; //mxd

	consolekeys[(int)'`'] = false;
	consolekeys[(int)'~'] = false;

	for (int i = 0; i < K_LAST; i++)
		keyshift[i] = i;
	for (int i = 'a'; i <= 'z'; i++)
		keyshift[i] = i - 'a' + 'A';

	keyshift[(int)'1'] = '!';
	keyshift[(int)'2'] = '@';
	keyshift[(int)'3'] = '#';
	keyshift[(int)'4'] = '$';
	keyshift[(int)'5'] = '%';
	keyshift[(int)'6'] = '^';
	keyshift[(int)'7'] = '&';
	keyshift[(int)'8'] = '*';
	keyshift[(int)'9'] = '(';
	keyshift[(int)'0'] = ')';
	keyshift[(int)'-'] = '_';
	keyshift[(int)'='] = '+';
	keyshift[(int)','] = '<';
	keyshift[(int)'.'] = '>';
	keyshift[(int)'/'] = '?';
	keyshift[(int)';'] = ':';
	keyshift[(int)'\''] = '"';
	keyshift[(int)'['] = '{';
	keyshift[(int)']'] = '}';
	keyshift[(int)'`'] = '~';
	keyshift[(int)'\\'] = '|';

	menubound[K_ESCAPE] = true;

	for (int i = 0; i < 12; i++)
		menubound[K_F1 + i] = true;

	// Register our functions
	Cmd_AddCommand("bind", Key_Bind_f);
	Cmd_AddCommand("unbind", Key_Unbind_f);
	Cmd_AddCommand("unbindall", Key_Unbindall_f);
	Cmd_AddCommand("bindlist", Key_Bindlist_f);
}

// Called by the system between frames for both key up and key down events
// Should NOT be called during an interrupt!
extern int scr_draw_loading;

void Key_Event(int key, qboolean down)
{
	static qboolean shift_down = false; //mxd. Made local

	// Hack for modal presses
	if (key_waiting == -1)
	{
		if (down)
			key_waiting = key;

		return;
	}

	// Update auto-repeat status
	if (down)
	{
		key_repeats[key]++;

		if (key != K_BACKSPACE 
			&& key != K_UPARROW // Added from Quake2max
			&& key != K_DOWNARROW 
			&& key != K_LEFTARROW 
			&& key != K_RIGHTARROW
			&& key != K_PAUSE 
			&& key != K_PGUP 
			&& key != K_KP_PGUP 
			&& key != K_PGDN
			&& key != K_KP_PGDN
			&& key != K_DEL
			&& !(key >= 'a' && key <= 'z')
			&& key_repeats[key] > 1)
			return; // Ignore most autorepeats
			
		if (key >= 200 && !keybindings[key])
			Com_Printf("%s is unbound, hit F4 to set.\n", Key_KeynumToString(key));
	}
	else
	{
		key_repeats[key] = 0;
	}

	// Track Shift key status between calls
	if (key == K_SHIFT)
		shift_down = down;

	// Console key is hardcoded, so the user can never unbind it
	if (key == '`' || key == '~')
	{
		if (down)
			Con_ToggleConsole_f();

		return;
	}

	// While in attract loop all keys besides F1 to F12 (to allow quick load and the like) are treated like escape.
	// Knightmare changed
	if (!cls.consoleActive && cl.attractloop && cls.key_dest != key_menu
		&& !(key >= K_F1 && key <= K_F12) 
		&& cl.cinematictime > 0 && cls.realtime - cl.cinematictime > 1000)
		key = K_ESCAPE; //TODO: (mxd) restore ability to skip cinematic using any key, or at least don't show main menu after skipping

	// Menu key is hardcoded, so the user can never unbind it
	if (key == K_ESCAPE)
	{
		if (!down)
			return;

		// Knightmare- allow disconnected menu
		if (cls.state == ca_disconnected && cls.key_dest != key_menu) // Added from Quake2Max
		{
			SCR_EndLoadingPlaque();	// Get rid of loading plaque
			Cbuf_AddText("d1\n");
			cls.consoleActive = false;
			M_Menu_Main_f();

			return;
		}

		// Knightmare- skip cinematic
		if (cl.cinematictime > 0 && !cl.attractloop && cls.realtime - cl.cinematictime > 1000)
			SCR_FinishCinematic();

		if (cl.frame.playerstate.stats[STAT_LAYOUTS] && cls.key_dest == key_game)
		{
			// Put away help computer / inventory
			Cbuf_AddText("cmd putaway\n");

			return;
		}

		switch (cls.key_dest)
		{
			case key_message: Key_Message(key); break; // Close chat window/
			case key_menu:    UI_Keydown(key); break;  // Close menu or one layer up
		
			case key_game:
			case key_console: M_Menu_Main_f(); break;  // Pause game and / or leave console, break into the menu.

			default: Com_Error(ERR_FATAL, "Bad cls.key_dest");
		}

		return;
	}

	// Track if any key is down for BUTTON_ANY
	/* This is one of the most ugly constructs I've	found so far in Quake II. When the game is in
	the intermission, the player can press any key to end it and advance into the next level. It
	should be easy to figure out at server level if	a button is pressed. But somehow the developers
	decided, that they'll need special move state BUTTON_ANY to solve this problem. So there's
	this global variable anykeydown. If it's not 0, CL_FinishMove() encodes BUTTON_ANY into the
	button state. The server reads this value and sends it to gi->ClientThink() where it's used
	to determine if the intermission shall end.	Needless to say that this is the only consumer of BUTTON_ANY.

	Since we cannot alter the network protocol nor the server <-> game API, I'll leave things alone	and try to forget. */
	keydown[key] = down;
	if (down)
	{
		if (key_repeats[key] == 1)
			anykeydown++;
	}
	else
	{
		anykeydown--;
		anykeydown = max(anykeydown, 0);
	}

	// Key up events only generate commands if the game key binding is a button command (leading + sign).
	// These will occur even in console mode, to keep the character from continuing an action started before a console switch.
	// Button commands include the kenum as a parameter, so multiple downs can be matched with ups.
	const uint time = Sys_Milliseconds(); //mxd

	if (!down)
	{
		char cmd[1024];
		char *kb = keybindings[key];
		if (kb && kb[0] == '+')
		{
			Com_sprintf(cmd, sizeof(cmd), "-%s %i %i\n", kb + 1, key, time);
			Cbuf_AddText(cmd);
		}

		if (keyshift[key] != key)
		{
			kb = keybindings[keyshift[key]];
			if (kb && kb[0] == '+')
			{
				Com_sprintf(cmd, sizeof(cmd), "-%s %i %i\n", kb + 1, key, time);
				Cbuf_AddText(cmd);
			}
		}

		return;
	}

	// If not a consolekey, send to the interpreter no matter what mode is
	// Knightmare changed
	if ((cls.key_dest == key_menu && menubound[key]) || (cls.consoleActive && !consolekeys[key])
		|| (cls.key_dest == key_game && (cls.state == ca_active || !consolekeys[key]) && !cls.consoleActive))
	{
		char cmd[1024];
		char *kb = keybindings[key];
		if (kb)
		{
			if (kb[0] == '+')
			{
				// Button commands add keynum and time as a parm
				Com_sprintf(cmd, sizeof(cmd), "%s %i %i\n", kb, key, time);
				Cbuf_AddText(cmd);
			}
			else
			{
				Cbuf_AddText(kb);
				Cbuf_AddText("\n");
			}
		}

		return;
	}

	// Apply Shift key modifier
	if (shift_down)
		key = keyshift[key];

	if (cls.consoleActive) // Knightmare added
	{
		Con_KeyDown(key);
	}
	else if (!scr_draw_loading) // Added check from Quake2Max
	{
		switch (cls.key_dest)
		{
			case key_message: Key_Message(key); break;
			case key_menu:	  UI_Keydown(key); break;

			case key_game:
			case key_console: Con_KeyDown(key); break;

			default: Com_Error(ERR_FATAL, "Bad cls.key_dest");
		}
	} // end Knightmare
}

//mxd. From Q2E
qboolean Key_IsDown(int key)
{
	if (key < 0 || key > 255)
		return false;

	return keydown[key];
}

void Key_ClearStates(void)
{
	anykeydown = false;

	for (int i = 0; i < K_LAST; i++)
	{
		if (keydown[i] || key_repeats[i])
			Key_Event(i, false);

		keydown[i] = 0;
		key_repeats[i] = 0;
	}
}

//mxd. Converts numpad keys to input chars regardless fo NumLock state.
int Key_ConvertNumPadKey(int key)
{
	switch (key)
	{
		case K_KP_SLASH:		return '/';
		case K_KP_MULT:			return '*';
		case K_KP_MINUS:		return '-';
		case K_KP_PLUS:			return '+';
		case K_KP_HOME:			return '7';
		case K_KP_UPARROW:		return '8';
		case K_KP_PGUP:			return '9';
		case K_KP_LEFTARROW:	return '4';
		case K_KP_5:			return '5';
		case K_KP_RIGHTARROW:	return '6';
		case K_KP_END:			return '1';
		case K_KP_DOWNARROW:	return '2';
		case K_KP_PGDN:			return '3';
		case K_KP_INS:			return '0';
		case K_KP_DEL:			return '.';
		default:				return key;
	}
}