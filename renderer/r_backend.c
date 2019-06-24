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

// r_backend.c: array and texture coord handling for protoshaders

#include "r_local.h"

#define TABLE_SIZE		1024
#define TABLE_MASK		1023

static float rb_sinTable[TABLE_SIZE];
static float rb_triangleTable[TABLE_SIZE];
static float rb_squareTable[TABLE_SIZE];
static float rb_sawtoothTable[TABLE_SIZE];
static float rb_inverseSawtoothTable[TABLE_SIZE];
static float rb_noiseTable[TABLE_SIZE];

// Vertex arrays
uint indexArray[MAX_INDICES];
float texCoordArray[MAX_TEXTURE_UNITS][MAX_VERTICES][2];
float vertexArray[MAX_VERTICES][3];
float colorArray[MAX_VERTICES][4];
float inTexCoordArray[MAX_VERTICES][2];

uint rb_vertex;
uint rb_index;

static void RB_BuildTables(void)
{
	for (int i = 0; i < TABLE_SIZE; i++)
	{
		const float f = (float)i / (float)TABLE_SIZE;

		rb_sinTable[i] = sinf(f * M_PI2);

		if (f < 0.25f)
			rb_triangleTable[i] = 4.0f * f;
		else if (f < 0.75f)
			rb_triangleTable[i] = 2.0f - 4.0f * f;
		else
			rb_triangleTable[i] = (f - 0.75f) * 4.0f - 1.0f;

		if (f < 0.5f)
			rb_squareTable[i] = 1.0f;
		else
			rb_squareTable[i] = -1.0f;

		rb_sawtoothTable[i] = f;
		rb_inverseSawtoothTable[i] = 1.0f - f;
		rb_noiseTable[i] = crand();
	}
}

static float *RB_TableForFunc(const waveFunc_t *func)
{
	switch (func->type)
	{
		case WAVEFORM_SIN:
			return rb_sinTable;
		case WAVEFORM_TRIANGLE:
			return rb_triangleTable;
		case WAVEFORM_SQUARE:
			return rb_squareTable;
		case WAVEFORM_SAWTOOTH:
			return rb_sawtoothTable;
		case WAVEFORM_INVERSESAWTOOTH:
			return rb_inverseSawtoothTable;
		case WAVEFORM_NOISE:
			return rb_noiseTable;

		case WAVEFORM_NONE:
		default:
			VID_Error(ERR_DROP, "RB_TableForFunc: unknown waveform type %i", func->type);
			return rb_sinTable;
	}
}

void RB_InitBackend(void)
{
	RB_BuildTables(); // Build waveform tables
}

float RB_CalcGlowColor(renderparms_t parms)
{
	if (parms.glow.type != WAVEFORM_NONE)
	{
		float* table = RB_TableForFunc(&parms.glow);
		const float rad = parms.glow.params[2] + parms.glow.params[3] * r_newrefdef.time;
		const float out = table[((int)(rad * TABLE_SIZE)) & TABLE_MASK] * parms.glow.params[1] + parms.glow.params[0];

		return clamp(out, 0.0f, 1.0f);
	}

	return 1.0f;
}

// Borrowed from EGL & Q2E
void RB_ModifyTextureCoords(float *inArray, float *inVerts, int numVerts, renderparms_t parms)
{
	if (parms.translate_x != 0.0f)
	{
		float *arr = inArray;
		for (int i = 0; i < numVerts; i++, arr += 2)
			arr[0] += parms.translate_x;
	}

	if (parms.translate_y != 0.0f)
	{
		float *arr = inArray;
		for (int i = 0; i < numVerts; i++, arr += 2)
			arr[1] += parms.translate_y;
	}

	if (parms.rotate != 0.0f)
	{
		const float rad = -DEG2RAD(parms.rotate * r_newrefdef.time);
		const float sint = sinf(rad);
		const float cost = cosf(rad);

		float *arr = inArray;
		for (int i = 0; i < numVerts; i++, arr += 2)
		{
			const float t1 = arr[0];
			const float t2 = arr[1];
			arr[0] = cost * (t1 - 0.5f) - sint * (t2 - 0.5f) + 0.5f;
			arr[1] = cost * (t2 - 0.5f) + sint * (t1 - 0.5f) + 0.5f;
		}
	}

	if (parms.stretch.type != WAVEFORM_NONE)
	{
		float *table = RB_TableForFunc(&parms.stretch);
		const float rad = parms.stretch.params[2] + parms.stretch.params[3] * r_newrefdef.time;
		float t1 = table[((int)(rad * TABLE_SIZE)) & TABLE_MASK] * parms.stretch.params[1] + parms.stretch.params[0];
		
		t1 = (t1 ? 1.0f / t1 : 1.0f);
		const float t2 = 0.5f - 0.5f * t1;

		float *arr = inArray;
		for (int i = 0; i < numVerts; i++, arr += 2)
		{
			arr[0] = arr[0] * t1 + t2;
			arr[1] = arr[1] * t1 + t2;
		}
	}

	if (parms.scale_x != 1.0f)
	{
		float *arr = inArray;
		for (int i = 0; i < numVerts; i++, arr += 2)
			arr[0] /= parms.scale_x;
	}

	if (parms.scale_y != 1.0f)
	{
		float *arr = inArray;
		for (int i = 0; i < numVerts; i++, arr += 2)
			arr[1] /= parms.scale_y;
	}

	if (parms.turb.type != WAVEFORM_NONE)
	{
		float *table = RB_TableForFunc(&parms.turb);
		const float t1 = parms.turb.params[2] + parms.turb.params[3] * r_newrefdef.time;
		const float scaler = 1.0f / 128.0f * 0.125f;

		float *arr = inArray;
		float *varr = inVerts;
		for (int i = 0; i < numVerts; i++, arr += 2, varr += 3)
		{
			arr[0] += (table[((int)(((varr[0] + varr[2]) * scaler + t1) * TABLE_SIZE)) & TABLE_MASK] * parms.turb.params[1] + parms.turb.params[0]);
			arr[1] += (table[((int)((varr[1] * scaler + t1) * TABLE_SIZE)) & TABLE_MASK] * parms.turb.params[1] + parms.turb.params[0]);
		}
	}

	if (parms.scroll_x != 0.0f)
	{
		float *arr = inArray;
		for (int i = 0; i < numVerts; i++, arr += 2)
			arr[0] += r_newrefdef.time * parms.scroll_x;
	}

	if (parms.scroll_y != 0.0f)
	{
		float *arr = inArray;
		for (int i = 0; i < numVerts; i++, arr += 2)
			arr[1] += r_newrefdef.time * parms.scroll_y;
	}
}

qboolean RB_CheckArrayOverflow(int numVerts, int numIndex)
{
	if (numVerts > MAX_VERTICES || numIndex > MAX_INDICES)
		Com_Error(ERR_DROP, "RB_CheckArrayOverflow: %i > MAX_VERTICES or %i > MAX_INDICES", numVerts, numIndex);

	return (rb_vertex + numVerts > MAX_VERTICES || rb_index + numIndex > MAX_INDICES);
}

void RB_DrawArrays(void)
{
	if (rb_vertex == 0 || rb_index == 0) // Nothing to render
		return;

	GL_LockArrays(rb_vertex);
	qglDrawRangeElements(GL_TRIANGLES, 0, rb_vertex, rb_index, GL_UNSIGNED_INT, indexArray);
	GL_UnlockArrays();
}

// Re-draws a mesh in outline mode
void RB_DrawMeshTris(void)
{
	int numTMUs = 0;

	if (!r_showtris->integer)
		return;

	if (r_showtris->integer == 1)
		GL_Disable(GL_DEPTH_TEST);

	qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	for (int i = 0; i < glConfig.max_texunits; i++)
	{
		if (glState.activetmu[i])
		{
			numTMUs++;
			GL_DisableTexture(i);
		}
	}
	qglDisableClientState(GL_COLOR_ARRAY);
	qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	RB_DrawArrays();

	qglEnableClientState(GL_COLOR_ARRAY);
	for (int i = 0; i < numTMUs; i++)
		GL_EnableTexture(i);
	qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	if (r_showtris->integer == 1)
		GL_Enable(GL_DEPTH_TEST);
}

void RB_RenderMeshGeneric(qboolean drawTris)
{
	RB_DrawArrays();

	if (drawTris)
		RB_DrawMeshTris();

	rb_vertex = 0;
	rb_index = 0;
}