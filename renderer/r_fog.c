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

// Global fog vars w/ defaults
static int fogmodels[] = { GL_LINEAR, GL_EXP, GL_EXP2 };

static qboolean r_fogenable = false;
static qboolean r_fogsuspended = false;
static int r_fogmodel = GL_LINEAR;
static float r_fogdensity = 50.0f;
static float r_fogskydensity = 5.0f;
static float r_fognear = 64.0f;
static float r_fogfar = 1024.0f;
static float r_fogskyfar = 10240.0f;
static GLfloat r_fogColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

void R_SetFog(void)
{
	if (!r_fogenable)	// Engine fog not enabled.
		return;			// Leave fog enabled if set by game DLL.

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
		qglFogf(GL_FOG_DENSITY, r_fogdensity / 10000.0f);
	}

	qglHint(GL_FOG_HINT, GL_NICEST);
}

void R_SetSkyFog(qboolean setSkyFog)
{
	if (!r_fogenable)	// Engine fog not enabled.
		return;			// Leave fog enabled if set by game DLL.

	if (r_fogmodel == GL_LINEAR)
	{
		const float fogfar = (setSkyFog ? r_fogskyfar : r_fogfar); //mxd
		qglFogf(GL_FOG_END, fogfar);
	}
	else
	{
		const float fogdensity = (setSkyFog ? r_fogskydensity : r_fogdensity); //mxd
		qglFogf(GL_FOG_DENSITY, fogdensity / 10000.0f);
	}
}

void R_SuspendFog(void)
{
	// Check if fog is enabled; if so, disable it
	if (qglIsEnabled(GL_FOG))
	{
		r_fogsuspended = true;
		qglDisable(GL_FOG);
	}
}

void R_ResumeFog(void)
{
	// Re-enable fog if it was on
	if (r_fogsuspended)
	{
		r_fogsuspended = false;
		qglEnable(GL_FOG);
	}
}

//mxd. Disabled. Fixes fog vars being reset after changing resolution when a map is loaded. Doesn't seem to break anything.
/*void R_InitFogVars(void)
{
	r_fogenable = false;
	r_fogsuspended = false;
	r_fogmodel = GL_LINEAR;
	r_fogdensity = 50.0f;
	r_fogskydensity = 5.0f;
	r_fognear = 64.0f;
	r_fogfar = 1024.0f;
	r_fogskyfar = 10240.0f;

	Vector4Set(r_fogColor, 1.0f, 1.0f, 1.0f, 1.0f);
}*/

void R_SetFogVars(qboolean enable, int model, int density, int start, int end, int red, int green, int blue)
{
	// Skip this if QGL subsystem is already down
	if (!qglDisable)
		return;

	r_fogenable = enable;
	if (!r_fogenable) // Recieved fog disable message
	{
		qglDisable(GL_FOG);
		return;
	}

	int temp = model;
	if (temp > 2 || temp < 0)
		temp = 0;

	float maxFogFar = (r_skydistance && r_skydistance->value ? r_skydistance->value : 10000.0f);
	maxFogFar = max(maxFogFar, 1024.0f);

	float skyRatio = (r_fog_skyratio && r_fog_skyratio->value ? r_fog_skyratio->value : 10.0f);
	skyRatio = clamp(skyRatio, 1.0f, 100.0f);

	r_fogmodel = fogmodels[temp];
	r_fogdensity = (float)density;
	r_fogskydensity = r_fogdensity / skyRatio;
	
	if (temp == 0) // GL_LINEAR
	{
		r_fognear = (float)start;
		r_fogfar = (float)end;
		r_fogskyfar = r_fogfar * skyRatio;
	}

	r_fogColor[0] = (float)red / 255.0f;
	r_fogColor[1] = (float)green / 255.0f;
	r_fogColor[2] = (float)blue / 255.0f;

	// Clamp vars
	r_fogdensity = clamp(r_fogdensity, 0.0f, 100.0f);
	r_fogskydensity = clamp(r_fogskydensity, 0.0f, (100.0f / skyRatio));
	r_fognear = clamp(r_fognear, 0.0f, 10000.0f - 64.0f);
	r_fogfar = clamp(r_fogfar, r_fognear + 64.0f, maxFogFar);
	r_fogskyfar = clamp(r_fogskyfar, (r_fognear + 64.0f) * skyRatio, maxFogFar * skyRatio);

	for (int i = 0; i < 3; i++)
		r_fogColor[i] = clamp(r_fogColor[i], 0.0f, 1.0f);
}

//mxd
void R_UpdateFogVars()
{
	int model = 0;
	for (int i = 0; i < sizeof(fogmodels) / sizeof(fogmodels[0]); i++)
	{
		if (r_fogmodel == fogmodels[i])
		{
			model = i;
			break;
		}
	}
	
	R_SetFogVars(r_fogenable, model, r_fogdensity, r_fognear, r_fogfar, r_fogColor[0] * 255.0f, r_fogColor[1] * 255.0f, r_fogColor[2] * 255.0f);
}