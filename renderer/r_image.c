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

// r_image.c

#include "r_local.h"

image_t	gltextures[MAX_GLTEXTURES];
int		numgltextures;

static byte gammatable[256];

unsigned d_8to24table[256];

qboolean GL_Upload8(const byte *data, int width, int height, imagetype_t type, qboolean mipmap);
qboolean GL_Upload32(uint *data, int width, int height, imagetype_t type, qboolean mipmap);

int gl_filter_min = GL_LINEAR_MIPMAP_LINEAR; //mxd. Was GL_LINEAR_MIPMAP_NEAREST
int gl_filter_max = GL_LINEAR;

typedef struct
{
	char *name;
	int minimize;
	int maximize;
} glmode_t;

static glmode_t modes[] =
{
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

#define NUM_GL_MODES (sizeof(modes) / sizeof (glmode_t))

typedef struct
{
	char *name;
	int mode;
} gltmode_t;

//mxd
static void GL_ApplyTextureMode(int texnum, int filter_min, int filter_mag, float anisotropy, qboolean mipmap)
{
	GL_Bind(texnum);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (mipmap ? filter_min : filter_mag));
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_mag);

	// Set anisotropic filter if supported and enabled
	if (mipmap && glConfig.anisotropic && anisotropy)
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
}

void GL_TextureMode(char *string)
{
	unsigned mode;

	for (mode = 0; mode < NUM_GL_MODES; mode++)
		if (!Q_stricmp(modes[mode].name, string))
			break;

	if (mode == NUM_GL_MODES)
	{
		VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Bad texture filtering mode name: '%s'.\n", string);
		return;
	}

	gl_filter_min = modes[mode].minimize;
	gl_filter_max = modes[mode].maximize;

	// Clamp selected anisotropy
	if (glConfig.anisotropic)
	{
		if (r_anisotropic->value > glConfig.max_anisotropy)
			Cvar_SetValue("r_anisotropic", glConfig.max_anisotropy);
		else if (r_anisotropic->value < 1.0f)
			Cvar_SetValue("r_anisotropic", 1.0f);
	}

	// Change all the existing mipmap texture objects
	image_t *glt = gltextures;
	for (int i = 0; i < numgltextures; i++, glt++)
		if (glt->texnum > 0) //mxd
			GL_ApplyTextureMode(glt->texnum, gl_filter_min, gl_filter_max, r_anisotropic->value, glt->mipmap);

	//mxd. Change lightmap filtering when _lightmap_scale is 1. The idea is to make them look like they are part of the texture
	if (gl_lms.lmshift == 0)
		for (int i = 1; i < gl_lms.current_lightmap_texture; i++)
			GL_ApplyTextureMode(glState.lightmap_textures + i, gl_filter_min, gl_filter_max, r_anisotropic->value, false); //mxd. Lightmap textures have no mipmaps
}

#pragma region ======================= Console functions

//mxd
typedef struct
{
	char *name;
	int width;
	int height;
	int paltype;
	imagetype_t type;
} imageinfo_t;

//mxd. Sort by type, then by name
static int R_SortImageinfos(const imageinfo_t *first, const imageinfo_t *second)
{
	if (first->type != second->type)
		return first->type - second->type;

	return Q_stricmp(first->name, second->name);
}

void R_ImageList_f(void)
{
	const char *palstrings[] = { "RGB ", "RGBA" }; //mxd. +RGBA

	//mxd. Paranoia check...
	if(numgltextures == 0)
	{
		Com_Printf(S_COLOR_GREEN"No textures loaded.\n");
		return;
	}

	//mxd. Collect image infos first...
	imageinfo_t *infos = malloc(sizeof(imageinfo_t) * numgltextures);
	int numinfos = 0;
	int texels = 0;

	image_t *image = gltextures;
	for (int i = 0; i < numgltextures; i++, image++)
	{
		if (image->texnum > 0)
		{
			infos[numinfos].name = image->name;
			infos[numinfos].width = image->upload_width;
			infos[numinfos].height = image->upload_height;
			infos[numinfos].paltype = image->has_alpha;
			infos[numinfos].type = image->type;
			numinfos++;

			texels += image->upload_width * image->upload_height;
		}
	}

	if (numinfos == 0)
	{
		Com_Printf(S_COLOR_GREEN"No textures loaded.\n");
		free(infos);
		return;
	}

	//mxd. Sort infos
	qsort(infos, numinfos, sizeof(imageinfo_t), (int(*)(const void *, const void *))R_SortImageinfos);

	// Print results
	VID_Printf(PRINT_ALL, S_COLOR_GREEN"Loaded textures:\n");

	for (int i = 0; i < numinfos; i++)
	{
		switch (infos[i].type)
		{
			case it_skin:	VID_Printf(PRINT_ALL, "Skin:     "); break;
			case it_sprite:	VID_Printf(PRINT_ALL, "Sprite:   "); break;
			case it_wall:	VID_Printf(PRINT_ALL, "Wall:     "); break;
			case it_pic:	VID_Printf(PRINT_ALL, "Picture:  "); break;
			case it_part:	VID_Printf(PRINT_ALL, "Particle: "); break;
			case it_sky:	VID_Printf(PRINT_ALL, "Sky:      "); break;
			case it_font:	VID_Printf(PRINT_ALL, "Font:     "); break;
			default:		VID_Printf(PRINT_ALL, "Unknown:  "); break;
		}
		
		VID_Printf(PRINT_ALL, "%4i x %-4i %s: %s\n", infos[i].width, infos[i].height, palstrings[infos[i].paltype], infos[i].name);
	}

	VID_Printf(PRINT_ALL, S_COLOR_GREEN"Total: %i textures, %i texels (not counting mipmaps).\n", numinfos, texels); //mxd

	//mxd. Free memory
	free(infos);
}

#pragma endregion 

#pragma region ======================= Scrap allocation

// Allocate all the little status bar objects into a single texture to crutch up inefficient hardware / drivers

#define MAX_SCRAPS		1
#define BLOCK_WIDTH		256
#define BLOCK_HEIGHT	256

byte scrap_texels[MAX_SCRAPS][BLOCK_WIDTH * BLOCK_HEIGHT];
qboolean scrap_dirty;

// Returns a texture number and the position inside it
static int Scrap_AllocBlock(int w, int h, int *x, int *y)
{
	static int scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH]; //mxd. Made local

	for (int texnum = 0; texnum < MAX_SCRAPS; texnum++)
	{
		int best = BLOCK_HEIGHT;

		for (int i = 0; i < BLOCK_WIDTH - w; i++)
		{
			int best2 = 0;
			int j;
			for (j = 0; j < w; j++)
			{
				if (scrap_allocated[texnum][i + j] >= best)
					break;

				if (scrap_allocated[texnum][i + j] > best2)
					best2 = scrap_allocated[texnum][i + j];
			}

			if (j == w)
			{
				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (int i = 0; i < w; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	return -1;
}

void Scrap_Upload(void)
{
	GL_Bind(TEXNUM_SCRAPS);
	GL_Upload8(scrap_texels[0], BLOCK_WIDTH, BLOCK_HEIGHT, it_pic, false);
	scrap_dirty = false;
}

#pragma endregion

#pragma region ======================= Image flood filling

typedef struct
{
	short x, y;
} floodfill_t;

// Must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP(off, dx, dy) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + dx; \
		fifo[inpt].y = y + dy; \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) \
	{ \
		fdc = pos[off];	\
	} \
}

// Fill background pixels so mipmapping doesn't have haloes
static void R_FloodFillSkin(byte *skin, int skinwidth, int skinheight)
{
	const byte fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t	fifo[FLOODFILL_FIFO_SIZE];
	int inpt = 0;
	int outpt = 0;
	int filledcolor = 0;

	// Attempt to find opaque black
	for (int i = 0; i < 256; ++i)
	{
		if (d_8to24table[i] == 255) // Alpha 1.0
		{
			filledcolor = i;
			break;
		}
	}

	// Can't fill to filled color or to transparent color (used as visited marker)
	if (fillcolor == filledcolor || fillcolor == 255)
		return;

	fifo[inpt].x = 0;
	fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		const int x = fifo[outpt].x;
		const int y = fifo[outpt].y;
		int fdc = filledcolor;
		byte *pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)
			FLOODFILL_STEP(-1, -1, 0);
		if (x < skinwidth - 1)
			FLOODFILL_STEP(1, 1, 0);

		if (y > 0)
			FLOODFILL_STEP(-skinwidth, 0, -1);
		if (y < skinheight - 1)
			FLOODFILL_STEP(skinwidth, 0, 1);

		skin[x + skinwidth * y] = fdc;
	}
}

#pragma endregion

// Scale up the pixel values in a texture to increase the lighting range
static void GL_LightScaleTexture(unsigned *in, int inwidth, int inheight)
{
	byte *p = (byte *)in;
	const int size = inwidth * inheight;
	
	for (int i = 0; i < size; i++, p += 4)
		for(int c = 0; c < 3; c++)
			p[c] = gammatable[p[c]];
}

/*
===============
here starts modified code by Heffo/changes by Nexus
===============
*/

static int upload_width; //** DMP made local to module
static int upload_height;

// Returns has_alpha
static qboolean GL_Upload32(uint *data, int width, int height, imagetype_t type, qboolean mipmap)
{
	uint *scaled;

	// Scan the texture for any non-255 alpha
	const int size = width * height;
	byte *scan = (byte *)data + 3;
	int format = GL_RGB;
	for (int i = 0; i < size; i++, scan += 4)
	{
		if (*scan != 255)
		{
			format = GL_RGBA;
			break;
		}
	}

	int scaled_width = min(glConfig.max_texsize, width);
	int scaled_height = min(glConfig.max_texsize, height);

	if (type == it_font)
	{
		//mxd. Upscale font textures to 1024x1024, so texture filtering causes less artifacts when using vanilla-sized font texture.
		const int maxsize = min(glConfig.max_texsize, 1024);

		while (true)
		{
			if (scaled_width >= maxsize && scaled_height >= maxsize)
				break;

			scaled_width <<= 1;
			scaled_height <<= 1;
		}
	}

	// Resample texture if needed
	if (scaled_width != width || scaled_height != height) 
	{
		scaled = malloc(scaled_width * scaled_height * 4);

		if (type == it_font) //mxd. Upscale fonts using nearest neighbour resizing.
			STBResizeNearest((byte *)data, width, height, (byte *)scaled, scaled_width, scaled_height);
		else
			STBResize((byte *)data, width, height, (byte *)scaled, scaled_width, scaled_height, true); //mxd
	}
	else
	{
		scaled = data;
	}

	if (!glState.gammaRamp)
		GL_LightScaleTexture(scaled, scaled_width, scaled_height);

	// Generate mipmaps and upload
	if (mipmap)
		qglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, true);
	else
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0); //mxd. Explicitly disable mipmaps. Fixes the first it_pic texture rendered all white...

	qglTexImage2D(GL_TEXTURE_2D, 0, format, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);

	if (scaled_width != width || scaled_height != height)
		free(scaled);

	upload_width = scaled_width;
	upload_height = scaled_height;

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (mipmap ? gl_filter_min : gl_filter_max));
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	// Set anisotropic filter if supported and enabled
	if (mipmap && glConfig.anisotropic && r_anisotropic->value)
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_anisotropic->value);

	return (format == GL_RGBA);
}

// Returns has_alpha
static qboolean GL_Upload8(const byte *data, int width, int height, imagetype_t type, qboolean mipmap)
{
	unsigned trans[512 * 256];

	const int size = width * height;
	if (size > sizeof(trans) / 4)
		VID_Error(ERR_DROP, "GL_Upload8: too large");

	for (int i = 0; i < size; i++)
	{
		int p = data[i];
		trans[i] = d_8to24table[p];

		if (p == 255)
		{
			// transparent, so scan around for another color to avoid alpha fringes
			// FIXME: do a full flood fill so mips work...
			if (i > width && data[i - width] != 255)
				p = data[i - width];
			else if (i < size - width && data[i + width] != 255)
				p = data[i + width];
			else if (i > 0 && data[i - 1] != 255)
				p = data[i - 1];
			else if (i < size - 1 && data[i + 1] != 255)
				p = data[i + 1];
			else
				p = 0;

			// Copy rgb components
			for(int c = 0; c < 3; c++)
				((byte *)&trans[i])[c] = ((byte *)&d_8to24table[p])[c];
		}
	}

	return GL_Upload32(trans, width, height, type, mipmap);
}

// This is also used as an entry point for the generated notexture
// Nexus - changes for hires-textures
image_t *R_LoadPic(char *name, byte *pic, int width, int height, imagetype_t type, int bits)
{
	image_t *image;
	int imagenum;
	
	// Find a free image_t
	for (imagenum = 0, image = gltextures; imagenum < numgltextures; imagenum++, image++)
		if (!image->texnum)
			break;

	if (imagenum == numgltextures)
	{
		if (numgltextures == MAX_GLTEXTURES)
			VID_Error(ERR_DROP, "%s: map has too many textures (max. is %i).", __func__, MAX_GLTEXTURES);

		numgltextures++;
	}

	const size_t len = strlen(name); //mxd
	if (len >= sizeof(image->name))
		VID_Error(ERR_DROP, "%s: image name \"%s\" is too long (%i / %i chars).", __func__, name, len, sizeof(image->name));

	Q_strncpyz(image->name, name, sizeof(image->name));
	image->registration_sequence = registration_sequence;

	//mxd. Store hash of name without extension
	const char *ext = COM_FileExtension(name);

	//mxd. Strip extension
	char namewe[MAX_QPATH];
	COM_StripExtension(name, namewe);

	image->hash = Com_HashFileName(namewe); // Knightmare added
	image->width = width;
	image->height = height;
	image->type = type;
	image->replace_scale_w = 1.0f; // Knightmare added
	image->replace_scale_h = 1.0f; 

	if (type == it_skin && bits == 8)
		R_FloodFillSkin(pic, width, height);

	// Replacement scaling hack for TGA/JPEG HUD images and skins

	// Check vanilla image size if we have a tga/png/jpg pic (name will still hold which all are 32-bit images)
	if ((type == it_pic || type == it_skin) && (!strcmp(ext, "tga") || !strcmp(ext, "png") || !strcmp(ext, "jpg")))
	{
		char tmp_name[256];
		strcpy(tmp_name, namewe);

		int pcxwidth, pcxheight;
		strcat(tmp_name, ".pcx");
		GetPCXInfo(tmp_name, &pcxwidth, &pcxheight);
		
		if (pcxwidth > 0 && pcxheight > 0)
		{
			image->replace_scale_w = (float)pcxwidth / image->width;
			image->replace_scale_h = (float)pcxheight / image->height;
		}
	}

	// Load little pics into the scrap
	if (image->type == it_pic && bits == 8 && image->width < 64 && image->height < 64)
	{
		int x, y;
		const int texnum = Scrap_AllocBlock(image->width, image->height, &x, &y);
		if (texnum == -1)
			goto nonscrap;

		scrap_dirty = true;

		// Copy the texels into the scrap block
		int k = 0;
		for (int i = 0; i < image->height; i++)
			for (int j = 0; j < image->width; j++, k++)
				scrap_texels[texnum][(y + i) * BLOCK_WIDTH + x + j] = pic[k];

		image->texnum = TEXNUM_SCRAPS + texnum;
		image->mipmap = false; //mxd
		image->has_alpha = true;
		image->sl = (x + 0.01f) / (float)BLOCK_WIDTH;
		image->sh = (x + image->width - 0.01f) / (float)BLOCK_WIDTH;
		image->tl = (y + 0.01f) / (float)BLOCK_WIDTH;
		image->th = (y + image->height - 0.01f) / (float)BLOCK_WIDTH;
	}
	else
	{
nonscrap:
		image->texnum = TEXNUM_IMAGES + (image - gltextures);
		GL_Bind(image->texnum);

		image->mipmap = (image->type != it_pic && image->type != it_sky && image->type != it_font); //mxd
		if (bits == 8)
			image->has_alpha = GL_Upload8(pic, width, height, image->type, image->mipmap);
		else
			image->has_alpha = GL_Upload32((uint *)pic, width, height, image->type, image->mipmap);

		image->upload_width = upload_width; // After power of 2 and scales
		image->upload_height = upload_height;
		image->sl = 0;
		image->sh = 1;
		image->tl = 0;
		image->th = 1;
	}

	return image;
}

// Store the names of last images that failed to load
#define NUM_FAIL_IMAGES 1024 //mxd. Was 256
static uint failedImageHashes[NUM_FAIL_IMAGES];
static uint failedImgListIndex;

void R_InitFailedImgList(void)
{
	memset(failedImageHashes, 0, sizeof(uint) * NUM_FAIL_IMAGES); //mxd
	failedImgListIndex = 0;
}

static qboolean R_CheckImgFailed(uint namehash) //mxd. Check hash instead of name
{
	for (int i = 0; i < NUM_FAIL_IMAGES; i++)
		if (namehash == failedImageHashes[i])
			return true; // We already tried to load this image, didn't find it

	return false;
}

static void R_AddToFailedImgList(char *name)
{
	if (!strncmp(name, "save/", 5)) // Don't add saveshots
		return;

	failedImageHashes[failedImgListIndex] = Com_HashFileName(name);
	failedImgListIndex++;

	// Wrap around to start of list
	if (failedImgListIndex >= NUM_FAIL_IMAGES)
		failedImgListIndex = 0;
}

// Finds or loads the given image
image_t *R_FindImage(char *name, imagetype_t type, qboolean silent)
{
	if (!name)
	{
		VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: empty filename\n", __func__); //mxd
		return NULL;
	}

	const int len = strlen(name);
	if (len < 5)
	{
		VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: invalid filename: '%s'\n", __func__, name); //mxd
		return NULL;
	}

	if (len >= MAX_QPATH)
	{
		VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: filename is too long: '%s' (%i / %i chars)\n", __func__, name, len, MAX_QPATH - 1); //mxd
		return NULL;
	}

	// Fix backslashes
	char *ptr;
	while ((ptr = strchr(name, '\\')))
		*ptr = '/';

	// Get extension (mxd. From YQ2)
	const char *ext = COM_FileExtension(name);

	//mxd. Strip extension
	char namewe[MAX_QPATH];
	COM_StripExtension(name, namewe);
	
	//mxd. Hash it
	const uint namehash = Com_HashFileName(namewe);

	// Look for it. mxd: Compare hashes instead of strings, use name without extension to avoid R_FindImage recursion.
	image_t *image = gltextures;
	for (int i = 0; i < numgltextures; i++, image++)
	{
		if(namehash == image->hash) //mxd
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	//mxd. All internal textures should be already loaded, all external ones must have an extension
	if(!*ext)
	{
		VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: failed to load internal texture '%s'\n", __func__, name); //mxd
		return NULL;
	}

	// Don't try again to load an image that just failed
	if (R_CheckImgFailed(namehash))
		return NULL; //mxd. No texture match. Period.

	// Load the pic from disk
	char tmp_name[MAX_QPATH];
	byte *pic = NULL;
	int width, height;

	//mxd. Try to load a tga, png or jpg image first (in that order/priority)
	if (STBLoad(namewe, "tga", &pic, &width, &height))
	{
		Com_sprintf(tmp_name, MAX_QPATH, "%s.tga", namewe);
		image = R_LoadPic(tmp_name, pic, width, height, type, 32);
	}
	else if (STBLoad(namewe, "png", &pic, &width, &height))
	{
		Com_sprintf(tmp_name, MAX_QPATH, "%s.png", namewe);
		image = R_LoadPic(tmp_name, pic, width, height, type, 32);
	}
	else if (STBLoad(namewe, "jpg", &pic, &width, &height))
	{
		Com_sprintf(tmp_name, MAX_QPATH, "%s.jpg", namewe);
		image = R_LoadPic(tmp_name, pic, width, height, type, 32);
	}
	else if (!strcmp(ext, "pcx"))
	{
		LoadPCX(name, &pic, NULL, &width, &height);
		if (pic)
			image = R_LoadPic(name, pic, width, height, type, 8);
		else
			image = NULL;
	}
	else if (!strcmp(ext, "wal"))
	{
		image = R_LoadWal(name, type);
	}
	else
	{
		image = NULL;
	}

	//mxd. Free memory
	free(pic);

	if (!image)
	{
		//mxd. We can only get here once per unique image name now, right?
		if(!silent)
			VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: can't find '%s'\n", __func__, name);

		R_AddToFailedImgList(namewe);
	}

	return image;
}

struct image_s *R_RegisterSkin(char *name)
{
	return R_FindImage(name, it_skin, false);
}

// Any image that was not touched on this registration sequence will be freed.
void R_FreeUnusedImages(void)
{
	// Never free notexture or particle textures
	glMedia.notexture->registration_sequence = registration_sequence;
	glMedia.whitetexture->registration_sequence = registration_sequence;
	glMedia.rawtexture->registration_sequence = registration_sequence; // ROQ support
	glMedia.envmappic->registration_sequence = registration_sequence;
	glMedia.spheremappic->registration_sequence = registration_sequence;
	glMedia.causticwaterpic->registration_sequence = registration_sequence;
	glMedia.causticslimepic->registration_sequence = registration_sequence;
	glMedia.causticlavapic->registration_sequence = registration_sequence;
	glMedia.shelltexture->registration_sequence = registration_sequence;
	glMedia.particlebeam->registration_sequence = registration_sequence;

	for (int i = 0; i < PARTICLE_TYPES; i++)
		if (glMedia.particletextures[i]) // Don't mess with null ones silly :p
			glMedia.particletextures[i]->registration_sequence = registration_sequence;

	image_t *image = gltextures;
	for (int i = 0; i < numgltextures; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
			continue; // Used this sequence

		if (!image->registration_sequence)
			continue; // Free image_t slot

		if (image->type == it_pic || image->type == it_font) //mxd. +it_font
			continue; // Don't free pics or fonts

		// Free it
		qglDeleteTextures(1, (const GLuint*)&image->texnum);
		memset(image, 0, sizeof(*image));
	}
}

void Draw_GetPalette()
{
	byte *pic, *pal;
	int width, height;

	// Get the palette
	LoadPCX("pics/colormap.pcx", &pic, &pal, &width, &height);
	if (!pal)
		VID_Error(ERR_FATAL, "Couldn't load pics/colormap.pcx");

	for (int i = 0; i < 256; i++)
	{
		const int r = pal[i * 3 + 0];
		const int g = pal[i * 3 + 1];
		const int b = pal[i * 3 + 2];
		
		d_8to24table[i] = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
	}

	d_8to24table[255] &= 0xffffff; // Color 255 is transparent

	free(pic);
	free(pal);
}

void R_InitImages(void)
{
	registration_sequence = 1;

	R_InitFailedImgList(); // Knightmare added
	Draw_GetPalette();

	const float g = vid_gamma->value;

	for (int i = 0; i < 256; i++)
	{
		if (g == 1)
		{
			gammatable[i] = i;
		}
		else
		{
			const float inf = 255 * powf((i + 0.5f) / 255.5f, g) + 0.5f;
			gammatable[i] = clamp(inf, 0, 255);
		}
	}

	R_InitBloomTextures(); // BLOOMS
}

// Frees a single pic. By Knightmare
void R_FreePic(char *name)
{
	//mxd. Compare hash instead of name
	char namewe[MAX_QPATH];
	COM_StripExtension(name, namewe);
	const uint hash = Com_HashFileName(namewe);

	image_t *image = gltextures;
	for (int i = 0; i < numgltextures; i++, image++)
	{
		if (!image->registration_sequence)
			continue; // Free image_t slot

		if (image->type != it_pic)
			continue; // Only free pics

		if(hash == image->hash) //mxd
		{
			// Free it
			qglDeleteTextures(1, (const GLuint*)&image->texnum);
			memset(image, 0, sizeof(*image));

			return; // We're done here
		}
	}
}

void R_ShutdownImages(void)
{
	image_t *image = gltextures;
	for (int i = 0; i < numgltextures; i++, image++)
	{
		if (!image->registration_sequence)
			continue; // Free image_t slot

		// Free it
		qglDeleteTextures(1, (const GLuint*)&image->texnum);
		memset(image, 0, sizeof(*image));
	}
}

//mxd
void R_LoadNormalmap(const char *texture, mtexinfo_t *tex)
{
	// Try to load image...
	char name[MAX_QPATH];
	byte *pic = NULL; // Stores RGBA image data
	int width, height;

	Com_sprintf(name, sizeof(name), "textures/%s_normal", texture);

	if (   !STBLoad(name, "tga", &pic, &width, &height)
		&& !STBLoad(name, "png", &pic, &width, &height)
		&& !STBLoad(name, "jpg", &pic, &width, &height))
	{
		return; // No dice...
	}

	// Texture and normalmap dimensions must match...
	if (tex->image->width == width && tex->image->height == height)
	{
		// Load nmap vectors...
		const int numvectors = width * height;
		tex->nmapvectors = malloc(sizeof(float) * 3 * numvectors);

		int n = 0;
		for (int i = 0; i < numvectors; i++)
		{
			for (int c = 0; c < 3; c++)
				tex->nmapvectors[i * 3 + c] = roundf((pic[n++] * 0.0078125f - 1.0f) * 10) / 10; // Convert from [0..255] to [-1..1] range. 0.0078125f == 1 / 128

			n++; // Skip alpha value...
		}
	}
	else
	{
		VID_Printf(PRINT_ALL, S_COLOR_YELLOW"R_LoadNormalmap: '%s' normalmap dimensions (%i x %i) don't match '%s' texture dimensions (%i x %i)\n", name, texture, width, height, tex->image->width, tex->image->height);
	}

	// Free the resource...
	free(pic);
}