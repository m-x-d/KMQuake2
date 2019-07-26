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
// world.c -- world query functions

#include "server.h"

#pragma region ======================= Entity area checking
//FIXME: this use of "area" is different from the bsp file use

typedef struct areanode_s
{
	int axis; // -1 = leaf node
	float dist;
	struct areanode_s *children[2];
	link_t trigger_edicts;
	link_t solid_edicts;
} areanode_t;

#define AREA_DEPTH	4
#define AREA_NODES	32

static areanode_t sv_areanodes[AREA_NODES];
static int sv_numareanodes;

static float *area_mins;
static float *area_maxs;
static edict_t **area_list;
static int area_count;
static int area_maxcount;
static int area_type;

int SV_HullForEntity(edict_t *ent);

// ClearLink is used for new headnodes
static void ClearLink(link_t *l)
{
	l->prev = l;
	l->next = l;
}

static void RemoveLink(link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

static void InsertLinkBefore(link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

// Builds a uniformly subdivided tree for the given world size
static areanode_t *SV_CreateAreaNode(const int depth, const vec3_t mins, const vec3_t maxs)
{
	areanode_t *anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink(&anode->trigger_edicts);
	ClearLink(&anode->solid_edicts);
	
	if (depth == AREA_DEPTH)
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;

		return anode;
	}
	
	vec3_t size;
	VectorSubtract(maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;
	
	anode->dist = 0.5f * (maxs[anode->axis] + mins[anode->axis]);

	vec3_t mins1, maxs1, mins2, maxs2;
	VectorCopy(mins, mins1);
	VectorCopy(mins, mins2);
	VectorCopy(maxs, maxs1);
	VectorCopy(maxs, maxs2);
	
	maxs1[anode->axis] = anode->dist;
	mins2[anode->axis] = anode->dist;
	
	anode->children[0] = SV_CreateAreaNode(depth + 1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode(depth + 1, mins1, maxs1);

	return anode;
}

void SV_ClearWorld()
{
	memset(sv_areanodes, 0, sizeof(sv_areanodes));
	sv_numareanodes = 0;
	SV_CreateAreaNode(0, sv.models[1]->mins, sv.models[1]->maxs);
}

void SV_UnlinkEdict(edict_t *ent)
{
	if (ent->area.prev)
	{
		RemoveLink(&ent->area);
		ent->area.prev = NULL;
		ent->area.next = NULL;
	}
}

#define MAX_TOTAL_ENT_LEAFS	128

void SV_LinkEdict(edict_t *ent)
{
	int leafs[MAX_TOTAL_ENT_LEAFS];
	int clusters[MAX_TOTAL_ENT_LEAFS];
	int topnode;

	if (ent->area.prev)
		SV_UnlinkEdict(ent); // Unlink from old position
		
	if (!ent->inuse || ent == ge->edicts)
		return; // Don't add the world

	// Set the size
	VectorSubtract(ent->maxs, ent->mins, ent->size);
	
	// Encode the size into the entity_state for client prediction
	if (ent->solid == SOLID_BBOX && !(ent->svflags & SVF_DEADMONSTER))
	{
		// Assume that x/y are equal and symetric
		const int i = clamp(ent->maxs[0] / 8, 1, 31);
		
		// z is not symetric
		const int j = clamp(-ent->mins[2] / 8, 1, 31);

		// And z maxs can be negative...
		const int k = clamp((ent->maxs[2] + 32) / 8, 1, 63);

		ent->s.solid = (k << 10) | (j << 5) | i;
	}
	else if (ent->solid == SOLID_BSP)
	{
		ent->s.solid = 31; // A solid_bbox will never create this value
	}
	else
	{
		ent->s.solid = 0;
	}

	// Set the abs box
	if (ent->solid == SOLID_BSP && VectorLengthSquared(ent->s.angles) > 0)
	{	
		// Expand for rotation
		float maxsize = 0;
		for (int i = 0; i < 3; i++)
		{
			float v = fabs(ent->mins[i]);
			maxsize = max(v, maxsize);

			v = fabs(ent->maxs[i]);
			maxsize = max(v, maxsize);
		}

		for (int i = 0; i < 3; i++)
		{
			ent->absmin[i] = ent->s.origin[i] - maxsize;
			ent->absmax[i] = ent->s.origin[i] + maxsize;
		}
	}
	else
	{
		// Normal
		VectorAdd(ent->s.origin, ent->mins, ent->absmin);
		VectorAdd(ent->s.origin, ent->maxs, ent->absmax);
	}

	// Because movement is clipped an epsilon away from an actual edge, we must fully check even when bounding boxes don't quite touch
	for (int i = 0; i < 3; i++)
	{
		ent->absmin[i] -= 1;
		ent->absmax[i] += 1;
	}

	// Link to PVS leafs
	ent->num_clusters = 0;
	ent->areanum = 0;
	ent->areanum2 = 0;

	//Get all leafs, including solids
	const int num_leafs = CM_BoxLeafnums(ent->absmin, ent->absmax, leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

	// Set areas
	for (int i = 0; i < num_leafs; i++)
	{
		clusters[i] = CM_LeafCluster(leafs[i]);
		const int area = CM_LeafArea(leafs[i]);
		if (area)
		{
			// Doors may legally straggle two areas, but nothing should evern need more than that
			if (ent->areanum && ent->areanum != area)
			{
				if (ent->areanum2 && ent->areanum2 != area && sv.state == ss_loading)
					Com_DPrintf("Object touching 3 areas at %f %f %f\n", ent->absmin[0], ent->absmin[1], ent->absmin[2]);

				ent->areanum2 = area;
			}
			else
			{
				ent->areanum = area;
			}
		}
	}

	if (num_leafs >= MAX_TOTAL_ENT_LEAFS)
	{
		// Assume we missed some leafs, and mark by headnode
		ent->num_clusters = -1;
		ent->headnode = topnode;
	}
	else
	{
		ent->num_clusters = 0;
		for (int i = 0; i < num_leafs; i++)
		{
			if (clusters[i] == -1)
				continue; // Not a visible leaf

			int j;
			for (j = 0; j < i; j++)
				if (clusters[j] == clusters[i])
					break;

			if (j == i)
			{
				if (ent->num_clusters == MAX_ENT_CLUSTERS)
				{
					// Assume we missed some leafs, and mark by headnode
					ent->num_clusters = -1;
					ent->headnode = topnode;

					break;
				}

				ent->clusternums[ent->num_clusters++] = clusters[i];
			}
		}
	}

	// If first time, make sure old_origin is valid
	if (!ent->linkcount)
		VectorCopy(ent->s.origin, ent->s.old_origin);

	ent->linkcount++;

	if (ent->solid == SOLID_NOT)
		return;

	// Find the first node that the ent's box crosses
	areanode_t *node = sv_areanodes;
	while (true)
	{
		if (node->axis == -1)
			break;

		if (ent->absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break; // Crosses the node
	}
	
	// Link it in
	if (ent->solid == SOLID_TRIGGER)
		InsertLinkBefore(&ent->area, &node->trigger_edicts);
	else
		InsertLinkBefore(&ent->area, &node->solid_edicts);
}

static void SV_AreaEdicts_r(areanode_t *node)
{
	// Touch linked edicts
	link_t *start;
	if (area_type == AREA_SOLID)
		start = &node->solid_edicts;
	else
		start = &node->trigger_edicts;

	link_t *next;
	for (link_t *l = start->next; l != start; l = next)
	{
		next = l->next;
		edict_t *check = (edict_t *)((byte *)l - (int)&(((edict_t *)0)->area)); //mxd. Was EDICT_FROM_AREA

		if (check->solid == SOLID_NOT)
			continue; // Deactivated

		if (check->absmin[0] > area_maxs[0] || check->absmin[1] > area_maxs[1] || check->absmin[2] > area_maxs[2]
		 || check->absmax[0] < area_mins[0] || check->absmax[1] < area_mins[1] || check->absmax[2] < area_mins[2])
			continue; // Not touching

		if (area_count == area_maxcount)
		{
			Com_Printf(S_COLOR_YELLOW"SV_AreaEdicts: MAXCOUNT\n");
			return;
		}

		area_list[area_count] = check;
		area_count++;
	}
	
	if (node->axis == -1)
		return; // Terminal node

	// Recurse down both sides
	if (area_maxs[node->axis] > node->dist)
		SV_AreaEdicts_r(node->children[0]);

	if (area_mins[node->axis] < node->dist)
		SV_AreaEdicts_r(node->children[1]);
}

int SV_AreaEdicts(vec3_t mins, vec3_t maxs, edict_t **list, const int maxcount, const int areatype)
{
	area_mins = mins;
	area_maxs = maxs;
	area_list = list;
	area_count = 0;
	area_maxcount = maxcount;
	area_type = areatype;

	SV_AreaEdicts_r(sv_areanodes);

	return area_count;
}

#pragma endregion

int SV_PointContents(vec3_t p)
{
	edict_t *touch[MAX_EDICTS];

	// Get base contents from world
	int contents = CM_PointContents(p, sv.models[1]->headnode);

	// Or in contents from all the other entities
	const int num = SV_AreaEdicts(p, p, touch, MAX_EDICTS, AREA_SOLID);

	for (int i = 0; i < num; i++)
	{
		edict_t *hit = touch[i];

		// Might intersect, so do an exact clip
		const int headnode = SV_HullForEntity(hit);
		const int c2 = CM_TransformedPointContents(p, headnode, hit->s.origin, hit->s.angles);

		contents |= c2;
	}

	return contents;
}

typedef struct
{
	// Enclose the test object along entire move
	vec3_t boxmins;
	vec3_t boxmaxs; 

	// Size of the moving object
	float *mins;
	float *maxs;

	// Size when clipping against mosnters
	vec3_t mins2;
	vec3_t maxs2;

	float *start;
	float *end;

	trace_t trace;
	edict_t *passedict;

	int contentmask;
} moveclip_t;

// Returns a headnode that can be used for testing or clipping an object of mins/maxs size.
// Offset is filled in to contain the adjustment that must be added to the
// testing object's origin to get a point to use with the returned hull.
int SV_HullForEntity(edict_t *ent)
{
	// Decide which clipping hull to use, based on the size
	if (ent->solid == SOLID_BSP)
	{
		// Explicit hulls in the BSP model
		cmodel_t *model = sv.models[ent->s.modelindex];

		if (!model)
		{
			Com_Error(ERR_FATAL, "MOVETYPE_PUSH with a non-bsp model.");
			return -1; //mxd. Silence PVS warning.
		}

		return model->headnode;
	}

	// Create a temp hull from bounding box sizes
	return CM_HeadnodeForBox(ent->mins, ent->maxs);
}

#pragma region ======================= SV_Trace and its helper methods

static void SV_ClipMoveToEntities(moveclip_t *clip)
{
	edict_t *touchlist[MAX_EDICTS];

	const int num = SV_AreaEdicts(clip->boxmins, clip->boxmaxs, touchlist, MAX_EDICTS, AREA_SOLID);

	// Be careful, it is possible to have an entity in this list removed before we get to it (killtriggered)
	for (int i = 0; i < num; i++)
	{
		edict_t *touch = touchlist[i];

		if (touch->solid == SOLID_NOT || touch == clip->passedict || clip->trace.allsolid)
			continue;

		if (clip->passedict)
		{
			if (touch->owner == clip->passedict)
				continue; // Don't clip against own missiles

			if (clip->passedict->owner == touch)
				continue; // Don't clip against owner
		}

		if (!(clip->contentmask & CONTENTS_DEADMONSTER) && (touch->svflags & SVF_DEADMONSTER))
			continue;

		// Might intersect, so do an exact clip
		const int headnode = SV_HullForEntity(touch);
		float *angles = touch->s.angles;

		if (touch->solid != SOLID_BSP)
			angles = vec3_origin; // Boxes don't rotate

		trace_t trace;
		if (touch->svflags & SVF_MONSTER)
			trace = CM_TransformedBoxTrace(clip->start, clip->end, clip->mins2, clip->maxs2, headnode, clip->contentmask, touch->s.origin, angles);
		else
			trace = CM_TransformedBoxTrace(clip->start, clip->end, clip->mins,  clip->maxs,  headnode, clip->contentmask, touch->s.origin, angles);

		if (trace.allsolid || trace.startsolid || trace.fraction < clip->trace.fraction)
		{
			trace.ent = touch;
			if (clip->trace.startsolid)
				trace.startsolid = true;

			clip->trace = trace;
		}
	}
}

static void SV_TraceBounds(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
	for (int i = 0; i < 3; i++)
	{
		if (end[i] > start[i])
		{
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}

// Moves the given mins/maxs volume through the world from start to end.
// Passedict and edicts owned by passedict are explicitly not checked.
trace_t SV_Trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passedict, const int contentmask)
{
	if (!mins)
		mins = vec3_origin;

	if (!maxs)
		maxs = vec3_origin;

	moveclip_t clip;
	memset(&clip, 0, sizeof(moveclip_t));

	// Clip to world
	clip.trace = CM_BoxTrace(start, end, mins, maxs, 0, contentmask);
	clip.trace.ent = ge->edicts;
	if (clip.trace.fraction == 0)
		return clip.trace; // Blocked by the world

	clip.contentmask = contentmask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passedict = passedict;

	VectorCopy(mins, clip.mins2);
	VectorCopy(maxs, clip.maxs2);
	
	// Create the bounding box of the entire move
	SV_TraceBounds(start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs);

	// Clip to other solid entities
	SV_ClipMoveToEntities(&clip);

	return clip.trace;
}

#pragma endregion