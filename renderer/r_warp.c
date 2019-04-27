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
// r_warp.c -- sky and water polygons

#include "r_local.h"

extern model_t *loadmodel;
msurface_t *warpface;

#define SUBDIVIDE_SIZE 64

void BoundPoly(int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 999999;
	maxs[0] = maxs[1] = maxs[2] = -999999;
	float *v = verts;

	for (int i = 0; i < numverts; i++)
	{
		for (int j = 0; j < 3; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
	}
}

void SubdividePolygon(int numverts, float *verts)
{
	vec3_t	mins, maxs;
	vec3_t	front[64], back[64];
	float	dist[64];
	vec3_t	total;

	if (numverts > 60)
		VID_Error(ERR_DROP, "SubdividePolygon: numverts = %i", numverts);

	BoundPoly(numverts, verts, mins, maxs);

	for (int i = 0; i < 3; i++)
	{
		float m = (mins[i] + maxs[i]) * 0.5;
		m = SUBDIVIDE_SIZE * floor(m / SUBDIVIDE_SIZE + 0.5);

		if (maxs[i] - m < 8 || m - mins[i] < 8)
			continue;

		// Cut it
		float *v = verts + i;
		for (int j = 0; j < numverts; j++, v+= 3)
			dist[j] = *v - m;

		// Wrap cases
		dist[numverts] = dist[0];
		v -= i;
		VectorCopy(verts, v);

		int f = 0;
		int b = 0;
		v = verts;

		for (int j = 0; j < numverts; j++, v+= 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy(v, front[f]);
				f++;
			}

			if (dist[j] <= 0)
			{
				VectorCopy(v, back[b]);
				b++;
			}

			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;

			if (dist[j] > 0 != dist[j + 1] > 0)
			{
				// Clip point
				const float frac = dist[j] / (dist[j] - dist[j + 1]);

				for (int k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);

				f++;
				b++;
			}
		}

		SubdividePolygon(f, front[0]);
		SubdividePolygon(b, back[0]);

		return;
	}

	// Add a point in the center to help keep warp valid
	glpoly_t *poly = ModChunk_Alloc(sizeof(glpoly_t) + (numverts - 2) * VERTEXSIZE * sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts + 2;
	
	// Alloc vertex light fields
	const int size = poly->numverts * 3 * sizeof(byte);
	poly->vertexlight = ModChunk_Alloc(size);
	poly->vertexlightbase = ModChunk_Alloc(size);
	memset(poly->vertexlight, 0, size);
	memset(poly->vertexlightbase, 0, size);
	poly->vertexlightset = false;
	
	VectorClear(total);
	float total_s = 0;
	float total_t = 0;

	for (int i = 0; i < numverts; i++, verts += 3)
	{
		VectorCopy(verts, poly->verts[i + 1]);
		const float s = DotProduct(verts, warpface->texinfo->vecs[0]);
		const float t = DotProduct(verts, warpface->texinfo->vecs[1]);

		total_s += s;
		total_t += t;
		VectorAdd(total, verts, total);

		poly->verts[i + 1][3] = s;
		poly->verts[i + 1][4] = t;
	}

	VectorScale(total, 1.0 / (float)numverts, poly->verts[0]);
	VectorCopy(poly->verts[0], poly->center); // For vertex lighting
	poly->verts[0][3] = total_s / numverts;
	poly->verts[0][4] = total_t / numverts;

	// Copy first vertex to last
	memcpy(poly->verts[numverts + 1], poly->verts[1], sizeof(poly->verts[0]));
}

// Breaks a polygon up along axial 64 unit boundaries so that turbulent warps can be done reasonably.
void R_SubdivideSurface(msurface_t *fa)
{
	vec3_t verts[64];
	float *vec;

	warpface = fa;

	// Convert edges back to a normal polygon
	for (int i = 0; i < fa->numedges; i++)
	{
		const int lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;

		VectorCopy(vec, verts[i]);
	}

	SubdividePolygon(fa->numedges, verts[0]);
}

// mxd. Returns the number of verts, which will be created during warp polygon subdivision.
// Used in Mod_GetAllocSizeBrushModel.

size_t warppolyvertssize; // In bytes

void CalculateWarpPolygonVertsSize(int numverts, float *verts)
{
	vec3_t	mins, maxs;
	vec3_t	front[64], back[64];
	float	dist[64];
	
	if (numverts > 60)
		VID_Error(ERR_DROP, "SubdividePolygon: numverts = %i", numverts);

	BoundPoly(numverts, verts, mins, maxs);

	for (int i = 0; i < 3; i++)
	{
		float m = (mins[i] + maxs[i]) * 0.5;
		m = SUBDIVIDE_SIZE * floor(m / SUBDIVIDE_SIZE + 0.5);

		if (maxs[i] - m < 8 || m - mins[i] < 8)
			continue;

		// Cut it
		float *v = verts + i;
		for (int j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// Wrap cases
		dist[numverts] = dist[0];
		v -= i;
		VectorCopy(verts, v);

		int f = 0;
		int b = 0;
		v = verts;

		for (int j = 0; j < numverts; j++, v += 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy(v, front[f]);
				f++;
			}

			if (dist[j] <= 0)
			{
				VectorCopy(v, back[b]);
				b++;
			}

			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;

			if (dist[j] > 0 != dist[j + 1] > 0)
			{
				// Clip point
				const float frac = dist[j] / (dist[j] - dist[j + 1]);

				for (int k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);

				f++;
				b++;
			}
		}

		CalculateWarpPolygonVertsSize(f, front[0]);
		CalculateWarpPolygonVertsSize(b, back[0]);

		return;
	}

	// Add to total size...
	warppolyvertssize += sizeof(glpoly_t) + (numverts - 2) * VERTEXSIZE * sizeof(float); // Center point...
	warppolyvertssize += (numverts + 2) * 6 * sizeof(byte); // 2x vertex light fields
}

size_t R_GetWarpSurfaceVertsSize(dface_t *face, dvertex_t *vertexes, dedge_t *edges, int *surfedges)
{
	vec3_t verts[64];

	const int numedges = LittleShort(face->numedges);
	const int firstedge = LittleLong(face->firstedge);

	// Convert edges back to a normal polygon
	for (int i = 0; i < numedges; i++)
	{
		const int lindex = LittleLong(surfedges[firstedge + i]);
		
		int vindex;
		if (lindex > 0)
			vindex = (unsigned short)LittleShort(edges[lindex].v[0]);
		else
			vindex = (unsigned short)LittleShort(edges[-lindex].v[1]);

		for (int c = 0; c < 3; c++)
			verts[i][c] = LittleFloat(vertexes[vindex].point[c]);
	}

	warppolyvertssize = 0;
	CalculateWarpPolygonVertsSize(numedges, verts[0]);

	return warppolyvertssize;
}

//=========================================================

// MrG - texture shader stuffs
#define DST_SIZE 16
unsigned int dst_texture_NV, dst_texture_ARB;

// Create the texture which warps texture shaders
void CreateDSTTex_NV(void)
{
	char data[DST_SIZE][DST_SIZE][2];

	for (int x = 0; x < DST_SIZE; x++)
	{
		for (int y = 0; y < DST_SIZE; y++)
		{
			data[x][y][0] = rand() % 255 - 128;
			data[x][y][1] = rand() % 255 - 128;
		}
	}

	qglGenTextures(1, &dst_texture_NV);
	qglBindTexture(GL_TEXTURE_2D, dst_texture_NV);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_DSDT8_NV, DST_SIZE, DST_SIZE, 0, GL_DSDT_NV, GL_BYTE, data);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

// Create the texture which warps texture shaders
void CreateDSTTex_ARB(void)
{
	unsigned char dist[DST_SIZE][DST_SIZE][4];

	srand(GetTickCount());
	for (int x = 0; x < DST_SIZE; x++)
	{
		for (int y = 0; y < DST_SIZE; y++)
		{
			dist[x][y][0] = rand() % 255;
			dist[x][y][1] = rand() % 255;
			dist[x][y][2] = rand() % 48;
			dist[x][y][3] = rand() % 48;
		}
	}

	qglGenTextures(1,&dst_texture_ARB);
	qglBindTexture(GL_TEXTURE_2D, dst_texture_ARB);
	qglTexImage2D (GL_TEXTURE_2D, 0, 4, DST_SIZE, DST_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, dist);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
	qglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
}

// Resets the texture which warps texture shaders. Needed after a vid_restart.
void R_InitDSTTex(void)
{
	dst_texture_NV = dst_texture_ARB = 0;
	CreateDSTTex_NV();
	CreateDSTTex_ARB();
}
//end MrG

extern image_t *R_TextureAnimation(msurface_t *surf);

// Backend for R_DrawWarpSurface
void RB_RenderWarpSurface(msurface_t *fa)
{
	float		args[7] = {0, 0.05, 0, 0, 0.04, 0, 0};
	image_t		*image = R_TextureAnimation(fa);
	const qboolean	light = r_warp_lighting->value && !(fa->texinfo->flags & SURF_NOLIGHTENV);
	const qboolean	texShaderWarpNV = glConfig.NV_texshaders && glConfig.multitexture && r_pixel_shader_warp->value;
	qboolean		texShaderWarpARB = glConfig.arb_fragment_program && glConfig.multitexture && r_pixel_shader_warp->value;
	const qboolean	texShaderWarp = (texShaderWarpNV || texShaderWarpARB);
	
	if (texShaderWarpNV && texShaderWarpARB)
		texShaderWarpARB = (r_pixel_shader_warp->value == 1.0f);

	if (rb_vertex == 0 || rb_index == 0) // Nothing to render
		return;

	c_brush_calls++;

	// Psychospaz's vertex lighting
	if (light)
	{
		GL_ShadeModel(GL_SMOOTH);
		if (!texShaderWarp)
			R_SetVertexRGBScale(true);
	}

	// Texture Shader waterwarp - MrG
	if (texShaderWarpARB)
	{
		GL_SelectTexture(0);
		GL_MBind(0, image->texnum);

		GL_EnableTexture(1);
		GL_MBind(1, dst_texture_ARB);

		GL_Enable(GL_FRAGMENT_PROGRAM_ARB);
		qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fragment_programs[F_PROG_WARP]);
		qglProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, r_rgbscale->value, r_rgbscale->value, r_rgbscale->value, 1.0);
	}
	else if (texShaderWarpNV)
	{
		GL_SelectTexture(0);
		GL_MBind(0, dst_texture_NV);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);

		GL_EnableTexture(1);
		GL_MBind(1, image->texnum);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_OFFSET_TEXTURE_2D_NV);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
		qglTexEnvfv(GL_TEXTURE_SHADER_NV, GL_OFFSET_TEXTURE_MATRIX_NV, &args[1]);

		// Psychospaz's lighting
		// use this so that the new water isnt so bright anymore
		// We won't bother check for the extensions availabiliy, as the hardware required
		// to make it this far definately supports this as well
		if (light)
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);

		GL_Enable(GL_TEXTURE_SHADER_NV);
	}
	else
	{
		GL_Bind(image->texnum);
	}

	RB_DrawArrays();

	// MrG - texture shader waterwarp
	if (texShaderWarpARB)
	{
		GL_Disable (GL_FRAGMENT_PROGRAM_ARB);
		GL_DisableTexture(1);
		GL_SelectTexture(0);
	}
	else if (texShaderWarpNV)
	{ 
		GL_DisableTexture(1);
		if (light)
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); // Psychospaz's lighting

		GL_SelectTexture(0);
		GL_Disable (GL_TEXTURE_SHADER_NV);
	}

	// Psychospaz's vertex lighting
	if (light)
	{
		GL_ShadeModel (GL_FLAT);
		if (!texShaderWarp)
			R_SetVertexRGBScale(false); 
	}

	RB_DrawMeshTris();
	rb_vertex = rb_index = 0;
}

// Does a water warp on the pre-fragmented glpoly_t chain.
// added Psychospaz's lightmaps on alpha surfaces
void R_DrawWarpSurface(msurface_t *fa, float alpha, qboolean render)
{
	float scroll;
	const float rdt = r_newrefdef.time;
	vec3_t point;
	const qboolean light = r_warp_lighting->value && !r_fullbright->value && !(fa->texinfo->flags & SURF_NOLIGHTENV);
	const qboolean texShaderNV = glConfig.NV_texshaders && glConfig.multitexture
								&& ( (!glConfig.arb_fragment_program && r_pixel_shader_warp->value)
									|| (glConfig.arb_fragment_program && r_pixel_shader_warp->value > 1) );

	c_brush_surfs++;

	const float dstscroll = -64 * ( (r_newrefdef.time * 0.15) - (int)(r_newrefdef.time * 0.15) );

	if (fa->texinfo->flags & SURF_FLOWING)
		scroll = -64 * ( (r_newrefdef.time * 0.5) - (int)(r_newrefdef.time * 0.5) );
	else
		scroll = 0.0f;

	for (glpoly_t *bp = fa->polys; bp; bp = bp->next)
	{
		c_brush_polys += bp->numverts - 2;
		glpoly_t *p = bp;

		if (RB_CheckArrayOverflow(p->numverts, (p->numverts - 2) * 3))
			RB_RenderWarpSurface(fa);

		for (int i = 0; i < p->numverts - 2; i++)
		{
			indexArray[rb_index++] = rb_vertex;
			indexArray[rb_index++] = rb_vertex + i + 1;
			indexArray[rb_index++] = rb_vertex + i + 2;
		}

		float* v = p->verts[0];
		for (int i = 0; i < p->numverts; i++, v += VERTEXSIZE)
		{
			float s = v[3] + sinf(v[4] * 0.125f + rdt) * 4; //mxd. Replaced precalculated sin values with sinf()
			float t = v[4] + sinf(v[3] * 0.125f + rdt) * 4;

			s += scroll;
			s *= DIV64;
			t *= DIV64;

//=============== Water waves ========================
			VectorCopy(v, point);
			if (r_waterwave->value > 0 && !(fa->texinfo->flags & SURF_FLOWING)
				&& fa->plane->normal[2] > 0
				&& fa->plane->normal[2] > fa->plane->normal[0]
				&& fa->plane->normal[2] > fa->plane->normal[1])
			{
				point[2] = v[2] + r_waterwave->value * sinf(v[0] * 0.025f + rdt) * sinf(v[2] * 0.05f + rdt);
			}
//=============== End water waves ====================

			// MrG - texture shader waterwarp
			if (texShaderNV)
			{
				VA_SetElem2(texCoordArray[0][rb_vertex], (v[3] + dstscroll) * DIV64, v[4] * DIV64);
				VA_SetElem2(texCoordArray[1][rb_vertex], s, t);
			}
			else
			{
				VA_SetElem2(texCoordArray[0][rb_vertex], s, t);
				VA_SetElem2(texCoordArray[1][rb_vertex], (v[3] + dstscroll) * DIV64, v[4] * DIV64);
			}

			if (light && p->vertexlight && p->vertexlightset)
				VA_SetElem4(colorArray[rb_vertex],
					(float)(p->vertexlight[i * 3 + 0] * DIV255),
					(float)(p->vertexlight[i * 3 + 1] * DIV255),
					(float)(p->vertexlight[i * 3 + 2] * DIV255),
					alpha);
			else
				VA_SetElem4(colorArray[rb_vertex], glState.inverse_intensity, glState.inverse_intensity, glState.inverse_intensity, alpha);

			VA_SetElem3(vertexArray[rb_vertex], point[0], point[1], point[2]);

			rb_vertex++;
		}
	}

	if (render)
		RB_RenderWarpSurface(fa);
}