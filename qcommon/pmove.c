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

#include "qcommon.h"


#define STEPSIZE	18

// All of the locals will be zeroed before each pmove, just to make damn sure we don't have any differences when running on client or server

typedef struct
{
	vec3_t origin; // Full float precision
	vec3_t velocity; // Full float precision

	vec3_t forward;
	vec3_t right;
	float frametime;

	csurface_t *groundsurface;
	cplane_t groundplane;
	int groundcontents;

	vec3_t previous_origin;
	qboolean ladder;
} pml_t;

static pmove_t *pm;
static pml_t pml;

// Knightmare- var for saving speed controls
extern player_state_t *clientstate;

// Movement parameters
// Knightmare- these are the defaults when connected to a legacy server...
static float pm_maxspeed = DEFAULT_MAXSPEED;
static float pm_duckspeed = DEFAULT_DUCKSPEED;
static float pm_waterspeed = DEFAULT_WATERSPEED;
static float pm_accelerate = DEFAULT_ACCELERATE;
static float pm_stopspeed = DEFAULT_STOPSPEED;

static float pm_wateraccelerate = 10;
float pm_airaccelerate = 0;
static float pm_friction = 6;
static float pm_waterfriction = 1;

// Knightmare- this function sets the max speed varibles
static void SetSpeedMax()
{
	pm_wateraccelerate = pm_accelerate;
	
	if (!clientstate) // Defaults, used if not connected
	{
		pm_maxspeed = DEFAULT_MAXSPEED;
		pm_duckspeed = DEFAULT_DUCKSPEED;
		pm_waterspeed = DEFAULT_WATERSPEED;
		pm_accelerate = DEFAULT_ACCELERATE;
		pm_stopspeed = DEFAULT_STOPSPEED;
	}
	else
	{
		pm_maxspeed = nonzero(clientstate->maxspeed, DEFAULT_MAXSPEED);
		pm_duckspeed = nonzero(clientstate->duckspeed, DEFAULT_DUCKSPEED);
		pm_waterspeed = nonzero(clientstate->waterspeed, DEFAULT_WATERSPEED);
		pm_accelerate = nonzero(clientstate->accel, DEFAULT_ACCELERATE);
		pm_stopspeed = nonzero(clientstate->stopspeed, DEFAULT_STOPSPEED);
	}
}

#define STOP_EPSILON	0.1f

// Slide off of the impacting object. Returns the blocked flags (1 = floor, 2 = step / wall).
// Walking up a step should kill some velocity
static void PM_ClipVelocity(const vec3_t in, const vec3_t normal, vec3_t out, const float overbounce)
{
	const float backoff = DotProduct(in, normal) * overbounce;

	for (int i = 0; i < 3; i++)
	{
		const float change = normal[i] * backoff;
		out[i] = in[i] - change;

		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}
}

#define MIN_STEP_NORMAL	0.7f // Can't step up onto very steep slopes
#define MAX_CLIP_PLANES	5

// Each intersection will try to step over the obstruction instead of sliding along it.
// Returns a new origin, velocity, and contact entity.
// Does not modify any world state?
static void PM_StepSlideMove_()
{
	vec3_t planes[MAX_CLIP_PLANES];

	vec3_t primal_velocity;
	VectorCopy(pml.velocity, primal_velocity);
	int numplanes = 0;
	
	float time_left = pml.frametime;

	for (int bumpcount = 0; bumpcount < 4; bumpcount++)
	{
		vec3_t end;
		for (int i = 0; i < 3; i++)
			end[i] = pml.origin[i] + time_left * pml.velocity[i];

		const trace_t trace = pm->trace(pml.origin, pm->mins, pm->maxs, end);

		if (trace.allsolid)
		{
			// Entity is trapped in another solid
			pml.velocity[2] = 0; // Don't build up falling damage
			return;
		}

		if (trace.fraction > 0)
		{
			// Actually covered some distance
			VectorCopy(trace.endpos, pml.origin);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			 break; // Moved the entire distance

		// Save entity for contact
		if (pm->numtouch < MAXTOUCH && trace.ent)
		{
			pm->touchents[pm->numtouch] = trace.ent;
			pm->numtouch++;
		}
		
		time_left -= time_left * trace.fraction;

		// Slide along this plane
		if (numplanes >= MAX_CLIP_PLANES)
		{
			// This shouldn't really happen
			VectorClear(pml.velocity);
			break;
		}

		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		// Modify original_velocity so it parallels all of the clip planes
		int planenum1;
		for (planenum1 = 0; planenum1 < numplanes; planenum1++)
		{
			PM_ClipVelocity(pml.velocity, planes[planenum1], pml.velocity, 1.01f);

			int planenum2;
			for (planenum2 = 0; planenum2 < numplanes; planenum2++)
				if (planenum2 != planenum1 && DotProduct(pml.velocity, planes[planenum2]) < 0)
					break; // Not ok

			if (planenum2 == numplanes)
				break;
		}
		
		if (planenum1 == numplanes)
		{
			// Go along the crease
			if (numplanes != 2)
			{
				VectorClear(pml.velocity);
				break;
			}

			vec3_t dir;
			CrossProduct(planes[0], planes[1], dir);
			const float d = DotProduct(dir, pml.velocity);
			VectorScale(dir, d, pml.velocity);
		}
		
		// If velocity is against the original velocity, stop dead to avoid tiny occilations in sloping corners
		if (DotProduct(pml.velocity, primal_velocity) <= 0)
		{
			VectorClear(pml.velocity);
			break;
		}
	}

	if (pm->s.pm_time)
		VectorCopy(primal_velocity, pml.velocity);
}

static void PM_StepSlideMove()
{
	vec3_t start_o, start_v;
	VectorCopy(pml.origin, start_o);
	VectorCopy(pml.velocity, start_v);

	PM_StepSlideMove_();

	vec3_t down_o, down_v;
	VectorCopy(pml.origin, down_o);
	VectorCopy(pml.velocity, down_v);

	vec3_t up;
	VectorCopy(start_o, up);
	up[2] += STEPSIZE;

	trace_t trace = pm->trace(up, pm->mins, pm->maxs, up);
	if (trace.allsolid)
		return; // Can't step up

	// Try sliding above
	VectorCopy(up, pml.origin);
	VectorCopy(start_v, pml.velocity);

	PM_StepSlideMove_();

	// Push down the final amount
	vec3_t down;
	VectorCopy(pml.origin, down);
	down[2] -= STEPSIZE;

	trace = pm->trace (pml.origin, pm->mins, pm->maxs, down);
	if (!trace.allsolid)
		VectorCopy(trace.endpos, pml.origin);

	VectorCopy(pml.origin, up);

	// Decide which one went farther
	const float down_dist = (down_o[0] - start_o[0]) * (down_o[0] - start_o[0])
						  + (down_o[1] - start_o[1]) * (down_o[1] - start_o[1]);
	const float up_dist = (up[0] - start_o[0]) * (up[0] - start_o[0])
						+ (up[1] - start_o[1]) * (up[1] - start_o[1]);

	if (down_dist > up_dist || trace.plane.normal[2] < MIN_STEP_NORMAL)
	{
		VectorCopy(down_o, pml.origin);
		VectorCopy(down_v, pml.velocity);

		return;
	}

	//!! Special case
	// If we were walking along a plane, then we need to copy the Z over.
	pml.velocity[2] = down_v[2];
}

// Handles both ground friction and water friction
static void PM_Friction()
{
	const float speed = VectorLength(pml.velocity);
	if (speed < 1)
	{
		pml.velocity[0] = 0;
		pml.velocity[1] = 0;

		return;
	}

	float drop = 0;

	// Apply ground friction
	if ((pm->groundentity && pml.groundsurface && !(pml.groundsurface->flags & SURF_SLICK)) || pml.ladder)
	{
		const float control = max(speed, pm_stopspeed);
		drop += control * pm_friction * pml.frametime;
	}

	// Apply water friction
	if (pm->waterlevel && !pml.ladder)
		drop += speed * pm_waterfriction * pm->waterlevel * pml.frametime;

	// Scale the velocity
	float newspeed = max(0, speed - drop);
	newspeed /= speed;

	VectorScale(pml.velocity, newspeed, pml.velocity);
}

// Handles user intended acceleration
static void PM_Accelerate(const vec3_t wishdir, const float wishspeed, const float accel)
{
	const float currentspeed = DotProduct(pml.velocity, wishdir);
	const float addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;

	float accelspeed = accel * pml.frametime * wishspeed;
	accelspeed = min(addspeed, accelspeed);
	
	for (int i = 0; i < 3; i++)
		pml.velocity[i] += accelspeed * wishdir[i];	
}

static void PM_AirAccelerate(const vec3_t wishdir, const float wishspeed, const float accel)
{
	const float wishspd = min(30, wishspeed);
	const float currentspeed = DotProduct(pml.velocity, wishdir);
	const float addspeed = wishspd - currentspeed;

	if (addspeed <= 0)
		return;

	float accelspeed = accel * wishspeed * pml.frametime;
	accelspeed = min(addspeed, accelspeed);
	
	for (int i = 0; i < 3; i++)
		pml.velocity[i] += accelspeed * wishdir[i];
}

static void PM_AddCurrents(vec3_t wishvel)
{
	// Account for ladders
	if (pml.ladder && fabs(pml.velocity[2]) <= 200)
	{
		if (pm->viewangles[PITCH] <= -15 && pm->cmd.forwardmove > 0)
			wishvel[2] = 200;
		else if (pm->viewangles[PITCH] >= 15 && pm->cmd.forwardmove > 0)
			wishvel[2] = -200;
		else if (pm->cmd.upmove != 0)
			wishvel[2] = 200 * sign(pm->cmd.upmove);
		else
			wishvel[2] = 0;

		// Limit horizontal speed when on a ladder
		wishvel[0] = clamp(wishvel[0], -25, 25);
		wishvel[1] = clamp(wishvel[1], -25, 25);
	}

	// Add water currents
	if (pm->watertype & MASK_CURRENT)
	{
		vec3_t v = { 0, 0, 0 };
		
		if (pm->watertype & CONTENTS_CURRENT_0)
			v[0] += 1;

		if (pm->watertype & CONTENTS_CURRENT_90)
			v[1] += 1;

		if (pm->watertype & CONTENTS_CURRENT_180)
			v[0] -= 1;

		if (pm->watertype & CONTENTS_CURRENT_270)
			v[1] -= 1;

		if (pm->watertype & CONTENTS_CURRENT_UP)
			v[2] += 1;

		if (pm->watertype & CONTENTS_CURRENT_DOWN)
			v[2] -= 1;

		float s = pm_waterspeed;
		if (pm->waterlevel == 1 && pm->groundentity)
			s /= 2;

		VectorMA(wishvel, s, v, wishvel);
	}

	// Add conveyor belt velocities
	if (pm->groundentity)
	{
		vec3_t v = { 0, 0, 0 };

		if (pml.groundcontents & CONTENTS_CURRENT_0)
			v[0] += 1;

		if (pml.groundcontents & CONTENTS_CURRENT_90)
			v[1] += 1;

		if (pml.groundcontents & CONTENTS_CURRENT_180)
			v[0] -= 1;

		if (pml.groundcontents & CONTENTS_CURRENT_270)
			v[1] -= 1;

		if (pml.groundcontents & CONTENTS_CURRENT_UP)
			v[2] += 1;

		if (pml.groundcontents & CONTENTS_CURRENT_DOWN)
			v[2] -= 1;

		VectorMA(wishvel, 100, v, wishvel);
	}
}

static void PM_WaterMove()
{
	vec3_t wishvel;

	// User intentions
	for (int i = 0; i < 3; i++)
		wishvel[i] = pml.forward[i] * pm->cmd.forwardmove + pml.right[i] * pm->cmd.sidemove;

	if (!pm->cmd.forwardmove && !pm->cmd.sidemove && !pm->cmd.upmove)
		wishvel[2] -= 60; // drift towards bottom
	else
		wishvel[2] += pm->cmd.upmove;

	PM_AddCurrents(wishvel);

	vec3_t wishdir;
	VectorCopy(wishvel, wishdir);
	float wishspeed = VectorNormalize(wishdir);

	if (wishspeed > pm_maxspeed)
	{
		VectorScale(wishvel, pm_maxspeed / wishspeed, wishvel);
		wishspeed = pm_maxspeed;
	}

	wishspeed *= 0.5f;

	PM_Accelerate(wishdir, wishspeed, pm_wateraccelerate);
	PM_StepSlideMove();
}

static void PM_AirMove()
{
	vec3_t wishvel;
	
	const float fmove = pm->cmd.forwardmove;
	const float smove = pm->cmd.sidemove;

	for (int i = 0; i < 2; i++)
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	wishvel[2] = 0;

	PM_AddCurrents(wishvel);

	vec3_t wishdir;
	VectorCopy(wishvel, wishdir);
	float wishspeed = VectorNormalize(wishdir);

	// Clamp to server defined max speed
	const float maxspeed = (pm->s.pm_flags & PMF_DUCKED ? pm_duckspeed : pm_maxspeed);

	if (wishspeed > maxspeed)
	{
		VectorScale(wishvel, maxspeed / wishspeed, wishvel);
		wishspeed = maxspeed;
	}
	
	if (pml.ladder)
	{
		PM_Accelerate(wishdir, wishspeed, pm_accelerate);
		if (!wishvel[2])
		{
			if (pml.velocity[2] > 0)
			{
				pml.velocity[2] -= pm->s.gravity * pml.frametime;
				pml.velocity[2] = max(0, pml.velocity[2]);
			}
			else
			{
				pml.velocity[2] += pm->s.gravity * pml.frametime;
				pml.velocity[2] = min(0, pml.velocity[2]);
			}
		}

		PM_StepSlideMove();
	}
	else if (pm->groundentity)
	{
		// Walking on ground
		pml.velocity[2] = 0; //!!! This is before the accel
		PM_Accelerate(wishdir, wishspeed, pm_accelerate);

// PGM	-- fix for negative trigger_gravity fields
		if (pm->s.gravity > 0)
			pml.velocity[2] = 0;
		else
			pml.velocity[2] -= pm->s.gravity * pml.frametime;
// PGM

		if (!pml.velocity[0] && !pml.velocity[1])
			return;

		PM_StepSlideMove();
	}
	else
	{
		// Not on ground, so little effect on velocity
		if (pm_airaccelerate)
			PM_AirAccelerate(wishdir, wishspeed, pm_accelerate);
		else
			PM_Accelerate(wishdir, wishspeed, 1);

		// Add gravity
		pml.velocity[2] -= pm->s.gravity * pml.frametime;
		PM_StepSlideMove();
	}
}

static void PM_CatagorizePosition()
{
	vec3_t point;

	// If the player hull point one unit down is solid, the player is on ground

	// See if standing on something solid
	point[0] = pml.origin[0];
	point[1] = pml.origin[1];
	point[2] = pml.origin[2] - 0.25f;

	if (pml.velocity[2] > 180) //!!ZOID changed from 100 to 180 (ramp accel)
	{
		pm->s.pm_flags &= ~PMF_ON_GROUND;
		pm->groundentity = NULL;
	}
	else
	{
		trace_t trace = pm->trace(pml.origin, pm->mins, pm->maxs, point);
		pml.groundplane = trace.plane;
		pml.groundsurface = trace.surface;
		pml.groundcontents = trace.contents;

		if (!trace.ent || (trace.plane.normal[2] < 0.7f && !trace.startsolid))
		{
			// Try a slightly smaller bounding box - this is to fix getting stuck up on angled walls and not being able to move (like you're stuck in the air).
			vec3_t mins;
			mins[0] = (pm->mins[0] ? pm->mins[0] + 1 : 0);
			mins[1] = (pm->mins[1] ? pm->mins[1] + 1 : 0);
			mins[2] = pm->mins[2];

			vec3_t maxs;
			maxs[0] = (pm->maxs[0] ? pm->maxs[0] - 1 : 0);
			maxs[1] = (pm->maxs[1] ? pm->maxs[1] - 1 : 0);
			maxs[2] = pm->maxs[2];

			trace_t trace2 = pm->trace(pml.origin, mins, maxs, point);
			
			if (!(trace2.plane.normal[2] < 0.7f && !trace2.startsolid))
			{
				memcpy(&trace, &trace2, sizeof(trace));
				pml.groundplane = trace.plane;
				pml.groundsurface = trace.surface;
				pml.groundcontents = trace.contents;
				pm->groundentity = trace.ent;
			}
			else
			{
				//mxd. No dice...
				pm->groundentity = NULL;
				pm->s.pm_flags &= ~PMF_ON_GROUND;
			}
		}
		else
		{
			pm->groundentity = trace.ent;

			// Hitting solid ground will end a waterjump
			if (pm->s.pm_flags & PMF_TIME_WATERJUMP)
			{
				pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
				pm->s.pm_time = 0;
			}

			if (!(pm->s.pm_flags & PMF_ON_GROUND))
			{
				// Just hit the ground
				pm->s.pm_flags |= PMF_ON_GROUND;

				// Don't do landing time if we were just going down a slope
				if (pml.velocity[2] < -200)
				{
					pm->s.pm_flags |= PMF_TIME_LAND;
					
					// Don't allow another jump for a little while
					if (pml.velocity[2] < -400)
						pm->s.pm_time = 25;	
					else
						pm->s.pm_time = 18;
				}
			}
		}

		if (pm->numtouch < MAXTOUCH && trace.ent)
		{
			pm->touchents[pm->numtouch] = trace.ent;
			pm->numtouch++;
		}
	}

	// Get waterlevel, accounting for ducking
	pm->waterlevel = 0;
	pm->watertype = 0;

	const int sample2 = pm->viewheight - pm->mins[2];
	const int sample1 = sample2 / 2;

	point[2] = pml.origin[2] + pm->mins[2] + 1;	
	int cont = pm->pointcontents(point);

	if (cont & MASK_WATER)
	{
		pm->watertype = cont;
		pm->waterlevel = 1;
		point[2] = pml.origin[2] + pm->mins[2] + sample1;
		cont = pm->pointcontents(point);

		if (cont & MASK_WATER)
		{
			pm->waterlevel = 2;
			point[2] = pml.origin[2] + pm->mins[2] + sample2;
			cont = pm->pointcontents(point);
			if (cont & MASK_WATER)
				pm->waterlevel = 3;
		}
	}
}

static void PM_CheckJump()
{
	// Hasn't been long enough since landing to jump again
	if (pm->s.pm_flags & PMF_TIME_LAND)
		return;

	if (pm->cmd.upmove < 10)
	{
		// Not holding jump
		pm->s.pm_flags &= ~PMF_JUMP_HELD;
		return;
	}

	// Must wait for jump to be released
	if (pm->s.pm_flags & PMF_JUMP_HELD)
		return;

	if (pm->s.pm_type == PM_DEAD)
		return;

	if (pm->waterlevel >= 2)
	{
		// Swimming, not jumping
		pm->groundentity = NULL;

		if (pml.velocity[2] <= -300)
			return;

		if (pm->watertype == CONTENTS_WATER)
			pml.velocity[2] = 100;
		else if (pm->watertype == CONTENTS_SLIME)
			pml.velocity[2] = 80;
		else
			pml.velocity[2] = 50;

		return;
	}

	if (pm->groundentity == NULL)
		return; // In air, so no effect

	pm->s.pm_flags |= PMF_JUMP_HELD;

	pm->groundentity = NULL;
	pml.velocity[2] = 270 + max(0, pml.velocity[2]);
}

static void PM_CheckSpecialMovement()
{
	if (pm->s.pm_time)
		return;

	pml.ladder = false;

	// Check for ladder
	vec3_t flatforward;
	VectorSet(flatforward, pml.forward[0], pml.forward[1], 0);
	VectorNormalize(flatforward);

	vec3_t spot;
	VectorMA(pml.origin, 1, flatforward, spot);
	const trace_t trace = pm->trace (pml.origin, pm->mins, pm->maxs, spot);
	if (trace.fraction < 1 && (trace.contents & CONTENTS_LADDER))
		pml.ladder = true;

	// Check for water jump
	if (pm->waterlevel != 2)
		return;

	VectorMA(pml.origin, 30, flatforward, spot);
	spot[2] += 4;
	int cont = pm->pointcontents(spot);
	if (!(cont & CONTENTS_SOLID))
		return;

	spot[2] += 16;
	cont = pm->pointcontents(spot);
	if (cont)
		return;

	// Jump out of water
	VectorScale(flatforward, 50, pml.velocity);
	pml.velocity[2] = 350;

	pm->s.pm_flags |= PMF_TIME_WATERJUMP;
	pm->s.pm_time = 255;
}

static void PM_FlyMove(const qboolean doclip)
{
	pm->viewheight = 22;

	// Friction
	const float speed = VectorLength(pml.velocity);
	if (speed < 1)
	{
		VectorClear(pml.velocity);
	}
	else
	{
		float drop = 0;
		const float friction = pm_friction * 1.5; // Extra friction
		const float control = max(speed, pm_stopspeed);
		drop += control * friction * pml.frametime;

		// Scale the velocity
		float newspeed = max(0, speed - drop);
		newspeed /= speed;

		VectorScale(pml.velocity, newspeed, pml.velocity);
	}

	// Accelerate
	const float fmove = pm->cmd.forwardmove;
	const float smove = pm->cmd.sidemove;
	
	VectorNormalize(pml.forward);
	VectorNormalize(pml.right);

	vec3_t wishvel;
	for (int i = 0; i < 3; i++)
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	wishvel[2] += pm->cmd.upmove;

	vec3_t wishdir;
	VectorCopy(wishvel, wishdir);
	float wishspeed = VectorNormalize(wishdir);

	// Clamp to server defined max speed
	if (wishspeed > pm_maxspeed)
	{
		VectorScale(wishvel, pm_maxspeed / wishspeed, wishvel);
		wishspeed = pm_maxspeed;
	}

	const float currentspeed = DotProduct(pml.velocity, wishdir);
	const float addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;

	float accelspeed = pm_accelerate * pml.frametime * wishspeed;
	accelspeed = min(addspeed, accelspeed);
	
	for (int i = 0; i < 3; i++)
		pml.velocity[i] += accelspeed * wishdir[i];

	if (doclip)
	{
		vec3_t end;
		for (int i = 0; i < 3; i++)
			end[i] = pml.origin[i] + pml.frametime * pml.velocity[i];

		const trace_t trace = pm->trace (pml.origin, pm->mins, pm->maxs, end);
		VectorCopy(trace.endpos, pml.origin);
	}
	else
	{
		// Move
		VectorMA(pml.origin, pml.frametime, pml.velocity, pml.origin);
	}
}

// Sets mins, maxs, and pm->viewheight
static void PM_CheckDuck()
{
	pm->mins[0] = -16;
	pm->mins[1] = -16;

	pm->maxs[0] = 16;
	pm->maxs[1] = 16;

	if (pm->s.pm_type == PM_GIB)
	{
		pm->mins[2] = 0;
		pm->maxs[2] = 16;
		pm->viewheight = 8;

		return;
	}

	pm->mins[2] = -24;

	if (pm->s.pm_type == PM_DEAD)
	{
		pm->s.pm_flags |= PMF_DUCKED;
	}
	else if (pm->cmd.upmove < 0 && (pm->s.pm_flags & PMF_ON_GROUND))
	{
		// Duck
		pm->s.pm_flags |= PMF_DUCKED;
	}
	else if (pm->s.pm_flags & PMF_DUCKED)
	{
		// Stand up if possible
		pm->maxs[2] = 32;
		const trace_t trace = pm->trace(pml.origin, pm->mins, pm->maxs, pml.origin);
		if (!trace.allsolid)
			pm->s.pm_flags &= ~PMF_DUCKED;
	}

	if (pm->s.pm_flags & PMF_DUCKED)
	{
		pm->maxs[2] = 4;
		pm->viewheight = -2;
	}
	else
	{
		pm->maxs[2] = 32;
		pm->viewheight = 22;
	}
}

static void PM_DeadMove()
{
	if (!pm->groundentity)
		return;

	// Extra friction
	const float forward = VectorLength(pml.velocity) - 20;
	if (forward <= 0)
	{
		VectorClear(pml.velocity);
	}
	else
	{
		VectorNormalize(pml.velocity);
		VectorScale(pml.velocity, forward, pml.velocity);
	}
}

static qboolean PM_GoodPosition()
{
	if (pm->s.pm_type == PM_SPECTATOR)
		return true;

	vec3_t origin;
	for (int i = 0; i < 3; i++)
		origin[i] = pm->s.origin[i] * 0.125f;

	const trace_t trace = pm->trace(origin, pm->mins, pm->maxs, origin);
	return !trace.allsolid;
}

// On exit, the origin will have a value that is pre-quantized to the 0.125 precision of the network channel and in a valid position.
static void PM_SnapPosition()
{
	// Try all single bits first
	static int jitterbits[8] = { 0, 4, 1, 2, 3, 5, 6, 7 };

	// Snap velocity to eigths
	for (int i = 0; i < 3; i++)
		pm->s.velocity[i] = (int)(pml.velocity[i] * 8);

	int sign[3];
	for (int i = 0; i < 3; i++)
	{
		sign[i] = (pml.origin[i] >= 0 ? 1 : -1);
		pm->s.origin[i] = (int)(pml.origin[i] * 8);
		if (pm->s.origin[i] * 0.125f == pml.origin[i])
			sign[i] = 0;
	}

	int base[3];
	VectorCopy(pm->s.origin, base);

	// Try all combinations
	for (int j = 0; j < 8; j++)
	{
		const int bits = jitterbits[j];
		VectorCopy(base, pm->s.origin);

		for (int i = 0; i < 3; i++)
			if (bits & (1 << i))
				pm->s.origin[i] += sign[i];

		if (PM_GoodPosition())
			return;
	}

	// Go back to the last position
	VectorCopy(pml.previous_origin, pm->s.origin);
}

static void PM_InitialSnapPosition()
{
	static int offset[] = { 0, -1, 1 };

	int base[3];
	VectorCopy(pm->s.origin, base);

	for (int z = 0; z < 3; z++)
	{
		pm->s.origin[2] = base[2] + offset[z];

		for (int y = 0; y < 3; y++)
		{
			pm->s.origin[1] = base[1] + offset[y];

			for (int x = 0; x < 3; x++)
			{
				pm->s.origin[0] = base[0] + offset[x];

				if (PM_GoodPosition())
				{
					for (int c = 0; c < 3; c++)
						pml.origin[c] = pm->s.origin[c] * 0.125f;

					VectorCopy(pm->s.origin, pml.previous_origin);

					return;
				}
			}
		}
	}

	Com_DPrintf(S_COLOR_RED"Bad InitialSnapPosition\n");
}

static void PM_ClampAngles()
{
	if (pm->s.pm_flags & PMF_TIME_TELEPORT)
	{
		pm->viewangles[YAW] = SHORT2ANGLE(pm->cmd.angles[YAW] + pm->s.delta_angles[YAW]);
		pm->viewangles[PITCH] = 0;
		pm->viewangles[ROLL] = 0;
	}
	else
	{
		// Circularly clamp the angles with deltas
		for (int i = 0; i < 3; i++)
		{
			const short temp = pm->cmd.angles[i] + pm->s.delta_angles[i];
			pm->viewangles[i] = SHORT2ANGLE(temp);
		}

		// Don't let the player look up or down more than 90 degrees
		if (pm->viewangles[PITCH] > 89 && pm->viewangles[PITCH] < 180)
			pm->viewangles[PITCH] = 89;
		else if (pm->viewangles[PITCH] < 271 && pm->viewangles[PITCH] >= 180)
			pm->viewangles[PITCH] = 271;
	}

	AngleVectors(pm->viewangles, pml.forward, pml.right, NULL);
}

// Can be called by either the server or the client
void Pmove(pmove_t *pmove)
{
	pm = pmove;

	// Knightmare- set speed controls here
	SetSpeedMax();

	// Clear results
	pm->numtouch = 0;
	VectorClear(pm->viewangles);
	pm->viewheight = 0;
	pm->groundentity = 0;
	pm->watertype = 0;
	pm->waterlevel = 0;

	// Clear all pmove local vars
	memset(&pml, 0, sizeof(pml));

	// Convert origin and velocity to float values
	for (int i = 0; i < 3; i++)
	{
		pml.origin[i] = pm->s.origin[i] * 0.125f;
		pml.velocity[i] = pm->s.velocity[i] * 0.125f;
	}

	// Save old org in case we get stuck
	VectorCopy(pm->s.origin, pml.previous_origin);

	pml.frametime = pm->cmd.msec * 0.001f;

	PM_ClampAngles();

	if (pm->s.pm_type == PM_SPECTATOR)
	{
		PM_FlyMove(false);
		PM_SnapPosition();

		return;
	}

	if (pm->s.pm_type >= PM_DEAD)
	{
		pm->cmd.forwardmove = 0;
		pm->cmd.sidemove = 0;
		pm->cmd.upmove = 0;
	}

	if (pm->s.pm_type == PM_FREEZE)
		return; // No movement at all

	// Set mins, maxs, and viewheight
	PM_CheckDuck();

	if (pm->snapinitial)
		PM_InitialSnapPosition();

	// Set groundentity, watertype, and waterlevel
	PM_CatagorizePosition();

	if (pm->s.pm_type == PM_DEAD)
		PM_DeadMove();

	PM_CheckSpecialMovement();

	// Drop timing counter
	if (pm->s.pm_time)
	{
		int msec = pm->cmd.msec >> 3;
		if (!msec)
			msec = 1;

		if (msec >= pm->s.pm_time)
		{
			pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
			pm->s.pm_time = 0;
		}
		else
		{
			pm->s.pm_time -= msec;
		}
	}

	if (pm->s.pm_flags & PMF_TIME_TELEPORT)
	{
		// Teleport pause stays exactly in place
	}
	else if (pm->s.pm_flags & PMF_TIME_WATERJUMP)
	{
		// Waterjump has no control, but falls
		pml.velocity[2] -= pm->s.gravity * pml.frametime;

		if (pml.velocity[2] < 0)
		{
			// Cancel as soon as we are falling down again
			pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
			pm->s.pm_time = 0;
		}

		PM_StepSlideMove();
	}
	else
	{
		PM_CheckJump();
		PM_Friction();

		if (pm->waterlevel >= 2)
		{
			PM_WaterMove();
		}
		else
		{
			vec3_t angles;
			VectorCopy(pm->viewangles, angles);

			if (angles[PITCH] > 180)
				angles[PITCH] -= 360;
			angles[PITCH] /= 3;

			AngleVectors(angles, pml.forward, pml.right, NULL);
			PM_AirMove();
		}
	}

	// Set groundentity, watertype and waterlevel for final spot.
	PM_CatagorizePosition();
	PM_SnapPosition();
}