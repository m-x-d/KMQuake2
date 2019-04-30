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

//=================================================

// Returns the length of formatting sequences (like ^1) in given string. Was named stringLengthExtra in KMQ2.
int CL_StringLengthExtra(const char *string)
{
	const unsigned len = strlen(string);
	int ulen = 0;

	for (unsigned i = 0; i < len; i++)
		if (string[i] == '^' && i < len - 1)
			ulen += 2;

	return ulen;
}

//mxd. Returns string length without formatting sequences (like ^1). Was named stringLen in KMQ2.
int CL_UnformattedStringLength(const char *string)
{
	return strlen(string) - CL_StringLengthExtra(string);
}

//mxd. Strips color markers (like '^1'), replaces special q2 font elements (like menu borders) with printable chars. Was named unformattedString in KMQ2.
char *CL_UnformattedString(const char *string)
{
	char *newstring = "";
	const unsigned len = strlen(string);

	for (unsigned i = 0; i < len; i++)
	{
		char c = (string[i] & ~128);

		if (c == '^' && i < len - 1) // Skip color markers
		{
			i++;
			continue;
		}

		//mxd. Replace unprintable chars (adapted from YQ2)
		if (c < ' ' && (c < '\t' || c > '\r'))
		{
			switch (c)
			{
				// No idea if the following two are ever sent here, but in conchars.pcx they look like this, so do the replacements
				case 0x10: c = '['; break;
				case 0x11: c = ']'; break;

				// Horizontal line chars
				case 0x1D: c = '<'; break; // start
				case 0x1E: c = '='; break; // mid
				case 0x1F: c = '>'; break; // end

				// Just replace all other unprintable chars with space, should be good enough
				default: c = ' '; break;
			}
		}

		newstring = va("%s%c", newstring, c);
	}

	return newstring;
}