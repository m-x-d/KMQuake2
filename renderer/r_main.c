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

// r_main.c

#include "r_local.h"
#include "vlights.h"

viddef_t vid;

model_t *r_worldmodel;

float gldepthmin;
float gldepthmax;

glconfig_t glConfig;
glstate_t glState;
glmedia_t glMedia;

entity_t *currententity;
int r_worldframe; // Added for trans animations
model_t *currentmodel;

cplane_t frustum[4];

int r_visframecount; // Bumped when going to a new PVS
int r_framecount; // Used for dlight push checking

// Per-frame statistics to print when r_speeds->value is set.
int c_brush_calls;
int c_brush_surfs;
int c_brush_polys;
int c_alias_polys;
int c_part_polys;

static float v_blend[4]; // Final blending color

// View origin
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

float r_world_matrix[16];

GLdouble r_farz; // Knightmare- variable sky range, made this a global var

// Screen size info
refdef_t r_newrefdef;

int r_viewcluster;
int r_viewcluster2;
int r_oldviewcluster;
int r_oldviewcluster2;

cvar_t *gl_driver;
cvar_t *gl_clear;

cvar_t *con_font; // Psychospaz's console font size option
cvar_t *con_font_size;
cvar_t *alt_text_color;
cvar_t *scr_netgraph_pos;

cvar_t *r_norefresh;
cvar_t *r_drawentities;
cvar_t *r_drawworld;
cvar_t *r_speeds;
cvar_t *r_fullbright;
cvar_t *r_novis;
cvar_t *r_nocull;
cvar_t *r_lerpmodels;
cvar_t *r_lefthand;
cvar_t *r_waterwave; // Water waves
cvar_t *r_caustics; // Barnes water caustics
cvar_t *r_glows; // Texture glows

cvar_t *r_dlights_normal; // Lerped dlights on models
cvar_t *r_model_shading;
cvar_t *r_model_dlights;

cvar_t *r_lightlevel; // FIXME: This is a HACK to get the client's light level

cvar_t *r_rgbscale; // Vic's RGB brightening

cvar_t *r_vertex_arrays;

cvar_t *r_ext_compiled_vertex_array;
cvar_t *r_arb_fragment_program;
cvar_t *r_arb_vertex_program;
cvar_t *r_arb_vertex_buffer_object;
cvar_t *r_pixel_shader_warp; // Allow disabling the nVidia water warp
cvar_t *r_trans_lighting; // Disabling of lightmaps on trans surfaces
cvar_t *r_warp_lighting; // Allow disabling of lighting on warp surfaces
cvar_t *r_solidalpha; // Allow disabling of trans33+trans66 surface flag combining
cvar_t *r_entity_fliproll; // Allow disabling of backwards alias model roll
cvar_t *r_old_nullmodel; // Allow selection of nullmodel

cvar_t *r_glass_envmaps; // Psychospaz's envmapping
cvar_t *r_trans_surf_sorting; // Translucent bmodel sorting
cvar_t *r_shelltype; // Entity shells: 0 = solid, 1 = warp, 2 = spheremap
cvar_t *r_screenshot_format; // Determines screenshot format
cvar_t *r_screenshot_jpeg_quality; // Heffo - JPEG Screenshots

cvar_t *r_lightcutoff; //** DMP - allow dynamic light cutoff to be user-settable

cvar_t *r_dlightshadowmapscale; //mxd. 0 - disabled, 1 - 1 ray per 1 lightmap pixel, 2 - 1 ray per 2x2 lightmap pixels, 4 - 1 ray per 4x4 lightmap pixels etc.
cvar_t *r_dlightshadowrange; //mxd. Dynamic lights don't cast shadows when distance between camera and dlight is > r_dlightshadowrange.
cvar_t *r_dlightnormalmapping; //mxd. Dynamic lights use normalmaps (1) or not (0).

cvar_t *r_drawbuffer;
cvar_t *r_lightmap;
cvar_t *r_shadows;
cvar_t *r_shadowalpha;
cvar_t *r_shadowrange;
cvar_t *r_transrendersort; // Correct trasparent sorting
cvar_t *r_particle_lighting; // Particle lighting
cvar_t *r_particle_min;
cvar_t *r_particle_max;
cvar_t *r_particle_mode; //mxd

cvar_t *r_particledistance;
cvar_t *r_particle_overdraw;

cvar_t *r_mode;
cvar_t *r_dynamic;

cvar_t *r_showtris;
cvar_t *r_showbbox; // Show model bounding box
cvar_t *r_ztrick;
cvar_t *r_finish;
cvar_t *r_cull;
cvar_t *r_polyblend;
cvar_t *r_flashblend;
cvar_t *r_saturatelighting;
cvar_t *r_swapinterval;
cvar_t *r_texturemode;
cvar_t *r_anisotropic;
cvar_t *r_anisotropic_avail;
cvar_t *r_lockpvs;

cvar_t *r_bloom; // BLOOMS

cvar_t *r_skydistance; //Knightmare- variable sky range
cvar_t *r_fog_skyratio; //Knightmare- variable sky fog ratio
cvar_t *r_saturation; //** DMP

// Returns true if the box is completely outside the frustom
qboolean R_CullBox(vec3_t mins, vec3_t maxs)
{
	if (r_nocull->integer)
		return false;

	for (int i = 0; i < 4; i++)
		if (BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2)
			return true;

	return false;
}

static void R_PolyBlend(void)
{
	if (!r_polyblend->integer || !v_blend[3])
		return;

	GL_Disable(GL_ALPHA_TEST);
	GL_Enable(GL_BLEND);
	GL_Disable(GL_DEPTH_TEST);
	GL_DisableTexture(0);

	qglLoadIdentity();

	// FIXME: get rid of these
	qglRotatef(-90, 1, 0, 0); // Put Z going up
	qglRotatef( 90, 0, 0, 1); // Put Z going up

	rb_vertex = 0;
	rb_index = 0;

	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 3;

	VA_SetElem3(vertexArray[rb_vertex], 10, 100, 100);
	VA_SetElem4(colorArray[rb_vertex], v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
	rb_vertex++;

	VA_SetElem3(vertexArray[rb_vertex], 10, -100, 100);
	VA_SetElem4(colorArray[rb_vertex], v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
	rb_vertex++;

	VA_SetElem3(vertexArray[rb_vertex], 10, -100, -100);
	VA_SetElem4(colorArray[rb_vertex], v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
	rb_vertex++;

	VA_SetElem3(vertexArray[rb_vertex], 10, 100, -100);
	VA_SetElem4(colorArray[rb_vertex], v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
	rb_vertex++;

	RB_RenderMeshGeneric(false);

	GL_Disable(GL_BLEND);
	GL_EnableTexture(0);
	//GL_Enable (GL_ALPHA_TEST);

	qglColor4f(1, 1, 1, 1);
}

static int SignbitsForPlane(cplane_t *out)
{
	// For fast box on planeside test
	int bits = 0;
	for (int j = 0; j < 3; j++)
		if (out->normal[j] < 0)
			bits |= 1 << j;

	return bits;
}

static void R_SetFrustum(void)
{
	// Rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector(frustum[0].normal, vup, vpn, -(90 - r_newrefdef.fov_x / 2 ));
	
	// Rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector(frustum[1].normal, vup, vpn, 90 - r_newrefdef.fov_x / 2);
	
	// Rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector(frustum[2].normal, vright, vpn, 90 - r_newrefdef.fov_y / 2);
	
	// Rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector(frustum[3].normal, vright, vpn, -(90 - r_newrefdef.fov_y / 2));

	for (int i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane(&frustum[i]);
	}
}

static void R_SetupFrame(void)
{
	r_framecount++;

	// Build the transformation matrix for the given view angles
	VectorCopy(r_newrefdef.vieworg, r_origin);

	AngleVectors(r_newrefdef.viewangles, vpn, vright, vup);

	// Current viewcluster
	if (!(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		mleaf_t *leaf = Mod_PointInLeaf(r_origin, r_worldmodel);
		r_viewcluster = leaf->cluster;
		r_viewcluster2 = leaf->cluster;

		// Check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{
			// Look down a bit
			vec3_t temp;
			VectorCopy(r_origin, temp);
			temp[2] -= 16;

			leaf = Mod_PointInLeaf(temp, r_worldmodel);
			if (!(leaf->contents & CONTENTS_SOLID) && leaf->cluster != r_viewcluster2)
				r_viewcluster2 = leaf->cluster;
		}
		else
		{
			// Look up a bit
			vec3_t temp;
			VectorCopy(r_origin, temp);
			temp[2] += 16;

			leaf = Mod_PointInLeaf(temp, r_worldmodel);
			if (!(leaf->contents & CONTENTS_SOLID) && leaf->cluster != r_viewcluster2)
				r_viewcluster2 = leaf->cluster;
		}
	}

	for (int i = 0; i < 4; i++)
		v_blend[i] = r_newrefdef.blend[i];

	c_brush_calls = 0;
	c_brush_surfs = 0;
	c_brush_polys = 0;
	c_alias_polys = 0;
	c_part_polys = 0;

	// Clear out the portion of the screen that the NOWORLDMODEL defines
	/*if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL )
	{
		GL_Enable( GL_SCISSOR_TEST );
		qglClearColor( 0.3, 0.3, 0.3, 1 );
		qglScissor( r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		qglClearColor( 1, 0, 0.5, 0.5 );
		GL_Disable( GL_SCISSOR_TEST );
	}*/
}

void MYgluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
	const GLdouble ymax = zNear * tan(fovy * M_PI / 360.0);
	const GLdouble ymin = -ymax;

	GLdouble xmin = ymin * aspect;
	GLdouble xmax = ymax * aspect;

	xmin += -(2 * glState.camera_separation) / zNear;
	xmax += -(2 * glState.camera_separation) / zNear;

	qglFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

void R_SetupGL(void)
{
	// Set up viewport
	const int x = floor(r_newrefdef.x * vid.width / vid.width);
	const int x2 = ceil((r_newrefdef.x + r_newrefdef.width) * vid.width / vid.width);
	const int y = floor(vid.height - r_newrefdef.y * vid.height / vid.height);
	const int y2 = ceil(vid.height - (r_newrefdef.y + r_newrefdef.height) * vid.height / vid.height);

	const int w = x2 - x;
	const int h = y - y2;

	qglViewport(x, y2, w, h);

	// Knightmare- variable sky range
	// Calc farz falue from skybox size
	if (r_skydistance->modified)
	{
		r_skydistance->modified = false;

		GLdouble boxsize = r_skydistance->value;
		boxsize -= 252 * ceil(boxsize / 2300);
		r_farz = 1.0;
		while (r_farz < boxsize) // Make this a power of 2
		{
			r_farz *= 2.0;
			if (r_farz >= 65536) // Don't make it larger than this
				break;
		}

		r_farz *= 2.0; // Double since boxsize is distance from camera to edge of skybox, not total size of skybox

		R_UpdateFogVars(); //mxd
	}
	// end Knightmare

	//mxd. Update variable sky fogging ratio.
	if (r_fog_skyratio->modified)
	{
		r_fog_skyratio->modified = false;
		R_UpdateFogVars();
	}

	// Set up projection matrix
	const float screenaspect = (float)r_newrefdef.width / r_newrefdef.height;
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();

	//Knightmare- 12/26/2001- increase back clipping plane distance
	MYgluPerspective(r_newrefdef.fov_y,  screenaspect,  4, r_farz); //was 4096

	GL_CullFace(GL_FRONT);

	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	qglRotatef(-90, 1, 0, 0); // Put Z going up
	qglRotatef( 90, 0, 0, 1); // Put Z going up
	qglRotatef(-r_newrefdef.viewangles[2], 1, 0, 0);
	qglRotatef(-r_newrefdef.viewangles[0], 0, 1, 0);
	qglRotatef(-r_newrefdef.viewangles[1], 0, 0, 1);
	qglTranslatef(-r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1], -r_newrefdef.vieworg[2]);

//	if ( glState.camera_separation != 0 && glState.stereo_enabled )
//		qglTranslatef ( glState.camera_separation, 0, 0 );

	qglGetFloatv(GL_MODELVIEW_MATRIX, r_world_matrix);

	// Set drawing parms
	GL_Set(GL_CULL_FACE, r_cull->integer); //mxd
	GL_Disable(GL_BLEND);
	GL_Disable(GL_ALPHA_TEST);
	GL_Enable(GL_DEPTH_TEST);

	rb_vertex = 0;
	rb_index = 0;
}

static void R_Clear(void)
{
	GLbitfield clearBits = 0; // Bitshifter's consolidation

	if (gl_clear->integer)
		clearBits |= GL_COLOR_BUFFER_BIT;

	if (r_ztrick->integer)
	{
		static int trickframe = 0;

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0.0f;
			gldepthmax = 0.49999f;
			GL_DepthFunc(GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1.0f;
			gldepthmax = 0.5f;
			GL_DepthFunc(GL_GEQUAL);
		}
	}
	else
	{
		clearBits |= GL_DEPTH_BUFFER_BIT;

		gldepthmin = 0.0f;
		gldepthmax = 1.0f;
		GL_DepthFunc(GL_LEQUAL);
	}

	GL_DepthRange(gldepthmin, gldepthmax);

	// Added stencil buffer
	if (glConfig.have_stencil)
	{
		qglClearStencil(1);
		clearBits |= GL_STENCIL_BUFFER_BIT;
	}

	if (clearBits) // Bitshifter's consolidation
		qglClear(clearBits);
}

// r_newrefdef must be set before the first call
static void R_RenderView(refdef_t *fd)
{
	if (r_norefresh->integer)
		return;

	r_newrefdef = *fd;

	if (!r_worldmodel && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
		VID_Error(ERR_DROP, "R_RenderView: NULL worldmodel");

	if (r_speeds->integer)
	{
		c_brush_calls = 0;
		c_brush_surfs = 0;
		c_brush_polys = 0;
		c_alias_polys = 0;
		c_part_polys = 0;
	}

	R_PushDlights();

	if (r_finish->integer)
		qglFinish();

	R_SetupFrame();
	R_SetFrustum();
	R_SetupGL();
	R_SetFog();
	R_MarkLeaves();	// Done here so we know if we're in water
	R_DrawWorld();

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL) // Options menu
	{
		R_SuspendFog();

		R_DrawAllEntities(false);
		R_DrawAllParticles();

		R_ResumeFog();
	}
	else
	{
		GL_Disable(GL_ALPHA_TEST);

		R_RenderDlights();

		if (r_transrendersort->integer)
		{
			//R_BuildParticleList();
			R_SortParticlesOnList();
			R_DrawAllDecals();
			//R_DrawAllEntityShadows();
			R_DrawSolidEntities();
			R_DrawEntitiesOnList(ents_trans);
		}
		else
		{
			R_DrawAllDecals();
			//R_DrawAllEntityShadows();
			R_DrawAllEntities(true);
		}

		R_DrawAllParticles();
		R_DrawEntitiesOnList(ents_viewweaps);

		R_ParticleStencil(1);
		R_DrawAlphaSurfaces();
		R_ParticleStencil(2);

		R_ParticleStencil(3);
		if (r_particle_overdraw->integer) // Redraw over alpha surfaces, those behind are occluded
			R_DrawAllParticles();
		R_ParticleStencil(4);

		// Always draw vwep last...
		R_DrawEntitiesOnList(ents_viewweaps_trans);

		R_BloomBlend(fd); // BLOOMS

		R_PolyBlend(); //mxd. Was R_Flash(), which called R_PolyBlend()...
	}
}

extern void Con_DrawString(int x, int y, char *string, int alpha);
extern float SCR_ScaledVideo(float param);
#define FONT_SIZE SCR_ScaledVideo(con_font_size->value)

static void R_SetGL2D(void)
{
	// Set 2D virtual screen size
	qglViewport(0, 0, vid.width, vid.height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, vid.width, vid.height, 0, -99999, 99999);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();
	GL_Disable(GL_DEPTH_TEST);
	GL_Disable(GL_CULL_FACE);
	GL_Disable(GL_BLEND);
	GL_Enable(GL_ALPHA_TEST);
	qglColor4f(1, 1, 1, 1);

	// Knightmare- draw r_speeds (modified from Echon's tutorial)
	if (r_speeds->value && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL)) // Don't do this for options menu
	{
		char line[128];
		const int lines = 5;

		for (int i = 0; i < lines; i++)
		{
			int len = 0;
			switch (i)
			{
				case 0: len = sprintf(line, S_COLOR_ALT S_COLOR_SHADOW"%5i wcall", c_brush_calls); break;
				case 1: len = sprintf(line, S_COLOR_ALT S_COLOR_SHADOW"%5i wsurf", c_brush_surfs); break;
				case 2: len = sprintf(line, S_COLOR_ALT S_COLOR_SHADOW"%5i wpoly", c_brush_polys); break;
				case 3: len = sprintf(line, S_COLOR_ALT S_COLOR_SHADOW"%5i epoly", c_alias_polys); break;
				case 4: len = sprintf(line, S_COLOR_ALT S_COLOR_SHADOW"%5i ppoly", c_part_polys); break;
				default: break;
			}

			int x;
			if (scr_netgraph_pos->value)
				x = r_newrefdef.width - (len * FONT_SIZE + FONT_SIZE / 2);
			else
				x = FONT_SIZE / 2;

			const int y = r_newrefdef.height - (lines - i) * (FONT_SIZE + 2);
			Con_DrawString(x, y, line, 255);
		}
	}
}

static void R_SetLightLevel(void)
{
	vec3_t shadelight;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	// Save off light value for server to look at (BIG HACK!)
	R_LightPoint(r_newrefdef.vieworg, shadelight, false);

	// Pick the greatest component, which should be the same as the mono value returned by software
	r_lightlevel->value = 150 * max(shadelight[0], max(shadelight[1], shadelight[2])); //mxd
}

void R_RenderFrame(refdef_t *fd)
{
	R_RenderView(fd);
	R_SetLightLevel();
	R_SetGL2D();
}

void GL_Strings_f(void);

static void R_Register(void)
{
	// Added Psychospaz's console font size option
	con_font = Cvar_Get("con_font", "default", CVAR_ARCHIVE);
	con_font_size = Cvar_Get("con_font_size", "8", CVAR_ARCHIVE);
	alt_text_color = Cvar_Get("alt_text_color", "2", CVAR_ARCHIVE);
	scr_netgraph_pos = Cvar_Get("netgraph_pos", "0", CVAR_ARCHIVE);

	gl_driver = Cvar_Get("gl_driver", "opengl32", CVAR_ARCHIVE);
	gl_clear = Cvar_Get("gl_clear", "0", 0);

	r_lefthand = Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	r_norefresh = Cvar_Get("r_norefresh", "0", CVAR_CHEAT);
	r_fullbright = Cvar_Get("r_fullbright", "0", CVAR_CHEAT);
	r_drawentities = Cvar_Get("r_drawentities", "1", 0);
	r_drawworld = Cvar_Get("r_drawworld", "1", CVAR_CHEAT);
	r_novis = Cvar_Get("r_novis", "0", CVAR_CHEAT);
	r_nocull = Cvar_Get("r_nocull", "0", CVAR_CHEAT);
	r_lerpmodels = Cvar_Get("r_lerpmodels", "1", 0);
	r_speeds = Cvar_Get("r_speeds", "0", 0);

	// Lerped dlights on models
	r_dlights_normal = Cvar_Get("r_dlights_normal", "1", CVAR_ARCHIVE);
	r_model_shading = Cvar_Get("r_model_shading", "2", CVAR_ARCHIVE);
	r_model_dlights = Cvar_Get("r_model_dlights", "8", CVAR_ARCHIVE);

	r_lightlevel = Cvar_Get("r_lightlevel", "0", 0);
	r_rgbscale = Cvar_Get("r_rgbscale", "2", CVAR_ARCHIVE); // Vic's RGB brightening

	r_waterwave = Cvar_Get("r_waterwave", "0", CVAR_ARCHIVE);
	r_caustics = Cvar_Get("r_caustics", "1", CVAR_ARCHIVE);
	r_glows = Cvar_Get("r_glows", "1", CVAR_ARCHIVE);

	// Correct trasparent sorting
	r_transrendersort = Cvar_Get("r_transrendersort", "1", CVAR_ARCHIVE);
	r_particle_lighting = Cvar_Get("r_particle_lighting", "1.0", CVAR_ARCHIVE);
	r_particledistance = Cvar_Get("r_particledistance", "0", CVAR_ARCHIVE);
	r_particle_overdraw = Cvar_Get("r_particle_overdraw", "0", CVAR_ARCHIVE);
	r_particle_min = Cvar_Get("r_particle_min", "0", CVAR_ARCHIVE);
	r_particle_max = Cvar_Get("r_particle_max", "0", CVAR_ARCHIVE);
	r_particle_mode = Cvar_Get("r_particle_mode", "1", CVAR_ARCHIVE); //mxd. 0 - Vanilla, 1 - KMQ2

	r_mode = Cvar_Get("r_mode", "3", CVAR_ARCHIVE);
	r_lightmap = Cvar_Get("r_lightmap", "0", 0);
	r_shadows = Cvar_Get("r_shadows", "1", CVAR_ARCHIVE); //mxd. Was 0
	r_shadowalpha = Cvar_Get("r_shadowalpha", "0.4", CVAR_ARCHIVE);
	r_shadowrange = Cvar_Get("r_shadowrange", "768", CVAR_ARCHIVE);

	r_dynamic = Cvar_Get("r_dynamic", "1", 0);
	r_showtris = Cvar_Get("r_showtris", "0", CVAR_CHEAT);
	r_showbbox = Cvar_Get("r_showbbox", "0", CVAR_CHEAT); // Show model bounding box
	r_ztrick = Cvar_Get("r_ztrick", "0", 0);
	r_finish = Cvar_Get("r_finish", "0", CVAR_ARCHIVE);
	r_cull = Cvar_Get("r_cull", "1", 0);
	r_polyblend = Cvar_Get("r_polyblend", "1", 0);
	r_flashblend = Cvar_Get("r_flashblend", "0", 0);
	r_texturemode = Cvar_Get("r_texturemode", "GL_NEAREST_MIPMAP_LINEAR", CVAR_ARCHIVE); //mxd. Was GL_LINEAR_MIPMAP_NEAREST
	r_anisotropic = Cvar_Get("r_anisotropic", "16", CVAR_ARCHIVE); //mxd. Was 0
	r_anisotropic_avail = Cvar_Get("r_anisotropic_avail", "0", 0);
	r_lockpvs = Cvar_Get("r_lockpvs", "0", 0);

	r_vertex_arrays = Cvar_Get("r_vertex_arrays", "1", CVAR_ARCHIVE);

	r_ext_compiled_vertex_array = Cvar_Get("r_ext_compiled_vertex_array", "1", CVAR_ARCHIVE);

	r_arb_fragment_program = Cvar_Get("r_arb_fragment_program", "1", CVAR_ARCHIVE);
	r_arb_vertex_program = Cvar_Get("_arb_vertex_program", "1", CVAR_ARCHIVE);

	r_arb_vertex_buffer_object = Cvar_Get("r_arb_vertex_buffer_object", "1", CVAR_ARCHIVE);

	// Allow disabling the nVidia water warp
	r_pixel_shader_warp = Cvar_Get("r_pixel_shader_warp", "1", CVAR_ARCHIVE);

	// Allow disabling of lightmaps on trans surfaces
	r_trans_lighting = Cvar_Get("r_trans_lighting", "2", CVAR_ARCHIVE);

	// Allow disabling of lighting on warp surfaces
	r_warp_lighting = Cvar_Get("r_warp_lighting", "1", CVAR_ARCHIVE);

	// Allow disabling of trans33+trans66 surface flag combining
	r_solidalpha = Cvar_Get("r_solidalpha", "1", CVAR_ARCHIVE);

	// Allow disabling of backwards alias model roll
	r_entity_fliproll = Cvar_Get("r_entity_fliproll", "1", CVAR_ARCHIVE);

	// Allow selection of nullmodel
	r_old_nullmodel = Cvar_Get("r_old_nullmodel", "0", CVAR_ARCHIVE);

	// Added Psychospaz's envmapping
	r_glass_envmaps = Cvar_Get("r_glass_envmaps", "1", CVAR_ARCHIVE);
	r_trans_surf_sorting = Cvar_Get("r_trans_surf_sorting", "0", CVAR_ARCHIVE);
	r_shelltype = Cvar_Get("r_shelltype", "1", CVAR_ARCHIVE);

	r_screenshot_format = Cvar_Get("r_screenshot_format", "jpg", CVAR_ARCHIVE); // Determines screenshot format
	r_screenshot_jpeg_quality = Cvar_Get("r_screenshot_jpeg_quality", "85", CVAR_ARCHIVE); // Heffo - JPEG Screenshots

	r_drawbuffer = Cvar_Get("r_drawbuffer", "GL_BACK", 0);
	r_swapinterval = Cvar_Get("r_swapinterval", "1", CVAR_ARCHIVE);

	r_saturatelighting = Cvar_Get("r_saturatelighting", "0", 0);

	r_bloom = Cvar_Get("r_bloom", "0", CVAR_ARCHIVE); // BLOOMS

	r_skydistance = Cvar_Get("r_skydistance", "10000", CVAR_ARCHIVE); // Variable sky range
	r_fog_skyratio = Cvar_Get("r_fog_skyratio", "10", CVAR_ARCHIVE);  // Variable sky fog ratio
	r_saturation = Cvar_Get("r_saturation", "1.0", CVAR_ARCHIVE);	  //** DMP saturation setting (.89 good for nvidia)
	r_lightcutoff = Cvar_Get("r_lightcutoff", "0", CVAR_ARCHIVE);	  //** DMP dynamic light cutoffnow variable

	r_dlightshadowmapscale = Cvar_Get("r_dlightshadowmapscale", "2", CVAR_ARCHIVE); //mxd
	r_dlightshadowrange = Cvar_Get("r_dlightshadowrange", "768", CVAR_ARCHIVE); //mxd
	r_dlightnormalmapping = Cvar_Get("r_dlightnormalmapping", "1", CVAR_ARCHIVE); //mxd

	Cmd_AddCommand("imagelist", R_ImageList_f);
	Cmd_AddCommand("screenshot", R_ScreenShot_f);
	Cmd_AddCommand("screenshot_silent", R_ScreenShot_Silent_f);
	Cmd_AddCommand("modellist", Mod_Modellist_f);
	Cmd_AddCommand("gl_strings", GL_Strings_f);
}

static qboolean R_SetMode(void)
{
	const qboolean fullscreen = vid_fullscreen->integer;
	r_skydistance->modified = true; // Skybox size variable
	vid_fullscreen->modified = false;
	r_mode->modified = false;

	rserr_t err = GLimp_SetMode(&vid.width, &vid.height, r_mode->value, fullscreen);
	if (err == rserr_ok)
	{
		glState.prev_mode = r_mode->value;
	}
	else
	{
		if (err == rserr_invalid_fullscreen)
		{
			Cvar_SetValue("vid_fullscreen", 0);
			vid_fullscreen->modified = false;
			VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: fullscreen unavailable in this mode\n", __func__);
			err = GLimp_SetMode(&vid.width, &vid.height, r_mode->value, false);

			if (err == rserr_ok)
				return true;
		}
		else if (err == rserr_invalid_mode)
		{
			Cvar_SetValue("r_mode", glState.prev_mode);
			r_mode->modified = false;
			VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: invalid mode\n", __func__);
		}

		// try setting it back to something safe
		err = GLimp_SetMode(&vid.width, &vid.height, glState.prev_mode, false);
		if (err != rserr_ok)
		{
			VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: could not revert to safe mode\n", __func__);
			return false;
		}
	}

	return true;
}

// Grabs GL extensions
static qboolean R_CheckGLExtensions()
{
	// OpenGL multitexture
	qglGetIntegerv(GL_MAX_TEXTURE_UNITS, &glConfig.max_texunits);
	VID_Printf(PRINT_ALL, "...GL_MAX_TEXTURE_UNITS: %i\n", glConfig.max_texunits);

	// GL_EXT_compiled_vertex_array
	// GL_SGI_compiled_vertex_array
	//TODO: mxd. Remove GL_EXT_compiled_vertex_array. Replace with GL_ARB_vertex_buffer_object?
	glConfig.extCompiledVertArray = false;
	if (GLAD_GL_EXT_compiled_vertex_array)
	{
		if (r_ext_compiled_vertex_array->value)
		{
			if (!qglLockArraysEXT || !qglUnlockArraysEXT)
			{
				VID_Printf(PRINT_ALL, "..." S_COLOR_RED "GL_EXT/SGI_compiled_vertex_array not properly supported!\n");
				qglLockArraysEXT	= NULL;
				qglUnlockArraysEXT	= NULL;
			}
			else
			{
				VID_Printf(PRINT_ALL, "...enabling GL_EXT/SGI_compiled_vertex_array\n");
				glConfig.extCompiledVertArray = true;
			}
		}
		else
		{
			VID_Printf(PRINT_ALL, "...ignoring GL_EXT/SGI_compiled_vertex_array\n");
		}
	}
	else
	{
		VID_Printf(PRINT_ALL, "...GL_EXT/SGI_compiled_vertex_array not found\n");
	}

	// GL_ARB_fragment_program
	glConfig.arb_fragment_program = false;
	if (GLAD_GL_ARB_fragment_program)
	{
		if (r_arb_fragment_program->value)
		{
			if (!qglProgramStringARB || !qglBindProgramARB
				|| !qglDeleteProgramsARB || !qglGenProgramsARB
				|| !qglProgramEnvParameter4dARB || !qglProgramEnvParameter4dvARB
				|| !qglProgramEnvParameter4fARB || !qglProgramEnvParameter4fvARB
				|| !qglProgramLocalParameter4dARB || !qglProgramLocalParameter4dvARB
				|| !qglProgramLocalParameter4fARB || !qglProgramLocalParameter4fvARB
				|| !qglGetProgramEnvParameterdvARB || !qglGetProgramEnvParameterfvARB
				|| !qglGetProgramLocalParameterdvARB || !qglGetProgramLocalParameterfvARB
				|| !qglGetProgramivARB || !qglGetProgramStringARB || !qglIsProgramARB)
			{
				VID_Printf(PRINT_ALL, "..." S_COLOR_RED "GL_ARB_fragment_program not properly supported!\n");
			}
			else
			{
				VID_Printf(PRINT_ALL, "...using GL_ARB_fragment_program\n");
				glConfig.arb_fragment_program = true;
			}
		}
		else
		{
			VID_Printf(PRINT_ALL, "...ignoring GL_ARB_fragment_program\n");
		}
	}
	else
	{
		VID_Printf(PRINT_ALL, "...GL_ARB_fragment_program not found\n");
	}

	// GL_ARB_vertex_program
	glConfig.arb_vertex_program = false;
	if (glConfig.arb_fragment_program)
	{
		if (GLAD_GL_ARB_vertex_program)
		{
			if (r_arb_vertex_program->value)
			{
				if (!qglGetVertexAttribdvARB || !qglGetVertexAttribfvARB || !qglGetVertexAttribivARB || !qglGetVertexAttribPointervARB)
				{
					VID_Printf(PRINT_ALL, "..." S_COLOR_RED "GL_ARB_vertex_program not properly supported!\n");
				}
				else 
				{
					VID_Printf(PRINT_ALL, "...using GL_ARB_vertex_program\n");
					glConfig.arb_vertex_program = true;
				}
			}
			else
			{
				VID_Printf(PRINT_ALL, "...ignoring GL_ARB_vertex_program\n");
			}
		}
		else
		{
			VID_Printf(PRINT_ALL, "...GL_ARB_vertex_program not found\n");
		}
	}

	R_Compile_ARB_Programs();

	// GL_EXT_texture_filter_anisotropic - NeVo
	glConfig.anisotropic = false;
	if (GLAD_GL_EXT_texture_filter_anisotropic)
	{
		VID_Printf(PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic\n");
		glConfig.anisotropic = true;
		qglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glConfig.max_anisotropy);
		Cvar_SetValue("r_anisotropic_avail", glConfig.max_anisotropy);
	}
	else
	{
		VID_Printf(PRINT_ALL, "..GL_EXT_texture_filter_anisotropic not found\n");
		glConfig.max_anisotropy = 0.0f;
		Cvar_SetValue("r_anisotropic_avail", 0.0f);
	}

	return true;
}

qboolean R_Init(char *reason)
{	
	Draw_GetPalette();
	R_Register();

	// Place default error
	memcpy(reason, "Unknown failure on intialization!\0", 34);

	// Set our "safe" modes
	glState.prev_mode = 3;

	// Create the window and set up the context
	if (!R_SetMode())
	{
		memcpy(reason, "Creation of the window/context setup failed!\0", 45);
		return false;
	}

	RB_InitBackend(); // Init mini-backend

	//mxd. Load GL pointrs through GLAD.
	if (!gladLoadGLLoader(&R_GetProcAddress))
	{
		VID_Printf(PRINT_ALL, S_COLOR_RED"%s: failed to load OpenGL function pointers!\n", __func__);
		return false;
	}

	//mxd. We need at least OpenGL 2.1. FOR SCIENCE (and shaders)!
	if (GLVersion.major < 2 || (GLVersion.major == 2 && GLVersion.minor < 1))
	{
		VID_Printf(PRINT_ALL, S_COLOR_RED"%s: OpenGL 2.1 required (got %d.%d)!\n", __func__, GLVersion.major, GLVersion.minor);
		return false;
	}

	// Get our various GL strings
	glConfig.vendor_string = (const char*)qglGetString(GL_VENDOR);
	VID_Printf(PRINT_ALL, "GL_VENDOR: %s\n", glConfig.vendor_string);
	glConfig.renderer_string = (const char*)qglGetString(GL_RENDERER);
	VID_Printf(PRINT_ALL, "GL_RENDERER: %s\n", glConfig.renderer_string);
	glConfig.version_string = (const char*)qglGetString(GL_VERSION);
	sscanf(glConfig.version_string, "%d.%d.%d", &glConfig.version_major, &glConfig.version_minor, &glConfig.version_release);
	VID_Printf(PRINT_ALL, "GL_VERSION: %s\n", glConfig.version_string);

	// Knighmare- added max texture size
	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &glConfig.max_texsize);
	VID_Printf(PRINT_ALL, "GL_MAX_TEXTURE_SIZE: %i\n", glConfig.max_texsize);

	glConfig.extensions_string = (const char*)qglGetString(GL_EXTENSIONS);
	
	if (developer->value > 0) // Print 2 extensions per line
	{
		unsigned line = 0;

		VID_Printf(PRINT_DEVELOPER, "GL_EXTENSIONS: ");
		char *extString = (char *)glConfig.extensions_string;
		while (true)
		{
			char *extTok = COM_Parse(&extString);
			if (!extTok[0])
				break;

			line++;
			if (line % 2 == 0)
				VID_Printf(PRINT_DEVELOPER, "%s\n", extTok);
			else
				VID_Printf(PRINT_DEVELOPER, "%s ", extTok);
		}

		if (line % 2 != 0)
			VID_Printf(PRINT_DEVELOPER, "\n");
	}

	r_swapinterval->modified = true; // Force swapinterval update

	// Grab extensions
	if (!R_CheckGLExtensions())
		return false;

	GL_SetDefaultState();

	R_InitImages();
	Mod_Init();
	R_InitMedia();
	R_DrawInitLocal();

	R_InitDSTTex(); // Init shader warp texture
	//R_InitFogVars(); // reset fog variables //mxd. Don't.
	VLight_Init(); // Vic's bmodel lights

	const int err = qglGetError();
	if (err != GL_NO_ERROR)
		VID_Printf(PRINT_ALL, S_COLOR_RED"%s: OpenGL initialization failed with error 0x%x!\n", __func__, err);

	return true;
}

void R_ClearState(void)
{	
	R_SetFogVars(false, 0, 0, 0, 0, 0, 0, 0); // Clear fog effets
	GL_EnableMultitexture(false);
	GL_SetDefaultState();
}

static void GL_Strings_f(void)
{
	uint line = 0;

	VID_Printf(PRINT_ALL, "GL_VENDOR: %s\n", glConfig.vendor_string);
	VID_Printf(PRINT_ALL, "GL_RENDERER: %s\n", glConfig.renderer_string);
	VID_Printf(PRINT_ALL, "GL_VERSION: %s\n", glConfig.version_string);
	VID_Printf(PRINT_ALL, "GL_MAX_TEXTURE_SIZE: %i\n", glConfig.max_texsize);
	VID_Printf(PRINT_ALL, "GL_EXTENSIONS: ");

	char *extString = (char *)glConfig.extensions_string;
	while (true)
	{
		char *extTok = COM_Parse(&extString);
		if (!extTok[0])
			break;

		line++;
		if ((line % 2) == 0)
			VID_Printf(PRINT_ALL, "%s\n", extTok);
		else
			VID_Printf(PRINT_ALL, "%s ", extTok);
	}

	if ((line % 2) != 0)
		VID_Printf(PRINT_ALL, "\n");
}

void R_Shutdown(void)
{	
	Cmd_RemoveCommand("modellist");
	Cmd_RemoveCommand("screenshot");
	Cmd_RemoveCommand("screenshot_silent");
	Cmd_RemoveCommand("imagelist");
	Cmd_RemoveCommand("gl_strings");

	// Knightmare- Free saveshot buffer
	free(saveshotdata);
	saveshotdata = NULL; // Make sure this is null after a vid restart!

	Mod_FreeAll();

	R_ShutdownImages();
	R_ClearDisplayLists();

	// Shut down OS-specific OpenGL stuff like contexts, etc.
	GLimp_Shutdown();
}

extern void RefreshFont(void);

void R_BeginFrame(float camera_separation)
{
	glState.camera_separation = camera_separation;

	// Knightmare- added Psychospaz's console font size option
	if (con_font->modified)
		RefreshFont();

	if (con_font_size->modified)
	{
		if (con_font_size->value < 4)
			Cvar_Set("con_font_size", "4");
		else if (con_font_size->value > 24)
			Cvar_Set("con_font_size", "24");

		con_font_size->modified = false;
	}
	// end Knightmare

	// Change modes if necessary
	if (r_mode->modified || vid_fullscreen->modified)
	{
		// FIXME: only restart if CDS is required
		cvar_t *ref = Cvar_Get("vid_ref", "gl", 0);
		ref->modified = true;
	}

	// Update gamma
	if (vid_gamma->modified)
	{
		vid_gamma->modified = false;
		R_UpdateGammaRamp();
	}

	GLimp_BeginFrame(camera_separation);

	// Go into 2D mode
	qglViewport(0, 0, vid.width, vid.height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, vid.width, vid.height, 0, -99999, 99999);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	GL_Disable(GL_DEPTH_TEST);
	GL_Disable(GL_CULL_FACE);
	GL_Disable(GL_BLEND);
	GL_Enable(GL_ALPHA_TEST);
	qglColor4f(1, 1, 1, 1);

	// Draw buffer stuff
	if (r_drawbuffer->modified)
	{
		r_drawbuffer->modified = false;

		if (glState.camera_separation == 0 || !glState.stereo_enabled)
		{
			if (Q_stricmp(r_drawbuffer->string, "GL_FRONT") == 0)
				qglDrawBuffer(GL_FRONT);
			else
				qglDrawBuffer(GL_BACK);
		}
	}

	// Texturemode stuff
	if (r_texturemode->modified || (glConfig.anisotropic && r_anisotropic->modified)) //mxd. https://github.com/yquake2/yquake2/blob/1e3135d4fc306b6085d607afdf766a9514235b0b/src/client/refresh/gl1/gl1_main.c#L1705
	{
		GL_TextureMode(r_texturemode->string);
		r_texturemode->modified = false;
		r_anisotropic->modified = false;
	}

	// Swapinterval stuff
	GL_UpdateSwapInterval();

	// Clear screen if desired
	R_Clear();
}