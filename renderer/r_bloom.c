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

// r_bloom.c: 2D lighting post process effect

#include "r_local.h"

static float diamond8x[8][8] =
{
	0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, 
	0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f, 
	0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f, 
	0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f, 
	0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f, 
	0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f, 
	0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f, 
	0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f
};

static float diamond6x[6][6] =
{
	0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 
	0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f,  
	0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f, 
	0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f, 
	0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f, 
	0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f
};

static float diamond4x[4][4] = 
{
	0.3f, 0.4f, 0.4f, 0.3f,  
	0.4f, 0.9f, 0.9f, 0.4f, 
	0.4f, 0.9f, 0.9f, 0.4f, 
	0.3f, 0.4f, 0.4f, 0.3f
};

static int bloom_size;

cvar_t *r_bloom_alpha;
cvar_t *r_bloom_diamond_size;
cvar_t *r_bloom_intensity;
cvar_t *r_bloom_threshold;
cvar_t *r_bloom_darken;
cvar_t *r_bloom_sample_size;
cvar_t *r_bloom_fast_sample;

image_t *r_bloomscreentexture;
image_t *r_bloomeffecttexture;
image_t *r_bloombackuptexture;
image_t *r_bloomdownsamplingtexture;

static int r_screendownsamplingtexture_size;
static int screen_texture_width;
static int screen_texture_height;
static int r_screenbackuptexture_width;
static int r_screenbackuptexture_height;

// Current refdef size:
static int curView_x;
static int curView_y;
static int curView_width;
static int curView_height;

// Texture coordinates of screen data inside screentexture
static float screenText_tcw;
static float screenText_tch;

static int sample_width;
static int sample_height;

// Texture coordinates of adjusted textures
static float sampleText_tcw;
static float sampleText_tch;

// this macro is in sample size workspace coordinates
#define R_Bloom_DrawStart			\
	rb_vertex = rb_index = 0;

#define R_Bloom_DrawFinish			\
	RB_DrawArrays();				\
	rb_vertex = rb_index = 0;

#define R_Bloom_SamplePass( xpos, ypos, r, g, b, a )								\
	indexArray[rb_index++] = rb_vertex+0;											\
	indexArray[rb_index++] = rb_vertex+1;											\
	indexArray[rb_index++] = rb_vertex+2;											\
	indexArray[rb_index++] = rb_vertex+0;											\
	indexArray[rb_index++] = rb_vertex+2;											\
	indexArray[rb_index++] = rb_vertex+3;											\
	VA_SetElem2(texCoordArray[0][rb_vertex], 0, sampleText_tch);					\
	VA_SetElem3(vertexArray[rb_vertex], xpos, ypos, 0);								\
	VA_SetElem4(colorArray[rb_vertex], r, g, b, a);									\
	rb_vertex++;																	\
	VA_SetElem2(texCoordArray[0][rb_vertex], 0, 0);									\
	VA_SetElem3(vertexArray[rb_vertex], xpos, ypos+sample_height, 0);				\
	VA_SetElem4(colorArray[rb_vertex], r, g, b, a);									\
	rb_vertex++;																	\
	VA_SetElem2(texCoordArray[0][rb_vertex], sampleText_tcw, 0);					\
	VA_SetElem3(vertexArray[rb_vertex], xpos+sample_width, ypos+sample_height, 0);	\
	VA_SetElem4(colorArray[rb_vertex], r, g, b, a);									\
	rb_vertex++;																	\
	VA_SetElem2(texCoordArray[0][rb_vertex], sampleText_tcw, sampleText_tch);		\
	VA_SetElem3(vertexArray[rb_vertex], xpos+sample_width, ypos, 0);				\
	VA_SetElem4(colorArray[rb_vertex], r, g, b, a);									\
	rb_vertex++;
	
#define R_Bloom_Quad( x, y, width, height, textwidth, textheight, r, g, b, a )	\
	rb_vertex = rb_index = 0;													\
	indexArray[rb_index++] = rb_vertex+0;										\
	indexArray[rb_index++] = rb_vertex+1;										\
	indexArray[rb_index++] = rb_vertex+2;										\
	indexArray[rb_index++] = rb_vertex+0;										\
	indexArray[rb_index++] = rb_vertex+2;										\
	indexArray[rb_index++] = rb_vertex+3;										\
	VA_SetElem2(texCoordArray[0][rb_vertex], 0, textheight);					\
	VA_SetElem3(vertexArray[rb_vertex],	x, y, 0);								\
	VA_SetElem4(colorArray[rb_vertex], r, g, b, a);								\
	rb_vertex++;																\
	VA_SetElem2(texCoordArray[0][rb_vertex], 0,	0);								\
	VA_SetElem3(vertexArray[rb_vertex], x, y+height, 0);						\
	VA_SetElem4(colorArray[rb_vertex], r, g, b, a);								\
	rb_vertex++;																\
	VA_SetElem2(texCoordArray[0][rb_vertex], textwidth,	0);						\
	VA_SetElem3(vertexArray[rb_vertex], x+width, y+height, 0);					\
	VA_SetElem4(colorArray[rb_vertex], r, g, b, a);								\
	rb_vertex++;																\
	VA_SetElem2(texCoordArray[0][rb_vertex], textwidth,	textheight);			\
	VA_SetElem3(vertexArray[rb_vertex], x+width, y, 0);							\
	VA_SetElem4(colorArray[rb_vertex], r, g, b, a);								\
	rb_vertex++;																\
	RB_DrawArrays();															\
	rb_vertex = rb_index = 0;

static void R_Bloom_InitBackUpTexture(int width, int height)
{
	byte* data = malloc(width * height * 4);
	memset(data, 0, width * height * 4);

	r_screenbackuptexture_width = width;
	r_screenbackuptexture_height = height; 

	r_bloombackuptexture = R_LoadPic("***r_bloombackuptexture***", (byte *)data, width, height, it_pic, 32);
	
	free(data);
}

static void R_Bloom_InitEffectTexture(void)
{
	if (r_bloom_sample_size->integer < 32)
		Cvar_SetValue("r_bloom_sample_size", 32);

	// Make sure bloom size is a power of 2
	bloom_size = r_bloom_sample_size->integer;
	float bloomsizecheck = (float)bloom_size;

	while (bloomsizecheck > 1.0f)
		bloomsizecheck /= 2.0f;

	if (bloomsizecheck != 1.0f)
	{
		bloom_size = 32;
		while(bloom_size < r_bloom_sample_size->integer)
			bloom_size *= 2;
	}

	// Make sure bloom size doesn't have stupid values
	if (bloom_size > screen_texture_width || bloom_size > screen_texture_height)
		bloom_size = min(screen_texture_width, screen_texture_height);

	if (bloom_size != r_bloom_sample_size->integer)
		Cvar_SetValue("r_bloom_sample_size", bloom_size);

	byte* data = malloc(bloom_size * bloom_size * 4);
	memset(data, 0, bloom_size * bloom_size * 4);

	r_bloomeffecttexture = R_LoadPic("***r_bloomeffecttexture***", (byte *)data, bloom_size, bloom_size, it_pic, 32);
	
	free(data);
}

static void R_Bloom_InitTextures(void)
{
	// Find closer power of 2 to screen size
	for (screen_texture_width = 1; screen_texture_width < vid.width; screen_texture_width *= 2) { }
	for (screen_texture_height = 1; screen_texture_height < vid.height; screen_texture_height *= 2) { }

	// Init the screen texture
	const int size = screen_texture_width * screen_texture_height * 4;
	byte* data = malloc(size);
	memset(data, 255, size);
	r_bloomscreentexture = R_LoadPic("***r_bloomscreentexture***", (byte *)data, screen_texture_width, screen_texture_height, it_pic, 32);
	free(data);

	// Validate bloom size and init the bloom effect texture.
	R_Bloom_InitEffectTexture();

	// If screensize is more than 2x the bloom effect texture, set up for stepped downsampling.
	r_bloomdownsamplingtexture = NULL;
	r_screendownsamplingtexture_size = 0;

	if(vid.width > (bloom_size * 2) && !r_bloom_fast_sample->value)
	{
		r_screendownsamplingtexture_size = bloom_size * 2;
		data = malloc(r_screendownsamplingtexture_size * r_screendownsamplingtexture_size * 4);
		memset(data, 0, r_screendownsamplingtexture_size * r_screendownsamplingtexture_size * 4);
		r_bloomdownsamplingtexture = R_LoadPic("***r_bloomdownsamplingtexture***", (byte *)data, r_screendownsamplingtexture_size, r_screendownsamplingtexture_size, it_pic, 32);
		free(data);
	}

	// Init the screen backup texture
	const int backuptexsize = (r_screendownsamplingtexture_size > 0 ? r_screendownsamplingtexture_size : bloom_size);
	R_Bloom_InitBackUpTexture(backuptexsize, backuptexsize);
}

void R_InitBloomTextures(void)
{
	r_bloom_alpha = Cvar_Get("r_bloom_alpha", "0.25", CVAR_ARCHIVE); // Was 0.33
	r_bloom_diamond_size = Cvar_Get("r_bloom_diamond_size", "8", CVAR_ARCHIVE);
	r_bloom_intensity = Cvar_Get("r_bloom_intensity", "2.5", CVAR_ARCHIVE);	// Was 0.6
	r_bloom_threshold = Cvar_Get("r_bloom_threshold", "0.68", CVAR_ARCHIVE); // Was 0.08
	r_bloom_darken = Cvar_Get("r_bloom_darken", "5", CVAR_ARCHIVE); // Was 4
	r_bloom_sample_size = Cvar_Get("r_bloom_sample_size", "256", CVAR_ARCHIVE);	// Was 128
	r_bloom_fast_sample = Cvar_Get("r_bloom_fast_sample", "0", CVAR_ARCHIVE);

	bloom_size = 0;
	if (!r_bloom->integer)
		return;

	R_Bloom_InitTextures();
}

static void R_Bloom_DrawEffect(void)
{
	GL_Bind(r_bloomeffecttexture->texnum);
	GL_Enable(GL_BLEND);
	GL_BlendFunc(GL_ONE, GL_ONE);
	GL_TexEnv(GL_MODULATE);

	rb_vertex = 0;
	rb_index = 0;

	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 1;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 0;
	indexArray[rb_index++] = rb_vertex + 2;
	indexArray[rb_index++] = rb_vertex + 3;

	VA_SetElem2(texCoordArray[0][rb_vertex], 0, sampleText_tch);
	VA_SetElem3(vertexArray[rb_vertex],	curView_x, curView_y, 0);
	VA_SetElem4(colorArray[rb_vertex], r_bloom_alpha->value, r_bloom_alpha->value, r_bloom_alpha->value, 1.0f);
	rb_vertex++;

	VA_SetElem2(texCoordArray[0][rb_vertex], 0,	0);
	VA_SetElem3(vertexArray[rb_vertex], curView_x, curView_y + curView_height, 0);
	VA_SetElem4(colorArray[rb_vertex], r_bloom_alpha->value, r_bloom_alpha->value, r_bloom_alpha->value, 1.0f);
	rb_vertex++;

	VA_SetElem2(texCoordArray[0][rb_vertex], sampleText_tcw, 0);
	VA_SetElem3(vertexArray[rb_vertex], curView_x + curView_width, curView_y + curView_height, 0);
	VA_SetElem4(colorArray[rb_vertex], r_bloom_alpha->value, r_bloom_alpha->value, r_bloom_alpha->value, 1.0f);
	rb_vertex++;

	VA_SetElem2(texCoordArray[0][rb_vertex], sampleText_tcw, sampleText_tch);
	VA_SetElem3(vertexArray[rb_vertex], curView_x + curView_width, curView_y, 0);
	VA_SetElem4(colorArray[rb_vertex], r_bloom_alpha->value, r_bloom_alpha->value, r_bloom_alpha->value, 1.0f);
	rb_vertex++;

	RB_DrawArrays();

	rb_vertex = 0;
	rb_index = 0;

	GL_Disable(GL_BLEND);
}

static void R_Bloom_GeneratexDiamonds(void)
{
	// Set up sample size workspace
	qglViewport(0, 0, sample_width, sample_height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, sample_width, sample_height, 0, -10, 100);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	// Copy small scene into r_bloomeffecttexture
	GL_Bind(r_bloomeffecttexture->texnum);
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

	// Start modifying the small scene corner
	GL_Enable(GL_BLEND);

	// Darkening passes
	if (r_bloom_darken->value)
	{
		GL_BlendFunc(GL_DST_COLOR, GL_ZERO);
		GL_TexEnv(GL_MODULATE);
		
		R_Bloom_DrawStart
		for (int i = 0; i < r_bloom_darken->value; i++)
		{
			R_Bloom_SamplePass(0, 0, 1.0f, 1.0f, 1.0f, 1.0f);
		}
		R_Bloom_DrawFinish

		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);
	}

	// Bluring passes
	GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
	
	if (r_bloom_diamond_size->value > 7 || r_bloom_diamond_size->value <= 3)
	{
		if (r_bloom_diamond_size->integer != 8)
			Cvar_SetValue("r_bloom_diamond_size", 8);

		R_Bloom_DrawStart
		for (int i = 0; i < r_bloom_diamond_size->value; i++)
		{
			for (int j = 0; j < r_bloom_diamond_size->value; j++)
			{
				const float intensity = r_bloom_intensity->value * 0.3 * diamond8x[i][j];
				if (intensity < r_bloom_threshold->value)
					continue;

				R_Bloom_SamplePass(i - 4, j - 4, intensity, intensity, intensity, 1.0);
			}
		}
		R_Bloom_DrawFinish
	}
	else if (r_bloom_diamond_size->value > 5)
	{
		if (r_bloom_diamond_size->value != 6)
			Cvar_SetValue("r_bloom_diamond_size", 6);

		R_Bloom_DrawStart
		for (int i = 0; i < r_bloom_diamond_size->value; i++)
		{
			for (int j = 0; j < r_bloom_diamond_size->value; j++)
			{
				const float intensity = r_bloom_intensity->value * 0.5 * diamond6x[i][j];
				if (intensity < r_bloom_threshold->value)
					continue;

				R_Bloom_SamplePass(i - 3, j - 3, intensity, intensity, intensity, 1.0);
			}
		}
		R_Bloom_DrawFinish
	}
	else if (r_bloom_diamond_size->value > 3)
	{
		if (r_bloom_diamond_size->integer != 4)
			Cvar_SetValue("r_bloom_diamond_size", 4);

		R_Bloom_DrawStart
		for (int i = 0; i < r_bloom_diamond_size->value; i++)
		{
			for (int j = 0; j < r_bloom_diamond_size->value; j++)
			{
				const float intensity = r_bloom_intensity->value * 0.8f * diamond4x[i][j];
				if (intensity < r_bloom_threshold->value)
					continue;

				R_Bloom_SamplePass(i - 2, j - 2, intensity, intensity, intensity, 1.0);
			}
		}
		R_Bloom_DrawFinish
	}
	
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

	// Restore full screen workspace
	qglViewport(0, 0, vid.width, vid.height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, vid.width, vid.height, 0, -10, 100);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();
}

static void R_Bloom_DownsampleView(void)
{
	GL_Disable(GL_BLEND);

	// Stepped downsample
	if (r_screendownsamplingtexture_size)
	{
		const int midsample_width = r_screendownsamplingtexture_size * sampleText_tcw;
		const int midsample_height = r_screendownsamplingtexture_size * sampleText_tch;
		
		// Copy the screen and draw resized
		GL_Bind(r_bloomscreentexture->texnum);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, curView_x, vid.height - (curView_y + curView_height), curView_width, curView_height);
		R_Bloom_Quad(0, vid.height - midsample_height, midsample_width, midsample_height, screenText_tcw, screenText_tch, 1.0f, 1.0f, 1.0f, 1.0f);
		
		// Now copy into Downsampling (mid-sized) texture
		GL_Bind(r_bloomdownsamplingtexture->texnum);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, midsample_width, midsample_height);

		// Now draw again in bloom size
		R_Bloom_Quad(0, vid.height - sample_height, sample_width, sample_height, sampleText_tcw, sampleText_tch, 0.5f, 0.5f, 0.5f, 1.0f);
		
		// Now blend the big screen texture into the bloom generation space (hoping it adds some blur)
		GL_Enable(GL_BLEND);
		GL_BlendFunc(GL_ONE, GL_ONE);
		GL_Bind(r_bloomscreentexture->texnum);
		R_Bloom_Quad(0, vid.height - sample_height, sample_width, sample_height, screenText_tcw, screenText_tch, 0.5f, 0.5f, 0.5f, 1.0f);
		GL_Disable(GL_BLEND);
	}
	else // Downsample simple
	{	
		GL_Bind(r_bloomscreentexture->texnum);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, curView_x, vid.height - (curView_y + curView_height), curView_width, curView_height);
		R_Bloom_Quad(0, vid.height - sample_height, sample_width, sample_height, screenText_tcw, screenText_tch, 1.0f, 1.0f, 1.0f, 1.0f);
	}
}

extern void R_SetupGL(void);

void R_BloomBlend(refdef_t *fd)
{
	if ((fd->rdflags & RDF_NOWORLDMODEL) || !r_bloom->value || r_showtris->value)
		return;

	if (!bloom_size)
		R_Bloom_InitTextures();

	if (screen_texture_width < bloom_size || screen_texture_height < bloom_size)
		return;
	
	// Set up full screen workspace
	qglViewport(0, 0, vid.width, vid.height);
	GL_TexEnv(GL_REPLACE); // Knightmare added
	GL_Disable(GL_DEPTH_TEST);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, vid.width, vid.height, 0, -10, 100);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();
	GL_Disable(GL_CULL_FACE);

	GL_Disable(GL_BLEND);
	GL_EnableTexture(0);

	// Set up current sizes
	curView_x = fd->x;
	curView_y = fd->y;
	curView_width = fd->width;
	curView_height = fd->height;
	screenText_tcw = ((float)fd->width / (float)screen_texture_width);
	screenText_tch = ((float)fd->height / (float)screen_texture_height);

	if (fd->height > fd->width)
	{
		sampleText_tcw = ((float)fd->width / (float)fd->height);
		sampleText_tch = 1.0f;
	}
	else
	{
		sampleText_tcw = 1.0f;
		sampleText_tch = ((float)fd->height / (float)fd->width);
	}

	sample_width = bloom_size * sampleText_tcw;
	sample_height = bloom_size * sampleText_tch;
	
	// Copy the screen space we'll use to work into the backup texture
	GL_Bind(r_bloombackuptexture->texnum);
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, r_screenbackuptexture_width, r_screenbackuptexture_height);

	// Create the bloom image
	R_Bloom_DownsampleView();
	R_Bloom_GeneratexDiamonds();

	// Restore the screen-backup to the screen
	GL_Disable(GL_BLEND);
	GL_Bind(r_bloombackuptexture->texnum);
	R_Bloom_Quad(0, vid.height - r_screenbackuptexture_height,
		r_screenbackuptexture_width,
		r_screenbackuptexture_height, 1.0, 1.0, 1, 1, 1, 1);

	R_Bloom_DrawEffect();

	// Knightmare added
	R_SetupGL();
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_EnableTexture(0);
}