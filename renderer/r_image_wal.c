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

// r_image_wal.c

#include "r_local.h"

image_t *R_LoadWal(char *name, imagetype_t type)
{
	miptex_t *mt;
	FS_LoadFile(name, (void **)&mt);
	if (!mt)
		return NULL;

	image_t *image = R_LoadPic(name, (byte *)mt + mt->offsets[0], mt->width, mt->height, it_wall, 8);

	FS_FreeFile((void *)mt);

	return image;
}

void GetWalInfo(char *name, int *width, int *height) //mxd. From YQ2
{
	*width = 0;
	*height = 0;
	
	miptex_t *mt;
	const int size = FS_LoadFile(name, (void **)&mt);

	if (!mt)
		return;

	if (size < (int)sizeof(miptex_t))
	{
		FS_FreeFile((void *)mt);
		return;
	}

	*width = mt->width;
	*height = mt->height;

	FS_FreeFile((void *)mt);
}