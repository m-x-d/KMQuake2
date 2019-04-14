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

// r_draw.c - 2D image drawing

#include "r_local.h"

image_t *draw_chars;

extern qboolean scrap_dirty;
void Scrap_Upload(void);

#define DEFAULT_FONT_SIZE 8.0f

void RefreshFont(void)
{
	con_font->modified = false;

	draw_chars = R_FindImage(va("fonts/%s.pcx", con_font->string), it_pic, true);

	if (!draw_chars) // fall back to default font
		draw_chars = R_FindImage("fonts/default.pcx", it_pic, true);

	if (!draw_chars) // fall back to old Q2 conchars
		draw_chars = R_FindImage("pics/conchars.pcx", it_pic, true);

	if (!draw_chars) // prevent crash caused by missing font
		VID_Error(ERR_FATAL, "RefreshFont: couldn't load pics/conchars");

	GL_Bind(draw_chars->texnum);
}


/*
===============
R_DrawInitLocal
===============
*/
void R_DrawInitLocal(void)
{
	image_t	*R_DrawFindPic(char *name);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	RefreshFont(); // load console characters (don't bilerp characters)
	R_InitChars(); // init char indexes
}


/*
================
R_CharMapScale
================
*/
float R_CharMapScale(void)
{
	return draw_chars->width / 128.0; //current width / original width
}


unsigned char_count;
/*
================
R_InitChars
================
*/
void R_InitChars(void)
{
	char_count = 0;
}

/*
================
R_FlushChars
================
*/
void R_FlushChars(void)
{
	if (rb_vertex == 0 || rb_index == 0) // nothing to flush
		return;

	GL_Disable(GL_ALPHA_TEST);
	GL_TexEnv(GL_MODULATE);
	GL_Enable(GL_BLEND);
	GL_DepthMask(false);
	GL_Bind(draw_chars->texnum);

	RB_RenderMeshGeneric(false);
	char_count = 0;

	GL_DepthMask(true);
	GL_Disable(GL_BLEND);
	GL_TexEnv(GL_REPLACE);
	GL_Enable(GL_ALPHA_TEST);
}

/*
================
R_DrawChar
Draws one variable sized graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be smoothly scrolled off.
================
*/
void R_DrawChar(float x, float y, int num, float scale, int red, int green, int blue, int alpha, qboolean italic, qboolean last)
{
	vec2_t		texCoord[4], verts[4];
	qboolean	addChar = true;

	num &= 255;
	alpha = clamp(alpha, 1, 255); //mxd

	if ((num & 127) == 32)	// space
		addChar = false;
	if (y <= -(scale * DEFAULT_FONT_SIZE))	// totally off screen
		addChar = false;

	const int row = num >> 4;
	const int col = num & 15;

	const float frow = row * 0.0625;
	const float fcol = col * 0.0625;
	const float size = 0.0625;
	const float cscale = scale * DEFAULT_FONT_SIZE;

	const float italicAdd = (italic ? cscale * 0.25 : 0);

	if (addChar)
	{
		Vector2Set(texCoord[0], fcol, frow);
		Vector2Set(texCoord[1], fcol + size, frow);
		Vector2Set(texCoord[2], fcol + size, frow + size);
		Vector2Set(texCoord[3], fcol, frow + size);

		Vector2Set(verts[0], x + italicAdd, y);
		Vector2Set(verts[1], x + cscale + italicAdd, y);
		Vector2Set(verts[2], x + cscale - italicAdd, y + cscale);
		Vector2Set(verts[3], x - italicAdd, y + cscale);

		if (char_count == 0)
			rb_vertex = rb_index = 0;
		if (rb_vertex + 4 >= MAX_VERTICES || rb_index + 6 >= MAX_INDICES)
			R_FlushChars();

		indexArray[rb_index++] = rb_vertex + 0;
		indexArray[rb_index++] = rb_vertex + 1;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex + 0;
		indexArray[rb_index++] = rb_vertex + 2;
		indexArray[rb_index++] = rb_vertex + 3;

		for (int i = 0; i < 4; i++)
		{
			VA_SetElem2(texCoordArray[0][rb_vertex], texCoord[i][0], texCoord[i][1]);
			VA_SetElem3(vertexArray[rb_vertex], verts[i][0], verts[i][1], 0);
			VA_SetElem4(colorArray[rb_vertex], red * DIV255, green * DIV255, blue * DIV255, alpha * DIV255);
			rb_vertex++;
		}

		char_count++;
	}

	if (last)
		R_FlushChars();
}


/*
=============
R_DrawFindPic
=============
*/
image_t	*R_DrawFindPic(char *name)
{
	image_t *gl;
	char	fullname[MAX_QPATH];

	if (name[0] != '/' && name[0] != '\\')
	{
		Com_sprintf(fullname, sizeof(fullname), "pics/%s.pcx", name);
		gl = R_FindImage(fullname, it_pic, false);
	}
	else
	{
		gl = R_FindImage(name + 1, it_pic, false);
	}

	return gl;
}

/*
=============
R_DrawGetPicSize
=============
*/
void R_DrawGetPicSize(int *w, int *h, char *pic)
{
	image_t *gl = R_DrawFindPic(pic);
	if (!gl)
	{
		*w = *h = 0; // returned -1 in KMQ2
		return;
	}

	// Factor in replace scale, so tga/jpg replacements are scaled down...
	*w = (int)(gl->width  * gl->replace_scale_w);
	*h = (int)(gl->height * gl->replace_scale_h); //BUG? mxd. was replace_scale_w
}


/*
=============
R_DrawStretchPic
=============
*/
void R_DrawStretchPic(int x, int y, int w, int h, char *pic, float alpha)
{
	vec2_t texCoord[4], verts[4];

	image_t *gl = R_DrawFindPic(pic);
	if (!gl)
		return; //mxd. R_FindImage has already printed a warning if the image wasn't found

	if (scrap_dirty)
		Scrap_Upload();

	// Psychospaz's transparent console support
	if (gl->has_alpha || alpha < 1.0)
	{
		GL_Disable(GL_ALPHA_TEST);
		GL_TexEnv(GL_MODULATE);
		GL_Enable(GL_BLEND);
		GL_DepthMask(false);
	}

	GL_Bind(gl->texnum);

	Vector2Set(texCoord[0], gl->sl, gl->tl);
	Vector2Set(texCoord[1], gl->sh, gl->tl);
	Vector2Set(texCoord[2], gl->sh, gl->th);
	Vector2Set(texCoord[3], gl->sl, gl->th);

	Vector2Set(verts[0], x, y);
	Vector2Set(verts[1], x + w, y);
	Vector2Set(verts[2], x + w, y + h);
	Vector2Set(verts[3], x, y + h);

	rb_vertex = rb_index = 0;

	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 3;

	for (int i = 0; i < 4; i++)
	{
		VA_SetElem2(texCoordArray[0][rb_vertex], texCoord[i][0], texCoord[i][1]);
		VA_SetElem3(vertexArray[rb_vertex], verts[i][0], verts[i][1], 0);
		VA_SetElem4(colorArray[rb_vertex], 1.0, 1.0, 1.0, alpha);
		rb_vertex++;
	}

	RB_RenderMeshGeneric(false);

	// Psychospaz's transparent console support
	if (gl->has_alpha || alpha < 1.0)
	{
		GL_DepthMask(true);
		GL_TexEnv(GL_REPLACE);
		GL_Disable(GL_BLEND);
		GL_Enable(GL_ALPHA_TEST);
	}
}


/*
=============
R_DrawScaledPic
Psychospaz's code for drawing stretched crosshairs
=============
*/
void R_DrawScaledPic(int x, int y, float scale, float alpha, char *pic)
{
	vec2_t texCoord[4], verts[4];

	image_t *gl = R_DrawFindPic(pic);
	if (!gl)
	{
		VID_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload();

	// add alpha support
	if (gl->has_alpha || alpha < 1.0)
	{
		GL_Disable(GL_ALPHA_TEST);
		GL_TexEnv(GL_MODULATE);
		GL_Enable(GL_BLEND);
		GL_DepthMask(false);
	}

	GL_Bind(gl->texnum);

	float scale_x = scale;
	float scale_y = scale;
	scale_x *= gl->replace_scale_w; // scale down if replacing a pcx image
	scale_y *= gl->replace_scale_h; // scale down if replacing a pcx image

	Vector2Set(texCoord[0], gl->sl, gl->tl);
	Vector2Set(texCoord[1], gl->sh, gl->tl);
	Vector2Set(texCoord[2], gl->sh, gl->th);
	Vector2Set(texCoord[3], gl->sl, gl->th);

	const float xoff = gl->width * scale_x - gl->width;
	const float yoff = gl->height * scale_y - gl->height;

	Vector2Set(verts[0], x, y);
	Vector2Set(verts[1], x + gl->width + xoff, y);
	Vector2Set(verts[2], x + gl->width + xoff, y + gl->height + yoff);
	Vector2Set(verts[3], x, y + gl->height + yoff);

	rb_vertex = rb_index = 0;

	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 3;

	for (int i = 0; i < 4; i++)
	{
		VA_SetElem2(texCoordArray[0][rb_vertex], texCoord[i][0], texCoord[i][1]);
		VA_SetElem3(vertexArray[rb_vertex], verts[i][0], verts[i][1], 0);
		VA_SetElem4(colorArray[rb_vertex], 1.0, 1.0, 1.0, alpha);
		rb_vertex++;
	}

	RB_RenderMeshGeneric(false);

	if (gl->has_alpha || alpha < 1.0)
	{
		GL_DepthMask(true);
		GL_TexEnv(GL_REPLACE);
		GL_Disable(GL_BLEND);
		GL_Enable(GL_ALPHA_TEST); // add alpha support
	}
}


/*
=============
R_DrawPic
=============
*/
void R_DrawPic(int x, int y, char *pic)
{
	vec2_t texCoord[4], verts[4];

	image_t *gl = R_DrawFindPic(pic);
	if (!gl)
	{
		VID_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload();

	GL_Bind(gl->texnum);

	Vector2Set(texCoord[0], gl->sl, gl->tl);
	Vector2Set(texCoord[1], gl->sh, gl->tl);
	Vector2Set(texCoord[2], gl->sh, gl->th);
	Vector2Set(texCoord[3], gl->sl, gl->th);

	Vector2Set(verts[0], x, y);
	Vector2Set(verts[1], x + gl->width, y);
	Vector2Set(verts[2], x + gl->width, y + gl->height);
	Vector2Set(verts[3], x, y + gl->height);

	rb_vertex = rb_index = 0;

	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 3;

	for (int i = 0; i < 4; i++)
	{
		VA_SetElem2(texCoordArray[0][rb_vertex], texCoord[i][0], texCoord[i][1]);
		VA_SetElem3(vertexArray[rb_vertex], verts[i][0], verts[i][1], 0);
		VA_SetElem4(colorArray[rb_vertex], 1.0, 1.0, 1.0, 1.0);
		rb_vertex++;
	}

	RB_RenderMeshGeneric(false);
}

/*
=============
R_DrawTileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down refresh window.
=============
*/
void R_DrawTileClear(int x, int y, int w, int h, char *pic)
{
	vec2_t texCoord[4], verts[4];

	image_t *image = R_DrawFindPic(pic);
	if (!image)
	{
		VID_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	GL_Bind(image->texnum);

	Vector2Set(texCoord[0], (float)x / (float)image->width, (float)y / (float)image->height);
	Vector2Set(texCoord[1], (float)(x + w) / (float)image->width, (float)y / (float)image->height);
	Vector2Set(texCoord[2], (float)(x + w) / (float)image->width, (float)(y + h) / (float)image->height);
	Vector2Set(texCoord[3], (float)x / (float)image->width, (float)(y + h) / (float)image->height);

	Vector2Set(verts[0], x, y);
	Vector2Set(verts[1], x + w, y);
	Vector2Set(verts[2], x + w, y + h);
	Vector2Set(verts[3], x, y + h);

	rb_vertex = rb_index = 0;

	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 3;

	for (int i = 0; i < 4; i++)
	{
		VA_SetElem2(texCoordArray[0][rb_vertex], texCoord[i][0], texCoord[i][1]);
		VA_SetElem3(vertexArray[rb_vertex], verts[i][0], verts[i][1], 0);
		VA_SetElem4(colorArray[rb_vertex], 1.0, 1.0, 1.0, 1.0);
		rb_vertex++;
	}

	RB_RenderMeshGeneric(false);
}


/*
======================
R_DrawFill

Fills a box of pixels with a 24-bit color w/ alpha
===========================
*/
void R_DrawFill(int x, int y, int w, int h, int red, int green, int blue, int alpha)
{
	vec2_t verts[4];

	red = min(red, 255);
	green = min(green, 255);
	blue = min(blue, 255);
	alpha = clamp(alpha, 1, 255);

	GL_Disable(GL_ALPHA_TEST);
	GL_TexEnv(GL_MODULATE);
	GL_Enable(GL_BLEND);
	GL_DepthMask(false);

	GL_Bind(glMedia.whitetexture->texnum);

	Vector2Set(verts[0], x, y);
	Vector2Set(verts[1], x + w, y);
	Vector2Set(verts[2], x + w, y + h);
	Vector2Set(verts[3], x, y + h);

	rb_vertex = rb_index = 0;

	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 3;

	for (int i = 0; i < 4; i++)
	{
		VA_SetElem3(vertexArray[rb_vertex], verts[i][0], verts[i][1], 0);
		VA_SetElem4(colorArray[rb_vertex], red * DIV255, green * DIV255, blue * DIV255, alpha * DIV255);
		rb_vertex++;
	}

	RB_RenderMeshGeneric(false);

	GL_DepthMask(true);
	GL_Disable(GL_BLEND);
	GL_TexEnv(GL_REPLACE);
	GL_Enable(GL_ALPHA_TEST);
}

//=============================================================================

/*
=============
R_DrawCameraEffect

Video camera effect
=============
*/
extern void Mod_SetRenderParmsDefaults(renderparms_t *parms);

void R_DrawCameraEffect(void)
{
	image_t			*image[2];
	float			texparms[2][4];
	vec2_t			texCoord[4];
	vec3_t			verts[4];
	renderparms_t	cameraParms;

	image[0] = R_DrawFindPic("/gfx/2d/screenstatic.tga");
	image[1] = R_DrawFindPic("/gfx/2d/scanlines.tga");

	if (!image[0] || !image[1])
		return;

	const int w = vid.width;
	const int h = vid.height;

	GL_Disable(GL_ALPHA_TEST);
	GL_TexEnv(GL_MODULATE);
	GL_Enable(GL_BLEND);
	GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_DepthMask(false);

	VectorSet(verts[0], 0, 0, 0);
	VectorSet(verts[1], w, 0, 0);
	VectorSet(verts[2], w, h, 0);
	VectorSet(verts[3], 0, h, 0);

	Vector4Set(texparms[0], 2, 2, -30, 10);
	Vector4Set(texparms[1], 1, 10, 0, 0);

	rb_vertex = rb_index = 0;

	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 3;

	rb_vertex = 4;

	for (int i = 0; i < 2; i++)
	{
		GL_Bind(image[i]->texnum);

		Vector2Set(texCoord[0], 0, 0);
		Vector2Set(texCoord[1], w / image[i]->width, 0);
		Vector2Set(texCoord[2], w / image[i]->width, h / image[i]->height);
		Vector2Set(texCoord[3], 0, h / image[i]->height);

		Mod_SetRenderParmsDefaults(&cameraParms);

		cameraParms.scale_x = texparms[i][0];
		cameraParms.scale_y = texparms[i][1];
		cameraParms.scroll_x = texparms[i][2];
		cameraParms.scroll_y = texparms[i][3];

		RB_ModifyTextureCoords(&texCoord[0][0], &verts[0][0], 4, cameraParms);

		for (int j = 0; j < 4; j++)
		{
			VA_SetElem2(texCoordArray[0][j], texCoord[j][0], texCoord[j][1]);
			VA_SetElem3(vertexArray[j], verts[j][0], verts[j][1], verts[j][2]);
			VA_SetElem4(colorArray[j], 1, 1, 1, 1);
		}

		RB_DrawArrays();
	}

	rb_vertex = rb_index = 0;

	GL_DepthMask(true);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_Disable(GL_BLEND);
	GL_TexEnv(GL_REPLACE);
	GL_Enable(GL_ALPHA_TEST);
}


//=============================================================================

/*
=============
R_DrawStretchRaw

Cinematic streaming
=============
*/
#ifdef ROQ_SUPPORT

void R_DrawStretchRaw(int x, int y, int w, int h, const byte *raw, int rawWidth, int rawHeight)
{
	int width = 1;
	int height = 1;
	vec2_t texCoord[4], verts[4];

	// Check the dimensions
	if (!glConfig.arbTextureNonPowerOfTwo) // skip if nonstandard textures sizes are supported
	{
		while (width < rawWidth)
			width <<= 1;

		while (height < rawHeight)
			height <<= 1;

		if (rawWidth != width || rawHeight != height)
			VID_Error(ERR_DROP, "R_DrawStretchRaw: size is not a power of two (%i x %i)", rawWidth, rawHeight);

		if (rawWidth > glConfig.max_texsize || rawHeight > glConfig.max_texsize)
			VID_Error(ERR_DROP, "R_DrawStretchRaw: size exceeds hardware limits (%i > %i or %i > %i)", rawWidth, glConfig.max_texsize, rawHeight, glConfig.max_texsize);
	}

	// Update the texture as appropriate
	GL_Bind(glMedia.rawtexture->texnum);
	
	if (rawWidth == glMedia.rawtexture->upload_width && rawHeight == glMedia.rawtexture->upload_height)
	{
		qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rawWidth, rawHeight, GL_RGBA, GL_UNSIGNED_BYTE, raw);
	}
	else
	{
		glMedia.rawtexture->upload_width = rawWidth;
		glMedia.rawtexture->upload_height = rawHeight;
		qglTexImage2D(GL_TEXTURE_2D, 0, gl_tex_solid_format, rawWidth, rawHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, raw);
	}

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Draw it
	Vector2Set(texCoord[0], 0, 0);
	Vector2Set(texCoord[1], 1, 0);
	Vector2Set(texCoord[2], 1, 1);
	Vector2Set(texCoord[3], 0, 1);

	Vector2Set(verts[0], x, y);
	Vector2Set(verts[1], x + w, y);
	Vector2Set(verts[2], x + w, y + h);
	Vector2Set(verts[3], x, y + h);

	rb_vertex = rb_index = 0;

	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 3;

	for (int i = 0; i < 4; i++)
	{
		VA_SetElem2(texCoordArray[0][rb_vertex], texCoord[i][0], texCoord[i][1]);
		VA_SetElem3(vertexArray[rb_vertex], verts[i][0], verts[i][1], 0);
		VA_SetElem4(colorArray[rb_vertex], 1, 1, 1, 1);
		rb_vertex++;
	}

	RB_RenderMeshGeneric(false);
}

#else // old 8-bit, 256x256 version

extern unsigned r_rawpalette[256];

void R_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data)
{
	unsigned		image32[256 * 256];
	unsigned char	image8[256 * 256];
	int				trows;
	float			hscale;
	vec2_t			texCoord[4], verts[4];

	// Nicolas' fix for stray pixels at bottom and top
	memset(image32, 0, sizeof(image32));

	GL_Bind(0);

	if (rows <= 256)
	{
		hscale = 1.0;
		trows = rows;
	}
	else
	{
		hscale = rows / 256.0;
		trows = 256;
	}

	const float t = rows * hscale / 256;

	if (!qglColorTableEXT)
	{
		for (int i = 0; i < trows; i++)
		{
			const int row = (int)(i * hscale);
			if (row > rows)
				break;

			byte *source = data + cols * row;
			unsigned *dest = &image32[i * 256];
			const int fracstep = cols * 0x10000 / 256;
			int frac = fracstep >> 1;

			for (int j = 0; j < 256; j++)
			{
				dest[j] = r_rawpalette[source[frac >> 16]];
				frac += fracstep;
			}
		}

		qglTexImage2D(GL_TEXTURE_2D, 0, gl_tex_solid_format, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, image32);
	}
	else
	{
		for (int i = 0; i < trows; i++)
		{
			const int row = (int)(i * hscale);
			if (row > rows)
				break;

			byte *source = data + cols * row;
			unsigned char *dest = &image8[i * 256];
			const int fracstep = cols * 0x10000 / 256;
			int frac = fracstep >> 1;

			for (int j = 0; j < 256; j++)
			{
				dest[j] = source[frac >> 16];
				frac += fracstep;
			}
		}

		qglTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, 256, 256, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, image8);
	}

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	//if (glConfig.renderer == GLREND_MCD || glConfig.renderer & GLREND_RENDITION) //mxd. Err... Who cares?
		//GL_Disable(GL_ALPHA_TEST);

	// Draw it
	Vector2Set(texCoord[0], 0, 0);
	Vector2Set(texCoord[1], 1, 0);
	Vector2Set(texCoord[2], 1, t);
	Vector2Set(texCoord[3], 0, t);

	Vector2Set(verts[0], x, y);
	Vector2Set(verts[1], x + w, y);
	Vector2Set(verts[2], x + w, y + h);
	Vector2Set(verts[3], x, y + h);

	rb_vertex = rb_index = 0;
	indexArray[rb_index++] = rb_vertex;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 3;

	for (int i = 0; i < 4; i++)
	{
		VA_SetElem2(texCoordArray[0][rb_vertex], texCoord[i][0], texCoord[i][1]);
		VA_SetElem3(vertexArray[rb_vertex], verts[i][0], verts[i][1], 0);
		VA_SetElem4(colorArray[rb_vertex], 1, 1, 1, 1);
		rb_vertex++;
	}

	RB_RenderMeshGeneric(false);

	//if (glConfig.renderer == GLREND_MCD || glConfig.renderer & GLREND_RENDITION) //mxd. Err... Who cares? 
		//GL_Enable(GL_ALPHA_TEST);
}
#endif // ROQ_SUPPORT