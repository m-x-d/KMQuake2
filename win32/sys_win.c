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
#include <intrin.h> //mxd. For __cpuid

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
			Sys_Quit();

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

void Sys_Quit(void)
{
	timeEndPeriod(1);

	CL_Shutdown();
	Qcommon_Shutdown();

	if (dedicated && dedicated->value)
		FreeConsole();

	Sys_ShutdownConsole();

	exit(0);
}


void WinError(void)
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

#pragma region ======================= CPU detection

// Adapted from GZDoom (https://github.com/coelckers/gzdoom/blob/2ae8d394418519b6c40bc117e08342039c77577a/src/x86.cpp#L74)

#define MAKE_ID(a, b, c, d)	((uint32_t)((a)|((b)<<8)|((c)<<16)|((d)<<24)))

CPUInfo CheckCPUID()
{
	int foo[4];

	CPUInfo cpu;
	memset(&cpu, 0, sizeof(cpu));
	cpu.DataL1LineSize = 32;	// Assume a 32-byte cache line

	// Get vendor ID
	__cpuid(foo, 0);
	cpu.dwVendorID[0] = foo[1];
	cpu.dwVendorID[1] = foo[3];
	cpu.dwVendorID[2] = foo[2];

	if (foo[1] == MAKE_ID('A', 'u', 't', 'h') &&
		foo[3] == MAKE_ID('e', 'n', 't', 'i') &&
		foo[2] == MAKE_ID('c', 'A', 'M', 'D'))
	{
		cpu.bIsAMD = true;
	}

	// Get features flags and other info
	__cpuid(foo, 1);
	cpu.FeatureFlags[0] = foo[1];	// Store brand index and other stuff
	cpu.FeatureFlags[1] = foo[2];	// Store extended feature flags
	cpu.FeatureFlags[2] = foo[3];	// Store feature flags

	cpu.HyperThreading = (foo[3] & (1 << 28)) > 0;

	// If CLFLUSH instruction is supported, get the real cache line size.
	if (foo[3] & (1 << 19))
		cpu.DataL1LineSize = (foo[1] & 0xFF00) >> (8 - 3);

	cpu.Stepping = foo[0] & 0x0F;
	cpu.Type = (foo[0] & 0x3000) >> 12;	// valid on Intel only
	cpu.Model = (foo[0] & 0xF0) >> 4;
	cpu.Family = (foo[0] & 0xF00) >> 8;

	if (cpu.Family == 15)
		cpu.Family += (foo[0] >> 20) & 0xFF; // Add extended family.

	if (cpu.Family == 6 || cpu.Family == 15)
		cpu.Model |= (foo[0] >> 12) & 0xF0; // Add extended model ID.

	// Check for extended functions.
	__cpuid(foo, 0x80000000);
	const unsigned int maxext = (unsigned int)foo[0];

	if (maxext >= 0x80000004)
	{ 
		// Get processor brand string.
		__cpuid((int *)&cpu.dwCPUString[0], 0x80000002);
		__cpuid((int *)&cpu.dwCPUString[4], 0x80000003);
		__cpuid((int *)&cpu.dwCPUString[8], 0x80000004);
	}

	if (cpu.bIsAMD)
	{
		if (maxext >= 0x80000005)
		{ 
			// Get data L1 cache info.
			__cpuid(foo, 0x80000005);
			cpu.AMD_DataL1Info = foo[2];
		}

		if (maxext >= 0x80000001)
		{ 
			// Get AMD-specific feature flags.
			__cpuid(foo, 0x80000001);
			cpu.AMDStepping = foo[0] & 0x0F;
			cpu.AMDModel = (foo[0] & 0xF0) >> 4;
			cpu.AMDFamily = (foo[0] & 0xF00) >> 8;

			if (cpu.AMDFamily == 15)
			{ 
				// Add extended model and family.
				cpu.AMDFamily += (foo[0] >> 20) & 0xFF;
				cpu.AMDModel |= (foo[0] >> 12) & 0xF0;
			}
			cpu.FeatureFlags[3] = foo[3];	// AMD feature flags
		}
	}

	return cpu;
}

static void Sys_DetectCPU(char *cpuString, int maxSize)
{
#if defined _M_IX86

	unsigned __int64	start, end, counter, stop, frequency;
	unsigned			speed;

	CPUInfo cpu = CheckCPUID();

	// Get CPU name
	char cpustring[4 * 4 * 3 + 1];

	// Why does Intel right-justify this string (on P4s) or add extra spaces (on Cores)?
	const char *f = cpu.CPUString;
	char *t;

	// Skip extra whitespace at the beginning.
	while (*f == ' ')
		++f;

	// Copy string to temp buffer, but condense consecutive spaces to a single space character.
	for (t = cpustring; *f != '\0'; ++f)
	{
		if (*f == ' ' && *(f - 1) == ' ')
			continue;
		*t++ = *f;
	}
	*t = '\0';

	// Store CPU name
	strncpy(cpuString, (cpustring[0] ? cpustring : "Unknown"), maxSize);

	// Check if RDTSC instruction is supported
	if ((cpu.FeatureFlags[0] >> 4) & 1)
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
	Q_strncatz(cpuString, va(" (%i logical cores)", sysInfo.dwNumberOfProcessors), maxSize);

	// Get extended instruction sets supported //mxd. We don't use any of these, so why bother?
	/*if (cpu.b3DNow || cpu.bSSE || cpu.bMMX || cpu.HyperThreading)
	{
		Q_strncatz(cpuString, " with", maxSize);
		qboolean first = true;

		if (cpu.bMMX)
		{
			Q_strncatz(cpuString, " MMX", maxSize);
			
			if (cpu.bMMXPlus)
				Q_strncatz(cpuString, "+", maxSize);

			first = false;
		}

		if (cpu.bSSE)
		{
			if (!first)
				Q_strncatz(cpuString, ",", maxSize);
			
			Q_strncatz(cpuString, " SSE", maxSize);
			first = false;
		}

		if (cpu.bSSE2)
		{
			if (!first)
				Q_strncatz(cpuString, ",", maxSize);
			
			Q_strncatz(cpuString, " SSE2", maxSize);
			first = false;
		}

		if (cpu.bSSE3)
		{
			if (!first) 
				Q_strncatz(cpuString, ",", maxSize);

			Q_strncatz(cpuString, " SSE3", maxSize);
			first = false;
		}

		if (cpu.bSSSE3)
		{
			if (!first)
				Q_strncatz(cpuString, ",", maxSize);

			Q_strncatz(cpuString, " SSSE3", maxSize);
			first = false;
		}

		if (cpu.bSSE41)
		{
			if (!first)
				Q_strncatz(cpuString, ",", maxSize);

			Q_strncatz(cpuString, " SSE4.1", maxSize);
			first = false;
		}

		if (cpu.bSSE42)
		{
			if (!first)
				Q_strncatz(cpuString, ",", maxSize);

			Q_strncatz(cpuString, " SSE4.2", maxSize);
			first = false;
		}

		if (cpu.b3DNow)
		{
			if (!first)
				Q_strncatz(cpuString, ",", maxSize);

			Q_strncatz(cpuString, " 3DNow!", maxSize);
			
			if (cpu.b3DNowPlus)
				Q_strncatz(cpuString, "+", maxSize);

			first = false;
		}

		if (cpu.HyperThreading)
		{
			if (!first)
				Q_strncatz(cpuString, ",", maxSize);

			Q_strncatz(cpuString, " HyperThreading", maxSize);
		}
	}*/

#else
	Q_strncpyz(cpuString, "Unknown", maxSize);
#endif
}

#pragma endregion 

#pragma region ======================= OS detection

// mxd. Adapted from Quake2xp

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
		char *osname = "(unknown version)"; //mxd
		char *numbits = Is64BitWindows() ? "x64" : "x32"; //mxd
		const qboolean workstation = (rtl_OsVer.wProductType == VER_NT_WORKSTATION); //mxd

		if (rtl_OsVer.dwMajorVersion == 5) // Windows 2000, Windows XP
		{
			switch (rtl_OsVer.dwMinorVersion)
			{
				case 0: osname = "2000"; break;
				case 1: osname = "XP"; break;
				case 2: osname = (workstation ? "XP" : "Server 2003"); break;
			}
		}
		else if (rtl_OsVer.dwMajorVersion == 6) // Windows 7, Windows 8
		{
			switch (rtl_OsVer.dwMinorVersion)
			{
				case 1: osname = (workstation ? "7" : "Server 2008 R2"); break;
				case 2: osname = (workstation ? "8" : "Server 2012"); break;
				case 3: osname = (workstation ? "8.1" : "Server 2012 R2"); break;
				case 4: osname = (workstation ? "10 (beta)" : "Server 2016 (beta)"); break;
			}
		}
		else if (rtl_OsVer.dwMajorVersion == 10) // Windows 10
		{
			switch (rtl_OsVer.dwMinorVersion)
			{
				case 0: osname = (workstation ? "10" : "Server 2016"); break;
			}
		}

		sprintf(result, "Windows %s %s %ls, build %d", osname, numbits, rtl_OsVer.szCSDVersion, rtl_OsVer.dwBuildNumber);
		return true;
	}
	
	return false;
}

#pragma endregion


void Sys_Init(void)
{
	timeBeginPeriod(1);

	// Detect OS
	char string[256]; // Knightmare added
	if(!GetOsName(string)) //mxd
		Sys_Error("Unsupported operating system");

	Com_Printf("OS:  %s\n", string);
	Cvar_Get("sys_osVersion", string, CVAR_NOSET|CVAR_LATCH|CVAR_SAVE_IGNORE);

	// Detect CPU
	Sys_DetectCPU(string, sizeof(string));
	Com_Printf("CPU: %s\n", string);
	Cvar_Get("sys_cpuString", string, CVAR_NOSET | CVAR_LATCH | CVAR_SAVE_IGNORE);

	// Get physical memory
	MEMORYSTATUSEX memStatus; // Knightmare added //mxd. GlobalMemoryStatus -> GlobalMemoryStatusEx
	memStatus.dwLength = sizeof(memStatus);
	GlobalMemoryStatusEx(&memStatus);
	sprintf(string, "%i", (int)(memStatus.ullTotalPhys >> 20)); //mxd. Uh oh! We'll be in trouble once average ram size exceeds 2 147 483 647 MB!
	Com_Printf("RAM: %s MB\n", string);
	Cvar_Get("sys_ramMegs", string, CVAR_NOSET|CVAR_LATCH|CVAR_SAVE_IGNORE); //mxd. Never used for anything other than printing to the console
}

void Sys_AppActivate(void)
{
	ShowWindow(cl_hwnd, SW_RESTORE);
	SetForegroundWindow(cl_hwnd );
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

	// Init console window
	Sys_InitDedConsole();
	Com_Printf("KMQuake 2 SBE %4.2f %s %s %s\n", VERSION, CPUSTRING, BUILDSTRING, __DATE__); //mxd. Version

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