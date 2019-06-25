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

// r_light.c

#include "r_local.h"

static int r_dlightframecount; //mxd. +static

#pragma region ======================= DYNAMIC LIGHTS BLEND RENDERING

#define DLIGHT_RADIUS 16.0f // was 32.0

void R_AddDlight(dlight_t *light)
{
	const float rad = light->intensity * 0.35f;

	vec3_t v;
	VectorSubtract(light->origin, r_origin, v);

	for (int i = 0; i < 3; i++)
		v[i] = light->origin[i] - vpn[i] * rad;

	if (RB_CheckArrayOverflow(DLIGHT_RADIUS + 1, DLIGHT_RADIUS * 3))
		RB_RenderMeshGeneric(true);

	for (int i = 1; i <= DLIGHT_RADIUS; i++)
	{
		indexArray[rb_index++] = rb_vertex;
		indexArray[rb_index++] = rb_vertex + i;
		indexArray[rb_index++] = rb_vertex + 1 + (i < DLIGHT_RADIUS ? i : 0);
	}

	VA_SetElem3(vertexArray[rb_vertex], v[0], v[1], v[2]);
	VA_SetElem4(colorArray[rb_vertex], light->color[0] * 0.2f, light->color[1] * 0.2f, light->color[2] * 0.2f, 1.0f);
	rb_vertex++;

	for (int i = DLIGHT_RADIUS; i > 0; i--)
	{
		const float a = i / DLIGHT_RADIUS * M_PI * 2;
		for (int j = 0; j < 3; j++)
			v[j] = light->origin[j] + vright[j] * cosf(a) * rad + vup[j] * sinf(a) * rad;

		VA_SetElem3(vertexArray[rb_vertex], v[0], v[1], v[2]);
		VA_SetElem4(colorArray[rb_vertex], 0.0f, 0.0f, 0.0f, 1.0f);
		rb_vertex++;
	}
}

void R_RenderDlights(void)
{
	if (!r_flashblend->value)
		return;

	r_dlightframecount = r_framecount + 1; // Because the count hasn't advanced yet for this frame

	GL_DepthMask(0);
	GL_DisableTexture(0);
	GL_ShadeModel(GL_SMOOTH);
	GL_Enable(GL_BLEND);
	GL_BlendFunc(GL_ONE, GL_ONE);

	rb_vertex = 0;
	rb_index = 0;

	dlight_t *l = r_newrefdef.dlights;
	for (int i = 0; i < r_newrefdef.num_dlights; i++, l++)
		R_AddDlight(l);

	RB_RenderMeshGeneric(true);

	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_Disable(GL_BLEND);
	GL_ShadeModel(GL_FLAT);
	GL_EnableTexture(0);
	GL_DepthMask(1);
}

#pragma endregion

#pragma region ======================= DYNAMIC LIGHTS

extern cvar_t *r_dlights_normal;

void R_MarkLights(dlight_t *light, int num, mnode_t *node)
{
	if (node->contents != -1)
		return;

	cplane_t *splitplane = node->plane;
	float dist = DotProduct(light->origin, splitplane->normal) - splitplane->dist;
	
	if (dist > light->intensity - r_lightcutoff->value)	//** DMP var dynalight cutoff
	{
		R_MarkLights(light, num, node->children[0]);
		return;
	}

	if (dist < -light->intensity + r_lightcutoff->value) //** DMP var dynalight cutoff
	{
		R_MarkLights(light, num, node->children[1]);
		return;
	}

	// Mark the polygons
	msurface_t *surf = r_worldmodel->surfaces + node->firstsurface;
	for (int i = 0; i < node->numsurfaces; i++, surf++)
	{
		// Knightmare- Discoloda's dynamic light clipping
		if (r_dlights_normal->value)
		{
			dist = DotProduct(light->origin, surf->plane->normal) - surf->plane->dist;

			const int sidebit = (dist < 0 ? SURF_PLANEBACK : 0); //mxd
			if ((surf->flags & SURF_PLANEBACK) != sidebit)
				continue;
		}

		if (surf->dlightframe != r_dlightframecount)
		{
			memset(surf->dlightbits, 0, sizeof(surf->dlightbits));
			surf->dlightbits[num >> 5] = 1 << (num & 31); // Was 0, fixes hyperblaster tearing
			surf->dlightframe = r_dlightframecount;
		}
		else
		{
			surf->dlightbits[num >> 5] |= 1 << (num & 31);
		}
	}

	R_MarkLights(light, num, node->children[0]);
	R_MarkLights(light, num, node->children[1]);
}

void R_PushDlights(void)
{
	if (r_flashblend->value)
		return;

	r_dlightframecount = r_framecount + 1; // Because the count hasn't advanced yet for this frame

	dlight_t *l = r_newrefdef.dlights;
	for (int i = 0; i < r_newrefdef.num_dlights; i++, l++)
		R_MarkLights(l, i, r_worldmodel->nodes);
}

#pragma endregion 

#pragma region ======================= LIGHT SAMPLING

vec3_t lightspot;

int RecursiveLightPoint(vec3_t color, mnode_t *node, vec3_t start, vec3_t end)
{
	if (node->contents != -1)
		return -1; // Didn't hit anything
	
	// Calculate mid point

	// FIXME: optimize for axial
	cplane_t *plane = node->plane;
	const float front = DotProduct(start, plane->normal) - plane->dist;
	const float back = DotProduct(end, plane->normal) - plane->dist;
	const int side = (front < 0);
	
	if ((back < 0) == side)
		return RecursiveLightPoint(color, node->children[side], start, end);
	
	const float frac = front / (front - back);
	vec3_t mid;
	for(int i = 0; i < 3; i++) //mxd
		mid[i] = start[i] + (end[i] - start[i]) * frac;
	
	// Go down front side
	const int r = RecursiveLightPoint(color, node->children[side], start, mid);
	if (r > -1)
		return r; // Hit something
		
	if ((back < 0) == side)
		return -1; // Didn't hit anything
		
	// Check for impact on this node
	VectorCopy(mid, lightspot);

	msurface_t *surf = r_worldmodel->surfaces + node->firstsurface;

	for (int i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (!surf->samples || surf->light_smax == 0 || surf->light_tmax == 0) //mxd. Also skip surfaces with non-initialized lightmap size
			continue; //mxd. No lightmap data. Was return 0;
		
		if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY) || surf->texinfo->flags & SURF_NODRAW) //mxd. Also skip NODRAW surfaces 
			continue; // No lightmaps

		mtexinfo_t *tex = surf->texinfo;
		
		int ds = DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3];
		int dt = DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3];

		if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
			continue;
		
		ds -= surf->texturemins[0];
		dt -= surf->texturemins[1];
		
		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		byte *lightmap = surf->samples;
		lightmap += 3 * ((dt >> gl_lms.lmshift) * surf->light_smax + (ds >> gl_lms.lmshift)); //mxd. 4 -> lmshift

		if (r_newrefdef.lightstyles)
		{
			// LordHavoc: enhanced to interpolate lighting
			int r00 = 0, g00 = 0, b00 = 0, r01 = 0, g01 = 0, b01 = 0, r10 = 0, g10 = 0, b10 = 0, r11 = 0, g11 = 0, b11 = 0;
			const int dsfrac = ds & 15;
			const int dtfrac = dt & 15;
			const int line3 = surf->light_smax * 3;
			
			for (int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				vec3_t scale;
				for (int c = 0; c < 3; c++) //mxd. V535 The variable 'i' is being used for this loop and for the outer loop.
					scale[c] = r_newrefdef.lightstyles[surf->styles[maps]].rgb[c];

				r00 += (float)lightmap[0] * scale[0];
				g00 += (float)lightmap[1] * scale[1];
				b00 += (float)lightmap[2] * scale[2];

				r01 += (float)lightmap[3] * scale[0];
				g01 += (float)lightmap[4] * scale[1];
				b01 += (float)lightmap[5] * scale[2];

				r10 += (float)lightmap[line3 + 0] * scale[0];
				g10 += (float)lightmap[line3 + 1] * scale[1];
				b10 += (float)lightmap[line3 + 2] * scale[2];

				r11 += (float)lightmap[line3 + 3] * scale[0];
				g11 += (float)lightmap[line3 + 4] * scale[1];
				b11 += (float)lightmap[line3 + 5] * scale[2];

				lightmap += 3 * surf->light_smax * ((surf->extents[1] >> gl_lms.lmshift) + 1); //mxd. 4 -> lmshift
			}

			color[0] = DIV256 * (float)(int)(((((((r11 - r10) * dsfrac >> 4) + r10) - (((r01 - r00) * dsfrac >> 4) + r00)) * dtfrac) >> 4) + (((r01 - r00) * dsfrac >> 4) + r00));
			color[1] = DIV256 * (float)(int)(((((((g11 - g10) * dsfrac >> 4) + g10) - (((g01 - g00) * dsfrac >> 4) + g00)) * dtfrac) >> 4) + (((g01 - g00) * dsfrac >> 4) + g00));
			color[2] = DIV256 * (float)(int)(((((((b11 - b10) * dsfrac >> 4) + b10) - (((b01 - b00) * dsfrac >> 4) + b00)) * dtfrac) >> 4) + (((b01 - b00) * dsfrac >> 4) + b00));
		}
		
		return 1;
	}

	// Go down back side
	return RecursiveLightPoint(color, node->children[!side], mid, end);
}

void R_LightPoint(vec3_t p, vec3_t color, qboolean isEnt)
{
	if (!r_worldmodel->lightdata)
	{
		VectorSetAll(color, 1.0f);
		return;
	}

	vec3_t end = { p[0], p[1], p[2] - 8192 }; //mxd. Was p[2] - 2048
	const float r = RecursiveLightPoint(color, r_worldmodel->nodes, p, end);
	if (r == -1)
	{
		VectorClear(color);
	}
	else
	{
		// This catches too bright modulated color
		for (int i = 0; i < 3; i++)
			color[i] = min(color[i], 1);
	}

	// Add dynamic lights
	dlight_t *dl = r_newrefdef.dlights;
	for (int lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, dl++)
	{
		if (dl->spotlight) // Skip spotlights
			continue;

		vec3_t dist;
		if (isEnt)
			VectorSubtract(currententity->origin, dl->origin, dist);
		else
			VectorSubtract(p, dl->origin, dist);

		const float add = (dl->intensity - VectorLength(dist)) * DIV256;
		if (add > 0)
			VectorMA(color, add, dl->color, color);
	}
}

void R_LightPointDynamics(vec3_t p, vec3_t color, m_dlight_t *list, int *amount, int max)
{
	if (!r_worldmodel->lightdata)
	{
		VectorSetAll(color, 1.0f);
		return;
	}
	
	vec3_t end = { p[0], p[1], p[2] - 8192 }; //mxd. Was p[2] - 2048
	const float r = RecursiveLightPoint(color, r_worldmodel->nodes, p, end);
	if (r == -1)
	{
		VectorClear(color);
	}
	else
	{
		// This catches too bright modulated color
		for (int i = 0; i < 3; i++)
			color[i] = min(color[i], 1);
	}

	// Add dynamic lights
	int m_dl = 0;
	dlight_t *dl = r_newrefdef.dlights;
	for (int lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, dl++)
	{
		if (dl->spotlight) // Skip spotlights
			continue;

		vec3_t dist;
		VectorSubtract(dl->origin, p, dist);

		const float add = (dl->intensity - VectorNormalize(dist)) * DIV256;
		if (add > 0)
		{
			float highest = -1;

			vec3_t dlColor;
			VectorScale(dl->color, add, dlColor);
			for (int i = 0; i < 3; i++)
				highest = max(dlColor[i], highest);

			if (m_dl < max)
			{
				list[m_dl].strength = highest;
				VectorCopy(dist, list[m_dl].direction);
				VectorCopy(dlColor, list[m_dl].color);
				m_dl++;
			}
			else
			{
				float least_val = 10;
				int least_index = 0;

				for (int i = 0; i < m_dl; i++)
				{
					if (list[i].strength < least_val)
					{
						least_val = list[i].strength;
						least_index = i;
					}
				}

				VectorAdd(color, list[least_index].color, color);
				list[least_index].strength = highest;
				VectorCopy(dist, list[least_index].direction);
				VectorCopy(dlColor, list[least_index].color);
			}
		}
	}

	*amount = m_dl;
}

void R_SurfLightPoint(msurface_t *surf, vec3_t p, vec3_t color, qboolean baselight)
{
	if (!r_worldmodel->lightdata)
	{
		VectorSetAll(color, 1.0f);
		return;
	}

	if (baselight)
	{
		vec3_t end = { p[0], p[1], p[2] - 8192 }; //mxd. Was p[2] - 2048
		const float r = RecursiveLightPoint(color, r_worldmodel->nodes, p, end);
		if (r == -1)
		{
			VectorClear(color);
		}
		else
		{
			// This catches too bright modulated color
			for (int i = 0; i < 3; i++)
				color[i] = min(color[i], 1);
		}
	}
	else
	{
		VectorClear(color);

		// Knightmare- fix for moving surfaces
		vec3_t forward, right, up;
		qboolean rotated = false;
		entity_t *hostent = (surf ? surf->entity : NULL); //mxd
		if (hostent && (hostent->angles[0] || hostent->angles[1] || hostent->angles[2]))
		{
			rotated = true;
			AngleVectors(hostent->angles, forward, right, up);
		}

		// Add dynamic lights
		dlight_t *dl = r_newrefdef.dlights;
		for (int lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, dl++)
		{
			// Spotlight || no dlight casting
			if (dl->spotlight || r_flashblend->value || !r_dynamic->value) 
				continue;

			// Knightmare- fix for moving surfaces
			vec3_t dlorigin;
			VectorCopy(dl->origin, dlorigin);
			if (hostent)
				VectorSubtract(dlorigin, hostent->origin, dlorigin);

			if (rotated)
			{
				vec3_t temp;
				VectorCopy(dlorigin, temp);
				dlorigin[0] = DotProduct(temp, forward);
				dlorigin[1] = -DotProduct(temp, right);
				dlorigin[2] = DotProduct(temp, up);
			}

			vec3_t dist;
			VectorSubtract(p, dlorigin, dist);
			// end Knightmare

			const float add = (dl->intensity - VectorLength(dist)) * DIV256;
			if (add > 0)
				VectorMA(color, add, dl->color, color);
		}
	}
}

// Knightmare- added Psychospaz's dynamic light-based shadows
extern void vectoangles(vec3_t value1, vec3_t angles);

void R_ShadowLight(vec3_t pos, vec3_t lightAdd)
{
	vec3_t dist;

	if (!r_worldmodel || !r_worldmodel->lightdata) // Keep old lame shadow
		return;
	
	VectorClear(lightAdd);

	// Barnes improved code
	float shadowdist = VectorNormalize(lightAdd);
	if (shadowdist > 4)
		shadowdist = 4;

	if (shadowdist < 1) // Old-style static shadow
	{
		const float add = currententity->angles[1] / 180 * M_PI;
		dist[0] = cosf(-add);
		dist[1] = sinf(-add);
		dist[2] = 1;
		VectorNormalize(dist);
		shadowdist = 1;
	}
	else // Shadow from dynamic lights
	{
		vec3_t angle;
		vectoangles(lightAdd, angle);
		angle[YAW] -= currententity->angles[YAW];
		AngleVectors(angle, dist, NULL, NULL);
	}

	VectorScale(dist, shadowdist, lightAdd); 
}

#pragma endregion 

#pragma region ======================= DYNAMIC LIGHTS SETUP

static float s_blocklights[LM_BLOCK_WIDTH * LM_BLOCK_HEIGHT * 4]; //mxd. Was [128*128*4] //Knightmare-  was [34*34*3], supports max chop size of 2048?

void R_AddDynamicLights(msurface_t *surf)
{
	static byte s_castedrays[LM_BLOCK_WIDTH * LM_BLOCK_HEIGHT]; //mxd. 0 - not yet cast, 1 - not blocked, 2 - blocked
	
	vec3_t		dlorigin, entOrigin, entAngles;
	qboolean	rotated = false;
	vec3_t		forward, right, up;
	mtexinfo_t	*tex = surf->texinfo;

	//mxd. Shadowmapping vars setup start
	float dlscaler = 0.0f;
	qboolean castshadows = (r_dlightshadowmapscale->integer ? true : false);
	qboolean skiplastrowandcolumn = false;
	int numrays = 0;

	if (castshadows)
	{
		const int smscale = (int)pow(2, clamp(r_dlightshadowmapscale->integer, 0, 5) - 1); // Convert [0, 1, 2, 3, 4, 5] to [0, 1, 2, 4, 8, 16]
		const float dlscale = max(gl_lms.lmscale, smscale); // Can't have dynamic light scaled lower than lightmap scale
		dlscaler = gl_lms.lmscale / dlscale; // Ratio between ligthmap scale and dynamic light raycasting scale. Expected to be in (0 .. 1] range.
		skiplastrowandcolumn = (gl_lms.lmscale == 1 && gl_lms.lmscale == dlscaler); // Skip processing last row and column of the shadowmap when lmscale == 1, because it's shifted to match texture pixels in R_BuildPolygonFromSurface

		if (dlscaler > 0 && dlscaler < 1)
			numrays = ceilf(surf->light_smax * dlscaler) * ceilf(surf->light_tmax * dlscaler);
	}
	//mxd. Shadowmapping vars setup end

	// Currententity is not valid for trans surfaces
	if (tex->flags & (SURF_TRANS33 | SURF_TRANS66))
	{
		if (surf->entity)
		{
			VectorCopy(surf->entity->origin, entOrigin);
			VectorCopy(surf->entity->angles, entAngles);
		}
		else
		{
			VectorCopy(vec3_origin, entOrigin);
			VectorCopy(vec3_origin, entAngles);
		}
	}
	else
	{
		VectorCopy(currententity->origin, entOrigin);
		VectorCopy(currententity->angles, entAngles);
	}

	if (entAngles[0] || entAngles[1] || entAngles[2])
	{
		rotated = true;
		AngleVectors(entAngles, forward, right, up);
	}

	for (int lnum = 0; lnum < r_newrefdef.num_dlights; lnum++)
	{
		if ( !(surf->dlightbits[lnum >> 5] & 1U << (lnum & 31)) )
			continue; // Not lit by this light

		dlight_t *dl = &r_newrefdef.dlights[lnum];

		if (dl->spotlight) // Skip spotlights
			continue;

		VectorCopy(dl->origin, dlorigin);
		VectorSubtract(dlorigin, entOrigin, dlorigin);

		if (rotated)
		{
			vec3_t temp;
			VectorCopy(dlorigin, temp);
			dlorigin[0] = DotProduct(temp, forward);
			dlorigin[1] = -DotProduct(temp, right);
			dlorigin[2] = DotProduct(temp, up);
		}

		//mxd. Skip shadowcasting when a light is too far away from camera...
		if (castshadows)
		{
			vec3_t v;
			VectorSubtract(dlorigin, r_origin, v);
			const float dist_sq = VectorLengthSquared(v);

			castshadows = (dist_sq < r_dlightshadowrange->integer * r_dlightshadowrange->integer);

			if (castshadows && numrays > 0)
				memset(s_castedrays, 0, numrays);
		}

		//mxd. Get distance from the surface plane...
		float dl_planedist_sq = DotProduct(dlorigin, surf->plane->normal) - surf->plane->dist;
		dl_planedist_sq *= dl_planedist_sq;

		const float dl_intensity_sq = dl->intensity * dl->intensity;
		const float dl_intensity_inv_sq = (1.0f / dl_intensity_sq) * 255.0f;

		const float dl_minlight_sq = r_lightcutoff->value * r_lightcutoff->value; //** DMP var dynalight cutoff
		if (dl_intensity_sq < dl_minlight_sq || dl_planedist_sq > dl_intensity_sq)
			continue;

		// Setup pointers
		float *pfBL = s_blocklights;
		vec3_t *lightmap_point = surf->lightmap_points; //mxd
		vec3_t *normalmap_normal = surf->normalmap_normals; //mxd
		const qboolean usenormalmapping = (r_dlightnormalmapping->integer && normalmap_normal); //mxd

		int smax, tmax;
		if(skiplastrowandcolumn)
		{
			smax = surf->light_smax - 1;
			tmax = surf->light_tmax - 1;
		}
		else
		{
			smax = surf->light_smax;
			tmax = surf->light_tmax;
		}

		int index = 0;

		for (int t = 0; t < tmax; t++)
		{
			for (int s = 0; s < smax; s++, index++)
			{
				//mxd. Check distance between light and lightmap coord
				vec3_t lightdir;
				VectorSubtract(dlorigin, lightmap_point[index], lightdir);
				const float dist_sq = VectorLengthSquared(lightdir);

				if (dist_sq < dl_intensity_sq)
				{
					float light_scaler = (dl_intensity_sq - dist_sq) * dl_intensity_inv_sq;
					
					//mxd. Software-based shadowmapping... Kills performance when both lmscale and r_dlightshadowmapscale is 1...
					qboolean isshaded = false;
					if (castshadows)
					{
						if (numrays > 0) // 1 ray per r_dlightshadowmapscale x r_dlightshadowmapscale texels
						{
							const int rayindex = (int)(t * dlscaler) * (int)ceilf(smax * dlscaler) + (int)(s * dlscaler);
							byte *castedray = &s_castedrays[rayindex];

							if (*castedray == 2) // 2 - blocked
							{
								isshaded = true;
							}
							else if (*castedray == 0) // 0 - not yet cast
							{
								const trace_t tr = CM_BoxTrace(dlorigin, lightmap_point[index], vec3_origin, vec3_origin, 0, CONTENTS_SOLID);
								*castedray = (tr.fraction < 1.0f ? 2 : 1);
								isshaded = (tr.fraction < 1.0f);
							}
						}
						else
						{
							const trace_t tr = CM_BoxTrace(dlorigin, lightmap_point[index], vec3_origin, vec3_origin, 0, CONTENTS_SOLID);
							isshaded = (tr.fraction < 1.0f);
						}
					}

					//mxd. Apply normalmapping?
					if(!isshaded && usenormalmapping)
					{
						//VectorNormalizeFast(lightdir);
						//const float nmapscaler = DotProduct(lightdir, normalmap_normal[index]);

						// Somewhat faster version of the above code...
						const float ilength = Q_rsqrt(dist_sq);
						const float nmapscaler = lightdir[0] * ilength * normalmap_normal[index][0]
											   + lightdir[1] * ilength * normalmap_normal[index][1]
											   + lightdir[2] * ilength * normalmap_normal[index][2];

						if (nmapscaler > 0)
							light_scaler *= nmapscaler;
						else
							isshaded = true;
					}

					if(!isshaded)
						for (int c = 0; c < 3; c++)
							pfBL[index * 3 + c] += dl->color[c] * light_scaler;
				}
			}

			if (skiplastrowandcolumn) //mxd
				index++;
		}
	}
}

void R_SetCacheState(msurface_t *surf)
{
	for (int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		surf->cached_light[maps] = r_newrefdef.lightstyles[surf->styles[maps]].white;

#ifdef BATCH_LM_UPDATES
	// Mark if dynamicly lit
	surf->cached_dlight = (surf->dlightframe == r_framecount);
#endif
}

// Combine and scale multiple lightmaps into the floating format in blocklights
void R_BuildLightMap(msurface_t *surf, byte *dest, int stride)
{
	if (surf->texinfo->flags & (SURF_SKY | SURF_WARP))
		VID_Error(ERR_DROP, "R_BuildLightMap called for non-lit surface");

	const int smax = surf->light_smax;
	const int tmax = surf->light_tmax;
	int size = smax * tmax;
	
	// FIXME- can this limit be directly increased?		Yep - Knightmare
	if (size > sizeof(s_blocklights) >> gl_lms.lmshift) //mxd. 4 -> lmshift
		VID_Error(ERR_DROP, "Bad s_blocklights size: %d", size);

	qboolean copysamples = false; //mxd
	size *= 3; //mxd

	if (surf->samples)
	{
		// Count the # of maps
		int nummaps = 0;
		while (nummaps < MAXLIGHTMAPS && surf->styles[nummaps] != 255)
			nummaps++;

		float scale[3];
		byte *lightmap = surf->samples;

		// Add all the lightmaps
		if (nummaps == 1)
		{
			// Single lightstyle...
			for (int i = 0; i < 3; i++)
				scale[i] = r_newrefdef.lightstyles[surf->styles[0]].rgb[i];

			if (scale[0] == 1.0f && scale[1] == 1.0f && scale[2] == 1.0f)
			{
				copysamples = (surf->dlightframe != r_framecount); //mxd
				if (!copysamples) //mxd. Skip if surf->samples can be copied directly to dest... 
					for (int i = 0; i < size; i++)
						s_blocklights[i] = lightmap[i];
			}
			else
			{
				for (int i = 0; i < size; i += 3)
					for (int c = 0; c < 3; c++)
						s_blocklights[i + c] = lightmap[i + c] * scale[c];
			}
		}
		else
		{
			// Multiple lightstyles...
			memset(s_blocklights, 0, sizeof(s_blocklights[0]) * size);

			for (int maps = 0; maps < nummaps; maps++)
			{
				for (int i = 0; i < 3; i++)
					scale[i] = r_newrefdef.lightstyles[surf->styles[maps]].rgb[i];

				if (scale[0] == 1.0f && scale[1] == 1.0f && scale[2] == 1.0f)
				{
					for (int i = 0; i < size; i ++)
						s_blocklights[i] += lightmap[i];
				}
				else
				{
					for (int i = 0; i < size; i += 3)
						for (int c = 0; c < 3; c++)
							s_blocklights[i + c] += lightmap[i + c] * scale[c];
				}

				lightmap += size; // Skip to next lightmap
			}
		}

		// Add all the dynamic lights
		if (surf->dlightframe == r_framecount)
			R_AddDynamicLights(surf);
	}
	else
	{
		// Set to full bright if no light data
		memset(s_blocklights, 255, sizeof(s_blocklights[0]) * size); //mxd
	}

	// Put into texture format
	stride -= smax << 2;
	float *bl = s_blocklights;

	int li = 0; //mxd. lightmap index
	int di = 0; //mxd. dest index

	if(copysamples) //mxd. Copy surf->samples to dest
	{
		byte *lightmap = surf->samples;

		for (int i = 0; i < tmax; i++, di += stride)
		{
			for (int j = 0; j < smax; j++, li += 3, di += 4)
			{
				if (gl_lms.format == GL_BGRA)
				{
					dest[di + 0] = lightmap[li + 2]; //b
					dest[di + 1] = lightmap[li + 1]; //g
					dest[di + 2] = lightmap[li + 0]; //r
				}
				else
				{
					dest[di + 0] = lightmap[li + 0]; //r
					dest[di + 1] = lightmap[li + 1]; //g
					dest[di + 2] = lightmap[li + 2]; //b
				}

				dest[di + 3] = 255; //a
			}
		}

		return;
	}

	for (int i = 0; i < tmax; i++, di += stride)
	{
		for (int j = 0; j < smax; j++, li += 3, di += 4)
		{
			int r = (int)bl[li + 0]; //Q_ftol( bl[0] ); //mxd. Direct cast is actually faster...
			int g = (int)bl[li + 1]; //Q_ftol( bl[1] );
			int b = (int)bl[li + 2]; //Q_ftol( bl[2] );

			// Catch negative lights
			if (r < 0) r = 0;
			if (g < 0) g = 0;
			if (b < 0) b = 0;

			// Determine the brightest of the three color components
			if(r > 255 || g > 255 || b > 255)
			{
				int max = 255;
				if (r > max) max = r;
				if (g > max) max = g;
				if (b > max) max = b;

				// Rescale all the color components if the intensity of the greatest channel exceeds 1.0
				if (max > 255)
				{
					const float t = 255.0f / max;

					r *= t;
					g *= t;
					b *= t;
				}
			}

			// Store
			if (gl_lms.format == GL_BGRA)
			{
				dest[di + 0] = b; //b
				dest[di + 1] = g; //g
				dest[di + 2] = r; //b
			}
			else
			{
				dest[di + 0] = r; //r
				dest[di + 1] = g; //g
				dest[di + 2] = b; //b
			}

			dest[di + 3] = 255; //a
		}
	}
}

#pragma endregion