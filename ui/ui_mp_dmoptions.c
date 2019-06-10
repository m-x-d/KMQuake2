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

// ui_mp_dmoptions.c -- DM options menu 

#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"

static char dmoptions_statusbar[128];

static menuframework_s s_dmoptions_menu;

static menuseparator_s	s_dmflags_title; //mxd
static menulist_s	s_friendlyfire_box;
static menulist_s	s_falls_box;
static menulist_s	s_weapons_stay_box;
static menulist_s	s_instant_powerups_box;
static menulist_s	s_powerups_box;
static menulist_s	s_health_box;
static menulist_s	s_spawn_farthest_box;
static menulist_s	s_teamplay_box;
static menulist_s	s_samelevel_box;
static menulist_s	s_force_respawn_box;
static menulist_s	s_armor_box;
static menulist_s	s_allow_exit_box;
static menulist_s	s_infinite_ammo_box;
static menulist_s	s_fixed_fov_box;
static menulist_s	s_quad_drop_box;

//Xatrix
static menulist_s	s_quadfire_drop_box;

//ROGUE
static menulist_s	s_no_mines_box;
static menulist_s	s_no_nukes_box;
static menulist_s	s_stack_double_box;
static menulist_s	s_no_spheres_box;

// CTF
static menulist_s	s_ctf_forceteam_box;
static menulist_s	s_ctf_armor_protect_box;
static menulist_s	s_ctf_notechs_box;

static menuaction_s	s_dmoptions_back_action;
extern	menulist_s	s_rules_box;

#define DF_CTF_FORCEJOIN	131072
#define DF_ARMOR_PROTECT	262144
#define DF_CTF_NO_TECH		524288

qboolean CTFMenuMode(void)
{
	const qboolean roguegame = FS_RoguePath(); //mxd
	return ((roguegame && s_rules_box.curvalue >= 3) || (!roguegame && s_rules_box.curvalue >= 2));
}

static void DMFlagCallback(void *self)
{
	menulist_s *f = (menulist_s *)self;
	int bit = 0;
	int flags = Cvar_VariableValue("dmflags");

	if (f == &s_friendlyfire_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_FRIENDLY_FIRE;
		else
			flags |= DF_NO_FRIENDLY_FIRE;
	}
	else if (f == &s_falls_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_FALLING;
		else
			flags |= DF_NO_FALLING;
	}
	else if (f == &s_weapons_stay_box) 
	{
		bit = DF_WEAPONS_STAY;
	}
	else if (f == &s_instant_powerups_box)
	{
		bit = DF_INSTANT_ITEMS;
	}
	else if (f == &s_allow_exit_box)
	{
		bit = DF_ALLOW_EXIT;
	}
	else if (f == &s_powerups_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_ITEMS;
		else
			flags |= DF_NO_ITEMS;
	}
	else if (f == &s_health_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_HEALTH;
		else
			flags |= DF_NO_HEALTH;
	}
	else if (f == &s_spawn_farthest_box)
	{
		bit = DF_SPAWN_FARTHEST;
	}
	else if (f == &s_teamplay_box)
	{
		if (f->curvalue == 1)
		{
			flags |= DF_SKINTEAMS;
			flags &= ~DF_MODELTEAMS;
		}
		else if (f->curvalue == 2)
		{
			flags |= DF_MODELTEAMS;
			flags &= ~DF_SKINTEAMS;
		}
		else
		{
			flags &= ~(DF_MODELTEAMS | DF_SKINTEAMS);
		}
	}
	else if (f == &s_samelevel_box)
	{
		bit = DF_SAME_LEVEL;
	}
	else if (f == &s_force_respawn_box)
	{
		bit = DF_FORCE_RESPAWN;
	}
	else if (f == &s_armor_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_ARMOR;
		else
			flags |= DF_NO_ARMOR;
	}
	else if (f == &s_infinite_ammo_box)
	{
		bit = DF_INFINITE_AMMO;
	}
	else if (f == &s_fixed_fov_box)
	{
		bit = DF_FIXED_FOV;
	}
	else if (f == &s_quad_drop_box)
	{
		bit = DF_QUAD_DROP;
	}
	else if (FS_ModType("xatrix"))	// XATRIX // Knightmare added
	{
		if (f == &s_quadfire_drop_box)
			bit = DF_QUADFIRE_DROP;
	}
	else if (FS_RoguePath())		// ROGUE
	{
		if (f == &s_no_mines_box)
			bit = DF_NO_MINES;
		else if (f == &s_no_nukes_box)
			bit = DF_NO_NUKES;
		else if (f == &s_stack_double_box)
			bit = DF_NO_STACK_DOUBLE;
		else if (f == &s_no_spheres_box)
			bit = DF_NO_SPHERES;
	}
	else if (CTFMenuMode())	// Knightmare added-  CTF flags
	{
		if (f == &s_ctf_forceteam_box)
			bit = DF_CTF_FORCEJOIN;
		else if (f == &s_ctf_armor_protect_box)
			bit = DF_ARMOR_PROTECT;
		else if (f == &s_ctf_notechs_box)
			bit = DF_CTF_NO_TECH;
	}

	if (f)
	{
		if (f->curvalue == 0)
			flags &= ~bit;
		else
			flags |= bit;
	}

	Cvar_SetValue("dmflags", flags);
	Com_sprintf(dmoptions_statusbar, sizeof(dmoptions_statusbar), "dmflags = %d", flags);
}

//mxd
void DMOptions_SetupMenuItem(menulist_s *item, const char *name, const int y, const int value)
{
	item->generic.type = MTYPE_SPINCONTROL;
	item->generic.x = 0;
	item->generic.y = y;
	item->generic.name = name;
	item->generic.callback = DMFlagCallback;
	item->itemnames = yesno_names;
	item->curvalue = value;
}

void DMOptions_MenuInit(void)
{
	static const char *teamplay_names[] = { "Disabled", "By skin", "By model", 0 };

	const int dmflags = Cvar_VariableValue("dmflags");
	int y = 0;

	s_dmoptions_menu.x = SCREEN_WIDTH * 0.5f;
	s_dmoptions_menu.y = DEFAULT_MENU_Y; //mxd //SCREEN_HEIGHT * 0.5f - 100;
	s_dmoptions_menu.nitems = 0;

	//mxd. Added title
	s_dmflags_title.generic.type = MTYPE_SEPARATOR;
	s_dmflags_title.generic.flags = QMF_LEFT_JUSTIFY;
	s_dmflags_title.generic.name = "Deathmatch flags";
	s_dmflags_title.generic.x = UI_CenteredX(&s_dmflags_title.generic, s_dmoptions_menu.x);
	s_dmflags_title.generic.y = y;

	DMOptions_SetupMenuItem(&s_falls_box, "Falling damage", y += 2 * MENU_LINE_SIZE, ((dmflags & DF_NO_FALLING) == 0));
	DMOptions_SetupMenuItem(&s_weapons_stay_box, "Weapons stay", y += MENU_LINE_SIZE, ((dmflags & DF_WEAPONS_STAY) != 0));
	DMOptions_SetupMenuItem(&s_instant_powerups_box, "Instant powerups", y += MENU_LINE_SIZE, ((dmflags & DF_INSTANT_ITEMS) != 0));
	DMOptions_SetupMenuItem(&s_powerups_box, "Allow powerups", y += MENU_LINE_SIZE, ((dmflags & DF_NO_ITEMS) == 0));
	DMOptions_SetupMenuItem(&s_health_box, "Allow health", y += MENU_LINE_SIZE, ((dmflags & DF_NO_HEALTH) == 0));
	DMOptions_SetupMenuItem(&s_armor_box, "Allow armor", y += MENU_LINE_SIZE, ((dmflags & DF_NO_ARMOR) == 0));
	DMOptions_SetupMenuItem(&s_spawn_farthest_box, "Spawn farthest", y += MENU_LINE_SIZE, ((dmflags & DF_SPAWN_FARTHEST) != 0));
	DMOptions_SetupMenuItem(&s_samelevel_box, "Same map", y += MENU_LINE_SIZE, ((dmflags & DF_SAME_LEVEL) != 0));
	DMOptions_SetupMenuItem(&s_force_respawn_box, "Force respawn", y += MENU_LINE_SIZE, ((dmflags & DF_FORCE_RESPAWN) != 0));

	DMOptions_SetupMenuItem(&s_teamplay_box, "Teamplay", y += MENU_LINE_SIZE, (dmflags & DF_SKINTEAMS) ? 1 : ((dmflags & DF_MODELTEAMS) ? 2 : 0));
	s_teamplay_box.itemnames = teamplay_names;

	DMOptions_SetupMenuItem(&s_allow_exit_box, "Allow exit", y += MENU_LINE_SIZE, ((dmflags & DF_ALLOW_EXIT) != 0));
	DMOptions_SetupMenuItem(&s_infinite_ammo_box, "Infinite ammo", y += MENU_LINE_SIZE, ((dmflags & DF_INFINITE_AMMO) != 0));
	DMOptions_SetupMenuItem(&s_fixed_fov_box, "Fixed FOV", y += MENU_LINE_SIZE, ((dmflags & DF_FIXED_FOV) != 0));
	DMOptions_SetupMenuItem(&s_quad_drop_box, "Quad drop", y += MENU_LINE_SIZE, ((dmflags & DF_QUAD_DROP) != 0));
	DMOptions_SetupMenuItem(&s_friendlyfire_box, "Friendly fire", y += MENU_LINE_SIZE, ((dmflags & DF_NO_FRIENDLY_FIRE) == 0));

	// Knightmare added
	if (FS_ModType("xatrix")) // XATRIX
	{
		DMOptions_SetupMenuItem(&s_quadfire_drop_box, "Dualfire drop", y += MENU_LINE_SIZE, ((dmflags & DF_QUADFIRE_DROP) != 0));
	}
	else if (FS_RoguePath()) // ROGUE
	{
		DMOptions_SetupMenuItem(&s_no_mines_box, "Remove mines", y += MENU_LINE_SIZE, ((dmflags & DF_NO_MINES) != 0));
		DMOptions_SetupMenuItem(&s_no_nukes_box, "Remove nukes", y += MENU_LINE_SIZE, ((dmflags & DF_NO_NUKES) != 0));
		DMOptions_SetupMenuItem(&s_stack_double_box, "2x/4x stacking off", y += MENU_LINE_SIZE, ((dmflags & DF_NO_STACK_DOUBLE) != 0));
		DMOptions_SetupMenuItem(&s_no_spheres_box, "Remove spheres", y += MENU_LINE_SIZE, ((dmflags & DF_NO_SPHERES) != 0));
	}
	else if (CTFMenuMode()) // CTF
	{
		DMOptions_SetupMenuItem(&s_ctf_forceteam_box, "CTF: force team join", y += MENU_LINE_SIZE, ((dmflags & DF_CTF_FORCEJOIN) != 0));
		DMOptions_SetupMenuItem(&s_ctf_armor_protect_box, "CTF: team armor protect", y += MENU_LINE_SIZE, ((dmflags & DF_ARMOR_PROTECT) != 0));
		DMOptions_SetupMenuItem(&s_ctf_notechs_box, "CTF: disable techs", y += MENU_LINE_SIZE, ((dmflags & DF_CTF_NO_TECH) != 0));
	}

	s_dmoptions_back_action.generic.type = MTYPE_ACTION;
	s_dmoptions_back_action.generic.name = (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_STARTSERVER); //mxd
	s_dmoptions_back_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_dmoptions_back_action.generic.x = UI_CenteredX(&s_dmoptions_back_action.generic, s_dmoptions_menu.x); //mxd. Draw centered
	s_dmoptions_back_action.generic.y = y += 2 * MENU_LINE_SIZE;
	s_dmoptions_back_action.generic.callback = UI_BackMenu;

	Menu_AddItem(&s_dmoptions_menu, &s_dmflags_title); //mxd
	Menu_AddItem(&s_dmoptions_menu, &s_falls_box);
	Menu_AddItem(&s_dmoptions_menu, &s_weapons_stay_box);
	Menu_AddItem(&s_dmoptions_menu, &s_instant_powerups_box);
	Menu_AddItem(&s_dmoptions_menu, &s_powerups_box);
	Menu_AddItem(&s_dmoptions_menu, &s_health_box);
	Menu_AddItem(&s_dmoptions_menu, &s_armor_box);
	Menu_AddItem(&s_dmoptions_menu, &s_spawn_farthest_box);
	Menu_AddItem(&s_dmoptions_menu, &s_samelevel_box);
	Menu_AddItem(&s_dmoptions_menu, &s_force_respawn_box);
	Menu_AddItem(&s_dmoptions_menu, &s_teamplay_box);
	Menu_AddItem(&s_dmoptions_menu, &s_allow_exit_box);
	Menu_AddItem(&s_dmoptions_menu, &s_infinite_ammo_box);
	Menu_AddItem(&s_dmoptions_menu, &s_fixed_fov_box);
	Menu_AddItem(&s_dmoptions_menu, &s_quad_drop_box);
	Menu_AddItem(&s_dmoptions_menu, &s_friendlyfire_box);

	
	if (FS_ModType("xatrix"))	 // XATRIX
	{
		Menu_AddItem(&s_dmoptions_menu, &s_quadfire_drop_box);
	}
	else if (FS_RoguePath())	 // ROGUE
	{
		Menu_AddItem(&s_dmoptions_menu, &s_no_mines_box);
		Menu_AddItem(&s_dmoptions_menu, &s_no_nukes_box);
		Menu_AddItem(&s_dmoptions_menu, &s_stack_double_box);
		Menu_AddItem(&s_dmoptions_menu, &s_no_spheres_box);
	}
	else if (CTFMenuMode()) // CTF
	{
		Menu_AddItem(&s_dmoptions_menu, &s_ctf_forceteam_box);
		Menu_AddItem(&s_dmoptions_menu, &s_ctf_armor_protect_box);
		Menu_AddItem(&s_dmoptions_menu, &s_ctf_notechs_box);
	}

	Menu_AddItem(&s_dmoptions_menu, &s_dmoptions_back_action);

	// Set the original dmflags statusbar
	DMFlagCallback(0);
	s_dmoptions_menu.statusbar = dmoptions_statusbar;
}

void DMOptions_MenuDraw(void)
{
	Menu_DrawBanner("m_banner_start_server"); // Added
	Menu_AdjustCursor(&s_dmoptions_menu, 1); //mxd
	Menu_Draw(&s_dmoptions_menu);
}

const char *DMOptions_MenuKey(int key)
{
	return Default_MenuKey(&s_dmoptions_menu, key);
}

void M_Menu_DMOptions_f(void)
{
	DMOptions_MenuInit();
	UI_PushMenu(DMOptions_MenuDraw, DMOptions_MenuKey);
}