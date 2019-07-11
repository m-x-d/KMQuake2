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

// Console

#pragma once

#define NUM_CON_TIMES	8 // was 4
#define CON_TEXTSIZE	65536 //mxd. Was 32768
#define CON_MAXCMDS		1024 //mxd
#define MAXCMDLINE		1024 // Max length of console command line
							 // Increased from 256, fixes buffer overflow if vert res > 2048
							 // Allows max vert res of 8192 for fullscreen console

enum commandtype_t //mxd
{
	TYPE_COMMAND,
	TYPE_ALIAS,
	TYPE_CVAR
};

typedef struct //mxd
{
	char *command;
	enum commandtype_t type;
} matchingcommand_t;

typedef struct
{
	qboolean initialized;

	char	text[CON_TEXTSIZE]; // Initially filled with space char.
	int		currentline;	// Line where next message will be printed. Can exceed text[] size.
	int		offsetx;		// Offset in current line for next print
	int		displayline;	// Bottom of console displays this line

	int		ormask;			// High bit mask for colored characters. 0 or 128

	int 	linewidth;		// Max. number of characters to fit in a single line
	int		totallines;		// Total text lines in console scrollback
	int		backedit;		// Text input cursor position, relative to the last entered character

	int		height;			// Total console height, in pixels. Changes when console is lowering/raising 

	float	notifytimes[NUM_CON_TIMES]; // cls.realtime time the line was generated. Used to draw transparent notify lines

	//mxd. Command auto-completion.
	matchingcommand_t commands[CON_MAXCMDS];
	char	*partialmatch;	// Text used to trigger autocompletion.
	int		commandscount;
	int		currentcommand;
	int		hintstartline;	// First line at which autocompletion hint was printed.
	int		hintendline;	// Last line at which autocompletion hint was printed.
} console_t;

extern console_t con;

void Con_CheckResize(void);
void Con_Init(void);
void Con_Shutdown(void); //mxd. From Q2E
void Con_DrawConsole(float heightratio, qboolean transparent); //Knightmare changed
void Con_KeyDown(int key); //mxd
void Con_Clear_f(void);
void Con_DrawNotify(void);
void Con_ClearNotify(void);
void Con_ToggleConsole_f(void);