/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2015 Daniel Gibson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * File formats supported by stb_image, for now only tga, png, jpg
 * See also https://github.com/nothings/stb
 *
 * =======================================================================
 */

#include <stdlib.h>
#include "r_local.h"

// Disable unneeded stuff
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_BMP
#define STBI_NO_GIF
#define STBI_NO_PSD
#define STBI_NO_PNM

// Make sure STB_image uses standard malloc(), as we'll use standard free() to deallocate
/*#define STBI_MALLOC(sz)    malloc(sz)
#define STBI_REALLOC(p,sz) realloc(p,sz)
#define STBI_FREE(p)       free(p)*/

// Include implementation part of stb_image into this file
#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image/stb_image.h"

// Include resize implementation
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../include/stb_image/stb_image_resize.h"

// Include write implementation
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../include/stb_image/stb_image_write.h"

/*
 * origname: the filename to be opened, might be without extension
 * ext: extension of the type we wanna open ("jpg", "png" or "tga")
 * pic: pointer RGBA pixel data will be assigned to
 */
qboolean STBLoad(const char *name, const char* ext, byte **pic, int *width, int *height)
{
	char filename[256];

	Q_strncpyz(filename, name, sizeof(filename));

	/* Add the extension */
	if (strcmp(COM_FileExtension(filename), ext) != 0)
	{
		Q_strncatz(filename, ".", sizeof(filename));
		Q_strncatz(filename, ext, sizeof(filename));
	}

	*pic = NULL;

	byte* rawdata = NULL;
	const int rawsize = FS_LoadFile(filename, (void **)&rawdata);
	if (!rawdata)
		return false;

	int w, h, bytesPerPixel;
	byte* data = stbi_load_from_memory(rawdata, rawsize, &w, &h, &bytesPerPixel, STBI_rgb_alpha);
	FS_FreeFile(rawdata);

	if (data == NULL)
	{
		VID_Printf(PRINT_ALL, "%s: couldn't load data from '%s' (%s)!\n", __func__, filename, stbi_failure_reason());
		return false;
	}

	VID_Printf(PRINT_DEVELOPER, "%s: loaded '%s'\n", __func__, filename);

	*pic = data;
	*width = w;
	*height = h;

	return true;
}

qboolean STBResize(byte *input_pixels, int input_width, int input_height, byte *output_pixels, int output_width, int output_height, qboolean usealpha)
{
	const int numchannels = (usealpha ? 4 : 3);
	return stbir_resize_uint8(input_pixels, input_width, input_height, 0, output_pixels, output_width, output_height, 0, numchannels);
}

qboolean STBSaveJPG(const char *filename, byte* source, int width, int height, int quality)
{
	stbi_flip_vertically_on_write(true);
	return stbi_write_jpg(filename, width, height, 3, source, quality);
}

qboolean STBSavePNG(const char *filename, byte* source, int width, int height)
{
	stbi_flip_vertically_on_write(true);
	return stbi_write_png(filename, width, height, 3, source, 0);
}

qboolean STBSaveTGA(const char *filename, byte* source, int width, int height)
{
	stbi_flip_vertically_on_write(true);
	return stbi_write_tga(filename, width, height, 3, source);
}