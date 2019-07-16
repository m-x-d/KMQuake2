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
// cl_inv.c -- client inventory screen

#include "client.h"

void CL_ParseInventory()
{
	for (int i = 0; i < MAX_ITEMS; i++)
		cl.inventory[i] = MSG_ReadShort(&net_message);
}

#define DISPLAY_ITEMS	17

void CL_DrawInventory()
{
	int index[MAX_ITEMS];
	char string[1024];
	char binding[1024];

	const int selected = cl.frame.playerstate.stats[STAT_SELECTED_ITEM];

	int num = 0;
	int selected_num = 0;

	for (int i = 0; i < MAX_ITEMS; i++)
	{
		if (i == selected)
			selected_num = num;

		if (cl.inventory[i])
		{
			index[num] = i;
			num++;
		}
	}

	// Determine scroll point
	int top = selected_num - DISPLAY_ITEMS / 2;
	if (num - top < DISPLAY_ITEMS)
		top = num - DISPLAY_ITEMS;
	top = max(top, 0);

	int x = SCREEN_WIDTH / 2 - 128;
	int y = SCREEN_HEIGHT / 2 - 116;

	SCR_DrawPic(x, y, 256, 192, ALIGN_CENTER, "inventory", hud_alpha->value);
	x += 24;
	y += 20;

	SCR_DrawString(x, y, ALIGN_CENTER, S_COLOR_BOLD"hotkey ### item", 255);
	y += 8;

	SCR_DrawString(x, y, ALIGN_CENTER, S_COLOR_BOLD"------ --- ----", 255);
	x += 16;
	y += 8;

	for (int i = top; i < num && i < top + DISPLAY_ITEMS; i++)
	{
		const int item = index[i];

		// Search for a binding
		const int cs_items = (LegacyProtocol() ? OLD_CS_ITEMS : CS_ITEMS); // Knightmare- BIG UGLY HACK for connected to server using old protocol. Changed config strings require different parsing
		Com_sprintf(binding, sizeof(binding), "use %s", cl.configstrings[cs_items + item]);

		char *bind = "";
		for (int j = 0; j < K_LAST; j++)
		{
			if (keybindings[j] && !Q_stricmp(keybindings[j], binding))
			{
				bind = Key_KeynumToString(j);
				break;
			}
		}

		char *text;
		if (item == selected) // Draw a blinky cursor by the selected item
			text = S_COLOR_BOLD">"S_COLOR_ITALIC"%3s %3i %7s";
		else 
			text = " "S_COLOR_BOLD S_COLOR_ALT"%3s %3i %7s";

		Com_sprintf(string, sizeof(string), text, bind, cl.inventory[item], cl.configstrings[cs_items + item]);
		SCR_DrawString(x, y, ALIGN_CENTER, string, 255);

		y += 8;
	}
}