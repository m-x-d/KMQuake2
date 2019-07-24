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

// cl_event.c -- entity events: footsteps, etc.
// moved from cl_fx.c

#include "client.h"

#define MAX_TEX_SURF 2048 // Was 256

struct texsurf_s
{
	int step_id;
	char texture[32];
};

typedef struct texsurf_s texsurf_t;
static texsurf_t tex_surf[MAX_TEX_SURF];
static int num_texsurfs;

static qboolean buf_gets(char *dest, const int destsize, char **f)
{
	char *old = *f;
	*f = strchr(old, '\n');

	if (!*f)
	{
		// No more new lines
		*f = old + strlen(old);

		if (!strlen(*f))
			return false; // End of file, nothing else to grab
	}

	(*f)++; // Advance past EOL
	strncpy(dest, old, min(destsize - 1, (int)(*f - old - 1)));

	return true;
}

// Reads in defintions for footsteps based on texture name.
void CL_ReadTextureSurfaceAssignments()
{
	char filename[MAX_OSPATH];
	char *footstep_data;
	char line[80];

	num_texsurfs = 0;

	Com_sprintf(filename, sizeof(filename), "texsurfs.txt");
	FS_LoadFile(filename, (void **)&footstep_data);

	if (!footstep_data)
		return;

	char *parsedata = footstep_data;

	while (buf_gets(line, sizeof(line), &parsedata) && num_texsurfs < MAX_TEX_SURF)
	{
		sscanf(line, "%d %31s", &tex_surf[num_texsurfs].step_id, tex_surf[num_texsurfs].texture);
		num_texsurfs++;
	}

	FS_FreeFile(footstep_data);
}

// Plays appropriate footstep sound depending on surface flags of the ground surface.
// Since this is a replacement for plain Jane EV_FOOTSTEP, we already know the player is definitely on the ground when this is called.
static void CL_FootSteps(const entity_state_t *ent, const qboolean loud)
{
	vec3_t end;
	struct sfx_s *stepsound;

	VectorCopy(ent->origin, end);
	end[2] -= 64;
	trace_t tr = CL_PMSurfaceTrace(ent->number, ent->origin, NULL, NULL, end, MASK_SOLID | MASK_WATER);
	if (!tr.surface)
		return;

	const int surface = tr.surface->flags & SURF_STEPMASK;
	const int r = (rand() & 3);
	float volume = 0.5f;

	switch (surface)
	{
		case SURF_METAL:
			stepsound = clMedia.sfx_metal_footsteps[r];
			break;

		case SURF_DIRT:
			stepsound = clMedia.sfx_dirt_footsteps[r];
			break;

		case SURF_VENT:
			stepsound = clMedia.sfx_vent_footsteps[r];
			break;

		case SURF_GRATE:
			stepsound = clMedia.sfx_grate_footsteps[r];
			break;

		case SURF_TILE:
			stepsound = clMedia.sfx_tile_footsteps[r];
			break;

		case SURF_GRASS:
			stepsound = clMedia.sfx_grass_footsteps[r];
			break;

		case SURF_SNOW:
			stepsound = clMedia.sfx_snow_footsteps[r];
			break;

		case SURF_CARPET:
			stepsound = clMedia.sfx_carpet_footsteps[r];
			break;

		case SURF_FORCE:
			stepsound = clMedia.sfx_force_footsteps[r];
			break;

		case SURF_GRAVEL:
			stepsound = clMedia.sfx_gravel_footsteps[r];
			break;

		case SURF_ICE:
			stepsound = clMedia.sfx_ice_footsteps[r];
			break;

		case SURF_SAND:
			stepsound = clMedia.sfx_sand_footsteps[r];
			break;

		case SURF_WOOD:
			stepsound = clMedia.sfx_wood_footsteps[r];
			break;

		case SURF_STANDARD:
			stepsound = clMedia.sfx_footsteps[r];
			volume = 1.0f;
			break;

		default:
			if (cl_footstep_override->integer && num_texsurfs)
			{
				for (int i = 0; i < num_texsurfs; i++)
				{
					if (strstr(tr.surface->name, tex_surf[i].texture) && tex_surf[i].step_id > 0)
					{
						tr.surface->flags |= (SURF_METAL << (tex_surf[i].step_id - 1));
						CL_FootSteps(ent, loud); // Start over

						return;
					}
				}
			}

			tr.surface->flags |= SURF_STANDARD;
			CL_FootSteps(ent, loud); // Start over
			return;
	}

	if (loud)
	{
		if (volume == 1.0f)
			S_StartSound(NULL, ent->number, CHAN_AUTO, stepsound, 1.0f, ATTN_NORM, 0);
		else
			volume = 1.0f;
	}

	//mxd. Intentionally played twice when loud && volume == 1 for extra LOUDS.
	S_StartSound(NULL, ent->number, CHAN_BODY, stepsound, volume, ATTN_NORM, 0);
}
//end Knightmare

// An entity has just been parsed that has an event value
void CL_EntityEvent(const entity_state_t *ent)
{
	switch (ent->event)
	{
		case EV_ITEM_RESPAWN:
			S_StartSound(NULL, ent->number, CHAN_WEAPON, S_RegisterSound("items/respawn1.wav"), 1, ATTN_IDLE, 0);
			CL_ItemRespawnParticles(ent->origin);
			break;

		case EV_PLAYER_TELEPORT:
			S_StartSound(NULL, ent->number, CHAN_WEAPON, S_RegisterSound("misc/tele1.wav"), 1, ATTN_IDLE, 0);
			CL_TeleportParticles(ent->origin);
			break;

		case EV_FALLSHORT:
			S_StartSound(NULL, ent->number, CHAN_AUTO, S_RegisterSound("player/land1.wav"), 1, ATTN_NORM, 0);
			break;

		case EV_FALL:
			S_StartSound(NULL, ent->number, CHAN_AUTO, S_RegisterSound("*fall2.wav"), 1, ATTN_NORM, 0);
			break;

		case EV_FALLFAR:
			S_StartSound(NULL, ent->number, CHAN_AUTO, S_RegisterSound("*fall1.wav"), 1, ATTN_NORM, 0);
			break;

		//Knightmare- Lazarus footsteps
		case EV_FOOTSTEP:
		case EV_LOUDSTEP:
			if (cl_footsteps->integer)
				CL_FootSteps(ent, (const qboolean)(ent->event == EV_LOUDSTEP));
			break;

		case EV_SLOSH:
			S_StartSound(NULL, ent->number, CHAN_BODY, clMedia.sfx_slosh[rand() & 3], 0.5f, ATTN_NORM, 0);
			break;

		case EV_WADE:
			S_StartSound(NULL, ent->number, CHAN_BODY, clMedia.sfx_wade[rand() & 3], 0.5f, ATTN_NORM, 0);
			break;

		case EV_WADE_MUD:
			S_StartSound(NULL, ent->number, CHAN_BODY, clMedia.sfx_mud_wade[rand() & 1], 0.5f, ATTN_NORM, 0);
			break;

		case EV_CLIMB_LADDER:
			S_StartSound(NULL, ent->number, CHAN_BODY, clMedia.sfx_ladder[rand() & 3], 0.5f, ATTN_NORM, 0);
			break;
	}
}