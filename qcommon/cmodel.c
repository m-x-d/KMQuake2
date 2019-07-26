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
// cmodel.c -- model loading

#include "qcommon.h"

typedef struct
{
	cplane_t *plane;
	int children[2]; // Negative numbers are leafs
} cnode_t;

typedef struct
{
	cplane_t *plane;
	mapsurface_t *surface;
} cbrushside_t;

typedef struct
{
	int contents;
	int cluster;
	int area;
	unsigned short firstleafbrush; // Change to int
	unsigned short numleafbrushes; // Change to int
} cleaf_t;

typedef struct
{
	int contents;
	int numsides;
	int firstbrushside;
	int checkcount; // To avoid repeated testings
} cbrush_t;

typedef struct
{
	int numareaportals;
	int firstareaportal;
	int floodnum; // if two areas have equal floodnums, they are connected
	int floodvalid;
} carea_t;

static int checkcount;

static int numbrushsides;
static cbrushside_t map_brushsides[MAX_MAP_BRUSHSIDES];

int numtexinfo;
mapsurface_t map_surfaces[MAX_MAP_TEXINFO];

static int numplanes;
static cplane_t map_planes[MAX_MAP_PLANES + 6]; // Extra for box hull

int numnodes;
static cnode_t map_nodes[MAX_MAP_NODES + 6]; // Extra for box hull

static int numleafs = 1; // Allow leaf funcs to be called without a map
static cleaf_t map_leafs[MAX_MAP_LEAFS];
static int emptyleaf;
//static int solidleaf; //mxd. Never used

static int numleafbrushes;
static unsigned short map_leafbrushes[MAX_MAP_LEAFBRUSHES]; // Change to int

static int numcmodels;
static cmodel_t map_cmodels[MAX_MAP_MODELS];

static int numbrushes;
static cbrush_t map_brushes[MAX_MAP_BRUSHES];

static int numvisibility;
static byte map_visibility[MAX_MAP_VISIBILITY];
static dvis_t *map_vis = (dvis_t *)map_visibility;

static int numentitychars;
static char map_entitystring[MAX_MAP_ENTSTRING];

static int numareas = 1;
static carea_t map_areas[MAX_MAP_AREAS];

static int numareaportals;
static dareaportal_t map_areaportals[MAX_MAP_AREAPORTALS];

static int numclusters = 1;

static mapsurface_t nullsurface;

static qboolean portalopen[MAX_MAP_AREAPORTALS];


cvar_t *map_noareas;

void CM_InitBoxHull(void);
void FloodAreaConnections(void);

int c_pointcontents;
int c_traces;
int c_brush_traces;

#pragma region ======================= MAP LOADING

static void CMod_LoadSubmodels(lump_t *l, byte *data)
{
	dmodel_t *in = (void *)(data + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadSubmodels: funny lump size");

	const int count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no models");
	if (count > MAX_MAP_MODELS)
		Com_Error(ERR_DROP, "Map has too many models");

	numcmodels = count;

	for (int i = 0; i < count; i++, in++)
	{
		cmodel_t *out = &map_cmodels[i];

		for (int j = 0; j < 3; j++)
		{
			// Spread the mins / maxs by a pixel
			out->mins[j] = in->mins[j] - 1;
			out->maxs[j] = in->maxs[j] + 1;
			out->origin[j] = in->origin[j];
		}

		out->headnode = in->headnode;
	}
}

static void CMod_LoadSurfaces(lump_t *l, byte *data)
{
	texinfo_t *in = (void *)(data + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadSurfaces: funny lump size");

	const int count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error(ERR_DROP, "Map with no surfaces");
	if (count > MAX_MAP_TEXINFO)
		Com_Error(ERR_DROP, "Map has too many surfaces");

	numtexinfo = count;
	mapsurface_t *out = map_surfaces;

	for (int i = 0; i < count; i++, in++, out++)
	{
		strncpy(out->c.name, in->texture, sizeof(out->c.name) - 1);
		strncpy(out->rname, in->texture, sizeof(out->rname) - 1);
		out->c.flags = in->flags;
		out->c.value = in->value;
	}
}

static void CMod_LoadNodes(lump_t *l, byte *data)
{
	dnode_t *in = (void *)(data + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadNodes: funny lump size");

	const int count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error(ERR_DROP, "Map has no nodes");
	if (count > MAX_MAP_NODES)
		Com_Error(ERR_DROP, "Map has too many nodes");

	numnodes = count;
	cnode_t *out = map_nodes;

	for (int i = 0; i < count; i++, out++, in++)
	{
		out->plane = map_planes + in->planenum;
		for (int j = 0; j < 2; j++)
			out->children[j] = in->children[j];
	}

}

static void CMod_LoadBrushes(lump_t *l, byte *data)
{
	dbrush_t *in = (void *)(data + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadBrushes: funny lump size");

	const int count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_BRUSHES)
		Com_Error(ERR_DROP, "Map has too many brushes");

	cbrush_t *out = map_brushes;
	numbrushes = count;

	for (int i = 0; i < count; i++, out++, in++)
	{
		out->firstbrushside = in->firstside;
		out->numsides = in->numsides;
		out->contents = in->contents;
	}
}

static void CMod_LoadLeafs(lump_t *l, byte *data)
{
	dleaf_t *in = (void *)(data + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadLeafs: funny lump size");

	const int count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no leafs");
	if (count > MAX_MAP_PLANES)
		Com_Error(ERR_DROP, "Map has too many planes");

	cleaf_t *out = map_leafs;
	numleafs = count;
	numclusters = 0;

	for (int i = 0; i < count; i++, in++, out++)
	{
		out->contents = in->contents;
		out->cluster = in->cluster;
		out->area = in->area;
		out->firstleafbrush = in->firstleafbrush;
		out->numleafbrushes = in->numleafbrushes;

		if (out->cluster >= numclusters)
			numclusters = out->cluster + 1;
	}

	if (map_leafs[0].contents != CONTENTS_SOLID)
		Com_Error(ERR_DROP, "Map leaf 0 is not CONTENTS_SOLID");

	//solidleaf = 0; //mxd. Never used
	emptyleaf = -1;
	for (int i = 1; i < numleafs; i++)
	{
		if (!map_leafs[i].contents)
		{
			emptyleaf = i;
			break;
		}
	}

	if (emptyleaf == -1)
		Com_Error(ERR_DROP, "Map does not have an empty leaf");
}

static void CMod_LoadPlanes(lump_t *l, byte *data)
{
	dplane_t *in = (void *)(data + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadPlanes: funny lump size");

	const int count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no planes");
	if (count > MAX_MAP_PLANES)
		Com_Error(ERR_DROP, "Map has too many planes");

	cplane_t *out = map_planes;
	numplanes = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		int bits = 0;
		for (int j = 0; j < 3; j++)
		{
			out->normal[j] = in->normal[j];
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = in->dist;
		out->type = in->type;
		out->signbits = bits;
	}
}

static void CMod_LoadLeafBrushes(lump_t *l, byte *data)
{
	unsigned short *in = (void *)(data + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadLeafBrushes: funny lump size");

	const int count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error(ERR_DROP, "Map with no planes");
	if (count > MAX_MAP_LEAFBRUSHES)
		Com_Error(ERR_DROP, "Map has too many leafbrushes");

	memcpy(map_leafbrushes, in, count * sizeof(ushort)); //mxd
	numleafbrushes = count;
}

static void CMod_LoadBrushSides(lump_t *l, byte *data)
{
	dbrushside_t *in = (void *)(data + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadBrushSides: funny lump size");

	const int count = l->filelen / sizeof(*in);

	// Need to save space for box planes
	if (count > MAX_MAP_BRUSHSIDES)
		Com_Error(ERR_DROP, "Map has too many planes");

	cbrushside_t *out = map_brushsides;
	numbrushsides = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		out->plane = &map_planes[in->planenum];
		if (in->texinfo >= numtexinfo)
			Com_Error(ERR_DROP, "Bad brushside texinfo");

		out->surface = &map_surfaces[in->texinfo];
	}
}

static void CMod_LoadAreas(lump_t *l, byte *data)
{
	darea_t *in = (void *)(data + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadAreas: funny lump size");

	const int count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_AREAS)
		Com_Error(ERR_DROP, "Map has too many areas");

	carea_t *out = map_areas;
	numareas = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		out->numareaportals = in->numareaportals;
		out->firstareaportal = in->firstareaportal;
		out->floodvalid = 0;
		out->floodnum = 0;
	}
}

static void CMod_LoadAreaPortals(lump_t *l, byte *data)
{
	dareaportal_t *in = (void *)(data + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "CMod_LoadAreaPortals: funny lump size");

	const int count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_AREAS)
		Com_Error(ERR_DROP, "Map has too many areas");

	memcpy(map_areaportals, in, count * sizeof(dareaportal_t));
	numareaportals = count;
}

static void CMod_LoadVisibility(lump_t *l, byte *data)
{
	numvisibility = l->filelen;
	if (l->filelen > MAX_MAP_VISIBILITY)
		Com_Error(ERR_DROP, "Map has too large visibility lump");

	memcpy(map_visibility, data + l->fileofs, l->filelen);
}

static void CMod_LoadEntityString(lump_t *l, byte *data, char *name)
{
	// Knightmare- .ent file support
	if (sv_entfile->integer)
	{
		char s[MAX_QPATH];
		char *buffer = NULL;

		const int nameLen = strlen(name);
		Q_strncpyz(s, name, sizeof(s));

		s[nameLen - 3] = 'e';
		s[nameLen - 2] = 'n';
		s[nameLen - 1] = 't';

		const int bufLen = FS_LoadFile(s, (void **)&buffer);
		if (buffer != NULL && bufLen > 1)
		{
			if (bufLen + 1 > sizeof(map_entitystring)) // jit fix
			{
				Com_Printf("CMod_LoadEntityString: .ent file %s too large: %i > %i.\n", s, bufLen, sizeof(map_entitystring));
				FS_FreeFile(buffer);
			}
			else
			{
				Com_Printf("CMod_LoadEntityString: .ent file %s loaded.\n", s);
				numentitychars = bufLen;
				memcpy(map_entitystring, buffer, bufLen);
				map_entitystring[bufLen] = 0; // jit entity bug - null terminate the entity string! 
				FS_FreeFile(buffer);

				return;
			}
		}
		else if (bufLen != -1) // Catch too-small entfile
		{
			Com_Printf("CMod_LoadEntityString: .ent file %s too small.\n", s);
			FS_FreeFile(buffer);
		}
		// Fall back to bsp entity string if no .ent file loaded
	}
	// end Knightmare

	numentitychars = l->filelen;

	if (l->filelen + 1 > sizeof(map_entitystring)) // jit fix
		Com_Error(ERR_DROP, "Map has too large entity lump");

	memcpy(map_entitystring, data + l->fileofs, l->filelen);
	map_entitystring[l->filelen] = 0; // jit entity bug - null terminate the entity string! 
}

// Loads in the map and all submodels
cmodel_t *CM_LoadMap(char *name, qboolean clientload, unsigned *checksum)
{
	static char map_name[MAX_QPATH] = { 0 }; //mxd. Made local
	static unsigned last_checksum;
	
	map_noareas = Cvar_Get("map_noareas", "0", 0);

	if (name && !strcmp(map_name, name) && (clientload || !Cvar_VariableValue("flushmap")) ) //mxd. Make sure "name" isn't null before using it
	{
		*checksum = last_checksum;
		if (!clientload)
		{
			memset(portalopen, 0, sizeof(portalopen));
			FloodAreaConnections();
		}

		return &map_cmodels[0]; // Still have the right version
	}

	// Free old stuff
	numplanes = 0;
	numnodes = 0;
	numleafs = 0;
	numcmodels = 0;
	numvisibility = 0;
	numentitychars = 0;
	map_entitystring[0] = 0;
	map_name[0] = 0;

	if (!name || !name[0])
	{
		numleafs = 1;
		numclusters = 1;
		numareas = 1;
		*checksum = 0;

		return &map_cmodels[0]; // Cinematic servers won't have anything at all
	}

	// Load the file
	byte *buf;
	const int length = FS_LoadFile(name, (void **)&buf);
	if (!buf)
	{
		Com_Error(ERR_DROP, "Couldn't load %s", name);
		return NULL; //mxd. Silence PVS warning.
	}

	last_checksum = Com_BlockChecksum(buf, length);
	*checksum = last_checksum;

	dheader_t header = *(dheader_t *)buf;
	if (header.version != BSPVERSION)
		Com_Error(ERR_DROP, "CM_LoadMap: %s has wrong version number (%i should be %i)", name, header.version, BSPVERSION);

	// Load into heap
	CMod_LoadSurfaces(&header.lumps[LUMP_TEXINFO], buf);
	CMod_LoadLeafs(&header.lumps[LUMP_LEAFS], buf);
	CMod_LoadLeafBrushes(&header.lumps[LUMP_LEAFBRUSHES], buf);
	CMod_LoadPlanes(&header.lumps[LUMP_PLANES], buf);
	CMod_LoadBrushes(&header.lumps[LUMP_BRUSHES], buf);
	CMod_LoadBrushSides(&header.lumps[LUMP_BRUSHSIDES], buf);
	CMod_LoadSubmodels(&header.lumps[LUMP_MODELS], buf);
	CMod_LoadNodes(&header.lumps[LUMP_NODES], buf);
	CMod_LoadAreas(&header.lumps[LUMP_AREAS], buf);
	CMod_LoadAreaPortals(&header.lumps[LUMP_AREAPORTALS], buf);
	CMod_LoadVisibility(&header.lumps[LUMP_VISIBILITY], buf);
	CMod_LoadEntityString(&header.lumps[LUMP_ENTITIES], buf, name);

	FS_FreeFile(buf);

	CM_InitBoxHull();
	memset(portalopen, 0, sizeof(portalopen));
	FloodAreaConnections();
	Q_strncpyz(map_name, name, sizeof(map_name));

	return &map_cmodels[0];
}

cmodel_t *CM_InlineModel(char *name)
{
	if (!name || name[0] != '*')
	{
		Com_Error(ERR_DROP, "CM_InlineModel: bad name");
		return NULL; //mxd. Silence PVS warning
	}

	const int num = atoi(name + 1);
	if (num < 1 || num >= numcmodels)
	{
		Com_Error(ERR_DROP, "CM_InlineModel: bad number");
		return NULL; //mxd. Silence PVS warning
	}

	return &map_cmodels[num];
}

int CM_NumClusters(void)
{
	return numclusters;
}

int CM_NumInlineModels(void)
{
	return numcmodels;
}

char *CM_EntityString(void)
{
	return map_entitystring;
}

int CM_LeafContents(const int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error(ERR_DROP, "CM_LeafContents: bad number");

	return map_leafs[leafnum].contents;
}

int CM_LeafCluster(const int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error(ERR_DROP, "CM_LeafCluster: bad number");

	return map_leafs[leafnum].cluster;
}

int CM_LeafArea(const int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error(ERR_DROP, "CM_LeafArea: bad number");

	return map_leafs[leafnum].area;
}

#pragma endregion 

static cplane_t *box_planes;
static int box_headnode;

// Set up the planes and nodes so that the six floats of a bounding box can just be stored out and get a proper clipping hull structure.
static void CM_InitBoxHull(void)
{
	box_headnode = numnodes;
	box_planes = &map_planes[numplanes];

	if (numnodes + 6 > MAX_MAP_NODES
		|| numbrushes + 1 > MAX_MAP_BRUSHES
		|| numleafbrushes + 1 > MAX_MAP_LEAFBRUSHES
		|| numbrushsides + 6 > MAX_MAP_BRUSHSIDES
		|| numplanes + 12 > MAX_MAP_PLANES)
	{
		Com_Error(ERR_DROP, "Not enough room for box tree");
	}

	cbrush_t *box_brush = &map_brushes[numbrushes]; //mxd. Made local
	box_brush->numsides = 6;
	box_brush->firstbrushside = numbrushsides;
	box_brush->contents = CONTENTS_MONSTER;

	cleaf_t *box_leaf = &map_leafs[numleafs]; //mxd. Made local
	box_leaf->contents = CONTENTS_MONSTER;
	box_leaf->firstleafbrush = numleafbrushes;
	box_leaf->numleafbrushes = 1;

	map_leafbrushes[numleafbrushes] = numbrushes;

	for (int i = 0; i < 6; i++)
	{
		const int side = i & 1;

		// Brush sides
		cbrushside_t *s = &map_brushsides[numbrushsides + i];
		s->plane = 	map_planes + (numplanes + i * 2 + side);
		s->surface = &nullsurface;

		// Nodes
		cnode_t *c = &map_nodes[box_headnode + i];
		c->plane = map_planes + (numplanes + i * 2);
		c->children[side] = -1 - emptyleaf;
		
		if (i != 5)
			c->children[side ^ 1] = box_headnode + i + 1;
		else
			c->children[side ^ 1] = -1 - numleafs;

		// Planes
		cplane_t *p = &box_planes[i * 2];
		p->type = i >> 1;
		p->signbits = 0;
		VectorClear(p->normal);
		p->normal[i >> 1] = 1;

		p = &box_planes[i * 2 + 1];
		p->type = 3 + (i >> 1);
		p->signbits = 0;
		VectorClear(p->normal);
		p->normal[i >> 1] = -1;
	}	
}

// To keep everything totally uniform, bounding boxes are turned into small BSP trees instead of being compared directly.
int	CM_HeadnodeForBox(const vec3_t mins, const vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -maxs[0];
	box_planes[2].dist = mins[0];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = maxs[1];
	box_planes[5].dist = -maxs[1];
	box_planes[6].dist = mins[1];
	box_planes[7].dist = -mins[1];
	box_planes[8].dist = maxs[2];
	box_planes[9].dist = -maxs[2];
	box_planes[10].dist = mins[2];
	box_planes[11].dist = -mins[2];

	return box_headnode;
}

static int CM_PointLeafnum_r(const vec3_t p, int num)
{
	float d;

	while (num >= 0)
	{
		cnode_t *node = map_nodes + num;
		cplane_t *plane = node->plane;
		
		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct(plane->normal, p) - plane->dist;

		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	c_pointcontents++; // Optimize counter

	return -1 - num;
}

int CM_PointLeafnum(const vec3_t p)
{
	if (!numplanes)
		return 0; // Sound may call this without map loaded

	return CM_PointLeafnum_r(p, 0);
}

static int leaf_count, leaf_maxcount;
static int *leaf_list;
static float *leaf_mins, *leaf_maxs;
static int leaf_topnode;

// Fills in a list of all the leafs touched
static void CM_BoxLeafnums_r(int nodenum)
{
	while (true)
	{
		if (nodenum < 0)
		{
			if (leaf_count >= leaf_maxcount)
				return;

			leaf_list[leaf_count++] = -1 - nodenum;
			return;
		}
	
		cnode_t *node = &map_nodes[nodenum];
		cplane_t *plane = node->plane;
		const int s = BOX_ON_PLANE_SIDE(leaf_mins, leaf_maxs, plane);
		if (s == 1)
		{
			nodenum = node->children[0];
		}
		else if (s == 2)
		{
			nodenum = node->children[1];
		}
		else
		{
			// Go down both
			if (leaf_topnode == -1)
				leaf_topnode = nodenum;

			CM_BoxLeafnums_r(node->children[0]);
			nodenum = node->children[1];
		}
	}
}

static int CM_BoxLeafnums_headnode(vec3_t mins, vec3_t maxs, int *list, int listsize, int headnode, int *topnode)
{
	leaf_list = list;
	leaf_count = 0;
	leaf_maxcount = listsize;
	leaf_mins = mins;
	leaf_maxs = maxs;

	leaf_topnode = -1;

	CM_BoxLeafnums_r(headnode);

	if (topnode)
		*topnode = leaf_topnode;

	return leaf_count;
}

int	CM_BoxLeafnums(vec3_t mins, vec3_t maxs, int *list, const int listsize, int *topnode)
{
	return CM_BoxLeafnums_headnode(mins, maxs, list, listsize, map_cmodels[0].headnode, topnode);
}

int CM_PointContents(const vec3_t p, const int headnode)
{
	if (!numnodes)	// Map not loaded
		return 0;

	const int l = CM_PointLeafnum_r(p, headnode);
	return map_leafs[l].contents;
}

// Handles offseting and rotation of the end points for moving and rotating entities
int	CM_TransformedPointContents(const vec3_t p, const int headnode, const vec3_t origin, vec3_t angles)
{
	vec3_t p_l;
	vec3_t temp;
	vec3_t forward, right, up;

	// Subtract origin offset
	VectorSubtract(p, origin, p_l);

	// Rotate start and end into the models frame of reference
	if (headnode != box_headnode && (angles[0] || angles[1] || angles[2]))
	{
		AngleVectors(angles, forward, right, up);

		VectorCopy(p_l, temp);
		p_l[0] = DotProduct(temp, forward);
		p_l[1] = -DotProduct(temp, right);
		p_l[2] = DotProduct(temp, up);
	}

	const int l = CM_PointLeafnum_r(p_l, headnode);
	return map_leafs[l].contents;
}

#pragma region ======================= BOX TRACING

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(0.03125)

static vec3_t trace_start, trace_end;
static vec3_t trace_mins, trace_maxs;
static vec3_t trace_extents;

static trace_t trace_trace;
static int trace_contents;
static qboolean trace_ispoint; // Optimized case

static void CM_ClipBoxToBrush(const vec3_t mins, const vec3_t maxs, const vec3_t p1, const vec3_t p2, trace_t *trace, cbrush_t *brush)
{
	if (!brush->numsides)
		return;

	float enterfrac = -1;
	float leavefrac = 1;
	cplane_t *clipplane = NULL;

	c_brush_traces++;

	qboolean getout = false;
	qboolean startout = false;
	cbrushside_t *leadside = NULL;

	for (int i = 0; i < brush->numsides; i++)
	{
		float dist;
		cbrushside_t *side = &map_brushsides[brush->firstbrushside + i];
		cplane_t *plane = side->plane;

		// FIXME: special case for axial

		if (!trace_ispoint)
		{
			// General box case. Push the plane out apropriately for mins/maxs
			// FIXME: use signbits into 8 way lookup for each mins/maxs
			vec3_t ofs;
			for (int j = 0; j < 3; j++)
				ofs[j] = (plane->normal[j] < 0 ? maxs[j] : mins[j]);

			dist = DotProduct(ofs, plane->normal);
			dist = plane->dist - dist;
		}
		else
		{
			// Special point case
			dist = plane->dist;
		}

		const float d1 = DotProduct(p1, plane->normal) - dist;
		const float d2 = DotProduct(p2, plane->normal) - dist;

		if (d2 > 0)
			getout = true; // Endpoint is not in solid
		if (d1 > 0)
			startout = true;

		// If completely in front of face, no intersection
		if (d1 > 0 && d2 >= d1)
			return;

		if (d1 <= 0 && d2 <= 0)
			continue;

		// Crosses face
		if (d1 > d2)
		{
			// Enter
			const float f = (d1 - DIST_EPSILON) / (d1 - d2);
			if (f > enterfrac)
			{
				enterfrac = f;
				clipplane = plane;
				leadside = side;
			}
		}
		else
		{
			// Leave
			const float f = (d1 + DIST_EPSILON) / (d1 - d2);
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	if (!startout)
	{
		// Original point was inside brush
		trace->startsolid = true;
		if (!getout)
			trace->allsolid = true;

		return;
	}

	if (enterfrac < leavefrac)
	{
		if (enterfrac > -1 && enterfrac < trace->fraction)
		{
			enterfrac = max(enterfrac, 0);

			trace->fraction = enterfrac;
			trace->plane = *clipplane;
			trace->surface = &(leadside->surface->c);
			trace->contents = brush->contents;
		}
	}
}

static void CM_TestBoxInBrush(const vec3_t mins, const vec3_t maxs, const vec3_t p1, trace_t *trace, cbrush_t *brush)
{
	if (!brush->numsides)
		return;

	for (int i = 0; i < brush->numsides; i++)
	{
		cbrushside_t *side = &map_brushsides[brush->firstbrushside + i];
		cplane_t *plane = side->plane;

		// FIXME: special case for axial
		// General box case. Push the plane out apropriately for mins/maxs
		// FIXME: use signbits into 8 way lookup for each mins/maxs
		vec3_t ofs;
		for (int j = 0; j < 3; j++)
			ofs[j] = (plane->normal[j] < 0 ? maxs[j] : mins[j]);

		float dist = DotProduct(ofs, plane->normal);
		dist = plane->dist - dist;

		const float d1 = DotProduct(p1, plane->normal) - dist;

		// If completely in front of face, no intersection
		if (d1 > 0)
			return;
	}

	// Inside this brush
	trace->startsolid = trace->allsolid = true;
	trace->fraction = 0;
	trace->contents = brush->contents;
}

static void CM_TraceToLeaf(const int leafnum)
{
	cleaf_t *leaf = &map_leafs[leafnum];
	if ( !(leaf->contents & trace_contents))
		return;

	// Trace line against all brushes in the leaf
	for (int k = 0; k < leaf->numleafbrushes; k++)
	{
		const int brushnum = map_leafbrushes[leaf->firstleafbrush + k];
		cbrush_t *b = &map_brushes[brushnum];
		if (b->checkcount == checkcount)
			continue; // Already checked this brush in another leaf

		b->checkcount = checkcount;

		if ( !(b->contents & trace_contents))
			continue;

		CM_ClipBoxToBrush(trace_mins, trace_maxs, trace_start, trace_end, &trace_trace, b);
		if (!trace_trace.fraction)
			return;
	}
}

static void CM_TestInLeaf(const int leafnum)
{
	cleaf_t *leaf = &map_leafs[leafnum];
	if (!(leaf->contents & trace_contents))
		return;

	// Trace line against all brushes in the leaf
	for (int k = 0; k < leaf->numleafbrushes; k++)
	{
		const int brushnum = map_leafbrushes[leaf->firstleafbrush + k];
		cbrush_t *b = &map_brushes[brushnum];
		if (b->checkcount == checkcount)
			continue; // Already checked this brush in another leaf

		b->checkcount = checkcount;

		if ( !(b->contents & trace_contents))
			continue;

		CM_TestBoxInBrush(trace_mins, trace_maxs, trace_start, &trace_trace, b);
		if (!trace_trace.fraction)
			return;
	}
}

static void CM_RecursiveHullCheck(const int num, const float p1f, const float p2f, const vec3_t p1, const vec3_t p2)
{
	float t1, t2, offset;
	float frac, frac2;
	float idist;
	vec3_t mid;
	int side;

	if (trace_trace.fraction <= p1f)
		return; // Already hit something nearer

	// If < 0, we are in a leaf node
	if (num < 0)
	{
		CM_TraceToLeaf(-1 - num);
		return;
	}

	// Find the point distances to the seperating plane and the offset for the size of the box
	cnode_t *node = map_nodes + num;
	cplane_t *plane = node->plane;

	if (plane->type < 3)
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = trace_extents[plane->type];
	}
	else
	{
		t1 = DotProduct(plane->normal, p1) - plane->dist;
		t2 = DotProduct(plane->normal, p2) - plane->dist;

		if (trace_ispoint)
		{
			offset = 0;
		}
		else
		{
			offset = fabsf(trace_extents[0] * plane->normal[0]) +
					 fabsf(trace_extents[1] * plane->normal[1]) +
					 fabsf(trace_extents[2] * plane->normal[2]);
		}
	}

	// See which sides we need to consider
	if (t1 >= offset && t2 >= offset)
	{
		CM_RecursiveHullCheck(node->children[0], p1f, p2f, p1, p2);
		return;
	}

	if (t1 < -offset && t2 < -offset)
	{
		CM_RecursiveHullCheck(node->children[1], p1f, p2f, p1, p2);
		return;
	}

	// Put the crosspoint DIST_EPSILON pixels on the near side
	if (t1 < t2)
	{
		idist = 1.0f / (t1 - t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON) * idist;
		frac =  (t1 - offset + DIST_EPSILON) * idist;
	}
	else if (t1 > t2)
	{
		idist = 1.0f / (t1 - t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON) * idist;
		frac =  (t1 + offset + DIST_EPSILON) * idist;
	}
	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// Move up to the node
	frac = clamp(frac, 0, 1);
	float midf = p1f + (p2f - p1f) * frac;
	for (int i = 0; i < 3; i++)
		mid[i] = p1[i] + frac * (p2[i] - p1[i]);

	CM_RecursiveHullCheck(node->children[side], p1f, midf, p1, mid);

	// Go past the node
	frac2 = clamp(frac2, 0, 1);
	midf = p1f + (p2f - p1f) * frac2;
	for (int i = 0; i < 3; i++)
		mid[i] = p1[i] + frac2 * (p2[i] - p1[i]);

	CM_RecursiveHullCheck(node->children[side ^ 1], midf, p2f, mid, p2);
}

//======================================================================

trace_t CM_BoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const int headnode, const int brushmask)
{
	checkcount++; // For multi-check avoidance
	c_traces++; // For statistics, may be zeroed

	// Fill in a default trace
	memset(&trace_trace, 0, sizeof(trace_trace));
	trace_trace.fraction = 1;
	trace_trace.surface = &nullsurface.c;

	if (!numnodes) // Map not loaded
		return trace_trace;

	trace_contents = brushmask;
	VectorCopy(start, trace_start);
	VectorCopy(end, trace_end);
	VectorCopy(mins, trace_mins); //crashes here
	VectorCopy(maxs, trace_maxs);

	// Check for position test special case
	if (VectorCompare(start, end))
	{
		int leafs[1024];
		vec3_t c1, c2;
		int topnode;

		VectorAdd(start, mins, c1);
		VectorAdd(start, maxs, c2);
		for (int i = 0; i < 3; i++)
		{
			c1[i] -= 1;
			c2[i] += 1;
		}

		const int numleafs = CM_BoxLeafnums_headnode(c1, c2, leafs, 1024, headnode, &topnode);
		for (int i = 0; i < numleafs; i++)
		{
			CM_TestInLeaf(leafs[i]);
			if (trace_trace.allsolid)
				break;
		}

		VectorCopy(start, trace_trace.endpos);
		return trace_trace;
	}

	// Check for point special case
	if(VectorCompare(mins, vec3_origin) && VectorCompare(maxs, vec3_origin))
	{
		trace_ispoint = true;
		VectorClear(trace_extents);
	}
	else
	{
		trace_ispoint = false;
		for(int i = 0; i < 3; i++)
			trace_extents[i] = (-mins[i] > maxs[i] ? -mins[i] : maxs[i]);
	}

	// General sweeping through world
	CM_RecursiveHullCheck(headnode, 0, 1, start, end);

	if (trace_trace.fraction == 1)
	{
		VectorCopy(end, trace_trace.endpos);
	}
	else
	{
		for (int i = 0; i < 3; i++)
			trace_trace.endpos[i] = start[i] + trace_trace.fraction * (end[i] - start[i]);
	}

	return trace_trace;
}

// Handles offseting and rotation of the end points for moving and rotating entities
trace_t CM_TransformedBoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const int headnode, const int brushmask, const vec3_t origin, const vec3_t angles)
{
	vec3_t start_l, end_l;
	vec3_t a;
	vec3_t forward, right, up;
	vec3_t temp;

	// Subtract origin offset
	VectorSubtract(start, origin, start_l);
	VectorSubtract(end, origin, end_l);

	// Rotate start and end into the models frame of reference
	const qboolean rotated = (headnode != box_headnode && (angles[0] || angles[1] || angles[2]));

	if (rotated)
	{
		AngleVectors(angles, forward, right, up);

		VectorCopy(start_l, temp);
		start_l[0] = DotProduct(temp, forward);
		start_l[1] = -DotProduct(temp, right);
		start_l[2] = DotProduct(temp, up);

		VectorCopy(end_l, temp);
		end_l[0] = DotProduct(temp, forward);
		end_l[1] = -DotProduct(temp, right);
		end_l[2] = DotProduct(temp, up);
	}

	// Sweep the box through the model
	trace_t trace = CM_BoxTrace(start_l, end_l, mins, maxs, headnode, brushmask);

	if (rotated && trace.fraction != 1.0f)
	{
		// FIXME: figure out how to do this with existing angles
		VectorNegate(angles, a);
		AngleVectors(a, forward, right, up);

		VectorCopy(trace.plane.normal, temp);
		trace.plane.normal[0] = DotProduct(temp, forward);
		trace.plane.normal[1] = -DotProduct(temp, right);
		trace.plane.normal[2] = DotProduct(temp, up);
	}

	for(int i = 0; i < 3; i++)
		trace.endpos[i] = start[i] + trace.fraction * (end[i] - start[i]);

	return trace;
}

#pragma endregion 

#pragma region ======================= PVS / PHS

static void CM_DecompressVis(byte *in, byte *out)
{
	int row = (numclusters + 7) >> 3;
	byte *out_p = out;

	if (!in || !numvisibility)
	{
		// No vis info, so make all visible
		while (row)
		{
			*out_p++ = 0xff;
			row--;
		}

		return;		
	}

	do
	{
		if (*in)
		{
			*out_p++ = *in++;
			continue;
		}
	
		int c = in[1];
		in += 2;
		if ((out_p - out) + c > row)
		{
			c = row - (out_p - out);
			Com_Printf(S_COLOR_YELLOW"Warning: Vis decompression overrun\n");
		}

		while (c)
		{
			*out_p++ = 0;
			c--;
		}
	} while (out_p - out < row);
}

byte *CM_ClusterPVS(const int cluster)
{
	static byte pvsrow[MAX_MAP_LEAFS / 8]; //mxd. Made local
	
	if (cluster == -1)
		memset(pvsrow, 0, (numclusters + 7) >> 3);
	else
		CM_DecompressVis(map_visibility + map_vis->bitofs[cluster][DVIS_PVS], pvsrow);

	return pvsrow;
}

byte *CM_ClusterPHS(const int cluster)
{
	static byte phsrow[MAX_MAP_LEAFS / 8]; //mxd. Made local
	
	if (cluster == -1)
		memset(phsrow, 0, (numclusters + 7) >> 3);
	else
		CM_DecompressVis(map_visibility + map_vis->bitofs[cluster][DVIS_PHS], phsrow);

	return phsrow;
}

#pragma endregion 

#pragma region ======================= AREAPORTALS

static void FloodArea_r(carea_t *area, const int floodnum, const int floodvalid)
{
	if (area->floodvalid == floodvalid)
	{
		if (area->floodnum == floodnum)
			return;

		Com_Error(ERR_DROP, "FloodArea_r: reflooded");
	}

	area->floodnum = floodnum;
	area->floodvalid = floodvalid;
	dareaportal_t *p = &map_areaportals[area->firstareaportal];

	for (int i = 0; i < area->numareaportals; i++, p++)
		if (portalopen[p->portalnum])
			FloodArea_r(&map_areas[p->otherarea], floodnum, floodvalid);
}

static void FloodAreaConnections(void)
{
	static int floodvalid = 0; //mxd. Made local
	
	// All current floods are now invalid
	floodvalid++;
	int floodnum = 0;

	// Area 0 is not used
	for (int i = 1; i < numareas; i++)
	{
		carea_t *area = &map_areas[i];
		if (area->floodvalid == floodvalid)
			continue; // Already flooded into

		floodnum++;
		FloodArea_r(area, floodnum, floodvalid);
	}
}

void CM_SetAreaPortalState(const int portalnum, qboolean open)
{
	if (portalnum > numareaportals)
		Com_Error(ERR_DROP, "areaportal > numareaportals");

	portalopen[portalnum] = open;
	FloodAreaConnections();
}

qboolean CM_AreasConnected(const int area1, const int area2)
{
	if (map_noareas->value)
		return true;

	if (area1 > numareas || area2 > numareas)
		Com_Error(ERR_DROP, "area > numareas");

	if (map_areas[area1].floodnum == map_areas[area2].floodnum)
		return true;

	return false;
}

// Writes a length byte followed by a bit vector of all the areas that area in the same flood as the area parameter.
// This is used by the client refreshes to cull visibility.
int CM_WriteAreaBits(byte *buffer, const int area)
{
	const int bytes = (numareas + 7) >> 3;

	if (map_noareas->value)
	{
		// For debugging, send everything
		memset(buffer, 255, bytes);
	}
	else
	{
		memset(buffer, 0, bytes);

		const int floodnum = map_areas[area].floodnum;
		for (int i = 0; i < numareas; i++)
			if (map_areas[i].floodnum == floodnum || !area)
				buffer[i >> 3] |= 1 << (i & 7);
	}

	return bytes;
}

// Writes the portal state to a savegame file
void CM_WritePortalState(FILE *f)
{
	fwrite(portalopen, sizeof(portalopen), 1, f);
}

// Reads the portal state from a savegame file and recalculates the area connections
void CM_ReadPortalState(const fileHandle_t f)
{
	FS_Read(portalopen, sizeof(portalopen), f);
	FloodAreaConnections();
}

// Returns true if any leaf under headnode has a cluster that is potentially visible
qboolean CM_HeadnodeVisible(const int nodenum, byte *visbits)
{
	if (nodenum < 0)
	{
		const int leafnum = -1 - nodenum;
		const int cluster = map_leafs[leafnum].cluster;

		if (cluster == -1)
			return false;

		if (visbits[cluster >> 3] & 1 << (cluster & 7))
			return true;

		return false;
	}

	cnode_t *node = &map_nodes[nodenum];
	if (CM_HeadnodeVisible(node->children[0], visbits))
		return true;

	return CM_HeadnodeVisible(node->children[1], visbits);
}

#pragma endregion