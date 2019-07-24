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

// cl_particle.c -- particle effect and decal management

#include "client.h"
#include "particles.h"

// Initializes all particle images
void CL_SetParticleImages(void)
{
	R_SetParticlePicture(particle_solid,		"***whitetexture***"); // Only used for sparks
	R_SetParticlePicture(particle_generic,		"gfx/particles/basic.tga");
	R_SetParticlePicture(particle_smoke,		"gfx/particles/smoke.tga");
	R_SetParticlePicture(particle_blood,		"gfx/particles/blood.tga");
	R_SetParticlePicture(particle_blooddrop,	"gfx/particles/blood_drop.tga");
	R_SetParticlePicture(particle_blooddrip,	"gfx/particles/blood_drip.tga");
	R_SetParticlePicture(particle_redblood,		"gfx/particles/blood_red.tga");
	R_SetParticlePicture(particle_bubble,		"gfx/particles/bubble.tga");
	R_SetParticlePicture(particle_blaster,		"gfx/particles/blaster.tga");
	R_SetParticlePicture(particle_blasterblob,	"gfx/particles/blaster_blob.tga");
	R_SetParticlePicture(particle_beam,			"gfx/particles/beam.tga");
	R_SetParticlePicture(particle_beam2,		"gfx/particles/beam2.tga"); // Only used for railgun
	R_SetParticlePicture(particle_lightning,	"gfx/particles/lightning.tga");
	R_SetParticlePicture(particle_inferno,		"gfx/particles/inferno.tga");
	
	// Animations
	// Rocket explosion
	R_SetParticlePicture(particle_rflash,		"gfx/particles/r_flash.tga");
	R_SetParticlePicture(particle_rexplosion1,	"gfx/particles/r_explod_1.tga");
	R_SetParticlePicture(particle_rexplosion2,	"gfx/particles/r_explod_2.tga");
	R_SetParticlePicture(particle_rexplosion3,	"gfx/particles/r_explod_3.tga");
	R_SetParticlePicture(particle_rexplosion4,	"gfx/particles/r_explod_4.tga");
	R_SetParticlePicture(particle_rexplosion5,	"gfx/particles/r_explod_5.tga");
	R_SetParticlePicture(particle_rexplosion6,	"gfx/particles/r_explod_6.tga");
	R_SetParticlePicture(particle_rexplosion7,	"gfx/particles/r_explod_7.tga");

	R_SetParticlePicture(particle_bfgmark,		"gfx/decals/bfgmark.tga");
	R_SetParticlePicture(particle_burnmark,		"gfx/decals/burnmark.tga");
	R_SetParticlePicture(particle_blooddecal1,	"gfx/decals/blood_1.tga");
	R_SetParticlePicture(particle_blooddecal2,	"gfx/decals/blood_2.tga");
	R_SetParticlePicture(particle_blooddecal3,	"gfx/decals/blood_3.tga");
	R_SetParticlePicture(particle_blooddecal4,	"gfx/decals/blood_4.tga");
	R_SetParticlePicture(particle_blooddecal5,	"gfx/decals/blood_5.tga");
	R_SetParticlePicture(particle_shadow,		"gfx/decals/shadow.tga");
	R_SetParticlePicture(particle_bulletmark,	"gfx/decals/bulletmark.tga");
	R_SetParticlePicture(particle_trackermark,	"gfx/decals/trackermark.tga");

	//mxd. Classic particles...
	R_SetParticlePicture(particle_classic, "***particle***");
}

#pragma region ======================= Particle management

cparticle_t *active_particles;
cparticle_t *free_particles;
static cparticle_t particles[MAX_PARTICLES];

static decalpolys_t *active_decals;
static decalpolys_t *free_decals;
static decalpolys_t decalfrags[MAX_DECAL_FRAGS];

// Cleans up the active_particles linked list
static void CL_CleanDecalPolys()
{
	decalpolys_t *next;
	decalpolys_t *active = NULL;
	decalpolys_t *tail = NULL;

	for (decalpolys_t *d = active_decals; d; d = next)
	{
		next = d->next;
		if (d->clearflag)
		{
			d->clearflag = false;
			d->numpolys = 0;
			d->nextpoly = NULL;
			d->node = NULL; // vis node
			d->next = free_decals;
			free_decals = d;

			continue;
		}
		d->next = NULL;

		if (!tail)
		{
			active = tail = d;
		}
		else
		{
			tail->next = d;
			tail = d;
		}
	}

	active_decals = active;
}

// Recursively flags a decal poly chain for cleaning
static void CL_ClearDecalPoly(decalpolys_t *decal)
{
	if (!decal)
		return;

	if (decal->nextpoly)
		CL_ClearDecalPoly(decal->nextpoly);

	decal->clearflag = true; // Tell cleaning loop to clean this up
}

// Clears all decal polys
static void CL_ClearAllDecalPolys()
{
	free_decals = &decalfrags[0];
	active_decals = NULL;

	for (int i = 0; i < MAX_DECAL_FRAGS; i++)
	{
		decalfrags[i].next = &decalfrags[i + 1];
		decalfrags[i].clearflag = false;
		decalfrags[i].numpolys = 0;
		decalfrags[i].nextpoly = NULL;
		decalfrags[i].node = NULL; // vis node
	}

	decalfrags[MAX_DECAL_FRAGS - 1].next = NULL;
}

// Retuns number of available decalpoly_t fields
static int CL_NumFreeDecalPolys()
{
	int count = 0;
	for (decalpolys_t *d = free_decals; d; d = d->next)
		count++;

	return count;
}

// Retuns first free decal poly
static decalpolys_t *CL_NewDecalPoly()
{
	if (!free_decals)
		return NULL;

	decalpolys_t *d = free_decals;
	free_decals = d->next;
	d->next = active_decals;
	active_decals = d;

	return d;
}

void CL_ClipDecal(cparticle_t *part, const float radius, const float orient, const vec3_t origin, const vec3_t dir)
{
	// Invalid decal
	if (radius <= 0 || VectorCompare(dir, vec3_origin)) 
		return;

	// Calculate orientation matrix
	vec3_t axis[3];
	VectorNormalize2(dir, axis[0]);
	PerpendicularVector(axis[1], axis[0]);
	RotatePointAroundVector(axis[2], axis[0], axis[1], orient);
	CrossProduct(axis[0], axis[2], axis[1]);

	vec3_t verts[MAX_DECAL_VERTS];
	markFragment_t fragments[MAX_FRAGMENTS_PER_DECAL];
	const int numfragments = R_MarkFragments(origin, axis, radius, MAX_DECAL_VERTS, verts, MAX_FRAGMENTS_PER_DECAL, fragments);
	
	if (numfragments == 0 || numfragments > CL_NumFreeDecalPolys()) // Nothing to display / not enough decalpolys free
		return;
	
	VectorScale(axis[1], 0.5f / radius, axis[1]);
	VectorScale(axis[2], 0.5f / radius, axis[2]);

	part->decalnum = numfragments;

	markFragment_t *fr = fragments;
	for (int i = 0; i < numfragments; i++, fr++)
	{
		decalpolys_t *decal = CL_NewDecalPoly();
		vec3_t v;

		if (!decal)
			return;

		decal->nextpoly = part->decal;
		part->decal = decal;
		decal->node = fr->node; // vis node

		for (int j = 0; j < fr->numPoints && j < MAX_VERTS_PER_FRAGMENT; j++)
		{
			VectorCopy(verts[fr->firstPoint + j], decal->polys[j]);
			VectorSubtract(decal->polys[j], origin, v);
			decal->coords[j][0] = DotProduct(v, axis[1]) + 0.5f;
			decal->coords[j][1] = DotProduct(v, axis[2]) + 0.5f;
			decal->numpolys = fr->numPoints;
		}
	}
}

//mxd
cparticle_t *CL_InitParticle()
{
	if (!free_particles)
		return NULL;

	cparticle_t *p = free_particles;
	free_particles = p->next;

	memset(p, 0, sizeof(cparticle_t)); // Reset properties...

	p->next = active_particles;
	active_particles = p;

	// Set defaults...
	p->time = cl.time;

	VectorSet(p->color, 255, 255, 255);

	p->blendfunc_src = GL_SRC_ALPHA;
	p->blendfunc_dst = GL_ONE;

	p->alpha = 1;
	p->size = 1;
	p->type = particle_generic;

	return p;
}

//mxd. Must be called after CL_InitParticle() to finish particle setup...
void CL_FinishParticleInit(cparticle_t *p)
{
	// Store previous origin...
	VectorCopy(p->org, p->oldorg);
	
	// Add decal?
	if (p->flags & PART_DECAL)
	{
		vec3_t dir;
		AngleVectors(p->angle, dir, NULL, NULL);
		VectorNegate(dir, dir);
		CL_ClipDecal(p, p->size, -p->angle[2], p->org, dir);

		if (!p->decalnum) // Kill on viewframe
			p->alpha = 0;
	}
	// Setup for classic particles mode? Don't do this for decals, because they have no classic counterpart and are controlled by a separate cvar.
	else if (p->type == particle_generic && r_particle_mode->integer == 0)
	{
		p->alpha = 1;
		p->type = particle_classic;
		p->size = 1;
		p->sizevel = 0;
		p->blendfunc_src = GL_SRC_ALPHA;
		p->blendfunc_dst = GL_ONE;
		VectorClear(p->colorvel);
	}
}

void CL_AddParticleLight(cparticle_t *p, const float light, const float lightvel, const float lcol0, const float lcol1, const float lcol2)
{
	for (int i = 0; i < MAX_PARTICLE_LIGHTS; i++)
	{
		cplight_t *plight = &p->lights[i];
		if (!plight->isactive)
		{
			plight->isactive = true;
			plight->light = light;
			plight->lightvel = lightvel;
			VectorSet(plight->lightcol, lcol0, lcol1, lcol2); //mxd

			return;
		}
	}
}

static void CL_ClearParticles()
{
	free_particles = &particles[0];
	active_particles = NULL;

	for (int i = 0; i < MAX_PARTICLES; i++)
	{
		particles[i].next = &particles[i+1];
		particles[i].decalnum = 0; // Knightmare added
		particles[i].decal = NULL; // Knightmare added
	}

	particles[MAX_PARTICLES - 1].next = NULL;
}

#pragma endregion

#pragma region ======================= Generic particle thinking routines

#define	STOP_EPSILON	0.1f

void CL_CalcPartVelocity(const cparticle_t *p, const float scale, const float time, vec3_t velocity)
{
	const float timesq = time * time;
	const int gravity = (p->flags & PART_GRAVITY ? PARTICLE_GRAVITY : 0); //mxd

	for (int i = 0; i < 2; i++)
		velocity[i] = scale * (p->vel[i] * time + p->accel[i] * timesq);
	velocity[2] = scale * (p->vel[2] * time + (p->accel[2] - gravity) * timesq);
}

static void CL_ClipParticleVelocity(const vec3_t in, const vec3_t normal, vec3_t out)
{
	const float backoff = VectorLength(in) * 0.25f + DotProduct(in, normal) * 3.0f;

	for (int i = 0; i < 3; i++)
	{
		const float change = normal[i] * backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}
}

void CL_ParticleBounceThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	const float clipsize = max(*size * 0.5f, 0.25f);
	const trace_t tr = CL_BrushTrace(p->oldorg, org, clipsize, MASK_SOLID); // Was 1
	
	if (tr.fraction < 1)
	{
		vec3_t velocity;
		CL_CalcPartVelocity(p, 1.0f, *time, velocity);
		CL_ClipParticleVelocity(velocity, tr.plane.normal, p->vel);

		VectorCopy(vec3_origin, p->accel);
		VectorCopy(tr.endpos, p->org);
		VectorCopy(p->org, org);
		VectorCopy(p->org, p->oldorg);

		p->alpha = *alpha;
		p->size = *size;

		p->time = cl.time;

		if (p->flags & PART_GRAVITY && VectorLength(p->vel) < 2)
			p->flags &= ~PART_GRAVITY;
	}
}

void CL_ParticleRotateThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	angle[2] = angle[0] + *time * angle[1] + *time * *time * angle[2];
}

void CL_DecalAlphaThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time)
{
	*alpha = powf(*alpha, 0.1f);
}

#pragma endregion

void CL_AddParticles()
{
	float alpha;
	vec3_t org, color, angle;

	cparticle_t *active = NULL;
	cparticle_t *tail = NULL;
	int decals = 0;
	float time = 0;

	cparticle_t *next;
	for (cparticle_t *p = active_particles; p; p = next)
	{
		next = p->next;
		const int flags = p->flags;

		// PMM - added INSTANT_PARTICLE handling for heat beam
		if (p->alphavel != INSTANT_PARTICLE)
		{
			time = (cl.time - p->time) * 0.001f;
			alpha = p->alpha + time * p->alphavel;
			if (flags & PART_DECAL)
			{
				if (decals >= r_decals->integer || alpha <= 0)
				{
					// Faded out
					p->alpha = 0;
					p->flags = 0;
					CL_ClearDecalPoly(p->decal); // Flag decal chain for cleaning
					p->decalnum = 0;
					p->decal = NULL;
					p->next = free_particles;
					free_particles = p;

					continue;
				}
			}
			else if (alpha <= 0)
			{
				// Faded out
				p->alpha = 0;
				p->flags = 0;
				p->next = free_particles;
				free_particles = p;

				continue;
			}
		}
		else
		{
			alpha = p->alpha;
		}

		p->next = NULL;
		if (!tail)
		{
			active = tail = p;
		}
		else
		{
			tail->next = p;
			tail = p;
		}

		alpha = clamp(alpha, 0.0f, 1.0f); //mxd

		const float timesq = time * time;
		
		for (int i = 0; i < 3; i++)
		{
			color[i] = p->color[i] + p->colorvel[i] * time;
			color[i] = clamp(color[i], 0, 255); //mxd
			
			angle[i] = p->angle[i];
			org[i] = p->org[i] + p->vel[i] * time + p->accel[i] * timesq;
		}

		if (p->flags & PART_GRAVITY)
			org[2] += timesq * -PARTICLE_GRAVITY;

		float size = p->size + p->sizevel * time;

		for (int i = 0; i < MAX_PARTICLE_LIGHTS; i++)
		{
			const cplight_t *plight = &p->lights[i];
			if (plight->isactive)
			{
				const float light = plight->light * alpha + plight->lightvel * time;
				V_AddLight(org, light, plight->lightcol[0], plight->lightcol[1], plight->lightcol[2]);
			}
		}

		particle_type type = p->type;

		if (p->think)
			p->think(p, org, angle, &alpha, &size, &type, &time);

		if (flags & PART_DECAL)
		{
			if (p->decalnum > 0 && p->decal)
			{
				for (decalpolys_t *d = p->decal; d; d = d->nextpoly)
					V_AddDecal(org, angle, color, alpha, p->blendfunc_src, p->blendfunc_dst, size, type, flags, d);
			}
			else
			{
				V_AddDecal(org, angle, color, alpha, p->blendfunc_src, p->blendfunc_dst, size, type, flags, NULL);
			}

			decals++;
		}
		else
		{
			V_AddParticle(org, angle, color, alpha, p->blendfunc_src, p->blendfunc_dst, size, type, flags);
		}
		
		if (p->alphavel == INSTANT_PARTICLE)
		{
			p->alphavel = 0.0f;
			p->alpha = 0.0f;
		}

		VectorCopy(org, p->oldorg);
	}

	active_particles = active;

	CL_CleanDecalPolys(); // Clean up active_decals linked list
}

void CL_ClearEffects()
{
	CL_ClearParticles();
	CL_ClearAllDecalPolys();
	CL_ClearDlights();
	CL_ClearLightStyles();
}

// Removes decal fragment pointers and resets decal fragment data. Called during vid_restart
qboolean CL_UnclipDecals()
{
	if (!active_decals) //mxd
		return false;
	
	for (cparticle_t *p = active_particles; p; p = p->next)
	{
		p->decalnum = 0;
		p->decal = NULL;
	}

	CL_ClearAllDecalPolys();

	return true; //mxd
}

// Re-clips all decals. Called during vid_restart
void CL_ReclipDecals()
{
	if (!active_decals) //mxd
		return;
	
	for (cparticle_t *p = active_particles; p; p = p->next)
	{
		p->decalnum = 0;
		p->decal = NULL;

		if (p->flags & PART_DECAL)
		{
			vec3_t dir;
			AngleVectors(p->angle, dir, NULL, NULL);
			VectorNegate(dir, dir);
			CL_ClipDecal(p, p->size, -p->angle[2], p->org, dir);

			if (!p->decalnum) // Kill on viewframe
				p->alpha = 0;
		}
	}
}