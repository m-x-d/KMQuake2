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

#include "../qcommon/qcommon.h"
#include "winquake.h"
#include <direct.h>
#include <io.h>


int curtime;

int Sys_Milliseconds(void)
{
	static int base;
	static qboolean	initialized = false;

	if (!initialized)
	{
		// Let base retain 16 bits of effectively random data
		base = timeGetTime() & 0xffff0000;
		initialized = true;
	}
	curtime = timeGetTime() - base;

	return curtime;
}

void Sys_Mkdir(char *path)
{
	_mkdir(path);
}

// From Q2E
void Sys_Rmdir(char *path)
{
	_rmdir(path);
}

// From Q2E
char *Sys_GetCurrentDirectory(void)
{
	static char	dir[MAX_OSPATH];

	if (!_getcwd(dir, sizeof(dir)))
		Sys_Error("Couldn't get current working directory");

	return dir;
}

//============================================

static char findbase[MAX_OSPATH];
static char findpath[MAX_OSPATH];
static int findhandle;

static qboolean CompareAttributes(unsigned found, unsigned musthave, unsigned canthave)
{
	if ((found & _A_RDONLY) && (canthave & SFF_RDONLY))
		return false;
	if ((found & _A_HIDDEN) && (canthave & SFF_HIDDEN))
		return false;
	if ((found & _A_SYSTEM) && (canthave & SFF_SYSTEM))
		return false;
	if ((found & _A_SUBDIR) && (canthave & SFF_SUBDIR))
		return false;
	if ((found & _A_ARCH) && (canthave & SFF_ARCH))
		return false;

	if ((musthave & SFF_RDONLY) && !(found & _A_RDONLY))
		return false;
	if ((musthave & SFF_HIDDEN) && !(found & _A_HIDDEN))
		return false;
	if ((musthave & SFF_SYSTEM) && !(found & _A_SYSTEM))
		return false;
	if ((musthave & SFF_SUBDIR) && !(found & _A_SUBDIR))
		return false;
	if ((musthave & SFF_ARCH) && !(found & _A_ARCH))
		return false;

	return true;
}

char *Sys_FindFirst(char *path, unsigned musthave, unsigned canthave)
{
	struct _finddata_t findinfo;

	if (findhandle)
		Sys_Error("Sys_BeginFind without close");
	findhandle = 0;

	COM_FilePath(path, findbase);
	findhandle = _findfirst(path, &findinfo);

	// Knightmare- AnthonyJ's player menu bug fix (not loading dirs when loose files are present in baseq2/players/)
	while (findhandle != -1)
	{
		if (CompareAttributes(findinfo.attrib, musthave, canthave))
		{
			Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
			return findpath;
		}

		if (_findnext(findhandle, &findinfo) == -1)
		{
			_findclose(findhandle);
			findhandle = -1;
		}
	}

	return NULL;
}

char *Sys_FindNext(unsigned musthave, unsigned canthave)
{
	struct _finddata_t findinfo;

	if (findhandle == -1)
		return NULL;

	// Knightmare- AnthonyJ's player menu bug fix (not loading dirs when loose files are present in baseq2/players/)
	while (_findnext(findhandle, &findinfo) != -1)
	{
		if (CompareAttributes(findinfo.attrib, musthave, canthave))
		{
			Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
			return findpath;
		}
	}
	
	return NULL;
}

void Sys_FindClose(void)
{
	if (findhandle != -1)
		_findclose(findhandle);

	findhandle = 0;
}

/*
=================
mxd. High-precision timers
=================
*/
#define NUM_TIMERS 16

static double PCFreq[NUM_TIMERS];
static __int64 CounterStart[NUM_TIMERS];

void Sys_TimerStart(int timerindex)
{
	if(timerindex < 0 || timerindex >= NUM_TIMERS)
		Sys_Error("Sys_TimerStart: invalid timerindex %i, expected a value in [0, %i] range!\n", timerindex, NUM_TIMERS - 1);
	
	LARGE_INTEGER li;
	if (!QueryPerformanceFrequency(&li))
		Com_CPrintf("QueryPerformanceFrequency failed!\n");

	PCFreq[timerindex] = (double)li.QuadPart / 1000.0; // in ms.

	QueryPerformanceCounter(&li);
	CounterStart[timerindex] = li.QuadPart;
}

double Sys_TimerGetElapsed(int timerindex)
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return (double)(li.QuadPart - CounterStart[timerindex]) / PCFreq[timerindex];
}