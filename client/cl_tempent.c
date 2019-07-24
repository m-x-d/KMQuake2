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
// cl_tempent.c -- client side temporary entities

#include "client.h"
#include "particles.h"

#define MAX_EXPLOSIONS	512 // Was 32
#define MAX_BEAMS		512 // Was 32
#define MAX_LASERS		512 // Was 32
#define MAX_SUSTAINS	32

typedef enum
{
	ex_free,
	ex_explosion,
	ex_misc,
	ex_flash,
	ex_mflash,
	ex_poly,
	ex_poly2
} exptype_t;

typedef struct
{
	exptype_t type;
	entity_t ent;

	int frames;
	float light;
	vec3_t lightcolor;
	float start;
	int baseframe;
} explosion_t;

static explosion_t cl_explosions[MAX_EXPLOSIONS];

typedef struct
{
	int entity;
	int dest_entity;
	struct model_s *model;
	int endtime;
	vec3_t offset;
	vec3_t start;
	vec3_t end;
} beam_t;

static beam_t cl_beams[MAX_BEAMS];
static beam_t cl_playerbeams[MAX_BEAMS]; //PMM - added this for player-linked beams. Currently only used by the plasma beam

typedef struct
{
	entity_t ent;
	int endtime;
} laser_t;
static laser_t cl_lasers[MAX_LASERS];

static cl_sustain_t cl_sustains[MAX_SUSTAINS]; //ROGUE

// Psychospaz's enhanced particle code
clientMedia_t clMedia;

extern void CL_Explosion_Particle(const vec3_t org, const float scale, const qboolean rocket);
extern void CL_Explosion_FlashParticle(const vec3_t org, const float size, const qboolean large);
extern void CL_BloodHit(const vec3_t org, const vec3_t dir);
extern void CL_GreenBloodHit(const vec3_t org, const vec3_t dir);
extern void CL_ParticleEffectSparks(const vec3_t org, const vec3_t dir, const vec3_t color, const int count);
extern void CL_ParticleBulletDecal(const vec3_t org, const vec3_t dir, const float size);
extern void CL_ParticlePlasmaBeamDecal(const vec3_t org, const vec3_t dir, const float size);
extern void CL_ParticleBlasterDecal(const vec3_t org, const vec3_t dir, const vec3_t color, const float size);
extern void CL_Explosion_Decal(vec3_t org, float size, int decalnum);
extern void CL_Explosion_Sparks(const vec3_t org, const int size, const int count);
extern void CL_BFGExplosionParticles(const vec3_t org);

extern void CL_ReadTextureSurfaceAssignments();

void CL_RegisterTEntSounds()
{
	clMedia.sfx_ric[0] = S_RegisterSound("world/ric1.wav");
	clMedia.sfx_ric[1] = S_RegisterSound("world/ric2.wav");
	clMedia.sfx_ric[2] = S_RegisterSound("world/ric3.wav");
	clMedia.sfx_lashit = S_RegisterSound("weapons/lashit.wav");
	clMedia.sfx_spark[0] = S_RegisterSound("world/spark5.wav");
	clMedia.sfx_spark[1] = S_RegisterSound("world/spark6.wav");
	clMedia.sfx_spark[2] = S_RegisterSound("world/spark7.wav");
	clMedia.sfx_railg = S_RegisterSound("weapons/railgf1a.wav");
	clMedia.sfx_rockexp = S_RegisterSound("weapons/rocklx1a.wav");
	clMedia.sfx_grenexp = S_RegisterSound("weapons/grenlx1a.wav");
	clMedia.sfx_watrexp = S_RegisterSound("weapons/xpld_wat.wav");
	
	// Xatrix
	clMedia.sfx_plasexp = S_RegisterSound("weapons/plasexpl.wav");
	
	// Rogue
	clMedia.sfx_lightning = S_RegisterSound("weapons/tesla.wav");
	clMedia.sfx_disrexp = S_RegisterSound("weapons/disrupthit.wav");
	
	// Shockwave impact
	clMedia.sfx_shockhit = S_RegisterSound("weapons/shockhit.wav");

	for (int i = 0; i < 4; i++)
	{
		clMedia.sfx_footsteps[i] = S_RegisterSound(va("player/step%i.wav", i + 1));

		// Lazarus footstep sounds
		clMedia.sfx_metal_footsteps[i]	= S_RegisterSound(va("player/pl_metal%i.wav", i + 1));
		clMedia.sfx_dirt_footsteps[i]	= S_RegisterSound(va("player/pl_dirt%i.wav", i + 1));
		clMedia.sfx_vent_footsteps[i]	= S_RegisterSound(va("player/pl_duct%i.wav", i + 1));
		clMedia.sfx_grate_footsteps[i]	= S_RegisterSound(va("player/pl_grate%i.wav", i + 1));
		clMedia.sfx_tile_footsteps[i]	= S_RegisterSound(va("player/pl_tile%i.wav", i + 1));
		clMedia.sfx_grass_footsteps[i]	= S_RegisterSound(va("player/pl_grass%i.wav", i + 1));
		clMedia.sfx_snow_footsteps[i]	= S_RegisterSound(va("player/pl_snow%i.wav", i + 1));
		clMedia.sfx_carpet_footsteps[i]	= S_RegisterSound(va("player/pl_carpet%i.wav", i + 1));
		clMedia.sfx_force_footsteps[i]	= S_RegisterSound(va("player/pl_force%i.wav", i + 1));
		clMedia.sfx_gravel_footsteps[i]	= S_RegisterSound(va("player/pl_gravel%i.wav", i + 1));
		clMedia.sfx_ice_footsteps[i]	= S_RegisterSound(va("player/pl_ice%i.wav", i + 1));
		clMedia.sfx_sand_footsteps[i]	= S_RegisterSound(va("player/pl_sand%i.wav", i + 1));
		clMedia.sfx_wood_footsteps[i]	= S_RegisterSound(va("player/pl_wood%i.wav", i + 1));
		clMedia.sfx_slosh[i]			= S_RegisterSound(va("player/pl_slosh%i.wav", i + 1));
		clMedia.sfx_wade[i]				= S_RegisterSound(va("player/pl_wade%i.wav", i + 1));
		clMedia.sfx_ladder[i]			= S_RegisterSound(va("player/pl_ladder%i.wav", i + 1));
	}

	for (int i = 0; i < 2; i++)
		clMedia.sfx_mud_wade[i] = S_RegisterSound(va("mud/wade_mud%i.wav", i + 1));

	// Read footstep defintion file
	if (cl_footstep_override->integer)
		CL_ReadTextureSurfaceAssignments();

	S_RegisterSound("player/land1.wav");
	S_RegisterSound("player/fall2.wav");
	S_RegisterSound("player/fall1.wav");
}	

void CL_RegisterTEntModels()
{
	clMedia.mod_explode = R_RegisterModel("models/objects/explode/tris.md2");
	clMedia.mod_smoke = R_RegisterModel("models/objects/smoke/tris.md2");
	clMedia.mod_flash = R_RegisterModel("models/objects/flash/tris.md2");
	clMedia.mod_parasite_segment = R_RegisterModel("models/monsters/parasite/segment/tris.md2");
	clMedia.mod_grapple_cable = R_RegisterModel("models/ctf/segment/tris.md2");
	clMedia.mod_parasite_tip = R_RegisterModel("models/monsters/parasite/tip/tris.md2");
	clMedia.mod_explo = R_RegisterModel("models/objects/r_explode/tris.md2");
	clMedia.mod_bfg_explo = R_RegisterModel("sprites/s_bfg2.sp2");
	clMedia.mod_powerscreen = R_RegisterModel("models/items/armor/effect/tris.md2");

	// Rogue
	clMedia.mod_explo_big = R_RegisterModel("models/objects/r_explode2/tris.md2");
	clMedia.mod_lightning = R_RegisterModel("models/proj/lightning/tris.md2");
	clMedia.mod_heatbeam = R_RegisterModel("models/proj/beam/tris.md2");
	clMedia.mod_monster_heatbeam = R_RegisterModel("models/proj/widowbeam/tris.md2");

	// New effect models
	clMedia.mod_shocksplash = R_RegisterModel("models/objects/shocksplash/tris.md2");

	R_RegisterModel("models/objects/laser/tris.md2");
	R_RegisterModel("models/objects/grenade2/tris.md2");
	R_RegisterModel("models/weapons/v_machn/tris.md2");
	R_RegisterModel("models/weapons/v_handgr/tris.md2");
	R_RegisterModel("models/weapons/v_shotg2/tris.md2");
	R_RegisterModel("models/objects/gibs/bone/tris.md2");
	R_RegisterModel("models/objects/gibs/sm_meat/tris.md2");
	R_RegisterModel("models/objects/gibs/bone2/tris.md2");

	R_DrawFindPic("w_machinegun");
	R_DrawFindPic("a_bullets");
	R_DrawFindPic("i_health");
	R_DrawFindPic("a_grenades");
}	

void CL_ClearTEnts()
{
	memset(cl_beams, 0, sizeof(cl_beams));
	memset(cl_explosions, 0, sizeof(cl_explosions));
	memset(cl_lasers, 0, sizeof(cl_lasers));

	//ROGUE
	memset(cl_playerbeams, 0, sizeof(cl_playerbeams));
	memset(cl_sustains, 0, sizeof(cl_sustains));
}

//mxd
static beam_t *CL_FindFreeBeam2(const int srcent, const int dstent, beam_t *beams)
{
	beam_t *b;
	int i;

	// Override any beam with the same entity(s)
	for (i = 0, b = beams; i < MAX_BEAMS; i++, b++)
		if (b->entity == srcent && (dstent == -1 || b->dest_entity == dstent))
			return b;

	// Find a free beam
	for (i = 0, b = beams; i < MAX_BEAMS; i++, b++)
		if (!b->model || b->endtime < cl.time)
			return b;

	Com_Printf("Beam list overflow!\n");
	return NULL;
}

//mxd
static beam_t *CL_FindFreeBeam(const int ent, beam_t *beams)
{
	return CL_FindFreeBeam2(ent, -1, beams);
}

//mxd
static cl_sustain_t *CL_FindFreeSustain()
{
	cl_sustain_t *s = cl_sustains;
	for (int i = 0; i < MAX_SUSTAINS; i++, s++)
		if (s->id == 0)
			return s;

	return NULL;
}

static explosion_t *CL_AllocExplosion()
{
	for (int i = 0; i < MAX_EXPLOSIONS; i++)
	{
		if (cl_explosions[i].type == ex_free)
		{
			memset(&cl_explosions[i], 0, sizeof (cl_explosions[i]));
			return &cl_explosions[i];
		}
	}

	// Find the oldest explosion
	int time = cl.time;
	int index = 0;

	for (int i = 0; i < MAX_EXPLOSIONS; i++)
	{
		if (cl_explosions[i].start < time)
		{
			time = cl_explosions[i].start;
			index = i;
		}
	}

	memset(&cl_explosions[index], 0, sizeof (cl_explosions[index]));
	return &cl_explosions[index];
}

static void CL_Explosion_Flash(vec3_t origin, const int dist, const int size, const qboolean plasma)
{
	if (cl_particle_scale->value > 1 || plasma)
	{
		const int limit = (plasma ? 1 : 2);
		for (int i = 0; i < limit; i++)
			CL_Explosion_FlashParticle(origin, size + 2 * dist, true);

		return;
	}

	vec3_t org;
	for (int i = -1; i < 2; i += 2)
	{
		for (int j = -1; j < 2; j += 2)
		{
			for (int k = -1; k < 2; k += 2)
			{
				org[0] = origin[0] + i * dist;
				org[1] = origin[1] + j * dist;
				org[2] = origin[2] + k * dist;
				CL_Explosion_FlashParticle(org, size, false);
			}
		}
	}
}

void CL_SmokeAndFlash(const vec3_t origin)
{
	explosion_t *ex;
	
	//mxd. Classic particles
	if (r_particle_mode->integer == 0)
	{
		ex = CL_AllocExplosion();
		VectorCopy(origin, ex->ent.origin);
		ex->type = ex_misc;
		ex->frames = 4;
		ex->ent.flags = RF_TRANSLUCENT;
		ex->start = cl.frame.servertime - 100;
		ex->ent.model = clMedia.mod_smoke;
	}

	ex = CL_AllocExplosion();
	VectorCopy(origin, ex->ent.origin);
	ex->type = ex_flash;
	ex->ent.flags |= RF_FULLBRIGHT;
	ex->frames = 2;
	ex->start = cl.frame.servertime - 100;
	ex->ent.model = clMedia.mod_flash;
}

static int CL_ParseBeam(struct model_s *model)
{
	const int ent = MSG_ReadShort(&net_message);
	
	vec3_t start, end;
	MSG_ReadPos(&net_message, start);
	MSG_ReadPos(&net_message, end);

	//mxd
	beam_t *b = CL_FindFreeBeam(ent, cl_beams);
	if(b != NULL)
	{
		b->entity = ent;
		b->model = model;
		b->endtime = cl.time + 200;
		VectorCopy(start, b->start);
		VectorCopy(end, b->end);
		VectorClear(b->offset);
	}

	return ent;
}

static int CL_ParseBeam2(struct model_s *model)
{
	const int ent = MSG_ReadShort(&net_message);

	vec3_t start, end, offset;
	MSG_ReadPos(&net_message, start);
	MSG_ReadPos(&net_message, end);
	MSG_ReadPos(&net_message, offset);

	//mxd
	beam_t *b = CL_FindFreeBeam(ent, cl_beams);
	if (b != NULL)
	{
		b->entity = ent;
		b->model = model;
		b->endtime = cl.time + 200;
		VectorCopy(start, b->start);
		VectorCopy(end, b->end);
		VectorCopy(offset, b->offset);
	}

	return ent;
}

//ROGUE. Adds to the cl_playerbeam array instead of the cl_beams array
static int CL_ParsePlayerBeam(struct model_s *model)
{
	const int ent = MSG_ReadShort(&net_message);
	
	vec3_t start, end, offset;
	MSG_ReadPos(&net_message, start);
	MSG_ReadPos(&net_message, end);
	
	// PMM - network optimization
	if (model == clMedia.mod_heatbeam)
	{
		VectorSet(offset, 2, 7, -3);
	}
	else if (model == clMedia.mod_monster_heatbeam)
	{
		model = clMedia.mod_heatbeam;
		VectorSet(offset, 0, 0, 0);
	}
	else
	{
		MSG_ReadPos(&net_message, offset);
	}

	//mxd
	beam_t *b = CL_FindFreeBeam(ent, cl_playerbeams);
	if(b != NULL)
	{
		b->model = model;
		b->endtime = cl.time + (b->entity == ent ? 200 : 100); // PMM - this needs to be 100 to prevent multiple heatbeams
		b->entity = ent;
		VectorCopy(start, b->start);
		VectorCopy(end, b->end);
		VectorCopy(offset, b->offset);
	}

	return ent;
}

extern void CL_LightningBeam(const vec3_t start, const vec3_t end, const int srcent, const int dstent, const float size);

// Psychspaz's enhanced particles
static int CL_ParseLightning()
{
	const int srcEnt = MSG_ReadShort(&net_message); // len
	const int dstEnt = MSG_ReadShort(&net_message); // dec

	vec3_t start, end;
	MSG_ReadPos(&net_message, start);
	MSG_ReadPos(&net_message, end);

	// Psychospaz's enhanced particle code
	if (r_particle_mode->integer == 1)
	{
		CL_LightningBeam(start, end, srcEnt, dstEnt, 5);
	}
	else // mxd. Classic beam
	{
		beam_t *b = CL_FindFreeBeam2(srcEnt, dstEnt, cl_beams);
		if(b != NULL)
		{
			b->entity = srcEnt;
			b->dest_entity = dstEnt;
			b->model = clMedia.mod_lightning;
			b->endtime = cl.time + 200;
			VectorCopy(start, b->start);
			VectorCopy(end, b->end);
			VectorClear(b->offset);
		}
	}
		
	return srcEnt;
}

// Psychspaz's enhanced particles
static void CL_ParseLaser(const int colors)
{
	vec3_t start, end;
	MSG_ReadPos(&net_message, start);
	MSG_ReadPos(&net_message, end);

	if(r_particle_mode->integer == 1)
	{
		vec3_t vec;
		VectorSubtract(end, start, vec);
		const float len = VectorNormalize(vec);
		
		vec3_t point;
		VectorCopy(vec, point);

		const int dec = 20;
		VectorScale(vec, dec, vec);

		vec3_t move;
		VectorCopy(start, move);

		for (int i = 0; i < len; i += dec)
		{
			cparticle_t *p = CL_InitParticle();
			if (!p)
				return;

			VectorCopy(point, p->angle);
			VectorCopy(move, p->org);
			VectorSet(p->color, 50, 255, 50);
			p->alphavel = -2.5f;
			p->size = dec * TWOTHIRDS;
			p->flags = PART_DIRECTION;
			p->type = particle_beam;

			CL_FinishParticleInit(p);

			VectorAdd(move, vec, move);
		}
	}
	else //mxd. Classic LAZORS!
	{
		laser_t *l = cl_lasers;
		for (int i = 0; i < MAX_LASERS; i++, l++)
		{
			if (l->endtime < cl.time)
			{
				l->ent.flags = RF_TRANSLUCENT | RF_BEAM;
				VectorCopy(start, l->ent.origin);
				VectorCopy(end, l->ent.oldorigin);
				l->ent.alpha = 0.3;
				l->ent.skinnum = (colors >> ((rand() % 4) * 8)) & 0xff;
				l->ent.model = NULL;
				l->ent.frame = 4;
				l->endtime = cl.time + 100;

				return;
			}
		}
	}
}

//ROGUE
static void CL_ParseSteam()
{
	vec3_t pos, dir;

	const int id = MSG_ReadShort(&net_message); // An id of -1 is an instant effect
	if (id != -1) // Sustains
	{
		cl_sustain_t *s = CL_FindFreeSustain();

		if (s)
		{
			s->id = id;
			s->count = MSG_ReadByte(&net_message);
			MSG_ReadPos(&net_message, s->org);
			MSG_ReadDir(&net_message, s->dir);
			const int r = MSG_ReadByte(&net_message);
			s->color = r & 0xff;
			s->magnitude = MSG_ReadShort(&net_message);
			s->endtime = cl.time + MSG_ReadLong(&net_message);
			s->think = CL_ParticleSteamEffect2;
			s->thinkinterval = 100;
			s->nextthink = cl.time;
		}
		else
		{
			// Read the stuff anyway...
			MSG_ReadByte(&net_message);
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			MSG_ReadByte(&net_message);
			MSG_ReadShort(&net_message);
			MSG_ReadLong(&net_message); // Interval
		}
	}
	else // Instant
	{
		const int cnt = MSG_ReadByte(&net_message);
		MSG_ReadPos(&net_message, pos);
		MSG_ReadDir(&net_message, dir);
		const int r = MSG_ReadByte(&net_message);
		const int magnitude = MSG_ReadShort(&net_message);

		vec3_t color;
		color8_to_vec3(r & 0xff, color);
		CL_ParticleSteamEffect(pos, dir, color, tv(-20, -20, -20), cnt, magnitude);
	}
}

static void CL_ParseWidow()
{
	const int id = MSG_ReadShort(&net_message);
	
	vec3_t pos;
	MSG_ReadPos(&net_message, pos);

	cl_sustain_t *s = CL_FindFreeSustain(); //mxd
	if (s)
	{
		s->id = id;
		VectorCopy(pos, s->org);
		s->endtime = cl.time + 2100;
		s->thinkinterval = 1;
		s->think = CL_Widowbeamout;
		s->nextthink = cl.time;
	}
}

static void CL_ParseNuke()
{
	vec3_t pos;
	MSG_ReadPos(&net_message, pos);

	cl_sustain_t *s = CL_FindFreeSustain(); //mxd
	if (s)
	{
		s->id = 21000;
		VectorCopy(pos, s->org);
		s->endtime = cl.time + 1000;
		s->thinkinterval = 1;
		s->think = CL_Nukeblast;
		s->nextthink = cl.time;
	}
}

// Psychospaz's enhanced particle code
void CL_GunSmokeEffect(const vec3_t org, const vec3_t dir)
{
	vec3_t velocity, origin;
	for (int j = 0; j < 3; j++)
	{
		origin[j] = org[j] + dir[j] * 10;
		velocity[j] = dir[j] * 10;
	}

	velocity[2] = 10;

	CL_ParticleSmokeEffect(origin, velocity, 10);
}

//mxd
static void CL_AllocClassicExplosion(const vec3_t pos, const qboolean grenade, const qboolean big)
{
	explosion_t	*ex = CL_AllocExplosion();
	VectorCopy(pos, ex->ent.origin);
	VectorSet(ex->lightcolor, 1.0f, 0.5f, 0.5f);
	ex->type = ex_poly;
	ex->ent.flags = RF_FULLBRIGHT | RF_NOSHADOW; // noshadow flag
	ex->start = cl.frame.servertime - 100;
	ex->light = 350;
	ex->ent.angles[1] = rand() % 360;
	ex->ent.model = (big ? clMedia.mod_explo_big : clMedia.mod_explo);

	if (grenade)
	{
		ex->frames = 19;
		ex->baseframe = 30;
	}
	else
	{
		if (frand() < 0.5f) 
			ex->baseframe = 15;
		ex->frames = 15;
	}
}

#pragma region ======================= CL_ParseTEnt

void CL_ParseTEnt()
{
	static byte splash_color[] = { 0x00, 0xe0, 0xb0, 0x50, 0xd0, 0xe0, 0xe8 }; //mxd. Made local
	
	vec3_t pos, pos2, dir;
	const int type = MSG_ReadByte(&net_message);

	switch (type)
	{
		// Bullet hitting flesh
		case TE_BLOOD:
		{
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1)
				CL_BloodHit(pos, dir);
			else //mxd. Classic particles
				CL_ParticleEffect(pos, dir, 0xe8, 60);
		} break;

		// Bullet hitting wall
		case TE_GUNSHOT:
		case TE_SPARKS:
		case TE_BULLET_SPARKS:
		{
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1)
			{
				if (type == TE_BULLET_SPARKS)
					VectorScale(dir, 2, dir);

				vec3_t c = { 255, 125, 10 };
				CL_ParticleEffectSparks(pos, dir, c, (type == TE_GUNSHOT) ? 5 : 10);
			}
			else //mxd. Classic particles
			{
				if (type == TE_GUNSHOT)
					CL_ParticleEffect(pos, dir, 0, 40);
				else
					CL_ParticleEffect(pos, dir, 0xe0, 6);
			}

			if (type != TE_SPARKS)
			{
				// Psychospaz's enhanced particle code
				if(r_particle_mode->integer == 1)
					CL_GunSmokeEffect(pos, dir);
				else //mxd. Classic particles
					CL_SmokeAndFlash(pos);

				CL_ParticleBulletDecal(pos, dir, 2.5f);

				// Impact sound
				const int cnt = rand() & 15;
				if (cnt > 0 && cnt < 4) //mxd
					S_StartSound(pos, 0, 0, clMedia.sfx_ric[cnt - 1], 1, ATTN_NORM, 0);
			}
		} break;

		// Bullet hitting wall
		case TE_SHOTGUN:
		{
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);

			// Psychospaz's enhanced particle code
			if(r_particle_mode->integer == 1)
			{
				CL_GunSmokeEffect(pos, dir);
				vec3_t c = { 200, 100, 10 };
				CL_ParticleEffectSparks(pos, dir, c, 8);
			}
			else //mxd. Classic particles
			{
				CL_ParticleEffect(pos, dir, 0, 20);
				CL_SmokeAndFlash(pos);
			}

			CL_ParticleBulletDecal(pos, dir, 2.8f);
		} break;

		case TE_SCREEN_SPARKS:
		case TE_SHIELD_SPARKS:
		{
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);

			CL_ParticleEffect(pos, dir, (type == TE_SCREEN_SPARKS ? 0xd0 : 0xb0), 40);

			//FIXME: replace or remove this sound
			S_StartSound(pos, 0, 0, clMedia.sfx_lashit, 1, ATTN_NORM, 0);
		} break;

		// Bullet hitting water
		case TE_SPLASH:
		{
			int cnt = MSG_ReadByte(&net_message);
			cnt = min(cnt, 40); // Cap at 40
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			int r = MSG_ReadByte(&net_message);

			const int color = (r > 6 ? 0x00 : splash_color[r]); //mxd

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1)
				CL_ParticleEffectSplash(pos, dir, color, cnt);
			else //mxd. Classic particles
				CL_ParticleEffect(pos, dir, color, cnt);

			if (r == SPLASH_SPARKS)
			{
				r = rand() & 3;
				if (r < 3)
					S_StartSound(pos, 0, 0, clMedia.sfx_spark[r], 1, ATTN_STATIC, 0);
			}
		} break;

		case TE_LASER_SPARKS:
		{
			const int cnt = MSG_ReadByte(&net_message);
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			const int color = MSG_ReadByte(&net_message);
			CL_ParticleEffect2(pos, dir, color, cnt, false);
		} break;

		// Railgun / red railgun effect
		case TE_RAILTRAIL:
		case TE_RAILTRAIL2:
		{	
			MSG_ReadPos(&net_message, pos);
			MSG_ReadPos(&net_message, pos2);
			CL_RailTrail(pos, pos2, type == TE_RAILTRAIL2); //mxd
			S_StartSound(pos2, 0, 0, clMedia.sfx_railg, 1, ATTN_NORM, 0);
		} break;

		case TE_EXPLOSION2:
		case TE_GRENADE_EXPLOSION:
		case TE_GRENADE_EXPLOSION_WATER:
		{
			MSG_ReadPos(&net_message, pos);
			if (cl_old_explosions->integer)
			{
				CL_AllocClassicExplosion(pos, true, false); //mxd
			}
			else
			{
				CL_Explosion_Particle(pos, 0, false);
				if (type != TE_GRENADE_EXPLOSION_WATER)
					CL_Explosion_Flash(pos, 10, 50, false);
			}

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1)
				CL_Explosion_Sparks(pos, 16, 128);
			else //mxd. Classic particles
				CL_ExplosionParticles(pos);

			if (type != TE_EXPLOSION2)
				CL_Explosion_Decal(pos, 50, particle_burnmark);

			struct sfx_s *sfx = (type == TE_GRENADE_EXPLOSION_WATER ? clMedia.sfx_watrexp : clMedia.sfx_grenexp); //mxd
			S_StartSound(pos, 0, 0, sfx, 1, ATTN_NORM, 0);
		} break;

		// RAFAEL
		case TE_PLASMA_EXPLOSION:
		{
			MSG_ReadPos(&net_message, pos);
			if (cl_old_explosions->integer)
			{
				CL_AllocClassicExplosion(pos, false, false); //mxd
			}
			else
			{
				CL_Explosion_Particle(pos, 0, true);
				CL_Explosion_Flash(pos, 10, 50, true);
			}

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1)
				CL_Explosion_Sparks(pos, 16, 128);
			else //mxd. Classic particles
				CL_ExplosionParticles(pos);

			CL_Explosion_Decal(pos, 50, particle_burnmark);

			struct sfx_s *sfx = (cl_plasma_explo_sound->value ? clMedia.sfx_plasexp : clMedia.sfx_rockexp); //mxd
			S_StartSound(pos, 0, 0, sfx, 1, ATTN_NORM, 0);
		} break;

		// PMM
		case TE_EXPLOSION1_BIG:
		{
			MSG_ReadPos(&net_message, pos);
			if (cl_old_explosions->integer)
			{
				CL_AllocClassicExplosion(pos, false, true); //mxd
			}
			else
			{
				CL_Explosion_Particle(pos, 150, true); // increased size
				CL_Explosion_Flash(pos, 30, 150, false);
			}

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1) //mxd
				CL_Explosion_Sparks(pos, 48, 128);

			S_StartSound(pos, 0, 0, clMedia.sfx_rockexp, 1, ATTN_NORM, 0);
		} break;

		// PMM
		case TE_EXPLOSION1_NP:
		{
			MSG_ReadPos(&net_message, pos);
			if (cl_old_explosions->integer)
			{
				CL_AllocClassicExplosion(pos, false, true); //mxd
			}
			else
			{
				CL_Explosion_Particle(pos, 50, true);
				CL_Explosion_Flash(pos, 7, 32, false);
			}

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1) //mxd
				CL_Explosion_Sparks(pos, 10, 128);

			S_StartSound(pos, 0, 0, clMedia.sfx_grenexp, 1, ATTN_NORM, 0);
		} break;

		case TE_EXPLOSION1:
		case TE_PLAIN_EXPLOSION:
		case TE_ROCKET_EXPLOSION:
		case TE_ROCKET_EXPLOSION_WATER:
		{
			MSG_ReadPos(&net_message, pos);
			if (cl_old_explosions->integer)
			{
				CL_AllocClassicExplosion(pos, false, false); //mxd
			}
			else
			{
				CL_Explosion_Particle(pos, 0, true);
				CL_Explosion_Flash(pos, 10, 50, false);
			}

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1)
				CL_Explosion_Sparks(pos, 16, 128);
			else //mxd. Classic particles
				CL_ExplosionParticles(pos);

			if (type == TE_ROCKET_EXPLOSION || type == TE_ROCKET_EXPLOSION_WATER)
				CL_Explosion_Decal(pos, 50, particle_burnmark);

			struct sfx_s *sfx = (type == TE_ROCKET_EXPLOSION_WATER ? clMedia.sfx_watrexp : clMedia.sfx_rockexp); //mxd
			S_StartSound(pos, 0, 0, sfx, 1, ATTN_NORM, 0);
		} break;

		case TE_BFG_EXPLOSION:
		{
			MSG_ReadPos(&net_message, pos);
			explosion_t	*ex = CL_AllocExplosion();
			VectorCopy(pos, ex->ent.origin);
			ex->type = ex_poly;
			ex->ent.flags |= RF_FULLBRIGHT;
			ex->start = cl.frame.servertime - 100;
			ex->light = 350;
			VectorSet(ex->lightcolor, 0.0f, 1.0f, 0.0f);
			ex->ent.model = clMedia.mod_bfg_explo;
			ex->ent.flags |= (RF_TRANSLUCENT | RF_TRANS_ADDITIVE);
			ex->ent.alpha = 0.3f;
			ex->frames = 4;
		} break;

		case TE_BFG_BIGEXPLOSION:
			MSG_ReadPos(&net_message, pos);
			CL_BFGExplosionParticles(pos);
			CL_Explosion_Decal(pos, 75, particle_bfgmark);
			break;

		case TE_BFG_LASER:
			CL_ParseLaser(0xd0d1d2d3);
			break;

		case TE_BUBBLETRAIL:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadPos(&net_message, pos2);
			CL_BubbleTrail(pos, pos2);
			break;

		case TE_PARASITE_ATTACK:
		case TE_MEDIC_CABLE_ATTACK:
			CL_ParseBeam(clMedia.mod_parasite_segment);
			break;

		// Boss teleporting to station
		case TE_BOSSTPORT:
			MSG_ReadPos(&net_message, pos);
			CL_BigTeleportParticles(pos);
			S_StartSound(pos, 0, 0, S_RegisterSound("misc/bigtele.wav"), 1, ATTN_NONE, 0);
			break;

		case TE_GRAPPLE_CABLE:
			CL_ParseBeam2(clMedia.mod_grapple_cable);
			break;

		// RAFAEL
		case TE_WELDING_SPARKS:
		{
			const int cnt = MSG_ReadByte(&net_message);
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			const int color = MSG_ReadByte(&net_message);

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1)
			{
				vec3_t sparkcolor = { color8red(color), color8green(color), color8blue(color) };
				CL_ParticleEffectSparks(pos, dir, sparkcolor, cnt); //mxd. 40 -> cnt (32 by default)
			}
			else //mxd. Classic particles
			{
				CL_ParticleEffect2(pos, dir, color, cnt, false);
			}

			explosion_t	*ex = CL_AllocExplosion();
			VectorCopy(pos, ex->ent.origin);
			ex->type = ex_flash;
			//TODO: note to self: we need a better no draw flag
			ex->ent.flags |= RF_BEAM;
			ex->start = cl.frame.servertime - 0.1f;
			ex->light = 100 + (rand() % 75);
			VectorSet(ex->lightcolor, 1.0f, 1.0f, 0.3f);
			ex->ent.model = clMedia.mod_flash;
			ex->frames = 2;
		} break;

		case TE_GREENBLOOD:
		{	
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1)
				CL_GreenBloodHit(pos, dir);
			else //mxd. Classic particles
				CL_ParticleEffect2(pos, dir, 0xdf, 30, false);
		} break;

		// RAFAEL
		case TE_TUNNEL_SPARKS:
		{
			const int cnt = MSG_ReadByte(&net_message);
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			const int color = MSG_ReadByte(&net_message);
			CL_ParticleEffect2(pos, dir, color, cnt, true);
		} break;

		// PMM -following code integrated for flechette (different color)
		case TE_BLASTER:			// blaster hitting wall
		case TE_BLASTER2:			// green blaster hitting wall
		case TE_BLUEHYPERBLASTER:	// blue blaster hitting wall
		case TE_REDBLASTER:			// red blaster hitting wall
		case TE_FLECHETTE:			// flechette hitting wall
		{
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);

			if (cl_old_explosions->integer || r_particle_mode->integer == 0) //mxd. Also show when vanilla particles are enabled
			{
				explosion_t	*ex = CL_AllocExplosion();
				VectorCopy(pos, ex->ent.origin);
				ex->ent.angles[0] = acosf(dir[2]) / M_PI * 180;

				if (dir[0]) // PMM - fixed to correct for pitch of 0
					ex->ent.angles[1] = atan2f(dir[1], dir[0]) / M_PI * 180;
				else if (dir[1] > 0)
					ex->ent.angles[1] = 90;
				else if (dir[1] < 0)
					ex->ent.angles[1] = 270;
				else
					ex->ent.angles[1] = 0;

				ex->type = ex_misc;
				ex->ent.flags |= (RF_FULLBRIGHT | RF_TRANSLUCENT | RF_NOSHADOW); // No shadow on these

				if (type == TE_BLASTER2)
					ex->ent.skinnum = 1;
				else if (type == TE_REDBLASTER)
					ex->ent.skinnum = 3;
				else if (type == TE_FLECHETTE || type == TE_BLUEHYPERBLASTER)
					ex->ent.skinnum = 2;
				else // TE_BLASTER
					ex->ent.skinnum = 0;

				ex->start = cl.frame.servertime - 100;
				ex->light = 150;

				switch (type) //mxd
				{
					case TE_BLASTER2:			VectorSet(ex->lightcolor, 0.15f, 1.00f, 0.15f); break;
					case TE_BLUEHYPERBLASTER:	VectorSet(ex->lightcolor, 0.19f, 0.41f, 0.75f); break;
					case TE_REDBLASTER:			VectorSet(ex->lightcolor, 0.75f, 0.41f, 0.19f); break;
					case TE_FLECHETTE:			VectorSet(ex->lightcolor, 0.39f, 0.61f, 0.75f); break;
					default: /* TE_BLASTER */	VectorSet(ex->lightcolor, 1.00f, 1.00f, 0.00f); break;
				}

				ex->ent.model = clMedia.mod_explode;
				ex->frames = 4;
			}

			// Psychospaz's enhanced particle code
			vec3_t color, colordelta;
			const float	partsize = (cl_old_explosions->integer ? 2 : 4);
			const int numparts = (cl_old_explosions->integer ? 12 : (32 / max(cl_particle_scale->value / 2, 1)));

			if (type == TE_BLASTER2)
			{
				VectorSet(color, 50, 235, 50);
				VectorSet(colordelta, -10, 0, -10);
			}
			else if (type == TE_BLUEHYPERBLASTER)
			{
				VectorSet(color, 50, 50, 235);
				VectorSet(colordelta, -10, 0, -10);
			}
			else if (type == TE_REDBLASTER)
			{
				VectorSet(color, 235, 50, 50);
				VectorSet(colordelta, 0, -90, -30);
			}
			else if (type == TE_FLECHETTE)
			{
				VectorSet(color, 100, 100, 195);
				VectorSet(colordelta, -10, 0, -10);
			}
			else // TE_BLASTER
			{
				VectorSet(color, 255, 150, 50);
				VectorSet(colordelta, 0, -90, -30);
			}

			CL_BlasterParticles(pos, dir, color, colordelta, numparts, partsize);
			CL_ParticleBlasterDecal(pos, dir, color, 10);
			S_StartSound(pos, 0, 0, clMedia.sfx_lashit, 1, ATTN_NORM, 0);
		} break;

		// Shockwave impact effect
		case TE_SHOCKSPLASH:
		{
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);

			// Spawn 5 rings
			for (int i = 0; i < 5; i++)
			{
				explosion_t	*ex = CL_AllocExplosion();
				VectorMA(pos, 16.0f * (i + 1), dir, ex->ent.origin);
				vectoangles(dir, ex->ent.angles);

				ex->type = ex_poly2;
				ex->ent.flags |= (RF_FULLBRIGHT | RF_TRANSLUCENT | RF_NOSHADOW);
				ex->start = cl.frame.servertime - 100;

				ex->light = 75;
				VectorSet(ex->lightcolor, 0.39f, 0.61f, 0.75f);

				ex->ent.model = clMedia.mod_shocksplash;
				ex->ent.alpha = 0.7f;
				ex->baseframe = 4 - i;
				ex->frames = 4 + i;
			}

			S_StartSound(pos, 0, 0, clMedia.sfx_shockhit, 1, ATTN_NONE, 0);
		} break;

		case TE_LIGHTNING:
		{
			// Psychospaz's enhanced particle code
			const int ent = CL_ParseLightning();
			S_StartSound(NULL, ent, CHAN_WEAPON, clMedia.sfx_lightning, 1, ATTN_NORM, 0);
		} break;

		case TE_DEBUGTRAIL:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadPos(&net_message, pos2);
			CL_DebugTrail(pos, pos2);
			break;

		case TE_FLASHLIGHT:
		{
			MSG_ReadPos(&net_message, pos);
			const int ent = MSG_ReadShort(&net_message);
			CL_Flashlight(ent, pos);
		} break;

		case TE_FORCEWALL:
		{
			MSG_ReadPos(&net_message, pos);
			MSG_ReadPos(&net_message, pos2);
			const int color = MSG_ReadByte(&net_message);
			CL_ForceWall(pos, pos2, color);
		} break;

		case TE_HEATBEAM:
			CL_ParsePlayerBeam(clMedia.mod_heatbeam);
			break;

		case TE_MONSTER_HEATBEAM:
			CL_ParsePlayerBeam(clMedia.mod_monster_heatbeam);
			break;

		case TE_HEATBEAM_SPARKS:
		{
			const int cnt = 50;
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			const int magnitude = 60;

			vec3_t color; //mxd
			if (r_particle_mode->integer == 1)
				VectorSet(color, 240, 240, 240);
			else
				VectorSet(color, 123, 123, 123);

			CL_ParticleSteamEffect(pos, dir, color, tv(-20, -20, -20), cnt, magnitude);
			S_StartSound(pos, 0, 0, clMedia.sfx_lashit, 1, ATTN_NORM, 0);
		} break;

		case TE_HEATBEAM_STEAM:
		{
			const int cnt = 20;
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			const int magnitude = 60;

			vec3_t color; //mxd
			if (r_particle_mode->integer == 1)
				VectorSet(color, 255, 150, 50);
			else
				VectorSet(color, 255, 171, 7);

			CL_ParticleSteamEffect(pos, dir, color, tv(0, -90, -30), cnt, magnitude);
			CL_ParticlePlasmaBeamDecal(pos, dir, 10); // Added burnmark
			S_StartSound(pos, 0, 0, clMedia.sfx_lashit, 1, ATTN_NORM, 0);
		} break;

		case TE_STEAM:
			CL_ParseSteam();
			break;

		case TE_BUBBLETRAIL2:
		{
			const int cnt = 3; // was 8
			MSG_ReadPos(&net_message, pos);
			MSG_ReadPos(&net_message, pos2);
			CL_BubbleTrail2(pos, pos2, cnt);
			S_StartSound(pos, 0, 0, clMedia.sfx_lashit, 1, ATTN_NORM, 0);
		} break;

		case TE_MOREBLOOD:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1)
			{
				CL_BloodHit(pos, dir);
				CL_BloodHit(pos, dir);
			}
			else //mxd. Classic particles
			{
				CL_ParticleEffect(pos, dir, 0xe8, 250);
			}
			break;

		case TE_CHAINFIST_SMOKE:
			dir[0] = 0; dir[1] = 0; dir[2] = 1;
			MSG_ReadPos(&net_message, pos);

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1)
				CL_ParticleSmokeEffect(pos, dir, 8);
			else //mxd. Classic particles
				CL_ClassicParticleSmokeEffect(pos, dir, 0, 20, 20);
			break;

		case TE_ELECTRIC_SPARKS:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1)
				CL_ElectricParticles(pos, dir, 40); // New blue electric sparks
			else //mxd. Classic particles
				CL_ParticleEffect(pos, dir, 0x75, 40);

			//FIXME : replace or remove this sound
			S_StartSound(pos, 0, 0, clMedia.sfx_lashit, 1, ATTN_NORM, 0);
			break;

		case TE_TRACKER_EXPLOSION:
			MSG_ReadPos(&net_message, pos);
			CL_ColorFlash(pos, 0, 150, tv(-1, -1, -1));

			// Psychospaz's enhanced particle code
			if (r_particle_mode->integer == 1)
				CL_Tracker_Explode(pos);
			else //mxd. Classic particles
				CL_ColorExplosionParticles(pos, 0, 1);

			CL_Explosion_Decal(pos, 14, particle_trackermark);
			S_StartSound(pos, 0, 0, clMedia.sfx_disrexp, 1, ATTN_NORM, 0);
			break;

		case TE_TELEPORT_EFFECT:
		case TE_DBALL_GOAL:
			MSG_ReadPos(&net_message, pos);
			CL_TeleportParticles(pos);
			break;

		case TE_WIDOWBEAMOUT:
			CL_ParseWidow();
			break;

		case TE_NUKEBLAST:
			CL_ParseNuke();
			break;

		case TE_WIDOWSPLASH:
			MSG_ReadPos(&net_message, pos);
			CL_WidowSplash(pos);
			break;

		default:
			Com_Error(ERR_DROP, "CL_ParseTEnt: bad type");
	}
}

#pragma endregion

// Backup of client angles
extern vec3_t old_viewangles;

static void CL_AddBeams()
{
	vec3_t dist, org;
	entity_t ent;
	float yaw, pitch;
	float model_length;
	vec3_t grapple_offset_dir;

	// Chasecam grapple offset stuff
	const int handmult = (info_hand && info_hand->integer == 1 ? -1 : 1); //mxd

	vec3_t thirdp_grapple_offset;
	VectorSet(thirdp_grapple_offset, 6, 16, 16);
	// end third person grapple

	// Update beams
	int	i;
	beam_t *b;
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
			continue;

		const qboolean firstperson = ((b->entity == cl.playernum + 1) && !cg_thirdperson->integer);
		const qboolean chasecam = ((b->entity == cl.playernum + 1) && cg_thirdperson->integer);

		// If coming from the player, update the start position
		if (firstperson) // Entity 0 is the world
		{
			VectorCopy(cl.refdef.vieworg, b->start);
			b->start[2] -= 22; // Adjust for view height
		}
		else if (chasecam)
		{
			player_state_t *ps = &cl.frame.playerstate;
			const int ofi = (cl.frame.serverframe - 1) & UPDATE_MASK;
			frame_t *oldframe = &cl.frames[ofi];

			if (oldframe->serverframe != cl.frame.serverframe-1 || !oldframe->valid)
				oldframe = &cl.frame;

			player_state_t *ops = &oldframe->playerstate;

			vec3_t f, r, u;
			AngleVectors(old_viewangles, f, r, u);
			VectorClear(grapple_offset_dir);

			for (int j = 0; j < 3; j++)
			{
				grapple_offset_dir[j] += f[j] * thirdp_grapple_offset[1];
				grapple_offset_dir[j] += r[j] * thirdp_grapple_offset[0] * handmult;
				grapple_offset_dir[j] += u[j] * (-thirdp_grapple_offset[2]);
			}

			for (int j = 0; j < 3; j++)
				b->start[j] = cl.predicted_origin[j] + ops->viewoffset[j] + cl.lerpfrac * (ps->viewoffset[j] - ops->viewoffset[j]) + grapple_offset_dir[j];
		}

		if (chasecam)
			VectorCopy(b->start, org);
		else
			VectorAdd(b->start, b->offset, org);

		// Calculate pitch and yaw
		VectorSubtract(b->end, org, dist);

		if (dist[1] == 0 && dist[0] == 0)
		{
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			// PMM - fixed to correct for pitch of 0
			if (dist[0])
				yaw = (atan2f(dist[1], dist[0]) * 180 / M_PI);
			else if (dist[1] > 0)
				yaw = 90;
			else
				yaw = 270;

			if (yaw < 0)
				yaw += 360;

			const float forward = sqrtf(dist[0] * dist[0] + dist[1] * dist[1]);
			pitch = (atan2f(dist[2], forward) * -180.0f / M_PI);

			if (pitch < 0)
				pitch += 360;
		}

		// Add new entities for the beams
		float d = VectorNormalize(dist);

		memset(&ent, 0, sizeof(ent));
		if (b->model == clMedia.mod_lightning)
		{
			model_length = 35.0f;
			d-= 20.0f; // Correction so it doesn't end in middle of tesla
		}
		else
		{
			model_length = 30.0f;
		}

		const float steps = ceilf(d / model_length);
		const float len = (d - model_length) / (steps - 1);

		// PMM - special case for lightning model .. if the real length is shorter than the model, flip it around & draw it from the end to the start.
		// This prevents the model from going through the tesla mine (instead it goes through the target)
		if (b->model == clMedia.mod_lightning && d <= model_length)
		{
			VectorCopy(b->end, ent.origin);

			ent.model = b->model;
			ent.flags |= RF_FULLBRIGHT;
			ent.angles[0] = pitch;
			ent.angles[1] = yaw;
			ent.angles[2] = rand() % 360;

			V_AddEntity(&ent);
			
			return;
		}

		while (d > 0)
		{
			VectorCopy(org, ent.origin);
			ent.model = b->model;

			if (b->model == clMedia.mod_lightning)
			{
				ent.flags |= RF_FULLBRIGHT;
				ent.angles[0] = -pitch;
				ent.angles[1] = yaw + 180.0f;
			}
			else
			{
				ent.angles[0] = pitch;
				ent.angles[1] = yaw;
			}

			ent.angles[2] = rand() % 360;
			ent.flags |= RF_NOSHADOW; // beams don't cast shadows
			V_AddEntity(&ent);

			for (int j = 0; j < 3; j++)
				org[j] += dist[j] * len;

			d -= model_length;
		}
	}
}

// ROGUE - draw player locked beams
static void CL_AddPlayerBeams()
{
	int i;
	beam_t *b;
	vec3_t dist, org;
	entity_t ent;
	float yaw, pitch;
	int framenum;
	float model_length;
	float hand_multiplier;
	vec3_t pbeam_offset_dir;

	//PMM
	if (info_hand)
	{
		if (info_hand->value == 2)
			hand_multiplier = 0;
		else if (info_hand->value == 1)
			hand_multiplier = -1;
		else
			hand_multiplier = 1;
	}
	else 
	{
		hand_multiplier = 1;
	}

	// Chasecam beam offset stuff
	int newhandmult = hand_multiplier;
	if (newhandmult == 0)
		newhandmult = 1;

	vec3_t thirdp_pbeam_offset;
	VectorSet(thirdp_pbeam_offset, 6.5f, 0, 12);
	// end chasecam beam offset stuff

	// Update beams
	for (i = 0, b = cl_playerbeams; i < MAX_BEAMS; i++, b++)
	{
		vec3_t f, r, u;

		if (!b->model || b->endtime < cl.time)
			continue;

		const qboolean firstperson = ((b->entity == cl.playernum + 1) && !cg_thirdperson->integer);
		const qboolean chasecam = ((b->entity == cl.playernum + 1) && cg_thirdperson->integer);

		if (clMedia.mod_heatbeam && b->model == clMedia.mod_heatbeam)
		{
			// If coming from the player, update the start position
			if (firstperson || chasecam)
			{
				// Set up gun position. Code straight out of CL_AddViewWeapon
				player_state_t *ps = &cl.frame.playerstate;
				const int frame = (cl.frame.serverframe - 1) & UPDATE_MASK;
				frame_t *oldframe = &cl.frames[frame];
				if (oldframe->serverframe != cl.frame.serverframe - 1 || !oldframe->valid)
					oldframe = &cl.frame; // Previous frame was dropped or involid

				player_state_t *ops = &oldframe->playerstate;

				// Lerp for chasecam mode
				if (chasecam)
				{
					// Use player's original viewangles
					AngleVectors(old_viewangles, f, r, u);
					VectorClear(pbeam_offset_dir);

					for (int j = 0; j < 3; j++)
					{
						pbeam_offset_dir[j] += f[j] * thirdp_pbeam_offset[1];
						pbeam_offset_dir[j] += r[j] * thirdp_pbeam_offset[0] * newhandmult;
						pbeam_offset_dir[j] += u[j] * (-thirdp_pbeam_offset[2]);
					}

					for (int j = 0; j < 3; j++)
						b->start[j] = cl.predicted_origin[j] + ops->viewoffset[j] + cl.lerpfrac * (ps->viewoffset[j] - ops->viewoffset[j]) + pbeam_offset_dir[j];

					VectorMA(b->start, newhandmult * b->offset[0], r, org);
					VectorMA(org, b->offset[1], f, org);
					VectorMA(org, b->offset[2], u, org);
				}
				else // Firstperson
				{
					for (int j = 0; j < 3; j++)
						b->start[j] = cl.refdef.vieworg[j] + ops->gunoffset[j] + cl.lerpfrac * (ps->gunoffset[j] - ops->gunoffset[j]);

					VectorMA(b->start, (hand_multiplier * b->offset[0]), cl.v_right, org);
					VectorMA(org, b->offset[1], cl.v_forward, org);
					VectorMA(org, b->offset[2], cl.v_up, org);
					
					if (info_hand && info_hand->value == 2) 
						VectorMA(org, -1, cl.v_up, org);

					// FIXME - take these out when final
					VectorCopy(cl.v_right, r);
					VectorCopy(cl.v_forward, f);
					VectorCopy(cl.v_up, u);
				}
			}
			else // Some other player or monster
			{
				VectorCopy(b->start, org);
			}
		}
		else // Some other beam model
		{
			// If coming from the player, update the start position. Skip for chasecam mode
			if (firstperson)
			{
				VectorCopy(cl.refdef.vieworg, b->start);
				b->start[2] -= 22; // Adjust for view height
			}

			VectorAdd(b->start, b->offset, org);
		}

		// Calculate pitch and yaw
		VectorSubtract(b->end, org, dist);

		//PMM
		if (clMedia.mod_heatbeam && b->model == clMedia.mod_heatbeam && (firstperson || chasecam))
		{
			const vec_t l = VectorLength(dist);
			VectorScale(f, l, dist);

			if (chasecam)
				VectorMA(dist, newhandmult * b->offset[0], r, dist);
			else
				VectorMA(dist, hand_multiplier * b->offset[0], r, dist);

			VectorMA(dist, b->offset[1], f, dist);
			VectorMA(dist, b->offset[2], u, dist);

			if (chasecam)
			{
				VectorMA(dist, -(newhandmult * thirdp_pbeam_offset[0]), r, dist);
				VectorMA(dist, thirdp_pbeam_offset[1], f, dist);
				VectorMA(dist, thirdp_pbeam_offset[2], u, dist);
			}

			if (info_hand && info_hand->value == 2 && !chasecam)
				VectorMA(org, -1, cl.v_up, org);
		}

		if (dist[1] == 0 && dist[0] == 0)
		{
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			// PMM - fixed to correct for pitch of 0
			if (dist[0])
				yaw = (atan2f(dist[1], dist[0]) * 180 / M_PI);
			else if (dist[1] > 0)
				yaw = 90;
			else
				yaw = 270;

			if (yaw < 0)
				yaw += 360;

			const float forward = sqrtf(dist[0] * dist[0] + dist[1] * dist[1]);
			pitch = (atan2f(dist[2], forward) * -180.0f / M_PI);
			if (pitch < 0)
				pitch += 360.0f;
		}
		
		if (clMedia.mod_heatbeam && b->model == clMedia.mod_heatbeam)
		{
			if (!firstperson)
			{
				framenum = 2;
				ent.angles[0] = -pitch;
				ent.angles[1] = yaw + 180.0f;
				ent.angles[2] = 0;

				// Skip this for chasecam mode
				if (!chasecam)
				{
					AngleVectors(ent.angles, f, r, u);

					// If it's a non-origin offset, it's a player, so use the hardcoded player offset
					if (!VectorCompare (b->offset, vec3_origin))
					{
						VectorMA(org, -b->offset[0] + 1,  r, org);
						VectorMA(org, -b->offset[1] + 12, f, org); //was 0
						VectorMA(org, -b->offset[2] - 5,  u, org); //was -10
					}
					else
					{
						// If it's a monster, do the particle effect
						CL_MonsterPlasma_Shell(b->start);
					}
				}
			}
			else
			{
				framenum = 1;
			}
		}

		// If it's the heatbeam, draw the particle effect. Also do this in chasecam mode
		if (clMedia.mod_heatbeam && b->model == clMedia.mod_heatbeam && (firstperson || chasecam))
			CL_HeatbeamParticles(org, dist);

		// Add new entities for the beams
		float d = VectorNormalize(dist);

		memset(&ent, 0, sizeof(ent));
		if (b->model == clMedia.mod_heatbeam)
		{
			model_length = 32.0f;
		}
		else if (b->model == clMedia.mod_lightning)
		{
			model_length = 35.0f;
			d-= 20.0f; // Correction so it doesn't end in middle of tesla
		}
		else
		{
			model_length = 30.0f;
		}

		const float steps = ceilf(d / model_length);
		const float len = (d - model_length) / (steps - 1);

		// PMM - special case for lightning model .. if the real length is shorter than the model, flip it around & draw it from the end to the start.
		// This prevents the model from going through the tesla mine (instead it goes through the target)
		if (b->model == clMedia.mod_lightning && d <= model_length)
		{
			VectorCopy(b->end, ent.origin);

			ent.model = b->model;
			ent.flags |= (RF_FULLBRIGHT | RF_NOSHADOW); // Beams don't cast shadows
			ent.angles[0] = pitch;
			ent.angles[1] = yaw;
			ent.angles[2] = rand() % 360;
			V_AddEntity(&ent);

			return;
		}

		while (d > 0)
		{
			VectorCopy(org, ent.origin);
			ent.model = b->model;

			if (clMedia.mod_heatbeam && b->model == clMedia.mod_heatbeam)
			{
				ent.flags |= RF_FULLBRIGHT;
				ent.angles[0] = -pitch;
				ent.angles[1] = yaw + 180.0f;
				ent.angles[2] = (cl.time) % 360;
				ent.frame = framenum;
			}
			else if (b->model == clMedia.mod_lightning)
			{
				ent.flags |= RF_FULLBRIGHT;
				ent.angles[0] = -pitch;
				ent.angles[1] = yaw + 180.0f;
				ent.angles[2] = rand() % 360;
			}
			else
			{
				ent.angles[0] = pitch;
				ent.angles[1] = yaw;
				ent.angles[2] = rand() % 360;
			}

			ent.flags |= RF_NOSHADOW; // beams don't cast shadows
			V_AddEntity(&ent);

			for (int j = 0; j < 3; j++)
				org[j] += dist[j] * len;

			d -= model_length;
		}
	}
}

static void CL_AddExplosions()
{
	entity_t *ent;
	int i;
	explosion_t *ex;

	memset(&ent, 0, sizeof(ent));

	for (i = 0, ex = cl_explosions; i < MAX_EXPLOSIONS; i++, ex++)
	{
		if (ex->type == ex_free)
			continue;

		const float frac = (cl.time - ex->start) / 100.0f;
		int f = floorf(frac);

		ent = &ex->ent;

		switch (ex->type)
		{
			case ex_mflash:
				if (f >= ex->frames - 1)
					ex->type = ex_free;
				break;

			case ex_misc:
				if (f >= ex->frames - 1)
					ex->type = ex_free;
				else
					ent->alpha = 1.0f - frac / (ex->frames - 1);
				break;

			case ex_flash:
				if (f >= 1)
					ex->type = ex_free;
				else
					ent->alpha = 1.0f;
				break;

			case ex_poly:
				if (f >= ex->frames - 1)
				{
					ex->type = ex_free;
					break;
				}

				ent->alpha = (16.0f - (float)f) / 16.0f;

				if (f < 10)
				{
					ent->skinnum = (f >> 1);
					ent->skinnum = max(0, ent->skinnum);
				}
				else
				{
					ent->flags |= RF_TRANSLUCENT;
					if (f < 13)
						ent->skinnum = 5;
					else
						ent->skinnum = 6;
				}
				break;

			case ex_poly2:
				if (f >= ex->frames - 1)
				{
					ex->type = ex_free;
				}
				else
				{
					// Since nothing else uses this, I'd just thought I'd change it...
					ent->alpha = max((0.7f - f * 0.1f), 0.1f);
					ent->skinnum = 0;
					ent->flags |= RF_TRANSLUCENT;
				}
				break;
		}

		if (ex->type == ex_free)
			continue;

		if (ex->light)
			V_AddLight(ent->origin, ex->light * ent->alpha, ex->lightcolor[0], ex->lightcolor[1], ex->lightcolor[2]);

		VectorCopy(ent->origin, ent->oldorigin);

		f = max(0, f);
		ent->frame = ex->baseframe + f + 1;
		ent->oldframe = ex->baseframe + f;
		ent->backlerp = 1.0f - cl.lerpfrac;

		V_AddEntity(ent);
	}
}

static void CL_AddLasers()
{
	laser_t *l = cl_lasers;
	for (int i = 0; i < MAX_LASERS; i++, l++)
		if (l->endtime >= cl.time)
			V_AddEntity(&l->ent);
}

// PMM - CL_Sustains
static void CL_ProcessSustain()
{
	cl_sustain_t *s = cl_sustains;
	for (int i = 0; i < MAX_SUSTAINS; i++, s++)
	{
		if (!s->id)
			continue;

		if (s->endtime >= cl.time && cl.time >= s->nextthink)
			s->think(s);
		else if (s->endtime < cl.time)
			s->id = 0;
	}
}

void CL_AddTEnts()
{
	CL_AddBeams();

	// PMM - draw plasma beams
	CL_AddPlayerBeams();
	CL_AddExplosions();
	CL_AddLasers();

	// PMM - set up sustain
	CL_ProcessSustain();
}