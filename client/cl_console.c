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

// console.c

#include "client.h"

console_t con;

cvar_t *con_notifytime;
cvar_t *con_alpha;		// Knightare- Psychospaz's transparent console
cvar_t *con_newconback;	// Whether to use new console background
cvar_t *con_oldconbar;	// Whether to draw bottom bar on old console
qboolean newconback_found = false; // Whether to draw Q3-style console

extern char key_lines[32][MAXCMDLINE];
extern int edit_line;
extern int key_linepos;


void Con_DrawString(int x, int y, char *string, int alpha)
{
	DrawStringGeneric(x, y, string, alpha, SCALETYPE_CONSOLE, false);
}

void Key_ClearTyping(void)
{
	key_lines[edit_line][1] = 0; // Clear any typing
	key_linepos = 1;
	con.backedit = 0;
}

void Con_ToggleConsole_f(void)
{
	// Knightmare- allow disconnected menu
	if (cls.state == ca_disconnected && cls.key_dest != key_menu)
	{
		// Start the demo loop again
		Cbuf_AddText("d1\n");
		return;
	}

	Key_ClearTyping();
	Con_ClearNotify();

	// Knightmare changed
	if (Cvar_VariableValue("maxclients") == 1 && Com_ServerState() && cls.key_dest != key_menu)
		Cvar_Set("paused", (cls.consoleActive ? "0" : "1"));

	cls.consoleActive = !cls.consoleActive; //mxd
}

void Con_ToggleChat_f(void)
{
	Key_ClearTyping();

	if (cls.consoleActive) // Knightmare added
	{
		if (cls.state == ca_active)
		{
			UI_ForceMenuOff();
			cls.consoleActive = false; // Knightmare added
			cls.key_dest = key_game;
		}
	}
	else
	{
		cls.consoleActive = true; // Knightmare added
	}

	Con_ClearNotify();
}

void Con_Clear_f(void)
{
	memset(con.text, ' ', CON_TEXTSIZE);

	//mxd. Also reset display line and curent line...
	con.display = con.current = con.totallines;
}

// Save the console contents out to a file
void Con_Dump_f(void)
{
	int l, x;
	char *line;
	char buffer[1024];
	char name[MAX_OSPATH];

	if (Cmd_Argc() > 2)
	{
		Com_Printf("Usage: condump <filename>\n");
		return;
	}

	if (con.linewidth >= 1024) //mxd. From YQ2
	{
		Com_Printf("%s: con.linewidth too large!\n", __func__);
		return;
	}

	char *filename = (Cmd_Argc() == 1 ? "condump" : Cmd_Argv(1)); //mxd. +Default filename
	Com_sprintf(name, sizeof(name), "%s/%s.txt", FS_Gamedir(), filename);

	FS_CreatePath(name);
	FILE *f = fopen(name, "w");
	if (!f)
	{
		Com_Printf("ERROR: couldn't open '%s'.\n", name);
		return;
	}

	// Skip empty lines
	for (l = con.current - con.totallines + 1; l <= con.current; l++)
	{
		line = con.text + (l % con.totallines) * con.linewidth;

		for (x = 0; x < con.linewidth; x++)
			if (line[x] != ' ')
				break;

		if (x != con.linewidth)
			break;
	}

	// Write the remaining lines
	buffer[con.linewidth] = 0;
	for (; l <= con.current; l++)
	{
		line = con.text + (l % con.totallines) * con.linewidth;
		strncpy(buffer, line, con.linewidth);

		for (x = con.linewidth - 1; x >= 0; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}

		fprintf(f, "%s\n", CL_UnformattedString(buffer)); //mxd. Strip color markers...
	}

	fclose(f);

	// Done
	Com_Printf("Dumped console text to %s.\n", name);
}

void Con_ClearNotify(void)
{
	for (int i = 0; i < NUM_CON_TIMES; i++)
		con.times[i] = 0;
}

void Con_MessageMode_f(void)
{
	chat_team = false;
	cls.key_dest = key_message;
	cls.consoleActive = false; // Knightmare added
}

void Con_MessageMode2_f(void)
{
	chat_team = true;
	cls.key_dest = key_message;
	cls.consoleActive = false; // Knightmare added
}

// If the line width has changed, reformat the buffer.
void Con_CheckResize(void)
{
	const int fontsize = ((con_font_size && con_font_size->integer) ? (int)FONT_SIZE : 8); //mxd
	int width = viddef.width / fontsize - 2;

	if (width == con.linewidth)
		return;

	if (width < 1) // video hasn't been initialized yet
	{
		width = 78; // was 38
		con.linewidth = width;
		con.backedit = 0;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		memset(con.text, ' ', CON_TEXTSIZE);
	}
	else
	{
		const int oldwidth = con.linewidth;
		con.linewidth = width;
		con.backedit = 0;

		const int oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;

		const int numlines = min(con.totallines, oldtotallines); //mxd
		const int numchars = min(con.linewidth, oldwidth); //mxd

		char tbuf[CON_TEXTSIZE];
		memcpy(tbuf, con.text, CON_TEXTSIZE);
		memset(con.text, ' ', CON_TEXTSIZE);

		for (int i = 0; i < numlines; i++)
			for (int j = 0; j < numchars; j++)
				con.text[(con.totallines - 1 - i) * con.linewidth + j] = tbuf[((con.current - i + oldtotallines) % oldtotallines) * oldwidth + j];

		Con_ClearNotify();
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}

void Con_Init(void)
{
	con.linewidth = -1;
	con.backedit = 0;

	Con_CheckResize();
	Com_Printf("Console initialized.\n");

	// Register our commands
	con_font_size = Cvar_Get("con_font_size", "8", CVAR_ARCHIVE); //mxd. Moved from CL_InitLocal()
	con_notifytime = Cvar_Get("con_notifytime", "4", 0); // Knightmare- increased for fade
	con_alpha = Cvar_Get("con_alpha", "0.5", CVAR_ARCHIVE); // Knightmare- Psychospaz's transparent console
	con_newconback = Cvar_Get("con_newconback", "0", CVAR_ARCHIVE);	// whether to use new console background
	con_oldconbar = Cvar_Get("con_oldconbar", "1", CVAR_ARCHIVE);	// whether to draw bottom bar on old console

	// Whether to use new-style console background
	newconback_found = false;
	if (   (FS_LoadFile("gfx/ui/newconback.tga", NULL) != -1)
		|| (FS_LoadFile("gfx/ui/newconback.png", NULL) != -1)
		|| (FS_LoadFile("gfx/ui/newconback.jpg", NULL) != -1))
		newconback_found = true;

	Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand("togglechat", Con_ToggleChat_f);
	Cmd_AddCommand("messagemode", Con_MessageMode_f);
	Cmd_AddCommand("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand("clear", Con_Clear_f);
	Cmd_AddCommand("condump", Con_Dump_f);

	con.initialized = true;
}

void Con_Linefeed(void)
{
	con.x = 0;
	if (con.display == con.current)
		con.display++;

	con.current++;
	memset(&con.text[(con.current % con.totallines) * con.linewidth], ' ', con.linewidth);
}

// Handles cursor positioning, line wrapping, etc.
// All console printing must go through this in order to be logged to disk.
// If no console is visible, the text will appear at the top of the game window.
void Con_Print(char *text)
{
	int c, l;
	static qboolean cr;
	int mask;

	if (!con.initialized)
		return;

	if (text[0] == 1 || text[0] == 2)
	{
		mask = 128; // Go to colored text
		text++;
	}
	else
	{
		mask = 0;
	}

	while ((c = *text))
	{
		// Count word length
		for (l = 0; l < con.linewidth; l++)
			if (text[l] <= ' ')
				break;

		// Word wrap
		if (l != con.linewidth && con.x + l > con.linewidth)
			con.x = 0;

		text++;

		if (cr)
		{
			con.current--;
			cr = false;
		}

		if (!con.x)
		{
			Con_Linefeed();
			
			// Mark time for transparent overlay
			if (con.current >= 0)
				con.times[con.current % NUM_CON_TIMES] = (float)cls.realtime;
		}

		switch (c)
		{
			case '\n':
				con.x = 0;
				break;

			case '\r':
				con.x = 0;
				cr = true;
				break;

			default: // Display character and advance
			{
				const int y = con.current % con.totallines;
				con.text[y * con.linewidth + con.x] = c | mask | con.ormask;
				con.x++;
				if (con.x >= con.linewidth)
					con.x = 0;
				break;
			}
		}
	}
}

void Con_CenteredPrint(char *text)
{
	char buffer[1024];

	int len = strlen(text);
	len = (con.linewidth - len) / 2;
	len = max(len, 0); //mxd

	memset(buffer, ' ', len);
	Q_strncpyz(buffer + len, text, sizeof(buffer) - 1);
	Q_strncatz(buffer, "\n", sizeof(buffer));
	Con_Print(buffer);
}

int Con_LinesOnScreen() //mxd
{
	return (int)((con.vislines - (2.75f * FONT_SIZE)) / FONT_SIZE); // Lines of text to draw
}

int Con_FirstLine() //mxd
{
	// Find the first line with text...
	for (int l = con.current - con.totallines + 1; l <= con.current; l++)
	{
		char *line = con.text + (l % con.totallines) * con.linewidth;

		for (int x = 0; x < con.linewidth; x++)
			if (line[x] != ' ')
				return l;
	}

	return con.current; // We wrapped around / buffer is empty
}


/*
==============================================================================
DRAWING
==============================================================================
*/

// The input line scrolls horizontally if typing goes beyond the right edge
void Con_DrawInput(void)
{
	// Don't draw anything (always draw if not active)
	if (!cls.consoleActive && cls.state == ca_active)
		return;	

	char *text = key_lines[edit_line];
	
	// Add the cursor frame
	if (con.backedit)
		text[key_linepos] = ' ';
	else
		text[key_linepos] = 10 + ((int)(cls.realtime >> 8) & 1);
	
	// Fill out remainder with spaces
	for (int i = key_linepos + 1; i < con.linewidth; i++)
		text[i] = ' ';
		
	// Prestep if horizontally scrolling
	if (key_linepos >= con.linewidth)
		text += 1 + key_linepos - con.linewidth;
		
	// Draw it
	char output[2048];
	Com_sprintf(output, sizeof(output), "");
	for (int i = 0; i < con.linewidth; i++)
	{
		if (con.backedit == key_linepos - i && ((int)(cls.realtime >> 8) & 1))
			Com_sprintf(output, sizeof(output), "%s%c", output, 11);
		else
			Com_sprintf(output, sizeof(output), "%s%c", output, text[i]);
	}

	Con_DrawString((int)(FONT_SIZE / 2), con.vislines - (int)(2.75f * FONT_SIZE), output, 255);

	// Remove cursor
	key_lines[edit_line][key_linepos] = 0;
}

// Draws the last few lines of output transparently over the game top
void Con_DrawNotify(void)
{
	int x;
	char output[2048];
	int i, j;
	float time;

	int lines = 0;
	int v = 0;

	Com_sprintf(output, sizeof(output), "");

	// This is the say msg while typeing...
	if (cls.key_dest == key_message)
	{
		char *target = (chat_team ? " say_team: " : " say: "); //mxd
		Com_sprintf(output, sizeof(output), "%s", target);

		char *s = chat_buffer;
		x = 0;

		const int len = (int)((viddef.width / FONT_SIZE) - (strlen(output) + 1)); //mxd
		if (chat_bufferlen > len)
			x += chat_bufferlen - len;

		while(s[x])
		{
			if (chat_backedit && chat_backedit == chat_bufferlen - x && ((int)(cls.realtime >> 8) & 1))
				Com_sprintf(output, sizeof(output), "%s%c", output, 11);
			else
				Com_sprintf(output, sizeof(output), "%s%c", output, (char)s[x]);

			x++;
		}

		if (!chat_backedit)
			Com_sprintf(output, sizeof(output), "%s%c", output, 10 + ((int)(cls.realtime >> 8) & 1));

		Con_DrawString(0, v, output, 255);

		v += (int)(FONT_SIZE * 2); // Make extra space so we have room
	}

	for (i = con.current - NUM_CON_TIMES + 1; i <= con.current; i++)
	{
		if (i < 0)
			continue;

		time = con.times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;

		time = cls.realtime - time;
		if (time > con_notifytime->value * 1000)
			continue;

		// Vertical offset set by closest to dissapearing
		lines++;
	}

	if (lines == 0)
		return;

	for (j = 0, i = con.current - NUM_CON_TIMES + 1; i <= con.current; i++, j++)
	{
		if (i < 0)
			continue;

		time = con.times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;

		time = cls.realtime - time;
		if (time > con_notifytime->value * 1000)
			continue;

		char *text = con.text + (i % con.totallines) * con.linewidth;
			
		int alpha = (int)(255 * sqrtf((1.0f - time / (con_notifytime->value * 1000.0f + 1.0f)) * (v + 8.0f) / (8.0f * lines)));
		alpha = clamp(alpha, 0, 255); //mxd

		Com_sprintf(output, sizeof(output), "");
		for (x = 0; x < con.linewidth; x++)
			Com_sprintf(output, sizeof(output), "%s%c", output, (char)text[x]);

		Con_DrawString((int)(FONT_SIZE / 2), v, output, alpha);

		v += (int)FONT_SIZE;
	}
}

// Draws the console with the solid background
void Con_DrawConsole(float heightratio, qboolean transparent)
{
	int		x, len;
	char	*text, output[1024];
	char	dlbar[1024];
	float	barwidth, barheight; //Knightmare added
	
	// Changeable download bar color
	int		red, green, blue;

	// Q3-style console bottom bar
	TextColor((int)alt_text_color->value, &red, &green, &blue);
	if ((newconback_found && con_newconback->value) || con_oldconbar->value)
	{
		barwidth = SCREEN_WIDTH;
		barheight = 2;
		SCR_AdjustFrom640(NULL, NULL, &barwidth, &barheight, ALIGN_STRETCH);
	}
	else
	{
		barwidth = 0;
		barheight = 0;
	}

	int lines = (int)(viddef.height * heightratio);
	if (lines <= 0)
		return;

	if (lines > viddef.height)
		lines = viddef.height;

	// Psychospaz's transparent console
	float alpha = 1.0f;
	if(transparent)
	{
		if (newconback_found && con_newconback->value)
			alpha = con_alpha->value;
		else
			alpha = heightratio * con_alpha->value * 2;
	}

	// Draw the background
	int y = lines - (int)barheight;
	if (y < 1)
		y = 0;
	else if (newconback_found && con_newconback->value)	// Q3-style console
		R_DrawStretchPic(0, 0, viddef.width, y, "/gfx/ui/newconback.pcx", alpha);
	else
		R_DrawStretchPic(0, (int)(lines - viddef.width * 0.75f - barheight), viddef.width, (int)(viddef.width * 0.75f), "conback", alpha); //mxd. Preserve aspect ratio (width * 0.75 == width / 4 * 3)

	// Changed to "KMQuake2 vx.xx"
	char version[64];
	Com_sprintf(version, sizeof(version), S_COLOR_BOLD S_COLOR_SHADOW S_COLOR_ALT"%s v%4.2f%s", ENGINE_NAME, VERSION, ENGINE_BUILD); //mxd. +ENGINE_NAME, +VERSION, +ENGINE_BUILD
	Con_DrawString((int)(viddef.width - FONT_SIZE * (CL_UnformattedStringLength((const char *)&version)) - 3), y - (int)(1.25f * FONT_SIZE), version, 255);

	if ((newconback_found && con_newconback->value) || con_oldconbar->value) // Q3-style console bottom bar
		R_DrawFill(0, y, (int)barwidth, (int)barheight, red, green, blue, 255);

	// Draw the text
	con.vislines = lines;
	int rows = Con_LinesOnScreen(); //mxd // Lines of text to draw
	y = lines - (int)(3.75f * FONT_SIZE);

	// Draw from the bottom up
	if (con.display != con.current)
	{
		// Draw arrows to show the buffer is backscrolled
		for (x = 0; x < con.linewidth; x += 4)
			R_DrawChar((x + 1) * FONT_SIZE, (float)y, '^', CON_FONT_SCALE, 255, 0, 0, 255, false, x + 4 >= con.linewidth);
	
		y -= (int)FONT_SIZE;
		rows--;
	}
	
	int row = con.display;
	for (int i = 0; i < rows; i++, y -= (int)FONT_SIZE, row--)
	{
		// Before || past scrollback wrap point
		if (row < 0 || con.current - row >= con.totallines)
			break;
			
		text = con.text + (row % con.totallines) * con.linewidth;

		Com_sprintf(output, sizeof(output), "");
		for (x = 0; x < con.linewidth; x++)
			Com_sprintf(output, sizeof(output), "%s%c", output, text[x]);

		Con_DrawString(4, y, output, 255);
	}

//ZOID- draw the download bar
#ifdef USE_CURL	// HTTP downloading from R1Q2
	if (cls.downloadname[0] && (cls.download || cls.downloadposition))
#else
	if (cls.download)
#endif	// USE_CURL
	{
		if ((text = strrchr(cls.downloadname, '/')) != NULL)
			text++;
		else
			text = cls.downloadname;

		memset(dlbar, 0, sizeof(dlbar)); // Clear dlbar

		// Make this a little shorter in case of longer version string
		x = con.linewidth - ((con.linewidth * 7) / 40) - (CL_UnformattedStringLength((const char *)&version) - 14);
		y = x - strlen(text) - (cls.downloadrate > 0.0f ? 19 : 8);

		const int maxchars = con.linewidth / 3;
		if ((int)strlen(text) > maxchars)
		{
			y = x - maxchars - 11;
			strncpy(dlbar, text, maxchars);
			dlbar[maxchars] = 0;
			Q_strncatz(dlbar, "...", sizeof(dlbar));
		}
		else
		{
			Q_strncpyz(dlbar, text, sizeof(dlbar));
		}

		Q_strncatz(dlbar, ": ", sizeof(dlbar));

		len = strlen(dlbar); //mxd
		
		// Init solid color download bar
		int graph_x = (int)((len + 1) * FONT_SIZE);
		int graph_y = (int)(con.vislines - (FONT_SIZE * 1.5f) - barheight); // was -12
		int graph_w = (int)(y * FONT_SIZE);
		int graph_h = (int)FONT_SIZE;

		for (int i = 0; i < y; i++) // Add blank spaces
			Com_sprintf(dlbar + len, sizeof(dlbar) - len, " ");

		if (cls.downloadrate > 0.0f)
			Com_sprintf(dlbar + len, sizeof(dlbar) - len, " %2d%% (%4.2fKB/s)", cls.downloadpercent, cls.downloadrate);
		else
			Com_sprintf(dlbar + len, sizeof(dlbar) - len, " %2d%%", cls.downloadpercent);

		// Draw it
		for (int i = 0; i < len; i++)
			if (dlbar[i] != ' ')
				R_DrawChar((i + 1) * FONT_SIZE, (float)graph_y, dlbar[i], CON_FONT_SCALE, 255, 255, 255, 255, false, i == (len - 1));

		// New solid color download bar
		graph_x--;
		graph_y--;
		graph_w += 2;
		graph_h += 2;

		R_DrawFill(graph_x, graph_y, graph_w, graph_h, 255, 255, 255, 90);
		R_DrawFill((int)(graph_x + graph_h * 0.2f), (int)(graph_y + graph_h * 0.2f),
				   (int)((graph_w - graph_h * 0.4f) * cls.downloadpercent * 0.01f), (int)(graph_h * 0.6f),
					red, green, blue, 255);
	}
//ZOID

	// Draw the input prompt, user text, and cursor if desired
	Con_DrawInput();
}