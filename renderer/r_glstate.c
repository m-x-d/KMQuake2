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

// r_glstate.c - OpenGL state manager from Q2E

#include "r_local.h"

void GL_Enable(GLenum cap)
{
	switch (cap)
	{
		case GL_CULL_FACE:
			if (glState.cullFace)
				return;
			glState.cullFace = true;
			break;

		case GL_POLYGON_OFFSET_FILL:
			if (glState.polygonOffsetFill)
				return;
			glState.polygonOffsetFill = true;
			break;

		case GL_VERTEX_PROGRAM_ARB:
			if (!glConfig.arb_vertex_program || glState.vertexProgram)
				return;
			glState.vertexProgram = true;
			break;

		case GL_FRAGMENT_PROGRAM_ARB:
			if (!glConfig.arb_fragment_program || glState.fragmentProgram)
				return;
			glState.fragmentProgram = true;
			break;

		case GL_ALPHA_TEST:
			if (glState.alphaTest)
				return;
			glState.alphaTest = true;
			break;

		case GL_BLEND:
			if (glState.blend)
				return;
			glState.blend = true;
			break;

		case GL_DEPTH_TEST:
			if (glState.depthTest)
				return;
			glState.depthTest = true;
			break;

		case GL_STENCIL_TEST:
			if (glState.stencilTest)
				return;
			glState.stencilTest = true;
			break;

		case GL_SCISSOR_TEST:
			if (glState.scissorTest)
				return;
			glState.scissorTest = true;
			break;

		default: //mxd
			VID_Error(ERR_QUIT, "%s: unknown parameter %i", __func__, cap);
			break;
	}

	qglEnable(cap);
}

void GL_Disable(GLenum cap)
{
	switch (cap)
	{
		case GL_CULL_FACE:
			if (!glState.cullFace)
				return;
			glState.cullFace = false;
			break;

		case GL_POLYGON_OFFSET_FILL:
			if (!glState.polygonOffsetFill)
				return;
			glState.polygonOffsetFill = false;
			break;

		case GL_VERTEX_PROGRAM_ARB:
			if (!glConfig.arb_vertex_program || !glState.vertexProgram)
				return;
			glState.vertexProgram = false;
			break;

		case GL_FRAGMENT_PROGRAM_ARB:
			if (!glConfig.arb_fragment_program || !glState.fragmentProgram)
				return;
			glState.fragmentProgram = false;
			break;

		case GL_ALPHA_TEST:
			if (!glState.alphaTest)
				return;
			glState.alphaTest = false;
			break;

		case GL_BLEND:
			if (!glState.blend)
				return;
			glState.blend = false;
			break;

		case GL_DEPTH_TEST:
			if (!glState.depthTest)
				return;
			glState.depthTest = false;
			break;

		case GL_STENCIL_TEST:
			if (!glState.stencilTest)
				return;
			glState.stencilTest = false;
			break;

		case GL_SCISSOR_TEST:
			if (!glState.scissorTest)
				return;
			glState.scissorTest = false;
			break;

		default: //mxd
			VID_Error(ERR_QUIT, "%s: unknown parameter %i", __func__, cap);
			break;
	}

	qglDisable(cap);
}

//mxd
void GL_Set(GLenum cap, qboolean enable)
{
	switch (cap)
	{
		case GL_CULL_FACE:
			if (glState.cullFace == enable)
				return;
			glState.cullFace = enable;
			break;

		case GL_POLYGON_OFFSET_FILL:
			if (glState.polygonOffsetFill == enable)
				return;
			glState.polygonOffsetFill = enable;
			break;

		case GL_VERTEX_PROGRAM_ARB:
			if (!glConfig.arb_vertex_program || glState.vertexProgram == enable)
				return;
			glState.vertexProgram = enable;
			break;

		case GL_FRAGMENT_PROGRAM_ARB:
			if (!glConfig.arb_fragment_program || glState.fragmentProgram == enable)
				return;
			glState.fragmentProgram = enable;
			break;

		case GL_ALPHA_TEST:
			if (glState.alphaTest == enable)
				return;
			glState.alphaTest = enable;
			break;

		case GL_BLEND:
			if (glState.blend == enable)
				return;
			glState.blend = enable;
			break;

		case GL_DEPTH_TEST:
			if (glState.depthTest == enable)
				return;
			glState.depthTest = enable;
			break;

		case GL_STENCIL_TEST:
			if (glState.stencilTest == enable)
				return;
			glState.stencilTest = enable;
			break;

		case GL_SCISSOR_TEST:
			if (glState.scissorTest == enable)
				return;
			glState.scissorTest = enable;
			break;

		default: //mxd
			VID_Error(ERR_QUIT, "%s: unknown parameter %i", __func__, cap);
			break;
	}

	qglEnable(cap);
}

// Set stencil buffer stenciling for shadows & color shells
void GL_Stencil(qboolean enable, qboolean shell)
{
	if (!glConfig.have_stencil)
		return;

	if (enable)
	{
		if (shell)
		{
			qglPushAttrib(GL_STENCIL_BUFFER_BIT);
			qglClear(GL_STENCIL_BUFFER_BIT);
		}

		GL_Enable(GL_STENCIL_TEST);
		qglStencilFunc(GL_EQUAL, 1, 2);
		qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
	}
	else
	{
		GL_Disable(GL_STENCIL_TEST);
		if (shell)
			qglPopAttrib();
	}
}

qboolean GL_HasStencil(void)
{
	return glConfig.have_stencil;
}

extern cvar_t *r_particle_overdraw;

// Uses stencil buffer to redraw particles only over trans surfaces
void R_ParticleStencil(int passnum)
{
	if (!glConfig.have_stencil || !r_particle_overdraw->integer)
		return;

	switch (passnum) //mxd
	{
		case 1: // Write area of trans surfaces to stencil buffer
			qglPushAttrib(GL_STENCIL_BUFFER_BIT); // save stencil buffer
			qglClearStencil(1);
			qglClear(GL_STENCIL_BUFFER_BIT);

			GL_Enable(GL_STENCIL_TEST);
			qglStencilFunc(GL_ALWAYS, 1, 0xFF);
			qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
			break;

		case 2: // Turn off writing
			GL_Disable(GL_STENCIL_TEST);
			break;

		case 3: // Enable drawing only to affected area
			GL_Enable(GL_STENCIL_TEST);
			qglStencilFunc(GL_NOTEQUAL, 1, 0xFF);
			qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
			break;

		case 4: // Turn off and restore
			GL_Disable(GL_STENCIL_TEST);
			qglPopAttrib(); // Restore stencil buffer
			break;
	}
}

void GL_Envmap(qboolean enable)
{
	if (enable)
	{
		qglTexGenf(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
		qglTexGenf(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
		
		if (!glState.texgen)
		{
			qglEnable(GL_TEXTURE_GEN_S);
			qglEnable(GL_TEXTURE_GEN_T);
			qglEnable(GL_TEXTURE_GEN_R);
			glState.texgen = true;
		}
	}
	else
	{
		if (glState.texgen)
		{
			qglDisable(GL_TEXTURE_GEN_S);
			qglDisable(GL_TEXTURE_GEN_T);
			qglDisable(GL_TEXTURE_GEN_R);
			glState.texgen = false;
		}
	}
}

void GL_ShadeModel(GLenum mode)
{
	if (glState.shadeModelMode != mode)
	{
		glState.shadeModelMode = mode;
		qglShadeModel(mode);
	}
}

void GL_TexEnv(GLenum mode)
{
	static GLenum lastmodes[2] = { -1, -1 };

	if (mode != lastmodes[glState.currenttmu])
	{
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
		lastmodes[glState.currenttmu] = mode;
	}
}

void GL_CullFace(GLenum mode)
{
	if (glState.cullMode != mode)
	{
		glState.cullMode = mode;
		qglCullFace(mode);
	}
}

void GL_PolygonOffset(GLfloat factor, GLfloat units)
{
	if (glState.offsetFactor != factor || glState.offsetUnits != units)
	{
		glState.offsetFactor = factor;
		glState.offsetUnits = units;
		qglPolygonOffset(factor, units);
	}
}

void GL_AlphaFunc(GLenum func, GLclampf ref)
{
	if (glState.alphaFunc != func || glState.alphaRef != ref)
	{
		glState.alphaFunc = func;
		glState.alphaRef = ref;
		qglAlphaFunc(func, ref);
	}
}

void GL_BlendFunc(GLenum src, GLenum dst)
{
	if (glState.blendSrc != src || glState.blendDst != dst)
	{
		glState.blendSrc = src;
		glState.blendDst = dst;
		qglBlendFunc(src, dst);
	}
}

void GL_DepthFunc(GLenum func)
{
	if (glState.depthFunc != func)
	{
		glState.depthFunc = func;
		qglDepthFunc(func);
	}
}

void GL_DepthMask(GLboolean mask)
{
	if (glState.depthMask != mask)
	{
		glState.depthMask = mask;
		qglDepthMask(mask);
	}
}

void GL_DepthRange(GLfloat rMin, GLfloat rMax)
{
	if (glState.depthMin != rMin || glState.depthMax != rMax)
	{
		glState.depthMin = rMin;
		glState.depthMax = rMax;
		qglDepthRange(rMin, rMax);
	}
}

void GL_LockArrays(int numVerts)
{
	if (!glConfig.extCompiledVertArray || glState.arraysLocked)
		return;

	qglLockArraysEXT(0, numVerts);
	glState.arraysLocked = true;
}

void GL_UnlockArrays(void)
{
	if (!glConfig.extCompiledVertArray || !glState.arraysLocked)
		return;

	qglUnlockArraysEXT();
	glState.arraysLocked = false;
}

void GL_EnableTexture(unsigned tmu)
{
	if (tmu >= MAX_TEXTURE_UNITS || tmu >= glConfig.max_texunits)
		return;

	GL_SelectTexture(tmu);
	qglEnable(GL_TEXTURE_2D);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, sizeof(texCoordArray[tmu][0]), texCoordArray[tmu][0]);
	glState.activetmu[tmu] = true;
}

void GL_DisableTexture(unsigned tmu)
{
	if (tmu >= MAX_TEXTURE_UNITS || tmu >= glConfig.max_texunits)
		return;

	GL_SelectTexture(tmu);
	qglDisable(GL_TEXTURE_2D);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glState.activetmu[tmu] = false;
}

// Only used for world drawing
void GL_EnableMultitexture(qboolean enable)
{
	if (enable)
	{
		GL_EnableTexture(1);
		GL_TexEnv(GL_REPLACE);
	}
	else
	{
		GL_DisableTexture(1);
		GL_TexEnv(GL_REPLACE);
	}

	GL_SelectTexture(0);
	GL_TexEnv(GL_REPLACE);
}

void GL_SelectTexture(unsigned tmu)
{
	if (tmu >= MAX_TEXTURE_UNITS || tmu >= glConfig.max_texunits)
		return;

	if (tmu == glState.currenttmu)
		return;

	glState.currenttmu = tmu;
	qglActiveTexture(GL_TEXTURE0 + tmu);
	qglClientActiveTexture(GL_TEXTURE0 + tmu);
}

void GL_Bind(int texnum)
{
	if (glState.currenttextures[glState.currenttmu] == texnum)
		return;

	glState.currenttextures[glState.currenttmu] = texnum;
	qglBindTexture(GL_TEXTURE_2D, texnum);
}

void GL_MBind(unsigned tmu, int texnum)
{
	if (tmu >= MAX_TEXTURE_UNITS || tmu >= glConfig.max_texunits)
		return;

	GL_SelectTexture(tmu);

	if (glState.currenttextures[tmu] == texnum)
		return;

	GL_Bind(texnum);
}

void GL_UpdateSwapInterval(void)
{
	static qboolean registering;

	// Don't swap interval if loading a map
	if (registering != registration_active)
		r_swapinterval->modified = true;

	if (r_swapinterval->modified)
	{
		r_swapinterval->modified = false;
		registering = registration_active;

		if (!glState.stereo_enabled)
			R_SetVsync(r_swapinterval->integer); //mxd
	}
}

void GL_SetDefaultState(void)
{
	// Reset the state manager
	glState.texgen = false;
	glState.cullFace = false;
	glState.polygonOffsetFill = false;
	glState.vertexProgram = false;
	glState.fragmentProgram = false;
	glState.alphaTest = false;
	glState.blend = false;
	glState.stencilTest = false;
	glState.depthTest = false;
	glState.scissorTest = false;
	glState.arraysLocked = false;

	glState.cullMode = GL_FRONT;
	glState.shadeModelMode = GL_FLAT;
	glState.depthMin = gldepthmin;
	glState.depthMax = gldepthmax;
	glState.offsetFactor = -1;
	glState.offsetUnits = -2;
	glState.alphaFunc = GL_GREATER;
	glState.alphaRef = 0.666f;
	glState.blendSrc = GL_SRC_ALPHA;
	glState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
	glState.depthFunc = GL_LEQUAL;
	glState.depthMask = GL_TRUE;

	// Set default state
	qglDisable(GL_TEXTURE_GEN_S);
	qglDisable(GL_TEXTURE_GEN_T);
	qglDisable(GL_TEXTURE_GEN_R);
	qglEnable(GL_TEXTURE_2D);
	qglDisable(GL_CULL_FACE);
	qglDisable(GL_POLYGON_OFFSET_FILL);
	qglDisable(GL_ALPHA_TEST);
	qglDisable(GL_BLEND);
	qglDisable(GL_STENCIL_TEST);
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_SCISSOR_TEST);

	qglCullFace(GL_FRONT);
	qglShadeModel(GL_FLAT);
	qglPolygonOffset(-1, -2);
	qglAlphaFunc(GL_GREATER, 0.666f);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthFunc(GL_LEQUAL);
	qglDepthMask(GL_TRUE);

	qglClearColor(1.0f, 0.0f, 0.5f, 0.5f);
	qglClearDepth(1.0);
	qglClearStencil(128);

	qglColor4f(1, 1, 1, 1);

	qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	GL_TextureMode(r_texturemode->string);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Vertex arrays
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglEnableClientState(GL_VERTEX_ARRAY);
	qglEnableClientState(GL_COLOR_ARRAY);

	qglTexCoordPointer(2, GL_FLOAT, sizeof(texCoordArray[0][0]), texCoordArray[0][0]);
	qglVertexPointer(3, GL_FLOAT, sizeof(vertexArray[0]), vertexArray[0]);
	qglColorPointer(4, GL_FLOAT, sizeof(colorArray[0]), colorArray[0]);
	// end vertex arrays

	glState.activetmu[0] = true;
	for (int i = 1; i < MAX_TEXTURE_UNITS; i++)
		glState.activetmu[i] = false;

	GL_TexEnv(GL_REPLACE);

	GL_UpdateSwapInterval();
}