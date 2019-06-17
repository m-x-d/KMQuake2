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

// r_particle.c -- particle rendering
// moved from r_main.c

#include "r_local.h"

#pragma region ======================= PARTICLE SORTING

typedef struct sortedpart_s
{
	particle_t *p;
	vec_t len;
} sortedpart_t;

static sortedpart_t sorted_parts[MAX_PARTICLES];

static int TransCompare(const void *arg1, const void *arg2) 
{
	const sortedpart_t *a1 = (sortedpart_t *)arg1;
	const sortedpart_t *a2 = (sortedpart_t *)arg2;

	if (r_transrendersort->value == 1 && a1->p && a2->p)
		return (a2->p->image - a1->p->image) * 10000 + (a2->len - a1->len); // FIXME: use hash of imagenum, blendfuncs, & critical flags

	return (a2->len - a1->len);
}

static sortedpart_t NewSortedPart(particle_t *p)
{
	vec3_t result;
	sortedpart_t s_part;

	s_part.p = p;
	VectorSubtract(p->origin, r_origin, result);
	s_part.len = (result[0] * result[0]) + (result[1] * result[1]) + (result[2] * result[2]);

	return s_part;
}

void R_SortParticlesOnList(void)
{
	for (int i = 0; i < r_newrefdef.num_particles; i++)
		sorted_parts[i] = NewSortedPart(&r_newrefdef.particles[i]);

	qsort((void *)sorted_parts, r_newrefdef.num_particles, sizeof(sortedpart_t), TransCompare);
}

#pragma endregion 

#pragma region ======================= PARTICLE RENDERING

#define DEPTHHACK_RANGE_SHORT	0.9985f
#define DEPTHHACK_RANGE_MID		0.975f
#define DEPTHHACK_RANGE_LONG	0.95f

#define DEPTHHACK_NONE	0
#define DEPTHHACK_SHORT	1
#define DEPTHHACK_MID	2
#define DEPTHHACK_LONG	3

void SetParticleOverbright(qboolean toggle);

static qboolean particle_overbright;
static qboolean fog_on;
static vec3_t particle_coord[4];
static vec3_t ParticleVec[4];
vec3_t shadelight;

//===================================================

// Particle batching struct
// Used for grouping particles by identical rendering state and rendering in vertex arrays
typedef struct particleRenderState_s
{
	GLenum polymode;
	qboolean overbright;
	qboolean polygon_offset_fill;
	qboolean alphatest;

	GLenum blendfunc_src;
	GLenum blendfunc_dst;
	int imagenum;
	int depth_hack;
} particleRenderState_t;

static particleRenderState_t currentParticleState;

// Compares new particle state to current array
static qboolean CompareParticleRenderState(particleRenderState_t *compare)
{
	if (!currentParticleState.polymode || !compare) // Not initialized
		return false;
	
	return currentParticleState.polymode == compare->polymode
		&& currentParticleState.overbright == compare->overbright
		&& currentParticleState.polygon_offset_fill == compare->polygon_offset_fill
		&& currentParticleState.alphatest == compare->alphatest
		&& currentParticleState.blendfunc_src == compare->blendfunc_src
		&& currentParticleState.blendfunc_dst == compare->blendfunc_dst
		&& currentParticleState.imagenum == compare->imagenum
		&& currentParticleState.depth_hack == compare->depth_hack;
}

static void R_DrawParticleArrays(void)
{
	c_part_polys += rb_index / 3;
	RB_RenderMeshGeneric(true);
}

// Draws array and sets new state if next particle is different
static void R_CheckParticleRenderState(particleRenderState_t *check, int numVerts, int numIndex)
{
	if (!check)
		return; // Catch null pointers

	// Check for overflow
	if (RB_CheckArrayOverflow(numVerts, numIndex))
		R_DrawParticleArrays();

	if (!CompareParticleRenderState(check))
	{
		if (currentParticleState.polymode) // Render particle array
			R_DrawParticleArrays();

		// Copy state data
		memcpy(&currentParticleState, check, sizeof(particleRenderState_t));

		// Set new render state
		GL_BlendFunc(currentParticleState.blendfunc_src, currentParticleState.blendfunc_dst);
		GL_Bind(currentParticleState.imagenum);

		float depthmax;
		switch (currentParticleState.depth_hack)
		{
			case DEPTHHACK_LONG:
				depthmax = gldepthmin + DEPTHHACK_RANGE_LONG * (gldepthmax - gldepthmin);
				break;

			case DEPTHHACK_MID:
				depthmax = gldepthmin + DEPTHHACK_RANGE_MID * (gldepthmax - gldepthmin);
				break;

			case DEPTHHACK_SHORT:
				depthmax = gldepthmin + DEPTHHACK_RANGE_SHORT * (gldepthmax - gldepthmin);
				break;

			case DEPTHHACK_NONE:
			default:
				depthmax = gldepthmax;
				break;
		}

		GL_DepthRange(gldepthmin, depthmax);

		SetParticleOverbright(currentParticleState.overbright);

		if (currentParticleState.polygon_offset_fill)
		{
			GL_Enable(GL_POLYGON_OFFSET_FILL);
			GL_PolygonOffset(-2, -1); 
		}
		else
		{
			GL_Disable(GL_POLYGON_OFFSET_FILL);
		}

		GL_Set(GL_ALPHA_TEST, currentParticleState.alphatest); //mxd
	}
	// Else just continue adding to array
}

// Clears particle render state and sets OpenGL state
static void R_BeginParticles(qboolean decals)
{
	if (!decals)
	{
		vec3_t particle_up, particle_right; //mxd. Made local
		VectorScale(vup, 0.75f, particle_up); //mxd
		VectorScale(vright, 0.75f, particle_right); //mxd

		VectorAdd(particle_up, particle_right, particle_coord[0]);
		VectorSubtract(particle_right, particle_up, particle_coord[1]);
		VectorNegate(particle_coord[0], particle_coord[2]);
		VectorNegate(particle_coord[1], particle_coord[3]);

		// No fog on particles
		fog_on = false;
		if (qglIsEnabled(GL_FOG)) // Check if fog is enabled
		{
			fog_on = true;
			qglDisable(GL_FOG); // If so, disable it
		}
	}

	// Clear particle rendering state
	memset(&currentParticleState, 0, sizeof(particleRenderState_t));
	rb_vertex = 0;
	rb_index = 0;
	particle_overbright = false;

	GL_TexEnv(GL_MODULATE);
	GL_DepthMask(false);
	GL_Enable(GL_BLEND);
	GL_ShadeModel(GL_SMOOTH);
}

// Renders any particles left in the vertex array and resets OpenGL state
static void R_FinishParticles(qboolean decals)
{
	if (currentParticleState.polymode && rb_vertex > 0 && rb_index > 0)
		R_DrawParticleArrays();

	GL_EnableTexture(0);
	SetParticleOverbright(false);
	GL_DepthRange(gldepthmin, gldepthmax);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(true);
	GL_Disable(GL_POLYGON_OFFSET_FILL);
	GL_Disable(GL_BLEND);
	qglColor4f(1, 1, 1, 1);

	// Re-enable fog if it was on
	if (!decals && fog_on)
		qglEnable(GL_FOG);
}

#pragma endregion 

static int TexParticle(int type)
{
	// Check for out of bounds image num
	type = clamp(type, 0, PARTICLE_TYPES - 1); //mxd
	
	// Check for bad particle image num
	if (!glMedia.particletextures[type])
		glMedia.particletextures[type] = glMedia.notexture;

	return glMedia.particletextures[type]->texnum;
}

static void SetBeamAngles(vec3_t start, vec3_t end, vec3_t up, vec3_t right)
{
	vec3_t move, delta;

	VectorSubtract(end, start, move);
	VectorNormalize(move);

	VectorCopy(move, up);
	VectorSubtract(r_newrefdef.vieworg, start, delta);
	CrossProduct(up, delta, right);
	VectorNormalize(right);
}

static void SetParticleOverbright(qboolean toggle)
{
	if (toggle != particle_overbright)
	{
		R_SetVertexRGBScale(toggle);
		particle_overbright = toggle;
	}
}

static void GetParticleLight(particle_t *p, vec3_t pos, float lighting, vec3_t shadelight)
{
	float brightest = 0;

	if (r_fullbright->value != 0 || !lighting)
	{
		VectorSet(shadelight, p->red, p->green, p->blue);
		return;
	}

	R_LightPoint(pos, shadelight, false);

	shadelight[0] = (lighting * shadelight[0] + (1 - lighting)) * p->red;
	shadelight[1] = (lighting * shadelight[1] + (1 - lighting)) * p->green;
	shadelight[2] = (lighting * shadelight[2] + (1 - lighting)) * p->blue;

	// This cleans up the lighting
	for (int i = 0; i < 3; i++)
		brightest = max(shadelight[i], brightest);

	if (brightest > 255)
	{
		for (int i = 0; i < 3; i++)
		{
			shadelight[i] *= 255 / brightest;
			shadelight[i] = min(255, shadelight[i]);
		}
	}

	for (int i = 0; i < 3; i++)
		shadelight[i] = max(0, shadelight[i]);
}

static void R_RenderParticle(particle_t *p)
{
	float len = 0;
	const float	lighting = r_particle_lighting->value;
	int numVerts, numIndex;
	
	vec3_t shadelight, move;
	vec4_t partColor;
	vec2_t tex_coord[4];
	vec3_t coord[4];
	vec3_t angl_coord[4];
	particleRenderState_t thisPart;

	Vector2Set(tex_coord[0], 0, 1);
	Vector2Set(tex_coord[1], 0, 0);
	Vector2Set(tex_coord[2], 1, 0);
	Vector2Set(tex_coord[3], 1, 1);

	VectorCopy(particle_coord[0], coord[0]);
	VectorCopy(particle_coord[1], coord[1]);
	VectorCopy(particle_coord[2], coord[2]);
	VectorCopy(particle_coord[3], coord[3]);

	const float size = max(p->size, 0.1);
	const qboolean shaded = (p->flags & PART_SHADED);

	if (shaded)
	{
		GetParticleLight(p, p->origin, lighting, shadelight);
		Vector4Set(partColor, shadelight[0] * DIV255, shadelight[1] * DIV255, shadelight[2] * DIV255, p->alpha);
	}
	else
	{
		Vector4Set(partColor, p->red * DIV255, p->green * DIV255, p->blue * DIV255, p->alpha);
	}

	thisPart.imagenum = TexParticle(p->image);
	thisPart.polymode = GL_TRIANGLES;
	thisPart.overbright = (p->flags & PART_OVERBRIGHT);
	thisPart.polygon_offset_fill = false;
	thisPart.alphatest = false;
	thisPart.blendfunc_src = p->blendfunc_src;
	thisPart.blendfunc_dst = p->blendfunc_dst;

	if (p->flags & PART_DEPTHHACK_SHORT) // Nice little poly-peeking - psychospaz
		thisPart.depth_hack = DEPTHHACK_SHORT;
	else if (p->flags & PART_DEPTHHACK_MID)
		thisPart.depth_hack = DEPTHHACK_MID;
	else if (p->flags & PART_DEPTHHACK_LONG)
		thisPart.depth_hack = DEPTHHACK_LONG;
	else
		thisPart.depth_hack = DEPTHHACK_NONE;

	// Estimate number of verts for this particle
	if (p->flags & PART_SPARK)
	{
		numVerts = 3;
		numIndex = 3;
	}
	else if (p->flags & PART_LIGHTNING)
	{
		VectorSubtract(p->origin, p->angle, move);
		len = VectorNormalize(move);
		numVerts = len / size + 1;
		numIndex = numVerts;
		numVerts *= 4;
		numIndex *= 6;
	}
	else // PART_BEAM, PART_DIRECTION, PART_ANGLED, default
	{
		numVerts = 4;
		numIndex = 6;
	}

	// Check if render state changed from last particle
	R_CheckParticleRenderState(&thisPart, numVerts, numIndex);

	if (p->flags & PART_SPARK) // Single triangles
	{
		vec3_t base, ang_up, ang_right;

		VectorMA(p->origin, -size, p->angle, base);
		SetBeamAngles(base, p->origin, ang_up, ang_right);

		VectorScale(ang_up, size * 2.0f, ang_up);
		VectorScale(ang_right, size * 0.25f, ang_right);

		VectorSubtract(ang_right, ang_up, angl_coord[0]);
		VectorAdd(ang_up, ang_right, angl_coord[1]);
		VectorNegate(angl_coord[1], angl_coord[1]);
		VectorAdd(p->origin, angl_coord[0], ParticleVec[0]);
		VectorAdd(p->origin, angl_coord[1], ParticleVec[1]);

		VA_SetElem3(vertexArray[rb_vertex], p->origin[0], p->origin[1], p->origin[2]);
		VA_SetElem4(colorArray[rb_vertex], partColor[0], partColor[1], partColor[2], partColor[3]);
		
		indexArray[rb_index++] = rb_vertex;
		rb_vertex++;

		for (int i = 0; i < 2; i++)
		{
			VA_SetElem3(vertexArray[rb_vertex], ParticleVec[i][0], ParticleVec[i][1], ParticleVec[i][2]);
			VA_SetElem4(colorArray[rb_vertex], 0, 0, 0, 0);

			indexArray[rb_index++] = rb_vertex;
			rb_vertex++;
		}
	}
	else if (p->flags & PART_BEAM) // Given start and end positions, will have fun here :)
	{
		vec3_t ang_up, ang_right;

		SetBeamAngles(p->angle, p->origin, ang_up, ang_right);
		VectorScale(ang_right, size, ang_right);
		
		VectorAdd(p->origin, ang_right, angl_coord[0]);
		VectorAdd(p->angle, ang_right, angl_coord[1]);
		VectorSubtract(p->angle, ang_right, angl_coord[2]);
		VectorSubtract(p->origin, ang_right, angl_coord[3]);

		indexArray[rb_index++] = rb_vertex + 0;
		indexArray[rb_index++] = rb_vertex + 1;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex + 0;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex + 3;
		
		for (int i = 0; i < 4; i++)
		{
			VA_SetElem2(texCoordArray[0][rb_vertex], tex_coord[i][0], tex_coord[i][1]);
			VA_SetElem3(vertexArray[rb_vertex], angl_coord[i][0], angl_coord[i][1], angl_coord[i][2]);
			VA_SetElem4(colorArray[rb_vertex], partColor[0], partColor[1], partColor[2], partColor[3]);

			rb_vertex++;
		}
	}
	else if (p->flags & PART_LIGHTNING) // Given start and end positions, will have fun here :)
	{
		float oldwarpadd = 0;
		float i = 0;
		vec3_t	lastvec, thisvec, tempvec;
		vec3_t	ang_up, ang_right, vdelta;
		
		const float warpsize = size * 0.4f; // Knightmare changed
		const float warpfactor = M_PI * 1.0f;
		const float time = -r_newrefdef.time * 20.0f;
				
		VectorCopy(move, ang_up);
		VectorSubtract(r_newrefdef.vieworg, p->angle, vdelta);
		CrossProduct(ang_up, vdelta, ang_right);
		if (!VectorCompare(ang_right, vec3_origin))
			VectorNormalize(ang_right);
		
		VectorScale(ang_right, size, ang_right);
		VectorScale(ang_up, size, ang_up);
		
		VectorScale(move, size, move);
		VectorCopy(p->angle, lastvec);
		VectorAdd(lastvec, move, thisvec);
		VectorCopy(thisvec, tempvec);
		
		// Lightning starts at point and then flares out
		#define LIGHTNINGWARPFUNCTION (0.25f * sinf(time + i + warpfactor) * (size / len))
		
		float warpadd = LIGHTNINGWARPFUNCTION;
		const float halflen = len / 2.0f;
		
		for(int j = 0; j < 3; j++)
			thisvec[j] = (thisvec[j] * 2 + crandom() * 5) / 2;

		while (len > size)
		{
			i += warpsize;
			VectorAdd(thisvec, ang_right, angl_coord[0]);
			VectorAdd(lastvec, ang_right, angl_coord[1]);
			VectorSubtract(lastvec, ang_right, angl_coord[2]);
			VectorSubtract(thisvec, ang_right, angl_coord[3]);

			Vector2Set(tex_coord[0], 0 + warpadd, 1);
			Vector2Set(tex_coord[1], 0 + oldwarpadd, 0);
			Vector2Set(tex_coord[2], 1 + oldwarpadd, 0);
			Vector2Set(tex_coord[3], 1 + warpadd, 1);

			indexArray[rb_index++] = rb_vertex + 0;
			indexArray[rb_index++] = rb_vertex + 1;
			indexArray[rb_index++] = rb_vertex + 2;
			indexArray[rb_index++] = rb_vertex + 0;
			indexArray[rb_index++] = rb_vertex + 2;
			indexArray[rb_index++] = rb_vertex + 3;

			for (int j = 0; j < 4; j++)
			{
				VA_SetElem2(texCoordArray[0][rb_vertex], tex_coord[j][0], tex_coord[j][1]);
				VA_SetElem3(vertexArray[rb_vertex], angl_coord[j][0], angl_coord[j][1], angl_coord[j][2]);
				VA_SetElem4(colorArray[rb_vertex], partColor[0], partColor[1], partColor[2], partColor[3]);
				rb_vertex++;
			}

			const float tlen = fabsf(len - halflen); //mxd. Was (len<halflen) ? fabsf(len-halflen) : fabsf(len-halflen);
			float factor = tlen / (size * size);
			factor = clamp(factor, 1, 4); //mxd
			
			oldwarpadd = warpadd;
			warpadd = LIGHTNINGWARPFUNCTION; // was 0.30*sin(time+i+warpfactor)
			
			len -= size;
			VectorCopy(thisvec, lastvec);
			VectorAdd(tempvec, move, tempvec);
			VectorAdd(thisvec, move, thisvec);

			for (int j = 0; j < 3; j++)
				thisvec[j] = ((thisvec[j] + crandom() * size) + tempvec[j] * factor) / (factor + 1);
		}
		
		// One more time
		if (len > 0)
		{
			VectorAdd(p->origin, ang_right, angl_coord[0]);
			VectorAdd(lastvec, ang_right, angl_coord[1]);
			VectorSubtract(lastvec, ang_right, angl_coord[2]);
			VectorSubtract(p->origin, ang_right, angl_coord[3]);

			Vector2Set(tex_coord[0], 0 + warpadd, 1);
			Vector2Set(tex_coord[1], 0 + oldwarpadd, 0);
			Vector2Set(tex_coord[2], 1 + oldwarpadd, 0);
			Vector2Set(tex_coord[3], 1 + warpadd, 1);

			indexArray[rb_index++] = rb_vertex + 0;
			indexArray[rb_index++] = rb_vertex + 1;
			indexArray[rb_index++] = rb_vertex + 2;
			indexArray[rb_index++] = rb_vertex + 0;
			indexArray[rb_index++] = rb_vertex + 2;
			indexArray[rb_index++] = rb_vertex + 3;

			for (int j = 0; j < 4; j++)
			{
				VA_SetElem2(texCoordArray[0][rb_vertex], tex_coord[j][0], tex_coord[j][1]);
				VA_SetElem3(vertexArray[rb_vertex], angl_coord[j][0], angl_coord[j][1], angl_coord[j][2]);
				VA_SetElem4(colorArray[rb_vertex], partColor[0], partColor[1], partColor[2], partColor[3]);

				rb_vertex++;
			}
		}
	}
	else if (p->flags & PART_DIRECTION) // Streched out in direction- beams etc...
	{
		vec3_t ang_up, ang_right, vdelta;

		VectorAdd(p->angle, p->origin, vdelta); 
		SetBeamAngles(vdelta, p->origin, ang_up, ang_right);
				
		VectorScale(ang_right, 0.75f, ang_right);
		VectorScale(ang_up, 0.75f * VectorLength(p->angle), ang_up);
		
		VectorAdd(ang_up, ang_right, angl_coord[0]);
		VectorSubtract(ang_right, ang_up, angl_coord[1]);
		VectorNegate(angl_coord[0], angl_coord[2]);
		VectorNegate(angl_coord[1], angl_coord[3]);
		
		VectorMA(p->origin, size, angl_coord[0], ParticleVec[0]);
		VectorMA(p->origin, size, angl_coord[1], ParticleVec[1]);
		VectorMA(p->origin, size, angl_coord[2], ParticleVec[2]);
		VectorMA(p->origin, size, angl_coord[3], ParticleVec[3]);

		indexArray[rb_index++] = rb_vertex + 0;
		indexArray[rb_index++] = rb_vertex + 1;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex + 0;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex + 3;

		for (int i = 0; i < 4; i++)
		{
			VA_SetElem2(texCoordArray[0][rb_vertex], tex_coord[i][0], tex_coord[i][1]);
			VA_SetElem3(vertexArray[rb_vertex], ParticleVec[i][0], ParticleVec[i][1], ParticleVec[i][2]);
			VA_SetElem4(colorArray[rb_vertex], partColor[0], partColor[1], partColor[2], partColor[3]);

			rb_vertex++;
		}
	}
	else if (p->flags & PART_ANGLED) // Facing direction...
	{
		vec3_t ang_up, ang_right, ang_forward;

		AngleVectors(p->angle, ang_forward, ang_right, ang_up); 
		
		VectorScale(ang_right, 0.75f, ang_right);
		VectorScale(ang_up, 0.75f, ang_up);
		
		VectorAdd(ang_up, ang_right, angl_coord[0]);
		VectorSubtract(ang_right, ang_up, angl_coord[1]);
		VectorNegate(angl_coord[0], angl_coord[2]);
		VectorNegate(angl_coord[1], angl_coord[3]);
		
		VectorMA(p->origin, size, angl_coord[0], ParticleVec[0]);
		VectorMA(p->origin, size, angl_coord[1], ParticleVec[1]);
		VectorMA(p->origin, size, angl_coord[2], ParticleVec[2]);
		VectorMA(p->origin, size, angl_coord[3], ParticleVec[3]);
		
		indexArray[rb_index++] = rb_vertex + 0;
		indexArray[rb_index++] = rb_vertex + 1;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex + 0;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex + 3;

		for (int j = 0; j < 4; j++)
		{
			VA_SetElem2(texCoordArray[0][rb_vertex], tex_coord[j][0], tex_coord[j][1]);
			VA_SetElem3(vertexArray[rb_vertex], ParticleVec[j][0],ParticleVec[j][1], ParticleVec[j][2]);
			VA_SetElem4(colorArray[rb_vertex], partColor[0], partColor[1], partColor[2], partColor[3]);

			rb_vertex++;
		}
	}
	else // Normal sprites
	{
		vec3_t ang_up, ang_right, ang_forward;

		if (p->angle[2]) // If we have roll, we do calcs
		{
			VectorSubtract(p->origin, r_newrefdef.vieworg, angl_coord[0]);
			
			VecToAngleRolled(angl_coord[0], p->angle[2], angl_coord[1]);
			AngleVectors(angl_coord[1], ang_forward, ang_right, ang_up); 
			
			VectorScale(ang_forward, 0.75f, ang_forward);
			VectorScale(ang_right, 0.75f, ang_right);
			VectorScale(ang_up, 0.75f, ang_up);
			
			VectorAdd(ang_up, ang_right, angl_coord[0]);
			VectorSubtract(ang_right, ang_up, angl_coord[1]);
			VectorNegate(angl_coord[0], angl_coord[2]);
			VectorNegate(angl_coord[1], angl_coord[3]);
			
			VectorMA(p->origin, size, angl_coord[0], ParticleVec[0]);
			VectorMA(p->origin, size, angl_coord[1], ParticleVec[1]);
			VectorMA(p->origin, size, angl_coord[2], ParticleVec[2]);
			VectorMA(p->origin, size, angl_coord[3], ParticleVec[3]);
		}
		else
		{
			VectorMA(p->origin, size, coord[0], ParticleVec[0]);
			VectorMA(p->origin, size, coord[1], ParticleVec[1]);
			VectorMA(p->origin, size, coord[2], ParticleVec[2]);
			VectorMA(p->origin, size, coord[3], ParticleVec[3]);
		}

		indexArray[rb_index++] = rb_vertex + 0;
		indexArray[rb_index++] = rb_vertex + 1;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex + 0;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex + 3;

		for (int j = 0; j < 4; j++)
		{
			VA_SetElem2(texCoordArray[0][rb_vertex], tex_coord[j][0], tex_coord[j][1]);
			VA_SetElem3(vertexArray[rb_vertex], ParticleVec[j][0], ParticleVec[j][1], ParticleVec[j][2]);
			VA_SetElem4(colorArray[rb_vertex], partColor[0], partColor[1], partColor[2], partColor[3]);

			rb_vertex++;
		}
	}
}

void R_DrawAllParticles(void)
{
	R_BeginParticles(false);

	for (int i = 0; i < r_newrefdef.num_particles; i++)
	{
		if (r_transrendersort->integer && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
		{
			if (r_particledistance->value > 0 && sorted_parts[i].len > 100.0f * r_particledistance->value)
				continue;

			R_RenderParticle(sorted_parts[i].p);
		}
		else
		{
			R_RenderParticle(&r_newrefdef.particles[i]);
		}
	}

	R_FinishParticles(false);
}

#pragma endregion

#pragma region ======================= DECAL RENDERING

void R_RenderDecal(particle_t *p)
{
	vec2_t tex_coord[4];
	vec3_t angl_coord[4];
	vec3_t ang_up, ang_right, ang_forward, color;
	vec4_t partColor;
	int aggregatemask = -1;

	particleRenderState_t thisPart;

	// Frustum cull
	if (p->decal)
	{
		for (int i = 0; i < p->decal->numpolys; i++)
		{
			int mask = 0;
			for (int j = 0; j < 4; j++)
			{
				const float dp = DotProduct(frustum[j].normal, p->decal->polys[i]);
				if (dp - frustum[j].dist < 0)
					mask |= 1 << j;
			}

			aggregatemask &= mask;
		}

		if (aggregatemask)
			return;
	}

	const float size = max(p->size, 0.1f);
	const float alpha = p->alpha;

	thisPart.polygon_offset_fill = (p->decal != NULL);

	thisPart.polymode = GL_TRIANGLES;
	thisPart.overbright = false;
	thisPart.alphatest = false;
	thisPart.blendfunc_src = p->blendfunc_src;
	thisPart.blendfunc_dst = p->blendfunc_dst;
	thisPart.imagenum = TexParticle(p->image);
	thisPart.depth_hack = DEPTHHACK_NONE;

	const int numVerts = (p->decal ? p->decal->numpolys : 4);
	const int numIndex = (p->decal ? (p->decal->numpolys - 2) * 3 : 4);

	// Check if render state changed from last particle
	R_CheckParticleRenderState(&thisPart, numVerts, numIndex);

	if (p->flags & PART_SHADED)
	{
		GetParticleLight(p, p->origin, r_particle_lighting->value, shadelight);
		VectorSet(color, shadelight[0] * DIV255, shadelight[1] * DIV255, shadelight[2] * DIV255);
	}
	else
	{
		VectorSet(shadelight, p->red, p->green, p->blue);
		VectorSet(color, p->red * DIV255, p->green * DIV255, p->blue * DIV255);
	}

	if (p->flags & PART_ALPHACOLOR)
		Vector4Set(partColor, color[0] * alpha, color[1] * alpha, color[2] * alpha, alpha);
	else
		Vector4Set(partColor, color[0], color[1], color[2], alpha);

	if (p->decal)
	{	
		for (int i = 0; i < p->decal->numpolys - 2; i++)
		{
			indexArray[rb_index++] = rb_vertex;
			indexArray[rb_index++] = rb_vertex + i + 1;
			indexArray[rb_index++] = rb_vertex + i + 2;
		}

		for (int i = 0; i < p->decal->numpolys; i++)
		{
			VA_SetElem2(texCoordArray[0][rb_vertex], p->decal->coords[i][0], p->decal->coords[i][1]);
			VA_SetElem3(vertexArray[rb_vertex], p->decal->polys[i][0], p->decal->polys[i][1], p->decal->polys[i][2]);
			VA_SetElem4(colorArray[rb_vertex], partColor[0], partColor[1], partColor[2], partColor[3]);

			rb_vertex++;
		}
	}
	else
	{
		AngleVectors(p->angle, ang_forward, ang_right, ang_up); 

		VectorScale(ang_right, 0.75f, ang_right);
		VectorScale(ang_up, 0.75f, ang_up);

		VectorAdd(ang_up, ang_right, angl_coord[0]);
		VectorSubtract(ang_right, ang_up, angl_coord[1]);
		VectorNegate(angl_coord[0], angl_coord[2]);
		VectorNegate(angl_coord[1], angl_coord[3]);
		
		VectorMA(p->origin, size, angl_coord[0], ParticleVec[0]);
		VectorMA(p->origin, size, angl_coord[1], ParticleVec[1]);
		VectorMA(p->origin, size, angl_coord[2], ParticleVec[2]);
		VectorMA(p->origin, size, angl_coord[3], ParticleVec[3]);

		Vector2Set(tex_coord[0], 0, 1);
		Vector2Set(tex_coord[1], 0, 0);
		Vector2Set(tex_coord[2], 1, 0);
		Vector2Set(tex_coord[3], 1, 1);

		indexArray[rb_index++] = rb_vertex + 0;
		indexArray[rb_index++] = rb_vertex + 1;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex + 0;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex + 3;

		for (int i = 0; i < 4; i++)
		{
			VA_SetElem2(texCoordArray[0][rb_vertex], tex_coord[i][0], tex_coord[i][1]);
			VA_SetElem3(vertexArray[rb_vertex], ParticleVec[i][0], ParticleVec[i][1], ParticleVec[i][2]);
			VA_SetElem4(colorArray[rb_vertex], partColor[0], partColor[1], partColor[2], partColor[3]);

			rb_vertex++;
		}
	}
}

void R_DrawAllDecals(void)
{
	R_BeginParticles(true);

	for (int i = 0; i < r_newrefdef.num_decals; i++)
	{
		particle_t *d = &r_newrefdef.decals[i];

		if (d->decal && (d->decal->node == NULL || d->decal->node->visframe != r_visframecount)) // Skip if not going to be visible
			continue;

		R_RenderDecal(d);
	}

	R_FinishParticles(true);
}

#pragma endregion