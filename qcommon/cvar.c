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
// cvar.c -- dynamic variable tracking

#include "qcommon.h"
#include "wildcard.h"

cvar_t	*cvar_vars;
qboolean cvar_allowCheats = true;

static qboolean Cvar_InfoValidate(char *s)
{
	return (!strchr(s, '\\') && !strchr(s, '\"') && !strchr(s, ';')); //mxd. strstr -> strchr
}

cvar_t *Cvar_FindVar(char *var_name)
{
	for (cvar_t *var = cvar_vars; var; var = var->next)
		if (!strcmp(var_name, var->name))
			return var;

	return NULL;
}

float Cvar_VariableValue(char *var_name)
{
	cvar_t *var = Cvar_FindVar(var_name);
	return (var ? var->value : 0); //mxd
}

int Cvar_VariableInteger(char *var_name)
{
	cvar_t *var = Cvar_FindVar(var_name);
	return (var ? var->integer : 0); //mxd
}

char *Cvar_VariableString(char *var_name)
{
	cvar_t *var = Cvar_FindVar(var_name);
	return (var ? var->string : "");
}

// Knightmare added
float Cvar_DefaultValue(char *var_name)
{
	cvar_t *var = Cvar_FindVar(var_name);
	return (var ? (float)atof(var->default_string) : 0);
}

// Knightmare added
int Cvar_DefaultInteger(char *var_name)
{
	cvar_t *var = Cvar_FindVar(var_name);
	return (var ? atoi(var->default_string) : 0);
}

// Knightmare added
char *Cvar_DefaultString(char *var_name)
{
	cvar_t *var = Cvar_FindVar(var_name);
	return (var ? var->default_string : "");
}

void Cvar_CompleteVariable(const char *partial, void(*callback)(const char *found)) //mxd. +callback
{
	if (!*partial)
		return;

	//mxd. Check, whether cvar name contains target string
	for (cvar_t *cvar = cvar_vars; cvar; cvar = cvar->next)
		if (Q_strcasestr(cvar->name, partial))
			callback(cvar->name);
}

// If the variable already exists, the value will not be set.
// The flags will be or'ed in if the variable exists.
cvar_t *Cvar_Get(char *var_name, char *var_value, int flags)
{
	if ((flags & (CVAR_USERINFO | CVAR_SERVERINFO)) && !Cvar_InfoValidate(var_name))
	{
		Com_Printf("Invalid info cvar name: '%s'\n", var_name);
		return NULL;
	}

	cvar_t *var = Cvar_FindVar(var_name);
	if (var)
	{
		var->flags |= flags;

		// Knightmare- added cvar defaults
		Z_Free(var->default_string);
		var->default_string = CopyString(var_value);

		return var;
	}

	if (!var_value)
		return NULL;

	if ((flags & (CVAR_USERINFO | CVAR_SERVERINFO)) && !Cvar_InfoValidate(var_value))
	{
		Com_Printf("Invalid info cvar value: '%s'\n", var_value);
		return NULL;
	}

	var = Z_Malloc(sizeof(*var));
	var->name = CopyString(var_name);
	var->string = CopyString(var_value);

	// Knightmare- added cvar defaults
	var->default_string = CopyString(var_value);
	var->modified = true;
	var->value = (float)atof(var->string);
	var->integer = atoi(var->string);

	// Link the variable in
	var->next = cvar_vars;
	cvar_vars = var;

	var->flags = flags;

	return var;
}

cvar_t *Cvar_Set2(char *var_name, char *value, qboolean force)
{
	cvar_t *var = Cvar_FindVar(var_name);
	if (!var)
		return Cvar_Get(var_name, value, 0); // Create it

	if ((var->flags & (CVAR_USERINFO | CVAR_SERVERINFO)) && !Cvar_InfoValidate(value))
	{
		Com_Printf("Invalid info cvar value: '%s'\n", value);
		return var;
	}

	if (!force)
	{
		if (var->flags & CVAR_NOSET)
		{
			Com_Printf("cvar '%s' is write-protected\n", var_name);
			return var;
		}

		if ((var->flags & CVAR_CHEAT) && !cvar_allowCheats)
		{
			Com_Printf("cvar '%s' is cheat protected.\n", var_name);
			return var;
		}

		if (var->flags & CVAR_LATCH)
		{
			if (var->latched_string)
			{
				if (strcmp(value, var->latched_string) == 0)
					return var;

				Z_Free(var->latched_string);
			}
			else
			{
				if (strcmp(value, var->string) == 0)
					return var;
			}

			if (Com_ServerState())
			{
				Com_Printf("cvar '%s' will be changed for the next game.\n", var_name);
				var->latched_string = CopyString(value);
			}
			else
			{
				var->string = CopyString(value);
				var->value = (float)atof(var->string);
				var->integer = atoi(var->string);

				if (!strcmp(var->name, "game"))
				{
					FS_SetGamedir(var->string);
					FS_ExecAutoexec();
				}
			}

			return var;
		}
	}
	else
	{
		if (var->latched_string)
		{
			Z_Free(var->latched_string);
			var->latched_string = NULL;
		}
	}

	if (!strcmp(value, var->string))
		return var; // Not changed

	var->modified = true;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true; // Transmit at next oportunity
	
	Z_Free(var->string); // Free the old value string
	
	var->string = CopyString(value);
	var->value = (float)atof(var->string);
	var->integer = atoi(var->string);

	return var;
}

cvar_t *Cvar_ForceSet(char *var_name, char *value)
{
	return Cvar_Set2(var_name, value, true);
}

cvar_t *Cvar_Set(char *var_name, char *value)
{
	return Cvar_Set2(var_name, value, false);
}

// Knightmare added
cvar_t *Cvar_SetToDefault(char *var_name)
{
	return Cvar_Set2(var_name, Cvar_DefaultString(var_name), false);
}

cvar_t *Cvar_FullSet(char *var_name, char *value, int flags)
{
	cvar_t *var = Cvar_FindVar(var_name);
	if (!var)
		return Cvar_Get(var_name, value, flags); // Create it

	var->modified = true;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true; // Transmit at next oportunity
	
	Z_Free(var->string); // Free the old value string
	
	var->string = CopyString(value);
	var->value = (float)atof(var->string);
	var->integer = atoi(var->string);
	var->flags = flags;

	return var;
}

void Cvar_SetValue(char *var_name, float value)
{
	char val[32];
	Com_sprintf(val, sizeof(val), "%g", value); //mxd. Avoid trailing zeroes (was %i or %f)
	Cvar_Set(var_name, val);
}

void Cvar_SetInteger(char *var_name, int integer)
{
	char val[32];
	Com_sprintf(val, sizeof(val), "%i", integer);
	Cvar_Set(var_name, val);
}

// Any variables with latched values will now be updated
void Cvar_GetLatchedVars(void)
{
	for (cvar_t *var = cvar_vars ; var ; var = var->next)
	{
		if (!var->latched_string)
			continue;

		Z_Free(var->string);
		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = (float)atof(var->string);
		var->integer = atoi(var->string);

		if (!strcmp(var->name, "game"))
		{
			FS_SetGamedir(var->string);
			FS_ExecAutoexec();
		}
	}
}

// Resets cvars that could be used for multiplayer cheating
// Borrowed from Q2E
void Cvar_FixCheatVars(qboolean allowCheats)
{
	if (cvar_allowCheats == allowCheats)
		return;

	cvar_allowCheats = allowCheats;

	if (cvar_allowCheats)
		return;

	for (cvar_t *var = cvar_vars; var; var = var->next)
	{
		if (!(var->flags & CVAR_CHEAT))
			continue;

		if (!Q_stricmp(var->string, var->default_string))
			continue;

		Cvar_Set2(var->name, var->default_string, true);
	}
}

// Handles variable inspection and changing from the console
qboolean Cvar_Command(void)
{
	// Check variables
	cvar_t *v = Cvar_FindVar(Cmd_Argv(0));
	if (!v)
		return false;
		
	// Perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		// Knightmare- show latched value if applicable
		if ((v->flags & CVAR_LATCH) && v->latched_string)
			Com_Printf("\"%s\" is \"%s\" : default is \"%s\", latched to \"%s\"\n", v->name, v->string, v->default_string, v->latched_string);
		else
			Com_Printf("\"%s\" is \"%s\" : default is \"%s\"\n", v->name, v->string, v->default_string);
	}
	else
	{
		Cvar_Set(v->name, Cmd_Argv(1));
	}

	return true;
}

// Allows setting and defining of arbitrary cvars from console
void Cvar_Set_f(void)
{
	const int c = Cmd_Argc();
	if (c != 3 && c != 4)
	{
		Com_Printf("usage: set <variable> <value> [u / s]\n");
		return;
	}

	if (c == 4)
	{
		int flags;
		if (!strcmp(Cmd_Argv(3), "u"))
		{
			flags = CVAR_USERINFO;
		}
		else if (!strcmp(Cmd_Argv(3), "s"))
		{
			flags = CVAR_SERVERINFO;
		}
		else
		{
			Com_Printf("flags can only be 'u' or 's'\n");
			return;
		}

		Cvar_FullSet(Cmd_Argv(1), Cmd_Argv(2), flags);
	}
	else
	{
		Cvar_Set(Cmd_Argv(1), Cmd_Argv(2));
	}
}

// Allows toggling of arbitrary cvars from console
void Cvar_Toggle_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: toggle <variable>\n");
		return;
	}

	cvar_t *var = Cvar_FindVar(Cmd_Argv(1));
	if (!var)
	{
		Com_Printf("'%s' is not a variable\n", Cmd_Argv(1));
		return;
	}

	Cvar_Set2(var->name, va("%i", !var->integer), false);
}

// Allows resetting of arbitrary cvars from console
void Cvar_Reset_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: reset <variable>\n");
		return;
	}

	cvar_t *var = Cvar_FindVar(Cmd_Argv(1));
	if (!var)
	{
		Com_Printf("'%s' is not a variable\n", Cmd_Argv(1));
		return;
	}

	Cvar_Set2(var->name, var->default_string, false);
}

// Appends lines containing "set variable value" for all variables with the archive flag set to true.
void Cvar_WriteVariables(char *path)
{
	char buffer[1024];

	FILE *f = fopen(path, "a");
	for (cvar_t *var = cvar_vars; var; var = var->next)
	{
		if (var->flags & CVAR_ARCHIVE)
		{
			Com_sprintf(buffer, sizeof(buffer), "set %s \"%s\"\n", var->name, var->string);
			fprintf(f, "%s", buffer);
		}
	}

	fclose(f);
}

//mxd
typedef struct
{
	cvar_t *cvar;
} cvarinfo_t;

//mxd
static int Cvar_SortCvarinfos(const cvarinfo_t *first, const cvarinfo_t *second)
{
	return Q_stricmp(first->cvar->name, second->cvar->name);
}

static void Cvar_List_f(void)
{
	char *wildcard = NULL;

	// RIOT's Quake3-sytle cvarlist
	const int argc = Cmd_Argc();

	if (argc != 1 && argc != 2)
	{
		Com_Printf("Usage: cvarlist [wildcard]\n");
		return;
	}

	if (argc == 2)
	{
		wildcard = Cmd_Argv(1);

		//mxd. If no wildcard chars are provided, treat as "arg*"
		if (strchr(wildcard, '*') == NULL && strchr(wildcard, '?') == NULL)
			wildcard = va("%s*", wildcard);
	}

	//mxd. Collect matching cvars first...
	int numtotal = 0;
	for (cvar_t *var = cvar_vars; var; var = var->next)
		numtotal++;

	//mxd. Paranoia check...
	if (numtotal == 0)
	{
		Com_Printf(S_COLOR_GREEN"No cvars...\n");
		return;
	}

	cvarinfo_t *infos = malloc(sizeof(cvarinfo_t) * numtotal);

	int nummatching = 0;
	for (cvar_t *var = cvar_vars; var; var = var->next)
		if (!wildcard || wildcardfit(wildcard, var->name))
			infos[nummatching++].cvar = var;

	//mxd. Sort by name
	qsort(infos, nummatching, sizeof(cvarinfo_t), (int(*)(const void *, const void *))Cvar_SortCvarinfos);

	//mxd. Print results
	for (int i = 0; i < nummatching; i++)
	{
		// Print legend
		if(i == 0)
			Com_Printf(S_COLOR_GREEN"Legend: A: Archive, U: UserInfo, S: ServerInfo, N: NoSet, L: Latch, C: Cheat\n");
		
		cvar_t *var = infos[i].cvar;

		char buffer[1024] = { 0 }; //mxd. Replaced Com_Printf with Q_strncatz (performance gain)

		const qboolean customvalue = Q_stricmp(var->string, var->default_string);
		if(customvalue)
			Q_strncatz(buffer, S_COLOR_CYAN, sizeof(buffer)); // Mark cvars with non-default values in cyan

		if (var->flags & CVAR_ARCHIVE)
			Q_strncatz(buffer, "A", sizeof(buffer));
		else
			Q_strncatz(buffer, "-", sizeof(buffer));

		if (var->flags & CVAR_USERINFO)
			Q_strncatz(buffer, "U", sizeof(buffer));
		else
			Q_strncatz(buffer, "-", sizeof(buffer));

		if (var->flags & CVAR_SERVERINFO)
			Q_strncatz(buffer, "S", sizeof(buffer));
		else
			Q_strncatz(buffer, "-", sizeof(buffer));

		if (var->flags & CVAR_NOSET)
			Q_strncatz(buffer, "N", sizeof(buffer));
		else if (var->flags & CVAR_LATCH)
			Q_strncatz(buffer, "L", sizeof(buffer));
		else
			Q_strncatz(buffer, "-", sizeof(buffer));

		if (var->flags & CVAR_CHEAT)
			Q_strncatz(buffer, "C", sizeof(buffer));
		else
			Q_strncatz(buffer, "-", sizeof(buffer));

		// Show latched value if applicable
		if ((var->flags & CVAR_LATCH) && var->latched_string)
		{
			if (customvalue) //mxd. Print default_string only on mismatch
				Q_strncatz(buffer, va(" %s: \"%s\" (default: \"%s\", latched: \"%s\")\n", var->name, var->string, var->default_string, var->latched_string), sizeof(buffer));
			else
				Q_strncatz(buffer, va(" %s: \"%s\" (latched: \"%s\")\n", var->name, var->string, var->latched_string), sizeof(buffer));
		}
		else if (customvalue) //mxd. Print default_string only on mismatch
		{
			Q_strncatz(buffer, va(" %s: \"%s\" (default: \"%s\")\n", var->name, var->string, var->default_string), sizeof(buffer));
		}
		else
		{
			Q_strncatz(buffer, va(" %s: \"%s\"\n", var->name, var->string), sizeof(buffer));
		}

		Com_Printf("%s", buffer);
	}

	if(argc == 1) //mxd
		Com_Printf(S_COLOR_GREEN"%i cvars\n", numtotal);
	else
		Com_Printf(S_COLOR_GREEN"%i cvars, %i matching\n", numtotal, nummatching);

	//mxd. Free memory
	free(infos);
}

qboolean userinfo_modified;

char *Cvar_BitInfo(int bit)
{
	static char info[MAX_INFO_STRING];
	info[0] = 0;

	for (cvar_t *var = cvar_vars; var; var = var->next)
		if (var->flags & bit)
			Info_SetValueForKey(info, var->name, var->string);

	return info;
}

// Returns an info string containing all the CVAR_USERINFO cvars
char *Cvar_Userinfo(void)
{
	return Cvar_BitInfo(CVAR_USERINFO);
}

// Returns an info string containing all the CVAR_SERVERINFO cvars
char *Cvar_Serverinfo(void)
{
	return Cvar_BitInfo(CVAR_SERVERINFO);
}

// Reads in all archived cvars
void Cvar_Init(void)
{
	Cmd_AddCommand("set", Cvar_Set_f);
	Cmd_AddCommand("toggle", Cvar_Toggle_f);
	Cmd_AddCommand("reset", Cvar_Reset_f);
	Cmd_AddCommand("cvarlist", Cvar_List_f);
}