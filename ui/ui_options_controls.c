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

// ui_options_controls.c -- the controls options menu

#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"

static menuframework_s	s_options_controls_menu;
static menuseparator_s	s_options_controls_header;
static menuslider_s		s_options_controls_sensitivity_slider;
static menulist_s		s_options_controls_alwaysrun_box;
static menulist_s		s_options_controls_thirdperson_box;
static menuslider_s		s_options_controls_thirdperson_distance_slider;
static menuslider_s		s_options_controls_thirdperson_angle_slider;
static menulist_s		s_options_controls_mouseacceleration_box; //mxd
static menulist_s		s_options_controls_mousesmoothing_box; //mxd
static menulist_s		s_options_controls_invertmouse_box;
static menulist_s		s_options_controls_autosensitivity_box;
static menulist_s		s_options_controls_lookspring_box;
static menulist_s		s_options_controls_lookstrafe_box;
static menulist_s		s_options_controls_freelook_box;
static menuaction_s		s_options_controls_customize_keys_action;
static menuaction_s		s_options_controls_defaults_action;
static menuaction_s		s_options_controls_back_action;

#pragma region ======================= Menu item callbacks

static void MouseSpeedFunc(void *unused)
{
	Cvar_SetValue("sensitivity", s_options_controls_sensitivity_slider.curvalue / 2.0f);
}

static void AlwaysRunFunc(void *unused)
{
	Cvar_SetValue("cl_run", s_options_controls_alwaysrun_box.curvalue);
}

// Psychospaz's chaseam
static void ThirdPersonFunc(void *unused)
{
	Cvar_SetValue("cg_thirdperson", s_options_controls_thirdperson_box.curvalue);
}

static void ThirdPersonDistFunc(void *unused)
{
	Cvar_SetValue("cg_thirdperson_dist", (int)(s_options_controls_thirdperson_distance_slider.curvalue * 25));
}

static void ThirdPersonAngleFunc(void *unused)
{
	Cvar_SetValue("cg_thirdperson_angle", (int)(s_options_controls_thirdperson_angle_slider.curvalue * 10));
}

static void FreeLookFunc(void *unused)
{
	Cvar_SetValue("freelook", s_options_controls_freelook_box.curvalue);
}

static void MouseAccelerationFunc(void *unused) //mxd
{
	Cvar_SetValue("m_acceleration", s_options_controls_mouseacceleration_box.curvalue);
}

static void MouseSmoothingFunc(void *unused) //mxd
{
	Cvar_SetValue("m_filter", s_options_controls_mousesmoothing_box.curvalue);
}

static void InvertMouseFunc(void *unused)
{
	Cvar_SetValue("m_pitch", -m_pitch->value);
}

static void AutosensitivityFunc(void *unused)
{
	Cvar_SetValue("autosensitivity", s_options_controls_autosensitivity_box.curvalue);
}

static void LookspringFunc(void *unused)
{
	Cvar_SetValue("lookspring", !lookspring->value);
}

static void LookstrafeFunc(void *unused)
{
	Cvar_SetValue("lookstrafe", !lookstrafe->value);
}

static void CustomizeControlsFunc(void *unused)
{
	M_Menu_Keys_f();
}

#pragma endregion

static void ControlsSetMenuItemValues(void)
{
	s_options_controls_sensitivity_slider.curvalue = Cvar_VariableValue("sensitivity") * 2;
	Cvar_SetValue("m_acceleration", ClampCvar(0, 1, Cvar_VariableValue("m_acceleration"))); //mxd
	s_options_controls_mouseacceleration_box.curvalue = Cvar_VariableValue("m_acceleration");
	Cvar_SetValue("m_filter", ClampCvar(0, 1, Cvar_VariableValue("m_filter"))); //mxd
	s_options_controls_mousesmoothing_box.curvalue = Cvar_VariableValue("m_filter");
	s_options_controls_invertmouse_box.curvalue = Cvar_VariableValue("m_pitch") < 0;

	Cvar_SetValue("autosensitivity", ClampCvar(0, 1, Cvar_VariableValue("autosensitivity")));
	s_options_controls_autosensitivity_box.curvalue = Cvar_VariableValue("autosensitivity");

	Cvar_SetValue("cg_thirdperson", ClampCvar(0, 1, Cvar_VariableValue("cg_thirdperson")));
	s_options_controls_thirdperson_box.curvalue				= Cvar_VariableValue("cg_thirdperson");
	s_options_controls_thirdperson_distance_slider.curvalue	= Cvar_VariableValue("cg_thirdperson_dist") / 25;
	s_options_controls_thirdperson_angle_slider.curvalue	= Cvar_VariableValue("cg_thirdperson_angle") / 10;

	Cvar_SetValue("cl_run", ClampCvar(0, 1, Cvar_VariableValue("cl_run")));
	s_options_controls_alwaysrun_box.curvalue = Cvar_VariableValue("cl_run");

	Cvar_SetValue("lookspring", ClampCvar(0, 1, Cvar_VariableValue("lookspring")));
	s_options_controls_lookspring_box.curvalue = Cvar_VariableValue("lookspring");

	Cvar_SetValue("lookstrafe", ClampCvar(0, 1, Cvar_VariableValue("lookstrafe")));
	s_options_controls_lookstrafe_box.curvalue = Cvar_VariableValue("lookstrafe");

	Cvar_SetValue("freelook", ClampCvar(0, 1, Cvar_VariableValue("freelook")));
	s_options_controls_freelook_box.curvalue = Cvar_VariableValue("freelook");
}

static void ControlsResetDefaultsFunc(void *unused)
{
	//Cvar_SetToDefault("sensitivity");
	//Cvar_SetToDefault("m_pitch");
	Cvar_SetToDefault("autosensitivity");
	Cvar_SetToDefault("m_filter"); //mxd
	Cvar_SetToDefault("m_acceleration"); //mxd

	Cvar_SetToDefault("cg_thirdperson");
	Cvar_SetToDefault("cg_thirdperson_dist");
	Cvar_SetToDefault("cg_thirdperson_angle");
	//Cvar_SetToDefault("cl_run");
	//Cvar_SetToDefault("lookspring");
	//Cvar_SetToDefault("lookstrafe");
	//Cvar_SetToDefault("freelook");

	Cbuf_AddText("exec defaultbinds.cfg\n"); // Reset keybindings 
	Cbuf_Execute();

	ControlsSetMenuItemValues();
}

void Options_Controls_MenuInit(void)
{
	int y = MENU_LINE_SIZE * 3;

	s_options_controls_menu.x = SCREEN_WIDTH * 0.5f;
	s_options_controls_menu.y = DEFAULT_MENU_Y; //mxd. Was SCREEN_HEIGHT * 0.5 - 58;
	s_options_controls_menu.nitems = 0;

	s_options_controls_header.generic.type	= MTYPE_SEPARATOR;
	s_options_controls_header.generic.name	= "Controls";
	s_options_controls_header.generic.x		= MENU_FONT_SIZE / 2 * strlen(s_options_controls_header.generic.name);
	s_options_controls_header.generic.y		= 0;

	s_options_controls_sensitivity_slider.generic.type		= MTYPE_SLIDER;
	s_options_controls_sensitivity_slider.generic.x			= 0;
	s_options_controls_sensitivity_slider.generic.y			= y;
	s_options_controls_sensitivity_slider.generic.name		= "Mouse sensitivity";
	s_options_controls_sensitivity_slider.generic.callback	= MouseSpeedFunc;
	s_options_controls_sensitivity_slider.minvalue			= 2;
	s_options_controls_sensitivity_slider.maxvalue			= 22;
	s_options_controls_sensitivity_slider.numdecimals		= 1; //mxd
	s_options_controls_sensitivity_slider.generic.statusbar	= "Changes sensitivity of mouse for looking around";
	s_options_controls_sensitivity_slider.cvar				= sensitivity; //mxd

	s_options_controls_mouseacceleration_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_controls_mouseacceleration_box.generic.x			= 0;
	s_options_controls_mouseacceleration_box.generic.y			= y += MENU_LINE_SIZE;
	s_options_controls_mouseacceleration_box.generic.name		= "Mouse acceleration";
	s_options_controls_mouseacceleration_box.generic.callback	= MouseAccelerationFunc;
	s_options_controls_mouseacceleration_box.itemnames			= yesno_names;
	s_options_controls_mouseacceleration_box.generic.statusbar	= "Enables SDL2 mouse acceleration";

	s_options_controls_mousesmoothing_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_controls_mousesmoothing_box.generic.x			= 0;
	s_options_controls_mousesmoothing_box.generic.y			= y += MENU_LINE_SIZE;
	s_options_controls_mousesmoothing_box.generic.name		= "Mouse smoothing";
	s_options_controls_mousesmoothing_box.generic.callback	= MouseSmoothingFunc;
	s_options_controls_mousesmoothing_box.itemnames			= yesno_names;
	s_options_controls_mousesmoothing_box.generic.statusbar	= "Interpolates mouse movement";

	s_options_controls_invertmouse_box.generic.type			= MTYPE_SPINCONTROL;
	s_options_controls_invertmouse_box.generic.x			= 0;
	s_options_controls_invertmouse_box.generic.y			= y += MENU_LINE_SIZE;
	s_options_controls_invertmouse_box.generic.name			= "Invert mouse";
	s_options_controls_invertmouse_box.generic.callback		= InvertMouseFunc;
	s_options_controls_invertmouse_box.itemnames			= yesno_names;
	s_options_controls_invertmouse_box.generic.statusbar	= "Inverts mouse y-axis movement";

	s_options_controls_autosensitivity_box.generic.type			= MTYPE_SPINCONTROL;
	s_options_controls_autosensitivity_box.generic.x			= 0;
	s_options_controls_autosensitivity_box.generic.y			= y += MENU_LINE_SIZE;
	s_options_controls_autosensitivity_box.generic.name			= "Scale mouse to FOV";
	s_options_controls_autosensitivity_box.generic.callback		= AutosensitivityFunc;
	s_options_controls_autosensitivity_box.itemnames			= yesno_names;
	s_options_controls_autosensitivity_box.generic.statusbar	= "Adjusts mouse sensitivity to zoomed FOV";

	s_options_controls_thirdperson_box.generic.type			= MTYPE_SPINCONTROL;
	s_options_controls_thirdperson_box.generic.x			= 0;
	s_options_controls_thirdperson_box.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_options_controls_thirdperson_box.generic.name			= "Third-person camera";
	s_options_controls_thirdperson_box.generic.callback		= ThirdPersonFunc;
	s_options_controls_thirdperson_box.itemnames			= yesno_names;
	s_options_controls_thirdperson_box.generic.statusbar	= "Enables third-person camera mode";

	s_options_controls_thirdperson_distance_slider.generic.type			= MTYPE_SLIDER;
	s_options_controls_thirdperson_distance_slider.generic.x			= 0;
	s_options_controls_thirdperson_distance_slider.generic.y			= y += MENU_LINE_SIZE;
	s_options_controls_thirdperson_distance_slider.generic.name			= "Camera distance";
	s_options_controls_thirdperson_distance_slider.generic.callback		= ThirdPersonDistFunc;
	s_options_controls_thirdperson_distance_slider.minvalue				= 1;
	s_options_controls_thirdperson_distance_slider.maxvalue				= 5;
	s_options_controls_thirdperson_distance_slider.generic.statusbar	= "Changes camera distance in third-person mode";
	s_options_controls_thirdperson_distance_slider.cvar					= cg_thirdperson_dist; //mxd

	s_options_controls_thirdperson_angle_slider.generic.type		= MTYPE_SLIDER;
	s_options_controls_thirdperson_angle_slider.generic.x			= 0;
	s_options_controls_thirdperson_angle_slider.generic.y			= y += MENU_LINE_SIZE;
	s_options_controls_thirdperson_angle_slider.generic.name		= "Camera angle";
	s_options_controls_thirdperson_angle_slider.generic.callback	= ThirdPersonAngleFunc;
	s_options_controls_thirdperson_angle_slider.minvalue			= 0;
	s_options_controls_thirdperson_angle_slider.maxvalue			= 4;
	s_options_controls_thirdperson_angle_slider.generic.statusbar	= "Changes camera angle in third-person mode";
	s_options_controls_thirdperson_angle_slider.cvar				= cg_thirdperson_angle; //mxd

	s_options_controls_alwaysrun_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_controls_alwaysrun_box.generic.x			= 0;
	s_options_controls_alwaysrun_box.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_options_controls_alwaysrun_box.generic.name		= "Always run";
	s_options_controls_alwaysrun_box.generic.callback	= AlwaysRunFunc;
	s_options_controls_alwaysrun_box.itemnames			= yesno_names;
	s_options_controls_alwaysrun_box.generic.statusbar	= "Enables running as default movement";

	s_options_controls_lookspring_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_controls_lookspring_box.generic.x			= 0;
	s_options_controls_lookspring_box.generic.y			= y += MENU_LINE_SIZE;
	s_options_controls_lookspring_box.generic.name		= "Lookspring";
	s_options_controls_lookspring_box.generic.callback	= LookspringFunc;
	s_options_controls_lookspring_box.itemnames			= yesno_names;
	s_options_controls_lookspring_box.generic.statusbar = "Centers the view vertically after moving forward";

	s_options_controls_lookstrafe_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_controls_lookstrafe_box.generic.x			= 0;
	s_options_controls_lookstrafe_box.generic.y			= y += MENU_LINE_SIZE;
	s_options_controls_lookstrafe_box.generic.name		= "Lookstrafe";
	s_options_controls_lookstrafe_box.generic.callback	= LookstrafeFunc;
	s_options_controls_lookstrafe_box.itemnames			= yesno_names;
	s_options_controls_lookstrafe_box.generic.statusbar = "Moving the mouse horizontally strafes the player";

	s_options_controls_freelook_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_controls_freelook_box.generic.x			= 0;
	s_options_controls_freelook_box.generic.y			= y += MENU_LINE_SIZE;
	s_options_controls_freelook_box.generic.name		= "Free look";
	s_options_controls_freelook_box.generic.callback	= FreeLookFunc;
	s_options_controls_freelook_box.itemnames			= yesno_names;
	s_options_controls_freelook_box.generic.statusbar	= "Enables free head movement with mouse";

	s_options_controls_customize_keys_action.generic.type		= MTYPE_ACTION;
	s_options_controls_customize_keys_action.generic.flags		= QMF_LEFT_JUSTIFY; //mxd
	s_options_controls_customize_keys_action.generic.name		= "Customize controls";
	s_options_controls_customize_keys_action.generic.x			= -MENU_FONT_SIZE * strlen(s_options_controls_customize_keys_action.generic.name); //mxd. Was MENU_FONT_SIZE;
	s_options_controls_customize_keys_action.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_options_controls_customize_keys_action.generic.callback	= CustomizeControlsFunc;

	s_options_controls_defaults_action.generic.type			= MTYPE_ACTION;
	s_options_controls_defaults_action.generic.flags		= QMF_LEFT_JUSTIFY; //mxd
	s_options_controls_defaults_action.generic.name			= "Reset to defaults";
	s_options_controls_defaults_action.generic.x			= -MENU_FONT_SIZE * strlen(s_options_controls_defaults_action.generic.name); //mxd. Was MENU_FONT_SIZE;
	s_options_controls_defaults_action.generic.y			= y += 2 * MENU_LINE_SIZE;
	s_options_controls_defaults_action.generic.callback		= ControlsResetDefaultsFunc;
	s_options_controls_defaults_action.generic.statusbar	= "Resets all control settings to internal defaults";

	s_options_controls_back_action.generic.type		= MTYPE_ACTION;
	s_options_controls_back_action.generic.flags	= QMF_LEFT_JUSTIFY; //mxd
	s_options_controls_back_action.generic.name		= (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_OPTIONS); //mxd
	s_options_controls_back_action.generic.x		= UI_CenteredX(&s_options_controls_back_action.generic, s_options_controls_menu.x); //mxd. Was MENU_FONT_SIZE;
	s_options_controls_back_action.generic.y		= y += 2 * MENU_LINE_SIZE;
	s_options_controls_back_action.generic.callback	= UI_BackMenu;

	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_header);
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_sensitivity_slider);
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_mouseacceleration_box); //mxd
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_mousesmoothing_box); //mxd
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_invertmouse_box);
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_autosensitivity_box);
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_thirdperson_box);
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_thirdperson_distance_slider);
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_thirdperson_angle_slider);
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_alwaysrun_box);
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_lookspring_box);
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_lookstrafe_box);
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_freelook_box);
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_customize_keys_action);
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_defaults_action);
	Menu_AddItem(&s_options_controls_menu, (void *)&s_options_controls_back_action);

	ControlsSetMenuItemValues();
}

void Options_Controls_MenuDraw(void)
{
	Menu_DrawBanner("m_banner_options");

	Menu_AdjustCursor(&s_options_controls_menu, 1);
	Menu_Draw(&s_options_controls_menu);
}

const char *Options_Controls_MenuKey(int key)
{
	return Default_MenuKey(&s_options_controls_menu, key);
}

void M_Menu_Options_Controls_f(void)
{
	Options_Controls_MenuInit();
	UI_PushMenu(Options_Controls_MenuDraw, Options_Controls_MenuKey);
}