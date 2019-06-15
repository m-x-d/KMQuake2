/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2000-2002 Mr. Hyde and Mad Dog

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

#include "g_local.h"

#pragma region ======================= Packet filtering

/*
You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".
Removeip will only remove an address specified exactly the same way. You cannot addip a subnet, then removeip a single host.

listip
Prints the current list of filters.

writeip
Dumps "addip <ip>" commands to listip.cfg so it can be execed at a later date. The filter lists are not saved and restored by default, because I beleive it would cause too much confusion.

filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game. This is the default setting.

If 0, then only addresses matching the list will be allowed. This lets you easily set up a private game, or a game that only allows players from your local network.
*/

typedef struct
{
	unsigned mask;
	unsigned compare;
} ipfilter_t;

#define	MAX_IPFILTERS 1024

static ipfilter_t ipfilters[MAX_IPFILTERS];
static int numipfilters;

static qboolean StringToFilter(char *s, ipfilter_t *f)
{
	char num[128];
	byte b[4] = { 0, 0, 0, 0 };
	byte m[4] = { 0, 0, 0, 0 };
	
	for (int i = 0; i < 4; i++)
	{
		if (*s < '0' || *s > '9')
		{
			safe_cprintf(NULL, PRINT_HIGH, "Bad filter address: %s\n", s);
			return false;
		}
		
		int j = 0;
		while (*s >= '0' && *s <= '9')
			num[j++] = *s++;

		num[j] = 0;
		b[i] = atoi(num);
		if (b[i] != 0)
			m[i] = 255;

		if (!*s)
			break;

		s++;
	}
	
	f->mask = *(unsigned *)m;
	f->compare = *(unsigned *)b;
	
	return true;
}

qboolean SV_FilterPacket(char *from)
{
	byte m[4];

	int c = 0;
	char *p = from;
	while (*p && c < 4)
	{
		m[c] = 0;
		while (*p >= '0' && *p <= '9')
		{
			m[c] = m[c] * 10 + (*p - '0');
			p++;
		}

		if (!*p || *p == ':')
			break;

		c++;
		p++;
	}

	const unsigned in = *(unsigned *)m;

	for (int i = 0; i < numipfilters; i++)
		if ((in & ipfilters[i].mask) == ipfilters[i].compare)
			return (int)filterban->value;

	return (int)!filterban->value;
}

#pragma endregion

#pragma region ======================= Server-side console commands

void Svcmd_Test_f(void)
{
	safe_cprintf(NULL, PRINT_HIGH, "Svcmd_Test_f()\n");
}

void SVCmd_AddIP_f(void)
{
	if (gi.argc() < 3)
	{
		safe_cprintf(NULL, PRINT_HIGH, "Usage: addip <ip-mask>\n");
		return;
	}

	int ipfilter;
	for (ipfilter = 0; ipfilter < numipfilters; ipfilter++)
		if (ipfilters[ipfilter].compare == 0xffffffff)
			break; // Free spot

	if (ipfilter == numipfilters)
	{
		if (numipfilters == MAX_IPFILTERS)
		{
			safe_cprintf(NULL, PRINT_HIGH, S_COLOR_YELLOW"IP filter list is full.\n");
			return;
		}

		numipfilters++;
	}
	
	if (!StringToFilter(gi.argv(2), &ipfilters[ipfilter]))
		ipfilters[ipfilter].compare = 0xffffffff;
}

void SVCmd_RemoveIP_f(void)
{
	if (gi.argc() < 3)
	{
		safe_cprintf(NULL, PRINT_HIGH, "Usage: sv removeip <ip-mask>\n");
		return;
	}

	ipfilter_t f;
	if (!StringToFilter(gi.argv(2), &f))
		return;

	for (int i = 0; i < numipfilters; i++)
	{
		if (ipfilters[i].mask == f.mask && ipfilters[i].compare == f.compare)
		{
			for (int j = i + 1; j < numipfilters; j++)
				ipfilters[j - 1] = ipfilters[j];

			numipfilters--;
			safe_cprintf(NULL, PRINT_HIGH, "IP removed.\n");

			return;
		}
	}

	safe_cprintf(NULL, PRINT_HIGH, S_COLOR_YELLOW"Didn't find '%s'.\n", gi.argv(2));
}

void SVCmd_ListIP_f(void)
{
	byte b[4];

	safe_cprintf(NULL, PRINT_HIGH, "Filter list:\n");
	for (int i = 0; i < numipfilters; i++)
	{
		*(unsigned *)b = ipfilters[i].compare;
		safe_cprintf(NULL, PRINT_HIGH, "%3i.%3i.%3i.%3i\n", b[0], b[1], b[2], b[3]);
	}
}

void SVCmd_WriteIP_f(void)
{
	cvar_t *game = gi.cvar("game", "", 0);

	char name[MAX_OSPATH];
	Com_sprintf(name, sizeof(name), "%s/listip.cfg", (*game->string ? game->string : GAMEVERSION));

	safe_cprintf(NULL, PRINT_HIGH, "Writing '%s'.\n", name);

	FILE *f = fopen(name, "wb");
	if (!f)
	{
		safe_cprintf(NULL, PRINT_HIGH, S_COLOR_YELLOW"Couldn't open '%s'\n", name);
		return;
	}
	
	fprintf(f, "set filterban %d\n", (int)filterban->value);

	byte b[4];
	for (int i = 0; i < numipfilters; i++)
	{
		*(unsigned *)b = ipfilters[i].compare;
		fprintf(f, "sv addip %i.%i.%i.%i\n", b[0], b[1], b[2], b[3]);
	}
	
	fclose(f);
}

#pragma endregion

// ServerCommand will be called when an "sv" command is issued.
// The game can issue gi.argc() / gi.argv() commands to get the rest of the parameters
void ServerCommand(void)
{
	char* cmd = gi.argv(1);

	if (Q_stricmp(cmd, "test") == 0)
	{
		Svcmd_Test_f();
	}
	else if (Q_stricmp(cmd, "addip") == 0)
	{
		SVCmd_AddIP_f();
	}
	else if (Q_stricmp(cmd, "removeip") == 0)
	{
		SVCmd_RemoveIP_f();
	}
	else if (Q_stricmp(cmd, "listip") == 0)
	{
		SVCmd_ListIP_f();
	}
	else if (Q_stricmp(cmd, "writeip") == 0)
	{
		SVCmd_WriteIP_f();
	}
	else if (Q_stricmp(cmd, "acedebug") == 0) // ACEBOT_ADD
	{
		if (gi.argc() == 2) // sv acedebug
		{
			debug_mode = !debug_mode;
			safe_bprintf(PRINT_MEDIUM, "ACE: debug mode %s.\n", (debug_mode ? "enabled" : "disabled"));
		}
		else if (gi.argc() == 3)
		{
			debug_mode = atoi(gi.argv(2));
			safe_bprintf(PRINT_MEDIUM, "ACE: debug mode %s.\n", (debug_mode ? "enabled" : "disabled"));
		}
		else
		{
			safe_bprintf(PRINT_MEDIUM, "Usage: sv acedebug <enable>\n");
		}
	}
	else if (Q_stricmp(cmd, "addbot") == 0)
	{
		if (!deathmatch->integer) // Knightmare added
		{
			safe_bprintf(PRINT_MEDIUM, S_COLOR_YELLOW"ACE: Can only spawn bots in deathmatch mode.\n");
			return;
		}

		if (ctf->integer) // team, name, skin
			ACESP_SpawnBot(gi.argv(2), gi.argv(3), gi.argv(4), NULL);
		else // name, skin
			ACESP_SpawnBot(NULL, gi.argv(2), gi.argv(3), NULL);
	}	
	else if (Q_stricmp(cmd, "removebot") == 0)
	{
		ACESP_RemoveBot(gi.argv(2));
	}
	else if (Q_stricmp(cmd, "savenodes") == 0)
	{
		ACEND_SaveNodes();
	} // ACEBOT_END
	else if (Q_stricmp(cmd, "dmpause") == 0) // Knightmare added- DM pause
	{
		if (!deathmatch->value)
		{
			safe_cprintf(NULL, PRINT_HIGH, S_COLOR_YELLOW"Dmpause only works in deathmatch.\n", cmd);
			paused = false;

			return;
		}

		paused = !paused;

		if (!paused) // Unfreeze players
		{
			for (int i = 0; i < game.maxclients; i++)
			{
				edict_t* player = &g_edicts[i + 1];
				if (!player->inuse || !player->client)
					continue;

				if (player->is_bot || player->client->ctf_grapple)
					continue;

				player->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
			}

			safe_bprintf(PRINT_HIGH, "Game unpaused\n");
		}
	}
	else
	{
		safe_cprintf(NULL, PRINT_HIGH, S_COLOR_YELLOW"Unknown server command: \"%s\".\n", cmd);
	}
}