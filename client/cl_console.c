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

static qboolean newconback_found = false; // Whether to draw Q3-style console

#define NUM_KEY_LINES 32 //mxd

static char key_lines[NUM_KEY_LINES][MAXCMDLINE]; // Manually entered console commands are stored here
static int edit_line;
static int history_line;
static int key_linepos; // Current index into key_lines[]

static void Con_ClearTyping(void)
{
	key_lines[edit_line][1] = 0; // Clear any typing
	key_linepos = 1;
	con.backedit = 0;
}

#pragma region ======================= Console commands

void Con_ToggleConsole_f(void)
{
	// Knightmare- allow disconnected menu
	if (cls.state == ca_disconnected && cls.key_dest != key_menu)
	{
		// Start the demo loop again
		Cbuf_AddText("d1\n");
		return;
	}

	Con_ClearTyping();
	Con_ClearNotify();

	// Knightmare changed
	if (Cvar_VariableValue("maxclients") == 1 && Com_ServerState() && cls.key_dest != key_menu)
		Cvar_Set("paused", (cls.consoleActive ? "0" : "1"));

	cls.consoleActive = !cls.consoleActive; //mxd
}

static void Con_ToggleChat_f(void)
{
	Con_ClearTyping();

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
	con.displayline = con.currentline = con.totallines;

	//mxd. Also reset autocomplete hintlines...
	con.hintstartline = -1;
	con.hintendline = -1;
}

// Save the console contents out to a file
static void Con_Dump_f(void)
{
	if (Cmd_Argc() > 2)
	{
		Com_Printf("Usage: condump [filename]\n");
		return;
	}

	char *filename = (Cmd_Argc() == 1 ? ENGINE_PREFIX"condump" : Cmd_Argv(1)); //mxd. +Default filename

	char name[MAX_OSPATH];
	Com_sprintf(name, sizeof(name), "%s/%s.txt", FS_Gamedir(), filename);

	FS_CreatePath(name);
	FILE *f = fopen(name, "w");
	if (!f)
	{
		Com_Printf("ERROR: couldn't open '%s'.\n", name);
		return;
	}

	// Skip empty lines
	int lineindex;
	for (lineindex = con.currentline - con.totallines + 1; lineindex <= con.currentline; lineindex++)
	{
		char *line = con.text + (lineindex % con.totallines) * con.linewidth;

		int pos;
		for (pos = 0; pos < con.linewidth; pos++)
			if (line[pos] != ' ')
				break;

		if (pos != con.linewidth)
			break;
	}

	// Write the remaining lines
	for (; lineindex <= con.currentline; lineindex++)
	{
		char *line = con.text + (lineindex % con.totallines) * con.linewidth;

		char buffer[MAXCMDLINE];
		strncpy(buffer, line, con.linewidth);

		// Zero-out trailing spaces
		buffer[con.linewidth] = 0;
		for (int pos = con.linewidth - 1; pos >= 0; pos--)
		{
			if (buffer[pos] == ' ')
				buffer[pos] = 0;
			else
				break;
		}

		fprintf(f, "%s\n", CL_UnformattedString(buffer)); //mxd. Strip color markers...
	}

	fclose(f);

	// Done
	Com_Printf("Dumped console text to '%s'.\n", name);
}

static void Con_MessageMode_f(void)
{
	chat_team = false;
	cls.key_dest = key_message;
	cls.consoleActive = false; // Knightmare added
}

static void Con_MessageMode2_f(void)
{
	chat_team = true;
	cls.key_dest = key_message;
	cls.consoleActive = false; // Knightmare added
}

#pragma endregion 

#pragma region ======================= Printing / utility

void Con_ClearNotify(void)
{
	for (int i = 0; i < NUM_CON_TIMES; i++)
		con.notifytimes[i] = 0;
}

// If the line width has changed, reformat the buffer.
void Con_CheckResize(void)
{
	const int fontsize = ((con_font_size && con_font_size->integer) ? (int)FONT_SIZE : 8); //mxd
	int width = viddef.width / fontsize - 2;

	if (width == con.linewidth)
		return;

	//mxd. Check hardcoded limits...
	if(width > MAXCMDLINE)
	{
		Com_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: maximum supported console text width exceeded (%i / %i chars)!\n", __func__, width, MAXCMDLINE); //mxd
		width = MAXCMDLINE;
	}

	if (width < 1) // Video hasn't been initialized yet
	{
		width = 78; // Was 38
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
				con.text[(con.totallines - 1 - i) * con.linewidth + j] = tbuf[((con.currentline - i + oldtotallines) % oldtotallines) * oldwidth + j];

		Con_ClearNotify();
	}

	con.currentline = con.totallines - 1;
	con.displayline = con.currentline;
}

static qboolean Con_StringSetParams(char modifier, char *colorcode, qboolean *bold, qboolean *shadow, qboolean *italic, qboolean *alt)
{
	// Sanity check
	if (!colorcode || !bold || !shadow || !italic || !alt)
		return false;

	switch (modifier)
	{
		case 'R': case 'r':
			*colorcode = 0;
			*bold = false;
			*shadow = false;
			*italic = false;
			*alt = false;
			return true;

		case 'B': case 'b':
			*bold = !*bold;
			return true;

		case 'S': case 's':
			*shadow = !*shadow;
			return true;

		case 'I': case 'i':
			*italic = !*italic;
			return true;

		case COLOR_RED:
		case COLOR_GREEN:
		case COLOR_YELLOW:
		case COLOR_BLUE:
		case COLOR_CYAN:
		case COLOR_MAGENTA:
		case COLOR_WHITE:
		case COLOR_BLACK:
		case COLOR_ORANGE:
		case COLOR_GRAY:
			*colorcode = modifier;
			return true;

		case 'A': case 'a': // Alt text color
			*alt = true;
			return true;
	}

	return false;
}

//mxd
static void Con_AddFormattingSequence(const char c)
{
	const int linestart = (con.currentline % con.totallines) * con.linewidth;

	con.text[linestart + (con.offsetx++)] = Q_COLOR_ESCAPE;
	con.text[linestart + (con.offsetx++)] = c;
}

static void Con_Linefeed(const char colorcode, const qboolean bold, const qboolean shadow, const qboolean italic, const qboolean alt)
{
	con.offsetx = 0;
	if (con.displayline == con.currentline)
		con.displayline++;

	con.currentline++;
	memset(&con.text[(con.currentline % con.totallines) * con.linewidth], ' ', con.linewidth);

	// Add any wrapped formatting
	if (colorcode != 0)
		Con_AddFormattingSequence(colorcode);

	if (bold)
		Con_AddFormattingSequence('b');

	if (shadow)
		Con_AddFormattingSequence('s');

	if (italic)
		Con_AddFormattingSequence('i');

	if (alt)
		Con_AddFormattingSequence('a');
}

// Handles cursor positioning, line wrapping, etc.
// All console printing must go through this in order to be logged to disk.
// If no console is visible, the text will appear at the top of the game window.
void Con_Print(char *text)
{
	static qboolean carriage_return;

	if (!con.initialized)
		return;

	// Vars for format wrapping
	char colorcode = 0;
	qboolean checkmodifier = false;
	qboolean bold = false;
	qboolean shadow = false;
	qboolean italic = false;
	qboolean alt = false;

	int mask = 0;
	if (text[0] == 1 || text[0] == 2)
	{
		mask = 128; // Go to colored text
		text++;
	}

	// Add to con.text[], char by char
	while (*text)
	{
		// Count word length
		int len;
		for (len = 0; len < con.linewidth; len++)
			if (text[len] <= ' ')
				break;

		// Word wrap
		if (len < con.linewidth && con.offsetx + len > con.linewidth)
			con.offsetx = 0;

		// Clear current line without advancing to the next one?
		if (carriage_return)
		{
			con.currentline--;
			carriage_return = false;
		}

		if (con.offsetx == 0)
		{
			Con_Linefeed(colorcode, bold, shadow, italic, alt);

			// Mark time for transparent overlay
			if (con.currentline >= 0)
				con.notifytimes[con.currentline % NUM_CON_TIMES] = (float)cls.realtime;
		}

		// Track formatting codes for word wrap
		const char modifier = (text[0] & ~128);
		if (checkmodifier) // Last char was a ^
		{
			Con_StringSetParams(modifier, &colorcode, &bold, &shadow, &italic, &alt);
			checkmodifier = false;
		}
		else
		{
			checkmodifier = (modifier == Q_COLOR_ESCAPE);
		}

		switch (*text)
		{
			case '\r':
				carriage_return = true;
				//mxd. Intentional fallthrough
			
			case '\n':
				con.offsetx = 0;
				colorcode = 0;
				bold = false;
				shadow = false;
				italic = false;
				alt = false;
				break;

			default: // Display character and advance
			{
				const int pos = (con.currentline % con.totallines) * con.linewidth;
				con.text[pos + (con.offsetx++)] = *text | mask | con.ormask;
				if (con.offsetx >= con.linewidth) //mxd. Triggered only when a single word is longer than con.linewidth?
					con.offsetx = 0;
				break;
			}
		}

		// Advance to next char
		text++;
	}
}

static int Con_LinesOnScreen() //mxd
{
	return (int)((con.height - (2.75f * FONT_SIZE)) / FONT_SIZE); // Lines of text to draw
}

static int Con_FirstLine() //mxd
{
	// Find the first line with text...
	for (int l = con.currentline - con.totallines + 1; l <= con.currentline; l++)
	{
		char *line = con.text + (l % con.totallines) * con.linewidth;

		for (int x = 0; x < con.linewidth; x++)
			if (line[x] != ' ')
				return l;
	}

	return con.currentline; // We wrapped around / buffer is empty
}

#pragma endregion

#pragma region ======================= Drawing

void Con_DrawString(int x, int y, char *string, int alpha)
{
	DrawStringGeneric(x, y, string, alpha, SCALETYPE_CONSOLE, false);
}

// The input line scrolls horizontally if typing goes beyond the right edge
static void Con_DrawInput(void)
{
	// Don't draw anything (always draw if not active)
	if (!cls.consoleActive && cls.state == ca_active)
		return;	

	char *text = key_lines[edit_line];
	
	// Add the cursor frame
	if (con.backedit)
		text[key_linepos] = ' ';
	else
		text[key_linepos] = 10 + ((int)(cls.realtime >> 8) & 1); // Alternate between empty char (10) and cursor char (11)
	
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
		// Alternate between cursor char (11) and text char
		if (con.backedit == key_linepos - i && ((int)(cls.realtime >> 8) & 1))
			Com_sprintf(output, sizeof(output), "%s%c", output, 11);
		else
			Com_sprintf(output, sizeof(output), "%s%c", output, text[i]);
	}

	Con_DrawString((int)(FONT_SIZE / 2), con.height - (int)(2.75f * FONT_SIZE), output, 255);

	// Remove cursor
	key_lines[edit_line][key_linepos] = 0;
}

// Draws the last few lines of output transparently over the game top
void Con_DrawNotify(void)
{
	int yoffset = 0;

	char output[2048];
	Com_sprintf(output, sizeof(output), "");

	// This is the say msg while typing...
	if (cls.key_dest == key_message)
	{
		char *target = (chat_team ? " say_team: " : " say: "); //mxd
		Com_sprintf(output, sizeof(output), "%s", target);

		char *s = chat_buffer;
		int x = 0;

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

		Con_DrawString(0, yoffset, output, 255);

		yoffset += (int)(FONT_SIZE * 2); // Make extra space so we have room
	}

	for (int i = con.currentline - NUM_CON_TIMES + 1; i <= con.currentline; i++)
	{
		if (i < 0)
			continue;

		float time = con.notifytimes[i % NUM_CON_TIMES];
		if (time == 0)
			continue;

		time = cls.realtime - time;
		if (time > con_notifytime->value * 1000)
		{
			con.notifytimes[i % NUM_CON_TIMES] = 0; //mxd
			continue;
		}

		char *text = con.text + (i % con.totallines) * con.linewidth;
			
		int alpha = (int)(255 * sqrtf(1.0f - time / (con_notifytime->value * 1000.0f + 1.0f))); //mxd. Don't use yoffset / number of lines to modify alpha
		alpha = clamp(alpha, 0, 255); //mxd

		Com_sprintf(output, sizeof(output), "");
		for (int x = 0; x < con.linewidth; x++)
			Com_sprintf(output, sizeof(output), "%s%c", output, (char)text[x]);

		Con_DrawString((int)(FONT_SIZE / 2), yoffset, output, alpha);

		yoffset += (int)FONT_SIZE;
	}
}

// Draws the console with the solid background
void Con_DrawConsole(float heightratio, qboolean transparent)
{
	// Changeable download bar color
	int red, green, blue;
	TextColor(alt_text_color->integer, &red, &green, &blue);

	// Q3-style console bottom bar
	float barwidth, barheight; //Knightmare added
	if ((newconback_found && con_newconback->integer) || con_oldconbar->integer)
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

	const int consoleheight = (int)(viddef.height * min(heightratio, 1.0f));
	if (consoleheight <= 0)
		return;

	// Psychospaz's transparent console
	float alpha = 1.0f;
	if(transparent)
	{
		if (newconback_found && con_newconback->integer)
			alpha = con_alpha->value;
		else
			alpha = heightratio * con_alpha->value * 2;
	}

	// Draw the background
	int conbar_y = consoleheight - (int)barheight;
	if (conbar_y < 1)
		conbar_y = 0;
	else if (newconback_found && con_newconback->value)	// Q3-style console
		R_DrawStretchPic(0, 0, viddef.width, conbar_y, "/gfx/ui/newconback.pcx", alpha);
	else
		R_DrawStretchPic(0, (int)(consoleheight - viddef.width * 0.75f - barheight), viddef.width, (int)(viddef.width * 0.75f), "conback", alpha); //mxd. Preserve aspect ratio (width * 0.75 == width / 4 * 3)

	// Draw engine name, version and build
	char version[64];
	Com_sprintf(version, sizeof(version), S_COLOR_BOLD S_COLOR_SHADOW S_COLOR_ALT"%s v%4.2f%s", ENGINE_NAME, VERSION, ENGINE_BUILD); //mxd. +ENGINE_NAME, +VERSION, +ENGINE_BUILD
	Con_DrawString((int)(viddef.width - FONT_SIZE * (CL_UnformattedStringLength((const char *)&version)) - 3), conbar_y - (int)(1.25f * FONT_SIZE), version, 255);

	// Draw Q3-style console bottom bar?
	if ((newconback_found && con_newconback->integer) || con_oldconbar->integer)
		R_DrawFill(0, conbar_y, (int)barwidth, (int)barheight, red, green, blue, 255);

	// Draw the text
	con.height = consoleheight;
	int lines = Con_LinesOnScreen(); //mxd. Lines of text to draw
	int line_y = consoleheight - (int)(3.75f * FONT_SIZE);

	// Draw from the bottom up
	if (con.displayline != con.currentline)
	{
		// Draw arrows to show the buffer is backscrolled
		for (int x = 0; x < con.linewidth; x += 4)
			R_DrawChar((x + 1) * FONT_SIZE, (float)line_y, '^', CON_FONT_SCALE, 255, 0, 0, 255, false, x + 4 >= con.linewidth);
	
		line_y -= (int)FONT_SIZE;
		lines--;
	}
	
	// Draw text from the bottom up
	int curline = con.displayline;
	for (int i = 0; i < lines; i++, line_y -= (int)FONT_SIZE, curline--)
	{
		// Before || past scrollback wrap point
		if (curline < 0 || con.currentline - curline >= con.totallines)
			break;
			
		char *text = con.text + (curline % con.totallines) * con.linewidth;

		char output[1024];
		Com_sprintf(output, sizeof(output), "");

		for (int x = 0; x < con.linewidth; x++)
			Com_sprintf(output, sizeof(output), "%s%c", output, text[x]);

		Con_DrawString(4, line_y, output, 255);
	}

//ZOID- draw the download bar
#ifdef USE_CURL	// HTTP downloading from R1Q2
	if (cls.downloadname[0] && (cls.download || cls.downloadposition))
#else
	if (cls.download)
#endif	// USE_CURL
	{
		char *downloadname = strrchr(cls.downloadname, '/');
		if (downloadname != NULL)
			downloadname++;
		else
			downloadname = cls.downloadname;

		char dlbar[1024];
		memset(dlbar, 0, sizeof(dlbar)); // Clear dlbar

		// Make this a little shorter in case of longer version string
		const int x = con.linewidth - ((con.linewidth * 7) / 40) - (CL_UnformattedStringLength((const char *)&version) - 14);
		int dlbar_width = x - strlen(downloadname) - (cls.downloadrate > 0.0f ? 19 : 8);

		const int maxchars = con.linewidth / 3;
		if ((int)strlen(downloadname) > maxchars)
		{
			dlbar_width = x - maxchars - 11;
			strncpy(dlbar, downloadname, maxchars);
			dlbar[maxchars] = 0;
			Q_strncatz(dlbar, "...", sizeof(dlbar));
		}
		else
		{
			Q_strncpyz(dlbar, downloadname, sizeof(dlbar));
		}

		Q_strncatz(dlbar, ": ", sizeof(dlbar));

		const int len = strlen(dlbar); //mxd
		
		// Init solid color download bar
		int graph_x = (int)((len + 1) * FONT_SIZE);
		int graph_y = (int)(con.height - (FONT_SIZE * 1.5f) - barheight); // was -12

		for (int i = 0; i < dlbar_width; i++) // Add blank spaces
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

		const int graph_w = (int)(dlbar_width * FONT_SIZE) + 2;
		const int graph_h = (int)FONT_SIZE + 2;

		R_DrawFill(graph_x, graph_y, graph_w, graph_h, 255, 255, 255, 90);
		R_DrawFill((int)(graph_x + graph_h * 0.2f), (int)(graph_y + graph_h * 0.2f),
				   (int)((graph_w - graph_h * 0.4f) * cls.downloadpercent * 0.01f), (int)(graph_h * 0.6f),
					red, green, blue, 255);
	}
//ZOID

	// Draw the input prompt, user text, and cursor if desired
	Con_DrawInput();
}

#pragma endregion

#pragma region ======================= Command auto-completion

//mxd
static void Con_ClearAutocompleteList()
{
	for (int i = 0; i < con.commandscount; i++)
		FreeString(con.commands[i].command);

	if (con.partialmatch != NULL)
	{
		FreeString(con.partialmatch);
		con.partialmatch = NULL;
	}

	con.commandscount = 0;
	con.currentcommand = -1;
	con.hintstartline = -1;
	con.hintendline = -1;
}

//mxd
static void Con_CompleteCommandCallback(const char *found)
{
	if (con.commandscount < CON_MAXCMDS)
	{
		con.commands[con.commandscount].command = CopyString(found);
		con.commands[con.commandscount].type = TYPE_COMMAND;
		con.commandscount++;
	}
}

//mxd
static void Con_CompleteAliasCallback(const char *found)
{
	if (con.commandscount < CON_MAXCMDS)
	{
		con.commands[con.commandscount].command = CopyString(found);
		con.commands[con.commandscount].type = TYPE_ALIAS;
		con.commandscount++;
	}
}

//mxd
static void Con_CompleteVariableCallback(const char *found)
{
	if (con.commandscount < CON_MAXCMDS)
	{
		con.commands[con.commandscount].command = CopyString(found);
		con.commands[con.commandscount].type = TYPE_CVAR;
		con.commandscount++;
	}
}

//mxd
static int Con_SortCommands(const matchingcommand_t *first, const matchingcommand_t *second)
{
	return Q_stricmp(first->command, second->command);
}

//mxd
static void Con_PrintMatchingCommands()
{
	static char matchtypename[3][8] = { "command", "alias", "cvar" }; // Contents must match commandtype_t types
	static int prevcommand = -1; // con.currentcommand from previous call

	// Store initial display line...
	const int displayline = con.displayline;

	// Replace previous lines if no extra messages were printed
	if (con.hintendline == con.currentline)
		con.currentline = con.hintstartline;
	else
		con.hintstartline = con.currentline;

	const int len = strlen(con.partialmatch);
	
	// Print matches count
	Com_Printf("\n%i matches for \"%s\":\n", con.commandscount, con.partialmatch);

	// Print matches
	int currentcommandline = -1;
	for (int i = 0; i < con.commandscount; i++)
	{
		// Display cvar value
		char *value = "";
		if (con.commands[i].type == TYPE_CVAR)
			value = va(": \"%s\"", Cvar_FindVar(con.commands[i].command)->string);

		// Exact match?
		if (i == con.currentcommand || (con.currentcommand == -1 && !Q_strcasecmp(con.partialmatch, con.commands[i].command)))
		{
			Com_Printf(S_COLOR_GREEN">>%s (%s)%s\n", con.commands[i].command, matchtypename[con.commands[i].type], value);
			con.currentcommand = i;
			currentcommandline = con.currentline;
		}
		else // Partial match. Highlight matching part
		{
			char *matchchar = Q_strcasestr(con.commands[i].command, con.partialmatch);

			char before[64], match[64], after[64];
			Q_strncpyz(before, con.commands[i].command, matchchar - con.commands[i].command + 1);
			Q_strncpyz(match, matchchar, len + 1);
			Q_strncpyz(after, matchchar + len, strlen(con.commands[i].command) - len + 1);

			Com_Printf(S_COLOR_WHITE"  %s"S_COLOR_YELLOW"%s"S_COLOR_WHITE"%s (%s)%s\n", before, match, after, matchtypename[con.commands[i].type], value);
		}
	}

	// Print usage hint
	Com_Printf("Press Tab to select next match, Ctrl-Tab to select previous.\n");

	// If current item is off-screen, scroll it into view
	if(currentcommandline != -1)
	{
		int visiblelines = Con_LinesOnScreen();
		if (con.currentline - displayline > 1) // Take the "backscroll marker" line into account. Use stored displayline, because con.displayline == con.currentline at this point.
			visiblelines--;

		// Scrolling up via Ctrl-Tab?
		const qboolean scrollup = (prevcommand != -1 && prevcommand > con.currentcommand); 
		const int scrollby = (scrollup ? 0 : visiblelines - 1);
		
		if (currentcommandline > displayline)
		{
			// Below console bottom
			con.displayline = min(con.currentline, currentcommandline + scrollby);
		}
		else if (currentcommandline < displayline - visiblelines + 1)
		{
			// Above console top. Scroll to hintstartline when displaying the first item
			con.displayline = (con.currentcommand == 0 ? con.hintstartline + visiblelines : max(con.hintstartline, currentcommandline + scrollby));
		}
		else
		{
			// Restore initial display line (it may've been modified by Con_Linefeed)...
			con.displayline = displayline;
		}

		// Scroll to console bottom to show usage hint when current scroll position is close to con.currentline
		if (con.currentline - currentcommandline < Con_LinesOnScreen()) // Ignore "backscroll marker" line.
			con.displayline = con.currentline;

		// Store current command index
		prevcommand = con.currentcommand;
	}
	else
	{
		// If no current item, scroll to current line
		con.displayline = con.currentline;

		// Reset current command index
		prevcommand = -1;
	}

	// Store hint end line
	con.hintendline = con.currentline;
}

//mxd. Copies given text to the input area
static void Con_SetInputText(const char *text)
{
	const int len = strlen(text);
	if(len == 0)
		return;
	
	// Copy text
	key_lines[edit_line][1] = '/';
	Q_strncpyz(key_lines[edit_line] + 2, text, sizeof(key_lines[edit_line]) - 2);
	key_linepos = len + 2;

	// Add trailing space
	key_lines[edit_line][key_linepos++] = ' ';

	// Add null terminator
	key_lines[edit_line][key_linepos] = 0;
}

static void Con_CompleteCommand()
{
	char partial[MAXCMDLINE];
	
	// Skip leading slash...
	char *in = key_lines[edit_line] + 1;
	while (*in && (*in == '\\' || *in == '/'))
		in++;

	// Copy partial command till the first space
	char *out = partial;
	while (*in && (*in != ' '))
		*out++ = *in++;
	*out = 0;

	// Nothing to search for?
	if(!partial[0])
		return;

	// Clear the list
	Con_ClearAutocompleteList();

	// Find matching commands, aliases and variables
	Cmd_CompleteCommand(partial, Con_CompleteCommandCallback);
	Cmd_CompleteAlias(partial, Con_CompleteAliasCallback);
	Cvar_CompleteVariable(partial, Con_CompleteVariableCallback);

	// Nothing found?
	if (con.commandscount == 0)
		return;

	// Only one result was found, so copy it to the edit line
	if (con.commandscount == 1)
	{
		Con_SetInputText(con.commands[0].command);
	}
	else // Display the list of matching cmds, aliases and cvars
	{
		// Store partial match
		con.partialmatch = CopyString(partial);
		
		// Sort results
		qsort(con.commands, con.commandscount, sizeof(matchingcommand_t), (int(*)(const void *, const void *))Con_SortCommands);

		// Print results
		Con_PrintMatchingCommands();
	}
}

#pragma endregion 

#pragma region ======================= Key processing

// Interactive line editing and console scrollback
void Con_KeyDown(int key) //mxd. Was Key_Console in cl_keys.c
{
	key = Key_ConvertNumPadKey(key); //mxd

	// Command completion
	if (key == K_TAB)
	{
		if (con.commandscount < 2)
		{
			Con_CompleteCommand();
		}
		else
		{
			// Cycle through commands
			if (Key_IsDown(K_CTRL))
			{
				con.currentcommand--;
				if (con.currentcommand < 0)
					con.currentcommand = con.commandscount - 1;
			}
			else
			{
				con.currentcommand++;
				if (con.currentcommand >= con.commandscount)
					con.currentcommand = 0;
			}

			// Set as input
			Con_SetInputText(con.commands[con.currentcommand].command);
			Con_PrintMatchingCommands();
		}

		con.backedit = 0; // Knightmare added

		return;
	}

	// Clipboard paste
	if ((toupper(key) == 'V' && Key_IsDown(K_CTRL)) || ((key == K_INS || key == K_KP_INS) && Key_IsDown(K_SHIFT)))
	{
		char *cbd = IN_GetClipboardData();
		if (cbd)
		{
			Con_ClearAutocompleteList(); //mxd
			
			strtok(cbd, "\n\r\b");

			int len = strlen(cbd);
			if (len + key_linepos >= MAXCMDLINE)
				len = MAXCMDLINE - key_linepos;

			if (len > 0)
			{
				cbd[len] = 0;
				Q_strncatz(key_lines[edit_line], cbd, sizeof(key_lines[edit_line]));
				key_linepos += len;
			}

			free(cbd);
		}

		con.backedit = 0;

		return;
	}
	
	// Clear buffer
	if ((key == 'l' || key == 'c') && Key_IsDown(K_CTRL)) //mxd. +Clear via Ctrl-C 
	{
		Cbuf_AddText("clear\n");
		con.backedit = 0;

		return;
	}
	
	// Execute a command
	if (key == K_ENTER || key == K_KP_ENTER)
	{
		Con_ClearAutocompleteList(); //mxd
		
		// Backslash text are commands, else chat
		if (key_lines[edit_line][1] == '\\' || key_lines[edit_line][1] == '/')
			Cbuf_AddText(key_lines[edit_line] + 2);	// Skip the >
		else
			Cbuf_AddText(key_lines[edit_line] + 1);	// Valid command

		Cbuf_AddText("\n");
		Com_Printf("%s\n", key_lines[edit_line]);

		edit_line = (edit_line + 1) & (NUM_KEY_LINES - 1);
		history_line = edit_line;
		key_lines[edit_line][0] = ']';
		key_linepos = 1;
		con.backedit = 0;

		if (cls.state == ca_disconnected)
			SCR_UpdateScreen();	// Force an update, because the command may take some time

		return;
	}
	
	// Delete previous character
	if (key == K_BACKSPACE)
	{
		if (key_linepos > 1)
		{
			Con_ClearAutocompleteList(); //mxd
			
			if (con.backedit && con.backedit < key_linepos)
			{
				if (key_linepos - con.backedit <= 1)
					return;

				for (int i = key_linepos - con.backedit - 1; i < key_linepos; i++)
					key_lines[edit_line][i] = key_lines[edit_line][i + 1];
			}

			key_linepos--; //mxd
		}

		return;
	}
	
	// Delete next character
	if (key == K_DEL)
	{
		if (key_linepos > 1 && con.backedit)
		{
			Con_ClearAutocompleteList(); //mxd
			
			for (int i = key_linepos - con.backedit; i < key_linepos; i++)
				key_lines[edit_line][i] = key_lines[edit_line][i + 1];

			con.backedit--;
			key_linepos--;
		}

		return;
	}
	
	// Previous character
	if (key == K_LEFTARROW) // Added from Quake2max
	{
		if (key_linepos > 1)
		{
			con.backedit++;
			con.backedit = min(con.backedit, key_linepos - 1); //mxd
		}

		return;
	}
	
	// Next character
	if (key == K_RIGHTARROW) // Added from Quake2max
	{
		if (key_linepos > 1)
		{
			con.backedit--;
			con.backedit = max(con.backedit, 0); //mxd
		}

		return;
	}
	
	// Display previous history line
	if (key == K_UPARROW || key == K_KP_UPARROW || (key == 'p' && Key_IsDown(K_CTRL)))
	{
		Con_ClearAutocompleteList(); //mxd
		
		do
		{
			history_line = (history_line - 1) & (NUM_KEY_LINES - 1);
		} while (history_line != edit_line && !key_lines[history_line][1]);

		if (history_line == edit_line)
			history_line = (edit_line + 1) & (NUM_KEY_LINES - 1);

		Q_strncpyz(key_lines[edit_line], key_lines[history_line], sizeof(key_lines[edit_line]));
		key_linepos = strlen(key_lines[edit_line]);

		return;
	}
	
	// Display next history line
	if (key == K_DOWNARROW || key == K_KP_DOWNARROW || (key == 'n' && Key_IsDown(K_CTRL)))
	{
		if (history_line == edit_line)
			return;

		Con_ClearAutocompleteList(); //mxd

		do
		{
			history_line = (history_line + 1) & (NUM_KEY_LINES - 1);
		} while (history_line != edit_line && !key_lines[history_line][1]);

		if (history_line == edit_line)
		{
			key_lines[edit_line][0] = ']';
			key_linepos = 1;
		}
		else
		{
			Q_strncpyz(key_lines[edit_line], key_lines[history_line], sizeof(key_lines[edit_line]));
			key_linepos = strlen(key_lines[edit_line]);
		}

		return;
	}
	
	// Scroll console up by lines on screen - 1
	if (key == K_PGUP || key == K_KP_PGUP)
	{
		const int linesonscreen = Con_LinesOnScreen(); //mxd
		const int firstline = Con_FirstLine();

		if (con.currentline - firstline >= linesonscreen) // Don't scroll if there are less lines than console space
		{
			con.displayline -= linesonscreen - 2; // Was 2
			con.displayline = max(con.displayline, firstline + linesonscreen - 2);
		}

		return;
	}
	
	// Scroll console down by lines on screen - 1
	if (key == K_PGDN || key == K_KP_PGDN) // Quake2max change
	{
		con.displayline += Con_LinesOnScreen() - 2; //mxd. Was 2
		con.displayline = min(con.displayline, con.currentline);

		return;
	}
	
	// Scroll console up by 2 lines
	if (key == K_MWHEELUP) //mxd
	{
		const int linesonscreen = Con_LinesOnScreen();
		const int firstline = Con_FirstLine();

		if (con.currentline - firstline >= linesonscreen) // Don't scroll if there are less lines than console space
		{
			con.displayline -= 2;
			con.displayline = max(con.displayline, firstline + linesonscreen - 2);
		}

		return;
	}
	
	// Scroll console down by 2 lines
	if (key == K_MWHEELDOWN) //mxd
	{
		con.displayline += 2;
		con.displayline = min(con.displayline, con.currentline);

		return;
	}
	
	// Buffer start
	if (key == K_HOME || key == K_KP_HOME)
	{
		const int linesonscreen = Con_LinesOnScreen(); //mxd
		const int firstline = Con_FirstLine();

		if (con.currentline - firstline >= linesonscreen) // Don't scroll if there are less lines than console space
			con.displayline = firstline + linesonscreen - 2;

		return;
	}
	
	// Buffer end
	if (key == K_END || key == K_KP_END)
	{
		con.displayline = con.currentline;

		return;
	}

	// Non-printable
	if (key < 32 || key > 126)
		return;
	
	// Process input keys
	if (key_linepos < MAXCMDLINE - 1)
	{
		Con_ClearAutocompleteList(); //mxd
		
		// Knightmare- added from Quake2Max
		if (con.backedit) // Insert character...
		{
			int i;
			for (i = key_linepos; i > key_linepos - con.backedit; i--)
				key_lines[edit_line][i] = key_lines[edit_line][i - 1];

			key_lines[edit_line][i] = key;
		}
		else
		{
			key_lines[edit_line][key_linepos] = key;
		}

		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
	}
}

#pragma endregion 

#pragma region ======================= Persistent console history

//mxd. Adapted from YQ2 (https://github.com/yquake2/yquake2/commit/1ce9bdba51c9921dc918d28a01868a4f821bd67c)
#define HISTORY_FILEPATH "%s/"ENGINE_PREFIX"console_history.txt"

static void Con_WriteConsoleHistory()
{
	char path[MAX_OSPATH];
	Com_sprintf(path, sizeof(path), HISTORY_FILEPATH, FS_Gamedir());

	FILE* f = fopen(path, "w");
	if (f == NULL)
	{
		Com_Printf("Opening console history file '%s' for writing failed!\n", path);
		return;
	}

	// Save the oldest lines first by starting at edit_line and going forward (and wrapping around)
	const int size = NUM_KEY_LINES * MAXCMDLINE * sizeof(char *);
	char **addedlines = malloc(size);
	memset(addedlines, 0, size);
	int numadded = 0;

	for (int i = 0; i < NUM_KEY_LINES; ++i)
	{
		const int lineindex = (edit_line + i) & (NUM_KEY_LINES - 1);
		const char* line = key_lines[lineindex];

		if (line[1] != '\0' && !FS_ItemInList(line, numadded, addedlines))
		{
			// If the line actually contains something besides the ] prompt,
			// and wasn't already added, write it to the file
			fputs(line, f);
			fputc('\n', f);

			FS_InsertInList(addedlines, line, numadded + 1, numadded);
			numadded++;
		}
	}

	FS_FreeFileList(addedlines, NUM_KEY_LINES);
	fclose(f);
}

// Initializes key_lines from history file, if available
static void Con_ReadConsoleHistory()
{
	char path[MAX_OSPATH];
	Com_sprintf(path, sizeof(path), HISTORY_FILEPATH, FS_Gamedir());

	FILE* f = fopen(path, "r");
	if (f == NULL)
	{
		Com_DPrintf("Opening console history file '%s' for reading failed!\n", path);
		return;
	}

	for (int i = 0; i < NUM_KEY_LINES; i++)
	{
		if (fgets(key_lines[i], MAXCMDLINE, f) == NULL)
		{
			// Probably EOF.. adjust edit_line and history_line and we're done here
			edit_line = i;
			history_line = i;

			break;
		}

		// Remove trailing newlines
		int lastcharpos = strlen(key_lines[i]) - 1;
		while ((key_lines[i][lastcharpos] == '\n' || key_lines[i][lastcharpos] == '\r') && lastcharpos >= 0)
		{
			key_lines[i][lastcharpos] = '\0';
			--lastcharpos;
		}
	}

	fclose(f);
}

#pragma endregion

#pragma region ======================= Init / shutdown

void Con_Init(void)
{
	//mxd. Moved from Key_Init()
	for (int i = 0; i < NUM_KEY_LINES; i++)
	{
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}

	key_linepos = 1;
	//mxd. End

	con.linewidth = -1;
	con.backedit = 0;
	con.hintstartline = -1; //mxd
	con.hintendline = -1; //mxd

	Con_CheckResize();
	Com_Printf("Console initialized.\n");

	// Register our commands
	con_font_size = Cvar_Get("con_font_size", "8", CVAR_ARCHIVE); //mxd. Moved from CL_InitLocal()
	con_notifytime = Cvar_Get("con_notifytime", "4", 0); // Knightmare- increased for fade
	con_alpha = Cvar_Get("con_alpha", "0.5", CVAR_ARCHIVE); // Knightmare- Psychospaz's transparent console
	con_newconback = Cvar_Get("con_newconback", "0", CVAR_ARCHIVE);	// whether to use new console background
	con_oldconbar = Cvar_Get("con_oldconbar", "1", CVAR_ARCHIVE);	// whether to draw bottom bar on old console

	// Whether to use new-style console background
	newconback_found = (FS_FileExists("gfx/ui/newconback.tga")
		|| FS_FileExists("gfx/ui/newconback.png")
		|| FS_FileExists("gfx/ui/newconback.jpg")); //mxd. FS_LoadFile -> FS_FileExists

	Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand("togglechat", Con_ToggleChat_f);
	Cmd_AddCommand("messagemode", Con_MessageMode_f);
	Cmd_AddCommand("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand("clear", Con_Clear_f);
	Cmd_AddCommand("condump", Con_Dump_f);

	Con_ReadConsoleHistory(); //mxd

	con.initialized = true;
}

//mxd. From Q2E
void Con_Shutdown(void)
{
	if (!con.initialized)
		return;

	Con_WriteConsoleHistory();
	Con_ClearAutocompleteList();

	Cmd_RemoveCommand("toggleconsole");
	Cmd_RemoveCommand("togglechat");
	Cmd_RemoveCommand("messagemode");
	Cmd_RemoveCommand("messagemode2");
	Cmd_RemoveCommand("clear");
	Cmd_RemoveCommand("condump");

	con.initialized = false;
}