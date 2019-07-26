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

// r_alias.c: alias triangle model functions

#include "r_local.h"
#include "vlights.h"

static vec3_t *tempVertexArray[MD3_MAX_MESHES];

static void R_LightAliasModel(vec3_t baselight, vec3_t normal, vec3_t lightOut, byte normalindex, qboolean shaded)
{
	if (r_fullbright->integer)
	{
		VectorSet(lightOut, 1.0f, 1.0f, 1.0f);
		return;
	}

	if (r_model_shading->integer)
	{
		if (shaded)
		{
			float lightscaler;
			if (r_model_shading->integer == 3)
				lightscaler = 2.0f * shadedots[normalindex] - 1;
			else if (r_model_shading->integer == 2)
				lightscaler = 1.5f * shadedots[normalindex] - 0.5f;
			else
				lightscaler = shadedots[normalindex];

			VectorScale(baselight, lightscaler, lightOut);
		}
		else
		{
			VectorCopy(baselight, lightOut);
		}

		if (model_dlights_num)
		{
			for (int i = 0; i < model_dlights_num; i++)
			{
				const float lightscaler = 2.0f * VLight_GetLightValue(normal, model_dlights[i].direction, currententity->angles[PITCH], currententity->angles[YAW], true);
				VectorMA(lightOut, lightscaler, model_dlights[i].color, lightOut);
			}
		}
	}
	else
	{
		VectorCopy(baselight, lightOut);
	}

	for (int i = 0; i < 3; i++)
		lightOut[i] = clamp(lightOut[i], 0.0f, 1.0f);
}

static qboolean R_AliasMeshesAreBatchable(maliasmodel_t *paliashdr, uint meshnum1, uint meshnum2, uint skinnum)
{
	if (!paliashdr)
		return false;

	maliasmesh_t *mesh1 = &paliashdr->meshes[meshnum1];
	maliasmesh_t *mesh2 = &paliashdr->meshes[meshnum2];

	if (!mesh1 || !mesh2)
		return false;

	const int skinnum1 = (skinnum < mesh1->num_skins ? skinnum : 0);
	const int skinnum2 = (skinnum < mesh2->num_skins ? skinnum : 0);
	renderparms_t *skinParms1 = &mesh1->skins[skinnum1].renderparms;
	renderparms_t *skinParms2 = &mesh2->skins[skinnum2].renderparms;

	if (currentmodel->skins[meshnum1][skinnum1] != currentmodel->skins[meshnum2][skinnum2])
		return false;

	if (mesh1->skins[skinnum1].glowimage != mesh2->skins[skinnum2].glowimage)
		return false;

	if (skinParms1->alphatest != skinParms2->alphatest)
		return false;

	if (skinParms1->basealpha != skinParms2->basealpha)
		return false;

	if (skinParms1->blend != skinParms2->blend)
		return false;

	if (skinParms1->blendfunc_src != skinParms2->blendfunc_src)
		return false;

	if (skinParms1->blendfunc_dst != skinParms2->blendfunc_dst)
		return false;

	if (skinParms1->envmap != skinParms2->envmap)
		return false;

	if ( (skinParms1->glow.type != skinParms2->glow.type)
		|| (skinParms1->glow.params[0] != skinParms2->glow.params[0])
		|| (skinParms1->glow.params[1] != skinParms2->glow.params[1])
		|| (skinParms1->glow.params[2] != skinParms2->glow.params[2])
		|| (skinParms1->glow.params[3] != skinParms2->glow.params[3]) )
		return false;

	if (skinParms1->nodraw != skinParms2->nodraw)
		return false;

	if (skinParms1->twosided != skinParms2->twosided)
		return false;

	return true;
}

// Backend for R_DrawAliasMeshes
static void RB_RenderAliasMesh(maliasmodel_t *paliashdr, uint meshnum, uint skinnum, image_t *skin)
{
	entity_t *e = currententity;
	const float thisalpha = colorArray[0][3];
	const qboolean shellModel = e->flags & RF_MASK_SHELL;

	if (!paliashdr)
		return;

	maliasmesh_t *mesh = &paliashdr->meshes[meshnum];

	if (!shellModel)
		GL_Bind(skin->texnum);

	// md3 skin scripting
	const renderparms_t skinParms = mesh->skins[skinnum].renderparms;

	GL_Set(GL_CULL_FACE, !skinParms.twosided); //mxd
	GL_Set(GL_ALPHA_TEST, skinParms.alphatest && !shellModel); //mxd
	GL_Set(GL_BLEND, thisalpha < 1.0f || skinParms.blend); //mxd

	if (skinParms.blend && !shellModel)
		GL_BlendFunc(skinParms.blendfunc_src, skinParms.blendfunc_dst);
	else if (shellModel)
		GL_BlendFunc(GL_ONE, GL_ONE);
	else
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// md3 skin scripting

	// Draw
	RB_DrawArrays();

	// Glow pass
	if (mesh->skins[skinnum].glowimage && !shellModel)
	{
		const float glowcolor = RB_CalcGlowColor(skinParms);
		qglDisableClientState(GL_COLOR_ARRAY);
		qglColor4f(glowcolor, glowcolor, glowcolor, 1.0f);

		GL_Enable(GL_BLEND);
		GL_BlendFunc(GL_ONE, GL_ONE);

		GL_Bind(mesh->skins[skinnum].glowimage->texnum);

		RB_DrawArrays();

		qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		qglEnableClientState(GL_COLOR_ARRAY);
	}

	// Envmap pass
	if (skinParms.envmap > 0.0f && !shellModel)
	{
		GL_Enable(GL_BLEND);
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		qglTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
		qglTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
		
		// Apply alpha to array
		for (uint i = 0; i < rb_vertex; i++)
			colorArray[i][3] = thisalpha * skinParms.envmap;

		GL_Bind(glMedia.envmappic->texnum);

		qglEnable(GL_TEXTURE_GEN_S);
		qglEnable(GL_TEXTURE_GEN_T);

		RB_DrawArrays();

		qglDisable(GL_TEXTURE_GEN_S);
		qglDisable(GL_TEXTURE_GEN_T);
	}

	RB_DrawMeshTris();
	rb_vertex = 0;
	rb_index = 0;

	// Restore state
	GL_Enable(GL_CULL_FACE);
	GL_Disable(GL_ALPHA_TEST);
	GL_Disable(GL_BLEND);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void R_DrawAliasMeshes(maliasmodel_t *paliashdr, entity_t *e, qboolean lerpOnly, qboolean mirrored)
{
	const qboolean shellModel = e->flags & RF_MASK_SHELL;
	const float backlerp = e->backlerp;
	const float frontlerp = 1.0f - backlerp;

	float alpha;
	if (shellModel && FlowingShell())
		alpha = 0.7f;
	else if (e->flags & RF_TRANSLUCENT)
		alpha = e->alpha;
	else
		alpha = 1.0f;

	maliasframe_t *frame = paliashdr->frames + e->frame;
	maliasframe_t *oldframe = paliashdr->frames + e->oldframe;

	vec3_t curScale, oldScale;
	VectorScale(frame->scale, frontlerp, curScale);
	VectorScale(oldframe->scale, backlerp, oldScale);

	const float mirrormult = (mirrored ? -1.0f : 1.0f);

	// Move should be the delta back to the previous frame * backlerp
	vec3_t delta;
	VectorSubtract(e->oldorigin, e->origin, delta);

	vec3_t vectors[3];
	AngleVectors(e->angles, vectors[0], vectors[1], vectors[2]);

	vec3_t move;
	move[0] =  DotProduct(delta, vectors[0]); // Forward
	move[1] = -DotProduct(delta, vectors[1]); // Left
	move[2] =  DotProduct(delta, vectors[2]); // Up

	VectorAdd(move, oldframe->translate, move);

	for (int i = 0; i < 3; i++)
		move[i] = backlerp * move[i] + frontlerp * frame->translate[i];

	GL_ShadeModel(GL_SMOOTH);
	GL_TexEnv(GL_MODULATE);
	R_SetVertexRGBScale(true);
	R_SetShellBlend(true);

	rb_vertex = 0;
	rb_index = 0;

	// New outer loop for whole model
	for (int meshnum = 0; meshnum < paliashdr->num_meshes; meshnum++)
	{
		const maliasmesh_t mesh = paliashdr->meshes[meshnum];

		// Select skin
		int skinnum;
		image_t *skin;
		if (e->skin)
		{
			// Custom player skin
			skinnum = 0;
			skin = e->skin;
		}
		else
		{
			skinnum = (e->skinnum < mesh.num_skins ? e->skinnum : 0); // Catch bad skinnums
			skin = currentmodel->skins[meshnum][skinnum];
			if (!skin)
			{
				skinnum = 0;
				skin = currentmodel->skins[meshnum][0];
			}
		}

		if (skin == NULL)
		{
			skinnum = 0;
			skin = glMedia.notexture;
		}

		// md3 skin scripting
		const renderparms_t skinParms = mesh.skins[skinnum].renderparms;

		if (skinParms.nodraw)
			continue; // Skip this mesh for this skin

		vec3_t meshlight;
		if (skinParms.fullbright)
			VectorSet(meshlight, 1.0f, 1.0f, 1.0f);
		else
			VectorCopy(shadelight, meshlight);

		const float meshalpha = alpha * skinParms.basealpha;
		// md3 skin scripting

		maliasvertex_t *v = mesh.vertexes + e->frame * mesh.num_verts;
		maliasvertex_t *ov = mesh.vertexes + e->oldframe * mesh.num_verts;
		const int baseindex = rb_vertex;

		// Set indices for each triangle
		for (int i = 0; i < mesh.num_tris; i++)
		{
			indexArray[rb_index++] = rb_vertex + mesh.indexes[3 * i + 0];
			indexArray[rb_index++] = rb_vertex + mesh.indexes[3 * i + 1];
			indexArray[rb_index++] = rb_vertex + mesh.indexes[3 * i + 2];
		}

		//mxd. (re)allocate arrays
		static int tempVertexArrayUsage[MD3_MAX_MESHES];

		const int arraysize = mesh.num_verts * sizeof(vec3_t);
		vec3_t *tempNormalsArray = malloc(arraysize);

		if(tempVertexArrayUsage[meshnum] < arraysize)
		{
			tempVertexArrayUsage[meshnum] = arraysize;
			
			free(tempVertexArray[meshnum]);
			tempVertexArray[meshnum] = malloc(arraysize);
		}

		for (int i = 0; i < mesh.num_verts; i++, v++, ov++)
		{
			// Lerp verts
			vec3_t curNormal;
			curNormal[0] = sinf(v->normal[0]) * cosf(v->normal[1]); //mxd. Replaced precalculated r_sinTable/r_cosTable values with sinf()/cosf()
			curNormal[1] = sinf(v->normal[0]) * sinf(v->normal[1]);
			curNormal[2] = cosf(v->normal[0]);

			vec3_t oldNormal;
			oldNormal[0] = sinf(ov->normal[0]) * cosf(ov->normal[1]);
			oldNormal[1] = sinf(ov->normal[0]) * sinf(ov->normal[1]);
			oldNormal[2] = cosf(ov->normal[0]);

			for (int c = 0; c < 3; c++) //mxd
				tempNormalsArray[i][c] = curNormal[c] + (oldNormal[c] - curNormal[c]) * backlerp;

			float shellscale = 0.0f;
			if (shellModel)
				shellscale = (e->flags & RF_WEAPONMODEL ? WEAPON_SHELL_SCALE: POWERSUIT_SCALE);

			VectorSet(tempVertexArray[meshnum][i], 
				(move[0] + ov->xyz[0] * oldScale[0] + v->xyz[0] * curScale[0] + tempNormalsArray[i][0] * shellscale),
				(move[1] + ov->xyz[1] * oldScale[1] + v->xyz[1] * curScale[1] + tempNormalsArray[i][1] * shellscale) * mirrormult,
				(move[2] + ov->xyz[2] * oldScale[2] + v->xyz[2] * curScale[2] + tempNormalsArray[i][2] * shellscale));

			// Skip drawing if we're only lerping the verts for a shadow-only rendering pass
			if (lerpOnly)
				continue;

			tempNormalsArray[i][1] *= mirrormult;

			// Calc lighting and alpha
			vec3_t lightcolor;
			if (shellModel)
				VectorCopy(meshlight, lightcolor);
			else
				R_LightAliasModel(meshlight, tempNormalsArray[i], lightcolor, v->lightnormalindex, !skinParms.nodiffuse);

			// Get tex coords
			vec2_t tempSkinCoord;
			if (shellModel && FlowingShell())
			{
				tempSkinCoord[0] = (tempVertexArray[meshnum][i][0] + tempVertexArray[meshnum][i][1]) * DIV40 + shellFlowH;
				tempSkinCoord[1] = tempVertexArray[meshnum][i][2] * DIV40 + shellFlowV; // was / 40
			}
			else
			{
				tempSkinCoord[0] = mesh.stcoords[i].st[0];
				tempSkinCoord[1] = mesh.stcoords[i].st[1];
			}

			// Add to arrays
			VA_SetElem2(texCoordArray[0][rb_vertex], tempSkinCoord[0], tempSkinCoord[1]);
			VA_SetElem3(vertexArray[rb_vertex], tempVertexArray[meshnum][i][0], tempVertexArray[meshnum][i][1], tempVertexArray[meshnum][i][2]);
			VA_SetElem4(colorArray[rb_vertex], lightcolor[0], lightcolor[1], lightcolor[2], meshalpha);

			rb_vertex++;
		}

		//mxd
		free(tempNormalsArray);

		if (!shellModel)
			RB_ModifyTextureCoords(&texCoordArray[0][baseindex][0], &vertexArray[baseindex][0], mesh.num_verts, skinParms);

		// Compare renderparms for next mesh and check for overflow
		if (meshnum < paliashdr->num_meshes - 1)
		{
			if ((shellModel || R_AliasMeshesAreBatchable(paliashdr, meshnum, meshnum + 1, e->skinnum))
				&& !RB_CheckArrayOverflow(paliashdr->meshes[meshnum + 1].num_verts, paliashdr->meshes[meshnum + 1].num_tris * 3))
				continue;
		}

		RB_RenderAliasMesh(paliashdr, meshnum, skinnum, skin);
	} // End new outer loop

	R_SetShellBlend(false);
	R_SetVertexRGBScale(false);
	GL_TexEnv(GL_REPLACE);
	GL_ShadeModel(GL_FLAT);
}

static qboolean R_CullAliasModel(vec3_t bbox[8], entity_t *e)
{
	maliasmodel_t *paliashdr = (maliasmodel_t *)currentmodel->extradata;

	if (e->frame >= paliashdr->num_frames || e->frame < 0)
	{
		VID_Printf(PRINT_ALL, "R_CullAliasModel %s: no such frame %d\n", currentmodel->name, e->frame);
		e->frame = 0;
	}

	if (e->oldframe >= paliashdr->num_frames || e->oldframe < 0)
	{
		VID_Printf(PRINT_ALL, "R_CullAliasModel %s: no such oldframe %d\n", currentmodel->name, e->oldframe);
		e->oldframe = 0;
	}

	maliasframe_t *pframe = paliashdr->frames + e->frame;
	maliasframe_t *poldframe = paliashdr->frames + e->oldframe;

	// Compute axially aligned mins and maxs
	vec3_t mins, maxs;
	if (pframe == poldframe)
	{
		VectorCopy(pframe->mins, mins);
		VectorCopy(pframe->maxs, maxs);
	}
	else
	{
		for (int i = 0; i < 3; i++ )
		{
			if (pframe->mins[i] < poldframe->mins[i])
				mins[i] = pframe->mins[i];
			else
				mins[i] = poldframe->mins[i];

			if (pframe->maxs[i] > poldframe->maxs[i])
				maxs[i] = pframe->maxs[i];
			else
				maxs[i] = poldframe->maxs[i];
		}
	}

	// jitspoe's bbox rotation fix
	// Compute and rotate bonding box
	vec3_t vectors[3];
	e->angles[ROLL] = -e->angles[ROLL]; // Roll is backwards
	AngleVectors(e->angles, vectors[0], vectors[1], vectors[2]);
	e->angles[ROLL] = -e->angles[ROLL]; // Roll is backwards

	VectorSubtract(vec3_origin, vectors[1], vectors[1]); // AngleVectors returns "right" instead of "left"
	
	for (int i = 0; i < 8; i++)
	{
		vec3_t tmp;
		tmp[0] = ((i & 1) ? mins[0] : maxs[0]);
		tmp[1] = ((i & 2) ? mins[1] : maxs[1]);
		tmp[2] = ((i & 4) ? mins[2] : maxs[2]);

		bbox[i][0] = vectors[0][0] * tmp[0] + vectors[1][0] * tmp[1] + vectors[2][0] * tmp[2] + e->origin[0];
		bbox[i][1] = vectors[0][1] * tmp[0] + vectors[1][1] * tmp[1] + vectors[2][1] * tmp[2] + e->origin[1];
		bbox[i][2] = vectors[0][2] * tmp[0] + vectors[1][2] * tmp[1] + vectors[2][2] * tmp[2] + e->origin[2];
	}

	// Cull
	int aggregatemask = ~0;
	for (int i = 0; i < 8; i++)
	{
		int mask = 0;
		for (int j = 0; j < 4; j++)
		{
			const float dp = DotProduct(frustum[j].normal, bbox[i]);
			if (dp - frustum[j].dist < 0)
				mask |= 1 << j;
		}

		aggregatemask &= mask;
	}

	return (aggregatemask > 0);
}

extern void CL_Shadow_Decal(const vec3_t org, float size, float alpha); //mxd

void R_DrawAliasModel(entity_t *e)
{
	vec3_t bbox[8];
	qboolean mirrorview = false;
	qboolean mirrormodel = false;

	// Also skip this for viewermodels and cameramodels
	if (!(e->flags & RF_WEAPONMODEL || e->flags & RF_VIEWERMODEL || e->renderfx & RF2_CAMERAMODEL))
	{
		if (R_CullAliasModel(bbox, e))
			return;
	}

	// Mirroring support
	if (e->flags & RF_WEAPONMODEL)
	{
		if (r_lefthand->value == 2)
			return;

		if (r_lefthand->value == 1)
			mirrorview = true;
	}
	else if (e->renderfx & RF2_CAMERAMODEL)
	{
		if (r_lefthand->value == 1)
			mirrormodel = true;
	}
	else if (e->flags & RF_MIRRORMODEL)
	{
		mirrormodel = true;
	}
	// end mirroring support

	maliasmodel_t *paliashdr = (maliasmodel_t *)currentmodel->extradata;

	R_SetShadeLight();

	if (e->flags & RF_DEPTHHACK) // Hack the depth range to prevent view model from poking into walls
	{
		const float scaler = (r_newrefdef.rdflags & RDF_NOWORLDMODEL ? 0.01f : 0.3f); //mxd
		GL_DepthRange(gldepthmin, gldepthmin + scaler * (gldepthmax - gldepthmin));
	}

	// Mirroring support
	if (mirrorview || mirrormodel)
		R_FlipModel(true, mirrormodel);

	for (int i = 0; i < paliashdr->num_meshes; i++)
		c_alias_polys += paliashdr->meshes[i].num_tris;

	qglPushMatrix();
	e->angles[ROLL] *= R_RollMult(); // Roll is backwards
	R_RotateForEntity(e, true);
	e->angles[ROLL] *= R_RollMult(); // Roll is backwards

	if (e->frame >= paliashdr->num_frames || e->frame < 0)
	{
		VID_Printf(PRINT_ALL, "R_DrawAliasModel %s: no such frame %i\n", currentmodel->name, e->frame);
		e->frame = 0;
		e->oldframe = 0;
	}

	if (e->oldframe >= paliashdr->num_frames || e->oldframe < 0)
	{
		VID_Printf(PRINT_ALL, "R_DrawAliasModel %s: no such oldframe %i\n", currentmodel->name, e->oldframe);
		e->frame = 0;
		e->oldframe = 0;
	}

	if (!r_lerpmodels->value)
		e->backlerp = 0;

	R_DrawAliasMeshes(paliashdr, e, false, mirrormodel);

	qglPopMatrix();

	// Mirroring support
	if (mirrorview || mirrormodel)
		R_FlipModel(false, mirrormodel);

	// Show model bounding box
	R_DrawAliasModelBBox(bbox, e, 1.0f, 1.0f, 1.0f, 1.0f);

	if (e->flags & RF_DEPTHHACK)
		GL_DepthRange(gldepthmin, gldepthmax);

	const float shadowalpha = R_CalcShadowAlpha(e);

	if (!(e->flags & (RF_WEAPONMODEL | RF_NOSHADOW))
		&& !(e->flags & RF_MASK_SHELL && e->flags & RF_TRANSLUCENT) // No shadows from shells
		&& r_shadows->integer >= 1 && shadowalpha >= DIV255)
	{
		//mxd. Simple decal shadows...

		// Lerp origin...
		vec3_t origin, delta;
		VectorSubtract(e->oldorigin, e->origin, delta);
		VectorMA(e->origin, e->backlerp, delta, origin);

		// Get model shading...
		vec3_t shadecolor;
		R_LightPoint(origin, shadecolor, false);
		float maxlight = 0;
		for (int i = 0; i < 3; i++)
			maxlight = max(shadecolor[i], maxlight);
		maxlight = min(1.0f, maxlight * 2); // Model shading is very dim...

		// Scale by distance fading part of aliasShadowAlpha and entity alpha...
		maxlight *= (shadowalpha / r_shadowalpha->value) * e->alpha;

		if(maxlight > 0.25f)
		{
			// Get model size from the first frame (which is the first frame of Idle animation for all vanilla monsters)
			maliasframe_t *pframe = paliashdr->frames;
			const float size = ((pframe->maxs[0] - pframe->mins[0]) + (pframe->maxs[1] - pframe->mins[1])) * 0.45f;

			// Trace from feet, not entity origin...
			origin[2] += pframe->mins[2];

			// Add decal
			CL_Shadow_Decal(origin, size, maxlight);
		}
	}
}