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

// cl_effects.c -- particle and decal effects parsing/generation

#include "client.h"
#include "particles.h"

static vec3_t avelocities[NUMVERTEXNORMALS];

void CL_LightningBeam(const vec3_t start, const vec3_t end, const int srcent, const int dstent, const float size)
{
	for (cparticle_t *ap = active_particles; ap; ap = ap->next)
	{
		if (ap->src_ent == srcent && ap->dst_ent == dstent && ap->type == particle_lightning)
		{
			ap->time = cl.time;
			VectorCopy(start, ap->angle);
			VectorCopy(end, ap->org);

			return;
		}
	}

	cparticle_t *p = CL_InitParticle();
	if (!p)
		return;

	VectorCopy(start, p->angle);
	VectorCopy(end, p->org);
	p->alphavel = -2;
	p->size = size;
	p->type = particle_lightning;
	p->flags = PART_LIGHTNING;

	p->src_ent = srcent;
	p->dst_ent = dstent;

	CL_FinishParticleInit(p);
}

void CL_Explosion_Decal(vec3_t org, float size, int decalnum)
{
	if (r_decals->integer)
	{
		const int offset = 8;
		vec3_t angle[6], ang;
		vec3_t end1, end2, normal, sorg, dorg;
		vec3_t planenormals[6];

		VectorSet(angle[0], -1, 0, 0);
		VectorSet(angle[1], 1, 0, 0);
		VectorSet(angle[2], 0, 1, 0);
		VectorSet(angle[3], 0, -1, 0);
		VectorSet(angle[4], 0, 0, 1);
		VectorSet(angle[5], 0, 0, -1);

		for (int i = 0; i < 6; i++)
		{
			VectorMA(org, -offset, angle[i], sorg); // Move origin 8 units back
			VectorMA(sorg, size / 2 + offset, angle[i], end1);
			trace_t trace1 = CL_Trace(sorg, end1, 0, CONTENTS_SOLID);
			
			if (trace1.fraction < 1) // Hit a surface
			{	
				// Make sure we haven't hit this plane before
				VectorCopy(trace1.plane.normal, planenormals[i]);

				for (int j = 0; j < i; j++)
					if (VectorCompare(planenormals[j], planenormals[i])) 
						goto skip; //mxd. Actually skip the plane

				// Try tracing directly to hit plane
				VectorNegate(trace1.plane.normal, normal);
				VectorMA(sorg, size / 2, normal, end2);
				trace_t trace2 = CL_Trace(sorg, end2, 0, CONTENTS_SOLID);

				// If second trace hit same plane
				if (trace2.fraction < 1 && VectorCompare(trace2.plane.normal, trace1.plane.normal))
					VectorCopy(trace2.endpos, dorg);
				else
					VectorCopy(trace1.endpos, dorg);

				VecToAngleRolled(normal, rand() % 360, ang);

				cparticle_t *p = CL_InitParticle();
				if (!p)
					return;

				VectorCopy(ang, p->angle);
				VectorCopy(dorg, p->org);
				p->alphavel = -1 / r_decal_life->value;
				p->blendfunc_src = GL_ZERO;
				p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
				p->size = size;
				p->type = decalnum;
				p->flags = (PART_SHADED | PART_DECAL | PART_ALPHACOLOR);
				p->think = CL_DecalAlphaThink;

				CL_FinishParticleInit(p);
			}

			skip:; //mxd
		}
	}
}

static void CL_ExplosionThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	if (*alpha > 0.85f)
		*type = particle_rexplosion1;
	else if (*alpha > 0.7f)
		*type = particle_rexplosion2;
	else if (*alpha > 0.5f)
		*type = particle_rexplosion3;
	else if (*alpha > 0.4f)
		*type = particle_rexplosion4;
	else if (*alpha > 0.25f)
		*type = particle_rexplosion5;
	else if (*alpha > 0.1f)
		*type = particle_rexplosion6;
	else 
		*type = particle_rexplosion7;

	*alpha *= 3.0f;
	*alpha = min(1.0f, *alpha);
}

static void CL_ExplosionBubbleThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	// Disappear when no longer underwater
	if (!(CM_PointContents(org, 0) & MASK_WATER))
	{
		p->think = NULL;
		p->alpha = 0;
	}
}

// Explosion effect
void CL_Explosion_Particle(const vec3_t org, const float size, const qboolean rocket)
{
	cparticle_t *p = CL_InitParticle();
	if (!p)
		return;

	VectorCopy(org, p->org);
	p->alphavel = (rocket ? -2 : -1.5f);
	p->size = (size != 0 ? size : (150 - (rocket ? 0 : 75))); //mxd. Operator '?:' has lower precedence than '-'; '-' will be evaluated first;
	p->type = particle_rexplosion1;
	p->flags = PART_DEPTHHACK_SHORT;
	p->think = CL_ExplosionThink;

	CL_FinishParticleInit(p);
	CL_AddParticleLight(p, 300, 0, 1, 0.514f, 0);
}

// Explosion fash
void CL_Explosion_FlashParticle(const vec3_t org, const float size, const qboolean large)
{
	cparticle_t *p = CL_InitParticle();
	if (!p)
		return;

	VectorCopy(org, p->org);
	VectorSet(p->color, 255, 175, 100);
	p->alphavel = -1.75f;
	p->size = (size != 0 ? size : 50);
	p->sizevel = -10;
	p->type = (large ? particle_rflash : particle_blaster);
	p->flags = (large ? PART_DEPTHHACK_SHORT : 0);

	CL_FinishParticleInit(p);
}

static void CL_ParticleExplosionSparksThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	// Setting up angle for sparks
	const float time1 = *time;
	const float time2 = time1 * time1;

	for (int i = 0; i < 2; i++)
		angle[i] = 0.25f * (p->vel[i] * time1 + p->accel[i] * time2);
	angle[2] = 0.25f * (p->vel[2] * time1 + (p->accel[2] - PARTICLE_GRAVITY) * time2);
}

void CL_Explosion_Sparks(const vec3_t org, const int size, const int count)
{
	for (int i = 0; i < (count / cl_particle_scale->value); i++) // Was 256
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() % size) - 16);
			p->vel[j] = (rand() % 150) - 75;
		}

		VectorSet(p->color, 255, 100, 25);
		p->alphavel = -0.8f / (0.5f + frand() * 0.3f);
		p->size = size;
		p->sizevel = size * -1.5f;
		p->type = particle_solid;
		p->flags = (PART_GRAVITY | PART_SPARK);
		p->think = CL_ParticleExplosionSparksThink;

		CL_FinishParticleInit(p);
	}
}

#pragma region ======================= Blood effects

void CL_BloodPuff(const vec3_t org, const vec3_t dir, const int count);

#define MAXBLEEDSIZE 5
#define TIMEBLOODGROW 2.5f
#define BLOOD_DECAL_CHANCE 0.5F

static void CL_ParticleBloodDecalThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	// This REALLY slows things down //TODO: mxd. Does it?
	/*if (*time<TIMEBLOODGROW)
	{
		vec3_t dir;

		*size *= sqrt(0.5 + 0.5*(*time/TIMEBLOODGROW));

		AngleVectors (angle, dir, NULL, NULL);
		VectorNegate(dir, dir);
		CL_ClipDecal(p, *size, angle[2], org, dir);
	}*/

	// Now calc alpha
	CL_DecalAlphaThink(p, org, angle, alpha, size, type, time);
}

static void CL_ParticleBloodThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	trace_t trace = CL_Trace(p->oldorg, org, 0, CONTENTS_SOLID); // Was 0.1

	if (trace.fraction < 1.0f) // Delete and stain...
	{
		qboolean became_decal = false;
		
		if (r_decals->integer && (p->flags & PART_LEAVEMARK)
			&& !VectorCompare(trace.plane.normal, vec3_origin)
			&& !(CM_PointContents(p->oldorg, 0) & MASK_WATER)) // No blood splatters underwater...
		{
			vec3_t normal, dir;
			qboolean greenblood = false;
			qboolean timedout = false;

			if (p->color[1] > 0 && p->color[2] > 0)
				greenblood = true;

			// Time cutoff for gib trails
			if (p->flags & PART_GRAVITY && !(p->flags & PART_DIRECTION))
			{
				const float threshhold = (greenblood ? 1.0f : 0.5f); // Gekk gibs go flyin faster...
				if ((cl.time - p->time) * 0.001f > threshhold)
					timedout = true;
			}

			if (!timedout)
			{
				VectorNegate(trace.plane.normal, normal);
				VecToAngleRolled(normal, rand() % 360, p->angle);
				
				VectorCopy(trace.endpos, p->org);
				VectorClear(p->vel);
				VectorClear(p->accel);
				p->type = particle_blooddecal1 + rand() % 5;
				p->blendfunc_src = GL_SRC_ALPHA; //GL_ZERO
				p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA; //GL_ONE_MINUS_SRC_COLOR
				p->flags = PART_DECAL | PART_SHADED | PART_ALPHACOLOR;
				p->alpha = *alpha;
				p->alphavel = -1.0f / r_decal_life->value;

				if (greenblood)
					p->color[1] = 210;
				else
					VectorScale(p->color, 0.5f, p->color); //mxd

				p->think = CL_ParticleBloodDecalThink;
				p->size = MAXBLEEDSIZE * 0.5f * (random() * 5.0f + 5);
				p->sizevel = 0;
				
				p->decalnum = 0;
				p->decal = NULL;
				AngleVectors(p->angle, dir, NULL, NULL);
				VectorNegate(dir, dir);
				CL_ClipDecal(p, p->size, -p->angle[2], p->org, dir);

				if (p->decalnum)
					became_decal = true;
			}
		}

		if (!became_decal)
		{
			*alpha = 0;
			*size = 0;
			p->alpha = 0;
			p->think = NULL; //mxd
		}
	}

	VectorCopy(org, p->oldorg);
}

static void CL_ParticleBloodDropThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	CL_CalcPartVelocity(p, 0.2f, *time, angle);

	float length = VectorNormalize(angle);
	length = min(MAXBLEEDSIZE, length);
	VectorScale(angle, -length, angle);

	// Now to trace for impact...
	CL_ParticleBloodThink(p, org, angle, alpha, size, type, time);
}

static void CL_ParticleBloodPuffThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	angle[2] = angle[0] + *time * angle[1] + *time * *time * angle[2];

	// Now to trace for impact...
	CL_ParticleBloodThink(p, org, angle, alpha, size, type, time);
}

static void CL_BloodSmack(const vec3_t org, const vec3_t dir)
{
	cparticle_t *p = CL_InitParticle();
	if (p)
	{
		VectorSet(p->angle, crand() * 180, crand() * 100, 0);
		VectorCopy(org, p->org);
		VectorCopy(dir, p->vel);
		VectorSet(p->color, 255, 0, 0);
		p->alphavel = -1.0f / (0.5f + frand() * 0.3f);
		p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
		p->size = 10;
		p->type = particle_redblood;
		p->flags = (PART_SHADED | PART_OVERBRIGHT);
		p->think = CL_ParticleRotateThink;

		CL_FinishParticleInit(p);
	}

	CL_BloodPuff(org, dir, 1);
}

static void CL_BloodBleed(const vec3_t org, const vec3_t dir, const int count)
{
	vec3_t pos;
	VectorScale(dir, 10, pos);

	for (int i = 0; i < count; i++)
	{
		for (int c = 0; c < 3; c++) //mxd
			pos[c] = dir[c] + random() * (cl_blood->value - 2) * 0.01f;
		
		VectorScale(pos, 10 + (cl_blood->value - 2) * 0.0001f * random(), pos);

		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		VectorCopy(org, p->angle);

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() & 7) - 4) + dir[j];
			p->vel[j] = pos[j] * (random() * 3 + 5);
		}

		VectorSet(p->color, 255, 0, 0);
		p->alpha = 0.7f;
		p->alphavel = -0.25f / (0.5f + frand() * 0.3f);
		p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
		p->size = MAXBLEEDSIZE * 0.5f;
		p->type = particle_blooddrip;
		p->flags = (PART_SHADED | PART_DIRECTION | PART_GRAVITY | PART_OVERBRIGHT);
		p->think = CL_ParticleBloodDropThink;

		if (i == 0 && random() < BLOOD_DECAL_CHANCE)
			p->flags |= PART_LEAVEMARK;

		CL_FinishParticleInit(p);
	}
}

static void CL_BloodPuff(const vec3_t org, const vec3_t dir, const int count)
{
	for (int i = 0; i < count; i++)
	{
		const float d = rand() & 31;

		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		VectorSet(p->angle, crand() * 180, crand() * 100, 0);

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() & 7) - 4) + d * dir[j];
			p->vel[j] = dir[j] * (crand() * 3 + 5);
		}

		VectorSet(p->accel, 0, 0, -100);
		VectorSet(p->color, 255, 0, 0);
		p->alphavel = -1.0f;
		p->size = 10;
		p->type = particle_blood;
		p->flags = PART_SHADED;
		p->think = CL_ParticleBloodPuffThink;

		if (i == 0 && random() < BLOOD_DECAL_CHANCE)
			p->flags |= PART_LEAVEMARK;

		CL_FinishParticleInit(p);
	}
}

void CL_BloodHit(const vec3_t org, const vec3_t dir)
{
	switch (cl_blood->integer)
	{
		case 1: CL_BloodPuff(org, dir, 5); break; // Puff
		case 2: CL_BloodSmack(org, dir); break; // Splat
		case 3: CL_BloodBleed(org, dir, 6); break; // Bleed
		case 4: CL_BloodBleed(org, dir, 16); break; // Gore
	}
}

// Green blood spray
void CL_GreenBloodHit(const vec3_t org, const vec3_t dir)
{
	if (cl_blood->value < 1) // Disable blood option
		return;

	for (int i = 0; i < 5; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		VectorSet(p->angle, crand() * 180, crand() * 100, 0);

		const float d = rand() & 31;
		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() & 7) - 4) + d * dir[j];
			p->vel[j] = dir[j] * (crand() * 3 + 5);
		}

		VectorSet(p->accel, 0, 0, -100);
		VectorSet(p->color, 255, 180, 50);
		p->alphavel = -1.0f;
		p->size = 10;
		p->type = particle_blood;
		p->flags = (PART_SHADED | PART_OVERBRIGHT);
		p->think = CL_ParticleBloodPuffThink;

		if (i == 0 && random() < BLOOD_DECAL_CHANCE)
			p->flags |= PART_LEAVEMARK;

		CL_FinishParticleInit(p);
	}
}

#pragma endregion

static void CL_ParticleSplashThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	vec3_t len;
	VectorSubtract(p->angle, org, len);

	// Setting up angle for sparks
	const float time1 = *time;
	const float time2 = time1 * time1;

	for (int i = 0; i < 2; i++)
		angle[i] = 0.5f * (p->vel[i] * time1 + p->accel[i] * time2);
	angle[2] = 0.5f * (p->vel[2] * time1 + (p->accel[2] - PARTICLE_GRAVITY) * time2);
}

// Water splashing
void CL_ParticleEffectSplash(const vec3_t org, const vec3_t dir, const int color8, const int count)
{
	vec3_t color;
	color8_to_vec3(color8, color);

	for (int i = 0; i < count; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		VectorCopy(org, p->angle);

		const float d = rand() & 5;
		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + d * dir[j];
			p->vel[j] = dir[j] * 40 + crand() * 10;
		}

		VectorCopy(color, p->color);
		p->alphavel = -0.75f / (0.5f + frand() * 0.3f);
		p->size = 5;
		p->sizevel = -7;
		p->type = particle_smoke;
		p->flags = (PART_GRAVITY | PART_DIRECTION);
		p->think = CL_ParticleSplashThink;

		CL_FinishParticleInit(p);
	}
}

static void CL_ParticleSparksThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	// Setting up angle for sparks
	const float time1 = *time;
	const float time2 = time1 * time1;

	for (int i = 0; i < 2; i++)
		angle[i] = 0.25f * (p->vel[i] * time1 + p->accel[i] * time2);
	angle[2] = 0.25f * (p->vel[2] * time1 + (p->accel[2] - PARTICLE_GRAVITY) * time2);
}

void CL_ParticleEffectSparks(const vec3_t org, const vec3_t dir, const vec3_t color, const int count)
{
	cparticle_t *p = NULL;

	for (int i = 0; i < count; i++)
	{
		p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() & 3) - 2);
			p->vel[j] = crand() * 20 + dir[j] * 40;
		}

		VectorCopy(color, p->color);
		p->alpha = 0.75f;
		p->alphavel = -1.0f / (0.5f + frand() * 0.3f);
		p->size = 4; //Knightmare- increase size
		p->type = particle_solid;
		p->flags = (PART_GRAVITY | PART_SPARK);
		p->think = CL_ParticleSparksThink;

		CL_FinishParticleInit(p);
	}

	// Added light effect
	CL_AddParticleLight(p, (count > 8 ? 130 : 65), 0, color[0] / 255, color[1] / 255, color[2] / 255);
}

#define DECAL_OFFSET 0.5f

void CL_ParticleBulletDecal(const vec3_t org, const vec3_t dir, const float size)
{
	if (!r_decals->integer)
		return;

	vec3_t end, origin;
	VectorMA(org, DECAL_OFFSET, dir, origin);
	VectorMA(org, -DECAL_OFFSET, dir, end);
	trace_t tr = CL_Trace(origin, end, 0, CONTENTS_SOLID);

	if (tr.fraction == 1 || VectorCompare(tr.plane.normal, vec3_origin)) //mxd. Other CL_*Decal have VectorCompare, why this was an exception?
		return;

	vec3_t ang, angle;
	VectorNegate(tr.plane.normal, angle);
	VecToAngleRolled(angle, rand() % 360, ang);

	cparticle_t *p = CL_InitParticle();
	if (!p)
		return;

	VectorCopy(ang, p->angle);
	VectorCopy(tr.endpos, p->org);
	p->alphavel = -1.0f / r_decal_life->value;
	p->blendfunc_src = GL_ZERO;
	p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
	p->size = size;
	p->type = particle_bulletmark;
	p->flags = (PART_SHADED | PART_DECAL | PART_ALPHACOLOR); // Was part_saturate
	p->think = CL_DecalAlphaThink;

	CL_FinishParticleInit(p);
}

#define RAIL_DECAL_OFFSET 2.0f

static void CL_ParticleRailDecal(const vec3_t org, const vec3_t dir, const float size, const qboolean isred)
{
	if (!r_decals->integer)
		return;

	vec3_t end, origin;
	VectorMA(org, -RAIL_DECAL_OFFSET, dir, origin);
	VectorMA(org, 2 * RAIL_DECAL_OFFSET, dir, end);
	trace_t tr = CL_Trace(origin, end, 0, CONTENTS_SOLID);

	if (tr.fraction == 1 || VectorCompare(tr.plane.normal, vec3_origin))
		return;

	vec3_t ang, angle;
	VectorNegate(tr.plane.normal, angle);
	VecToAngleRolled(angle, rand() % 360, ang);

	//mxd
	vec3_t color;
	if (isred)
		VectorSet(color, 255, 20, 20);
	else
		VectorSet(color, cl_railred->value, cl_railgreen->value, cl_railblue->value);

	cparticle_t *p = CL_InitParticle();
	if (!p)
		return;

	VectorCopy(ang, p->angle);
	VectorCopy(tr.endpos, p->org);
	p->alphavel = -1.0f / r_decal_life->value;
	p->blendfunc_src = GL_ZERO;
	p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
	p->size = size;
	p->type = particle_bulletmark;
	p->flags = (PART_SHADED | PART_DECAL | PART_ALPHACOLOR);
	p->think = CL_DecalAlphaThink;

	CL_FinishParticleInit(p);

	p = CL_InitParticle();
	if (!p)
		return;

	VectorCopy(ang, p->angle);
	VectorCopy(tr.endpos, p->org);
	VectorCopy(color, p->color);
	p->alphavel = -0.25f;
	p->size = size;
	p->type = particle_generic;
	p->flags = PART_DECAL;

	CL_FinishParticleInit(p);

	p = CL_InitParticle();
	if (!p)
		return;

	VectorCopy(ang, p->angle);
	VectorCopy(tr.endpos, p->org);
	p->alphavel = -0.25f;
	p->size = size * 0.67f;
	p->type = particle_generic;
	p->flags = PART_DECAL;

	CL_FinishParticleInit(p);
}

void CL_ParticleBlasterDecal(const vec3_t org, const vec3_t dir, const vec3_t color, const float size)
{
	if (!r_decals->integer)
		return;

	vec3_t end, origin;
	VectorMA(org, DECAL_OFFSET, dir, origin);
	VectorMA(org, -DECAL_OFFSET, dir, end);
	trace_t tr = CL_Trace(origin, end, 0, CONTENTS_SOLID);

	if (tr.fraction == 1 || VectorCompare(tr.plane.normal, vec3_origin))
		return;

	vec3_t ang, angle;
	VectorNegate(tr.plane.normal, angle);
	VecToAngleRolled(angle, rand() % 360, ang);

	cparticle_t *p = CL_InitParticle();
	if (!p)
		return;

	VectorCopy(ang, p->angle);
	VectorCopy(tr.endpos, p->org);
	p->alpha = 0.7f;
	p->alphavel = -1.0f / r_decal_life->value;
	p->blendfunc_src = GL_ZERO;
	p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
	p->size = size;
	p->type = particle_shadow;
	p->flags = (PART_SHADED | PART_DECAL);

	CL_FinishParticleInit(p);

	p = CL_InitParticle();
	if (!p)
		return;

	VectorCopy(ang, p->angle);
	VectorCopy(origin, p->org);
	VectorCopy(color, p->color);
	p->alphavel = -0.3f;
	p->size = size * 0.4f;
	p->type = particle_generic;
	p->flags = (PART_SHADED | PART_DECAL);

	CL_FinishParticleInit(p);

	p = CL_InitParticle();
	if (!p)
		return;

	VectorCopy(ang, p->angle);
	VectorCopy(origin, p->org);
	VectorCopy(color, p->color);
	p->alphavel = -0.6f;
	p->size = size * 0.3f;
	p->type = particle_generic;
	p->flags = (PART_SHADED | PART_DECAL);

	CL_FinishParticleInit(p);
}

void CL_ParticlePlasmaBeamDecal(const vec3_t org, const vec3_t dir, const float size)
{
	if (!r_decals->integer)
		return;

	vec3_t end, origin;
	VectorMA(org, DECAL_OFFSET, dir, origin);
	VectorMA(org, -DECAL_OFFSET, dir, end);
	trace_t tr = CL_Trace(origin, end, 0, CONTENTS_SOLID);

	if (tr.fraction == 1 || VectorCompare(tr.plane.normal, vec3_origin))
		return;

	vec3_t ang, angle;
	VectorNegate(tr.plane.normal, angle);
	VecToAngleRolled(angle, rand() % 360, ang);

	cparticle_t *p = CL_InitParticle();
	if (!p)
		return;

	VectorCopy(ang, p->angle);
	VectorCopy(tr.endpos, p->org);
	p->alpha = 0.85f;
	p->alphavel = -1.0f / r_decal_life->value;
	p->blendfunc_src = GL_ZERO;
	p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
	p->size = size;
	p->type = particle_shadow;
	p->flags = (PART_SHADED | PART_DECAL);

	CL_FinishParticleInit(p);
}

//mxd
void CL_Shadow_Decal(const vec3_t org, float size, float alpha)
{
	if(cl_paused->integer) // Don't draw when paused (messes up rendering of the first frame after unpausing)
		return;
	
	vec3_t end, origin, dir;
	VectorSet(dir, 0, 0, -1); // Straight down
	VectorMA(org, -DECAL_OFFSET, dir, origin);
	VectorMA(org, size, dir, end);
	trace_t tr = CL_Trace(origin, end, 0, CONTENTS_SOLID);

	if (tr.fraction == 1 || VectorCompare(tr.plane.normal, vec3_origin))
		return;

	size *= 1.0f - tr.fraction; // Shrink by distance
	alpha *= 1.0f - tr.fraction; // Fade by distance

	vec3_t ang, angle;
	VectorNegate(tr.plane.normal, angle);
	VecToAngleRolled(angle, 0, ang);

	cparticle_t *p = CL_InitParticle();
	if (!p)
		return;

	VectorCopy(ang, p->angle);
	VectorCopy(tr.endpos, p->org);
	p->alpha = alpha;
	p->alphavel = INSTANT_PARTICLE;
	p->blendfunc_src = GL_ZERO;
	p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
	p->size = size;
	p->type = particle_shadow;
	p->flags = (PART_SHADED | PART_DECAL);

	CL_FinishParticleInit(p);
}

void CL_TeleporterParticles(entity_state_t *ent)
{
	for (int i = 0; i < 8; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
			p->org[j] = ent->origin[j] - 16 + (rand() & 31);

		VectorSet(p->vel, crand() * 14, crand() * 14, 80 + (rand() & 7));
		VectorSet(p->color, 230 + crand() * 25, 125 + crand() * 25, 25 + crand() * 25);
		p->alphavel = -0.5f;
		p->size = 2;
		p->type = particle_generic;
		p->flags = PART_GRAVITY;

		CL_FinishParticleInit(p);
	}
}

void CL_LogoutEffect(const vec3_t org, const int type)
{
	vec3_t color;

	switch (type) //mxd
	{
		case MZ_LOGIN:	VectorSet(color, 20, 200, 20); break; // green
		case MZ_LOGOUT: VectorSet(color, 200, 20, 20); break; // red
		default:		VectorSet(color, 200, 200, 20); break; // yeller
	}

	for (int i = 0; i < 500; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		VectorSet(p->org, org[0] - 16 + frand() * 32,
						  org[1] - 16 + frand() * 32,
						  org[2] - 24 + frand() * 56);

		for (int j = 0; j < 3; j++)
			p->vel[j] = crand() * 20;

		VectorCopy(color, p->color);
		p->alphavel = -1.0f / (1.0f + frand() * 0.3f);
		p->type = particle_generic;
		p->flags = PART_GRAVITY;

		CL_FinishParticleInit(p);
	}
}

void CL_ItemRespawnParticles(const vec3_t org)
{
	for (int i = 0; i < 64; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + crand() * 8;
			p->vel[j] = crand() * 8;
		}

		VectorSet(p->accel, 0, 0, PARTICLE_GRAVITY * 0.2f);
		VectorSet(p->color, 0, 150 + rand() * 25, 0);
		p->alphavel = -1.0f / (1.0f + frand() * 0.3f);
		p->type = particle_generic;
		p->flags = PART_GRAVITY;

		CL_FinishParticleInit(p);
	}
}

void CL_BigTeleportParticles(const vec3_t org)
{
	static vec3_t colortable[] =
	{
		{ 10, 150, 50 },
		{ 50, 150, 10 },
		{ 150, 50, 10 },
		{ 50, 10, 150 },
	};

	for (int i = 0; i < (1024 / cl_particle_scale->value); i++) // Was 4096
	{
		const int index = rand() & 3;
		const float angle = M_PI * 2 * (rand() & 1023) / 1023.0f;
		const float dist = rand() & 31;

		const float cosa = cosf(angle); //mxd
		const float sina = sinf(angle); //mxd

		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		p->org[0] = org[0] + cosa * dist;
		p->org[1] = org[1] + sina * dist;
		p->org[2] = org[2] + 8 + (rand() % 90);

		p->vel[0] = cosa * (70 + (rand() & 63));
		p->vel[1] = sina * (70 + (rand() & 63));
		p->vel[2] = -100 + (rand() & 31);

		p->accel[0] = -cosa * 100;
		p->accel[1] = -sina * 100;
		p->accel[2] = PARTICLE_GRAVITY * 4;

		VectorCopy(colortable[index], p->color);
		p->alphavel = -0.1f / (0.5f + frand() * 0.3f);
		p->size = 5;
		p->sizevel = 0.15f / (0.5f + frand() * 0.3f);
		p->type = particle_generic;

		CL_FinishParticleInit(p);
	}
}

#define BLASTER_PARTICLE_MAX_VELOCITY	100
#define BLASTER_PARTICLE_MIN_SIZE		1.0f
#define BLASTER_PARTICLE_MAX_SIZE		5.0f

// Wall impact puffs
static void CL_ParticleBlasterThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	vec3_t len;
	float clipsize = 1.0f;
	VectorSubtract(p->angle, org, len);

	*size *= BLASTER_PARTICLE_MAX_SIZE / VectorLength(len) / (4 - *size);
	*size += *time * p->sizevel;
	*size = clamp(*size, BLASTER_PARTICLE_MIN_SIZE, BLASTER_PARTICLE_MAX_SIZE); //mxd

	CL_ParticleBounceThink(p, org, angle, alpha, &clipsize, type, time); // Was size

	float length = VectorNormalize(p->vel);
	length = min(BLASTER_PARTICLE_MAX_VELOCITY, length); //mxd. Passing VectorNormalize to min() was not such a great idea (because it's evaluated twice)...
	VectorScale(p->vel, length, p->vel);
}

// Wall impact puffs
void CL_BlasterParticles(const vec3_t org, const vec3_t dir, const vec3_t color, const vec3_t colorvel, const int count, const float size)
{
	//mxd. Classic particles
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicBlasterParticles(org, dir);
		return;
	}
	
	const float speed = 0.75f;
	cparticle_t *p = NULL;

	for (int i = 0; i < count; i++)
	{
		p = CL_InitParticle();
		if (!p)
			return;

		VectorCopy(org, p->angle);

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + dir[j] * (1 + random() * 3 + BLASTER_PARTICLE_MAX_SIZE / 2.0f);
			p->vel[j] = (dir[j] * 75 + crand() * 40) * speed;
		}

		VectorCopy(color, p->color);
		VectorCopy(colorvel, p->colorvel);
		p->alphavel = -0.5f / (0.5f + frand() * 0.3f);
		p->size = size; // Was 4
		p->sizevel = size * -0.125f; // Was -0.5
		p->type = particle_generic;
		p->flags = PART_GRAVITY;
		p->think = CL_ParticleBlasterThink;

		CL_FinishParticleInit(p);
	}

	// Added light effect
	CL_AddParticleLight(p, 150, 0, color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f);
}

void CL_BlasterTrail(const vec3_t start, const vec3_t end, const vec3_t color, const vec3_t colorvel)
{
	//mxd. Classic particles
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicBlasterTrail(start, end);
		return;
	}
	
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);
	VectorMA(move, 5.0f, vec, move);

	const int dec = 4 * cl_particle_scale->value; 
	VectorScale(vec, dec, vec);

	for (int i = 0; i < len; i += dec)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand();
			p->vel[j] = crand() * 5;
		}

		VectorCopy(color, p->color);
		VectorCopy(colorvel, p->colorvel);
		p->alphavel = -1.0f / (0.5f + frand() * 0.3f);
		p->size = 3;
		p->sizevel = -7;
		p->type = particle_generic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

// Hyperblaster particle glow effect
static void CL_HyperBlasterGlow(const vec3_t start, const vec3_t end, const vec3_t color, const vec3_t colorvel)
{
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	VectorNormalize(vec);
	VectorMA(move, 16.5f, vec, move);

	const float dec = 3.0f;
	VectorScale(vec, dec, vec);

	for (int i = 0; i < 12; i++) // Was 18
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + 0.5f * crand();
			p->vel[j] = crand() * 5;
		}

		VectorCopy(color, p->color);
		VectorCopy(colorvel, p->colorvel);
		p->alphavel = INSTANT_PARTICLE;
		p->size = 4.2f - (0.1f * i);
		p->type = particle_generic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

static void CL_BlasterTracer(const vec3_t origin, const vec3_t angle, const vec3_t color, const float len, const float size)
{
	vec3_t dir;
	AngleVectors(angle, dir, NULL, NULL);
	VectorScale(dir, len, dir);

	cparticle_t *p = CL_InitParticle();
	if (!p)
		return;

	VectorCopy(dir, p->angle);
	VectorCopy(origin, p->org);
	VectorCopy(color, p->color);
	p->alphavel = INSTANT_PARTICLE;
	p->blendfunc_src = GL_ONE;
	p->size = size;
	p->type = particle_blasterblob;
	p->flags = (PART_DIRECTION | PART_INSTANT | PART_OVERBRIGHT);

	CL_FinishParticleInit(p);
}

void CL_HyperBlasterEffect(const vec3_t start, const vec3_t end, const vec3_t angle, const vec3_t color, const vec3_t colorvel, const float len, const float size)
{
	if (cl_particle_scale->value < 2)
		CL_HyperBlasterGlow(start, end, color, colorvel);

	CL_BlasterTracer(end, angle, color, len, size);
}

void CL_QuadTrail(const vec3_t start, const vec3_t end)
{
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	const int dec = 5;
	VectorScale(vec, dec, vec);

	for (int i = 0; i < len; i += dec)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand() * 16;
			p->vel[j] = crand() * 5;
		}

		VectorSet(p->color, 0, 0, 200);
		p->alphavel = -1.0f / (0.8f + frand() * 0.2f);
		p->type = particle_generic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

void CL_FlagTrail(const vec3_t start, const vec3_t end, const qboolean isred, const qboolean isgreen)
{
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	const int dec = 5;
	VectorScale(vec, dec, vec);

	//mxd
	vec3_t color = { 0, 0, 0 };

	if (isred) 
		color[0] = 255;
	else if (isgreen) 
		color[1] = 255;
	else 
		color[2] = 255;

	for (int i = 0; i < len; i += dec)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand() * 16;
			p->vel[j] = crand() * 5;
		}

		VectorCopy(color, p->color);
		p->alphavel = -1.0f / (0.8f + frand() * 0.2f);
		p->type = particle_generic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

void CL_DiminishingTrail(const vec3_t start, const vec3_t end, centity_t *old, const int flags)
{
	//mxd. Classic particles...
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicDiminishingTrail(start, end, old, flags);
		return;
	}
	
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	float dec = (flags & EF_ROCKET ? 10 : 2);
	dec *= cl_particle_scale->value;
	VectorScale(vec, dec, vec);

	float orgscale, velscale;
	if (old->trailcount > 900)
	{
		orgscale = 4;
		velscale = 15;
	}
	else if (old->trailcount > 800)
	{
		orgscale = 2;
		velscale = 10;
	}
	else
	{
		orgscale = 1;
		velscale = 5;
	}

	cparticle_t *p;

	for (int i = 0; i < len; i += dec)
	{
		if (flags & EF_ROCKET)
		{
			if (CM_PointContents(move, 0) & MASK_WATER)
			{
				p = CL_InitParticle();
				if (!p)
					return;

				VectorSet(p->angle, 0, 0, crand() * 360);
				VectorCopy(move, p->org);
				VectorSet(p->vel, crand() * 9, crand() * 9, crand() * 9 + 5);
				p->alpha = 0.75f;
				p->alphavel = -0.2f / (1.0f + frand() * 0.2f);
				p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
				p->size = 1 + random() * 3;
				p->sizevel = 1;
				p->type = particle_bubble;
				p->flags = (PART_TRANS | PART_SHADED);
				p->think = CL_ExplosionBubbleThink;

				CL_FinishParticleInit(p);
			}
			else
			{
				p = CL_InitParticle();
				if (!p)
					return;

				VectorSet(p->angle, crand() * 180, crand() * 100, 0);
				VectorCopy(move, p->org);
				VectorSet(p->vel, crand() * 5, crand() * 5, crand() * 10);
				VectorSet(p->accel, 0, 0, 5);
				VectorSetAll(p->colorvel, -50);
				p->alpha = 0.75f;
				p->alphavel = -0.75f;
				p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
				p->size = 5;
				p->sizevel = 15;
				p->type = particle_smoke;
				p->flags = (PART_TRANS | PART_SHADED);
				p->think = CL_ParticleRotateThink;

				CL_FinishParticleInit(p);
			}
		}
		else
		{
			// Drop less particles as it flies
			if ((rand() & 1023) < old->trailcount)
			{
				if (flags & EF_GIB)
				{
					if (cl_blood->value > 1)
					{
						p = CL_InitParticle();
						if (!p)
							return;

						VectorSet(p->angle, 0, 0, random() * 360);

						for (int j = 0; j < 3; j++)
						{
							p->org[j] = move[j] + crand() * orgscale;
							p->vel[j] = crand() * velscale;
						}

						VectorSet(p->color, 255, 0, 0);
						p->alpha = 0.75f;
						p->alphavel = -0.75f / (1.0f + frand() * 0.4f);
						p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
						p->size = 3 + random() * 2;
						p->type = particle_blooddrop;
						p->flags = (PART_OVERBRIGHT | PART_GRAVITY | PART_SHADED);
						p->think = CL_ParticleBloodThink;

						CL_FinishParticleInit(p);
					}
					else
					{
						p = CL_InitParticle();
						if (!p)
							return;

						for (int j = 0; j < 3; j++)
						{
							p->org[j] = move[j] + crand() * orgscale;
							p->vel[j] = crand() * velscale;
						}

						VectorSet(p->color, 255, 0, 0);
						p->alphavel = -1.0f / (1.0f + frand() * 0.4f);
						p->size = 5;
						p->sizevel = -1;
						p->type = particle_blood;
						p->flags = (PART_GRAVITY | PART_SHADED);
						p->think = CL_ParticleBloodThink;

						CL_FinishParticleInit(p);
					}

					if (crand() < 0.0001f)
						p->flags |= PART_LEAVEMARK;
				}
				else if (flags & EF_GREENGIB)
				{
					p = CL_InitParticle();
					if (!p)
						return;

					for (int j = 0; j < 3; j++)
					{
						p->org[j] = move[j] + crand() * orgscale;
						p->vel[j] = crand() * velscale;
					}

					VectorSet(p->color, 255, 180, 50);
					p->alphavel = -0.5f / (1.0f + frand() * 0.4f);
					p->size = 5;
					p->sizevel = -1;
					p->type = particle_blood;
					p->flags = (PART_OVERBRIGHT | PART_GRAVITY | PART_SHADED);
					p->think = CL_ParticleBloodThink;

					if (crand() < 0.0001f)
						p->flags |= PART_LEAVEMARK;

					CL_FinishParticleInit(p);
				}
				else if (flags & EF_GRENADE) // No overbrights on grenade trails
				{
					if (CM_PointContents(move, 0) & MASK_WATER)
					{
						p = CL_InitParticle();
						if (!p)
							return;

						VectorSet(p->angle, 0, 0, crand() * 360);
						VectorCopy(move, p->org);
						VectorSet(p->vel, crand() * 9, crand() * 9, crand() * 9 + 5);
						p->alpha = 0.75f;
						p->alphavel = -0.2f / (1.0f + frand() * 0.2f);
						p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
						p->size = 1 + random() * 3;
						p->sizevel = 1;
						p->type = particle_bubble;
						p->flags = (PART_TRANS | PART_SHADED);
						p->think = CL_ExplosionBubbleThink;

						CL_FinishParticleInit(p);
					}
					else
					{
						p = CL_InitParticle();
						if (!p)
							return;

						VectorSet(p->angle, crand() * 180, crand() * 50, 0);
							
						for (int j = 0; j < 3; j++)
						{
							p->org[j] = move[j] + crand() * orgscale;
							p->vel[j] = crand() * velscale;
						}

						VectorSet(p->accel, 0, 0, 20);
						p->alpha = 0.5f;
						p->alphavel = -0.5f;
						p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
						p->size = 5;
						p->sizevel = 5;
						p->type = particle_smoke;
						p->flags = (PART_TRANS | PART_SHADED);
						p->think = CL_ParticleRotateThink;

						CL_FinishParticleInit(p);
					}
				}
				else
				{
					p = CL_InitParticle();
					if (!p)
						return;

					VectorSet(p->angle, crand() * 180, crand() * 50, 0);

					for (int j = 0; j < 3; j++)
					{
						p->org[j] = move[j] + crand() * orgscale;
						p->vel[j] = crand() * velscale;
					}

					VectorSet(p->accel, 0, 0, 20);
					p->alpha = 0.5f;
					p->alphavel = -0.5f;
					p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
					p->size = 5;
					p->sizevel = 5;
					p->type = particle_smoke;
					p->flags = (PART_OVERBRIGHT | PART_TRANS | PART_SHADED);
					p->think = CL_ParticleRotateThink;

					CL_FinishParticleInit(p);
				}
			}

			old->trailcount = max(100, old->trailcount - 5);
		}

		VectorAdd(move, vec, move);
	}
}

void CL_RocketTrail(const vec3_t start, const vec3_t end, centity_t *old)
{
	//mxd. Classic particles...
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicRocketTrail(start, end, old);
		return;
	}
	
	// Smoke
	CL_DiminishingTrail(start, end, old, EF_ROCKET);

	// Fire
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	float dec = cl_particle_scale->value;
	VectorScale(vec, dec, vec);

	for (int i = 0; i < len; i += dec)
	{
		// Falling particles
		if ((rand() & 7) == 0)
		{
			cparticle_t *p = CL_InitParticle();
			if (!p)
				return;

			for (int j = 0; j < 3; j++)
			{
				p->org[j] = move[j] + crand() * 5;
				p->vel[j] = crand() * 20;
			}

			VectorSet(p->accel, 0, 0, 20);
			VectorSet(p->color, 255, 255, 200);
			VectorSet(p->colorvel, 0, -50, 0);
			p->alphavel = -1.0f / (1.0f + frand() * 0.2f);
			p->size = 2;
			p->sizevel = -2;
			p->type = particle_blaster;
			p->flags = PART_GRAVITY;

			CL_FinishParticleInit(p);
		}

		VectorAdd(move, vec, move);
	}

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);
	dec = 1.5f * cl_particle_scale->value;
	VectorScale(vec, dec, vec);

	for (int i = 0; i < len; i += dec)
	{
		// Flame
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		VectorSet(p->angle, crand() * 180, crand() * 100, 0);
		VectorCopy(move, p->org);

		for (int j = 0; j < 3; j++)
			p->vel[j] = crand() * 5;

		VectorSet(p->accel, 0, 0, 5);
		VectorSet(p->color, 255, 225, 200);
		VectorSetAll(p->colorvel, -50);
		p->alpha = 0.5f;
		p->alphavel = -2;
		p->size = 5;
		p->sizevel = 5;
		p->type = particle_inferno;
		p->think = CL_ParticleRotateThink;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

// Returns true if the first vector is farther from the viewpoint.
static qboolean FartherPoint(const vec3_t pt1, const vec3_t pt2)
{
	vec3_t distance1, distance2;

	VectorSubtract(pt1, cl.refdef.vieworg, distance1);
	VectorSubtract(pt2, cl.refdef.vieworg, distance2);

	return (VectorLengthSquared(distance1) > VectorLengthSquared(distance2));
}

#define DEVRAILSTEPS	2
#define RAILTRAILSPACE	15 // The length of each piece...

static void CL_RailSprial(const vec3_t start, const vec3_t end, const qboolean isred)
{
	// Draw from closest point
	vec3_t move, vec;
	if (FartherPoint(start, end))
	{
		VectorCopy(end, move);
		VectorSubtract(start, end, vec);
	}
	else
	{
		VectorCopy(start, move);
		VectorSubtract(end, start, vec);
	}

	float len = VectorNormalize(vec);
	len = min(len, cl_rail_length->value); // Cap length

	vec3_t right, up;
	MakeNormalVectors(vec, right, up);

	VectorScale(vec, cl_rail_space->value * cl_particle_scale->value, vec);

	//mxd
	vec3_t color;
	if (isred)
		VectorSet(color, 255, 20, 20);
	else
		VectorSet(color, cl_railred->value, cl_railgreen->value, cl_railblue->value);

	for (int i = 0; i < len; i += cl_rail_space->value * cl_particle_scale->value)
	{
		const float d = i * 0.1f;
		const float c = cosf(d);
		const float s = sinf(d);

		vec3_t dir;
		VectorScale(right, c, dir);
		VectorMA(dir, s, up, dir);

		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + dir[j] * 3;
			p->vel[j] = dir[j] * 6;
		}

		VectorCopy(color, p->color);
		p->alphavel = -1.0f;
		p->size = 3;
		p->type = particle_generic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

#define DEVRAIL_SIZE	7.5f

static void CL_ParticleDevRailThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	vec3_t len;
	VectorSubtract(p->angle, org, len);
	
	*size *= (float)(DEVRAIL_SIZE / VectorLength(len)) * 0.5f / (4 - *size);
	*size = min(*size, DEVRAIL_SIZE); //mxd

	// Setting up angle for sparks
	const float time1 = *time;
	const float time2 = time1 * time1;

	for (int i = 0; i < 2; i++)
		angle[i] = 3 * (p->vel[i] * time1 + p->accel[i] * time2);
	angle[2] = 3 * (p->vel[2] * time1 + (p->accel[2] - PARTICLE_GRAVITY) * time2);
}

static void CL_DevRailTrail(const vec3_t start, const vec3_t end, const qboolean isred)
{
	// Draw from closest point
	vec3_t move, vec;
	if (FartherPoint(start, end))
	{
		VectorCopy(end, move);
		VectorSubtract(start, end, vec);
	}
	else
	{
		VectorCopy(start, move);
		VectorSubtract(end, start, vec);
	}

	float len = VectorNormalize(vec);
	len = min(len, cl_rail_length->value); // Cap length

	vec3_t point;
	VectorCopy(vec, point);

	const int dec = 4;
	VectorScale(vec, dec, vec);
	
	vec3_t color; //mxd
	if (isred)
		VectorSet(color, 255, 20, 20);
	else
		VectorSet(color, cl_railred->value, cl_railgreen->value, cl_railblue->value);

	cparticle_t *p;
	int step = 0;
	for (int i = 0; i < len; i += dec)
	{
		step++;

		if (step >= DEVRAILSTEPS)
		{
			for (step = 3; step > 0; step--)
			{
				p = CL_InitParticle();
				if (!p)
					return;

				VectorCopy(point, p->angle);
				VectorCopy(move, p->org);
				VectorCopy(color, p->color);
				VectorSet(p->colorvel, 0, -90, -30);
				p->alpha = 0.75f;
				p->alphavel = -0.75f;
				p->size = dec * DEVRAILSTEPS * TWOTHIRDS;
				p->type = particle_beam2;
				p->flags = PART_DIRECTION;

				CL_FinishParticleInit(p);
			}
		}

		p = CL_InitParticle();
		if (!p)
			return;

		VectorCopy(move, p->org);
		VectorSet(p->vel, crand() * 10, crand() * 10, crand() * 10 + 20);
		VectorCopy(color, p->color);
		p->alphavel = -0.75f / (0.5f + frand() * 0.3f);
		p->size = 2;
		p->sizevel = -0.25f;
		p->type = particle_solid;
		p->flags = (PART_GRAVITY | PART_SPARK);
		p->think = CL_ParticleDevRailThink;

		CL_FinishParticleInit(p);
		
		p = CL_InitParticle();
		if (!p)
			return;

		VectorSet(p->angle, crand() * 180, crand() * 100, 0);
		VectorCopy(move, p->org);
		VectorSet(p->vel, crand() * 10, crand() * 10, crand() * 10 + 20);
		VectorSet(p->accel, 0, 0, 5);
		p->alpha = 0.25f;
		p->alphavel = -0.25f;
		p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
		p->size = 5;
		p->sizevel = 10;
		p->type = particle_smoke;
		p->flags = (PART_TRANS | PART_GRAVITY | PART_OVERBRIGHT);
		p->think = CL_ParticleRotateThink;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

void CL_RailTrail(const vec3_t start, const vec3_t end, const qboolean isred)
{
	vec3_t vec;
	VectorSubtract(end, start, vec);
	VectorNormalize(vec);
	CL_ParticleRailDecal(end, vec, 7, isred);

	//mxd. Classic particles (we still want decal, because it's controlled by a different cvar)
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicRailTrail(start, end, isred);
		return;
	}

	if (cl_railtype->value == 2)
	{
		CL_DevRailTrail(start, end, isred);
		return;
	}

	// Draw from closest point
	vec3_t move;
	if (FartherPoint(start, end))
	{
		VectorCopy(end, move);
		VectorSubtract(start, end, vec);
	}
	else
	{
		VectorCopy(start, move);
		VectorSubtract(end, start, vec);
	}

	float len = VectorNormalize(vec);
	if (cl_railtype->value == 0)
		len = min(len, cl_rail_length->value); // Cap length

	vec3_t point;
	VectorCopy(vec, point);
	VectorScale(vec, RAILTRAILSPACE, vec);

	const qboolean colored = (cl_railtype->value != 0);

	vec3_t color;
	if (colored)
	{
		if (isred)
			VectorSet(color, 255, 20, 20);
		else
			VectorSet(color, cl_railred->value, cl_railgreen->value, cl_railblue->value);
	}
	else
	{
		VectorSetAll(color, 255);
	}

	for (int i = 0; i < len; i += RAILTRAILSPACE)
	{
		vec3_t last;
		VectorCopy(move, last);
		VectorAdd(move, vec, move);

		for (int j = 0; j < 3; j++)
		{
			cparticle_t *p = CL_InitParticle();
			if (!p)
				return;

			VectorCopy(last, p->angle);
			VectorCopy(move, p->org);
			VectorCopy(color, p->color);
			p->alpha = 0.75f;
			p->alphavel = -0.75f;
			p->size = RAILTRAILSPACE * TWOTHIRDS;
			p->sizevel = (colored ? 0 : -5);
			p->type = particle_beam2;
			p->flags = PART_BEAM;

			CL_FinishParticleInit(p);
		}
	}

	if (cl_railtype->value == 0)
		CL_RailSprial(start, end, isred);
}

void CL_IonripperTrail(const vec3_t start, const vec3_t end)
{
	//mxd. Classic particles
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicIonripperTrail(start, end);
		return;
	}
	
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	const int dec = 3 * cl_particle_scale->value;
	VectorScale(vec, dec, vec);

	for (int i = 0; i < len; i += dec)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		VectorCopy(move, p->org);
		VectorSet(p->color, 255, 75, 0);
		p->alpha = 0.75f;
		p->alphavel = -1.0f / (0.3f + frand() * 0.2f);
		p->size = 3;
		p->type = particle_generic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

void CL_BubbleTrail(const vec3_t start, const vec3_t end)
{
	//mxd. Classic particles
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicBubbleTrail(start, end);
		return;
	}
	
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);

	const float len = VectorNormalize(vec);
	const float dec = 32;
	VectorScale(vec, dec, vec);

	for (int i = 0; i < len; i += dec)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand() * 2;
			p->vel[j] = crand() * 5;
		}

		p->alpha = 0.75f;
		p->alphavel = -0.5f / (1.0f + frand() * 0.2f);
		p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
		p->size = (frand() > 0.25f ? 1 : (frand() > 0.5f ? 2 : (frand() > 0.75f ? 3 : 4)));
		p->sizevel = 1;
		p->type = particle_bubble;
		p->flags = (PART_TRANS | PART_SHADED);

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

#define BEAMLENGTH	16

static void CL_FlyParticles(const vec3_t origin, int count)
{
	count = min(NUMVERTEXNORMALS, count);

	if (!avelocities[0][0])
	{
		for (int i = 0; i < NUMVERTEXNORMALS; i++)
			for (int c = 0; c < 3; c++)
				avelocities[i][c] = (rand() & 255) * 0.01f;
	}

	vec3_t forward;
	const float ltime = cl.time / 1000.0f;

	for (int i = 0; i < count; i += 2)
	{
		float angle = ltime * avelocities[i][0];
		const float sy = sinf(angle);
		const float cy = cosf(angle);

		angle = ltime * avelocities[i][1];
		const float sp = sinf(angle);
		const float cp = cosf(angle);
	
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		const float dist = sinf(ltime + i) * 64;

		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
			p->org[j] = origin[j] + vertexnormals[i][j] * dist + forward[j] * BEAMLENGTH;

		VectorClear(p->color);
		p->alphavel = -100;
		p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
		p->size = 1 + sinf(i + ltime);
		p->sizevel = 1;
		p->type = particle_generic;
		p->flags = PART_TRANS;

		CL_FinishParticleInit(p);
	}
}

// Corpse files effect
void CL_FlyEffect(centity_t *ent, const vec3_t origin)
{
	int count;
	int starttime;

	if (ent->fly_stoptime < cl.time)
	{
		starttime = cl.time;
		ent->fly_stoptime = cl.time + 60000;
	}
	else
	{
		starttime = ent->fly_stoptime - 60000;
	}

	int n = cl.time - starttime;
	if (n < 20000)
	{
		count = n * 162 / 20000.0f;
	}
	else
	{
		n = ent->fly_stoptime - cl.time;
		if (n < 20000)
			count = n * 162 / 20000.0f;
		else
			count = 162;
	}

	CL_FlyParticles(origin, count);
}

void CL_BfgParticles(const entity_t *ent)
{
	if (!avelocities[0][0])
	{
		for (int i = 0; i < NUMVERTEXNORMALS; i++)
			for (int c = 0; c < 3; c++)
				avelocities[i][c] = (rand() & 255) * 0.01f;
	}

	vec3_t v, forward;
	float dist = 64;
	const float ltime = cl.time / 1000.0f;

	for (int i = 0; i < NUMVERTEXNORMALS; i++)
	{
		float angle = ltime * avelocities[i][0];
		const float sy = sinf(angle);
		const float cy = cosf(angle);

		angle = ltime * avelocities[i][1];
		const float sp = sinf(angle);
		const float cp = cosf(angle);
	
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		const float dist2 = dist;
		dist = sinf(ltime + i) * 64;

		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		VectorCopy(ent->origin, p->angle);

		for (int j = 0; j < 3; j++)
			p->org[j] = ent->origin[j] + vertexnormals[i][j] * dist + forward[j] * BEAMLENGTH;

		VectorSet(p->color, 50, 200 * dist2, 20);
		p->alphavel = -100;

		vec3_t len;
		VectorSubtract(p->angle, p->org, len);
		p->size = 300.0f / VectorLength(len) * 0.75f;

		p->sizevel = 1;
		p->type = particle_generic;

		CL_FinishParticleInit(p);

		VectorSubtract(p->org, ent->origin, v);
		dist = VectorLength(v) / 90.0f;
	}
}

// RAFAEL
void CL_TrapParticles(entity_t *ent)
{
	ent->origin[2] -= 14;

	vec3_t start, end;
	VectorCopy(ent->origin, start);
	VectorCopy(ent->origin, end);
	end[2] += 64;

	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	const int dec = 5;
	VectorScale(vec, dec, vec);

	for (int i = 0; i < len; i += dec)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand();
			p->vel[j] = crand() * 15;
		}

		VectorSet(p->accel, 0, 0, PARTICLE_GRAVITY);
		VectorSet(p->color, 230 + crand() * 25, 125 + crand() * 25, 25 + crand() * 25);
		p->alphavel = -1.0f / (0.3f + frand() * 0.2f);
		p->size = 3;
		p->sizevel = -3;
		p->type = particle_generic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}

	ent->origin[2] += 14;

	for (int i = -2; i <= 2; i += 4)
	{
		for (int j = -2; j <= 2; j += 4)
		{
			for (int k = -2; k <= 4; k += 4)
			{
				vec3_t dir = { j * 8, i * 8, k * 8 };
				VectorNormalize(dir);
				VectorScale(dir, 50 + rand() & 63, dir);

				cparticle_t *p = CL_InitParticle();
				if (!p)
					return;

				VectorSet(p->org, ent->origin[0] + i + ((rand() & 23) * crand()),
								  ent->origin[1] + j + ((rand() & 23) * crand()),
								  ent->origin[2] + k + ((rand() & 23) * crand()));

				VectorCopy(dir, p->vel);
				VectorSet(p->color, 230 + crand() * 25, 125 + crand() * 25, 25 + crand() * 25);
				p->alphavel = -1.0f / (0.3f + frand() * 0.2f);
				p->sizevel = 1;
				p->type = particle_generic;
				p->flags = PART_GRAVITY;

				CL_FinishParticleInit(p);
			}
		}
	}
}

void CL_BFGExplosionParticles(const vec3_t org)
{
	for (int i = 0; i < 256; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() % 32) - 16);
			p->vel[j] = (rand() % 150) - 75;
		}

		VectorSet(p->color, 50, 100 + rand() * 50, 0); //Knightmare- made more green
		p->alphavel = -0.8f / (0.5f + frand() * 0.3f);
		p->size = 10;
		p->sizevel = -10;
		p->type = particle_generic;
		p->flags = PART_GRAVITY;

		CL_FinishParticleInit(p);
	}
}

void CL_TeleportParticles(const vec3_t org)
{
	for (int i = -16; i <= 16; i += 4)
	{
		for (int j = -16; j <= 16; j += 4)
		{
			for (int k = -16; k <= 32; k += 4)
			{
				vec3_t dir = { j * 16, i * 16, k * 16 };
				VectorNormalize(dir);
				VectorScale(dir, 150 + (rand() & 63), dir);

				vec3_t color;
				if (r_particle_mode->integer == 1) //mxd
					VectorSet(color, 200 + 55 * rand(), 200 + 55 * rand(), 200 + 55 * rand());
				else
					color8_to_vec3(7 + (rand() & 7), color);

				cparticle_t *p = CL_InitParticle();
				if (!p)
					return;

				VectorSet(p->org, org[0] + i + (rand() & 3),
								  org[1] + j + (rand() & 3),
								  org[2] + k + (rand() & 3));

				VectorCopy(dir, p->vel);
				VectorCopy(color, p->color);
				p->alphavel = -1.0f / (0.3f + (rand() & 7) * 0.02f);
				p->sizevel = 3;
				p->type = particle_generic;
				p->flags = PART_GRAVITY;

				CL_FinishParticleInit(p);
			}
		}
	}
}

// Flash of light
void CL_ColorFlash(const vec3_t pos, const int ent, const int intensity, const vec3_t color1)
{
	cdlight_t *dl = CL_AllocDlight(ent);
	VectorCopy(pos, dl->origin);
	dl->radius = intensity;
	dl->minlight = 250;
	dl->die = cl.time + 100;
	VectorCopy(color1, dl->color); //mxd
}

void CL_Flashlight(const int ent, const vec3_t pos)
{
	CL_ColorFlash(pos, ent, 400, tv(1, 1, 1)); //mxd
}

void CL_DebugTrail(const vec3_t start, const vec3_t end)
{
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	const float dec = 8; // Was 2
	VectorScale(vec, dec, vec);
	VectorCopy(start, move);

	for (int i = 0; i < len; i += dec)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		VectorCopy(move, p->org);
		VectorSet(p->color, 50, 50, 255);
		p->alphavel = -0.75f;
		p->size = 7.5f;
		p->type = particle_generic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

void CL_ForceWall(const vec3_t start, const vec3_t end, const int color8)
{
	vec3_t color;
	color8_to_vec3(color8, color);
	
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	const int dec = 4;
	VectorScale(vec, dec, vec);

	for (int i = 0; i < len; i += dec)
	{
		if (frand() > 0.3f)
		{
			cparticle_t *p = CL_InitParticle();
			if (!p)
				return;

			for (int j = 0; j < 3; j++)
			{
				p->org[j] = move[j] + crand() * 3;
				p->color[j] = color[j] + 5;
			}

			p->vel[2] = -40 - (crand() * 10);
			p->alphavel = -1.0f / (3.0f + frand() * 0.5f);
			p->size = 5;
			p->type = particle_generic;

			CL_FinishParticleInit(p);
		}

		VectorAdd(move, vec, move);
	}
}

// Lets you control the # of bubbles by setting the distance between the spawns)
void CL_BubbleTrail2(const vec3_t start, const vec3_t end, const int dist)
{
	//mxd. Classic particles
	if(r_particle_mode->integer == 0)
	{
		CL_ClassicBubbleTrail2(start, end, dist);
		return;
	}
	
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);

	const float len = VectorNormalize(vec);
	VectorScale(vec, dist, vec);

	for (int i = 0; i < len; i += dist)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand() * 2;
			p->vel[j] = crand() * 5;
		}

		p->org[2] -= 4;
		p->vel[2] += 6;

		p->alpha = 0.75f;
		p->alphavel = -0.5f / (1.0f + frand() * 0.2f);
		p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
		p->size = (frand() > 0.25f ? 1 : (frand() > 0.5f ? 2 : (frand() > 0.75f ? 3 : 4)));
		p->sizevel = 1;
		p->type = particle_bubble;
		p->flags = (PART_TRANS | PART_SHADED);

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

void CL_HeatbeamParticles(const vec3_t start, const vec3_t forward)
{
	float ltime;
	float step;
	float variance;
	float size;
	int maxsteps;

	vec3_t end;
	VectorMA(start, 4096, forward, end);

	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	// FIXME - pmm - these might end up using old values?
	vec3_t right, up;
	VectorCopy(cl.v_right, right);
	VectorCopy(cl.v_up, up);

	if (cg_thirdperson->integer)
	{
		ltime = cl.time / 250.0f;
		step = 96;
		maxsteps = 10;
		variance = 1.2f;
		size = 2;
	}
	else
	{
		ltime = cl.time / 1000.0f;
		step = 32;
		maxsteps = 7;
		variance = 0.5f;
		size = 1;
	}

	const float start_pt = fmodf(ltime * 96.0f, step);
	VectorMA(move, start_pt, vec, move);

	VectorScale(vec, step, vec);

	const float rstep = M_PI / 10.0f * min(cl_particle_scale->value, 2);
	for (int i = start_pt; i < len; i += step)
	{
		if (i > step * maxsteps) // Don't bother after the nth ring
			break;

		for (float rot = 0; rot < M_PI * 2; rot += rstep)
		{
			const float c = cosf(rot) * variance;
			const float s = sinf(rot) * variance;
			
			// Trim it so it looks like it's starting at the origin
			vec3_t dir;
			if (i < 10)
			{
				VectorScale(right, c * (i / 10.0f), dir);
				VectorMA(dir, s * (i / 10.0f), up, dir);
			}
			else
			{
				VectorScale(right, c, dir);
				VectorMA(dir, s, up, dir);
			}

			cparticle_t *p = CL_InitParticle();
			if (!p)
				return;

			for (int j = 0; j < 3; j++)
				p->org[j] = move[j] + dir[j] * 2; //Knightmare- decreased radius

			VectorSet(p->color, 200 + rand() * 50, 200 + rand() * 25, rand() * 50);
			p->alpha = 0.25f; // Decreased alpha
			p->alphavel = -1000.0f;
			p->size = size; // Shrunk size
			p->sizevel = 1;
			p->type = particle_blaster;

			CL_FinishParticleInit(p);
		}

		VectorAdd(move, vec, move);
	}
}

// Puffs with velocity along direction, with some randomness thrown in
void CL_ParticleSteamEffect(const vec3_t org, const vec3_t dir, const vec3_t color, const vec3_t colorvel, const int count, const int magnitude)
{
	vec3_t r, u;
	MakeNormalVectors(dir, r, u);

	for (int i = 0; i < count; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
			p->org[j] = org[j] + magnitude * 0.1f * crand();

		VectorCopy(color, p->color);
		VectorCopy(colorvel, p->colorvel);
		p->alpha = 0.5f;
		p->alphavel = -1.0f / (0.5f + frand() * 0.3f);
		p->size = 4;
		p->sizevel = -2;
		p->type = particle_smoke;

		VectorScale(dir, magnitude, p->vel);
		float d = crand() * magnitude / 3;
		VectorMA(p->vel, d, r, p->vel);
		d = crand() * magnitude / 3;
		VectorMA(p->vel, d, u, p->vel);

		CL_FinishParticleInit(p);
	}
}

// Puffs with velocity along direction, with some randomness thrown in
void CL_ParticleSteamEffect2(cl_sustain_t *self)
{
	vec3_t dir, r, u;
	VectorCopy(self->dir, dir);
	MakeNormalVectors(dir, r, u);
	
	for (int i = 0; i < self->count; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
			p->org[j] = self->org[j] + self->magnitude * 0.1f * crand();

		color8_to_vec3(self->color + (rand() & 7), p->color);
		p->alphavel = -1.0f / (0.5f + frand() * 0.3f);
		p->size = 4;
		p->type = particle_smoke;
		p->flags = PART_GRAVITY;

		VectorScale(dir, self->magnitude, p->vel);
		float d = crand() * self->magnitude / 3;
		VectorMA(p->vel, d, r, p->vel);
		d = crand() * self->magnitude / 3;
		VectorMA(p->vel, d, u, p->vel);

		CL_FinishParticleInit(p);
	}

	self->nextthink += self->thinkinterval;
}

void CL_TrackerTrail(const vec3_t start, const vec3_t end)
{
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	vec3_t forward, up, angle_dir;
	VectorCopy(vec, forward);
	vectoangles(forward, angle_dir);
	AngleVectors(angle_dir, forward, NULL, up);

	const float dec = 3 * max(cl_particle_scale->value / 2, 1);
	VectorScale(vec, dec, vec);

	for (int i = 0; i < len; i += dec)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		p->vel[2] = 5;
		VectorClear(p->color);
		p->alphavel = -2.0f;
		p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
		p->size = 2;
		p->type = particle_generic;
		p->flags = PART_TRANS;

		const float dist = DotProduct(move, forward);
		VectorMA(move, 8 * cosf(dist), up, p->org);

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

void CL_Tracker_Shell(const vec3_t origin)
{
	for(int i = 0; i < 300 / cl_particle_scale->value; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		VectorClear(p->color);
		p->alphavel = -2.0f;
		p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
		p->type = particle_generic;
		p->flags = PART_TRANS;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
		VectorMA(origin, 40, dir, p->org);

		CL_FinishParticleInit(p);
	}
}

void CL_MonsterPlasma_Shell(const vec3_t origin)
{
	for(int i = 0; i < 40; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		VectorSet(p->color, 220, 140, 50); //Knightmare- this was all black
		p->alphavel = INSTANT_PARTICLE;
		p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
		p->size = 2;
		p->type = particle_generic;
		p->flags = (PART_TRANS | PART_INSTANT);

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
	
		VectorMA(origin, 10, dir, p->org);

		CL_FinishParticleInit(p);
	}
}

void CL_Widowbeamout(const cl_sustain_t *self)
{
	static vec3_t colortable[] = 
	{
		{ 125, 185, 255 },
		{ 255, 125, 185 },
		{ 185, 255, 125 },
		{ 125, 255, 185 },
		{ 185, 125, 255 },
		{ 255, 185, 125 }
	};
	
	//mxd. Classic particles
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicWidowbeamout(self);
		return;
	}

	const float ratio = 1.0f - ((self->endtime - cl.time) / 2100.0f);

	for (int i = 0; i < 300; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		const int index = rand() & 5;
		VectorCopy(colortable[index], p->color);
		p->alphavel = INSTANT_PARTICLE;
		p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
		p->size = 2;
		p->type = particle_generic;
		p->flags = (PART_TRANS | PART_INSTANT);

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
	
		VectorMA(self->org, (45.0f * ratio), dir, p->org);

		CL_FinishParticleInit(p);
	}
}

void CL_Nukeblast(const cl_sustain_t *self)
{
	static vec3_t colortable[] = 
	{
		{ 185, 185, 255 },
		{ 155, 155, 255 },
		{ 125, 125, 255 },
		{ 95,  95,  255 }
	};
	
	//mxd. Classic particles...
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicNukeblast(self);
		return;
	}

	const float ratio = 1.0f - ((self->endtime - cl.time) / 1000.0f);
	const float size = ratio * ratio;

	for(int i = 0; i < (700 / cl_particle_scale->value); i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		const int index = rand() & 3;
		VectorCopy(colortable[index], p->color);
		p->alpha = 1.0f - size;
		p->alphavel = INSTANT_PARTICLE;
		p->size = 10 * (0.5f + ratio * 0.5f);
		p->sizevel = -1;
		p->type = particle_generic;
		p->flags = PART_INSTANT;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
		VectorScale(dir, -1, p->angle);

		VectorMA(self->org, 200.0f * size, dir, p->org); // Was 100
		VectorMA(vec3_origin, 400.0f * size, dir, p->vel); // Was 100

		CL_FinishParticleInit(p);
	}
}

void CL_WidowSplash(const vec3_t org)
{
	//mxd. Classic particles
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicWidowSplash(org);
		return;
	}
	
	for (int i = 0; i < 256; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		VectorSet(p->color, rand() & 255, rand() & 255, rand() & 255);
		p->alphavel = -0.8f / (0.5f + frand() * 0.3f);
		p->size = 3;
		p->type = particle_generic;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand()); //mxd
		VectorNormalize(dir);

		VectorMA(org, 45.0f, dir, p->org);
		VectorMA(vec3_origin, 40.0f, dir, p->vel);

		CL_FinishParticleInit(p);
	}
}

void CL_Tracker_Explode(const vec3_t origin)
{
	for (int i = 0; i < (300 / cl_particle_scale->value); i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		p->accel[2] = -10;
		p->alphavel = -0.5f;
		p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
		p->size = 2;
		p->type = particle_generic;
		p->flags = PART_TRANS;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand()); //mxd
		VectorNormalize(dir);

		VectorCopy(origin, p->org); //Knightmare- start at center, not edge
		VectorScale(dir, crand() * 128, p->vel); // Was backdir, 64

		CL_FinishParticleInit(p);
	}
}

void CL_TagTrail(const vec3_t start, const vec3_t end, const int color8)
{
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	const int dec = 5;
	VectorScale(vec, dec, vec);

	for (int i = 0; i < len; i += dec)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand() * 16;
			p->vel[j] = crand() * 5;
		}

		p->accel[2] = 20;
		color8_to_vec3(color8, p->color);
		p->alphavel = -1.0f / (0.8f + frand() * 0.2f);
		p->size = 1.5f;
		p->type = particle_generic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

void CL_ColorExplosionParticles(const vec3_t org, const int color8, const int run)
{
	for (int i = 0; i < 128; i++) //mxd. Classic implementation
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		color8_to_vec3(color8 + (rand() % run), p->color);

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() % 32) - 16);
			p->vel[j] = (rand() % 256) - 128;
		}

		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -0.4f / (0.6f + frand() * 0.2f);
		p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
		p->size = 2;
		p->type = particle_generic;

		CL_FinishParticleInit(p);
	}
}

// Like the steam effect, but unaffected by gravity
void CL_ParticleSmokeEffect(const vec3_t org, const vec3_t dir, const float size)
{
	cparticle_t *p = CL_InitParticle();
	if (!p)
		return;

	VectorSet(p->angle, crand() * 180, crand() * 100, 0);
	VectorCopy(org, p->org);
	VectorCopy(dir, p->vel);
	p->accel[2] = 10;
	p->alpha = frand() * 0.25f + 0.75f;
	p->alphavel = -1.0f;
	p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
	p->size = size;
	p->sizevel = 5;
	p->type = particle_smoke;
	p->flags = (PART_TRANS | PART_SHADED | PART_OVERBRIGHT);
	p->think = CL_ParticleRotateThink;

	CL_FinishParticleInit(p);
}

static void CL_ParticleElectricSparksThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	const float time1 = *time;
	const float time2 = time1 * time1;

	for (int i = 0; i < 2; i++)
		angle[i] = 0.25f * (p->vel[i] * time1 + p->accel[i] * time2);
	angle[2] = 0.25f * (p->vel[2] * time1 + (p->accel[2] - PARTICLE_GRAVITY) * time2);
}

// New sparks for Rogue turrets
void CL_ElectricParticles(const vec3_t org, const vec3_t dir, const int count)
{
	for (int i = 0; i < count; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		const float d = rand() & 31;
		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() & 7) - 4) + d * dir[j];
			p->vel[j] = crand() * 20;
		}

		p->accel[2] = -PARTICLE_GRAVITY;
		VectorSet(p->color, 25, 100, 255);
		VectorSetAll(p->colorvel, 50);
		p->alphavel = -1.0f / (0.5f + frand() * 0.3f);
		p->size = 6;
		p->sizevel = -3;
		p->type = particle_solid;
		p->flags = (PART_GRAVITY | PART_SPARK);
		p->think = CL_ParticleElectricSparksThink;

		CL_FinishParticleInit(p);
	}
}