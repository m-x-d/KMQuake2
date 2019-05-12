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

// Vic did core of this - THANK YOU KING VIC!!! - psychospaz

#include "r_local.h"
#include "vlights.h"

#define VLIGHT_CLAMP_MIN	50.0f
#define VLIGHT_CLAMP_MAX	256.0f

#define VLIGHT_GRIDSIZE_X	256
#define VLIGHT_GRIDSIZE_Y	256

static vec3_t vlightgrid[VLIGHT_GRIDSIZE_X][VLIGHT_GRIDSIZE_Y];

void VLight_InitAnormTable(void)
{
	for (int x = 0; x < VLIGHT_GRIDSIZE_X; x++)
	{
		float angle = (x * 360.0f / VLIGHT_GRIDSIZE_X) * (M_PI / 180.0f);
		const float sy = sinf(angle);
		const float cy = cosf(angle);

		for (int y = 0; y < VLIGHT_GRIDSIZE_Y; y++)
		{
			angle = (y * 360.0f / VLIGHT_GRIDSIZE_X) * (M_PI / 180.0f);
			const float sp = sinf(angle);
			const float cp = cosf(angle);
			
			vlightgrid[x][y][0] = cp * cy;
			vlightgrid[x][y][1] = cp * sy;
			vlightgrid[x][y][2] = -sp;
		}
	}
}

float VLight_GetLightValue(vec3_t normal, vec3_t dir, float apitch, float ayaw, qboolean dlight)
{
	float angle1, angle2;

	if (normal[1] == 0 && normal[0] == 0)
	{
		angle2 = 0;
		if (normal[2] > 0)
			angle1 = 90;
		else
			angle1 = 270;
	}
	else
	{
		angle2 = atan2(normal[1], normal[0]) * (180.0f / M_PI);
		if (angle2 < 0)
			angle2 += 360;

		const float forward = sqrtf(normal[0] * normal[0] + normal[1] * normal[1]);
		angle1 = atan2(normal[2], forward) * (180.0f / M_PI);
		if (angle1 < 0)
			angle1 += 360;
	}
	
	int pitchofs = (angle1 + apitch) * VLIGHT_GRIDSIZE_X / 360;
	int yawofs = (angle2 + ayaw) * VLIGHT_GRIDSIZE_Y / 360;

	while (pitchofs > VLIGHT_GRIDSIZE_X - 1)
		pitchofs -= VLIGHT_GRIDSIZE_X;
	while (pitchofs < 0)
		pitchofs += VLIGHT_GRIDSIZE_X;

	while (yawofs > VLIGHT_GRIDSIZE_Y - 1)
		yawofs -= VLIGHT_GRIDSIZE_Y;
	while (yawofs < 0)
		yawofs += VLIGHT_GRIDSIZE_Y;
	
	if (dlight)
	{
		float light = DotProduct(vlightgrid[pitchofs][yawofs], dir);
		light = clamp(light, 0, 1);

		return light;
	}
	else
	{
		float light = (DotProduct(vlightgrid[pitchofs][yawofs], dir) + 2.0f) * 63.5f;
		light = clamp(light, VLIGHT_CLAMP_MIN, VLIGHT_CLAMP_MAX);
		
		return light / 256.0f;
	}
}

//mxd. Never used
/*float VLight_LerpLight(int index1, int index2, float ilerp, vec3_t dir, vec3_t angles, qboolean dlight)
{
	vec3_t normal;
	for(int i = 0; i < 3; i++)
		normal[i] = vertexnormals[index1][i] + (vertexnormals[index2][i] - vertexnormals[index1][i]) * ilerp;
	VectorNormalize(normal);

	return VLight_GetLightValue(normal, dir, angles[PITCH], angles[YAW], dlight);
}*/

void VLight_Init(void)
{
	VLight_InitAnormTable();
}