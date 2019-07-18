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

#include "client.h"

void CL_CheckPredictionError()
{
	if (!cl_predict->integer || (cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION))
		return;

	// Calculate the last usercmd_t we sent that the server has processed
	int frame = cls.netchan.incoming_acknowledged;
	frame &= (CMD_BACKUP - 1);

	// Compare what the server returned with what we had predicted it to be
	int delta[3];
	VectorSubtract(cl.frame.playerstate.pmove.origin, cl.predicted_origins[frame], delta);

	// Save the prediction error for interpolation
	const int len = abs(delta[0]) + abs(delta[1]) + abs(delta[2]);
	if (len > 640) // 80 world units
	{
		// A teleport or something
		VectorClear(cl.prediction_error);
	}
	else
	{
		if (cl_showmiss->integer && (delta[0] || delta[1] || delta[2]))
			Com_Printf("prediction miss on %i: %i\n", cl.frame.serverframe, delta[0] + delta[1] + delta[2]);

		VectorCopy(cl.frame.playerstate.pmove.origin, cl.predicted_origins[frame]);

		// Save for error itnerpolation
		for (int i = 0; i < 3; i++)
			cl.prediction_error[i] = delta[i] * 0.125f;
	}
}

static void CL_ClipMoveToEntities(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, trace_t *tr)
{
	int headnode;
	float *angles;

	for (int i = 0; i < cl.frame.num_entities; i++)
	{
		const int num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		entity_state_t *ent = &cl_parse_entities[num];

		if (!ent->solid)
			continue;

		if (ent->number == cl.playernum + 1)
			continue;

		if (ent->solid == 31)
		{
			// Special value for bmodel
			cmodel_t *cmodel = cl.model_clip[ent->modelindex];
			if (!cmodel)
				continue;

			headnode = cmodel->headnode;
			angles = ent->angles;
		}
		else
		{
			// Encoded bbox
			const int x =  8 * (ent->solid & 31);
			const int zd = 8 * ((ent->solid >> 5) & 31);
			const int zu = 8  *((ent->solid >> 10) & 63) - 32;

			vec3_t bmins, bmaxs;
			bmins[0] = bmins[1] = -x;
			bmaxs[0] = bmaxs[1] = x;
			bmins[2] = -zd;
			bmaxs[2] = zu;

			headnode = CM_HeadnodeForBox(bmins, bmaxs);
			angles = vec3_origin; // Boxes don't rotate
		}

		if (tr->allsolid)
			return;

		trace_t trace = CM_TransformedBoxTrace(start, end, mins, maxs, headnode, MASK_PLAYERSOLID, ent->origin, angles);

		if (trace.allsolid || trace.startsolid || trace.fraction < tr->fraction)
		{
			trace.ent = (struct edict_s *)ent;
			if (tr->startsolid)
				trace.startsolid = true;

			*tr = trace;
		}
	}
}

// Similar to above, but uses entnum as reference.
static void CL_ClipMoveToEntities2(const int entnum, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, trace_t *tr)
{
	int headnode;
	float *angles;

	for (int i = 0; i < cl.frame.num_entities ; i++)
	{
		const int num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		entity_state_t *ent = &cl_parse_entities[num];

		if (!ent->solid)
			continue;

		// Don't clip against the passed entity number
		if (ent->number == entnum)
			continue;

		if (ent->solid == 31)
		{
			// Special value for bmodel
			cmodel_t *cmodel = cl.model_clip[ent->modelindex];
			if (!cmodel)
				continue;

			headnode = cmodel->headnode;
			angles = ent->angles;
		}
		else
		{
			// Encoded bbox
			const int x =  8 * (ent->solid & 31);
			const int zd = 8 * ((ent->solid >> 5) & 31);
			const int zu = 8 * ((ent->solid >> 10) & 63) - 32;

			vec3_t bmins, bmaxs;
			bmins[0] = bmins[1] = -x;
			bmaxs[0] = bmaxs[1] = x;
			bmins[2] = -zd;
			bmaxs[2] = zu;

			headnode = CM_HeadnodeForBox(bmins, bmaxs);
			angles = vec3_origin; // Boxes don't rotate
		}

		if (tr->allsolid)
			return;

		trace_t trace = CM_TransformedBoxTrace(start, end, mins, maxs, headnode, MASK_PLAYERSOLID, ent->origin, angles);

		if (trace.allsolid || trace.startsolid || trace.fraction < tr->fraction)
		{
			trace.ent = (struct edict_s *)ent;
			if (tr->startsolid)
				trace.startsolid = true;

			*tr = trace;
		}
	}
}

// Similar to CL_ClipMoveToEntities, but only checks against brush models.
static void CL_ClipMoveToBrushEntities(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, trace_t *tr)
{
	for (int i = 0; i < cl.frame.num_entities; i++)
	{
		const int num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		entity_state_t *ent = &cl_parse_entities[num];

		if (ent->solid != 31) // Brush models only
			continue;

		// Special value for bmodel
		cmodel_t *cmodel = cl.model_clip[ent->modelindex];
		if (!cmodel)
			continue;

		const int headnode = cmodel->headnode;
		float *angles = ent->angles;

		if (tr->allsolid)
			return;

		trace_t trace = CM_TransformedBoxTrace(start, end, mins, maxs, headnode,  MASK_PLAYERSOLID, ent->origin, angles);

		if (trace.allsolid || trace.startsolid || trace.fraction < tr->fraction)
		{
			trace.ent = (struct edict_s *)ent;
			if (tr->startsolid)
				trace.startsolid = true;

			*tr = trace;
		}
	}
}

trace_t CL_Trace(const vec3_t start, const vec3_t end, const float size, const int contentmask)
{
	vec3_t maxs, mins;

	VectorSet(maxs, size, size, size);
	VectorSet(mins, -size, -size, -size);

	return CM_BoxTrace(start, end, mins, maxs, 0, contentmask);
}

// Similar to CL_Trace, but also clips against brush models.
trace_t CL_BrushTrace(const vec3_t start, const vec3_t end, const float size, const int contentmask)
{
	vec3_t maxs, mins;

	VectorSet(maxs, size, size, size);
	VectorSet(mins, -size, -size, -size);

	trace_t t = CM_BoxTrace(start, end, mins, maxs, 0, contentmask);
	if (t.fraction < 1.0f)
		t.ent = (struct edict_s *)1;

	// Check all solid brush models
	CL_ClipMoveToBrushEntities(start, mins, maxs, end, &t);

	return t;
}

trace_t CL_PMTrace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;

	// Check against world
	trace_t t = CM_BoxTrace(start, end, mins, maxs, 0, MASK_PLAYERSOLID);
	if (t.fraction < 1.0f)
		t.ent = (struct edict_s *)1;

	// Check all other solid models
	CL_ClipMoveToEntities(start, mins, maxs, end, &t);

	return t;
}

//Knightmare added- this can check using masks, good for checking surface flags. Also checks for bmodels
trace_t CL_PMSurfaceTrace(const int playernum, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, const int contentmask)
{
	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;

	// Check against world
	trace_t t = CM_BoxTrace(start, end, mins, maxs, 0, contentmask);
	if (t.fraction < 1.0f)
		t.ent = (struct edict_s *)1;

	// Check all other solid models
	CL_ClipMoveToEntities2(playernum, start, mins, maxs, end, &t);

	return t;
}

int CL_PMpointcontents(const vec3_t point)
{
	int contents = CM_PointContents(point, 0);

	for (int i = 0; i < cl.frame.num_entities; i++)
	{
		const int num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		entity_state_t *ent = &cl_parse_entities[num];

		if (ent->solid != 31) // Special value for bmodel
			continue;

		cmodel_t *cmodel = cl.model_clip[ent->modelindex];
		if (!cmodel)
			continue;

		contents |= CM_TransformedPointContents(point, cmodel->headnode, ent->origin, ent->angles);
	}

	return contents;
}

// Modified version of above for ignoring bmodels by Berserker
typedef struct model_s model_t;
int CL_PMpointcontents2(const vec3_t point, const model_t *ignore)
{
	int contents = CM_PointContents(point, 0);

	for (int i = 0; i < cl.frame.num_entities; i++)
	{
		const int num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		entity_state_t *ent = &cl_parse_entities[num];

		if (ent->solid != 31) // Special value for bmodel
			continue;

		if (cl.model_draw[ent->modelindex] == ignore)
			continue;

		cmodel_t *cmodel = cl.model_clip[ent->modelindex];
		if (!cmodel)
			continue;

		contents |= CM_TransformedPointContents(point, cmodel->headnode, ent->origin, ent->angles);
	}

	return contents;
}

// Sets cl.predicted_origin and cl.predicted_angles
void CL_PredictMovement()
{
#ifdef CLIENT_SPLIT_NETFRAME
	static int last_step_frame = 0;
#endif

	if (cls.state != ca_active || cl_paused->integer)
		return;

	if (!cl_predict->integer || (cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION))
	{
		// Just set angles
		for (int i = 0; i < 3; i++)
			cl.predicted_angles[i] = cl.viewangles[i] + SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[i]);

		return;
	}

	int ack = cls.netchan.incoming_acknowledged;
	const int current = cls.netchan.outgoing_sequence;

	// If we are too far out of date, just freeze
	if (current - ack >= CMD_BACKUP)
	{
		if (cl_showmiss->integer)
			Com_Printf(S_COLOR_YELLOW"Exceeded CMD_BACKUP\n");

		return;
	}

	// Copy current state to pmove
	pmove_t pm;
	memset(&pm, 0, sizeof(pm));
	pm.trace = CL_PMTrace;
	pm.pointcontents = CL_PMpointcontents;

	pm_airaccelerate = atof(cl.configstrings[CS_AIRACCEL]);

	pm.s = cl.frame.playerstate.pmove;

#ifdef CLIENT_SPLIT_NETFRAME
	if (cl_async->integer)
	{
		// Run frames
		while (++ack <= current) // Changed '<' to '<=' cause current is our pending cmd
		{
			const int frame = ack & (CMD_BACKUP - 1);
			usercmd_t *cmd = &cl.cmds[frame];

			if (!cmd->msec) // Ignore 'null' usercmd entries
				continue;

			pm.cmd = *cmd;
			Pmove(&pm);

			// Save for debug checking
			VectorCopy(pm.s.origin, cl.predicted_origins[frame]);
		}

		const int oldframe = (ack - 2) & (CMD_BACKUP - 1);
		const int oldz = cl.predicted_origins[oldframe][2];
		const int step = pm.s.origin[2] - oldz;

		// TODO: Add Paril's step down fix here
		if (last_step_frame != current && step > 63 && step < 160 && (pm.s.pm_flags & PMF_ON_GROUND))
		{
			cl.predicted_step = step * 0.125f;
			cl.predicted_step_time = cls.realtime - cls.netFrameTime * 500;
			last_step_frame = current;
		}
	}
	else
	{
#endif // CLIENT_SPLIT_NETFRAME
		// Run frames
		while (++ack < current)
		{
			const int frame = ack & (CMD_BACKUP - 1);
			usercmd_t *cmd = &cl.cmds[frame];

			pm.cmd = *cmd;
			Pmove(&pm);

			// Save for debug checking
			VectorCopy(pm.s.origin, cl.predicted_origins[frame]);
		}

		const int oldframe = (ack - 2) & (CMD_BACKUP - 1);
		const int oldz = cl.predicted_origins[oldframe][2];
		const int step = pm.s.origin[2] - oldz;

		if (step > 63 && step < 160 && (pm.s.pm_flags & PMF_ON_GROUND))
		{
			cl.predicted_step = step * 0.125f;
			cl.predicted_step_time = cls.realtime - cls.netFrameTime * 500;
		}
#ifdef CLIENT_SPLIT_NETFRAME
	}
#endif // CLIENT_SPLIT_NETFRAME

	// Copy results out for rendering
	for(int i = 0; i < 3; i++)
		cl.predicted_origin[i] = pm.s.origin[i] * 0.125f;

	VectorCopy(pm.viewangles, cl.predicted_angles);
}