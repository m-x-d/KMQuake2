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
#include <SDL2/SDL_main.h>

void Sys_Sleep(int msec)
{
	SDL_Delay(msec); //mxd. Was Sleep(msec);
}

unsigned Sys_TickCount(void)
{
	return SDL_GetTicks(); //mxd. Was GetTickCount();
}

#pragma region ======================= SYSTEM IO

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

#pragma endregion

void Sys_Init(void)
{
	timeBeginPeriod(1);

	Com_Printf("\n------------ System Info ------------\n"); //mxd

	// Detect OS
	char string[256]; // Knightmare added
	if(!Sys_GetOsName(string)) //mxd
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

#pragma region ======================= GAME DLL

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

#pragma region ======================= Set High-DPI mode

//mxd. Adapted from Yamagi Quake2
typedef enum PROCESS_DPI_AWARENESS
{
	DPI_UNAWARE,
	SYSTEM_DPI_AWARE,
	PER_MONITOR_DPI_AWARE
} PROCESS_DPI_AWARENESS;

static void Sys_SetHighDPIMode(void)
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

int main(int argc, char **argv)
{
	// Setup DPI awareness
	Sys_SetHighDPIMode();

	// Init console window
	Sys_InitDedConsole();
	Com_Printf("%s %4.2f %s %s %s\n", ENGINE_NAME, VERSION, CPUSTRING, BUILDSTRING, __DATE__); //mxd. ENGINE_NAME, Version

	// Init engine subsystems
	Qcommon_Init(argc, argv);
	int oldtime = Sys_Milliseconds();

	// Main window message loop
	int time, newtime;
	while (true)
	{
		// If at a full screen console, don't update unless needed
		if (minimized || (dedicated && dedicated->integer))
			SDL_Delay(1); //mxd. Was Sleep(1);

		// DarkOne's CPU usage fix
		while (true)
		{
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
			if (time > 0) break;
			SDL_Delay(0); //mxd. Was Sleep(0); // may also use Sleep(1); to free more CPU, but it can lower your fps
		}

		Qcommon_Frame(time);
		oldtime = newtime;
	}
}