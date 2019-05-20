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

static vec3_t avelocities [NUMVERTEXNORMALS];


//=================================================

/*
===============
CL_LightningBeam
===============
*/
/*void CL_LightningBeam(vec3_t start, vec3_t end, int srcEnt, int dstEnt, float size)
{
	cparticle_t *list;
	cparticle_t *p=NULL;
	
	for (list=active_particles; list; list=list->next)
		if (list->src_ent == srcEnt && list->dst_ent == dstEnt )
		{
			p=list;
			p->time = cl.time;
			VectorCopy(start, p->angle);
			VectorCopy(end, p->org);

			if (p->link)
			{
				p->link->time = cl.time;
				VectorCopy(start, p->link->angle);
				VectorCopy(end, p->link->org);
			}
			else
			{
				p->link = CL_SetupParticle (
					start[0],	start[1],	start[2],
					end[0],		end[1],		end[2],
					0,	0,	0,
					0,		0,		0,
					150,	150,	255,
					0,	0,	0,
					1,		-1.0,
					size,		0,
					particle_beam,
					PART_BEAM,
					NULL,0);
				
			}
			break;
		}

	if (p)
		return;

	p = CL_SetupParticle (
		start[0],	start[1],	start[2],
		end[0],		end[1],		end[2],
		0,	0,	0,
		0,		0,		0,
		115,	115,	255,
		0,	0,	0,
		1,		-1.0,
		size,		0,
		particle_lightning,
		PART_LIGHTNING,
		NULL,0);

	p->src_ent=srcEnt;
	p->dst_ent=dstEnt;

	p->link = CL_SetupParticle (
		start[0],	start[1],	start[2],
		end[0],		end[1],		end[2],
		0,	0,	0,
		0,		0,		0,
		150,	150,	255,
		0,	0,	0,
		1,		-1.0,
		size,		0,
		particle_beam,
		PART_BEAM,
		NULL,0);
}*/

void CL_LightningBeam (vec3_t start, vec3_t end, int srcEnt, int dstEnt, float size)
{
	cparticle_t *p;

	for (cparticle_t *list = active_particles; list; list = list->next)
	{
		if (list->src_ent == srcEnt && list->dst_ent == dstEnt && list->image == particle_lightning)
		{
			p = list;
			p->time = cl.time;
			VectorCopy(start, p->angle);
			VectorCopy(end, p->org);

			return;
		}
	}

	p = CL_SetupParticle(
		start[0], start[1], start[2],
		end[0], end[1], end[2],
		0, 0, 0,
		0, 0, 0,
		255, 255, 255,
		0, 0, 0,
		1, -2,
		GL_SRC_ALPHA, GL_ONE,
		size, 0,
		particle_lightning,
		PART_LIGHTNING,
		0, false);

	if (!p)
		return;

	p->src_ent = srcEnt;
	p->dst_ent = dstEnt;
}

/*
===============
CL_Explosion_Decal
===============
*/
void CL_Explosion_Decal (vec3_t org, float size, int decalnum)
{
	if (r_decals->value)
	{
		const int offset = 8;	//size/2
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
			VectorMA(org, -offset, angle[i], sorg); // move origin 8 units back
			VectorMA(sorg, size / 2 + offset, angle[i], end1);
			trace_t trace1 = CL_Trace(sorg, end1, 0, CONTENTS_SOLID);
			
			if (trace1.fraction < 1) // hit a surface
			{	
				// make sure we haven't hit this plane before
				VectorCopy(trace1.plane.normal, planenormals[i]);

				for (int j = 0; j < i; j++)
					if (VectorCompare(planenormals[j], planenormals[i])) 
						goto skip; //mxd. Actually skip the plane

				// try tracing directly to hit plane
				VectorNegate(trace1.plane.normal, normal);
				VectorMA(sorg, size / 2, normal, end2);
				trace_t trace2 = CL_Trace(sorg, end2, 0, CONTENTS_SOLID);

				// if second trace hit same plane
				if (trace2.fraction < 1 && VectorCompare(trace2.plane.normal, trace1.plane.normal))
					VectorCopy(trace2.endpos, dorg);
				else
					VectorCopy(trace1.endpos, dorg);

				//if (CM_PointContents(dorg,0) & MASK_WATER) // no scorch marks underwater
				//	continue;
				VecToAngleRolled(normal, rand() % 360, ang);
				CL_SetupParticle(
					ang[0], ang[1], ang[2],
					dorg[0], dorg[1], dorg[2],
					0, 0, 0,
					0, 0, 0,
					255, 255, 255,
					0, 0, 0,
					1, -1 / r_decal_life->value,
					GL_ZERO, GL_ONE_MINUS_SRC_ALPHA,
					size, 0,
					decalnum, // particle_burnmark
					PART_SHADED | PART_DECAL | PART_ALPHACOLOR,
					CL_DecalAlphaThink, true);
			}

			skip:; //mxd
		}
	}
}


/*
===============
CL_ExplosionThink
===============
*/
void CL_ExplosionThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	if (*alpha > 0.85)
		*image = particle_rexplosion1;
	else if (*alpha > 0.7)
		*image = particle_rexplosion2;
	else if (*alpha > 0.5)
		*image = particle_rexplosion3;
	else if (*alpha > 0.4)
		*image = particle_rexplosion4;
	else if (*alpha > 0.25)
		*image = particle_rexplosion5;
	else if (*alpha > 0.1)
		*image = particle_rexplosion6;
	else 
		*image = particle_rexplosion7;

	*alpha *= 3.0;

	if (*alpha > 1.0)
		*alpha = 1;

	p->thinknext = true;
}

/*
===============
CL_ExplosionBubbleThink
===============
*/
void CL_ExplosionBubbleThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{

	if (CM_PointContents(org, 0) & MASK_WATER)
	{
		p->thinknext = true;
	}
	else
	{
		p->think = NULL;
		p->alpha = 0;
	}
}

/*
===============
CL_Explosion_Particle

Explosion effect
===============
*/
void CL_Explosion_Particle (vec3_t org, float size, qboolean rocket)
{
	cparticle_t *p = CL_SetupParticle(
		0, 0, 0,
		org[0], org[1], org[2],
		0, 0, 0,
		0, 0, 0,
		255, 255, 255,
		0, 0, 0,
		1, (rocket) ? -2 : -1.5,
		GL_SRC_ALPHA, GL_ONE,
		//GL_ONE, GL_ONE,
		(size != 0 ? size : (150 - (rocket ? 0 : 75))), 0, //mxd. Operator '?:' has lower precedence than '-'; '-' will be evaluated first
		particle_rexplosion1,
		PART_DEPTHHACK_SHORT,
		CL_ExplosionThink, true);
	
	if (p)
		CL_AddParticleLight(p, 300, 0, 1, 0.514, 0);
}

/*
===============
CL_ExplosionParticles (mxd. Vanilla version)
===============
*/
void CL_ExplosionParticles(const vec3_t org)
{
	for (int i = 0; i < 256; i++)
	{
		const int color8 = 0xe0 + (rand() & 7);
		vec3_t color = { color8red(color8), color8green(color8), color8blue(color8) };
		
		vec3_t origin, velocity;
		for (int j = 0; j < 3; j++)
		{
			origin[j] = org[j] + ((rand()%32) - 16);
			velocity[j] = (rand()%384) - 192;
		}
		
		CL_SetupParticle(
			0, 0, 0,
			origin[0], origin[1], origin[2],
			velocity[0], velocity[1], velocity[2],
			0, 0, 0,
			color[0], color[1], color[2],
			0, 0, 0,
			1.0, -0.8 / (0.5 + frand()*0.3),
			GL_SRC_ALPHA, GL_ONE,
			1, 0,
			particle_classic,
			PART_GRAVITY,
			NULL,
			false);
	}
}

/*
===============
CL_Explosion_FlashParticle

Explosion fash
===============
*/
void CL_Explosion_FlashParticle (vec3_t org, float size, qboolean large)
{
	const int flags = (large ? PART_DEPTHHACK_SHORT : 0); //mxd
	const int image = (large ? particle_rflash : particle_blaster);

	CL_SetupParticle(
		0, 0, 0,
		org[0], org[1], org[2],
		0, 0, 0,
		0, 0, 0,
		255, 175, 100,
		0, 0, 0,
		1, -1.75,
		GL_SRC_ALPHA, GL_ONE,
		(size != 0 ? size : 50), -10,
		image, flags,
		NULL, false);
}


/*
===============
CL_ParticleExplosionSparksThink
===============
*/
void CL_ParticleExplosionSparksThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	//setting up angle for sparks
	const float time1 = *time;
	const float time2 = time1 * time1;

	for (int i = 0; i < 2; i++)
		angle[i] = 0.25 * (p->vel[i] * time1 + p->accel[i] * time2);
	angle[2] = 0.25 * (p->vel[2] * time1 + (p->accel[2] - PARTICLE_GRAVITY) * time2);

	p->thinknext = true;
}

/*
===============
CL_Explosion_Sparks
===============
*/
void CL_Explosion_Sparks (vec3_t org, int size, int count)
{
	for (int i = 0; i < (count / cl_particle_scale->value); i++) // was 256
	{
		CL_SetupParticle (
			0,	0,	0,
			org[0] + ((rand()%size)-16),	org[1] + ((rand()%size)-16),	org[2] + ((rand()%size)-16),
			(rand()%150)-75,	(rand()%150)-75,	(rand()%150)-75,
			0,		0,		0,
			255,	100,	25,
			0,	0,	0,
			1,		-0.8 / (0.5 + frand()*0.3),
			GL_SRC_ALPHA, GL_ONE,
			size,		size*-1.5f,		// was 6, -9
			particle_solid,
			PART_GRAVITY|PART_SPARK,
			CL_ParticleExplosionSparksThink, true);
	}
}


/*
=====================

Blood effects

=====================
*/
void CL_ParticleBloodThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time);
void CL_BloodPuff (vec3_t org, vec3_t dir, int count);

#define MAXBLEEDSIZE 5
#define TIMEBLOODGROW 2.5f
#define BLOOD_DECAL_CHANCE 0.5F

/*
===============
CL_ParticleBloodDecalThink
===============
*/
void CL_ParticleBloodDecalThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{	// This REALLY slows things down
	/*if (*time<TIMEBLOODGROW)
	{
		vec3_t dir;

		*size *= sqrt(0.5 + 0.5*(*time/TIMEBLOODGROW));

		AngleVectors (angle, dir, NULL, NULL);
		VectorNegate(dir, dir);
		CL_ClipDecal(p, *size, angle[2], org, dir);
	}*/

	//now calc alpha
	CL_DecalAlphaThink (p, org, angle, alpha, size, image, time);
}

/*
===============
CL_ParticleBloodDropThink
===============
*/
void CL_ParticleBloodDropThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	CL_CalcPartVelocity(p, 0.2, time, angle);

	float length = VectorNormalize(angle);
	if (length > MAXBLEEDSIZE) length = MAXBLEEDSIZE;
	VectorScale(angle, -length, angle);

	//now to trace for impact...
	CL_ParticleBloodThink (p, org, angle, alpha, size, image, time);
}

/*
===============
CL_ParticleBloodPuffThink
===============
*/
void CL_ParticleBloodPuffThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	angle[2] = angle[0] + *time * angle[1] + *time * *time * angle[2];

	//now to trace for impact...
	CL_ParticleBloodThink (p, org, angle, alpha, size, image, time);
}

/*
===============
CL_ParticleBloodThink
===============
*/
void CL_ParticleBloodThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	trace_t trace = CL_Trace(p->oldorg, org, 0, CONTENTS_SOLID); // was 0.1
	qboolean became_decal = false;

	if (trace.fraction < 1.0) // delete and stain...
	{
		if (r_decals->value && (p->flags & PART_LEAVEMARK)
			&& !VectorCompare(trace.plane.normal, vec3_origin)
			&& !(CM_PointContents(p->oldorg, 0) & MASK_WATER)) // no blood splatters underwater...
		{
			vec3_t	normal, dir;
			qboolean greenblood = false;
			qboolean timedout = false;
			if (p->color[1] > 0 && p->color[2] > 0)
				greenblood = true;

			// time cutoff for gib trails
			if (p->flags & PART_GRAVITY && !(p->flags & PART_DIRECTION))
			{
				// gekk gibs go flyin faster...
				if (greenblood && (cl.time - p->time) * 0.001 > 1.0F)
					timedout = true;
				if (!greenblood && (cl.time - p->time) * 0.001 > 0.5F)
					timedout = true;
			}

			if (!timedout)
			{
				VectorNegate(trace.plane.normal, normal);
				VecToAngleRolled(normal, rand()%360, p->angle);
				
				VectorCopy(trace.endpos, p->org);
				VectorClear(p->vel);
				VectorClear(p->accel);
				p->image = CL_GetRandomBloodParticle();
				p->blendfunc_src = GL_SRC_ALPHA; //GL_ZERO
				p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA; //GL_ONE_MINUS_SRC_COLOR
				p->flags = PART_DECAL | PART_SHADED | PART_ALPHACOLOR;
				p->alpha = *alpha;
				p->alphavel = -1 / r_decal_life->value;

				if (greenblood)
					p->color[1] = 210;
				else
					VectorScale(p->color, 0.5, p->color); //mxd

				p->start = CL_NewParticleTime();
				p->think = CL_ParticleBloodDecalThink;
				p->thinknext = true;
				p->size = MAXBLEEDSIZE * 0.5 * (random() * 5.0 + 5);
				//p->size = *size*(random()*5.0+5);
				p->sizevel = 0;
				
				p->decalnum = 0;
				p->decal = NULL;
				AngleVectors (p->angle, dir, NULL, NULL);
				VectorNegate(dir, dir);
				CL_ClipDecal(p, p->size, -p->angle[2], p->org, dir);

				if (p->decalnum)
					became_decal = true;
				//else
				//	Com_Printf(S_COLOR_YELLOW"Blood decal not clipped!\n");
			}
		}

		if (!became_decal)
		{
			*alpha = 0;
			*size = 0;
			p->alpha = 0;
		}
	}

	VectorCopy(org, p->oldorg);

	p->thinknext = true;
}


/*
===============
CL_BloodSmack
===============
*/
void CL_BloodSmack (vec3_t org, vec3_t dir)
{
	CL_SetupParticle (
		crand()*180, crand()*100, 0,
		org[0],	org[1],	org[2],
		dir[0],	dir[1],	dir[2],
		0,		0,		0,
		255,	0,		0,
		0,		0,		0,
		1.0,		-1 / (0.5 + frand()*0.3), //was -0.75
		GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
		10,			0,			
		particle_redblood,
		PART_SHADED|PART_OVERBRIGHT,
		CL_ParticleRotateThink,true);

	CL_BloodPuff(org, dir, 1);
}


/*
===============
CL_BloodBleed
===============
*/
void CL_BloodBleed (vec3_t org, vec3_t dir, int count)
{
	vec3_t	pos;
	VectorScale(dir, 10, pos);

	for (int i = 0; i < count; i++)
	{
		for (int c = 0; c < 3; c++) //mxd
			pos[c] = dir[c] + random() * (cl_blood->value - 2) * 0.01;
		
		VectorScale(pos, 10 + (cl_blood->value-2) * 0.0001 * random(), pos);

		cparticle_t *p = CL_SetupParticle (
			org[0], org[1], org[2],
			org[0] + ((rand()&7)-4) + dir[0],	org[1] + ((rand()&7)-4) + dir[1],	org[2] + ((rand()&7)-4) + dir[2],
			pos[0]*(random()*3+5),	pos[1]*(random()*3+5),	pos[2]*(random()*3+5),
			0,		0,		0,
			255,	0,		0,
			0,		0,		0,
			0.7,		-0.25 / (0.5 + frand()*0.3),
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			MAXBLEEDSIZE*0.5,		0,			
			particle_blooddrip,
			PART_SHADED|PART_DIRECTION|PART_GRAVITY|PART_OVERBRIGHT,
			CL_ParticleBloodDropThink,true);

		if (p && i == 0 && random() < BLOOD_DECAL_CHANCE)
			p->flags |= PART_LEAVEMARK;
	}

}

/*
===============
CL_BloodPuff
===============
*/
void CL_BloodPuff (vec3_t org, vec3_t dir, int count)
{
	for (int i = 0; i < count; i++)
	{
		const float d = rand()&31;
		cparticle_t *p = CL_SetupParticle (
			crand()*180, crand()*100, 0,
			org[0] + ((rand()&7)-4) + d*dir[0],	org[1] + ((rand()&7)-4) + d*dir[1],	org[2] + ((rand()&7)-4) + d*dir[2],
			dir[0]*(crand()*3+5),	dir[1]*(crand()*3+5),	dir[2]*(crand()*3+5),
			0,			0,			-100,
			255,		0,			0,
			0,			0,			0,
			1.0,		-1.0,
			GL_SRC_ALPHA, GL_ONE,
			10,			0,
			particle_blood,
			PART_SHADED,
			CL_ParticleBloodPuffThink, true);

		if (p && i == 0 && random() < BLOOD_DECAL_CHANCE)
			p->flags |= PART_LEAVEMARK;
	}
}

/*
===============
CL_BloodHit
===============
*/
void CL_BloodHit (vec3_t org, vec3_t dir)
{
	if (cl_blood->value < 1) // disable blood option
		return;
	if (cl_blood->value == 2) // splat
		CL_BloodSmack(org, dir);
	else if (cl_blood->value == 3) // bleed
		CL_BloodBleed (org, dir, 6);
	else if (cl_blood->value == 4) // gore
		CL_BloodBleed (org, dir, 16);
	else // 1 = puff
		CL_BloodPuff(org, dir, 5);
}

/*
==================
CL_GreenBloodHit

green blood spray
==================
*/
void CL_GreenBloodHit (vec3_t org, vec3_t dir)
{
	if (cl_blood->value < 1) // disable blood option
		return;

	for (int i = 0; i < 5; i++)
	{
		const float d = rand()&31;
		cparticle_t *p = CL_SetupParticle (
			crand()*180, crand()*100, 0,
			org[0] + ((rand()&7)-4) + d*dir[0],	org[1] + ((rand()&7)-4) + d*dir[1],	org[2] + ((rand()&7)-4) + d*dir[2],
			dir[0]*(crand()*3+5),	dir[1]*(crand()*3+5),	dir[2]*(crand()*3+5),
			0,		0,		-100,
			255,	180,	50,
			0,		0,		0,
			1,		-1.0,
			GL_SRC_ALPHA, GL_ONE,
			10,			0,			
			particle_blood,
			PART_SHADED|PART_OVERBRIGHT,
			CL_ParticleBloodPuffThink, true);

		if (p && i == 0 && random() < BLOOD_DECAL_CHANCE)
			p->flags |= PART_LEAVEMARK;
	}

}

/*
===============
CL_ParticleEffect

Wall impact puffs
===============
*/
void CL_ParticleEffect (vec3_t org, vec3_t dir, int color8, int count)
{
	for (int i = 0; i < count; i++)
	{
		//mxd
		cparticle_t *p = CL_InitParticle();
		if (!p) return;

		color8_to_vec3(color8 + (rand()&7), p->color);

		const float d = rand()&31;
		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand()&7) - 4) + d * dir[j];
			p->vel[j] = crand() * 20;
		}

		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -1.0 / (0.5 + frand()*0.3);
		p->flags = PART_GRAVITY;
	}
}

/*
===============
CL_ParticleEffect2
===============
*/

#define colorAdd 25
void CL_ParticleEffect2 (vec3_t org, vec3_t dir, int color8, int count)
{
	for (int i = 0; i < count; i++)
	{
		//mxd
		cparticle_t *p = CL_InitParticle();
		if (!p) return;

		color8_to_vec3(color8, p->color);

		const float d = rand()&7;
		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand()&7) - 4) + d * dir[j];
			p->vel[j] = crand() * 20;

			if (r_particle_mode->integer != 0)
				p->color[j] += colorAdd;
		}

		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -1.0 / (0.5 + frand()*0.3);
		p->flags = PART_GRAVITY;
	}
}

// RAFAEL
/*
===============
CL_ParticleEffect3
===============
*/

void CL_ParticleEffect3 (vec3_t org, vec3_t dir, int color8, int count)
{
	for (int i = 0; i < count; i++)
	{
		//mxd
		cparticle_t *p = CL_InitParticle();
		if (!p) return;

		color8_to_vec3(color8, p->color);

		const float d = rand()&7;
		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand()&7) - 4) + d * dir[j];
			p->vel[j] = crand() * 20;

			if (r_particle_mode->integer != 0)
				p->color[j] += colorAdd;
		}

		p->accel[2] = PARTICLE_GRAVITY; //mxd. The only difference between this and CL_ParticleEffect2...
		p->alphavel = -1.0 / (0.5 + frand()*0.3);
		p->flags = PART_GRAVITY;
	}
}


/*
===============
CL_ParticleSplashThink
===============
*/
#define SplashSize 7.5
void CL_ParticleSplashThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	vec3_t len;
	VectorSubtract(p->angle, org, len);
	
//	*size *= (float)(SplashSize/VectorLength(len)) * 0.5/((4-*size));
//	if (*size > SplashSize)
//		*size = SplashSize;

	//setting up angle for sparks
	const float time1 = *time;
	const float time2 = time1 * time1;

	for (int i = 0; i < 2; i++)
		angle[i] = 0.5 * (p->vel[i] * time1 + p->accel[i] * time2);
	angle[2] = 0.5 * (p->vel[2] * time1 + (p->accel[2] - PARTICLE_GRAVITY) * time2);

	p->thinknext = true;
}

/*
===============
CL_ParticleEffectSplash

Water Splashing
===============
*/
void CL_ParticleEffectSplash (vec3_t org, vec3_t dir, int color8, int count)
{
	vec3_t color = {color8red(color8), color8green(color8), color8blue(color8)};

	for (int i = 0; i < count; i++)
	{
		const float d = rand()&5;
		CL_SetupParticle (
			org[0],	org[1],	org[2],
			org[0]+d*dir[0],	org[1]+d*dir[1],	org[2]+d*dir[2],
			dir[0]*40 + crand()*10,	dir[1]*40 + crand()*10,	dir[2]*40 + crand()*10,
			0,		0,		0,
			color[0],	color[1],	color[2],
			0,	0,	0,
			1,		-0.75 / (0.5 + frand()*0.3),
			GL_SRC_ALPHA, GL_ONE,
			5,			-7,			
			particle_smoke,
			PART_GRAVITY|PART_DIRECTION   /*|PART_TRANS|PART_SHADED*/,
			CL_ParticleSplashThink,true);
	}
}

/*
===============
CL_ParticleSparksThink
===============
*/
void CL_ParticleSparksThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	//setting up angle for sparks
	const float time1 = *time;
	const float time2 = time1 * time1;

	for (int i = 0; i < 2; i++)
		angle[i] = 0.25 * (p->vel[i] * time1 + p->accel[i] * time2);
	angle[2] = 0.25 * (p->vel[2] * time1 + (p->accel[2] - PARTICLE_GRAVITY) * time2);

	p->thinknext = true;
}

/*
===============
CL_ParticleEffectSparks
===============
*/
void CL_ParticleEffectSparks (vec3_t org, vec3_t dir, vec3_t color, int count)
{
	cparticle_t *p = NULL;

	for (int i = 0; i < count; i++)
	{
		p = CL_SetupParticle(
			0, 0, 0,
			org[0] + ((rand() & 3) - 2), 
			org[1] + ((rand() & 3) - 2), 
			org[2] + ((rand() & 3) - 2),
			crand() * 20 + dir[0] * 40,
			crand() * 20 + dir[1] * 40,
			crand() * 20 + dir[2] * 40,
			0, 0, 0,
			color[0], color[1], color[2],
			0, 0, 0,
			0.75, -1.0 / (0.5 + frand() * 0.3),
			GL_SRC_ALPHA, GL_ONE,
			4, 0, //Knightmare- increase size
			particle_solid,
			PART_GRAVITY | PART_SPARK,
			CL_ParticleSparksThink, true);
	}

	if (p) // added light effect
		CL_AddParticleLight(p, (count > 8 ? 130 : 65), 0, color[0] / 255, color[1] / 255, color[2] / 255);
}


/*
===============
CL_ParticleBulletDecal
===============
*/
#define DECAL_OFFSET 0.5f
void CL_ParticleBulletDecal (vec3_t org, vec3_t dir, float size)
{
	vec3_t ang, angle, end, origin;

	if (!r_decals->value)
		return;

	VectorMA(org, DECAL_OFFSET, dir, origin);
	VectorMA(org, -DECAL_OFFSET, dir, end);
	trace_t tr = CL_Trace(origin, end, 0, CONTENTS_SOLID);

	if (tr.fraction == 1 || VectorCompare(tr.plane.normal, vec3_origin)) //mxd. Other CL_*Decal have VectorCompare, why this was an exception?
		return;

	VectorNegate(tr.plane.normal, angle);
	VecToAngleRolled(angle, rand() % 360, ang);
	VectorCopy(tr.endpos, origin);

	CL_SetupParticle(
		ang[0], ang[1], ang[2],
		origin[0], origin[1], origin[2],
		0, 0, 0,
		0, 0, 0,
		255, 255, 255,
		0, 0, 0,
		1, -1 / r_decal_life->value,
		GL_ZERO, GL_ONE_MINUS_SRC_ALPHA,
		size, 0,
		particle_bulletmark,
		PART_SHADED | PART_DECAL | PART_ALPHACOLOR, // was part_saturate
		CL_DecalAlphaThink, true);
}


/*
===============
CL_ParticleRailDecal
===============
*/
#define RAIL_DECAL_OFFSET 2.0f
void CL_ParticleRailDecal (vec3_t org, vec3_t dir, float size, qboolean isRed)
{
	vec3_t ang, angle, end, origin;

	if (!r_decals->value)
		return;

	VectorMA(org, -RAIL_DECAL_OFFSET, dir, origin);
	VectorMA(org, 2 * RAIL_DECAL_OFFSET, dir, end);
	trace_t tr = CL_Trace(origin, end, 0, CONTENTS_SOLID);

	if (tr.fraction == 1 || VectorCompare(tr.plane.normal, vec3_origin))
		return;

	VectorNegate(tr.plane.normal, angle);
	VecToAngleRolled(angle, rand() % 360, ang);
	VectorCopy(tr.endpos, origin);

	//mxd
	vec3_t color;
	if (isRed)
		VectorSet(color, 255, 20, 20);
	else
		VectorSet(color, cl_railred->value, cl_railgreen->value, cl_railblue->value);

	CL_SetupParticle(
		ang[0], ang[1], ang[2],
		origin[0], origin[1], origin[2],
		0, 0, 0,
		0, 0, 0,
		255, 255, 255,
		0, 0, 0,
		1, -1 / r_decal_life->value,
		GL_ZERO, GL_ONE_MINUS_SRC_ALPHA,
		size, 0,
		particle_bulletmark,
		PART_SHADED | PART_DECAL | PART_ALPHACOLOR,
		CL_DecalAlphaThink, true);

	CL_SetupParticle(
		ang[0], ang[1], ang[2],
		origin[0], origin[1], origin[2],
		0, 0, 0,
		0, 0, 0,
		color[0], color[1], color[2],
		0, 0, 0,
		1, -0.25,
		GL_SRC_ALPHA, GL_ONE,
		size, 0,
		particle_generic,
		PART_DECAL,
		NULL, false);

	CL_SetupParticle(
		ang[0], ang[1], ang[2],
		origin[0], origin[1], origin[2],
		0, 0, 0,
		0, 0, 0,
		255, 255, 255,
		0, 0, 0,
		1, -0.25,
		GL_SRC_ALPHA, GL_ONE,
		size * 0.67, 0,
		particle_generic,
		PART_DECAL,
		NULL, false);
}


/*
===============
CL_ParticleBlasterDecal
===============
*/
void CL_ParticleBlasterDecal (vec3_t org, vec3_t dir, float size, int red, int green, int blue)
{
	vec3_t ang, angle, end, origin;

	if (!r_decals->value)
		return;
 
	VectorMA(org, DECAL_OFFSET, dir, origin);
	VectorMA(org, -DECAL_OFFSET, dir, end);
	trace_t tr = CL_Trace(origin, end, 0, CONTENTS_SOLID);

	if (tr.fraction == 1 || VectorCompare(tr.plane.normal, vec3_origin))
		return;

	VectorNegate(tr.plane.normal, angle);
	VecToAngleRolled(angle, rand() % 360, ang);
	VectorCopy(tr.endpos, origin);

	CL_SetupParticle(
		ang[0], ang[1], ang[2],
		origin[0], origin[1], origin[2],
		0, 0, 0,
		0, 0, 0,
		255, 255, 255,
		0, 0, 0,
		0.7, -1 / r_decal_life->value,
		GL_ZERO, GL_ONE_MINUS_SRC_ALPHA,
		size, 0,
		particle_shadow,
		PART_SHADED | PART_DECAL,
		NULL, false);

	CL_SetupParticle(
		ang[0], ang[1], ang[2],
		origin[0], origin[1], origin[2],
		0, 0, 0,
		0, 0, 0,
		red, green, blue,
		0, 0, 0,
		1, -0.3,
		GL_SRC_ALPHA, GL_ONE,
		size * 0.4, 0,
		particle_generic,
		PART_SHADED | PART_DECAL,
		NULL, false);

	CL_SetupParticle(
		ang[0], ang[1], ang[2],
		origin[0], origin[1], origin[2],
		0, 0, 0,
		0, 0, 0,
		red, green, blue,
		0, 0, 0,
		1, -0.6,
		GL_SRC_ALPHA, GL_ONE,
		size * 0.3, 0,
		particle_generic,
		PART_SHADED | PART_DECAL,
		NULL, false);
}


/*
===============
CL_ParticlePlasmaBeamDecal
===============
*/
void CL_ParticlePlasmaBeamDecal (vec3_t org, vec3_t dir, float size)
{
	vec3_t ang, angle, end, origin;

	if (!r_decals->value)
		return;
 
	VectorMA(org, DECAL_OFFSET, dir, origin);
	VectorMA(org, -DECAL_OFFSET, dir, end);
	trace_t tr = CL_Trace(origin, end, 0, CONTENTS_SOLID);

	if (tr.fraction == 1 || VectorCompare(tr.plane.normal, vec3_origin))
		return;

	VectorNegate(tr.plane.normal, angle);
	VecToAngleRolled(angle, rand() % 360, ang);
	VectorCopy(tr.endpos, origin);

	CL_SetupParticle(
		ang[0], ang[1], ang[2],
		origin[0], origin[1], origin[2],
		0, 0, 0,
		0, 0, 0,
		255, 255, 255,
		0, 0, 0,
		0.85, -1 / r_decal_life->value,
		GL_ZERO, GL_ONE_MINUS_SRC_ALPHA,
		size, 0,
		particle_shadow,
		PART_SHADED | PART_DECAL,
		NULL, false);
}


/*
===============
CL_Shadow_Decal (mxd)
===============
*/

void CL_Shadow_Decal(vec3_t org, float size, float alpha)
{
	if(cl_paused->integer) // Don't draw when paused (messes up rendering of the first frame after unpausing)
		return;
	
	vec3_t ang, angle, end, origin, dir;

	VectorSet(dir, 0, 0, -1); // Straight down
	VectorMA(org, -DECAL_OFFSET, dir, origin);
	VectorMA(org, size, dir, end);
	trace_t tr = CL_Trace(origin, end, 0, CONTENTS_SOLID);

	if (tr.fraction == 1 || VectorCompare(tr.plane.normal, vec3_origin))
		return;

	size *= 1.0f - tr.fraction; // Shrink by distance
	alpha *= 1.0f - tr.fraction; // Fade by distance

	VectorNegate(tr.plane.normal, angle);
	VecToAngleRolled(angle, 0, ang);
	VectorCopy(tr.endpos, origin);

	CL_SetupParticle(
		ang[0], ang[1], ang[2],
		origin[0], origin[1], origin[2],
		0, 0, 0,
		0, 0, 0,
		255, 255, 255,
		0, 0, 0,
		alpha, INSTANT_PARTICLE,
		GL_ZERO, GL_ONE_MINUS_SRC_ALPHA,
		size, 0,
		particle_shadow,
		PART_SHADED | PART_DECAL,
		NULL, false);
}


/*
===============
CL_TeleporterParticles
===============
*/
void CL_TeleporterParticles (entity_state_t *ent)
{
	for (int i = 0; i < 8; i++)
	{
		CL_SetupParticle(
			0, 0, 0,
			ent->origin[0] - 16 + (rand() & 31),
			ent->origin[1] - 16 + (rand() & 31),
			ent->origin[2] - 16 + (rand() & 31),
			crand() * 14, crand() * 14, 80 + (rand() & 7),
			0, 0, 0,
			230 + crand() * 25, 125 + crand() * 25, 25 + crand() * 25,
			0, 0, 0,
			1, -0.5,
			GL_SRC_ALPHA, GL_ONE,
			2, 0,
			particle_generic,
			PART_GRAVITY,
			NULL, false);
	}
}


/*
===============
CL_LogoutEffect
===============
*/
void CL_LogoutEffect (vec3_t org, int type)
{
	vec3_t	color;

	switch (type) //mxd
	{
		case MZ_LOGIN:	VectorSet(color, 20, 200, 20); break; // green
		case MZ_LOGOUT: VectorSet(color, 200, 20, 20); break; // red
		default:		VectorSet(color, 200, 200, 20); break; // yeller
	}

	for (int i = 0; i < 500; i++)
	{
		CL_SetupParticle(
			0, 0, 0,
			org[0] - 16 + frand() * 32, 
			org[1] - 16 + frand() * 32, 
			org[2] - 24 + frand() * 56,
			crand() * 20, crand() * 20, crand() * 20,
			0, 0, 0,
			color[0], color[1], color[2],
			0, 0, 0,
			1, -1.0 / (1.0 + frand()*0.3),
			GL_SRC_ALPHA, GL_ONE,
			1, 0,
			particle_generic,
			PART_GRAVITY,
			NULL, false);
	}
}


/*
===============
CL_ItemRespawnParticles
===============
*/
void CL_ItemRespawnParticles (vec3_t org)
{
	for (int i = 0; i < 64; i++)
	{
		CL_SetupParticle(
			0, 0, 0,
			org[0] + crand() * 8,
			org[1] + crand() * 8,
			org[2] + crand() * 8,
			crand() * 8, crand() * 8, crand() * 8,
			0, 0, PARTICLE_GRAVITY*0.2,
			0, 150 + rand() * 25, 0,
			0, 0, 0,
			1, -1.0 / (1.0 + frand()*0.3),
			GL_SRC_ALPHA, GL_ONE,
			1, 0,
			particle_generic,
			PART_GRAVITY,
			NULL, false);
	}
}


/*
===============
CL_BigTeleportParticles
===============
*/
void CL_BigTeleportParticles (vec3_t org)
{
	static int colortable0[4] = {10,  50,  150, 50};
	static int colortable1[4] = {150, 150, 50,  10};
	static int colortable2[4] = {50,  10,  10,  150};

	for (int i = 0; i < (1024 / cl_particle_scale->value); i++) // was 4096
	{
		const int index = rand() & 3;
		const float angle = M_PI * 2 * (rand() & 1023) / 1023.0f;
		const float dist = rand() & 31;

		const float cosa = cosf(angle); //mxd
		const float sina = sinf(angle); //mxd

		CL_SetupParticle(
			0, 0, 0,
			org[0] + cosa * dist, org[1] + sina * dist, org[2] + 8 + (rand() % 90),
			cosa * (70 + (rand() & 63)), sina * (70 + (rand() & 63)), -100 + (rand() & 31),
			-cosa * 100, -sina * 100, PARTICLE_GRAVITY * 4,
			colortable0[index], colortable1[index], colortable2[index],
			0, 0, 0,
			1, -0.1f / (0.5f + frand() * 0.3f),
			GL_SRC_ALPHA, GL_ONE,
			5, 0.15f / (0.5f + frand() * 0.3f), // was 2, 0.05
			particle_generic,
			0,
			NULL, false);
	}
}


/*
===============
CL_ParticleBlasterThink

Wall impact puffs
===============
*/
#define pBlasterMaxVelocity 100
#define pBlasterMinSize 1.0
#define pBlasterMaxSize 5.0

void CL_ParticleBlasterThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	vec3_t len;
	float clipsize = 1.0;
	VectorSubtract(p->angle, org, len);

	*size *= (float)(pBlasterMaxSize / VectorLength(len)) * 1.0 / (4 - *size);
	*size += *time * p->sizevel;
	*size = clamp(*size, pBlasterMinSize, pBlasterMaxSize); //mxd

	CL_ParticleBounceThink(p, org, angle, alpha, &clipsize, image, time); // was size

	vec_t length = VectorNormalize(p->vel);
	length = min(pBlasterMaxVelocity, length); //mxd. Passing VectorNormalize to min() was not such a great idea (because it's evaluated twice)...
	VectorScale(p->vel, length, p->vel);
}


/*
===============
CL_ClassicBlasterParticles (mxd)
===============
*/
void CL_ClassicBlasterParticles(vec3_t org, vec3_t dir)
{
	for (int i = 0; i < 40; i++)
	{
		cparticle_t	*p = CL_InitParticle();
		if (!p) return;

		color8_to_vec3(0xe0 + (rand() & 7), p->color);

		const float d = rand() & 15;
		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() & 7) - 4) + d * dir[j];
			p->vel[j] = dir[j] * 30 + crand() * 40;
		}

		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -1.0 / (0.5 + frand() * 0.3);
		p->image = particle_classic;
		p->flags = PART_GRAVITY;
	}
}


/*
===============
CL_BlasterParticles

Wall impact puffs
===============
*/
void CL_BlasterParticles (vec3_t org, vec3_t dir, int count, float size, int red, int green, int blue, int reddelta, int greendelta, int bluedelta)
{
	//mxd. Classic particles
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicBlasterParticles(org, dir);
		return;
	}
	
	const float speed = 0.75;
	cparticle_t *p = NULL;
	vec3_t origin;

	for (int i = 0; i < count; i++)
	{
		for (int j = 0; j < 3; j++) //mxd
			origin[j] = org[j] + dir[j] * (1 + random() * 3 + pBlasterMaxSize / 2.0);

		p = CL_SetupParticle (
			org[0],	org[1],	org[2],
			origin[0],	origin[1],	origin[2],
			(dir[0]*75 + crand()*40)*speed,	(dir[1]*75 + crand()*40)*speed,	(dir[2]*75 + crand()*40)*speed,
			0,		0,		0,
			red,		green,		blue,
			reddelta,	greendelta,	bluedelta,
			1,		-0.5 / (0.5 + frand()*0.3),
			GL_SRC_ALPHA, GL_ONE,
			size,	size * -0.125,		// was 4, -0.5
			particle_generic,
			PART_GRAVITY,
			CL_ParticleBlasterThink, true);
	
	/*	d = rand()&5;
		p = CL_SetupParticle (
			org[0],	org[1],	org[2],
			org[0]+((rand()&5)-2)+d*dir[0],	org[1]+((rand()&5)-2)+d*dir[1],	org[2]+((rand()&5)-2)+d*dir[2],
			(dir[0]*50 + crand()*20)*speed,	(dir[1]*50 + crand()*20)*speed,	(dir[2]*50 + crand()*20)*speed,
			0,			0,			0,
			red,		green,		blue,
			reddelta,	greendelta,	bluedelta,
			1,		-1.0 / (0.5 + frand()*0.3),
			GL_SRC_ALPHA, GL_ONE,
			4,		-1.0,
			particle_generic,
			PART_GRAVITY,
			CL_ParticleBlasterThink,true);*/
	}

	if (p) // added light effect
		CL_AddParticleLight (p, 150, 0, ((float)red)/255, ((float)green)/255, ((float)blue)/255);
}

/*
===============
CL_ClassicBlasterTrail (mxd)
===============
*/
void CL_ClassicBlasterTrail(vec3_t start, vec3_t end)
{
	vec3_t move, vec;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	const int dec = 5;
	VectorScale(vec, dec, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		cparticle_t	*p = CL_InitParticle();
		if (!p) return;

		p->alphavel = -1.0 / (0.3 + frand()*0.2);
		color8_to_vec3(0xe0, p->color);

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand();
			p->vel[j] = crand() * 5;
		}

		p->image = particle_classic;

		VectorAdd(move, vec, move);
	}
}

/*
===============
CL_BlasterTrail
===============
*/
void CL_BlasterTrail (vec3_t start, vec3_t end, int red, int green, int blue, int reddelta, int greendelta, int bluedelta)
{
	//mxd. Classic particles
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicBlasterTrail(start, end);
		return;
	}
	
	vec3_t move, vec;

	VectorCopy(start, move);
	VectorSubtract(start, end, vec);
	float len = VectorNormalize(vec);
	VectorMA(move, -5.0f, vec, move);

	const int dec = 4 * cl_particle_scale->value; 
	VectorScale (vec, dec, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		CL_SetupParticle (
			0,			0,			0,
			move[0] + crand(),	move[1] + crand(),	move[2] + crand(),
			crand() * 5,	crand() * 5,	crand() * 5,
			0,			0,			0,
			red,		green,		blue,
			reddelta,	greendelta,	bluedelta,
			1,			-1.0 / (0.5 + frand() * 0.3),
			GL_SRC_ALPHA, GL_ONE,
			3,			-7,	// was 4, -6;
			particle_generic,
			0,
			NULL, false);

		VectorAdd(move, vec, move);
	}
}


/*
===============
CL_HyperBlasterGlow

Hyperblaster particle glow effect
===============
*/
void CL_HyperBlasterGlow (vec3_t start, vec3_t end, int red, int green, int blue, int reddelta, int greendelta, int bluedelta)
{
	vec3_t move, vec;

	VectorCopy(start, move);
	VectorSubtract(start, end, vec);
	VectorNormalize(vec);
	VectorMA(move, -16.5f, vec, move);

	const float dec = 3.0f; // was 1, 5
	VectorScale(vec, dec, vec);

	for (int i = 0; i < 12; i++) // was 18
	{
		const float size = 4.2f - (0.1f * i);

		CL_SetupParticle (
			0,			0,			0,
			move[0] + 0.5 * crand(),	move[1] + 0.5 * crand(),	move[2] + 0.5 * crand(),
			crand() * 5,	crand() * 5,	crand() * 5,
			0,			0,			0,
			red,		green,		blue,
			reddelta,	greendelta,	bluedelta,
			1,			INSTANT_PARTICLE, // was -16.0 / (0.5 + frand()*0.3),
			GL_SRC_ALPHA, GL_ONE,
			size,			0, // was 3, -36; 5, -60
			particle_generic,
			0,
			NULL, false);

		VectorAdd(move, vec, move);
	}
}


/*
===============
CL_BlasterTracer 
===============
*/
void CL_BlasterTracer (vec3_t origin, vec3_t angle, int red, int green, int blue, float len, float size)
{
	vec3_t dir;
	
	AngleVectors(angle, dir, NULL, NULL);
	VectorScale(dir, len, dir);

	CL_SetupParticle(
		dir[0], dir[1], dir[2],
		origin[0], origin[1], origin[2],
		0, 0, 0,
		0, 0, 0,
		red, green, blue,
		0, 0, 0,
		1, INSTANT_PARTICLE,
		GL_ONE, GL_ONE, // was GL_SRC_ALPHA, GL_ONE
		size, 0,
		particle_blasterblob, // was particle_generic
		PART_DIRECTION | PART_INSTANT | PART_OVERBRIGHT,
		NULL, false);
}

void CL_HyperBlasterEffect (vec3_t start, vec3_t end, vec3_t angle, int red, int green, int blue, int reddelta, int greendelta, int bluedelta, float len, float size)
{
	if (cl_particle_scale->value < 2)
		CL_HyperBlasterGlow(start, end, red, green, blue, reddelta, greendelta, bluedelta);

	CL_BlasterTracer(end, angle, red, green, blue, len, size);
}


/*
===============
CL_QuadTrail
===============
*/
void CL_QuadTrail (vec3_t start, vec3_t end)
{
	vec3_t move, vec;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	const int dec = 5;
	VectorScale(vec, 5, vec);

	while (len > 0)
	{
		len -= dec;

		CL_SetupParticle(
			0, 0, 0,
			move[0] + crand() * 16,
			move[1] + crand() * 16,
			move[2] + crand() * 16,
			crand() * 5, crand() * 5, crand() * 5,
			0, 0, 0,
			0, 0, 200,
			0, 0, 0,
			1, -1.0 / (0.8 + frand() * 0.2),
			GL_SRC_ALPHA, GL_ONE,
			1, 0,
			particle_generic,
			0,
			NULL, false);

		VectorAdd(move, vec, move);
	}
}


/*
===============
CL_FlagTrail
===============
*/
void CL_FlagTrail (vec3_t start, vec3_t end, qboolean isred, qboolean isgreen)
{
	vec3_t move, vec;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	const int dec = 5;
	VectorScale(vec, 5, vec);

	//mxd
	vec3_t color;
	VectorClear(color);

	if (isred) 
		color[0] = 255;
	else if (isgreen) 
		color[1] = 255;
	else 
		color[2] = 255;

	while (len > 0)
	{
		len -= dec;

		CL_SetupParticle(
			0, 0, 0,
			move[0] + crand() * 16,
			move[1] + crand() * 16,
			move[2] + crand() * 16,
			crand() * 5, crand() * 5, crand() * 5,
			0, 0, 0,
			color[0], color[1], color[2],
			0, 0, 0,
			1, -1.0 / (0.8 + frand() * 0.2),
			GL_SRC_ALPHA, GL_ONE,
			1, 0,
			particle_generic,
			0,
			NULL, false);

		VectorAdd(move, vec, move);
	}
}


/*
===============
CL_ClassicDiminishingTrail (mxd)
===============
*/
void CL_ClassicDiminishingTrail(vec3_t start, vec3_t end, centity_t *old, int flags)
{
	vec3_t move, vec;
	float orgscale, velscale;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	const float dec = 0.5;
	VectorScale(vec, dec, vec);

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

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;

		// Drop less particles as it flies
		if ((rand() & 1023) < old->trailcount)
		{
			cparticle_t *p = CL_InitParticle2(flags);
			if (!p) return;

			for (int i = 0; i < 3; i++)
			{
				p->org[i] = move[i] + crand() * orgscale;
				p->vel[i] = crand() * velscale;
			}

			if (flags & EF_GIB)
			{
				p->alphavel = -1.0 / (1 + frand() * 0.4);
				color8_to_vec3(0xe8 + (rand() & 7), p->color);
				p->vel[2] -= PARTICLE_GRAVITY;
			}
			else if (flags & EF_GREENGIB)
			{
				p->alphavel = -1.0 / (1 + frand() * 0.4);
				color8_to_vec3(0xdb + (rand() & 7), p->color);
				p->vel[2] -= PARTICLE_GRAVITY;
			}
			else
			{
				p->alphavel = -1.0 / (1 + frand() * 0.2);
				color8_to_vec3(4 + (rand() & 7), p->color);
				p->accel[2] = 20;
			}
		}

		old->trailcount = max(100, old->trailcount - 5);
		VectorAdd(move, vec, move);
	}
}


/*
===============
CL_DiminishingTrail
===============
*/
void CL_DiminishingTrail (vec3_t start, vec3_t end, centity_t *old, int flags)
{
	//mxd. Classic particles...
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicDiminishingTrail(start, end, old, flags);
		return;
	}
	
	cparticle_t	*p;
	vec3_t		move;
	vec3_t		vec;
	float		orgscale;
	float		velscale;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	float dec = (flags & EF_ROCKET ? 10 : 2);
	dec *= cl_particle_scale->value;
	VectorScale(vec, dec, vec);

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

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;

		if (flags & EF_ROCKET)
		{
			if (CM_PointContents(move, 0) & MASK_WATER)
			{
				CL_SetupParticle(
					0, 0, crand() * 360,
					move[0], move[1], move[2],
					crand() * 9, crand() * 9, crand() * 9 + 5,
					0, 0, 0,
					255, 255, 255,
					0, 0, 0,
					0.75, -0.2 / (1 + frand() * 0.2),
					GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
					1 + random() * 3, 1,
					particle_bubble,
					PART_TRANS | PART_SHADED,
					CL_ExplosionBubbleThink, true);
			}
			else
			{
				CL_SetupParticle(
					crand() * 180, crand() * 100, 0,
					move[0], move[1], move[2],
					crand() * 5, crand() * 5, crand() * 10,
					0, 0, 5,
					255, 255, 255,
					-50, -50, -50,
					0.75, -0.75,	// was 1, -0.5
					GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
					5, 15,
					particle_smoke,
					PART_TRANS | PART_SHADED,
					CL_ParticleRotateThink, true);
			}
		}
		else
		{
			// drop less particles as it flies
			if ((rand() & 1023) < old->trailcount)
			{
				if (flags & EF_GIB)
				{
					if (cl_blood->value > 1)
					{
						p = CL_SetupParticle(
							0, 0, random() * 360,
							move[0] + crand() * orgscale, move[1] + crand() * orgscale, move[2] + crand() * orgscale,
							crand() * velscale, crand() * velscale, crand() * velscale,
							0, 0, 0,
							255, 0, 0,
							0, 0, 0,
							0.75, -0.75 / (1 + frand() * 0.4),
							GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
							3 + random() * 2, 0,
							particle_blooddrop,
							PART_OVERBRIGHT | PART_GRAVITY | PART_SHADED,
							CL_ParticleBloodThink, true);
					}
					else
					{
						p = CL_SetupParticle(
							0, 0, 0,
							move[0] + crand() * orgscale, move[1] + crand() * orgscale, move[2] + crand() * orgscale,
							crand() * velscale, crand() * velscale, crand() * velscale,
							0, 0, 0,
							255, 0, 0,
							0, 0, 0,
							1, -1.0 / (1 + frand() * 0.4),
							GL_SRC_ALPHA, GL_ONE,
							5, -1,
							particle_blood,
							PART_GRAVITY | PART_SHADED,
							CL_ParticleBloodThink, true);
					}

					if (p && crand() < (double)0.0001f)
						p->flags |= PART_LEAVEMARK;
				}
				else if (flags & EF_GREENGIB)
				{
					p = CL_SetupParticle (
						0,	0,	0,
						move[0] + crand() * orgscale,	move[1] + crand() * orgscale,	move[2] + crand() * orgscale,
						crand() * velscale,	crand() * velscale,	crand() * velscale,
						0,		0,		0,
						255,	180,	50,
						0,		0,		0,
						1,		-0.5 / (1 + frand() * 0.4),
						GL_SRC_ALPHA, GL_ONE,
						5,			-1,
						particle_blood,
						PART_OVERBRIGHT | PART_GRAVITY | PART_SHADED,
						CL_ParticleBloodThink, true);

					if (p && crand() < (double)0.0001f) 
						p->flags |= PART_LEAVEMARK;
				}
				else if (flags & EF_GRENADE) // no overbrights on grenade trails
				{
					if (CM_PointContents(move, 0) & MASK_WATER)
					{
						CL_SetupParticle(
							0, 0, crand() * 360,
							move[0], move[1], move[2],
							crand() * 9, crand() * 9, crand() * 9 + 5,
							0, 0, 0,
							255, 255, 255,
							0, 0, 0,
							0.75, -0.2 / (1 + frand() * 0.2),
							GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
							1 + random() * 3, 1,
							particle_bubble,
							PART_TRANS | PART_SHADED,
							CL_ExplosionBubbleThink, true);
					}
					else
					{
						CL_SetupParticle(
							crand() * 180, crand() * 50, 0,
							move[0] + crand() * orgscale, move[1] + crand() * orgscale, move[2] + crand() * orgscale,
							crand() * velscale, crand() * velscale, crand() * velscale,
							0, 0, 20,
							255, 255, 255,
							0, 0, 0,
							0.5, -0.5,
							GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
							5, 5,
							particle_smoke,
							PART_TRANS | PART_SHADED,
							CL_ParticleRotateThink, true);
					}
				}
				else
				{
					CL_SetupParticle(
						crand() * 180, crand() * 50, 0,
						move[0] + crand() * orgscale, move[1] + crand() * orgscale, move[2] + crand() * orgscale,
						crand() * velscale, crand() * velscale, crand() * velscale,
						0, 0, 20,
						255, 255, 255,
						0, 0, 0,
						0.5, -0.5,
						GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
						5, 5,
						particle_smoke,
						PART_OVERBRIGHT | PART_TRANS | PART_SHADED,
						CL_ParticleRotateThink, true);
				}
			}

			old->trailcount = max(100, old->trailcount - 5);
		}

		VectorAdd(move, vec, move);
	}
}

/*
===============
CL_ClassicRocketTrail (mxd)
===============
*/
void CL_ClassicRocketTrail(vec3_t start, vec3_t end, centity_t *old)
{
	// Smoke
	CL_ClassicDiminishingTrail(start, end, old, EF_ROCKET);

	// Fire
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	while (len > 0)
	{
		len -= 1;

		if (!free_particles)
			return;

		// falling particles
		if ((rand() & 7) == 0)
		{
			cparticle_t *p = CL_InitParticle();
			if (!p) return;

			p->alphavel = -1.0 / (1 + frand() * 0.2);
			color8_to_vec3(0xdc + (rand() & 3), p->color);

			for (int j = 0; j < 3; j++)
			{
				p->org[j] = move[j] + crand() * 5;
				p->vel[j] = crand() * 20;
			}

			p->accel[2] = -PARTICLE_GRAVITY;
			p->flags = PART_GRAVITY;
			p->image = particle_classic;
		}

		VectorAdd(move, vec, move);
	}
}


/*
===============
CL_RocketTrail
===============
*/
void CL_RocketTrail (vec3_t start, vec3_t end, centity_t *old)
{
	//mxd. Classic particles...
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicRocketTrail(start, end, old);
		return;
	}
	
	vec3_t move, vec;

	// smoke
	CL_DiminishingTrail (start, end, old, EF_ROCKET);

	// fire
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	float dec = cl_particle_scale->value;
	VectorScale(vec, dec, vec);

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;

		// falling particles
		if ((rand() & 7) == 0)
		{
			CL_SetupParticle(
				0, 0, 0,
				move[0] + crand() * 5, move[1] + crand() * 5, move[2] + crand() * 5,
				crand() * 20, crand() * 20, crand() * 20,
				0, 0, 20,
				255, 255, 200,
				0, -50, 0,
				1, -1.0 / (1 + frand()*0.2),
				GL_SRC_ALPHA, GL_ONE,
				2, -2,
				particle_blaster,
				PART_GRAVITY,
				NULL, false);
		}

		VectorAdd (move, vec, move);
	}

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);
	dec = 1.5 * cl_particle_scale->value;
	VectorScale(vec, dec, vec);

	while (len > 0)
	{
		len -= dec;

		// flame
		CL_SetupParticle(
			crand() * 180, crand() * 100, 0,
			move[0], move[1], move[2],
			crand() * 5, crand() * 5, crand() * 5,
			0, 0, 5,
			255, 225, 200,
			-50, -50, -50,
			0.5, -2, // was 0.75, -3
			GL_SRC_ALPHA, GL_ONE,
			5, 5,
			particle_inferno,
			0,
			CL_ParticleRotateThink, true);

		VectorAdd(move, vec, move);
	}
}


/*
===============
FartherPoint
Returns true if the first vector is farther from the viewpoint.
===============
*/
qboolean FartherPoint (vec3_t pt1, vec3_t pt2)
{
	vec3_t distance1, distance2;

	VectorSubtract(pt1, cl.refdef.vieworg, distance1);
	VectorSubtract(pt2, cl.refdef.vieworg, distance2);

	return (VectorLengthSquared(distance1) > VectorLengthSquared(distance2));
}


/*
===============
CL_RailSprial
===============
*/
#define DEVRAILSTEPS 2
//this is the length of each piece...
#define RAILTRAILSPACE 15

void CL_RailSprial (vec3_t start, vec3_t end, qboolean isRed)
{
	vec3_t		move;
	vec3_t		vec;
	vec3_t		right, up;
	vec3_t		dir;

	// Draw from closest point
	if (FartherPoint(start, end))
	{
		VectorCopy(end, move);
		VectorSubtract(start, end, vec);
	}
	else
	{
		VectorCopy(start, move);
		VectorSubtract (end, start, vec);
	}

	float len = VectorNormalize(vec);
	len = min(len, cl_rail_length->value); // cap length
	MakeNormalVectors(vec, right, up);

	VectorScale(vec, cl_rail_space->value * cl_particle_scale->value, vec);

	//mxd
	vec3_t color;
	if (isRed)
		VectorSet(color, 255, 20, 20);
	else
		VectorSet(color, cl_railred->value, cl_railgreen->value, cl_railblue->value);

	for (int i = 0; i < len; i += cl_rail_space->value * cl_particle_scale->value)
	{
		const float d = i * 0.1f;
		const float c = cosf(d);
		const float s = sinf(d);

		VectorScale(right, c, dir);
		VectorMA(dir, s, up, dir);

		CL_SetupParticle(
			0, 0, 0,
			move[0] + dir[0] * 3, move[1] + dir[1] * 3, move[2] + dir[2] * 3,
			dir[0] * 6, dir[1] * 6, dir[2] * 6,
			0, 0, 0,
			color[0], color[1], color[2],
			0, 0, 0,
			1, -1.0,
			GL_SRC_ALPHA, GL_ONE,
			3, 0,
			particle_generic,
			0,
			NULL, false);

		VectorAdd(move, vec, move);
	}
}

/*
===============
CL_ParticleDevRailThink
===============
*/
void CL_ParticleDevRailThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	vec3_t len;
	VectorSubtract(p->angle, org, len);
	
	*size *= (float)(SplashSize / VectorLength(len)) * 0.5 / (4 - *size);
	*size = min(*size, SplashSize); //mxd

	//setting up angle for sparks
	const float time1 = *time;
	const float time2 = time1 * time1;

	for (int i = 0; i < 2; i++)
		angle[i] = 3 * (p->vel[i] * time1 + p->accel[i] * time2);
	angle[2] = 3 * (p->vel[2] * time1 + (p->accel[2] - PARTICLE_GRAVITY) * time2);

	p->thinknext = true;
}

/*
===============
CL_DevRailTrail
===============
*/
void CL_DevRailTrail (vec3_t start, vec3_t end, qboolean isRed)
{
	vec3_t move, vec, point;
	int i = 0;

	// Draw from closest point
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
	len = min(len, cl_rail_length->value); // cap length
	VectorCopy(vec, point);

	const int dec = 4;
	VectorScale (vec, dec, vec);

	vec3_t color; //mxd
	if (isRed)
		VectorSet(color, 255, 20, 20);
	else
		VectorSet(color, cl_railred->value, cl_railgreen->value, cl_railblue->value);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;
		i++;

		if (i >= DEVRAILSTEPS)
		{
			for (i = 3; i > 0; i--)
			{
				CL_SetupParticle(
					point[0], point[1], point[2],
					move[0], move[1], move[2],
					0, 0, 0,
					0, 0, 0,
					color[0], color[1], color[2],
					0, -90, -30,
					0.75, -.75,
					GL_SRC_ALPHA, GL_ONE,
					dec * DEVRAILSTEPS * TWOTHIRDS, 0,
					particle_beam2,
					PART_DIRECTION,
					NULL, false);
			}
		}

		CL_SetupParticle(
			0, 0, 0,
			move[0], move[1], move[2],
			crand() * 10, crand() * 10, crand() * 10 + 20,
			0, 0, 0,
			color[0], color[1], color[2],
			0, 0, 0,
			1, -0.75 / (0.5 + frand()*0.3),
			GL_SRC_ALPHA, GL_ONE,
			2, -0.25,
			particle_solid,
			PART_GRAVITY | PART_SPARK,
			CL_ParticleDevRailThink, true);
		
		CL_SetupParticle(
			crand() * 180, crand() * 100, 0,
			move[0], move[1], move[2],
			crand() * 10, crand() * 10, crand() * 10 + 20,
			0, 0, 5,
			255, 255, 255,
			0, 0, 0,
			0.25, -0.25,
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			5, 10,
			particle_smoke,
			PART_TRANS | PART_GRAVITY | PART_OVERBRIGHT,
			CL_ParticleRotateThink, true);

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_ClassicRailTrail (mxd)
===============
*/
void CL_ClassicRailTrail(vec3_t start, vec3_t end, qboolean isRed)
{
	vec3_t move, vec, dir;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	vec3_t right, up;
	MakeNormalVectors(vec, right, up);

	const int color8 = (isRed ? 0x44 : 0x74);

	for (int i = 0; i < len; i++)
	{
		cparticle_t	*p = CL_InitParticle();
		if (!p) return;

		const float d = i * 0.1f;
		const float c = cosf(d);
		const float s = sinf(d);

		VectorScale(right, c, dir);
		VectorMA(dir, s, up, dir);

		p->alphavel = -1.0 / (1 + frand() * 0.2);
		color8_to_vec3(color8 + (rand() & 7), p->color);

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + dir[j] * 3;
			p->vel[j] = dir[j] * 6;
		}

		p->image = particle_classic;
		VectorAdd(move, vec, move);
	}

	const float dec = 0.75;
	VectorScale(vec, dec, vec);
	VectorCopy(start, move);

	while (len > 0)
	{
		len -= dec;

		cparticle_t	*p = CL_InitParticle();
		if (!p) return;

		p->alphavel = -1.0 / (0.6 + frand() * 0.2);
		color8_to_vec3(rand() & 7, p->color);

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand() * 3;
			p->vel[j] = crand() * 3;
		}

		p->image = particle_classic;
		VectorAdd(move, vec, move);
	}
}

/*
===============
CL_RailTrail
===============
*/
void CL_RailTrail (vec3_t start, vec3_t end, qboolean isRed)
{
	//mxd. Classic particles
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicRailTrail(start, end, isRed);
		return;
	}
	
	vec3_t		move, last;
	vec3_t		vec, point;
	int			beamred, beamgreen, beamblue;
	const qboolean colored = (cl_railtype->value != 0);

	VectorSubtract(end, start, vec);
	VectorNormalize(vec);
	CL_ParticleRailDecal(end, vec, 7, isRed);

	if (cl_railtype->value == 2)
	{
		CL_DevRailTrail(start, end, isRed);
		return;
	}

	// Draw from closest point
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
		len = min(len, cl_rail_length->value); // cap length

	VectorCopy(vec, point);
	VectorScale(vec, RAILTRAILSPACE, vec);

	if (colored)
	{
		if (isRed)
		{
			beamred = 255;
			beamgreen = beamblue = 20;
		}
		else
		{
			beamred = cl_railred->value;
			beamgreen = cl_railgreen->value;
			beamblue = cl_railblue->value;
		}
	}
	else
	{
		beamred = beamgreen = beamblue = 255;
	}

	while (len > 0)
	{
		VectorCopy(move, last);	
		VectorAdd(move, vec, move);

		len -= RAILTRAILSPACE;

		for (int i = 0; i < 3; i++)
		{
			CL_SetupParticle(
				last[0], last[1], last[2],
				move[0], move[1], move[2],
				0, 0, 0,
				0, 0, 0,
				beamred, beamgreen, beamblue,
				0, 0, 0,
				0.75, -0.75,
				GL_SRC_ALPHA, GL_ONE,
				RAILTRAILSPACE * TWOTHIRDS, (colored ? 0 : -5),
				particle_beam2,
				PART_BEAM,
				NULL, false);
		}
	}

	if (cl_railtype->value == 0)
		CL_RailSprial(start, end, isRed);
}


// RAFAEL
/*
===============
CL_ClassicIonripperTrail (mxd)
===============
*/
void CL_ClassicIonripperTrail(vec3_t start, vec3_t ent)
{
	qboolean left = false;

	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(ent, start, vec);
	float len = VectorNormalize(vec);

	const int dec = 5;
	VectorScale(vec, dec, vec);

	while (len > 0)
	{
		len -= dec;

		cparticle_t *p = CL_InitParticle();
		if (!p) return;

		p->alpha = 0.5f;
		p->alphavel = -1.0 / (0.3 + frand() * 0.2);
		color8_to_vec3(0xe4 + (rand() & 3), p->color);

		VectorCopy(move, p->org);
		p->vel[0] = 10 * (left ? 1 : -1);
		p->image = particle_classic;
		left = !left;

		VectorAdd(move, vec, move);
	}
}

/*
===============
CL_IonripperTrail
===============
*/
void CL_IonripperTrail (vec3_t start, vec3_t ent)
{
	//mxd. Classic particles
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicIonripperTrail(start, ent);
		return;
	}
	
	vec3_t move;
	vec3_t vec;
	vec3_t leftdir, up;

	VectorCopy(start, move);
	VectorSubtract(ent, start, vec);
	float len = VectorNormalize(vec);

	MakeNormalVectors(vec, leftdir, up);

	const int dec = 3 * cl_particle_scale->value;
	VectorScale(vec, dec, vec);

	while (len > 0)
	{
		len -= dec;

		CL_SetupParticle(
			0, 0, 0,
			move[0], move[1], move[2],
			0, 0, 0,
			0, 0, 0,
			255, 75, 0,
			0, 0, 0,
			0.75, -1.0 / (0.3 + frand() * 0.2),
			GL_SRC_ALPHA, GL_ONE,
			3, 0,			// was dec
			particle_generic,
			0,
			NULL, false);

		VectorAdd(move, vec, move);
	}
}

/*
===============
CL_ClassicBubbleTrail (mxd)
===============
*/
void CL_ClassicBubbleTrail(vec3_t start, vec3_t end)
{
	vec3_t move, vec;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	const float dec = 32;
	VectorScale(vec, dec, vec);

	for (int i = 0; i < len; i += dec)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p) return;

		p->alphavel = -1.0 / (1 + frand() * 0.2);
		color8_to_vec3(4 + (rand() & 7), p->color);

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand() * 2;
			p->vel[j] = crand() * 5;
		}
		p->vel[2] += 6;
		p->image = particle_classic;

		VectorAdd(move, vec, move);
	}
}


/*
===============
CL_BubbleTrail
===============
*/
void CL_BubbleTrail (vec3_t start, vec3_t end)
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
		const float size = (frand() > 0.25 ? 1 : (frand() > 0.5 ? 2 : (frand() > 0.75 ? 3 : 4)));

		CL_SetupParticle(
			0, 0, 0,
			move[0] + crand() * 2, move[1] + crand() * 2, move[2] + crand() * 2,
			crand() * 5, crand() * 5, crand() * 5 + 6,
			0, 0, 0,
			255, 255, 255,
			0, 0, 0,
			0.75, -0.5 / (1 + frand() * 0.2),
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			size, 1,
			particle_bubble,
			PART_TRANS | PART_SHADED,
			NULL, false);

		VectorAdd(move, vec, move);
	}
}


/*
===============
CL_FlyParticles
===============
*/
#define	BEAMLENGTH			16


void CL_FlyParticles (vec3_t origin, int count)
{
	count = min(NUMVERTEXNORMALS, count);

	if (!avelocities[0][0])
	{
		for (int i = 0; i < NUMVERTEXNORMALS; i++)
			for (int c = 0; c < 3; c++)
				avelocities[i][c] = (rand() & 255) * 0.01f;
	}

	vec3_t forward;
	const float ltime = (float)cl.time / 1000.0;

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

		CL_SetupParticle(
			0, 0, 0,
			origin[0] + vertexnormals[i][0] * dist + forward[0] * BEAMLENGTH,
			origin[1] + vertexnormals[i][1] * dist + forward[1] * BEAMLENGTH,
			origin[2] + vertexnormals[i][2] * dist + forward[2] * BEAMLENGTH,
			0, 0, 0,
			0, 0, 0,
			0, 0, 0,
			0, 0, 0,
			1, -100,
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			1 + sin(i + ltime), 1,
			particle_generic,
			PART_TRANS,
			NULL, false);
	}
}

/*
===============
CL_FlyEffect
===============
*/
void CL_FlyEffect (centity_t *ent, vec3_t origin)
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
		count = n * 162 / 20000.0;
	}
	else
	{
		n = ent->fly_stoptime - cl.time;
		if (n < 20000)
			count = n * 162 / 20000.0;
		else
			count = 162;
	}

	CL_FlyParticles(origin, count);
}


/*
===============
CL_ParticleBFGThink
===============
*/
void CL_ParticleBFGThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	vec3_t len;
	VectorSubtract(p->angle, p->org, len);
	
	*size = (float)(300 / VectorLength(len) * 0.75);
}

#define	BEAMLENGTH	16

/*
===============
CL_BfgParticles
===============
*/
void CL_BfgParticles (entity_t *ent)
{
	if (!avelocities[0][0])
	{
		for (int i = 0; i < NUMVERTEXNORMALS; i++)
			for (int c = 0; c < 3; c++)
				avelocities[i][c] = (rand() & 255) * 0.01f;
	}

	vec3_t v, forward;
	float dist = 64;
	const float ltime = (float)cl.time / 1000.0;

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

		cparticle_t *p = CL_SetupParticle(
			ent->origin[0], ent->origin[1], ent->origin[2],
			ent->origin[0] + vertexnormals[i][0] * dist + forward[0] * BEAMLENGTH,
			ent->origin[1] + vertexnormals[i][1] * dist + forward[1] * BEAMLENGTH,
			ent->origin[2] + vertexnormals[i][2] * dist + forward[2] * BEAMLENGTH,
			0, 0, 0,
			0, 0, 0,
			50, 200 * dist2, 20,
			0, 0, 0,
			1, -100,
			GL_SRC_ALPHA, GL_ONE,
			1, 1,
			particle_generic,
			0,
			CL_ParticleBFGThink, true);
		
		if (!p)
			return;

		VectorSubtract(p->org, ent->origin, v);
		dist = VectorLength(v) / 90.0;
	}
}


// RAFAEL
/*
===============
CL_TrapParticles
===============
*/
void CL_TrapParticles (entity_t *ent)
{
	vec3_t move;
	vec3_t vec;
	vec3_t start, end;

	ent->origin[2] -= 14;
	VectorCopy(ent->origin, start);
	VectorCopy(ent->origin, end);
	end[2] += 64;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	const int dec = 5;
	VectorScale(vec, 5, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		CL_SetupParticle(
			0, 0, 0,
			move[0] + crand(), move[1] + crand(), move[2] + crand(),
			crand() * 15, crand() * 15, crand() * 15,
			0, 0, PARTICLE_GRAVITY,
			230 + crand() * 25, 125 + crand() * 25, 25 + crand() * 25,
			0, 0, 0,
			1, -1.0 / (0.3 + frand() * 0.2),
			GL_SRC_ALPHA, GL_ONE,
			3, -3,
			particle_generic,
			0,
			NULL, false);

		VectorAdd(move, vec, move);
	}

	vec3_t dir, org;

	ent->origin[2] += 14;
	VectorCopy(ent->origin, org);

	for (int i = -2; i <= 2; i += 4)
	{
		for (int j = -2; j <= 2; j += 4)
		{
			for (int k = -2; k <= 4; k += 4)
			{
				VectorSet(dir, j * 8, i * 8, k * 8);
				VectorNormalize(dir);
				const float vel = 50 + rand() & 63;

				CL_SetupParticle(
					0, 0, 0,
					org[0] + i + ((rand() & 23) * crand()),
					org[1] + j + ((rand() & 23) * crand()),
					org[2] + k + ((rand() & 23) * crand()),
					dir[0] * vel, dir[1] * vel, dir[2] * vel,
					0, 0, 0,
					230 + crand() * 25, 125 + crand() * 25, 25 + crand() * 25,
					0, 0, 0,
					1, -1.0 / (0.3 + frand()*0.2),
					GL_SRC_ALPHA, GL_ONE,
					1, 1,
					particle_generic,
					PART_GRAVITY,
					NULL, false);
			}
		}
	}
}


/*
===============
CL_BFGExplosionParticles
===============
*/
//FIXME combined with CL_ExplosionParticles
void CL_BFGExplosionParticles (vec3_t org)
{
	for (int i = 0; i < 256; i++)
	{
		CL_SetupParticle(
			0, 0, 0,
			org[0] + ((rand() % 32) - 16), org[1] + ((rand() % 32) - 16), org[2] + ((rand() % 32) - 16),
			(rand() % 150) - 75, (rand() % 150) - 75, (rand() % 150) - 75,
			0, 0, 0,
			50, 100 + rand() * 50, 0, //Knightmare- made more green
			0, 0, 0,
			1, -0.8 / (0.5 + frand() * 0.3),
			GL_SRC_ALPHA, GL_ONE,
			10, -10,
			particle_generic,
			PART_GRAVITY,
			NULL, false);
	}
}


/*
===============
CL_TeleportParticles
===============
*/
void CL_TeleportParticles (vec3_t org)
{
	vec3_t dir, color;

	for (int i = -16; i <= 16; i += 4)
	{
		for (int j = -16; j <= 16; j += 4)
		{
			for (int k = -16; k <= 32; k += 4)
			{
				VectorSet(dir, j * 16, i * 16, k * 16);
				VectorNormalize(dir);
				const float vel = 150 + (rand() & 63);

				if (r_particle_mode->integer == 1) //mxd
					VectorSet(color, 200 + 55 * rand(), 200 + 55 * rand(), 200 + 55 * rand());
				else
					color8_to_vec3(7 + (rand() & 7), color);

				CL_SetupParticle(
					0, 0, 0,
					org[0] + i + (rand() & 3), org[1] + j + (rand() & 3), org[2] + k + (rand() & 3),
					dir[0] * vel, dir[1] * vel, dir[2] * vel,
					0, 0, 0,
					color[0], color[1], color[2],
					0, 0, 0,
					1, -1.0 / (0.3 + (rand() & 7) * 0.02),
					GL_SRC_ALPHA, GL_ONE,
					1, 3,
					particle_generic,
					PART_GRAVITY,
					NULL, false);
			}
		}
	}
}


/*
===============
CL_ColorFlash - flash of light
===============
*/
void CL_ColorFlash (vec3_t pos, int ent, int intensity, float r, float g, float b)
{
	cdlight_t *dl = CL_AllocDlight(ent);
	VectorCopy(pos, dl->origin);
	dl->radius = intensity;
	dl->minlight = 250;
	dl->die = cl.time + 100;
	VectorSet(dl->color, r, g, b); //mxd
}

/*
===============
CL_Flashlight
===============
*/
void CL_Flashlight(int ent, vec3_t pos)
{
	CL_ColorFlash(pos, ent, 400, 1, 1, 1); //mxd
}


/*
===============
CL_DebugTrail
===============
*/
void CL_DebugTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	vec3_t		right, up;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	MakeNormalVectors(vec, right, up);

	const float dec = 8; // was 2
	VectorScale(vec, dec, vec);
	VectorCopy(start, move);

	while (len > 0)
	{
		len -= dec;

		CL_SetupParticle(
			0, 0, 0,
			move[0], move[1], move[2],
			0, 0, 0,
			0, 0, 0,
			50, 50, 255,
			0, 0, 0,
			1, -0.75,
			GL_SRC_ALPHA, GL_ONE,
			7.5, 0,
			particle_generic,
			0,
			NULL, false);

		VectorAdd(move, vec, move);
	}
}


/*
===============
CL_ForceWall
===============
*/
void CL_ForceWall (vec3_t start, vec3_t end, int color8)
{
	vec3_t move, vec;
	vec3_t color = { color8red(color8), color8green(color8), color8blue(color8)};

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	VectorScale(vec, 4, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= 4;
		
		if (frand() > 0.3)
		{
			CL_SetupParticle(
				0, 0, 0,
				move[0] + crand() * 3, move[1] + crand() * 3, move[2] + crand() * 3,
				0, 0, -40 - (crand() * 10),
				0, 0, 0,
				color[0] + 5, color[1] + 5, color[2] + 5,
				0, 0, 0,
				1, -1.0 / (3.0 + frand() * 0.5),
				GL_SRC_ALPHA, GL_ONE,
				5, 0,
				particle_generic,
				0,
				NULL, false);
		}

		VectorAdd (move, vec, move);
	}
}


/*
===============
CL_ClassicBubbleTrail2 (mxd)
===============
*/
void CL_ClassicBubbleTrail2(vec3_t start, vec3_t end, int dist)
{
	vec3_t move, vec;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	const float dec = dist;
	VectorScale(vec, dec, vec);

	for (int i = 0; i < len; i += dec)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p) return;

		p->alphavel = -1.0 / (1 + frand() * 0.1);
		color8_to_vec3(4 + (rand() & 7), p->color);

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand() * 2;
			p->vel[j] = crand() * 10;
		}

		p->org[2] -= 4;
		p->vel[2] += 20;
		p->image = particle_classic;

		VectorAdd(move, vec, move);
	}
}


/*
===============
CL_BubbleTrail2 (lets you control the # of bubbles by setting the distance between the spawns)
===============
*/
void CL_BubbleTrail2 (vec3_t start, vec3_t end, int dist)
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
		vec3_t color, velocity; //mxd
		VectorSet(color, 255, 255, 255);
		VectorSet(velocity, crand() * 5, crand() * 5, crand() * 5 + 6);

		const float size = (frand() > 0.25 ? 1 : (frand() > 0.5 ? 2 : (frand() > 0.75 ? 3 : 4)));
		CL_SetupParticle(
			0, 0, 0,
			move[0] + crand() * 2, move[1] + crand() * 2, move[2] + crand() * 2 - 4,
			velocity[0], velocity[1], velocity[2],
			0, 0, 0,
			color[0], color[1], color[2],
			0, 0, 0,
			0.75, -0.5 / (1 + frand() * 0.2),
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			size, 1,
			particle_bubble,
			PART_TRANS | PART_SHADED,
			NULL, false);

		VectorAdd(move, vec, move);
	}
}


/*
===============
CL_HeatbeamParticles
===============
*/
void CL_HeatbeamParticles (vec3_t start, vec3_t forward)
{
	vec3_t		move;
	vec3_t		vec;
	vec3_t		right, up;
	vec3_t		dir;
	float		ltime;
	float		step;
	float		variance;
	float		size;
	int			maxsteps;
	vec3_t		end;

	VectorMA(start, 4096, forward, end);

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	// FIXME - pmm - these might end up using old values?
	VectorCopy(cl.v_right, right);
	VectorCopy(cl.v_up, up);

	if (cg_thirdperson->value)
	{
		ltime = (float)cl.time / 250.0;
		step = 96;
		maxsteps = 10;
		variance = 1.2;
		size = 2;
	}
	else
	{
		ltime = (float)cl.time / 1000.0;
		step = 32;
		maxsteps = 7;
		variance = 0.5;
		size = 1;
	}

	const float start_pt = fmod(ltime * 96.0, step);
	VectorMA(move, start_pt, vec, move);

	VectorScale(vec, step, vec);

	const float rstep = M_PI / 10.0 * min(cl_particle_scale->value, 2);
	for (int i = start_pt; i < len; i += step)
	{
		if (i > step * maxsteps) // don't bother after the nth ring
			break;

		for (float rot = 0; rot < M_PI * 2; rot += rstep)
		{
			const float c = cosf(rot) * variance;
			const float s = sinf(rot) * variance;
			
			// trim it so it looks like it's starting at the origin
			if (i < 10)
			{
				VectorScale(right, c * (i / 10.0), dir);
				VectorMA(dir, s * (i / 10.0), up, dir);
			}
			else
			{
				VectorScale(right, c, dir);
				VectorMA(dir, s, up, dir);
			}

			CL_SetupParticle(
				0, 0, 0,
				move[0] + dir[0] * 2, move[1] + dir[1] * 2, move[2] + dir[2] * 2, //Knightmare- decreased radius
				0, 0, 0,
				0, 0, 0,
				200 + rand() * 50, 200 + rand() * 25, rand() * 50,
				0, 0, 0,
				0.25, -1000.0, // decreased alpha
				GL_SRC_ALPHA, GL_ONE,
				size, 1,		// shrunk size
				particle_blaster,
				0,
				NULL, false);
		}

		VectorAdd(move, vec, move);
	}
}


/*
===============
CL_ParticleSteamEffect

Puffs with velocity along direction, with some randomness thrown in
===============
*/
void CL_ParticleSteamEffect (vec3_t org, vec3_t dir, int red, int green, int blue, int reddelta, int greendelta, int bluedelta, int count, int magnitude)
{
	vec3_t r, u;
	MakeNormalVectors(dir, r, u);

	const int particletype = (r_particle_mode->integer == 1 ? particle_smoke : particle_classic); //mxd

	for (int i = 0; i < count; i++)
	{
		cparticle_t *p = CL_SetupParticle(
			0, 0, 0,
			org[0] + magnitude * 0.1 * crand(), 
			org[1] + magnitude * 0.1 * crand(), 
			org[2] + magnitude * 0.1 * crand(),
			0, 0, 0,
			0, 0, 0,
			red, green, blue,
			reddelta, greendelta, bluedelta,
			0.5, -1.0 / (0.5 + frand() * 0.3),
			GL_SRC_ALPHA, GL_ONE,
			4, -2,
			particletype,
			0,
			NULL, false);

		if (!p)
			return;

		VectorScale(dir, magnitude, p->vel);
		float d = crand() * magnitude / 3;
		VectorMA(p->vel, d, r, p->vel);
		d = crand() * magnitude / 3;
		VectorMA(p->vel, d, u, p->vel);
	}
}


/*
===============
CL_ParticleSteamEffect2

Puffs with velocity along direction, with some randomness thrown in
===============
*/
void CL_ParticleSteamEffect2 (cl_sustain_t *self)
{
	const int color8 = self->color + (rand() & 7);
	vec3_t color = { color8red(color8), color8green(color8), color8blue(color8) };

	vec3_t dir, r, u;
	VectorCopy(self->dir, dir);
	MakeNormalVectors(dir, r, u);

	const int particletype = (r_particle_mode->integer == 1 ? particle_smoke : particle_classic); //mxd

	for (int i = 0; i < self->count; i++)
	{
		cparticle_t *p = CL_SetupParticle(
			0, 0, 0,
			self->org[0] + self->magnitude * 0.1 * crand(), 
			self->org[1] + self->magnitude * 0.1 * crand(), 
			self->org[2] + self->magnitude * 0.1 * crand(),
			0, 0, 0,
			0, 0, 0,
			color[0], color[1], color[2],
			0, 0, 0,
			1.0, -1.0 / (0.5 + frand() * 0.3),
			GL_SRC_ALPHA, GL_ONE,
			4, 0,
			particletype,
			PART_GRAVITY,
			NULL, false);

		if (!p)
			return;

		VectorScale(dir, self->magnitude, p->vel);
		float d = crand() * self->magnitude / 3;
		VectorMA(p->vel, d, r, p->vel);
		d = crand() * self->magnitude / 3;
		VectorMA(p->vel, d, u, p->vel);
	}

	self->nextthink += self->thinkinterval;
}


/*
===============
CL_TrackerTrail
===============
*/
void CL_TrackerTrail (vec3_t start, vec3_t end)
{
	vec3_t move;
	vec3_t vec;
	vec3_t forward, right, up, angle_dir;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	VectorCopy(vec, forward);
	vectoangles(forward, angle_dir);
	AngleVectors(angle_dir, forward, right, up);

	const float dec = 3 * max(cl_particle_scale->value / 2, 1);
	VectorScale(vec, dec, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		cparticle_t *p = CL_SetupParticle (
			0,	0,	0,
			0,	0,	0,
			0,	0,	5,
			0,	0,	0,
			0,	0,	0,
			0,	0,	0,
			1.0,	-2.0,
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			2,		0,			
			particle_generic,
			PART_TRANS,
			NULL, false);

		if (!p)
			return;

		const float dist = DotProduct(move, forward);
		VectorMA(move, 8 * cosf(dist), up, p->org);

		VectorAdd(move, vec, move);
	}
}


/*
===============
CL_TrackerShell
===============
*/
void CL_Tracker_Shell(vec3_t origin)
{
	for(int i = 0; i < 300 / cl_particle_scale->value; i++)
	{
		cparticle_t *p = CL_SetupParticle (
			0,	0,	0,
			0,	0,	0,
			0,	0,	0,
			0,	0,	0,
			0,	0,	0,
			0,	0,	0,
			1.0,	-2.0,
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			1,		0,	//Knightmare- changed size		
			particle_generic,
			PART_TRANS,
			NULL, false);

		if (!p)
			return;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
		VectorMA(origin, 40, dir, p->org);
	}
}


/*
======================
CL_MonsterPlasma_Shell
======================
*/
void CL_MonsterPlasma_Shell(vec3_t origin)
{
	for(int i = 0; i < 40; i++)
	{
		cparticle_t *p = CL_SetupParticle (
			0,		0,		0,
			0,		0,		0,
			0,		0,		0,
			0,		0,		0,
			220,	140,	50, //Knightmare- this was all black
			0,		0,		0,
			1.0,	INSTANT_PARTICLE,
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			2,		0,			
			particle_generic,
			PART_TRANS|PART_INSTANT,
			NULL, false);

		if (!p)
			return;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
	
		VectorMA(origin, 10, dir, p->org);
	}
}

/*
===============
CL_ClassicWidowbeamout (mxd)
===============
*/
void CL_ClassicWidowbeamout(cl_sustain_t *self)
{
	static int colortable[4] = { 2 * 8, 13 * 8, 21 * 8, 18 * 8 };
	const float ratio = 1.0 - (((float)self->endtime - (float)cl.time) / 2100.0);

	for (int i = 0; i < 300; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p) return;

		color8_to_vec3(colortable[rand() & 3], p->color);
		p->alphavel = INSTANT_PARTICLE;
		p->flags = PART_INSTANT;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
		VectorMA(self->org, 45.0 * ratio, dir, p->org);
	}
}


/*
===============
CL_Widowbeamout
===============
*/
void CL_Widowbeamout (cl_sustain_t *self)
{
	//mxd. Classic particles
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicWidowbeamout(self);
		return;
	}
	
	static int colortable0[6] = {125,	255,	185,	125,	185,	255};
	static int colortable1[6] = {185,	125,	255,	255,	125,	185};
	static int colortable2[6] = {255,	185,	125,	185,	255,	125};

	const float ratio = 1.0 - (((float)self->endtime - (float)cl.time) / 2100.0);

	for(int i = 0; i < 300; i++)
	{
		const int index = rand() & 5;
		cparticle_t *p = CL_SetupParticle (
			0,	0,	0,
			0,	0,	0,
			0,	0,	0,
			0,	0,	0,
			colortable0[index],	colortable1[index],	colortable2[index],
			0,	0,	0,
			1.0,	INSTANT_PARTICLE,
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			2,		0,			
			particle_generic,
			PART_TRANS|PART_INSTANT,
			NULL, false);

		if (!p)
			return;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
	
		VectorMA(self->org, (45.0 * ratio), dir, p->org);
	}
}

/*
============
CL_ClassicNukeblast (mxd)
============
*/
void CL_ClassicNukeblast(cl_sustain_t *self)
{
	static int colortable[4] = { 110, 112, 114, 116 };
	const float ratio = 1.0 - (((float)self->endtime - (float)cl.time) / 1000.0);

	for (int i = 0; i < (700 / cl_particle_scale->value); i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p) return;

		color8_to_vec3(colortable[rand() & 3], p->color);
		p->alphavel = INSTANT_PARTICLE;
		p->flags = PART_INSTANT;
		p->image = particle_classic;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);

		VectorMA(self->org, 200.0 * ratio, dir, p->org);
	}
}


/*
============
CL_Nukeblast
============
*/
void CL_Nukeblast (cl_sustain_t *self)
{
	//mxd. Classic particles...
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicNukeblast(self);
		return;
	}
	
	static int colortable0[4] = {185,	155,	125,	95};
	static int colortable1[4] = {185,	155,	125,	95};
	static int colortable2[4] = {255,	255,	255,	255};

	const float ratio = 1.0 - (((float)self->endtime - (float)cl.time) / 1000.0);
	const float size = ratio * ratio;

	for(int i = 0; i < (700 / cl_particle_scale->value); i++)
	{
		const int index = rand()&3;
		cparticle_t *p = CL_SetupParticle (
			0,	0,	0,
			0,	0,	0,
			0,	0,	0,
			0,	0,	0,
			colortable0[index],	colortable1[index],	colortable2[index],
			0,	0,	0,
			1-size,	INSTANT_PARTICLE,
			GL_SRC_ALPHA, GL_ONE,
			10 * (0.5 + ratio * 0.5),	-1,
			particle_generic,
			PART_INSTANT,
			NULL, false);

		if (!p)
			return;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
		VectorScale(dir, -1, p->angle);

		VectorMA(self->org, 200.0 * size, dir, p->org); //was 100
		VectorMA(vec3_origin, 400.0 * size, dir, p->vel); //was 100

	}
}


/*
==============
CL_ClassicWidowSplash (mxd)
==============
*/
void CL_ClassicWidowSplash(vec3_t org)
{
	static int colortable[4] = { 2 * 8, 13 * 8, 21 * 8, 18 * 8 };
	
	for (int i = 0; i < 256; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p) return;

		color8_to_vec3(colortable[rand() & 3], p->color);

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
		VectorMA(org, 45.0, dir, p->org);
		VectorMA(vec3_origin, 40.0, dir, p->vel);

		p->alphavel = -0.8 / (0.5 + frand()*0.3);
	}
}


/*
==============
CL_WidowSplash
==============
*/
void CL_WidowSplash (vec3_t org)
{
	//mxd. Classic particles
	if (r_particle_mode->integer == 0)
	{
		CL_ClassicWidowSplash(org);
		return;
	}
	
	for (int i = 0; i < 256; i++)
	{
		cparticle_t *p = CL_SetupParticle (
			0,	0,	0,
			0,	0,	0,
			0,	0,	0,
			0,	0,	0,
			rand() & 255,	rand() & 255,	rand() & 255,
			0,	0,	0,
			1.0,		-0.8 / (0.5 + frand() * 0.3),
			GL_SRC_ALPHA, GL_ONE,
			3,			0,			
			particle_generic,
			0,
			NULL, false);

		if (!p)
			return;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand()); //mxd
		VectorNormalize(dir);

		VectorMA(org, 45.0, dir, p->org);
		VectorMA(vec3_origin, 40.0, dir, p->vel);
	}
}


/*
==================
CL_Tracker_Explode
==================
*/
void CL_Tracker_Explode (vec3_t	origin)
{
	for (int i = 0; i < (300 / cl_particle_scale->value); i++)
	{
		cparticle_t *p = CL_SetupParticle (
			0,		0,		0,
			0,		0,		0,
			0,		0,		0,
			0,		0,		-10, //was 20
			0,		0,		0,
			0,		0,		0,
			1.0,	-0.5,
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			2,		0,			
			particle_generic,
			PART_TRANS,
			NULL, false);

		if (!p)
			return;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand()); //mxd
		VectorNormalize(dir);

		VectorCopy(origin, p->org); //Knightmare- start at center, not edge
		VectorScale(dir, crand() * 128, p->vel); //was backdir, 64
	}
	
}


/*
===============
CL_TagTrail
===============
*/
void CL_TagTrail (vec3_t start, vec3_t end, int color8)
{
	vec3_t		move;
	vec3_t		vec;
	vec3_t color = { color8red(color8), color8green(color8), color8blue(color8)};

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	const int dec = 5;
	VectorScale(vec, 5, vec);

	while (len >= 0)
	{
		len -= dec;

		CL_SetupParticle(
			0, 0, 0,
			move[0] + crand() * 16, move[1] + crand() * 16, move[2] + crand() * 16,
			crand() * 5, crand() * 5, crand() * 5,
			0, 0, 20,
			color[0], color[1], color[2],
			0, 0, 0,
			1.0, -1.0 / (0.8 + frand() * 0.2),
			GL_SRC_ALPHA, GL_ONE,
			1.5, 0,
			particle_generic,
			0,
			NULL, false);

		VectorAdd(move, vec, move);
	}
}


/*
==========================
CL_ColorExplosionParticles
==========================
*/
void CL_ColorExplosionParticles (vec3_t org, int color8, int run)
{
	for (int i = 0; i < 128; i++) //mxd. Classic implementation
	{
		cparticle_t	*p = CL_InitParticle();
		if (!p) return;

		color8_to_vec3(color8 + (rand() % run), p->color);

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() % 32) - 16);
			p->vel[j] = (rand() % 256) - 128;
		}

		p->accel[2] = -PARTICLE_GRAVITY;
		p->alphavel = -0.4 / (0.6 + frand() * 0.2);
		p->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
		p->size = 2;
	}
}



/*
=======================
CL_ParticleSmokeEffect - like the steam effect, but unaffected by gravity
=======================
*/
void CL_ParticleSmokeEffect (vec3_t org, vec3_t dir, float size)
{
	const float alpha = frand() * 0.25 + 0.75;

	CL_SetupParticle(
		crand() * 180, crand() * 100, 0,
		org[0], org[1], org[2],
		dir[0], dir[1], dir[2],
		0, 0, 10,
		255, 255, 255,
		0, 0, 0,
		alpha, -1.0,
		GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
		size, 5,
		particle_smoke,
		PART_TRANS | PART_SHADED | PART_OVERBRIGHT,
		CL_ParticleRotateThink, true);
}

//mxd
void CL_ClassicParticleSmokeEffect(vec3_t org, vec3_t dir, int color8, int count, int magnitude)
{
	vec3_t r, u;
	MakeNormalVectors(dir, r, u);

	for (int i = 0; i < count; i++)
	{
		cparticle_t	*p = CL_InitParticle();
		if (!p) return;

		color8_to_vec3(color8 + (rand() & 7), p->color);

		for (int j = 0; j < 3; j++)
			p->org[j] = org[j] + magnitude * 0.1 * crand();

		VectorScale(dir, magnitude, p->vel);
		float d = crand()*magnitude / 3;
		VectorMA(p->vel, d, r, p->vel);
		d = crand() * magnitude / 3;
		VectorMA(p->vel, d, u, p->vel);

		p->alphavel = -1.0 / (0.5 + frand() * 0.3);
		p->image = particle_classic;
	}
}


/*
===============
CL_ParticleElectricSparksThink
===============
*/
void CL_ParticleElectricSparksThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	const float time1 = *time;
	const float time2 = time1 * time1;

	for (int i = 0; i < 2; i++)
		angle[i] = 0.25 * (p->vel[i] * time1 + p->accel[i] * time2);
	angle[2] = 0.25 * (p->vel[2] * time1 + (p->accel[2] - PARTICLE_GRAVITY) * time2);

	p->thinknext = true;
}

/*
===============
CL_ElectricParticles

new sparks for Rogue turrets
===============
*/
void CL_ElectricParticles (vec3_t org, vec3_t dir, int count)
{
	vec3_t start;

	for (int i = 0; i < count; i++)
	{
		const float d = rand() & 31;
		for (int j = 0; j < 3; j++)
			start[j] = org[j] + ((rand() & 7) - 4) + d * dir[j];

		CL_SetupParticle(
			0, 0, 0,
			start[0], start[1], start[2],
			crand() * 20, crand() * 20, crand() * 20,
			0, 0, -PARTICLE_GRAVITY,
			25, 100, 255,
			50, 50, 50,
			1, -1.0 / (0.5 + frand() * 0.3),
			GL_SRC_ALPHA, GL_ONE,
			6, -3,
			particle_solid,
			PART_GRAVITY | PART_SPARK,
			CL_ParticleElectricSparksThink, true);
	}
}


//Knightmare- removed for Psychospaz's enhanced particle code
#if 0
/*
===============
CL_SmokeTrail
===============
*/
void CL_SmokeTrail (vec3_t start, vec3_t end, int colorStart, int colorRun, int spacing)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;

	VectorCopy(start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	VectorScale (vec, spacing, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= spacing;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1+frand()*0.5);
		p->color = colorStart + (rand() % colorRun);
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = move[j] + crand()*3;
			p->accel[j] = 0;
		}
		p->vel[2] = 20 + crand()*5;

		VectorAdd (move, vec, move);
	}
}


/*
===============
CL_FlameEffects
===============
*/
void CL_FlameEffects (centity_t *ent, vec3_t origin)
{
	int			n, count;
	int			j;
	cparticle_t	*p;

	count = rand() & 0xF;

	for(n=0;n<count;n++)
	{
		if (!free_particles)
			return;
			
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		
		VectorClear (p->accel);
		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1+frand()*0.2);
		p->color = 226 + (rand() % 4);
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = origin[j] + crand()*5;
			p->vel[j] = crand()*5;
		}
		p->vel[2] = crand() * -10;
		p->accel[2] = -PARTICLE_GRAVITY;
	}

	count = rand() & 0x7;

	for(n=0;n<count;n++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1+frand()*0.5);
		p->color = 0 + (rand() % 4);
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = origin[j] + crand()*3;
		}
		p->vel[2] = 20 + crand()*5;
	}

}


/*
===============
CL_GenericParticleEffect
===============
*/
void CL_GenericParticleEffect (vec3_t org, vec3_t dir, int color, int count, int numcolors, int dirspread, float alphavel)
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		if (numcolors > 1)
			p->color = color + (rand() & numcolors);
		else
			p->color = color;

		d = rand() & dirspread;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()&7)-4) + d*dir[j];
			p->vel[j] = crand()*20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
//		VectorCopy(accel, p->accel);
		p->alpha = 1.0;

		p->alphavel = -1.0 / (0.5 + frand()*alphavel);
//		p->alphavel = alphavel;
	}
}
#endif
//end Knightmare