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

/*
===============
CL_SetParticleImages
This initializes all particle images - mods play with this...
===============
*/
void CL_SetParticleImages (void)
{
	R_SetParticlePicture(particle_solid,		"***whitetexture***");				// only used for sparks
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
	R_SetParticlePicture(particle_beam2,		"gfx/particles/beam2.tga"); // only used for railgun
	R_SetParticlePicture(particle_lightning,	"gfx/particles/lightning.tga");
	R_SetParticlePicture(particle_inferno,		"gfx/particles/inferno.tga");
//	R_SetParticlePicture(particle_lensflare,	"gfx/particles/lensflare.tga");
//	R_SetParticlePicture(particle_lightflare,	"gfx/particles/lightflare.jpg");
//	R_SetParticlePicture(particle_shield,		"gfx/particles/shield.jpg");
	//animations
	//rocket explosion
	R_SetParticlePicture(particle_rflash,		"gfx/particles/r_flash.tga");
	R_SetParticlePicture(particle_rexplosion1,	"gfx/particles/r_explod_1.tga");
	R_SetParticlePicture(particle_rexplosion2,	"gfx/particles/r_explod_2.tga");
	R_SetParticlePicture(particle_rexplosion3,	"gfx/particles/r_explod_3.tga");
	R_SetParticlePicture(particle_rexplosion4,	"gfx/particles/r_explod_4.tga");
	R_SetParticlePicture(particle_rexplosion5,	"gfx/particles/r_explod_5.tga");
	R_SetParticlePicture(particle_rexplosion6,	"gfx/particles/r_explod_6.tga");
	R_SetParticlePicture(particle_rexplosion7,	"gfx/particles/r_explod_7.tga");
	//disruptor explosion		
//	R_SetParticlePicture(particle_dexplosion1,	"gfx/particles/d_explod_1.tga");
//	R_SetParticlePicture(particle_dexplosion2,	"gfx/particles/d_explod_2.tga");
//	R_SetParticlePicture(particle_dexplosion3,	"gfx/particles/d_explod_3.tga");

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
//	R_SetParticlePicture(particle_footprint,	"gfx/decals/footprint.tga");

	//mxd. Classic particles...
	R_SetParticlePicture(particle_classic, "***particle***");
}


/*
===============
CL_GetRandomBloodParticle
===============
*/
int CL_GetRandomBloodParticle (void)
{
	return particle_blooddecal1 + rand()%5;
}


/*
==============================================================

PARTICLE MANAGEMENT

==============================================================
*/

cparticle_t	*active_particles, *free_particles;
cparticle_t	particles[MAX_PARTICLES];
int			cl_numparticles = MAX_PARTICLES;

decalpolys_t	*active_decals, *free_decals;
decalpolys_t	decalfrags[MAX_DECAL_FRAGS];
int				cl_numdecalfrags = MAX_DECAL_FRAGS;


/*
===============
CL_CleanDecalPolys
Cleans up the active_particles linked list
===============
*/
void CL_CleanDecalPolys (void)
{
	decalpolys_t *next;
	decalpolys_t *active = NULL, *tail = NULL;

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
			active = tail = d;
		else
		{
			tail->next = d;
			tail = d;
		}
	}

	active_decals = active;
}


/*
===============
CL_ClearDecalPoly
Recursively flags a decal poly chain for cleaning
===============
*/
void CL_ClearDecalPoly (decalpolys_t *decal)
{
	if (!decal)
		return;
	if (decal->nextpoly)
		CL_ClearDecalPoly(decal->nextpoly);
	decal->clearflag = true; // tell cleaning loop to clean this up
}


/*
===============
CL_ClearAllDecalPolys
Clears all decal polys
===============
*/
void CL_ClearAllDecalPolys (void)
{
	free_decals = &decalfrags[0];
	active_decals = NULL;

	for (int i = 0 ;i < cl_numdecalfrags ; i++)
	{
		decalfrags[i].next = &decalfrags[i+1];
		decalfrags[i].clearflag = false;
		decalfrags[i].numpolys = 0;
		decalfrags[i].nextpoly = NULL;
		decalfrags[i].node = NULL; // vis node
	}
	decalfrags[cl_numdecalfrags - 1].next = NULL;
}


/*
===============
CL_NumFreeDecalPolys
Retuns number of available decalpoly_t fields
===============
*/
int CL_NumFreeDecalPolys (void)
{
	int count = 0;
	for (decalpolys_t *d = free_decals; d; d = d->next)
		count++;
	return count;
}


/*
===============
CL_NewDecalPoly
Retuns first free decal poly
===============
*/
decalpolys_t *CL_NewDecalPoly(void)
{
	if (!free_decals)
		return NULL;

	decalpolys_t *d = free_decals;
	free_decals = d->next;
	d->next = active_decals;
	active_decals = d;
	return d;
}


/*
===============
CL_ClipDecal
===============
*/
void CL_ClipDecal (cparticle_t *part, float radius, float orient, vec3_t origin, vec3_t dir)
{
	vec3_t	axis[3], verts[MAX_DECAL_VERTS];
	int i;
	markFragment_t *fr, fragments[MAX_FRAGMENTS_PER_DECAL];
	
	// invalid decal
	if ( radius <= 0 || VectorCompare (dir, vec3_origin) ) 
		return;

	// calculate orientation matrix
	VectorNormalize2 ( dir, axis[0] );
	PerpendicularVector ( axis[1], axis[0] );
	RotatePointAroundVector ( axis[2], axis[0], axis[1], orient );
	CrossProduct ( axis[0], axis[2], axis[1] );

	const int numfragments = R_MarkFragments (origin, axis, radius, MAX_DECAL_VERTS, verts, MAX_FRAGMENTS_PER_DECAL, fragments);
	
	if (numfragments == 0 || numfragments > CL_NumFreeDecalPolys()) // nothing to display / not enough decalpolys free
		return;
	
	VectorScale ( axis[1], 0.5f / radius, axis[1] );
	VectorScale ( axis[2], 0.5f / radius, axis[2] );

	part->decalnum = numfragments;
	for ( i = 0, fr = fragments; i < numfragments; i++, fr++ )
	{
		decalpolys_t *decal = CL_NewDecalPoly();
		vec3_t v;

		if (!decal)
			return;

		decal->nextpoly = part->decal;
		part->decal = decal;
		//Com_Printf("Number of verts in fragment: %i\n", fr->numPoints);
		decal->node = fr->node; // vis node

		for (int j = 0; j < fr->numPoints && j < MAX_VERTS_PER_FRAGMENT; j++ )
		{
			VectorCopy ( verts[fr->firstPoint+j], decal->polys[j] );
			VectorSubtract ( decal->polys[j], origin, v );
			decal->coords[j][0] = DotProduct ( v, axis[1] ) + 0.5f;
			decal->coords[j][1] = DotProduct ( v, axis[2] ) + 0.5f;
			decal->numpolys = fr->numPoints;
		}
	}
}


/*
===============
CL_NewParticleTime
===============
*/
float CL_NewParticleTime (void)
{
	float lerpedTime = cl.time;
	return lerpedTime;
}


/*
===============
CL_SetupParticle
===============
*/
cparticle_t *CL_SetupParticle (
			float angle0,		float angle1,		float angle2,
			float org0,			float org1,			float org2,
			float vel0,			float vel1,			float vel2,
			float accel0,		float accel1,		float accel2,
			float color0,		float color1,		float color2,
			float colorvel0,	float colorvel1,	float colorvel2,
			float alpha,		float alphavel,
			int	blendfunc_src,	int blendfunc_dst,
			float size,			float sizevel,			
			int	image,
			int flags,
			void (*think)(cparticle_t *p, vec3_t p_org, vec3_t p_angle, float *p_alpha, float *p_size, int *p_image, float *p_time),
			qboolean thinknext)
{
	if (!free_particles)
		return NULL;

	cparticle_t *p = free_particles;
	free_particles = p->next;
	p->next = active_particles;
	active_particles = p;

	p->time = cl.time;

	//mxd. Gross hacks
	if (image == particle_generic && r_particle_mode->integer == 0)
	{
		alpha = 1;
		image = particle_classic;
		size = 1;
		sizevel = 0;
		colorvel0 = 0;
		colorvel1 = 0;
		colorvel2 = 0;
		blendfunc_src = GL_SRC_ALPHA;
		blendfunc_dst = GL_ONE;
	}

	VectorSet(p->angle, angle0, angle1, angle2);

	VectorSet(p->org, org0, org1, org2);
	VectorSet(p->oldorg, org0, org1, org2);

	VectorSet(p->vel, vel0, vel1, vel2);
	VectorSet(p->accel, accel0, accel1, accel2);

	VectorSet(p->color, color0, color1, color2);
	VectorSet(p->colorvel, colorvel0, colorvel1, colorvel2);

	p->blendfunc_src = blendfunc_src;
	p->blendfunc_dst = blendfunc_dst;

	p->alpha = alpha;
	p->alphavel = alphavel;
	p->size = size;
	p->sizevel = sizevel;

	p->image = image;
	p->flags = flags;

	p->src_ent = 0;
	p->dst_ent = 0;

	p->think = think;
	p->thinknext = thinknext;

	for (int j = 0; j < P_LIGHTS_MAX; j++)
	{
		cplight_t *plight = &p->lights[j];
		plight->isactive = false;
		plight->light = 0;
		plight->lightvel = 0;
		VectorClear(plight->lightcol); //mxd
	}

	p->decalnum = 0;
	p->decal = NULL;

	if (flags & PART_DECAL)
	{
		vec3_t dir;
		AngleVectors (p->angle, dir, NULL, NULL);
		VectorNegate(dir, dir);
		CL_ClipDecal(p, p->size, -p->angle[2], p->org, dir);

		if (!p->decalnum) // kill on viewframe
			p->alpha = 0;
	}

	return p;
}

/*
===============
CL_InitParticle (mxd)
===============
*/
cparticle_t *CL_InitParticle2(int flags)
{
	if (!free_particles)
		return NULL;

	cparticle_t *p = free_particles;
	free_particles = p->next;
	p->next = active_particles;
	active_particles = p;

	p->time = cl.time;

	VectorClear(p->angle);
	VectorClear(p->org);
	VectorClear(p->oldorg);
	VectorClear(p->vel);
	VectorClear(p->accel);
	VectorSet(p->color, 255, 255, 255);
	VectorClear(p->colorvel);

	p->blendfunc_src = GL_SRC_ALPHA;
	p->blendfunc_dst = GL_ONE;

	p->alpha = 1;
	p->alphavel = 0;
	p->size = 1;
	p->sizevel = 0;

	p->image = (r_particle_mode->integer == 0 ? particle_classic : particle_generic);
	p->flags = flags;

	p->src_ent = 0;
	p->dst_ent = 0;

	p->think = NULL;
	p->thinknext = false;

	for (int i = 0; i < P_LIGHTS_MAX; i++)
	{
		cplight_t *plight = &p->lights[i];
		plight->isactive = false;
		plight->light = 0;
		plight->lightvel = 0;
		VectorClear(plight->lightcol);
	}

	p->decalnum = 0;
	p->decal = NULL;

	if (flags & PART_DECAL)
	{
		vec3_t dir;
		AngleVectors(p->angle, dir, NULL, NULL);
		VectorNegate(dir, dir);
		CL_ClipDecal(p, p->size, -p->angle[2], p->org, dir);

		if (!p->decalnum) // kill on viewframe
			p->alpha = 0;
	}

	return p;
}

cparticle_t *CL_InitParticle()
{
	cparticle_t *p = CL_InitParticle2(0);
	return p;
}


/*
===============
CL_AddParticleLight
===============
*/
void CL_AddParticleLight (cparticle_t *p, float light, float lightvel, float lcol0, float lcol1, float lcol2)
{
	for (int i = 0; i < P_LIGHTS_MAX; i++)
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


/*
===============
CL_ClearParticles
===============
*/
void CL_ClearParticles (void)
{
	free_particles = &particles[0];
	active_particles = NULL;

	for (int i = 0; i < cl_numparticles; i++)
	{
		particles[i].next = &particles[i+1];
		particles[i].decalnum = 0; // Knightmare added
		particles[i].decal = NULL; // Knightmare added
	}

	particles[cl_numparticles-1].next = NULL;
}


/*
======================================

GENERIC PARTICLE THINKING ROUTINES

======================================
*/
//#define SplashSize		10
#define	STOP_EPSILON	0.1

/*
===============
CL_CalcPartVelocity
===============
*/
void CL_CalcPartVelocity (cparticle_t *p, float scale, float *time, vec3_t velocity)
{
	const float time1 = *time;
	const float time2 = time1 * time1;
	const int gravity = (p->flags & PART_GRAVITY ? PARTICLE_GRAVITY : 0); //mxd

	for (int i = 0; i < 2; i++)
		velocity[i] = scale * (p->vel[i] * time1 + p->accel[i] * time2);
	velocity[2] = scale * (p->vel[2] * time1 + (p->accel[2] - gravity) * time2);
}

/*
===============
CL_ClipParticleVelocity
===============
*/
void CL_ClipParticleVelocity (vec3_t in, vec3_t normal, vec3_t out)
{
	const float backoff = VectorLength(in) * 0.25 + DotProduct(in, normal) * 3.0f;

	for (int i = 0; i < 3; i++)
	{
		const float change = normal[i] * backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}
}

/*
===============
CL_ParticleBounceThink
===============
*/
void CL_ParticleBounceThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	const float clipsize = max(*size * 0.5, 0.25);
	trace_t tr = CL_BrushTrace (p->oldorg, org, clipsize, MASK_SOLID); // was 1
	
	if (tr.fraction < 1)
	{
		vec3_t velocity;
		CL_CalcPartVelocity(p, 1, time, velocity);
		CL_ClipParticleVelocity(velocity, tr.plane.normal, p->vel);

		VectorCopy(vec3_origin, p->accel);
		VectorCopy(tr.endpos, p->org);
		VectorCopy(p->org, org);
		VectorCopy(p->org, p->oldorg);

		p->alpha = *alpha;
		p->size = *size;

		p->start = p->time = CL_NewParticleTime();

		if (p->flags & PART_GRAVITY && VectorLength(p->vel) < 2)
			p->flags &= ~PART_GRAVITY;
	}

	p->thinknext = true;
}

/*
===============
CL_ParticleRotateThink
===============
*/
void CL_ParticleRotateThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	angle[2] = angle[0] + *time * angle[1] + *time * *time * angle[2];
	p->thinknext=true;
}

/*
===============
CL_DecalAlphaThink
===============
*/
void CL_DecalAlphaThink (cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time)
{
	*alpha = pow(*alpha, 0.1);
	p->thinknext = true;
}


//===================================================================

/*
===============
CL_AddParticles
===============
*/
void CL_AddParticles (void)
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
			time = (cl.time - p->time) * 0.001;
			alpha = p->alpha + time * p->alphavel;
			if (flags & PART_DECAL)
			{
				if (decals >= r_decals->value || alpha <= 0)
				{
					// faded out
					p->alpha = 0;
					p->flags = 0;
					CL_ClearDecalPoly (p->decal); // flag decal chain for cleaning
					p->decalnum = 0;
					p->decal = NULL;
					p->next = free_particles;
					free_particles = p;

					continue;
				}
			}
			else if (alpha <= 0)
			{
				// faded out
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
			active = tail = p;
		else
		{
			tail->next = p;
			tail = p;
		}

		alpha = clamp(alpha, 0.0, 1.0); //mxd

		const float time2 = time * time;
		int image = p->image;

		for (int i = 0; i < 3; i++)
		{
			color[i] = p->color[i] + p->colorvel[i] * time;
			color[i] = clamp(color[i], 0, 255); //mxd
			
			angle[i] = p->angle[i];
			org[i] = p->org[i] + p->vel[i] * time + p->accel[i] * time2;
		}

		if (p->flags & PART_GRAVITY)
			org[2] += time2 * -PARTICLE_GRAVITY;

		float size = p->size + p->sizevel * time;

		for (int i = 0; i < P_LIGHTS_MAX; i++)
		{
			const cplight_t *plight = &p->lights[i];
			if (plight->isactive)
			{
				const float light = plight->light * alpha + plight->lightvel * time;
				V_AddLight(org, light, plight->lightcol[0], plight->lightcol[1], plight->lightcol[2]);
			}
		}

		if (p->thinknext && p->think)
		{
			p->thinknext=false;
			p->think(p, org, angle, &alpha, &size, &image, &time);
		}

		if (flags & PART_DECAL)
		{
			if (p->decalnum > 0 && p->decal)
			{
				for (decalpolys_t *d = p->decal; d; d = d->nextpoly)
					V_AddDecal(org, angle, color, alpha, p->blendfunc_src, p->blendfunc_dst, size, image, flags, d);
			}
			else
			{
				V_AddDecal(org, angle, color, alpha, p->blendfunc_src, p->blendfunc_dst, size, image, flags, NULL);
			}

			decals++;
		}
		else
		{
			V_AddParticle(org, angle, color, alpha, p->blendfunc_src, p->blendfunc_dst, size, image, flags);
		}
		
		if (p->alphavel == INSTANT_PARTICLE)
		{
			p->alphavel = 0.0;
			p->alpha = 0.0;
		}

		VectorCopy(org, p->oldorg);
	}

	active_particles = active;

	CL_CleanDecalPolys(); // clean up active_decals linked list
}


/*
==============
CL_ClearEffects
==============
*/
void CL_ClearEffects (void)
{
	CL_ClearParticles ();
	CL_ClearAllDecalPolys ();
	CL_ClearDlights ();
	CL_ClearLightStyles ();
}


/*
==============
CL_UnclipDecals
Removes decal fragment pointers and resets decal fragment data
Called during a vid_restart
==============
*/
void CL_UnclipDecals (void)
{
	//Com_Printf ("Unclipping decals\n");
	for (cparticle_t *p = active_particles; p; p=p->next)
	{
		p->decalnum = 0;
		p->decal = NULL;
	}

	CL_ClearAllDecalPolys();
}


/*
==============
CL_ReclipDecals
Re-clips all decals
Called during a vid_restart
==============
*/
void CL_ReclipDecals (void)
{
	//Com_Printf ("Reclipping decals\n");
	for (cparticle_t *p = active_particles; p; p = p->next)
	{
		p->decalnum = 0;
		p->decal = NULL;
		if (p->flags & PART_DECAL)
		{
			vec3_t dir;
			AngleVectors (p->angle, dir, NULL, NULL);
			VectorNegate(dir, dir);
			CL_ClipDecal(p, p->size, -p->angle[2], p->org, dir);

			if (!p->decalnum) // kill on viewframe
				p->alpha = 0;
		}
	}
}
