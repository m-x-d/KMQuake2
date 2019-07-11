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

#pragma once

#include <stdio.h>
#include <glad/glad.h> //mxd
#include "../client/ref.h"

#define PITCH	0 // up / down
#define YAW		1 // left / right
#define ROLL	2 // fall over

#ifndef __VIDDEF_T
#define __VIDDEF_T
typedef struct
{
	int width; // Coordinates from main game
	int height;
} viddef_t;
#endif

extern viddef_t vid;

// Skins will be outline flood filled and mip mapped.
// Pics and sprites with alpha will be outline flood filled.
// Pics won't be mip mapped.
typedef enum 
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky,
	it_part, //Knightmare added
	it_font //mxd
} imagetype_t;

typedef struct image_s
{
	char name[MAX_QPATH]; // Game path, including extension.
	uint hash; // Used to speed up searching. //mxd. Changed: stores hash of name without extension!
	imagetype_t type;

	int width; // Source image size.
	int height;
	int upload_width; // Source image size after power of two and picmip.
	int upload_height;
	
	int registration_sequence; // 0 = free
	struct msurface_s *texturechain; // For sort-by-texture world drawing.
	struct msurface_s *warp_texturechain; // Same as above, for warp surfaces.
	int texnum;	// gl texture binding
	float sl, tl, sh, th; // 0,0 - 1,1 unless part of the scrap
	
	qboolean has_alpha;
	qboolean mipmap; //mxd

	float replace_scale_w; // Knightmare- for scaling hi-res replacement images
	float replace_scale_h; // Knightmare- for scaling hi-res replacement images
} image_t;

#define MAX_LIGHTMAPS		256 // Change by Brendon Chung, was 128

#define TEXNUM_LIGHTMAPS	1024
#define TEXNUM_SCRAPS		(TEXNUM_LIGHTMAPS + MAX_LIGHTMAPS) // Was 1152
#define TEXNUM_IMAGES		(TEXNUM_SCRAPS + 1) // Was 1153

#define MAX_GLTEXTURES		4096 // Knightmare increased, was 1024

#include "r_alias.h" // Harven MD3. Needs image_t struct

//===================================================================

typedef enum
{
	rserr_ok,
	rserr_invalid_fullscreen,
	rserr_invalid_mode,
	rserr_unknown
} rserr_t;

#include "r_model.h"

extern void GL_UpdateSwapInterval();

extern float gldepthmin;
extern float gldepthmax;

//====================================================

extern image_t gltextures[MAX_GLTEXTURES];
extern int numgltextures;

#define PARTICLE_TYPES 256

typedef enum
{
	DL_NULLMODEL1 = 0,
	DL_NULLMODEL2,
	NUM_DISPLAY_LISTS
} displaylist_t;

typedef struct glmedia_s
{
	image_t *notexture;		// Used for bad textures
	image_t *whitetexture;	// Used for solid colors
	image_t *rawtexture;	// Used for cinematics
	image_t *envmappic;
	image_t *spheremappic;
	image_t *shelltexture;
	image_t *causticwaterpic;
	image_t *causticslimepic;
	image_t *causticlavapic;
	image_t *particlebeam;
	image_t *particletextures[PARTICLE_TYPES];
	unsigned displayLists[NUM_DISPLAY_LISTS];
} glmedia_t;

extern glmedia_t glMedia;

extern entity_t *currententity;
extern int r_worldframe; // Knightmare added for trans animations
extern model_t *currentmodel;
extern int r_visframecount;
extern int r_framecount;
extern cplane_t frustum[4];

// Debug statistics vars
extern int c_brush_calls;
extern int c_brush_surfs;
extern int c_brush_polys;
extern int c_alias_polys;
extern int c_part_polys;

// Knightmare- saveshot buffer
extern byte *saveshotdata;

extern int gl_filter_min;
extern int gl_filter_max;

//
// View origin
//
extern vec3_t vup; // Up view vector
extern vec3_t vpn; // Forward view vector
extern vec3_t vright; // Right view vector
extern vec3_t r_origin; // Player view origin

extern GLdouble r_farz; // Knightmare- variable sky range, made this a global var

//
// Screen size info
//
extern refdef_t r_newrefdef;
extern int r_viewcluster;
extern int r_viewcluster2;
extern int r_oldviewcluster;
extern int r_oldviewcluster2;

extern cvar_t *gl_clear;
extern cvar_t *gl_driver;

extern cvar_t *r_norefresh;
extern cvar_t *r_lefthand;
extern cvar_t *r_drawentities;
extern cvar_t *r_drawworld;
extern cvar_t *r_speeds;
extern cvar_t *r_fullbright;
extern cvar_t *r_novis;
extern cvar_t *r_nocull;
extern cvar_t *r_lerpmodels;

extern cvar_t *r_waterwave; // Knightmare- water waves
extern cvar_t *r_caustics; // Barnes water caustics
extern cvar_t *r_glows; // Texture glows

// Knightmare- lerped dlights on models
extern cvar_t *r_model_shading;
extern cvar_t *r_model_dlights;

extern cvar_t *r_lightlevel; // FIXME: This is a HACK to get the client's light level

extern cvar_t *r_rgbscale; // Knightmare- added Vic's RGB brightening

// Knightmare- added Psychospaz's console font size option
extern cvar_t *con_font;
extern cvar_t *con_font_size;

extern cvar_t *r_vertex_arrays;

extern cvar_t *r_ext_compiled_vertex_array;
extern cvar_t *r_arb_vertex_buffer_object;
extern cvar_t *r_pixel_shader_warp;	// Allow disabling the nVidia water warp
extern cvar_t *r_trans_lighting;	// Allow disabling of lighting on trans surfaces
extern cvar_t *r_warp_lighting;		// Allow disabling of lighting on warp surfaces
extern cvar_t *r_solidalpha;		// Allow disabling of trans33+trans66 surface flag combining
extern cvar_t *r_entity_fliproll;	// Allow disabling of backwards alias model roll
extern cvar_t *r_old_nullmodel;		// Allow selection of nullmodel

extern cvar_t *r_glass_envmaps; // Psychospaz's envmapping
extern cvar_t *r_trans_surf_sorting; // Transparent bmodel sorting
extern cvar_t *r_shelltype; // Entity shells: 0 = solid, 1 = warp, 2 = spheremap

extern cvar_t *r_lightcutoff; //** DMP - allow dynamic light cutoff to be user-settable

extern cvar_t *r_dlightshadowmapscale; //mxd
extern cvar_t *r_dlightshadowrange; //mxd
extern cvar_t *r_dlightnormalmapping; //mxd

extern cvar_t *r_screenshot_format; // Determines screenshot format
extern cvar_t *r_screenshot_jpeg_quality; // Heffo - JPEG Screenshots

extern cvar_t *r_mode;
extern cvar_t *r_lightmap;
extern cvar_t *r_shadows;
extern cvar_t *r_shadowalpha;
extern cvar_t *r_shadowrange;
extern cvar_t *r_transrendersort; // Correct trasparent sorting
extern cvar_t *r_particle_lighting; // Particle lighting
extern cvar_t *r_particle_min;
extern cvar_t *r_particle_max;
extern cvar_t *r_particle_mode; //mxd
extern cvar_t *r_particledistance;

extern cvar_t *r_dynamic;
extern cvar_t *r_showtris;
extern cvar_t *r_showbbox; // Knightmare- show model bounding box
extern cvar_t *r_finish;
extern cvar_t *r_ztrick;
extern cvar_t *r_cull;
extern cvar_t *r_polyblend;
extern cvar_t *r_flashblend;
extern cvar_t *r_drawbuffer;
extern cvar_t *r_swapinterval;
extern cvar_t *r_anisotropic;
extern cvar_t *r_anisotropic_avail;
extern cvar_t *r_texturemode;
extern cvar_t *r_saturatelighting;
extern cvar_t *r_lockpvs;

extern cvar_t *r_skydistance; //Knightmare- variable sky range
extern cvar_t *r_fog_skyratio; //Knightmare- variable sky fog ratio
extern cvar_t *r_saturation; //** DMP

extern cvar_t *r_bloom;

extern cvar_t *vid_fullscreen;
extern cvar_t *vid_gamma;

//mxd. Made global...
extern cvar_t *vid_xpos; // X coordinate of window position
extern cvar_t *vid_ypos; // Y coordinate of window position

extern float r_world_matrix[16];

// Entity sorting struct
typedef struct sortedelement_s sortedelement_t;
struct sortedelement_s //mxd. Removed typedef. Fixes C4091: 'typedef ': ignored on left of 'sortedelement_s' when no variable is declared
{
	void  *data;
	vec_t len;
	vec3_t org;
	sortedelement_t *left, *right;
};

//
// r_entity.c
//
extern sortedelement_t *ents_trans;
extern sortedelement_t *ents_viewweaps;
extern sortedelement_t *ents_viewweaps_trans;

void R_DrawEntitiesOnList(sortedelement_t *list);
void R_DrawAllEntities(qboolean addViewWeaps);
void R_DrawSolidEntities();

//
// r_particle.c
//
void R_SortParticlesOnList();
void R_DrawAllParticles();
void R_DrawAllDecals(void);

//
// r_particle_setup.c
//
void R_CreateDisplayLists();
void R_ClearDisplayLists();
void R_InitMedia();

//
// r_screenshot.c
//
void R_ScreenShot_f(void);
void R_ScreenShot_Silent_f(void);

//
// r_light.c
//
#define LIGHTMAP_BYTES 4

#define LM_BLOCK_WIDTH	256 //mxd. Was 128
#define LM_BLOCK_HEIGHT	256 //mxd. Was 128

#define GL_LIGHTMAP_FORMAT	GL_BGRA // was GL_RGBA
#define GL_LIGHTMAP_TYPE	GL_UNSIGNED_INT_8_8_8_8_REV	// was GL_UNSIGNED_BYTE

#define BATCH_LM_UPDATES

typedef struct
{
	unsigned int left;
	unsigned int right;
	unsigned int top;
	unsigned int bottom;
} rect_t;

typedef struct
{
	int internal_format;
	int format;
	int type;
	int current_lightmap_texture;
	int lmshift; //mxd
	int lmscale; //mxd

	msurface_t *lightmap_surfaces[MAX_LIGHTMAPS];

	int allocated[LM_BLOCK_WIDTH];

	// The lightmap texture data needs to be kept in main memory, so texsubimage can update properly
	unsigned lightmap_buffer[LM_BLOCK_WIDTH * LM_BLOCK_HEIGHT];
#ifdef BATCH_LM_UPDATES	// Knightmare added
	unsigned *lightmap_update[MAX_LIGHTMAPS];
	rect_t lightrect[MAX_LIGHTMAPS];
	qboolean modified[MAX_LIGHTMAPS];
#endif // BATCH_LM_UPDATES
} gllightmapstate_t;

extern gllightmapstate_t gl_lms;


void R_LightPoint(vec3_t p, vec3_t color, qboolean isEnt);
void R_LightPointDynamics(vec3_t p, vec3_t color, m_dlight_t *list, int *amount, int max);
void R_PushDlights();
void R_ShadowLight(vec3_t pos, vec3_t lightAdd);
void R_MarkLights(dlight_t *light, int bit, mnode_t *node);

//====================================================================

extern model_t *r_worldmodel;

extern unsigned d_8to24table[256];
extern int registration_sequence;

//
// r_model.c
//
void R_DrawAliasModel(entity_t *e); // Harven MD3
void R_DrawBrushModel(entity_t *e);
void R_DrawSpriteModel(entity_t *e);
void R_DrawBeam(entity_t *e);
void R_DrawWorld();
void R_RenderDlights();
void R_DrawAlphaSurfaces();
void R_DrawInitLocal();
void R_SubdivideSurface(model_t *model, msurface_t *fa); //mxd. +model
qboolean R_CullBox(vec3_t mins, vec3_t maxs);
void R_RotateForEntity(entity_t *e, qboolean full);
int R_RollMult();
void R_MarkLeaves();

//
// r_alias_misc.c
//
// Precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
extern float r_avertexnormal_dots[SHADEDOT_QUANT][256];

extern float *shadedots;

#define MAX_MODEL_DLIGHTS 32
extern m_dlight_t model_dlights[MAX_MODEL_DLIGHTS];
extern int model_dlights_num;

extern vec3_t shadelight;
extern vec3_t lightspot;
extern float shellFlowH;
extern float shellFlowV;

void R_SetShellBlend(qboolean toggle);
void R_SetVertexRGBScale(qboolean toggle);
qboolean FlowingShell();
float R_CalcShadowAlpha(entity_t *e);
void R_FlipModel(qboolean on, qboolean cullOnly);
void R_SetShadeLight(void);
void R_DrawAliasModelBBox(vec3_t bbox[8], entity_t *e, float red, float green, float blue, float alpha);

//
// r_backend.c
//
// MrG's Vertex array stuff
#define MAX_TEXTURE_UNITS	4 // was 2
#define MAX_VERTICES		16384
#define MAX_INDICES			(MAX_VERTICES * 4)


#define VA_SetElem2(v,a,b)		((v)[0]=(a),(v)[1]=(b))
#define VA_SetElem3(v,a,b,c)	((v)[0]=(a),(v)[1]=(b),(v)[2]=(c))
#define VA_SetElem4(v,a,b,c,d)	((v)[0]=(a),(v)[1]=(b),(v)[2]=(c),(v)[3]=(d))

#define VA_SetElem2v(v,a)	((v)[0]=(a)[0],(v)[1]=(a)[1])
#define VA_SetElem3v(v,a)	((v)[0]=(a)[0],(v)[1]=(a)[1],(v)[2]=(a)[2])
#define VA_SetElem4v(v,a)	((v)[0]=(a)[0],(v)[1]=(a)[1],(v)[2]=(a)[2],(v)[3]=(a)[3])

extern uint indexArray[MAX_INDICES];
extern float texCoordArray[MAX_TEXTURE_UNITS][MAX_VERTICES][2];
extern float inTexCoordArray[MAX_VERTICES][2];
extern float vertexArray[MAX_VERTICES][3];
extern float colorArray[MAX_VERTICES][4];
extern uint rb_vertex;
extern uint rb_index;
// end vertex array stuff

void RB_InitBackend();
float RB_CalcGlowColor (renderparms_t parms);
void RB_ModifyTextureCoords(float *inArray, float *vertexArray, int numVerts, renderparms_t parms);
qboolean RB_CheckArrayOverflow(int numVerts, int numIndex);
void RB_RenderMeshGeneric(qboolean drawTris);
void RB_DrawArrays();
void RB_DrawMeshTris();

//
// r_arb_program.c
//
void R_Compile_ARB_Programs();

typedef enum
{
	F_PROG_HEATHAZEMASK = 0,
	F_PROG_WARP,
	F_PROG_WATER_DISTORT,
	NUM_FRAGMENT_PROGRAM
} fr_progs;

typedef enum
{
	V_PROG_DISTORT = 0,
	NUM_VERTEX_PROGRAM
} vrt_progs;

extern GLuint fragment_programs[NUM_FRAGMENT_PROGRAM];
extern GLuint vertex_programs[NUM_VERTEX_PROGRAM];

//
// r_warp.c
//
void R_InitDSTTex();
void R_DrawWarpSurface(msurface_t *fa, float alpha, qboolean render);
size_t R_GetWarpSurfaceVertsSize(dface_t *face, dvertex_t *vertexes, dedge_t *edges, int *surfedges); //mxd

//
// r_sky.c
//
void R_AddSkySurface(msurface_t *fa);
void R_ClearSkyBox();
void R_DrawSkyBox();

// 
// r_bloom.c 
// 
void R_BloomBlend(refdef_t *fd); 
void R_InitBloomTextures(); 

//
// r_draw.c
//
void R_DrawGetPicSize(int *w, int *h, char *name);
void R_DrawPic(int x, int y, char *name);
// Added alpha for Psychospaz's transparent console
void R_DrawStretchPic(int x, int y, int w, int h, char *name, float alpha);
// Psychospaz's scaled crosshair support
void R_DrawScaledPic(int x, int y, float scale, float alpha, char *pic);
void R_InitChars();
void R_FlushChars();
void R_DrawChar(float x, float y, int num, float scale, int red, int green, int blue, int alpha, qboolean italic, qboolean last);
void R_DrawFill(int x, int y, int w, int h, int red, int green, int blue, int alpha);

//
// r_image.c
//
void Draw_GetPalette();
struct image_s *R_RegisterSkin(char *name);

image_t *R_LoadPic(char *name, byte *pic, int width, int height, imagetype_t type, int bits);
image_t	*R_FindImage(char *name, imagetype_t type, qboolean silent); //mxd. +silent
void GL_TextureMode(char *string);
void R_ImageList_f(void);
void R_InitFailedImgList();
void R_InitImages();
void R_ShutdownImages();
void R_FreeUnusedImages();
void R_LoadNormalmap(const char *texture, mtexinfo_t *tex); //mxd

//
// r_image_pcx.c
//
void LoadPCX(char *filename, byte **pic, byte **palette, int *width, int *height);
void GetPCXInfo(char *filename, int *width, int *height); //mxd. From YQ2

//
// r_image_wal.c
//
image_t *R_LoadWal(char *name, imagetype_t type); //mxd
void GetWalInfo(char *name, int *width, int *height); //mxd

//
// r_image_stb.c (mxd)
//
qboolean STBLoad(const char *origname, const char* type, byte **pic, int *width, int *height);
qboolean STBResize(const byte *input_pixels, const int input_width, const int input_height, byte *output_pixels, const int output_width, const int output_height, const qboolean usealpha);
void STBResizeNearest(const byte* input_pixels, const int input_width, const int input_height, byte* output_pixels, const int output_width, const int output_height);
qboolean STBSaveJPG(const char *filename, byte* source, int width, int height, int quality);
qboolean STBSavePNG(const char *filename, byte* source, int width, int height);
qboolean STBSaveTGA(const char *filename, byte* source, int width, int height);

//
// r_fog.c
//
void R_SetFog();
void R_SetSkyFog(qboolean setSkyFog);
void R_SuspendFog(void);
void R_ResumeFog(void);
//void R_InitFogVars(); //mxd. Disabled
void R_SetFogVars(qboolean enable, int model, int density, int start, int end, int red, int green, int blue);
void R_UpdateFogVars(); //mxd

//
// r_sdl2.c (mxd)
//
void R_SetVsync(qboolean enable);
void R_UpdateGammaRamp();
void R_ShutdownContext();
void *R_GetProcAddress(const char* proc);
qboolean R_PrepareForWindow();

/*
** GL config stuff
*/

typedef struct
{
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extensions_string;

	// For parsing OpenGL version
	int version_major;
	int version_minor;
	int version_release;

	// Max texture size
	int max_texsize;
	int max_texunits;

	qboolean have_stencil;

	qboolean extCompiledVertArray;

	// Texture shader support
	qboolean arb_fragment_program;
	qboolean arb_vertex_program;

	// Anisotropic filtering
	qboolean anisotropic;
	float max_anisotropy;
} glconfig_t;

// Knightmare- OpenGL state manager
typedef struct
{
	qboolean fullscreen;

	int prev_mode;

	int lightmap_textures;

	int currenttextures[MAX_TEXTURE_UNITS];
	unsigned int currenttmu;
	qboolean activetmu[MAX_TEXTURE_UNITS];

	float camera_separation;
	qboolean stereo_enabled;

	// Advanced state manager - MrG
	qboolean texgen;

	qboolean gammaRamp;

	qboolean cullFace;
	qboolean polygonOffsetFill; // Knightmare added
	qboolean vertexProgram;
	qboolean fragmentProgram;
	qboolean alphaTest;
	qboolean blend;
	qboolean stencilTest;
	qboolean depthTest;
	qboolean scissorTest;

	qboolean arraysLocked;
	// End - MrG

	GLenum cullMode;
	GLenum shadeModelMode;
	GLfloat depthMin;
	GLfloat depthMax;
	GLfloat offsetFactor;
	GLfloat offsetUnits;
	GLenum alphaFunc;
	GLclampf alphaRef;
	GLenum blendSrc;
	GLenum blendDst;
	GLenum depthFunc;
	GLboolean depthMask;
} glstate_t;

extern glconfig_t glConfig;
extern glstate_t glState;

//
// r_glstate.c
//
void GL_Stencil(qboolean enable, qboolean shell);
void R_ParticleStencil(int passnum);
qboolean GL_HasStencil();

void GL_Enable(GLenum cap);
void GL_Disable(GLenum cap);
void GL_Set(GLenum cap, qboolean enable); //mxd
void GL_ShadeModel(GLenum mode);
void GL_TexEnv(GLenum value);
void GL_CullFace(GLenum mode);
void GL_PolygonOffset(GLfloat factor, GLfloat units);
void GL_AlphaFunc(GLenum func, GLclampf ref);
void GL_BlendFunc(GLenum src, GLenum dst);
void GL_DepthFunc(GLenum func);
void GL_DepthMask(GLboolean mask);
void GL_DepthRange(GLfloat rMin, GLfloat rMax);
void GL_LockArrays(int numVerts);
void GL_UnlockArrays();
void GL_EnableTexture(unsigned tmu);
void GL_DisableTexture(unsigned tmu);
void GL_EnableMultitexture(qboolean enable);
void GL_SelectTexture(unsigned tmu);
void GL_Bind(int texnum);
void GL_MBind(unsigned tmu, int texnum);
void GL_SetDefaultState();

// IMPORTED FUNCTIONS
void VID_Error(int err_level, char *str, ...);
void CL_SetParticleImages();
void VID_Printf(int print_level, char *str, ...);

qboolean VID_GetModeInfo(int *width, int *height, int mode);
void VID_NewWindow(int width, int height);

// IMPLEMENTATION-SPECIFIC FUNCTIONS
void GLimp_BeginFrame(float camera_separation);
void GLimp_EndFrame();
void GLimp_Shutdown();
rserr_t GLimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen); //mxd. int -> rserr_t