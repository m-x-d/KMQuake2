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

// ui_main.c -- the main menu & support functions
 
#include <ctype.h>
#include "../client/client.h"
#include "ui_local.h"

static int m_main_cursor;

//mxd. Entity used to draw quad cursor model
static entity_t quadent;

#define MAIN_ITEMS	5

static char *main_names[] =
{
	"m_main_game",
	"m_main_multiplayer",
	"m_main_options",
	"m_main_video",
	"m_main_quit",
	0
};

static void FindMenuCoords(int *xoffset, int *ystart, int *totalheight, int *widest)
{
	int w, h;

	*totalheight = 0;
	*widest = -1;

	for (int i = 0; main_names[i] != 0; i++)
	{
		R_DrawGetPicSize(&w, &h, main_names[i]);
		if (w > *widest)
			*widest = w;

		*totalheight += (h + 12);
	}

	*xoffset = (SCREEN_WIDTH - *widest + 70) * 0.5f;
	*ystart = SCREEN_HEIGHT * 0.5f - 100;
}

// Draws an animating cursor with the point at x, y.
// The pic will extend to the left of x, and both above and below y.
// Used only if quad damage model isn't loaded.
static void UI_DrawMainCursor(const int x, const int y, const int f)
{
	static qboolean cached = false;
	char cursorname[80];
	int w, h;

	if (!cached)
	{
		for (int i = 0; i < NUM_MAINMENU_CURSOR_FRAMES; i++)
		{
			Com_sprintf(cursorname, sizeof(cursorname), "m_cursor%d", i);
			R_DrawFindPic(cursorname);
		}

		cached = true;
	}

	Com_sprintf(cursorname, sizeof(cursorname), "m_cursor%d", f);
	R_DrawGetPicSize(&w, &h, cursorname);
	SCR_DrawPic(x, y, w, h, ALIGN_CENTER, cursorname, 1.0f);
}

// Draws a rotating quad damage model. Expects quadent to be initialized.
static void UI_DrawMainCursor3D(const int x, const int y)
{
	// Set model yaw
	quadent.angles[1] = anglemod(cl.time / 10.0f);
	
	// Initialize a custom refdef
	refdef_t refdef;
	memset(&refdef, 0, sizeof(refdef));

	// Size 24x34
	float rx = x;
	float ry = y;
	float rw = 24;
	float rh = 34;
	SCR_AdjustFrom640(&rx, &ry, &rw, &rh, ALIGN_CENTER);

	refdef.x = rx;
	refdef.y = ry;
	refdef.width = rw;
	refdef.height = rh;
	refdef.fov_x = 40;
	refdef.fov_y = CalcFov(refdef.fov_x, refdef.width, refdef.height);
	refdef.time = cls.realtime * 0.001f;
	refdef.rdflags = RDF_NOWORLDMODEL;
	refdef.num_entities = 1;
	refdef.entities = &quadent;

	R_RenderFrame(&refdef);
}

//mxd. Initializes quadent and loads the quad damage model.
static qboolean UI_CheckQuadModel(void)
{
	static qboolean loadfailed;
	static int registration_sequence = -1;

	if (loadfailed)
		return false;
	
	if (quadent.model != NULL && R_GetRegistartionSequence(quadent.model) == registration_sequence)
		return true;
	
	quadent.model = R_RegisterModel("models/ui/quad_cursor.md2");
	quadent.flags = RF_FULLBRIGHT | RF_NOSHADOW | RF_DEPTHHACK;
	VectorSet(quadent.origin, 40, 0, -18);
	VectorCopy(quadent.origin, quadent.oldorigin);

	if (quadent.model != NULL)
		registration_sequence = R_GetRegistartionSequence(quadent.model);

	loadfailed = (quadent.model == NULL);

	return loadfailed;
}

void M_Main_Draw(void)
{
	int w, h;
	int ystart;
	int	xoffset;
	int widest = -1;
	int totalheight = 0;
	char litname[80];

	FindMenuCoords(&xoffset, &ystart, &totalheight, &widest);

	for (int i = 0; main_names[i] != 0; i++)
	{
		if (i != m_main_cursor)
		{
			R_DrawGetPicSize(&w, &h, main_names[i]);
			SCR_DrawPic(xoffset, (ystart + i * 40 + 3), w, h, ALIGN_CENTER, main_names[i], 1.0f);
		}
	}

	Q_strncpyz(litname, main_names[m_main_cursor], sizeof(litname));
	Q_strncatz(litname, "_sel", sizeof(litname));
	R_DrawGetPicSize(&w, &h, litname);
	SCR_DrawPic(xoffset - 1, (ystart + m_main_cursor * 40 + 2), w + 2, h + 2, ALIGN_CENTER, litname, 1.0f);

	// Draw our nifty quad damage model as a cursor if it's loaded.
	if (UI_CheckQuadModel())
		UI_DrawMainCursor3D(xoffset - 27, ystart + (m_main_cursor * 40 + 1));
	else
		UI_DrawMainCursor(xoffset - 25, ystart + (m_main_cursor * 40 + 1), (int)(cls.realtime / 100) % NUM_MAINMENU_CURSOR_FRAMES);

	R_DrawGetPicSize(&w, &h, "m_main_plaque");
	SCR_DrawPic(xoffset - (w / 2 + 50), ystart, w, h, ALIGN_CENTER, "m_main_plaque", 1.0f);
	const int last_h = h;

	R_DrawGetPicSize(&w, &h, "m_main_logo");
	SCR_DrawPic(xoffset - (w / 2 + 50), ystart + last_h + 20, w, h, ALIGN_CENTER, "m_main_logo", 1.0f);
}

static void OpenMenuFromMain(void)
{
	switch (m_main_cursor)
	{
		case 0: M_Menu_Game_f(); break;
		case 1: M_Menu_Multiplayer_f(); break;
		case 2: M_Menu_Options_f(); break;
		case 3: M_Menu_Video_f(); break;
		case 4: M_Menu_Quit_f(); break;
	}
}

void UI_CheckMainMenuMouse(void)
{
	static int hover = 0; //mxd
	
	int ystart;
	int	xoffset;
	int widest;
	int totalheight;
	char *sound = NULL;
	mainmenuobject_t buttons[MAIN_ITEMS];

	const int oldhover = hover;
	hover = 0;

	FindMenuCoords(&xoffset, &ystart, &totalheight, &widest);
	for (int i = 0; main_names[i] != 0; i++)
		UI_AddMainButton(&buttons[i], i, xoffset, ystart + (i * 40 + 3), main_names[i]);

	// Exit with double click 2nd mouse button
	if (!cursor.buttonused[MOUSEBUTTON2] && cursor.buttonclicks[MOUSEBUTTON2] == 2)
	{
		UI_PopMenu();
		sound = menu_out_sound;
		cursor.buttonused[MOUSEBUTTON2] = true;
		cursor.buttonclicks[MOUSEBUTTON2] = 0;
	}

	for (int i = MAIN_ITEMS - 1; i >= 0; i--)
	{
		if (   cursor.x >= buttons[i].min[0] && cursor.x <= buttons[i].max[0]
			&& cursor.y >= buttons[i].min[1] && cursor.y <= buttons[i].max[1])
		{
			if (cursor.mouseaction)
				m_main_cursor = i;

			hover = i + 1;

			if (oldhover == hover && hover - 1 == m_main_cursor
				&& !cursor.buttonused[MOUSEBUTTON1] && cursor.buttonclicks[MOUSEBUTTON1] == 1)
			{
				OpenMenuFromMain();
				sound = menu_move_sound;
				cursor.buttonused[MOUSEBUTTON1] = true;
				cursor.buttonclicks[MOUSEBUTTON1] = 0;
			}

			break;
		}
	}

	if (hover == 0)
	{
		cursor.buttonused[MOUSEBUTTON1] = false;
		cursor.buttonclicks[MOUSEBUTTON1] = 0;
		cursor.buttontime[MOUSEBUTTON1] = 0;
	}

	cursor.mouseaction = false;

	if (sound)
		S_StartLocalSound(sound);
}

const char *M_Main_Key(int key)
{
	const char *sound = menu_move_sound;

	switch (key)
	{
	case K_ESCAPE:
#ifdef ERASER_COMPAT_BUILD // special hack for Eraser build
		if (cls.state == ca_disconnected)
			M_Menu_Quit_f();
		else
			UI_PopMenu();
#else	// Can't exit menu if disconnected, so restart demo loop
		if (cls.state == ca_disconnected)
			Cbuf_AddText("d1\n");
		UI_PopMenu();
#endif
		break;

	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		return sound;

	case K_KP_UPARROW:
	case K_UPARROW:
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		return sound;

	case K_KP_ENTER:
	case K_ENTER:
		m_entersound = true;
		OpenMenuFromMain();
	}

	return NULL;
}

void M_Menu_Main_f(void)
{
	UI_PushMenu(M_Main_Draw, M_Main_Key);
}