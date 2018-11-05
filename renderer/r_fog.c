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

// r_fog.c -- fog handling
// moved from r_main.c

#include "r_local.h"

// global fog vars w/ defaults
int FogModels[3] = {GL_LINEAR, GL_EXP, GL_EXP2};

static	qboolean r_fogenable = false;
static	int		r_fogmodel = GL_LINEAR;
static	float	r_fogdensity = 50.0;
static	float	r_fognear = 64.0;
static	float	r_fogfar = 1024.0;
static	GLfloat r_fogColor[4] = {1.0, 1.0, 1.0, 1.0};

/*
================
R_SetFog
================
*/
void R_SetFog (void)
{
	if (!r_fogenable)	// engine fog not enabled
		return;			// leave fog enabled if set by game DLL

	r_fogColor[3] = 1.0;
	qglEnable(GL_FOG);
	qglClearColor(r_fogColor[0], r_fogColor[1], r_fogColor[2], r_fogColor[3]); // Clear the background color to the fog color
	qglFogi(GL_FOG_MODE, r_fogmodel);
	qglFogfv(GL_FOG_COLOR, r_fogColor);

	if (r_fogmodel == GL_LINEAR)
	{
		qglFogf(GL_FOG_START, r_fognear); 
		qglFogf(GL_FOG_END, r_fogfar);
	}
	else
	{
		qglFogf(GL_FOG_DENSITY, r_fogdensity / 10000.f);
	}

	qglHint(GL_FOG_HINT, GL_NICEST);
}

/*
================
R_InitFogVars
================
*/
void R_InitFogVars (void)
{
	r_fogenable = false;
	r_fogmodel = GL_LINEAR;
	r_fogdensity = 50.0;
	r_fognear = 64.0;
	r_fogfar = 1024.0;
	r_fogColor[0] = r_fogColor[1] = r_fogColor[2] = r_fogColor[3] = 1.0f;
}

/*
================
R_SetFogVars
================
*/
void R_SetFogVars (qboolean enable, int model, int density, int start, int end, int red, int green, int blue)
{
	// Skip this if QGL subsystem is already down
	if (!qglDisable)
		return;

	r_fogenable = enable;
	if (!r_fogenable) // recieved fog disable message
	{
		qglDisable(GL_FOG);
		return;
	}

	int temp = model;
	if (temp > 2 || temp < 0)
		temp = 0;

	r_fogmodel = FogModels[temp];
	r_fogdensity = (float)density;
	
	if (temp == 0) // GL_LINEAR
	{
		r_fognear = (float)start;
		r_fogfar = (float)end;
	}

	r_fogColor[0] = (float)red / 255.0f;
	r_fogColor[1] = (float)green / 255.0f;
	r_fogColor[2] = (float)blue / 255.0f;

	// clamp vars
	r_fogdensity = clamp(r_fogdensity, 0.0f, 100.0f);
	r_fognear = clamp(r_fognear, 0.0f, 10000.0f - 64.0f);
	r_fogfar = clamp(r_fogfar, r_fognear + 64.0f, 10000.0f);

	for (int i = 0; i < 3; i++)
		r_fogColor[i] = clamp(r_fogColor[i], 0.0f, 1.0f);
}
