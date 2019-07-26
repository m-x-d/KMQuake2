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

// cl_effects_classic.c -- Vanilla particle effects (mxd)

#include "client.h"
#include "particles.h"

//mxd. Vanilla version
void CL_ExplosionParticles(const vec3_t org)
{
	for (int i = 0; i < 256; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() % 32) - 16);
			p->vel[j] = (rand() % 384) - 192;
		}

		color8_to_vec3(BLASTER_PARTICLE_COLOR + (rand() & 7), p->color);
		p->alphavel = -0.8f / (0.5f + frand() * 0.3f);
		p->type = particle_classic;
		p->flags = PART_GRAVITY;

		CL_FinishParticleInit(p);
	}
}

// Wall impact puffs
void CL_ParticleEffect(const vec3_t org, const vec3_t dir, const int color8, const int count)
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
		color8_to_vec3(color8 + (rand() & 7), p->color);
		p->alphavel = -1.0f / (0.5f + frand() * 0.3f);
		p->type = particle_generic;
		p->flags = PART_GRAVITY;

		CL_FinishParticleInit(p);
	}
}

void CL_ParticleEffect2(const vec3_t org, const vec3_t dir, const int color8, const int count, const qboolean invertgravity) //mxd. +invertgravity
{
	const float acceleration = (invertgravity ? PARTICLE_GRAVITY : -PARTICLE_GRAVITY); //mxd

	for (int i = 0; i < count; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		color8_to_vec3(color8, p->color);

		const float d = rand() & 7;
		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() & 7) - 4) + d * dir[j];
			p->vel[j] = crand() * 20;

			if (r_particle_mode->integer != 0)
				p->color[j] += 25;
		}

		p->accel[2] = acceleration;
		p->alphavel = -1.0f / (0.5f + frand() * 0.3f);
		p->type = particle_generic;
		p->flags = PART_GRAVITY;

		CL_FinishParticleInit(p);
	}
}

void CL_ClassicBlasterParticles(const vec3_t org, const vec3_t dir)
{
	for (int i = 0; i < 40; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		const float d = rand() & 15;
		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() & 7) - 4) + d * dir[j];
			p->vel[j] = dir[j] * 30 + crand() * 40;
		}

		p->accel[2] = -PARTICLE_GRAVITY;
		color8_to_vec3(BLASTER_PARTICLE_COLOR + (rand() & 7), p->color);
		p->alphavel = -1.0f / (0.5f + frand() * 0.3f);
		p->type = particle_classic;
		p->flags = PART_GRAVITY;

		CL_FinishParticleInit(p);
	}
}

void CL_ClassicBlasterTrail(const vec3_t start, const vec3_t end)
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
			p->org[j] = move[j] + crand();
			p->vel[j] = crand() * 5;
		}

		color8_to_vec3(BLASTER_PARTICLE_COLOR, p->color);
		p->alphavel = -1.0f / (0.3f + frand() * 0.2f);
		p->type = particle_classic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

void CL_ClassicBubbleTrail(const vec3_t start, const vec3_t end)
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
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand() * 2;
			p->vel[j] = crand() * 5;
		}

		p->vel[2] += 6;

		color8_to_vec3(4 + (rand() & 7), p->color);
		p->alphavel = -1.0f / (1 + frand() * 0.2f);
		p->type = particle_classic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

void CL_ClassicBubbleTrail2(const vec3_t start, const vec3_t end, const int dist)
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
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand() * 2;
			p->vel[j] = crand() * 10;
		}

		p->org[2] -= 4;
		p->vel[2] += 20;

		color8_to_vec3(4 + (rand() & 7), p->color);
		p->alphavel = -1.0f / (1 + frand() * 0.1f);
		p->type = particle_classic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

void CL_ClassicDiminishingTrail(const vec3_t start, const vec3_t end, centity_t *old, const int flags)
{
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	const float dec = 0.5f;
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

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;

		// Drop less particles as it flies
		if ((rand() & 1023) < old->trailcount)
		{
			cparticle_t *p = CL_InitParticle();
			if (!p)
				return;

			for (int i = 0; i < 3; i++)
			{
				p->org[i] = move[i] + crand() * orgscale;
				p->vel[i] = crand() * velscale;
			}

			if (flags & EF_GIB)
			{
				color8_to_vec3(0xe8 + (rand() & 7), p->color);
				p->alphavel = -1.0f / (1 + frand() * 0.4f);
				p->vel[2] -= PARTICLE_GRAVITY;
			}
			else if (flags & EF_GREENGIB)
			{
				color8_to_vec3(0xdb + (rand() & 7), p->color);
				p->alphavel = -1.0f / (1 + frand() * 0.4f);
				p->vel[2] -= PARTICLE_GRAVITY;
			}
			else
			{
				color8_to_vec3(4 + (rand() & 7), p->color);
				p->alphavel = -1.0f / (1 + frand() * 0.2f);
				p->accel[2] = 20;
			}

			p->flags = flags;

			CL_FinishParticleInit(p);
		}

		old->trailcount = max(100, old->trailcount - 5);
		VectorAdd(move, vec, move);
	}
}

void CL_ClassicIonripperTrail(const vec3_t start, const vec3_t ent)
{
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(ent, start, vec);
	const float len = VectorNormalize(vec);

	const int dec = 5;
	VectorScale(vec, dec, vec);

	int dir = 1;

	for (int i = 0; i < len; i += dec)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		VectorCopy(move, p->org);
		p->vel[0] = 10 * dir;
		color8_to_vec3(0xe4 + (rand() & 3), p->color);
		p->alpha = 0.5f;
		p->alphavel = -1.0f / (0.3f + frand() * 0.2f);
		p->type = particle_classic;

		CL_FinishParticleInit(p);

		dir *= -1;

		VectorAdd(move, vec, move);
	}
}

void CL_ClassicNukeblast(const cl_sustain_t *self)
{
	static int colortable[] = { 110, 112, 114, 116 };
	const float ratio = 1.0f - ((self->endtime - cl.time) / 1000.0f);

	for (int i = 0; i < (700 / cl_particle_scale->value); i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		color8_to_vec3(colortable[rand() & 3], p->color);
		p->alphavel = INSTANT_PARTICLE;
		p->type = particle_classic;
		p->flags = PART_INSTANT;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);

		VectorMA(self->org, 200.0f * ratio, dir, p->org);

		CL_FinishParticleInit(p);
	}
}

void CL_ClassicParticleSmokeEffect(const vec3_t org, const vec3_t dir, const int color8, const int count, const int magnitude)
{
	vec3_t r, u;
	MakeNormalVectors(dir, r, u);

	for (int i = 0; i < count; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		color8_to_vec3(color8 + (rand() & 7), p->color);

		for (int j = 0; j < 3; j++)
			p->org[j] = org[j] + magnitude * 0.1f * crand();

		VectorScale(dir, magnitude, p->vel);

		float d = crand() * magnitude / 3;
		VectorMA(p->vel, d, r, p->vel);

		d = crand() * magnitude / 3;
		VectorMA(p->vel, d, u, p->vel);

		p->alphavel = -1.0f / (0.5f + frand() * 0.3f);
		p->type = particle_classic;

		CL_FinishParticleInit(p);
	}
}

void CL_ClassicRailTrail(const vec3_t start, const vec3_t end, const qboolean isred)
{
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	vec3_t right, up;
	MakeNormalVectors(vec, right, up);

	const int color8 = (isred ? 0x44 : 0x74);

	for (int i = 0; i < len; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		const float d = i * 0.1f;
		const float c = cosf(d);
		const float s = sinf(d);

		vec3_t dir;
		VectorScale(right, c, dir);
		VectorMA(dir, s, up, dir);

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + dir[j] * 3;
			p->vel[j] = dir[j] * 6;
		}

		p->alphavel = -1.0f / (1 + frand() * 0.2f);
		color8_to_vec3(color8 + (rand() & 7), p->color);
		p->type = particle_classic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}

	const float dec = 0.75f;
	VectorScale(vec, dec, vec);
	VectorCopy(start, move);

	for (float i = 0; i < len; i += dec)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand() * 3;
			p->vel[j] = crand() * 3;
		}

		p->alphavel = -1.0f / (0.6f + frand() * 0.2f);
		color8_to_vec3(rand() & 7, p->color);
		p->type = particle_classic;

		CL_FinishParticleInit(p);

		VectorAdd(move, vec, move);
	}
}

void CL_ClassicRocketTrail(const vec3_t start, const vec3_t end, centity_t *old)
{
	// Smoke
	CL_ClassicDiminishingTrail(start, end, old, EF_ROCKET);

	// Fire
	vec3_t move, vec;
	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const float len = VectorNormalize(vec);

	for (int i = 0; i < len; i++)
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

			p->accel[2] = -PARTICLE_GRAVITY;
			color8_to_vec3(0xdc + (rand() & 3), p->color);
			p->alphavel = -1.0f / (1 + frand() * 0.2f);
			p->type = particle_classic;
			p->flags = PART_GRAVITY;

			CL_FinishParticleInit(p);
		}

		VectorAdd(move, vec, move);
	}
}

void CL_ClassicWidowSplash(const vec3_t org)
{
	static int colortable[] = { 2 * 8, 13 * 8, 21 * 8, 18 * 8 };

	for (int i = 0; i < 256; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		color8_to_vec3(colortable[rand() & 3], p->color);

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
		VectorMA(org, 45.0f, dir, p->org);
		VectorMA(vec3_origin, 40.0f, dir, p->vel);

		p->alphavel = -0.8f / (0.5f + frand() * 0.3f);

		CL_FinishParticleInit(p);
	}
}

void CL_ClassicWidowbeamout(const cl_sustain_t *self)
{
	static int colortable[] = { 2 * 8, 13 * 8, 21 * 8, 18 * 8 };
	const float ratio = 1.0f - ((self->endtime - cl.time) / 2100.0f);

	for (int i = 0; i < 300; i++)
	{
		cparticle_t *p = CL_InitParticle();
		if (!p)
			return;

		color8_to_vec3(colortable[rand() & 3], p->color);
		p->alphavel = INSTANT_PARTICLE;
		p->type = particle_classic;
		p->flags = PART_INSTANT;

		vec3_t dir;
		VectorSet(dir, crand(), crand(), crand());
		VectorNormalize(dir);
		VectorMA(self->org, 45.0f * ratio, dir, p->org);

		CL_FinishParticleInit(p);
	}
}