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
//#include "r_cin.h"
#include "../include/jpeg/jpeglib.h"
#ifdef PNG_SUPPORT
#include "../include/zlibpng/png.h"
#endif	// PNG_SUPPORT

image_t		gltextures[MAX_GLTEXTURES];
int			numgltextures;
int			base_textureid;		// gltextures[i] = base_textureid+i

static byte			 intensitytable[256];
static unsigned char gammatable[256];

cvar_t		*r_intensity;

unsigned	d_8to24table[256];
float		d_8to24tablef[256][3]; //Knightmare- MrG's Vertex array stuff

qboolean GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, qboolean is_sky);
qboolean GL_Upload32 (unsigned *data, int width, int height,  qboolean mipmap);

#define GL_SOLID_FORMAT 3 //mxd
#define GL_ALPHA_FORMAT 4 //mxd

int		gl_tex_solid_format = 3;
int		gl_tex_alpha_format = 4;
int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;

void GL_SetTexturePalette(unsigned palette[256])
{
	unsigned char temptable[768];

	if (qglColorTableEXT)
	{
		for (int i = 0; i < 256; i++)
		{
			temptable[i * 3 + 0] = (palette[i] >> 0) & 0xff;
			temptable[i * 3 + 1] = (palette[i] >> 8) & 0xff;
			temptable[i * 3 + 2] = (palette[i] >> 16) & 0xff;
		}

		qglColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT,
						 GL_RGB,
						 256,
						 GL_RGB,
						 GL_UNSIGNED_BYTE,
						 temptable);
	}
}


typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] =
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

gltmode_t gl_alpha_modes[] =
{
	{ "default", 4 },
	{ "GL_RGBA", GL_RGBA },
	{ "GL_RGBA8", GL_RGBA8 },
	{ "GL_RGB5_A1", GL_RGB5_A1 },
	{ "GL_RGBA4", GL_RGBA4 },
	{ "GL_RGBA2", GL_RGBA2 },
};

#define NUM_GL_ALPHA_MODES (sizeof(gl_alpha_modes) / sizeof (gltmode_t))

gltmode_t gl_solid_modes[] =
{
	{ "default", 3 },
	{ "GL_RGB", GL_RGB },
	{ "GL_RGB8", GL_RGB8 },
	{ "GL_RGB5", GL_RGB5 },
	{ "GL_RGB4", GL_RGB4 },
	{ "GL_R3_G3_B2", GL_R3_G3_B2 },
#ifdef GL_RGB2_EXT
	{ "GL_RGB2", GL_RGB2_EXT },
#endif
};

#define NUM_GL_SOLID_MODES (sizeof(gl_solid_modes) / sizeof (gltmode_t))

/*
===============
GL_ApplyTextureMode (mxd)
===============
*/
void GL_ApplyTextureMode(int texnum, int filter_min, int filter_mag, float anisotropy)
{
	GL_Bind(texnum);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_min);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_mag);

	// Set anisotropic filter if supported and enabled
	if (glConfig.anisotropic && anisotropy)
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
}

/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode(char *string)
{
	unsigned mode;

	for (mode = 0; mode < NUM_GL_MODES; mode++)
	{
		if (!Q_stricmp(modes[mode].name, string))
			break;
	}

	if (mode == NUM_GL_MODES)
	{
		VID_Printf(PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[mode].minimize;
	gl_filter_max = modes[mode].maximize;

	// clamp selected anisotropy
	if (glConfig.anisotropic)
	{
		if (r_anisotropic->value > glConfig.max_anisotropy)
			Cvar_SetValue("r_anisotropic", glConfig.max_anisotropy);
		else if (r_anisotropic->value < 1.0)
			Cvar_SetValue("r_anisotropic", 1.0);
	}

	// change all the existing mipmap texture objects
	image_t *glt = gltextures;
	const int filter = (!strncmp(r_texturemode->string, "GL_NEAREST", 10) ? GL_NEAREST : GL_LINEAR); //mxd

	for (int i = 0; i < numgltextures; i++, glt++)
	{
		if (glt->texnum < 1) //mxd
			continue;

		//mxd. Also sky
		if(glt->type == it_sky)
		{
			for (int c = 0; c < 6; c++)
				GL_ApplyTextureMode(sky_images[c]->texnum, filter, filter, r_anisotropic->value);
		}
		else if (glt->type != it_pic)
		{
			GL_ApplyTextureMode(glt->texnum, gl_filter_min, gl_filter_max, r_anisotropic->value);
		}
	}

	//mxd. Change lightmap filtering when _lightmap_scale is 1. The idea is to make them look like they are part of the texture
	if(gl_lms.lmshift == 0)
	{
		for (int i = 1; i < gl_lms.current_lightmap_texture; i++)
			GL_ApplyTextureMode(glState.lightmap_textures + i, filter, filter, r_anisotropic->value);
	}
}

/*
===============
GL_TextureAlphaMode
===============
*/
void GL_TextureAlphaMode( char *string )
{
	unsigned mode;

	for (mode = 0; mode < NUM_GL_ALPHA_MODES; mode++)
	{
		if (!Q_stricmp(gl_alpha_modes[mode].name, string))
			break;
	}

	if (mode == NUM_GL_ALPHA_MODES)
	{
		VID_Printf(PRINT_ALL, "bad alpha texture mode name\n");
		return;
	}

	gl_tex_alpha_format = gl_alpha_modes[mode].mode;
}

/*
===============
GL_TextureSolidMode
===============
*/
void GL_TextureSolidMode( char *string )
{
	unsigned mode;

	for (mode = 0; mode < NUM_GL_SOLID_MODES; mode++)
	{
		if (!Q_stricmp(gl_solid_modes[mode].name, string))
			break;
	}

	if (mode == NUM_GL_SOLID_MODES)
	{
		VID_Printf(PRINT_ALL, "bad solid texture mode name\n");
		return;
	}

	gl_tex_solid_format = gl_solid_modes[mode].mode;
}

/*
===============
R_ImageList_f
===============
*/
void R_ImageList_f (void)
{
	int		i;
	image_t	*image;

	const char *palstrings[2] = { "RGB", "PAL" };

	VID_Printf(PRINT_ALL, "------------------\n");
	int texels = 0;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (image->texnum <= 0)
			continue;

		texels += image->upload_width * image->upload_height;

		switch (image->type)
		{
		case it_skin:
			VID_Printf(PRINT_ALL, "M");
			break;

		case it_sprite:
			VID_Printf(PRINT_ALL, "S");
			break;

		case it_wall:
			VID_Printf(PRINT_ALL, "W");
			break;

		case it_pic:
		case it_part:
			VID_Printf(PRINT_ALL, "P");
			break;

		default:
			VID_Printf(PRINT_ALL, " ");
			break;
		}

		VID_Printf(PRINT_ALL,  " %3i %3i %s: %s\n",
			image->upload_width, image->upload_height, palstrings[image->paletted], image->name);
	}

	VID_Printf(PRINT_ALL, "Total texel count (not counting mipmaps): %i\n", texels);
}


/*
=============================================================================

  scrap allocation

  Allocate all the little status bar objects into a single texture to crutch up inefficient hardware / drivers

=============================================================================
*/

#define	MAX_SCRAPS		1
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH * BLOCK_HEIGHT];
qboolean	scrap_dirty;

// returns a texture number and the position inside it
int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int j;

	for (int texnum = 0; texnum < MAX_SCRAPS; texnum++)
	{
		int best = BLOCK_HEIGHT;

		for (int i = 0; i < BLOCK_WIDTH - w; i++)
		{
			int best2 = 0;

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

int	scrap_uploads;

void Scrap_Upload (void)
{
	scrap_uploads++;
	GL_Bind(TEXNUM_SCRAPS);
	GL_Upload8(scrap_texels[0], BLOCK_WIDTH, BLOCK_HEIGHT, false, false);
	scrap_dirty = false;
}

/*
=================================================================

PCX LOADING

=================================================================
*/


/*
==============
LoadPCX
==============
*/
void LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height)
{
	*pic = NULL;
	*palette = NULL;

	//
	// load the file
	//
	byte *raw;
	const int len = FS_LoadFile(filename, (void **)&raw);
	if (!raw)
	{
		VID_Printf(PRINT_DEVELOPER, "Bad pcx file %s\n", filename);
		return;
	}

	//
	// parse the PCX file
	//
	pcx_t *pcx = (pcx_t *)raw;

    pcx->xmin = LittleShort(pcx->xmin);
    pcx->ymin = LittleShort(pcx->ymin);
    pcx->xmax = LittleShort(pcx->xmax);
    pcx->ymax = LittleShort(pcx->ymax);
    pcx->hres = LittleShort(pcx->hres);
    pcx->vres = LittleShort(pcx->vres);
    pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
    pcx->palette_type = LittleShort(pcx->palette_type);

	raw = &pcx->data;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 640
		|| pcx->ymax >= 480)
	{
		VID_Printf (PRINT_ALL, "Bad pcx file %s\n", filename);
		return;
	}

	byte *out = malloc((pcx->ymax + 1) * (pcx->xmax + 1));
	*pic = out;
	byte *pix = out;

	if (palette)
	{
		*palette = malloc(768);
		memcpy(*palette, (byte *)pcx + len - 768, 768);
	}

	if (width)
		*width = pcx->xmax + 1;
	if (height)
		*height = pcx->ymax + 1;

	for (int y = 0; y <= pcx->ymax; y++, pix += pcx->xmax + 1)
	{
		for (int x = 0; x <= pcx->xmax;)
		{
			int dataByte = *raw++;
			int runLength;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
			{
				runLength = 1;
			}

			while(runLength-- > 0)
				pix[x++] = dataByte;
		}
	}

	if (raw - (byte *)pcx > len)
	{
		VID_Printf(PRINT_DEVELOPER, "PCX file %s was malformed", filename);
		free(*pic);
		*pic = NULL;
	}

	FS_FreeFile(pcx);
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

// Definitions for image types
#define TGA_Null		0   // no image data
#define TGA_Map			1   // Uncompressed, color-mapped images
#define TGA_RGB			2   // Uncompressed, RGB images
#define TGA_Mono		3   // Uncompressed, black and white images
#define TGA_RLEMap		9   // Runlength encoded color-mapped images
#define TGA_RLERGB		10   // Runlength encoded RGB images
#define TGA_RLEMono		11   // Compressed, black and white images
#define TGA_CompMap		32   // Compressed color-mapped data, using Huffman, Delta, and runlength encoding
#define TGA_CompMap4	33   // Compressed color-mapped data, using Huffman, Delta, and runlength encoding. 4-pass quadtree-type process
// Definitions for interleave flag
#define TGA_IL_None		0   // non-interleaved
#define TGA_IL_Two		1   // two-way (even/odd) interleaving
#define TGA_IL_Four		2   // four way interleaving
#define TGA_IL_Reserved	3   // reserved
// Definitions for origin flag
#define TGA_O_UPPER		0   // Origin in lower left-hand corner
#define TGA_O_LOWER		1   // Origin in upper left-hand corner
#define MAXCOLORS		16384

typedef struct _TargaHeader
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/*
=============
R_LoadTGA
NiceAss: LoadTGA() from Q2Ice, it supports more formats
=============
*/
void R_LoadTGA(char *filename, byte **pic, int *width, int *height)
{
	int i;
	int map_idx;
	TargaHeader header;
	byte tmp[2], r, g, b, a, j, k, l;

	// load file
	byte *data;
	FS_LoadFile(filename, &data);

	if (!data)
		return;

	byte *pdata = data;

	header.id_length = *pdata++;
	header.colormap_type = *pdata++;
	header.image_type = *pdata++;

	tmp[0] = pdata[0];
	tmp[1] = pdata[1];
	header.colormap_index = LittleShort(*((short *)tmp));
	pdata += 2;
	tmp[0] = pdata[0];
	tmp[1] = pdata[1];
	header.colormap_length = LittleShort(*((short *)tmp));
	pdata += 2;
	header.colormap_size = *pdata++;
	header.x_origin = LittleShort(*((short *)pdata));
	pdata += 2;
	header.y_origin = LittleShort(*((short *)pdata));
	pdata += 2;
	header.width = LittleShort(*((short *)pdata));
	pdata += 2;
	header.height = LittleShort(*((short *)pdata));
	pdata += 2;
	header.pixel_size = *pdata++;
	header.attributes = *pdata++;

	if (header.id_length)
		pdata += header.id_length;

	// validate TGA type
	switch (header.image_type)
	{
	case TGA_Map:
	case TGA_RGB:
	case TGA_Mono:
	case TGA_RLEMap:
	case TGA_RLERGB:
	case TGA_RLEMono:
		break;

	default:
		VID_Error(ERR_DROP, "R_LoadTGA: Only type 1 (map), 2 (RGB), 3 (mono), 9 (RLEmap), 10 (RLERGB), 11 (RLEmono) TGA images supported\n");
		return;
	}

	// validate color depth
	switch (header.pixel_size)
	{
	case 8:
	case 15:
	case 16:
	case 24:
	case 32:
		break;

	default:
		VID_Error(ERR_DROP, "R_LoadTGA: Only 8, 15, 16, 24 and 32 bit images (with colormaps) supported\n");
		return;
	}

	r = g = b = a = l = 0;

	// if required, read the color map information
	byte *ColorMap = NULL;
	const int mapped = header.colormap_type == 1 && (header.image_type == TGA_Map || header.image_type == TGA_RLEMap || header.image_type == TGA_CompMap || header.image_type == TGA_CompMap4);
	if (mapped)
	{
		// validate colormap size
		switch (header.colormap_size)
		{
		case 8:
		case 16:
		case 32:
		case 24:
			break;

		default:
			VID_Error(ERR_DROP, "R_LoadTGA: Only 8, 16, 24 and 32 bit colormaps supported\n");
			return;
		}

		const int temp1 = header.colormap_index;
		const int temp2 = header.colormap_length;
		if (temp1 + temp2 + 1 >= MAXCOLORS)
		{
			FS_FreeFile(data);
			return;
		}

		ColorMap = (byte *)malloc(MAXCOLORS * 4);
		map_idx = 0;
		for (i = temp1; i < temp1 + temp2; ++i, map_idx += 4)
		{
			// read appropriate number of bytes, break into rgb & put in map
			switch (header.colormap_size)
			{
			case 8:
				r = g = b = *pdata++;
				a = 255;
				break;

			case 15:
				j = *pdata++;
				k = *pdata++;
				l = ((unsigned int)k << 8) + j;
				r = (byte)(((k & 0x7C) >> 2) << 3);
				g = (byte)((((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3);
				b = (byte)((j & 0x1F) << 3);
				a = 255;
				break;

			case 16:
				j = *pdata++;
				k = *pdata++;
				l = ((unsigned int)k << 8) + j;
				r = (byte)(((k & 0x7C) >> 2) << 3);
				g = (byte)((((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3);
				b = (byte)((j & 0x1F) << 3);
				a = (k & 0x80) ? 255 : 0;
				break;

			case 24:
				b = *pdata++;
				g = *pdata++;
				r = *pdata++;
				a = 255;
				l = 0;
				break;

			case 32:
				b = *pdata++;
				g = *pdata++;
				r = *pdata++;
				a = *pdata++;
				l = 0;
				break;

			}

			ColorMap[map_idx + 0] = r;
			ColorMap[map_idx + 1] = g;
			ColorMap[map_idx + 2] = b;
			ColorMap[map_idx + 3] = a;
		}
	}

	// check run-length encoding
	const int rlencoded = header.image_type == TGA_RLEMap || header.image_type == TGA_RLERGB || header.image_type == TGA_RLEMono;
	int RLE_count = 0;
	int RLE_flag = 0;

	const int w = header.width;
	const int h = header.height;

	if (width)
		*width = w;
	if (height)
		*height = h;

	const int size = w * h * 4;
	*pic = (byte *)malloc(size);

	memset(*pic, 0, size);

	// read the Targa file body and convert to portable format
	const int pixel_size = header.pixel_size;
	const int origin =     (header.attributes & 0x20) >> 5;
	const int interleave = (header.attributes & 0xC0) >> 6;
	int truerow = 0;
	int baserow = 0;

	for (int y = 0; y < h; y++)
	{
		int realrow = truerow;
		if (origin == TGA_O_UPPER)
			realrow = h - realrow - 1;

		byte *dst = *pic + realrow * w * 4;

		for (int x = 0; x < w; x++)
		{
			// check if run length encoded
			if (rlencoded)
			{
				if (!RLE_count)
				{
					// have to restart run
					i = *pdata++;
					RLE_flag = (i & 0x80);
					
					if (!RLE_flag)
						RLE_count = i + 1; // stream of unencoded pixels
					else
						RLE_count = i - 127; // single pixel replicated

					// decrement count & get pixel
					--RLE_count;
				}
				else
				{
					// have already read count & (at least) first pixel
					--RLE_count;
					if (RLE_flag)
						goto PixEncode; // replicated pixels
				}
			}

			// read appropriate number of bytes, break into RGB
			switch (pixel_size)
			{
			case 8:
				r = g = b = l = *pdata++;
				a = 255;
				break;

			case 15:
				j = *pdata++;
				k = *pdata++;
				l = ((unsigned int)k << 8) + j;
				r = (byte)(((k & 0x7C) >> 2) << 3);
				g = (byte)((((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3);
				b = (byte)((j & 0x1F) << 3);
				a = 255;
				break;

			case 16:
				j = *pdata++;
				k = *pdata++;
				l = ((unsigned int)k << 8) + j;
				r = (byte)(((k & 0x7C) >> 2) << 3);
				g = (byte)((((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3);
				b = (byte)((j & 0x1F) << 3);
				a = 255;
				break;

			case 24:
				b = *pdata++;
				g = *pdata++;
				r = *pdata++;
				a = 255;
				l = 0;
				break;

			case 32:
				b = *pdata++;
				g = *pdata++;
				r = *pdata++;
				a = *pdata++;
				l = 0;
				break;

			default:
				VID_Error(ERR_DROP, "Illegal pixel_size '%d' in file '%s'\n", filename);
				return;
			}

		PixEncode:
			if (mapped)
			{
				map_idx = l * 4;
				*dst++ = ColorMap[map_idx + 0];
				*dst++ = ColorMap[map_idx + 1];
				*dst++ = ColorMap[map_idx + 2];
				*dst++ = ColorMap[map_idx + 3];
			}
			else
			{
				*dst++ = r;
				*dst++ = g;
				*dst++ = b;
				*dst++ = a;
			}
		}

		if (interleave == TGA_IL_Four)
			truerow += 4;
		else if (interleave == TGA_IL_Two)
			truerow += 2;
		else
			truerow++;

		if (truerow >= h)
			truerow = ++baserow;
	}

	if (mapped)
		free(ColorMap);

	FS_FreeFile(data);
}


/*
=================================================================

PNG LOADING

From Quake2Max

=================================================================
*/
#ifdef PNG_SUPPORT

typedef struct png_handle_s 
{
	char	*tmpBuf;
	int		tmpi;
	long	fBgColor;		// DL Background color Added 30/05/2000
	int		fTransparent;	// DL Is this Image Transparent?   Added 30/05/2000
	long	fRowBytes;		// DL Added 30/05/2000
	double	fGamma;			// DL Added 07/06/2000
	double	fScreenGamma;	// DL Added 07/06/2000
	char	*fRowPtrs;		// DL Changed for consistancy 30/05/2000  
	char	*data;			// property data: pByte read fData;
	char	*title;
	char	*author;
	char	*description;
	int		bitDepth;
	int		bytesPerPixel;
	int		colorType;
	int		height;
	int		width;
	int		interlace;
	int		compression;
	int		filter;
	double	lastModified;
	int		transparent;
} png_handle_t;

png_handle_t *r_png_handle = 0;

void R_InitializePNGData (void) 
{
	// Initialize Data and RowPtrs
	if (r_png_handle->data) 
	{
		free(r_png_handle->data);
		r_png_handle->data = 0;
	}

	if (r_png_handle->fRowPtrs) 
	{
		free(r_png_handle->fRowPtrs);
		r_png_handle->fRowPtrs = 0;
	}

	r_png_handle->data = malloc(r_png_handle->height * r_png_handle->fRowBytes ); // DL Added 30/5/2000
	r_png_handle->fRowPtrs = malloc(sizeof(void*) * r_png_handle->height);

	if (r_png_handle->data && r_png_handle->fRowPtrs) 
	{
		long * cvaluep = (long*)r_png_handle->fRowPtrs;    
		for (long y = 0; y < r_png_handle->height; y++)
			cvaluep[y] = (long)r_png_handle->data + (y * (long)r_png_handle->fRowBytes); //DL Added 08/07/2000      
	}
}

void R_CreatePNG (void) 
{
	if (r_png_handle)
		return;

	r_png_handle = malloc(sizeof(png_handle_t));
	r_png_handle->data = 0;
	r_png_handle->fRowPtrs = 0;
	r_png_handle->height = 0;
	r_png_handle->width = 0;
	r_png_handle->colorType = PNG_COLOR_TYPE_RGBA;	// was PNG_COLOR_TYPE_RGB
	r_png_handle->interlace = PNG_INTERLACE_NONE;
	r_png_handle->compression = PNG_COMPRESSION_TYPE_DEFAULT;
	r_png_handle->filter = PNG_FILTER_TYPE_DEFAULT;
}

void R_DestroyPNG (qboolean keepData) 
{  
	if (!r_png_handle) 
		return;

	if (r_png_handle->data && !keepData) 
		free(r_png_handle->data);

	if (r_png_handle->fRowPtrs) 
		free(r_png_handle->fRowPtrs);

	free(r_png_handle);
	r_png_handle = NULL;
}

void PNGAPI R_ReadPNGData (png_structp png, png_bytep data, png_size_t length) 
{
	// called by pnglib
	for (unsigned i = 0; i < length; i++) 
		data[i] = r_png_handle->tmpBuf[r_png_handle->tmpi++]; // give pnglib some more bytes  
}


/*
==============
R_LoadPNG
==============
*/
void R_LoadPNG (char *filename, byte **pic, int *width, int *height) 
{
	byte ioBuffer[8192];
	
	*pic = NULL;

	byte *raw;
	FS_LoadFile(filename, (void **)&raw);

	if (!raw)
	{
		// Knightmare- skip this unless developer >= 2 because it spams the console
		if (developer->value > 1)
			VID_Printf (PRINT_DEVELOPER, "Bad png file %s\n", filename);

		return;
	}

	if (png_sig_cmp(raw, 0, 4)) 
		return;  

	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (!png) 
		return;

	png_infop pnginfo = png_create_info_struct(png);

	if (!pnginfo)
	{
		png_destroy_read_struct(&png, &pnginfo, 0);
		return;
	}

	R_CreatePNG(); // creates the r_png_handle struct

	r_png_handle->tmpBuf = raw; //buf = whole file content
	r_png_handle->tmpi = 0; 
	png_set_read_fn(png, ioBuffer, R_ReadPNGData);
	png_read_info(png, pnginfo);

	png_get_IHDR(png, pnginfo, &r_png_handle->width, &r_png_handle->height, &r_png_handle->bitDepth,
				&r_png_handle->colorType, &r_png_handle->interlace, &r_png_handle->compression, &r_png_handle->filter);
	// ...removed bgColor code here...

	if (r_png_handle->colorType == PNG_COLOR_TYPE_PALETTE)  
		png_set_palette_to_rgb(png);

	if (r_png_handle->colorType == PNG_COLOR_TYPE_GRAY && r_png_handle->bitDepth < 8) 
		png_set_gray_1_2_4_to_8(png);

	// Add alpha channel if present
	if (png_get_valid(png, pnginfo, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);

	// Expand grayscale to RGB
	if (r_png_handle->colorType == PNG_COLOR_TYPE_GRAY || r_png_handle->colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);

	if (r_png_handle->bitDepth < 8)
		png_set_expand(png);

	// hax: expand 24bit to 32bit
	if (r_png_handle->bitDepth == 8 && r_png_handle->colorType == PNG_COLOR_TYPE_RGB) 
		png_set_filler(png, 255, PNG_FILLER_AFTER);

	// update the info structure
	png_read_update_info(png, pnginfo);

	r_png_handle->fRowBytes = png_get_rowbytes(png, pnginfo);
	r_png_handle->bytesPerPixel = png_get_channels(png, pnginfo);  // DL Added 30/08/2000

	R_InitializePNGData();
	if (r_png_handle->data && r_png_handle->fRowPtrs)
		png_read_image(png, (png_bytepp)r_png_handle->fRowPtrs);

	png_read_end(png, pnginfo); // read last information chunks

	png_destroy_read_struct(&png, &pnginfo, 0);

	// only load 32 bit by now...
	if (r_png_handle->bitDepth == 8)
	{
		*pic = r_png_handle->data;
		*width = r_png_handle->width;
		*height = r_png_handle->height;
	}
	else
	{
		VID_Printf(PRINT_DEVELOPER, "Bad png color depth: %s\n", filename);
		*pic = NULL;
		free(r_png_handle->data);
	}

	R_DestroyPNG(true);
	FS_FreeFile((void *)raw);
}
#endif	// PNG_SUPPORT

/*
=================================================================

JPEG LOADING

By Robert 'Heffo' Heffernan

=================================================================
*/

void jpg_null(j_decompress_ptr cinfo)
{
}

unsigned char jpg_fill_input_buffer(j_decompress_ptr cinfo)
{
    VID_Printf(PRINT_ALL, "Premature end of JPEG data\n");
    return 1;
}

void jpg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	if(cinfo->src->bytes_in_buffer >= (size_t) num_bytes) // //mxd. bytes_in_buffer can't be < 0
	{
		cinfo->src->next_input_byte += (size_t)num_bytes;
		cinfo->src->bytes_in_buffer -= (size_t)num_bytes;
	}
	else
	{
		VID_Printf(PRINT_ALL, "Premature end of JPEG data\n");
	}
}

void jpeg_mem_src(j_decompress_ptr cinfo, const unsigned char *mem, unsigned long len) //mxd. Was byte *mem, int len. Fixes C4028: formal parameters 2 and 3 different from declaration
{
    cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(struct jpeg_source_mgr));
    cinfo->src->init_source = jpg_null;
    cinfo->src->fill_input_buffer = jpg_fill_input_buffer;
    cinfo->src->skip_input_data = jpg_skip_input_data;
    cinfo->src->resync_to_restart = jpeg_resync_to_restart;
    cinfo->src->term_source = jpg_null;
    cinfo->src->bytes_in_buffer = len;
    cinfo->src->next_input_byte = mem;
}

#define DSTATE_START	200	/* after create_decompress */
#define DSTATE_INHEADER	201	/* reading header markers, no SOS yet */

//mxd. JPEG Error handling...
struct custom_jpeg_error_mgr
{
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};

typedef struct custom_jpeg_error_mgr *custom_jpeg_error_ptr;

static void custom_jpeg_error_exit(j_common_ptr cinfo)
{
	// cinfo->err really points to custom_jpeg_error_mgr struct, so coerce pointer
	custom_jpeg_error_ptr err = (custom_jpeg_error_ptr)cinfo->err;

	// Return control to the setjmp point
	longjmp(err->setjmp_buffer, 1);
}

/*
==============
R_LoadJPG
==============
*/
void R_LoadJPG (char *filename, byte **pic, int *width, int *height)
{
	struct jpeg_decompress_struct	cinfo;
	struct custom_jpeg_error_mgr	jerr; //mxd
	byte							*rawdata;

	// Load JPEG file into memory
	const int rawsize = FS_LoadFile(filename, (void **)&rawdata);
	if (!rawdata)
	{
		VID_Printf(PRINT_DEVELOPER, "Bad jpg file %s\n", filename);
		return;	
	}

	// Knightmare- check for bad data
	if (	rawdata[6] != 'J'
		||	rawdata[7] != 'F'
		||	rawdata[8] != 'I'
		||	rawdata[9] != 'F')
	{
		VID_Printf(PRINT_ALL, "Bad jpg file %s\n", filename);
		FS_FreeFile(rawdata);
		return;
	}

	// Initialise libJpeg Object
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = custom_jpeg_error_exit; //mxd
	if (setjmp(jerr.setjmp_buffer)) //mxd
	{
		// Display the message
		char buffer[JMSG_LENGTH_MAX];
		(*cinfo.err->format_message) ((j_common_ptr)&cinfo, buffer);
		VID_Printf(PRINT_ALL, "Failed to load jpg file %s. %s\n", filename, buffer);

		// Get rid of data
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}

	jpeg_create_decompress(&cinfo);

	// Feed JPEG memory into the libJpeg Object
	jpeg_mem_src(&cinfo, rawdata, rawsize);

	// Process JPEG header
	jpeg_read_header(&cinfo, true); // bombs out here

	// Start Decompression
	jpeg_start_decompress(&cinfo);

	// Check Color Components
	if(cinfo.output_components != 3)
	{
		VID_Printf(PRINT_ALL, "Failed to load jpg file %s: invalid JPEG color components\n", filename);
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}

	// Allocate Memory for decompressed image
	byte *rgbadata = malloc(cinfo.output_width * cinfo.output_height * 4);
	if(!rgbadata)
	{
		VID_Printf(PRINT_ALL, "Insufficient RAM for JPEG buffer\n");
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}

	// Pass sizes to output
	*width = cinfo.output_width; *height = cinfo.output_height;

	// Allocate Scanline buffer
	byte *scanline = malloc(cinfo.output_width * 3);
	if(!scanline)
	{
		VID_Printf(PRINT_ALL, "Insufficient RAM for JPEG scanline buffer\n");
		free(rgbadata);
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}

	// Read Scanlines, and expand from RGB to RGBA
	byte *q = rgbadata;
	while(cinfo.output_scanline < cinfo.output_height)
	{
		byte *p = scanline;
		jpeg_read_scanlines(&cinfo, &scanline, 1);

		for(unsigned i = 0; i < cinfo.output_width; i++)
		{
			q[0] = p[0];
			q[1] = p[1];
			q[2] = p[2];
			q[3] = 255;

			p += 3;
			q += 4;
		}
	}

	// Free the scanline buffer
	free(scanline);

	// Finish Decompression
	jpeg_finish_decompress(&cinfo);

	// Destroy JPEG object
	jpeg_destroy_decompress(&cinfo);

	// Free raw data buffer
	FS_FreeFile(rawdata);

	// Return the 'rgbadata'
	*pic = rgbadata;
}


/*
====================================================================

IMAGE FLOOD FILLING

====================================================================
*/


/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes
=================
*/

typedef struct
{
	short x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void R_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	const byte			fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = 0;

	// attempt to find opaque black
	for (int i = 0; i < 256; ++i)
	{
		if (d_8to24table[i] == 255) // alpha 1.0
		{
			filledcolor = i;
			break;
		}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
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

		if (x > 0)				FLOODFILL_STEP(-1, -1, 0);
		if (x < skinwidth - 1)	FLOODFILL_STEP(1, 1, 0);
		if (y > 0)				FLOODFILL_STEP(-skinwidth, 0, -1);
		if (y < skinheight - 1)	FLOODFILL_STEP(skinwidth, 0, 1);

		skin[x + skinwidth * y] = fdc;
	}
}

//=======================================================



/*
================
GL_ResampleTextureLerpLine
from DarkPlaces
================
*/

void GL_ResampleTextureLerpLine (byte *in, byte *out, int inwidth, int outwidth) //mxd. Very similar to R_ResampleShotLerpLine, except alpha handling
{ 
	int j, f;

	const int fstep = (int)(inwidth * 65536.0f / outwidth);
	const int endx = inwidth - 1;
	int oldx = 0;

	for (j = 0, f = 0; j < outwidth; j++, f += fstep) 
	{
		const int xi = (int) f >> 16; 
		if (xi != oldx) 
		{ 
			in += (xi - oldx) * 4; 
			oldx = xi; 
		}

		if (xi < endx) 
		{
			const int l2 = f & 0xFFFF;
			const int l1 = 0x10000 - l2; 

			*out++ = (byte)((in[0] * l1 + in[4] * l2) >> 16);
			*out++ = (byte)((in[1] * l1 + in[5] * l2) >> 16); 
			*out++ = (byte)((in[2] * l1 + in[6] * l2) >> 16); 
			*out++ = (byte)((in[3] * l1 + in[7] * l2) >> 16); 
		} 
		else // last pixel of the line has no pixel to lerp to 
		{ 
			*out++ = in[0]; 
			*out++ = in[1]; 
			*out++ = in[2]; 
			*out++ = in[3]; 
		} 
	} 
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight) //mxd. Very similar to R_ResampleShot, except alpha handling
{ 
	int i, j, f;

	byte *out = outdata;
	const int fstep = (int)(inheight * 65536.0f / outheight);

	byte *row1 = malloc(outwidth * 4);
	byte *row2 = malloc(outwidth * 4);
	byte *inrow = indata;
	int oldy = 0;
	const int endy = inheight - 1;

	GL_ResampleTextureLerpLine(inrow, row1, inwidth, outwidth);
	GL_ResampleTextureLerpLine(inrow + inwidth * 4, row2, inwidth, outwidth);

	for (i = 0, f = 0; i < outheight; i++, f += fstep)
	{
		const int yi = f >> 16;
		if (yi != oldy)
		{
			inrow = (byte *)indata + inwidth * 4 * yi;

			if (yi == oldy + 1)
				memcpy(row1, row2, outwidth * 4);
			else
				GL_ResampleTextureLerpLine(inrow, row1, inwidth, outwidth);

			if (yi < endy)
				GL_ResampleTextureLerpLine(inrow + inwidth * 4, row2, inwidth, outwidth);
			else
				memcpy(row2, row1, outwidth * 4);

			oldy = yi;
		}

		if (yi < endy)
		{
			const int l2 = f & 0xFFFF;
			const int l1 = 0x10000 - l2;

			for (j = 0; j < outwidth; j++)
			{
				*out++ = (byte)((*row1++ * l1 + *row2++ * l2) >> 16);
				*out++ = (byte)((*row1++ * l1 + *row2++ * l2) >> 16);
				*out++ = (byte)((*row1++ * l1 + *row2++ * l2) >> 16);
				*out++ = (byte)((*row1++ * l1 + *row2++ * l2) >> 16);
			}

			row1 -= outwidth * 4;
			row2 -= outwidth * 4;
		}
		else // last line has no pixels to lerp to 
		{
			for (j = 0; j < outwidth; j++)
			{
				*out++ = *row1++;
				*out++ = *row1++;
				*out++ = *row1++;
				*out++ = *row1++;
			}

			row1 -= outwidth * 4;
		}
	}

	free(row1);
	free(row2);
}


/*
================
GL_LightScaleTexture

Scale up the pixel values in a texture to increase the lighting range
================
*/
void GL_LightScaleTexture (unsigned *in, int inwidth, int inheight, qboolean only_gamma)
{
	byte *p = (byte *)in;
	const int size = inwidth * inheight;
	
	if (only_gamma)
	{
		for (int i = 0; i < size; i++, p += 4)
			for(int c = 0; c < 3; c++)
				p[c] = gammatable[p[c]];
	}
	else
	{
		for (int i = 0 ; i < size; i++, p += 4)
			for (int c = 0; c < 3; c++)
				p[c] = gammatable[intensitytable[p[c]]];
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap (byte *in, int width, int height)
{
	width <<= 2;
	height >>= 1;
	byte *out = in;

	for (int h = 0; h < height; h++, in += width)
	{
		for (int w = 0; w < width; w += 8, out += 4, in += 8)
		{
			out[0] = (in[0] + in[4] + in[width + 0] + in[width + 4]) >> 2;
			out[1] = (in[1] + in[5] + in[width + 1] + in[width + 5]) >> 2;
			out[2] = (in[2] + in[6] + in[width + 2] + in[width + 6]) >> 2;
			out[3] = (in[3] + in[7] + in[width + 3] + in[width + 7]) >> 2;
		}
	}
}


/*
================
GL_BuildPalettedTexture
================
*/
void GL_BuildPalettedTexture (unsigned char *paletted_texture, unsigned char *scaled, int scaled_width, int scaled_height)
{
	for (int i = 0; i < scaled_width * scaled_height; i++ )
	{
		const unsigned int r = (scaled[0] >> 3) & 31;
		const unsigned int g = (scaled[1] >> 2) & 63;
		const unsigned int b = (scaled[2] >> 3) & 31;

		const unsigned int c = r | (g << 5) | (b << 11);

		paletted_texture[i] = glState.d_16to8table[c];

		scaled += 4;
	}
}


/*
===============
here starts modified code
by Heffo/changes by Nexus
===============
*/

static int		upload_width, upload_height; //** DMP made local to module
static qboolean uploaded_paletted;			//** DMP ditto

int nearest_power_of_2 (int size)
{
	int i = 2;

	// NeVo - infinite loop bug-fix
	if (size == 1)
		return size; 

	while (true) 
	{
		i <<= 1;
		if (size == i)
			return i;

		if (size > i && size < (i << 1)) 
		{
			if (size >= (i + (i << 1)) / 2)
				return i << 1;

			return i;
		}
	}
}

/*
===============
GL_Upload32

Returns has_alpha
===============
*/
//#define USE_GLMIPMAP
qboolean GL_Upload32 (unsigned *data, int width, int height, qboolean mipmap)
{
	unsigned 	*scaled;
	int			scaled_width, scaled_height;
	int			comp;

	uploaded_paletted = false;

	//
	// scan the texture for any non-255 alpha
	//
	const int size = width * height;
	byte *scan = (byte *)data + 3;
	int samples = GL_SOLID_FORMAT;
	for (int i = 0; i < size; i++, scan += 4)
	{
		if (*scan != 255)
		{
			samples = GL_ALPHA_FORMAT;
			break;
		}
	}

	// Heffo - ARB Texture Compression
	qglHint(GL_TEXTURE_COMPRESSION_HINT_ARB, GL_NICEST);
	if (samples == GL_SOLID_FORMAT)
		comp = (glState.texture_compression ? GL_COMPRESSED_RGB_ARB : gl_tex_solid_format);
	else if (samples == GL_ALPHA_FORMAT)
		comp = (glState.texture_compression ? GL_COMPRESSED_RGBA_ARB : gl_tex_alpha_format);

	//
	// find sizes to scale to
	//
	if (glConfig.arbTextureNonPowerOfTwo && (!mipmap || r_nonpoweroftwo_mipmaps->value))
	{
		scaled_width = width;
		scaled_height = height;
	}
	else
	{
		scaled_width = nearest_power_of_2(width);
		scaled_height = nearest_power_of_2(height);
	}

	scaled_width = min(glConfig.max_texsize, scaled_width);
	scaled_height = min(glConfig.max_texsize, scaled_height);

	//
	// allow sampling down of the world textures for speed
	//
	if (mipmap && (int)r_picmip->value > 0)
	{
		int maxsize;

		if ((int)r_picmip->value == 1)		// clamp to 512x512
			maxsize = 512;
		else if ((int)r_picmip->value == 2) // clamp to 256x256
			maxsize = 256;
		else								// clamp to 128x128
			maxsize = 128;

		while (true)
		{
			if (scaled_width <= maxsize && scaled_height <= maxsize)
				break;

			scaled_width >>= 1;
			scaled_height >>= 1;
		}
	}

	//
	// resample texture if needed
	//
	if (scaled_width != width || scaled_height != height) 
	{
		scaled = malloc(scaled_width * scaled_height * 4);
		GL_ResampleTexture(data, width, height, scaled, scaled_width, scaled_height);
	}
	else
	{
		scaled_width = width;
		scaled_height = height;
		scaled = data;
	}

	if (!glState.gammaRamp)
		GL_LightScaleTexture(scaled, scaled_width, scaled_height, !mipmap);

	//
	// generate mipmaps and upload
	//
#ifdef USE_GLMIPMAP
	qglTexImage2D (GL_TEXTURE_2D, 0, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	if (mipmap)
	{
		int		mip_width, mip_height, miplevel = 0;

		mip_width = scaled_width;	mip_height = scaled_height;
		while (mip_width > 1 || mip_height > 1)
		{
			GL_MipMap ((byte *)scaled, mip_width, mip_height);
			mip_width = max(mip_width>>1, 1);
			mip_height = max(mip_height>>1, 1);
			miplevel++;
			qglTexImage2D (GL_TEXTURE_2D, miplevel, comp, mip_width, mip_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}
#else
	if (mipmap)
	{
		if (glState.sgis_mipmap)
		{
			qglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, true);
			qglTexImage2D(GL_TEXTURE_2D, 0, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		} 
		else
		{
			gluBuild2DMipmaps(GL_TEXTURE_2D, comp, scaled_width, scaled_height, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}
	else
	{
		qglTexImage2D(GL_TEXTURE_2D, 0, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	}
#endif

	if (scaled_width != width || scaled_height != height)
		free(scaled);

	upload_width = scaled_width;
	upload_height = scaled_height;

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (mipmap ? gl_filter_min : gl_filter_max));
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	// Set anisotropic filter if supported and enabled
	if (mipmap && glConfig.anisotropic && r_anisotropic->value)
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_anisotropic->value);

	return (samples == GL_ALPHA_FORMAT || samples == GL_COMPRESSED_RGBA_ARB);
}

/*
===============
GL_Upload8

Returns has_alpha
===============
*/
qboolean GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, qboolean is_sky )
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

			// copy rgb components
			for(int c = 0; c < 3; c++)
				((byte *)&trans[i])[c] = ((byte *)&d_8to24table[p])[c];
		}
	}

	return GL_Upload32(trans, width, height, mipmap);
}


/*
================
R_LoadPic

This is also used as an entry point for the generated notexture
Nexus  - changes for hires-textures
================
*/
image_t *R_LoadPic (char *name, byte *pic, int width, int height, imagetype_t type, int bits)
{
	image_t		*image;
	int			i;
	char s[128]; 

	// find a free image_t
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->texnum)
			break;
	}

	if (i == numgltextures)
	{
		if (numgltextures == MAX_GLTEXTURES)
			VID_Error(ERR_DROP, "MAX_GLTEXTURES");

		numgltextures++;
	}

	image = &gltextures[i];

	if (strlen(name) >= sizeof(image->name))
		VID_Error(ERR_DROP, "Draw_LoadPic: image name \"%s\" is too long (maximum is %i)", name, sizeof(image->name));

	Q_strncpyz(image->name, name, sizeof(image->name));
	image->hash = Com_HashFileName(name, 0, false);	// Knightmare added
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->type = type;
	image->replace_scale_w = image->replace_scale_h = 1.0f; // Knightmare added

	if (type == it_skin && bits == 8)
		R_FloodFillSkin(pic, width, height);

// replacement scaling hack for TGA/JPEG HUD images and skins
// TODO: replace this with shaders as soon as they are supported
	const int len = strlen(name); 
	Q_strncpyz(s, name, sizeof(s));
	// check if we have a tga/jpg pic
#ifdef PNG_SUPPORT
	if (type == it_pic && (!strcmp(s + len - 4, ".tga") || !strcmp(s + len - 4, ".png") || !strcmp(s + len - 4, ".jpg")) )
#else	// PNG_SUPPORT
	if ((type == it_pic) && (!strcmp(s+len-4, ".tga") || !strcmp(s+len-4, ".jpg")) )
#endif	// PNG_SUPPORT
	{ 
		byte	*pcx, *palette;
		int		pcxwidth, pcxheight;
		s[len - 3] = 'p'; // replace extension 
		s[len - 2] = 'c';
		s[len - 1] = 'x';
		LoadPCX(s, &pcx, &palette, &pcxwidth, &pcxheight); // load .pcx file 
		
		if (pcx && pcxwidth > 0 && pcxheight > 0)
		{
			image->replace_scale_w = (float)pcxwidth / image->width;
			image->replace_scale_h = (float)pcxheight / image->height;
		}

		if (pcx) free(pcx);
		if (palette) free(palette);
	}	

	// load little pics into the scrap
	if (image->type == it_pic && bits == 8 && image->width < 64 && image->height < 64)
	{
		int x, y;
		const int texnum = Scrap_AllocBlock(image->width, image->height, &x, &y);
		if (texnum == -1)
			goto nonscrap;

		scrap_dirty = true;

		// copy the texels into the scrap block
		int k = 0;
		for (int i = 0; i < image->height; i++)
			for (int j = 0; j < image->width; j++, k++)
				scrap_texels[texnum][(y + i) * BLOCK_WIDTH + x + j] = pic[k];

		image->texnum = TEXNUM_SCRAPS + texnum;
		image->scrap = true;
		image->has_alpha = true;
		image->sl = (x + 0.01) / (float)BLOCK_WIDTH;
		image->sh = (x + image->width - 0.01) / (float)BLOCK_WIDTH;
		image->tl = (y + 0.01) / (float)BLOCK_WIDTH;
		image->th = (y + image->height - 0.01) / (float)BLOCK_WIDTH;
	}
	else
	{
nonscrap:
		image->scrap = false;
		image->texnum = TEXNUM_IMAGES + (image - gltextures);
		GL_Bind(image->texnum);

		if (bits == 8)
			image->has_alpha = GL_Upload8(pic, width, height, (image->type != it_pic && image->type != it_sky), image->type == it_sky);
		else
			image->has_alpha = GL_Upload32((unsigned *)pic, width, height, (image->type != it_pic && image->type != it_sky));

		image->upload_width = upload_width;		// after power of 2 and scales
		image->upload_height = upload_height;
		image->paletted = uploaded_paletted;
		image->sl = 0;
		image->sh = 1;
		image->tl = 0;
		image->th = 1;
	}

	return image;
}


// store the names of last images that failed to load
#define NUM_FAIL_IMAGES 256
char lastFailedImage[NUM_FAIL_IMAGES][MAX_OSPATH];
long lastFailedImageHash[NUM_FAIL_IMAGES];
static unsigned failedImgListIndex;

/*
===============
R_InitFailedImgList
===============
*/
void R_InitFailedImgList (void)
{
	for (int i = 0; i < NUM_FAIL_IMAGES; i++)
	{
		Com_sprintf(lastFailedImage[i], sizeof(lastFailedImage[i]), "\0");
		lastFailedImageHash[i] = 0;
	}

	failedImgListIndex = 0;
}

/*
===============
R_CheckImgFailed
===============
*/
qboolean R_CheckImgFailed (char *name)
{
	const long hash = Com_HashFileName(name, 0, false);
	for (int i = 0; i < NUM_FAIL_IMAGES; i++)
		if (hash == lastFailedImageHash[i] && lastFailedImage[i][0] && !strcmp(name, lastFailedImage[i])) // compare hash first
			return true; // we already tried to load this image, didn't find it

	return false;
}

/*
===============
R_AddToFailedImgList
===============
*/
void R_AddToFailedImgList (char *name)
{
	if (!strncmp(name, "save/", 5)) // don't add saveshots
		return;

	Com_sprintf(lastFailedImage[failedImgListIndex], sizeof(lastFailedImage[failedImgListIndex]), "%s", name);
	lastFailedImageHash[failedImgListIndex] = Com_HashFileName(name, 0, false);
	failedImgListIndex++;

	// wrap around to start of list
	if (failedImgListIndex >= NUM_FAIL_IMAGES)
		failedImgListIndex = 0;
}

/*
================
R_LoadWal
================
*/
image_t *R_LoadWal (char *name, imagetype_t type)
{
	miptex_t *mt;
	FS_LoadFile(name, (void **)&mt);
	if (!mt)
	{
		if (type == it_wall)
			VID_Printf(PRINT_ALL, "R_FindImage: can't load %s\n", name);

		return NULL;
	}

	const int width = LittleLong(mt->width);
	const int height = LittleLong(mt->height);
	const int ofs = LittleLong(mt->offsets[0]);

	image_t *image = R_LoadPic(name, (byte *)mt + ofs, width, height, it_wall, 8);

	FS_FreeFile((void *)mt);

	return image;
}


/*
===============
R_FindImage

Finds or loads the given image
===============
*/
image_t	*R_FindImage (char *name, imagetype_t type)
{
	image_t	*image;
	int		i;
	int		width, height;
	char	s[128];

	if (!name)
		return NULL;

	const int len = strlen(name);
	if (len < 5)
		return NULL;

	// fix up bad image paths
    char *tmp = name;
    while (*tmp != 0)
    {
        if (*tmp == '\\')
            *tmp = '/';
        tmp++;
    }

	// look for it
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!strcmp(name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	// don't try again to load an image that just failed
	if (R_CheckImgFailed(name))
	{
		if (!strcmp(name + len - 4, ".tga"))
		{
#ifdef PNG_SUPPORT
			// fall back to png
			Q_strncpyz(s, name, sizeof(s));
			s[len - 3] = 'p';
			s[len - 2] = 'n';
			s[len - 1] = 'g';
			return R_FindImage(s, type);
#else	// PNG_SUPPORT
			// fall back to jpg
			Q_strncpyz(s, name, sizeof(s));
			s[len - 3] = 'j';
			s[len - 2] = 'p';
			s[len - 1] = 'g';
			return R_FindImage(s, type);
#endif	// PNG_SUPPORT
		}

#ifdef PNG_SUPPORT
		if (!strcmp(name + len - 4, ".png"))
		{
			// fall back to jpg
			Q_strncpyz(s, name, sizeof(s));
			s[len - 3] = 'j';
			s[len - 2] = 'p';
			s[len - 1] = 'g';
			return R_FindImage(s, type);
		}
#endif	// PNG_SUPPORT

		return NULL;
	}

	// MrG's automatic JPG & TGA loading
	// search for TGAs or JPGs to replace .pcx and .wal images
	if (!strcmp(name + len - 4, ".pcx") || !strcmp(name + len - 4, ".wal")) //TODO: mxd. shouldn't this actually look for JPGs (and probably PNGs)?
	{
		Q_strncpyz(s, name, sizeof(s));
		s[len - 3] = 't';
		s[len - 2] = 'g';
		s[len - 1] = 'a';
		image = R_FindImage(s, type);
		if (image)
			return image;
	}

	//
	// load the pic from disk
	//
	byte *pic = NULL;
	byte *palette = NULL;

	if (!strcmp(name + len - 4, ".pcx"))
	{
		LoadPCX(name, &pic, &palette, &width, &height);
		if (pic)
			image = R_LoadPic(name, pic, width, height, type, 8);
		else
			image = NULL;
	}
	else if (!strcmp(name + len - 4, ".wal"))
	{
		image = R_LoadWal(name, type);
	}
	else if (!strcmp(name + len - 4, ".tga"))
	{
		R_LoadTGA(name, &pic, &width, &height);
		if (pic)
			image = R_LoadPic(name, pic, width, height, type, 32);
#ifdef PNG_SUPPORT
		else
		{ 
			// fall back to png
			R_AddToFailedImgList(name);
			Q_strncpyz(s, name, sizeof(s));
			s[len - 3] = 'p';
			s[len - 2] = 'n';
			s[len - 1] = 'g';
			return R_FindImage(s, type);
		}
#else	// PNG_SUPPORT
		else
		{
			// fall back to jpg
			R_AddToFailedImgList(name);
			Q_strncpyz(s, name, sizeof(s));
			s[len - 3] = 'j';
			s[len - 2] = 'p';
			s[len - 1] = 'g';
			return R_FindImage(s, type);
		}
#endif	// PNG_SUPPORT
	}
#ifdef PNG_SUPPORT
	else if (!strcmp(name + len - 4, ".png"))
	{
		R_LoadPNG(name, &pic, &width, &height);
		if (pic)
		{
			image = R_LoadPic(name, pic, width, height, type, 32);
		}
		else
		{
			// fall back to jpg
			R_AddToFailedImgList(name);
			Q_strncpyz (s, name, sizeof(s));
			s[len - 3] = 'j';
			s[len - 2] = 'p';
			s[len - 1] = 'g';
			return R_FindImage(s, type);
		}
	}
#endif	// PNG_SUPPORT
	else if (!strcmp(name + len - 4, ".jpg")) // Heffo - JPEG support
	{
		R_LoadJPG(name, &pic, &width, &height);
		if (pic)
			image = R_LoadPic(name, pic, width, height, type, 32);
		else
			image = NULL;
	}
	else
	{
		image = NULL;
	}

	if (!image)
		R_AddToFailedImgList(name);

	if (pic) free(pic);
	if (palette) free(palette);

	return image;
}



/*
===============
R_RegisterSkin
===============
*/
struct image_s *R_RegisterSkin (char *name)
{
	return R_FindImage(name, it_skin);
}


/*
================
R_FreeUnusedImages

Any image that was not touched on this registration sequence will be freed.
================
*/
void R_FreeUnusedImages (void)
{
	int		i;
	image_t	*image;

	// never free notexture or particle textures
	glMedia.notexture->registration_sequence = registration_sequence;
	glMedia.whitetexture->registration_sequence = registration_sequence;
#ifdef ROQ_SUPPORT
	glMedia.rawtexture->registration_sequence = registration_sequence;
#endif // ROQ_SUPPORT
	glMedia.envmappic->registration_sequence = registration_sequence;
	glMedia.spheremappic->registration_sequence = registration_sequence;
	glMedia.causticwaterpic->registration_sequence = registration_sequence;
	glMedia.causticslimepic->registration_sequence = registration_sequence;
	glMedia.causticlavapic->registration_sequence = registration_sequence;
	glMedia.shelltexture->registration_sequence = registration_sequence;
	glMedia.particlebeam->registration_sequence = registration_sequence;

	for (i = 0; i < PARTICLE_TYPES; i++)
		if (glMedia.particletextures[i]) // dont mess with null ones silly :p
			glMedia.particletextures[i]->registration_sequence = registration_sequence;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
			continue; // used this sequence

		if (!image->registration_sequence)
			continue; // free image_t slot

		if (image->type == it_pic)
			continue; // don't free pics

		// free it
		qglDeleteTextures(1, &image->texnum);
		memset(image, 0, sizeof(*image));
	}
}


/*
===============
Draw_GetPalette
===============
*/
void Draw_GetPalette ()
{
	byte	*pic, *pal;
	int		width, height;

	// get the palette
	LoadPCX("pics/colormap.pcx", &pic, &pal, &width, &height);
	if (!pal)
		VID_Error(ERR_FATAL, "Couldn't load pics/colormap.pcx");

	for (int i = 0; i < 256; i++)
	{
		const int r = pal[i * 3 + 0];
		const int g = pal[i * 3 + 1];
		const int b = pal[i * 3 + 2];
		
		const unsigned v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		d_8to24table[i] = LittleLong(v);
	}

	d_8to24table[255] &= LittleLong(0xffffff);	// 255 is transparent

	free(pic);
	free(pal);
}


/*
===============
R_InitImages
===============
*/
void R_InitImages (void)
{
	registration_sequence = 1;

	// Knightmare- added Vic's RGB brightening
	r_intensity = Cvar_Get("r_intensity", (glConfig.mtexcombine ? "1" : "2"), 0);

	if (r_intensity->value <= 1)
		Cvar_Set("r_intensity", "1");

	glState.inverse_intensity = 1 / r_intensity->value;

	R_InitFailedImgList(); // Knightmare added
	Draw_GetPalette();

	if (qglColorTableEXT)
	{
		FS_LoadFile("pics/16to8.dat", &glState.d_16to8table);
		if (!glState.d_16to8table)
			VID_Error(ERR_FATAL, "Couldn't load pics/16to8.pcx");
	}

	float g = vid_gamma->value;
	if (glConfig.rendType == GLREND_VOODOO) //TODO: get rid
		g = 1.0f;

	for (int i = 0; i < 256; i++)
	{
		if (g == 1)
		{
			gammatable[i] = i;
		}
		else
		{
			const float inf = 255 * pow((i + 0.5) / 255.5, g) + 0.5;
			gammatable[i] = clamp(inf, 0, 255);
		}
	}

	for (int i = 0; i < 256; i++)
	{
		const int intensity = i * r_intensity->value;
		intensitytable[i] = min(intensity, 255);
	}

	R_InitBloomTextures(); // BLOOMS
}

/*
===============
R_FreePic
by Knightmare
Frees a single pic
===============
*/
void R_FreePic (char *name)
{
	int		i;
	image_t	*image;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->registration_sequence)
			continue; // free image_t slot

		if (image->type != it_pic)
			continue; // only free pics

		if (!strcmp(name, image->name))
		{
			// free it
			qglDeleteTextures(1, &image->texnum);
			memset(image, 0, sizeof(*image));
			return; //we're done here
		}
	}
}

/*
===============
R_ShutdownImages
===============
*/
void R_ShutdownImages (void)
{
	int		i;
	image_t	*image;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->registration_sequence)
			continue; // free image_t slot

		// free it
		qglDeleteTextures(1, &image->texnum);
		memset(image, 0, sizeof(*image));
	}
}