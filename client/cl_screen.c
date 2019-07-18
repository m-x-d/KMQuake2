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

// cl_screen.c -- master for refresh, status bar, console, chat, notify, etc

// Full screen console
// Put up loading plaque
// Blanked background with loading plaque
// Blanked background with menu
// Cinematics
// Full screen image for quit and victory
// End of unit intermissions

#include "client.h"

float		scr_con_current;	// Aproaches scr_conlines at scr_conspeed
float		scr_conlines;		// 0.0 to 1.0 lines of console to display

float		scr_letterbox_current;	// Aproaches scr_lboxlines at scr_conspeed
float		scr_letterbox_lines;	// 0.0 to 1.0 lines of letterbox to display
qboolean	scr_letterbox_active;
qboolean	scr_hidehud;

qboolean	scr_initialized; // Ready to draw

int			scr_draw_loading;

cvar_t		*scr_conspeed;
cvar_t		*scr_letterbox;
cvar_t		*scr_centertime;
cvar_t		*scr_showturtle;
cvar_t		*scr_showpause;

cvar_t		*scr_netgraph;
cvar_t		*scr_netgraph_pos;
cvar_t		*scr_timegraph;
cvar_t		*scr_debuggraph;
cvar_t		*scr_graphheight;
cvar_t		*scr_graphscale;
cvar_t		*scr_graphshift;

cvar_t		*scr_simple_loadscreen; // Whether to use reduced load screen

cvar_t		*hud_scale;
cvar_t		*hud_width;
cvar_t		*hud_height;
cvar_t		*hud_alpha;
cvar_t		*hud_squeezedigits;

cvar_t		*crosshair;
cvar_t		*crosshair_scale; // Psychospaz's scalable corsshair
cvar_t		*crosshair_alpha;
cvar_t		*crosshair_pulse;

cvar_t		*cl_drawfps; //Knightmare 12/28/2001- BramBo's FPS counter
cvar_t		*cl_demomessage;
cvar_t		*cl_loadpercent;

#define LOADSCREEN_NAME "/gfx/ui/unknownmap.pcx"

#define	ICON_WIDTH	24
#define	ICON_HEIGHT	24
#define	CHAR_WIDTH	16
#define	ICON_SPACE	8

#pragma region ======================= SCREEN SCALING

void SCR_InitScreenScale(void)
{
	screenScale.x = viddef.width / SCREEN_WIDTH;
	screenScale.y = viddef.height / SCREEN_HEIGHT;
	screenScale.avg = min(screenScale.x, screenScale.y);
}

float SCR_ScaledVideo(float param)
{
	return param * screenScale.avg;
}

float SCR_VideoScale(void)
{
	return screenScale.avg;
}

//Adjusted for resolution and screen aspect ratio
void SCR_AdjustFrom640(float *x, float *y, float *w, float *h, scralign_t align)
{
	SCR_InitScreenScale();

	const float xscale = viddef.width / SCREEN_WIDTH;
	const float yscale = viddef.height / SCREEN_HEIGHT;
	
	//mxd. Setup local copies
	float lx = (x ? *x : 0);
	float ly = (y ? *y : 0);
	float lw = (w ? *w : 0);
	float lh = (h ? *h : 0);

	// Aspect-ratio independent scaling
	switch (align)
	{
	case ALIGN_CENTER:
		lw *= screenScale.avg;
		lh *= screenScale.avg;
		lx = (lx - (0.5f * SCREEN_WIDTH)) * screenScale.avg + (0.5f * viddef.width);
		ly = (ly - (0.5f * SCREEN_HEIGHT)) * screenScale.avg + (0.5f * viddef.height);
		break;

	case ALIGN_TOP:
		lw *= screenScale.avg;
		lh *= screenScale.avg;
		lx = (lx - (0.5f * SCREEN_WIDTH)) * screenScale.avg + (0.5f * viddef.width);
		ly *= screenScale.avg;
		break;

	case ALIGN_BOTTOM:
		lw *= screenScale.avg;
		lh *= screenScale.avg;
		lx = (lx - (0.5f * SCREEN_WIDTH)) * screenScale.avg + (0.5f * viddef.width);
		ly = (ly - SCREEN_HEIGHT) * screenScale.avg + viddef.height;
		break;

	case ALIGN_RIGHT:
		lw *= screenScale.avg;
		lh *= screenScale.avg;
		lx = (lx - SCREEN_WIDTH) * screenScale.avg + viddef.width;
		ly = (ly - (0.5f * SCREEN_HEIGHT)) * screenScale.avg + (0.5f * viddef.height);
		break;

	case ALIGN_LEFT:
		lw *= screenScale.avg;
		lh *= screenScale.avg;
		lx *= screenScale.avg;
		ly = (ly - (0.5f * SCREEN_HEIGHT)) * screenScale.avg + (0.5f * viddef.height);
		break;

	case ALIGN_TOPRIGHT:
		lw *= screenScale.avg;
		lh *= screenScale.avg;
		lx = (lx - SCREEN_WIDTH) * screenScale.avg + viddef.width;
		ly *= screenScale.avg;
		break;

	case ALIGN_TOPLEFT:
		lw *= screenScale.avg;
		lh *= screenScale.avg;
		lx *= screenScale.avg;
		ly *= screenScale.avg;
		break;

	case ALIGN_BOTTOMRIGHT:
		lw *= screenScale.avg;
		lh *= screenScale.avg;
		lx = (lx - SCREEN_WIDTH) * screenScale.avg + viddef.width;
		ly = (ly - SCREEN_HEIGHT) * screenScale.avg + viddef.height;
		break;

	case ALIGN_BOTTOMLEFT:
		lw *= screenScale.avg;
		lh *= screenScale.avg;
		lx *= screenScale.avg;
		ly = (ly - SCREEN_HEIGHT) * screenScale.avg + viddef.height;
		break;

	case ALIGN_BOTTOM_STRETCH:
		lw *= xscale;
		lh *= screenScale.avg;
		lx *= xscale;
		ly = (ly - SCREEN_HEIGHT) * screenScale.avg + viddef.height;
		break;

	case ALIGN_STRETCH:
	default:
		lw *= xscale;
		lh *= yscale;
		lx *= xscale;
		ly *= yscale;
		break;
	}

	//mxd. Apply local copies
	if (x) *x = lx;
	if (y) *y = ly;
	if (w) *w = lw;
	if (h) *h = lh;
}

//Coordinates are 640*480 virtual values
void SCR_DrawFill(float x, float y, float width, float height, scralign_t align, int red, int green, int blue, int alpha)
{
	SCR_AdjustFrom640(&x, &y, &width, &height, align);
	R_DrawFill(x, y, width, height, red, green, blue, alpha);
}

//Coordinates are 640*480 virtual values
void SCR_DrawPic(float x, float y, float width, float height, scralign_t align, char *pic, float alpha)
{
	SCR_AdjustFrom640(&x, &y, &width, &height, align);
	R_DrawStretchPic(x, y, width, height, pic, alpha);
}

//Coordinates are 640*480 virtual values
void SCR_DrawChar(float x, float y, scralign_t align, int num, int red, int green, int blue, int alpha, qboolean italic, qboolean last)
{
	SCR_AdjustFrom640(&x, &y, NULL, NULL, align);
	R_DrawChar(x, y, num, screenScale.avg, red, green, blue, alpha, italic, last);
}

//Coordinates are 640*480 virtual values
void SCR_DrawString(float x, float y, scralign_t align, const char *string, int alpha)
{
	SCR_AdjustFrom640(&x, &y, NULL, NULL, align);
	DrawStringGeneric(x, y, string, alpha, SCALETYPE_MENU, false);
}

//===============================================================================

void Hud_DrawString(int x, int y, const char *string, int alpha, qboolean isStatusBar)
{
	DrawStringGeneric(x, y, string, alpha, (isStatusBar) ? SCALETYPE_HUD : SCALETYPE_MENU, false);
}

static void Hud_DrawStringAlt(int x, int y, const char *string, int alpha, qboolean isStatusBar)
{
	DrawStringGeneric(x, y, string, alpha, (isStatusBar) ? SCALETYPE_HUD : SCALETYPE_MENU, true);
}

//FPS counter, code combined from BramBo and Q2E
#define FPS_FRAMES 4

static void SCR_ShowFPS(void)
{
	static int previousTimes[FPS_FRAMES];
	static int previousTime, fpscounter;
	static unsigned int	index;
	static char	fpsText[32];

	if (cls.state != ca_active || !cl_drawfps->value)
		return;

	InitHudScale();
	if (cl.time + 1000 < fpscounter)
		fpscounter = cl.time + 100;

	const int time = Sys_Milliseconds();
	previousTimes[index % FPS_FRAMES] = time - previousTime;
	previousTime = time;
	index++;

	if (index <= FPS_FRAMES)
		return;

	// Average multiple frames together to smooth changes out a bit
	int total = 0;
	for (int i = 0; i < FPS_FRAMES; i++)
		total += previousTimes[i];
	const int fps = 1000 * FPS_FRAMES / max(total, 1);

	if (cl.time > fpscounter)
	{
		Com_sprintf(fpsText, sizeof(fpsText), S_COLOR_BOLD S_COLOR_SHADOW"%3ifps", fps);
		fpscounter = cl.time + 100;
	}

	// leave space for 3-digit frag counter
	const int fragsSize = HudScale() * 3 * (CHAR_WIDTH + 2);
	const int x = viddef.width - strlen(fpsText) * HUD_FONT_SIZE * SCR_VideoScale() - max(fragsSize, SCR_ScaledVideo(68));
	const int y = 0;
	DrawStringGeneric(x, y, fpsText, 255, SCALETYPE_MENU, false);
}

#pragma endregion

#pragma region ======================= BAR GRAPHS

// A new packet was just parsed
static int currentping;

void CL_AddNetgraph()
{
	const int in = cls.netchan.incoming_acknowledged & (CMD_BACKUP - 1);
	currentping = cls.realtime - cl.cmd_time[in];

	// If using the debuggraph for something else, don't add the net lines
	if (scr_debuggraph->value || scr_timegraph->value)
		return;

	for (int i = 0; i < cls.netchan.dropped; i++)
		SCR_DebugGraph(30, 0x40);

	for (int i = 0; i < cl.surpressCount; i++)
		SCR_DebugGraph(30, 0xdf);

	// See what the latency was on this packet
	const int ping = min(currentping / 30, 30);
	SCR_DebugGraph(ping, 0xd0);
}

typedef struct
{
	float value;
	int color;
} graphsamp_t;

static int current;
static graphsamp_t values[1024];

void SCR_DebugGraph(float value, int color)
{
	values[current & 1023].value = value;
	values[current & 1023].color = color;
	current++;
}

static void SCR_DrawDebugGraph(void)
{
	int x, y;
	static float lasttime = 0;
	static int fps, ping;

	const int h = (2 * FONT_SIZE > 40) ? 60 + 2 * FONT_SIZE : 100;
	const int w = (9 * FONT_SIZE > 100) ? 9 * FONT_SIZE : 100;

	if (scr_netgraph_pos->value == 0) // Bottom right
	{
		x = viddef.width - w - 3;
		y = viddef.height - h - 3;
	}	
	else // Bottom left
	{
		x = 0;
		y = viddef.height - h - 3;
	}

	// Transparent background
	R_DrawFill(x, y, w + 2, h + 2, 255, 255, 255, 90);

	for (int a = 0; a < w; a++)
	{
		const int i = (current - 1 - a + 1024) & 1023;
		float v = values[i].value * scr_graphscale->value;
		const int color = values[i].color;
		
		if (v < 1)
			v += h * (1 + (int)(-v / h));

		int max = (int)v % h + 1;
		int min = y + h - max - scr_graphshift->value;

		// Bind to box!
		if (min < y + 1) min = y + 1;
		if (min > y + h) min = y + h;
		if (min + max > y + h) max = y + h - max;

		R_DrawFill(x + w - a, min, 1, max, color8red(color), color8green(color), color8blue(color), 255);
	}

	if (cls.realtime - lasttime > 50)
	{
		lasttime = cls.realtime;
		fps = (cls.renderFrameTime ? 1 / cls.renderFrameTime: 0);
		ping = currentping;
	}

	DrawStringGeneric(x, y + 5, va(S_COLOR_SHADOW"fps: %3i", fps), 255, SCALETYPE_CONSOLE, false);
	DrawStringGeneric(x, y + 5 + FONT_SIZE , va(S_COLOR_SHADOW"ping:%3i", ping), 255, SCALETYPE_CONSOLE, false);

	// draw border
	R_DrawFill(x,			y,				(w + 2),	1,			0, 0, 0, 255);
	R_DrawFill(x,			y + (h + 2),	(w + 2),	1,			0, 0, 0, 255);
	R_DrawFill(x,			y,				1,			(h + 2),	0, 0, 0, 255);
	R_DrawFill(x + (w + 2),	y,				1,			(h + 2),	0, 0, 0, 255);
}

#pragma endregion

#pragma region ======================= CENTER PRINTING

static char scr_centerstring[1024];
static float scr_centertime_start;
static float scr_centertime_off;
static float scr_centertime_end;
static int scr_center_lines;

//mxd. Calculate time in seconds to read given string. Assumes 120 words per minute (a word per 0.5 seconds) reading speed.
static float GetReadTime(const char *str)
{
	int numwords = 1;
	
	int c = 0;
	qboolean prevcharwasspace = false;
	while(str[c])
	{
		if (str[c] == ' ' || str[c] == '\n')
		{
			if (!prevcharwasspace)
				numwords++;

			prevcharwasspace = true;
		}
		else
		{
			prevcharwasspace = false;
		}

		c++;
	}

	return ceilf(numwords * 0.5f);
}

// Called for important messages that should stay in the center of the screen for a few moments
void SCR_CenterPrint(char *str)
{
	strncpy(scr_centerstring, str, sizeof(scr_centerstring) - 1);
	scr_centertime_off = max(GetReadTime(scr_centerstring) + 1.5f, scr_centertime->value); //mxd. +GetReadTime, +0.5 sec. fade in, +1 sec. fade out
	scr_centertime_end = cl.time + scr_centertime_off * 1000; //mxd. Was scr_centertime_end = scr_centertime_off
	scr_centertime_start = cl.time;
	scr_center_lines = 1;

	// Echo it to the console
	Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");

	char *start = str;
	do	
	{
		// Scan the width of the line
		int len, totallen;
		for (len = 0, totallen = 0; totallen < 40; len++, totallen++)
		{
			if (start[totallen] == '\n' || !start[totallen])
				break;

			// Take colouring sequences into account...
			if (IsColoredString(&start[totallen]))
				len -= 2;
		}

		// Copy input text to line
		char line[64];
		Q_strncpyz(line, start, totallen + 1);

		// Print to console, prepend with spaces for centering
		const int padding = (40 - len) / 2;
		Com_Printf("%*s%s\n", padding, " ", line);

		// Advance input text to next line
		start += totallen;

		if (!*start)
			break;

		// Skip the \n
		start++;

		// Count the number of lines for centering
		scr_center_lines++;
	} while (true);

	Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Con_ClearNotify();
}

static void SCR_DrawCenterString(void)
{
	// Added Psychospaz's fading centerstrings
	//const int alpha = 255 * (1 - (cl.time + (scr_centertime->value - 1) - scr_centertime_start) / 1000.0f / scr_centertime_end);

	//mxd. Fade in during the first half-second, fade out during the last second.
	int alpha = 255;

	if (cl.time - scr_centertime_start < 500)
		alpha *= (cl.time - scr_centertime_start) / 500.0f;
	else if (scr_centertime_end - cl.time < 1000)
		alpha *= (scr_centertime_end - cl.time) / 1000.0f;

	char *start = scr_centerstring;

	int y;
	if (scr_center_lines <= 4)
		y = viddef.height * 0.35f;
	else
		y = FONT_SIZE * 6; //mxd. Was 48

	do
	{
		// Scan the width of the line
		int len, totallen;
		for (len = 0, totallen = 0; len < 40; len++, totallen++)
		{
			if (start[totallen] == '\n' || !start[totallen])
				break;

			// Take colouring sequences into account...
			if (IsColoredString(&start[totallen]))
				len -= 2;
		}

		// Copy input text to line
		char line[512];
		Q_strncpyz(line, start, totallen + 1);

		// Print centered
		DrawStringGeneric((int)((viddef.width - CL_UnformattedStringLength(line) * FONT_SIZE) * 0.5f), y, line, alpha, SCALETYPE_CONSOLE, false);
		y += FONT_SIZE;

		// Advance input text to next line
		start += totallen;

		if (!*start)
			break;

		// Skip the \n
		start++;
	} while (true);
}

static void SCR_CheckDrawCenterString(void)
{
	scr_centertime_off -= cls.renderFrameTime;
	
	if (scr_centertime_off > 0)
		SCR_DrawCenterString();
}

#pragma endregion

#pragma region ======================= Console commands

static void SCR_SizeUp_f(void)
{
	// Handle HUD scale
	const int hudscale = min(Cvar_VariableValue("hud_scale") + 1, 6); //mxd. +min
	Cvar_SetValue("hud_scale", hudscale);
}

static void SCR_SizeDown_f(void)
{
	// Handle HUD scale
	const int hudscale = max(Cvar_VariableValue("hud_scale") - 1, 1); //mxd. +max
	Cvar_SetValue("hud_scale", hudscale);
}

// Set a specific sky and rotation speed
static void SCR_Sky_f(void)
{
	float rotate;
	vec3_t axis;

	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: sky <basename> [rotate] [axis x y z]\n");
		return;
	}

	if (Cmd_Argc() > 2)
		rotate = atof(Cmd_Argv(2));
	else
		rotate = 0;

	if (Cmd_Argc() == 6)
	{
		axis[0] = atof(Cmd_Argv(3));
		axis[1] = atof(Cmd_Argv(4));
		axis[2] = atof(Cmd_Argv(5));
	}
	else
	{
		axis[0] = 0;
		axis[1] = 0;
		axis[2] = 1;
	}

	R_SetSky(Cmd_Argv(1), rotate, axis);
}

void SCR_DumpStatusLayout_f(void)
{
	char buffer[2048];
	char rawLine[MAX_QPATH + 1];
	char statLine[32];
	char name[MAX_OSPATH];

	if (Cmd_Argc() > 2)
	{
		Com_Printf("Usage: dumpstatuslayout <filename>\n");
		return;
	}

	if(Cmd_Argc() == 1) //mxd. Auto filename
		Com_sprintf(name, sizeof(name), "%s/statusbar_layout.txt", FS_Gamedir());
	else
		Com_sprintf(name, sizeof(name), "%s/%s.txt", FS_Gamedir(), Cmd_Argv(1));

	FS_CreatePath(name);
	FILE* f = fopen(name, "w");
	if (!f)
	{
		Com_Printf(S_COLOR_RED"Error: couldn't open '%s'.\n", name);
		return;
	}

	// Statusbar layout is in multiple configstrings starting at CS_STATUSBAR and ending at CS_AIRACCEL
	char* p = &buffer[0];
	int bufcount = 0;

	for (int i = CS_STATUSBAR; i < CS_AIRACCEL; i++)
	{
		for (int j = 0; j < MAX_QPATH; j++)
		{
			// Check for end
			if (cl.configstrings[i][j] == '\0')
				break;

			//mxd. Skip extra spaces
			if ((cl.configstrings[i][j] == '\t' || cl.configstrings[i][j] == ' ') && bufcount > 0 && (*(p - 1) == ' ' || *(p - 1) == '\n'))
				continue;

			*p = cl.configstrings[i][j];

			if (*p == '\t')
				*p = ' ';

			// Check for "endif", insert newline after
			if(bufcount > 4 && !strncmp(p - 4, "endif", 5))
			{
				p++;
				bufcount++;
				*p = '\n';
			}

			//mxd. Check for "if", insert newline before
			if (bufcount > 2 && !strncmp(p - 2, " if", 3))
				*(p - 2) = '\n';

			p++;
			bufcount++;
		}
	}

	fwrite(&buffer, 1, bufcount, f);
	buffer[0] = 0;
	bufcount = 0;

	// Write out the raw dump
	Com_sprintf(statLine, sizeof(statLine), "\nRaw Dump\n--------\n");
	Q_strncatz(buffer, statLine, sizeof(buffer));
	bufcount += strlen(statLine);
	fwrite(&buffer, 1, bufcount, f);
	buffer[0] = 0;
	bufcount = 0;

	for (int i = CS_STATUSBAR; i < CS_AIRACCEL; i++)
	{
		memset(rawLine, 0, sizeof(rawLine));

		for (int j = 0; j < MAX_QPATH; j++)
		{
			rawLine[j] = cl.configstrings[i][j];

			if (rawLine[j] == '\0' || rawLine[j] == '\t')
				rawLine[j] = ' ';
		}

		rawLine[MAX_QPATH] = '\n';
		fwrite(&rawLine, 1, sizeof(rawLine), f);
	}

	// Write out the stat values for debugging
	Com_sprintf(statLine, sizeof(statLine), "\nStat Values\n-----------\n");
	Q_strncatz(buffer, statLine, sizeof(buffer));
	bufcount += strlen(statLine);

	for (int i = 0; i < MAX_STATS; i++)
	{
		Com_sprintf(statLine, sizeof(statLine), "%i: %i\n", i, cl.frame.playerstate.stats[i]);

		// Prevent buffer overflow
		if (bufcount + strlen(statLine) >= sizeof(buffer))
		{
			fwrite(&buffer, 1, bufcount, f);
			buffer[0] = 0;
			bufcount = 0;
		}

		Q_strncatz(buffer, statLine, sizeof(buffer));
		bufcount += strlen(statLine);
	}

	fwrite(&buffer, 1, bufcount, f);
	buffer[0] = 0;

	fclose(f);

	Com_Printf("Dumped statusbar layout to '%s'.\n", name);
}

static void SCR_Loading_f(void)
{
	SCR_BeginLoadingPlaque(NULL);
}

static void SCR_TimeRefresh_f(void)
{
	if (cls.state != ca_active)
	{
		Com_Printf("This command requires a map to be loaded!\n");
		return;
	}

	const int start = Sys_Milliseconds();

	if (Cmd_Argc() == 2)
	{
		// Run without page flipping
		R_BeginFrame(0);
		for (int i = 0; i < 128; i++)
		{
			cl.refdef.viewangles[1] = i / 128.0f * 360.0f;
			R_RenderFrame(&cl.refdef);
		}
		GLimp_EndFrame();
	}
	else
	{
		for (int i = 0; i < 128; i++)
		{
			cl.refdef.viewangles[1] = i / 128.0f * 360.0f;

			R_BeginFrame(0);
			R_RenderFrame(&cl.refdef);
			GLimp_EndFrame();
		}
	}

	const int stop = Sys_Milliseconds();
	const float time = (stop - start) / 1000.0f;
	Com_Printf("%f seconds (%f fps)\n", time, 128 / time);
}

#pragma endregion

void SCR_Init(void)
{
	//Knightmare 12/28/2001- FPS counter
	cl_drawfps = Cvar_Get("cl_drawfps", "0", CVAR_ARCHIVE);
	cl_demomessage = Cvar_Get("cl_demomessage", "1", CVAR_ARCHIVE);
	cl_loadpercent = Cvar_Get("cl_loadpercent", "0", CVAR_ARCHIVE);

	scr_conspeed = Cvar_Get("scr_conspeed", "3", 0);
	scr_letterbox = Cvar_Get("scr_letterbox", "1", CVAR_ARCHIVE);
	scr_showturtle = Cvar_Get("scr_showturtle", "0", 0);
	scr_showpause = Cvar_Get("scr_showpause", "1", 0);
	// Knightmare- increased for fade
	scr_centertime = Cvar_Get("scr_centertime", "3.5", 0);
	scr_netgraph = Cvar_Get("netgraph", "0", 0);
	scr_netgraph_pos = Cvar_Get("netgraph_pos", "0", CVAR_ARCHIVE);
	scr_timegraph = Cvar_Get("timegraph", "0", 0);
	scr_debuggraph = Cvar_Get("debuggraph", "0", 0);
	scr_graphheight = Cvar_Get("graphheight", "32", 0);
	scr_graphscale = Cvar_Get("graphscale", "1", 0);
	scr_graphshift = Cvar_Get("graphshift", "0", 0);

	crosshair = Cvar_Get("crosshair", "1", CVAR_ARCHIVE);
	crosshair_scale = Cvar_Get("crosshair_scale", "1", CVAR_ARCHIVE);
	crosshair_alpha = Cvar_Get("crosshair_alpha", "1", CVAR_ARCHIVE);
	crosshair_pulse = Cvar_Get("crosshair_pulse", "0.25", CVAR_ARCHIVE);

	scr_simple_loadscreen = Cvar_Get("scr_simple_loadscreen", "1", CVAR_ARCHIVE);

	hud_scale = Cvar_Get("hud_scale", "3", CVAR_ARCHIVE);
	hud_width = Cvar_Get("hud_width", "640", CVAR_ARCHIVE);
	hud_height = Cvar_Get("hud_height", "480", CVAR_ARCHIVE);
	hud_alpha = Cvar_Get("hud_alpha", "1", CVAR_ARCHIVE);
	hud_squeezedigits = Cvar_Get("hud_squeezedigits", "1", CVAR_ARCHIVE);

	// Register our commands
	Cmd_AddCommand("timerefresh", SCR_TimeRefresh_f);
	Cmd_AddCommand("loading", SCR_Loading_f);
	Cmd_AddCommand("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand("sizedown", SCR_SizeDown_f);
	Cmd_AddCommand("sky", SCR_Sky_f);
	Cmd_AddCommand("dumpstatuslayout", SCR_DumpStatusLayout_f);

	SCR_InitScreenScale();
	InitHudScale();

	scr_initialized = true;
}

// Moved from cl_view.c, what the hell was it doing there?
// Psychospaz's new crosshair code
void SCR_DrawCrosshair(void)
{
	//mxd. Made local
	static char crosshair_pic[MAX_QPATH];
	
	if (!crosshair->integer || scr_hidehud)
		return;

	if (crosshair->modified)
	{
		//mxd. Handle here instead of SCR_TouchPics to avoid message spam when crosshair image is missing
		if (FS_ModType("dday")) // dday has no crosshair (FORCED)
		{
			Cvar_SetInteger("crosshair", 0);
		}
		else
		{
			if (crosshair->integer > 100 || crosshair->integer < 0) //Knightmare increased
				Cvar_SetInteger("crosshair", 1); //mxd. Don't directly set the value

			Com_sprintf(crosshair_pic, sizeof(crosshair_pic), "ch%i", crosshair->integer);

			int w, h;
			R_DrawGetPicSize(&w, &h, crosshair_pic);
			if (w == 0 || h == 0)
				crosshair_pic[0] = 0;
		}

		//mxd. Set last to avoid mutiple calls to this block
		crosshair->modified = false;
	}

	if (crosshair_scale->modified)
	{
		if (crosshair_scale->value > CROSSHAIR_SCALE_MAX)
			Cvar_SetValue("crosshair_scale", CROSSHAIR_SCALE_MAX);
		else if (crosshair_scale->value < CROSSHAIR_SCALE_MIN)
			Cvar_SetValue("crosshair_scale", CROSSHAIR_SCALE_MIN);

		//mxd. Set last to avoid mutiple calls to this block
		crosshair_scale->modified = false;
	}

	if (!crosshair_pic[0])
		return;

	const float scaledsize = crosshair_scale->value * CROSSHAIR_SIZE;
	const float pulsealpha = crosshair_alpha->value * crosshair_pulse->value;
	float alpha = crosshair_alpha->value - pulsealpha + pulsealpha * sinf(anglemod(cl.time * 0.005f));
	alpha = clamp(alpha, 0.0f, 1.0f);

	SCR_DrawPic(((float)SCREEN_WIDTH  - scaledsize) * 0.5f,
				((float)SCREEN_HEIGHT - scaledsize) * 0.5f,
				scaledsize, scaledsize, ALIGN_CENTER, crosshair_pic, alpha);
}

static void SCR_DrawNet(void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged > CMD_BACKUP - 2)
	{
		//mxd. Draw scaled. And pulsating.
		int w, h;
		R_DrawGetPicSize(&w, &h, "net");

		const float alpha = 0.625f + (sinf(anglemod(cl.time * 0.005f)) * 0.375f);
		SCR_DrawPic(1, 1, w, h, ALIGN_LEFT, "net", alpha);
	}
}

static void SCR_DrawAlertMessagePicture(char *name, qboolean center, int yOffset)
{
	int w, h;
	R_DrawGetPicSize(&w, &h, name);
	if (w)
	{
		const float ratio = 35.0f / (float)h;
		h = 35;
		w *= ratio;

		const int x = (SCREEN_WIDTH - w) * 0.5f; //mxd
		const int y = (center ? (SCREEN_HEIGHT - h) * 0.5f : SCREEN_HEIGHT * 0.5f + yOffset); //mxd

		SCR_DrawPic(x, y, w, h, ALIGN_CENTER, name, 1.0f);
	}
}

static void SCR_DrawPause(void)
{
	// Turn off for screenshots || not paused || Knightmare- no need to draw when in menu
	if (!scr_showpause->value || !cl_paused->value || cls.key_dest == key_menu)
		return;

	int w, h;
	R_DrawGetPicSize(&w, &h, "pause");
	SCR_DrawPic((SCREEN_WIDTH - w) * 0.5f, (SCREEN_HEIGHT - h) * 0.5f, w, h, ALIGN_CENTER, "pause", 1.0f);
}

#pragma region ======================= Load screen drawing

#define LOADBAR_TIC_SIZE_X 4
#define LOADBAR_TIC_SIZE_Y 4

static void SCR_DrawLoadingTagProgress(char *picName, int yOffset, int percent)
{
	const int w = 160; // Size of loading_bar.tga = 320x80
	const int h = 40;
	const int x = (SCREEN_WIDTH - w) * 0.5f;
	const int y = (SCREEN_HEIGHT - h) * 0.5f;
	const int barPos = clamp(percent, 0, 100) / 4;

	SCR_DrawPic(x, y + yOffset, w, h, ALIGN_CENTER, picName, 1.0f);

	for (int i = 0; i < barPos; i++)
		SCR_DrawPic(x + 33 + (i * LOADBAR_TIC_SIZE_X), y + 28 + yOffset, LOADBAR_TIC_SIZE_X, LOADBAR_TIC_SIZE_Y, ALIGN_CENTER, "loading_led1", 1.0f);
}

static void SCR_DrawLoadingBar(float x, float y, float w, float h, int percent, float sizeRatio)
{	
	int red, green, blue;

	// Changeable download/map load bar color
	TextColor((int)alt_text_color->value, &red, &green, &blue);
	const float iRatio = 1 - fabsf(sizeRatio);
	const float hiRatio = iRatio * 0.5f;

	SCR_DrawFill(x, y, w, h, ALIGN_STRETCH, 255, 255, 255, 90);

	if (percent != 0)
		SCR_DrawFill(x + (h * hiRatio), y + (h * hiRatio), (w - (h * iRatio)) * percent * 0.01f, h * sizeRatio, ALIGN_STRETCH, red, green, blue, 255);
}

// Gets virtual 640x480 x-pos and width for a fullscreen pic of any aspect ratio.
static void SCR_GetPicPosWidth(char *pic, int *x, int *w)
{
	if (!pic || !x || !w) // Catch null pointers
		return;

	int picWidth, picHeight;
	R_DrawGetPicSize(&picWidth, &picHeight, pic);
	const float picAspect = (float)picWidth / (float)picHeight;
	const int virtualWidth = SCREEN_HEIGHT * picAspect;
	const int virtual_x = (SCREEN_WIDTH - virtualWidth) / 2;

	*x = virtual_x;
	*w = virtualWidth;
}

char *load_saveshot;
static char newmapname[MAX_QPATH]; //mxd. Filename of the map being loaded, passed locally from server side. Added so proper levelshot can be drawn even before CL_PrepRefresh is called

static void SCR_DrawLoading(void)
{
	if (!scr_draw_loading)
	{
		loadingPercent = 0;
		return;
	}

	scr_draw_loading = 0;
	
	int plaqueOffset;
	char picName[MAX_QPATH];
	char *loadMsg;

	qboolean haveMapPic = false;
	const qboolean simplePlaque = (scr_simple_loadscreen->integer != 0);

	//mxd. Find background image to display during loading
	const qboolean haveSaveshot = (load_saveshot && load_saveshot[0] && R_DrawFindPic(load_saveshot));
	const qboolean isMap = (loadingMessages[0] && cl.configstrings[CS_MODELS + 1][0]); // loadingMessages is updated in CL_PrepRefresh. Before that, cl.configstrings may hold previous map name

	//mxd. Get levelshot filename
	if(!haveSaveshot)
	{
		picName[0] = 0;
		
		//mxd. If present, newmapname will hold either a map name without extension or .dm2 / .cin / .roq / .pcx filename.
		if(newmapname[0])
		{
			const char *ext = COM_FileExtension(newmapname);
			
			if (!*ext) // Store map name
			{
				Com_sprintf(picName, sizeof(picName), "/levelshots/%s.pcx", newmapname);
			}
			else if (!Q_stricmp(ext, "pcx") || !Q_stricmp(ext, "cin") || !Q_stricmp(ext, "roq")) // Skip drawing when loading .cin / .roq / .pcx
			{
				SCR_DrawFill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ALIGN_STRETCH, 0, 0, 0, 255);
				return;
			}
		}
		
		// We will get here when loading a .dm2 demo
		if(!picName[0] && cl.configstrings[CS_MODELS + 1][0])
		{
			char mapfile[64];
			Q_strncpyz(mapfile, cl.configstrings[CS_MODELS + 1] + 5, sizeof(mapfile)); // Skip "maps/"
			mapfile[strlen(mapfile) - 4] = 0; // Cut off ".bsp"

			Com_sprintf(picName, sizeof(picName), "/levelshots/%s.pcx", mapfile);
		}
	}

	//mxd. Draw bg fill...
	SCR_DrawFill(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ALIGN_STRETCH, 0, 0, 0, 255);

	//mxd. Check saveshot first...
	if (haveSaveshot)
	{
		// Show saveshot
		SCR_DrawPic(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ALIGN_STRETCH, load_saveshot, 1.0f);
		haveMapPic = true;
	}
	else if(picName[0] && R_DrawFindPic(picName)) // Try levelshot
	{
		int w, h;
		R_DrawGetPicSize(&w, &h, picName);

		if (w == h) //mxd. Scale KMQ2 square levelshots to fill screen
		{
			const int height = viddef.width / 4 * 3;
			const int y = -(height - viddef.height) / 2;
			R_DrawStretchPic(0, y, viddef.width, height, picName, 1.0f);
		}
		else if((float)w / h > (float)viddef.width / viddef.height) //mxd. Image aspect ratio is more widescreen than the screen
		{
			const int width = w * ((float)viddef.height / h);
			const int x = -(width - viddef.width) / 2;
			R_DrawStretchPic(x, 0, width, viddef.height, picName, 1.0f);
		}
		else //mxd. Image aspect ratio is less widescreen than the screen
		{
			const int height = h * ((float)viddef.width / w);
			const int y = -(height - viddef.height) / 2;
			R_DrawStretchPic(0, y, viddef.width, height, picName, 1.0f);
		}

		haveMapPic = true;
	}
	else if (R_DrawFindPic(LOADSCREEN_NAME)) // Fall back on loadscreen
	{
		int x, w;
		SCR_GetPicPosWidth(LOADSCREEN_NAME, &x, &w);
		SCR_DrawPic(x, 0, w, SCREEN_HEIGHT, ALIGN_CENTER, LOADSCREEN_NAME, 1.0f);
	}

	// Add Download info stuff...
#ifdef USE_CURL	// HTTP downloading from R1Q2
	if (cls.downloadname[0] && (cls.download || cls.downloadposition))
#else
	if (cls.download) // download bar...
#endif	// USE_CURL
	{
		if (simplePlaque)
		{
			plaqueOffset = -48;
			if (cls.downloadrate > 0.0f)
				loadMsg = va("Downloading ["S_COLOR_ALT"%s"S_COLOR_WHITE"]: %3d%% (%4.2fKB/s)", cls.downloadname, cls.downloadpercent, cls.downloadrate);
			else
				loadMsg = va("Downloading ["S_COLOR_ALT"%s"S_COLOR_WHITE"]: %3d%%", cls.downloadname, cls.downloadpercent);

			SCR_DrawString((SCREEN_WIDTH - MENU_FONT_SIZE * CL_UnformattedStringLength(loadMsg)) * 0.5f,
							SCREEN_HEIGHT * 0.5f + (plaqueOffset + 48), ALIGN_CENTER, loadMsg, 255);

			SCR_DrawLoadingTagProgress("downloading_bar", plaqueOffset, cls.downloadpercent);
		}
		else
		{
			plaqueOffset = -130;
			loadMsg = va("Downloading ["S_COLOR_ALT"%s"S_COLOR_WHITE"]", cls.downloadname);

			SCR_DrawString((SCREEN_WIDTH - MENU_FONT_SIZE * CL_UnformattedStringLength(loadMsg)) * 0.5f,
							SCREEN_HEIGHT * 0.5f + MENU_FONT_SIZE * 4.5f, ALIGN_CENTER, loadMsg, 255);

			if (cls.downloadrate > 0.0f)
				loadMsg = va("%3d%% (%4.2fKB/s)", cls.downloadpercent, cls.downloadrate);
			else
				loadMsg = va("%3d%%", cls.downloadpercent);

			SCR_DrawString((SCREEN_WIDTH - MENU_FONT_SIZE * CL_UnformattedStringLength(loadMsg)) * 0.5f,
							SCREEN_HEIGHT * 0.5f + MENU_FONT_SIZE * 6, ALIGN_CENTER, loadMsg, 255);

			SCR_DrawLoadingBar(SCREEN_WIDTH * 0.5f - 180, SCREEN_HEIGHT * 0.5f + 60, 360, 24, cls.downloadpercent, 0.75f);
			SCR_DrawAlertMessagePicture("downloading", false, -plaqueOffset);
		}
	}
	else if (isMap)
	{
		// Loading message stuff && loading bar...
		qboolean drawMapName = false;
		qboolean drawLoadingMsg = false;

		if (!simplePlaque)
		{
			plaqueOffset = -72;	// was -130
			drawMapName = true;
			drawLoadingMsg = true;
		}
		else if (!haveMapPic)
		{
			plaqueOffset = -48;
			drawMapName = true;
		}
		else
		{
			plaqueOffset = 0;
		}

		if (drawMapName)
		{
			loadMsg = va(S_COLOR_SHADOW S_COLOR_WHITE"Loading Map ["S_COLOR_ALT"%s"S_COLOR_WHITE"]", cl.configstrings[CS_NAME]);
			SCR_DrawString((SCREEN_WIDTH - MENU_FONT_SIZE * CL_UnformattedStringLength(loadMsg)) * 0.5f,
							SCREEN_HEIGHT * 0.5f + (plaqueOffset + 48), ALIGN_CENTER, loadMsg, 255);	// was - MENU_FONT_SIZE*7.5
		}

		if (drawLoadingMsg)
		{
			loadMsg = va(S_COLOR_SHADOW"%s", loadingMessages);
			SCR_DrawString((SCREEN_WIDTH - MENU_FONT_SIZE * CL_UnformattedStringLength(loadMsg)) * 0.5f,
							SCREEN_HEIGHT * 0.5f + (plaqueOffset + 72), ALIGN_CENTER, loadMsg, 255);	// was - MENU_FONT_SIZE*4.5
		}

		if (simplePlaque)
		{
			SCR_DrawLoadingTagProgress("loading_bar", plaqueOffset, (int)loadingPercent);
		}
		else
		{
			SCR_DrawLoadingBar(SCREEN_WIDTH * 0.5f - 180, SCREEN_HEIGHT - 20, 360.0f, 15.0f, (int)loadingPercent, 0.6f);
			SCR_DrawAlertMessagePicture("loading", false, plaqueOffset);
		}
	}
	else
	{
		// Just a plain old loading plaque
		if (simplePlaque && loadingPercent > 0)
			SCR_DrawLoadingTagProgress("loading_bar", 0, (int)loadingPercent);
		else
			SCR_DrawAlertMessagePicture("loading", true, 0);
	}
}

void SCR_BeginLoadingPlaque(const char *mapname) //mxd. +mapname
{
	S_StopAllSounds();
	cl.sound_prepped = false; // Don't play ambients

	if (developer->value)
		return;

	cls.consoleActive = false; // Knightmare added

	if (cl.cinematictime > 0)
		scr_draw_loading = 2; // Clear to black first
	else
		scr_draw_loading = 1;

	//mxd. Store level name... This can also be a .dm2, .cin, .roq or .pcx filename. 
	if (mapname)
		Q_strncpyz(newmapname, mapname, sizeof(newmapname));

	SCR_UpdateScreen();
	cls.disable_screen = Sys_Milliseconds();
	cls.disable_servercount = cl.servercount;
}

void SCR_EndLoadingPlaque(void)
{
	// Make loading saveshot null here
	load_saveshot = NULL;
	cls.disable_screen = 0;
	scr_draw_loading = 0; // Knightmare added
	newmapname[0] = 0; //mxd
	Con_ClearNotify();
}

#pragma endregion

#pragma region ======================= Letterbox / camera effect drawing

#define LETTERBOX_RATIO 0.5625 // 16:9 aspect ratio (inverse)
//#define LETTERBOX_RATIO 0.625f // 16:10 aspect ratio
void SCR_RunLetterbox(void)
{
	// Decide on the height of the letterbox
	if (scr_letterbox->value && (cl.refdef.rdflags & RDF_LETTERBOX))
	{
		scr_letterbox_lines = (1 - min(1, viddef.width * LETTERBOX_RATIO / viddef.height)) * 0.5f;
		scr_letterbox_active = true;
		scr_hidehud = true;
	}
	else if (cl.refdef.rdflags & RDF_CAMERAEFFECT)
	{
		scr_letterbox_lines = 0;
		scr_letterbox_active = false;
		scr_hidehud = true;
	}
	else
	{
		scr_letterbox_lines = 0;
		scr_letterbox_active = false;
		scr_hidehud = false;
	}
	
	if (scr_letterbox_current > scr_letterbox_lines)
	{
		scr_letterbox_current -= cls.renderFrameTime;
		scr_letterbox_current = max(scr_letterbox_lines, scr_letterbox_current);
	}
	else if (scr_letterbox_current < scr_letterbox_lines)
	{
		scr_letterbox_current += cls.renderFrameTime;
		scr_letterbox_current = min(scr_letterbox_lines, scr_letterbox_current);
	}
}

static void SCR_DrawCameraEffect(void)
{
	if (cl.refdef.rdflags & RDF_CAMERAEFFECT)
		R_DrawCameraEffect();
}

static void SCR_DrawLetterbox(void)
{
	if (!scr_letterbox_active)
		return;

	const int boxheight = scr_letterbox_current * viddef.height;
	const int boxalpha = scr_letterbox_current / scr_letterbox_lines * 255;
	R_DrawFill(0, 0, viddef.width, boxheight, 0, 0, 0, boxalpha);
	R_DrawFill(0, viddef.height - boxheight, viddef.width, boxheight, 0, 0, 0, boxalpha);
}

#pragma endregion

#pragma region ======================= Console drawing

// Scrolls console up or down
void SCR_RunConsole(void)
{
	// Decide on the height of the console
	scr_conlines = (cls.consoleActive ? 0.5 : 0); //mxd
	
	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed->value * cls.renderFrameTime;
		scr_con_current = max(scr_conlines, scr_con_current); //mxd
	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed->value * cls.renderFrameTime;
		scr_con_current = min(scr_conlines, scr_con_current);
	}
}

void SCR_DrawConsole(void)
{
	Con_CheckResize();

	// Can't render menu or game
	if (cls.key_dest != key_menu
		&& ( (cls.state == ca_disconnected || cls.state == ca_connecting) 
			|| ((cls.state != ca_active || !cl.refresh_prepped) && cl.cinematicframe <= 0 && Com_ServerState() < 3) )) // was != 5
	{
		if (!scr_draw_loading) // No background
			R_DrawFill(0, 0, viddef.width, viddef.height, 0, 0, 0, 255);

		Con_DrawConsole(0.5f, false);
		return;
	}

	if (scr_con_current)
		Con_DrawConsole(scr_con_current, true);
	else if (!cls.consoleActive && !cl.cinematictime && (cls.key_dest == key_game || cls.key_dest == key_message))
		Con_DrawNotify(); // Only draw notify in game
}

#pragma endregion

#pragma region ======================= HUD CODE

#define STAT_MINUS 10 // num frame for '-' stats digit
char *sb_nums[2][11] = 
{
	{ "num_0", "num_1", "num_2", "num_3", "num_4", "num_5", "num_6", "num_7", "num_8", "num_9", "num_minus" },
	{ "anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5", "anum_6", "anum_7", "anum_8", "anum_9", "anum_minus" }
};

// Knghtmare- scaled HUD support functions
float ScaledHud(float param)
{
	return param * hudScale.avg;
}

float HudScale()
{
	return hudScale.avg;
}

void InitHudScale()
{
	switch (hud_scale->integer)
	{
		case 0:
			Cvar_SetValue("hud_width", 0);
			Cvar_SetValue("hud_height", 0);
			break;

		case 1:
			Cvar_SetValue("hud_width", 1024);
			Cvar_SetValue("hud_height", 768);
			break;

		case 2:
			Cvar_SetValue("hud_width", 800);
			Cvar_SetValue("hud_height", 600);
			break;

		case 3:
		default:
			Cvar_SetValue("hud_width", 640);
			Cvar_SetValue("hud_height", 480);
			break;

		case 4:
			Cvar_SetValue("hud_width", 512);
			Cvar_SetValue("hud_height", 384);
			break;

		case 5:
			Cvar_SetValue("hud_width", 400);
			Cvar_SetValue("hud_height", 300);
			break;

		case 6:
			Cvar_SetValue("hud_width", 320);
			Cvar_SetValue("hud_height", 240);
			break;
	}

	// Allow disabling of hud scaling.
	// Also, don't scale if < hud_width, then it would be smaller.
	if (hud_scale->value && viddef.width > hud_width->value)
	{
		hudScale.x = viddef.width / hud_width->value;
		hudScale.y = viddef.height / hud_height->value;
		hudScale.avg = min(hudScale.x, hudScale.y); // Use smaller value instead of average
	}
	else
	{
		hudScale.x = 1;
		hudScale.y = 1;
		hudScale.avg = 1;
	}
}
// end Knightmare

// Allow embedded \n in the string
void SizeHUDString(char *string, int *w, int *h, qboolean isStatusBar)
{
	// Get our scaling function
	float (*scaleForScreen)(float in) = (isStatusBar ? ScaledHud : SCR_ScaledVideo);

	int lines = 1;
	int width = 0;

	int current = 0;
	while (*string)
	{
		if (*string == '\n')
		{
			lines++;
			current = 0;
		}
		else
		{
			current++;
			if (current > width)
				width = current;
		}

		string++;
	}

	*w = width * scaleForScreen(8);
	*h = lines * scaleForScreen(8);
}

void HUD_DrawConfigString(char *string, int x, int y, int centerwidth, qboolean altcolor, qboolean isStatusBar)
{
	char line[1024];

	// Get our scaling function
	float (*scaleForScreen)(float in) = (isStatusBar ? ScaledHud : SCR_ScaledVideo);

	const int margin = x;
	const float charscaler = scaleForScreen(8); //mxd

	while (*string)
	{
		// Scan out one line of text from the string
		int width = 0;
		while (*string && *string != '\n')
			line[width++] = *string++;

		line[width] = 0;

		if (centerwidth)
			x = margin + (centerwidth - CL_UnformattedStringLength(line) * charscaler) / 2;
		else
			x = margin;

		if (altcolor)
		{
			// Knightmare- text color hack
			Com_sprintf(line, sizeof(line), S_COLOR_ALT"%s", line);
			Hud_DrawStringAlt(x, y, line, 255, isStatusBar);
		}
		else
		{
			Hud_DrawString(x, y, line, 255, isStatusBar);
		}

		if (*string)
		{
			string++; // Skip the \n
			y += charscaler;
		}
	}
}

void SCR_DrawField(int x, int y, int color, int width, int value, qboolean flash, qboolean isStatusBar)
{
	char num[16];
	int frame;
	float (*scaleForScreen)(float in);
	float (*getScreenScale)();

	if (width < 1)
		return;

	// Get our scaling functions
	if (isStatusBar)
	{
		scaleForScreen = ScaledHud;
		getScreenScale = HudScale;
	}
	else
	{
		scaleForScreen = SCR_ScaledVideo;
		getScreenScale = SCR_VideoScale;
	}

	// Draw number string
	float fieldScale = getScreenScale();
	width = min(width, 5);

	Com_sprintf(num, sizeof(num), "%i", value);
	int len = strlen(num);
	if (len > width)
	{
		if (hud_squeezedigits->integer)
		{
			len = min(len, width + 2);
			fieldScale =  (1.0f - ((1.0f - (float)width / (float)len) * 0.5f)) * getScreenScale();
		}
		else
		{
			len = width;
		}
	}

	const float digitWidth = fieldScale * (float)CHAR_WIDTH;
	const float flashWidth = len * digitWidth;
	const float digitOffset = width * scaleForScreen(CHAR_WIDTH) - flashWidth;
	x += 2 + digitOffset;

	if (flash)
		R_DrawStretchPic(x, y, flashWidth, scaleForScreen(ICON_HEIGHT), "field_3", hud_alpha->value);

	char *ptr = num;
	while (*ptr && len > 0)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr - '0';

		R_DrawStretchPic(x, y, digitWidth, scaleForScreen(ICON_HEIGHT), sb_nums[color][frame], hud_alpha->value);

		x += digitWidth;
		ptr++;
		len--;
	}
}

// Allows rendering code to cache all needed sbar graphics
void SCR_TouchPics(void)
{
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 11; j++)
			R_DrawFindPic(sb_nums[i][j]);
}

void SCR_ExecuteLayoutString(char *s, qboolean isStatusBar)
{
	int value;
	char string[1024];
	int width;
	clientinfo_t *ci;

	float (*scaleForScreen)(float in);
	float (*getScreenScale)();

	if (cls.state != ca_active || !cl.refresh_prepped || !s[0])
		return;

	// Get our scaling functions
	if (isStatusBar)
	{
		scaleForScreen = ScaledHud;
		getScreenScale = HudScale;
	}
	else
	{
		scaleForScreen = SCR_ScaledVideo;
		getScreenScale = SCR_VideoScale;
	}

	InitHudScale();
	int x = 0;
	int y = 0;

	while (s)
	{
		char *token = COM_Parse(&s);
		if (!strcmp(token, "xl"))
		{
			token = COM_Parse(&s);
			x = scaleForScreen(atoi(token));
		}
		else if (!strcmp(token, "xr"))
		{
			token = COM_Parse(&s);
			x = viddef.width + scaleForScreen(atoi(token));
		}
		else if (!strcmp(token, "xv"))
		{
			token = COM_Parse(&s);
			x = viddef.width / 2 - scaleForScreen(160) + scaleForScreen(atoi(token));
		}
		else if (!strcmp(token, "yt"))
		{
			token = COM_Parse(&s);
			y = scaleForScreen(atoi(token));
		}
		else if (!strcmp(token, "yb"))
		{
			token = COM_Parse(&s);
			y = viddef.height + scaleForScreen(atoi(token));
		}
		else if (!strcmp(token, "yv"))
		{
			token = COM_Parse(&s);
			y = viddef.height / 2 - scaleForScreen(120) + scaleForScreen(atoi(token));
		}
		else if (!strcmp(token, "pic"))
		{	
			// Draw a pic from a stat number
			token = COM_Parse(&s);
			value = cl.frame.playerstate.stats[atoi(token)];

			// Knightmare- 1/2/2002- BIG UGLY HACK for old demos or connected to server using old protocol;
			// Changed config strings require different offsets
			const int csimages = (LegacyProtocol() ? OLD_CS_IMAGES : CS_IMAGES); //mxd
			const int maximages = (LegacyProtocol() ? OLD_MAX_IMAGES : MAX_IMAGES); //mxd
			if (value >= maximages) // Knightmare- don't bomb out
			{
				Com_Printf(S_COLOR_YELLOW"Warning: Pic >= MAX_IMAGES\n");
				value = maximages - 1;
			}

			if (cl.configstrings[csimages + value][0]) //mxd. V600 Consider inspecting the condition. The pointer is always not equal to NULL.
				R_DrawScaledPic(x, y, getScreenScale(), hud_alpha->value, cl.configstrings[csimages + value]);
			//end Knightmare
		}
		else if (!strcmp(token, "client"))
		{
			// Draw a deathmatch client block
			token = COM_Parse(&s);
			x = viddef.width / 2 - scaleForScreen(160) + scaleForScreen(atoi(token));
			token = COM_Parse(&s);
			y = viddef.height / 2 - scaleForScreen(120) + scaleForScreen(atoi(token));

			token = COM_Parse(&s);
			value = atoi(token);
			if (value >= MAX_CLIENTS || value < 0)
				Com_Error(ERR_DROP, "client >= MAX_CLIENTS");
			ci = &cl.clientinfo[value];

			token = COM_Parse(&s);
			const int score = atoi(token);

			token = COM_Parse(&s);
			const int ping = atoi(token);

			token = COM_Parse(&s);
			const int time = atoi(token);

			Hud_DrawStringAlt(x + scaleForScreen(32), y, va(S_COLOR_ALT"%s", ci->name), 255, isStatusBar);
			Hud_DrawString(x + scaleForScreen(32), y + scaleForScreen(8), "Score: ", 255, isStatusBar);
			Hud_DrawStringAlt(x + scaleForScreen(32 + 7 * 8), y + scaleForScreen(8), va(S_COLOR_ALT"%i", score), 255, isStatusBar);
			Hud_DrawString(x + scaleForScreen(32), y + scaleForScreen(16), va("Ping:  %i", ping), 255, isStatusBar);
			Hud_DrawString(x + scaleForScreen(32), y + scaleForScreen(24), va("Time:  %i", time), 255, isStatusBar);

			if (!ci->icon)
				ci = &cl.baseclientinfo;

			R_DrawScaledPic(x, y, getScreenScale(), hud_alpha->value,  ci->iconname);
		}
		else if (!strcmp(token, "ctf") || !strcmp(token, "3tctf")) // Knightmare- 3Team CTF block //mxd. Identical logic
		{
			// Draw a ctf client block
			token = COM_Parse(&s);
			x = viddef.width / 2  - scaleForScreen(160) + scaleForScreen(atoi(token));
			token = COM_Parse(&s);
			y = viddef.height / 2 - scaleForScreen(120) + scaleForScreen(atoi(token));

			token = COM_Parse(&s);
			value = atoi(token);
			if (value >= MAX_CLIENTS || value < 0)
				Com_Error(ERR_DROP, "client >= MAX_CLIENTS");
			ci = &cl.clientinfo[value];

			token = COM_Parse(&s);
			const int score = atoi(token);

			token = COM_Parse(&s);
			const int ping = min(atoi(token), 999); //mxd. +min

			char block[80];
			// Double spaced before player name for 2 flag icons
			sprintf(block, "%3d %3d  %-12.12s", score, ping, ci->name);

			if (value == cl.playernum)
				Hud_DrawStringAlt(x, y, block, 255, isStatusBar);
			else
				Hud_DrawString(x, y, block, 255, isStatusBar);
		}
		else if (!strcmp(token, "picn"))
		{
			// Draw a pic from a name
			token = COM_Parse(&s);
			R_DrawScaledPic(x, y, getScreenScale(), hud_alpha->value, token);
		}
		else if (!strcmp(token, "num"))
		{
			// draw a number
			token = COM_Parse(&s);
			width = atoi(token);
			token = COM_Parse(&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			SCR_DrawField(x, y, 0, width, value, false, isStatusBar);
		}
		else if (!strcmp(token, "hnum"))
		{
			// Health number
			int	color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_HEALTH];
			
			if (value > 25)
				color = 0; // Green
			else if (value > 0)
				color = (cl.frame.serverframe >> 2) & 1; // Flash
			else
				color = 1;

			SCR_DrawField(x, y, color, width, value, (cl.frame.playerstate.stats[STAT_FLASHES] & 1), isStatusBar);
		}
		else if (!strcmp(token, "anum"))
		{
			// Ammo number
			int	color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_AMMO];

			if (value > 5)
				color = 0; // Green
			else if (value >= 0)
				color = (cl.frame.serverframe >> 2) & 1; // Flash
			else
				continue; // Negative number = don't show

			SCR_DrawField(x, y, color, width, value, (cl.frame.playerstate.stats[STAT_FLASHES] & 4), isStatusBar);
		}
		else if (!strcmp(token, "rnum"))
		{
			width = 3;
			value = cl.frame.playerstate.stats[STAT_ARMOR];
			if (value < 1)
				continue;

			const int color = 0; // Green

			SCR_DrawField(x, y, color, width, value, (cl.frame.playerstate.stats[STAT_FLASHES] & 2), isStatusBar);
		}
		else if (!strcmp(token, "stat_string"))
		{
			token = COM_Parse(&s);
			int index = atoi(token);

			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error(ERR_DROP, "Bad stat_string index");

			index = cl.frame.playerstate.stats[index];

			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error(ERR_DROP, "Bad stat_string index");

			Hud_DrawString(x, y, cl.configstrings[index], 255, isStatusBar);
		}
		else if (!strcmp(token, "cstring"))
		{
			token = COM_Parse(&s);
			HUD_DrawConfigString(token, x, y, scaleForScreen(320), false, isStatusBar);
		}
		else if (!strcmp(token, "string"))
		{
			token = COM_Parse(&s);
			Hud_DrawString(x, y, token, 255, isStatusBar);
		}
		else if (!strcmp(token, "cstring2"))
		{
			token = COM_Parse(&s);
			HUD_DrawConfigString(token, x, y, scaleForScreen(320), true, isStatusBar);
		}
		else if (!strcmp(token, "string2"))
		{
			token = COM_Parse(&s);
			Com_sprintf(string, sizeof(string), S_COLOR_ALT"%s", token);
			Hud_DrawStringAlt(x, y, string, 255, isStatusBar);
		}
		else if (!strcmp(token, "if"))
		{
			// Draw a number
			token = COM_Parse(&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			if (!value)
			{	
				// Skip to endif
				while (s && strcmp(token, "endif"))
					token = COM_Parse(&s);
			}
		}
	}
}

// The status bar is a small layout program that is based on the stats array
void SCR_DrawStats(void)
{
	SCR_ExecuteLayoutString(cl.configstrings[CS_STATUSBAR], true);
}

void SCR_DrawLayout(void)
{
	if (!cl.frame.playerstate.stats[STAT_LAYOUTS])
		return;

	// Special hack for visor HUD addition in Zaero
	const qboolean isStatusBar = (strstr(cl.layout, "\"Tracking ") != NULL); //mxd
	SCR_ExecuteLayoutString(cl.layout, isStatusBar);
}

#pragma endregion

static void DrawDemoMessage(void) //mxd. Similar logic used in Menu_DrawStatusBar()
{
	// Running demo message
	if (cl.attractloop && !(cl.cinematictime > 0 && cls.realtime - cl.cinematictime > 1000))
	{
		char *message = "Running Demo";

		SCR_DrawFill(0, SCREEN_HEIGHT - (MENU_FONT_SIZE + 3), SCREEN_WIDTH, MENU_FONT_SIZE + 4, ALIGN_BOTTOM_STRETCH, 0, 0, 0, 255); //Black shade
		SCR_DrawFill(0, SCREEN_HEIGHT - (MENU_FONT_SIZE + 2), SCREEN_WIDTH, MENU_FONT_SIZE + 2, ALIGN_BOTTOM_STRETCH, 60, 60, 60, 255); // Gray shade
		SCR_DrawString(SCREEN_WIDTH / 2 - (strlen(message) / 2) * MENU_FONT_SIZE, SCREEN_HEIGHT - (MENU_FONT_SIZE + 1), ALIGN_BOTTOM, message, 255);
	}
}

// This is called every frame, and can also be called explicitly to flush text to the screen.
void SCR_UpdateScreen(void)
{
	int numframes;
	float separation[2] = { 0, 0 };

	// If the screen is disabled (loading plaque is up, or vid mode changing), do nothing at all
	if (cls.disable_screen)
	{
		if (cls.download) // Knightmare- don't time out on downloads
			cls.disable_screen = Sys_Milliseconds();

		if (Sys_Milliseconds() - cls.disable_screen > 120000 && cl.refresh_prepped && cl.cinematictime == 0) // Knightmare- don't time out on vid restart
		{
			cls.disable_screen = 0;
			Com_Printf("Loading plaque timed out.\n");

			return; // Knightmare- moved here for loading screen
		}

		scr_draw_loading = 2; // Knightmare- added for loading screen
	}

	if (!scr_initialized || !con.initialized)
		return; // Not initialized yet

	// Range check cl_camera_separation so we don't inadvertently fry someone's brain
	if (cl_stereo_separation->value > 1.0f)
		Cvar_SetValue("cl_stereo_separation", 1.0f);
	else if (cl_stereo_separation->value < 0)
		Cvar_SetValue("cl_stereo_separation", 0.0f);

	if (cl_stereo->value)
	{
		numframes = 2;
		separation[0] = -cl_stereo_separation->value / 2;
		separation[1] =  cl_stereo_separation->value / 2;
	}
	else
	{
		separation[0] = 0;
		separation[1] = 0;
		numframes = 1;
	}

	for (int i = 0; i < numframes; i++)
	{
		R_BeginFrame(separation[i]);

		if (scr_draw_loading == 2)
		{
			// Knightmare- refresh loading screen
			SCR_DrawLoading();

			// Knightmare- set back for loading screen
			if (cls.disable_screen)
				scr_draw_loading = 2;

			if (cls.consoleActive)
				Con_DrawConsole(0.5, false);

			// NO FULLSCREEN CONSOLE!!!
			continue;
		}

		// If a cinematic is supposed to be running, handle menus and console specially
		if (cl.cinematictime > 0)
		{
			if (cls.key_dest == key_menu)
				UI_Draw();
			else
				SCR_DrawCinematic();
		}
		else 
		{
			// Knightmare added- disconnected menu
			if (cls.state == ca_disconnected && cls.key_dest != key_menu && !scr_draw_loading) 
			{
				SCR_EndLoadingPlaque(); // Get rid of loading plaque
				cls.consoleActive = true; // Show error in console
				M_Menu_Main_f();
			}

			V_RenderView(separation[i]);

			// Don't draw crosshair while in menu
			if (cls.key_dest != key_menu) 
				SCR_DrawCrosshair();

			if (!scr_hidehud)
			{
				SCR_DrawStats();
				if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 1)
					SCR_DrawLayout();
				if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 2)
					CL_DrawInventory();
			}

			SCR_DrawCameraEffect();
			SCR_DrawLetterbox();

			SCR_DrawNet();
			SCR_CheckDrawCenterString();

			if (scr_timegraph->value)
				SCR_DebugGraph(cls.netFrameTime * 300, 0);

			if (scr_debuggraph->value || scr_timegraph->value || scr_netgraph->value)
				SCR_DrawDebugGraph();

			SCR_DrawPause();

			if (cl_demomessage->value)
				DrawDemoMessage();

			SCR_ShowFPS();
			UI_Draw();
			SCR_DrawLoading();
		}

		SCR_DrawConsole();
	}

	GLimp_EndFrame();
}