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
// r_fragment.c -- fragment clipping

#include "r_local.h"

#define ON_EPSILON				0.1f

#define MAX_FRAGMENT_POINTS		128
#define MAX_FRAGMENT_PLANES		6

static int cm_numMarkPoints;
static int cm_maxMarkPoints;
static vec3_t *cm_markPoints;

static int cm_numMarkFragments;
static int cm_maxMarkFragments;
static markFragment_t *cm_markFragments;

static cplane_t cm_markPlanes[MAX_FRAGMENT_PLANES];

static int cm_markCheckCount;

static int PlaneTypeForNormal(const vec3_t normal)
{
	if (normal[0] == 1.0f)
		return PLANE_X;

	if (normal[1] == 1.0f)
		return PLANE_Y;

	if (normal[2] == 1.0f)
		return PLANE_Z; 

	return 3; //PLANE_NON_AXIAL;
}

static float *WorldVertex(const int index, msurface_t *surf)
{
	const int edgeindex = r_worldmodel->surfedges[surf->firstedge + index];
	if (edgeindex >= 0)
		return &r_worldmodel->vertexes[r_worldmodel->edges[edgeindex].v[0]].position[0];
	else
		return &r_worldmodel->vertexes[r_worldmodel->edges[-edgeindex].v[1]].position[0];
}

static void R_ClipFragment(int numpoints, vec3_t points, const int stage, markFragment_t *mf)
{
	vec3_t front[MAX_FRAGMENT_POINTS];
	float dists[MAX_FRAGMENT_POINTS];
	int sides[MAX_FRAGMENT_POINTS];

	if (numpoints > MAX_FRAGMENT_POINTS - 2)
		VID_Error(ERR_DROP, "%s: MAX_FRAGMENT_POINTS hit", __func__);

	if (stage == MAX_FRAGMENT_PLANES)
	{
		// Fully clipped
		if (numpoints > 2)
		{
			mf->numPoints = numpoints;
			mf->firstPoint = cm_numMarkPoints;
			
			if (cm_numMarkPoints + numpoints > cm_maxMarkPoints)
				numpoints = cm_maxMarkPoints - cm_numMarkPoints;

			float *point = points;
			for (int i = 0; i < numpoints; i++, point += 3)
				VectorCopy(point, cm_markPoints[cm_numMarkPoints + i]);

			cm_numMarkPoints += numpoints;
		}

		return;
	}

	qboolean frontside = false;

	cplane_t *plane = &cm_markPlanes[stage];
	float *point = points;
	for (int i = 0; i < numpoints; i++, point += 3)
	{
		if (plane->type < 3)
			dists[i] = point[plane->type] - plane->dist;
		else
			dists[i] = DotProduct(point, plane->normal) - plane->dist;

		if (dists[i] > ON_EPSILON)
		{
			frontside = true;
			sides[i] = SIDE_FRONT;
		}
		else if (dists[i] < -ON_EPSILON)
		{
			sides[i] = SIDE_BACK;
		}
		else
		{
			sides[i] = SIDE_ON;
		}
	}

	if (!frontside) // Not clipped
		return;

	// Clip it
	dists[numpoints] = dists[0];
	sides[numpoints] = sides[0];
	VectorCopy(points, (points + (numpoints * 3)));

	int pointindex = 0;
	point = points;
	for (int i = 0; i < numpoints; i++, point += 3)
	{
		switch (sides[i])
		{
			case SIDE_FRONT:
			case SIDE_ON:
				VectorCopy(point, front[pointindex]);
				pointindex++;
				break;

			default:
				break;
		}

		if (sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		const float dist = dists[i] / (dists[i] - dists[i + 1]);

		front[pointindex][0] = point[0] + (point[3] - point[0]) * dist;
		front[pointindex][1] = point[1] + (point[4] - point[1]) * dist;
		front[pointindex][2] = point[2] + (point[5] - point[2]) * dist;

		pointindex++;
	}

	// Continue
	R_ClipFragment(pointindex, front[0], stage + 1, mf);
}

static void R_ClipFragmentToSurface(msurface_t *surf, const vec3_t normal, mnode_t *node)
{
	if (cm_numMarkPoints >= cm_maxMarkPoints || cm_numMarkFragments >= cm_maxMarkFragments) // Already reached the limit somewhere else
		return;

	if (surf->texinfo->flags & SURF_ALPHATEST) // Alpha test surface - no decals
		return;

	const qboolean planeback = surf->flags & SURF_PLANEBACK;
	const float d = DotProduct(normal, surf->plane->normal);
	if ((planeback && d > -0.75f) || (!planeback && d < 0.75f))
		return; // Greater than X degrees

	vec3_t points[MAX_FRAGMENT_POINTS];
	for (int i = 2; i < surf->numedges; i++)
	{
		markFragment_t *mf = &cm_markFragments[cm_numMarkFragments];
		mf->firstPoint = mf->numPoints = 0;
		mf->node = node; // Vis node
		
		VectorCopy(WorldVertex(0, surf), points[0]);
		VectorCopy(WorldVertex(i - 1, surf), points[1]);
		VectorCopy(WorldVertex(i, surf), points[2]);

		R_ClipFragment(3, points[0], 0, mf);

		if (mf->numPoints)
		{
			cm_numMarkFragments++;

			if (cm_numMarkPoints >= cm_maxMarkPoints || cm_numMarkFragments >= cm_maxMarkFragments)
				return;
		}
	}
}

static void R_RecursiveMarkFragments(const vec3_t origin, const vec3_t normal, const float radius, mnode_t *node)
{
	if (cm_numMarkPoints >= cm_maxMarkPoints || cm_numMarkFragments >= cm_maxMarkFragments) // Already reached the limit somewhere else
		return;

	if (node->contents != -1)
		return;

	// Find which side of the node we are on
	float dist;
	cplane_t *plane = node->plane;
	if (plane->type < 3)
		dist = origin[plane->type] - plane->dist;
	else
		dist = DotProduct(origin, plane->normal) - plane->dist;
	
	// Go down the appropriate sides
	if (dist > radius)
	{
		R_RecursiveMarkFragments(origin, normal, radius, node->children[0]);
		return;
	}

	if (dist < -radius)
	{
		R_RecursiveMarkFragments(origin, normal, radius, node->children[1]);
		return;
	}

	// Clip against each surface
	msurface_t *surf = r_worldmodel->surfaces + node->firstsurface;
	for (int i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->checkCount == cm_markCheckCount)
			continue; // Already checked this surface in another node

		if (surf->texinfo->flags & (SURF_SKY | SURF_WARP))
			continue;

		surf->checkCount = cm_markCheckCount;

		R_ClipFragmentToSurface(surf, normal, node);
	}

	// Recurse down the children
	R_RecursiveMarkFragments(origin, normal, radius, node->children[0]);
	R_RecursiveMarkFragments(origin, normal, radius, node->children[1]);
}

int R_MarkFragments(const vec3_t origin, const vec3_t axis[3], float radius, int maxPoints, vec3_t *points, int maxFragments, markFragment_t *fragments)
{
	if (!r_worldmodel || !r_worldmodel->nodes)
		return 0; // Map not loaded

	cm_markCheckCount++; // For multi-check avoidance

	// Initialize fragments
	cm_numMarkPoints = 0;
	cm_maxMarkPoints = maxPoints;
	cm_markPoints = points;

	cm_numMarkFragments = 0;
	cm_maxMarkFragments = maxFragments;
	cm_markFragments = fragments;

	// Calculate clipping planes
	for (int i = 0; i < 3; i++)
	{
		const float dot = DotProduct(origin, axis[i]);

		VectorCopy(axis[i], cm_markPlanes[i * 2 + 0].normal);
		cm_markPlanes[i * 2 + 0].dist = dot - radius;
		cm_markPlanes[i * 2 + 0].type = PlaneTypeForNormal(cm_markPlanes[i * 2 + 0].normal);

		VectorNegate(axis[i], cm_markPlanes[i * 2 + 1].normal);
		cm_markPlanes[i * 2 + 1].dist = -dot - radius;
		cm_markPlanes[i * 2 + 1].type = PlaneTypeForNormal(cm_markPlanes[i * 2 + 1].normal);
	}

	// Clip against world geometry
	R_RecursiveMarkFragments(origin, axis[0], radius, r_worldmodel->nodes);

	return cm_numMarkFragments;
}