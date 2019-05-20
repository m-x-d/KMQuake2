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

// cl_util.c -- misc client utility functions

#include "client.h"

extern unsigned d_8to24table[256]; //mxd. Let's use actual palette

int color8red(int color8)
{
	return d_8to24table[color8] & 0xff;
}

int color8green(int color8)
{
	return (d_8to24table[color8] >> 8) & 0xff;
}

int color8blue(int color8)
{
	return (d_8to24table[color8] >> 16) & 0xff;
}

//mxd
void color8_to_vec3(int color8, vec3_t v)
{
	VectorSet(v, color8red(color8), color8green(color8), color8blue(color8));
}

//=================================================

void vectoangles(vec3_t vec, vec3_t angles)
{
	float yaw, pitch;
	
	if (vec[1] == 0 && vec[0] == 0)
	{
		yaw = 0;
		if (vec[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		// PMM - fixed to correct for pitch of 0
		if (vec[0])
			yaw = atan2f(vec[1], vec[0]) * 180 / M_PI;
		else if (vec[1] > 0)
			yaw = 90;
		else
			yaw = 270;

		if (yaw < 0)
			yaw += 360;

		const float forward = sqrtf(vec[0] * vec[0] + vec[1] * vec[1]);
		pitch = atan2f(vec[2], forward) * 180 / M_PI;
		if (pitch < 0)
			pitch += 360;
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}