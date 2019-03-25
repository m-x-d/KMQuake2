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

#pragma warning( disable : 4229 )  // mgraph gets this

#include <windows.h>
#include <dsound.h>
#include <stdint.h> //mxd

//#define	WINDOW_STYLE	(WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE)
#define	WINDOW_STYLE	(WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_VISIBLE)

extern	HINSTANCE	global_hInstance;

extern LPDIRECTSOUND pDS;
extern LPDIRECTSOUNDBUFFER pDSBuf;

extern DWORD gSndBufSize;

extern HWND			cl_hwnd;
extern qboolean		ActiveApp, Minimized;

//void IN_Activate (qboolean active); //mxd. Redundant declaration
void IN_MouseEvent (int mstate);

extern int		window_center_x, window_center_y;
extern RECT		window_rect;

extern HWND		hwnd_dialog; // Knightmare added

#define NEW_DED_CONSOLE // enable new dedicated console //TODO: get rid of this, use only the new console?

#ifdef NEW_DED_CONSOLE
void Sys_ShowConsole (qboolean show);
void Sys_ShutdownConsole (void);
void Sys_InitDedConsole (void);
#endif // NEW_DED_CONSOLE

//mxd. Borrowed from GZDoom
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