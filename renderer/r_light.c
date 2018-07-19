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

int	r_dlightframecount;

void vectoangles (vec3_t value1, vec3_t angles);

//#define	DLIGHT_CUTOFF	64	// Knightmare- no longer hard-coded

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

#define DLIGHT_RADIUS 16.0 // was 32.0

/*
=============
R_AddDlight
=============
*/
void R_AddDlight (dlight_t *light)
{
	const float rad = light->intensity * 0.35;

	vec3_t v;
	VectorSubtract(light->origin, r_origin, v);

	for (int i = 0; i < 3; i++)
		v[i] = light->origin[i] - vpn[i]*rad;

	if (RB_CheckArrayOverflow(DLIGHT_RADIUS + 1, DLIGHT_RADIUS * 3)) 
		RB_RenderMeshGeneric(true);

	for (int i = 1; i <= DLIGHT_RADIUS; i++)
	{
		indexArray[rb_index++] = rb_vertex;
		indexArray[rb_index++] = rb_vertex + i;
		indexArray[rb_index++] = rb_vertex + 1 + (i < DLIGHT_RADIUS ? i : 0);
	}

	VA_SetElem3(vertexArray[rb_vertex], v[0], v[1], v[2]);
	VA_SetElem4(colorArray[rb_vertex], light->color[0] * 0.2, light->color[1] * 0.2, light->color[2] * 0.2, 1.0);
	rb_vertex++;

	for (int i = DLIGHT_RADIUS; i > 0; i--)
	{
		const float a = i / DLIGHT_RADIUS * M_PI * 2;
		for (int j = 0; j < 3; j++)
			v[j] = light->origin[j] + vright[j] * cosf(a) * rad + vup[j] * sinf(a) * rad;

		VA_SetElem3(vertexArray[rb_vertex], v[0], v[1], v[2]);
		VA_SetElem4(colorArray[rb_vertex], 0, 0, 0, 1.0);
		rb_vertex++;
	}
}

/*
=============
R_RenderDlights
=============
*/
void R_RenderDlights (void)
{
	if (!r_flashblend->value)
		return;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't advanced yet for this frame

	GL_DepthMask(0);
	GL_DisableTexture(0);
	GL_ShadeModel(GL_SMOOTH);
	GL_Enable(GL_BLEND);
	GL_BlendFunc(GL_ONE, GL_ONE);

	rb_vertex = rb_index = 0;

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


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights
=============
*/
extern cvar_t *r_dlights_normal;
void R_MarkLights (dlight_t *light, int num, mnode_t *node)
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

	if (dist < -light->intensity + r_lightcutoff->value)	//** DMP var dynalight cutoff
	{
		R_MarkLights(light, num, node->children[1]);
		return;
	}
		
	// mark the polygons
	msurface_t *surf = r_worldmodel->surfaces + node->firstsurface;
	for (int i = 0; i < node->numsurfaces; i++, surf++)
	{
		// Knightmare- Discoloda's dynamic light clipping
		if (r_dlights_normal->value)
		{
			dist = DotProduct (light->origin, surf->plane->normal) - surf->plane->dist;

			const int sidebit = (dist < 0 ? SURF_PLANEBACK : 0); //mxd
			if ((surf->flags & SURF_PLANEBACK) != sidebit)
				continue;
		}
		// end Knightmare

		if (surf->dlightframe != r_dlightframecount)
		{
			memset(surf->dlightbits, 0, sizeof(surf->dlightbits));
			surf->dlightbits[num >> 5] = 1 << (num & 31);	// was 0, fixes hyperblaster tearing
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


/*
=============
R_PushDlights
=============
*/
void R_PushDlights (void)
{
	if (r_flashblend->value)
		return;

	r_dlightframecount = r_framecount + 1; // because the count hasn't advanced yet for this frame

	dlight_t *l = r_newrefdef.dlights;
	for (int i = 0; i < r_newrefdef.num_dlights; i++, l++)
		R_MarkLights(l, i, r_worldmodel->nodes);
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

vec3_t		pointcolor;
cplane_t	*lightplane; // used as shadow plane
vec3_t		lightspot;

/*
===============
RecursiveLightPoint
===============
*/
int RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
	if (node->contents != -1)
		return -1;		// didn't hit anything
	
	// calculate mid point

	// FIXME: optimize for axial
	cplane_t *plane = node->plane;
	const float front = DotProduct(start, plane->normal) - plane->dist;
	const float back = DotProduct(end, plane->normal) - plane->dist;
	const int side = (front < 0);
	
	if ((back < 0) == side)
		return RecursiveLightPoint(node->children[side], start, end);
	
	const float frac = front / (front-back);
	vec3_t mid;
	for(int i = 0; i < 3; i++) //mxd
		mid[i] = start[i] + (end[i] - start[i]) * frac;
	
	// go down front side	
	const int r = RecursiveLightPoint(node->children[side], start, mid);
	if (r > -1)
		return r;		// hit something
		
	if ((back < 0) == side)
		return -1;		// didn't hit anything
		
	// check for impact on this node
	VectorCopy(mid, lightspot);
	lightplane = plane;

	msurface_t *surf = r_worldmodel->surfaces + node->firstsurface;
	if (!surf->samples)
		return 0;

	for (int i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY)) 
			continue;	// no lightmaps

		mtexinfo_t *tex = surf->texinfo;
		
		int ds = DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3];
		int dt = DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3];

		if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
			continue;
		
		ds -= surf->texturemins[0];
		dt -= surf->texturemins[1];
		
		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		ds >>= surf->lmshift; //mxd. Was 4
		dt >>= surf->lmshift; //mxd. Was 4

		byte *lightmap = surf->samples;
		lightmap += 3 * (dt * ((surf->extents[0] >> surf->lmshift) + 1) + ds); //mxd. 4 -> lmshift

		VectorCopy(vec3_origin, pointcolor);
		if (r_newrefdef.lightstyles)
		{
			for (int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				vec3_t scale;
				for (int c = 0; c < 3; c++) //mxd. V535 The variable 'i' is being used for this loop and for the outer loop.
					scale[c] = r_modulate->value * r_newrefdef.lightstyles[surf->styles[maps]].rgb[c];

				for (int c = 0; c < 3; c++) //mxd
					pointcolor[c] += lightmap[c] * scale[c] * DIV255;

				lightmap += 3 * ((surf->extents[0] >> surf->lmshift) + 1) * ((surf->extents[1] >> surf->lmshift) + 1); //mxd. 4 -> lmshift
			}
		}
		
		return 1;
	}

	// go down back side
	return RecursiveLightPoint(node->children[!side], mid, end);
}


/*
===============
R_MaxColorVec

Psychospaz's lighting on alpha surfaces
===============
*/
void R_MaxColorVec (vec3_t color)
{
	float brightest = 0.0f;

	for (int i = 0; i < 3; i++)
		brightest = max(color[i], brightest); //mxd
	
	if (brightest > 255)
		VectorScale(color, 255 / brightest, color); //mxd

	for (int i = 0; i < 3; i++)
		color[i] = clamp(color[i], 0, 1); //mxd
}


/*
===============
R_LightPoint
===============
*/
void R_LightPoint (vec3_t p, vec3_t color, qboolean isEnt)
{
	if (!r_worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 1.0;
		return;
	}

	vec3_t end = { p[0], p[1], p[2] - 2048 };
	const float r = RecursiveLightPoint(r_worldmodel->nodes, p, end);
	
	if (r == -1)
		VectorCopy(vec3_origin, color);
	else
		VectorCopy(pointcolor, color);

	// this catches too bright modulated color
	for (int i = 0; i < 3; i++)
		if (color[i] > 1) color[i] = 1;

	// add dynamic lights
	dlight_t *dl = r_newrefdef.dlights;
	for (int lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, dl++)
	{
		if (dl->spotlight) // spotlights
			continue;

		vec3_t dist;
		if (isEnt)
			VectorSubtract(currententity->origin, dl->origin, dist);
		else
			VectorSubtract(p, dl->origin, dist);

		float add = dl->intensity - VectorLength(dist);
		add *= DIV256;
		if (add > 0)
			VectorMA(color, add, dl->color, color);
	}
}


/*
===============
R_LightPointDynamics
===============
*/
void R_LightPointDynamics (vec3_t p, vec3_t color, m_dlight_t *list, int *amount, int max)
{
	if (!r_worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 1.0;
		return;
	}
	
	vec3_t end = { p[0], p[1], p[2] - 2048 };
	const float r = RecursiveLightPoint (r_worldmodel->nodes, p, end);
	
	if (r == -1)
		VectorCopy(vec3_origin, color);
	else
		VectorCopy(pointcolor, color);

	//this catches too bright modulated color
	for (int i = 0; i < 3; i++)
		if (color[i] > 1) color[i] = 1;

	// add dynamic lights
	int m_dl = 0;
	dlight_t *dl = r_newrefdef.dlights;
	for (int lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, dl++)
	{
		if (dl->spotlight) // spotlights
			continue;

		vec3_t dist;
		VectorSubtract(dl->origin, p, dist);

		float add = dl->intensity - VectorNormalize(dist);
		add *= DIV256;
		if (add > 0)
		{
			float highest = -1;

			vec3_t dlColor;
			VectorScale(dl->color, add, dlColor);
			for (int i = 0; i < 3; i++)
				if (highest < dlColor[i]) highest = dlColor[i];

			if (m_dl < max)
			{
				list[m_dl].strength = highest;
				VectorCopy(dist, list[m_dl].direction);
				VectorCopy(dlColor, list[m_dl].color);
				m_dl++;
			}
			else
			{
				float	least_val = 10;
				int		least_index = 0;

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


/*
===============
R_SurfLightPoint
===============
*/
void R_SurfLightPoint (msurface_t *surf, vec3_t p, vec3_t color, qboolean baselight)
{
	static const vec3_t startOffset[4] = { {0, 0, 0}, {-1, 0, 0}, {0, -1, 0}, {-1, -1, 0} };
	
	if (!r_worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 1.0;
		return;
	}

	if (baselight)
	{
		vec3_t start, end;
		float r = 0;

		for (int i = 0; i < 4; i++) // test multiple points to avoid dark corners
		{
			VectorCopy(p, start);
			VectorAdd(start, startOffset[i], start);
			end[0] = start[0];
			end[1] = start[1];
			end[2] = start[2] - 2048;

			r = RecursiveLightPoint(r_worldmodel->nodes, start, end);

			if (r != -1)
				break;
		}

		if (r == -1)
			VectorCopy(vec3_origin, color);
		else
			VectorCopy(pointcolor, color);

		// this catches too bright modulated color
		for (int i = 0; i < 3; i++)
			if (color[i] > 1) color[i] = 1;
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

		// add dynamic lights
		dlight_t *dl = r_newrefdef.dlights;
		for (int lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, dl++)
		{
			// spotlights || no dlight casting
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

			float add = dl->intensity - VectorLength(dist);
			add *= DIV256;
			if (add > 0)
				VectorMA(color, add, dl->color, color);
		}
	}
}

//===================================================================
// Knightmare- added Psychospaz's dynamic light-based shadows

/*
===============
R_ShadowLight
===============
*/
void R_ShadowLight (vec3_t pos, vec3_t lightAdd)
{
	vec3_t dist;

	if (!r_worldmodel || !r_worldmodel->lightdata) // keep old lame shadow
		return;
	
	VectorClear(lightAdd);

	// add dynamic light shadow angles
	if (r_shadows->value == 2)
	{
		dlight_t *dl = r_newrefdef.dlights;
		for (int lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, dl++)
		{
			if (dl->spotlight) //spotlights
				continue;

			VectorSubtract(dl->origin, pos, dist);
			const float add = 0.2f * sqrtf(dl->intensity - VectorLength(dist));
			if (add > 0)
			{
				VectorNormalize(dist);
				VectorScale(dist, add, dist);
				VectorAdd(lightAdd, dist, lightAdd);
			}
		}
	}

	// Barnes improved code
	float shadowdist = VectorNormalize(lightAdd);
	if (shadowdist > 4)
		shadowdist = 4;

	if (shadowdist < 1) // old style static shadow
	{
		const float add = currententity->angles[1] / 180 * M_PI;
		dist[0] = cosf(-add);
		dist[1] = sinf(-add);
		dist[2] = 1;
		VectorNormalize(dist);
		shadowdist = 1;
	}
	else // shadow from dynamic lights
	{
		vec3_t angle;
		vectoangles(lightAdd, angle);
		angle[YAW] -= currententity->angles[YAW];
		AngleVectors(angle, dist, NULL, NULL);
	}

	VectorScale(dist, shadowdist, lightAdd); 
}

//===================================================================

static float s_blocklights[128 * 128 * 4]; //Knightmare-  was [34*34*3], supports max chop size of 2048?
/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (msurface_t *surf)
{
	vec3_t		impact, local, dlorigin, entOrigin, entAngles;
	qboolean	rotated = false;
	vec3_t		forward, right, up;
	const int	lmscale = 1 << surf->lmshift; //mxd

	const int smax = (surf->extents[0] >> surf->lmshift) + 1; //mxd. 4 -> lmshift
	const int tmax = (surf->extents[1] >> surf->lmshift) + 1; //mxd. 4 -> lmshift
	mtexinfo_t *tex = surf->texinfo;

	// currententity is not valid for trans surfaces
	if (tex->flags & (SURF_TRANS33|SURF_TRANS66))
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
		AngleVectors (entAngles, forward, right, up);
	}

	for (int lnum = 0; lnum < r_newrefdef.num_dlights; lnum++)
	{
		if ( !(surf->dlightbits[lnum >> 5] & (1U << (lnum & 31)) ) )
			continue; // not lit by this light

		dlight_t *dl = &r_newrefdef.dlights[lnum];

		if (dl->spotlight) //spotlights
			continue;

		float frad = dl->intensity;

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

		float fdist = DotProduct(dlorigin, surf->plane->normal) - surf->plane->dist;
		frad -= fabsf(fdist);
		// rad is now the highest intensity on the plane

		float fminlight = r_lightcutoff->value; 	//** DMP var dynalight cutoff
		if (frad < fminlight)
			continue;

		fminlight = frad - fminlight;

		for (int i = 0; i < 3; i++)
			impact[i] = dlorigin[i] - surf->plane->normal[i] * fdist;

		local[0] = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];

		float *pfBL = s_blocklights;
		for (int t = 0; t < tmax; t++)
		{
			int td = local[1] - t * lmscale;
			if (td < 0)
				td = -td;

			for (int s = 0; s < smax; s++, pfBL += 3)
			{
				int sd = local[0] - s * lmscale; //mxd. Was Q_ftol(local[0] - fsacc); Why Q_ftol?

				if (sd < 0)
					sd = -sd;

				if (sd > td)
					fdist = sd + (td >> 1);
				else
					fdist = td + (sd >> 1);

				if (fdist < fminlight)
				{
					for(int c = 0; c < 3; c++)
						pfBL[c] += (frad - fdist) * dl->color[c];
				}
			}
		}
	}
}


/*
===============
R_SetCacheState
===============
*/
void R_SetCacheState (msurface_t *surf)
{
	for (int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		surf->cached_light[maps] = r_newrefdef.lightstyles[surf->styles[maps]].white;

#ifdef BATCH_LM_UPDATES
	// mark if dynamicly lit
	surf->cached_dlight = (surf->dlightframe == r_framecount);
#endif
}


/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the floating format in blocklights
===============
*/
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int			r, g, b, a, max;
	float		scale[4];
	int			nummaps;
	float		*bl;

	if (surf->texinfo->flags & (SURF_SKY | SURF_WARP))
		VID_Error(ERR_DROP, "R_BuildLightMap called for non-lit surface");

	const int smax = (surf->extents[0] >> surf->lmshift) + 1; //mxd. 4 -> lmshift
	const int tmax = (surf->extents[1] >> surf->lmshift) + 1; //mxd. 4 -> lmshift
	const int size = smax * tmax;

	// FIXME- can this limit be directly increased?		Yep - Knightmare
	if (size > sizeof(s_blocklights) >> 4)
		VID_Error(ERR_DROP, "Bad s_blocklights size: %d", size);

	// set to full bright if no light data
	if (!surf->samples)
	{
		for (int i = 0; i < size * 3; i++)
			s_blocklights[i] = 255;

		goto store;
	}

	// count the # of maps
	for (nummaps = 0; nummaps < MAXLIGHTMAPS && surf->styles[nummaps] != 255; nummaps++)
		;

	byte *lightmap = surf->samples;

	// add all the lightmaps
	if(nummaps != 1) //mxd
		memset(s_blocklights, 0, sizeof(s_blocklights[0]) * size * 3);

	for (int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
	{
		bl = s_blocklights;

		for (int i = 0; i < 3; i++)
			scale[i] = r_modulate->value * r_newrefdef.lightstyles[surf->styles[maps]].rgb[i];

		for (int i = 0; i < size; i++, bl += 3) //mxd
		{
			if (nummaps == 1)
				VectorClear(bl);
			
			for (int c = 0; c < 3; c++)
				bl[c] += lightmap[i * 3 + c] * scale[c];
		}

		lightmap += size * 3; // skip to next lightmap
	}

	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights(surf);

	// put into texture format
store:
	stride -= smax << 2;
	bl = s_blocklights;

	const int monolightmap = r_monolightmap->string[0]; //mxd. //TODO: get rid of this. Nobody cares about PowerVR anymore

	if ( monolightmap == '0' )
	{
		for (int i=0 ; i<tmax ; i++, dest += stride)
		{
			for (int j=0 ; j<smax ; j++)
			{
				
				r = Q_ftol( bl[0] );
				g = Q_ftol( bl[1] );
				b = Q_ftol( bl[2] );

				// catch negative lights
				if (r < 0)
					r = 0;
				if (g < 0)
					g = 0;
				if (b < 0)
					b = 0;

				//
				// determine the brightest of the three color components
				//
				if (r > g)
					max = r;
				else
					max = g;
				if (b > max)
					max = b;

				//
				// alpha is ONLY used for the mono lightmap case.  For this reason
				// we set it to the brightest of the color components so that 
				// things don't get too dim.
				//
				//a = max;

				//
				// rescale all the color components if the intensity of the greatest
				// channel exceeds 1.0
				//
				if (max > 255)
				{
					float t = 255.0F / max;

					r = r*t;
					g = g*t;
					b = b*t;
					//a = a*t;
				}
				a = 255;	// fix for alpha test

				if (gl_lms.format == GL_BGRA)
				{
					dest[0] = b;
					dest[1] = g;
					dest[2] = r;
					dest[3] = a;
				}
				else
				{
					dest[0] = r;
					dest[1] = g;
					dest[2] = b;
					dest[3] = a;
				}

				bl += 3;
				dest += 4;
			}
		}
	}
	else
	{
		for (int i=0 ; i<tmax ; i++, dest += stride)
		{
			for (int j=0 ; j<smax ; j++)
			{
				
				r = Q_ftol( bl[0] );
				g = Q_ftol( bl[1] );
				b = Q_ftol( bl[2] );

				// catch negative lights
				if (r < 0)
					r = 0;
				if (g < 0)
					g = 0;
				if (b < 0)
					b = 0;

				//
				// determine the brightest of the three color components
				//
				if (r > g)
					max = r;
				else
					max = g;
				if (b > max)
					max = b;

				//
				// alpha is ONLY used for the mono lightmap case.  For this reason
				// we set it to the brightest of the color components so that 
				// things don't get too dim.
				//
				a = max;

				//
				// rescale all the color components if the intensity of the greatest
				// channel exceeds 1.0
				//
				if (max > 255)
				{
					float t = 255.0F / max;

					r = r*t;
					g = g*t;
					b = b*t;
					a = a*t;
				}

				//
				// So if we are doing alpha lightmaps we need to set the R, G, and B
				// components to 0 and we need to set alpha to 1-alpha.
				//
				switch ( monolightmap )
				{
				case 'L':
				case 'I':
					r = a;
					g = b = 0;
					a = 255;	// fix for alpha test
					break;
				case 'C':
					// try faking colored lighting
					a = 255 - ((r+g+b)/3); //Knightmare changed
					r *= a*0.003921568627450980392156862745098; // /255.0;
					g *= a*0.003921568627450980392156862745098; // /255.0;
					b *= a*0.003921568627450980392156862745098; // /255.0;
					a = 255;	// fix for alpha test
					break;
				case 'A':
				//	r = g = b = 0;
					a = 255 - a;
					r = g = b = a;
					break;
				default:
					r = g = b = a;
					a = 255;	// fix for alpha test
					break;
				}

				if (gl_lms.format == GL_BGRA)
				{
					dest[0] = b;
					dest[1] = g;
					dest[2] = r;
					dest[3] = a;
				}
				else
				{
					dest[0] = r;
					dest[1] = g;
					dest[2] = b;
					dest[3] = a;
				}

				bl += 3;
				dest += 4;
			}
		}
	}
}
