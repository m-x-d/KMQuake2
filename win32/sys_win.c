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
// sys_win.h

#include "../qcommon/qcommon.h"
#include "winquake.h"
#include "resource.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include "../win32/conproc.h"

#define MINIMUM_WIN_MEMORY	0x0a00000
#define MAXIMUM_WIN_MEMORY	0x1000000

int			starttime;
qboolean	ActiveApp; //mxd. int -> qboolean
qboolean	Minimized;

static HANDLE		hinput, houtput;

unsigned	sys_msg_time;
unsigned	sys_frame_time;


static HANDLE		qwclsemaphore;

#define	MAX_NUM_ARGVS	128
int			argc;
char		*argv[MAX_NUM_ARGVS];


#ifndef NEW_DED_CONSOLE
/*
===============================================================================

DEDICATED CONSOLE

===============================================================================
*/
static char	console_text[256];
static int	console_textlen;

/*
================
Sys_InitConsole
================
*/
void Sys_InitConsole (void)
{
	if (!dedicated->value)
		return;

	if (!AllocConsole ())
		Sys_Error ("Couldn't create dedicated server console");
	hinput = GetStdHandle (STD_INPUT_HANDLE);
	houtput = GetStdHandle (STD_OUTPUT_HANDLE);
	
	// let QHOST hook in
	InitConProc (argc, argv);
}


/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
	INPUT_RECORD	recs[1024];
	int		dummy;
	int		ch, numread, numevents;

	if (!dedicated || !dedicated->value)
		return NULL;

	for ( ;; )
	{
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);	

						if (console_textlen)
						{
							console_text[console_textlen] = 0;
							console_textlen = 0;
							return console_text;
						}
						break;

					case '\b':
						if (console_textlen)
						{
							console_textlen--;
							WriteFile(houtput, "\b \b", 3, &dummy, NULL);	
						}
						break;

					default:
						if (ch >= ' ')
						{
							if (console_textlen < sizeof(console_text)-2)
							{
								WriteFile(houtput, &ch, 1, &dummy, NULL);	
								console_text[console_textlen] = ch;
								console_textlen++;
							}
						}
						break;
				}
			}
		}
	}
	return NULL;
}


/*
================
Sys_ConsoleOutput

Print text to the dedicated console
================
*/
void Sys_ConsoleOutput (char *string)
{
	int		dummy;
	char	text[256];

	if (!dedicated || !dedicated->value)
		return;

	if (console_textlen)
	{
		text[0] = '\r';
		memset(&text[1], ' ', console_textlen);
		text[console_textlen+1] = '\r';
		text[console_textlen+2] = 0;
		WriteFile(houtput, text, console_textlen+2, &dummy, NULL);
	}

	WriteFile(houtput, string, strlen(string), &dummy, NULL);

	if (console_textlen)
		WriteFile(houtput, console_text, console_textlen, &dummy, NULL);
}

//================================================================
#endif // NEW_DED_CONSOLE

/*
================
Sys_Sleep
================
*/
void Sys_Sleep (int msec)
{
	Sleep(msec);
}

/*
================
Sys_TickCount
================
*/
unsigned Sys_TickCount (void)
{
	return GetTickCount();
}

/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents (void)
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage(&msg, NULL, 0, 0))
			Sys_Quit();

		sys_msg_time = msg.time;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// grab frame time 
	sys_frame_time = timeGetTime();	// FIXME: should this be at start?
}


/*
================
Sys_GetClipboardData

================
*/
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


/*
===============================================================================

SYSTEM IO

===============================================================================
*/

#ifndef NEW_DED_CONSOLE
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	CL_Shutdown ();
	Qcommon_Shutdown ();

	va_start (argptr, error);
//	vsprintf (text, error, argptr);
	Q_vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	MessageBox(NULL, text, "Error", 0 /* MB_OK */ );

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);

// shut down QHOST hooks if necessary
	DeinitConProc ();

	exit (1);
}
#endif // NEW_DED_CONSOLE


void Sys_Quit (void)
{
	timeEndPeriod(1);

	CL_Shutdown();
	Qcommon_Shutdown();
	CloseHandle(qwclsemaphore);
	if (dedicated && dedicated->value)
		FreeConsole();

// shut down QHOST hooks if necessary
	DeinitConProc();

#ifdef NEW_DED_CONSOLE
	Sys_ShutdownConsole();
#endif

	exit(0);
}


void WinError (void)
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
}

//================================================================

/*
================
Sys_ScanForCD

================
*/
char *Sys_ScanForCD (void)
{
	static char	cddir[MAX_OSPATH];
	static qboolean	done;
	char		drive[4];
	char		test[MAX_QPATH];
	qboolean	missionpack = false; // Knightmare added

	if (done)		// don't re-check
		return cddir;

	// no abort/retry/fail errors
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

			break; // game parameter only appears once in command line
		}
	}

	// scan the drives
	for (drive[0] = 'c'; drive[0] <= 'z'; drive[0]++)
	{
		// where activision put the stuff...
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

//================================================================

/*
=================
Sys_DetectCPU
l33t CPU detection
Borrowed from Q2E
=================
*/
static qboolean Sys_DetectCPU (char *cpuString, int maxSize)
{
#if defined _M_IX86

	char				vendor[16];
	int					stdBits, features, moreFeatures, extFeatures;
	unsigned __int64	start, end, counter, stop, frequency;
	unsigned			speed;
	qboolean			hasMMX, hasMMXExt, has3DNow, has3DNowExt, hasSSE, hasSSE2, hasSSE3;

	// Check if CPUID instruction is supported
	__try {
		__asm {
			mov eax, 0
			cpuid
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER){
		return false;
	}

	// Get CPU info
	__asm {
		; // Get vendor identifier
		mov eax, 0
		cpuid
		mov dword ptr[vendor+0], ebx
		mov dword ptr[vendor+4], edx
		mov dword ptr[vendor+8], ecx
		mov dword ptr[vendor+12], 0

		; // Get standard bits and features
		mov eax, 1
		cpuid
		mov stdBits, eax
		mov moreFeatures, ecx ; // Knightmare added
		mov features, edx

		; // Check if extended functions are present
		mov extFeatures, 0
		mov eax, 80000000h
		cpuid
		cmp eax, 80000000h
		jbe NoExtFunction

		; // Get extended features
		mov eax, 80000001h
		cpuid
		mov extFeatures, edx

NoExtFunction:
	}

	// Get CPU name
	if (!Q_stricmp(vendor, "AuthenticAMD"))
		strncpy(cpuString, "AMD", maxSize);
	else if (!Q_stricmp(vendor, "GenuineIntel"))
		strncpy(cpuString, "Intel", maxSize);
	else
		strncpy(cpuString, vendor, maxSize);

	// Check if RDTSC instruction is supported
	if ((features >> 4) & 1)
	{
		// Measure CPU speed
		QueryPerformanceFrequency((LARGE_INTEGER *)&frequency);

		__asm {
			rdtsc
			mov dword ptr[start+0], eax
			mov dword ptr[start+4], edx
		}

		QueryPerformanceCounter((LARGE_INTEGER *)&stop);
		stop += frequency;

		do
		{
			QueryPerformanceCounter((LARGE_INTEGER *)&counter);
		} while (counter < stop);

		__asm {
			rdtsc
			mov dword ptr[end+0], eax
			mov dword ptr[end+4], edx
		}

		speed = (unsigned)((end - start) / 1000000);

		Q_strncatz(cpuString, va(" @ %u MHz", speed), maxSize);
	}

	//mxd. Get number of cores
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	Q_strncatz(cpuString, va(" (%i cores)", sysInfo.dwNumberOfProcessors), maxSize);

	// Get extended instruction sets supported
	hasMMX = (features >> 23) & 1;
	hasMMXExt = (extFeatures >> 22) & 1;
	has3DNow = (extFeatures >> 31) & 1;
	has3DNowExt = (extFeatures >> 30) & 1;
	hasSSE = (features >> 25) & 1;
	hasSSE2 = (features >> 26) & 1;
	hasSSE3 = (moreFeatures >> 0) & 1;

	if (hasMMX || has3DNow || hasSSE)
	{
		Q_strncatz(cpuString, " with", maxSize);

		if (hasMMX)
		{
			Q_strncatz(cpuString, " MMX", maxSize);

			if (hasMMXExt)
				Q_strncatz(cpuString, "+", maxSize);
		}

		if (has3DNow)
		{
			if(hasMMX) 
				Q_strncatz(cpuString, ",", maxSize); //mxd

			Q_strncatz(cpuString, " 3DNow!", maxSize);

			if (has3DNowExt)
				Q_strncatz(cpuString, "+", maxSize);
		}

		if (hasSSE)
		{
			if (hasMMX || has3DNow)
				Q_strncatz(cpuString, ",", maxSize); //mxd

			Q_strncatz(cpuString, " SSE", maxSize);

			if (hasSSE3)
				Q_strncatz(cpuString, "3", maxSize);
			else if (hasSSE2)
				Q_strncatz(cpuString, "2", maxSize);
		}
	}

	return true;

#else
	Q_strncpyz(cpuString, "Alpha AXP", maxSize);
	return true;
#endif
}

/*
================
GetOsName (mxd. Adapted from Quake2xp)
================
*/

qboolean Is64BitWindows()
{
	BOOL f64 = FALSE;
	return IsWow64Process(GetCurrentProcess(), &f64) && f64;
}

qboolean GetOsVersion(RTL_OSVERSIONINFOEXW* pk_OsVer)
{
	typedef LONG(WINAPI* tRtlGetVersion)(RTL_OSVERSIONINFOEXW*);

	memset(pk_OsVer, 0, sizeof(RTL_OSVERSIONINFOEXW));
	pk_OsVer->dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);

	const HMODULE h_NtDll = GetModuleHandleW(L"ntdll.dll");
	tRtlGetVersion f_RtlGetVersion = (tRtlGetVersion)GetProcAddress(h_NtDll, "RtlGetVersion");

	if (!f_RtlGetVersion)
		return FALSE; // This will never happen (all processes load ntdll.dll)

	const LONG status = f_RtlGetVersion(pk_OsVer);
	return status == 0; // STATUS_SUCCESS;
}

qboolean GetOsName(char* result)
{
	RTL_OSVERSIONINFOEXW rtl_OsVer;
	
	if (GetOsVersion(&rtl_OsVer))
	{
		char *osname = "Windows"; //mxd
		char *numbits = Is64BitWindows() ? "x64" : "x32"; //mxd
		const qboolean workstation = (rtl_OsVer.wProductType == VER_NT_WORKSTATION); //mxd

		if (rtl_OsVer.dwMajorVersion == 5) // Windows 2000, Windows XP
		{
			switch (rtl_OsVer.dwMinorVersion)
			{
				case 0: osname = "Windows 2000"; break;
				case 1: osname = "Windows XP"; break;
				case 2: osname = (workstation ? "Windows XP x64" : "Windows Server 2003"); break;
			}
		}
		else if (rtl_OsVer.dwMajorVersion == 6) // Windows 7, Windows 8
		{
			switch (rtl_OsVer.dwMinorVersion)
			{
				case 1: osname = (workstation ? "Windows 7" : "Windows Server 2008 R2"); break;
				case 2: osname = (workstation ? "Windows 8" : "Windows Server 2012"); break;
				case 3: osname = (workstation ? "Windows 8.1" : "Windows Server 2012"); break;
			}
		}
		else if (rtl_OsVer.dwMajorVersion == 10) // Windows 10
		{
			switch (rtl_OsVer.dwMinorVersion)
			{
				case 0: osname = (workstation ? "Windows 10" : "Windows Server 2016"); break;
			}
		}

		sprintf(result, "%s %s %ls, build %d", osname, numbits, rtl_OsVer.szCSDVersion, rtl_OsVer.dwBuildNumber);
		return true;
	}
	
	return false;
}

/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
	timeBeginPeriod(1);

	// Detect OS
	char string[64]; // Knightmare added
	if(!GetOsName(string)) //mxd
		Sys_Error("Unsupported operating system");

	Com_Printf("OS:  %s\n", string);
	Cvar_Get("sys_osVersion", string, CVAR_NOSET|CVAR_LATCH|CVAR_SAVE_IGNORE);

	// Detect CPU
	if (Sys_DetectCPU(string, sizeof(string)))
	{
		Com_Printf("CPU: %s\n", string);
		Cvar_Get("sys_cpuString", string, CVAR_NOSET|CVAR_LATCH|CVAR_SAVE_IGNORE);
	}
	else
	{
		Com_Printf("CPU: Unknown\n");
		Cvar_Get("sys_cpuString", "Unknown", CVAR_NOSET|CVAR_LATCH|CVAR_SAVE_IGNORE);
	}

	// Get physical memory
	MEMORYSTATUSEX memStatus; // Knightmare added //mxd. GlobalMemoryStatus -> GlobalMemoryStatusEx
	memStatus.dwLength = sizeof(memStatus);
	GlobalMemoryStatusEx(&memStatus);
	sprintf(string, "%i", (int)(memStatus.ullTotalPhys >> 20)); //mxd. Uh oh! We'll be in trouble once average ram size exceeds 2 147 483 647 MB!
	Com_Printf("RAM: %s MB\n", string);
	Cvar_Get("sys_ramMegs", string, CVAR_NOSET|CVAR_LATCH|CVAR_SAVE_IGNORE); //mxd. Never used for anything other than printing to the console

#ifndef NEW_DED_CONSOLE
	Sys_InitConsole (); // show dedicated console, moved to function
#endif
}


/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/

/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate (void)
{
	ShowWindow(cl_hwnd, SW_RESTORE);
	SetForegroundWindow(cl_hwnd );
}

/*
========================================================================

GAME DLL

========================================================================
*/

static HINSTANCE	game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	if (!FreeLibrary(game_library))
		Com_Error(ERR_FATAL, "FreeLibrary failed for game library");
	game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetGameAPI (void *parms)
{
	void	*(*GetGameAPI) (void *);
	char	name[MAX_OSPATH];
	char	*path;
	char	cwd[MAX_OSPATH];

#if defined _M_IX86
	//Knightmare- changed DLL name for better cohabitation
	const char *gamename = "kmq2gamex86.dll"; 

#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif

#elif defined _M_ALPHA
	const char *gamename = "kmq2gameaxp.dll";

#ifdef NDEBUG
	const char *debugdir = "releaseaxp";
#else
	const char *debugdir = "debugaxp";
#endif

#endif

	if (game_library)
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	// check the current debug directory first for development purposes
	_getcwd (cwd, sizeof(cwd));
	Com_sprintf(name, sizeof(name), "%s/%s/%s", cwd, debugdir, gamename);
	game_library = LoadLibrary ( name );
	if (game_library)
	{
		Com_DPrintf ("LoadLibrary (%s)\n", name);
	}
	else
	{
#ifdef DEBUG
		// check the current directory for other development purposes
		Com_sprintf(name, sizeof(name), "%s/%s", cwd, gamename);
		game_library = LoadLibrary( name );
		if (game_library)
		{
			Com_DPrintf("LoadLibrary (%s)\n", name);
		}
		else
#endif
		{
			// now run through the search paths
			path = NULL;
			while (true)
			{
				path = FS_NextPath(path);
				if (!path)
					return NULL; // couldn't find one anywhere

				Com_sprintf(name, sizeof(name), "%s/%s", path, gamename);
				game_library = LoadLibrary(name);
				if (game_library)
				{
					Com_DPrintf("LoadLibrary (%s)\n",name);
					break;
				}
			}
		}
	}

	GetGameAPI = (void *)GetProcAddress(game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame ();		
		return NULL;
	}

	return GetGameAPI(parms);
}

//=======================================================================


/*
==================
ParseCommandLine

==================
*/
void ParseCommandLine (LPSTR lpCmdLine)
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

/*
==================
WinMain

==================
*/
HINSTANCE	global_hInstance;
HWND		hwnd_dialog; // Knightmare added

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG				msg;
	int				time, oldtime, newtime;
	char			*cddir;
	qboolean		cdscan = false; // Knightmare added

	/* previous instances do not exist in Win32 */
	if (hPrevInstance)
		return 0;

	global_hInstance = hInstance;

	ParseCommandLine(lpCmdLine);

#ifndef NEW_DED_CONSOLE
	// Knightmare- startup logo, code from TomazQuake
	//if (!(dedicated && dedicated->value))
	{
		hwnd_dialog = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, NULL);
		RECT			rect; // Knightmare added

		if (hwnd_dialog)
		{
			if (GetWindowRect (hwnd_dialog, &rect))
			{
				if (rect.left > (rect.top * 2))
				{
					SetWindowPos (hwnd_dialog, 0, (rect.left/2) - ((rect.right - rect.left)/2), rect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
				}
			}

			ShowWindow (hwnd_dialog, SW_SHOWDEFAULT);
			UpdateWindow (hwnd_dialog);
			SetForegroundWindow (hwnd_dialog);
		}
	}
	// end Knightmare
#endif

#ifdef NEW_DED_CONSOLE // init debug console
	Sys_InitDedConsole ();
	Com_Printf("KMQuake 2 SBE %4.2f %s %s %s\n", VERSION, CPUSTRING, BUILDSTRING, __DATE__); //mxd. Version
#endif

	// Knightmare- scan for cd command line option
	for (int i = 0; i < argc; i++)
	{
		if (!strcmp(argv[i], "scanforcd"))
		{
			cdscan = true;
			break;
		}
	}

	// if we find the CD, add a +set cddir xxx command line
	if (cdscan)
	{
		cddir = Sys_ScanForCD();
		if (cddir && argc < MAX_NUM_ARGVS - 3)
		{
			int i;

			// don't override a cddir on the command line
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
	oldtime = Sys_Milliseconds();

	/* main window message loop */
	while (true)
	{
		// if at a full screen console, don't update unless needed
		if (Minimized || (dedicated && dedicated->value) )
			Sleep(1);

		while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage (&msg, NULL, 0, 0))
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

		_controlfp( _PC_24, _MCW_PC );
		Qcommon_Frame(time);

		oldtime = newtime;
	}
}
