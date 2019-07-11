/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2019 MaxED
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

// SDL2 backend for the renderer.

#include "r_local.h"
#include "../win32/sdlquake.h" //mxd

static SDL_GLContext context = NULL;

void R_SetVsync(qboolean enable) //mxd
{
	SDL_GL_SetSwapInterval(enable);
}

// Updates the gamma ramp.
void R_UpdateGammaRamp()
{
	const float gamma = vid_gamma->value;

	ushort ramp[256];
	SDL_CalculateGammaRamp(gamma, ramp);

	if (SDL_SetWindowGammaRamp(window, ramp, ramp, ramp) != 0)
		VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Setting gamma failed: %s\n", SDL_GetError());
}

// Initializes the OpenGL context
int R_InitContext()
{
	// Coders are stupid
	if (window == NULL)
	{
		VID_Error(ERR_FATAL, "SDL_Window must be created before calling R_InitContext()!");
		return false;
	}

	// Initialize GL context.
	context = SDL_GL_CreateContext(window);

	if (context == NULL)
	{
		VID_Printf(PRINT_ALL, S_COLOR_RED"R_InitContext(): Creating OpenGL Context failed: %s\n", SDL_GetError());
		window = NULL;
		return false;
	}

	// Check if we've got 8 stencil bits.
	int stencil_bits = 0;
	
	if (glState.stencilTest && SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits) != 8)
		glState.stencilTest = false;

	// Initialize gamma.
	vid_gamma->modified = true;

	return true;
}

// Shuts the GL context down
void R_ShutdownContext(void)
{
	if (window && context)
	{
		SDL_GL_DeleteContext(context);
		context = NULL;
	}
}

// Returns the adress of a GL function
void *R_GetProcAddress(const char* proc)
{
	return SDL_GL_GetProcAddress(proc);
}

// Called before creating SDL window
qboolean R_PrepareForWindow()
{
	// Set GL context attributs bound to the window.
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	glState.stencilTest = (SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8) == 0);

	return true;
}