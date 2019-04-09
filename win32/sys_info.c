
// sys_info.c - CPU and OS name detection (mxd).

#include "../qcommon/qcommon.h"
#include <stdio.h>
#include <intrin.h>
#include <stdint.h>
#include <Windows.h>

#pragma region ======================= CPU detection

// Adapted from GZDoom (https://github.com/coelckers/gzdoom/blob/2ae8d394418519b6c40bc117e08342039c77577a/src/x86.cpp#L74)

typedef struct // 92 bytes
{
	union
	{
		char VendorID[16];
		uint32_t dwVendorID[4];
	};
	union
	{
		char CPUString[48];
		uint32_t dwCPUString[12];
	};

	uint8_t Stepping;
	uint8_t Model;
	uint8_t Family;
	uint8_t Type;
	uint8_t HyperThreading;

	union
	{
		struct
		{
			uint8_t BrandIndex;
			uint8_t CLFlush;
			uint8_t CPUCount;
			uint8_t APICID;

			uint32_t bSSE3 : 1;
			uint32_t DontCare1 : 8;
			uint32_t bSSSE3 : 1;
			uint32_t DontCare1a : 9;
			uint32_t bSSE41 : 1;
			uint32_t bSSE42 : 1;
			uint32_t DontCare2a : 11;

			uint32_t bFPU : 1;
			uint32_t bVME : 1;
			uint32_t bDE : 1;
			uint32_t bPSE : 1;
			uint32_t bRDTSC : 1;
			uint32_t bMSR : 1;
			uint32_t bPAE : 1;
			uint32_t bMCE : 1;
			uint32_t bCX8 : 1;
			uint32_t bAPIC : 1;
			uint32_t bReserved1 : 1;
			uint32_t bSEP : 1;
			uint32_t bMTRR : 1;
			uint32_t bPGE : 1;
			uint32_t bMCA : 1;
			uint32_t bCMOV : 1;
			uint32_t bPAT : 1;
			uint32_t bPSE36 : 1;
			uint32_t bPSN : 1;
			uint32_t bCFLUSH : 1;
			uint32_t bReserved2 : 1;
			uint32_t bDS : 1;
			uint32_t bACPI : 1;
			uint32_t bMMX : 1;
			uint32_t bFXSR : 1;
			uint32_t bSSE : 1;
			uint32_t bSSE2 : 1;
			uint32_t bSS : 1;
			uint32_t bHTT : 1;
			uint32_t bTM : 1;
			uint32_t bReserved3 : 1;
			uint32_t bPBE : 1;

			uint32_t DontCare2 : 22;
			uint32_t bMMXPlus : 1;		// AMD's MMX extensions
			uint32_t bMMXAgain : 1;		// Just a copy of bMMX above
			uint32_t DontCare3 : 6;
			uint32_t b3DNowPlus : 1;
			uint32_t b3DNow : 1;
		};
		uint32_t FeatureFlags[4];
	};

	uint8_t AMDStepping;
	uint8_t AMDModel;
	uint8_t AMDFamily;
	uint8_t bIsAMD;

	union
	{
		struct
		{
			uint8_t DataL1LineSize;
			uint8_t DataL1LinesPerTag;
			uint8_t DataL1Associativity;
			uint8_t DataL1SizeKB;
		};
		uint32_t AMD_DataL1Info;
	};
} CPUInfo;

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

void Sys_GetCpuName(char *result, int resultsize)
{
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
	strncpy(result, (cpustring[0] ? cpustring : "Unknown"), resultsize);

	// Check if RDTSC instruction is supported
	if ((cpu.FeatureFlags[0] >> 4) & 1)
	{
		// Measure CPU speed
		QueryPerformanceFrequency((LARGE_INTEGER *)&frequency);

		__asm {
			rdtsc
			mov dword ptr[start + 0], eax
			mov dword ptr[start + 4], edx
		}

		QueryPerformanceCounter((LARGE_INTEGER *)&stop);
		stop += frequency;

		do
		{
			QueryPerformanceCounter((LARGE_INTEGER *)&counter);
		} while (counter < stop);

		__asm {
			rdtsc
			mov dword ptr[end + 0], eax
			mov dword ptr[end + 4], edx
		}

		speed = (unsigned)((end - start) / 1000000);

		Q_strncatz(result, va(" @ %u MHz", speed), resultsize);
	}

	//mxd. Get number of cores
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	Q_strncatz(result, va(" (%i logical cores)", sysInfo.dwNumberOfProcessors), resultsize);

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

qboolean Sys_GetOsName(char* result)
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