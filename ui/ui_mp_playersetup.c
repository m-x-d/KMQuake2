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

// ui_playersetup.c -- the player setup menu 

#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"

extern menuframework_s	s_multiplayer_menu;

static menuframework_s	s_player_config_menu;
static menufield_s		s_player_name_field;
static menulist_s		s_player_model_box;
static menulist_s		s_player_skin_box;
static menulist_s		s_player_handedness_box;
static menulist_s		s_player_rate_box;
static menuseparator_s	s_player_skin_title;
static menuseparator_s	s_player_model_title;
static menuseparator_s	s_player_hand_title;
static menuseparator_s	s_player_rate_title;
static menuaction_s		s_player_back_action;

// Save skins and models here so as to not have to re-register every frame
struct model_s *playermodel;
struct model_s *weaponmodel;
struct image_s *playerskin;
char *currentweaponmodel;

#define MAX_DISPLAYNAME 16
#define MAX_PLAYERMODELS 1024
#define	NUM_SKINBOX_ITEMS 7

typedef struct
{
	int nskins;
	char **skindisplaynames;
	char displayname[MAX_DISPLAYNAME];
	char directory[MAX_QPATH];
} playermodelinfo_s;

static playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

static int rate_tbl[] = { 2500, 3200, 5000, 10000, 15000, 25000, 0 };
static const char *rate_names[] = { "28.8 Modem", "33.6 Modem", "56K/Single ISDN", "Dual ISDN", "Cable/DSL", "T1/LAN", "User defined", 0 };

#pragma region ======================= Menu item callbacks

static void HandednessCallback(void *unused)
{
	Cvar_SetValue("hand", s_player_handedness_box.curvalue);
}

static void RateCallback(void *unused)
{
	if (s_player_rate_box.curvalue != sizeof(rate_tbl) / sizeof(*rate_tbl) - 1)
		Cvar_SetValue("rate", rate_tbl[s_player_rate_box.curvalue]);
}

static void ModelCallback(void *unused)
{
	char scratch[MAX_QPATH];

	s_player_skin_box.itemnames = s_pmi[s_player_model_box.curvalue].skindisplaynames;
	s_player_skin_box.numitemnames = s_pmi[s_player_model_box.curvalue].nskins; //mxd. Added to make SpinControl_DoSlide correctly wrap around when navigating backwards
	s_player_skin_box.curvalue = 0;

	// Only register model and skin on starup or when changed
	Com_sprintf(scratch, sizeof(scratch), "players/%s/tris.md2", s_pmi[s_player_model_box.curvalue].directory);
	playermodel = R_RegisterModel(scratch);

	Com_sprintf(scratch, sizeof(scratch), "players/%s/%s.pcx", 
		s_pmi[s_player_model_box.curvalue].directory, 
		s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);

	playerskin = R_RegisterSkin(scratch);

	// Show current weapon model (if any)
	if (currentweaponmodel && strlen(currentweaponmodel))
	{
		Com_sprintf(scratch, sizeof(scratch), "players/%s/%s", 
			s_pmi[s_player_model_box.curvalue].directory, 
			currentweaponmodel);

		weaponmodel = R_RegisterModel(scratch);
		if (!weaponmodel)
		{
			Com_sprintf(scratch, sizeof(scratch), "players/%s/weapon.md2", s_pmi[s_player_model_box.curvalue].directory);
			weaponmodel = R_RegisterModel(scratch);
		}
	}
	else
	{
		Com_sprintf(scratch, sizeof(scratch), "players/%s/weapon.md2", s_pmi[s_player_model_box.curvalue].directory);
		weaponmodel = R_RegisterModel(scratch);
	}
}


// Only register skin on starup and when changed
static void SkinCallback(void *unused)
{
	char scratch[MAX_QPATH];

	Com_sprintf(scratch, sizeof(scratch), "players/%s/%s.pcx", 
		s_pmi[s_player_model_box.curvalue].directory,
		s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);

	playerskin = R_RegisterSkin(scratch);
}

//mxd
static void PlayerConfigBackFunc(void *unused)
{
	PConfigAccept();
	UI_BackMenu(unused);
}

#pragma endregion

static qboolean IconOfSkinExists(char *skin, char **files, int nfiles, char *suffix)
{
	char scratch[1024];

	Q_strncpyz(scratch, skin, sizeof(scratch));
	*strrchr(scratch, '.') = 0;
	Q_strncatz(scratch, suffix, sizeof(scratch));

	for (int i = 0; i < nfiles; i++)
		if (strcmp(files[i], scratch) == 0)
			return true;

	return false;
}

// Adds menu support for TGA, PNG and JPG skins
static qboolean IsValidSkin(char **filelist, int numFiles, int index)
{
	const int len = strlen(filelist[index]);

	if (   !strcmp(filelist[index] + max(len - 4, 0), ".pcx")
		|| !strcmp(filelist[index] + max(len - 4, 0), ".tga")
		|| !strcmp(filelist[index] + max(len - 4, 0), ".png")
		|| !strcmp(filelist[index] + max(len - 4, 0), ".jpg"))
	{
		if (   strcmp(filelist[index] + max(len - 6, 0), "_i.pcx")
			&& strcmp(filelist[index] + max(len - 6, 0), "_i.tga")
			&& strcmp(filelist[index] + max(len - 6, 0), "_i.png")
			&& strcmp(filelist[index] + max(len - 6, 0), "_i.jpg"))
		{
			if (   IconOfSkinExists(filelist[index], filelist, numFiles - 1, "_i.pcx")
				|| IconOfSkinExists(filelist[index], filelist, numFiles - 1, "_i.tga")
				|| IconOfSkinExists(filelist[index], filelist, numFiles - 1, "_i.png")
				|| IconOfSkinExists(filelist[index], filelist, numFiles - 1, "_i.jpg"))
			{
				return true;
			}
		}
	}

	return false;
}

static void PlayerConfig_ScanDirectories(void)
{
	char *path = NULL;
	
	s_numplayermodels = 0;

	// Loop back to here if there were no valid player models found in the selected path
	while(true)
	{
		int ndirs;
		char **dirnames = NULL;
		
		// Get a list of directories
		while((path = FS_NextPath(path)) != NULL)
		{
			char findname[1024];
			Com_sprintf(findname, sizeof(findname), "%s/players/*.*", path);

			dirnames = FS_ListFiles(findname, &ndirs, SFF_SUBDIR, 0);
			if (dirnames)
				break;
		}

		if (dirnames == NULL)
			return;

		// Go through the subdirectories
		int npms = min(ndirs, MAX_PLAYERMODELS);
		if (s_numplayermodels + npms > MAX_PLAYERMODELS)
			npms = MAX_PLAYERMODELS - s_numplayermodels;

		for (int i = 0; i < npms; i++)
		{
			if (dirnames[i] == 0)
				continue;

			// Check if dirnames[i] is already added to the s_pmi[i].directory list
			char *a = strrchr(dirnames[i], '/');
			char *b = strrchr(dirnames[i], '\\');
			char *c = max(a, b);

			qboolean already_added = false;
			for (int k = 0; k < s_numplayermodels; k++)
			{
				if (!strcmp(s_pmi[k].directory, c + 1))
				{
					already_added = true;
					break;
				}
			}

			if (already_added)
				continue; // todo: add any skins for this model not already listed to skindisplaynames

			// Verify the existence of tris.md2
			char scratch[1024];
			Q_strncpyz(scratch, dirnames[i], sizeof(scratch));
			Q_strncatz(scratch, "/tris.md2", sizeof(scratch));
			if (!Sys_FindFirst(scratch, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM))
			{
				free(dirnames[i]);
				dirnames[i] = 0;
				Sys_FindClose();

				continue;
			}
			Sys_FindClose();

			// Verify the existence of at least one skin
			int nimagefiles;
			Q_strncpyz(scratch, va("%s%s", dirnames[i], "/*.*"), sizeof(scratch)); // was "/*.pcx"
			char **imagenames = FS_ListFiles(scratch, &nimagefiles, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);

			if (!imagenames)
			{
				free(dirnames[i]);
				dirnames[i] = 0;

				continue;
			}

			// Count valid skins, which consist of a skin with a matching "_i" icon
			int nskins = 0;
			for (int k = 0; k < nimagefiles; k++)
				if (IsValidSkin(imagenames, nimagefiles, k))
					nskins++;

			if (nskins == 0)
				continue;

			// Add extra empty item at the end, because Menu_AddItem() counts number of spincontrol items by counting itemnames to '\0' item
			char **skinnames = malloc(sizeof(char *) * (nskins + 1));
			memset(skinnames, 0, sizeof(char *) * (nskins + 1));

			// Copy the valid skins
			int skinindex = 0;
			for (int k = 0; k < nimagefiles; k++)
			{
				if (IsValidSkin(imagenames, nimagefiles, k))
				{
					char *a2 = strrchr(imagenames[k], '/');
					char *b2 = strrchr(imagenames[k], '\\');
					char *c2 = max(a2, b2);

					Q_strncpyz(scratch, c2 + 1, sizeof(scratch));

					if (strrchr(scratch, '.'))
						*strrchr(scratch, '.') = 0;

					skinnames[skinindex] = strdup(scratch);
					skinindex++;
				}
			}

			// At this point we have a valid player model
			s_pmi[s_numplayermodels].nskins = nskins;
			s_pmi[s_numplayermodels].skindisplaynames = skinnames;

			// Make short name for the model
			a = strrchr(dirnames[i], '/');
			b = strrchr(dirnames[i], '\\');
			c = max(a, b);

			strncpy(s_pmi[s_numplayermodels].displayname, c + 1, MAX_DISPLAYNAME - 1);
			Q_strncpyz(s_pmi[s_numplayermodels].directory, c + 1, sizeof(s_pmi[s_numplayermodels].directory));

			FS_FreeFileList(imagenames, nimagefiles);

			s_numplayermodels++;
		}
		
		if (ndirs > 0)
			FS_FreeFileList(dirnames, ndirs);
	}
}

static int pmicmpfnc(const void *_a, const void *_b)
{
	const playermodelinfo_s *a = (const playermodelinfo_s *) _a;
	const playermodelinfo_s *b = (const playermodelinfo_s *) _b;

	// Sort by male, female, cyborg then alphabetical
	if (strcmp(a->directory, "male") == 0)
		return -1;
	if (strcmp(b->directory, "male") == 0)
		return 1;

	if (strcmp(a->directory, "female") == 0)
		return -1;
	if (strcmp(b->directory, "female") == 0)
		return 1;

	if (strcmp(a->directory, "cyborg") == 0) //mxd
		return -1;
	if (strcmp(b->directory, "cyborg") == 0)
		return 1;

	return strcmp(a->directory, b->directory);
}

qboolean PlayerConfig_MenuInit(void)
{
	static const char *handedness[] = { "Right", "Left", "Center", 0 };
	
	char currentdirectory[1024];
	char currentskin[1024];
	char scratch[MAX_QPATH];
	int y = 0;

	int currentdirectoryindex = 0;
	int currentskinindex = 0;

	cvar_t *hand = Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);

	PlayerConfig_ScanDirectories();

	if (s_numplayermodels == 0)
		return false;

	if (hand->value < 0 || hand->value > 2)
		Cvar_SetValue("hand", 0);

	Q_strncpyz(currentdirectory, info_skin->string, sizeof(currentdirectory));

	if (strchr(currentdirectory, '/'))
	{
		Q_strncpyz(currentskin, strchr(currentdirectory, '/' ) + 1, sizeof(currentskin));
		*strchr(currentdirectory, '/') = 0;
	}
	else if (strchr(currentdirectory, '\\'))
	{
		Q_strncpyz(currentskin, strchr(currentdirectory, '\\') + 1, sizeof(currentskin));
		*strchr(currentdirectory, '\\') = 0;
	}
	else
	{
		Q_strncpyz(currentdirectory, "male", sizeof(currentdirectory));
		Q_strncpyz(currentskin, "grunt", sizeof(currentskin));
	}

	qsort(s_pmi, s_numplayermodels, sizeof(s_pmi[0]), pmicmpfnc);

	memset(s_pmnames, 0, sizeof(s_pmnames));
	for (int i = 0; i < s_numplayermodels; i++)
	{
		s_pmnames[i] = s_pmi[i].displayname;
		if (Q_stricmp(s_pmi[i].directory, currentdirectory) == 0)
		{
			currentdirectoryindex = i;

			for (int j = 0; j < s_pmi[i].nskins; j++)
			{
				if (Q_stricmp(s_pmi[i].skindisplaynames[j], currentskin) == 0)
				{
					currentskinindex = j;
					break;
				}
			}
		}
	}
	
	s_player_config_menu.x = SCREEN_WIDTH * 0.5f - 210;
	s_player_config_menu.y = DEFAULT_MENU_Y; //mxd. Was SCREEN_HEIGHT * 0.5 - 70;
	s_player_config_menu.nitems = 0;

	s_player_name_field.generic.type = MTYPE_FIELD;
	s_player_name_field.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_name_field.generic.name = "Name";
	s_player_name_field.generic.x = -MENU_FONT_SIZE;
	s_player_name_field.generic.y = y;
	s_player_name_field.length = 20;
	s_player_name_field.visible_length = 21; //mxd. Add space to draw cursor
	Q_strncpyz(s_player_name_field.buffer, info_name->string, sizeof(s_player_name_field.buffer));
	s_player_name_field.cursor = strlen(info_name->string);

	s_player_model_title.generic.type = MTYPE_SEPARATOR;
	s_player_model_title.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_model_title.generic.name = "Model";
	s_player_model_title.generic.x = -7 * MENU_FONT_SIZE;
	s_player_model_title.generic.y = y += 3 * MENU_LINE_SIZE;

	s_player_model_box.generic.type = MTYPE_SPINCONTROL;
	s_player_model_box.generic.x = -7 * MENU_FONT_SIZE;
	s_player_model_box.generic.y = y += MENU_LINE_SIZE;
	s_player_model_box.generic.callback = ModelCallback;
	s_player_model_box.generic.cursor_offset = -6 * MENU_FONT_SIZE;
	s_player_model_box.curvalue = currentdirectoryindex;
	s_player_model_box.itemnames = s_pmnames;

	s_player_skin_title.generic.type = MTYPE_SEPARATOR;
	s_player_skin_title.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_skin_title.generic.name = "Skin";
	s_player_skin_title.generic.x = -7 * MENU_FONT_SIZE;
	s_player_skin_title.generic.y = y += 2 * MENU_LINE_SIZE;

	s_player_skin_box.generic.type = MTYPE_SPINCONTROL;
	s_player_skin_box.generic.x = -7 * MENU_FONT_SIZE;
	s_player_skin_box.generic.y = y += MENU_LINE_SIZE;
	s_player_skin_box.generic.name = 0;
	s_player_skin_box.generic.callback = SkinCallback; // Knightmare added, was 0
	s_player_skin_box.generic.cursor_offset = -6 * MENU_FONT_SIZE;
	s_player_skin_box.curvalue = currentskinindex;
	s_player_skin_box.itemnames = s_pmi[currentdirectoryindex].skindisplaynames;

	s_player_hand_title.generic.type = MTYPE_SEPARATOR;
	s_player_hand_title.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_hand_title.generic.name = "Handedness";
	s_player_hand_title.generic.x = -7 * MENU_FONT_SIZE;
	s_player_hand_title.generic.y = y += 2 * MENU_LINE_SIZE;

	s_player_handedness_box.generic.type = MTYPE_SPINCONTROL;
	s_player_handedness_box.generic.x = -7 * MENU_FONT_SIZE;
	s_player_handedness_box.generic.y = y += MENU_LINE_SIZE;
	s_player_handedness_box.generic.name = 0;
	s_player_handedness_box.generic.cursor_offset = -6 * MENU_FONT_SIZE;
	s_player_handedness_box.generic.callback = HandednessCallback;
	s_player_handedness_box.curvalue = Cvar_VariableValue("hand");
	s_player_handedness_box.itemnames = handedness;
	
	int currate;
	for (currate = 0; currate < sizeof(rate_tbl) / sizeof(*rate_tbl) - 1; currate++)
		if (Cvar_VariableValue("rate") == rate_tbl[currate])
			break;
		
	s_player_rate_title.generic.type = MTYPE_SEPARATOR;
	s_player_rate_title.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_rate_title.generic.name = "Connection speed";
	s_player_rate_title.generic.x = -7 * MENU_FONT_SIZE;
	s_player_rate_title.generic.y = y += 2 * MENU_LINE_SIZE;

	s_player_rate_box.generic.type = MTYPE_SPINCONTROL;
	s_player_rate_box.generic.x = -7 * MENU_FONT_SIZE;
	s_player_rate_box.generic.y = y += MENU_LINE_SIZE;
	s_player_rate_box.generic.name = 0;
	s_player_rate_box.generic.cursor_offset = -6 * MENU_FONT_SIZE;
	s_player_rate_box.generic.callback = RateCallback;
	s_player_rate_box.curvalue = currate;
	s_player_rate_box.itemnames = rate_names;

	s_player_back_action.generic.type = MTYPE_ACTION;
	s_player_back_action.generic.name = (UI_MenuDepth() == 0 ? MENU_BACK_CLOSE : MENU_BACK_TO_MULTIPLAYER); //mxd
	s_player_back_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_back_action.generic.x = -5 * MENU_FONT_SIZE;
	s_player_back_action.generic.y = y += 2 * MENU_LINE_SIZE;
	s_player_back_action.generic.statusbar = NULL;
	s_player_back_action.generic.callback = PlayerConfigBackFunc; //mxd. Let's accept changes. Was UI_BackMenu;

	// Only register model and skin on starup or when changed
	Com_sprintf(scratch, sizeof(scratch), "players/%s/tris.md2", s_pmi[s_player_model_box.curvalue].directory);
	playermodel = R_RegisterModel(scratch);

	Com_sprintf(scratch, sizeof(scratch), "players/%s/%s.pcx", 
		s_pmi[s_player_model_box.curvalue].directory, 
		s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);
	playerskin = R_RegisterSkin(scratch);

	// Show current weapon model (if any)
	if (currentweaponmodel && strlen(currentweaponmodel))
	{
		Com_sprintf(scratch, sizeof(scratch), "players/%s/%s", s_pmi[s_player_model_box.curvalue].directory, currentweaponmodel);
		weaponmodel = R_RegisterModel(scratch);
		if (!weaponmodel)
		{
			Com_sprintf(scratch, sizeof(scratch), "players/%s/weapon.md2", s_pmi[s_player_model_box.curvalue].directory);
			weaponmodel = R_RegisterModel(scratch);
		}
	}
	else
	{
		Com_sprintf(scratch, sizeof(scratch), "players/%s/weapon.md2", s_pmi[s_player_model_box.curvalue].directory);
		weaponmodel = R_RegisterModel(scratch);
	}

	Menu_AddItem(&s_player_config_menu, &s_player_name_field);
	Menu_AddItem(&s_player_config_menu, &s_player_model_title);
	Menu_AddItem(&s_player_config_menu, &s_player_model_box);

	if (s_player_skin_box.itemnames)
	{
		Menu_AddItem(&s_player_config_menu, &s_player_skin_title);
		Menu_AddItem(&s_player_config_menu, &s_player_skin_box);
	}

	Menu_AddItem(&s_player_config_menu, &s_player_hand_title);
	Menu_AddItem(&s_player_config_menu, &s_player_handedness_box);
	Menu_AddItem(&s_player_config_menu, &s_player_rate_title);
	Menu_AddItem(&s_player_config_menu, &s_player_rate_box);
	Menu_AddItem(&s_player_config_menu, &s_player_back_action);

	return true;
}

qboolean PlayerConfig_CheckIncerement(int dir, float x, float y, float w, float h)
{
	float min[2], max[2];

	float x1 = x;
	float y1 = y;
	float w1 = w;
	float h1 = h;

	SCR_AdjustFrom640(&x1, &y1, &w1, &h1, ALIGN_CENTER);

	min[0] = x1;
	max[0] = x1 + w1;
	min[1] = y1;
	max[1] = y1 + h1;

	if (cursor.x >= min[0] && cursor.x <= max[0] &&
		cursor.y >= min[1] && cursor.y <= max[1] &&
		!cursor.buttonused[MOUSEBUTTON1] &&
		cursor.buttonclicks[MOUSEBUTTON1] == 1)
	{
		if (dir) // dir==1 is left
		{
			if (--s_player_skin_box.curvalue < 0)
				s_player_skin_box.curvalue = s_pmi[s_player_model_box.curvalue].nskins - 1; //mxd. Wrap around
		}
		else
		{
			if (++s_player_skin_box.curvalue > s_pmi[s_player_model_box.curvalue].nskins - 1) //mxd. Wrap around
				s_player_skin_box.curvalue = 0;
		}

		char *sound = menu_move_sound;
		cursor.buttonused[MOUSEBUTTON1] = true;
		cursor.buttonclicks[MOUSEBUTTON1] = 0;

		if (sound)
			S_StartLocalSound(sound);

		SkinCallback(NULL);

		return true;
	}

	return false;
}

void PlayerConfig_MouseClick(void)
{
	const float icon_x = SCREEN_WIDTH * 0.5f - 5;
	const float icon_y = SCREEN_HEIGHT - 108;
	float icon_offset = 0;

	buttonmenuobject_t buttons[NUM_SKINBOX_ITEMS];
	for (int i = 0; i < NUM_SKINBOX_ITEMS; i++)
		buttons[i].index = -1;

	int skinindex;
	if (s_pmi[s_player_model_box.curvalue].nskins < NUM_SKINBOX_ITEMS || s_player_skin_box.curvalue < 4)
		skinindex = 0;
	else if (s_player_skin_box.curvalue > s_pmi[s_player_model_box.curvalue].nskins - 4)
		skinindex = s_pmi[s_player_model_box.curvalue].nskins - NUM_SKINBOX_ITEMS;
	else
		skinindex = s_player_skin_box.curvalue - 3;

	// Skins list left arrow click
	if (PlayerConfig_CheckIncerement(1, icon_x - 39, icon_y, 32, 32))
		return;

	for (int count = 0; count < NUM_SKINBOX_ITEMS; skinindex++, count++)
	{
		if (skinindex < 0 || skinindex >= s_pmi[s_player_model_box.curvalue].nskins)
			continue;

		UI_AddButton(&buttons[count], skinindex, icon_x + icon_offset, icon_y, 32, 32);
		icon_offset += 34;
	}

	icon_offset = NUM_SKINBOX_ITEMS * 34;

	// Skins list right arrow click
	if (PlayerConfig_CheckIncerement(0, icon_x + icon_offset + 5, icon_y, 32, 32))
		return;

	for (int i = 0; i < NUM_SKINBOX_ITEMS; i++)
	{
		if (buttons[i].index == -1)
			continue;

		if (cursor.x >= buttons[i].min[0] && cursor.x <= buttons[i].max[0] &&
			cursor.y >= buttons[i].min[1] && cursor.y <= buttons[i].max[1])
		{
			if (!cursor.buttonused[MOUSEBUTTON1] && cursor.buttonclicks[MOUSEBUTTON1] == 1)
			{
				s_player_skin_box.curvalue = buttons[i].index;

				char *sound = menu_move_sound;
				cursor.buttonused[MOUSEBUTTON1] = true;
				cursor.buttonclicks[MOUSEBUTTON1] = 0;

				if (sound)
					S_StartLocalSound(sound);

				SkinCallback(NULL);
			}

			break;
		}
	}
}

#define SKIN_SELECTOR_ARROW_L "/gfx/ui/arrows/arrow_left.pcx" //mxd
#define SKIN_SELECTOR_ARROW_R "/gfx/ui/arrows/arrow_right.pcx"
#define SKIN_SELECTOR_LIST_BG "/gfx/ui/listbox_background.pcx"

void PlayerConfig_DrawSkinSelection(void)
{
	char scratch[MAX_QPATH];
	const float	icon_x = SCREEN_WIDTH * 0.5f - 5;
	const float	icon_y = SCREEN_HEIGHT - 108;
	float icon_offset = 0;
	int color[3];

	TextColor((int)Cvar_VariableValue("alt_text_color"), &color[0], &color[1], &color[2]);

	int skinindex;
	if (s_pmi[s_player_model_box.curvalue].nskins < NUM_SKINBOX_ITEMS || s_player_skin_box.curvalue < 4)
		skinindex = 0;
	else if (s_player_skin_box.curvalue > s_pmi[s_player_model_box.curvalue].nskins - 4)
		skinindex = s_pmi[s_player_model_box.curvalue].nskins - NUM_SKINBOX_ITEMS;
	else
		skinindex = s_player_skin_box.curvalue - 3;

	// Left arrow
	Com_sprintf(scratch, sizeof(scratch), SKIN_SELECTOR_ARROW_L);
	SCR_DrawPic(icon_x - 39, icon_y + 2, 32, 32, ALIGN_CENTER, scratch, 1.0f);

	// Background
	SCR_DrawFill(icon_x - 3, icon_y - 3, NUM_SKINBOX_ITEMS * 34 + 4, 38, ALIGN_CENTER, 0, 0, 0, 255);
	if (R_DrawFindPic(SKIN_SELECTOR_LIST_BG))
	{
		float x = icon_x - 2;
		float y = icon_y - 2;
		float w = NUM_SKINBOX_ITEMS * 34 + 2;
		float h = 36;
		SCR_AdjustFrom640(&x, &y, &w, &h, ALIGN_CENTER);
		R_DrawStretchPic((int)x, (int)y, (int)w, (int)h, SKIN_SELECTOR_LIST_BG, 1.0f); //mxd. R_DrawTileClear -> R_DrawStretchPic
	}
	else
	{
		SCR_DrawFill(icon_x - 2, icon_y - 2, NUM_SKINBOX_ITEMS * 34 + 2, 36, ALIGN_CENTER, 60, 60, 60, 255);
	}
		
	for (int count = 0; count < NUM_SKINBOX_ITEMS; skinindex++, count++)
	{
		if (skinindex < 0 || skinindex >= s_pmi[s_player_model_box.curvalue].nskins)
			continue;

		Com_sprintf(scratch, sizeof(scratch), "/players/%s/%s_i.pcx", 
			s_pmi[s_player_model_box.curvalue].directory,
			s_pmi[s_player_model_box.curvalue].skindisplaynames[skinindex]);

		if (skinindex == s_player_skin_box.curvalue)
			SCR_DrawFill(icon_x + icon_offset - 1, icon_y - 1, 34, 34, ALIGN_CENTER, color[0], color[1], color[2], 255);

		SCR_DrawPic(icon_x + icon_offset, icon_y, 32, 32,  ALIGN_CENTER, scratch, 1.0f);
		icon_offset += 34;
	}

	// Right arrow
	icon_offset = NUM_SKINBOX_ITEMS * 34;
	Com_sprintf(scratch, sizeof(scratch), SKIN_SELECTOR_ARROW_R);
	SCR_DrawPic(icon_x + icon_offset + 5, icon_y + 2, 32, 32, ALIGN_CENTER, scratch, 1.0f);
}

void PlayerConfig_MenuDraw(void)
{
	Menu_DrawBanner("m_banner_plauer_setup"); // Typo for image name is id's fault

	refdef_t refdef;
	memset(&refdef, 0, sizeof(refdef));

	//mxd. Use full screen size, but with 4:3 fov.
	refdef.width = viddef.width;
	refdef.height = viddef.height;
	refdef.fov_x = 50;
	refdef.fov_y = CalcFov(refdef.fov_x, SCREEN_WIDTH * screenScale.avg, viddef.height);
	refdef.time = cls.realtime * 0.001f;
 
	if (s_pmi[s_player_model_box.curvalue].skindisplaynames)
	{
		vec3_t modelOrg;
		entity_t entity[2]; // Psychopspaz's support for showing weapon model

		refdef.num_entities = 0;
		refdef.entities = entity;

		const float frametime = cl.time * 0.01f; //mxd

		float backlerp; //mxd
		if (frametime < (int)frametime)
			backlerp = 1.0f - ((int)frametime - frametime);
		else
			backlerp = 1.0f - (frametime - (int)frametime);

		//mxd. Oscillate side to side
		float yaw = 190 + 45 * sinf(cl.time * 0.001f);
		if (info_hand->value == 1)
			yaw -= 90; //mxd. Fixes model facing when switching handedness

		VectorSet(modelOrg, 150, -25, 0);

		// Setup player model
		entity_t *ent = &entity[0];
		memset(&entity[0], 0, sizeof(entity[0]));

		// Moved registration code to init and change only
		ent->model = playermodel;
		ent->skin = playerskin;

		ent->flags = RF_FULLBRIGHT | RF_NOSHADOW | RF_DEPTHHACK;
		if (info_hand->value == 1)
			ent->flags |= RF_MIRRORMODEL;

		VectorCopy(modelOrg, ent->origin); //mxd
		VectorCopy(ent->origin, ent->oldorigin);

		ent->frame = (int)frametime % 40; // Idle animation is first and is 40 frames long
		ent->oldframe = (int)(frametime - 1) % 40;
		ent->backlerp = backlerp;
		ent->angles[1] = yaw;

		refdef.num_entities++;

		// Setup weapon model
		ent = &entity[1];
		memset(&entity[1], 0, sizeof(entity[1]));

		// Moved registration code to init and change only
		ent->model = weaponmodel;

		if (ent->model)
		{
			ent->skinnum = 0;

			ent->flags = RF_FULLBRIGHT | RF_NOSHADOW | RF_DEPTHHACK;
			if (info_hand->value == 1)
				ent->flags |= RF_MIRRORMODEL;

			VectorCopy(modelOrg, ent->origin); //mxd
			VectorCopy(ent->origin, ent->oldorigin);

			ent->frame = (int)frametime % 40;
			ent->oldframe = (int)(frametime - 1) % 40;
			ent->backlerp = backlerp;
			ent->angles[1] = yaw;

			refdef.num_entities++;
		}

		refdef.areabits = 0;
		refdef.lightstyles = 0;
		refdef.rdflags = RDF_NOWORLDMODEL;

		R_RenderFrame(&refdef); //mxd. Draw below controls
		Menu_Draw(&s_player_config_menu);
		PlayerConfig_DrawSkinSelection();
	}
}

void PConfigAccept(void)
{
	char scratch[1024];

	char *name = (strlen(s_player_name_field.buffer) == 0 ? Cvar_DefaultString("name") : s_player_name_field.buffer); //mxd. Avoid empty player names
	Cvar_Set("name", name);

	Com_sprintf(scratch, sizeof(scratch), "%s/%s", 
		s_pmi[s_player_model_box.curvalue].directory, 
		s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);

	Cvar_Set("skin", scratch);

	for (int i = 0; i < s_numplayermodels; i++)
	{
		for (int j = 0; j < s_pmi[i].nskins; j++)
		{
			free(s_pmi[i].skindisplaynames[j]);
			s_pmi[i].skindisplaynames[j] = 0;
		}

		free(s_pmi[i].skindisplaynames);
		s_pmi[i].skindisplaynames = 0;
		s_pmi[i].nskins = 0;
	}
}

const char *PlayerConfig_MenuKey(int key)
{
	if (key == K_ESCAPE)
		PConfigAccept();

	return Default_MenuKey(&s_player_config_menu, key);
}

void M_Menu_PlayerConfig_f(void)
{
	if (!PlayerConfig_MenuInit())
	{
		s_multiplayer_menu.statusbar = "No valid player models found";
	}
	else
	{
		s_multiplayer_menu.statusbar = NULL;
		UI_PushMenu(PlayerConfig_MenuDraw, PlayerConfig_MenuKey);
	}
}