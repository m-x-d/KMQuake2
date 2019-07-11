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

/*
This is the client side of the render backend, implemented trough SDL.
The SDL window and related functrion (mouse grap, fullscreen switch)
are implemented here, everything else is in the r_sdl2.c.
 */

#include <SDL2/SDL_video.h>
#include "../renderer/r_local.h"
#include "../win32/sdlquake.h" //mxd

#define WINDOW_TITLE		ENGINE_NAME
#define WINDOW_TITLE_MP1	ENGINE_NAME" - The Reckoning"
#define WINDOW_TITLE_MP2	ENGINE_NAME" - Ground Zero"

extern cvar_t *vid_ref;

SDL_Window *window = NULL;
static qboolean initSuccessful = false;

// (Un)grab Input
void GLimp_GrabInput(qboolean grab)
{
	const SDL_bool dograb = (grab ? SDL_TRUE : SDL_FALSE);
	
	if (window != NULL)
		SDL_SetWindowGrab(window, dograb);

	if (SDL_SetRelativeMouseMode(dograb) < 0)
		Com_Printf(S_COLOR_YELLOW"WARNING: setting relative mouse mode failed: %s\n", SDL_GetError());
}

// Shuts the SDL render backend down
static void ShutdownGraphics(void)
{
	if (window)
	{
		// Cleanly ungrab input (needs window)
		GLimp_GrabInput(false);
		SDL_DestroyWindow(window);

		window = NULL;
	}

	initSuccessful = false; // Not initialized anymore
}

extern qboolean FS_ModType(char *name);

static qboolean CreateSDLWindow(int flags, int x, int y, int w, int h)
{
	char *title;
	if (FS_ModType("xatrix")) // q2mp1
		title = WINDOW_TITLE_MP1;
	else if (FS_ModType("rogue")) // q2mp2
		title = WINDOW_TITLE_MP2;
	else
		title = WINDOW_TITLE;
	
	window = SDL_CreateWindow(title, x, y, w, h, flags);

	return window != NULL;
}

static qboolean GLimp_GetWindowSize(int *w, int *h)
{
	if (window == NULL || w == NULL || h == NULL)
		return false;

	SDL_DisplayMode m;

	if (SDL_GetWindowDisplayMode(window, &m) != 0)
	{
		VID_Printf(PRINT_ALL, "Can't get display mode: %s\n", SDL_GetError());
		return false;
	}

	*w = m.w;
	*h = m.h;

	return true;
}

qboolean GLimp_GetWindowPosition(int *x, int *y) //mxd
{
	if (window == NULL)
		return false;
	
	SDL_GetWindowPosition(window, x, y);
	return true;
}

qboolean GLimp_SetWindowPosition(const int x, const int y) //mxd
{
	if (window == NULL)
		return false;
	
	SDL_SetWindowPosition(window, x, y);
	return true;
}

void GLimp_AppActivate() //mxd
{
	SDL_ShowWindow(window);
	SDL_RaiseWindow(window);
}

//mxd. Process SDL_WINDOWEVENT
void GLimp_WindowEvent(SDL_Event *event)
{
	switch (event->window.event)
	{
		case SDL_WINDOWEVENT_FOCUS_LOST:
		case SDL_WINDOWEVENT_FOCUS_GAINED:
		{	
			const Uint32 flags = SDL_GetWindowFlags(window);
			const qboolean appactive = (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED);
			const qboolean isminimized = (flags & SDL_WINDOW_MINIMIZED);

			if (VID_AppActivate(appactive, isminimized)) // Let the input know...
				GLimp_AppActivate();
		} break;

		case SDL_WINDOWEVENT_MOVED:
			if (window != NULL)
			{
				int x, y;
				SDL_GetWindowPosition(window, &x, &y);
				Cvar_SetValue("vid_xpos", x);
				Cvar_SetValue("vid_ypos", y);
				vid_xpos->modified = false;
				vid_ypos->modified = false;
			}
			break;

		case SDL_WINDOWEVENT_SIZE_CHANGED:
			//TODO: mxd. Update custom window size
			break;
	}
}

static int GetFullscreenType()
{
	if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP)
		return 1;

	if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)
		return 2;

	return 0;
}

// (Re)initializes the actual window
qboolean GLimp_InitGraphics(int fullscreen, int *pwidth, int *pheight)
{
	uint flags = SDL_WINDOW_OPENGL | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

	// Only do this if we already have a working window and a fully	initialized rendering backend
	// GLimp_InitGraphics() is also	called when recovering if creating GL context fails or the one we got is unusable.
	int curWidth, curHeight;
	if (initSuccessful && GLimp_GetWindowSize(&curWidth, &curHeight) && (curWidth == *pwidth) && (curHeight == *pheight))
	{
		// If we want fullscreen, but aren't
		if (GetFullscreenType())
		{
			SDL_SetWindowFullscreen(window, flags);
			Cvar_SetValue("vid_fullscreen", fullscreen);
		}

		// Are we now?
		if (GetFullscreenType())
			return true;
	}

	// Is the surface used?
	if (window)
	{
		R_ShutdownContext();
		ShutdownGraphics();

		window = NULL;
	}

	// We need the window size for the menu, the HUD, etc.
	VID_NewWindow(*pwidth, *pheight);

	// Reset SDL
	SDL_GL_ResetAttributes();

	// Let renderer prepare things (set OpenGL attributes).
	if (!R_PrepareForWindow())
		return false; // It's PrepareForWindow() job to log an error

	//mxd. Check window position cvars
	const int wx = (!Q_stricmp(vid_xpos->string, "centered") ? SDL_WINDOWPOS_CENTERED : vid_xpos->integer);
	const int wy = (!Q_stricmp(vid_ypos->string, "centered") ? SDL_WINDOWPOS_CENTERED : vid_ypos->integer);

	// Create the window
	while (true)
	{
		if (!CreateSDLWindow(flags, wx, wy, *pwidth, *pheight))
		{
			if (*pwidth != 640 || *pheight != 480 || (flags & SDL_WINDOW_FULLSCREEN_DESKTOP))
			{
				Com_Printf(S_COLOR_RED"SDL SetVideoMode failed: %s\n", SDL_GetError());
				Com_Printf(S_COLOR_RED"Reverting to windowed r_mode 4 (640x480).\n");

				// Try to recover
				Cvar_SetValue("r_mode", 4);
				Cvar_SetValue("vid_fullscreen", 0);

				*pwidth = 640;
				*pheight = 480;

				flags &= ~SDL_WINDOW_FULLSCREEN_DESKTOP;
			}
			else
			{
				Com_Error(ERR_FATAL, "Failed to revert to r_mode 4. Exiting...\n");
				return false;
			}
		}
		else
		{
			break;
		}
	}

	//mxd. Window position was applied
	vid_xpos->modified = false;
	vid_ypos->modified = false;

	if (!R_InitContext())
	{
		window = NULL;
		return false; // InitContext() should have logged an error.
	}

	//TODO: Set the window icon - For SDL2, this must be done after creating the window
	//SetSDLIcon();

	// No cursor
	SDL_ShowCursor(0);

	initSuccessful = true;

	return true;
}

rserr_t GLimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen)
{
	VID_Printf(PRINT_ALL, "Initializing OpenGL display\n");
	VID_Printf(PRINT_ALL, "...setting mode %d:", mode);

	if (!VID_GetModeInfo(pwidth, pheight, mode))
	{
		VID_Printf(PRINT_ALL, " invalid mode\n");
		return rserr_invalid_mode;
	}

	VID_Printf(PRINT_ALL, " %dx%d %s\n", *pwidth, *pheight, (fullscreen ? "FS" : "W"));

	if (!GLimp_InitGraphics(fullscreen, pwidth, pheight))
		return rserr_invalid_mode;

	return rserr_ok;
}

// Shuts the SDL video subsystem down. Must be called after evrything's finished and clean up.
void GLimp_Shutdown(void)
{
	SDL_GL_ResetAttributes();
	ShutdownGraphics();

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

// Initializes the SDL video subsystem. Must be called before anything else.
qboolean GLimp_Init()
{
	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			VID_Printf(PRINT_ALL, S_COLOR_RED"Couldn't init SDL video: %s.\n", SDL_GetError());
			return false;
		}

		SDL_version version;

		SDL_GetVersion(&version);
		Com_Printf("SDL version is: %i.%i.%i\n", (int)version.major, (int)version.minor, (int)version.patch);
		Com_Printf("SDL video driver is \"%s\".\n", SDL_GetCurrentVideoDriver());
	}

	return true;
}

void GLimp_BeginFrame(float camera_separation)
{
	if (camera_separation < 0 && glState.stereo_enabled)
		qglDrawBuffer(GL_BACK_LEFT);
	else if (camera_separation > 0 && glState.stereo_enabled)
		qglDrawBuffer(GL_BACK_RIGHT);
	else
		qglDrawBuffer(GL_BACK);
}

// Swaps the buffers and shows the next frame.
void GLimp_EndFrame(void)
{
	SDL_GL_SwapWindow(window);
}