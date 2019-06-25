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

// r_alias_misc.c: shared model rendering functions

#include "r_local.h"

// Precalculated dot products for quantized angles
float r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anormtab.h"
;

float *shadedots = r_avertexnormal_dots[0];

vec3_t shadelight;

m_dlight_t model_dlights[MAX_MODEL_DLIGHTS];
int model_dlights_num;

float shellFlowH;
float shellFlowV;

#define SHADOW_FADE_DIST 128

float R_CalcShadowAlpha(entity_t *e)
{
	vec3_t vec;
	float minRange, outAlpha;

	VectorSubtract(e->origin, r_origin, vec);
	const float dist = VectorLength(vec);

	if (r_newrefdef.fov_y >= 90)
		minRange = r_shadowrange->value;
	else // Reduced FOV means longer range
		minRange = r_shadowrange->value * (90 / r_newrefdef.fov_y); // This can't be zero
	const float maxRange = minRange + SHADOW_FADE_DIST;

	if (dist <= minRange) // In range
		outAlpha = r_shadowalpha->value;
	else if (dist >= maxRange) // Out of range
		outAlpha = 0.0f;
	else // Fade based on distance
		outAlpha = r_shadowalpha->value * (fabsf(dist - maxRange) / SHADOW_FADE_DIST);

	return outAlpha;
}

void R_SetVertexRGBScale(qboolean toggle)
{
	if (!r_rgbscale->integer)
		return;

	if (toggle) // Turn on
	{
		qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, r_rgbscale->integer);
		qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
		
		GL_TexEnv(GL_COMBINE);
	}
	else // Turn off
	{
		GL_TexEnv(GL_MODULATE);
		qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
	}
}

qboolean EnvMapShell(void)
{
	return (r_shelltype->value == 2 || (r_shelltype->value == 1 && currententity->alpha == 1.0f));
}

qboolean FlowingShell(void)
{
	return (r_shelltype->value == 1 && currententity->alpha != 1.0f);
}

void R_SetShellBlend(qboolean toggle)
{
	// Shells only
	if (!(currententity->flags & RF_MASK_SHELL))
		return;

	if (toggle) // Turn on
	{
		// Psychospaz's envmapping
		if (EnvMapShell())
		{
			qglTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
			qglTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);

			GL_Bind(glMedia.spheremappic->texnum);

			qglEnable(GL_TEXTURE_GEN_S);
			qglEnable(GL_TEXTURE_GEN_T);
		}
		else if (FlowingShell())
		{
			GL_Bind(glMedia.shelltexture->texnum);
		}
		else
		{
			GL_DisableTexture(0);
		}

		GL_Stencil(true, true);

		shellFlowH = 0.25f * sinf(r_newrefdef.time * 0.5f * M_PI);
		shellFlowV = -(r_newrefdef.time / 2.0f); 
	}
	else // Turn off
	{
		// Psychospaz's envmapping
		if (EnvMapShell())
		{
			qglDisable(GL_TEXTURE_GEN_S);
			qglDisable(GL_TEXTURE_GEN_T);
		}
		else if (!FlowingShell())
		{
			GL_EnableTexture(0);
		}

		GL_Stencil(false, true);
	}
}

extern void MYgluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);

void R_FlipModel(qboolean on, qboolean cullOnly)
{
	if (on)
	{
		if (!cullOnly)
		{
			qglMatrixMode(GL_PROJECTION);
			qglPushMatrix();
			qglLoadIdentity();
			qglScalef(-1, 1, 1);
			MYgluPerspective(r_newrefdef.fov_y, (float)r_newrefdef.width / r_newrefdef.height, 4, r_farz); // Knightmare- was 4096
			qglMatrixMode(GL_MODELVIEW);
		}

		GL_CullFace(GL_BACK);
	}
	else
	{
		if (!cullOnly)
		{
			qglMatrixMode(GL_PROJECTION);
			qglPopMatrix();
			qglMatrixMode(GL_MODELVIEW);
		}

		GL_CullFace(GL_FRONT);
	}
}

void R_SetShadeLight(void)
{
	if (currententity->flags & RF_MASK_SHELL)
	{
		VectorClear(shadelight);

		if (currententity->flags & RF_SHELL_HALF_DAM)
		{
			shadelight[0] = 0.56f;
			shadelight[1] = 0.59f;
			shadelight[2] = 0.45f;
		}

		if (currententity->flags & RF_SHELL_DOUBLE)
		{
			shadelight[0] = 0.9f;
			shadelight[1] = 0.7f;
		}

		if (currententity->flags & RF_SHELL_RED)
			shadelight[0] = 1.0f;

		if (currententity->flags & RF_SHELL_GREEN)
			shadelight[1] = 1.0f;

		if (currententity->flags & RF_SHELL_BLUE)
			shadelight[2] = 1.0f;
	}
	else if (currententity->flags & RF_FULLBRIGHT)
	{
		VectorSetAll(shadelight, 1.0f); //mxd
	}
	else
	{
		// Set up basic lighting...
		if (r_model_shading->value && r_model_dlights->integer)
		{
			const int max = clamp(r_model_dlights->integer, 0, MAX_MODEL_DLIGHTS); //mxd. +clamp
			R_LightPointDynamics(currententity->origin, shadelight, model_dlights, &model_dlights_num, max);
		}
		else
		{
			R_LightPoint(currententity->origin, shadelight, false);
			model_dlights_num = 0;
		}

		// Player lighting hack for communication back to server
		if (currententity->flags & RF_WEAPONMODEL)
		{
			// Pick the greatest component, which should be the same as the mono value returned by software
			r_lightlevel->value = 150 * max(shadelight[0], max(shadelight[1], shadelight[2])); //mxd
		}
	}

	if (currententity->flags & RF_MINLIGHT)
	{
		int i;
		for (i = 0; i < 3; i++)
			if (shadelight[i] > 0.02f)
				break;

		if (i == 3)
			VectorSetAll(shadelight, 0.02f); //mxd
	}

	if (currententity->flags & RF_GLOW)
	{
		// Bonus items will pulse with time
		const float scale = 0.2f * sinf(r_newrefdef.time * 7);
		for (int i = 0; i < 3; i++)
		{
			const float minlight = shadelight[i] * 0.8f;
			shadelight[i] += scale;
			shadelight[i] = max(minlight, shadelight[i]);
		}
	}

	if (r_newrefdef.rdflags & RDF_IRGOGGLES && currententity->flags & RF_IR_VISIBLE)
		VectorSet(shadelight, 1.0f, 0.0f, 0.0f); //mxd

	shadedots = r_avertexnormal_dots[((int)(currententity->angles[1] * (SHADEDOT_QUANT / 360.0f))) & (SHADEDOT_QUANT - 1)];
}

void R_DrawAliasModelBBox(vec3_t bbox[8], entity_t *e, float red, float green, float blue, float alpha)
{
	if (!r_showbbox->integer)
		return;

	if (e->flags & RF_WEAPONMODEL || e->flags & RF_VIEWERMODEL || e->flags & RF_BEAM || e->renderfx & RF2_CAMERAMODEL)
		return;

	GL_Disable(GL_CULL_FACE);
	qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	qglDisableClientState(GL_COLOR_ARRAY);
	qglColor4f(red, green, blue, alpha);
	GL_DisableTexture(0);

	rb_vertex = 0;
	rb_index = 0;

	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 3;

	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 4;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 4;
	indexArray[rb_index++] = rb_vertex + 5;

	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 4;
	indexArray[rb_index++] = rb_vertex + 6;

	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 5;
	indexArray[rb_index++] = rb_vertex + 7;

	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 3;
	indexArray[rb_index++] = rb_vertex + 7;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 6;
	indexArray[rb_index++] = rb_vertex + 7;

	indexArray[rb_index++] = rb_vertex + 4;
	indexArray[rb_index++] = rb_vertex + 5;
	indexArray[rb_index++] = rb_vertex + 7;
	indexArray[rb_index++] = rb_vertex + 4;
	indexArray[rb_index++] = rb_vertex + 6;
	indexArray[rb_index++] = rb_vertex + 7;

	for (int i = 0; i < 8; i++)
	{
		VA_SetElem3v(vertexArray[rb_vertex], bbox[i]);
		rb_vertex++;
	}

	RB_DrawArrays();

	rb_vertex = 0;
	rb_index = 0;

	GL_EnableTexture(0);
	qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	qglEnableClientState(GL_COLOR_ARRAY);
	qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	GL_Enable(GL_CULL_FACE);
}