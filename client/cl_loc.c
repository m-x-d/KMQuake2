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

NoCheat LOC support by NiceAss
Edited and Fixed by Xile and FourthX
*/

#include "client.h"

#ifdef LOC_SUPPORT

typedef struct
{
	vec3_t origin;
	char name[64];
	qboolean used;
} loc_t;

#define MAX_LOCATIONS 768

static loc_t locations[MAX_LOCATIONS];

static int CL_FreeLoc()
{
	for (int i = 0; i < MAX_LOCATIONS; i++)
		if (!locations[i].used)
			return i;

	// Just keep overwriting the last one....
	return MAX_LOCATIONS - 1;
}

void CL_LoadLoc()
{
	char mapname[MAX_QPATH];
	char filename[MAX_OSPATH];
	char *buf;

	memset(locations, 0, sizeof(loc_t) * MAX_LOCATIONS);

	// Format map pathname
	Q_strncpyz(mapname, cl.configstrings[CS_MODELS + 1] + 5, sizeof(mapname)); // Skip "maps/"
	mapname[strlen(mapname) - 4] = 0; // Remove ".bsp"
	Com_sprintf(filename, sizeof(filename), "locs/%s.loc", mapname);

	// Load file and check buffer and size
	const int fSize = FS_LoadFile(filename, (void**)&buf);
	if (!buf)
	{
		Com_DPrintf(S_COLOR_YELLOW"CL_LoadLoc: couldn't load '%s'!\n", filename);
		return;
	}

	if (fSize < 7)
	{
		Com_Printf(S_COLOR_YELLOW"CL_LoadLoc: loc file '%s' is too small (%d bytes)!\n", filename, fSize);
		FS_FreeFile(buf);

		return;
	}

	// Check if it's in floating-point format
	char *line = buf;
	int nLines = 0;
	const qboolean fpFormat = (strstr(buf, ".000") || strstr(buf, ".125") || strstr(buf, ".250") || strstr(buf, ".375") 
							|| strstr(buf, ".500") || strstr(buf, ".625") || strstr(buf, ".750") || strstr(buf, ".875"));
	const float fpScaler = (fpFormat ? 1.0f : 0.125f); //mxd
	while (*line && (line < buf + fSize))
	{
		// Overwrite new line characters with null
		char *nl = strchr(line, '\n');
		if (nl)
			*nl = '\0';

		// Nullify the carriage return too!
		char *cr = strchr(line, '\r');
		if (cr)
			*cr = '\0';
		nLines++;

		// Skip comments
		const qboolean isCommentLine = (line[0] == ':' || line[0] == ';' || line[0] == '/');
		if (!isCommentLine)
		{
			// Break the line up into 4 tokens
			char *token1 = line;
			char *token2 = strchr(token1, ' ');
			if (token2 == NULL)
			{
				Com_Printf(S_COLOR_YELLOW"CL_LoadLoc: line %d is incomplete in '%s'!\n", nLines, filename);
				continue;
			}

			*token2 = '\0';
			token2++;

			char *token3 = strchr(token2, ' ');
			if (token3 == NULL)
			{
				Com_Printf(S_COLOR_YELLOW"CL_LoadLoc: line %d is incomplete in '%s'!\n", nLines, filename);
				continue;
			}

			*token3 = '\0';
			token3++;

			char *token4 = strchr(token3, ' ');
			if (token4 == NULL)
			{
				Com_Printf(S_COLOR_YELLOW"CL_LoadLoc: line %d is incomplete in '%s'!\n", nLines, filename);
				continue;
			}

			*token4 = '\0';
			token4++;

			// Floating-point format has a ':' between coords and label
			if (fpFormat)
			{
				char *tok = token4;
				token4 = strchr(tok, ' ');
				if (token4 == NULL)
				{
					Com_Printf(S_COLOR_YELLOW"CL_LoadLoc: line %d is incomplete in '%s'!\n", nLines, filename);
					continue;
				}

				*token4 = '\0';
				token4++;
			}

			// Copy the data to the struct
			int index = CL_FreeLoc();
			locations[index].origin[0] = (float)atof(token1) * fpScaler;
			locations[index].origin[1] = (float)atof(token2) * fpScaler;
			locations[index].origin[2] = (float)atof(token3) * fpScaler;

			Q_strncpyz(locations[index].name, token4, sizeof(locations[index].name));
			locations[index].used = true;
		}

		if (!nl)
			break;

		line = nl + 1;
	}

	FS_FreeFile(buf);
	Com_Printf("CL_LoadLoc: loaded %d locations from '%s'.\n", nLines, filename);
}

static int CL_LocIndex(const vec3_t origin)
{
	vec3_t diff; // FourthX fix
	float minDist = 999999;
	int locIndex = -1;

	for (int i = 0; i < MAX_LOCATIONS; i++)
	{
		if (!locations[i].used)
			continue;

		VectorSubtract(origin, locations[i].origin, diff);
		
		const float dist = VectorLengthSquared(diff);
		if (dist < minDist)
		{
			minDist = dist;
			locIndex = i;
		}
	}

	return locIndex;
}

void CL_LocDelete()
{
	vec3_t point;

	for(int i = 0; i < 3; i++)
		point[i] = cl.frame.playerstate.pmove.origin[i] * 0.125f;

	const int index = CL_LocIndex(point);

	if (index != -1)
	{
		locations[index].used = false;
		Com_Printf("Location '%s' deleted.\n", locations[index].name); // Xile reworked.
	}
	else
	{
		Com_Printf(S_COLOR_YELLOW"Warning: no location to delete!\n");
	}
}

void CL_LocAdd(char *name)
{
	int index = CL_FreeLoc();

	for (int i = 0; i < 3; i++)
		locations[index].origin[i] = cl.frame.playerstate.pmove.origin[i] * 0.125f;

	Q_strncpyz(locations[index].name, name, sizeof(locations[index].name));
	locations[index].used = true;

	Com_Printf("Location '%s' added at (%.3f %.3f %.3f). Loc #%d.\n", locations[index].name,
		locations[index].origin[0],
		locations[index].origin[1],
		locations[index].origin[2],
		index);
}

void CL_LocWrite()
{
	char mapname[MAX_QPATH];
	char filename[MAX_OSPATH];

	// Format map pathname
	Q_strncpyz(mapname, cl.configstrings[CS_MODELS + 1] + 5, sizeof(mapname));
	mapname[strlen(mapname) - 4] = 0;
	Com_sprintf(filename, sizeof(filename), "%s/locs/%s.loc", FS_Gamedir(), mapname);

	FS_CreatePath(filename);

	FILE *f = fopen(filename, "w");
	if (f == NULL)
	{
		Com_Printf(S_COLOR_YELLOW"Unable to open 'locs/%s.loc' for writing.\n", mapname);
		return;
	}

	fprintf(f, "// This location file is generated by %s, edit at your own risk.\n", ENGINE_NAME); //mxd. +ENGINE_NAME

	for (int i = 0; i < MAX_LOCATIONS; i++)
	{
		if (!locations[i].used)
			continue;

		fprintf(f, "%d %d %d %s\n",
			(int)(locations[i].origin[0] * 8),
			(int)(locations[i].origin[1] * 8),
			(int)(locations[i].origin[2] * 8),
			locations[i].name);
	}

	fclose(f);

	Com_Printf("Saved location data to 'locs/%s.loc'.\n", mapname);
}

void CL_LocPlace()
{
	vec3_t point, end;

	for (int i = 0; i < 3; i++)
		point[i] = cl.frame.playerstate.pmove.origin[i] * 0.125f;

	const int index1 = CL_LocIndex(point);

	VectorMA(cl.predicted_origin, WORLD_SIZE, cl.v_forward, end); // Was 8192
	const trace_t tr = CM_BoxTrace(cl.predicted_origin, end, vec3_origin, vec3_origin, 0, MASK_PLAYERSOLID);
	const int index2 = CL_LocIndex(tr.endpos);

	if (index1 != -1)
		Cvar_ForceSet("loc_here", locations[index1].name);
	else
		Cvar_ForceSet("loc_here", "");

	if (index2 != -1)
		Cvar_ForceSet("loc_there", locations[index2].name);
	else
		Cvar_ForceSet("loc_there", "");
}

void CL_AddViewLocs()
{
	vec3_t point, diff;

	if (!cl_drawlocs->value)
		return;

	for (int i = 0; i < 3; i++)
		point[i] = cl.frame.playerstate.pmove.origin[i] * 0.125f;

	const int index = CL_LocIndex(point);

	for (int i = 0; i < MAX_LOCATIONS; i++)
	{
		entity_t ent;

		if (!locations[i].used)
			continue;

		for (int c = 0; c < 3; c++)
			diff[c] = (float)cl.frame.playerstate.pmove.origin[c] * 0.125f - locations[i].origin[c];

		if (VectorLengthSquared(diff) > 4000 * 4000)
			continue;

		memset(&ent, 0, sizeof(entity_t));
		VectorCopy(locations[i].origin, ent.origin);
		ent.skinnum = 0;
		ent.skin = NULL;
		ent.model = NULL;

		if (i == index)
			ent.origin[2] += sinf(cl.time * 0.01f) * 10.0f;

		V_AddEntity(&ent);
	}
}

void CL_LocHelp_f()
{
	// Xile/jitspoe - simple help cmd for reference
	Com_Printf("Loc Commands:\n"
			   "-------------\n"
			   "loc_add <label/description>\n"
			   "loc_del\n"
			   "loc_save\n"
			   "cl_drawlocs\n"
			   "say_team $loc_here\n"
			   "say_team $loc_there\n"
			   "-------------\n");
}

#endif	// LOC_SUPPORT