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
// r_particle_setup.c - particle image loading

#include "r_local.h"

#define NULLTEX_SIZE	16

#define RAWTEX_WIDTH	320 //mxd
#define RAWTEX_HEIGHT	240 //mxd

image_t *R_CreateNullTexture(void)
{
	const static byte nulltexture[NULLTEX_SIZE][NULLTEX_SIZE] = //mxd. Color indices
	{
		{ 1,1,0,0,0,0,0,0,0,0,0,0,0,0,2,2 },
		{ 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2 },
		{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
		{ 0,0,0,3,3,0,0,0,0,0,0,3,3,0,0,0 },
		{ 0,0,0,3,3,3,0,0,0,0,3,3,3,0,0,0 },
		{ 0,0,0,0,3,3,3,0,0,3,3,3,0,0,0,0 },
		{ 0,0,0,0,0,3,3,3,3,3,3,0,0,0,0,0 },
		{ 0,0,0,0,0,0,3,3,3,3,0,0,0,0,0,0 },
		{ 0,0,0,0,0,0,3,3,3,3,0,0,0,0,0,0 },
		{ 0,0,0,0,0,3,3,3,3,3,3,0,0,0,0,0 },
		{ 0,0,0,0,3,3,3,0,0,3,3,3,0,0,0,0 },
		{ 0,0,0,3,3,3,0,0,0,0,3,3,3,0,0,0 },
		{ 0,0,0,3,3,0,0,0,0,0,0,3,3,0,0,0 },
		{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
		{ 2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 },
		{ 2,2,0,0,0,0,0,0,0,0,0,0,0,0,1,1 },
	};

	const static byte nulltexturecolors[4][4] = //mxd. Colors
	{
		{ 0, 0, 0, 255 },	   //black
		{ 141, 208, 6, 255 },  //green
		{ 88, 194, 229, 255 }, //blue
		{ 243, 4, 75, 255 },   //red
	};
	
	byte nulltex[NULLTEX_SIZE][NULLTEX_SIZE][4]; // Image RGBA bytes

	for (int x = 0; x < NULLTEX_SIZE; x++) //mxd
		for (int y = 0; y < NULLTEX_SIZE; y++)
			memcpy(nulltex[x][y], nulltexturecolors[nulltexture[x][y]], 4);

	return R_LoadPic("***notexture***", (byte *)nulltex, NULLTEX_SIZE, NULLTEX_SIZE, it_wall, 32);
}

// mxd. Vanilla particles
void R_InitParticleTexture(void)
{
	#define DOTTEX_SIZE 8
	
	const static byte dottexture[DOTTEX_SIZE][DOTTEX_SIZE] = // Alpha channel
	{
		{ 0,0,0,0,0,0,0,0 },
		{ 0,0,1,1,0,0,0,0 },
		{ 0,1,1,1,1,0,0,0 },
		{ 0,1,1,1,1,0,0,0 },
		{ 0,0,1,1,0,0,0,0 },
		{ 0,0,0,0,0,0,0,0 },
		{ 0,0,0,0,0,0,0,0 },
		{ 0,0,0,0,0,0,0,0 },
	};
	
	byte data[DOTTEX_SIZE][DOTTEX_SIZE][4]; // Image RGBA bytes
	memset(data, 255, sizeof(data)); // Pre-set all channels to white

	// Apply alpha
	for (int x = 0; x < DOTTEX_SIZE; x++)
		for (int y = 0; y < DOTTEX_SIZE; y++)
			data[y][x][3] = dottexture[x][y] * 255;

	R_LoadPic("***particle***", (byte *)data, DOTTEX_SIZE, DOTTEX_SIZE, it_part, 32);
}

image_t *LoadPartImg(char *name, imagetype_t type)
{
	image_t *image = R_FindImage(name, type, false);
	return (image ? image : glMedia.notexture);
}

void R_SetParticlePicture(int num, char *name)
{
	glMedia.particletextures[num] = LoadPartImg(name, it_part);
}

void R_CreateDisplayLists(void)
{
	glMedia.displayLists[0] = qglGenLists(NUM_DISPLAY_LISTS);
	for (int i = 1; i < NUM_DISPLAY_LISTS; i++)
		glMedia.displayLists[i] = glMedia.displayLists[i - 1] + 1;
	
	qglNewList(glMedia.displayLists[DL_NULLMODEL1], GL_COMPILE);
		qglBegin(GL_TRIANGLE_FAN);
		qglVertex3f(0, 0, -16);
		for(int i = 0; i < 5; i++) //mxd
			qglVertex3f(16 * cos(i * M_PIHALF), 16 * sin(i * M_PIHALF), 0);
		qglEnd();

		qglBegin(GL_TRIANGLE_FAN);
		qglVertex3f(0, 0, 16);
		for (int i = 4; i > -1; i--) //mxd
			qglVertex3f(16 * cos(i * M_PIHALF), 16 * sin(i * M_PIHALF), 0);
		qglEnd();
		qglColor4f(1, 1, 1, 1);
	qglEndList();

	qglNewList(glMedia.displayLists[DL_NULLMODEL2], GL_COMPILE);
		qglLineWidth(6.0f);
		qglBegin(GL_LINES);

		qglColor4ub(255, 0, 0, 255);
		qglVertex3f(0, 0, 0);
		qglVertex3f(16, 0, 0);

		qglColor4ub(64, 0, 0, 255);
		qglVertex3f(0, 0, 0);
		qglVertex3f(-16, 0, 0);

		qglColor4ub(0, 255, 0, 255);
		qglVertex3f(0, 0, 0);
		qglVertex3f(0, 16, 0);

		qglColor4ub(0, 64, 0, 255);
		qglVertex3f(0, 0, 0);
		qglVertex3f(0, -16, 0);

		qglColor4ub(0, 0, 255, 255);
		qglVertex3f(0, 0, 0);
		qglVertex3f(0, 0, 16);

		qglColor4ub(0, 0, 64, 255);
		qglVertex3f(0, 0, 0);
		qglVertex3f(0, 0, -16);

		qglEnd();
		qglLineWidth(1.0f);
		qglColor4f(1,1,1,1);
	qglEndList();
}

void R_ClearDisplayLists(void)
{
	if (glMedia.displayLists[0] != 0) // clear only if not null
		qglDeleteLists(glMedia.displayLists[0], NUM_DISPLAY_LISTS);
}

void R_InitMedia(void)
{
	R_InitParticleTexture(); //mxd
	glMedia.notexture = R_CreateNullTexture(); // Generate null texture

	byte whitetex[NULLTEX_SIZE * NULLTEX_SIZE * 4];
	memset(whitetex, 255, sizeof(whitetex));
	glMedia.whitetexture = R_LoadPic("***whitetexture***", whitetex, NULLTEX_SIZE, NULLTEX_SIZE, it_wall, 32);

	byte rawtex[RAWTEX_WIDTH * RAWTEX_HEIGHT * 4]; // ROQ support //mxd. 256x256 -> 320x240
	memset(rawtex, 255, sizeof(rawtex));
	glMedia.rawtexture = R_LoadPic("***rawtexture***", rawtex, RAWTEX_WIDTH, RAWTEX_HEIGHT, it_pic, 32);
	
	glMedia.envmappic =		  LoadPartImg("gfx/effects/envmap.tga", it_wall);
	glMedia.spheremappic =	  LoadPartImg("gfx/effects/spheremap.tga", it_skin);
	glMedia.shelltexture =	  LoadPartImg("gfx/effects/shell_generic.tga", it_skin);
	glMedia.causticwaterpic = LoadPartImg("gfx/water/caustic_water.tga", it_wall);
	glMedia.causticslimepic = LoadPartImg("gfx/water/caustic_slime.tga", it_wall);
	glMedia.causticlavapic =  LoadPartImg("gfx/water/caustic_lava.tga", it_wall);
	glMedia.particlebeam =	  LoadPartImg("gfx/particles/beam.tga", it_part);

	// Psychospaz's enhanced particles
	for (int x = 0; x < PARTICLE_TYPES; x++)
		glMedia.particletextures[x] = NULL;

	for (int x = 0; x < NUM_DISPLAY_LISTS; x++) 
		glMedia.displayLists[x] = 0; // was NULL

	R_CreateDisplayLists();
	CL_SetParticleImages();
}