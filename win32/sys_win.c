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
#include <stdio.h>
#include <direct.h>

qboolean ActiveApp; //mxd. int -> qboolean
qboolean Minimized;

HINSTANCE global_hInstance;

unsigned sys_msg_time;
unsigned sys_frame_time;

#define	MAX_NUM_ARGVS 128
int argc;
char *argv[MAX_NUM_ARGVS];


void Sys_Sleep(int msec)
{
	Sleep(msec);
}

unsigned Sys_TickCount(void)
{
	return GetTickCount();
}

// Send Key_Event calls
void Sys_SendKeyEvents(void)
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage(&msg, NULL, 0, 0))
			Sys_Quit(false);

		sys_msg_time = msg.time;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Grab frame time 
	sys_frame_time = timeGetTime();	// FIXME: should this be at start?
}

char *Sys_GetClipboardData(void)
{
	char *data = NULL;

	if (OpenClipboard(NULL) != 0)
	{
		HANDLE hClipboardData = GetClipboardData(CF_TEXT);
		if (hClipboardData)
		{
			char *cliptext = GlobalLock(hClipboardData);
			if (cliptext)
			{
				data = malloc(GlobalSize(hClipboardData) + 1);
				Q_strncpyz(data, cliptext, GlobalSize(hClipboardData) + 1);
				GlobalUnlock(hClipboardData);
			}
		}

		CloseClipboard();
	}

	return data;
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

	if (dedicated && dedicated->value)
		FreeConsole();

	Sys_ShutdownConsole();
	Com_CloseLogfile(); //mxd. Make it the last thing closed

	exit(0);
}

/*void WinError(void) //mxd. Never called
{
	LPVOID lpMsgBuf;

	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);

	// Display the string.
	MessageBox(NULL, lpMsgBuf, "GetLastError", MB_OK | MB_ICONINFORMATION);

	// Free the buffer.
	LocalFree(lpMsgBuf);
}*/

#pragma endregion


char *Sys_ScanForCD(void)
{
	static char	cddir[MAX_OSPATH];
	static qboolean	done;
	char		drive[4];
	char		test[MAX_QPATH];
	qboolean	missionpack = false; // Knightmare added

	if (done) // Don't re-check
		return cddir;

	// No abort/retry/fail errors
	SetErrorMode (SEM_FAILCRITICALERRORS);

	drive[0] = 'c';
	drive[1] = ':';
	drive[2] = '\\';
	drive[3] = 0;

	Com_Printf("\nScanning for game CD data path...");

	done = true;

	// Knightmare- check if mission pack gamedir is set
	for (int i = 0; i < argc; i++)
	{
		if (!strcmp(argv[i], "game") && (i + 1 < argc))
		{
			if (!strcmp(argv[i + 1], "rogue") || !strcmp(argv[i + 1], "xatrix"))
				missionpack = true;

			break; // Game parameter only appears once in command line
		}
	}

	// Scan the drives
	for (drive[0] = 'c'; drive[0] <= 'z'; drive[0]++)
	{
		// Where activision put the stuff...
		if (missionpack) // Knightmare- mission packs have cinematics in different path
		{
			sprintf(cddir, "%sdata\\max", drive);
			sprintf(test, "%sdata\\patch\\quake2.exe", drive);
		}
		else
		{
			sprintf(cddir, "%sinstall\\data", drive);
			sprintf(test, "%sinstall\\data\\quake2.exe", drive);
		}

		FILE *f = fopen(test, "r");
		if (f)
		{
			fclose (f);
			if (GetDriveType(drive) == DRIVE_CDROM)
			{
				Com_Printf(" found %s\n", cddir);
				return cddir;
			}
		}
	}

	Com_Printf(" could not find %s on any CDROM drive!\n", test);

	cddir[0] = 0;
	
	return NULL;
}

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

void Sys_AppActivate(void)
{
	ShowWindow(cl_hwnd, SW_RESTORE);
	SetForegroundWindow(cl_hwnd);
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
			return NULL; // couldn't find one anywhere

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

void ParseCommandLine(LPSTR lpCmdLine)
{
	argc = 1;
	argv[0] = "exe";

	while (*lpCmdLine && argc < MAX_NUM_ARGVS)
	{
		while (*lpCmdLine && (*lpCmdLine <= 32 || *lpCmdLine > 126))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && (*lpCmdLine > 32 && *lpCmdLine <= 126))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG			msg;
	int			time, newtime;
	qboolean	cdscan = false; // Knightmare added

	global_hInstance = hInstance;

	ParseCommandLine(lpCmdLine);

	// Setup DPI awareness
	Sys_SetHighDPIMode();

	// Init console window
	Sys_InitDedConsole();
	Com_Printf("%s %4.2f %s %s %s\n", ENGINE_NAME, VERSION, CPUSTRING, BUILDSTRING, __DATE__); //mxd. ENGINE_NAME, Version

	// Knightmare- scan for cd command line option
	for (int i = 0; i < argc; i++)
	{
		if (!strcmp(argv[i], "scanforcd"))
		{
			cdscan = true;
			break;
		}
	}

	// If we find the CD, add a +set cddir xxx command line
	if (cdscan)
	{
		char* cddir = Sys_ScanForCD();
		if (cddir && argc < MAX_NUM_ARGVS - 3)
		{
			int i;

			// Don't override a cddir on the command line
			for (i = 0; i < argc ; i++)
				if (!strcmp(argv[i], "cddir"))
					break;

			if (i == argc)
			{
				argv[argc++] = "+set";
				argv[argc++] = "cddir";
				argv[argc++] = cddir;
			}
		}
	}

	Qcommon_Init(argc, argv);
	int oldtime = Sys_Milliseconds();

	// Main window message loop
	while (true)
	{
		// If at a full screen console, don't update unless needed
		if (Minimized || (dedicated && dedicated->integer))
			Sleep(1);

		while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage(&msg, NULL, 0, 0))
				Com_Quit();

			sys_msg_time = msg.time;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// DarkOne's CPU usage fix
		while (true)
		{
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
			if (time > 0) break;
			Sleep(0); // may also use Speep(1); to free more CPU, but it can lower your fps
		}

		Qcommon_Frame(time);
		oldtime = newtime;
	}
}