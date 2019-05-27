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

// r_image_pcx.c

#include "r_local.h"

void LoadPCX(char *filename, byte **pic, byte **palette, int *width, int *height)
{
	*pic = NULL;

	if(palette)
		*palette = NULL;

	// Load the file
	byte *raw;
	const int len = FS_LoadFile(filename, (void **)&raw);
	if (!raw)
		return;

	// Parse the PCX file
	pcx_t *pcx = (pcx_t *)raw;
	raw = &pcx->data;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 640
		|| pcx->ymax >= 480)
	{
		VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: pcx file '%s' is unsupported.\n", __func__, filename);
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

			if ((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
			{
				runLength = 1;
			}

			while (runLength-- > 0)
				pix[x++] = dataByte;
		}
	}

	if (raw - (byte *)pcx > len)
	{
		VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: pcx file '%s' is malformed.\n", __func__, filename);
		free(*pic);
		*pic = NULL;
	}

	FS_FreeFile(pcx);
}

void GetPCXInfo(char *filename, int *width, int *height) //mxd. From YQ2
{
	*width = 0;
	*height = 0;
	
	byte *raw;
	const int size = FS_LoadFile(filename, (void **)&raw);

	if (!raw)
		return;

	if (size <= (int)sizeof(pcx_t)) //mxd. Added header size check
	{
		FS_FreeFile(raw);
		return;
	}

	pcx_t* pcx = (pcx_t *)raw;

	*width = pcx->xmax + 1;
	*height = pcx->ymax + 1;

	FS_FreeFile(raw);
}