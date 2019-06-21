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
** QGL.H
*/

#pragma once

#ifdef _WIN32
	#include <windows.h>
#endif

#ifdef __linux__
	#include <GL/glx.h>
#endif

void *QGL_GetProcAddress(const char *name); //mxd
qboolean QGL_Init(const char *dllname);
void QGL_Shutdown(void);

#ifdef _WIN32

extern int   (WINAPI *qwglChoosePixelFormat)(HDC, CONST PIXELFORMATDESCRIPTOR *);
extern int   (WINAPI *qwglDescribePixelFormat)(HDC, int, UINT, LPPIXELFORMATDESCRIPTOR);
extern int   (WINAPI *qwglGetPixelFormat)(HDC);
extern BOOL  (WINAPI *qwglSetPixelFormat)(HDC, int, CONST PIXELFORMATDESCRIPTOR *);
extern BOOL  (WINAPI *qwglSwapBuffers)(HDC);

extern BOOL  (WINAPI *qwglCopyContext)(HGLRC, HGLRC, UINT);
extern HGLRC (WINAPI *qwglCreateContext)(HDC);
extern HGLRC (WINAPI *qwglCreateLayerContext)(HDC, int);
extern BOOL  (WINAPI *qwglDeleteContext)(HGLRC);
extern HGLRC (WINAPI *qwglGetCurrentContext)(VOID);
extern HDC   (WINAPI *qwglGetCurrentDC)(VOID);
extern PROC  (WINAPI *qwglGetProcAddress)(LPCSTR);
extern BOOL  (WINAPI *qwglMakeCurrent)(HDC, HGLRC);
extern BOOL  (WINAPI *qwglShareLists)(HGLRC, HGLRC);
extern BOOL  (WINAPI *qwglUseFontBitmaps)(HDC, DWORD, DWORD, DWORD);

extern BOOL  (WINAPI *qwglUseFontOutlines)(HDC, DWORD, DWORD, DWORD, FLOAT, FLOAT, int, LPGLYPHMETRICSFLOAT);

extern BOOL  (WINAPI *qwglDescribeLayerPlane)(HDC, int, int, UINT, LPLAYERPLANEDESCRIPTOR);
extern int   (WINAPI *qwglSetLayerPaletteEntries)(HDC, int, int, int, CONST COLORREF *);
extern int   (WINAPI *qwglGetLayerPaletteEntries)(HDC, int, int, int, COLORREF *);
extern BOOL  (WINAPI *qwglRealizeLayerPalette)(HDC, int, BOOL);
extern BOOL  (WINAPI *qwglSwapLayerBuffers)(HDC, UINT);

extern BOOL ( WINAPI *qwglSwapIntervalEXT)(int interval);

#endif

#ifdef __linux__

// local function in dll
extern void *qwglGetProcAddress(char *symbol);

extern void (*qgl3DfxSetPaletteEXT)(GLuint *);

//GLX Functions
extern XVisualInfo * (*qglXChooseVisual)( Display *dpy, int screen, int *attribList );
extern GLXContext (*qglXCreateContext)( Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct );
extern void (*qglXDestroyContext)( Display *dpy, GLXContext ctx );
extern Bool (*qglXMakeCurrent)( Display *dpy, GLXDrawable drawable, GLXContext ctx);
extern void (*qglXCopyContext)( Display *dpy, GLXContext src, GLXContext dst, GLuint mask );
extern void (*qglXSwapBuffers)( Display *dpy, GLXDrawable drawable );

#endif // linux