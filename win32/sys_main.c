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

// sys_main.c

#include "../qcommon/qcommon.h"
#include <SDL2/SDL_main.h>

#ifdef _WIN32
	#include "winquake.h"
#endif

int main(int argc, char **argv)
{
#ifdef _WIN32
	// Setup DPI awareness
	Sys_SetHighDPIMode();

	// Init console window
	Sys_InitDedConsole();
#endif

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
			Sys_Sleep(1); //mxd. Was Sleep(1);

		// DarkOne's CPU usage fix
		while (true)
		{
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
			if (time > 0) break;
			Sys_Sleep(0); //mxd. Was Sleep(0); // may also use Sleep(1); to free more CPU, but it can lower your fps
		}

		Qcommon_Frame(time);
		oldtime = newtime;
	}
}