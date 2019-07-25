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
// common.c -- misc functions used in client and server
#include "qcommon.h"
#include <setjmp.h>
#include "../client/console.h" //mxd

#ifdef _WIN32
	#include "../win32/winquake.h"
#endif

#define MAX_NUM_ARGVS	50

static int com_argc;
static char *com_argv[MAX_NUM_ARGVS + 1];

static jmp_buf abortframe; // An ERR_DROP occured, exit the entire frame

FILE *log_stats_file;

cvar_t *host_speeds;
cvar_t *log_stats;
cvar_t *developer;
cvar_t *timescale;
cvar_t *fixedtime;
cvar_t *logfile_active; // 1 = buffer log, 2 = flush after each print
cvar_t *showtrace;
cvar_t *dedicated;

// Knightmare- for the game DLL to tell what engine it's running under
cvar_t *sv_engine;
cvar_t *sv_engine_version;

static FILE *logfile;

static int server_state;

// host_speeds times
int time_before_game;
int time_after_game;
int time_before_ref;
int time_after_ref;


/*
============================================================================
	CLIENT / SERVER interactions
============================================================================
*/

static int rd_target;
static char *rd_buffer;
static int rd_buffersize;
static void (*rd_flush)(int target, char *buffer);

void Com_BeginRedirect(int target, char *buffer, int buffersize, void (*flush)(int ftarget, char *fbuffer))
{
	if (!target || !buffer || !buffersize || !flush)
		return;

	rd_target = target;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;

	*rd_buffer = 0;
}

void Com_EndRedirect()
{
	rd_flush(rd_target, rd_buffer);

	rd_target = 0;
	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
}

// Both client and server can use this, and it will output to the apropriate place.
extern char *CL_UnformattedString(const char *string); //mxd

//mxd. Store early Com_Printf messages added before logfile_active is initialized and loaded from config.cfg / autoexec.cfg.
#define MAX_EARLY_MESSAGES	256
static char *earlymsg[MAX_EARLY_MESSAGES];
static int earlymsgcount = 0;

static void FreeEarlyMessages()
{
	if (earlymsgcount > 0)
	{
		for (int i = 0; i < earlymsgcount; i++)
			free(earlymsg[i]);

		earlymsgcount = 0;
	}
}

void Com_Printf(char *fmt, ...)
{
	va_list argptr;
	static char msg[MAXPRINTMSG]; //mxd. +static
	static qboolean earlymessagesaddedtoconsole = false; //mxd
	static qboolean earlymessagesaddedtolog = false; //mxd

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr); // Fix for nVidia 191.xx crash
	va_end(argptr);

	if (rd_target)
	{
		if ((int)(strlen(msg) + strlen(rd_buffer)) > rd_buffersize - 1)
		{
			rd_flush(rd_target, rd_buffer);
			*rd_buffer = 0;
		}

		Q_strncatz(rd_buffer, msg, rd_buffersize);
		return;
	}

	//mxd. Add early messages to the console?
	if (con.initialized && !earlymessagesaddedtoconsole)
	{
		for (int i = 0; i < earlymsgcount; i++)
			Con_Print(va("%s\n", earlymsg[i]));

		earlymessagesaddedtoconsole = true;
	}

	Con_Print(msg);

	//mxd. Skip colored text marker
	char *text = msg;
	if (text[0] == 1 || text[0] == 2)
		text++;

	//mxd. If logfile or console isn't initialized yet, store in temporary buffer...
	if ((!earlymessagesaddedtoconsole || !earlymessagesaddedtolog) && earlymsgcount < MAX_EARLY_MESSAGES)
	{
		const int textsize = strlen(text) * sizeof(char);
		earlymsg[earlymsgcount] = malloc(textsize);
		Q_strncpyz(earlymsg[earlymsgcount], text, textsize);
		earlymsgcount++;
	}

	// Remove color escapes and special font chars
	text = CL_UnformattedString(text);

	// Echo to debugging console
	if (text[strlen(text) - 1] != '\r') // Skip overwritten outputs
		Sys_ConsoleOutput(text);

	// Save to logfile?
	if (logfile_active && logfile_active->integer)
	{
		if (!logfile)
		{
			char name[MAX_QPATH];
			Com_sprintf(name, sizeof(name), "%s/"ENGINE_PREFIX"console.log", FS_Gamedir());
			if (logfile_active->integer > 2)
				logfile = fopen(name, "a");
			else
				logfile = fopen(name, "w");
		}

		//mxd. Add early messages?
		if (logfile && !earlymessagesaddedtolog)
		{
			for (int i = 0; i < earlymsgcount; i++)
				fprintf(logfile, "%s\n", CL_UnformattedString(earlymsg[i]));

			earlymessagesaddedtolog = true;
		}

		if (logfile)
			fprintf(logfile, "%s", text);

		if (logfile_active->integer > 1)
			fflush(logfile); // Force it to save every time
	}
}

// A Com_Printf that only shows up if the "developer" cvar is set
void Com_DPrintf(char *fmt, ...)
{
	va_list	argptr;
	static char msg[MAXPRINTMSG]; //mxd. +static
		
	if (!developer || !developer->value)
		return; // don't confuse non-developers with techie stuff...

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr); // fix for nVidia 191.xx crash
	va_end(argptr);
	
	Com_Printf("%s", msg);
}


// mxd. A Com_Printf that only prints to VS console in DEBUG build
void Com_CPrintf(char *fmt, ...)
{
#ifndef _DEBUG
	return;
#endif
	
	va_list	argptr;
	static char msg[MAXPRINTMSG]; //mxd. +static

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr); // fix for nVidia 191.xx crash
	va_end(argptr);

#if defined(_WIN32)
	OutputDebugString(msg);
#else
	Com_Printf(S_COLOR_MAGENTA"%s", msg);
#endif
}

// Both client and server can use this, and it will do the apropriate things.
void Com_Error(int code, char *fmt, ...)
{
	va_list argptr;
	static char msg[MAXPRINTMSG]; //mxd. +static
	static qboolean	recursive;

	if (recursive)
		Sys_Error("%s: recursive error after: %s", __func__, msg);
	recursive = true;

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr); // fix for nVidia 191.xx crash
	va_end(argptr);
	
	if (code == ERR_DISCONNECT)
	{
		CL_Drop();
		recursive = false;
		longjmp(abortframe, -1);
	}

	if (code == ERR_DROP)
	{
		Com_Printf(S_COLOR_RED"********************\n"
				   S_COLOR_RED"ERROR: %s\n"
				   S_COLOR_RED"********************\n", msg);
		SV_Shutdown(va("Server crashed: %s\n", msg), false);
		CL_Drop();
		recursive = false;
		longjmp(abortframe, -1);
	}

	SV_Shutdown(va("Server fatal crashed: %s\n", msg), false);
	//CL_Shutdown(); //mxd. Called from Sys_Error()

	Sys_Error("%s", msg);
}

// Both client and server can use this, and it will do the apropriate things.
void Com_Quit(void)
{
	SV_Shutdown("Server quit\n", false);
	Sys_Quit(false);
}

//mxd
void Com_CloseLogfile(void)
{
	if (logfile)
	{
		fclose(logfile);
		logfile = NULL;
	}
}

int Com_ServerState(void)
{
	return server_state;
}

void Com_SetServerState(int state)
{
	server_state = state;
}

#pragma region ======================= HARDCODED VERTEX NORMALS

//mxd. Stored in anorms.h in Q2
vec3_t vertexnormals[NUMVERTEXNORMALS] =
{
	{-0.525731, 0.000000, 0.850651},
	{-0.442863, 0.238856, 0.864188},
	{-0.295242, 0.000000, 0.955423},
	{-0.309017, 0.500000, 0.809017},
	{-0.162460, 0.262866, 0.951056},
	{0.000000, 0.000000, 1.000000},
	{0.000000, 0.850651, 0.525731},
	{-0.147621, 0.716567, 0.681718},
	{0.147621, 0.716567, 0.681718},
	{0.000000, 0.525731, 0.850651},
	{0.309017, 0.500000, 0.809017},
	{0.525731, 0.000000, 0.850651},
	{0.295242, 0.000000, 0.955423},
	{0.442863, 0.238856, 0.864188},
	{0.162460, 0.262866, 0.951056},
	{-0.681718, 0.147621, 0.716567},
	{-0.809017, 0.309017, 0.500000},
	{-0.587785, 0.425325, 0.688191},
	{-0.850651, 0.525731, 0.000000},
	{-0.864188, 0.442863, 0.238856},
	{-0.716567, 0.681718, 0.147621},
	{-0.688191, 0.587785, 0.425325},
	{-0.500000, 0.809017, 0.309017},
	{-0.238856, 0.864188, 0.442863},
	{-0.425325, 0.688191, 0.587785},
	{-0.716567, 0.681718, -0.147621},
	{-0.500000, 0.809017, -0.309017},
	{-0.525731, 0.850651, 0.000000},
	{0.000000, 0.850651, -0.525731},
	{-0.238856, 0.864188, -0.442863},
	{0.000000, 0.955423, -0.295242},
	{-0.262866, 0.951056, -0.162460},
	{0.000000, 1.000000, 0.000000},
	{0.000000, 0.955423, 0.295242},
	{-0.262866, 0.951056, 0.162460},
	{0.238856, 0.864188, 0.442863},
	{0.262866, 0.951056, 0.162460},
	{0.500000, 0.809017, 0.309017},
	{0.238856, 0.864188, -0.442863},
	{0.262866, 0.951056, -0.162460},
	{0.500000, 0.809017, -0.309017},
	{0.850651, 0.525731, 0.000000},
	{0.716567, 0.681718, 0.147621},
	{0.716567, 0.681718, -0.147621},
	{0.525731, 0.850651, 0.000000},
	{0.425325, 0.688191, 0.587785},
	{0.864188, 0.442863, 0.238856},
	{0.688191, 0.587785, 0.425325},
	{0.809017, 0.309017, 0.500000},
	{0.681718, 0.147621, 0.716567},
	{0.587785, 0.425325, 0.688191},
	{0.955423, 0.295242, 0.000000},
	{1.000000, 0.000000, 0.000000},
	{0.951056, 0.162460, 0.262866},
	{0.850651, -0.525731, 0.000000},
	{0.955423, -0.295242, 0.000000},
	{0.864188, -0.442863, 0.238856},
	{0.951056, -0.162460, 0.262866},
	{0.809017, -0.309017, 0.500000},
	{0.681718, -0.147621, 0.716567},
	{0.850651, 0.000000, 0.525731},
	{0.864188, 0.442863, -0.238856},
	{0.809017, 0.309017, -0.500000},
	{0.951056, 0.162460, -0.262866},
	{0.525731, 0.000000, -0.850651},
	{0.681718, 0.147621, -0.716567},
	{0.681718, -0.147621, -0.716567},
	{0.850651, 0.000000, -0.525731},
	{0.809017, -0.309017, -0.500000},
	{0.864188, -0.442863, -0.238856},
	{0.951056, -0.162460, -0.262866},
	{0.147621, 0.716567, -0.681718},
	{0.309017, 0.500000, -0.809017},
	{0.425325, 0.688191, -0.587785},
	{0.442863, 0.238856, -0.864188},
	{0.587785, 0.425325, -0.688191},
	{0.688191, 0.587785, -0.425325},
	{-0.147621, 0.716567, -0.681718},
	{-0.309017, 0.500000, -0.809017},
	{0.000000, 0.525731, -0.850651},
	{-0.525731, 0.000000, -0.850651},
	{-0.442863, 0.238856, -0.864188},
	{-0.295242, 0.000000, -0.955423},
	{-0.162460, 0.262866, -0.951056},
	{0.000000, 0.000000, -1.000000},
	{0.295242, 0.000000, -0.955423},
	{0.162460, 0.262866, -0.951056},
	{-0.442863, -0.238856, -0.864188},
	{-0.309017, -0.500000, -0.809017},
	{-0.162460, -0.262866, -0.951056},
	{0.000000, -0.850651, -0.525731},
	{-0.147621, -0.716567, -0.681718},
	{0.147621, -0.716567, -0.681718},
	{0.000000, -0.525731, -0.850651},
	{0.309017, -0.500000, -0.809017},
	{0.442863, -0.238856, -0.864188},
	{0.162460, -0.262866, -0.951056},
	{0.238856, -0.864188, -0.442863},
	{0.500000, -0.809017, -0.309017},
	{0.425325, -0.688191, -0.587785},
	{0.716567, -0.681718, -0.147621},
	{0.688191, -0.587785, -0.425325},
	{0.587785, -0.425325, -0.688191},
	{0.000000, -0.955423, -0.295242},
	{0.000000, -1.000000, 0.000000},
	{0.262866, -0.951056, -0.162460},
	{0.000000, -0.850651, 0.525731},
	{0.000000, -0.955423, 0.295242},
	{0.238856, -0.864188, 0.442863},
	{0.262866, -0.951056, 0.162460},
	{0.500000, -0.809017, 0.309017},
	{0.716567, -0.681718, 0.147621},
	{0.525731, -0.850651, 0.000000},
	{-0.238856, -0.864188, -0.442863},
	{-0.500000, -0.809017, -0.309017},
	{-0.262866, -0.951056, -0.162460},
	{-0.850651, -0.525731, 0.000000},
	{-0.716567, -0.681718, -0.147621},
	{-0.716567, -0.681718, 0.147621},
	{-0.525731, -0.850651, 0.000000},
	{-0.500000, -0.809017, 0.309017},
	{-0.238856, -0.864188, 0.442863},
	{-0.262866, -0.951056, 0.162460},
	{-0.864188, -0.442863, 0.238856},
	{-0.809017, -0.309017, 0.500000},
	{-0.688191, -0.587785, 0.425325},
	{-0.681718, -0.147621, 0.716567},
	{-0.442863, -0.238856, 0.864188},
	{-0.587785, -0.425325, 0.688191},
	{-0.309017, -0.500000, 0.809017},
	{-0.147621, -0.716567, 0.681718},
	{-0.425325, -0.688191, 0.587785},
	{-0.162460, -0.262866, 0.951056},
	{0.442863, -0.238856, 0.864188},
	{0.162460, -0.262866, 0.951056},
	{0.309017, -0.500000, 0.809017},
	{0.147621, -0.716567, 0.681718},
	{0.000000, -0.525731, 0.850651},
	{0.425325, -0.688191, 0.587785},
	{0.587785, -0.425325, 0.688191},
	{0.688191, -0.587785, 0.425325},
	{-0.955423, 0.295242, 0.000000},
	{-0.951056, 0.162460, 0.262866},
	{-1.000000, 0.000000, 0.000000},
	{-0.850651, 0.000000, 0.525731},
	{-0.955423, -0.295242, 0.000000},
	{-0.951056, -0.162460, 0.262866},
	{-0.864188, 0.442863, -0.238856},
	{-0.951056, 0.162460, -0.262866},
	{-0.809017, 0.309017, -0.500000},
	{-0.864188, -0.442863, -0.238856},
	{-0.951056, -0.162460, -0.262866},
	{-0.809017, -0.309017, -0.500000},
	{-0.681718, 0.147621, -0.716567},
	{-0.681718, -0.147621, -0.716567},
	{-0.850651, 0.000000, -0.525731},
	{-0.688191, 0.587785, -0.425325},
	{-0.587785, 0.425325, -0.688191},
	{-0.425325, 0.688191, -0.587785},
	{-0.425325, -0.688191, -0.587785},
	{-0.587785, -0.425325, -0.688191},
	{-0.688191, -0.587785, -0.425325}
};

#pragma endregion

/*
==============================================================================
	MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

//
// Writing functions
//

void MSG_WriteChar(sizebuf_t *sb, const int c)
{
#ifdef PARANOID
	if (c < -128 || c > 127)
		Com_Error(ERR_FATAL, "MSG_WriteChar: range error");
#endif

	byte *buf = SZ_GetSpace(sb, 1);
	buf[0] = c;
}

void MSG_WriteByte(sizebuf_t *sb, const int c)
{
#ifdef PARANOID
	if (c < 0 || c > 255)
		Com_Error(ERR_FATAL, "MSG_WriteByte: range error");
#endif

	byte *buf = SZ_GetSpace(sb, 1);
	buf[0] = c;
}

void MSG_WriteShort(sizebuf_t *sb, const int c)
{
#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
		Com_Error(ERR_FATAL, "MSG_WriteShort: range error");
#endif

	byte *buf = SZ_GetSpace(sb, 2);
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

void MSG_WriteLong(sizebuf_t *sb, const int c)
{
	byte *buf = SZ_GetSpace(sb, 4);
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

void MSG_WriteFloat(sizebuf_t *sb, const float f)
{
	union
	{
		float f;
		int	l;
	} dat;
	
	
	dat.f = f;
	
	SZ_Write(sb, &dat.l, 4);
}

void MSG_WriteString(sizebuf_t *sb, const char *s)
{
	if (!s)
		SZ_Write(sb, "", 1);
	else
		SZ_Write(sb, s, strlen(s) + 1);
}

// 24-bit coordinate transmission code
#ifdef LARGE_MAP_SIZE

#define BIT_23	0x00800000
#define UPRBITS	0xFF000000

extern qboolean LegacyProtocol();

static void MSG_WriteCoordNew(sizebuf_t *sb, const float f)
{
	const int tmp = f * 8;				// 1/8 granulation, leaves bounds of +/-1M in signed 24-bit form
	const byte trans1 = tmp >> 16;		// bits 16-23
	const unsigned short trans2 = tmp;	// bits 0-15

	// Don't mess with sign bits on this end to allow overflow (map wrap-around).
	MSG_WriteByte(sb, trans1);
	MSG_WriteShort(sb, trans2);
}

static float MSG_ReadCoordNew(sizebuf_t *msg_read)
{
	const byte trans1 = MSG_ReadByte(msg_read);
	const ushort trans2 = MSG_ReadShort(msg_read);

	int tmp = trans1 << 16;	// bits 16-23
	tmp += trans2;			// bits 0-15

	// Sign bit 23 means it's negative, so fill upper
	// 8 bits with 1s for 2's complement negative.
	if (tmp & BIT_23)	
		tmp |= UPRBITS;

	return tmp * (1.0 / 8);	// restore 1/8 granulation
}

// Player movement coords are already in 1/8 precision integer form
void MSG_WritePMCoordNew(sizebuf_t *sb, const int in)
{
	const byte trans1 = in >> 16;	  // bits 16-23
	const unsigned short trans2 = in; // bits 0-15

	MSG_WriteByte(sb, trans1);
	MSG_WriteShort(sb, trans2);
}

int MSG_ReadPMCoordNew(sizebuf_t *msg_read)
{
	const byte trans1 = MSG_ReadByte(msg_read);
	const unsigned short trans2 = MSG_ReadShort(msg_read);

	int tmp = trans1 << 16;	// bits 16-23
	tmp += trans2;			// bits 0-15

	// Sign bit 23 means it's negative, so fill upper 8 bits with 1s for 2's complement negative.
	if (tmp & BIT_23)
		tmp |= UPRBITS;

	return tmp;
}

#endif // LARGE_MAP_SIZE

#ifdef LARGE_MAP_SIZE

void MSG_WriteCoord(sizebuf_t *sb, const float f)
{
	MSG_WriteCoordNew(sb, f);
}

#else // LARGE_MAP_SIZE

void MSG_WriteCoord(sizebuf_t *sb, const float f)
{
	MSG_WriteShort(sb, (int)(f * 8));
}

#endif // LARGE_MAP_SIZE

#ifdef LARGE_MAP_SIZE

void MSG_WritePos(sizebuf_t *sb, const vec3_t pos)
{
	MSG_WriteCoordNew(sb, pos[0]);
	MSG_WriteCoordNew(sb, pos[1]);
	MSG_WriteCoordNew(sb, pos[2]);
}

#else // LARGE_MAP_SIZE

void MSG_WritePos(sizebuf_t *sb, vec3_t pos)
{
	MSG_WriteShort(sb, (int)(pos[0] * 8));
	MSG_WriteShort(sb, (int)(pos[1] * 8));
	MSG_WriteShort(sb, (int)(pos[2] * 8));
}

#endif // LARGE_MAP_SIZE

void MSG_WriteAngle(sizebuf_t *sb, const float f)
{
	MSG_WriteByte(sb, (int)(f * 256 / 360) & 255);
}

void MSG_WriteAngle16(sizebuf_t *sb, const float f)
{
	MSG_WriteShort(sb, ANGLE2SHORT(f));
}

void MSG_WriteDeltaUsercmd(sizebuf_t *sb, usercmd_t *from, usercmd_t *cmd)
{
	// Send the movement message
	int bits = 0;
	if (cmd->angles[0] != from->angles[0])
		bits |= CM_ANGLE1;
	if (cmd->angles[1] != from->angles[1])
		bits |= CM_ANGLE2;
	if (cmd->angles[2] != from->angles[2])
		bits |= CM_ANGLE3;
	if (cmd->forwardmove != from->forwardmove)
		bits |= CM_FORWARD;
	if (cmd->sidemove != from->sidemove)
		bits |= CM_SIDE;
	if (cmd->upmove != from->upmove)
		bits |= CM_UP;
	if (cmd->buttons != from->buttons)
		bits |= CM_BUTTONS;
	if (cmd->impulse != from->impulse)
		bits |= CM_IMPULSE;

	MSG_WriteByte(sb, bits);

	if (bits & CM_ANGLE1)
		MSG_WriteShort(sb, cmd->angles[0]);
	if (bits & CM_ANGLE2)
		MSG_WriteShort(sb, cmd->angles[1]);
	if (bits & CM_ANGLE3)
		MSG_WriteShort(sb, cmd->angles[2]);

	if (bits & CM_FORWARD)
		MSG_WriteShort(sb, cmd->forwardmove);
	if (bits & CM_SIDE)
		MSG_WriteShort(sb, cmd->sidemove);
	if (bits & CM_UP)
		MSG_WriteShort(sb, cmd->upmove);

	if (bits & CM_BUTTONS)
		MSG_WriteByte(sb, cmd->buttons);
	if (bits & CM_IMPULSE)
		MSG_WriteByte(sb, cmd->impulse);

	MSG_WriteByte(sb, cmd->msec);
	MSG_WriteByte(sb, cmd->lightlevel);
}

void MSG_WriteDir(sizebuf_t *sb, const vec3_t dir)
{
	if (!dir)
	{
		MSG_WriteByte(sb, 0);
		return;
	}

	float bestd = 0;
	int best = 0;
	for (int i = 0; i < NUMVERTEXNORMALS; i++)
	{
		const float d = DotProduct(dir, vertexnormals[i]);
		if (d > bestd)
		{
			bestd = d;
			best = i;
		}
	}

	MSG_WriteByte(sb, best);
}

void MSG_ReadDir(sizebuf_t *sb, vec3_t dir)
{
	const int b = MSG_ReadByte(sb);
	if (b >= NUMVERTEXNORMALS)
		Com_Error(ERR_DROP, "MSF_ReadDir: out of range");
	VectorCopy(vertexnormals[b], dir);
}

// Writes part of a packetentities message.
// Can delta from either a baseline or a previous packet_entity
void MSG_WriteDeltaEntity(entity_state_t *from, entity_state_t *to, sizebuf_t *msg, const qboolean force, const qboolean newentity)
{
	if (!to->number)
		Com_Error(ERR_FATAL, "Unset entity number");

	if (to->number >= MAX_EDICTS)
		Com_Error(ERR_FATAL, "Entity number >= MAX_EDICTS");

	// Send an update
	int bits = 0;

	if (to->number >= 256)
		bits |= U_NUMBER16; // number8 is implicit otherwise

	if (to->origin[0] != from->origin[0])
		bits |= U_ORIGIN1;
	if (to->origin[1] != from->origin[1])
		bits |= U_ORIGIN2;
	if (to->origin[2] != from->origin[2])
		bits |= U_ORIGIN3;

	if (to->angles[0] != from->angles[0])
		bits |= U_ANGLE1;
	if (to->angles[1] != from->angles[1])
		bits |= U_ANGLE2;
	if (to->angles[2] != from->angles[2])
		bits |= U_ANGLE3;
		
	if (to->skinnum != from->skinnum)
	{
		if ((unsigned)to->skinnum < 256)
			bits |= U_SKIN8;
		else if ((unsigned)to->skinnum < 0x10000)
			bits |= U_SKIN16;
		else
			bits |= (U_SKIN8 | U_SKIN16);
	}
		
	if (to->frame != from->frame)
	{
		if (to->frame < 256)
			bits |= U_FRAME8;
		else
			bits |= U_FRAME16;
	}

	if (to->effects != from->effects)
	{
		if (to->effects < 256)
			bits |= U_EFFECTS8;
		else if (to->effects < 0x8000)
			bits |= U_EFFECTS16;
		else
			bits |= U_EFFECTS8 | U_EFFECTS16;
	}
	
	if (to->renderfx != from->renderfx)
	{
		if (to->renderfx < 256)
			bits |= U_RENDERFX8;
		else if (to->renderfx < 0x8000)
			bits |= U_RENDERFX16;
		else
			bits |= U_RENDERFX8 | U_RENDERFX16;
	}
	
	if (to->solid != from->solid)
		bits |= U_SOLID;

	// Event is not delta compressed, just 0 compressed
	if (to->event)
		bits |= U_EVENT;
	
	if (to->modelindex != from->modelindex)
		bits |= U_MODEL;
	if (to->modelindex2 != from->modelindex2)
		bits |= U_MODEL2;
	if (to->modelindex3 != from->modelindex3)
		bits |= U_MODEL3;
	if (to->modelindex4 != from->modelindex4)
		bits |= U_MODEL4;

#ifdef NEW_ENTITY_STATE_MEMBERS
	// 1/18/2002- extra model indices	
	if (to->modelindex5 != from->modelindex5)
		bits |= U_MODEL5;
	if (to->modelindex6 != from->modelindex6)
		bits |= U_MODEL6;
#endif

	if (to->sound != from->sound)
		bits |= U_SOUND;

#if defined(NEW_ENTITY_STATE_MEMBERS) && defined(LOOP_SOUND_ATTENUATION)
	if (to->attenuation != from->attenuation)
		bits |= U_ATTENUAT;
#endif

	if (newentity || (to->renderfx & RF_BEAM))
		bits |= U_OLDORIGIN;

	// Knightmare 5/11/2002- added alpha
#ifdef NEW_ENTITY_STATE_MEMBERS
	// Cap new value to correct range
	to->alpha = clamp(to->alpha, 0.0, 1.0);

	// Since the floating point value is never quite the same, compare the new and the old as what they will be sent as
	if ((int)(to->alpha * 255) != (int)(from->alpha * 255))
		bits |= U_ALPHA;
#endif

	//
	// Write the message
	//
	if (!bits && !force)
		return; // nothing to send!

	//----------

	if (bits & 0xff000000)
		bits |= U_MOREBITS3 | U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x00ff0000)
		bits |= U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x0000ff00)
		bits |= U_MOREBITS1;

	MSG_WriteByte(msg, bits & 255);

	if (bits & 0xff000000)
	{
		MSG_WriteByte(msg, (bits >> 8) & 255);
		MSG_WriteByte(msg, (bits >> 16) & 255);
		MSG_WriteByte(msg, (bits >> 24) & 255);
	}
	else if (bits & 0x00ff0000)
	{
		MSG_WriteByte(msg, (bits >> 8) & 255);
		MSG_WriteByte(msg, (bits >> 16) & 255);
	}
	else if (bits & 0x0000ff00)
	{
		MSG_WriteByte(msg, (bits >> 8) & 255);
	}

	//----------

	if (bits & U_NUMBER16)
		MSG_WriteShort(msg, to->number);
	else
		MSG_WriteByte(msg, to->number);

	//Knightmare- 12/23/2001. Changed these to shorts
	if (bits & U_MODEL)
		MSG_WriteShort(msg, to->modelindex);
	if (bits & U_MODEL2)
		MSG_WriteShort(msg, to->modelindex2);
	if (bits & U_MODEL3)
		MSG_WriteShort(msg, to->modelindex3);
	if (bits & U_MODEL4)
		MSG_WriteShort(msg, to->modelindex4);

#ifdef NEW_ENTITY_STATE_MEMBERS
	// 1/18/2002- extra model indices
	if (bits & U_MODEL5)
		MSG_WriteShort(msg, to->modelindex5);
	if (bits & U_MODEL6)
		MSG_WriteShort(msg, to->modelindex6);
#endif

	if (bits & U_FRAME8)
		MSG_WriteByte(msg, to->frame);
	if (bits & U_FRAME16)
		MSG_WriteShort(msg, to->frame);

	if ((bits & U_SKIN8) && (bits & U_SKIN16))		// used for laser colors
		MSG_WriteLong(msg, to->skinnum);
	else if (bits & U_SKIN8)
		MSG_WriteByte(msg, to->skinnum);
	else if (bits & U_SKIN16)
		MSG_WriteShort(msg, to->skinnum);


	if ((bits & (U_EFFECTS8 | U_EFFECTS16)) == (U_EFFECTS8 | U_EFFECTS16))
		MSG_WriteLong(msg, to->effects);
	else if (bits & U_EFFECTS8)
		MSG_WriteByte(msg, to->effects);
	else if (bits & U_EFFECTS16)
		MSG_WriteShort(msg, to->effects);

	if ((bits & (U_RENDERFX8 | U_RENDERFX16)) == (U_RENDERFX8 | U_RENDERFX16))
		MSG_WriteLong(msg, to->renderfx);
	else if (bits & U_RENDERFX8)
		MSG_WriteByte(msg, to->renderfx);
	else if (bits & U_RENDERFX16)
		MSG_WriteShort(msg, to->renderfx);

	if (bits & U_ORIGIN1)
		MSG_WriteCoord(msg, to->origin[0]);
	if (bits & U_ORIGIN2)
		MSG_WriteCoord(msg, to->origin[1]);
	if (bits & U_ORIGIN3)
		MSG_WriteCoord(msg, to->origin[2]);

	if (LegacyProtocol())
	{
		if (bits & U_ANGLE1)
			MSG_WriteAngle(msg, to->angles[0]);
		if (bits & U_ANGLE2)
			MSG_WriteAngle(msg, to->angles[1]);
		if (bits & U_ANGLE3)
			MSG_WriteAngle(msg, to->angles[2]);
	}
	else //mxd. Send more precise angles. Fixes jerky movement of func_rotating with slow speed.
	{
		if (bits & U_ANGLE1)
			MSG_WriteAngle16(msg, to->angles[0]);
		if (bits & U_ANGLE2)
			MSG_WriteAngle16(msg, to->angles[1]);
		if (bits & U_ANGLE3)
			MSG_WriteAngle16(msg, to->angles[2]);
	}

	if (bits & U_OLDORIGIN)
	{
		MSG_WriteCoord(msg, to->old_origin[0]);
		MSG_WriteCoord(msg, to->old_origin[1]);
		MSG_WriteCoord(msg, to->old_origin[2]);
	}

#ifdef NEW_ENTITY_STATE_MEMBERS
	//Knightmare 5/11/2002- added alpha
	if (bits & U_ALPHA)
		MSG_WriteByte(msg, (byte)(to->alpha * 255));
#endif

	//Knightmare- 12/23/2001. Changed this to short
	if (bits & U_SOUND)
		MSG_WriteShort(msg, to->sound);

#if defined(NEW_ENTITY_STATE_MEMBERS) && defined(LOOP_SOUND_ATTENUATION)
	if (bits & U_ATTENUAT)
		MSG_WriteByte(msg, (int)(min(max(to->attenuation, 0.0f), 4.0f) * 64.0));
#endif

	if (bits & U_EVENT)
		MSG_WriteByte(msg, to->event);
	if (bits & U_SOLID)
		MSG_WriteShort(msg, to->solid);
}

/*
============================================================
	READING FUNCTIONS
============================================================
*/
void MSG_BeginReading(sizebuf_t *msg)
{
	msg->readcount = 0;
}

// returns -1 if no more characters are available
int MSG_ReadChar(sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount + 1 > msg_read->cursize)
		c = -1;
	else
		c = (signed char)msg_read->data[msg_read->readcount];

	msg_read->readcount++;
	
	return c;
}

int MSG_ReadByte(sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount + 1 > msg_read->cursize)
		c = -1;
	else
		c = (unsigned char)msg_read->data[msg_read->readcount];

	msg_read->readcount++;
	
	return c;
}

int MSG_ReadShort(sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount + 2 > msg_read->cursize)
		c = -1;
	else
		c = (short)(msg_read->data[msg_read->readcount] + (msg_read->data[msg_read->readcount + 1] << 8));
	
	msg_read->readcount += 2;
	
	return c;
}

int MSG_ReadLong(sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount + 4 > msg_read->cursize)
	{
		c = -1;
	}
	else
	{
		c = msg_read->data[msg_read->readcount]
			+ (msg_read->data[msg_read->readcount + 1] << 8)
			+ (msg_read->data[msg_read->readcount + 2] << 16)
			+ (msg_read->data[msg_read->readcount + 3] << 24);
	}
	
	msg_read->readcount += 4;
	
	return c;
}

float MSG_ReadFloat(sizebuf_t *msg_read)
{
	union
	{
		byte b[4];
		float f;
		int l;
	} dat;
	
	if (msg_read->readcount + 4 > msg_read->cursize)
	{
		dat.f = -1;
	}
	else
	{
		dat.b[0] = msg_read->data[msg_read->readcount];
		dat.b[1] = msg_read->data[msg_read->readcount + 1];
		dat.b[2] = msg_read->data[msg_read->readcount + 2];
		dat.b[3] = msg_read->data[msg_read->readcount + 3];
	}

	msg_read->readcount += 4;

	return dat.f;
}

char *MSG_ReadString(sizebuf_t *msg_read)
{
	static char	string[2048];

	int l = 0;
	do
	{
		const int c = MSG_ReadChar(msg_read);
		if (c == -1 || c == 0)
			break;

		string[l] = c;
		l++;
	} while (l < sizeof(string) - 1);
	
	string[l] = 0;
	
	return string;
}

char *MSG_ReadStringLine(sizebuf_t *msg_read)
{
	static char	string[2048];

	int l = 0;
	do
	{
		const int c = MSG_ReadChar(msg_read);
		if (c == -1 || c == 0 || c == '\n')
			break;

		string[l] = c;
		l++;
	} while (l < sizeof(string) - 1);
	
	string[l] = 0;
	
	return string;
}


#ifdef LARGE_MAP_SIZE

float MSG_ReadCoord(sizebuf_t *msg_read)
{
	if (LegacyProtocol())
		return MSG_ReadShort(msg_read) * (1.0 / 8);

	return MSG_ReadCoordNew(msg_read);
}

#else // LARGE_MAP_SIZE

float MSG_ReadCoord(sizebuf_t *msg_read)
{
	return MSG_ReadShort(msg_read) * (1.0 / 8);
}

#endif // LARGE_MAP_SIZE


#ifdef LARGE_MAP_SIZE

void MSG_ReadPos(sizebuf_t *msg_read, vec3_t pos)
{
	if (LegacyProtocol())
	{
		pos[0] = MSG_ReadShort(msg_read) * (1.0 / 8);
		pos[1] = MSG_ReadShort(msg_read) * (1.0 / 8);
		pos[2] = MSG_ReadShort(msg_read) * (1.0 / 8);
	}
	else
	{
		pos[0] = MSG_ReadCoordNew(msg_read);
		pos[1] = MSG_ReadCoordNew(msg_read);
		pos[2] = MSG_ReadCoordNew(msg_read);
	}
}

#else // LARGE_MAP_SIZE

void MSG_ReadPos(sizebuf_t *msg_read, vec3_t pos)
{
	pos[0] = MSG_ReadShort(msg_read) * (1.0 / 8);
	pos[1] = MSG_ReadShort(msg_read) * (1.0 / 8);
	pos[2] = MSG_ReadShort(msg_read) * (1.0 / 8);
}

#endif // LARGE_MAP_SIZE


float MSG_ReadAngle(sizebuf_t *msg_read)
{
	return MSG_ReadChar(msg_read) * (360.0 / 256);
}

float MSG_ReadAngle16(sizebuf_t *msg_read)
{
	return SHORT2ANGLE(MSG_ReadShort(msg_read));
}

void MSG_ReadDeltaUsercmd(sizebuf_t *msg_read, usercmd_t *from, usercmd_t *move)
{
	memcpy(move, from, sizeof(*move));

	const int bits = MSG_ReadByte(msg_read);
		
	// Read current angles
	if (bits & CM_ANGLE1)
		move->angles[0] = MSG_ReadShort(msg_read);
	if (bits & CM_ANGLE2)
		move->angles[1] = MSG_ReadShort(msg_read);
	if (bits & CM_ANGLE3)
		move->angles[2] = MSG_ReadShort(msg_read);
		
	// Read movement
	if (bits & CM_FORWARD)
		move->forwardmove = MSG_ReadShort(msg_read);
	if (bits & CM_SIDE)
		move->sidemove = MSG_ReadShort(msg_read);
	if (bits & CM_UP)
		move->upmove = MSG_ReadShort(msg_read);
	
	// Read buttons
	if (bits & CM_BUTTONS)
		move->buttons = MSG_ReadByte(msg_read);

	if (bits & CM_IMPULSE)
		move->impulse = MSG_ReadByte(msg_read);

	// Read time to run command
	move->msec = MSG_ReadByte(msg_read);

	// Read the light level
	move->lightlevel = MSG_ReadByte(msg_read);
}


void MSG_ReadData(sizebuf_t *msg_read, void *data, int len)
{
	for (int i = 0; i < len; i++)
		((byte *)data)[i] = MSG_ReadByte(msg_read);
}


//===========================================================================

void SZ_Init(sizebuf_t *buf, byte *data, int length)
{
	memset(buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
}

void SZ_Clear(sizebuf_t *buf)
{
	buf->cursize = 0;
	buf->overflowed = false;
}

void *SZ_GetSpace(sizebuf_t *buf, int length)
{
	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Com_Error(ERR_FATAL, "SZ_GetSpace: overflow without allowoverflow set");
		
		if (length > buf->maxsize)
			Com_Error(ERR_FATAL, "SZ_GetSpace: %i is > full buffer size", length);
			
		Com_Printf("SZ_GetSpace: overflow\n");
		SZ_Clear(buf); 
		buf->overflowed = true;
	}

	void *data = buf->data + buf->cursize;
	buf->cursize += length;
	
	return data;
}

void SZ_Write(sizebuf_t *buf, const void *data, const int length)
{
	memcpy(SZ_GetSpace(buf, length), data, length);
}

void SZ_Print(sizebuf_t *buf, char *data)
{
	const int len = strlen(data) + 1;

	if (buf->cursize)
	{
		if (buf->data[buf->cursize - 1])
			memcpy((byte *)SZ_GetSpace(buf, len), data, len); // no trailing 0
		else
			memcpy((byte *)SZ_GetSpace(buf, len - 1) - 1, data, len); // write over trailing 0
	}
	else
	{
		memcpy((byte *)SZ_GetSpace(buf, len), data, len);
	}
}


//============================================================================

// Returns the position (1 to argc-1) in the program's argument list where the given parameter apears, or 0 if not present
int COM_CheckParm(char *parm)
{
	for (int i = 1; i < com_argc; i++)
		if (!strcmp(parm, com_argv[i]))
			return i;
		
	return 0;
}

int COM_Argc(void)
{
	return com_argc;
}

char *COM_Argv(int arg)
{
	if (arg < 0 || arg >= com_argc || !com_argv[arg])
		return "";

	return com_argv[arg];
}

void COM_ClearArgv(int arg)
{
	if (arg < 0 || arg >= com_argc || !com_argv[arg])
		return;

	com_argv[arg] = "";
}

void COM_InitArgv(int argc, char **argv)
{
	if (argc > MAX_NUM_ARGVS)
		Com_Error(ERR_FATAL, "argc > MAX_NUM_ARGVS");

	com_argc = argc;
	for (int i = 0; i < argc; i++)
	{
		if (!argv[i] || strlen(argv[i]) >= MAX_TOKEN_CHARS)
			com_argv[i] = "";
		else
			com_argv[i] = argv[i];
	}
}

// Adds the given string at the end of the current argument list
void COM_AddParm(char *parm)
{
	if (com_argc == MAX_NUM_ARGVS)
		Com_Error(ERR_FATAL, "COM_AddParm: MAX_NUM)ARGS");

	com_argv[com_argc++] = parm;
}

char *CopyString(const char *in)
{
	char *out = Z_Malloc(strlen(in) + 1);
	Q_strncpyz(out, in, strlen(in) + 1);
	return out;
}

//mxd. From Q2E
void FreeString(char *in)
{
	if (!in)
		Com_Error(ERR_FATAL, "FreeString: NULL string\n");

	Z_Free(in);
}

void Info_Print(char *s)
{
	char key[512];
	char value[512];

	if (*s == '\\')
		s++;

	while (*s)
	{
		char *o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		const int l = o - key;
		if (l < 20)
		{
			memset(o, ' ', 20 - l);
			key[20] = 0;
		}
		else
		{
			*o = 0;
		}

		Com_Printf("%s", key);

		if (!*s)
		{
			Com_Printf("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;

		while (*s && *s != '\\')
			*o++ = *s++;

		*o = 0;

		if (*s)
			s++;

		Com_Printf("%s\n", value);
	}
}


/*
==============================================================================
	ZONE MEMORY ALLOCATION

just cleared malloc with counters now...
==============================================================================
*/

#define Z_MAGIC 0x1d1d

typedef struct zhead_s
{
	struct zhead_s *prev, *next;
	short magic;
	short tag; // for group free
	int size;
} zhead_t;

zhead_t	z_chain;
int z_count, z_bytes;

void Z_Free(void *ptr)
{
	zhead_t *z = (zhead_t *)ptr - 1;

	if (z->magic != Z_MAGIC)
		Com_Error(ERR_FATAL, "Z_Free: bad magic");

	z->prev->next = z->next;
	z->next->prev = z->prev;

	z_count--;
	z_bytes -= z->size;
	free(z);
}

void Z_Stats_f(void)
{
	Com_Printf("%i bytes in %i blocks\n", z_bytes, z_count);
}

void Z_FreeTags(int tag)
{
	zhead_t *next;

	for (zhead_t *z = z_chain.next; z != &z_chain; z = next)
	{
		next = z->next;
		if (z->tag == tag)
			Z_Free((void *)(z + 1));
	}
}

void *Z_TagMalloc(int size, int tag)
{
	size += sizeof(zhead_t);
	zhead_t *z = malloc(size);
	if (!z)
	{
		Com_Error(ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes", size);
		return NULL; //mxd. Silence PVS warning...
	}

	memset(z, 0, size);
	z_count++;
	z_bytes += size;
	z->magic = Z_MAGIC;
	z->tag = tag;
	z->size = size;

	z->next = z_chain.next;
	z->prev = &z_chain;
	z_chain.next->prev = z;
	z_chain.next = z;

	return (void *)(z + 1);
}

void *Z_Malloc(int size)
{
	return Z_TagMalloc(size, 0);
}

#pragma region ======================= Hashing

static byte chktbl[1024] =
{
	0x84, 0x47, 0x51, 0xc1, 0x93, 0x22, 0x21, 0x24, 0x2f, 0x66, 0x60, 0x4d, 0xb0, 0x7c, 0xda,
	0x88, 0x54, 0x15, 0x2b, 0xc6, 0x6c, 0x89, 0xc5, 0x9d, 0x48, 0xee, 0xe6, 0x8a, 0xb5, 0xf4,
	0xcb, 0xfb, 0xf1, 0x0c, 0x2e, 0xa0, 0xd7, 0xc9, 0x1f, 0xd6, 0x06, 0x9a, 0x09, 0x41, 0x54,
	0x67, 0x46, 0xc7, 0x74, 0xe3, 0xc8, 0xb6, 0x5d, 0xa6, 0x36, 0xc4, 0xab, 0x2c, 0x7e, 0x85,
	0xa8, 0xa4, 0xa6, 0x4d, 0x96, 0x19, 0x19, 0x9a, 0xcc, 0xd8, 0xac, 0x39, 0x5e, 0x3c, 0xf2,
	0xf5, 0x5a, 0x72, 0xe5, 0xa9, 0xd1, 0xb3, 0x23, 0x82, 0x6f, 0x29, 0xcb, 0xd1, 0xcc, 0x71,
	0xfb, 0xea, 0x92, 0xeb, 0x1c, 0xca, 0x4c, 0x70, 0xfe, 0x4d, 0xc9, 0x67, 0x43, 0x47, 0x94,
	0xb9, 0x47, 0xbc, 0x3f, 0x01, 0xab, 0x7b, 0xa6, 0xe2, 0x76, 0xef, 0x5a, 0x7a, 0x29, 0x0b,
	0x51, 0x54, 0x67, 0xd8, 0x1c, 0x14, 0x3e, 0x29, 0xec, 0xe9, 0x2d, 0x48, 0x67, 0xff, 0xed,
	0x54, 0x4f, 0x48, 0xc0, 0xaa, 0x61, 0xf7, 0x78, 0x12, 0x03, 0x7a, 0x9e, 0x8b, 0xcf, 0x83,
	0x7b, 0xae, 0xca, 0x7b, 0xd9, 0xe9, 0x53, 0x2a, 0xeb, 0xd2, 0xd8, 0xcd, 0xa3, 0x10, 0x25,
	0x78, 0x5a, 0xb5, 0x23, 0x06, 0x93, 0xb7, 0x84, 0xd2, 0xbd, 0x96, 0x75, 0xa5, 0x5e, 0xcf,
	0x4e, 0xe9, 0x50, 0xa1, 0xe6, 0x9d, 0xb1, 0xe3, 0x85, 0x66, 0x28, 0x4e, 0x43, 0xdc, 0x6e,
	0xbb, 0x33, 0x9e, 0xf3, 0x0d, 0x00, 0xc1, 0xcf, 0x67, 0x34, 0x06, 0x7c, 0x71, 0xe3, 0x63,
	0xb7, 0xb7, 0xdf, 0x92, 0xc4, 0xc2, 0x25, 0x5c, 0xff, 0xc3, 0x6e, 0xfc, 0xaa, 0x1e, 0x2a,
	0x48, 0x11, 0x1c, 0x36, 0x68, 0x78, 0x86, 0x79, 0x30, 0xc3, 0xd6, 0xde, 0xbc, 0x3a, 0x2a,
	0x6d, 0x1e, 0x46, 0xdd, 0xe0, 0x80, 0x1e, 0x44, 0x3b, 0x6f, 0xaf, 0x31, 0xda, 0xa2, 0xbd,
	0x77, 0x06, 0x56, 0xc0, 0xb7, 0x92, 0x4b, 0x37, 0xc0, 0xfc, 0xc2, 0xd5, 0xfb, 0xa8, 0xda,
	0xf5, 0x57, 0xa8, 0x18, 0xc0, 0xdf, 0xe7, 0xaa, 0x2a, 0xe0, 0x7c, 0x6f, 0x77, 0xb1, 0x26,
	0xba, 0xf9, 0x2e, 0x1d, 0x16, 0xcb, 0xb8, 0xa2, 0x44, 0xd5, 0x2f, 0x1a, 0x79, 0x74, 0x87,
	0x4b, 0x00, 0xc9, 0x4a, 0x3a, 0x65, 0x8f, 0xe6, 0x5d, 0xe5, 0x0a, 0x77, 0xd8, 0x1a, 0x14,
	0x41, 0x75, 0xb1, 0xe2, 0x50, 0x2c, 0x93, 0x38, 0x2b, 0x6d, 0xf3, 0xf6, 0xdb, 0x1f, 0xcd,
	0xff, 0x14, 0x70, 0xe7, 0x16, 0xe8, 0x3d, 0xf0, 0xe3, 0xbc, 0x5e, 0xb6, 0x3f, 0xcc, 0x81,
	0x24, 0x67, 0xf3, 0x97, 0x3b, 0xfe, 0x3a, 0x96, 0x85, 0xdf, 0xe4, 0x6e, 0x3c, 0x85, 0x05,
	0x0e, 0xa3, 0x2b, 0x07, 0xc8, 0xbf, 0xe5, 0x13, 0x82, 0x62, 0x08, 0x61, 0x69, 0x4b, 0x47,
	0x62, 0x73, 0x44, 0x64, 0x8e, 0xe2, 0x91, 0xa6, 0x9a, 0xb7, 0xe9, 0x04, 0xb6, 0x54, 0x0c,
	0xc5, 0xa9, 0x47, 0xa6, 0xc9, 0x08, 0xfe, 0x4e, 0xa6, 0xcc, 0x8a, 0x5b, 0x90, 0x6f, 0x2b,
	0x3f, 0xb6, 0x0a, 0x96, 0xc0, 0x78, 0x58, 0x3c, 0x76, 0x6d, 0x94, 0x1a, 0xe4, 0x4e, 0xb8,
	0x38, 0xbb, 0xf5, 0xeb, 0x29, 0xd8, 0xb0, 0xf3, 0x15, 0x1e, 0x99, 0x96, 0x3c, 0x5d, 0x63,
	0xd5, 0xb1, 0xad, 0x52, 0xb8, 0x55, 0x70, 0x75, 0x3e, 0x1a, 0xd5, 0xda, 0xf6, 0x7a, 0x48,
	0x7d, 0x44, 0x41, 0xf9, 0x11, 0xce, 0xd7, 0xca, 0xa5, 0x3d, 0x7a, 0x79, 0x7e, 0x7d, 0x25,
	0x1b, 0x77, 0xbc, 0xf7, 0xc7, 0x0f, 0x84, 0x95, 0x10, 0x92, 0x67, 0x15, 0x11, 0x5a, 0x5e,
	0x41, 0x66, 0x0f, 0x38, 0x03, 0xb2, 0xf1, 0x5d, 0xf8, 0xab, 0xc0, 0x02, 0x76, 0x84, 0x28,
	0xf4, 0x9d, 0x56, 0x46, 0x60, 0x20, 0xdb, 0x68, 0xa7, 0xbb, 0xee, 0xac, 0x15, 0x01, 0x2f,
	0x20, 0x09, 0xdb, 0xc0, 0x16, 0xa1, 0x89, 0xf9, 0x94, 0x59, 0x00, 0xc1, 0x76, 0xbf, 0xc1,
	0x4d, 0x5d, 0x2d, 0xa9, 0x85, 0x2c, 0xd6, 0xd3, 0x14, 0xcc, 0x02, 0xc3, 0xc2, 0xfa, 0x6b,
	0xb7, 0xa6, 0xef, 0xdd, 0x12, 0x26, 0xa4, 0x63, 0xe3, 0x62, 0xbd, 0x56, 0x8a, 0x52, 0x2b,
	0xb9, 0xdf, 0x09, 0xbc, 0x0e, 0x97, 0xa9, 0xb0, 0x82, 0x46, 0x08, 0xd5, 0x1a, 0x8e, 0x1b,
	0xa7, 0x90, 0x98, 0xb9, 0xbb, 0x3c, 0x17, 0x9a, 0xf2, 0x82, 0xba, 0x64, 0x0a, 0x7f, 0xca,
	0x5a, 0x8c, 0x7c, 0xd3, 0x79, 0x09, 0x5b, 0x26, 0xbb, 0xbd, 0x25, 0xdf, 0x3d, 0x6f, 0x9a,
	0x8f, 0xee, 0x21, 0x66, 0xb0, 0x8d, 0x84, 0x4c, 0x91, 0x45, 0xd4, 0x77, 0x4f, 0xb3, 0x8c,
	0xbc, 0xa8, 0x99, 0xaa, 0x19, 0x53, 0x7c, 0x02, 0x87, 0xbb, 0x0b, 0x7c, 0x1a, 0x2d, 0xdf,
	0x48, 0x44, 0x06, 0xd6, 0x7d, 0x0c, 0x2d, 0x35, 0x76, 0xae, 0xc4, 0x5f, 0x71, 0x85, 0x97,
	0xc4, 0x3d, 0xef, 0x52, 0xbe, 0x00, 0xe4, 0xcd, 0x49, 0xd1, 0xd1, 0x1c, 0x3c, 0xd0, 0x1c,
	0x42, 0xaf, 0xd4, 0xbd, 0x58, 0x34, 0x07, 0x32, 0xee, 0xb9, 0xb5, 0xea, 0xff, 0xd7, 0x8c,
	0x0d, 0x2e, 0x2f, 0xaf, 0x87, 0xbb, 0xe6, 0x52, 0x71, 0x22, 0xf5, 0x25, 0x17, 0xa1, 0x82,
	0x04, 0xc2, 0x4a, 0xbd, 0x57, 0xc6, 0xab, 0xc8, 0x35, 0x0c, 0x3c, 0xd9, 0xc2, 0x43, 0xdb,
	0x27, 0x92, 0xcf, 0xb8, 0x25, 0x60, 0xfa, 0x21, 0x3b, 0x04, 0x52, 0xc8, 0x96, 0xba, 0x74,
	0xe3, 0x67, 0x3e, 0x8e, 0x8d, 0x61, 0x90, 0x92, 0x59, 0xb6, 0x1a, 0x1c, 0x5e, 0x21, 0xc1,
	0x65, 0xe5, 0xa6, 0x34, 0x05, 0x6f, 0xc5, 0x60, 0xb1, 0x83, 0xc1, 0xd5, 0xd5, 0xed, 0xd9,
	0xc7, 0x11, 0x7b, 0x49, 0x7a, 0xf9, 0xf9, 0x84, 0x47, 0x9b, 0xe2, 0xa5, 0x82, 0xe0, 0xc2,
	0x88, 0xd0, 0xb2, 0x58, 0x88, 0x7f, 0x45, 0x09, 0x67, 0x74, 0x61, 0xbf, 0xe6, 0x40, 0xe2,
	0x9d, 0xc2, 0x47, 0x05, 0x89, 0xed, 0xcb, 0xbb, 0xb7, 0x27, 0xe7, 0xdc, 0x7a, 0xfd, 0xbf,
	0xa8, 0xd0, 0xaa, 0x10, 0x39, 0x3c, 0x20, 0xf0, 0xd3, 0x6e, 0xb1, 0x72, 0xf8, 0xe6, 0x0f,
	0xef, 0x37, 0xe5, 0x09, 0x33, 0x5a, 0x83, 0x43, 0x80, 0x4f, 0x65, 0x2f, 0x7c, 0x8c, 0x6a,
	0xa0, 0x82, 0x0c, 0xd4, 0xd4, 0xfa, 0x81, 0x60, 0x3d, 0xdf, 0x06, 0xf1, 0x5f, 0x08, 0x0d,
	0x6d, 0x43, 0xf2, 0xe3, 0x11, 0x7d, 0x80, 0x32, 0xc5, 0xfb, 0xc5, 0xd9, 0x27, 0xec, 0xc6,
	0x4e, 0x65, 0x27, 0x76, 0x87, 0xa6, 0xee, 0xee, 0xd7, 0x8b, 0xd1, 0xa0, 0x5c, 0xb0, 0x42,
	0x13, 0x0e, 0x95, 0x4a, 0xf2, 0x06, 0xc6, 0x43, 0x33, 0xf4, 0xc7, 0xf8, 0xe7, 0x1f, 0xdd,
	0xe4, 0x46, 0x4a, 0x70, 0x39, 0x6c, 0xd0, 0xed, 0xca, 0xbe, 0x60, 0x3b, 0xd1, 0x7b, 0x57,
	0x48, 0xe5, 0x3a, 0x79, 0xc1, 0x69, 0x33, 0x53, 0x1b, 0x80, 0xb8, 0x91, 0x7d, 0xb4, 0xf6,
	0x17, 0x1a, 0x1d, 0x5a, 0x32, 0xd6, 0xcc, 0x71, 0x29, 0x3f, 0x28, 0xbb, 0xf3, 0x5e, 0x71,
	0xb8, 0x43, 0xaf, 0xf8, 0xb9, 0x64, 0xef, 0xc4, 0xa5, 0x6c, 0x08, 0x53, 0xc7, 0x00, 0x10,
	0x39, 0x4f, 0xdd, 0xe4, 0xb6, 0x19, 0x27, 0xfb, 0xb8, 0xf5, 0x32, 0x73, 0xe5, 0xcb, 0x32
};

// For proxy protecting
byte COM_BlockSequenceCRCByte(byte *base, int length, int sequence)
{
	byte chkb[64];

	if (sequence < 0)
		Sys_Error("%s: sequence < 0, this shouldn't happen\n", __func__);

	byte *p = chktbl + (sequence % (sizeof(chktbl) - 4));

	length = min(60, length);
	memcpy(chkb, base, length);

	chkb[length] = p[0];
	chkb[length + 1] = p[1];
	chkb[length + 2] = p[2];
	chkb[length + 3] = p[3];

	length += 4;

	unsigned short crc = CRC_Block(chkb, length);

	int x = 0;
	for (int n = 0; n < length; n++)
		x += chkb[n];

	crc = (crc ^ x) & 0xff;

	return crc;
}

//mxd. Moved from q_shared.c (unused by game.dll, xxhash.h greatly confuses extractfuncs.exe...)
uint Com_HashFileName(const char *fname)
{
	int i = 0;
	char tohash[MAX_QPATH];
	int len = 0;

	if (fname[0] == '/' || fname[0] == '\\')
		i++; // Skip leading slash

	while (fname[i] != '\0')
	{
		char letter = tolower(fname[i]);

		if (letter == '\\')
			letter = '/'; // Fix filepaths

		tohash[len++] = letter;
		i++;
	}

	tohash[len] = '\0';

	return XXH32(tohash, len, 42);
}

#pragma endregion

float frand(void)
{
	return (rand() & 32767) * (1.0 / 32767);
}

float crand(void)
{
	return (rand() & 32767) * (2.0 / 32767) - 1;
}

void Key_Init(void);
void SCR_EndLoadingPlaque(void);

// Throws a fatal error to test error shutdown procedures
void Com_Error_f(void)
{
	char *msg = (Cmd_Argc() == 2 ? Cmd_Argv(1) : "Something happened and everything was lost!");
	Com_Error(ERR_FATAL, "%s", msg);
}

void Qcommon_Init(int argc, char **argv)
{
	char *s;

	if (setjmp(abortframe))
		Sys_Error("Error during initialization");

	z_chain.next = z_chain.prev = &z_chain;

	// Prepare enough of the subsystems to handle cvar and command buffer management
	COM_InitArgv(argc, argv);

	Cbuf_Init();

	Cmd_Init();
	Cvar_Init();

	Key_Init();

	// We need to add the early commands twice, because a basedir needs to be set before execing
	// config files, but we want other parms to override the settings of the config files
	Cbuf_AddEarlyCommands(false);
	Cbuf_Execute();

	FS_InitFilesystem();

	Cbuf_AddText("exec default.cfg\n");
	Cbuf_AddText("exec kmq2config.cfg\n");

	Cbuf_AddEarlyCommands(true);
	Cbuf_Execute();

	// Init commands and vars
	Cmd_AddCommand("z_stats", Z_Stats_f);
	Cmd_AddCommand("error", Com_Error_f);

	host_speeds = Cvar_Get("host_speeds", "0", 0);
	log_stats = Cvar_Get("log_stats", "0", 0);
	developer = Cvar_Get("developer", "0", 0);
	timescale = Cvar_Get("timescale", "1", CVAR_CHEAT);
	fixedtime = Cvar_Get("fixedtime", "0", CVAR_CHEAT);
	logfile_active = Cvar_Get("logfile", "0", 0);
	showtrace = Cvar_Get("showtrace", "0", 0);
#ifdef DEDICATED_ONLY
	dedicated = Cvar_Get("dedicated", "1", CVAR_NOSET);
#else
	dedicated = Cvar_Get("dedicated", "0", CVAR_NOSET);
#endif

	// Knightmare- for the game DLL to tell what engine it's running under
	sv_engine = Cvar_Get("sv_engine", ENGINE_NAME, CVAR_SERVERINFO | CVAR_NOSET | CVAR_LATCH); //mxd. "KMQuake2" -> ENGINE_NAME
	sv_engine_version = Cvar_Get("sv_engine_version", va("%4.2f", VERSION), CVAR_SERVERINFO | CVAR_NOSET | CVAR_LATCH);
	// end Knightmare
	
	s = va("%4.2f %s %s %s", VERSION, CPUSTRING, __DATE__, BUILDSTRING);
	Cvar_Get("version", s, CVAR_SERVERINFO | CVAR_NOSET);

	if (dedicated->integer)
		Cmd_AddCommand("quit", Com_Quit);

	Sys_Init();

	NET_Init();
	Netchan_Init();

	SV_Init();
	CL_Init();

#ifdef _WIN32
	if (!dedicated->integer) // Hide console
		Sys_ShowConsole(false);
#endif

	// add + commands from command line
	if (!Cbuf_AddLateCommands())
	{
		// If the user didn't give any commands, run default action
		if (!dedicated->value)
			Cbuf_AddText("d1\n");
		else
			Cbuf_AddText("dedicated_start\n");

		Cbuf_Execute();
	}
	else
	{
		// The user asked for something explicit, so drop the loading plaque
		SCR_EndLoadingPlaque();
	}

	Com_Printf("=== %s %s v%4.2f Initialized ===\n\n", ENGINE_NAME, CPUSTRING, VERSION);

	//mxd. Free after logfile_active cvar is initialized and at least one Com_Printf call is made...
	FreeEarlyMessages();
}

extern int c_traces;
extern int c_brush_traces;
extern int c_pointcontents;

void Qcommon_Frame(int msec)
{
	char *s;

	if (setjmp(abortframe))
		return; // an ERR_DROP was thrown

	if (log_stats->modified)
	{
		log_stats->modified = false;

		if (log_stats->integer)
		{
			if (log_stats_file)
			{
				fclose(log_stats_file);
				log_stats_file = 0;
			}

			log_stats_file = fopen("stats.log", "w");
			if (log_stats_file)
				fprintf(log_stats_file, "entities,dlights,parts,frame time\n");
		}
		else
		{
			if (log_stats_file)
			{
				fclose(log_stats_file);
				log_stats_file = 0;
			}
		}
	}

	if (fixedtime->integer)
	{
		msec = fixedtime->integer;
	}
	else if (timescale->value)
	{
		msec *= timescale->value;
		msec = max(msec, 1);
	}

	if (showtrace->integer)
	{
		Com_Printf("%4i traces, %4i brush traces, %4i points\n", c_traces, c_brush_traces, c_pointcontents);

		c_traces = 0;
		c_brush_traces = 0;
		c_pointcontents = 0;
	}

	do
	{
		s = Sys_ConsoleInput();
		if (s)
			Cbuf_AddText(va("%s\n", s));
	} while (s);

	Cbuf_Execute();

	// Print performance stats?
	if (host_speeds->integer)
	{
		const int time_before = Sys_Milliseconds();

		SV_Frame(msec);

		const int time_between = Sys_Milliseconds();

		CL_Frame(msec);
		
		const int time_after = Sys_Milliseconds();
		
		const int all = time_after - time_before;
		int sv = time_between - time_before;
		int cl = time_after - time_between;
		const int gm = time_after_game - time_before_game;
		const int rf = time_after_ref - time_before_ref;
		sv -= gm;
		cl -= rf;

		Com_Printf("all:%3i server:%3i game:%3i client:%3i refresh:%3i\n", all, sv, gm, cl, rf);
	}
	else // Just run the game logic
	{
		SV_Frame(msec);
		CL_Frame(msec);
	}
}

void Qcommon_Shutdown(void)
{	
	FS_Shutdown();
}

// String parsing function from r1q2
void StripHighBits(char *string, int highbits)
{
	char *p = string;
	const byte high = (highbits ? 127 : 255);

	while (string[0])
	{
		const byte c = *string++;

		if (c >= 32 && c <= high)
			*p++ = c;
	}

	p[0] = '\0';
}

// Security function from r1q2
qboolean IsValidChar(int c)
{
	if (!isalnum(c) && c != '_' && c != '-')
		return false;
	return true;
}

// String parsing function from r1q2
void ExpandNewLines(char *string)
{
	char *q = string;
	char *s = q;

	if (!string[0])
		return;

	while (*(q + 1))
	{
		if (*q == '\\' && *(q+1) == 'n')
		{
			*s++ = '\n';
			q++;
		}
		else
		{
			*s++ = *q;
		}
		q++;

		//crashfix, check if we reached eol on an expansion.
		if (!*q)
			break;
	}

	if (*q)
		*s++ = *q;
	*s = '\0';
}

// String parsing function from r1q2
char *StripQuotes(char *string)
{
	if (!string[0])
		return string;

	const size_t len = strlen(string);

	if (string[0] == '"' && string[len - 1] == '"')
	{
		string[len - 1] = 0;
		return string + 1;
	}

	return string;
}

// String parsing function from r1q2
const char *MakePrintable(const void *subject, size_t numchars)
{
	static char printable[4096];
	char tmp[8];

	if (!subject)
	{
		Q_strncpyz(printable, "(null)", sizeof(printable));
		return printable;
	}

	const byte *s = (const byte *)subject;
	char *p = printable;
	int len = 0;

	if (!numchars)
		numchars = strlen((const char *)s);

	while (numchars--)
	{
		if (isprint(s[0]))
		{
			*p++ = s[0];
			len++;
		}
		else
		{
			sprintf(tmp, "%.3d", s[0]);
			*p++ = '\\';
			*p++ = tmp[0];
			*p++ = tmp[1];
			*p++ = tmp[2];
			len += 4;
		}

		if (len >= sizeof(printable) - 5)
			break;

		s++;
	}

	printable[len] = 0;
	return printable;
}