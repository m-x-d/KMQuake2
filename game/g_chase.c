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

#include "g_local.h"

void UpdateChaseCam(edict_t *ent)
{
	// Is our chase target gone?
	if (!ent->client->chase_target->inuse || ent->client->chase_target->client->resp.spectator)
	{
		edict_t *old = ent->client->chase_target;
		ChaseNext(ent);

		if (ent->client->chase_target == old)
		{
			ent->client->chase_target = NULL;
			ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;

			return;
		}
	}

	edict_t *targ = ent->client->chase_target;

	vec3_t ownerv;
	VectorCopy(targ->s.origin, ownerv);
	ownerv[2] += targ->viewheight;

	vec3_t oldgoal;
	VectorCopy(ent->s.origin, oldgoal);

	vec3_t angles;
	VectorCopy(targ->client->v_angle, angles);
	angles[PITCH] = min(56, angles[PITCH]);

	vec3_t forward, right;
	AngleVectors(angles, forward, right, NULL);
	VectorNormalize(forward);

	vec3_t o;
	VectorMA(ownerv, -30, forward, o);

	if (o[2] < targ->s.origin[2] + 20)
		o[2] = targ->s.origin[2] + 20;

	// Jump animation lifts
	if (!targ->groundentity)
		o[2] += 16;

	trace_t trace = gi.trace(ownerv, vec3_origin, vec3_origin, o, targ, MASK_SOLID);

	vec3_t goal;
	VectorCopy(trace.endpos, goal);
	VectorMA(goal, 2, forward, goal);

	// Pad for floors and ceilings
	VectorCopy(goal, o);
	o[2] += 6;
	trace = gi.trace(goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
	if (trace.fraction < 1.0f)
	{
		VectorCopy(trace.endpos, goal);
		goal[2] -= 6;
	}

	VectorCopy(goal, o);
	o[2] -= 6;
	trace = gi.trace(goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
	if (trace.fraction < 1.0f)
	{
		VectorCopy(trace.endpos, goal);
		goal[2] += 6;
	}

	if (targ->deadflag)
		ent->client->ps.pmove.pm_type = PM_DEAD;
	else
		ent->client->ps.pmove.pm_type = PM_FREEZE;

	VectorCopy(goal, ent->s.origin);
	for (int i = 0; i < 3; i++)
		ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(targ->client->v_angle[i] - ent->client->resp.cmd_angles[i]);

	if (targ->deadflag)
	{
		ent->client->ps.viewangles[ROLL] = 40;
		ent->client->ps.viewangles[PITCH] = -15;
		ent->client->ps.viewangles[YAW] = targ->client->killer_yaw;
	}
	else
	{
		VectorCopy(targ->client->v_angle, ent->client->ps.viewangles);
		VectorCopy(targ->client->v_angle, ent->client->v_angle);
	}

	ent->viewheight = 0;
	ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

	gi.linkentity(ent);
}

void ChaseNext(edict_t *ent)
{
	if (!ent->client->chase_target)
		return;

	edict_t *e;
	int i = ent->client->chase_target - g_edicts;

	do
	{
		i++;
		if (i > maxclients->integer)
			i = 1;

		e = g_edicts + i;

		if (!e->inuse)
			continue;

		if (!e->client->resp.spectator)
			break;
	} while (e != ent->client->chase_target);

	ent->client->chase_target = e;
	ent->client->update_chase = true;
}

void ChasePrev(edict_t *ent)
{
	if (!ent->client->chase_target)
		return;

	edict_t *e;
	int i = ent->client->chase_target - g_edicts;

	do
	{
		i--;
		if (i < 1)
			i = maxclients->integer;

		e = g_edicts + i;

		if (!e->inuse)
			continue;

		if (!e->client->resp.spectator)
			break;
	} while (e != ent->client->chase_target);

	ent->client->chase_target = e;
	ent->client->update_chase = true;
}

void GetChaseTarget(edict_t *ent)
{
	for (int i = 1; i <= maxclients->integer; i++)
	{
		edict_t *other = g_edicts + i;

		if (other->inuse && !other->client->resp.spectator)
		{
			ent->client->chase_target = other;
			ent->client->update_chase = true;
			UpdateChaseCam(ent);

			return;
		}
	}

	safe_centerprintf(ent, "No other players to chase.");
}