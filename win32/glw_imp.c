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
** GLW_IMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
** GLimp_SwitchFullscreen
**
*/
#include <assert.h>
#include <windows.h>
#include "../renderer/r_local.h"
#include "resource.h"
#include "glw_win.h"
#include "winquake.h"

qboolean GLimp_InitGL(void);

glwstate_t glw_state;

extern cvar_t *vid_ref;

// Knightmare- added Vic's hardware gammaramp
static WORD original_ramp[3][256];
static WORD gamma_ramp[3][256];

static void InitGammaRamp(void)
{
	if (!r_ignorehwgamma->value)
		glState.gammaRamp = GetDeviceGammaRamp(glw_state.hDC, original_ramp);
	else
		glState.gammaRamp = false;

	if (glState.gammaRamp)
		vid_gamma->modified = true;
}

static void ShutdownGammaRamp(void)
{
	if (!glState.gammaRamp)
		return;
	
	SetDeviceGammaRamp(glw_state.hDC, original_ramp);
}

void UpdateGammaRamp(void)
{
	if (!glState.gammaRamp)
		return;
	
	memcpy(gamma_ramp, original_ramp, sizeof(original_ramp));

	for (int o = 0; o < 3; o++) 
	{
		for (int i = 0; i < 256; i++) 
		{
			int v = 255 * powf((i + 0.5f) / 255.5f, vid_gamma->value) + 0.5f;
			v = clamp(v, 0, 255); //mxd
			gamma_ramp[o][i] = (WORD)v << 8;
		}
	}

	SetDeviceGammaRamp(glw_state.hDC, gamma_ramp);
}

static void ToggleGammaRamp(qboolean enable)
{
	if (!glState.gammaRamp)
		return;

	SetDeviceGammaRamp(glw_state.hDC, (enable ? gamma_ramp : original_ramp));
}
// end Vic's hardware gammaramp

extern qboolean modType(char *name);

#define	WINDOW_CLASS_NAME	ENGINE_NAME // changed
#define	WINDOW_CLASS_NAME2	ENGINE_NAME" - The Reckoning" // changed
#define	WINDOW_CLASS_NAME3	ENGINE_NAME" - Ground Zero" // changed

static qboolean VID_CreateWindow(int width, int height, qboolean fullscreen)
{
	WNDCLASS wc;
	int stylebits;
	int x, y;
	int exstyle;

	/* Register the frame class */
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC)glw_state.wndproc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = glw_state.hInstance;

	if (modType("xatrix")) // q2mp1
		wc.hIcon = LoadIcon(glw_state.hInstance, MAKEINTRESOURCE(IDI_ICON2));
	else if (modType("rogue")) // q2mp2
		wc.hIcon = LoadIcon(glw_state.hInstance, MAKEINTRESOURCE(IDI_ICON3));
	else 
		wc.hIcon = LoadIcon(glw_state.hInstance, MAKEINTRESOURCE(IDI_ICON1));

	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (void *)COLOR_GRAYTEXT;
	wc.lpszMenuName = 0;
	wc.lpszClassName = WINDOW_CLASS_NAME;

	if (!RegisterClass(&wc))
		VID_Error(ERR_FATAL, "Couldn't register window class");

	if (fullscreen)
	{
		exstyle = WS_EX_TOPMOST;
		//stylebits = WS_POPUP|WS_VISIBLE;
		stylebits = WS_POPUP | WS_SYSMENU | WS_VISIBLE;
	}
	else
	{
		exstyle = 0;
		//stylebits = WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE;
		stylebits = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
	}

	RECT r = {
		.left = 0,
		.top = 0,
		.right = width,
		.bottom = height
	};

	AdjustWindowRect(&r, stylebits, FALSE);

	const int w = r.right - r.left;
	const int h = r.bottom - r.top;

	if (fullscreen)
	{
		x = 0;
		y = 0;
	}
	else
	{
		cvar_t *vid_xpos = Cvar_Get("vid_xpos", "0", 0);
		cvar_t *vid_ypos = Cvar_Get("vid_ypos", "0", 0);
		x = vid_xpos->integer;
		y = vid_ypos->integer;
	}

	glw_state.hWnd = CreateWindowEx(
		 exstyle, 
		 WINDOW_CLASS_NAME,
		 ENGINE_NAME, //mxd. Changed to ENGINE_NAME
		 stylebits,
		 x, y, w, h,
		 NULL,
		 NULL,
		 glw_state.hInstance,
		 NULL);

	if (!glw_state.hWnd)
		VID_Error(ERR_FATAL, "Couldn't create window");
	
	ShowWindow(glw_state.hWnd, SW_SHOW);
	UpdateWindow(glw_state.hWnd);

	// Init all the gl stuff for the window
	if (!GLimp_InitGL())
	{
		VID_Printf(PRINT_ALL, "VID_CreateWindow() - GLimp_InitGL failed\n");
		return false;
	}

	SetForegroundWindow(glw_state.hWnd);
	SetFocus(glw_state.hWnd);

	// Let the sound and input subsystems know about the new window
	VID_NewWindow(width, height);

	return true;
}

rserr_t GLimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen)
{
	VID_Printf(PRINT_ALL, "Initializing OpenGL display\n");
	VID_Printf(PRINT_ALL, "...setting mode %d:", mode);

	int width, height;
	if (!VID_GetModeInfo(&width, &height, mode))
	{
		VID_Printf(PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	VID_Printf(PRINT_ALL, " %dx%d %s\n", width, height, (fullscreen ? "FS" : "W"));

	// Destroy the existing window
	if (glw_state.hWnd)
		GLimp_Shutdown();

	// Do a CDS if needed
	if (fullscreen) //TODO: mxd. ditch fullscreen, use borderless window
	{
		DEVMODE dm;

		VID_Printf(PRINT_ALL, "...attempting fullscreen\n");

		memset(&dm, 0, sizeof(dm));

		dm.dmSize = sizeof(dm);

		dm.dmPelsWidth  = width;
		dm.dmPelsHeight = height;
		dm.dmFields	 = DM_PELSWIDTH | DM_PELSHEIGHT;

		VID_Printf(PRINT_ALL, "...calling CDS: ");
		if (ChangeDisplaySettings(&dm, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
		{
			*pwidth = width;
			*pheight = height;

			glState.fullscreen = true;

			VID_Printf(PRINT_ALL, "ok\n");

			if (!VID_CreateWindow(width, height, true))
				return rserr_invalid_mode;

			return rserr_ok;
		}
		else
		{
			*pwidth = width;
			*pheight = height;

			VID_Printf(PRINT_ALL, "failed\n");
			VID_Printf(PRINT_ALL, "...calling CDS assuming dual monitors:");

			dm.dmPelsWidth = width * 2;
			dm.dmPelsHeight = height;
			dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

			// Our first CDS failed, so maybe we're running on some weird dual monitor system 
			if (ChangeDisplaySettings(&dm, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			{
				VID_Printf(PRINT_ALL, " failed\n");
				VID_Printf(PRINT_ALL, "...setting windowed mode\n");

				ChangeDisplaySettings(0, 0);

				*pwidth = width;
				*pheight = height;
				glState.fullscreen = false;

				if (!VID_CreateWindow(width, height, false))
					return rserr_invalid_mode;

				return rserr_invalid_fullscreen;
			}
			else
			{
				VID_Printf(PRINT_ALL, " ok\n");
				if (!VID_CreateWindow(width, height, true))
					return rserr_invalid_mode;

				glState.fullscreen = true;
				return rserr_ok;
			}
		}
	}
	else
	{
		VID_Printf(PRINT_ALL, "...setting windowed mode\n");

		ChangeDisplaySettings(0, 0);

		*pwidth = width;
		*pheight = height;
		glState.fullscreen = false;

		if (!VID_CreateWindow(width, height, false))
			return rserr_invalid_mode;
	}

	return rserr_ok;
}

// This routine does all OS specific shutdown procedures for the OpenGL subsystem.
// Under OpenGL this means NULLing out the current DC and HGLRC, deleting 
// the rendering context, and releasing the DC acquired for the window.
// The state structure is also nulled out.
void GLimp_Shutdown(void)
{
	// Knightmare- added Vic's hardware gamma ramp
	ShutdownGammaRamp();

	if (qwglMakeCurrent && !qwglMakeCurrent(NULL, NULL))
		VID_Printf(PRINT_ALL, "%s: wglMakeCurrent failed\n", __func__);

	if (glw_state.hGLRC)
	{
		if (qwglDeleteContext && !qwglDeleteContext(glw_state.hGLRC))
			VID_Printf(PRINT_ALL, "%s: wglDeleteContext failed\n", __func__);

		glw_state.hGLRC = NULL;
	}

	if (glw_state.hDC)
	{
		if (!ReleaseDC(glw_state.hWnd, glw_state.hDC))
			VID_Printf(PRINT_ALL, "%s: ReleaseDC failed\n", __func__);

		glw_state.hDC = NULL;
	}

	if (glw_state.hWnd)
	{	
		ShowWindow(glw_state.hWnd, SW_HIDE); //Knightmare- remove leftover button on taskbar
		DestroyWindow(glw_state.hWnd);
		glw_state.hWnd = NULL;
	}

	if (glw_state.log_fp)
	{
		fclose(glw_state.log_fp);
		glw_state.log_fp = 0;
	}

	UnregisterClass(WINDOW_CLASS_NAME, glw_state.hInstance);

	if (glState.fullscreen)
	{
		ChangeDisplaySettings(0, 0);
		glState.fullscreen = false;
	}
}

// This routine is responsible for initializing the OS specific portions of OpenGL.
// Under Win32 this means dealing with the pixelformats and doing the wgl interface stuff.
qboolean GLimp_Init(void *hinstance, void *wndproc)
{
	glw_state.hInstance = (HINSTANCE)hinstance;
	glw_state.wndproc = wndproc;

	return true;
}

static qboolean GLimp_InitGL(void)
{
	PIXELFORMATDESCRIPTOR pfd = 
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,								// version number
		PFD_DRAW_TO_WINDOW |			// support window
		PFD_SUPPORT_OPENGL |			// support OpenGL
		PFD_DOUBLEBUFFER,				// double buffered
		PFD_TYPE_RGBA,					// RGBA type
		32,								// 32-bit color depth // was 24
		0, 0, 0, 0, 0, 0,				// color bits ignored
		0,								// no alpha buffer
		0,								// shift bit ignored
		0,								// no accumulation buffer
		0, 0, 0, 0, 					// accum bits ignored
		//Knightmare 12/24/2001- stencil buffer
		24,								// 24-bit z-buffer, was 32	
		8,								// 8-bit stencil buffer
		//end Knightmare
		0,								// no auxiliary buffer
		PFD_MAIN_PLANE,					// main layer
		0,								// reserved
		0, 0, 0							// layer masks ignored
	};

	cvar_t *stereo = Cvar_Get("cl_stereo", "0", 0);

	// Set PFD_STEREO if necessary
	glState.stereo_enabled = (stereo->integer != 0);
	if (glState.stereo_enabled)
	{
		VID_Printf(PRINT_ALL, "...attempting to use stereo\n");
		pfd.dwFlags |= PFD_STEREO;
	}

	// Get a DC for the specified window
	if (glw_state.hDC != NULL)
		VID_Printf(PRINT_ALL, "%s: non-NULL DC exists\n", __func__);

	glw_state.hDC = GetDC(glw_state.hWnd);
	if (glw_state.hDC == NULL)
	{
		VID_Printf(PRINT_ALL, "%s: GetDC failed\n", __func__);
		return false;
	}

	const int pixelformat = ChoosePixelFormat(glw_state.hDC, &pfd);
	if (pixelformat == 0)
	{
		VID_Printf(PRINT_ALL, "%s: ChoosePixelFormat failed\n", __func__);
		return false;
	}

	if (!SetPixelFormat(glw_state.hDC, pixelformat, &pfd))
	{
		VID_Printf(PRINT_ALL, "%s: SetPixelFormat failed\n", __func__);
		return false;
	}

	DescribePixelFormat(glw_state.hDC, pixelformat, sizeof(pfd), &pfd);

	// Report if stereo is desired but unavailable
	if (!(pfd.dwFlags & PFD_STEREO) && (stereo->integer != 0))
	{
		VID_Printf(PRINT_ALL, "...failed to select stereo pixel format\n");
		Cvar_SetValue("cl_stereo", 0);
		glState.stereo_enabled = false;
	}

	// Startup the OpenGL subsystem by creating a context and making it current
	if ((glw_state.hGLRC = qwglCreateContext(glw_state.hDC)) == 0)
	{
		VID_Printf(PRINT_ALL, "%s: qwglCreateContext failed\n", __func__);
		goto fail;
	}

	if (!qwglMakeCurrent(glw_state.hDC, glw_state.hGLRC))
	{
		VID_Printf(PRINT_ALL, "%s: qwglMakeCurrent failed\n", __func__);
		goto fail;
	}

	// Print out PFD specifics 
	VID_Printf(PRINT_ALL, "PIXELFORMAT: color(%d-bits) Z(%d-bit)\n", (int)pfd.cColorBits, (int)pfd.cDepthBits);

	// Knightmare- Vic's hardware gamma stuff
	InitGammaRamp();

	// Moved these to GL_SetDefaultState
	//glState.blend = false;
	//glState.alphaTest = false;
	//end Knightmare

	if (pfd.cStencilBits)
	{
		VID_Printf(PRINT_ALL, "...using stencil buffer\n");
		glConfig.have_stencil = true;
	}

/*	Moved to GL_SetDefaultState in r_glstate.c
	// Vertex arrays
	qglEnableClientState (GL_TEXTURE_COORD_ARRAY);
	qglEnableClientState (GL_VERTEX_ARRAY);
	qglEnableClientState (GL_COLOR_ARRAY);

	qglTexCoordPointer (2, GL_FLOAT, sizeof(texCoordArray[0][0]), texCoordArray[0][0]);
	qglVertexPointer (3, GL_FLOAT, sizeof(vertexArray[0]), vertexArray[0]);
	qglColorPointer (4, GL_FLOAT, sizeof(colorArray[0]), colorArray[0]);
	//glState.activetmu[0] = true;
	// end vertex arrays
*/

	return true;

fail:
	if (glw_state.hGLRC)
	{
		qwglDeleteContext(glw_state.hGLRC);
		glw_state.hGLRC = NULL;
	}

	if (glw_state.hDC)
	{
		ReleaseDC(glw_state.hWnd, glw_state.hDC);
		glw_state.hDC = NULL;
	}

	return false;
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

// Responsible for doing a swapbuffers and possibly for other stuff as yet to be determined.
// Probably better not to make this a GLimp function and instead do a call to GLimp_SwapBuffers.
void GLimp_EndFrame(void)
{
	const int err = qglGetError();
	if (err != GL_NO_ERROR)	// Output error code instead
		VID_Printf(PRINT_DEVELOPER, "OpenGL Error %i\n", err);

	if (!stricmp(r_drawbuffer->string, "GL_BACK") && !qwglSwapBuffers(glw_state.hDC))
		VID_Error(ERR_FATAL, "%s: SwapBuffers() failed!\n", __func__);
}

void GLimp_AppActivate(qboolean active)
{
	static qboolean	desktop_restored;
	cvar_t *restore_desktop = Cvar_Get("win_alttab_restore_desktop", "1", CVAR_ARCHIVE);

	if (active)
	{
		ToggleGammaRamp(true);
		SetForegroundWindow(glw_state.hWnd);
		ShowWindow(glw_state.hWnd, SW_RESTORE);

		// Knightmare- restore desktop settings on alt-tabbing from fullscreen
		if (vid_fullscreen->value && desktop_restored && glw_state.hGLRC != NULL) //TODO: mxd. not needed?
		{
			int width, height;
			DEVMODE	dm;

			if (!VID_GetModeInfo(&width, &height, r_mode->integer))
			{
				VID_Printf(PRINT_ALL, "invalid mode\n");
				return;
			}
			
			memset(&dm, 0, sizeof(dm));
			dm.dmSize = sizeof(dm);
			dm.dmPelsWidth = width;
			dm.dmPelsHeight = height;
			dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

			VID_Printf(PRINT_ALL, "...calling CDS: ");
			if (ChangeDisplaySettings(&dm, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
			{
				VID_Printf(PRINT_ALL, "ok\n");
			}
			else
			{
				// Our first CDS failed, so maybe we're running on some weird dual monitor system 
				VID_Printf(PRINT_ALL, "failed\n");
				VID_Printf(PRINT_ALL, "...calling CDS assuming dual monitors: ");

				dm.dmPelsWidth = width * 2;
				dm.dmPelsHeight = height;
				dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

				if (ChangeDisplaySettings(&dm, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
					VID_Printf(PRINT_ALL, "ok\n");
			}

			desktop_restored = false;
		}
	}
	else
	{
		ToggleGammaRamp(false);

		if (vid_fullscreen->integer)
		{
			ShowWindow(glw_state.hWnd, SW_MINIMIZE);

			// Knightmare- restore desktop settings on alt-tabbing from fullscreen
			desktop_restored = (restore_desktop->integer != 0);
			if (desktop_restored)
				ChangeDisplaySettings(0, 0);
		}
	}
}