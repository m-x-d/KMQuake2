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
// r_model_md2.c -- md2 model loading

#include "r_local.h"

static int *md2IndRemap;
static uint *md2TempIndex;
static uint *md2TempStIndex;

#pragma region ======================= Helper methods

static int FindTriangleWithEdge(maliasmesh_t *mesh, index_t p1, index_t p2, int ignore)
{
	int count = 0;
	int match = -1;

	index_t *indexes = mesh->indexes;
	for (int i = 0; i < mesh->num_tris; i++, indexes += 3)
	{
		if ((indexes[0] == p2 && indexes[1] == p1)
			|| (indexes[1] == p2 && indexes[2] == p1)
			|| (indexes[2] == p2 && indexes[0] == p1))
		{
			if (i != ignore)
				match = i;

			count++;
		}
		else if ((indexes[0] == p1 && indexes[1] == p2)
			|| (indexes[1] == p1 && indexes[2] == p2)
			|| (indexes[2] == p1 && indexes[0] == p2))
		{
			count++;
		}
	}

	// Detect edges shared by three triangles and make them seams
	if (count > 2)
		match = -1;

	return match;
}

void Mod_BuildTriangleNeighbors(maliasmesh_t *mesh)
{
	int		i, *n;
	index_t	*index;

	for (i = 0, n = mesh->trneighbors, index = mesh->indexes; i < mesh->num_tris; i++, n += 3, index += 3)
	{
		n[0] = FindTriangleWithEdge(mesh, index[0], index[1], i);
		n[1] = FindTriangleWithEdge(mesh, index[1], index[2], i);
		n[2] = FindTriangleWithEdge(mesh, index[2], index[0], i);
	}
}

static void NormalToLatLong(const vec3_t normal, byte bytes[2])
{
	if (normal[0] == 0 && normal[1] == 0)
	{
		if (normal[2] > 0)
		{
			// Lattitude = 0, Longitude = 0
			bytes[0] = 0;
			bytes[1] = 0;
		}
		else
		{
			// Lattitude = 0, Longitude = 128
			bytes[0] = 128;
			bytes[1] = 0;
		}
	}
	else
	{
		const int lat = RAD2DEG(atan2(normal[1], normal[0])) * (255.0 / 360.0);
		const int lng = RAD2DEG(acos(normal[2])) * (255.0 / 360.0);

		bytes[0] = lng & 0xff;
		bytes[1] = lat & 0xff;
	}
}

#pragma endregion

// Calc exact alloc size for MD2 in memory
static size_t GetAllocSizeMD2(void *buffer)
{
	dmdl_t *pinmodel = (dmdl_t *)buffer;

	// Count unique verts
	int num_verts = 0;
	const int num_index = pinmodel->num_tris * 3;
	dtriangle_t *pintri = (dtriangle_t *)((byte *)pinmodel + pinmodel->ofs_tris);

	//mxd. (re)allocate remap arrays...
	static int remaparraysize = 0;
	const int newremaparraysize = pinmodel->num_tris * 3 * sizeof(int);

	if (newremaparraysize > remaparraysize)
	{
		remaparraysize = newremaparraysize;

		free(md2IndRemap);
		free(md2TempIndex);
		free(md2TempStIndex);

		md2IndRemap = malloc(remaparraysize);
		md2TempIndex = malloc(remaparraysize);
		md2TempStIndex = malloc(remaparraysize);
	}

	for (int i = 0; i < pinmodel->num_tris; i++)
	{
		for (int c = 0; c < 3; c++)
		{
			md2TempIndex[i * 3 + c] = (uint)pintri[i].index_xyz[c];
			md2TempStIndex[i * 3 + c] = (uint)pintri[i].index_st[c];
		}
	}

	memset(md2IndRemap, -1, remaparraysize);

	for (int i = 0; i < num_index; i++)
	{
		if (md2IndRemap[i] != -1)
			continue;

		for (int j = 0; j < num_index; j++)
			if (i != j && md2TempIndex[j] == md2TempIndex[i] && md2TempStIndex[j] == md2TempStIndex[i])
				md2IndRemap[j] = i;
	}

	for (int i = 0; i < num_index; i++)
		if (md2IndRemap[i] == -1)
			num_verts++;

	const int num_skins = max(pinmodel->num_skins, 1); // Hack because player models have no skin refs

	// Calc sizes rounded to cacheline
	uint totalsize = ALIGN_TO_CACHELINE(sizeof(maliasmodel_t));
	totalsize += ALIGN_TO_CACHELINE(sizeof(maliasmesh_t));
	totalsize += ALIGN_TO_CACHELINE(sizeof(index_t) * pinmodel->num_tris * 3);
	totalsize += ALIGN_TO_CACHELINE(sizeof(maliascoord_t) * num_verts);
	totalsize += ALIGN_TO_CACHELINE(sizeof(maliasframe_t) * pinmodel->num_frames);
	totalsize += ALIGN_TO_CACHELINE(pinmodel->num_frames * num_verts * sizeof(maliasvertex_t));
	totalsize += ALIGN_TO_CACHELINE(sizeof(int) * pinmodel->num_tris * 3);
	totalsize += ALIGN_TO_CACHELINE(sizeof(maliasskin_t) * num_skins);

	return totalsize;
}

// Based on md2 loading code in Q2E 0.40
void Mod_LoadAliasMD2Model(model_t *mod, void *buffer)
{
	//mxd. Allocate memory
	mod->extradata = ModChunk_Begin(GetAllocSizeMD2(buffer));

	dmdl_t *pinmodel = (dmdl_t *)buffer;
	maliasmodel_t *poutmodel = ModChunk_Alloc(sizeof(maliasmodel_t));

	// Sanity checks
	if (pinmodel->version != ALIAS_VERSION)
		VID_Error(ERR_DROP, "Model %s has wrong version number (%i should be %i)", mod->name, pinmodel->version, ALIAS_VERSION);

	poutmodel->num_frames = pinmodel->num_frames;
	if (poutmodel->num_frames <= 0)
		VID_Error(ERR_DROP, "Model %s has invalid number of frames (%i)", mod->name, poutmodel->num_frames);
	else if (poutmodel->num_frames > MAX_FRAMES)
		VID_Error(ERR_DROP, "Model %s has too many frames (%i, maximum is %i)", mod->name, poutmodel->num_frames, MAX_FRAMES);

	poutmodel->num_tags = 0;
	poutmodel->num_meshes = 1;

	const double skinWidth = 1.0 / (double)pinmodel->skinwidth;
	const double skinHeight = 1.0 / (double)pinmodel->skinheight;

	// Load mesh info
	maliasmesh_t *poutmesh = poutmodel->meshes = ModChunk_Alloc(sizeof(maliasmesh_t));

	Com_sprintf(poutmesh->name, sizeof(poutmesh->name), "md2mesh"); // Mesh name in script must match this

	poutmesh->num_tris = pinmodel->num_tris;
	if (poutmesh->num_tris <= 0)
		VID_Error(ERR_DROP, "Model %s has invalid number of triangles (%i)", mod->name, poutmesh->num_tris);

	poutmesh->num_verts = pinmodel->num_xyz;
	if (poutmesh->num_verts <= 0)
		VID_Error(ERR_DROP, "Model %s has invalid number of vertices (%i)", mod->name, poutmesh->num_verts);

	poutmesh->num_skins = pinmodel->num_skins;
	if (poutmesh->num_skins < 0)
		Com_Error(ERR_DROP, "Model %s has invalid number of skins (%i)", mod->name, poutmesh->num_skins);
	else if (poutmesh->num_skins > MAX_MD2SKINS)
		Com_Error(ERR_DROP, "Model %s has too many skins (%i, maximum is %i)", mod->name, poutmesh->num_skins, MAX_MD2SKINS);

	// Build the list of unique vertices
	const int numIndices = poutmesh->num_tris * 3;
	int numVertices = 0;

	index_t *poutindex = poutmesh->indexes = ModChunk_Alloc(sizeof(index_t) * poutmesh->num_tris * 3);

	// Count unique vertices
	for (int i = 0; i < numIndices; i++)
	{
		if (md2IndRemap[i] != -1)
			continue;

		poutindex[i] = numVertices++;
		md2IndRemap[i] = i;
	}

	poutmesh->num_verts = numVertices;

	// Remap remaining indices
	for (int i = 0; i < numIndices; i++)
		if (md2IndRemap[i] != i)
			poutindex[i] = poutindex[md2IndRemap[i]];

	// Load base S and T vertices
	dstvert_t *pincoord = (dstvert_t *)((byte *)pinmodel + pinmodel->ofs_st);
	maliascoord_t *poutcoord = poutmesh->stcoords = ModChunk_Alloc(sizeof(maliascoord_t) * poutmesh->num_verts);

	for (int i = 0; i < numIndices; i++)
	{
		poutcoord[poutindex[i]].st[0] = (float)(((double)pincoord[md2TempStIndex[md2IndRemap[i]]].s + 0.5f) * skinWidth);
		poutcoord[poutindex[i]].st[1] = (float)(((double)pincoord[md2TempStIndex[md2IndRemap[i]]].t + 0.5f) * skinHeight);
	}

	// Load frames
	maliasframe_t *poutframe = poutmodel->frames = ModChunk_Alloc(sizeof(maliasframe_t) * poutmodel->num_frames);
	maliasvertex_t *poutvert = poutmesh->vertexes = ModChunk_Alloc(poutmodel->num_frames * poutmesh->num_verts * sizeof(maliasvertex_t));

	mod->radius = 0;
	ClearBounds(mod->mins, mod->maxs);

	for (int i = 0; i < poutmodel->num_frames; i++, poutframe++, poutvert += numVertices)
	{
		daliasframe_t *pinframe = (daliasframe_t *)((byte *)pinmodel + pinmodel->ofs_frames + i * pinmodel->framesize);

		for (int c = 0; c < 3; c++)
		{
			poutframe->scale[c] = pinframe->scale[c];
			poutframe->translate[c] = pinframe->translate[c];
		}

		VectorCopy(poutframe->translate, poutframe->mins);
		VectorMA(poutframe->translate, 255, poutframe->scale, poutframe->maxs);

		poutframe->radius = Mod_RadiusFromBounds(poutframe->mins, poutframe->maxs);

		mod->radius = max(mod->radius, poutframe->radius);
		AddPointToBounds(poutframe->mins, mod->mins, mod->maxs);
		AddPointToBounds(poutframe->maxs, mod->mins, mod->maxs);

		// Load vertices and normals
		for (int j = 0; j < numIndices; j++)
		{
			for (int c = 0; c < 3; c++)
				poutvert[poutindex[j]].xyz[c] = (short)pinframe->verts[md2TempIndex[md2IndRemap[j]]].v[c];

			poutvert[poutindex[j]].lightnormalindex = pinframe->verts[md2TempIndex[md2IndRemap[j]]].lightnormalindex;

			vec3_t normal;
			for (int c = 0; c < 3; c++)
				normal[c] = vertexnormals[poutvert[poutindex[j]].lightnormalindex][c];

			NormalToLatLong(normal, poutvert[poutindex[j]].normal);
		}
	}

	// Build triangle neighbors
	poutmesh->trneighbors = ModChunk_Alloc(sizeof(int) * poutmesh->num_tris * 3);
	Mod_BuildTriangleNeighbors(poutmesh);

	// Register all skins
	char name[MD3_MAX_PATH];
	if (poutmesh->num_skins <= 0) // Hack for player models with no skin refs
	{
		maliasskin_t *poutskin = poutmesh->skins = ModChunk_Alloc(sizeof(maliasskin_t));
		poutmesh->num_skins = 1;
		Com_sprintf(name, sizeof(name), "players/male/grunt.pcx");
		memcpy(poutskin->name, name, MD3_MAX_PATH);
		Com_sprintf(poutskin->glowname, sizeof(poutskin->glowname), "\0"); // Set null glowskin
		mod->skins[0][0] = R_FindImage(name, it_skin, false);
	}
	else
	{
		maliasskin_t *poutskin = poutmesh->skins = ModChunk_Alloc(sizeof(maliasskin_t) * poutmesh->num_skins);
		for (int i = 0; i < poutmesh->num_skins; i++, poutskin++)
		{
			memcpy(name, ((char *)pinmodel + pinmodel->ofs_skins + i * MAX_SKINNAME), MD3_MAX_PATH);
			memcpy(poutskin->name, name, MD3_MAX_PATH);
			Com_sprintf(poutskin->glowname, sizeof(poutskin->glowname), "\0"); // Set null glowskin
			mod->skins[0][i] = R_FindImage(name, it_skin, false);
		}
	}

	mod->hasAlpha = false;
	Mod_LoadModelScript(mod, poutmodel); // MD3 skin scripting

	mod->type = mod_md3;
}