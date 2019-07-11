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

// menu_backend.c -- supporting code for menu widgets


#include <string.h>
#include <ctype.h>

#include "../client/client.h"
#include "ui_local.h"

#define VID_WIDTH viddef.width
#define VID_HEIGHT viddef.height

// Psychospaz's menu mouse support
static int MouseOverAlpha(menucommon_s *m)
{
	if (cursor.menuitem == m)
	{
		const int alpha = 125 + 25 * cosf(anglemod(cl.time * 0.005f));
		return clamp(alpha, 0, 255);
	}

	return 255;
}

static void Action_DoEnter(menuaction_s *a)
{
	if (a->generic.callback)
		a->generic.callback(a);
}

static void Action_Draw(menuaction_s *a)
{
	const int alpha = MouseOverAlpha(&a->generic);

	if (a->generic.flags & QMF_LEFT_JUSTIFY)
	{
		if (a->generic.flags & QMF_GRAYED)
		{
			Menu_DrawStringDark(a->generic.x + a->generic.parent->x + LCOLUMN_OFFSET,
								a->generic.y + a->generic.parent->y, a->generic.name, alpha);
		}
		else
		{
			Menu_DrawString(a->generic.x + a->generic.parent->x + LCOLUMN_OFFSET,
							a->generic.y + a->generic.parent->y, a->generic.name, alpha);
		}
	}
	else
	{
		if (a->generic.flags & QMF_GRAYED)
		{
			Menu_DrawStringR2LDark(a->generic.x + a->generic.parent->x + LCOLUMN_OFFSET,
								   a->generic.y + a->generic.parent->y, a->generic.name, alpha);
		}
		else
		{
			Menu_DrawStringR2L(a->generic.x + a->generic.parent->x + LCOLUMN_OFFSET,
							   a->generic.y + a->generic.parent->y, a->generic.name, alpha);
		}
	}

	if (a->generic.ownerdraw)
		a->generic.ownerdraw(a);
}

static qboolean Field_DoEnter(menufield_s *f)
{
	if (f->generic.callback)
	{
		f->generic.callback(f);
		return true;
	}

	return false;
}

static void Field_Draw(menufield_s *f)
{
	const int alpha = MouseOverAlpha(&f->generic);
	char tempbuffer[128] = "";

	if (f->generic.name)
	{
		Menu_DrawStringR2LDark(f->generic.x + f->generic.parent->x + LCOLUMN_OFFSET,
							   f->generic.y + f->generic.parent->y, f->generic.name, 255);
	}

	const int xtra = CL_StringLengthExtra(f->buffer);
	if (xtra > 0)
	{
		strncpy(tempbuffer, f->buffer + f->visible_offset, f->visible_length);
		const int offset = strlen(tempbuffer) - xtra;

		if (offset > f->visible_length)
		{
			f->visible_offset = offset - f->visible_length;
			strncpy(tempbuffer, f->buffer + f->visible_offset - xtra, f->visible_length + xtra);
		}
	}
	else
	{
		strncpy(tempbuffer, f->buffer + f->visible_offset, f->visible_length);
	}

	// Draw top-left border char
	SCR_DrawChar(f->generic.x + f->generic.parent->x + MENU_FONT_SIZE * 2,
				 f->generic.y + f->generic.parent->y - 4, ALIGN_CENTER, 18, 255, 255, 255, 255, false, false); 

	// Draw bottom-left border char
	SCR_DrawChar(f->generic.x + f->generic.parent->x + MENU_FONT_SIZE * 2,
				 f->generic.y + f->generic.parent->y + 4, ALIGN_CENTER, 24, 255, 255, 255, 255, false, false); 

	// Draw top-right border char
	SCR_DrawChar(f->generic.x + f->generic.parent->x + (3 + f->visible_length) * MENU_FONT_SIZE,
				 f->generic.y + f->generic.parent->y - 4, ALIGN_CENTER, 20, 255, 255, 255, 255, false, false); 

	// Draw bottom-right border char
	SCR_DrawChar(f->generic.x + f->generic.parent->x + (3 + f->visible_length) * MENU_FONT_SIZE,
				 f->generic.y + f->generic.parent->y + 4, ALIGN_CENTER, 26, 255, 255, 255, 255, false, false);

	for (int i = 0; i < f->visible_length; i++)
	{
		// Draw top border char
		SCR_DrawChar(f->generic.x + f->generic.parent->x + (3 + i) * MENU_FONT_SIZE,
					 f->generic.y + f->generic.parent->y - 4, ALIGN_CENTER, 19, 255, 255, 255, 255, false, false);

		// Draw bottom border char
		SCR_DrawChar(f->generic.x + f->generic.parent->x + (3 + i) * MENU_FONT_SIZE,
					 f->generic.y + f->generic.parent->y + 4, ALIGN_CENTER, 25, 255, 255, 255, 255, false, (i == (f->visible_length - 1)));
	}

	// Add cursor thingie
	if (Menu_ItemAtCursor(f->generic.parent) == f && ((int)(Sys_Milliseconds() / 250)) & 1)
		tempbuffer[f->cursor] = 11; //mxd. Actually place it where it should be

	// Draw text
	Menu_DrawString(f->generic.x + f->generic.parent->x + MENU_FONT_SIZE * 3,
					f->generic.y + f->generic.parent->y, tempbuffer, alpha);

	//mxd. Ownerdraw support for all item types
	if (f->generic.ownerdraw)
		f->generic.ownerdraw(f);
}

extern int keydown[];

qboolean Field_Key(menufield_s *f, int key)
{
	// Support pasting from the clipboard
	if ((toupper(key) == 'V' && keydown[K_CTRL]) || ((key == K_INS || key == K_KP_INS) && keydown[K_SHIFT]))
	{
		//TODO: (mxd) paste at cursor position, update text instead of replacing it?
		char *cbd = IN_GetClipboardData();
		if (cbd)
		{
			strtok(cbd, "\n\r\b");

			strncpy(f->buffer, cbd, f->length - 1);
			f->cursor = strlen(f->buffer);
			f->visible_offset = f->cursor - f->visible_length;
			f->visible_offset = max(f->visible_offset, 0);

			free(cbd);
		}

		return true;
	}

	// Process control keys
	switch (key)
	{
		case K_LEFTARROW: //mxd
			if(f->cursor > 0)
				f->cursor--;
			return true;

		case K_RIGHTARROW: //mxd
			if (f->cursor < (int)strlen(&f->buffer[0]))
				f->cursor++;
			return true;

		case K_BACKSPACE:
			if (f->cursor > 0)
			{
				memmove(&f->buffer[f->cursor - 1], &f->buffer[f->cursor], strlen(&f->buffer[f->cursor]) + 1);
				f->cursor--;

				if (f->visible_offset)
					f->visible_offset--;
			}
			return true; //mxd. Was break

		case K_DEL:
			memmove(&f->buffer[f->cursor], &f->buffer[f->cursor + 1], strlen(&f->buffer[f->cursor + 1]) + 1);
			return true; //mxd. Was break

		case K_KP_ENTER:
		case K_ENTER:
		case K_ESCAPE:
		case K_TAB:
			return false;
	}

	// Remap keypad keys to printable chars regardless of NumLock state
	key = Key_ConvertNumPadKey(key);

	//mxd. Only printable chars from this point
	if (key < 32 || key > 126)
		return false;

	// Process input key
	if (!isdigit(key) && (f->generic.flags & QMF_NUMBERSONLY))
		return false;

	if (f->cursor < f->length)
	{
		//mxd. Move text after cursor?
		if(f->buffer[f->cursor + 1])
			memmove(&f->buffer[f->cursor + 1], &f->buffer[f->cursor], min(f->length - f->cursor - 1, (int)strlen(&f->buffer[f->cursor]) + 1));
		else
			f->buffer[f->cursor + 1] = 0;
		
		f->buffer[f->cursor++] = key;

		if (f->cursor > f->visible_length)
			f->visible_offset++;
	}

	return true;
}

/*static void MenuList_DoEnter(menulist_s *l)
{
	const int start = l->generic.y / 10 + 1;
	l->curvalue = l->generic.parent->cursor - start;

	if (l->generic.callback)
		l->generic.callback(l);
}*/

static void MenuList_Draw(menulist_s *l)
{
	int y = 0;
	const int alpha = MouseOverAlpha(&l->generic);

	Menu_DrawStringR2LDark(l->generic.x + l->generic.parent->x - 2 * MENU_FONT_SIZE,
						   l->generic.y + l->generic.parent->y, l->generic.name, alpha);

	const char** n = l->itemnames;

	SCR_DrawFill(l->generic.parent->x + l->generic.x - 112, l->generic.parent->y + l->generic.y + (l->curvalue + 1) * MENU_LINE_SIZE,
				128, MENU_LINE_SIZE, ALIGN_CENTER, color8red(16), color8green(16), color8blue(16), 255);

	while (*n)
	{
		Menu_DrawStringR2LDark(l->generic.x + l->generic.parent->x + LCOLUMN_OFFSET,
							   l->generic.y + l->generic.parent->y + y + MENU_LINE_SIZE, *n, alpha);
		n++;
		y += MENU_LINE_SIZE;
	}

	//mxd. Ownerdraw support for all item types
	if (l->generic.ownerdraw)
		l->generic.ownerdraw(l);
}

static void Separator_Draw(menuseparator_s *s)
{
	if (!s->generic.name)
		return;

	const int alpha = MouseOverAlpha(&s->generic);

	if (s->generic.flags & QMF_LEFT_JUSTIFY) //mxd. Added QMF_LEFT_JUSTIFY support
	{
		Menu_DrawStringDark(s->generic.x + s->generic.parent->x,
							s->generic.y + s->generic.parent->y, s->generic.name, alpha);
	}
	else
	{
		Menu_DrawStringR2LDark(s->generic.x + s->generic.parent->x,
							   s->generic.y + s->generic.parent->y, s->generic.name, alpha);
	}

	//mxd. Ownerdraw support for all item types
	if (s->generic.ownerdraw)
		s->generic.ownerdraw(s);
}

static void Slider_DoSlide(menuslider_s *s, int dir)
{
	s->curvalue += dir;
	s->curvalue = clamp(s->curvalue, s->minvalue, s->maxvalue);

	if (s->generic.callback)
		s->generic.callback(s);
}

//mxd. Returns true if slider value was changed
static qboolean Slider_MouseClick(void *item)
{
	menuslider_s *s = (menuslider_s*)item;

	// Technically "slider start" and "slider end" chars are outside of valid slider range,
	// but allowing to click on them to set min / max value seems logical from usability standpoint.

	// Get start and end coordinates of the clickable area, taking "slider start" and "slider end" chars into account.
	float sliderstart = s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET;
	float sliderend = s->generic.x + s->generic.parent->x + (SLIDER_RANGE + 1) * MENU_FONT_SIZE + RCOLUMN_OFFSET;
	SCR_AdjustFrom640(&sliderstart, NULL, NULL, NULL, ALIGN_CENTER);
	SCR_AdjustFrom640(&sliderend, NULL, NULL, NULL, ALIGN_CENTER);

	if (cursor.x < sliderstart || cursor.x > sliderend)
		return false;

	// Calculate slider position using actual slider start
	const float fontsize = SCR_ScaledVideo(MENU_FONT_SIZE);
	const int pos = cursor.x - (sliderstart + fontsize); // Valid slider range starts at 0.5 * fontsize from sliderstart, using thumb center adds another 0.5 * fontsize  
	const float fvalue = pos / ((SLIDER_RANGE - 1) * fontsize);
	const int ivalue = (int)roundf(s->minvalue + fvalue * (s->maxvalue - s->minvalue));

	const float prevvalue = s->curvalue;
	s->curvalue = clamp(ivalue, s->minvalue, s->maxvalue);

	if (prevvalue == s->curvalue)
		return false;

	if (s->generic.callback)
		s->generic.callback(s);

	return true;
}

#define SLIDER_RANGE 10

static void Slider_Draw(menuslider_s *s)
{
	const int alpha = MouseOverAlpha(&s->generic);

	// Draw title
	Menu_DrawStringR2LDark(s->generic.x + s->generic.parent->x + LCOLUMN_OFFSET,
						   s->generic.y + s->generic.parent->y, s->generic.name, alpha);
	
	// Draw slider bg middle char
	for (int i = 0; i < SLIDER_RANGE; i++)
	{
		//mxd: offset 0.5 chars from the start char, not 1, like in KMQ2, so the thumb looks more natural at start/end positions
		SCR_DrawChar(s->generic.x + s->generic.parent->x + i * MENU_FONT_SIZE + RCOLUMN_OFFSET + MENU_FONT_SIZE * 0.5f,
					 s->generic.y + s->generic.parent->y, ALIGN_CENTER, 129, 255, 255, 255, 255, false, false);
	}

	// Draw slider bg start char
	SCR_DrawChar(s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET,
		s->generic.y + s->generic.parent->y, ALIGN_CENTER, 128, 255, 255, 255, 255, false, false);

	// Draw slider bg end char
	SCR_DrawChar(s->generic.x + s->generic.parent->x + SLIDER_RANGE * MENU_FONT_SIZE + RCOLUMN_OFFSET,
				 s->generic.y + s->generic.parent->y, ALIGN_CENTER, 130, 255, 255, 255, 255, false, false);

	// Convert curvalue to 0..1 range
	float range = (s->curvalue - s->minvalue) / (s->maxvalue - s->minvalue);
	range = clamp(range, 0, 1);

	// Draw slider thumb
	SCR_DrawChar(s->generic.x + s->generic.parent->x + MENU_FONT_SIZE * ((SLIDER_RANGE - 1) * range) + RCOLUMN_OFFSET + MENU_FONT_SIZE * 0.5f,
				 s->generic.y + s->generic.parent->y, ALIGN_CENTER, 131, 255, 255, 255, 255, false, true);

	//mxd. Draw value
	const float fvalue = (s->cvar ? s->cvar->value : s->curvalue);
	
	char *value;
	if (s->numdecimals > 0) // Draw with fixed number of decimals?
		value = va("%.*f", s->numdecimals, fvalue);
	else
		value = va("%g", fvalue);

	Menu_DrawString(s->generic.x + s->generic.parent->x + SLIDER_RANGE * MENU_FONT_SIZE + RCOLUMN_OFFSET + MENU_FONT_SIZE * 1.5f,
					s->generic.y + s->generic.parent->y, value, alpha);

	//mxd. Ownerdraw support for all item types
	if (s->generic.ownerdraw)
		s->generic.ownerdraw(s);
}

/*static void SpinControl_DoEnter(menulist_s *s)
{
	if (!s->itemnames || !s->numitemnames)
		return;

	s->curvalue++;
	if (s->itemnames[s->curvalue] == 0)
		s->curvalue = 0;

	if (s->generic.callback)
		s->generic.callback(s);
}*/

static void SpinControl_DoSlide(menulist_s *s, int dir)
{
	if (!s->itemnames || !s->numitemnames)
		return;

	s->curvalue += dir;

	if (s->curvalue < 0)
		s->curvalue = s->numitemnames - 1; // was 0
	else if (s->itemnames[s->curvalue] == 0)
		s->curvalue = 0; // was --

	if (s->generic.callback)
		s->generic.callback(s);
}
 
void SpinControl_Draw(menulist_s *s)
{
	const int alpha = MouseOverAlpha(&s->generic);
	char buffer[100];

	if (s->generic.name)
	{
		Menu_DrawStringR2LDark(s->generic.x + s->generic.parent->x + LCOLUMN_OFFSET,
							   s->generic.y + s->generic.parent->y, s->generic.name, alpha);
	}

	if (!strchr(s->itemnames[s->curvalue], '\n'))
	{
		Menu_DrawString(s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET,
						s->generic.y + s->generic.parent->y, s->itemnames[s->curvalue], alpha);
	}
	else
	{
		Q_strncpyz(buffer, s->itemnames[s->curvalue], sizeof(buffer));
		*strchr(buffer, '\n') = 0;
		Menu_DrawString(s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET,
						s->generic.y + s->generic.parent->y, buffer, alpha);
		
		Q_strncpyz(buffer, strchr(s->itemnames[s->curvalue], '\n') + 1, sizeof(buffer));
		Menu_DrawString(s->generic.x + s->generic.parent->x + RCOLUMN_OFFSET,
						s->generic.y + s->generic.parent->y + MENU_LINE_SIZE, buffer, alpha);
	}

	//mxd. Ownerdraw support for all item types
	if (s->generic.ownerdraw)
		s->generic.ownerdraw(s);
}

void Menu_AddItem(menuframework_s *menu, void *item)
{
	if (menu->nitems == 0)
		menu->nslots = 0;

	if (menu->nitems < MAXMENUITEMS)
	{
		menu->items[menu->nitems] = item;
		((menucommon_s *)menu->items[menu->nitems])->parent = menu;
		menu->nitems++;
	}

	menu->nslots = Menu_TallySlots(menu);
	menulist_s* list = (menulist_s *)item;

	if(list->generic.type == MTYPE_SPINCONTROL)
	{
		int i = 0;
		while (list->itemnames[i])
			i++;

		list->numitemnames = i;
	}
}

// Checks if an item can be used as a cursor position.
qboolean Menu_ItemIsValidCursorPosition(void *item)
{
	if (!item)
		return false;

	// Hidden items are invalid
	menucommon_s *citem = (menucommon_s *)item;
	if (citem->flags & QMF_HIDDEN)
		return false;

	return (citem->type != MTYPE_SEPARATOR);
}

// This function takes the given menu, the direction, and attempts
// to adjust the menu's cursor so that it's at the next available slot.
void Menu_AdjustCursor(menuframework_s *m, int dir)
{
	// See if it's in a valid spot
	if (m->cursor >= 0 && m->cursor < m->nitems && Menu_ItemIsValidCursorPosition(Menu_ItemAtCursor(m)))
		return;

	// It's not in a valid spot, so crawl in the direction indicated until we find a valid spot
	while (true)
	{
		if (Menu_ItemIsValidCursorPosition(Menu_ItemAtCursor(m)))
			break;

		m->cursor += dir;
		if (m->cursor >= m->nitems)
			m->cursor = 0;
		else if(m->cursor < 0)
			m->cursor = m->nitems - 1;
	}
}

void Menu_Center(menuframework_s *menu)
{
	const int height = ((menucommon_s *)menu->items[menu->nitems - 1])->y + 10;
	menu->y = (SCREEN_HEIGHT - height) * 0.5f;
}

void Menu_Draw(menuframework_s *menu)
{
	// Draw contents
	for (int i = 0; i < menu->nitems; i++)
	{
		// Skip hidden items
		if (((menucommon_s *)menu->items[i])->flags & QMF_HIDDEN)
			continue;

		switch (((menucommon_s *)menu->items[i])->type)
		{
			case MTYPE_FIELD:
				Field_Draw((menufield_s *)menu->items[i]);
				break;

			case MTYPE_SLIDER:
				Slider_Draw((menuslider_s *)menu->items[i]);
				break;

			case MTYPE_LIST:
				MenuList_Draw((menulist_s *)menu->items[i]);
				break;

			case MTYPE_SPINCONTROL:
				SpinControl_Draw((menulist_s *)menu->items[i]);
				break;

			case MTYPE_ACTION:
				Action_Draw((menuaction_s *)menu->items[i]);
				break;

			case MTYPE_SEPARATOR:
				Separator_Draw((menuseparator_s *)menu->items[i]);
				break;
		}
	}

	// Now check mouseovers - psychospaz
	cursor.menu = menu;

	if (cursor.mouseaction)
	{
		menucommon_s *lastitem = cursor.menuitem;
		UI_RefreshCursorLink();

		for (int i = menu->nitems; i >= 0 ; i--)
		{
			menucommon_s *item = ((menucommon_s *)menu->items[i]);

			if (!item || item->type == MTYPE_SEPARATOR)
				continue;

			float x1 = menu->x + item->x + RCOLUMN_OFFSET; // + 2 chars for space + cursor
			float y1 = menu->y + item->y;
			float w1 = 0;
			float h1 = MENU_FONT_SIZE;

			SCR_AdjustFrom640(&x1, &y1, &w1, &h1, ALIGN_CENTER);

			int min[2] = { x1, y1 };
			int max[2] = { x1 + w1, y1 + h1 };

			int type;
			switch (item->type)
			{
				case MTYPE_ACTION:
					{
						const int len = strlen(item->name);
						
						if (item->flags & QMF_LEFT_JUSTIFY)
						{
							min[0] += SCR_ScaledVideo(LCOLUMN_OFFSET * 2);
							max[0] = min[0] + SCR_ScaledVideo(len * MENU_FONT_SIZE);
						}
						else
						{
							min[0] -= SCR_ScaledVideo(len * MENU_FONT_SIZE + MENU_FONT_SIZE * 3);
						}

						type = MENUITEM_ACTION;
					}
					break;

				case MTYPE_SLIDER:
					{
						if (item->name)
						{
							const int len = strlen(item->name);
							min[0] -= SCR_ScaledVideo(len * MENU_FONT_SIZE - LCOLUMN_OFFSET * 2);
						}
						else
						{
							min[0] -= SCR_ScaledVideo(16);
						}

						max[0] += SCR_ScaledVideo((SLIDER_RANGE + 4) * MENU_FONT_SIZE);
						type = MENUITEM_SLIDER;
					}
					break;

				case MTYPE_LIST:
				case MTYPE_SPINCONTROL:
					{
						menulist_s *spin = menu->items[i];

						if (item->name)
							min[0] -= SCR_ScaledVideo(strlen(item->name) * MENU_FONT_SIZE - LCOLUMN_OFFSET * 2);

						const int len = strlen(spin->itemnames[spin->curvalue]);
						max[0] += SCR_ScaledVideo(len * MENU_FONT_SIZE);

						type = MENUITEM_ROTATE;
					}
					break;

				case MTYPE_FIELD:
					{
						menufield_s *text = menu->items[i];

						const int len = text->visible_length + 2;

						max[0] += SCR_ScaledVideo(len * MENU_FONT_SIZE);
						type = MENUITEM_TEXT;
					}
					break;

				default:
					continue;
			}

			if (   cursor.x >= min[0] && cursor.x <= max[0]
				&& cursor.y >= min[1] && cursor.y <= max[1])
			{
				// New item
				if (lastitem != item)
				{
					for (int j = 0; j < MENU_CURSOR_BUTTON_MAX; j++)
					{
						cursor.buttonclicks[j] = 0;
						cursor.buttontime[j] = 0;
					}
				}

				cursor.menuitem = item;
				cursor.menuitemtype = type;
				
				menu->cursor = i;

				break;
			}
		}

		//mxd. Reset menu mouse clicks when mouse moved away from an item (some interactive menu elements aren't added as items, 
		// so buttonclicks from a prevoiusly clicked menu item are applied to them on hover...)
		//TODO: redo those items as menuitems, remove explicit calls to PlayerConfig_MouseClick and MenuCrosshair_MouseClick in UI_ThinkMouseCursor (Knightmare already did this in his current build?)
		if(lastitem != NULL && cursor.menuitem == NULL)
		{
			for (int i = 0; i < MENU_CURSOR_BUTTON_MAX; i++)
			{
				cursor.buttonclicks[i] = 0;
				cursor.buttontime[i] = 0;
			}
		}
	}

	cursor.mouseaction = false;
	// end mouseover code

	menucommon_s *item = Menu_ItemAtCursor(menu);

	if (item && item->cursordraw)
	{
		item->cursordraw(item);
	}
	else if (menu->cursordraw)
	{
		menu->cursordraw(menu);
	}
	else if (item && item->type != MTYPE_FIELD)
	{
		int x = menu->x + item->cursor_offset; //mxd
		if (item->flags & QMF_LEFT_JUSTIFY)
			x += item->x - 24;

		SCR_DrawChar(x, menu->y + item->y,
			ALIGN_CENTER, 12 + ((int)(Sys_Milliseconds() / 250) & 1),
			255, 255, 255, 255, false, true);
	}

	if (item && item->statusbarfunc)
		item->statusbarfunc((void *)item);
	else if (item && item->statusbar)
		Menu_DrawStatusBar(item->statusbar);
	else
		Menu_DrawStatusBar(menu->statusbar);
}

void Menu_DrawStatusBar(const char *string) //mxd. Similar logic used in DrawDemoMessage()
{
	SCR_DrawFill(0, SCREEN_HEIGHT - (MENU_FONT_SIZE + 3), SCREEN_WIDTH, MENU_FONT_SIZE + 4, ALIGN_BOTTOM_STRETCH, 0, 0, 0, 255); // Black shade
	
	if (string)
	{
		SCR_DrawFill(0, SCREEN_HEIGHT - (MENU_FONT_SIZE + 2), SCREEN_WIDTH, MENU_FONT_SIZE + 2, ALIGN_BOTTOM_STRETCH, 60, 60, 60, 255); // Gray shade
		SCR_DrawString(SCREEN_WIDTH / 2 - (strlen(string) / 2) * MENU_FONT_SIZE, SCREEN_HEIGHT - (MENU_FONT_SIZE + 1), ALIGN_BOTTOM, string, 255);
	}
}

void Menu_DrawString(int x, int y, const char *string, int alpha)
{
	SCR_DrawString(x, y, ALIGN_CENTER, string, alpha);
}

void Menu_DrawStringDark(int x, int y, const char *string, int alpha)
{
	char newstring[1024];
	Com_sprintf(newstring, sizeof(newstring), S_COLOR_ALT"%s", string);
	SCR_DrawString(x, y, ALIGN_CENTER, newstring, alpha);
}

void Menu_DrawStringR2L(int x, int y, const char *string, int alpha)
{
	x -= CL_UnformattedStringLength(string) * MENU_FONT_SIZE;
	SCR_DrawString(x, y, ALIGN_CENTER, string, alpha);
}

void Menu_DrawStringR2LDark(int x, int y, const char *string, int alpha)
{
	char newstring[1024];
	Com_sprintf(newstring, sizeof(newstring), S_COLOR_ALT"%s", string);
	x -= CL_UnformattedStringLength(string) * MENU_FONT_SIZE;
	SCR_DrawString(x, y, ALIGN_CENTER, newstring, alpha);
}

void Menu_DrawTextBox(int x, int y, int width, int lines)
{
	// Draw left side
	int cx = x;
	int cy = y;

	SCR_DrawChar(cx, cy, ALIGN_CENTER, 1, 255, 255, 255, 255, false, false);
	for (int n = 0; n < lines; n++)
	{
		cy += MENU_FONT_SIZE;
		SCR_DrawChar(cx, cy, ALIGN_CENTER, 4, 255, 255, 255, 255, false, false);
	}
	SCR_DrawChar(cx, cy + MENU_FONT_SIZE, ALIGN_CENTER, 7, 255, 255, 255, 255, false, false);

	// Draw middle
	cx += MENU_FONT_SIZE;
	while (width > 0)
	{
		cy = y;
		SCR_DrawChar(cx, cy, ALIGN_CENTER, 2, 255, 255, 255, 255, false, false);
		for (int n = 0; n < lines; n++)
		{
			cy += MENU_FONT_SIZE;
			SCR_DrawChar(cx, cy, ALIGN_CENTER, 5, 255, 255, 255, 255, false, false);
		}
		SCR_DrawChar(cx, cy + MENU_FONT_SIZE, ALIGN_CENTER, 8, 255, 255, 255, 255, false, false);
		width -= 1;
		cx += MENU_FONT_SIZE;
	}

	// Draw right side
	cy = y;
	SCR_DrawChar(cx, cy, ALIGN_CENTER, 3, 255, 255, 255, 255, false, false);
	for (int n = 0; n < lines; n++)
	{
		cy += MENU_FONT_SIZE;
		SCR_DrawChar(cx, cy, ALIGN_CENTER, 6, 255, 255, 255, 255, false, false);
	}
	SCR_DrawChar(cx, cy + MENU_FONT_SIZE, ALIGN_CENTER, 9, 255, 255, 255, 255, false, true);
}

void Menu_DrawBanner(char *name)
{
	int w, h;
	R_DrawGetPicSize(&w, &h, name);
	SCR_DrawPic(SCREEN_WIDTH / 2 - w / 2, SCREEN_HEIGHT / 2 - 150, w, h, ALIGN_CENTER, name, 1.0f);
}


void *Menu_ItemAtCursor(menuframework_s *m)
{
	if (m->cursor < 0 || m->cursor >= m->nitems)
		return 0;

	return m->items[m->cursor];
}

qboolean Menu_SelectItem(menuframework_s *s)
{
	menucommon_s *item = (menucommon_s *)Menu_ItemAtCursor(s);
	if (!item)
		return false;

	switch (item->type)
	{
		case MTYPE_FIELD:
			return Field_DoEnter((menufield_s *)item);

		case MTYPE_ACTION:
			Action_DoEnter((menuaction_s *)item);
			return true;

		case MTYPE_LIST:
			//Menulist_DoEnter((menulist_s *)item);
			return false;

		case MTYPE_SPINCONTROL:
			//SpinControl_DoEnter((menulist_s *)item);
			return false;

		default:
			return false;
	}
}

qboolean Menu_MouseSelectItem(menucommon_s *item)
{
	if (!item)
		return false;
	
	switch (item->type)
	{
		case MTYPE_FIELD:
			return Field_DoEnter((menufield_s *)item);

		case MTYPE_ACTION:
			Action_DoEnter((menuaction_s *)item);
			return true;

		case MTYPE_LIST:
		case MTYPE_SPINCONTROL:
		default:
			return false;
	}
}

void Menu_SlideItem(menuframework_s *s, int dir)
{
	menucommon_s *item = (menucommon_s *)Menu_ItemAtCursor(s);
	if (!item)
		return;

	if(item->type == MTYPE_SLIDER)
		Slider_DoSlide((menuslider_s *)item, dir);
	else if(item->type == MTYPE_SPINCONTROL)
		SpinControl_DoSlide((menulist_s *)item, dir);
}

int Menu_TallySlots(menuframework_s *menu)
{
	int total = 0;

	for (int i = 0; i < menu->nitems; i++)
	{
		if (((menucommon_s *)menu->items[i])->type == MTYPE_LIST)
		{
			int nitems = 0;
			const char **n = ((menulist_s *)menu->items[i])->itemnames;

			while (*n)
			{
				nitems++;
				n++;
			}

			total += nitems;
		}
		else
		{
			total++;
		}
	}

	return total;
}

#pragma region ======================= Menu Mouse Cursor - psychospaz

void UI_RefreshCursorMenu(void)
{
	cursor.menu = NULL;
}

void UI_RefreshCursorLink(void)
{
	cursor.menuitem = NULL;
}

extern void(*m_drawfunc)(void);

void UI_Think_MouseCursor(void)
{
	char *sound = NULL;
	menuframework_s *m = (menuframework_s *)cursor.menu;

	if (m_drawfunc == M_Main_Draw) // have to hack for main menu :p
	{
		UI_CheckMainMenuMouse();
		return;
	}

	if (m_drawfunc == M_Credits_MenuDraw) // have to hack for credits :p
	{
		if (cursor.buttonclicks[MOUSEBUTTON2])
		{
			cursor.buttonused[MOUSEBUTTON2] = true;
			cursor.buttonclicks[MOUSEBUTTON2] = 0;
			cursor.buttonused[MOUSEBUTTON1] = true;
			cursor.buttonclicks[MOUSEBUTTON1] = 0;
			S_StartLocalSound(menu_out_sound);

			if (creditsBuffer)
				FS_FreeFile(creditsBuffer);

			UI_PopMenu();

			return;
		}
	}

	if (!m)
		return;

	// Exit with double click 2nd mouse button
	if (cursor.menuitem)
	{
		// MOUSE1
		if (cursor.buttondown[MOUSEBUTTON1] && cursor.buttonclicks[MOUSEBUTTON1] && !cursor.buttonused[MOUSEBUTTON1])
		{
			if (cursor.menuitemtype == MENUITEM_SLIDER)
			{
				if(Slider_MouseClick(cursor.menuitem)) //mxd. Play sound only when slider value has changed, otherwise we'll play 1 menu_move_sound per frame
					sound = menu_move_sound;
				//mxd. Don't release the button, so user can drag the slider
			}
			else if (cursor.menuitemtype == MENUITEM_ROTATE)
			{
				Menu_SlideItem(m, (menu_rotate->value ? -1 : 1));
				sound = menu_move_sound;
				cursor.buttonused[MOUSEBUTTON1] = true;
			}
			else
			{
				cursor.buttonused[MOUSEBUTTON1] = true;
				Menu_MouseSelectItem(cursor.menuitem);
				sound = menu_move_sound;
			}
		}

		// MOUSE2
		if (cursor.buttondown[MOUSEBUTTON2] && cursor.buttonclicks[MOUSEBUTTON2] && !cursor.buttonused[MOUSEBUTTON2])
		{
			if (cursor.menuitemtype == MENUITEM_SLIDER)
			{
				if(Slider_MouseClick(cursor.menuitem)) //mxd
					sound = menu_move_sound;
				//mxd. Don't release the button, so user can drag the slider
			}
			else if (cursor.menuitemtype == MENUITEM_ROTATE)
			{
				Menu_SlideItem(m, (menu_rotate->value ? 1 : -1));
				sound = menu_move_sound;
				cursor.buttonused[MOUSEBUTTON2] = true;
			}
		}
	}
	else if (!cursor.buttonused[MOUSEBUTTON2] && (cursor.buttonclicks[MOUSEBUTTON2] == 2) && cursor.buttondown[MOUSEBUTTON2])
	{
		//mxd. Emulate Esc press to let current menu code handle the closing. 
		UI_Keydown(K_ESCAPE);

		cursor.buttonused[MOUSEBUTTON2] = true;
		cursor.buttonclicks[MOUSEBUTTON2] = 0;
		cursor.buttonused[MOUSEBUTTON1] = true;
		cursor.buttonclicks[MOUSEBUTTON1] = 0;
	}

	// Clicking on the player model menu...
	if (m_drawfunc == PlayerConfig_MenuDraw)
		PlayerConfig_MouseClick();

	// Clicking on the screen menu
	if (m_drawfunc == Options_Screen_MenuDraw)
		MenuCrosshair_MouseClick();

	if (sound)
		S_StartLocalSound(sound);
}

#if 1

void UI_Draw_Cursor(void) //TODO: mxd. Switch to "hover" cursor when hovering over items?
{
	const float scale = SCR_ScaledVideo(ui_cursor_scale->value); // 0.4
	char *cur_img = UI_MOUSECURSOR_PIC;

	// Get sizing vars
	int w, h;
	R_DrawGetPicSize(&w, &h, UI_MOUSECURSOR_PIC);
	const float ofs_x = SCR_ScaledVideo(w) * ui_cursor_scale->value * 0.5f;
	const float ofs_y = SCR_ScaledVideo(h) * ui_cursor_scale->value * 0.5f;
	
	R_DrawScaledPic(cursor.x - ofs_x, cursor.y - ofs_y, scale, 1.0f, cur_img);
}

#else

void UI_Draw_Cursor (void)
{
	float	alpha = 1, scale = SCR_ScaledVideo(0.66);
	int		w, h;
	char	*overlay = NULL;
	char	*cur_img = NULL;

	if (m_drawfunc == M_Main_Draw)
	{
		if (MainMenuMouseHover)
		{
			if ((cursor.buttonused[0] && cursor.buttonclicks[0])
				|| (cursor.buttonused[1] && cursor.buttonclicks[1]))
			{
				cur_img = "/gfx/ui/cursors/m_cur_click.pcx";
				alpha = 0.85 + 0.15*sin(anglemod(cl.time*0.005));
			}
			else
			{
				cur_img = "/gfx/ui/cursors/m_cur_hover.pcx";
				alpha = 0.85 + 0.15*sin(anglemod(cl.time*0.005));
			}
		}
		else
			cur_img = "/gfx/ui/cursors/m_cur_main.pcx";
		overlay = "/gfx/ui/cursors/m_cur_over.pcx";
	}
	else
	{
		if (cursor.menuitem)
		{
			if (cursor.menuitemtype == MENUITEM_TEXT)
			{
				cur_img = "/gfx/ui/cursors/m_cur_text.pcx";
			}
			else
			{
				if ((cursor.buttonused[0] && cursor.buttonclicks[0])
					|| (cursor.buttonused[1] && cursor.buttonclicks[1]))
				{
					cur_img = "/gfx/ui/cursors/m_cur_click.pcx";
					alpha = 0.85 + 0.15*sin(anglemod(cl.time*0.005));
				}
				else
				{
					cur_img = "/gfx/ui/cursors/m_cur_hover.pcx";
					alpha = 0.85 + 0.15*sin(anglemod(cl.time*0.005));
				}
				overlay = "/gfx/ui/cursors/m_cur_over.pcx";
			}
		}
		else
		{
			cur_img = "/gfx/ui/cursors/m_cur_main.pcx";
			overlay = "/gfx/ui/cursors/m_cur_over.pcx";
		}
	}
	
	if (cur_img)
	{
		R_DrawGetPicSize( &w, &h, cur_img );
		R_DrawScaledPic( cursor.x - scale*w/2, cursor.y - scale*h/2, scale, alpha, cur_img);

		if (overlay) {
			R_DrawGetPicSize( &w, &h, overlay );
			R_DrawScaledPic( cursor.x - scale*w/2, cursor.y - scale*h/2, scale, 1, overlay);
		}
	}
}

#endif

#pragma endregion