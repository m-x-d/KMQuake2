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

// ui_options_effects.c -- the effects options menu

#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"

static menuframework_s	s_options_effects_menu;
static menuseparator_s	s_options_effects_header;
static menulist_s		s_options_effects_blood_box;
static menulist_s		s_options_effects_oldexplosions_box;
static menulist_s		s_options_effects_plasmaexplosound_box;
static menulist_s		s_options_effects_itembob_box;
static menuslider_s		s_options_effects_decal_slider;
static menuslider_s		s_options_effects_particle_comp_slider;
static menulist_s		s_options_effects_railtrail_box;
static menuslider_s		s_options_effects_railcolor_slider[3];
static menulist_s		s_options_effects_footstep_box;
static menuaction_s		s_options_effects_defaults_action;
static menuaction_s		s_options_effects_back_action;

//mxd. Rail color preview
static int rail_color_frame_x;
static int rail_color_frame_y;
static int rail_color_frame_size;

#pragma region ======================= Menu item callbacks

static void BloodFunc(void *unused)
{
	Cvar_SetValue("cl_blood", s_options_effects_blood_box.curvalue);
}

static void OldExplosionFunc(void *unused)
{
	Cvar_SetValue("cl_old_explosions", s_options_effects_oldexplosions_box.curvalue);
}

static void PlasmaExploSoundFunc(void *unused)
{
	Cvar_SetValue("cl_plasma_explo_sound", s_options_effects_plasmaexplosound_box.curvalue);
}

static void ItemBobFunc(void *unused)
{
	Cvar_SetValue("cl_item_bobbing", s_options_effects_itembob_box.curvalue);
}

static void ParticleCompFunc(void *unused)
{
	Cvar_SetValue("cl_particle_scale", (s_options_effects_particle_comp_slider.curvalue - 3) * -1 + 3);
}

static void DecalCallback(void *unused)
{
	Cvar_SetValue("r_decals", s_options_effects_decal_slider.curvalue * 50);
}

// Psychospaz's changeable rail trail
static void RailTrailFunc(void *unused)
{
	Cvar_SetValue("cl_railtype", s_options_effects_railtrail_box.curvalue);
}

static void RailColorRedFunc(void *unused)
{
	Cvar_SetValue("cl_railred", s_options_effects_railcolor_slider[0].curvalue * 16);
}

static void RailColorGreenFunc(void *unused)
{
	Cvar_SetValue("cl_railgreen", s_options_effects_railcolor_slider[1].curvalue * 16);
}

static void RailColorBlueFunc(void *unused)
{
	Cvar_SetValue("cl_railblue", s_options_effects_railcolor_slider[2].curvalue * 16);
}

// Foostep override option
static void FootStepFunc(void *unused)
{
	Cvar_SetValue("cl_footstep_override", s_options_effects_footstep_box.curvalue);
}

#pragma endregion

//mxd
static void RailColorPreviewDrawFunc(void *self)
{
	SCR_DrawFill(rail_color_frame_x + 0, rail_color_frame_y + 0, rail_color_frame_size - 0, rail_color_frame_size - 0, ALIGN_CENTER, 60, 60, 60, 255);
	SCR_DrawFill(rail_color_frame_x + 1, rail_color_frame_y + 1, rail_color_frame_size - 2, rail_color_frame_size - 2, ALIGN_CENTER,
		cl_railred->integer, cl_railgreen->integer, cl_railblue->integer, 255);
}

static void EffectsSetMenuItemValues(void)
{
	Cvar_SetValue("cl_blood", ClampCvar(0, 4, Cvar_VariableValue("cl_blood")));
	s_options_effects_blood_box.curvalue = Cvar_VariableValue("cl_blood");

	Cvar_SetValue("cl_old_explosions", ClampCvar(0, 1, Cvar_VariableValue("cl_old_explosions")));
	s_options_effects_oldexplosions_box.curvalue = Cvar_VariableValue("cl_old_explosions");

	Cvar_SetValue("cl_plasma_explo_sound", ClampCvar(0, 1, Cvar_VariableValue("cl_plasma_explo_sound")));
	s_options_effects_plasmaexplosound_box.curvalue = Cvar_VariableValue("cl_plasma_explo_sound");

	Cvar_SetValue("cl_item_bobbing", ClampCvar(0, 1, Cvar_VariableValue("cl_item_bobbing")));
	s_options_effects_itembob_box.curvalue = Cvar_VariableValue("cl_item_bobbing");

	Cvar_SetValue("r_decals", ClampCvar(0, 1000, Cvar_VariableValue("r_decals")));
	s_options_effects_decal_slider.curvalue = Cvar_VariableValue("r_decals") / 50;

	Cvar_SetValue("cl_particle_scale", ClampCvar(0, 5, Cvar_VariableValue("cl_particle_scale")));
	s_options_effects_particle_comp_slider.curvalue = (Cvar_VariableValue("cl_particle_scale") - 3) * -1 + 3;

	Cvar_SetValue("cl_railtype", ClampCvar(0, 2, Cvar_VariableValue("cl_railtype")));
	s_options_effects_railtrail_box.curvalue = Cvar_VariableValue("cl_railtype");
	s_options_effects_railcolor_slider[0].curvalue = Cvar_VariableValue("cl_railred") / 16;
	s_options_effects_railcolor_slider[1].curvalue = Cvar_VariableValue("cl_railgreen") / 16;
	s_options_effects_railcolor_slider[2].curvalue = Cvar_VariableValue("cl_railblue") / 16;

	Cvar_SetValue("cl_footstep_override", ClampCvar(0, 1, Cvar_VariableValue("cl_footstep_override")));
	s_options_effects_footstep_box.curvalue = Cvar_VariableValue("cl_footstep_override");
}

static void EffectsResetDefaultsFunc(void *unused)
{
	Cvar_SetToDefault("cl_blood");
	Cvar_SetToDefault("cl_old_explosions");
	Cvar_SetToDefault("cl_plasma_explo_sound");
	Cvar_SetToDefault("cl_item_bobbing");
	Cvar_SetToDefault("r_decals");
	Cvar_SetToDefault("cl_particle_scale");
	Cvar_SetToDefault("cl_railtype");
	Cvar_SetToDefault("cl_railred");
	Cvar_SetToDefault("cl_railgreen");
	Cvar_SetToDefault("cl_railblue");
	Cvar_SetToDefault("cl_footstep_override");

	EffectsSetMenuItemValues();
}

void Options_Effects_MenuInit(void)
{
	static const char *blood_names[] = { "None", "Puff", "Splat", "Bleed", "Gore", 0 };
	static const char *railtrail_names[] = { "Colored spiral", "Colored beam", "Colored devrail", 0 };

	int y = MENU_LINE_SIZE * 3;

	s_options_effects_menu.x = SCREEN_WIDTH * 0.5f;
	s_options_effects_menu.y = DEFAULT_MENU_Y; //mxd. Was SCREEN_HEIGHT * 0.5 - 58;
	s_options_effects_menu.nitems = 0;

	s_options_effects_header.generic.type	= MTYPE_SEPARATOR;
	s_options_effects_header.generic.name	= "Effects";
	s_options_effects_header.generic.x		= MENU_FONT_SIZE / 2 * strlen(s_options_effects_header.generic.name);
	s_options_effects_header.generic.y		= 0;
	
	s_options_effects_blood_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_effects_blood_box.generic.x			= 0;
	s_options_effects_blood_box.generic.y			= y;
	s_options_effects_blood_box.generic.name		= "Blood type";
	s_options_effects_blood_box.generic.callback	= BloodFunc;
	s_options_effects_blood_box.itemnames			= blood_names;
	s_options_effects_blood_box.generic.statusbar	= "Changes blood effect type";

	s_options_effects_oldexplosions_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_effects_oldexplosions_box.generic.x			= 0;
	s_options_effects_oldexplosions_box.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_options_effects_oldexplosions_box.generic.name		= "Old-style explosions";
	s_options_effects_oldexplosions_box.generic.callback	= OldExplosionFunc;
	s_options_effects_oldexplosions_box.itemnames			= yesno_names;
	s_options_effects_oldexplosions_box.generic.statusbar	= "Brings back those cheesy model explosions";

	s_options_effects_plasmaexplosound_box.generic.type			= MTYPE_SPINCONTROL;
	s_options_effects_plasmaexplosound_box.generic.x			= 0;
	s_options_effects_plasmaexplosound_box.generic.y			= y += MENU_LINE_SIZE;
	s_options_effects_plasmaexplosound_box.generic.name			= "Unique plasma explosion sound";
	s_options_effects_plasmaexplosound_box.generic.callback		= PlasmaExploSoundFunc;
	s_options_effects_plasmaexplosound_box.itemnames			= yesno_names;
	s_options_effects_plasmaexplosound_box.generic.statusbar	= "Gives Phalanx Cannon plasma explosions a unique sound";

	s_options_effects_itembob_box.generic.type					= MTYPE_SPINCONTROL;
	s_options_effects_itembob_box.generic.x						= 0;
	s_options_effects_itembob_box.generic.y						= y += MENU_LINE_SIZE;
	s_options_effects_itembob_box.generic.name					= "Item bobbing";
	s_options_effects_itembob_box.generic.callback				= ItemBobFunc;
	s_options_effects_itembob_box.itemnames						= yesno_names;
	s_options_effects_itembob_box.generic.statusbar				= "Adds bobbing effect to rotating items";

	s_options_effects_decal_slider.generic.type					= MTYPE_SLIDER;
	s_options_effects_decal_slider.generic.x					= 0;
	s_options_effects_decal_slider.generic.y					= y += 2 * MENU_LINE_SIZE;
	s_options_effects_decal_slider.generic.name					= "Decal quantity";
	s_options_effects_decal_slider.generic.callback				= DecalCallback;
	s_options_effects_decal_slider.minvalue						= 0;
	s_options_effects_decal_slider.maxvalue						= 20;
	s_options_effects_decal_slider.generic.statusbar			= "How many decals to display at once (max is 1000)";
	s_options_effects_decal_slider.cvar							= r_decals; //mxd

	s_options_effects_particle_comp_slider.generic.type			= MTYPE_SLIDER;
	s_options_effects_particle_comp_slider.generic.x			= 0;
	s_options_effects_particle_comp_slider.generic.y			= y += MENU_LINE_SIZE;
	s_options_effects_particle_comp_slider.generic.name			= "Particle effect complexity";
	s_options_effects_particle_comp_slider.generic.callback		= ParticleCompFunc;
	s_options_effects_particle_comp_slider.minvalue				= 1;
	s_options_effects_particle_comp_slider.maxvalue				= 5;
	s_options_effects_particle_comp_slider.generic.statusbar	= "Lower = faster performance";

	// Psychospaz's changeable rail trail
	s_options_effects_railtrail_box.generic.type			= MTYPE_SPINCONTROL;
	s_options_effects_railtrail_box.generic.x				= 0;
	s_options_effects_railtrail_box.generic.y				= y += 2 * MENU_LINE_SIZE;
	s_options_effects_railtrail_box.generic.name			= "Railtrail type";
	s_options_effects_railtrail_box.generic.callback		= RailTrailFunc;
	s_options_effects_railtrail_box.itemnames				= railtrail_names;
	s_options_effects_railtrail_box.generic.statusbar		= "Changes railgun particle effect";

	s_options_effects_railcolor_slider[0].generic.type		= MTYPE_SLIDER;
	s_options_effects_railcolor_slider[0].generic.x			= 0;
	s_options_effects_railcolor_slider[0].generic.y			= y += MENU_LINE_SIZE;
	s_options_effects_railcolor_slider[0].generic.name		= "Railtrail - red";
	s_options_effects_railcolor_slider[0].generic.callback	= RailColorRedFunc;
	s_options_effects_railcolor_slider[0].minvalue			= 0;
	s_options_effects_railcolor_slider[0].maxvalue			= 16;
	s_options_effects_railcolor_slider[0].generic.statusbar	= "Changes red component of railtrail";
	s_options_effects_railcolor_slider[0].cvar				= cl_railred; //mxd
	s_options_effects_railcolor_slider[0].generic.ownerdraw = RailColorPreviewDrawFunc; //mxd

	s_options_effects_railcolor_slider[1].generic.type		= MTYPE_SLIDER;
	s_options_effects_railcolor_slider[1].generic.x			= 0;
	s_options_effects_railcolor_slider[1].generic.y			= y += MENU_LINE_SIZE;
	s_options_effects_railcolor_slider[1].generic.name		= "Railtrail - green";
	s_options_effects_railcolor_slider[1].generic.callback	= RailColorGreenFunc;
	s_options_effects_railcolor_slider[1].minvalue			= 0;
	s_options_effects_railcolor_slider[1].maxvalue			= 16;
	s_options_effects_railcolor_slider[1].generic.statusbar	= "Changes green component of railtrail";
	s_options_effects_railcolor_slider[1].cvar				= cl_railgreen; //mxd

	s_options_effects_railcolor_slider[2].generic.type		= MTYPE_SLIDER;
	s_options_effects_railcolor_slider[2].generic.x			= 0;
	s_options_effects_railcolor_slider[2].generic.y			= y += MENU_LINE_SIZE;
	s_options_effects_railcolor_slider[2].generic.name		= "Railtrail - blue";
	s_options_effects_railcolor_slider[2].generic.callback	= RailColorBlueFunc;
	s_options_effects_railcolor_slider[2].minvalue			= 0;
	s_options_effects_railcolor_slider[2].maxvalue			= 16;
	s_options_effects_railcolor_slider[2].generic.statusbar	= "Changes blue component of railtrail";
	s_options_effects_railcolor_slider[2].cvar				= cl_railblue; //mxd

	// Foostep override option
	s_options_effects_footstep_box.generic.type			= MTYPE_SPINCONTROL;
	s_options_effects_footstep_box.generic.x			= 0;
	s_options_effects_footstep_box.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_options_effects_footstep_box.generic.name			= "Override footstep sounds";
	s_options_effects_footstep_box.generic.callback		= FootStepFunc;
	s_options_effects_footstep_box.itemnames			= yesno_names;
	s_options_effects_footstep_box.generic.statusbar	= "Use footstep sounds overrides defined in texsurfs.txt";

	s_options_effects_defaults_action.generic.type		= MTYPE_ACTION;
	s_options_effects_defaults_action.generic.name		= "Reset to defaults";
	s_options_effects_defaults_action.generic.flags		= QMF_LEFT_JUSTIFY; //mxd
	s_options_effects_defaults_action.generic.x			= -MENU_FONT_SIZE * strlen(s_options_effects_defaults_action.generic.name); //mxd. Was MENU_FONT_SIZE;
	s_options_effects_defaults_action.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_options_effects_defaults_action.generic.callback	= EffectsResetDefaultsFunc;
	s_options_effects_defaults_action.generic.statusbar	= "Resets all effects settings to internal defaults";

	s_options_effects_back_action.generic.type			= MTYPE_ACTION;
	s_options_effects_back_action.generic.flags			= QMF_LEFT_JUSTIFY; //mxd
	s_options_effects_back_action.generic.name			= (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_OPTIONS); //mxd
	s_options_effects_back_action.generic.x				= UI_CenteredX(&s_options_effects_back_action.generic, s_options_effects_menu.x); //mxd. Was MENU_FONT_SIZE;
	s_options_effects_back_action.generic.y				= y += 2 * MENU_LINE_SIZE;
	s_options_effects_back_action.generic.callback		= UI_BackMenu;

	//mxd. Setup rail color preview frame position and size
	rail_color_frame_x = s_options_effects_menu.x + s_options_effects_railcolor_slider[0].generic.x + MENU_FONT_SIZE * 18;
	rail_color_frame_y = s_options_effects_menu.y + s_options_effects_railcolor_slider[0].generic.y;
	rail_color_frame_size = s_options_effects_railcolor_slider[2].generic.y + MENU_FONT_SIZE - s_options_effects_railcolor_slider[0].generic.y;

	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_header);
	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_blood_box);
	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_oldexplosions_box);
	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_plasmaexplosound_box);
	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_itembob_box);
	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_decal_slider);
	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_particle_comp_slider);
	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_railtrail_box);
	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_railcolor_slider[0]);
	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_railcolor_slider[1]);
	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_railcolor_slider[2]);
	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_footstep_box);
	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_defaults_action);
	Menu_AddItem(&s_options_effects_menu, (void *)&s_options_effects_back_action);

	EffectsSetMenuItemValues();
}

void Options_Effects_MenuDraw(void)
{
	Menu_DrawBanner("m_banner_options");

	Menu_AdjustCursor(&s_options_effects_menu, 1);
	Menu_Draw(&s_options_effects_menu);
}

const char *Options_Effects_MenuKey(int key)
{
	return Default_MenuKey(&s_options_effects_menu, key);
}

void M_Menu_Options_Effects_f(void)
{
	Options_Effects_MenuInit();
	UI_PushMenu(Options_Effects_MenuDraw, Options_Effects_MenuKey);
}