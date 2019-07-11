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

// sys_win.c

#include "../qcommon/qcommon.h"
#include "winquake.h"
#include "sdlquake.h" //mxd
#include <stdio.h>
#include <direct.h>
#include <io.h>

#pragma region ======================= Timing

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

void Sys_Sleep(int msec)
{
	SDL_Delay(msec); //mxd. Was Sleep(msec);
}

unsigned Sys_TickCount(void)
{
	return SDL_GetTicks(); //mxd. Was GetTickCount();
}

#pragma endregion

#pragma region ======================= High-precision timers

#define NUM_TIMERS 16

static double PCFreq[NUM_TIMERS];
static __int64 CounterStart[NUM_TIMERS];

void Sys_TimerStart(int timerindex)
{
	if (timerindex < 0 || timerindex >= NUM_TIMERS)
		Sys_Error("%s: invalid timerindex %i, expected a value in [0, %i] range!\n", __func__, timerindex, NUM_TIMERS - 1);

	LARGE_INTEGER li;
	if (!QueryPerformanceFrequency(&li))
		Com_CPrintf("%s: QueryPerformanceFrequency failed!\n", __func__);

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

#pragma endregion

#pragma region ======================= Directory access

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

//mxd. Just a wrapper around _access (https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/access-waccess)
qboolean Sys_Access(char *path, enum accessmode_t mode)
{
	return (_access(path, mode) != -1);
}

#pragma endregion

#pragma region ======================= Filesystem searching

static char findpath[MAX_OSPATH];
static char findbase[MAX_OSPATH];
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
		Sys_Error("Sys_FindFirst called without calling Sys_FindClose!");

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

#pragma endregion

#pragma region ======================= Init / shutdown

void Sys_Quit(qboolean error) //mxd. +error
{
	timeEndPeriod(1);

	if(!error) //mxd. Otherwise those were already called
	{
		CL_Shutdown();
		Qcommon_Shutdown();
	}

	if (dedicated && dedicated->integer)
		FreeConsole();

	Sys_ShutdownConsole();
	Com_CloseLogfile(); //mxd. Make it the last thing closed

	exit(0);
}

void Sys_Init(void)
{
	timeBeginPeriod(1);

	Com_Printf("\n------------ System Info ------------\n"); //mxd

	// Detect OS
	char string[256]; // Knightmare added
	if (!Sys_GetOsName(string)) //mxd
		Sys_Error("Unsupported operating system");

	Com_Printf("OS:  %s\n", string);

	// Detect CPU
	Sys_GetCpuName(string, sizeof(string));
	Com_Printf("CPU: %s\n", string);

	// Get physical memory
	MEMORYSTATUSEX memStatus; // Knightmare added //mxd. GlobalMemoryStatus -> GlobalMemoryStatusEx
	memStatus.dwLength = sizeof(memStatus);
	GlobalMemoryStatusEx(&memStatus);
	sprintf(string, "%i", (int)(memStatus.ullTotalPhys >> 20)); //mxd. Uh oh! We'll be in trouble once average ram size exceeds 2 147 483 647 MB!
	Com_Printf("RAM: %s MB\n", string);

	Com_Printf("-------------------------------------\n\n"); //mxd
}

#pragma endregion

#pragma region ======================= Game dll load / unload

static HINSTANCE game_library;

void Sys_UnloadGame(void)
{
	if (!FreeLibrary(game_library))
		Com_Error(ERR_FATAL, "FreeLibrary failed for game library");

	game_library = NULL;
}

// Loads the game dll
void *Sys_GetGameAPI(void *parms)
{
	if (game_library)
		Com_Error(ERR_FATAL, "Sys_GetGameAPI called without calling Sys_UnloadingGame first");

	// Run through the search paths
	char* path = NULL;
	char name[MAX_OSPATH];
	while (true)
	{
		path = FS_NextPath(path);
		if (!path)
			return NULL; // Couldn't find one anywhere

		Com_sprintf(name, sizeof(name), "%s/%s", path, "kmq2gamex86.dll");
		game_library = LoadLibrary(name);
		if (game_library)
		{
			Com_DPrintf("LoadLibrary (%s)\n", name);
			break;
		}
	}

	// Get game API
	void*(*GetGameAPI)(void*) = (void *)GetProcAddress(game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame();
		return NULL;
	}

	return GetGameAPI(parms);
}

#pragma endregion

#pragma region ======================= High-DPI support

//mxd. Adapted from Yamagi Quake2
typedef enum PROCESS_DPI_AWARENESS
{
	DPI_UNAWARE,
	SYSTEM_DPI_AWARE,
	PER_MONITOR_DPI_AWARE
} PROCESS_DPI_AWARENESS;

void Sys_SetHighDPIMode(void)
{
	// Win8.1 and later
	HINSTANCE shcoreDLL = LoadLibrary("SHCORE.DLL");
	if (shcoreDLL)
	{
		HRESULT (WINAPI *SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS da) = (HRESULT(WINAPI *)(PROCESS_DPI_AWARENESS))GetProcAddress(shcoreDLL, "SetProcessDpiAwareness");
		if (SetProcessDpiAwareness)
		{
			SetProcessDpiAwareness(PER_MONITOR_DPI_AWARE);
			return;
		}
	}

	// Vista, Win7 and Win8
	HINSTANCE userDLL = LoadLibrary("USER32.DLL");
	if (userDLL)
	{
		BOOL (WINAPI *SetProcessDPIAware)(void) = (BOOL(WINAPI *)(void))GetProcAddress(userDLL, "SetProcessDPIAware");
		if (SetProcessDPIAware)
		{
			SetProcessDPIAware();
			return;
		}
	}

	// Unknown OSes of the future!
	Com_Printf(S_COLOR_YELLOW"Failed to set High-DPI awareness mode!");
}

#pragma endregion