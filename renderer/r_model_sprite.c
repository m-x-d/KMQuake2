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
// r_model_sprite.c -- sprite loading

#include "r_local.h"

void Mod_LoadSpriteModel(model_t *mod, void *buffer, size_t filesize)
{
	//mxd. Allocate memory, copy sprite to extradata
	const size_t memsize = ALIGN_TO_CACHELINE(filesize);
	ModChunk_Begin(memsize);
	mod->extradata = ModChunk_Alloc(memsize);
	memcpy(mod->extradata, buffer, filesize); //mxd

	dsprite_t *sprout = (dsprite_t *)mod->extradata;
	
	// Sanity checks
	if (sprout->version != SPRITE_VERSION)
		VID_Error(ERR_DROP, "%s has wrong version number (%i should be %i)", mod->name, sprout->version, SPRITE_VERSION);

	if (sprout->numframes > MAX_MD2SKINS)
		VID_Error(ERR_DROP, "%s has too many frames (%i > %i)", mod->name, sprout->numframes, MAX_MD2SKINS);

	// Load images
	for (int i = 0; i < sprout->numframes; i++)
		mod->skins[0][i] = R_FindImage(sprout->frames[i].name, it_sprite, false);

	mod->type = mod_sprite;
}