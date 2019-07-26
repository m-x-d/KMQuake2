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
// r_model.c -- model loading and caching

#include "r_local.h"

void Mod_LoadBrushModel(model_t *mod, void *buffer);

#define MAX_MOD_KNOWN (MAX_MODELS * 2) // Knightmare- was 512
static model_t mod_known[MAX_MOD_KNOWN];
static int mod_numknown;

// The inline * models from the current map are kept seperate
static model_t mod_inline[MAX_MOD_KNOWN];

int registration_sequence;
qboolean registration_active; // Map registration flag

#pragma region ======================= Helper functions

mleaf_t *Mod_PointInLeaf(vec3_t p, model_t *model)
{
	if (!model || !model->nodes)
	{
		VID_Error(ERR_DROP, "Mod_PointInLeaf: bad model");
		return NULL; //mxd. Silence PVS warning.
	}

	mnode_t *node = model->nodes;
	while (true)
	{
		if (node->contents != -1)
			return (mleaf_t *)node;

		cplane_t *plane = node->plane;
		const float d = DotProduct(p, plane->normal) - plane->dist;

		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}
}

static byte *Mod_DecompressVis(byte *in, model_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS / 8];

	int row = (model->vis->numclusters + 7) >> 3;
	byte *out = decompressed;

	if (!in)
	{
		// No vis info, so make all visible
		while (row)
		{
			*out++ = 255;
			row--;
		}

		return decompressed;
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		int c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
	
	return decompressed;
}

byte *Mod_ClusterPVS(int cluster, model_t *model)
{
	static byte novis[MAX_MAP_LEAFS / 8]; //mxd. Made local
	static qboolean initialized = false;
	
	if (cluster == -1 || !model->vis)
	{
		if(!initialized)
		{
			memset(novis, 255, sizeof(novis));
			initialized = true;
		}
		
		return novis;
	}

	return Mod_DecompressVis((byte *)model->vis + model->vis->bitofs[cluster][DVIS_PVS], model);
}

float Mod_RadiusFromBounds(vec3_t mins, vec3_t maxs)
{
	vec3_t corner;
	for (int i = 0; i < 3; i++)
		corner[i] = (fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]));

	return VectorLength(corner);
}

#pragma endregion 

#pragma region ======================= Console functions

//mxd
typedef struct
{
	char *name;
	size_t size;
} modelinfo_t;

//mxd
static int Mod_SortModelinfos(const modelinfo_t *first, const modelinfo_t *second)
{
	return Q_stricmp(first->name, second->name);
}

void Mod_Modellist_f(void)
{
	//mxd. Paranoia check...
	if(mod_numknown == 0)
	{
		Com_Printf(S_COLOR_GREEN"No models loaded.\n");
		return;
	}
	
	//mxd. Collect model infos first...
	modelinfo_t *infos = malloc(sizeof(modelinfo_t) * mod_numknown);
	int numinfos = 0;
	int bytestotal = 0;

	model_t *mod = mod_known;
	for (int i = 0; i < mod_numknown; i++, mod++)
	{
		if (mod->name[0])
		{
			infos[numinfos].name = mod->name;
			infos[numinfos].size = mod->extradatasize;
			numinfos++;

			bytestotal += mod->extradatasize;
		}
	}

	if (numinfos == 0)
	{
		Com_Printf(S_COLOR_GREEN"No models loaded.\n");
		free(infos);
		return;
	}

	//mxd. Sort infos by name
	qsort(infos, numinfos, sizeof(modelinfo_t), (int(*)(const void *, const void *))Mod_SortModelinfos);

	// Print results
	VID_Printf(PRINT_ALL, S_COLOR_GREEN"Loaded models:\n");

	for (int i = 0; i < numinfos; i++)
		VID_Printf(PRINT_ALL, "%7.2f Kb. : %s\n", infos[i].size / 1024.0f, infos[i].name); // Print size in Kb.

	VID_Printf(PRINT_ALL, S_COLOR_GREEN"Total: %i models (%0.2f Mb.).\n", numinfos, bytestotal / (1024.0f * 1024.0f)); // Print size in Mb.

	//mxd. Free memory
	free(infos);
}

#pragma endregion 

#pragma region ======================= .wal size hashing

// Store the names and sizes of .wal files
typedef struct walsize_s
{
	uint hash;
	int width;
	int height;
} walsize_t;

#define NUM_WALSIZES 1024 //mxd. Was 256
static walsize_t walSizeList[NUM_WALSIZES];
static unsigned walSizeListIndex;

static void Mod_InitWalSizeList(void)
{
	for (int i = 0; i < NUM_WALSIZES; i++)
	{
		walSizeList[i].hash = 0;
		walSizeList[i].width = 0;
		walSizeList[i].height = 0;
	}

	walSizeListIndex = 0;
}

static qboolean Mod_CheckWalSizeList(const char *name, int *width, int *height)
{
	if (!strlen(name)) //mxd. Sanity check
	{
		if (width)
			*width = 0;
		if (height)
			*height = 0;

		return true;
	}

	const uint hash = Com_HashFileName(name); //mxd. Rewritten to use hash only
	for (int i = 0; i < NUM_WALSIZES; i++)
	{
		if (hash == walSizeList[i].hash)
		{
			// Return size of texture
			if (width)
				*width = walSizeList[i].width;
			if (height)
				*height = walSizeList[i].height;

			return true;
		}
	}

	return false;
}

static void Mod_AddToWalSizeList(const char *name, int width, int height)
{
	walSizeList[walSizeListIndex].hash = Com_HashFileName(name);
	walSizeList[walSizeListIndex].width = width;
	walSizeList[walSizeListIndex].height = height;
	walSizeListIndex++;

	// Wrap around to start of list
	if (walSizeListIndex >= NUM_WALSIZES)
		walSizeListIndex = 0;
}

// Adapted from Q2E
static void Mod_GetWalSize(const char *name, int *width, int *height)
{
	char path[MAX_QPATH];
	Com_sprintf(path, sizeof(path), "textures/%s.wal", name);

	if (Mod_CheckWalSizeList(name, width, height)) // Check if already in list
		return;

	GetWalInfo(path, width, height); //mxd
	Mod_AddToWalSizeList(name, *width, *height); // Add to list
}

#pragma endregion 

#pragma region ======================= Model loading

void Mod_Init(void)
{
	registration_active = false; // Map registration flag
}

// Loads in a model for the given name
model_t *Mod_ForName(char *name, qboolean crash)
{
	if (!name[0])
		VID_Error(ERR_DROP, "%s: empty name", __func__);

	// Inline models are grabbed only from worldmodel
	if (name[0] == '*')
	{
		const int index = atoi(name + 1);
		if (!r_worldmodel || index < 1 ||  index >= r_worldmodel->numsubmodels)
			VID_Error(ERR_DROP, "%s: bad inline model number: %i", __func__, index);

		return &mod_inline[index];
	}

	// Search the currently loaded models
	model_t *mod = mod_known;
	for (int i = 0; i < mod_numknown; i++, mod++)
	{
		if (!mod->name[0])
			continue;

		if (!strcmp(mod->name, name))
			return mod;
	}
	
	// Find a free model slot spot
	mod = mod_known;
	int mod_index;
	for (mod_index = 0; mod_index < mod_numknown; mod_index++, mod++)
		if (!mod->name[0])
			break; // Free spot

	if (mod_index == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			VID_Error(ERR_DROP, "%s: too many models (%i max.)", __func__, MAX_MOD_KNOWN);

		mod_numknown++;
	}

	Q_strncpyz(mod->name, name, sizeof(mod->name));
	
	// Load the file
	unsigned *buf;
	const int modfilelen = FS_LoadFile(mod->name, (void**)&buf);

	if (!buf)
	{
		if (crash)
			VID_Error(ERR_DROP, "%s: file '%s' does not exist", __func__, mod->name);

		memset(mod->name, 0, sizeof(mod->name));
		return NULL;
	}

	// Fill it in
	switch (buf[0])
	{
		case IDALIASHEADER:
			Mod_LoadAliasMD2Model(mod, buf);
			break;

		case IDMD3HEADER: //Harven++ MD3
			Mod_LoadAliasMD3Model(mod, buf);
			break;
	
		case IDSPRITEHEADER:
			Mod_LoadSpriteModel(mod, buf, modfilelen);
			break;
	
		case IDBSPHEADER:
			Mod_LoadBrushModel(mod, buf);
			break;

		default:
			VID_Error(ERR_DROP, "Mod_NumForName: unknown fileid for %s", mod->name);
			break;
	}

	mod->extradatasize = ModChunk_End();

	FS_FreeFile(buf);

	return mod;
}

#pragma endregion

#pragma region ======================= Brushmodel loading

static void Mod_LoadLighting(model_t *m, byte *data)
{
	lump_t *l = &((dheader_t *)data)->lumps[LUMP_LIGHTING];
	
	if (l->filelen)
	{
		m->lightdata = ModChunk_Alloc(l->filelen);
		memcpy(m->lightdata, data + l->fileofs, l->filelen);
	}
	else
	{
		m->lightdata = NULL;
	}
}

static void Mod_LoadVisibility(model_t *m, byte *data)
{
	lump_t *l = &((dheader_t *)data)->lumps[LUMP_VISIBILITY];
	
	if (l->filelen)
	{
		m->vis = ModChunk_Alloc(l->filelen);
		memcpy(m->vis, data + l->fileofs, l->filelen);
	}
	else
	{
		m->vis = NULL;
	}
}

static void Mod_LoadVertexes(model_t *m, byte *data)
{
	lump_t *l = &((dheader_t *)data)->lumps[LUMP_VERTEXES];
	
	dvertex_t *in = (void *)(data + l->fileofs);
	const int count = l->filelen / sizeof(*in);
	mvertex_t *out = ModChunk_Alloc(count * sizeof(*out));

	m->vertexes = out;
	m->numvertexes = count;

	for (int i = 0; i < count; i++, in++, out++)
		for(int c = 0; c < 3; c++)
			out->position[c] = in->point[c];
}

static void Mod_LoadSubmodels(model_t *m, byte *data)
{
	lump_t *l = &((dheader_t *)data)->lumps[LUMP_MODELS];
	
	dmodel_t *in = (void *)(data + l->fileofs);
	const int count = l->filelen / sizeof(*in);
	mmodel_t *out = ModChunk_Alloc(count * sizeof(*out));

	m->submodels = out;
	m->numsubmodels = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		for (int j = 0; j < 3 ; j++)
		{
			// Spread the mins / maxs by a pixel
			out->mins[j] = in->mins[j] - 1;
			out->maxs[j] = in->maxs[j] + 1;
			out->origin[j] = in->origin[j];
		}

		out->radius = Mod_RadiusFromBounds(out->mins, out->maxs);
		out->headnode = in->headnode;
		out->firstface = in->firstface;
		out->numfaces = in->numfaces;
	}
}

static void Mod_LoadEdges(model_t *m, byte *data)
{
	lump_t *l = &((dheader_t *)data)->lumps[LUMP_EDGES];
	
	dedge_t *in = (void *)(data + l->fileofs);
	const int count = l->filelen / sizeof(*in);
	medge_t *out = ModChunk_Alloc((count + 1) * sizeof(*out));

	m->edges = out;
	m->numedges = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = in->v[0];
		out->v[1] = in->v[1];
	}
}

static void Mod_LoadTexinfo(model_t *m, byte *data)
{
	lump_t *l = &((dheader_t *)data)->lumps[LUMP_TEXINFO];

	texinfo_t *in = (void *)(data + l->fileofs);
	const int count = l->filelen / sizeof(*in);
	mtexinfo_t *out = ModChunk_Alloc(count * sizeof(*out));

	m->texinfo = out;
	m->numtexinfo = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		for (int a = 0; a < 2; a++)
			for (int b = 0; b < 4; b++)
				out->vecs[a][b] = in->vecs[a][b];

		out->flags = in->flags;

		if (in->nexttexinfo > 0)
			out->next = m->texinfo + in->nexttexinfo;
		else
			out->next = NULL;

		char name[MAX_QPATH];
		Com_sprintf(name, sizeof(name), "textures/%s.wal", in->texture);
		out->image = R_FindImage(name, it_wall, false);
		if (!out->image)
			out->image = glMedia.notexture; //mxd. R_FindImage will print a warning message if the texture wasn't found

		// Added glow
		Com_sprintf(name, sizeof(name), "textures/%s_glow.wal", in->texture);
		out->glow = R_FindImage(name, it_skin, true);
		if (!out->glow)
			out->glow = glMedia.notexture;

		//mxd. Load normalmaps...
		R_LoadNormalmap(in->texture, out);
		
		// Q2E HACK: find .wal dimensions for texture coord generation
		// NOTE: Once Q3 map support is added, be sure to disable this
		// for Q3 format maps, because they will be natively textured with hi-res textures.
		Mod_GetWalSize(in->texture, &out->texWidth, &out->texHeight);

		// If no .wal texture was found, use width & height of actual texture
		if (out->texWidth == 0 || out->texHeight == 0)
		{
			out->texWidth = out->image->width; //mxd. Only used in caustics rendering
			out->texHeight = out->image->height;
		}
	}

	// Count animation frames
	for (int i = 0; i < count; i++)
	{
		out = &m->texinfo[i];
		out->numframes = 1;
		for (mtexinfo_t *step = out->next; step && step != out; step = step->next)
			out->numframes++;
	}
}

// Fills in s->texturemins[] and s->extents[]
static void CalcSurfaceExtents(model_t *m, msurface_t *s)
{
	float mins[2], maxs[2];
	mvertex_t *v;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;
	
	mtexinfo_t *tex = s->texinfo;
	
	for (int i = 0; i < s->numedges; i++)
	{
		const int e = m->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &m->vertexes[m->edges[e].v[0]];
		else
			v = &m->vertexes[m->edges[-e].v[1]];
		
		for (int j = 0; j < 2; j++)
		{
			// The following calculation is sensitive to floating-point precision.  It needs to produce the same result that the
			// light compiler does, because R_BuildLightMap uses surf->extents to know the width/height of a surface's lightmap,
			// and incorrect rounding here manifests itself as patches of "corrupted" looking lightmaps.
			// Most light compilers are win32 executables, so they use x87 floating point.  This means the multiplies and adds
			// are done at 80-bit precision, and the result is rounded down to 32-bits and stored in val.
			// Adding the casts to double seems to be good enough to fix lighting glitches when Quakespasm is compiled as x86_64
			// and using SSE2 floating-point.  A potential trouble spot is the hallway at the beginning of mfxsp17.  -- ericw
			const float val = (double)v->position[0] * (double)tex->vecs[j][0] +
							  (double)v->position[1] * (double)tex->vecs[j][1] +
							  (double)v->position[2] * (double)tex->vecs[j][2] +
													   (double)tex->vecs[j][3];

			mins[j] = min(val, mins[j]); //mxd
			maxs[j] = max(val, maxs[j]); //mxd
		}
	}

	for (int i = 0; i < 2; i++)
	{	
		const int bmins = floorf(mins[i] / gl_lms.lmscale); //mxd. 16 -> lmscale
		const int bmaxs =  ceilf(maxs[i] / gl_lms.lmscale);

		s->texturemins[i] = bmins * gl_lms.lmscale;
		s->extents[i] = (bmaxs - bmins) * gl_lms.lmscale;
	}
}

extern void R_BuildPolygonFromSurface(msurface_t *fa);
extern void R_CreateSurfaceLightmap(msurface_t *surf);
extern void R_SetupLightmapPoints(msurface_t *surf); //mxd
extern void R_EndBuildingLightmaps(void);
extern void R_BeginBuildingLightmaps(model_t *m);

static void Mod_LoadFaces(model_t *m, byte *data)
{
	lump_t *l = &((dheader_t *)data)->lumps[LUMP_FACES];
	
	dface_t *in = (void *)(data + l->fileofs);
	const int count = l->filelen / sizeof(*in);
	msurface_t *out = ModChunk_Alloc(count * sizeof(*out));

	m->surfaces = out;
	m->numsurfaces = count;

	currentmodel = m;

	R_BeginBuildingLightmaps(m);

	for (int surfnum = 0; surfnum < count; surfnum++, in++, out++)
	{
		out->firstedge = in->firstedge;
		out->numedges = in->numedges;
		out->flags = 0;
		out->polys = NULL;

		if (in->side)
			out->flags |= SURF_PLANEBACK;

		out->plane = m->planes + in->planenum;

		if (in->texinfo < 0 || in->texinfo >= m->numtexinfo)
			VID_Error(ERR_DROP, "Mod_LoadFaces: bad texinfo number");

		out->texinfo = m->texinfo + in->texinfo;

		CalcSurfaceExtents(m, out);

		// Lighting info
		for (int i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];

		if (in->lightofs == -1)
			out->samples = NULL;
		else
			out->samples = m->lightdata + in->lightofs;
		
		// Set the drawing flags
		if (out->texinfo->flags & SURF_WARP)
		{
			out->flags |= SURF_DRAWTURB;
			for (int i = 0; i < 2; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}

			R_SubdivideSurface(m, out); // Cut up polygon for warps
		}
		// Knightmare- Psychospaz's envmapping. Windows get glass (envmap) effect, warp surfaces don't
		else if (out->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
		{
			if (!(out->texinfo->flags & SURF_NOLIGHTENV))
				out->flags |= SURF_ENVMAP;
		}

		// Create lightmaps and polygons
		R_CreateSurfaceLightmap(out);
		R_SetupLightmapPoints(out); //mxd
		R_BuildPolygonFromSurface(out);
	}

	R_EndBuildingLightmaps();
}

static void Mod_SetParent(mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents == -1)
	{
		Mod_SetParent(node->children[0], node);
		Mod_SetParent(node->children[1], node);
	}
}

static void Mod_LoadNodes(model_t *m, byte *data)
{
	lump_t *l = &((dheader_t *)data)->lumps[LUMP_NODES];
	
	dnode_t *in = (void *)(data + l->fileofs);
	const int count = l->filelen / sizeof(*in);
	mnode_t *out = ModChunk_Alloc(count * sizeof(*out));

	m->nodes = out;
	m->numnodes = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		for (int j = 0; j < 3; j++)
		{
			out->minmaxs[j] = in->mins[j];
			out->minmaxs[j + 3] = in->maxs[j];
		}
	
		out->plane = m->planes + in->planenum;
		out->firstsurface = in->firstface;
		out->numsurfaces =  in->numfaces;
		out->contents = -1; // Differentiate from leafs

		for (int j = 0; j < 2; j++)
		{
			if (in->children[j] >= 0)
				out->children[j] = m->nodes + in->children[j];
			else
				out->children[j] = (mnode_t *)(m->leafs + (-1 - in->children[j]));
		}
	}
	
	Mod_SetParent(m->nodes, NULL); // Sets nodes and leafs
}

static void Mod_LoadLeafs(model_t *m, byte *data)
{
	lump_t *l = &((dheader_t *)data)->lumps[LUMP_LEAFS];
	
	dleaf_t *in = (void *)(data + l->fileofs);
	const int count = l->filelen / sizeof(*in);
	mleaf_t *out = ModChunk_Alloc(count * sizeof(*out));
	
	m->leafs = out;
	m->numleafs = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		for (int j = 0; j < 3; j++)
		{
			out->minmaxs[j] = in->mins[j];
			out->minmaxs[j + 3] = in->maxs[j];
		}

		out->contents = in->contents;
		out->cluster = in->cluster;
		out->area = in->area;

		out->firstmarksurface = m->marksurfaces + in->firstleafface; // Knightmare- make sure this doesn't turn negative!
		out->nummarksurfaces = in->numleaffaces;
		
		// Underwater flag for caustics
		if (out->contents & (MASK_WATER))
		{	
			unsigned int flag;
			if (out->contents & CONTENTS_LAVA)
				flag = SURF_UNDERLAVA;
			else if (out->contents & CONTENTS_SLIME)
				flag = SURF_UNDERSLIME;
			else
				flag = SURF_UNDERWATER;

			for (int j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= flag;
		}
	}	
}

static void Mod_LoadMarksurfaces(model_t *m, byte *data)
{
	lump_t *l = &((dheader_t *)data)->lumps[LUMP_LEAFFACES];
	
	unsigned short *in = (void *)(data + l->fileofs);
	const int count = l->filelen / sizeof(*in);
	msurface_t **out = ModChunk_Alloc(count * sizeof(*out));

	m->marksurfaces = out;
	m->nummarksurfaces = count;

	for (int i = 0; i < count; i++)
	{
		if (in[i] >= m->numsurfaces)
			VID_Error(ERR_DROP, "Mod_ParseMarksurfaces: bad surface number");

		out[i] = m->surfaces + in[i];
	}
}

static void Mod_LoadSurfedges(model_t *m, byte *data)
{
	lump_t *l = &((dheader_t *)data)->lumps[LUMP_SURFEDGES];
	
	int *in = (void *)(data + l->fileofs);
	const int count = l->filelen / sizeof(*in);
	
	const uint size = count * sizeof(int);
	m->surfedges = ModChunk_Alloc(size);
	memcpy(m->surfedges, in, size); //mxd

	m->numsurfedges = count;
}

static void Mod_LoadPlanes(model_t *m, byte *data)
{
	lump_t *l = &((dheader_t *)data)->lumps[LUMP_PLANES];
	
	dplane_t *in = (void *)(data + l->fileofs);
	const int count = l->filelen / sizeof(*in);
	cplane_t *out = ModChunk_Alloc(count * sizeof(*out));
	
	m->planes = out;
	m->numplanes = count;

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

// Adapted from https://github.com/Shpoike/Quakespasm/blob/843ba06637951fee6b81958b8c36c718900b752c/quakespasm/Quake/gl_model.c#L1017
// This just quickly scans the worldspawn entity for a single key. Returning both _prefixed and non prefixed keys.
// (wantkey argument should not have a _prefix.)
static const char *Mod_ParseWorldspawnKey(byte *data, const char *wantkey, char *buffer, size_t sizeofbuffer)
{
	lump_t *l = &((dheader_t *)data)->lumps[LUMP_ENTITIES];
	
	// Get entstring...
	char map_entitystring[MAX_MAP_ENTSTRING];
	if (l->filelen + 1 > sizeof(map_entitystring))
		return NULL;

	memcpy(map_entitystring, data + l->fileofs, l->filelen);
	map_entitystring[l->filelen] = '\0';

	// Try to find wantkey among the first entity props...
	char foundkey[128];
	char *entstr = map_entitystring;
	char *com_token = COM_Parse(&entstr);

	if (entstr && com_token[0] == '{')
	{
		while (true)
		{
			com_token = COM_Parse(&entstr);

			if (!entstr || com_token[0] == '}') // Error or end of worldspawn
				break;

			if (com_token[0] == '_')
				strcpy(foundkey, com_token + 1);
			else
				strcpy(foundkey, com_token);

			com_token = COM_Parse(&entstr);

			if (!entstr) // Error
				break; 

			if (!strcmp(wantkey, foundkey))
			{
				Q_strncpyz(buffer, com_token, sizeofbuffer);
				return buffer;
			}
		}
	}

	return NULL;
}

//mxd
static size_t Mod_GetAllocSizeBrushModel(model_t *model, void *data)
{
	if (model != mod_known)
		VID_Error(ERR_DROP, "Loaded a brush model after the world");
	
	// Header...
	dheader_t *header = (dheader_t *)data;
	if (header->version != BSPVERSION)
		VID_Error(ERR_DROP, "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", model->name, header->version, BSPVERSION);

	size_t allocSize = ALIGN_TO_CACHELINE(sizeof(dheader_t));

	// LUMP_VERTEXES
	lump_t *l = &header->lumps[LUMP_VERTEXES];
	if (l->filelen % sizeof(dvertex_t))
		VID_Error(ERR_DROP, "Mod_LoadVertexes: funny lump size in %s", model->name);

	int count = l->filelen / sizeof(dvertex_t);
	allocSize += ALIGN_TO_CACHELINE(count * sizeof(mvertex_t));

	// LUMP_EDGES
	l = &header->lumps[LUMP_EDGES];
	if (l->filelen % sizeof(dedge_t))
		VID_Error(ERR_DROP, "Mod_LoadEdges: funny lump size in %s", model->name);

	count = l->filelen / sizeof(dedge_t);
	allocSize += ALIGN_TO_CACHELINE((count + 1) * sizeof(medge_t));

	// LUMP_SURFEDGES
	l = &header->lumps[LUMP_SURFEDGES];
	if (l->filelen % sizeof(int))
		VID_Error(ERR_DROP, "Mod_LoadSurfedges: funny lump size in %s", model->name);

	count = l->filelen / sizeof(int);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		VID_Error(ERR_DROP, "Mod_LoadSurfedges: bad surfedges count in %s: %i", model->name, count);

	allocSize += ALIGN_TO_CACHELINE(count * sizeof(int));

	// LUMP_LIGHTING
	l = &header->lumps[LUMP_LIGHTING];
	if (l->filelen > 0)
		allocSize += ALIGN_TO_CACHELINE(l->filelen);

	// LUMP_PLANES
	l = &header->lumps[LUMP_PLANES];
	if (l->filelen % sizeof(dplane_t))
		VID_Error(ERR_DROP, "Mod_LoadPlanes: funny lump size in %s", model->name);

	count = l->filelen / sizeof(dplane_t);
	allocSize += ALIGN_TO_CACHELINE(count * sizeof(cplane_t));

	// LUMP_TEXINFO
	l = &header->lumps[LUMP_TEXINFO];
	if (l->filelen % sizeof(texinfo_t))
		VID_Error(ERR_DROP, "Mod_LoadTexinfo: funny lump size in %s", model->name);

	count = l->filelen / sizeof(texinfo_t);
	allocSize += ALIGN_TO_CACHELINE(count * sizeof(mtexinfo_t));

	// LUMP_FACES
	l = &header->lumps[LUMP_FACES];
	if (l->filelen % sizeof(dface_t))
		VID_Error(ERR_DROP, "Mod_LoadFaces: funny lump size in %s", model->name);

	count = l->filelen / sizeof(dface_t);
	allocSize += ALIGN_TO_CACHELINE(count * sizeof(msurface_t));

	// Now count all glpoly_t allocations...
	dface_t *face = (void *)((byte *)header + l->fileofs);

	l = &header->lumps[LUMP_TEXINFO];
	texinfo_t *texinfo = (void *)((byte *)header + l->fileofs);

	l = &header->lumps[LUMP_VERTEXES];
	dvertex_t *vert = (void *)((byte *)header + l->fileofs);

	l = &header->lumps[LUMP_EDGES];
	dedge_t *edge = (void *)((byte *)header + l->fileofs);

	l = &header->lumps[LUMP_SURFEDGES];
	int *surfedge = (void *)((byte *)header + l->fileofs);

	for(int i = 0; i < count; i++, face++)
	{
		const int flags = texinfo[face->texinfo].flags;
		int facesize;

		if(flags & SURF_WARP)
		{
			facesize = R_GetWarpSurfaceVertsSize(face, vert, edge, surfedge);
		}
		else
		{
			facesize = sizeof(glpoly_t) + (face->numedges - 4) * VERTEXSIZE * sizeof(float);

			if (flags & (SURF_TRANS33 | SURF_TRANS66)) // 2x vertex light fields
				facesize += face->numedges * 6 * sizeof(byte);
		}

		allocSize += ALIGN_TO_CACHELINE(facesize);
	}

	// LUMP_LEAFFACES
	l = &header->lumps[LUMP_LEAFFACES];
	if (l->filelen % sizeof(short))
		VID_Error(ERR_DROP, "Mod_LoadMarksurfaces: funny lump size in %s", model->name);

	count = l->filelen / sizeof(short);
	allocSize += ALIGN_TO_CACHELINE(count * sizeof(msurface_t));

	// LUMP_VISIBILITY
	l = &header->lumps[LUMP_VISIBILITY];
	if (l->filelen > 0)
		allocSize += ALIGN_TO_CACHELINE(l->filelen);

	// LUMP_LEAFS
	l = &header->lumps[LUMP_LEAFS];
	if (l->filelen % sizeof(dleaf_t))
		VID_Error(ERR_DROP, "Mod_LoadLeafs: funny lump size in %s", model->name);

	count = l->filelen / sizeof(dleaf_t);
	allocSize += ALIGN_TO_CACHELINE(count * sizeof(mleaf_t));

	// LUMP_NODES
	l = &header->lumps[LUMP_NODES];
	if (l->filelen % sizeof(dnode_t))
		VID_Error(ERR_DROP, "Mod_LoadNodes: funny lump size in %s", model->name);

	count = l->filelen / sizeof(dnode_t);
	allocSize += ALIGN_TO_CACHELINE(count * sizeof(mnode_t));

	// LUMP_MODELS
	l = &header->lumps[LUMP_MODELS];
	if (l->filelen % sizeof(dmodel_t))
		VID_Error(ERR_DROP, "Mod_LoadSubmodels: funny lump size in %s", model->name);

	count = l->filelen / sizeof(dmodel_t);

	// Knightmare- catch submodel overflow
	if (count >= MAX_MOD_KNOWN)
		VID_Error(ERR_DROP, "Mod_LoadSubmodels: too many submodels (%i >= %i) in %s", count, MAX_MOD_KNOWN, model->name);

	allocSize += ALIGN_TO_CACHELINE(count * sizeof(mmodel_t));

	// Return total size...
	return allocSize;
}

static void Mod_LoadBrushModel(model_t *mod, void *buffer)
{
	//mxd. Allocate memory
	mod->extradata = ModChunk_Begin(Mod_GetAllocSizeBrushModel(mod, buffer)); //mxd. Was 0x1000000
	
	mod->type = mod_brush;

	//mxd. Get _lightmap_scale form worldspawn and store it in gl_lms.lmshift...
	char scalebuf[16];
	byte lmshift = 4;
	if (Mod_ParseWorldspawnKey(buffer, "lightmap_scale", scalebuf, sizeof(scalebuf)))
	{
		const int value = atoi(scalebuf);
		if(value > 0 && value != 16)
		{
			for (lmshift = 0; (1 << lmshift) < value && lmshift < 254; lmshift++) {}
			VID_Printf(PRINT_DEVELOPER, "Using custom lightmap scale (%i) and shift (%i)\n", value, lmshift);
		}
	}
	gl_lms.lmshift = lmshift;
	gl_lms.lmscale = 1 << lmshift;

	// Load into heap
	Mod_LoadVertexes(mod, buffer);
	Mod_LoadEdges(mod, buffer);
	Mod_LoadSurfedges(mod, buffer);
	Mod_LoadLighting(mod, buffer);
	Mod_LoadPlanes(mod, buffer);
	Mod_LoadTexinfo(mod, buffer);
	Mod_LoadFaces(mod, buffer);
	Mod_LoadMarksurfaces(mod, buffer);
	Mod_LoadVisibility(mod, buffer);
	Mod_LoadLeafs(mod, buffer);
	Mod_LoadNodes(mod, buffer);
	Mod_LoadSubmodels(mod, buffer);
	mod->numframes = 2; // Regular and alternate animation
	
	// Set up submodels
	for (int i = 0; i < mod->numsubmodels; i++)
	{
		// Copy base properties from the world model to mod_inline
		mod_inline[i] = *mod;

		// Apply submodel properties
		mmodel_t *bm = &mod->submodels[i];
		model_t *mi = &mod_inline[i];

		mi->firstmodelsurface = bm->firstface;
		mi->nummodelsurfaces = bm->numfaces;
		mi->firstnode = bm->headnode;
		mi->radius = bm->radius;
		mi->numleafs = bm->visleafs;
		VectorCopy(bm->maxs, mi->maxs);
		VectorCopy(bm->mins, mi->mins);

		if (mi->firstnode >= mod->numnodes)
			VID_Error(ERR_DROP, "Inline model %i has bad firstnode", i);
	}
}

#pragma endregion

#pragma region ======================= Model registration

// Specifies the model that will be used as the world
void R_BeginRegistration(char *model)
{
	registration_sequence++;
	r_oldviewcluster = -1; // Force markleafs

	Mod_InitWalSizeList(); // Clear wal size list

	char fullname[MAX_QPATH];
	Com_sprintf(fullname, sizeof(fullname), "maps/%s.bsp", model);

	// Explicitly free the old map if different. This guarantees that mod_known[0] is the world map
	cvar_t *flushmap = Cvar_Get("flushmap", "0", 0);
	if (strcmp(mod_known[0].name, fullname) || flushmap->integer)
	{
		if(mod_known[0].type != mod_bad) //mxd. Don't free twice!
			Mod_Free(&mod_known[0]);

		// Clear this on map change (case of different server and autodownloading)
		R_InitFailedImgList();
	}

	r_worldmodel = Mod_ForName(fullname, true);
	r_viewcluster = -1;
	registration_active = true; // Map registration flag
}

struct model_s *R_RegisterModel(char *name)
{
	// Knightmare- MD3 autoreplace code
	const int len = strlen(name);
	if (!strcmp(name + len - 4, ".md2")) // Look if we have a .md2 file
	{
		char s[128];
		Q_strncpyz(s, name, sizeof(s));
		s[len - 1] = '3';
		model_t *mod = R_RegisterModel(s);

		if (mod)
			return mod;
	}
	// end Knightmare

	model_t *mod = Mod_ForName(name, false);
	if (mod)
	{
		mod->registration_sequence = registration_sequence;

		// Register any images used by the models
		if (mod->type == mod_sprite)
		{
			dsprite_t *sprout = (dsprite_t *)mod->extradata;
			for (int i = 0; i < sprout->numframes; i++)
				mod->skins[0][i] = R_FindImage(sprout->frames[i].name, it_sprite, false);
		}
		else if (mod->type == mod_md3) // Harven MD3
		{
			maliasmodel_t *pheader3 = (maliasmodel_t *)mod->extradata;

			for (int i = 0; i < pheader3->num_meshes; i++)
			{
				for (int k = 0; k < pheader3->meshes[i].num_skins; k++)
				{
					mod->skins[i][k] = R_FindImage(pheader3->meshes[i].skins[k].name, it_skin, false);

					if (strlen(pheader3->meshes[i].skins[k].glowname))
						pheader3->meshes[i].skins[k].glowimage = R_FindImage(pheader3->meshes[i].skins[k].glowname, it_skin, true);
				}
			}

			mod->numframes = pheader3->num_frames;
		}
		else if (mod->type == mod_brush)
		{
			for (int i = 0; i < mod->numtexinfo; i++)
			{
				mod->texinfo[i].image->registration_sequence = registration_sequence;
				mod->texinfo[i].glow->registration_sequence = registration_sequence;
			}
		}
	}

	return mod;
}

void R_EndRegistration(void)
{
	for (int i = 0; i < mod_numknown; i++)
	{
		model_t *mod = mod_known + i;
		if (mod->name[0] && mod->registration_sequence != registration_sequence)
			Mod_Free(mod); // Don't need this model
	}

	R_FreeUnusedImages();

	registration_active = false; // Map registration flag
}

//mxd
int R_GetRegistartionSequence(struct model_s *model)
{
	return model->registration_sequence;
}

#pragma endregion

#pragma region ======================= Model unloading

void Mod_Free(model_t *mod)
{
	//mxd. Free normalmap/shadowmap-related data
	for (int i = 0; i < mod->numsurfaces; i++)
	{
		free(mod->surfaces[i].lightmap_points);
		free(mod->surfaces[i].normalmap_normals);
	}

	//mxd. Free more normalmap-related data
	if (mod->texinfo)
		free(mod->texinfo->nmapvectors);
	
	ModChunk_Free(mod->extradata); //mxd
	memset(mod, 0, sizeof(*mod));
}

void Mod_FreeAll(void)
{
	for (int i = 0; i < mod_numknown; i++)
		if (mod_known[i].extradatasize)
			Mod_Free(&mod_known[i]);
}

#pragma endregion