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

// cl_lights.c -- light style, dynamic light, and muzzle flash parsing and management
// moved from cl_fx.c

#include "client.h"

#pragma region ======================= Light style management

typedef struct
{
	int length;
	float value[3];
	float map[MAX_QPATH];
} clightstyle_t;

static clightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];
static int lastofs;

void CL_ClearLightStyles()
{
	memset(cl_lightstyle, 0, sizeof(cl_lightstyle));
	lastofs = -1;
}

void CL_RunLightStyles()
{
	const int ofs = cl.time / 100;
	if (ofs == lastofs)
		return;

	lastofs = ofs;

	clightstyle_t *ls = cl_lightstyle;
	for (int i = 0; i < MAX_LIGHTSTYLES; i++, ls++)
	{
		if (!ls->length)
		{
			VectorSetAll(ls->value, 1.0f);
			continue;
		}

		const int index = (ls->length == 1 ? 0 : ofs % ls->length);
		VectorSetAll(ls->value, ls->map[index]);
	}
}

void CL_SetLightstyle(const int i)
{
	// Knightmare- BIG UGLY HACK for old connected to server using old protocol
	// Changed config strings require different parsing
	const int cs_lights = (LegacyProtocol() ? OLD_CS_LIGHTS : CS_LIGHTS);
	char *s = cl.configstrings[i + cs_lights];

	const int len = strlen(s);
	cl_lightstyle[i].length = len;

	for (int k = 0; k < len; k++)
		cl_lightstyle[i].map[k] = (float)(s[k] - 'a') / (float)('m' - 'a');
}

void CL_AddLightStyles()
{
	clightstyle_t *ls = cl_lightstyle;
	for (int i = 0; i < MAX_LIGHTSTYLES; i++, ls++)
		V_AddLightStyle(i, ls->value[0], ls->value[1], ls->value[2]);
}

#pragma endregion

#pragma region ======================= DLight management

static cdlight_t cl_dlights[MAX_DLIGHTS];

void CL_ClearDlights()
{
	memset(cl_dlights, 0, sizeof(cl_dlights));
}

cdlight_t *CL_AllocDlight(const int key)
{
	cdlight_t *dl;

	// First look for an exact key match
	if (key)
	{
		dl = cl_dlights;
		for (int i = 0; i < MAX_DLIGHTS; i++, dl++)
		{
			if (dl->key == key)
			{
				memset(dl, 0, sizeof(*dl));
				dl->key = key;

				return dl;
			}
		}
	}

	// Then look for anything else
	dl = cl_dlights;
	for (int i = 0; i < MAX_DLIGHTS; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			memset(dl, 0, sizeof(*dl));
			dl->key = key;

			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset(dl, 0, sizeof(*dl));
	dl->key = key;

	return dl;
}

void CL_RunDLights()
{
	cdlight_t *dl = cl_dlights;
	for (int i = 0; i < MAX_DLIGHTS; i++, dl++)
	{
		if (!dl->radius)
			continue;
		
		if (dl->die < cl.time)
		{
			dl->radius = 0;
			continue; //mxd. Was return;
		}

		dl->radius -= cls.renderFrameTime * dl->decay;
		dl->radius = max(0, dl->radius);
	}
}

void CL_AddDLights()
{
	cdlight_t *dl = cl_dlights;
	for (int i = 0; i < MAX_DLIGHTS; i++, dl++)
		if (dl->radius)
			V_AddLight(dl->origin, dl->radius, dl->color[0], dl->color[1], dl->color[2]);
}

//mxd. Enhanced or classic particles
static void CL_GunEffect(const vec3_t origin)
{
	// Psychospaz's enhanced particle code
	if (r_particle_mode->integer == 1)
	{
		CL_GunSmokeEffect(origin, vec3_origin);
	}
	else // Classic particles
	{
		CL_ParticleEffect(origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash(origin);
	}
}

#pragma endregion

#pragma region ======================= CL_ParseMuzzleFlash

void CL_ParseMuzzleFlash()
{
	char soundname[64];

	const int i = (ushort)MSG_ReadShort(&net_message); // Knightmare- make sure this doesn't turn negative!
	if (i < 1 || i >= MAX_EDICTS)
		Com_Error(ERR_DROP, "CL_ParseMuzzleFlash: bad entity");

	int weapon = MSG_ReadByte(&net_message);
	const int silenced = weapon & MZ_SILENCED;
	weapon &= ~MZ_SILENCED;

	centity_t *pl = &cl_entities[i];

	cdlight_t *dl = CL_AllocDlight(i);
	VectorCopy(pl->current.origin, dl->origin);

	vec3_t forward, right;
	AngleVectors(pl->current.angles, forward, right, NULL);
	VectorMA(dl->origin, 18, forward, dl->origin);
	VectorMA(dl->origin, 16, right, dl->origin);

	dl->radius = (rand() & 31) + (silenced ? 100 : 200); //mxd
	dl->minlight = 32;
	dl->die = cl.time; // + 0.1;

	const float volume = (silenced ? 0.2f : 1.0f);

	switch (weapon)
	{
		case MZ_BLASTER:
			VectorSet(dl->color, 1.0f, 1.0f, 0.15f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
			break;

		case MZ_BLUEHYPERBLASTER:
			VectorSet(dl->color, 0.15f, 0.15f, 1.0f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
			break;

		case MZ_HYPERBLASTER:
			VectorSet(dl->color, 1.0f, 1.0f, 0.15f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
			break;

		case MZ_MACHINEGUN:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0);
			break;

		case MZ_SHOTGUN:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/shotgf1b.wav"), volume, ATTN_NORM, 0);
			S_StartSound(NULL, i, CHAN_AUTO,   S_RegisterSound("weapons/shotgr1b.wav"), volume, ATTN_NORM, 0.1);
			break;

		case MZ_SSHOTGUN:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/sshotf1b.wav"), volume, ATTN_NORM, 0);
			break;

		case MZ_CHAINGUN1:
			dl->radius = 200 + (rand() & 31);
			VectorSet(dl->color, 1.0f, 0.25f, 0.0f); //mxd
			Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0);
			break;

		case MZ_CHAINGUN2:
			dl->radius = 225 + (rand() & 31);
			VectorSet(dl->color, 1.0f, 0.5f, 0.0f); //mxd
			dl->die = cl.time + 0.1f; // Long delay
			Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0);
			Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0.05f);
			break;

		case MZ_CHAINGUN3:
			dl->radius = 250 + (rand() & 31);
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			dl->die = cl.time + 0.1f; // Long delay
			Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0);
			Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0.033f);
			Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0.066f);
			break;

		case MZ_RAILGUN:
			VectorSet(dl->color, 0.5f, 0.5f, 1.0f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/railgf1a.wav"), volume, ATTN_NORM, 0);
			break;

		case MZ_ROCKET:
			VectorSet(dl->color, 1.0f, 0.5f, 0.2f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/rocklf1a.wav"), volume, ATTN_NORM, 0);
			S_StartSound(NULL, i, CHAN_AUTO,   S_RegisterSound("weapons/rocklr1b.wav"), volume, ATTN_NORM, 0.1f);
			break;

		case MZ_GRENADE:
			VectorSet(dl->color, 1.0f, 0.5f, 0.0f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/grenlf1a.wav"), volume, ATTN_NORM, 0);
			S_StartSound(NULL, i, CHAN_AUTO,   S_RegisterSound("weapons/grenlr1b.wav"), volume, ATTN_NORM, 0.1f);
			break;

		case MZ_BFG:
			VectorSet(dl->color, 0.0f, 1.0f, 0.0f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/bfg__f1y.wav"), volume, ATTN_NORM, 0);
			break;

		case MZ_LOGIN:
			VectorSet(dl->color, 0.0f, 1.0f, 0.0f); //mxd
			dl->die = cl.time + 1;
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("misc/tele1.wav"), 1, ATTN_NORM, 0); // Knightmare: "weapons/grenlf1a.wav" -> "misc/tele1.wav"
			CL_LogoutEffect(pl->current.origin, weapon);
			break;

		case MZ_LOGOUT:
			VectorSet(dl->color, 1.0f, 0.0f, 0.0f); //mxd
			dl->die = cl.time + 1;
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("misc/tele1.wav"), 1, ATTN_NORM, 0); // Knightmare: "weapons/grenlf1a.wav" -> "misc/tele1.wav"
			CL_LogoutEffect(pl->current.origin, weapon);
			break;

		case MZ_RESPAWN:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			dl->die = cl.time + 1;
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
			CL_LogoutEffect(pl->current.origin, weapon);
			break;

		// RAFAEL
		case MZ_PHALANX:
			VectorSet(dl->color, 1.0f, 0.5f, 0.5f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/plasshot.wav"), volume, ATTN_NORM, 0);
			break;

		// RAFAEL
		case MZ_IONRIPPER:
			VectorSet(dl->color, 1.0f, 0.5f, 0.5f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/rippfire.wav"), volume, ATTN_NORM, 0);
			break;

		// PGM start
		case MZ_ETF_RIFLE:
			VectorSet(dl->color, 0.9f, 0.7f, 0.0f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/nail1.wav"), volume, ATTN_NORM, 0);
			break;

		case MZ_SHOTGUN2:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/shotg2.wav"), volume, ATTN_NORM, 0);
			break;

		case MZ_HEATBEAM:
		case MZ_NUKE2:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			dl->die = cl.time + 100;
			break;

		case MZ_BLASTER2:
			VectorSet(dl->color, 0.15f, 1.0f, 0.15f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/blastf1a.wav"), volume, ATTN_NORM, 0); // FIXME - different sound for blaster2 ??
			break;

		case MZ_TRACKER:
			// Negative flashes handled the same in gl/soft until CL_AddDLights
			VectorSet(dl->color, -1.0f, -1.0f, -1.0f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/disint2.wav"), volume, ATTN_NORM, 0);
			break;

		case MZ_NUKE1:
			VectorSet(dl->color, 1.0f, 0.0f, 0.0f); //mxd
			dl->die = cl.time + 100;
			break;

		case MZ_NUKE4:
			VectorSet(dl->color, 0.0f, 0.0f, 1.0f); //mxd
			dl->die = cl.time + 100;
			break;

		case MZ_NUKE8:
			VectorSet(dl->color, 0.0f, 1.0f, 1.0f); //mxd
			dl->die = cl.time + 100;
			break;
		// PGM end

		//Knightmare 1/3/2002- blue blaster and green hyperblaster and red blaster and hyperblaster
		case MZ_BLUEBLASTER:
			VectorSet(dl->color, 0.15f, 0.15f, 1.0f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
			break;

		case MZ_GREENHYPERBLASTER:
			VectorSet(dl->color, 0.15f, 1.0f, 0.15f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
			break;

		case MZ_REDBLASTER:
			VectorSet(dl->color, 1.0f, 0.15f, 0.15f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
			break;

		case MZ_REDHYPERBLASTER:
			VectorSet(dl->color, 1.0f, 0.15f, 0.15f); //mxd
			S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
			break;
		//end Knightmare
	}
}

#pragma endregion 

#pragma region ======================= CL_ParseMuzzleFlash2

void CL_ParseMuzzleFlash2()
{
	const int ent = (ushort)MSG_ReadShort(&net_message); // Knightmare- make sure this doesn't turn negative!
	if (ent < 1 || ent >= MAX_EDICTS)
		Com_Error(ERR_DROP, "CL_ParseMuzzleFlash2: bad entity");

	const int flash_number = MSG_ReadByte(&net_message);

	// Locate the origin
	vec3_t forward, right;
	AngleVectors(cl_entities[ent].current.angles, forward, right, NULL);

	vec3_t origin;
	origin[0] = cl_entities[ent].current.origin[0] + forward[0] * monster_flash_offset[flash_number][0] + right[0] * monster_flash_offset[flash_number][1];
	origin[1] = cl_entities[ent].current.origin[1] + forward[1] * monster_flash_offset[flash_number][0] + right[1] * monster_flash_offset[flash_number][1];
	origin[2] = cl_entities[ent].current.origin[2] + forward[2] * monster_flash_offset[flash_number][0] + right[2] * monster_flash_offset[flash_number][1] + monster_flash_offset[flash_number][2];

	cdlight_t *dl = CL_AllocDlight(ent);
	VectorCopy(origin,  dl->origin);
	dl->radius = 200.0f + (rand() & 31);
	dl->minlight = 32;
	dl->die = (float)cl.time; // + 0.1;

	switch (flash_number)
	{
		case MZ2_SOLDIER_MACHINEGUN_1:
		case MZ2_SOLDIER_MACHINEGUN_2:
		case MZ2_SOLDIER_MACHINEGUN_3:
		case MZ2_SOLDIER_MACHINEGUN_4:
		case MZ2_SOLDIER_MACHINEGUN_5:
		case MZ2_SOLDIER_MACHINEGUN_6:
		case MZ2_SOLDIER_MACHINEGUN_7:
		case MZ2_SOLDIER_MACHINEGUN_8:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			CL_GunEffect(origin); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("soldier/solatck3.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_GUNNER_MACHINEGUN_1:
		case MZ2_GUNNER_MACHINEGUN_2:
		case MZ2_GUNNER_MACHINEGUN_3:
		case MZ2_GUNNER_MACHINEGUN_4:
		case MZ2_GUNNER_MACHINEGUN_5:
		case MZ2_GUNNER_MACHINEGUN_6:
		case MZ2_GUNNER_MACHINEGUN_7:
		case MZ2_GUNNER_MACHINEGUN_8:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			CL_GunEffect(origin); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("gunner/gunatck2.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_INFANTRY_MACHINEGUN_1:
		case MZ2_INFANTRY_MACHINEGUN_2:
		case MZ2_INFANTRY_MACHINEGUN_3:
		case MZ2_INFANTRY_MACHINEGUN_4:
		case MZ2_INFANTRY_MACHINEGUN_5:
		case MZ2_INFANTRY_MACHINEGUN_6:
		case MZ2_INFANTRY_MACHINEGUN_7:
		case MZ2_INFANTRY_MACHINEGUN_8:
		case MZ2_INFANTRY_MACHINEGUN_9:
		case MZ2_INFANTRY_MACHINEGUN_10:
		case MZ2_INFANTRY_MACHINEGUN_11:
		case MZ2_INFANTRY_MACHINEGUN_12:
		case MZ2_INFANTRY_MACHINEGUN_13:
		case MZ2_ACTOR_MACHINEGUN_1:
		case MZ2_SUPERTANK_MACHINEGUN_1:
		case MZ2_SUPERTANK_MACHINEGUN_2:
		case MZ2_SUPERTANK_MACHINEGUN_3:
		case MZ2_SUPERTANK_MACHINEGUN_4:
		case MZ2_SUPERTANK_MACHINEGUN_5:
		case MZ2_SUPERTANK_MACHINEGUN_6:
		case MZ2_TURRET_MACHINEGUN: // PGM
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			CL_GunEffect(origin); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("infantry/infatck1.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_BOSS2_MACHINEGUN_L1:
		case MZ2_BOSS2_MACHINEGUN_L2:
		case MZ2_BOSS2_MACHINEGUN_L3:
		case MZ2_BOSS2_MACHINEGUN_L4:
		case MZ2_BOSS2_MACHINEGUN_L5:
		case MZ2_CARRIER_MACHINEGUN_L1: // PMM
		case MZ2_CARRIER_MACHINEGUN_L2: // PMM
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			CL_GunEffect(origin); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("infantry/infatck1.wav"), 1, ATTN_NONE, 0);
			break;

		case MZ2_SOLDIER_BLASTER_1:
		case MZ2_SOLDIER_BLASTER_2:
		case MZ2_SOLDIER_BLASTER_3:
		case MZ2_SOLDIER_BLASTER_4:
		case MZ2_SOLDIER_BLASTER_5:
		case MZ2_SOLDIER_BLASTER_6:
		case MZ2_SOLDIER_BLASTER_7:
		case MZ2_SOLDIER_BLASTER_8:
		case MZ2_TURRET_BLASTER: // PGM
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("soldier/solatck2.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_FLYER_BLASTER_1:
		case MZ2_FLYER_BLASTER_2:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("flyer/flyatck3.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_MEDIC_BLASTER_1:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("medic/medatck1.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_HOVER_BLASTER_1:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("hover/hovatck1.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_FLOAT_BLASTER_1:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("floater/fltatck1.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_SOLDIER_SHOTGUN_1:
		case MZ2_SOLDIER_SHOTGUN_2:
		case MZ2_SOLDIER_SHOTGUN_3:
		case MZ2_SOLDIER_SHOTGUN_4:
		case MZ2_SOLDIER_SHOTGUN_5:
		case MZ2_SOLDIER_SHOTGUN_6:
		case MZ2_SOLDIER_SHOTGUN_7:
		case MZ2_SOLDIER_SHOTGUN_8:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			CL_GunEffect(origin); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("soldier/solatck1.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_TANK_BLASTER_1:
		case MZ2_TANK_BLASTER_2:
		case MZ2_TANK_BLASTER_3:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("tank/tnkatck3.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_TANK_MACHINEGUN_1:
		case MZ2_TANK_MACHINEGUN_2:
		case MZ2_TANK_MACHINEGUN_3:
		case MZ2_TANK_MACHINEGUN_4:
		case MZ2_TANK_MACHINEGUN_5:
		case MZ2_TANK_MACHINEGUN_6:
		case MZ2_TANK_MACHINEGUN_7:
		case MZ2_TANK_MACHINEGUN_8:
		case MZ2_TANK_MACHINEGUN_9:
		case MZ2_TANK_MACHINEGUN_10:
		case MZ2_TANK_MACHINEGUN_11:
		case MZ2_TANK_MACHINEGUN_12:
		case MZ2_TANK_MACHINEGUN_13:
		case MZ2_TANK_MACHINEGUN_14:
		case MZ2_TANK_MACHINEGUN_15:
		case MZ2_TANK_MACHINEGUN_16:
		case MZ2_TANK_MACHINEGUN_17:
		case MZ2_TANK_MACHINEGUN_18:
		case MZ2_TANK_MACHINEGUN_19:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			CL_GunEffect(origin); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound(va("tank/tnkatk2%c.wav", 'a' + rand() % 5)), 1, ATTN_NORM, 0);
			break;

		case MZ2_CHICK_ROCKET_1:
		case MZ2_TURRET_ROCKET: // PGM
			VectorSet(dl->color, 1.0f, 0.5f, 0.2f); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("chick/chkatck2.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_TANK_ROCKET_1:
		case MZ2_TANK_ROCKET_2:
		case MZ2_TANK_ROCKET_3:
			VectorSet(dl->color, 1.0f, 0.5f, 0.2f); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("tank/tnkatck1.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_SUPERTANK_ROCKET_1:
		case MZ2_SUPERTANK_ROCKET_2:
		case MZ2_SUPERTANK_ROCKET_3:
		case MZ2_BOSS2_ROCKET_1:
		case MZ2_BOSS2_ROCKET_2:
		case MZ2_BOSS2_ROCKET_3:
		case MZ2_BOSS2_ROCKET_4:
		case MZ2_CARRIER_ROCKET_1:
			VectorSet(dl->color, 1.0f, 0.5f, 0.2f); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("tank/rocket.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_GUNNER_GRENADE_1:
		case MZ2_GUNNER_GRENADE_2:
		case MZ2_GUNNER_GRENADE_3:
		case MZ2_GUNNER_GRENADE_4:
			VectorSet(dl->color, 1.0f, 0.5f, 0.0f); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("gunner/gunatck3.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_GLADIATOR_RAILGUN_1:
		case MZ2_CARRIER_RAILGUN: // PMM
		case MZ2_WIDOW_RAIL: // PMM
			VectorSet(dl->color, 0.5f, 0.5f, 1.0f); //mxd
			break;

		// --- Xian's shit starts ---
		case MZ2_MAKRON_BFG:
		case MZ2_JORG_BFG_1:
			VectorSet(dl->color, 0.5f, 1.0f, 0.5f); //mxd
			break;

		case MZ2_MAKRON_BLASTER_1:
		case MZ2_MAKRON_BLASTER_2:
		case MZ2_MAKRON_BLASTER_3:
		case MZ2_MAKRON_BLASTER_4:
		case MZ2_MAKRON_BLASTER_5:
		case MZ2_MAKRON_BLASTER_6:
		case MZ2_MAKRON_BLASTER_7:
		case MZ2_MAKRON_BLASTER_8:
		case MZ2_MAKRON_BLASTER_9:
		case MZ2_MAKRON_BLASTER_10:
		case MZ2_MAKRON_BLASTER_11:
		case MZ2_MAKRON_BLASTER_12:
		case MZ2_MAKRON_BLASTER_13:
		case MZ2_MAKRON_BLASTER_14:
		case MZ2_MAKRON_BLASTER_15:
		case MZ2_MAKRON_BLASTER_16:
		case MZ2_MAKRON_BLASTER_17:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("makron/blaster.wav"), 1, ATTN_NORM, 0);
			break;
	
		case MZ2_JORG_MACHINEGUN_L1:
		case MZ2_JORG_MACHINEGUN_L2:
		case MZ2_JORG_MACHINEGUN_L3:
		case MZ2_JORG_MACHINEGUN_L4:
		case MZ2_JORG_MACHINEGUN_L5:
		case MZ2_JORG_MACHINEGUN_L6:
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			CL_GunEffect(origin); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("boss3/xfire.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_JORG_MACHINEGUN_R1:
		case MZ2_JORG_MACHINEGUN_R2:
		case MZ2_JORG_MACHINEGUN_R3:
		case MZ2_JORG_MACHINEGUN_R4:
		case MZ2_JORG_MACHINEGUN_R5:
		case MZ2_JORG_MACHINEGUN_R6:
		case MZ2_BOSS2_MACHINEGUN_R1:
		case MZ2_BOSS2_MACHINEGUN_R2:
		case MZ2_BOSS2_MACHINEGUN_R3:
		case MZ2_BOSS2_MACHINEGUN_R4:
		case MZ2_BOSS2_MACHINEGUN_R5:
		case MZ2_CARRIER_MACHINEGUN_R1: // PMM
		case MZ2_CARRIER_MACHINEGUN_R2: // PMM
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			CL_GunEffect(origin); //mxd
			break;
		// --- Xian's shit ends ---

		// ROGUE
		case MZ2_STALKER_BLASTER:
		case MZ2_DAEDALUS_BLASTER:
		case MZ2_MEDIC_BLASTER_2:
		case MZ2_WIDOW_BLASTER:
		case MZ2_WIDOW_BLASTER_SWEEP1:
		case MZ2_WIDOW_BLASTER_SWEEP2:
		case MZ2_WIDOW_BLASTER_SWEEP3:
		case MZ2_WIDOW_BLASTER_SWEEP4:
		case MZ2_WIDOW_BLASTER_SWEEP5:
		case MZ2_WIDOW_BLASTER_SWEEP6:
		case MZ2_WIDOW_BLASTER_SWEEP7:
		case MZ2_WIDOW_BLASTER_SWEEP8:
		case MZ2_WIDOW_BLASTER_SWEEP9:
		case MZ2_WIDOW_BLASTER_100:
		case MZ2_WIDOW_BLASTER_90:
		case MZ2_WIDOW_BLASTER_80:
		case MZ2_WIDOW_BLASTER_70:
		case MZ2_WIDOW_BLASTER_60:
		case MZ2_WIDOW_BLASTER_50:
		case MZ2_WIDOW_BLASTER_40:
		case MZ2_WIDOW_BLASTER_30:
		case MZ2_WIDOW_BLASTER_20:
		case MZ2_WIDOW_BLASTER_10:
		case MZ2_WIDOW_BLASTER_0:
		case MZ2_WIDOW_BLASTER_10L:
		case MZ2_WIDOW_BLASTER_20L:
		case MZ2_WIDOW_BLASTER_30L:
		case MZ2_WIDOW_BLASTER_40L:
		case MZ2_WIDOW_BLASTER_50L:
		case MZ2_WIDOW_BLASTER_60L:
		case MZ2_WIDOW_BLASTER_70L:
		case MZ2_WIDOW_RUN_1:
		case MZ2_WIDOW_RUN_2:
		case MZ2_WIDOW_RUN_3:
		case MZ2_WIDOW_RUN_4:
		case MZ2_WIDOW_RUN_5:
		case MZ2_WIDOW_RUN_6:
		case MZ2_WIDOW_RUN_7:
		case MZ2_WIDOW_RUN_8:
			VectorSet(dl->color, 0.0f, 1.0f, 0.0f); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("tank/tnkatck3.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_WIDOW_DISRUPTOR:
			VectorSet(dl->color, -1.0f, -1.0f, -1.0f); //mxd
			S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("weapons/disint2.wav"), 1, ATTN_NORM, 0);
			break;

		case MZ2_WIDOW_PLASMABEAM:
		case MZ2_WIDOW2_BEAMER_1:
		case MZ2_WIDOW2_BEAMER_2:
		case MZ2_WIDOW2_BEAMER_3:
		case MZ2_WIDOW2_BEAMER_4:
		case MZ2_WIDOW2_BEAMER_5:
		case MZ2_WIDOW2_BEAM_SWEEP_1:
		case MZ2_WIDOW2_BEAM_SWEEP_2:
		case MZ2_WIDOW2_BEAM_SWEEP_3:
		case MZ2_WIDOW2_BEAM_SWEEP_4:
		case MZ2_WIDOW2_BEAM_SWEEP_5:
		case MZ2_WIDOW2_BEAM_SWEEP_6:
		case MZ2_WIDOW2_BEAM_SWEEP_7:
		case MZ2_WIDOW2_BEAM_SWEEP_8:
		case MZ2_WIDOW2_BEAM_SWEEP_9:
		case MZ2_WIDOW2_BEAM_SWEEP_10:
		case MZ2_WIDOW2_BEAM_SWEEP_11:
			dl->radius = 300.0f + (rand() & 100);
			VectorSet(dl->color, 1.0f, 1.0f, 0.0f); //mxd
			dl->die = cl.time + 200.0f;
			break;
		// ROGUE end
	}
}

#pragma endregion