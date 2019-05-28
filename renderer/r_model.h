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

// r_model.h -- brush models

// d*_t structures are on-disk representations
// m*_t structures are in-memory

#pragma once

// In memory representation
typedef struct
{
	vec3_t position;
} mvertex_t;

typedef struct
{
	vec3_t mins, maxs;
	vec3_t origin; // For sounds or lights
	float radius;
	int headnode;
	int visleafs; // Not including the solid leaf 0
	int firstface, numfaces;
} mmodel_t;


#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWTURB		16
#define SURF_DRAWBACKGROUND	64
#define SURF_LIGHTMAPPED	128
#define SURF_UNDERWATER		256
#define SURF_UNDERSLIME		512
#define SURF_UNDERLAVA		1024
#define SURF_MASK_CAUSTIC	(SURF_UNDERWATER | SURF_UNDERSLIME | SURF_UNDERLAVA)
#define SURF_ENVMAP			2048 // Psychospaz's envmapping

typedef struct
{
	unsigned short v[2];
} medge_t;

typedef struct mtexinfo_s
{
	float vecs[2][4];
	int texWidth;	// Added Q2E hack
	int texHeight;	// Added Q2E hack
	int flags;
	int numframes;
	struct mtexinfo_s *next; // Animation chain
	image_t *image;
	image_t *glow; // Glow overlay
	float *nmapvectors; //mxd. texWidth * texHeight of float[3] normalmap vectors, in [-1 .. 1] range
} mtexinfo_t;

#define	VERTEXSIZE	7

typedef struct glpoly_s
{
	struct glpoly_s *next;
	struct glpoly_s *chain;
	int numverts;

	qboolean vertexlightset;
	byte *vertexlightbase;
	byte *vertexlight;
	vec3_t center;

	float verts[4][VERTEXSIZE]; // Variable sized (xyz s1t1 s2t2)
} glpoly_t;

typedef struct msurface_s
{
	int visframe; // Should be drawn when node is crossed

	cplane_t *plane;
	int flags;

	int firstedge; // Look up in model->surfedges[], negative numbers are backwards edges
	int numedges;
	
	short texturemins[2];
	short extents[2];

	int light_s, light_t; // gl lightmap coordinates
	int light_smax, light_tmax;
	int dlight_s, dlight_t; // gl lightmap coordinates for dynamic lightmaps

	vec3_t *lightmap_points; //mxd. Centers of lightmap texels, in world space, size is light_smax * light_tmax
	vec3_t *normalmap_normals; //mxd. Normalmap vectors, relative to surface normal, used only when lmscale is 1

	glpoly_t *polys; // Multiple if warped
	struct msurface_s *texturechain;
	struct msurface_s *lightmapchain;

	mtexinfo_t *texinfo;
	
	// Lighting info
	int dlightframe;
	int dlightbits[(MAX_DLIGHTS + 31) >> 5]; // Derived from MAX_DLIGHTS
	qboolean cached_dlight;

	int lightmaptexturenum;
	byte styles[MAXLIGHTMAPS];
	float cached_light[MAXLIGHTMAPS]; // Values currently used in lightmap
	byte *samples; // [numstyles*surfsize]
	byte *stains; // Knightmare- added stainmaps

	void *chain_part;
	void *chain_ent;

	int checkCount;
	entity_t *entity; // Entity pointer
} msurface_t;

typedef struct mnode_s
{
	// Common with leaf
	int contents; // -1, to differentiate from leafs
	int visframe; // Node needs to be traversed if current
	
	float minmaxs[6]; // For bounding box culling

	struct mnode_s *parent;

	// Node specific
	cplane_t *plane;
	struct mnode_s *children[2];

	unsigned short firstsurface;
	unsigned short numsurfaces;
} mnode_t;

typedef struct mleaf_s
{
	// Common with node
	int contents;	// Will be a negative contents number
	int visframe;	// Node needs to be traversed if current

	float minmaxs[6];	// For bounding box culling

	struct mnode_s *parent;

	// Leaf specific
	int cluster;
	int area;

	msurface_t **firstmarksurface;
	int nummarksurfaces;
} mleaf_t;

//===================================================================

// Whole model
typedef enum
{
	mod_bad,
	mod_brush,
	mod_sprite,
	mod_md3 //mxd. Renamed from mod_alias
} modtype_t;

typedef struct model_s
{
	char name[MAX_QPATH];

	int registration_sequence;

	modtype_t type;
	int numframes;
	
	int flags;

	// Volume occupied by the model graphics
	vec3_t mins, maxs;
	float radius;

	// Solid volume for clipping 
	qboolean clipbox;
	vec3_t clipmins, clipmaxs;

	// Brush model
	int firstmodelsurface, nummodelsurfaces;
	int lightmap; // Only for submodels

	int numsubmodels;
	mmodel_t *submodels;

	int numplanes;
	cplane_t *planes;

	int numleafs; // Number of visible leafs, not counting 0
	mleaf_t *leafs;

	int numvertexes;
	mvertex_t *vertexes;

	int numedges;
	medge_t *edges;

	int numnodes;
	int firstnode;
	mnode_t *nodes;

	int numtexinfo;
	mtexinfo_t *texinfo;

	int numsurfaces;
	msurface_t *surfaces;

	int numsurfedges;
	int *surfedges;

	int nummarksurfaces;
	msurface_t **marksurfaces;

	dvis_t *vis;

	byte *lightdata;

	// For alias models and skins
	// Echon's per-mesh skin support
	image_t *skins[MD3_MAX_MESHES][MAX_MD2SKINS];

	size_t extradatasize;
	void *extradata;

	qboolean hasAlpha; // If model has scripted transparency
} model_t;

//============================================================================

#define ALIGN_TO_CACHELINE(x) (((x) + 31) & ~31) //mxd

void Mod_Init(void);
model_t *Mod_ForName(char *name, qboolean crash);
mleaf_t *Mod_PointInLeaf(vec3_t p, model_t *model);
byte *Mod_ClusterPVS(int cluster, model_t *model);
float Mod_RadiusFromBounds(vec3_t mins, vec3_t maxs); //mxd

void Mod_Modellist_f(void);

void Mod_FreeAll(void);
void Mod_Free(model_t *mod);

extern qboolean registration_active; // Map registration flag

// Model memory allocation
size_t ModChunk_End(void);
void ModChunk_Free(void *base);
void *ModChunk_Begin(size_t maxsize);
void *ModChunk_Alloc(size_t size);

// MD2 / MD3 / Sprite / MD2/MD3 .script loading
void Mod_LoadAliasMD2Model(model_t *mod, void *buffer);
void Mod_LoadAliasMD3Model(model_t *mod, void *buffer); //Harven++
void Mod_LoadSpriteModel(model_t *mod, void *buffer, size_t memsize);
void Mod_LoadModelScript(model_t *mod, maliasmodel_t *aliasmod);