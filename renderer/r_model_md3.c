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
	const int numFrames = LittleLong(pinmodel->num_frames);
	const int numTags = LittleLong(pinmodel->num_tags);
	const int numMeshes = LittleLong(pinmodel->num_meshes);

	// Calc sizes rounded to cacheline
	const size_t headerSize = ALIGN_TO_CACHELINE(sizeof(maliasmodel_t));
	const size_t frameSize = ALIGN_TO_CACHELINE(sizeof(maliasframe_t) * numFrames);
	const size_t tagSize = ALIGN_TO_CACHELINE(sizeof(maliastag_t) * numFrames * numTags);
	const size_t meshSize = ALIGN_TO_CACHELINE(sizeof(maliasmesh_t) * numMeshes);

	dmd3mesh_t *pinmesh = (dmd3mesh_t *)((byte *)pinmodel + LittleLong(pinmodel->ofs_meshes));
	size_t skinSize = 0, indexSize = 0, coordSize = 0, vertSize = 0, trNeighborsSize = 0;

	for (int i = 0; i < numMeshes; i++)
	{
		const int numSkins = LittleLong(pinmesh->num_skins);
		const int numTris = LittleLong(pinmesh->num_tris);
		const int numVerts = LittleLong(pinmesh->num_verts);

		skinSize += ALIGN_TO_CACHELINE(sizeof(maliasskin_t) * numSkins);
		indexSize += ALIGN_TO_CACHELINE(sizeof(index_t) * numTris * 3);
		coordSize += ALIGN_TO_CACHELINE(sizeof(maliascoord_t) * numVerts);
		vertSize += ALIGN_TO_CACHELINE(numFrames * numVerts * sizeof(maliasvertex_t));
		trNeighborsSize += ALIGN_TO_CACHELINE(sizeof(int) * numTris * 3);

		pinmesh = (dmd3mesh_t *)((byte *)pinmesh + LittleLong(pinmesh->meshsize));
	}

	const size_t allocSize = headerSize + frameSize + tagSize + meshSize + skinSize + indexSize + coordSize + vertSize + trNeighborsSize;

	return allocSize;
}

extern void Mod_BuildTriangleNeighbors(maliasmesh_t *mesh); //mxd

void Mod_LoadAliasMD3Model(model_t *mod, void *buffer)
{
	//mxd. Allocate memory
	mod->extradata = ModChunk_Begin(GetAllocSizeMD3(buffer));

	dmd3_t *pinmodel = (dmd3_t *)buffer;
	const int version = LittleLong(pinmodel->version);

	if (version != MD3_ALIAS_VERSION)
		VID_Error(ERR_DROP, "Model %s has wrong version number (%i should be %i)", mod->name, version, MD3_ALIAS_VERSION);

	maliasmodel_t *poutmodel = ModChunk_Alloc(sizeof(maliasmodel_t));

	// Byte-swap the header fields
	poutmodel->num_frames = LittleLong(pinmodel->num_frames);
	poutmodel->num_tags = LittleLong(pinmodel->num_tags);
	poutmodel->num_meshes = LittleLong(pinmodel->num_meshes);

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
	dmd3frame_t *pinframe = (dmd3frame_t *)((byte *)pinmodel + LittleLong(pinmodel->ofs_frames));
	maliasframe_t *poutframe = poutmodel->frames = ModChunk_Alloc(sizeof(maliasframe_t) * poutmodel->num_frames);

	mod->radius = 0;
	ClearBounds(mod->mins, mod->maxs);

	for (int i = 0; i < poutmodel->num_frames; i++, pinframe++, poutframe++)
	{
		for (int j = 0; j < 3; j++)
		{
			poutframe->mins[j] = LittleFloat(pinframe->mins[j]);
			poutframe->maxs[j] = LittleFloat(pinframe->maxs[j]);
			poutframe->scale[j] = MD3_XYZ_SCALE;
			poutframe->translate[j] = LittleFloat(pinframe->translate[j]);
		}

		poutframe->radius = LittleFloat(pinframe->radius);

		mod->radius = max(mod->radius, poutframe->radius);
		AddPointToBounds(poutframe->mins, mod->mins, mod->maxs);
		AddPointToBounds(poutframe->maxs, mod->mins, mod->maxs);
	}

	// Load tags
	dmd3tag_t *pintag = (dmd3tag_t *)((byte *)pinmodel + LittleLong(pinmodel->ofs_tags));
	maliastag_t *pouttag = poutmodel->tags = ModChunk_Alloc(sizeof(maliastag_t) * poutmodel->num_frames * poutmodel->num_tags);

	for (int i = 0; i < poutmodel->num_frames; i++)
	{
		for (int l = 0; l < poutmodel->num_tags; l++, pintag++, pouttag++)
		{
			memcpy(pouttag->name, pintag->name, MD3_MAX_PATH);
			for (int j = 0; j < 3; j++)
			{
				pouttag->orient.origin[j] = LittleFloat(pintag->orient.origin[j]);

				for (int c = 0; c < 3; c++)
					pouttag->orient.axis[c][j] = LittleFloat(pintag->orient.axis[c][j]);
			}
		}
	}

	// Load meshes
	dmd3mesh_t *pinmesh = (dmd3mesh_t *)((byte *)pinmodel + LittleLong(pinmodel->ofs_meshes));
	maliasmesh_t *poutmesh = poutmodel->meshes = ModChunk_Alloc(sizeof(maliasmesh_t)*poutmodel->num_meshes);

	for (int i = 0; i < poutmodel->num_meshes; i++, poutmesh++)
	{
		memcpy(poutmesh->name, pinmesh->name, MD3_MAX_PATH);

		if (strncmp((const char *)pinmesh->id, "IDP3", 4))
			VID_Error(ERR_DROP, "Mesh %s in model %s has wrong id (%i should be %i)", poutmesh->name, mod->name, LittleLong((int)pinmesh->id), IDMD3HEADER);

		poutmesh->num_tris = LittleLong(pinmesh->num_tris);
		poutmesh->num_skins = LittleLong(pinmesh->num_skins);
		poutmesh->num_verts = LittleLong(pinmesh->num_verts);

		if (poutmesh->num_skins <= 0)
			VID_Error(ERR_DROP, "Mesh %i in model %s has no skins", i, mod->name);
		else if (poutmesh->num_skins > MD3_MAX_SHADERS)
			VID_Error(ERR_DROP, "Mesh %i in model %s has too many skins (%i, maximum is %i)", i, mod->name, poutmesh->num_skins, MD3_MAX_SHADERS);

		if (poutmesh->num_tris <= 0)
			VID_Error(ERR_DROP, "Mesh %i in model %s has no triangles", i, mod->name);

		if (poutmesh->num_verts <= 0)
			VID_Error(ERR_DROP, "Mesh %i in model %s has no vertices", i, mod->name);

		// Register all skins
		dmd3skin_t *pinskin = (dmd3skin_t *)((byte *)pinmesh + LittleLong(pinmesh->ofs_skins));
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
		index_t *pinindex = (index_t *)((byte *)pinmesh + LittleLong(pinmesh->ofs_tris));
		index_t *poutindex = poutmesh->indexes = ModChunk_Alloc(sizeof(index_t) * poutmesh->num_tris * 3);

		for (int j = 0; j < poutmesh->num_tris; j++, pinindex += 3, poutindex += 3)
			for (int c = 0; c < 3; c++)
				poutindex[c] = (index_t)LittleLong(pinindex[c]);

		// Load texture coordinates
		dmd3coord_t *pincoord = (dmd3coord_t *)((byte *)pinmesh + LittleLong(pinmesh->ofs_tcs));
		maliascoord_t *poutcoord = poutmesh->stcoords = ModChunk_Alloc(sizeof(maliascoord_t) * poutmesh->num_verts);

		for (int j = 0; j < poutmesh->num_verts; j++, pincoord++, poutcoord++)
		{
			poutcoord->st[0] = LittleFloat(pincoord->st[0]);
			poutcoord->st[1] = LittleFloat(pincoord->st[1]);
		}

		// Load vertices and normals
		dmd3vertex_t *pinvert = (dmd3vertex_t *)((byte *)pinmesh + LittleLong(pinmesh->ofs_verts));
		maliasvertex_t *poutvert = poutmesh->vertexes = ModChunk_Alloc(poutmodel->num_frames * poutmesh->num_verts * sizeof(maliasvertex_t));

		for (int l = 0; l < poutmodel->num_frames; l++)
		{
			for (int j = 0; j < poutmesh->num_verts; j++, pinvert++, poutvert++)
			{
				for (int c = 0; c < 3; c++)
					poutvert->xyz[c] = LittleShort(pinvert->point[c]);

				poutvert->normal[0] = (LittleShort(pinvert->norm) >> 0) & 0xff;
				poutvert->normal[1] = (LittleShort(pinvert->norm) >> 8) & 0xff;

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

		pinmesh = (dmd3mesh_t *)((byte *)pinmesh + LittleLong(pinmesh->meshsize));

		// Build triangle neighbors
		poutmesh->trneighbors = ModChunk_Alloc(sizeof(int) * poutmesh->num_tris * 3);
		Mod_BuildTriangleNeighbors(poutmesh);
	}

	mod->hasAlpha = false;
	Mod_LoadModelScript(mod, poutmodel); // MD3 skin scripting

	mod->type = mod_alias;
}