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
// r_model_script.c -- md3 .script parsing

#include "r_local.h"

static GLenum Mod_ParseBlendMode(char *name)
{
	if (!name)
		return -1;

	if (!Q_strcasecmp(name, "gl_zero"))
		return GL_ZERO;

	if (!Q_strcasecmp(name, "gl_one"))
		return GL_ONE;

	if (!Q_strcasecmp(name, "gl_src_color"))
		return GL_SRC_COLOR;

	if (!Q_strcasecmp(name, "gl_one_minus_src_color"))
		return GL_ONE_MINUS_SRC_COLOR;

	if (!Q_strcasecmp(name, "gl_src_alpha"))
		return GL_SRC_ALPHA;

	if (!Q_strcasecmp(name, "gl_one_minus_src_alpha"))
		return GL_ONE_MINUS_SRC_ALPHA;

	if (!Q_strcasecmp(name, "gl_dst_alpha"))
		return GL_DST_ALPHA;

	if (!Q_strcasecmp(name, "gl_one_minus_dst_alpha"))
		return GL_ONE_MINUS_DST_ALPHA;

	if (!Q_strcasecmp(name, "gl_dst_color"))
		return GL_DST_COLOR;

	if (!Q_strcasecmp(name, "gl_one_minus_dst_color"))
		return GL_ONE_MINUS_DST_COLOR;

	if (!Q_strcasecmp(name, "gl_src_alpha_saturate"))
		return GL_SRC_ALPHA_SATURATE;

	if (!Q_strcasecmp(name, "gl_constant_color"))
		return GL_CONSTANT_COLOR;

	if (!Q_strcasecmp(name, "gl_one_minus_constant_color"))
		return GL_ONE_MINUS_CONSTANT_COLOR;

	if (!Q_strcasecmp(name, "gl_constant_alpha"))
		return GL_CONSTANT_ALPHA;

	if (!Q_strcasecmp(name, "gl_one_minus_constant_alpha"))
		return GL_ONE_MINUS_CONSTANT_ALPHA;

	return -1;
}

static qboolean Mod_GetNextParm(char **data, char **output)
{
	if (!*data)
		return false;

	*output = COM_ParseExt(data, false);

	if (!*data)
	{
		VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: EOF without closing brace\n");
		return false;
	}

	if (!*output[0] || *output[0] == '}')
		return false;

	return true;
}

static qboolean Mod_ParseWaveFunc(char **data, waveForm_t *out)
{
	char *tok = NULL;

	if (!*data || !out || !Mod_GetNextParm(data, &tok))
		return false;

	if (!Q_strcasecmp(tok, "sin"))
		*out = WAVEFORM_SIN;
	else if (!Q_strcasecmp(tok, "triangle"))
		*out = WAVEFORM_TRIANGLE;
	else if (!Q_strcasecmp(tok, "square"))
		*out = WAVEFORM_SQUARE;
	else if (!Q_strcasecmp(tok, "sawtooth"))
		*out = WAVEFORM_SAWTOOTH;
	else if (!Q_strcasecmp(tok, "inversesawtooth"))
		*out = WAVEFORM_INVERSESAWTOOTH;
	else if (!Q_strcasecmp(tok, "noise"))
		*out = WAVEFORM_NOISE;
	else
	{
		*out = WAVEFORM_NONE;
		return false;
	}

	return true;
}

static qboolean Mod_ParseFloat(char **data, float *outnum, qboolean normalized)
{
	char *token = NULL;

	if (!*data || !outnum || !Mod_GetNextParm(data, &token))
		return false;

	if (normalized)
		*outnum = clamp(atof(token), 0.0f, 1.0f);
	else
		*outnum = atof(token);

	return true;
}

void Mod_ParseModelScript(maliasskin_t *skin, char **data, char *dataStart, int dataSize, char *meshname, int skinnum, char *scriptname)
{
	renderparms_t *skinParms = &skin->renderparms;

	// Get the opening curly brace
	char *token = COM_ParseExt(data, true);
	if (!*data)
	{
		VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: unexpected EOF in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
		return;
	}

	if (token[0] != '{')
	{
		VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: found '%s' when expecting '{' in mesh %s, skin %i in %s\n", token, meshname, skinnum, scriptname);
		return;
	}

	// Go through all the parms
	while (*data < dataStart + dataSize)
	{
		token = COM_ParseExt(data, true);
		if (!*data)
		{
			VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: EOF in mesh %s, skin %i in %s without closing brace\n", meshname, skinnum, scriptname);
			break;
		}

		if (token[0] == '}')
			break; // End of skin

		if (!Q_strcasecmp(token, "twosided"))
		{
			skinParms->twosided = true;
		}
		else if (!Q_strcasecmp(token, "alphatest"))
		{
			skinParms->alphatest = true;
			skinParms->noshadow = true; // Also noshadow
		}
		else if (!Q_strcasecmp(token, "fullbright"))
		{
			skinParms->fullbright = true;
		}
		else if (!Q_strcasecmp(token, "nodraw"))
		{
			skinParms->nodraw = true;
			skinParms->noshadow = true; // Also noshadow
		}
		else if (!Q_strcasecmp(token, "noshadow"))
		{
			skinParms->noshadow = true;
		}
		else if (!Q_strcasecmp(token, "nodiffuse"))
		{
			skinParms->nodiffuse = true;
		}
		else if (!Q_strcasecmp(token, "envmap"))
		{
			if (!Mod_ParseFloat(data, &skinParms->envmap, true))
			{
				VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameter for 'envmap' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
				break;
			}
		}
		else if (!Q_strcasecmp(token, "alpha") || !Q_strcasecmp(token, "trans"))
		{
			if (!Mod_ParseFloat(data, &skinParms->basealpha, true))
			{
				VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameter for '%s' in mesh %s, skin %i in %s\n", token, meshname, skinnum, scriptname);
				break;
			}
		}
		else if (!Q_strcasecmp(token, "tcmod"))
		{
			if (!Mod_GetNextParm(data, &token))
			{
				VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameter for 'tcmod' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
				break;
			}

			if (!Q_strcasecmp(token, "translate"))
			{
				if (!Mod_ParseFloat(data, &skinParms->translate_x, false))
				{
					VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameters for 'tcmod translate' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
					break;
				}

				if (!Mod_ParseFloat(data, &skinParms->translate_y, false))
				{
					VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameter for 'tcmod translate' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
					skinParms->translate_x = 0.0f;
					break;
				}
			}
			else if (!Q_strcasecmp(token, "rotate"))
			{
				if (!Mod_ParseFloat(data, &skinParms->rotate, false))
				{
					VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameter for 'tcmod rotate' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
					break;
				}
			}
			else if (!Q_strcasecmp(token, "scale"))
			{
				if (!Mod_ParseFloat(data, &skinParms->scale_x, false))
				{
					VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameters for 'tcmod scale' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
					break;
				}

				if (!Mod_ParseFloat(data, &skinParms->scale_y, false))
				{
					VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameter for 'tcmod scale' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
					skinParms->scale_x = 1.0f;
					break;
				}
			}
			else if (!Q_strcasecmp(token, "stretch"))
			{
				if (!Mod_ParseWaveFunc(data, &skinParms->stretch.type))
				{
					VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing or invalid waveform for 'tcmod stretch' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
					break;
				}

				for (int i = 0; i < 4; i++)
				{
					if (!Mod_ParseFloat(data, &skinParms->stretch.params[i], false))
					{
						VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameters for 'tcmod stretch' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
						break;
					}
				}
			}
			else if (!Q_strcasecmp(token, "turb"))
			{
				// First parm (base) is unused, so just read twice
				for (int i = 0; i < 4; i++)
				{
					if (!Mod_ParseFloat(data, &skinParms->turb.params[i], false))
					{
						VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameters for 'tcmod turb' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
						break;
					}
				}

				skinParms->turb.type = WAVEFORM_SIN;
			}
			else if (!Q_strcasecmp(token, "scroll"))
			{
				if (!Mod_ParseFloat(data, &skinParms->scroll_x, false))
				{
					VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameters for 'tcmod scroll' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
					break;
				}

				if (!Mod_ParseFloat(data, &skinParms->scroll_y, false))
				{
					VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameter for 'tcmod scroll' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
					skinParms->scroll_x = 0.0f;
					break;
				}
			}
			else
			{
				VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: unknown type '%s' for 'tcmod' in mesh %s, skin %i in %s\n", token, meshname, skinnum, scriptname);
				break;
			}
		}
		else if (!Q_strcasecmp(token, "blendfunc"))
		{
			// Parse blend parm
			if (!Mod_GetNextParm(data, &token))
			{
				VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameter(s) for 'blendfunc' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
				break;
			}

			if (!Q_strcasecmp(token, "add"))
			{
				skinParms->blendfunc_src = GL_ONE;
				skinParms->blendfunc_dst = GL_ONE;
			}
			else if (!Q_strcasecmp(token, "filter"))
			{
				skinParms->blendfunc_src = GL_DST_COLOR;
				skinParms->blendfunc_dst = GL_ZERO;
			}
			else if (!Q_strcasecmp(token, "blend"))
			{
				skinParms->blendfunc_src = GL_SRC_ALPHA;
				skinParms->blendfunc_dst = GL_ONE_MINUS_SRC_ALPHA;
			}
			else
			{
				// Parse 2nd blend parm	
				char *token2 = NULL;
				if (!Mod_GetNextParm(data, &token2))
				{
					VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing 2nd parameter for 'blendfunc' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
					break;
				}

				skinParms->blendfunc_src = Mod_ParseBlendMode(token);
				if (skinParms->blendfunc_src == -1)
				{
					VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: invalid blend func src in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
					break;
				}

				skinParms->blendfunc_dst = Mod_ParseBlendMode(token2);
				if (skinParms->blendfunc_dst == -1)
				{
					VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: invalid blend func dst in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
					break;
				}
			}

			skinParms->blend = true;
		}
		else if (!Q_strcasecmp(token, "glow"))
		{
			if (!Mod_GetNextParm(data, &token)) // glowname
			{
				VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing image name for 'glow' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
				break;
			}

			char glowname[MD3_MAX_PATH];
			memcpy(glowname, token, MD3_MAX_PATH);

			if (!Mod_GetNextParm(data, &token)) // Type
			{
				VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing 'identity' or 'wave' for 'glow' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
				break;
			}

			if (!Q_strcasecmp(token, "wave"))
			{
				if (!Mod_ParseWaveFunc(data, &skinParms->glow.type))
				{
					VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing or invalid waveform for 'glow <glowskin> wave' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
					break;
				}

				for (int i = 0; i < 4; i++)
				{
					if (!Mod_ParseFloat(data, &skinParms->glow.params[i], false))
					{
						VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: missing parameters for 'glow <glowskin> wave' in mesh %s, skin %i in %s\n", meshname, skinnum, scriptname);
						break;
					}
				}
			}
			else if (!Q_strcasecmp(token, "identity"))
			{
				skinParms->glow.type = WAVEFORM_NONE;
			}
			else // Only wave or identity
			{
				VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: unknown type '%s' for 'glow' in mesh %s, skin %i in %s\n", token, meshname, skinnum, scriptname);
				break;
			}

			skin->glowimage = R_FindImage(glowname, it_skin, true);
			if (skin->glowimage)
				memcpy(skin->glowname, glowname, MD3_MAX_PATH);
		}
		else // Invalid parameter
		{
			VID_Printf(PRINT_ALL, S_COLOR_YELLOW"Mod_ParseModelScript: unknown parameter '%s' in mesh %s, skin %i in %s\n", token, meshname, skinnum, scriptname);
			break;
		}
	}
}

void Mod_SetRenderParmsDefaults(renderparms_t *parms)
{
	parms->twosided = false;
	parms->alphatest = false;
	parms->fullbright = false;
	parms->nodraw = false;
	parms->noshadow = false;
	parms->nodiffuse = false;
	parms->envmap = 0.0f;
	parms->basealpha = 1.0f;
	parms->blend = false;
	parms->blendfunc_src = -1;
	parms->blendfunc_dst = -1;
	parms->translate_x = 0.0f;
	parms->translate_y = 0.0f;
	parms->rotate = 0.0f;
	parms->scale_x = 1.0f;
	parms->scale_y = 1.0f;
	parms->glow.type = WAVEFORM_NONE;
	parms->stretch.type = WAVEFORM_NONE;
	parms->turb.type = WAVEFORM_NONE;
	parms->scroll_x = 0.0f;
	parms->scroll_y = 0.0f;
}

void Mod_LoadModelScript(model_t *mod, maliasmodel_t *aliasmod)
{
	char scriptname[MAX_QPATH];
	char *buf;

	// Set defaults
	for (int i = 0; i < aliasmod->num_meshes; i++)
	{
		for (int j = 0; j < aliasmod->meshes[i].num_skins; j++)
		{
			Mod_SetRenderParmsDefaults(&aliasmod->meshes[i].skins[j].renderparms);
			aliasmod->meshes[i].skins[j].glowimage = NULL;
		}
	}

	COM_StripExtension(mod->name, scriptname);
	Q_strncatz(scriptname, ".script", sizeof(scriptname));
	const int buf_size = FS_LoadFile(scriptname, (void**)&buf);

	if (buf_size < 1)
		return;

	for (int i = 0; i < aliasmod->num_meshes; i++)
	{
		for (int j = 0; j < aliasmod->meshes[i].num_skins; j++)
		{
			// Search the script file for that meshname
			char *parse_data = buf;  // Copy data postion
			while (parse_data < (buf + buf_size))
			{
				char *token = COM_Parse(&parse_data);
				if (!parse_data || !token)
					break;

				if (!Q_strncasecmp(token, aliasmod->meshes[i].name, strlen(aliasmod->meshes[i].name)) && atoi(COM_FileExtension(token)) == j)
				{
					// Load the parms for that skin
					Mod_ParseModelScript(&aliasmod->meshes[i].skins[j], &parse_data, buf, buf_size, aliasmod->meshes[i].name, j, scriptname);
					break;
				}
			}
		}
	}

	FS_FreeFile(buf);

	// Check if model has any alpha surfaces for sorting
	for (int i = 0; i < aliasmod->num_meshes; i++)
		for (int j = 0; j < aliasmod->meshes[i].num_skins; j++)
			if (aliasmod->meshes[i].skins[j].renderparms.blend || aliasmod->meshes[i].skins[j].renderparms.basealpha < 1.0f) //TODO: mxd. Shouldn't this also be done if renderparms.add !=0 ?
				mod->hasAlpha = true;
}