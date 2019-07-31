/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2000-2002 Mr. Hyde and Mad Dog

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

// g_phys.c

// pushmove objects do not obey gravity, and do not interact with each other or trigger fields, but block normal movement and push normal objects when they move.
// onground is set for toss objects when they come to a complete rest. It is set for steping or walking objects.

// Doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
// Bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
// Corpses are SOLID_NOT and MOVETYPE_TOSS
// Crates are SOLID_BBOX and MOVETYPE_TOSS
// Walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
// Flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

// solid_edge items only clip against bsp models.

#include "g_local.h"

static qboolean wasonground;
static qboolean onconveyor;
static edict_t *blocker;

// Identical to player's P_FallingDamage... except of course ent doesn't have to be a player.
static void Other_FallingDamage(edict_t *ent)
{
	if (!ent || ent->movetype == MOVETYPE_NOCLIP)
		return;

	float delta;
	if (ent->oldvelocity[2] < 0 && ent->velocity[2] > ent->oldvelocity[2] && !ent->groundentity)
		delta = ent->oldvelocity[2];
	else if (ent->groundentity)
		delta = ent->velocity[2] - ent->oldvelocity[2];
	else
		return;

	delta = delta * delta * 0.0001f;

	switch (ent->waterlevel)
	{
		case 1: delta *= 0.5f; break;
		case 2: delta *= 0.25f; break;
		case 3: return; // Never take falling damage if completely underwater
	}

	if (delta < 1)
		return;

	if (delta < 15)
	{
		ent->s.event = EV_FOOTSTEP;
		return;
	}

	if (delta > 30)
	{
		ent->pain_debounce_time = level.time; // No normal pain sound
		int damage = (delta - 30) / 2;
		damage = max(1, damage);

		if (!deathmatch->integer || !(dmflags->integer & DF_NO_FALLING))
		{
			vec3_t dir = { 0, 0, 1 };
			T_Damage(ent, world, world, dir, ent->s.origin, vec3_origin, damage, 0, 0, MOD_FALLING);
		}
	}
}

static edict_t *SV_TestEntityPosition(edict_t *ent)
{
	const int mask = (ent->clipmask ? ent->clipmask : MASK_SOLID);

	trace_t trace;
	if (ent->solid == SOLID_BSP)
	{
		vec3_t org, mins, maxs;
		VectorAdd(ent->s.origin, ent->origin_offset, org);
		VectorSubtract(ent->mins, ent->origin_offset, mins);
		VectorSubtract(ent->maxs, ent->origin_offset, maxs);

		trace = gi.trace(org, mins, maxs, org, ent, mask);
	}
	else
	{
		trace = gi.trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin, ent, mask);
	}

	if (trace.startsolid)
	{
		// Lazarus - work around for players/monsters standing on dead monsters causing those monsters to gib when rotating brush models are in the vicinity
		if ((ent->svflags & SVF_DEADMONSTER) && (trace.ent->client || (trace.ent->svflags & SVF_MONSTER)))
			return NULL;

		// Lazarus - return a bit more useful info than simply "g_edicts"
		if (trace.ent)
			return trace.ent;

		return world;
	}
	
	return NULL;
}

static void SV_CheckVelocity(edict_t *ent)
{
	// Bound velocity
	if (ent && VectorLength(ent->velocity) > sv_maxvelocity->value)
	{
		VectorNormalizeFast(ent->velocity);
		VectorScale(ent->velocity, sv_maxvelocity->value, ent->velocity);
	}
}

// Runs thinking code for this frame if necessary. Returns true when ent->think was NOT called.
static qboolean SV_RunThink(edict_t *ent)
{
	if (ent->nextthink <= 0 || ent->nextthink > level.time + 0.001f)
		return true;

	ent->nextthink = 0;

	if (!ent->think)
		gi.error("NULL ent->think for %s", ent->classname);

	ent->think(ent);

	return false;
}

// Two entities have touched, so run their touch functions
static void SV_Impact(edict_t *e1, trace_t *trace)
{
	edict_t *e2 = trace->ent;

	if (e1->touch && e1->solid != SOLID_NOT)
		e1->touch(e1, e2, &trace->plane, trace->surface);

	if (e2->touch && e2->solid != SOLID_NOT)
		e2->touch(e2, e1, NULL, NULL);
}

#define STOP_EPSILON	0.1f

// Slide off of the impacting object. Returns the blocked flags (1 = floor, 2 = step / wall)
static int ClipVelocity(const vec3_t in, const vec3_t normal, vec3_t out, const float overbounce)
{
	int blocked = 0;
	if (normal[2] > 0)
		blocked |= 1; // Floor
	else if (normal[2] == 0)
		blocked |= 2; // Step

	const float backoff = DotProduct(in, normal) * overbounce;

	for (int i = 0; i < 3; i++)
	{
		out[i] = in[i] - normal[i] * backoff;

		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}

	return blocked;
}

//mxd. Reflect velocity vector off plane normal
static void ReflectVelocity(const vec3_t in, const vec3_t normal, vec3_t out, const float velocityscale)
{
	const float len = VectorLength(in);

	// Regular reflect most likely won't cut it. Try to bounce away from steep incline...
	if (len < 5 && normal[2] <= 0.7f)
	{
		vec3_t n;
		VectorCopy(normal, n);

		for (int i = 0; i < 3; i++)
			n[i] += crandom() * STOP_EPSILON;

		VectorScale(n, 5 + random() * 2.5f, out);

		return;
	}

	// https://stackoverflow.com/questions/35006037/reflect-vector-in-3d-space
	vec3_t vnormal = { -in[0], -in[1], -in[2] }; // Velocity vector 
	VectorNormalizeFast(vnormal);

	vec3_t snormal; // Scaled normal
	const float dot = DotProduct(vnormal, normal);
	VectorScale(normal, 2 * dot, snormal);

	vec3_t reflected;
	VectorSubtract(snormal, vnormal, reflected);
	VectorScale(reflected, len * velocityscale, out);

	for (int i = 0; i < 3; i++)
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
}

#define MAX_CLIP_PLANES	5

// The basic solid body movement clip that slides along multiple planes.
// Returns the clipflags if the velocity was modified (hit something solid).
// 1 = floor
// 2 = wall / step
// 4 = dead stop
static int SV_FlyMove(edict_t *ent, const float time, const int mask)
{
	int numbumps;
	int numplanes;
	vec3_t planes[MAX_CLIP_PLANES];
	vec3_t primal_velocity;
	vec3_t original_velocity;
	float time_left;
	int blocked;
	int num_retries = 0;

retry:
	numbumps = 4;
	
	blocked = 0;
	VectorCopy(ent->velocity, original_velocity);
	VectorCopy(ent->velocity, primal_velocity);
	numplanes = 0;
	
	time_left = time;

	ent->groundentity = NULL;
	for (int bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		vec3_t end;
		for (int i = 0; i < 3; i++)
			end[i] = ent->s.origin[i] + time_left * ent->velocity[i];

		trace_t trace = gi.trace(ent->s.origin, ent->mins, ent->maxs, end, ent, mask);

		if (trace.allsolid)
		{
			// Entity is trapped in another solid
			VectorClear(ent->velocity);
			return 3;
		}

		if (trace.fraction > 0)
		{
			// Actually covered some distance
			VectorCopy(trace.endpos, ent->s.origin);
			VectorCopy(ent->velocity, original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			 break; // Moved the entire distance

		blocker = trace.ent;
		edict_t *hit = trace.ent;

		// Lazarus: If the pushed entity is a conveyor, raise us up and try again
		if (!num_retries && wasonground)
		{
			if (hit->movetype == MOVETYPE_CONVEYOR && trace.plane.normal[2] > 0.7f)
			{
				vec3_t above;
				VectorCopy(end, above);
				above[2] += 32;

				trace = gi.trace(above, ent->mins, ent->maxs, end, ent, mask);
				VectorCopy(trace.endpos, end);
				end[2] += 1;

				VectorSubtract(end, ent->s.origin, ent->velocity);
				VectorScale(ent->velocity, 1.0f / time_left, ent->velocity);
				num_retries++;

				goto retry;
			}
		}

		// If blocked by player AND on a conveyor
		if (hit->client && onconveyor)
		{
			if (ent->mass > hit->mass)
			{
				vec3_t player_dest;
				VectorMA(hit->s.origin, time_left, ent->velocity, player_dest);
				const trace_t ptrace = gi.trace(hit->s.origin, hit->mins, hit->maxs, player_dest, hit, hit->clipmask);
				
				if (ptrace.fraction == 1.0f)
				{
					VectorCopy(player_dest, hit->s.origin);
					gi.linkentity(hit);

					goto retry;
				}
			}

			blocked |= 8;
		}

		if (trace.plane.normal[2] > 0.7f)
		{
			blocked |= 1; // Floor

			if (hit->solid == SOLID_BSP)
			{
				ent->groundentity = hit;
				ent->groundentity_linkcount = hit->linkcount;
			}
		}

		if (!trace.plane.normal[2])
			blocked |= 2; // Step

		// Run the impact function
		SV_Impact(ent, &trace);
		if (!ent->inuse)
			break; // Removed by the impact function

		time_left -= time_left * trace.fraction;
		
		// Cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{
			// This shouldn't really happen
			VectorClear(ent->velocity);
			blocked |= 3;

			return blocked;
		}

		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		// Modify original_velocity so it parallels all of the clip planes
		int planenum1;
		vec3_t new_velocity;
		for (planenum1 = 0; planenum1 < numplanes; planenum1++)
		{
			
			ClipVelocity(original_velocity, planes[planenum1], new_velocity, 1);

			int planenum2;
			for (planenum2 = 0; planenum2 < numplanes; planenum2++)
			{
				if (planenum2 != planenum1 && !VectorCompare(planes[planenum1], planes[planenum2]))
				{
					if (DotProduct(new_velocity, planes[planenum2]) < 0)
						break; // Not ok
				}
			}

			if (planenum2 == numplanes)
				break;
		}
		
		if (planenum1 != numplanes)
		{
			// Go along this plane
			VectorCopy(new_velocity, ent->velocity);
		}
		else
		{
			// Go along the crease
			if (numplanes != 2)
			{
				VectorClear(ent->velocity);
				blocked |= 7;

				return blocked;
			}

			vec3_t dir;
			CrossProduct(planes[0], planes[1], dir);
			const float d = DotProduct(dir, ent->velocity);
			VectorScale(dir, d, ent->velocity);
		}

		// If original velocity is against the original velocity, stop dead to avoid tiny occilations in sloping corners
		if (DotProduct(ent->velocity, primal_velocity) <= 0)
		{
			VectorClear(ent->velocity);
			return blocked;
		}
	}

	return blocked;
}

// The basic solid body movement clip that slides along multiple planes.
// Returns the clipflags if the velocity was modified (hit something solid).
// 1 = floor
// 2 = wall / step
// 4 = dead stop
static int SV_PushableMove(edict_t *ent, const float time, const int mask)
{
	int numbumps;
	int numplanes;
	vec3_t planes[MAX_CLIP_PLANES];
	vec3_t primal_velocity, original_velocity;
	float time_left;
	int blocked;
	int num_retries = 0;

	// Corrective stuff added for bmodels with no origin brush
	vec3_t mins, maxs;
	vec3_t origin;

retry:

	numbumps = 4;
	ent->bounce_me = 0;
	
	blocked = 0;
	VectorCopy (ent->velocity, original_velocity);
	VectorCopy (ent->velocity, primal_velocity);
	numplanes = 0;
	
	time_left = time;

	VectorAdd(ent->s.origin, ent->origin_offset, origin);
	VectorCopy(ent->size, maxs);
	VectorScale(maxs, 0.5f, maxs);
	VectorNegate(maxs, mins);

	ent->groundentity = NULL;

	for (int bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		vec3_t end;
		for (int i = 0; i < 3; i++)
			end[i] = origin[i] + time_left * ent->velocity[i];

		trace_t trace = gi.trace(origin, mins, maxs, end, ent, mask);

		if (trace.allsolid)
		{
			// Entity is trapped in another solid
			VectorClear(ent->velocity);
			return 3;
		}

		if (trace.fraction > 0)
		{
			// Actually covered some distance
			VectorCopy(trace.endpos, origin);
			VectorSubtract(origin, ent->origin_offset, ent->s.origin);
			VectorCopy(ent->velocity, original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			break; // Moved the entire distance

		blocker = trace.ent;
		edict_t *hit = trace.ent;

		// Lazarus: If the pushed entity is a conveyor, raise us up and try again
		if (!num_retries && wasonground)
		{
			if (hit->movetype == MOVETYPE_CONVEYOR && trace.plane.normal[2] > 0.7f)
			{
				vec3_t above;
				VectorCopy(end, above);
				above[2] += 32;

				trace = gi.trace(above, mins, maxs, end, ent, mask);
				VectorCopy(trace.endpos, end);
				VectorSubtract(end, origin, ent->velocity);

				VectorScale(ent->velocity, 1.0f / time_left, ent->velocity);
				num_retries++;

				goto retry;
			}
		}

		// If blocked by player AND on a conveyor
		if (hit->client && onconveyor)
		{
			if (ent->mass > hit->mass)
			{
				vec3_t player_dest;
				VectorMA(hit->s.origin, time_left, ent->velocity, player_dest);
				const trace_t ptrace = gi.trace(hit->s.origin, hit->mins, hit->maxs, player_dest, hit, hit->clipmask);
				
				if (ptrace.fraction == 1.0f)
				{
					VectorCopy(player_dest, hit->s.origin);
					gi.linkentity(hit);

					goto retry;
				}
			}

			blocked |= 8;
		}

		if (trace.plane.normal[2] > 0.7f)
		{
			// Lazarus: special case - if this ent or the impact ent is in water, motion is NOT blocked.
			if (hit->movetype != MOVETYPE_PUSHABLE || (ent->waterlevel == 0 && hit->waterlevel == 0))
			{
				blocked |= 1; // Floor

				if (hit->solid == SOLID_BSP)
				{
					ent->groundentity = hit;
					ent->groundentity_linkcount = hit->linkcount;
				}
			}
		}

		if (!trace.plane.normal[2])
			blocked |= 2; // Step

		// Run the impact function
		SV_Impact(ent, &trace);
		if (!ent->inuse)
			break; // Removed by the impact function

		time_left -= time_left * trace.fraction;

		// Clipped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{
			// This shouldn't really happen
			VectorClear(ent->velocity);
			blocked |= 3;

			return blocked;
		}

		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		// Modify original_velocity so it parallels all of the clip planes
		int planenum1;
		vec3_t new_velocity;
		for (planenum1 = 0; planenum1 < numplanes; planenum1++)
		{
			// DH: experimenting here. 1 is no bounce, 1.5 bounces like a grenade, 2 is a superball
			if (ent->bounce_me == 1)
			{
				ClipVelocity(original_velocity, planes[planenum1], new_velocity, 1.4f);
				
				// Stop small oscillations
				if (new_velocity[2] < 60)
				{
					ent->groundentity = trace.ent;
					ent->groundentity_linkcount = trace.ent->linkcount;
					VectorClear(new_velocity);
				}
				else
				{
					// Add a bit of random horizontal motion
					if (!new_velocity[0])
						new_velocity[0] = crandom() * new_velocity[2] / 4;

					if (!new_velocity[1])
						new_velocity[1] = crandom() * new_velocity[2] / 4;
				}
			}
			else if (ent->bounce_me == 2)
			{
				VectorCopy(ent->velocity, new_velocity);
			}
			else
			{
				ClipVelocity(original_velocity, planes[planenum1], new_velocity, 1);
			}

			int planenum2;
			for (planenum2 = 0; planenum2 < numplanes; planenum2++)
			{
				if (planenum2 != planenum1 && !VectorCompare(planes[planenum1], planes[planenum2]))
				{
					if (DotProduct(new_velocity, planes[planenum2]) < 0)
						break; // Not ok
				}
			}

			if (planenum2 == numplanes)
				break;
		}
		
		if (planenum1 != numplanes)
		{
			// Go along this plane
			VectorCopy(new_velocity, ent->velocity);
		}
		else
		{
			// Go along the crease
			if (numplanes != 2)
			{
				VectorClear(ent->velocity);
				blocked |= 7;

				return blocked;
			}

			vec3_t dir;
			CrossProduct(planes[0], planes[1], dir);
			const float d = DotProduct(dir, ent->velocity);
			VectorScale(dir, d, ent->velocity);
		}

		// If velocity is against the original velocity, stop dead to avoid tiny occilations in sloping corners
		if (!ent->bounce_me && DotProduct(ent->velocity, primal_velocity) <= 0.0f)
		{
			VectorClear(ent->velocity);
			return blocked;
		}
	}

	return blocked;
}

void SV_AddGravity(edict_t *ent)
{
	if (level.time > ent->gravity_debounce_time)
		ent->velocity[2] -= ent->gravity * sv_gravity->value * FRAMETIME;
}

// Does not change the entities velocity at all.
// Called for MOVETYPE_TOSS, MOVETYPE_BOUNCE, MOVETYPE_FLY, MOVETYPE_FLYMISSILE, MOVETYPE_RAIN
static trace_t SV_PushEntity(edict_t *ent, const vec3_t push)
{
	trace_t trace;

	int mask = (ent->clipmask ? ent->clipmask : MASK_SOLID);
	int num_retries = 0;

	vec3_t start, end;
	VectorCopy(ent->s.origin, start);
	VectorAdd(start, push, end);

retry:

	trace = gi.trace(start, ent->mins, ent->maxs, end, ent, mask);
	
	// Harven fix start
	if (trace.startsolid || trace.allsolid)
	{
		mask ^= CONTENTS_DEADMONSTER;
		trace = gi.trace (start, ent->mins, ent->maxs, end, ent, mask);
	}
	// Harven fix end

	VectorCopy(trace.endpos, ent->s.origin);
	gi.linkentity(ent);

	if (trace.fraction != 1.0f)
	{
		SV_Impact(ent, &trace);

		// If the pushed entity went away and the pusher is still there
		if (!trace.ent->inuse && ent->inuse)
		{
			// Move the pusher back and try again
			VectorCopy(start, ent->s.origin);
			gi.linkentity(ent);

			goto retry;
		}

		// Lazarus: If the pushed entity is a conveyor, raise us up and try again
		if (!num_retries && wasonground)
		{
			if (trace.ent->movetype == MOVETYPE_CONVEYOR && trace.plane.normal[2] > 0.7f && !trace.startsolid)
			{
				vec3_t above;
				VectorCopy(end,above);
				above[2] += 32;

				trace = gi.trace(above, ent->mins, ent->maxs, end, ent, mask);
				VectorCopy(trace.endpos, end);
				VectorCopy(start, ent->s.origin);
				gi.linkentity(ent);
				num_retries++;

				goto retry;
			}
		}

		if (onconveyor && !trace.ent->client)
		{
			// If blocker can be damaged, destroy it. Otherwise destroy blockee.
			if (trace.ent->takedamage == DAMAGE_YES)
				T_Damage(trace.ent, ent, ent, vec3_origin, trace.ent->s.origin, vec3_origin, 100000, 1, 0, MOD_CRUSH);
			else
				T_Damage(ent, trace.ent, trace.ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 1, 0, MOD_CRUSH);
		}
	}

	if (ent->inuse)
		G_TouchTriggers(ent);

	return trace;
}

typedef struct
{
	edict_t	*ent;
	vec3_t origin;
	vec3_t angles;
	float deltayaw;
} pushed_t;

static pushed_t pushed[MAX_EDICTS];
static pushed_t *pushed_p;
static edict_t *obstacle;

static void MoveRiders(edict_t *platform, edict_t *ignore, vec3_t move, vec3_t amove, const qboolean turn)
{
	edict_t *rider = g_edicts + 1;
	for (int i = 1; i <= globals.num_edicts; i++, rider++)
	{
		if (rider->groundentity == platform && rider != ignore)
		{
			VectorAdd(rider->s.origin, move, rider->s.origin);
			
			if (turn && amove[YAW] != 0.0f)
			{
				rider->s.angles[YAW] += amove[YAW];
				if (rider->client)
				{
					rider->client->ps.pmove.delta_angles[YAW] += ANGLE2SHORT(amove[YAW]);
					rider->client->ps.pmove.pm_type = PM_FREEZE;
					rider->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
				}
			}

			gi.linkentity(rider);

			if (SV_TestEntityPosition(rider))
			{
				// Move is blocked. Since this is for riders, not pushees, it should be ok to just back the move for this rider off.
				VectorSubtract(rider->s.origin, move, rider->s.origin);
				
				if (turn && amove[YAW] != 0.0f)
				{
					rider->s.angles[YAW] -= amove[YAW];
					if (rider->client)
					{
						rider->client->ps.pmove.delta_angles[YAW] -= ANGLE2SHORT(amove[YAW]);
						rider->client->ps.viewangles[YAW] -= amove[YAW];
					}
				}

				gi.linkentity(rider);
			}
			else
			{
				// Move this rider's riders
				MoveRiders(rider, ignore, move, amove, turn);
			}
		}
	}
}

// Returns the actual bounding box of a bmodel. This is a big improvement over
// what q2 normally does with rotating bmodels - q2 sets absmin, absmax to a cube
// that will completely contain the bmodel at *any* rotation on *any* axis, whether
// the bmodel can actually rotate to that angle or not. This leads to a lot of
// false block tests in SV_Push if another bmodel is in the vicinity.
static void RealBoundingBox(edict_t *ent, vec3_t mins, vec3_t maxs)
{
	vec3_t p[8];

	for (int k = 0; k < 2; k++)
	{
		const int k4 = k * 4;
		if (k)
			p[k4][2] = ent->maxs[2];
		else
			p[k4][2] = ent->mins[2];

		p[k4 + 1][2] = p[k4][2];
		p[k4 + 2][2] = p[k4][2];
		p[k4 + 3][2] = p[k4][2];

		for (int j = 0; j < 2; j++)
		{
			const int j2 = j * 2;
			if (j)
				p[j2 + k4][1] = ent->maxs[1];
			else
				p[j2 + k4][1] = ent->mins[1];

			p[j2 + k4 + 1][1] = p[j2 + k4][1];

			for (int i = 0; i < 2; i++)
			{
				if (i)
					p[i + j2 + k4][0] = ent->maxs[0];
				else
					p[i + j2 + k4][0] = ent->mins[0];
			}
		}
	}

	vec3_t forward, left, up;
	AngleVectors(ent->s.angles, forward, left, up);

	for (int i = 0; i < 8; i++)
	{
		vec3_t f1, l1, u1;

		VectorScale(forward, p[i][0], f1);
		VectorScale(left, -p[i][1], l1);
		VectorScale(up, p[i][2], u1);

		VectorAdd(ent->s.origin, f1, p[i]);
		VectorAdd(p[i], l1, p[i]);
		VectorAdd(p[i], u1, p[i]);
	}

	VectorCopy(p[0], mins);
	VectorCopy(p[0], maxs);

	for (int i = 1; i < 8; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			mins[j] = min(mins[j], p[i][j]);
			maxs[j] = max(maxs[j], p[i][j]);
		}
	}
}

// Objects need to be moved back on a failed push, otherwise riders would continue to slide.
static qboolean SV_Push(edict_t *pusher, vec3_t move, vec3_t amove)
{
	// Clamp the move to 1/8 units, so the position will be accurate for client side prediction.
	for (int i = 0; i < 3; i++)
	{
		float temp = move[i] * 8.0f;
		if (temp > 0.0f)
			temp += 0.5f;
		else
			temp -= 0.5f;

		move[i] = 0.125f * (int)temp;
	}

	// Lazarus: temp turn indicates whether riders should rotate with the pusher
	const qboolean turn = (pusher->turn_rider || turn_rider->integer); // Knightmare- changed this from AND to OR

	// We need this for pushing things later
	vec3_t org;
	VectorSubtract(vec3_origin, amove, org);

	vec3_t forward, right, up;
	AngleVectors(org, forward, right, up);

	// Save the pusher's original position
	pushed_p->ent = pusher;
	VectorCopy(pusher->s.origin, pushed_p->origin);
	VectorCopy(pusher->s.angles, pushed_p->angles);

	if (pusher->client)
		pushed_p->deltayaw = pusher->client->ps.pmove.delta_angles[YAW];

	pushed_p++;

	// Move the pusher to it's final position
	VectorAdd(pusher->s.origin, move, pusher->s.origin);
	VectorAdd(pusher->s.angles, amove, pusher->s.angles);
	gi.linkentity(pusher);

	// Lazarus: Standard Q2 takes a horrible shortcut with rotating brush models, setting absmin and absmax to a cube that would
	// contain the brush model if it could rotate around ANY axis.
	// The result is a lot of false hits on intersections, particularly when you have multiple rotating brush models in the same area.
	// RealBoundingBox gives us the actual bounding box at the current angles.
	vec3_t realmins, realmaxs;
	RealBoundingBox(pusher, realmins, realmaxs);

	// See if any solid entities are inside the final position
	edict_t *check = g_edicts + 1;
	for (int e = 1; e < globals.num_edicts; e++, check++)
	{
		if (!check->inuse || check == pusher->owner || !check->area.prev) // unused || Lazarus: owner can't block us || not linked in anywhere
			continue;

		switch (check->movetype)
		{
			case MOVETYPE_PUSH:
			case MOVETYPE_STOP:
			case MOVETYPE_NONE:
			case MOVETYPE_NOCLIP:
			case MOVETYPE_PENDULUM:
				continue;
		}

		// If the entity is standing on the pusher, it will definitely be moved.
		if (check->groundentity != pusher)
		{
			// See if the ent needs to be tested
			if (check->absmin[0] >= realmaxs[0] || check->absmin[1] >= realmaxs[1] || check->absmin[2] >= realmaxs[2]
			 || check->absmax[0] <= realmins[0] || check->absmax[1] <= realmins[1] || check->absmax[2] <= realmins[2])
				continue;

			// See if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition(check))
				continue;
		}

		// Lazarus: func_tracktrain-specific stuff
		// If train is *driven*, then hurt monsters/players it touches NOW rather than waiting to be blocked.
		if ((pusher->flags & FL_TRACKTRAIN) && pusher->owner && ((check->svflags & SVF_MONSTER) || check->client) && check->groundentity != pusher)
		{
			vec3_t dir;
			VectorSubtract(check->s.origin, pusher->s.origin, dir);
			dir[2] += 16;
			VectorNormalize(dir);

			const int knockback = (int)(fabsf(pusher->moveinfo.current_speed) * check->mass / 300.0f);
			T_Damage(check, pusher, pusher, dir, check->s.origin, vec3_origin, pusher->dmg, knockback, 0, MOD_CRUSH);
		}

		if (pusher->movetype == MOVETYPE_PUSH || pusher->movetype == MOVETYPE_PENDULUM || check->groundentity == pusher)
		{
			// Move this entity
			pushed_p->ent = check;
			VectorCopy(check->s.origin, pushed_p->origin);
			VectorCopy(check->s.angles, pushed_p->angles);
			pushed_p++;

			// Try moving the contacted entity 
			VectorAdd(check->s.origin, move, check->s.origin);

			// Lazarus: if turn_rider is set, do it. We don't do this by default 'cause it can be a fairly drastic change in gameplay.
			if (turn && check->groundentity == pusher)
			{
				if (!check->client)
				{
					check->s.angles[YAW] += amove[YAW];
				}
				else
				{
					if (amove[YAW] != 0.0f)
					{
						check->client->ps.pmove.delta_angles[YAW] += ANGLE2SHORT(amove[YAW]);
						check->client->ps.viewangles[YAW] += amove[YAW];

						// PM_FREEZE makes the turn smooth, even though it will be turned off by ClientThink in the very next video frame
						check->client->ps.pmove.pm_type = PM_FREEZE;

						// PMF_NO_PREDICTION overrides .exe's client physics, which really doesn't like for us to change player angles.
						// Note that this isn't strictly necessary, since Lazarus 1.7 and later automatically turn prediction off (in ClientThink)
						// when player is riding a MOVETYPE_PUSH.
						check->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
					}

					if (amove[PITCH] != 0.0f)
					{
						const float delta_yaw = (check->s.angles[YAW] - pusher->s.angles[YAW]) * M_PI / 180.0f;
						const float pitch = amove[PITCH] * cosf(delta_yaw);

						check->client->ps.pmove.delta_angles[PITCH] += ANGLE2SHORT(pitch);
						check->client->ps.viewangles[PITCH] += pitch;
						check->client->ps.pmove.pm_type = PM_FREEZE;
						check->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
					}
				}
			}

			vec3_t move2 = { 0, 0, 0 };

			// Lazarus: This is where we attempt to move check due to a rotation, WITHOUT embedding check in pusher (or anything else)
			if (amove[PITCH] != 0 || amove[YAW] != 0 || amove[ROLL] != 0)
			{
				// Argh! - always need to do this, except for pendulums
				if (pusher->movetype != MOVETYPE_PENDULUM)
				{
					// Figure movement due to the pusher's amove
					vec3_t org_check;
					VectorAdd(check->s.origin, check->origin_offset, org_check);
					VectorSubtract(org_check, pusher->s.origin, org);

					vec3_t org2;
					org2[0] = DotProduct(org, forward);
					org2[1] = -DotProduct(org, right);
					org2[2] = DotProduct(org, up);

					VectorSubtract(org2, org, move2);
					VectorAdd(check->s.origin, move2, check->s.origin);
				}

				// Argh! - on top of a rotating pusher (moved the groundentity check here)
				if (check->groundentity == pusher)
				{
					if (amove[PITCH] != 0 || amove[ROLL] != 0)
					{
						VectorCopy(check->s.origin, org);
						org[2] += 2 * check->mins[2];

						// Argh! - this should fix collision problem with simple rotating pushers, trains still seem okay too but I haven't tested them thoroughly.
						trace_t tr = gi.trace(check->s.origin, check->mins, check->maxs, org, check, MASK_SOLID);
						if (!tr.startsolid && tr.fraction < 1)
							check->s.origin[2] = tr.endpos[2];

						// Lazarus: func_tracktrain is a special case. Since we KNOW (if the map was constructed properly) that "move_origin" is a safe position, we
						// can infer that there should be a safe (not embedded) position somewhere between move_origin and the proposed new location.
						if ((pusher->flags & FL_TRACKTRAIN) && (check->client || (check->svflags & SVF_MONSTER)))
						{
							vec3_t f, l, u;
							AngleVectors(pusher->s.angles, f, l, u);
							VectorScale(f, pusher->move_origin[0], f);
							VectorScale(l, -pusher->move_origin[1], l);
							
							VectorAdd(pusher->s.origin, f, org);
							VectorAdd(org, l, org);
							org[2] += pusher->move_origin[2] + 1;
							org[2] += 16 * (fabsf(u[0]) + fabsf(u[1]));

							tr = gi.trace(org, check->mins, check->maxs, check->s.origin, check, MASK_SOLID);
							if (!tr.startsolid)
							{
								VectorCopy(tr.endpos, check->s.origin);
								VectorCopy(check->s.origin, org);
								org[2] -= 128;

								tr = gi.trace(check->s.origin, check->mins, check->maxs, org, check, MASK_SOLID);
								if (tr.fraction > 0)
									VectorCopy(tr.endpos,check->s.origin);
							}
						}
					}
				}
			}
			
			// May have pushed them off an edge
			if (check->groundentity != pusher)
				check->groundentity = NULL;

			// Lazarus - don't block movewith trains with a rider - they may end up being stuck, but that beats a small pitch or roll causing blocked trains/gibbed monsters.
			if (check->movewith_ent == pusher)
			{
				gi.linkentity(check);
				continue;
			}

			edict_t *block = SV_TestEntityPosition(check);

			if (block && (pusher->flags & FL_TRACKTRAIN) && (check->client || (check->svflags & SVF_MONSTER)) && check->groundentity == pusher)
			{
				// Lazarus: Last hope. If this doesn't get rider out of the way he's gonna be stuck.
				vec3_t f, l, u;
				AngleVectors(pusher->s.angles, f, l, u);
				VectorScale(f, pusher->move_origin[0], f);
				VectorScale(l, -pusher->move_origin[1], l);

				VectorAdd(pusher->s.origin, f, org);
				VectorAdd(org, l, org);
				org[2] += pusher->move_origin[2] + 1;
				org[2] += 16 * (fabsf(u[0]) + fabsf(u[1]));

				trace_t tr = gi.trace(org, check->mins, check->maxs, check->s.origin, check, MASK_SOLID);
				if (!tr.startsolid)
				{
					VectorCopy(tr.endpos, check->s.origin);
					VectorCopy(check->s.origin, org);
					org[2] -= 128;

					tr = gi.trace(check->s.origin, check->mins, check->maxs, org, check, MASK_SOLID);

					if (tr.fraction > 0.0f)
						VectorCopy(tr.endpos, check->s.origin);

					block = SV_TestEntityPosition(check);
				}
			}

			if (!block)
			{
				// Pushed ok
				gi.linkentity(check);

				// Lazarus: Move check riders, and riders of riders, and... well, you get the pic
				vec3_t move3;
				VectorAdd(move, move2, move3);
				MoveRiders(check, NULL, move3, amove, turn);
				
				// Impact?
				continue;
			}

			// If it is ok to leave in the old position, do it.	This is only relevent for riding entities, not pushed.
			VectorSubtract(check->s.origin, move,  check->s.origin);
			VectorSubtract(check->s.origin, move2, check->s.origin);
			
			if (turn)
			{
				// Argh! - angle
				check->s.angles[YAW] -= amove[YAW];
				if (check->client)
				{
					check->client->ps.pmove.delta_angles[YAW] -= ANGLE2SHORT(amove[YAW]);
					check->client->ps.viewangles[YAW] -= amove[YAW];
				}
			}

			block = SV_TestEntityPosition(check);

			if (!block)
			{
				pushed_p--;
				continue;
			}

			if (check->svflags & SVF_GIB) //Knightmare- gibs don't block
			{
				G_FreeEdict(check);
				pushed_p--;

				continue;
			}
		}
		
		// Save off the obstacle so we can call the block function
		obstacle = check;

		// Move back any entities we already moved. Go backwards, so if the same entity was pushed twice, it goes back to the original position.
		for (pushed_t *p = pushed_p - 1; p >= pushed; p--)
		{
			VectorCopy(p->origin, p->ent->s.origin);
			VectorCopy(p->angles, p->ent->s.angles);
			
			if (p->ent->client)
				p->ent->client->ps.pmove.delta_angles[YAW] = p->deltayaw;

			gi.linkentity(p->ent);
		}

		return false;
	}

	//FIXME: is there a better way to handle this?
	// See if anything we moved has touched a trigger.
	for (pushed_t *p = pushed_p - 1; p >= pushed; p--)
		G_TouchTriggers(p->ent);

	return true;
}

// Bmodel objects don't interact with each other, but push all box objects
static void SV_Physics_Pusher(edict_t *ent)
{
	// If not a team captain, movement will be handled elsewhere.
	if (ent->flags & FL_TEAMSLAVE)
		return;

	// Make sure all team slaves can move before commiting any moves or calling any think functions.
	// If the move is blocked, all moved objects will be backed out.
	pushed_p = pushed;

	edict_t *part;
	for (part = ent; part; part = part->teamchain)
	{
		if (part->attracted)
		{
			part->velocity[0] = 0.0f;
			part->velocity[1] = 0.0f;
		}

		if (VectorLengthSquared(part->velocity) || VectorLengthSquared(part->avelocity))
		{
			// Object is moving
			vec3_t move, amove;
			VectorScale(part->velocity, FRAMETIME, move);
			VectorScale(part->avelocity, FRAMETIME, amove);

			if (!SV_Push(part, move, amove))
				break; // Move was blocked

			if (part->moveinfo.is_blocked)
			{
				part->moveinfo.is_blocked = false;
				if (part->moveinfo.sound_middle)
					part->s.sound = part->moveinfo.sound_middle;
			}
		}
	}

	if (pushed_p > &pushed[MAX_EDICTS - 1])
		gi.error(ERR_FATAL, "pushed_p > &pushed[MAX_EDICTS - 1], memory corrupted");

	if (part && !part->attracted)
	{
		// The move failed, bump all nextthink times and back out moves.
		for (edict_t *mv = ent; mv; mv = mv->teamchain)
		{
			if (mv->nextthink > 0)
				mv->nextthink += FRAMETIME;
		}

		// If the pusher has a "blocked" function, call it otherwise, just stay in place until the obstacle is gone.
		if (part->blocked)
		{
			// Lazarus: func_pushables with health < 0 & vehicles ALWAYS block pushers
			if ((obstacle->movetype == MOVETYPE_PUSHABLE && obstacle->health < 0) || obstacle->movetype == MOVETYPE_VEHICLE)
			{
				part->moveinfo.is_blocked = true;

				if (part->s.sound)
				{
					if (part->moveinfo.sound_end)
						gi.sound(part, CHAN_NO_PHS_ADD + CHAN_VOICE, part->moveinfo.sound_end, 1, ATTN_STATIC, 0);

					part->s.sound = 0;
				}

				// Lazarus: More special-case stuff. Man I hate doing this
				if (part->movetype == MOVETYPE_PENDULUM)
				{
					if (fabsf(part->s.angles[ROLL]) > 2)
					{
						part->moveinfo.start_angles[ROLL] = part->s.angles[ROLL];
						VectorClear(part->avelocity);
						part->startframe = 0;
					}
					else
					{
						part->spawnflags &= ~1;
						part->moveinfo.start_angles[ROLL] = 0;
						VectorClear(part->s.angles);
						VectorClear(part->avelocity);
					}
				}
			}
			else
			{
				part->blocked(part, obstacle);
				part->moveinfo.is_blocked = true;
			}
		}
	}
	else
	{
		// The move succeeded, so call all think functions.
		for (edict_t *p = ent; p; p = p->teamchain)
			SV_RunThink(p);
	}
}

// Non-moving objects can only think
static void SV_Physics_None(edict_t *ent)
{
	// Regular thinking
	SV_RunThink(ent);
}

// A moving object that doesn't obey physics
static void SV_Physics_Noclip(edict_t *ent)
{
	// Regular thinking
	if (SV_RunThink(ent))
	{
		VectorMA(ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);
		VectorMA(ent->s.origin, FRAMETIME, ent->velocity, ent->s.origin);

		gi.linkentity(ent);
	}
}

// Toss, bounce, and fly movement. When onground, do nothing.
static void SV_Physics_Toss(edict_t *ent)
{
	// Regular thinking
	SV_RunThink(ent);

	// If not a team captain, so movement will be handled elsewhere
	if (ent->flags & FL_TEAMSLAVE)
		return;

	if (ent->groundentity)
		wasonground = true;

	if (ent->velocity[2] > 0.0f)
		ent->groundentity = NULL;

	// Check for the groundentity going away
	if (ent->groundentity && !ent->groundentity->inuse)
		ent->groundentity = NULL;

	// Lazarus: conveyor
	if (ent->groundentity && ent->groundentity->movetype == MOVETYPE_CONVEYOR)
	{
		edict_t	*ground = ent->groundentity;

		vec3_t point;
		VectorCopy(ent->s.origin, point);
		point[2] += 1;

		vec3_t end;
		VectorCopy(point, end);
		end[2] -= 256;

		const trace_t tr = gi.trace(point, ent->mins, ent->maxs, end, ent, MASK_SOLID);

		// tr.ent HAS to be ground, but just in case we screwed something up:
		if (tr.ent == ground)
		{
			onconveyor = true;

			ent->velocity[0] = ground->movedir[0] * ground->speed;
			ent->velocity[1] = ground->movedir[1] * ground->speed;

			if (tr.plane.normal[2] > 0.0f)
			{
				ent->velocity[2] = ground->speed * sqrtf(1.0f - tr.plane.normal[2] * tr.plane.normal[2]) / tr.plane.normal[2];

				if (DotProduct(ground->movedir, tr.plane.normal) > 0) // Then we're moving down
					ent->velocity[2] = -ent->velocity[2];
			}

			vec3_t move;
			VectorScale(ent->velocity, FRAMETIME, move);
			SV_PushEntity(ent, move);

			if (!ent->inuse)
				return;

			M_CheckGround(ent);
		}
	}

	// If onground, return without moving
	if (ent->groundentity)
		return;

	vec3_t old_origin;
	VectorCopy(ent->s.origin, old_origin);

	SV_CheckVelocity(ent);

	// Add gravity
	if (ent->movetype != MOVETYPE_FLY && ent->movetype != MOVETYPE_FLYMISSILE && ent->movetype != MOVETYPE_VEHICLE && ent->movetype != MOVETYPE_RAIN)
		SV_AddGravity(ent);

	// Move angles
	VectorMA(ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);

	// Move origin
	vec3_t move;
	VectorScale(ent->velocity, FRAMETIME, move);
	trace_t trace = SV_PushEntity(ent, move);

	if (!ent->inuse)
		return;

	// isinwater = ent->watertype & MASK_WATER;
	if (trace.fraction < 1.0f)
	{
		//mxd. Reflect off steep inclines
		if (trace.plane.normal[2] <= 0.7f)
		{
			ReflectVelocity(ent->velocity, trace.plane.normal, ent->velocity, 0.9f);
		}
		else
		{
			const float backoff = (ent->movetype == MOVETYPE_BOUNCE ? 1.0f + bounce_bounce->value : 1.0f);
			ClipVelocity(ent->velocity, trace.plane.normal, ent->velocity, backoff);

			// Stop if on ground
			if (ent->velocity[2] < bounce_minv->value || ent->movetype != MOVETYPE_BOUNCE)
			{
				ent->groundentity = trace.ent;
				ent->groundentity_linkcount = trace.ent->linkcount;
				VectorClear(ent->velocity);
				VectorClear(ent->avelocity);
			}
		}

		if (ent->touch) //mxd. Re-enabled
			ent->touch(ent, trace.ent, &trace.plane, trace.surface);
	}

	// Lazarus: MOVETYPE_RAIN doesn't cause splash noises when touching water
	if (ent->movetype != MOVETYPE_RAIN)
	{
		// Check for water transition
		const qboolean wasinwater = (ent->watertype & MASK_WATER);
		ent->watertype = gi.pointcontents(ent->s.origin);
		const qboolean isinwater = (ent->watertype & MASK_WATER);

		ent->waterlevel = (isinwater ? 1 : 0);

		// tpp... Don't do sounds for the camera
		if (Q_stricmp(ent->classname, "chasecam"))
		{
			if (!wasinwater && isinwater)
				gi.positioned_sound(old_origin, g_edicts, CHAN_AUTO, gi.soundindex("misc/h2ohit1.wav"), 1, 1, 0);
			else if (wasinwater && !isinwater)
				gi.positioned_sound(ent->s.origin, g_edicts, CHAN_AUTO, gi.soundindex("misc/h2ohit1.wav"), 1, 1, 0);
		}
	}

	// Move teamslaves
	for (edict_t *slave = ent->teamchain; slave; slave = slave->teamchain)
	{
		VectorCopy(ent->s.origin, slave->s.origin);
		gi.linkentity(slave);
	}
}

#define SV_FRICTION			6
#define SV_WATERFRICTION	1

static void SV_AddRotationalFriction(edict_t *ent)
{
	VectorMA(ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);
	const float adjustment = FRAMETIME * sv_stopspeed->value * SV_FRICTION; //PGM now a cvar
	
	for (int n = 0; n < 3; n++)
	{
		if (ent->avelocity[n] > 0.0f)
		{
			ent->avelocity[n] -= adjustment;
			ent->avelocity[n] = max(0, ent->avelocity[n]);
		}
		else
		{
			ent->avelocity[n] += adjustment;
			ent->avelocity[n] = min(0, ent->avelocity[n]);
		}
	}
}

#define WATER_DENSITY 0.00190735f

static float RiderMass(edict_t *platform)
{
	float mass = 0;

	edict_t *rider = g_edicts + 1;
	for (int i = 1; i <= globals.num_edicts; i++, rider++)
	{
		if (rider == platform || !rider->inuse)
			continue;

		if (rider->groundentity == platform)
		{
			mass += rider->mass;
			mass += RiderMass(rider);
		}
		else if (rider->movetype == MOVETYPE_PUSHABLE)
		{
			// Bah - special case for func_pushable riders.
			// Swimming func_pushables don't really have a groundentity, even though 
			// they may be sitting on another swimming func_pushable, which is what we need to know.
			vec3_t point;
			VectorCopy(rider->s.origin, point);
			point[2] -= 0.25f;

			const trace_t trace = gi.trace(rider->s.origin, rider->mins, rider->maxs, point, rider, MASK_MONSTERSOLID);
			if (trace.plane.normal[2] < 0.7f && !trace.startsolid)
				continue;

			if (!trace.startsolid && !trace.allsolid && trace.ent == platform)
			{
				mass += rider->mass;
				mass += RiderMass(rider);
			}
		}
	}

	return mass;
}

// Monsters freefall when they don't have a ground entity, otherwise all movement is done with discrete steps.
// This is also used for objects that have become still on the ground, but will fall if the floor is pulled out from under them.
// FIXME: is this true?
static void SV_Physics_Step(edict_t *ent)
{
	// Airborne monsters should always check for ground
	if (!ent->groundentity)
		M_CheckGround(ent);

	const int oldwaterlevel = ent->waterlevel;

	vec3_t old_origin;
	VectorCopy(ent->s.origin, old_origin);

	// Lazarus: If density hasn't been calculated yet, do so now
	if (ent->mass > 0 && ent->density == 0.0f)
	{
		ent->volume = ent->size[0] * ent->size[1] * ent->size[2];
		ent->density = ent->mass/ent->volume;

		if (ent->movetype == MOVETYPE_PUSHABLE)
		{
			// This stuff doesn't apply to anything else, and caused monster_flipper to sink...
			ent->bob = min(2.0f, 300.0f / ent->mass);
			ent->duration = max(2.0f, 1.0f + ent->mass / 100.0f);
			
			// Figure out neutral bouyancy line for this entity.
			// This isn't entirely realistic, but helps gameplay:
			// Arbitrary mass limit for func_pushable that be pushed on land is 500.
			// So make a mass = 500+, 64x64x64 crate sink (otherwise, player might cause a 501 crate to leave water and expect to be able to push it).
			// Max floating density is then 0.0019073486328125
			if (ent->density > WATER_DENSITY)
				ent->flags &= ~FL_SWIM; // Sinks like a rock
		}
	}

	vec3_t point;

	// If not a monster, then determine whether we're in water (monsters take care of this in g_monster.c).
	if (!(ent->svflags & SVF_MONSTER) && (ent->flags & FL_SWIM))
	{
		point[0] = (ent->absmax[0] + ent->absmin[0]) / 2;
		point[1] = (ent->absmax[1] + ent->absmin[1]) / 2;
		point[2] = (ent->absmin[2] + 1);

		int contents = gi.pointcontents(point);
		if (!(contents & MASK_WATER))
		{
			ent->waterlevel = 0;
			ent->watertype = 0;
		}
		else
		{
			ent->watertype = contents;
			ent->waterlevel = 1;
			point[2] = ent->absmin[2] + ent->size[2] / 2;
			contents = gi.pointcontents(point);

			if (contents & MASK_WATER)
			{
				ent->waterlevel = 2;
				point[2] = ent->absmax[2];
				contents = gi.pointcontents(point);

				if (contents & MASK_WATER)
					ent->waterlevel = 3;
			}
		}
	}
	
	edict_t *ground = ent->groundentity;

	SV_CheckVelocity(ent);

	if (ground)
		wasonground = true;
		
	if (ent->avelocity[0] || ent->avelocity[1] || ent->avelocity[2])
		SV_AddRotationalFriction(ent);

	// Add gravity except: flying monsters, swimming monsters who are in the water.
	qboolean hitsound = false;
	if (!wasonground && !(ent->flags & FL_FLY) && !((ent->flags & FL_SWIM) && ent->waterlevel > 2))
	{
		if (ent->velocity[2] < sv_gravity->value * -0.1f)
			hitsound = true;

		if (ent->waterlevel == 0)
			SV_AddGravity(ent);
	}

	// Friction for flying monsters that have been given vertical velocity
	if ((ent->flags & FL_FLY) && ent->velocity[2] != 0.0f)
	{
		const float speed = fabsf(ent->velocity[2]);
		const float control = max(speed, sv_stopspeed->value);
		const float friction = SV_FRICTION / 3.0f;

		float newspeed = speed - (FRAMETIME * control * friction);
		newspeed = max(0, newspeed);
		newspeed /= speed;

		ent->velocity[2] *= newspeed;
	}

	// Friction for swimming monsters that have been given vertical velocity
	if (ent->movetype != MOVETYPE_PUSHABLE)
	{
		// Lazarus: This is id's swag at drag. It works mostly, but for submerged crates we can do better.
		if ((ent->flags & FL_SWIM) && ent->velocity[2] != 0)
		{
			const float speed = fabsf(ent->velocity[2]);
			const float control = max(speed, sv_stopspeed->value);
			
			float newspeed = speed - (FRAMETIME * control * SV_WATERFRICTION * ent->waterlevel);
			newspeed = max(0, newspeed);
			newspeed /= speed;

			ent->velocity[2] *= newspeed;
		}
	}

	// Lazarus: Floating stuff
	if (ent->movetype == MOVETYPE_PUSHABLE && (ent->flags & FL_SWIM) && ent->waterlevel)
	{
		float waterlevel;

		if (ent->waterlevel < 3)
		{
			vec3_t end;
			VectorCopy(point, end);
			
			point[2] = ent->absmax[2];
			end[2]   = ent->absmin[2];

			const trace_t tr = gi.trace(point, NULL, NULL, end, ent, MASK_WATER);
			waterlevel = tr.endpos[2];
		}
		else
		{
			// Not right, but really all we need to know
			waterlevel = ent->absmax[2] + 1;
		}

		const float total_mass = RiderMass(ent) + ent->mass;
		const float area = ent->size[0] * ent->size[1];

		if (waterlevel < ent->absmax[2])
		{
			// For partially submerged crates, use same psuedo-friction thing used on other entities.
			// This isn't really correct, but then neither is our drag calculation used for fully submerged crates good for this situation.
			if (ent->velocity[2] != 0.0f)
			{
				const float speed = fabs(ent->velocity[2]);
				const float control = max(speed, sv_stopspeed->value);

				float newspeed = speed - (FRAMETIME * control * SV_WATERFRICTION * ent->waterlevel);
				newspeed = max(0, newspeed);
				newspeed /= speed;

				ent->velocity[2] *= newspeed;
			}

			// Apply physics and bob AFTER friction, or the damn thing will never move.
			const float force = -total_mass + ((waterlevel - ent->absmin[2]) * area * WATER_DENSITY);
			const float accel = force * sv_gravity->value / total_mass;
			ent->velocity[2] += accel * FRAMETIME;

			const int time = ent->duration * 10;
			const float t0 = ent->bobframe % time;
			const float t1 = (ent->bobframe + 1) % time;
			const float z0 = sinf(2 * M_PI * t0 / time);
			const float z1 = sinf(2 * M_PI * t1 / time);

			ent->velocity[2] += ent->bob * (z1 - z0) * 10.0f;
			ent->bobframe = (ent->bobframe + 1) % time;
		}
		else
		{
			// Crate is fully submerged
			float force = -total_mass + ent->volume * WATER_DENSITY;

			if (sv_gravity->value)
			{
				float drag = 0.00190735f * 1.05f * area * (ent->velocity[2] * ent->velocity[2]) / sv_gravity->value;

				if (drag > fabsf(force))
				{
					// Drag actually CAN be > total weight, but if we do this we tend to
					// get crates flying back out of the water after being dropped from some height.
					drag = fabsf(force);
				}

				if (ent->velocity[2] > 0.0f)
					drag = -drag;

				force += drag;
			}

			const float accel = force * sv_gravity->value / total_mass;
			ent->velocity[2] += accel * FRAMETIME;
		}

		if (ent->watertype & MASK_CURRENT)
		{
			// Move with current, relative to mass. Mass 400 or less will move at 50 units/sec.
			const float v = (ent->mass > 400 ? ent->mass * 0.125f : 50.0f);
			const int current = ent->watertype & MASK_CURRENT;

			switch (current)
			{
				case CONTENTS_CURRENT_0:    ent->velocity[0] = v;  break;
				case CONTENTS_CURRENT_90:   ent->velocity[1] = v;  break;
				case CONTENTS_CURRENT_180:  ent->velocity[0] = -v; break;
				case CONTENTS_CURRENT_270:  ent->velocity[1] = -v; break;
				case CONTENTS_CURRENT_UP :  ent->velocity[2] = max(v, ent->velocity[2]); break;
				case CONTENTS_CURRENT_DOWN: ent->velocity[2] = min(-v, ent->velocity[2]); break;
			}
		}
	}

	// Conveyor
	if (wasonground && ground->movetype == MOVETYPE_CONVEYOR)
	{
		vec3_t start;
		VectorCopy(ent->s.origin, start);
		start[2] += 1;

		vec3_t end;
		VectorCopy(start, end);
		end[2] -= 256;

		const trace_t tr = gi.trace(start, ent->mins, ent->maxs, end, ent, MASK_SOLID);
		
		// tr.ent HAS to be ground, but just in case we screwed something up:
		if (tr.ent == ground)
		{
			onconveyor = true;
			ent->velocity[0] = ground->movedir[0] * ground->speed;
			ent->velocity[1] = ground->movedir[1] * ground->speed;

			if (tr.plane.normal[2] > 0)
			{
				ent->velocity[2] = ground->speed * sqrtf(1.0f - tr.plane.normal[2] * tr.plane.normal[2]) / tr.plane.normal[2];
				
				if (DotProduct(ground->movedir, tr.plane.normal) > 0.0f) // Then we're moving down.
					ent->velocity[2] = -ent->velocity[2] + 2;
			}
		}
	}

	if (ent->velocity[0] || ent->velocity[1] || ent->velocity[2])
	{
		// Apply friction. Let dead monsters who aren't completely onground slide
		if (!onconveyor && (wasonground || (ent->flags & (FL_SWIM | FL_FLY))) && !(ent->health <= 0.0f && !M_CheckBottom(ent)))
		{
			float *vel = ent->velocity;
			const float speed = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);

			if (speed != 0)
			{
				const float control = max(speed, sv_stopspeed->value);
				float newspeed = speed - FRAMETIME * control * SV_FRICTION;
				newspeed = max(0, newspeed);
				newspeed /= speed;

				vel[0] *= newspeed;
				vel[1] *= newspeed;
			}
		}

		int mask;
		if (ent->svflags & SVF_MONSTER)
			mask = MASK_MONSTERSOLID;
		else if (ent->movetype == MOVETYPE_PUSHABLE)
			mask = MASK_MONSTERSOLID | MASK_PLAYERSOLID;
		else if (ent->clipmask)
			mask = ent->clipmask; // Lazarus edition
		else
			mask = MASK_SOLID;

		int block;
		if (ent->movetype == MOVETYPE_PUSHABLE)
			block = SV_PushableMove(ent, FRAMETIME, mask);
		else
			block = SV_FlyMove(ent, FRAMETIME, mask);

		if (block && !(block & 8) && onconveyor)
		{
			if (blocker && blocker->takedamage == DAMAGE_YES)
				T_Damage(blocker, world, world, vec3_origin, ent->s.origin, vec3_origin, 100000, 1, 0, MOD_CRUSH);
			else
				T_Damage(ent, world, world, vec3_origin, ent->s.origin, vec3_origin, 100000, 1, 0, MOD_CRUSH);

			if (!ent->inuse)
				return;
		}

		gi.linkentity(ent);
		G_TouchTriggers(ent);
		if (!ent->inuse)
			return;

		if (ent->groundentity && !wasonground && hitsound)
			gi.sound(ent, 0, gi.soundindex("world/land.wav"), 1, 1, 0);

		// Move func_pushable riders
		if (ent->movetype == MOVETYPE_PUSHABLE)
		{
			if (ent->bounce_me == 2)
				VectorMA(old_origin, FRAMETIME, ent->velocity, ent->s.origin);

			vec3_t move;
			VectorSubtract(ent->s.origin, old_origin, move);

			edict_t *e = g_edicts + 1;
			for (int i = 1; i < globals.num_edicts; i++, e++)
			{
				if (e == ent)
					continue;

				if (e->groundentity == ent)
				{
					vec3_t end;
					VectorAdd(e->s.origin, move, end);

					const trace_t tr = gi.trace(e->s.origin, e->mins, e->maxs, end, ent, MASK_SOLID);
					VectorCopy(tr.endpos, e->s.origin);
					gi.linkentity(e);
				}
			}
		}
	}
	else if (ent->movetype == MOVETYPE_PUSHABLE || !strcmp(ent->classname, "func_breakaway")) //Knightmare- also do func_breakaways
	{
		// We run touch function for non-moving func_pushables every frame to see if they are touching, for example, a trigger_mass.
		G_TouchTriggers(ent);
		if (!ent->inuse)
			return;
	}

	// Lazarus: Add falling damage for entities that can be damaged
	if (ent->takedamage == DAMAGE_YES)
	{
		Other_FallingDamage(ent);
		VectorCopy(ent->velocity, ent->oldvelocity);
	}

	if (!oldwaterlevel && ent->waterlevel && !ent->groundentity)
	{
		if (ent->watertype & CONTENTS_MUD)
			gi.sound(ent, CHAN_BODY, gi.soundindex("mud/mud_in2.wav"), 1, ATTN_NORM, 0);
		else if ((ent->watertype & CONTENTS_SLIME) || (ent->watertype & CONTENTS_WATER))
			gi.sound(ent, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_NORM, 0);
	}

	// Regular thinking
	SV_RunThink(ent);
	VectorCopy(ent->velocity, ent->oldvelocity);
}

static int SV_VehicleMove(edict_t *ent, const float time, const int mask)
{
	trace_t trace;

	const int numbumps = 4;
	int blocked = 0;

	vec3_t planes[MAX_CLIP_PLANES];
	int numplanes = 0;

	vec3_t original_velocity;
	VectorCopy(ent->velocity, original_velocity);

	vec3_t primal_velocity;
	VectorCopy(ent->velocity, primal_velocity);
	
	vec3_t xy_velocity;
	VectorCopy(ent->velocity, xy_velocity);
	xy_velocity[2] = 0;
	const float xy_speed = VectorLength(xy_velocity);

	float time_left = time;

	vec3_t origin;
	VectorAdd(ent->s.origin, ent->origin_offset, origin);

	vec3_t maxs;
	VectorCopy(ent->size, maxs);
	VectorScale(maxs, 0.5f, maxs);

	vec3_t mins;
	VectorNegate(maxs, mins);
	mins[2] += 1;

	ent->groundentity = NULL;

	edict_t *ignore = ent;

	vec3_t start;
	VectorCopy(origin, start);

	for (int bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		vec3_t end;
		for (int i = 0; i < 3; i++)
			end[i] = origin[i] + time_left * ent->velocity[i];

retry:

		trace = gi.trace(start, mins, maxs, end, ignore, mask);
		if (trace.ent && trace.ent->movewith_ent == ent)
		{
			ignore = trace.ent;
			VectorCopy(trace.endpos, start);

			goto retry;
		}

		if (trace.allsolid)
		{
			// Entity is trapped in another solid 
			if (trace.ent && (trace.ent->svflags & SVF_MONSTER))
			{
				vec3_t dir;
				VectorSubtract(trace.ent->s.origin, ent->s.origin, dir);
				dir[2] = 0;
				VectorNormalize(dir);
				dir[2] = 0.2f;

				vec3_t new_velocity, new_origin;
				VectorMA(trace.ent->velocity, 32, dir, new_velocity);
				VectorMA(trace.ent->s.origin, FRAMETIME, new_velocity, new_origin);

				trace_t tr = gi.trace(trace.ent->s.origin, trace.ent->mins, trace.ent->maxs, new_origin, trace.ent, MASK_MONSTERSOLID);
				if (tr.fraction == 1.0f)
				{
					VectorCopy(new_origin, trace.ent->s.origin);
					VectorCopy(new_velocity, trace.ent->velocity);
					gi.linkentity(trace.ent);
				}
			}
			else if (trace.ent->client && xy_speed > 0.0f)
			{
				// If player is relatively close to the vehicle move_origin, AND the vehicle is still moving, then most likely 
				// the player just disengaged the vehicle and isn't really trapped. Move player along with vehicle.
				vec3_t forward, left, f1, l1, drive, offset;

				AngleVectors(ent->s.angles, forward, left, NULL);
				VectorScale(forward, ent->move_origin[0], f1);
				VectorScale(left, ent->move_origin[1], l1);
				VectorAdd(ent->s.origin, f1, drive);
				VectorAdd(drive, l1, drive);
				VectorSubtract(drive, trace.ent->s.origin, offset);

				if (fabsf(offset[2]) < 64.0f)
					offset[2] = 0;

				if (VectorLength(offset) < 16.0f)
				{
					VectorAdd(trace.ent->s.origin, end, trace.ent->s.origin);
					VectorSubtract(trace.ent->s.origin, origin, trace.ent->s.origin);
					gi.linkentity(trace.ent);

					goto not_allsolid;
				}
			}

			VectorClear(ent->velocity);
			VectorClear(ent->avelocity);

			return 3;
		}

not_allsolid:

		if (trace.fraction > 0.0f)
		{
			// Actually covered some distance
			VectorCopy(trace.endpos, origin);
			VectorSubtract(origin, ent->origin_offset, ent->s.origin);
			VectorCopy(ent->velocity, original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1.0f)
			break; // Moved the entire distance

		edict_t *hit = trace.ent;

		if (trace.plane.normal[2] > 0.7f)
		{
			blocked |= 1; // Floor

			if (hit->solid == SOLID_BSP)
			{
				ent->groundentity = hit;
				ent->groundentity_linkcount = hit->linkcount;
			}
		}

		if (trace.plane.normal[0] > 0 || trace.plane.normal[1] > 0)
			blocked |= 1;

		if (!trace.plane.normal[2])
			blocked |= 2; // Step

		// Run the impact function
		SV_Impact(ent, &trace);
		if (!ent->inuse)
			break; // Vehicle destroyed

		if (!trace.ent->inuse)
		{
			blocked = 0;
			break;
		}

		if (trace.ent->classname)
		{
			if (ent->owner && (trace.ent->svflags & (SVF_MONSTER | SVF_DEADMONSTER)))
				continue; // Handled in vehicle_touch
			
			if (trace.ent->movetype != MOVETYPE_PUSHABLE)
			{
				// If not a func_pushable, match speeds...
				VectorCopy(trace.ent->velocity, ent->velocity);
			}
			else if (ent->mass && VectorLengthSquared(ent->velocity))
			{
				// Otherwise push func_pushable (if vehicle has mass & is moving).
				const float m = (float)ent->mass / trace.ent->mass;

				for (int i = 0; i < 2; i++)
				{
					const float v = (m * ent->velocity[i] + trace.ent->velocity[i]) / (1.0f + m);
					
					ent->velocity[i] = v;
					trace.ent->velocity[i] = v;
					trace.ent->oldvelocity[i] = v;
				}

				gi.linkentity(trace.ent);
			}
		}

		time_left -= time_left * trace.fraction;

		// Cliped to another plane.
		if (numplanes >= MAX_CLIP_PLANES)
		{
			VectorClear(ent->velocity);
			VectorClear(ent->avelocity);

			return 3;
		}

		// Players, monsters and func_pushables don't block us.
		if (trace.ent->client || trace.ent->svflags & SVF_MONSTER || trace.ent->movetype == MOVETYPE_PUSHABLE)
		{
			blocked = 0;
			continue;
		}

		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		// Modify original_velocity so it parallels all of the clip planes
		int planenum1;
		vec3_t new_velocity;
		for (planenum1 = 0; planenum1 < numplanes; planenum1++)
		{
			ClipVelocity(original_velocity, planes[planenum1], new_velocity, 2);

			int planenum2;
			for (planenum2 = 0; planenum2 < numplanes; planenum2++)
			{
				if (planenum2 != planenum1 && !VectorCompare(planes[planenum1], planes[planenum2]))
				{
					if (DotProduct(new_velocity, planes[planenum2]) < 0.0f)
						break; // Not ok
				}
			}

			if (planenum2 == numplanes)
				break;
		}

		if (planenum1 != numplanes)
		{
			// Go along this plane
			VectorCopy(new_velocity, ent->velocity);
			VectorCopy(new_velocity, ent->oldvelocity);
		}
		else
		{
			// Go along the crease
			// DWH: What the hell does this do?
			if (numplanes != 2)
			{
				ent->moveinfo.state = 0;
				ent->moveinfo.next_speed = 0;
				VectorClear(ent->velocity);
				VectorClear(ent->oldvelocity);
				VectorClear(ent->avelocity);

				return 7;
			}

			vec3_t dir;
			CrossProduct(planes[0], planes[1], dir);
			const float d = DotProduct(dir, ent->velocity);
			VectorScale(dir, d, ent->velocity);
		}

		// If original velocity is against the original velocity, stop dead to avoid tiny occilations in sloping corners
		if (DotProduct(ent->velocity, primal_velocity) <= 0.0f)
		{
			ent->moveinfo.state = 0;
			ent->moveinfo.next_speed = 0;
			VectorClear(ent->velocity);
			VectorClear(ent->oldvelocity);
			VectorClear(ent->avelocity);

			return blocked;
		}
	}

	return blocked;
}

static void SV_Physics_Vehicle(edict_t *ent)
{
	//  See if we're on the ground
	if (!ent->groundentity)
		M_CheckGround(ent);

	edict_t *ground = ent->groundentity;
	SV_CheckVelocity(ent);
	if (ground)
		wasonground = true;

	// Move angles
	VectorMA(ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);

	if (ent->velocity[0] || ent->velocity[1] || ent->velocity[2])
	{
		if (ent->org_size[0])
		{
			// Adjust bounding box for yaw
			const float yaw = ent->s.angles[YAW] * M_PI / 180.0f;
			const float ca = cosf(yaw);
			const float sa = sinf(yaw);

			vec3_t s2;
			VectorCopy(ent->org_size, s2);
			VectorScale(s2, 0.5f, s2);

			vec3_t p[2][2];
			p[0][0][0] = -s2[0] * ca + s2[1] * sa;
			p[0][0][1] = -s2[1] * ca - s2[0] * sa;
			p[0][1][0] =  s2[0] * ca + s2[1] * sa;
			p[0][1][1] = -s2[1] * ca + s2[0] * sa;
			p[1][0][0] = -s2[0] * ca - s2[1] * sa;
			p[1][0][1] =  s2[1] * ca - s2[0] * sa;
			p[1][1][0] =  s2[0] * ca - s2[1] * sa;
			p[1][1][1] =  s2[1] * ca + s2[0] * sa;

			vec3_t mins;
			mins[0] = min(p[0][0][0], p[0][1][0]);
			mins[0] = min(mins[0], p[1][0][0]);
			mins[0] = min(mins[0], p[1][1][0]);

			mins[1] = min(p[0][0][1], p[0][1][1]);
			mins[1] = min(mins[1], p[1][0][1]);
			mins[1] = min(mins[1], p[1][1][1]);

			vec3_t maxs;
			maxs[0] = max(p[0][0][0], p[0][1][0]);
			maxs[0] = max(maxs[0], p[1][0][0]);
			maxs[0] = max(maxs[0], p[1][1][0]);

			maxs[1] = max(p[0][0][1], p[0][1][1]);
			maxs[1] = max(maxs[1], p[1][0][1]);
			maxs[1] = max(maxs[1], p[1][1][1]);

			ent->size[0] = maxs[0] - mins[0];
			ent->size[1] = maxs[1] - mins[1];

			ent->mins[0] = -ent->size[0] / 2;
			ent->mins[1] = -ent->size[1] / 2;

			ent->maxs[0] =  ent->size[0] / 2;
			ent->maxs[1] =  ent->size[1] / 2;

			gi.linkentity(ent);
		}

		SV_VehicleMove(ent, FRAMETIME, MASK_ALL);
		gi.linkentity(ent);
		G_TouchTriggers(ent);

		if (!ent->inuse)
			return;
	}

	// Regular thinking
	SV_RunThink(ent);
	VectorCopy(ent->velocity, ent->oldvelocity);
}

// Does not change the entities velocity at all
static trace_t SV_DebrisEntity(edict_t *ent, const vec3_t push)
{
	vec3_t start, end;
	VectorCopy(ent->s.origin, start);
	VectorAdd(start, push, end);

	const int mask = (ent->clipmask ? ent->clipmask : MASK_SHOT);

	trace_t trace = gi.trace (start, ent->mins, ent->maxs, end, ent, mask);
	VectorCopy(trace.endpos, ent->s.origin);
	gi.linkentity(ent);

	if (trace.fraction != 1.0f)
	{
		if (trace.surface && (trace.surface->flags & SURF_SKY))
		{
			G_FreeEdict(ent);
			return trace;
		}

		// Touching a player or monster
		if (trace.ent->client || (trace.ent->svflags & SVF_MONSTER))
		{
			// If rock has no mass we really don't care who it hits.
			if (!ent->mass)
				return trace;

			float speed1 = VectorLength(ent->velocity);
			if (speed1 == 0.0f)
				return trace;

			const float speed2 = VectorLength(trace.ent->velocity);

			vec3_t v1;
			VectorCopy(ent->velocity, v1);
			VectorNormalize(v1);

			vec3_t v2;
			VectorCopy(trace.ent->velocity, v2);
			VectorNormalize(v2);

			const float dot = -DotProduct(v1, v2);
			speed1 += speed2 * dot;

			if (speed1 <= 0.0f)
				return trace;

			const float scale = ent->mass / 200.0f * speed1;
			VectorMA(trace.ent->velocity, scale, v1, trace.ent->velocity);
			
			// Take a swag at it... 
			if (speed1 > 100)
			{
				const int damage = (int)(ent->mass * speed1 / 5000.0f);
				if (damage)
					T_Damage(trace.ent, world, world, v1, trace.ent->s.origin, vec3_origin, damage, 0, DAMAGE_NO_KNOCKBACK, MOD_CRUSH);
			}

			if (ent->touch)
				ent->touch(ent, trace.ent, &trace.plane, trace.surface);

			gi.linkentity(trace.ent);
		}
		// Knightmare- if one func_breakaway lands on another one resting on something other than the world, transfer force to the entity below it.
		else if (trace.ent && trace.ent->classname && !strcmp(trace.ent->classname, "func_breakaway") && trace.ent->solid == SOLID_BBOX)
		{
			trace_t	newtrace;
			edict_t *other = trace.ent;

			while (other && other->classname && !strcmp(other->classname, "func_breakaway") && other->solid == SOLID_BBOX)
			{
				vec3_t newstart, newend;
				VectorCopy(other->s.origin, newstart);
				VectorAdd(newstart, push, newend);

				newtrace = gi.trace(newstart, other->mins, other->maxs, newend, other, mask);

				if (newtrace.ent)
					other = newtrace.ent;
				else
					break;
			}

			if (other && other != trace.ent)
				SV_Impact(ent, &newtrace);
			else
				SV_Impact(ent, &trace);
		}
		else
		{
			SV_Impact(ent, &trace);
		}
	}

	return trace;
}

// Toss, bounce, and fly movement.  When onground, do nothing.
static void SV_Physics_Debris(edict_t *ent)
{
	// Regular thinking
	SV_RunThink(ent);

	if (ent->velocity[2] > 0)
		ent->groundentity = NULL;

	// Check for the groundentity going away
	if (ent->groundentity && !ent->groundentity->inuse)
		ent->groundentity = NULL;

	// If onground, return without moving
	if (ent->groundentity)
		return;

	vec3_t old_origin;
	VectorCopy(ent->s.origin, old_origin);
	SV_CheckVelocity(ent);
	SV_AddGravity(ent);

	// Move angles
	VectorMA(ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles); //Knightmare- avelocity of target angle breakaway is constant

	// Move origin
	vec3_t move;
	VectorScale(ent->velocity, FRAMETIME, move);
	const trace_t trace = SV_DebrisEntity(ent, move);
	if (!ent->inuse)
		return;

	if (trace.fraction < 1.0f)
	{
		const float backoff = 1.0f + ent->attenuation;
		ClipVelocity(ent->velocity, trace.plane.normal, ent->velocity, backoff);

		// Stop if on ground
		if (trace.plane.normal[2] > 0.3f && ent->velocity[2] < 60.0f)
		{
			ent->groundentity = trace.ent;
			ent->groundentity_linkcount = trace.ent->linkcount;
			VectorClear(ent->velocity);
			VectorClear(ent->avelocity);
		}
	}
	
	// Check for water transition
	const qboolean wasinwater = (ent->watertype & MASK_WATER);
	ent->watertype = gi.pointcontents(ent->s.origin);

	const qboolean isinwater = ent->watertype & MASK_WATER;
	ent->waterlevel = (isinwater ? 1 : 0);

	if (!wasinwater && isinwater)
		gi.positioned_sound(old_origin, g_edicts, CHAN_AUTO, gi.soundindex("misc/h2ohit1.wav"), 1, 1, 0);
	else if (wasinwater && !isinwater)
		gi.positioned_sound(ent->s.origin, g_edicts, CHAN_AUTO, gi.soundindex("misc/h2ohit1.wav"), 1, 1, 0);
}

// REAL simple - all we do is check for player riders and adjust their position.
// Only gotcha here is we have to make sure we don't end up embedding player in *another* object that's being moved by the conveyor.
static void SV_Physics_Conveyor(edict_t *ent)
{
	vec3_t v, move;
	VectorScale(ent->movedir, ent->speed, v);
	VectorScale(v, FRAMETIME, move);

	for (int i = 0; i < game.maxclients; i++)
	{
		edict_t *player = g_edicts + 1 + i;

		if (!player->inuse || !player->groundentity || player->groundentity != ent)
			continue;

		// Look below player, make sure he's on a conveyor.
		vec3_t point;
		VectorCopy(player->s.origin, point);
		point[2] += 1.0f;

		vec3_t end;
		VectorCopy(point, end);
		end[2] -= 256.0f;

		trace_t tr = gi.trace(point, player->mins, player->maxs, end, player, MASK_SOLID);

		// tr.ent HAS to be conveyor, but just in case we screwed something up:
		if (tr.ent == ent)
		{
			if (tr.plane.normal[2] > 0.0f)
			{
				v[2] = ent->speed * sqrtf(1.0f - tr.plane.normal[2] * tr.plane.normal[2]) / tr.plane.normal[2];

				if (DotProduct(ent->movedir, tr.plane.normal) > 0.0f) // Then we're moving down
					v[2] = -v[2];

				move[2] = v[2] * FRAMETIME;
			}

			VectorAdd(player->s.origin, move, end);
			tr = gi.trace(player->s.origin, player->mins, player->maxs, end, player, player->clipmask);
			VectorCopy(tr.endpos, player->s.origin);

			gi.linkentity(player);
		}
	}
}

void G_RunEntity(edict_t *ent)
{
	if (level.freeze && Q_stricmp(ent->classname, "chasecam"))
		return;

	if (ent->prethink)
		ent->prethink(ent);

	onconveyor = false;
	wasonground = false;
	blocker = NULL;

	switch (ent->movetype)
	{
		case MOVETYPE_PUSH:
		case MOVETYPE_STOP:
		case MOVETYPE_PENDULUM:
			SV_Physics_Pusher(ent);
			break;

		case MOVETYPE_NONE:
			SV_Physics_None(ent);
			break;

		case MOVETYPE_NOCLIP:
			SV_Physics_Noclip(ent);
			break;

		case MOVETYPE_STEP:
		case MOVETYPE_PUSHABLE:
			SV_Physics_Step(ent);
			break;

		case MOVETYPE_TOSS:
		case MOVETYPE_BOUNCE:
		case MOVETYPE_FLY:
		case MOVETYPE_FLYMISSILE:
		case MOVETYPE_RAIN:
			SV_Physics_Toss(ent);
			break;

		case MOVETYPE_DEBRIS:
			SV_Physics_Debris(ent);
			break;

		case MOVETYPE_VEHICLE:
			SV_Physics_Vehicle(ent);
			break;

		case MOVETYPE_WALK: // Lazarus
			SV_Physics_None(ent); 
			break;

		case MOVETYPE_CONVEYOR:
			SV_Physics_Conveyor(ent);
			break;

		default:
			gi.error("%s: bad movetype %i.", __func__, ent->movetype);			
	}

	if (ent->postthink)	//Knightmare added
		ent->postthink(ent);
}