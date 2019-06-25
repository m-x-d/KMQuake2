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

// ui_video_advanced.c -- the advanced video menu
 
#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"

extern cvar_t *vid_ref;

static menuframework_s	s_video_advanced_menu;
static menuseparator_s	s_options_advanced_header;
static menulist_s		s_rgbscale_box;
static menulist_s		s_trans_lighting_box;
static menulist_s		s_warp_lighting_box;
static menuslider_s		s_lightcutoff_slider;
static menulist_s		s_dlight_shadowmap_quality_box; //mxd
static menulist_s		s_dlight_normalmapping_box; //mxd
static menulist_s		s_solidalpha_box;
static menulist_s		s_texshader_warp_box;
static menuslider_s		s_waterwave_slider;
static menulist_s		s_caustics_box;
static menulist_s		s_particle_overdraw_box;
static menulist_s		s_particle_mode_box; //mxd
static menulist_s		s_lightbloom_box;
static menulist_s		s_modelshading_box;
static menulist_s		s_shadows_box;
static menulist_s		s_two_side_stencil_box;
static menulist_s		s_ent_shell_box;
static menulist_s		s_glass_envmap_box;
static menulist_s		s_screenshotformat_box;
static menuslider_s		s_screenshotjpegquality_slider;
static menuaction_s		s_back_action;


static void Video_Advanced_MenuSetValues(void)
{
	Cvar_SetValue("r_rgbscale", ClampCvar(1, 2, Cvar_VariableValue("r_rgbscale")));
	if (Cvar_VariableValue("r_rgbscale") == 1)
		s_rgbscale_box.curvalue = 0;
	else
		s_rgbscale_box.curvalue = 1;

	Cvar_SetValue("r_trans_lighting", ClampCvar(0, 2, Cvar_VariableValue("r_trans_lighting")));
	s_trans_lighting_box.curvalue = Cvar_VariableValue("r_trans_lighting");

	Cvar_SetValue("r_warp_lighting", ClampCvar(0, 1, Cvar_VariableValue("r_warp_lighting")));
	s_warp_lighting_box.curvalue = Cvar_VariableValue("r_warp_lighting");

	Cvar_SetValue("r_lightcutoff", ClampCvar(0, 64, Cvar_VariableValue("r_lightcutoff")));
	s_lightcutoff_slider.curvalue = Cvar_VariableValue("r_lightcutoff") / 8.0f;

	Cvar_SetValue("r_dlightshadowmapscale", ClampCvar(0, 5, Cvar_VariableValue("r_dlightshadowmapscale"))); //mxd
	const int dlightshadowmapscale = Cvar_VariableValue("r_dlightshadowmapscale");
	if (dlightshadowmapscale == 0)
		s_dlight_shadowmap_quality_box.curvalue = 0;
	else
		s_dlight_shadowmap_quality_box.curvalue = 6 - dlightshadowmapscale;

	Cvar_SetValue("r_dlightnormalmapping", ClampCvar(0, 1, Cvar_VariableValue("r_dlightnormalmapping"))); //mxd
	s_dlight_normalmapping_box.curvalue = Cvar_VariableValue("r_dlightnormalmapping");

	Cvar_SetValue("r_glass_envmaps", ClampCvar(0, 1, Cvar_VariableValue("r_glass_envmaps")));
	s_glass_envmap_box.curvalue = Cvar_VariableValue("r_glass_envmaps");

	Cvar_SetValue("r_solidalpha", ClampCvar(0, 1, Cvar_VariableValue("r_solidalpha")));
	s_solidalpha_box.curvalue = Cvar_VariableValue("r_solidalpha");

	Cvar_SetValue("r_pixel_shader_warp", ClampCvar(0, 1, Cvar_VariableValue("r_pixel_shader_warp")));
	s_texshader_warp_box.curvalue = Cvar_VariableValue("r_pixel_shader_warp");

	Cvar_SetValue("r_waterwave", ClampCvar(0, 24, Cvar_VariableValue("r_waterwave")));
	s_waterwave_slider.curvalue = Cvar_VariableValue("r_waterwave");

	Cvar_SetValue("r_caustics", ClampCvar(0, 2, Cvar_VariableValue("r_caustics")));
	s_caustics_box.curvalue = Cvar_VariableValue("r_caustics");

	Cvar_SetValue("r_particle_mode", ClampCvar(0, 1, Cvar_VariableValue("r_particle_mode"))); //mxd
	s_particle_mode_box.curvalue = Cvar_VariableValue("r_particle_mode");

	Cvar_SetValue("r_particle_overdraw", ClampCvar(0, 1, Cvar_VariableValue("r_particle_overdraw")));
	s_particle_overdraw_box.curvalue = Cvar_VariableValue("r_particle_overdraw");

	Cvar_SetValue("r_bloom", ClampCvar(0, 1, Cvar_VariableValue("r_bloom")));
	s_lightbloom_box.curvalue = Cvar_VariableValue("r_bloom");

	Cvar_SetValue("r_model_shading", ClampCvar(0, 3, Cvar_VariableValue("r_model_shading")));
	s_modelshading_box.curvalue = Cvar_VariableValue("r_model_shading");

	Cvar_SetValue("r_shadows", ClampCvar(0, 1, Cvar_VariableValue("r_shadows")));
	s_shadows_box.curvalue = Cvar_VariableValue("r_shadows");

	Cvar_SetValue("r_stencilTwoSide", ClampCvar(0, 1, Cvar_VariableValue("r_stencilTwoSide")));
	s_two_side_stencil_box.curvalue = Cvar_VariableValue("r_stencilTwoSide");

	Cvar_SetValue("r_shelltype", ClampCvar(0, 2, Cvar_VariableValue("r_shelltype")));
	s_ent_shell_box.curvalue = Cvar_VariableValue("r_shelltype");

	char *sshotformat = Cvar_VariableString("r_screenshot_format");
	if (!Q_strcasecmp(sshotformat, "jpg"))
		s_screenshotformat_box.curvalue = 0;
	else if (!Q_strcasecmp(sshotformat, "png"))
		s_screenshotformat_box.curvalue = 1;
	else // tga
		s_screenshotformat_box.curvalue = 2;

	Cvar_SetValue("r_screenshot_jpeg_quality", ClampCvar(50, 100, Cvar_VariableValue("r_screenshot_jpeg_quality")));
	s_screenshotjpegquality_slider.curvalue = (Cvar_VariableValue("r_screenshot_jpeg_quality") - 50) / 5;
}

#pragma region ======================= Menu item callbacks

static void RGBSCaleCallback(void *unused)
{
	Cvar_SetValue("r_rgbscale", s_rgbscale_box.curvalue + 1);
}

static void TransLightingCallback(void *unused)
{
	Cvar_SetValue("r_trans_lighting", s_trans_lighting_box.curvalue);
}

static void WarpLightingCallback(void *unused)
{
	Cvar_SetValue("r_warp_lighting", s_warp_lighting_box.curvalue);
}

static void LightCutoffCallback(void *unused)
{
	Cvar_SetValue("r_lightcutoff", s_lightcutoff_slider.curvalue * 8.0f);
}

static void DlightShadowmapScaleCallback(void *unused) //mxd
{
	if (s_dlight_shadowmap_quality_box.curvalue == 0)
		Cvar_SetValue("r_dlightshadowmapscale", 0);
	else
		Cvar_SetValue("r_dlightshadowmapscale", 6 - s_dlight_shadowmap_quality_box.curvalue);
}

static void DlightNormalmappingCallback(void *unused) //mxd
{
	Cvar_SetValue("r_dlightnormalmapping", s_dlight_normalmapping_box.curvalue);
}

static void GlassEnvmapCallback(void *unused)
{
	Cvar_SetValue("r_glass_envmaps", s_glass_envmap_box.curvalue);
}

static void SolidAlphaCallback(void *unused)
{
	Cvar_SetValue("r_solidalpha", s_solidalpha_box.curvalue);
}

static void TexShaderWarpCallback(void *unused)
{
	Cvar_SetValue("r_pixel_shader_warp", s_texshader_warp_box.curvalue);
}

static void WaterWaveCallback(void *unused)
{
	Cvar_SetValue("r_waterwave", s_waterwave_slider.curvalue);
}

static void CausticsCallback(void *unused)
{
	Cvar_SetValue("r_caustics", s_caustics_box.curvalue);
}

static void ParticleModeCallback(void *unused) //mxd
{
	Cvar_SetValue("r_particle_mode", s_particle_mode_box.curvalue);
}

static void ParticleOverdrawCallback(void *unused)
{
	Cvar_SetValue("r_particle_overdraw", s_particle_overdraw_box.curvalue);
}

static void LightBloomCallback(void *unused)
{
	Cvar_SetValue("r_bloom", s_lightbloom_box.curvalue);
}

static void ModelShadingCallback(void *unused)
{
	Cvar_SetValue("r_model_shading", s_modelshading_box.curvalue);
}

static void ShadowsCallback(void *unused)
{
	Cvar_SetValue("r_shadows", s_shadows_box.curvalue);
}

static void TwoSideStencilCallback(void *unused)
{
	Cvar_SetValue("r_stencilTwoSide", s_two_side_stencil_box.curvalue);
}

static void EntShellCallback(void *unused)
{
	Cvar_SetValue("r_shelltype", s_ent_shell_box.curvalue);
}

static void ScreenshotFormatCallback(void *unused)
{
	switch (s_screenshotformat_box.curvalue)
	{
		case 0:
			Cvar_Set("r_screenshot_format", "jpg"); 
			break;

		case 2:
			Cvar_Set("r_screenshot_format", "tga"); 
			break;

		case 1:
		default:
			Cvar_Set("r_screenshot_format", "png");
			break;
	}
}

static void JPEGScreenshotQualityCallback(void *unused)
{
	Cvar_SetValue("r_screenshot_jpeg_quality", (s_screenshotjpegquality_slider.curvalue * 5 + 50));
}

#pragma endregion

void Menu_Video_Advanced_Init(void)
{
	static const char *lighting_names[] = { "No", "Vertex", "Lightmap (if available)", 0 };
	static const char *shading_names[] = { "Off", "Low", "Medium", "High", 0 };
	static const char *shadow_names[] = { "No", "Blob", 0 }; //mxd. "static planar" -> "blob"
	static const char *ifsupported_names[] = { "No", "If supported", 0 };
	static const char *caustics_names[] = { "No", "Software warp", "Hardware warp (if supported)", 0 };
	static const char *shell_names[] = { "Solid", "Flowing", "Envmap", 0 };
	static const char *screenshotformat_names[] = { "JPEG", "PNG", "TGA", 0 };
	static const char *dlight_shadowmap_quality_names[] = { "Disabled", "Very low", "Low", "Medium", "High", "Very high", 0 }; //mxd

	int y = 0;

	s_video_advanced_menu.x = SCREEN_WIDTH * 0.5f;
	s_video_advanced_menu.y = DEFAULT_MENU_Y; //mxd. Was SCREEN_HEIGHT * 0.5f - 100;
	s_video_advanced_menu.nitems = 0;

	s_options_advanced_header.generic.type		= MTYPE_SEPARATOR;
	s_options_advanced_header.generic.name		= "Advanced Options";
	s_options_advanced_header.generic.x			= MENU_FONT_SIZE / 2 * strlen(s_options_advanced_header.generic.name);
	s_options_advanced_header.generic.y			= y;

	s_rgbscale_box.generic.type				= MTYPE_SPINCONTROL;
	s_rgbscale_box.generic.x				= 0;
	s_rgbscale_box.generic.y				= y += 2 * MENU_LINE_SIZE;
	s_rgbscale_box.generic.name				= "RGB enhance";
	s_rgbscale_box.generic.callback			= RGBSCaleCallback;
	s_rgbscale_box.itemnames				= yesno_names;
	s_rgbscale_box.generic.statusbar		= "Brightens textures without washing them out";

	s_trans_lighting_box.generic.type		= MTYPE_SPINCONTROL;
	s_trans_lighting_box.generic.x			= 0;
	s_trans_lighting_box.generic.y			= y += MENU_LINE_SIZE;
	s_trans_lighting_box.generic.name		= "Translucent lighting";
	s_trans_lighting_box.generic.callback	= TransLightingCallback;
	s_trans_lighting_box.itemnames			= lighting_names;
	s_trans_lighting_box.generic.statusbar	= "Vertex lighting on translucent surfaces";

	s_warp_lighting_box.generic.type		= MTYPE_SPINCONTROL;
	s_warp_lighting_box.generic.x			= 0;
	s_warp_lighting_box.generic.y			= y += MENU_LINE_SIZE;
	s_warp_lighting_box.generic.name		= "Warp surface lighting";
	s_warp_lighting_box.generic.callback	= WarpLightingCallback;
	s_warp_lighting_box.itemnames			= yesno_names;
	s_warp_lighting_box.generic.statusbar	= "Vertex lighting on water and other warping surfaces";

	s_lightcutoff_slider.generic.type		= MTYPE_SLIDER;
	s_lightcutoff_slider.generic.x			= 0;
	s_lightcutoff_slider.generic.y			= y += MENU_LINE_SIZE;
	s_lightcutoff_slider.generic.name		= "Dynamic light cutoff";
	s_lightcutoff_slider.generic.callback	= LightCutoffCallback;
	s_lightcutoff_slider.minvalue			= 0;
	s_lightcutoff_slider.maxvalue			= 8;
	s_lightcutoff_slider.generic.statusbar	= "Lower = smoother blend, higher = faster";
	s_lightcutoff_slider.cvar				= Cvar_FindVar("r_lightcutoff"); //mxd;

	//mxd
	s_dlight_shadowmap_quality_box.generic.type			= MTYPE_SPINCONTROL;
	s_dlight_shadowmap_quality_box.generic.x			= 0;
	s_dlight_shadowmap_quality_box.generic.y			= y += MENU_LINE_SIZE;
	s_dlight_shadowmap_quality_box.generic.name			= "Dynamic light shadowmap quality";
	s_dlight_shadowmap_quality_box.generic.callback		= DlightShadowmapScaleCallback;
	s_dlight_shadowmap_quality_box.itemnames			= dlight_shadowmap_quality_names;
	s_dlight_shadowmap_quality_box.generic.statusbar	= "Maximum quality depends on lightmap resolution.";

	//mxd
	s_dlight_normalmapping_box.generic.type				= MTYPE_SPINCONTROL;
	s_dlight_normalmapping_box.generic.x				= 0;
	s_dlight_normalmapping_box.generic.y				= y += MENU_LINE_SIZE;
	s_dlight_normalmapping_box.generic.name				= "Dynamic light normalmapping";
	s_dlight_normalmapping_box.generic.callback			= DlightNormalmappingCallback;
	s_dlight_normalmapping_box.itemnames				= yesno_names;
	s_dlight_normalmapping_box.generic.statusbar		= "Requires maps built with per-pixel lightmap resolution and normalmap textures.";

	s_glass_envmap_box.generic.type			= MTYPE_SPINCONTROL;
	s_glass_envmap_box.generic.x			= 0;
	s_glass_envmap_box.generic.y			= y += MENU_LINE_SIZE;
	s_glass_envmap_box.generic.name			= "Glass envmaps";
	s_glass_envmap_box.generic.callback		= GlassEnvmapCallback;
	s_glass_envmap_box.itemnames			= yesno_names;
	s_glass_envmap_box.generic.statusbar	= "Enable environment mapping on transparent surfaces";

	s_solidalpha_box.generic.type			= MTYPE_SPINCONTROL;
	s_solidalpha_box.generic.x				= 0;
	s_solidalpha_box.generic.y				= y += MENU_LINE_SIZE;
	s_solidalpha_box.generic.name			= "Solid alphas";
	s_solidalpha_box.generic.callback		= SolidAlphaCallback;
	s_solidalpha_box.itemnames				= yesno_names;
	s_solidalpha_box.generic.statusbar		= "Enable solid drawing of trans33 + trans66 surfaces";

	s_texshader_warp_box.generic.type		= MTYPE_SPINCONTROL;
	s_texshader_warp_box.generic.x			= 0;
	s_texshader_warp_box.generic.y			= y += MENU_LINE_SIZE;
	s_texshader_warp_box.generic.name		= "Texture shader warp";
	s_texshader_warp_box.generic.callback	= TexShaderWarpCallback;
	s_texshader_warp_box.itemnames			= ifsupported_names;
	s_texshader_warp_box.generic.statusbar	= "Enables hardware water warping effect";

	s_waterwave_slider.generic.type			= MTYPE_SLIDER;
	s_waterwave_slider.generic.x			= 0;
	s_waterwave_slider.generic.y			= y += MENU_LINE_SIZE;
	s_waterwave_slider.generic.name			= "Water wave size";
	s_waterwave_slider.generic.callback		= WaterWaveCallback;
	s_waterwave_slider.minvalue				= 0;
	s_waterwave_slider.maxvalue				= 24;
	s_waterwave_slider.generic.statusbar	= "Size of waves on flat water surfaces";
	s_waterwave_slider.cvar					= Cvar_FindVar("r_waterwave"); //mxd

	s_caustics_box.generic.type				= MTYPE_SPINCONTROL;
	s_caustics_box.generic.x				= 0;
	s_caustics_box.generic.y				= y += MENU_LINE_SIZE;
	s_caustics_box.generic.name				= "Underwater caustics";
	s_caustics_box.generic.callback			= CausticsCallback;
	s_caustics_box.itemnames				= caustics_names;
	s_caustics_box.generic.statusbar		= "Caustic effect on underwater surfaces";

	s_particle_mode_box.generic.type		= MTYPE_SPINCONTROL; //mxd
	s_particle_mode_box.generic.x			= 0;
	s_particle_mode_box.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_particle_mode_box.generic.name		= "Enhanced particles";
	s_particle_mode_box.generic.callback	= ParticleModeCallback;
	s_particle_mode_box.itemnames			= yesno_names;
	s_particle_mode_box.generic.statusbar	= "Enhanced particle and beam effects";

	s_particle_overdraw_box.generic.type		= MTYPE_SPINCONTROL;
	s_particle_overdraw_box.generic.x			= 0;
	s_particle_overdraw_box.generic.y			= y += MENU_LINE_SIZE;
	s_particle_overdraw_box.generic.name		= "Particle overdraw";
	s_particle_overdraw_box.generic.callback	= ParticleOverdrawCallback;
	s_particle_overdraw_box.itemnames			= yesno_names;
	s_particle_overdraw_box.generic.statusbar	= "Redraw particles over trans surfaces";

	s_lightbloom_box.generic.type			= MTYPE_SPINCONTROL;
	s_lightbloom_box.generic.x				= 0;
	s_lightbloom_box.generic.y				= y += MENU_LINE_SIZE;
	s_lightbloom_box.generic.name			= "Bloom";
	s_lightbloom_box.generic.callback		= LightBloomCallback;
	s_lightbloom_box.itemnames				= yesno_names;
	s_lightbloom_box.generic.statusbar		= "Enables blooming of bright lights";

	//TODO: mxd. bloom-related controls 

	s_modelshading_box.generic.type			= MTYPE_SPINCONTROL;
	s_modelshading_box.generic.x			= 0;
	s_modelshading_box.generic.y			= y += MENU_LINE_SIZE;
	s_modelshading_box.generic.name			= "Model shading";
	s_modelshading_box.generic.callback		= ModelShadingCallback;
	s_modelshading_box.itemnames			= shading_names;
	s_modelshading_box.generic.statusbar	= "Level of shading to use on models";

	s_shadows_box.generic.type				= MTYPE_SPINCONTROL;
	s_shadows_box.generic.x					= 0;
	s_shadows_box.generic.y					= y += MENU_LINE_SIZE;
	s_shadows_box.generic.name				= "Model shadows";
	s_shadows_box.generic.callback			= ShadowsCallback;
	s_shadows_box.itemnames					= shadow_names;
	s_shadows_box.generic.statusbar			= "Type of model shadows to draw";

	s_two_side_stencil_box.generic.type			= MTYPE_SPINCONTROL;
	s_two_side_stencil_box.generic.x			= 0;
	s_two_side_stencil_box.generic.y			= y += MENU_LINE_SIZE;
	s_two_side_stencil_box.generic.name			= "Two-sided stenciling";
	s_two_side_stencil_box.generic.callback		= TwoSideStencilCallback;
	s_two_side_stencil_box.itemnames			= ifsupported_names;
	s_two_side_stencil_box.generic.statusbar	= "Use single-pass shadow stenciling";

	s_ent_shell_box.generic.type				= MTYPE_SPINCONTROL;
	s_ent_shell_box.generic.x					= 0;
	s_ent_shell_box.generic.y					= y += MENU_LINE_SIZE;
	s_ent_shell_box.generic.name				= "Entity shell type";
	s_ent_shell_box.generic.callback			= EntShellCallback;
	s_ent_shell_box.itemnames					= shell_names;
	s_ent_shell_box.generic.statusbar			= "Envmap effect may cause instability on ATI cards";

	s_screenshotformat_box.generic.type			= MTYPE_SPINCONTROL;
	s_screenshotformat_box.generic.x			= 0;
	s_screenshotformat_box.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_screenshotformat_box.generic.name			= "Screenshot format";
	s_screenshotformat_box.generic.callback		= ScreenshotFormatCallback;
	s_screenshotformat_box.itemnames			= screenshotformat_names;
	s_screenshotformat_box.generic.statusbar	= "Image format for screenshots";

	s_screenshotjpegquality_slider.generic.type			= MTYPE_SLIDER;
	s_screenshotjpegquality_slider.generic.x			= 0;
	s_screenshotjpegquality_slider.generic.y			= y += MENU_LINE_SIZE;
	s_screenshotjpegquality_slider.generic.name			= "JPEG screenshot quality";
	s_screenshotjpegquality_slider.generic.callback		= JPEGScreenshotQualityCallback;
	s_screenshotjpegquality_slider.minvalue				= 0;
	s_screenshotjpegquality_slider.maxvalue				= 10;
	s_screenshotjpegquality_slider.generic.statusbar	= "Quality of JPG screenshots, 50-100%";
	s_screenshotjpegquality_slider.cvar					= Cvar_FindVar("r_screenshot_jpeg_quality"); //mxd

	s_back_action.generic.type					= MTYPE_ACTION;
	s_back_action.generic.flags					= QMF_LEFT_JUSTIFY; //mxd
	s_back_action.generic.name					= (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_VIDEO); //mxd
	s_back_action.generic.x						= UI_CenteredX(&s_back_action.generic, s_video_advanced_menu.x); //mxd
	s_back_action.generic.y						= y += 2 * MENU_LINE_SIZE;
	s_back_action.generic.callback				= UI_BackMenu;

	Video_Advanced_MenuSetValues();

	Menu_AddItem(&s_video_advanced_menu, (void *)&s_options_advanced_header);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_rgbscale_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_trans_lighting_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_warp_lighting_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_lightcutoff_slider);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_dlight_shadowmap_quality_box); //mxd
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_dlight_normalmapping_box); //mxd
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_glass_envmap_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_solidalpha_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_texshader_warp_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_waterwave_slider);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_caustics_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_particle_mode_box); //mxd
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_particle_overdraw_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_lightbloom_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_modelshading_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_shadows_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_two_side_stencil_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_ent_shell_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_screenshotformat_box);
	Menu_AddItem(&s_video_advanced_menu, (void *)&s_screenshotjpegquality_slider);

	Menu_AddItem(&s_video_advanced_menu, (void *)&s_back_action);
}

void Menu_Video_Advanced_Draw(void)
{
	// Draw the banner
	Menu_DrawBanner("m_banner_video");

	// Move cursor to a reasonable starting position
	Menu_AdjustCursor(&s_video_advanced_menu, 1);

	// Draw the menu
	Menu_Draw(&s_video_advanced_menu);
}

const char *Video_Advanced_MenuKey(int key)
{
	return Default_MenuKey(&s_video_advanced_menu, key);
}

void M_Menu_Video_Advanced_f(void)
{
	Menu_Video_Advanced_Init();
	UI_PushMenu(Menu_Video_Advanced_Draw, Video_Advanced_MenuKey);
}