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

// r_misc.c - particle image loading, and screenshots

#include "r_local.h"
#include "../include/jpeg/jpeglib.h" // Heffo - JPEG Screenshots
#ifdef PNG_SUPPORT
#include "../include/zlibpng/png.h"
#endif	// PNG_SUPPORT


/*
==================
R_CreateNullTexture
==================
*/

#define NULLTEX_SIZE 16

byte nulltexture[NULLTEX_SIZE][NULLTEX_SIZE] = //mxd. Lets actually draw some shit :)
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

byte nulltexturecolors[4][4] = //mxd
{
	{ 0, 0, 0, 255 },	   //black
	{ 141, 208, 6, 255 },  //green
	{ 88, 194, 229, 255 }, //blue
	{ 243, 4, 75, 255 },   //red
};

image_t * R_CreateNullTexture (void)
{
	byte nulltex[NULLTEX_SIZE][NULLTEX_SIZE][4];
	memset (nulltex, 32, sizeof(nulltex));

	for (int x = 0; x < NULLTEX_SIZE; x++) //mxd
		for (int y = 0; y < NULLTEX_SIZE; y++)
			for (int c = 0; c < 4; c++)
				nulltex[x][y][c] = nulltexturecolors[nulltexture[x][y]][c];

	return R_LoadPic ("***notexture***", (byte *)nulltex, NULLTEX_SIZE, NULLTEX_SIZE, it_wall, 32);
}


/*
==================
R_InitParticleTexture (mxd. Vanilla particles)
==================
*/
byte dottexture[8][8] =
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

void R_InitParticleTexture(void)
{
	byte	data[8][8][4];

	// particle texture
	for (int x = 0; x < 8; x++)
	{
		for (int y = 0; y < 8; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y] * 255;
		}
	}

	R_LoadPic("***particle***", (byte *)data, 8, 8, it_part, 32);
}


/*
==================
LoadPartImg
==================
*/
image_t *LoadPartImg (char *name, imagetype_t type)
{
	image_t *image = R_FindImage(name, type);
	if (!image) image = glMedia.notexture;
	return image;
}


/*
==================
R_SetParticlePicture
==================
*/
void R_SetParticlePicture (int num, char *name)
{
	glMedia.particletextures[num] = LoadPartImg (name, it_part);
}


/*
==================
R_CreateDisplayLists
==================
*/
void R_CreateDisplayLists (void)
{
	for (int i = 0; i<NUM_DISPLAY_LISTS; i++)
	{
		if (i == 0)
			glMedia.displayLists[i] = qglGenLists(NUM_DISPLAY_LISTS);
		else	
			glMedia.displayLists[i] = glMedia.displayLists[i - 1] + 1;
	}
	
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
		qglColor4f (1,1,1,1);
	qglEndList();
}


/*
==================
R_ClearDisplayLists
==================
*/
void R_ClearDisplayLists (void)
{
	if (glMedia.displayLists[0] != 0)	// clear only if not null
		qglDeleteLists (glMedia.displayLists[0], NUM_DISPLAY_LISTS);
}


/*
==================
R_InitMedia
==================
*/
void R_InitMedia (void)
{
	byte	whitetex[NULLTEX_SIZE][NULLTEX_SIZE][4];
#ifdef ROQ_SUPPORT
	byte	data2D[256 * 256 * 4]; // Raw texture
#endif // ROQ_SUPPORT

	R_InitParticleTexture(); //mxd
	glMedia.notexture = R_CreateNullTexture (); // Generate null texture

	memset (whitetex, 255, sizeof(whitetex));
	glMedia.whitetexture = R_LoadPic ("***whitetexture***", (byte *)whitetex, NULLTEX_SIZE, NULLTEX_SIZE, it_wall, 32);

#ifdef ROQ_SUPPORT
	memset(data2D, 255, 256 * 256 * 4);
	glMedia.rawtexture = R_LoadPic("***rawtexture***", data2D, 256, 256, it_pic, 32);
#endif // ROQ_SUPPORT
	
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
		glMedia.displayLists[x] = 0;	// was NULL

	R_CreateDisplayLists();
	CL_SetParticleImages();
}


/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 

typedef struct _TargaHeader
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/*
================
R_ResampleShotLerpLine
from DarkPlaces
================
*/
void R_ResampleShotLerpLine (byte *in, byte *out, int inwidth, int outwidth) //mxd. Very similar to GL_ResampleTextureLerpLine, except alpha handling
{ 
	int j, f;

	const int fstep = (int)(inwidth * 65536.0f / outwidth);
	const int endx = inwidth - 1; 
	int oldx = 0;

	for (j = 0, f = 0; j < outwidth; j++, f += fstep) 
	{
		const int xi = (int)f >> 16; 
		if (xi != oldx) 
		{ 
			in += (xi - oldx) * 3; 
			oldx = xi; 
		}

		if (xi < endx) 
		{
			const int l2 = f & 0xFFFF; 
			const int l1 = 0x10000 - l2;

			*out++ = (byte)((in[0] * l1 + in[3] * l2) >> 16);
			*out++ = (byte)((in[1] * l1 + in[4] * l2) >> 16); 
			*out++ = (byte)((in[2] * l1 + in[5] * l2) >> 16); 
		} 
		else // last pixel of the line has no pixel to lerp to 
		{ 
			*out++ = in[0]; 
			*out++ = in[1]; 
			*out++ = in[2]; 
		} 
	} 
}


/*
================
R_ResampleShot
================
*/
void R_ResampleShot (void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight) //mxd. Very similar to GL_ResampleTexture, except alpha handling
{ 
	int i, j, f;

	byte *out = outdata;
	const int fstep = (int)(inheight * 65536.0f / outheight); 

	byte *row1 = malloc(outwidth * 3); 
	byte *row2 = malloc(outwidth * 3); 
	byte *inrow = indata; 
	int oldy = 0;
	const int endy = inheight - 1;

	R_ResampleShotLerpLine(inrow, row1, inwidth, outwidth);
	R_ResampleShotLerpLine(inrow + inwidth * 3, row2, inwidth, outwidth);

	for (i = 0, f = 0; i < outheight; i++,f += fstep) 
	{
		const int yi = f >> 16; 
		if (yi != oldy) 
		{ 
			inrow = (byte *)indata + inwidth * 3 * yi;
			if (yi == oldy + 1) 
				memcpy(row1, row2, outwidth * 3); 
			else 
				R_ResampleShotLerpLine(inrow, row1, inwidth, outwidth);

			if (yi < endy) 
				R_ResampleShotLerpLine(inrow + inwidth*3, row2, inwidth, outwidth); 
			else 
				memcpy(row2, row1, outwidth * 3); 
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
			} 

			row1 -= outwidth * 3; 
			row2 -= outwidth * 3; 
		} 
		else // last line has no pixels to lerp to 
		{ 
			for (j = 0; j < outwidth; j++) 
			{ 
				*out++ = *row1++; 
				*out++ = *row1++; 
				*out++ = *row1++; 
			}

			row1 -= outwidth * 3; 
		} 
	}

	free(row1); 
	free(row2); 
} 


/* 
================== 
R_ScaledScreenshot
by Knightmare
================== 
*/

byte *saveshotdata;

void R_ScaledScreenshot (char *name) //TODO: mxd: error handling
{
	struct jpeg_compress_struct		cinfo;
	struct jpeg_error_mgr			jerr;
	JSAMPROW						s[1];
	char							shotname[MAX_OSPATH];

	if (!saveshotdata)	return;

	// Round down width to nearest multiple of 4
	const int grab_width = vid.width & ~3;

	// Optional hi-res saveshots
	int saveshotWidth = 256;
	int saveshotHeight = saveshotWidth;

	if (r_saveshotsize->value)
	{
		if (grab_width >= 1024)
			saveshotWidth = 1024;
		else if (grab_width >= 512)
			saveshotWidth = 512;

		if (vid.height >= 1024)
			saveshotHeight = 1024;
		else if (vid.height >= 512)
			saveshotHeight = 512;
	}

	// Allocate room for reduced screenshot
	byte *jpgdata = malloc(saveshotWidth * saveshotHeight * 3);
	if (!jpgdata)
		return;

	// Resize grabbed screen
	R_ResampleShot(saveshotdata, grab_width, vid.height, jpgdata, saveshotWidth, saveshotHeight);

	// Open the file for Binary Output
	Com_sprintf(shotname, sizeof(shotname), "%s", name);
	FILE *file = fopen(shotname, "wb");
	if (!file)
	{
		VID_Printf(PRINT_ALL, "Menu_ScreenShot: Couldn't create %s\n", name); 
		return;
 	}

	// Initialise the JPEG compression object
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, file);

	// Setup JPEG Parameters
	cinfo.image_width = saveshotWidth; //256;
	cinfo.image_height = saveshotHeight; //256;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 85, TRUE); // was 100

	// Start Compression
	jpeg_start_compress(&cinfo, true);

	// Feed Scanline data
	const int offset = (cinfo.image_width * cinfo.image_height * 3) - (cinfo.image_width * 3);
	while (cinfo.next_scanline < cinfo.image_height)
	{
		s[0] = &jpgdata[offset - (cinfo.next_scanline * (cinfo.image_width * 3))];
		jpeg_write_scanlines(&cinfo, s, 1);
	}

	// Finish Compression
	jpeg_finish_compress(&cinfo);

	// Destroy JPEG object
	jpeg_destroy_compress(&cinfo);

	// Close File
	fclose(file);

	// Free Reduced screenshot
	free(jpgdata);
}


/* 
================== 
R_GrabScreen
by Knightmare
================== 
*/
void R_GrabScreen (void)
{
	// Free saveshot buffer first
	if (saveshotdata)
		free(saveshotdata);

	// Round down width to nearest multiple of 4
	const int grab_width = vid.width & ~3;
	const int grab_x = (vid.width - grab_width) / 2;

	// Allocate room for a copy of the framebuffer
	saveshotdata = malloc(grab_width * vid.height * 3);
	if (!saveshotdata)
		return;

	// Read the framebuffer into our storage
	qglReadPixels(grab_x, 0, grab_width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, saveshotdata);
}


/* 
================== 
R_ScreenShot_JPG
By Robert 'Heffo' Heffernan
================== 
*/
void R_ScreenShot_JPG (qboolean silent) //TODO: mxd: error handling
{
	struct jpeg_compress_struct		cinfo;
	struct jpeg_error_mgr			jerr;
	JSAMPROW						s[1];
	FILE							*file;
	char							picname[80], checkname[MAX_OSPATH];
	int								i;

	// Create the scrnshots directory if it doesn't exist
	Com_sprintf(checkname, sizeof(checkname), "%s/scrnshot", FS_Gamedir());
	Sys_Mkdir (checkname);

	// Knightmare- changed screenshot filenames, up to 1000 screenies
	// Find a file name to save it to 
	//Q_strncpyz(picname, "quake00.jpg", sizeof(picname));

	for (i=0; i<=999; i++) 
	{ 
		int one, ten, hundred;

		hundred = i*0.01;
		ten = (i - hundred*100)*0.1;
		one = i - hundred*100 - ten*10;

		Com_sprintf(picname, sizeof(picname), "kmquake2_%i%i%i.jpg", hundred, ten, one);
		Com_sprintf(checkname, sizeof(checkname), "%s/scrnshot/%s", FS_Gamedir(), picname);
		file = fopen (checkname, "rb");
		if (!file)
			break;	// file doesn't exist
		fclose (file);
	} 
	if (i==1000) 
	{
		VID_Printf(PRINT_ALL, "R_ScreenShot_JPG: Couldn't create a file\n"); 
		return;
 	}

	// Open the file for Binary Output
	file = fopen(checkname, "wb");
	if(!file)
	{
		VID_Printf(PRINT_ALL, "R_ScreenShot_JPG: Couldn't create a file\n"); 
		return;
 	}

	// Round down width to nearest multiple of 4
	const int grab_width = vid.width & ~3;
	const int grab_x = (vid.width - grab_width) / 2;

	// Allocate room for a copy of the framebuffer
	byte *rgbdata = malloc(grab_width * vid.height * 3);
	if(!rgbdata)
	{
		fclose(file);
		return;
	}

	// Read the framebuffer into our storage
	qglReadPixels(grab_x, 0, grab_width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, rgbdata);

	// Initialise the JPEG compression object
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, file);

	// Setup JPEG Parameters
	cinfo.image_width = grab_width;
	cinfo.image_height = vid.height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;
	jpeg_set_defaults(&cinfo);
	if ((r_screenshot_jpeg_quality->value >= 101) || (r_screenshot_jpeg_quality->value <= 0))
		Cvar_Set("r_screenshot_jpeg_quality", "85");
	jpeg_set_quality(&cinfo, r_screenshot_jpeg_quality->value, TRUE);

	// Start Compression
	jpeg_start_compress(&cinfo, true);

	// Feed Scanline data
	const int offset = (cinfo.image_width * cinfo.image_height * 3) - (cinfo.image_width * 3);
	while(cinfo.next_scanline < cinfo.image_height)
	{
		s[0] = &rgbdata[offset - (cinfo.next_scanline * (cinfo.image_width * 3))];
		jpeg_write_scanlines(&cinfo, s, 1);
	}

	// Finish Compression
	jpeg_finish_compress(&cinfo);

	// Destroy JPEG object
	jpeg_destroy_compress(&cinfo);

	// Close File
	fclose(file);

	// Free Temp Framebuffer
	free(rgbdata);

	// Done!
	if (!silent)
		VID_Printf(PRINT_ALL, "Wrote %s\n", picname);
}


#ifdef PNG_SUPPORT
/* 
================== 
R_ScreenShot_PNG
================== 
*/
void R_ScreenShot_PNG (qboolean silent)
{
	char		picname[80], checkname[MAX_OSPATH];
	int			i, grab_width, grab_x;
	byte		*rgbdata;
	FILE		*file;

	// create the scrnshots directory if it doesn't exist
	Com_sprintf(checkname, sizeof(checkname), "%s/scrnshot", FS_Gamedir());
	Sys_Mkdir (checkname);

// 
// find a file name to save it to 
// 

	// Knightmare- changed screenshot filenames, up to 100 screenies
	//Q_strncpyz(picname, "quake00.png", sizeof(picname));

	for (i=0; i<=999; i++) 
	{ 
		//picname[5] = i/10 + '0'; 
		//picname[6] = i%10 + '0'; 
		int one, ten, hundred;

		hundred = i*0.01;
		ten = (i - hundred*100)*0.1;
		one = i - hundred*100 - ten*10;

		Com_sprintf(picname, sizeof(picname), "kmquake2_%i%i%i.png", hundred, ten, one);
		Com_sprintf(checkname, sizeof(checkname), "%s/scrnshot/%s", FS_Gamedir(), picname);
		file = fopen (checkname, "rb");
		if (!file)
			break;	// file doesn't exist
		fclose (file);
	} 
	if (i==1000) 
	{
		VID_Printf(PRINT_ALL, "R_ScreenShot_PNG: Couldn't create a file\n"); 
		return;
 	}

	// Round down width to nearest multiple of 4
	grab_width = vid.width & ~3;
	grab_x = (vid.width - grab_width) / 2;

	// Allocate room for a copy of the framebuffer
	rgbdata = malloc(grab_width * vid.height * 3);
	if(!rgbdata)
	{
		return;
	}

	// Read the framebuffer into our storage
	qglReadPixels(grab_x, 0, grab_width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, rgbdata);

	png_structp png_sptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (!png_sptr)
	{
		free(rgbdata);
		VID_Printf(PRINT_ALL, "R_ScreenShot_PNG: Couldn't create PNG struct\n"); 
		return;
	}

	png_infop png_infoptr = png_create_info_struct(png_sptr);
	if (!png_infoptr)
	{
		png_destroy_write_struct(&png_sptr, 0);
		free(rgbdata);
		VID_Printf(PRINT_ALL, "R_ScreenShot_PNG: Couldn't create info struct\n"); 
		return;
	}

	if (setjmp(png_sptr->jmpbuf))
	{
		png_destroy_info_struct(png_sptr, &png_infoptr);
		png_destroy_write_struct(&png_sptr, 0);
		free(rgbdata);
		VID_Printf(PRINT_ALL, "R_ScreenShot_PNG: bad data\n"); 
		return;
	}

	// open png file
	file = fopen(checkname, "wb");
	if(!file)
	{
		png_destroy_info_struct(png_sptr, &png_infoptr);
		png_destroy_write_struct (&png_sptr, 0);
		free(rgbdata);
		VID_Printf(PRINT_ALL, "R_ScreenShot_PNG: Couldn't create a file\n"); 
		return;
 	}

	// encode and output
	png_init_io(png_sptr, file);
	png_set_IHDR(png_sptr, png_infoptr, grab_width, vid.height, 8,
		PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(png_sptr, png_infoptr);
	for (i = vid.height - 1; i >= 0; i--)
	{
		void *lineptr = rgbdata + i * grab_width * 3;
		png_write_row(png_sptr, lineptr);
	}
	png_write_end(png_sptr, png_infoptr);

	// clean up
	fclose(file);
	png_destroy_info_struct(png_sptr, &png_infoptr);
	png_destroy_write_struct(&png_sptr, 0);
	free(rgbdata);

	if (!silent)
		VID_Printf(PRINT_ALL, "Wrote %s\n", picname);
}
#endif	// PNG_SUPPORT


/* 
================== 
R_ScreenShot_TGA
================== 
*/  
void R_ScreenShot_TGA (qboolean silent) 
{
	char picname[80]; 
	char checkname[MAX_OSPATH];
	int  i;
	FILE *f;

	// create the scrnshots directory if it doesn't exist
	Com_sprintf(checkname, sizeof(checkname), "%s/scrnshot", FS_Gamedir());
	Sys_Mkdir(checkname);

// 
// find a file name to save it to 
// 

	// Knightmare- changed screenshot filenames, up to 100 screenies
	for (i = 0; i <= 999; i++) 
	{
		const int hundred = i * 0.01;
		const int ten = (i - hundred * 100)*0.1;
		const int one = i - hundred * 100 - ten * 10;

		Com_sprintf(picname, sizeof(picname), "kmquake2_%i%i%i.tga", hundred, ten, one);
		Com_sprintf(checkname, sizeof(checkname), "%s/scrnshot/%s", FS_Gamedir(), picname);
		f = fopen(checkname, "rb");
		if (!f)
			break;	// file doesn't exist

		fclose(f);
	} 

	if (i == 1000) 
	{
		VID_Printf(PRINT_ALL, "R_ScreenShot_TGA: Couldn't create a file\n"); 
		return;
 	}

	// Round down width to nearest multiple of 4
	const int grab_width = vid.width & ~3;
	const int grab_x = (vid.width - grab_width) / 2;

	byte *buffer = malloc(grab_width * vid.height * 3 + 18);
	memset(buffer, 0, 18);
	buffer[2] = 2; // uncompressed type
	buffer[12] = grab_width & 255;
	buffer[13] = grab_width >> 8;
	buffer[14] = vid.height & 255;
	buffer[15] = vid.height >> 8;
	buffer[16] = 24;	// pixel size

	qglReadPixels(grab_x, 0, grab_width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18); 

	// swap rgb to bgr
	const int c = 18 + grab_width * vid.height * 3;
	for (i = 18; i < c; i += 3)
	{
		const int temp = buffer[i];
		buffer[i] = buffer[i + 2];
		buffer[i + 2] = temp;
	}

	f = fopen(checkname, "wb");
	fwrite(buffer, 1, c, f);
	fclose(f);

	free(buffer);

	if (!silent)
		VID_Printf(PRINT_ALL, "Wrote %s\n", picname);
}


/*
==================
R_ScreenShot (mxd)
==================
*/

void R_ScreenShot (qboolean silent)
{
	if (!Q_strcasecmp(r_screenshot_format->string, "jpg"))
		R_ScreenShot_JPG(silent);
#ifdef PNG_SUPPORT
	else if (!Q_strcasecmp(r_screenshot_format->string, "png"))
		R_ScreenShot_PNG(silent);
#endif	// PNG_SUPPORT
	else
		R_ScreenShot_TGA(silent);
}


/* 
================== 
R_ScreenShot_f
================== 
*/  
void R_ScreenShot_f (void)
{
	R_ScreenShot(false); //mxd
}


/* 
================== 
R_ScreenShot_Silent_f
================== 
*/  
void R_ScreenShot_Silent_f (void)
{
	R_ScreenShot(true); //mxd
}


/* 
================== 
R_ScreenShot_TGA_f
================== 
*/  
void R_ScreenShot_TGA_f (void)
{
	R_ScreenShot_TGA(false);
}


/* 
================== 
R_ScreenShot_TGA_f
================== 
*/  
void R_ScreenShot_JPG_f (void)
{
	R_ScreenShot_JPG(false);
}


/* 
================== 
R_ScreenShot_PNG_f
================== 
*/  
void R_ScreenShot_PNG_f (void)
{
	R_ScreenShot_PNG(false);
}

//============================================================================== 


/*
=================
GL_UpdateSwapInterval
=================
*/
void GL_UpdateSwapInterval (void)
{
	static qboolean registering;

	// don't swap interval if loading a map
	if (registering != registration_active)
		r_swapinterval->modified = true;

	if (r_swapinterval->modified)
	{
		r_swapinterval->modified = false;
		registering = registration_active;

		if (!glState.stereo_enabled) 
		{
#ifdef _WIN32
			if (qwglSwapIntervalEXT)
				qwglSwapIntervalEXT(registration_active ? 0 : r_swapinterval->value);
#endif
		}
	}
}
