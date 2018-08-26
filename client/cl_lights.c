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

void CL_LogoutEffect(vec3_t org, int type);
void CL_GunSmokeEffect(vec3_t org, vec3_t dir);

/*
==============================================================

LIGHT STYLE MANAGEMENT

==============================================================
*/

typedef struct
{
	int		length;
	float	value[3];
	float	map[MAX_QPATH];
} clightstyle_t;

clightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
int				lastofs;

/*
================
CL_ClearLightStyles
================
*/
void CL_ClearLightStyles(void)
{
	memset(cl_lightstyle, 0, sizeof(cl_lightstyle));
	lastofs = -1;
}

/*
================
CL_RunLightStyles
================
*/
void CL_RunLightStyles(void)
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
			ls->value[0] = ls->value[1] = ls->value[2] = 1.0;
			continue;
		}

		const int index = (ls->length == 1 ? 0 : ofs % ls->length);
		ls->value[0] = ls->value[1] = ls->value[2] = ls->map[index];
	}
}


void CL_SetLightstyle (int i)
{
	// Knightmare- BIG UGLY HACK for old connected to server using old protocol
	// Changed config strings require different parsing
	const int cs_lights = (LegacyProtocol() ? OLD_CS_LIGHTS : CS_LIGHTS);
	char *s = cl.configstrings[i + cs_lights];

	const int len = strlen(s);
	if (len >= MAX_QPATH) //mxd: never triggered, because cl.configstrings[][64]
		Com_Error(ERR_DROP, "svc_lightstyle length=%i", len);

	cl_lightstyle[i].length = len;

	for (int k = 0; k < len; k++)
		cl_lightstyle[i].map[k] = (float)(s[k] - 'a') / (float)('m' - 'a');
}

/*
================
CL_AddLightStyles
================
*/
void CL_AddLightStyles (void)
{
	clightstyle_t *ls = cl_lightstyle;
	for (int i = 0; i < MAX_LIGHTSTYLES; i++, ls++)
		V_AddLightStyle(i, ls->value[0], ls->value[1], ls->value[2]);
}


/*
==============================================================

DLIGHT MANAGEMENT

==============================================================
*/

cdlight_t cl_dlights[MAX_DLIGHTS];

/*
================
CL_ClearDlights
================
*/
void CL_ClearDlights (void)
{
	memset(cl_dlights, 0, sizeof(cl_dlights));
}

/*
===============
CL_AllocDlight
===============
*/
cdlight_t *CL_AllocDlight (int key)
{
	cdlight_t *dl;

	// first look for an exact key match
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

	// then look for anything else
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

/*
===============
CL_NewDlight
===============
*/
void CL_NewDlight (int key, float x, float y, float z, float radius, float time)
{
	cdlight_t *dl = CL_AllocDlight(key);
	VectorSet(dl->origin, x, y, z);
	dl->radius = radius;
	dl->die = cl.time + time;
}


/*
===============
CL_RunDLights
===============
*/
void CL_RunDLights (void)
{
	cdlight_t *dl = cl_dlights;
	for (int i = 0; i < MAX_DLIGHTS; i++, dl++)
	{
		if (!dl->radius)
			continue;
		
		if (dl->die < cl.time)
		{
			dl->radius = 0;
			return;
		}

		dl->radius -= cls.renderFrameTime * dl->decay;
		dl->radius = max(0, dl->radius);
	}
}

/*
===============
CL_AddDLights
===============
*/
void CL_AddDLights (void)
{
	cdlight_t *dl = cl_dlights;
	for (int i = 0; i < MAX_DLIGHTS; i++, dl++)
		if (dl->radius)
			V_AddLight(dl->origin, dl->radius, dl->color[0], dl->color[1], dl->color[2]);
}


/*
==============
CL_ParseMuzzleFlash
==============
*/
void CL_ParseMuzzleFlash (void)
{
	vec3_t	fv, rv;
	char	soundname[64];

	const int i = (unsigned short)MSG_ReadShort(&net_message); // Knightmare- make sure this doesn't turn negative!
	if (i < 1 || i >= MAX_EDICTS)
		Com_Error (ERR_DROP, "CL_ParseMuzzleFlash: bad entity");

	int weapon = MSG_ReadByte(&net_message);
	const int silenced = weapon & MZ_SILENCED;
	weapon &= ~MZ_SILENCED;

	centity_t *pl = &cl_entities[i];

	cdlight_t *dl = CL_AllocDlight(i);
	VectorCopy(pl->current.origin, dl->origin);
	AngleVectors(pl->current.angles, fv, rv, NULL);
	VectorMA(dl->origin, 18, fv, dl->origin);
	VectorMA(dl->origin, 16, rv, dl->origin);
	dl->radius = (rand() & 31) + (silenced ? 100 : 200); //mxd
	dl->minlight = 32;
	dl->die = cl.time; // + 0.1;

	const float volume = (silenced ? 0.2 : 1);

	switch (weapon)
	{
	case MZ_BLASTER:
		VectorSet(dl->color, 1, 1, 0.15); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_BLUEHYPERBLASTER:
		VectorSet(dl->color, 0.15, 0.15, 1); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_HYPERBLASTER:
		VectorSet(dl->color, 1, 1, 0.15); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_MACHINEGUN:
		VectorSet(dl->color, 1, 1, 0); //mxd
		Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0);
		break;

	case MZ_SHOTGUN:
		VectorSet(dl->color, 1, 1, 0); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/shotgf1b.wav"), volume, ATTN_NORM, 0);
		S_StartSound(NULL, i, CHAN_AUTO,   S_RegisterSound("weapons/shotgr1b.wav"), volume, ATTN_NORM, 0.1);
		break;

	case MZ_SSHOTGUN:
		VectorSet(dl->color, 1, 1, 0); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/sshotf1b.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_CHAINGUN1:
		dl->radius = 200 + (rand() & 31);
		VectorSet(dl->color, 1, 0.25, 0); //mxd
		Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0);
		break;

	case MZ_CHAINGUN2:
		dl->radius = 225 + (rand() & 31);
		VectorSet(dl->color, 1, 0.5, 0); //mxd
		dl->die = cl.time + 0.1;	// long delay
		Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0);
		Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0.05);
		break;

	case MZ_CHAINGUN3:
		dl->radius = 250 + (rand()&31);
		VectorSet(dl->color, 1, 1, 0); //mxd
		dl->die = cl.time + 0.1;	// long delay
		Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0);
		Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0.033);
		Com_sprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (rand() % 5) + 1);
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0.066);
		break;

	case MZ_RAILGUN:
		VectorSet(dl->color, 0.5, 0.5, 1); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/railgf1a.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_ROCKET:
		VectorSet(dl->color, 1, 0.5, 0.2); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/rocklf1a.wav"), volume, ATTN_NORM, 0);
		S_StartSound(NULL, i, CHAN_AUTO,   S_RegisterSound("weapons/rocklr1b.wav"), volume, ATTN_NORM, 0.1);
		break;

	case MZ_GRENADE:
		VectorSet(dl->color, 1, 0.5, 0); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/grenlf1a.wav"), volume, ATTN_NORM, 0);
		S_StartSound(NULL, i, CHAN_AUTO,   S_RegisterSound("weapons/grenlr1b.wav"), volume, ATTN_NORM, 0.1);
		break;

	case MZ_BFG:
		VectorSet(dl->color, 0, 1, 0); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/bfg__f1y.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_LOGIN:
		VectorSet(dl->color, 0, 1, 0); //mxd
		dl->die = cl.time + 1.0;
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("misc/tele1.wav"), 1, ATTN_NORM, 0); // Knightmare: "weapons/grenlf1a.wav" -> "misc/tele1.wav"
		CL_LogoutEffect(pl->current.origin, weapon);
		break;

	case MZ_LOGOUT:
		VectorSet(dl->color, 1, 0, 0); //mxd
		dl->die = cl.time + 1.0;
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("misc/tele1.wav"), 1, ATTN_NORM, 0); // Knightmare: "weapons/grenlf1a.wav" -> "misc/tele1.wav"
		CL_LogoutEffect(pl->current.origin, weapon);
		break;

	case MZ_RESPAWN:
		VectorSet(dl->color, 1, 1, 0); //mxd
		dl->die = cl.time + 1.0;
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
		CL_LogoutEffect(pl->current.origin, weapon);
		break;

	// RAFAEL
	case MZ_PHALANX:
		VectorSet(dl->color, 1, 0.5, 0.5); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/plasshot.wav"), volume, ATTN_NORM, 0);
		break;

	// RAFAEL
	case MZ_IONRIPPER:
		VectorSet(dl->color, 1, 0.5, 0.5); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/rippfire.wav"), volume, ATTN_NORM, 0);
		break;

// ======================
// PGM
	case MZ_ETF_RIFLE:
		VectorSet(dl->color, 0.9, 0.7, 0); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/nail1.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_SHOTGUN2:
		VectorSet(dl->color, 1, 1, 0); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/shotg2.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_HEATBEAM:
		VectorSet(dl->color, 1, 1, 0); //mxd
		dl->die = cl.time + 100;
		break;

	case MZ_BLASTER2:
		VectorSet(dl->color, 0.15, 1, 0.15); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/blastf1a.wav"), volume, ATTN_NORM, 0); // FIXME - different sound for blaster2 ??
		break;

	case MZ_TRACKER:
		// negative flashes handled the same in gl/soft until CL_AddDLights
		VectorSet(dl->color, -1, -1, -1); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/disint2.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_NUKE1:
		VectorSet(dl->color, 1, 0, 0); //mxd
		dl->die = cl.time + 100;
		break;

	case MZ_NUKE2:
		VectorSet(dl->color, 1, 1, 0); //mxd
		dl->die = cl.time + 100;
		break;

	case MZ_NUKE4:
		VectorSet(dl->color, 0, 0, 1); //mxd
		dl->die = cl.time + 100;
		break;

	case MZ_NUKE8:
		VectorSet(dl->color, 0, 1, 1); //mxd
		dl->die = cl.time + 100;
		break;

// PGM
// ======================
//Knightmare 1/3/2002- blue blaster and green hyperblaster and red blaster and hyperblaster
	case MZ_BLUEBLASTER:
		VectorSet(dl->color, 0.15, 0.15, 1); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_GREENHYPERBLASTER:
		VectorSet(dl->color, 0.15, 1, 0.15); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_REDBLASTER:
		VectorSet(dl->color, 1, 0.15, 0.15); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_REDHYPERBLASTER:
		VectorSet(dl->color, 1, 0.15, 0.15); //mxd
		S_StartSound(NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
		break;
//end Knightmare
	}
}


//mxd. Enhanced or classic particles
void CL_GunEffect(vec3_t origin)
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


/*
==============
CL_ParseMuzzleFlash2
==============
*/
void CL_ParseMuzzleFlash2 (void) 
{
	vec3_t	origin;
	vec3_t	forward, right;
	char	soundname[64];

	const int ent = (unsigned short)MSG_ReadShort(&net_message); // Knightmare- make sure this doesn't turn negative!
	if (ent < 1 || ent >= MAX_EDICTS)
		Com_Error(ERR_DROP, "CL_ParseMuzzleFlash2: bad entity");

	const int flash_number = MSG_ReadByte(&net_message);

	// locate the origin
	AngleVectors(cl_entities[ent].current.angles, forward, right, NULL);
	origin[0] = cl_entities[ent].current.origin[0] + forward[0] * monster_flash_offset[flash_number][0] + right[0] * monster_flash_offset[flash_number][1];
	origin[1] = cl_entities[ent].current.origin[1] + forward[1] * monster_flash_offset[flash_number][0] + right[1] * monster_flash_offset[flash_number][1];
	origin[2] = cl_entities[ent].current.origin[2] + forward[2] * monster_flash_offset[flash_number][0] + right[2] * monster_flash_offset[flash_number][1] + monster_flash_offset[flash_number][2];

	cdlight_t *dl = CL_AllocDlight(ent);
	VectorCopy (origin,  dl->origin);
	dl->radius = 200 + (rand() & 31);
	dl->minlight = 32;
	dl->die = cl.time;	// + 0.1;

	switch (flash_number)
	{
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
		VectorSet(dl->color, 1, 1, 0); //mxd
		CL_GunEffect(origin); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("infantry/infatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_SOLDIER_MACHINEGUN_1:
	case MZ2_SOLDIER_MACHINEGUN_2:
	case MZ2_SOLDIER_MACHINEGUN_3:
	case MZ2_SOLDIER_MACHINEGUN_4:
	case MZ2_SOLDIER_MACHINEGUN_5:
	case MZ2_SOLDIER_MACHINEGUN_6:
	case MZ2_SOLDIER_MACHINEGUN_7:
	case MZ2_SOLDIER_MACHINEGUN_8:
		VectorSet(dl->color, 1, 1, 0); //mxd
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
		VectorSet(dl->color, 1, 1, 0); //mxd
		CL_GunEffect(origin); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("gunner/gunatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_ACTOR_MACHINEGUN_1:
	case MZ2_SUPERTANK_MACHINEGUN_1:
	case MZ2_SUPERTANK_MACHINEGUN_2:
	case MZ2_SUPERTANK_MACHINEGUN_3:
	case MZ2_SUPERTANK_MACHINEGUN_4:
	case MZ2_SUPERTANK_MACHINEGUN_5:
	case MZ2_SUPERTANK_MACHINEGUN_6:
	case MZ2_TURRET_MACHINEGUN:			// PGM
		VectorSet(dl->color, 1, 1, 0); //mxd
		CL_GunEffect(origin); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("infantry/infatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_BOSS2_MACHINEGUN_L1:
	case MZ2_BOSS2_MACHINEGUN_L2:
	case MZ2_BOSS2_MACHINEGUN_L3:
	case MZ2_BOSS2_MACHINEGUN_L4:
	case MZ2_BOSS2_MACHINEGUN_L5:
	case MZ2_CARRIER_MACHINEGUN_L1:		// PMM
	case MZ2_CARRIER_MACHINEGUN_L2:		// PMM
		VectorSet(dl->color, 1, 1, 0); //mxd
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
	case MZ2_TURRET_BLASTER:			// PGM
		VectorSet(dl->color, 1, 1, 0); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("soldier/solatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_FLYER_BLASTER_1:
	case MZ2_FLYER_BLASTER_2:
		VectorSet(dl->color, 1, 1, 0); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("flyer/flyatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_MEDIC_BLASTER_1:
		VectorSet(dl->color, 1, 1, 0); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("medic/medatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_HOVER_BLASTER_1:
		VectorSet(dl->color, 1, 1, 0); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("hover/hovatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_FLOAT_BLASTER_1:
		VectorSet(dl->color, 1, 1, 0); //mxd
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
		VectorSet(dl->color, 1, 1, 0); //mxd
		CL_GunEffect(origin); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("soldier/solatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_BLASTER_1:
	case MZ2_TANK_BLASTER_2:
	case MZ2_TANK_BLASTER_3:
		VectorSet(dl->color, 1, 1, 0); //mxd
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
		VectorSet(dl->color, 1, 1, 0); //mxd
		CL_GunEffect(origin); //mxd
		Com_sprintf(soundname, sizeof(soundname), "tank/tnkatk2%c.wav", 'a' + rand() % 5);
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound(soundname), 1, ATTN_NORM, 0);
		break;

	case MZ2_CHICK_ROCKET_1:
	case MZ2_TURRET_ROCKET:			// PGM
		VectorSet(dl->color, 1, 0.5, 0.2); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("chick/chkatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_ROCKET_1:
	case MZ2_TANK_ROCKET_2:
	case MZ2_TANK_ROCKET_3:
		VectorSet(dl->color, 1, 0.5, 0.2); //mxd
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
//	case MZ2_CARRIER_ROCKET_2:
//	case MZ2_CARRIER_ROCKET_3:
//	case MZ2_CARRIER_ROCKET_4:
		VectorSet(dl->color, 1, 0.5, 0.2); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("tank/rocket.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_GUNNER_GRENADE_1:
	case MZ2_GUNNER_GRENADE_2:
	case MZ2_GUNNER_GRENADE_3:
	case MZ2_GUNNER_GRENADE_4:
		VectorSet(dl->color, 1, 0.5, 0); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("gunner/gunatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_GLADIATOR_RAILGUN_1:
	// PMM
	case MZ2_CARRIER_RAILGUN:
	case MZ2_WIDOW_RAIL:
	// pmm
		VectorSet(dl->color, 0.5, 0.5, 1); //mxd
		break;

// --- Xian's shit starts ---
	case MZ2_MAKRON_BFG:
		VectorSet(dl->color, 0.5, 1, 0.5); //mxd
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
		VectorSet(dl->color, 1, 1, 0); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("makron/blaster.wav"), 1, ATTN_NORM, 0);
		break;
	
	case MZ2_JORG_MACHINEGUN_L1:
	case MZ2_JORG_MACHINEGUN_L2:
	case MZ2_JORG_MACHINEGUN_L3:
	case MZ2_JORG_MACHINEGUN_L4:
	case MZ2_JORG_MACHINEGUN_L5:
	case MZ2_JORG_MACHINEGUN_L6:
		VectorSet(dl->color, 1, 1, 0); //mxd
		CL_GunEffect(origin); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("boss3/xfire.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_JORG_MACHINEGUN_R1:
	case MZ2_JORG_MACHINEGUN_R2:
	case MZ2_JORG_MACHINEGUN_R3:
	case MZ2_JORG_MACHINEGUN_R4:
	case MZ2_JORG_MACHINEGUN_R5:
	case MZ2_JORG_MACHINEGUN_R6:
		VectorSet(dl->color, 1, 1, 0); //mxd
		CL_GunEffect(origin); //mxd
		break;

	case MZ2_JORG_BFG_1:
		VectorSet(dl->color, 0.5, 1, 0.5); //mxd
		break;

	case MZ2_BOSS2_MACHINEGUN_R1:
	case MZ2_BOSS2_MACHINEGUN_R2:
	case MZ2_BOSS2_MACHINEGUN_R3:
	case MZ2_BOSS2_MACHINEGUN_R4:
	case MZ2_BOSS2_MACHINEGUN_R5:
	case MZ2_CARRIER_MACHINEGUN_R1:			// PMM
	case MZ2_CARRIER_MACHINEGUN_R2:			// PMM
		VectorSet(dl->color, 1, 1, 0); //mxd
		CL_GunEffect(origin); //mxd
		break;

// ======
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
		VectorSet(dl->color, 0, 1, 0); //mxd
		S_StartSound(NULL, ent, CHAN_WEAPON, S_RegisterSound("tank/tnkatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_WIDOW_DISRUPTOR:
		VectorSet(dl->color, -1, -1, -1); //mxd
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
		dl->radius = 300 + (rand() & 100);
		VectorSet(dl->color, 1, 1, 0); //mxd
		dl->die = cl.time + 200;
		break;
// ROGUE
// ======

// --- Xian's shit ends ---
	}
}