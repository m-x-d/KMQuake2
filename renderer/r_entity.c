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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

// r_entity.c -- entity handling and null model rendering
// moved from r_main.c

#include "r_local.h"

void R_RotateForEntity(entity_t *e, qboolean full)
{
	qglTranslatef(e->origin[0], e->origin[1], e->origin[2]);
	qglRotatef(e->angles[1], 0, 0, 1);

	if (full)
	{
		qglRotatef(e->angles[0], 0, 1, 0);
		qglRotatef(e->angles[2], 1, 0, 0);
	}
}

int R_RollMult(void)
{
	return (r_entity_fliproll->value ? -1 : 1); //TODO: mxd. What't the purpose of r_entity_fliproll?..
}

static void R_DrawNullModel(void)
{
	qglPushMatrix();
	R_RotateForEntity(currententity, true);
	GL_DisableTexture(0);

	if (r_old_nullmodel->value) //TODO: mxd. What't the purpose of r_old_nullmodel?..
	{
		vec3_t shadelight;
		if (currententity->flags & RF_FULLBRIGHT)
			VectorSet(shadelight, 1.0f, 1.0f, 1.0f);
		else
			R_LightPoint(currententity->origin, shadelight, false);

		qglColor3fv(shadelight);
		qglCallList(glMedia.displayLists[DL_NULLMODEL1]);
	}
	else
	{
		qglCallList(glMedia.displayLists[DL_NULLMODEL2]);
	}

	GL_EnableTexture(0);
	qglPopMatrix();
	qglColor4f(1, 1, 1, 1);
}

#pragma region ======================= TREE BUILDING AND USAGE

static int entstosort;
sortedelement_t theents[MAX_ENTITIES];
sortedelement_t *ents_trans;
sortedelement_t *ents_viewweaps;
sortedelement_t *ents_viewweaps_trans;

static void ResetEntSortList(void)
{
	entstosort = 0;
	ents_trans = NULL;
	ents_viewweaps = NULL;
	ents_viewweaps_trans = NULL;
}

static sortedelement_t *NewSortEnt(entity_t *ent)
{
	sortedelement_t *element = &theents[entstosort];

	vec3_t distance;
	VectorSubtract(ent->origin, r_origin, distance);
	VectorCopy(ent->origin, element->org);

	element->data = (entity_t *)ent;
	element->len = (vec_t)VectorLength(distance);
	element->left = NULL;
	element->right = NULL;

	return element;
}

static void ElementAddNode(sortedelement_t *base, sortedelement_t *thisElement)
{
	while(true)
	{
		if (thisElement->len > base->len)
		{
			if (base->left)
			{
				base = base->left;
			}
			else
			{
				base->left = thisElement;
				return;
			}
		}
		else
		{
			if (base->right)
			{
				base = base->right;
			}
			else
			{
				base->right = thisElement;
				return;
			}
		}
	}
}

static void AddEntViewWeapTree(entity_t *ent, qboolean trans)
{
	sortedelement_t *thisEnt = NewSortEnt(ent);

	if (!thisEnt)
		return;

	if (!trans)
	{
		if (ents_viewweaps)
			ElementAddNode(ents_viewweaps, thisEnt);
		else
			ents_viewweaps = thisEnt;
	}
	else
	{
		if (ents_viewweaps_trans)
			ElementAddNode(ents_viewweaps_trans, thisEnt);
		else
			ents_viewweaps_trans = thisEnt;	
	}

	entstosort++;
}

static void AddEntTransTree(entity_t *ent)
{
	sortedelement_t *thisEnt = NewSortEnt(ent);

	if (!thisEnt)
		return;

	if (ents_trans)
		ElementAddNode(ents_trans, thisEnt);
	else
		ents_trans = thisEnt;

	entstosort++;
}

#pragma endregion 

static void ParseRenderEntity(entity_t *ent)
{
	currententity = ent;

	if (currententity->flags & RF_BEAM)
	{
		R_DrawBeam(currententity);
	}
	else
	{
		currentmodel = currententity->model;
		if (!currentmodel)
		{
			R_DrawNullModel();
			return;
		}

		switch (currentmodel->type)
		{
			case mod_md3:	 R_DrawAliasModel(currententity); break; //Harven MD3
			case mod_brush:  R_DrawBrushModel(currententity); break;
			case mod_sprite: R_DrawSpriteModel(currententity); break;
			default:
				VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Warning: ParseRenderEntity: %s: bad model type (%i)\n", currentmodel->name, currentmodel->type);
				break;
		}
	}
}

static void RenderEntTree(sortedelement_t *element)
{
	if (!element)
		return;

	RenderEntTree(element->left);

	if (element->data)
		ParseRenderEntity(element->data);

	RenderEntTree(element->right);
}

//mxd
static qboolean EntityHasAlpha(entity_t *e)
{
	if (e->flags & RF_TRANSLUCENT)
		return true;

	// Check for md3 mesh transparency
	if (!(e->flags & RF_BEAM) && e->model && e->model->type == mod_md3 && e->model->hasAlpha)
		return true;

	return false;
}

void R_DrawAllEntities(qboolean addViewWeaps)
{
	if (!r_drawentities->value)
		return;

	ResetEntSortList();

	// Opaque models
	for (int i = 0; i < r_newrefdef.num_entities; i++)
	{
		currententity = &r_newrefdef.entities[i];
		const qboolean alpha = EntityHasAlpha(currententity); //mxd

		if (currententity->flags & RF_WEAPONMODEL)
		{
			if (addViewWeaps)
				AddEntViewWeapTree(currententity, alpha);

			continue;
		}

		if (alpha)
			continue;

		ParseRenderEntity(currententity);
	}

	GL_DepthMask(0);

	// Translucent models
	for (int i = 0; i < r_newrefdef.num_entities; i++)
	{
		currententity = &r_newrefdef.entities[i];
		const qboolean alpha = EntityHasAlpha(currententity); //mxd

		if (!alpha || currententity->flags & RF_WEAPONMODEL)
			continue;

		ParseRenderEntity(currententity);
	}

	GL_DepthMask(1);
}

void R_DrawSolidEntities()
{
	if (!r_drawentities->integer)
		return;

	ResetEntSortList();

	for (int i = 0; i < r_newrefdef.num_entities; i++)
	{
		currententity = &r_newrefdef.entities[i];
		const qboolean alpha = EntityHasAlpha(currententity); //mxd

		if (currententity->flags & RF_WEAPONMODEL)
		{
			AddEntViewWeapTree(currententity, alpha);
			continue;
		}

		if (alpha)
		{
			AddEntTransTree(currententity);
			continue;
		}

		ParseRenderEntity(currententity);
	}
}

void R_DrawEntitiesOnList(sortedelement_t *list)
{
	if (r_drawentities->integer)
		RenderEntTree(list);
}