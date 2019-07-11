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

// ui_video.c -- the video options menu

#include "../client/client.h"
#include "ui_local.h"

extern cvar_t *vid_ref;

static menuframework_s	s_video_menu;
static menulist_s		s_mode_list;
static menuseparator_s	s_customwidth_title;
static menuseparator_s	s_customheight_title;
static menufield_s		s_customwidth_field;
static menufield_s		s_customheight_field;
static menulist_s		s_fs_box;
static menuslider_s		s_brightness_slider;
static menulist_s		s_texfilter_box;
static menulist_s		s_aniso_box;
static menulist_s		s_vsync_box;
static menulist_s		s_adjust_fov_box;
static menulist_s		s_async_box;

static menuaction_s		s_advanced_action;
static menuaction_s		s_defaults_action;
static menuaction_s		s_apply_action;
static menuaction_s		s_backmain_action;

#pragma region ======================= Menu item callbacks

static void VidModeCallback(void *unused)
{
	s_customwidth_title.generic.flags	= (s_mode_list.curvalue != 0) ? QMF_HIDDEN : 0;
	s_customwidth_field.generic.flags	= (s_mode_list.curvalue != 0) ? (QMF_NUMBERSONLY | QMF_HIDDEN) : QMF_NUMBERSONLY;
	s_customheight_title.generic.flags	= (s_mode_list.curvalue != 0) ? QMF_HIDDEN : 0;
	s_customheight_field.generic.flags	= (s_mode_list.curvalue != 0) ? (QMF_NUMBERSONLY | QMF_HIDDEN) : QMF_NUMBERSONLY;
}

static void BrightnessCallback(void *s)
{
	Cvar_SetValue("vid_gamma", s_brightness_slider.curvalue / 10.0f);
}

static void AnisotropicCallback(void *s) //mxd
{
	menulist_s *list = (menulist_s *)s;
	Cvar_SetValue("r_anisotropic", (list->curvalue == 0 ? 0 : pow(2, list->curvalue)));
}

static void TextureFilterCallback(void *unused) //mxd
{
	switch ((int)s_texfilter_box.curvalue)
	{
		case 0:  Cvar_Set("r_texturemode", "GL_NEAREST_MIPMAP_LINEAR"); break;
		case 1:  Cvar_Set("r_texturemode", "GL_LINEAR_MIPMAP_NEAREST"); break;
		case 2:
		default: Cvar_Set("r_texturemode", "GL_LINEAR_MIPMAP_LINEAR"); break;
	}
}

static void VsyncCallback(void *unused)
{
	Cvar_SetValue("r_swapinterval", s_vsync_box.curvalue);
}

static void AdjustFOVCallback(void *unused)
{
	Cvar_SetValue("cl_widescreen_fov", s_adjust_fov_box.curvalue);
}

static void AsyncCallback(void *unused)
{
	Cvar_SetValue("cl_async", s_async_box.curvalue);
}

static void AdvancedOptions(void *s)
{
	M_Menu_Video_Advanced_f();
}

#pragma endregion 

extern void Menu_Video_Init(void);

static void ResetVideoDefaults(void *unused)
{
	Cvar_SetToDefault("vid_fullscreen");
	Cvar_SetToDefault("vid_gamma");
	Cvar_SetToDefault("r_mode");
	Cvar_SetToDefault("r_texturemode");
	Cvar_SetToDefault("r_anisotropic");
	Cvar_SetToDefault("r_swapinterval");
	Cvar_SetToDefault("cl_widescreen_fov");
	Cvar_SetToDefault("cl_async");

	Cvar_SetToDefault("r_rgbscale");
	Cvar_SetToDefault("r_trans_lighting");
	Cvar_SetToDefault("r_warp_lighting");
	Cvar_SetToDefault("r_lightcutoff");
	Cvar_SetToDefault("r_glass_envmaps");
	Cvar_SetToDefault("r_solidalpha");
	Cvar_SetToDefault("r_pixel_shader_warp");
	Cvar_SetToDefault("r_waterwave");
	Cvar_SetToDefault("r_caustics");
	Cvar_SetToDefault("r_particle_overdraw");
	Cvar_SetToDefault("r_bloom");
	Cvar_SetToDefault("r_model_shading");
	Cvar_SetToDefault("r_shadows");
	Cvar_SetToDefault("r_shelltype");
	Cvar_SetToDefault("r_screenshot_format");
	Cvar_SetToDefault("r_screenshot_jpeg_quality");

	Menu_Video_Init();
}

static void PrepareVideoRefresh(void)
{
	// Set the right mode for refresh
	Cvar_Set("vid_ref", "gl");
	Cvar_Set("gl_driver", "opengl32");

	// Tell them they're modified so they refresh
	vid_ref->modified = true;
}

static void ApplyChanges(void *unused)
{
	const int mode = s_mode_list.curvalue;
	Cvar_SetValue("r_mode", (mode == 0) ? -1 : mode + 2); // offset for eliminating < 640x480 modes
	if (mode == 0)	// Knightmare- use custom mode fields
	{
		const int customW = atoi(s_customwidth_field.buffer);
		const int customH = atoi(s_customheight_field.buffer);
		Cvar_SetValue("r_customwidth", ClampCvar(640, 99999, customW));
		Cvar_SetValue("r_customheight", ClampCvar(480, 99999, customH));

		// Update fields in case values were clamped
		char* customStr = Cvar_VariableString("r_customwidth");
		Q_strncpyz(s_customwidth_field.buffer, customStr, sizeof(s_customwidth_field.buffer));
		s_customwidth_field.cursor = strlen(customStr);
		
		customStr = Cvar_VariableString("r_customheight");
		Q_strncpyz(s_customheight_field.buffer, customStr, sizeof(s_customwidth_field.buffer));
		s_customheight_field.cursor = strlen(customStr);
	}

	Cvar_SetValue("vid_fullscreen", s_fs_box.curvalue);
	Cvar_SetValue("r_swapinterval", s_vsync_box.curvalue);
	Cvar_SetValue("cl_widescreen_fov", s_adjust_fov_box.curvalue);

	PrepareVideoRefresh();
}

// Knightmare added
int texfilter_box_setval(void)
{
	char *texmode = Cvar_VariableString("r_texturemode");
	if (!Q_strcasecmp(texmode, "GL_NEAREST_MIPMAP_LINEAR")) //mxd
		return 0;

	if (!Q_strcasecmp(texmode, "GL_LINEAR_MIPMAP_NEAREST"))
		return 1;

	return 2; // Trilinear
}

static const char **GetAnisoNames()
{
	static const char *aniso00_names[] = { "Not supported", 0 };
	static const char *aniso02_names[] = { "Off", "2x", 0 };
	static const char *aniso04_names[] = { "Off", "2x", "4x", 0 };
	static const char *aniso08_names[] = { "Off", "2x", "4x", "8x", 0 };
	static const char *aniso16_names[] = { "Off", "2x", "4x", "8x", "16x", 0 };
	
	const float aniso_avail = Cvar_VariableValue("r_anisotropic_avail");

	if (aniso_avail < 2.0)
		return aniso00_names;
	if (aniso_avail < 4.0)
		return aniso02_names;
	if (aniso_avail < 8.0)
		return aniso04_names;
	if (aniso_avail < 16.0)
		return aniso08_names;
	return aniso16_names; // >= 16.0
}


float GetAnisoCurValue()
{
	const float aniso_avail = Cvar_VariableValue("r_anisotropic_avail");
	const float anisoValue = ClampCvar(0, aniso_avail, Cvar_VariableValue("r_anisotropic"));
	
	if (aniso_avail == 0) // not available
		return 0;

	if (anisoValue < 2.0)
		return 0;
	if (anisoValue < 4.0)
		return 1;
	if (anisoValue < 8.0)
		return 2;
	if (anisoValue < 16.0)
		return 3;
	return 4; // >= 16.0
}

void Menu_Video_Init(void)
{
	// Knightmare- added 1280x1024, 1400x1050, 856x480, 1024x480 modes, removed 320x240, 400x300, 512x384 modes
	static const char *resolutions[] = 
	{
		#include "../qcommon/vid_resolutions.h"
	};
	static const char *mip_names[] = { "Nearest", "Bilinear", "Trilinear", 0 }; //mxd. Added "Nearest"

	int y = 0;

	if (!con_font_size)
		con_font_size = Cvar_Get("con_font_size", "8", CVAR_ARCHIVE);

	s_video_menu.x = SCREEN_WIDTH * 0.5f;
	s_video_menu.y = DEFAULT_MENU_Y; //mxd. Was SCREEN_HEIGHT * 0.5f - 80;
	s_video_menu.nitems = 0;

	s_mode_list.generic.type		= MTYPE_SPINCONTROL;
	s_mode_list.generic.name		= "Video mode";
	s_mode_list.generic.x			= 0;
	s_mode_list.generic.y			= y;
	s_mode_list.itemnames			= resolutions;
	s_mode_list.generic.callback	= VidModeCallback;
	const int mode = Cvar_VariableInteger("r_mode");
	s_mode_list.curvalue			= (mode == -1) ? 0 : max(mode - 2, 1); // Offset for getting rid of < 640x480 resolutions
	s_mode_list.generic.statusbar	= "Changes screen resolution";

	s_customwidth_title.generic.type	= MTYPE_SEPARATOR;
	s_customwidth_title.generic.flags	= (s_mode_list.curvalue != 0) ? QMF_HIDDEN : 0;
	s_customwidth_title.generic.name	= "Custom width";
	s_customwidth_title.generic.x		= -2 * MENU_FONT_SIZE;
	s_customwidth_title.generic.y		= y += 1.5f * MENU_LINE_SIZE;
	
	s_customwidth_field.generic.type		= MTYPE_FIELD;
	s_customwidth_field.generic.flags		= (s_mode_list.curvalue != 0) ? (QMF_NUMBERSONLY | QMF_HIDDEN) : QMF_NUMBERSONLY;
	s_customwidth_field.generic.x			= -14 * MENU_FONT_SIZE;
	s_customwidth_field.generic.y			= y + 1.5f * MENU_LINE_SIZE;
	s_customwidth_field.length				= 5;
	s_customwidth_field.visible_length		= 6;
	char* customStr = Cvar_VariableString("r_customwidth");
	Q_strncpyz(s_customwidth_field.buffer, customStr, sizeof(s_customwidth_field.buffer));
	s_customwidth_field.cursor				= strlen(customStr);

	s_customheight_title.generic.type	= MTYPE_SEPARATOR;
	s_customheight_title.generic.flags	= (s_mode_list.curvalue != 0) ? QMF_HIDDEN : 0;
	s_customheight_title.generic.name	= "Custom height";
	s_customheight_title.generic.x		= 14.5f * MENU_FONT_SIZE;
	s_customheight_title.generic.y		= y;

	s_customheight_field.generic.type		= MTYPE_FIELD;
	s_customheight_field.generic.flags		= (s_mode_list.curvalue != 0) ? (QMF_NUMBERSONLY | QMF_HIDDEN) : QMF_NUMBERSONLY;
	s_customheight_field.generic.x			= 2 * MENU_FONT_SIZE;
	s_customheight_field.generic.y			= y + 1.5f * MENU_LINE_SIZE;
	s_customheight_field.length				= 5;
	s_customheight_field.visible_length		= 6;
	customStr = Cvar_VariableString("r_customheight");
	Q_strncpyz(s_customheight_field.buffer, customStr, sizeof(s_customheight_field.buffer));
	s_customheight_field.cursor				= strlen(customStr);

	s_fs_box.generic.type			= MTYPE_SPINCONTROL;
	s_fs_box.generic.x				= 0;
	s_fs_box.generic.y				= y += 3.5f * MENU_LINE_SIZE;
	s_fs_box.generic.name			= "Fullscreen";
	s_fs_box.itemnames				= yesno_names;
	s_fs_box.curvalue				= ClampCvar(0, 1, Cvar_VariableInteger("vid_fullscreen"));
	s_fs_box.generic.statusbar		= "Changes bettween fullscreen and windowed display mode";

	s_brightness_slider.generic.type		= MTYPE_SLIDER;
	s_brightness_slider.generic.x			= 0;
	s_brightness_slider.generic.y			= y += MENU_LINE_SIZE;
	s_brightness_slider.generic.name		= "Brightness";
	s_brightness_slider.generic.callback	= BrightnessCallback;
	s_brightness_slider.minvalue			= 1;
	s_brightness_slider.maxvalue			= 20;
	s_brightness_slider.curvalue			= ClampCvar(1, 20, Cvar_VariableValue("vid_gamma") * 10);
	s_brightness_slider.numdecimals			= 1; //mxd
	s_brightness_slider.generic.statusbar	= "Changes display brightness";
	s_brightness_slider.cvar				= Cvar_FindVar("vid_gamma"); //mxd

	s_texfilter_box.generic.type		= MTYPE_SPINCONTROL;
	s_texfilter_box.generic.x			= 0;
	s_texfilter_box.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_texfilter_box.generic.name		= "Texture filter";
	s_texfilter_box.generic.callback	= TextureFilterCallback; //mxd
	s_texfilter_box.curvalue			= texfilter_box_setval();
	s_texfilter_box.itemnames			= mip_names;
	s_texfilter_box.generic.statusbar	= "Changes texture filtering mode";

	s_aniso_box.generic.type		= MTYPE_SPINCONTROL;
	s_aniso_box.generic.x			= 0;
	s_aniso_box.generic.y			= y += MENU_LINE_SIZE;
	s_aniso_box.generic.name		= "Anisotropic filtering";
	s_aniso_box.generic.callback	= AnisotropicCallback; //mxd
	s_aniso_box.curvalue			= GetAnisoCurValue();
	s_aniso_box.itemnames			= GetAnisoNames();
	s_aniso_box.generic.statusbar	= "Changes the level of anisotropic mipmap filtering";

	s_vsync_box.generic.type			= MTYPE_SPINCONTROL;
	s_vsync_box.generic.x				= 0;
	s_vsync_box.generic.y				= y += 2 * MENU_LINE_SIZE;
	s_vsync_box.generic.name			= "Vertical sync";
	s_vsync_box.generic.callback		= VsyncCallback;
	s_vsync_box.curvalue				= ClampCvar(0, 1, Cvar_VariableInteger("r_swapinterval"));
	s_vsync_box.itemnames				= yesno_names;
	s_vsync_box.generic.statusbar		= "Sync framerate with monitor refresh";

	s_adjust_fov_box.generic.type		= MTYPE_SPINCONTROL; //TODO: mxd. Remove? Who DOESN'T want this?
	s_adjust_fov_box.generic.x			= 0;
	s_adjust_fov_box.generic.y			= y += MENU_LINE_SIZE;
	s_adjust_fov_box.generic.name		= "FOV autoscaling";
	s_adjust_fov_box.generic.callback	= AdjustFOVCallback;
	s_adjust_fov_box.curvalue			= ClampCvar(0, 1, Cvar_VariableInteger("cl_widescreen_fov"));
	s_adjust_fov_box.itemnames			= yesno_names;
	s_adjust_fov_box.generic.statusbar	= "Automatic scaling of fov for widescreen modes";

	s_async_box.generic.type			= MTYPE_SPINCONTROL;
	s_async_box.generic.x				= 0;
	s_async_box.generic.y				= y += MENU_LINE_SIZE;
	s_async_box.generic.name			= "Asynchronous refresh";
	s_async_box.generic.callback		= AsyncCallback;
	s_async_box.curvalue				= ClampCvar(0, 1, Cvar_VariableValue("cl_async"));
	s_async_box.itemnames				= yesno_names;
	s_async_box.generic.statusbar		= "Decouples network framerate from render framerate";

	s_advanced_action.generic.type		= MTYPE_ACTION;
	s_advanced_action.generic.flags		= QMF_LEFT_JUSTIFY; //mxd
	s_advanced_action.generic.name		= "Advanced options";
	s_advanced_action.generic.x			= -MENU_FONT_SIZE * strlen(s_advanced_action.generic.name); //mxd. Was 0;
	s_advanced_action.generic.y			= y += 3 * MENU_LINE_SIZE;
	s_advanced_action.generic.callback	= AdvancedOptions;

	s_defaults_action.generic.type		= MTYPE_ACTION;
	s_defaults_action.generic.flags		= QMF_LEFT_JUSTIFY; //mxd
	s_defaults_action.generic.name		= "Reset to defaults";
	s_defaults_action.generic.x			= -MENU_FONT_SIZE * strlen(s_defaults_action.generic.name); //mxd. Was 0;
	s_defaults_action.generic.y			= y += 3 * MENU_LINE_SIZE;
	s_defaults_action.generic.callback	= ResetVideoDefaults;
	s_defaults_action.generic.statusbar	= "Resets all video settings to internal defaults";

	// Changed cancel to apply changes, thanx to MrG
	s_apply_action.generic.type			= MTYPE_ACTION;
	s_apply_action.generic.flags		= QMF_LEFT_JUSTIFY; //mxd
	s_apply_action.generic.name			= "Apply changes";
	s_apply_action.generic.x			= -MENU_FONT_SIZE * strlen(s_apply_action.generic.name); //mxd. Was 0;
	s_apply_action.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_apply_action.generic.callback		= ApplyChanges;

	s_backmain_action.generic.type		= MTYPE_ACTION;
	s_backmain_action.generic.flags		= QMF_LEFT_JUSTIFY; //mxd
	s_backmain_action.generic.name		= (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_MAIN); //mxd
	s_backmain_action.generic.x			= UI_CenteredX(&s_backmain_action.generic, s_video_menu.x); //mxd. Was 0;
	s_backmain_action.generic.y			= y += 3 * MENU_LINE_SIZE;
	s_backmain_action.generic.callback	= UI_BackMenu;

	Menu_AddItem(&s_video_menu, (void *)&s_mode_list);

	Menu_AddItem(&s_video_menu, (void *)&s_customwidth_title);
	Menu_AddItem(&s_video_menu, (void *)&s_customwidth_field);
	Menu_AddItem(&s_video_menu, (void *)&s_customheight_title);
	Menu_AddItem(&s_video_menu, (void *)&s_customheight_field);

	Menu_AddItem(&s_video_menu, (void *)&s_fs_box);
	Menu_AddItem(&s_video_menu, (void *)&s_brightness_slider);
	Menu_AddItem(&s_video_menu, (void *)&s_texfilter_box);
	Menu_AddItem(&s_video_menu, (void *)&s_aniso_box);
	Menu_AddItem(&s_video_menu, (void *)&s_vsync_box);
	Menu_AddItem(&s_video_menu, (void *)&s_adjust_fov_box);
	Menu_AddItem(&s_video_menu, (void *)&s_async_box);

	Menu_AddItem(&s_video_menu, (void *)&s_advanced_action);
	Menu_AddItem(&s_video_menu, (void *)&s_defaults_action);
	Menu_AddItem(&s_video_menu, (void *)&s_apply_action);
	Menu_AddItem(&s_video_menu, (void *)&s_backmain_action);
}

void Menu_Video_Draw(void)
{
	// Draw the banner
	Menu_DrawBanner("m_banner_video");

	// Move cursor to a reasonable starting position
	Menu_AdjustCursor(&s_video_menu, 1);

	// Draw the menu
	Menu_Draw(&s_video_menu);
}

const char *Video_MenuKey(int key)
{
	return Default_MenuKey(&s_video_menu, key);
}

void M_Menu_Video_f(void)
{
	Menu_Video_Init();
	UI_PushMenu(Menu_Video_Draw, Video_MenuKey);
}