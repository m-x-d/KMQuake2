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
// r_model_md3.c -- md3 model loading

#include "r_local.h"

// Calc exact alloc size for MD3 in memory
static size_t GetAllocSizeMD3(void *buffer)
{
	dmd3_t *pinmodel = (dmd3_t *)buffer;

	// Calc sizes rounded to cacheline
	uint totalsize = ALIGN_TO_CACHELINE(sizeof(maliasmodel_t));
	totalsize += ALIGN_TO_CACHELINE(sizeof(maliasframe_t) * pinmodel->num_frames);
	totalsize += ALIGN_TO_CACHELINE(sizeof(maliastag_t) * pinmodel->num_frames * pinmodel->num_tags);
	totalsize += ALIGN_TO_CACHELINE(sizeof(maliasmesh_t) * pinmodel->num_meshes);

	dmd3mesh_t *pinmesh = (dmd3mesh_t *)((byte *)pinmodel + pinmodel->ofs_meshes);

	for (int i = 0; i < pinmodel->num_meshes; i++)
	{
		totalsize += ALIGN_TO_CACHELINE(sizeof(maliasskin_t) * pinmesh->num_skins);
		totalsize += ALIGN_TO_CACHELINE(sizeof(index_t) * pinmesh->num_tris * 3);
		totalsize += ALIGN_TO_CACHELINE(sizeof(maliascoord_t) * pinmesh->num_verts);
		totalsize += ALIGN_TO_CACHELINE(pinmodel->num_frames * pinmesh->num_verts * sizeof(maliasvertex_t));
		totalsize += ALIGN_TO_CACHELINE(sizeof(int) * pinmesh->num_tris * 3);

		pinmesh = (dmd3mesh_t *)((byte *)pinmesh + pinmesh->meshsize);
	}

	return totalsize;
}

extern void Mod_BuildTriangleNeighbors(maliasmesh_t *mesh); //mxd

void Mod_LoadAliasMD3Model(model_t *mod, void *buffer)
{
	//mxd. Allocate memory
	mod->extradata = ModChunk_Begin(GetAllocSizeMD3(buffer));

	dmd3_t *pinmodel = (dmd3_t *)buffer;

	if (pinmodel->version != MD3_ALIAS_VERSION)
		VID_Error(ERR_DROP, "Model %s has wrong version number (%i should be %i)", mod->name, pinmodel->version, MD3_ALIAS_VERSION);

	maliasmodel_t *poutmodel = ModChunk_Alloc(sizeof(maliasmodel_t));

	// Copy header fields
	poutmodel->num_frames = pinmodel->num_frames;
	poutmodel->num_tags = pinmodel->num_tags;
	poutmodel->num_meshes = pinmodel->num_meshes;

	// Sanity checks
	if (poutmodel->num_frames <= 0)
		VID_Error(ERR_DROP, "Model %s has no frames", mod->name);
	else if (poutmodel->num_frames > MD3_MAX_FRAMES)
		VID_Error(ERR_DROP, "Model %s has too many frames (%i, maximum is %i)", mod->name, poutmodel->num_frames, MD3_MAX_FRAMES);

	if (poutmodel->num_tags > MD3_MAX_TAGS)
		VID_Error(ERR_DROP, "Model %s has too many tags (%i, maximum is %i)", mod->name, poutmodel->num_tags, MD3_MAX_TAGS);
	else if (poutmodel->num_tags < 0)
		VID_Error(ERR_DROP, "Model %s has invalid number of tags", mod->name);

	if (poutmodel->num_meshes <= 0)
		VID_Error(ERR_DROP, "Model %s has no meshes", mod->name);
	else if (poutmodel->num_meshes > MD3_MAX_MESHES)
		VID_Error(ERR_DROP, "Model %s has too many meshes (%i, maximum is %i)", mod->name, poutmodel->num_meshes, MD3_MAX_MESHES);

	// Load frames
	dmd3frame_t *pinframe = (dmd3frame_t *)((byte *)pinmodel + pinmodel->ofs_frames);
	maliasframe_t *poutframe = poutmodel->frames = ModChunk_Alloc(sizeof(maliasframe_t) * poutmodel->num_frames);

	mod->radius = 0;
	ClearBounds(mod->mins, mod->maxs);

	for (int i = 0; i < poutmodel->num_frames; i++, pinframe++, poutframe++)
	{
		for (int j = 0; j < 3; j++)
		{
			poutframe->mins[j] = pinframe->mins[j];
			poutframe->maxs[j] = pinframe->maxs[j];
			poutframe->scale[j] = MD3_XYZ_SCALE;
			poutframe->translate[j] = pinframe->translate[j];
		}

		poutframe->radius = pinframe->radius;

		mod->radius = max(mod->radius, poutframe->radius);
		AddPointToBounds(poutframe->mins, mod->mins, mod->maxs);
		AddPointToBounds(poutframe->maxs, mod->mins, mod->maxs);
	}

	// Load tags
	dmd3tag_t *pintag = (dmd3tag_t *)((byte *)pinmodel + pinmodel->ofs_tags);
	maliastag_t *pouttag = poutmodel->tags = ModChunk_Alloc(sizeof(maliastag_t) * poutmodel->num_frames * poutmodel->num_tags);

	for (int i = 0; i < poutmodel->num_frames; i++)
	{
		for (int l = 0; l < poutmodel->num_tags; l++, pintag++, pouttag++)
		{
			memcpy(pouttag->name, pintag->name, MD3_MAX_PATH);
			for (int j = 0; j < 3; j++)
			{
				pouttag->orient.origin[j] = pintag->orient.origin[j];

				for (int c = 0; c < 3; c++)
					pouttag->orient.axis[c][j] = pintag->orient.axis[c][j];
			}
		}
	}

	// Load meshes
	dmd3mesh_t *pinmesh = (dmd3mesh_t *)((byte *)pinmodel + pinmodel->ofs_meshes);
	maliasmesh_t *poutmesh = poutmodel->meshes = ModChunk_Alloc(sizeof(maliasmesh_t)*poutmodel->num_meshes);

	for (int i = 0; i < poutmodel->num_meshes; i++, poutmesh++)
	{
		memcpy(poutmesh->name, pinmesh->name, MD3_MAX_PATH);

		if (strncmp(pinmesh->id, "IDP3", 4))
			VID_Error(ERR_DROP, "Mesh %s in model %s has wrong id (%s should be IDP3)", poutmesh->name, mod->name, pinmesh->id);

		poutmesh->num_tris = pinmesh->num_tris;
		poutmesh->num_skins = pinmesh->num_skins;
		poutmesh->num_verts = pinmesh->num_verts;

		if (poutmesh->num_skins <= 0)
			VID_Error(ERR_DROP, "Mesh %i in model %s has no skins", i, mod->name);
		else if (poutmesh->num_skins > MD3_MAX_SHADERS)
			VID_Error(ERR_DROP, "Mesh %i in model %s has too many skins (%i, maximum is %i)", i, mod->name, poutmesh->num_skins, MD3_MAX_SHADERS);

		if (poutmesh->num_tris <= 0)
			VID_Error(ERR_DROP, "Mesh %i in model %s has no triangles", i, mod->name);

		if (poutmesh->num_verts <= 0)
			VID_Error(ERR_DROP, "Mesh %i in model %s has no vertices", i, mod->name);

		// Register all skins
		dmd3skin_t *pinskin = (dmd3skin_t *)((byte *)pinmesh + pinmesh->ofs_skins);
		maliasskin_t *poutskin = poutmesh->skins = ModChunk_Alloc(sizeof(maliasskin_t) * poutmesh->num_skins);

		for (int j = 0; j < poutmesh->num_skins; j++, pinskin++, poutskin++)
		{
			char name[MD3_MAX_PATH];
			memcpy(name, pinskin->name, MD3_MAX_PATH);

			if (name[1] == 'o') //mxd. Dafuk is that?..
				name[0] = 'm';
			if (name[1] == 'l')
				name[0] = 'p';

			memcpy(poutskin->name, name, MD3_MAX_PATH);
			Com_sprintf(poutskin->glowname, sizeof(poutskin->glowname), "\0"); // Set null glowskin
			mod->skins[i][j] = R_FindImage(name, it_skin, false);
		}

		// Load indices
		index_t *pinindex = (index_t *)((byte *)pinmesh + pinmesh->ofs_tris);
		index_t *poutindex = poutmesh->indexes = ModChunk_Alloc(sizeof(index_t) * poutmesh->num_tris * 3);

		for (int j = 0; j < poutmesh->num_tris; j++, pinindex += 3, poutindex += 3)
			for (int c = 0; c < 3; c++)
				poutindex[c] = pinindex[c];

		// Load texture coordinates
		dmd3coord_t *pincoord = (dmd3coord_t *)((byte *)pinmesh + pinmesh->ofs_tcs);
		maliascoord_t *poutcoord = poutmesh->stcoords = ModChunk_Alloc(sizeof(maliascoord_t) * poutmesh->num_verts);

		for (int j = 0; j < poutmesh->num_verts; j++, pincoord++, poutcoord++)
		{
			poutcoord->st[0] = pincoord->st[0];
			poutcoord->st[1] = pincoord->st[1];
		}

		// Load vertices and normals
		dmd3vertex_t *pinvert = (dmd3vertex_t *)((byte *)pinmesh + pinmesh->ofs_verts);
		maliasvertex_t *poutvert = poutmesh->vertexes = ModChunk_Alloc(poutmodel->num_frames * poutmesh->num_verts * sizeof(maliasvertex_t));

		for (int l = 0; l < poutmodel->num_frames; l++)
		{
			for (int j = 0; j < poutmesh->num_verts; j++, pinvert++, poutvert++)
			{
				for (int c = 0; c < 3; c++)
					poutvert->xyz[c] = pinvert->point[c];

				poutvert->normal[0] = (pinvert->norm >> 0) & 0xff;
				poutvert->normal[1] = (pinvert->norm >> 8) & 0xff;

				float lat = (pinvert->norm >> 8) & 0xff;
				float lng = (pinvert->norm & 0xff);

				lat *= M_PI / 128;
				lng *= M_PI / 128;

				vec3_t normal;
				normal[0] = cosf(lat) * sinf(lng);
				normal[1] = sinf(lat) * sinf(lng);
				normal[2] = cosf(lng);

				// Use ye olde quantized normals for shading
				float maxdot = -999999.0;
				int maxdotindex = -1;

				for (int k = 0; k < NUMVERTEXNORMALS; k++)
				{
					const float dot = DotProduct(normal, vertexnormals[k]);
					if (dot > maxdot)
					{
						maxdot = dot;
						maxdotindex = k;
					}
				}

				poutvert->lightnormalindex = maxdotindex;
			}
		}

		pinmesh = (dmd3mesh_t *)((byte *)pinmesh + pinmesh->meshsize);

		// Build triangle neighbors
		poutmesh->trneighbors = ModChunk_Alloc(sizeof(int) * poutmesh->num_tris * 3);
		Mod_BuildTriangleNeighbors(poutmesh);
	}

	mod->hasAlpha = false;
	Mod_LoadModelScript(mod, poutmodel); // MD3 skin scripting

	mod->type = mod_md3;
}