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
// cmd.c -- Quake script command processing module

#include "qcommon.h"

#define MAX_ALIAS_NAME		32
#define ALIAS_LOOP_COUNT	16

typedef struct cmdalias_s
{
	struct cmdalias_s *next;
	char name[MAX_ALIAS_NAME];
	char *value;
} cmdalias_t;

cmdalias_t *cmd_alias;
qboolean cmd_wait;
int alias_count; // For detecting runaway loops

// Causes execution of the remainder of the command buffer to be delayed until next frame.
// This allows commands like: bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
void Cmd_Wait_f(void)
{
	cmd_wait = true;
}

/*
=============================================================================
	COMMAND BUFFER
=============================================================================
*/

sizebuf_t cmd_text;
byte cmd_text_buf[32768]; // Knightmare increased, was 8192
byte defer_text_buf[32768]; // Knightmare increased, was 8192


void Cbuf_Init(void)
{
	SZ_Init(&cmd_text, cmd_text_buf, sizeof(cmd_text_buf));
}

// Adds command text at the end of the buffer
void Cbuf_AddText(char *text)
{
	const int l = strlen(text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Com_Printf("Cbuf_AddText: overflow\n");
		return;
	}

	SZ_Write(&cmd_text, text, strlen(text));
}

// Adds command text immediately after the current command.
// Adds a \n to the text.
// FIXME: actually change the command buffer to do less copying.
void Cbuf_InsertText(char *text)
{
	char *temp = NULL;

	// Copy off any commands still remaining in the exec buffer
	const int templen = cmd_text.cursize;
	if (templen)
	{
		temp = Z_Malloc(templen);
		memcpy(temp, cmd_text.data, templen);
		SZ_Clear(&cmd_text);
	}
		
	// Add the entire text of the file
	Cbuf_AddText(text);
	
	// Add the copied off data
	if (templen)
	{
		SZ_Write(&cmd_text, temp, templen);
		Z_Free(temp);
	}
}

void Cbuf_CopyToDefer(void)
{
	memcpy(defer_text_buf, cmd_text_buf, cmd_text.cursize);
	defer_text_buf[cmd_text.cursize] = 0;
	cmd_text.cursize = 0;
}

void Cbuf_InsertFromDefer(void)
{
	Cbuf_InsertText(defer_text_buf);
	defer_text_buf[0] = 0;
}

void Cbuf_ExecuteText(int exec_when, char *text)
{
	switch (exec_when)
	{
		case EXEC_NOW:		Cmd_ExecuteString(text); break;
		case EXEC_INSERT:	Cbuf_InsertText(text); break;
		case EXEC_APPEND:	Cbuf_AddText(text); break;
		default: Com_Error(ERR_FATAL, "Cbuf_ExecuteText: bad exec_when");
	}
}

void Cbuf_Execute(void)
{
	int i;
	char line[1024];

	alias_count = 0; // Don't allow infinite alias loops

	while (cmd_text.cursize)
	{
		// Find a \n or ; line break
		char *text = (char *)cmd_text.data;

		int quotes = 0;
		for (i = 0; i < cmd_text.cursize; i++)
		{
			if (text[i] == '"')
				quotes++;
			if (!(quotes & 1) &&  text[i] == ';')
				break;	// Don't break if inside a quoted string
			if (text[i] == '\n')
				break;
		}

		const int size = min(i, (int)sizeof(line) - 1); //mxd. Fix for overflow vulnerability
		memcpy(line, text, size);
		line[size] = 0;
		
		// Delete the text from the command buffer and move remaining commands down.
		// This is necessary because commands (exec, alias) can insert data at the beginning of the text buffer.
		if (i == cmd_text.cursize)
		{
			cmd_text.cursize = 0;
		}
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove(text, text + i, cmd_text.cursize);
		}

		// Execute the command line
		Cmd_ExecuteString(line);
		
		if (cmd_wait)
		{
			// Skip out while text still remains in buffer, leaving it for next frame
			cmd_wait = false;
			break;
		}
	}
}

// Adds command line parameters as script statements.
// Commands lead with a +, and continue until another +.
// "+set" commands are added early, so they are guaranteed to be set before the client and server initialize for the first time.
// Other commands are added late, after all initialization is complete.
void Cbuf_AddEarlyCommands(qboolean clear)
{
	for (int i = 0; i < COM_Argc(); i++)
	{
		char *s = COM_Argv(i);
		if (strcmp(s, "+set"))
			continue;

		Cbuf_AddText(va("set %s %s\n", COM_Argv(i + 1), COM_Argv(i + 2)));
		if (clear)
		{
			COM_ClearArgv(i);
			COM_ClearArgv(i + 1);
			COM_ClearArgv(i + 2);
		}

		i += 2;
	}
}

// Adds command line parameters as script statements.
// Commands lead with a + and continue until another + or -.
// Example: quake2.exe +vid_ref gl +map amlev1

// Returns true if any late commands were added, which will keep the demoloop from immediately starting.
qboolean Cbuf_AddLateCommands(void)
{
	// Build the combined string to parse from
	int argslen = 0;
	const int argc = COM_Argc();

	for (int i = 1; i < argc; i++)
		argslen += strlen(COM_Argv(i)) + 1;

	if (argslen == 0)
		return false;
		
	char *text = Z_Malloc(argslen + 1);
	text[0] = 0;
	for (int i = 1; i < argc; i++)
	{
		Q_strncatz(text, COM_Argv(i), argslen + 1);
		if (i != argc - 1)
			Q_strncatz(text, " ", argslen + 1);
	}
	
	// Pull out the commands
	char *build = Z_Malloc(argslen + 1);
	build[0] = 0;
	
	for (int i = 0; i < argslen - 1; i++)
	{
		if (text[i] == '+')
		{
			i++;

			// Find next command start position
			int nextcmdstart = i;
			while ((text[nextcmdstart] != '+') && (text[nextcmdstart] != '-') && (text[nextcmdstart] != 0))
				nextcmdstart++;

			// Store next command start char
			const char c = text[nextcmdstart];
			text[nextcmdstart] = 0;

			// Copy current command
			Q_strncatz(build, text + i, argslen + 1);
			Q_strncatz(build, "\n", argslen + 1);

			// Restore next command start char
			text[nextcmdstart] = c;

			// Advance to next command
			i = nextcmdstart - 1;
		}
	}

	const qboolean ret = (build[0] != 0);
	if (ret)
		Cbuf_AddText(build);
	
	Z_Free(text);
	Z_Free(build);

	return ret;
}


/*
==============================================================================
	SCRIPT COMMANDS
==============================================================================
*/

void Cmd_Exec_f(void)
{
	char *f;

	if (Cmd_Argc() != 2)
	{
		Com_Printf("exec <filename> : execute a script file\n");
		return;
	}

	const int len = FS_LoadFile(Cmd_Argv(1), (void **)&f);
	if (!f)
	{
		Com_Printf("couldn't exec %s\n", Cmd_Argv(1));
		return;
	}
	Com_Printf("execing %s\n", Cmd_Argv(1));
	
	// The file doesn't have a trailing 0, so we need to copy it off
	char *f2 = Z_Malloc(len + 2); // Echon fix- was len+1
	memcpy (f2, f, len);
	f2[len] = '\n';  // Echon fix added
	f2[len + 1] = '\0'; // Echon fix- was len, = 0

	Cbuf_InsertText(f2);

	Z_Free(f2);
	FS_FreeFile (f);
}

// Just prints the rest of the line to the console
void Cmd_Echo_f(void)
{
	for (int i = 1; i < Cmd_Argc(); i++)
		Com_Printf("%s ", Cmd_Argv(i));
	Com_Printf("\n");
}

// Creates a new command that executes a command string (possibly ; seperated)
void Cmd_Alias_f(void)
{
	cmdalias_t *a;
	char cmd[1024];

	if (Cmd_Argc() == 1)
	{
		Com_Printf("Current alias commands:\n");
		for (a = cmd_alias; a; a = a->next)
			Com_Printf("%s : %s\n", a->name, a->value);

		return;
	}

	char *s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME)
	{
		Com_Printf("Alias name is too long\n");
		return;
	}

	// If the alias already exists, reuse it
	for (a = cmd_alias; a; a = a->next)
	{
		if (!strcmp(s, a->name))
		{
			Z_Free(a->value);
			break;
		}
	}

	if (!a)
	{
		a = Z_Malloc(sizeof(cmdalias_t));
		a->next = cmd_alias;
		cmd_alias = a;
	}

	Q_strncpyz(a->name, s, sizeof(a->name));

	// Copy the rest of the command line
	cmd[0] = 0; // Start out with a null string
	const int c = Cmd_Argc();
	for (int i = 2; i < c; i++)
	{
		Q_strncatz(cmd, Cmd_Argv(i), sizeof(cmd));
		if (i != c - 1)
			Q_strncatz(cmd, " ", sizeof(cmd));
	}

	Q_strncatz(cmd, "\n", sizeof(cmd));
	
	a->value = CopyString(cmd);
}

/*
=============================================================================
	COMMAND EXECUTION
=============================================================================
*/

typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	char					*name;
	xcommand_t				function;
} cmd_function_t;

static int cmd_argc;
static char *cmd_argv[MAX_STRING_TOKENS];
static char *cmd_null_string = "";
static char cmd_args[MAX_STRING_CHARS];

static cmd_function_t *cmd_functions; // Possible commands to execute


int Cmd_Argc(void)
{
	return cmd_argc;
}

char *Cmd_Argv(int arg)
{
	if (arg < 0 || arg >= cmd_argc)
		return cmd_null_string;
	return cmd_argv[arg];
}

// Returns a single string containing argv(1) to argv(argc()-1)
char *Cmd_Args(void)
{
	return cmd_args;
}

char *Cmd_MacroExpandString(char *text)
{
	static char expanded[MAX_STRING_CHARS];
	char temporary[MAX_STRING_CHARS];

	qboolean inquote = false;
	char *scan = text;

	int len = strlen(scan);
	if (len >= MAX_STRING_CHARS)
	{
		Com_Printf("Line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
		return NULL;
	}

	int count = 0;

	for (int i = 0; i < len; i++)
	{
		if (scan[i] == '"')
			inquote ^= 1;

		if (inquote || scan[i] != '$') // Don't expand inside quotes
			continue; 

		// Scan out the complete macro
		char *start = scan + i + 1;
		char *token = COM_Parse(&start);
		if (!start)
			continue;
	
		token = Cvar_VariableString(token);

		const int tokenlen = strlen(token);
		len += tokenlen;
		if (len >= MAX_STRING_CHARS)
		{
			Com_Printf("Expanded line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
			return NULL;
		}

		Q_strncpyz(temporary, scan, i);
		Q_strncpyz(temporary + i, token, sizeof(temporary) - i);
		Q_strncpyz(temporary + i + tokenlen, start, sizeof(temporary) - i - tokenlen);

		Q_strncpyz(expanded, temporary, sizeof(expanded));
		scan = expanded;
		i--;

		if (++count == 100)
		{
			Com_Printf("Macro expansion loop, discarded.\n");
			return NULL;
		}
	}

	if (inquote)
	{
		Com_Printf("Line has unmatched quote, discarded.\n");
		return NULL;
	}

	return scan;
}

// Parses the given string into command line tokens.
// $cvars will be expanded unless they are in a quoted token
void Cmd_TokenizeString(char *text, qboolean macroexpand)
{
	// Clear the args from the last string
	for (int i = 0; i < cmd_argc; i++)
		Z_Free(cmd_argv[i]);
		
	cmd_argc = 0;
	cmd_args[0] = 0;
	
	// Macro expand the text
	if (macroexpand)
	{
#if defined(LOC_SUPPORT) && !defined(DEDICATED_ONLY) // Xile/NiceAss LOC
		extern void CL_LocPlace(void);
		CL_LocPlace();
#endif
		text = Cmd_MacroExpandString(text);
	}

	if (!text)
		return;

	while (true)
	{
		// Skip whitespace up to a /n
		while (*text && *text <= ' ' && *text != '\n')
			text++;
		
		if (*text == '\n')
		{
			// A newline seperates commands in the buffer
			text++;
			break;
		}

		if (!*text)
			return;

		// Set cmd_args to everything after the first arg
		if (cmd_argc == 1)
		{
			// [SkulleR]'s fix for overflow vulnerability
			Q_strncpyz(cmd_args, text, sizeof(cmd_args));
			cmd_args[sizeof(cmd_args) - 1] = 0; 

			// Strip off any trailing whitespace
			int l = strlen(cmd_args) - 1;
			for (; l >= 0; l--)
			{
				if (cmd_args[l] <= ' ')
					cmd_args[l] = 0;
				else
					break;
			}
		}
			
		char *com_token = COM_Parse(&text);
		if (!text)
			return;

		if (cmd_argc < MAX_STRING_TOKENS)
		{
			cmd_argv[cmd_argc] = Z_Malloc(strlen(com_token) + 1);
			Q_strncpyz(cmd_argv[cmd_argc], com_token, strlen(com_token) + 1);
			cmd_argc++;
		}
	}
	
}

void Cmd_AddCommand(char *cmd_name, xcommand_t function)
{
	cmd_function_t *cmd;
	
	// Fail if the command is a variable name
	if (Cvar_VariableString(cmd_name)[0])
	{
		Com_Printf("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}
	
	// Fail if the command already exists
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!strcmp(cmd_name, cmd->name))
		{
			Com_Printf("Cmd_AddCommand: %s already defined\n", cmd_name);
			return;
		}
	}

	cmd = Z_Malloc(sizeof(cmd_function_t));
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

void Cmd_RemoveCommand(char *cmd_name)
{
	cmd_function_t **back = &cmd_functions;
	while (true)
	{
		cmd_function_t *cmd = *back;

		if (!cmd)
		{
			Com_Printf("Cmd_RemoveCommand: %s not added\n", cmd_name);
			return;
		}

		if (!strcmp (cmd_name, cmd->name))
		{
			*back = cmd->next;
			Z_Free(cmd);
			return;
		}

		back = &cmd->next;
	}
}

qboolean Cmd_Exists(char *cmd_name)
{
	for (cmd_function_t *cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strcmp (cmd_name,cmd->name))
			return true;

	return false;
}

// Knightmare - added command auto-complete
char *Cmd_CompleteCommand(char *partial, qboolean *exactmatch)
{
	#define MAX_MATCHES 1024 //mxd
	enum commandtype_t { TYPE_COMMAND, TYPE_ALIAS, TYPE_CVAR };
	static char pmatchtypename[3][8] = { "command", "alias", "cvar" };
	
	*exactmatch = false;
	const int len = strlen(partial);

	if (!len)
		return NULL;

	//mxd. Init collections
	char *pmatch[MAX_MATCHES];
	memset(pmatch, 0, sizeof(pmatch[0]) * MAX_MATCHES);
	byte pmatchtype[MAX_MATCHES];
	memset(pmatchtype, 0, MAX_MATCHES);

	int nummatches = 0;

	//mxd. Collect partial matches, which contain specified string
	for (cmd_function_t *cmd = cmd_functions; cmd && nummatches < MAX_MATCHES; cmd = cmd->next)
	{
		if (Q_strcasestr(cmd->name, partial))
		{
			pmatch[nummatches] = cmd->name;
			pmatchtype[nummatches] = TYPE_COMMAND;
			nummatches++;
		}
	}

	for (cmdalias_t *a = cmd_alias; a && nummatches < MAX_MATCHES; a = a->next)
	{
		if (Q_strcasestr(a->name, partial))
		{
			pmatch[nummatches] = a->name;
			pmatchtype[nummatches] = TYPE_ALIAS;
			nummatches++;
		}
	}

	for (cvar_t *cvar = cvar_vars; cvar && nummatches < MAX_MATCHES; cvar = cvar->next)
	{
		if (Q_strcasestr(cvar->name, partial))
		{
			pmatch[nummatches] = cvar->name;
			pmatchtype[nummatches] = TYPE_CVAR;
			nummatches++;
		}
	}

	// Check results
	if (nummatches > 0)
	{
		Com_Printf("\n%i %s for \"%s\":\n", nummatches, (nummatches > 1 ? "matches" : "match"), partial);

		//mxd. Sort'em... https://stackoverflow.com/questions/3893937/c-array-sorting-tips
		for (int i = 0; i < nummatches - 1; ++i)
		{
			for (int j = 0; j < nummatches - 1 - i; ++j)
			{
				int pos = 0;
				while (pmatch[j][pos] && pmatch[j][pos] == pmatch[j + 1][pos])
					pos++;

				if (pmatch[j][pos] > pmatch[j + 1][pos])
				{
					char *temp = pmatch[j + 1];
					pmatch[j + 1] = pmatch[j];
					pmatch[j] = temp;

					const byte temp2 = pmatchtype[j + 1];
					pmatchtype[j + 1] = pmatchtype[j];
					pmatchtype[j] = temp2;
				}
			}
		}

		// Print'em
		int exactmatchindex = -1;
		for (int i = 0; i < nummatches; i++)
		{
			// Exact match?
			if (!Q_strcasecmp(partial, pmatch[i]))
			{
				char *highlight = (nummatches > 1 ? ">>" : "  "); // Marking a single match looks silly...
				Com_Printf(S_COLOR_GREEN"%s%s (%s)\n", highlight, pmatch[i], pmatchtypename[pmatchtype[i]]);
				exactmatchindex = i;
			}
			else // Partial match. Highlight matching part
			{
				char *matchchar = Q_strcasestr(pmatch[i], partial);

				char before[64], match[64], after[64];
				Q_strncpyz(before, pmatch[i], matchchar - pmatch[i] + 1);
				Q_strncpyz(match, matchchar, len + 1);
				Q_strncpyz(after, matchchar + len, strlen(pmatch[i]) - len + 1);

				Com_Printf(S_COLOR_WHITE"  %s"S_COLOR_YELLOW"%s"S_COLOR_WHITE"%s (%s)\n", before, match, after, pmatchtypename[pmatchtype[i]]);
			}
		}

		// If there's exact match or a single match, return it
		if (exactmatchindex != -1)
		{
			*exactmatch = true;
			return pmatch[exactmatchindex];
		}

		if (nummatches == 1)
		{
			*exactmatch = true;
			return pmatch[0];
		}
	}
	else
	{
		// No matches...
		return NULL;
	}

	//mxd. Proceed only when all matches start with specified string
	for (int i = 0; i < nummatches; ++i)
	{
		int pos = 0;
		while (pmatch[i][pos] && partial[pos] && pmatch[i][pos] == partial[pos])
			pos++;

		if (pos != len)
			return NULL;
	}

	//mxd. Find the common part among all found matches, which start with specified string
	char *retval = malloc(256);
	Q_strncpyz(retval, partial, len + 1);
	int c = len;
	qboolean diff = false;

	while (!diff && c < 256)
	{
		retval[c] = pmatch[0][c];
		for (int i = 0; i < nummatches; i++)
		{
			if (c > (int)strlen(pmatch[i]))
				continue;

			if (retval[c] != pmatch[i][c])
			{
				retval[c] = 0;
				diff = true;
				break;
			}
		}

		c++;
	}

	return retval;
}

// A complete command line has been parsed, so try to execute it
// FIXME: lookupnoadd the token to speed search?
void Cmd_ExecuteString(char *text)
{
	Cmd_TokenizeString(text, true);
			
	// Execute the command line
	if (!Cmd_Argc())
		return; // No tokens

	// Check functions
	for (cmd_function_t *cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!Q_strcasecmp(cmd_argv[0], cmd->name))
		{
			if (!cmd->function)
				Cmd_ExecuteString(va("cmd %s", text)); // Forward to server command
			else
				cmd->function();

			return;
		}
	}

	// Check alias
	for (cmdalias_t *a = cmd_alias; a; a = a->next)
	{
		if (!Q_strcasecmp(cmd_argv[0], a->name))
		{
			if (++alias_count == ALIAS_LOOP_COUNT)
			{
				Com_Printf("ALIAS_LOOP_COUNT\n");
				return;
			}

			Cbuf_InsertText(a->value);
			return;
		}
	}
	
	// Check cvars
	if (Cvar_Command())
		return;

	// Send it as a server command if we are connected
	Cmd_ForwardToServer();
}

void Cmd_List_f(void)
{
	int i = 0;
	for (cmd_function_t *cmd = cmd_functions; cmd; cmd = cmd->next, i++)
		Com_Printf("%s\n", cmd->name);
	Com_Printf("%i commands\n", i);
}

void Cmd_Init(void)
{
	// Register our commands
	Cmd_AddCommand("cmdlist", Cmd_List_f);
	Cmd_AddCommand("exec", Cmd_Exec_f);
	Cmd_AddCommand("echo", Cmd_Echo_f);
	Cmd_AddCommand("alias", Cmd_Alias_f);
	Cmd_AddCommand("wait", Cmd_Wait_f);
}