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

// Main windowed and fullscreen graphics interface module.
// This module is used for both the software and OpenGL rendering versions of the Quake refresh engine.

#include "..\client\client.h"

// Console variables that we need to access from this module
cvar_t *vid_gamma;
cvar_t *vid_ref;	// Name of Refresh DLL loaded
cvar_t *vid_xpos;	// X coordinate of window position
cvar_t *vid_ypos;	// Y coordinate of window position
cvar_t *vid_fullscreen;
cvar_t *r_customwidth;
cvar_t *r_customheight;

// Global variables used internally by this module
viddef_t viddef; // Global video state; used by other modules
static qboolean kmgl_active = false;

// Application state
qboolean activeapp; //mxd. int -> qboolean
qboolean minimized;

#pragma region ======================= DLL GLUE

void VID_Printf(int print_level, char *fmt, ...)
{
	va_list	argptr;
	static char msg[MAXPRINTMSG]; //mxd. +static
	
	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	if (print_level == PRINT_ALL)
	{
		Com_Printf("%s", msg);
	}
	else if (print_level == PRINT_DEVELOPER)
	{
		Com_DPrintf("%s", msg);
	}
	else if (print_level == PRINT_ALERT)
	{
		MessageBox(0, msg, "PRINT_ALERT", MB_ICONWARNING);
		OutputDebugString(msg);
	}
}

void VID_Error(int err_level, char *fmt, ...)
{
	va_list	argptr;
	static char msg[MAXPRINTMSG]; //mxd. +static
	
	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_Error(err_level, "%s", msg);
}

qboolean VID_AppActivate(qboolean active, qboolean minimize)
{
	minimized = minimize;

	Key_ClearStates();

	// We don't want to act like we're active if we're minimized
	activeapp = (active && !minimized);

	// Minimize/restore mouse-capture on demand
	IN_Activate(activeapp);

	return activeapp;
}

// Console command to restart the video mode and refresh DLL. We do this
// simply by setting the modified flag for the vid_ref variable, which will
// cause the entire video mode and refresh DLL to be reset on the next frame.
void VID_Restart_f(void)
{
	vid_ref->modified = true;
}

void VID_Front_f(void)
{
	GLimp_AppActivate(); //mxd
}

#pragma endregion

#pragma region ======================= Video mode info / switching

typedef struct vidmode_s
{
	const char *description;
	int width, height;
	int mode;
} vidmode_t;

qboolean VID_GetModeInfo(int *width, int *height, int mode)
{
	// Knightmare- added 1280x1024, 1400x1050, 856x480, 1024x480 modes
	static vidmode_t vid_modes[] =
	{
		#include "../qcommon/vid_modes.h"
	};
	static int num_vid_modes = (int)(sizeof(vid_modes) / sizeof(vid_modes[0])); //mxd
	
	if (mode == -1) // Custom mode
	{
		*width  = r_customwidth->integer;
		*height = r_customheight->integer;
		return true;
	}

	if (mode < 0 || mode >= num_vid_modes)
		return false;

	*width  = vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return true;
}

void VID_NewWindow(int width, int height)
{
	viddef.width  = width;
	viddef.height = height;

	cl.force_refdef = true; // Can't use a paused refdef
}

static void UpdateVideoRef()
{
	qboolean reclip_decals = false;
	qboolean vid_reloading = false; // Knightmare- flag to not unnecessarily drop console
	char reason[128];

	if (vid_ref->modified)
	{
		cl.force_refdef = true; // Can't use a paused refdef
		S_StopAllSounds();

		// Unclip decals
		reclip_decals = CL_UnclipDecals();
	}

	while (vid_ref->modified)
	{
		// Refresh has changed
		vid_ref->modified = false;
		vid_fullscreen->modified = true;
		cl.refresh_prepped = false;
		cls.disable_screen = (cl.cinematictime == 0); // Knightmare added
		vid_reloading = true;
		// end Knightmare

		// Compacted code from VID_LoadRefresh
		if (kmgl_active)
		{
			R_Shutdown();
			kmgl_active = false;
		}

		Com_Printf("\n------ Renderer Initialization ------\n");

		if (!R_Init(reason))
		{
			R_Shutdown();
			kmgl_active = false;
			Com_Error(ERR_FATAL, "Couldn't initialize OpenGL renderer!\n%s", reason);
		}

		Com_Printf( "------------------------------------\n");

		kmgl_active = true;
	}

	// Added to close loading screen
	if (cl.refresh_prepped && vid_reloading)
		cls.disable_screen = false;

	// Re-clip decals
	if (cl.refresh_prepped && reclip_decals)
		CL_ReclipDecals();
}

// This function gets called once just before drawing each frame, and it's sole purpose in life
// is to check to see if any of the video mode parameters have changed, and if they have to 
// update the rendering DLL and/or video mode to match.
void VID_CheckChanges(void)
{
	// Update changed vid_ref
	UpdateVideoRef();

	// Update our window position
	if (vid_xpos->modified || vid_ypos->modified)
	{
		if (!vid_fullscreen->integer)
			GLimp_SetWindowPosition(vid_xpos->integer, vid_ypos->integer); //mxd

		vid_xpos->modified = false;
		vid_ypos->modified = false;
	}
}

#pragma endregion

#pragma region ======================= Init / shutdown

void VID_Init(void)
{
	// Create the video variables so we know how to start the graphics drivers
	vid_ref = Cvar_Get("vid_ref", "gl", CVAR_ARCHIVE);
	vid_xpos = Cvar_Get("vid_xpos", "centered", CVAR_ARCHIVE); //mxd. Was 3
	vid_ypos = Cvar_Get("vid_ypos", "centered", CVAR_ARCHIVE); //mxd. Was 22
	vid_fullscreen = Cvar_Get("vid_fullscreen", "1", CVAR_ARCHIVE);
	vid_gamma = Cvar_Get("vid_gamma", "0.8", CVAR_ARCHIVE); // was 1.0
	r_customwidth = Cvar_Get("r_customwidth", "1600", CVAR_ARCHIVE);
	r_customheight = Cvar_Get("r_customheight", "1024", CVAR_ARCHIVE);

	// Force vid_ref to gl. Older versions of Lazarus code check only vid_ref = gl for fadein effects
	Cvar_Set("vid_ref", "gl");

	// Add some console commands that we want to handle
	Cmd_AddCommand("vid_restart", VID_Restart_f);
	Cmd_AddCommand("vid_front", VID_Front_f);

	// Initializes the video backend. This is NOT the renderer itself, just the client side support stuff!
	if (!GLimp_Init())
		Com_Error(ERR_FATAL, "Couldn't initialize the graphics subsystem!\n");

	// Start the graphics mode and load refresh DLL
	VID_CheckChanges();
}

void VID_Shutdown(void)
{
	if (kmgl_active)
	{
		R_Shutdown();
		kmgl_active = false;
	}
}

#pragma endregion