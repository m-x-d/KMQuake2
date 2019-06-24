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

// r_sky.c -- sky rendering
// Moved from r_warp.c

#include "r_local.h"

static char skyname[MAX_QPATH];
static float skyrotate;
static vec3_t skyaxis;
static image_t *sky_images[6];

static vec3_t skyclip[6] =
{
	{ 1,  1, 0 },
	{ 1, -1, 0 },
	{ 0, -1, 1 },
	{ 0,  1, 1 },
	{ 1,  0, 1 },
	{-1,  0, 1 }
};

// 1 = s, 2 = t, 3 = 2048
static int st_to_vec[6][3] =
{
	{ 3, -1, 2 },
	{-3,  1, 2 },

	{ 1,  3, 2 },
	{-1, -3, 2 },

	{-2, -1, 3 },	// 0 degrees yaw, look straight up
	{ 2, -1,-3 }	// look straight down
};

// s = [0]/[2], t = [1]/[2]
static int vec_to_st[6][3] =
{
	{-2,  3,  1 },
	{ 2,  3, -1 },

	{ 1,  3,  2 },
	{-1,  3, -2 },

	{-2, -1,  3 },
	{-2,  1, -3 }
};

static float skymins[2][6];
static float skymaxs[2][6];
static float sky_min;
static float sky_max;

static void DrawSkyPolygon(int nump, vec3_t vecs)
{
	vec3_t v, av;
	float s, t, dv;

	// Decide which face it maps to
	VectorCopy(vec3_origin, v);
	float *vp = vecs;
	for (int i = 0; i < nump; i++, vp += 3)
		VectorAdd(vp, v, v);

	for(int i = 0; i < 3; i++)
		av[i] = fabs(v[i]);

	int axis;
	if (av[0] > av[1] && av[0] > av[2])
		axis = (v[0] < 0 ? 1 : 0);
	else if (av[1] > av[2] && av[1] > av[0])
		axis = (v[1] < 0 ? 3 : 2);
	else
		axis = (v[2] < 0 ? 5 : 4);

	// Project new texture coords
	for (int i = 0; i < nump; i++, vecs += 3)
	{
		int j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];

		if (dv < 0.001f)
			continue; // Don't divide by zero

		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j - 1] / dv;
		else
			s = vecs[j - 1] / dv;

		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j - 1] / dv;
		else
			t = vecs[j - 1] / dv;

		skymins[0][axis] = min(s, skymins[0][axis]);
		skymins[1][axis] = min(t, skymins[1][axis]);

		skymaxs[0][axis] = max(s, skymaxs[0][axis]);
		skymaxs[1][axis] = max(t, skymaxs[1][axis]);
	}
}

#define	ON_EPSILON		0.1f // Point on plane side epsilon
#define	MAX_CLIP_VERTS	64

static void ClipSkyPolygon(int nump, vec3_t vecs, int stage)
{
	float *v;
	float dists[MAX_CLIP_VERTS];
	int sides[MAX_CLIP_VERTS];
	int i;

	if (nump > MAX_CLIP_VERTS - 2)
		VID_Error(ERR_DROP, "ClipSkyPolygon: MAX_CLIP_VERTS");

	if (stage == 6)
	{
		// Fully clipped, so draw it
		DrawSkyPolygon(nump, vecs);
		return;
	}

	qboolean front = false;
	qboolean back = false;
	float *norm = skyclip[stage];
	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		const float d = DotProduct(v, norm);

		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
		{
			sides[i] = SIDE_ON;
		}

		dists[i] = d;
	}

	if (!front || !back)
	{
		// Not clipped
		ClipSkyPolygon(nump, vecs, stage + 1);
		return;
	}

	// Clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy(vecs, (vecs + (i * 3)));

	vec3_t newv[2][MAX_CLIP_VERTS];
	int newc[2] = { 0, 0 };

	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		switch (sides[i])
		{
			case SIDE_FRONT:
				VectorCopy(v, newv[0][newc[0]]);
				newc[0]++;
				break;

			case SIDE_BACK:
				VectorCopy(v, newv[1][newc[1]]);
				newc[1]++;
				break;

			case SIDE_ON:
				VectorCopy(v, newv[0][newc[0]]);
				newc[0]++;
				VectorCopy(v, newv[1][newc[1]]);
				newc[1]++;
				break;
		}

		if (sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		const float d = dists[i] / (dists[i] - dists[i + 1]);

		for (int j = 0; j < 3; j++)
		{
			const float e = v[j] + d * (v[j + 3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}

		newc[0]++;
		newc[1]++;
	}

	// Continue
	ClipSkyPolygon(newc[0], newv[0][0], stage + 1);
	ClipSkyPolygon(newc[1], newv[1][0], stage + 1);
}

void R_AddSkySurface(msurface_t *fa)
{
	vec3_t verts[MAX_CLIP_VERTS];

	// Calculate vertex values for sky box
	for (glpoly_t *p = fa->polys; p; p = p->next)
	{
		for (int i = 0; i<p->numverts; i++)
			VectorSubtract(p->verts[i], r_origin, verts[i]);

		ClipSkyPolygon(p->numverts, verts[0], 0);
	}
}

void R_ClearSkyBox(void)
{
	for (int i = 0; i < 6; i++)
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}

static void MakeSkyVec(float s, float t, int axis)
{
	vec3_t v, b;

	//Knightmare- 12/26/2001- variable back clipping plane distance
	b[0] = s * r_skydistance->value;
	b[1] = t * r_skydistance->value;
	b[2] = r_skydistance->value;
	//end Knightmare

	for (int j = 0; j < 3; j++)
	{
		const int k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
	}

	// Avoid bilerp seam
	s = (s + 1) * 0.5f;
	t = (t + 1) * 0.5f;

	s = clamp(s, sky_min, sky_max);
	t = clamp(t, sky_min, sky_max);
	t = 1.0f - t;

	VA_SetElem2(texCoordArray[0][rb_vertex], s, t);
	VA_SetElem3(vertexArray[rb_vertex], v[0], v[1], v[2]);
	VA_SetElem4(colorArray[rb_vertex], 1.0f, 1.0f, 1.0f, 1.0f);

	rb_vertex++;
}

void R_DrawSkyBox(void)
{
	static int skytexorder[6] = { 0, 2, 1, 3, 4, 5 }; //mxd. Made local
	
	if (skyrotate)
	{
		int c;

		// Check for no sky at all
		for (c = 0; c < 6; c++)
			if (skymins[0][c] < skymaxs[0][c] && skymins[1][c] < skymaxs[1][c])
				break;
		
		if (c == 6)
			return; // Nothing visible
	}

	R_SetSkyFog(true); // Set sky distance fog

	qglPushMatrix();
	qglTranslatef(r_origin[0], r_origin[1], r_origin[2]);
	qglRotatef(r_newrefdef.time * skyrotate, skyaxis[0], skyaxis[1], skyaxis[2]);

	for (int i = 0; i < 6; i++)
	{
		if (skyrotate)
		{
			// Hack, forces full sky to draw when rotating
			skymins[0][i] = -1;
			skymins[1][i] = -1;
			skymaxs[0][i] = 1;
			skymaxs[1][i] = 1;
		}

		if (skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])
			continue;

		GL_Bind(sky_images[skytexorder[i]]->texnum);

		rb_vertex = 0;
		rb_index = 0;

		indexArray[rb_index++] = rb_vertex;
		indexArray[rb_index++] = rb_vertex + 1;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex + 3;

		MakeSkyVec(skymins[0][i], skymins[1][i], i);
		MakeSkyVec(skymins[0][i], skymaxs[1][i], i);
		MakeSkyVec(skymaxs[0][i], skymaxs[1][i], i);
		MakeSkyVec(skymaxs[0][i], skymins[1][i], i);

		RB_RenderMeshGeneric(true);
	}

	qglPopMatrix();

	R_SetSkyFog(false); // Restore normal distance fog
}

void R_SetSky(char *name, float rotate, vec3_t axis)
{
	// 3dstudio environment map names
	static char *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" }; //mxd. Made local
	
	char pathname[MAX_QPATH];
	float imagesize;

	strncpy(skyname, name, sizeof(skyname) - 1);
	skyrotate = rotate;
	VectorCopy(axis, skyaxis);

	for (int i = 0; i < 6; i++)
	{
		Com_sprintf(pathname, sizeof(pathname), "env/%s%s.tga", skyname, suf[i]);
		sky_images[i] = R_FindImage(pathname, it_sky, false);
		if (!sky_images[i])
			sky_images[i] = glMedia.notexture;

		if (sky_images[i]->height == sky_images[i]->width && sky_images[i]->width >= 256)
			imagesize = sky_images[i]->width;
		else
			imagesize = 256.0;

		imagesize = min(1024.0f, imagesize); // Cap at 1024

		sky_min = 1.0f / imagesize; // Was 256
		sky_max = (imagesize - 1.0f) / imagesize;
	}
}