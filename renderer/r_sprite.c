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
// r_sprite.c -- sprite rendering
// moved from r_main.c

#include "r_local.h"

void R_DrawSpriteModel(entity_t *e)
{
	// Don't even bother culling, because it's just a single polygon without a surface cache
	dsprite_t *psprite = (dsprite_t *)currentmodel->extradata;

	e->frame %= psprite->numframes;

	dsprframe_t *frame = &psprite->frames[e->frame];
	if (!frame)
		return;

	c_alias_polys += 2;

	//mxd. Rotated sprite
	vec3_t up, right;
	if(e->angles[ROLL])
	{
		vec3_t tmp;
		VecToAngleRolled(vpn, e->angles[ROLL], tmp); // Roll the forward view vector
		AngleVectors(tmp, NULL, right, up);
	}
	else // Normal sprite
	{
		VectorCopy(vup, up);
		VectorCopy(vright, right);
	}
	
	const float alpha = (e->flags & RF_TRANSLUCENT ? e->alpha : 1.0f);

	R_SetVertexRGBScale(true);

	if (e->flags & RF_DEPTHHACK) //mxd. Hack the depth range to prevent sprites from poking into walls
	{
		const float scaler = (r_newrefdef.rdflags & RDF_NOWORLDMODEL ? 0.01 : 0.3);
		GL_DepthRange(gldepthmin, gldepthmin + scaler * (gldepthmax - gldepthmin));
	}

	// Psychospaz's additive transparency
	if ((currententity->flags & RF_TRANS_ADDITIVE) /*&& alpha != 1.0f*/) //mxd. Works fine even with alpha 1
	{ 
		GL_Enable(GL_BLEND);
		GL_TexEnv(GL_MODULATE);
		GL_Disable(GL_ALPHA_TEST);
		GL_DepthMask(false);
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	}
	else
	{
		GL_TexEnv(GL_MODULATE);
		/*if (alpha == 1.0f) //mxd. Works fine even with alpha 1
		{
			GL_Enable(GL_ALPHA_TEST);
			GL_DepthMask(true);
		}
		else
		{*/
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			GL_DepthMask(false);
			GL_Enable(GL_BLEND);
			GL_Disable(GL_ALPHA_TEST);
		//}
	}
	GL_Bind(currentmodel->skins[0][e->frame]->texnum);

	vec2_t texCoord[4];
	Vector2Set(texCoord[0], 0, 1);
	Vector2Set(texCoord[1], 0, 0);
	Vector2Set(texCoord[2], 1, 0);
	Vector2Set(texCoord[3], 1, 1);

	//mxd
	float scale = 1.0f;
	if (e->flags & RF_NOSCALE)
	{
		vec3_t diff;
		VectorSubtract(e->origin, r_newrefdef.vieworg, diff);
		scale = VectorLength(diff) * 0.00625f;
		scale = max(1.0f, scale);
	}

	vec3_t point[4];
	VectorMA(e->origin, -frame->origin_y * scale, up, point[0]);
	VectorMA(point[0], -frame->origin_x * scale, right, point[0]);

	VectorMA(e->origin, (frame->height - frame->origin_y) * scale, up, point[1]);
	VectorMA(point[1], -frame->origin_x * scale, right, point[1]);

	VectorMA(e->origin, (frame->height - frame->origin_y) * scale, up, point[2]);
	VectorMA(point[2], (frame->width - frame->origin_x) * scale, right, point[2]);

	VectorMA(e->origin, -frame->origin_y * scale, up, point[3]);
	VectorMA(point[3], (frame->width - frame->origin_x) * scale, right, point[3]);

	rb_vertex = 0;
	rb_index = 0;

	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 3;

	for (int i = 0; i < 4; i++)
	{
		VA_SetElem2(texCoordArray[0][rb_vertex], texCoord[i][0], texCoord[i][1]);
		VA_SetElem3(vertexArray[rb_vertex], point[i][0], point[i][1], point[i][2]);
		VA_SetElem4(colorArray[rb_vertex], 1.0f, 1.0f, 1.0f, alpha);
		rb_vertex++;
	}

	RB_DrawArrays();

	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_TexEnv(GL_REPLACE);
	GL_DepthMask(true);
	GL_Disable(GL_ALPHA_TEST);
	GL_Disable(GL_BLEND);

	R_SetVertexRGBScale(false);

	RB_DrawMeshTris();

	if (e->flags & RF_DEPTHHACK) //mxd
		GL_DepthRange(gldepthmin, gldepthmax);

	rb_vertex = 0;
	rb_index = 0;
}