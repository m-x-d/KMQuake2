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

// qfiles.h: quake file formats
// This file must be identical in the quake and utils directories

#pragma once

#pragma region ======================= .PAK

// The .pak files are just a linear collapse of a directory tree
#define IDPAKHEADER		(('K' << 24) + ('C' << 16) + ('A' << 8) + 'P')

typedef struct
{
	char name[56];
	int filepos, filelen;
} dpackfile_t;

typedef struct
{
	int ident; // == IDPAKHEADER
	int dirofs;
	int dirlen;
} dpackheader_t;

//#define	MAX_FILES_IN_PACK	4096 //mxd. Never used

#pragma endregion

#pragma region ======================= .PCX

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned char	data; // Unbounded
} pcx_t;

#pragma endregion

#pragma region ======================= .MD2

#define IDALIASHEADER	(('2' << 24) + ('P' << 16) + ('D' << 8) + 'I')
#define ALIAS_VERSION	8

#define MAX_FRAMES		512
#define MAX_MD2SKINS	32
#define	MAX_SKINNAME	64

typedef struct
{
	short s;
	short t;
} dstvert_t;

typedef struct 
{
	short index_xyz[3];
	short index_st[3];
} dtriangle_t;

typedef struct
{
	byte v[3]; // Scaled byte to fit in frame mins/maxs
	byte lightnormalindex;
} dtrivertx_t;

typedef struct
{
	float scale[3];		// Multiply byte verts by this
	float translate[3];	// Then add this
	char name[16];		// Frame name from grabbing
	dtrivertx_t verts[1]; // Variable sized
} daliasframe_t;

// The glcmd format:
// A positive integer starts a tristrip command, followed by that many vertex structures.
// A negative integer starts a trifan command, followed by -x vertexes.
// A zero indicates the end of the command list.
// A vertex consists of a floating point s, a floating point t, and an integer vertex index.

typedef struct
{
	int ident;
	int version;

	int skinwidth;
	int skinheight;
	int framesize; // Byte size of each frame

	int num_skins;
	int num_xyz;
	int num_st;		// Greater than num_xyz for seams
	int num_tris;
	int num_glcmds;	// dwords in strip/fan command list
	int num_frames;

	int ofs_skins;	// Each skin is a MAX_SKINNAME string.
	int ofs_st;		// Byte offset from start for stverts.
	int ofs_tris;	// Offset for dtriangles.
	int ofs_frames;	// Offset for first frame.
	int ofs_glcmds;	
	int ofs_end;	// Offset to the end of the file.

} dmdl_t;

#pragma endregion 

#pragma region ======================= .MD3

#define IDMD3HEADER			(('3' << 24) + ('P' << 16) + ('D' << 8)+'I')

#define MD3_ALIAS_VERSION	15
#define MD3_ALIAS_MAX_LODS	4

#define MD3_MAX_SHADERS		256		// Per mesh
#define MD3_MAX_FRAMES		1024	// Per model
#define	MD3_MAX_MESHES		32		// Per model
#define MD3_MAX_TAGS		16		// Per frame
#define MD3_MAX_PATH		64

// Vertex scales
#define	MD3_XYZ_SCALE		(1.0f / 64)

typedef unsigned int index_t;

typedef struct
{
	float st[2];
} dmd3coord_t;

typedef struct
{
	short point[3];
	short norm;
} dmd3vertex_t;

typedef struct
{
    vec3_t mins;
	vec3_t maxs;
    vec3_t translate;
    float radius;
    char creator[16];
} dmd3frame_t;

typedef struct 
{
	vec3_t origin;
	float axis[3][3];
} dorientation_t;

typedef struct
{
	char name[MD3_MAX_PATH]; // Tag name
	float origin[3];
	dorientation_t orient;
} dmd3tag_t;

typedef struct 
{
	char name[MD3_MAX_PATH];
	int unused; // Shader
} dmd3skin_t;

typedef struct
{
    char id[4];

    char name[MD3_MAX_PATH];

	int flags;

    int num_frames;
    int num_skins;
    int num_verts;
    int num_tris;

    int ofs_tris;
    int ofs_skins;
    int ofs_tcs;
    int ofs_verts;

    int meshsize;
} dmd3mesh_t;

typedef struct
{
    int id;
    int version;

    char filename[MD3_MAX_PATH];

	int flags;

    int num_frames;
    int num_tags;
    int num_meshes;
    int num_skins;

    int ofs_frames;
    int ofs_tags;
    int ofs_meshes;
    int ofs_end;
} dmd3_t;

#pragma endregion 

#pragma region ======================= .SP2

#define IDSPRITEHEADER	(('2' << 24) + ('S' << 16) + ('D' << 8) + 'I') // Little-endian "IDS2"
#define SPRITE_VERSION	2

typedef struct
{
	int width, height;
	int origin_x, origin_y; // Raster coordinates inside pic
	char name[MAX_SKINNAME]; // Name of pcx file
} dsprframe_t;

typedef struct
{
	int ident;
	int version;
	int numframes;
	dsprframe_t frames[1]; // Variable sized
} dsprite_t;

#pragma endregion 

#pragma region ======================= .WAL

#define	MIPLEVELS	4

typedef struct miptex_s
{
	char name[32];
	unsigned width, height;
	unsigned offsets[MIPLEVELS]; // Four mip maps stored
	char animname[32]; // Next frame in animation chain
	int flags;
	int contents;
	int value;
} miptex_t;

#pragma endregion 

#pragma region ======================= .BSP

#define IDBSPHEADER	(('P' << 24) + ('S' << 16) + ('B' << 8) + 'I') // Little-endian "IBSP"

#define BSPVERSION	38

// Upper design bounds
// leaffaces, leafbrushes, planes, and verts are bounded by 16 bit short limits
#define	MAX_MAP_MODELS		1024

#ifdef LARGE_MAP_SIZE
	#define	MAX_MAP_BRUSHES	16384
#else
	#define	MAX_MAP_BRUSHES	8192
#endif

#ifdef LARGE_MAP_SIZE
	#define	MAX_MAP_ENTSTRING	0x80000
#else
	#define	MAX_MAP_ENTSTRING	0x40000
#endif

#define	MAX_MAP_TEXINFO		16384 // Was 8192
#define	MAX_MAP_AREAS		256
#define	MAX_MAP_AREAPORTALS	1024
#define	MAX_MAP_PLANES		65536
#define	MAX_MAP_NODES		65536
#define	MAX_MAP_BRUSHSIDES	65536
#define	MAX_MAP_LEAFS		65536
#define	MAX_MAP_VERTS		65536
#define	MAX_MAP_FACES		65536
#define	MAX_MAP_LEAFFACES	65536
#define	MAX_MAP_LEAFBRUSHES 65536
#define	MAX_MAP_PORTALS		65536
#define	MAX_MAP_EDGES		128000
#define	MAX_MAP_SURFEDGES	256000
#define	MAX_MAP_LIGHTING	0x200000
#define	MAX_MAP_VISIBILITY	0x100000

//=============================================================================

typedef struct
{
	int fileofs, filelen;
} lump_t;

#define	LUMP_ENTITIES		0
#define	LUMP_PLANES			1
#define	LUMP_VERTEXES		2
#define	LUMP_VISIBILITY		3
#define	LUMP_NODES			4
#define	LUMP_TEXINFO		5
#define	LUMP_FACES			6
#define	LUMP_LIGHTING		7
#define	LUMP_LEAFS			8
#define	LUMP_LEAFFACES		9
#define	LUMP_LEAFBRUSHES	10
#define	LUMP_EDGES			11
#define	LUMP_SURFEDGES		12
#define	LUMP_MODELS			13
#define	LUMP_BRUSHES		14
#define	LUMP_BRUSHSIDES		15
#define	LUMP_POP			16
#define	LUMP_AREAS			17
#define	LUMP_AREAPORTALS	18
#define	HEADER_LUMPS		19

typedef struct
{
	int ident;
	int version;	
	lump_t lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	float mins[3], maxs[3];
	float origin[3]; // For sounds or lights
	int headnode;
	int firstface, numfaces; // Submodels just draw faces without walking the bsp tree
} dmodel_t;

typedef struct
{
	float point[3];
} dvertex_t;

typedef struct
{
	float normal[3];
	float dist;
	int type; // PLANE_X - PLANE_ANYZ //TODO: remove? trivial to regenerate
} dplane_t;

typedef struct
{
	int planenum;
	int children[2]; // Negative numbers are -(leafs+1), not nodes
	short mins[3]; // For frustom culling //TODO: change to int
	short maxs[3]; //TODO: change to int
	unsigned short firstface; //TODO: change to int
	unsigned short numfaces; // Counting both sides //TODO: change to int
} dnode_t;

typedef struct texinfo_s
{
	float vecs[2][4];	// [s/t][xyz offset]
	int flags;			// miptex flags + overrides.
	int value;			// Light emission, etc.
	char texture[32];	// Texture name (textures/*.wal).
	int nexttexinfo;	// For animations, -1 = end of chain.
} texinfo_t;

// Note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct
{
	unsigned short v[2]; // Vertex numbers //TODO: change to int
} dedge_t;

#define	MAXLIGHTMAPS	4

typedef struct
{
	unsigned short planenum; //TODO: change to int
	short side;

	int firstedge; // We must support > 64k edges
	short numedges;		
	short texinfo;

	// Lighting info
	byte styles[MAXLIGHTMAPS];
	int lightofs; // Start of [numstyles*surfsize] samples
} dface_t;

typedef struct
{
	int contents; // OR of all brushes (not needed?)

	short cluster;
	short area;

	short mins[3]; // For frustum culling //TODO: change to int
	short maxs[3]; //TODO: change to int

	unsigned short firstleafface; //TODO: change to int
	unsigned short numleaffaces; //TODO: change to int

	unsigned short	firstleafbrush; //TODO: change to int
	unsigned short	numleafbrushes; //TODO: change to int
} dleaf_t;

typedef struct
{
	unsigned short planenum; // Facing out of the leaf //TODO: change to int
	short texinfo;
} dbrushside_t;

typedef struct
{
	int firstside;
	int numsides;
	int contents;
} dbrush_t;

// The visibility lump consists of a header with a count, 
// then byte offsets for the PVS and PHS of each cluster, 
// then the raw compressed bit vectors.

#define	DVIS_PVS	0
#define	DVIS_PHS	1

typedef struct
{
	int numclusters;
	int bitofs[8][2]; // bitofs[numclusters][2]
} dvis_t;

// Each area has a list of portals that lead into other areas.
// When portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be.
typedef struct
{
	int portalnum;
	int otherarea;
} dareaportal_t;

typedef struct
{
	int numareaportals;
	int firstareaportal;
} darea_t;

#pragma endregion 