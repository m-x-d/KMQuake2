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
// winquake.h: Win32-specific Quake header file

#pragma once

#include <Windows.h>

#define MAXPRINTMSG		8192 // was 4096, fix for nVidia 191.xx crash //mxd. Moved from common.c / sys_console.c

extern HINSTANCE global_hInstance;

extern HWND cl_hwnd;
extern qboolean ActiveApp;
extern qboolean Minimized;

void IN_MouseEvent(int mstate);

// Console window
void Sys_ShowConsole(qboolean show);
void Sys_ShutdownConsole(void);
void Sys_InitDedConsole(void);

//mxd. CPU and OS name detection
qboolean Sys_GetOsName(char* result);
void Sys_GetCpuName(char *cpuString, int maxSize);