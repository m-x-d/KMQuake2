/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2019 MaxED

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

// r_screenshot.c - screenshots!

#include "r_local.h"

#define MAX_SCREENSHOTS 1000
#define SCREENSHOT_FORMAT_STRING "%s/screenshots/"ENGINE_PREFIX"%03i.%s"

static qboolean GetScreenshotFilename(char *filename, int size, const char *ext)
{
	for (int i = 0; i < MAX_SCREENSHOTS; i++)
	{
		Com_sprintf(filename, size, SCREENSHOT_FORMAT_STRING, FS_Gamedir(), i, ext);
		FILE *f = fopen(filename, "rb");
		if (!f)
			return true; // File doesn't exist

		fclose(f);
	}

	// No free indices...
	return false;
}

static void R_ScreenShot(qboolean silent)
{
	// Sanity check...
	if (Q_strcasecmp(r_screenshot_format->string, "jpg") &&
		Q_strcasecmp(r_screenshot_format->string, "png") &&
		Q_strcasecmp(r_screenshot_format->string, "tga"))
	{
		Cvar_Set("r_screenshot_format", "png");
	}
	
	// Create the screenshots directory if it doesn't exist
	char checkname[MAX_OSPATH];
	Com_sprintf(checkname, sizeof(checkname), "%s/screenshots", FS_Gamedir());
	Sys_Mkdir(checkname);

	// Find a file name to save to
	char filename[MAX_OSPATH];
	if (!GetScreenshotFilename(filename, sizeof(filename), r_screenshot_format->string))
	{
		VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: Couldn't create a file: too many screenshots (max. is %i)!\n", __func__, MAX_SCREENSHOTS - 1);
		return;
	}

	// Grab pixels
	byte *buffer = malloc(vid.width * vid.height * 3);
	qglPixelStorei(GL_PACK_ALIGNMENT, 1); //mxd. Align to byte instead of word.
	qglReadPixels(0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer);

	// Save the image...
	if (!Q_strcasecmp(r_screenshot_format->string, "jpg"))
	{
		// Check quality...
		if (r_screenshot_jpeg_quality->integer > 100 || r_screenshot_jpeg_quality->integer < 1)
			Cvar_Set("r_screenshot_jpeg_quality", "85");
		
		if (!STBSaveJPG(filename, buffer, vid.width, vid.height, r_screenshot_jpeg_quality->integer))
			VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: Couldn't save a file\n", __func__);
	}
	else if(!Q_strcasecmp(r_screenshot_format->string, "png"))
	{
		if (!STBSavePNG(filename, buffer, vid.width, vid.height))
			VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: Couldn't save a file\n", __func__);
	}
	else // TGA
	{
		if (!STBSaveTGA(filename, buffer, vid.width, vid.height))
			VID_Printf(PRINT_ALL, S_COLOR_YELLOW"%s: Couldn't save a file\n", __func__);
	}

	free(buffer);

	// Done
	if (!silent)
		VID_Printf(PRINT_ALL, "Wrote %s\n", filename);
}

void R_ScreenShot_f(void)
{
	R_ScreenShot(false);
}

void R_ScreenShot_Silent_f(void)
{
	R_ScreenShot(true);
}

byte *saveshotdata;

// By Knightmare. Creates preview screenshot for save/load menus.
void R_ScaledScreenshot(char *filename)
{
	if (!saveshotdata)
		return;

	const int saveshotwidth = vid.width / 2;
	const int saveshotheight = vid.height / 2;

	// Allocate room for reduced screenshot
	byte *jpgdata = malloc(saveshotwidth * saveshotheight * 3);
	if (!jpgdata)
		return;

	//mxd. Resize grabbed screen
	STBResize(saveshotdata, vid.width, vid.height, jpgdata, saveshotwidth, saveshotheight, false);

	// Check filename
	FILE *file = fopen(filename, "wb");
	if (!file)
	{
		VID_Printf(PRINT_ALL, "%s: Couldn't create %s\n", __func__, filename);
		return;
	}
	fclose(file);

	//mxd. Save image
	if (!STBSaveJPG(filename, jpgdata, saveshotwidth, saveshotheight, 95))
		VID_Printf(PRINT_ALL, "%s: Couldn't save %s\n", __func__, filename);

	// Free screenshot data
	free(jpgdata);
}

void R_GrabScreen(void) // Knightmare
{
	// Free saveshot buffer first
	free(saveshotdata);

	// Allocate room for a copy of the framebuffer
	saveshotdata = malloc(vid.width * vid.height * 3);
	if (!saveshotdata)
		return;

	// Read the framebuffer into our storage
	qglPixelStorei(GL_PACK_ALIGNMENT, 1); //mxd. Align to byte instead of word.
	qglReadPixels(0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, saveshotdata);
}