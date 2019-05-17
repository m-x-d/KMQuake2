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
// r_model_malloc.c -- model memory allocation

#include "r_local.h"

// It turns out that the Hunk_Begin(), Hunk_Alloc(), and Hunk_Free()
// functions used for BSP, alias model, and sprite loading are a wrapper around
// VirtualAlloc()/VirtualFree() on Win32 and they should only be used
// rarely and for large blocks of memory.  After about 185-190 VirtualAlloc()
// reserve calls are made on Win7 x64, the subsequent call fails.  Bad Carmack, bad!

// These ModChunk_ functions are a replacement that wrap around malloc()/free()
// and return pointers into sections aligned on cache lines.
// The only caveat is that the allocation size passed to ModChunk_Begin() is
// immediately allocated, not reserved.  Calling it with a maximum memory size
// for each model type would be hugely wasteful.  So a size equal or greater to
// the total amount requested via ModChunk_Alloc() calls in the model loading
// functions must be calculated first.

static int chunkcount;

static byte *membase;
static size_t maxchunksize;
static size_t curchunksize;

void *ModChunk_Begin(size_t maxsize)
{
	// Alocate a chunk of memory, should be exact size needed!
	curchunksize = 0;
	maxchunksize = maxsize;

	membase = malloc(maxsize);

	if (!membase)
		Sys_Error("ModChunk_Begin: malloc of size %i failed, %i chunks already allocated", maxsize, chunkcount);

	memset(membase, 0, maxsize);

	return (void *)membase;
}

void *ModChunk_Alloc(size_t size)
{
	// Round to cacheline
	size = ALIGN_TO_CACHELINE(size);

	curchunksize += size;
	if (curchunksize > maxchunksize)
		Sys_Error("ModChunk_Alloc: overflow");

	return (void *)(membase + curchunksize - size);
}

size_t ModChunk_End(void)
{
	chunkcount++;
	return curchunksize;
}

void ModChunk_Free(void *base)
{
	//mxd. Sanity check
	if (chunkcount == 0)
		Sys_Error("ModChunk_Free: no chunks to free");
	
	free(base);

	chunkcount--;
}