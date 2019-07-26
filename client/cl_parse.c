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
// cl_parse.c  -- parse a message received from the server

#include "client.h"
#include "snd_ogg.h"

char *svc_strings[256] =
{
	"svc_bad",

	"svc_muzzleflash",
	"svc_muzzlflash2",
	"svc_temp_entity",
	"svc_layout",
	"svc_inventory",

	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_sound",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",
	"svc_centerprint",
	"svc_download",
	"svc_playerinfo",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_frame"
};

void CL_RegisterSounds()
{
	S_BeginRegistration();
	CL_RegisterTEntSounds();

	// Knightmare- 1/2/2002- ULTRA-CHEESY HACK for old demos or connected to server using old protocol
	// Changed config strings require different offsets
	const int cs_sounds = (LegacyProtocol() ? OLD_CS_SOUNDS : CS_SOUNDS); //mxd
	const int max_sounds = (LegacyProtocol() ? OLD_MAX_SOUNDS : MAX_SOUNDS); //mxd

	for (int i = 1; i < max_sounds; i++)
	{
		if (!cl.configstrings[cs_sounds + i][0])
			break;

		cl.sound_precache[i] = S_RegisterSound(cl.configstrings[cs_sounds + i]);
		IN_Update(); // Pump message loop
	}

	//end Knightmare
	S_EndRegistration();

	// Knightmare added
	// CL_RegisterSounds is only called while the refresh is prepped
	// during a sound resart, so we can use this to restart the music track
	if (cls.state == ca_active && cl.refresh_prepped)
		CL_PlayBackgroundTrack();
}

#pragma region ======================= SERVER CONNECTING MESSAGES

// A utility function that determines if parsing of old protocol should be used.
qboolean LegacyProtocol()
{
	return ((Com_ServerState() && cls.serverProtocol <= OLD_PROTOCOL_VERSION) || cls.serverProtocol == OLD_PROTOCOL_VERSION);
}

static void CL_ParseServerData(void)
{
	Com_DPrintf("Serverdata packet received.\n");

	// Wipe the client_state_t struct
	CL_ClearState();
	cls.state = ca_connected;

	// Parse protocol version number
	const int protocol = MSG_ReadLong(&net_message);
	cls.serverProtocol = protocol;

	// BIG HACK to let demos from release work with the 3.0x patch!!!
	// Knightmare- also allow connectivity with servers using the old protocol
	if (!LegacyProtocol() && protocol != PROTOCOL_VERSION && protocol != OLD_PROTOCOL_VERSION)
		Com_Error(ERR_DROP, "Server returned version %i, not %i", protocol, PROTOCOL_VERSION);

	cl.servercount = MSG_ReadLong(&net_message);
	cl.attractloop = MSG_ReadByte(&net_message);

	// Game directory
	char *str = MSG_ReadString(&net_message);
	strncpy(cl.gamedir, str, sizeof(cl.gamedir) - 1);

	// Set gamedir
	if (((*str && (!fs_gamedirvar->string || !*fs_gamedirvar->string || strcmp(fs_gamedirvar->string, str)))
		|| (!*str && fs_gamedirvar->string && *fs_gamedirvar->string)) //mxd. Otherwise we might dereference a NULL fs_gamedirvar->string
		&& !cl.attractloop) // Knightmare- don't allow demos to change this
		Cvar_Set("game", str);

	// Parse player entity number
	cl.playernum = MSG_ReadShort(&net_message);

	// Get the full level name
	str = MSG_ReadString(&net_message);

	if (cl.playernum == -1)
	{
		// Playing a cinematic or showing a pic, not a level
		SCR_PlayCinematic(str);
	}
	else
	{
		// Seperate the printfs so the server message can have a color
		Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
		con.ormask = 128;
		Com_Printf("%c"S_COLOR_SHADOW S_COLOR_ALT"%s\n", 2, str); //mxd. First char marks the text as colored (see Con_Print())
		con.ormask = 0;

		// Need to prep refresh at next oportunity
		cl.refresh_prepped = false;
	}
}

static void CL_ParseBaseline(void)
{
	entity_state_t nullstate;
	memset(&nullstate, 0, sizeof(nullstate));

	int bits;
	const int newnum = CL_ParseEntityBits(&bits);
	entity_state_t *es = &cl_entities[newnum].baseline;

	CL_ParseDelta(&nullstate, es, newnum, bits);
}

void CL_LoadClientinfo(clientinfo_t *ci, char *s)
{
	char model_name[MAX_QPATH];
	char skin_name[MAX_QPATH];
	char model_filename[MAX_QPATH];
	char skin_filename[MAX_QPATH];
	char weapon_filename[MAX_QPATH];

	strncpy(ci->cinfo, s, sizeof(ci->cinfo));
	ci->cinfo[sizeof(ci->cinfo) - 1] = 0;

	// Isolate the player's name
	strncpy(ci->name, s, sizeof(ci->name));
	ci->name[sizeof(ci->name) - 1] = 0;
	char *t = strchr(s, '\\'); //mxd. strstr -> strchr
	if (t)
	{
		ci->name[t - s] = 0;
		s = t + 1;
	}

	if (cl_noskins->value || *s == 0)
	{
		Com_sprintf(model_filename, sizeof(model_filename), "players/male/tris.md2");
		Com_sprintf(weapon_filename, sizeof(weapon_filename), "players/male/weapon.md2");
		Com_sprintf(skin_filename, sizeof(skin_filename), "players/male/grunt.pcx");
		Com_sprintf(ci->iconname, sizeof(ci->iconname), "/players/male/grunt_i.pcx");

		ci->model = R_RegisterModel(model_filename);
		memset(ci->weaponmodel, 0, sizeof(ci->weaponmodel));

		ci->weaponmodel[0] = R_RegisterModel(weapon_filename);
		ci->skin = R_RegisterSkin(skin_filename);
		ci->icon = R_DrawFindPic(ci->iconname);
	}
	else
	{
		// Isolate the model name
		Q_strncpyz(model_name, s, sizeof(model_name));
		t = strchr(model_name, '/'); //mxd. strstr -> strchr
		if (!t)
			t = strchr(model_name, '\\'); //mxd. strstr -> strchr
		if (!t)
			t = model_name;
		*t = 0;

		// Isolate the skin name
		Q_strncpyz(skin_name, s + strlen(model_name) + 1, sizeof(skin_name));

		// Model file
		Com_sprintf(model_filename, sizeof(model_filename), "players/%s/tris.md2", model_name);
		ci->model = R_RegisterModel(model_filename);
		if (!ci->model)
		{
			Q_strncpyz(model_name, "male", sizeof(model_name));
			Com_sprintf(model_filename, sizeof(model_filename), "players/male/tris.md2");
			ci->model = R_RegisterModel(model_filename);
		}

		// Skin file
		Com_sprintf(skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
		ci->skin = R_RegisterSkin(skin_filename);

		// If we don't have the skin and the model wasn't male, see if the male has it (this is for CTF's skins)
 		if (!ci->skin && Q_stricmp(model_name, "male"))
		{
			// Change model to male
			Q_strncpyz(model_name, "male", sizeof(model_name));
			Com_sprintf(model_filename, sizeof(model_filename), "players/male/tris.md2");
			ci->model = R_RegisterModel(model_filename);

			// See if the skin exists for the male model
			Com_sprintf(skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
			ci->skin = R_RegisterSkin(skin_filename);
		}

		// If we still don't have a skin, it means that the male model didn't have it, so default to grunt
		if (!ci->skin)
		{
			// See if the skin exists for the male model
			Com_sprintf(skin_filename, sizeof(skin_filename), "players/%s/grunt.pcx", model_name, skin_name);
			ci->skin = R_RegisterSkin(skin_filename);
		}

		// Weapon file
		for (int i = 0; i < num_cl_weaponmodels; i++)
		{
			Com_sprintf(weapon_filename, sizeof(weapon_filename), "players/%s/%s", model_name, cl_weaponmodels[i]);
			ci->weaponmodel[i] = R_RegisterModel(weapon_filename);

			if (!ci->weaponmodel[i] && strcmp(model_name, "cyborg") == 0)
			{
				// Try male
				Com_sprintf(weapon_filename, sizeof(weapon_filename), "players/male/%s", cl_weaponmodels[i]);
				ci->weaponmodel[i] = R_RegisterModel(weapon_filename);
			}

			if (!cl_vwep->value)
				break; // Only one when vwep is off
		}

		// Icon file
		Com_sprintf(ci->iconname, sizeof(ci->iconname), "/players/%s/%s_i.pcx", model_name, skin_name);
		ci->icon = R_DrawFindPic(ci->iconname);
	}

	// Must have loaded all data types to be valud
	if (!ci->skin || !ci->icon || !ci->model || !ci->weaponmodel[0])
	{
		ci->skin = NULL;
		ci->icon = NULL;
		ci->model = NULL;
		ci->weaponmodel[0] = NULL;
	}
}

// Load the skin, icon, and model for a client
void CL_ParseClientinfo(int player)
{
	// Knightmare- 1/2/2002- GROSS HACK for old demos or connected to server using old protocol
	// Changed config strings require different offsets
	const int cs_playerskins = (LegacyProtocol() ? OLD_CS_PLAYERSKINS : CS_PLAYERSKINS); //mxd
	char *s = cl.configstrings[player + cs_playerskins];

	clientinfo_t *ci = &cl.clientinfo[player];
	CL_LoadClientinfo(ci, s);
}

// Returns correct OGG track number for mission packs.
// This assumes that the standard Q2 CD was ripped as track02-track11, and the Rogue CD as track12-track21.
static int CL_MissionPackCDTrack(int tracknum)
{
	// 1 is illegal (=> data track on CD), 0 means "no track"
	if (tracknum < 2)
		return 0;
	
	if (FS_ModType("rogue") || cl_rogue_music->integer)
		return (tracknum < 12 ? tracknum + 10 : tracknum);

	// An out-of-order mix from Q2 and Rogue CDs
	if (FS_ModType("xatrix") || cl_xatrix_music->integer)
	{
		switch (tracknum)
		{
			case 2: return 9;
			case 3: return 13;
			case 4: return 14;
			case 5: return 7;
			case 6: return 16;
			case 7: return 2;
			case 8: return 15;
			case 9: return 3;
			case 10: return 4;
			case 11: return 18;
			default: return tracknum;
		}
	}

	return tracknum;
}

void CL_PlayBackgroundTrack()
{
	char name[MAX_QPATH];

	if (!cl.refresh_prepped)
		return;

	// Using a named audio track intead of numbered
	if (strlen(cl.configstrings[CS_CDTRACK]) > 2)
	{
		Com_sprintf(name, sizeof(name), "music/%s.ogg", cl.configstrings[CS_CDTRACK]);
		if (FS_FileExists(name) || Sys_Access(name, ACC_EXISTS)) //mxd. FS_LoadFile -> FS_FileExists; file existance check for GOG tracks...
		{
			S_StartBackgroundTrack(name, name, musictrackframe->integer); //mxd. +musictrackframe
			return;
		}
	}

	const int track = atoi(cl.configstrings[CS_CDTRACK]);

	if (track == 0)
	{
		S_StopBackgroundTrack();
		return;
	}

	// If an OGG file exists play it
	Com_sprintf(name, sizeof(name), "music/track%02i.ogg", CL_MissionPackCDTrack(track));
	if (FS_FileExists(name) || Sys_Access(name, ACC_EXISTS)) //mxd. FS_LoadFile -> FS_FileExists; file existance check for GOG tracks...
		S_StartBackgroundTrack(name, name, musictrackframe->integer); //mxd. +musictrackframe
}

void CL_ParseConfigString()
{
	int max_models, max_sounds, max_images, cs_lights, cs_sounds, cs_images, cs_playerskins;
	char olds[MAX_QPATH];

	// Knightmare- hack for connected to server using old protocol
	// Changed config strings require different parsing
	if (LegacyProtocol())
	{
		max_models = OLD_MAX_MODELS;
		max_sounds = OLD_MAX_SOUNDS;
		max_images = OLD_MAX_IMAGES;
		cs_lights = OLD_CS_LIGHTS;
		cs_sounds = OLD_CS_SOUNDS;
		cs_images = OLD_CS_IMAGES;
		cs_playerskins = OLD_CS_PLAYERSKINS;
	}
	else
	{
		max_models = MAX_MODELS;
		max_sounds = MAX_SOUNDS;
		max_images = MAX_IMAGES;
		cs_lights = CS_LIGHTS;
		cs_sounds = CS_SOUNDS;
		cs_images = CS_IMAGES;
		cs_playerskins = CS_PLAYERSKINS;
	}

	int i = MSG_ReadShort(&net_message);
	if (i < 0 || i >= MAX_CONFIGSTRINGS)
		Com_Error(ERR_DROP, "configstring > MAX_CONFIGSTRINGS");

	char *s = MSG_ReadString(&net_message);

	Q_strncpyz(olds, cl.configstrings[i], sizeof(olds));

	// Check length
	const uint length = strlen(s);
	if (length >= (sizeof(cl.configstrings[0]) * (MAX_CONFIGSTRINGS - i)) - 1)
		Com_Error(ERR_DROP, "%s: string %i exceeds available buffer space!", __func__, i);

	// Don't use a null-terminated strncpy here!!
	if (i >= CS_STATUSBAR && i < CS_AIRACCEL) // Allow writes to statusbar strings to overflow
	{	
		strncpy(cl.configstrings[i], s, (sizeof(cl.configstrings[i]) * (CS_AIRACCEL - i)) - 1);
		cl.configstrings[CS_AIRACCEL - 1][MAX_QPATH - 1] = 0; // null-terminate end of section
	}
	else if (LegacyProtocol() && (i >= OLD_CS_GENERAL && i < OLD_MAX_CONFIGSTRINGS)) // Allow writes to general strings to overflow
	{	
		strncpy(cl.configstrings[i], s, (sizeof(cl.configstrings[i]) * (OLD_MAX_CONFIGSTRINGS - i)) - 1);
		cl.configstrings[OLD_MAX_CONFIGSTRINGS - 1][MAX_QPATH - 1] = 0; // null-terminate end of section
	}
	else if (!LegacyProtocol() && (i >= CS_GENERAL && i < CS_PAKFILE)) // Allow writes to general strings to overflow
	{	
		strncpy(cl.configstrings[i], s, (sizeof(cl.configstrings[i]) * (CS_PAKFILE - i)) - 1);
		cl.configstrings[CS_PAKFILE - 1][MAX_QPATH - 1] = 0; // null-terminate end of section
	}
	else
	{
		if (length >= MAX_QPATH)
			Com_Printf(S_COLOR_YELLOW"%s: configstring %i is too long (%i / %i chars).\n", __func__, i, length, MAX_QPATH - 1);

		Q_strncpyz(cl.configstrings[i], s, sizeof(cl.configstrings[i]));
	}

	// Do something apropriate 
	if (i >= cs_lights && i < cs_lights + MAX_LIGHTSTYLES)
	{
		CL_SetLightstyle(i - cs_lights);
	}
	else if (i == CS_CDTRACK)
	{
		if (cl.refresh_prepped)
			CL_PlayBackgroundTrack();
	}
	else if (i == CS_MAXCLIENTS) // From R1Q2
	{
		if (!cl.attractloop)
			cl.maxclients = atoi(cl.configstrings[CS_MAXCLIENTS]);
	}
	else if (i >= CS_MODELS && i < CS_MODELS + max_models)
	{
		if (cl.refresh_prepped)
		{
			cl.model_draw[i - CS_MODELS] = R_RegisterModel(cl.configstrings[i]);
			if (cl.configstrings[i][0] == '*')
				cl.model_clip[i - CS_MODELS] = CM_InlineModel(cl.configstrings[i]);
			else
				cl.model_clip[i - CS_MODELS] = NULL;
		}
	}
	else if (i >= cs_sounds && i < cs_sounds + max_sounds) //Knightmare- was MAX_MODELS
	{
		if (cl.refresh_prepped)
			cl.sound_precache[i - cs_sounds] = S_RegisterSound(cl.configstrings[i]);
	}
	else if (i >= cs_images && i < cs_images + max_images) //Knightmare- was MAX_IMAGES
	{
		if (cl.refresh_prepped)
			cl.image_precache[i - cs_images] = R_DrawFindPic(cl.configstrings[i]);
	}
	else if (i >= cs_playerskins && i < cs_playerskins + MAX_CLIENTS)
	{
		// From R1Q2- a hack to avoid parsing non-skins from mods that overload CS_PLAYERSKINS
		if (i - cs_playerskins < cl.maxclients)
		{
			if (cl.refresh_prepped && strcmp(olds, s))
				CL_ParseClientinfo(i - cs_playerskins);
		}
		else
		{
			Com_DPrintf("CL_ParseConfigString: Ignoring out-of-range playerskin %d (%s)\n", i, MakePrintable(s, 0));
		}
	}
}

#pragma endregion

#pragma region ======================= ACTION MESSAGES

static void CL_ParseStartSoundPacket(void)
{
	vec3_t	pos_v;
	float	*pos;
	int		channel, ent;
	int		sound_num;
	float	volume;
	float	attenuation;
	float	ofs;

	const int flags = MSG_ReadByte(&net_message);

	// Knightmare- 12/23/2001
	// Read sound indices as bytes only if playing old demos or connected to server using old protocol; otherwise, read as shorts
	if (LegacyProtocol())
		sound_num = MSG_ReadByte(&net_message);
	else
		sound_num = MSG_ReadShort(&net_message);
	//end Knightmare

	if (flags & SND_VOLUME)
		volume = MSG_ReadByte(&net_message) / 255.0f;
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
	if (flags & SND_ATTENUATION)
		attenuation = MSG_ReadByte(&net_message) / 64.0f;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	if (flags & SND_OFFSET)
		ofs = MSG_ReadByte(&net_message) / 1000.0f;
	else
		ofs = 0;

	if (flags & SND_ENT)
	{
		// Entity-relative
		channel = MSG_ReadShort(&net_message); 
		ent = channel >> 3;
		if (ent > MAX_EDICTS)
			Com_Error(ERR_DROP, "CL_ParseStartSoundPacket: ent = %i", ent);

		channel &= 7;
	}
	else
	{
		ent = 0;
		channel = 0;
	}

	if (flags & SND_POS)
	{
		// Positioned in space
		MSG_ReadPos(&net_message, pos_v);
		pos = pos_v;
	}
	else
	{
		// Use entity number
		pos = NULL;
	}

	if (!cl.sound_precache[sound_num])
		return;

	S_StartSound(pos, ent, channel, cl.sound_precache[sound_num], volume, attenuation, ofs);
}

void SHOWNET(char *s)
{
	if (cl_shownet->value >= 2)
		Com_Printf("%3i:%s\n", net_message.readcount - 1, s);
}

// Catches malicious stuffed commands from the server.
// Simply disconnects when the stuffed command is quit or error, same effect as kicking the player.
// Uses list of malicious commands from xian.
static qboolean CL_FilterStuffText(char *stufftext)
{
	static char *bad_stuffcmds[] =
	{
		"sensitivity",
		"unbindall",
		"unbind",
		"bind",
		"exec",
		"kill",
		"rate",
		"cl_maxfps",
		"r_maxfps",
		"net_maxfps",
		"quit",
		"error",
		0
	};

	// Skip leading spaces
	char *parsetext = stufftext;
	while (*parsetext == ' ')
		parsetext++;

	// Handle quit and error stuffs specially
	if (!strncmp(parsetext, "quit", 4) || !strncmp(parsetext, "error", 5))
	{
		Com_Printf(S_COLOR_YELLOW"%s: server stuffed 'quit' or 'error' command, disconnecting...\n", __func__);
		CL_Disconnect();

		return false;
	}

	// Don't allow stuffing of renderer cvars
	if (!strncmp(parsetext, "gl_", 3) || !strncmp(parsetext, "r_", 2))
		return false;

	// The Generations mod stuffs exec g*.cfg  for classes, so limit exec stuffs to .cfg files
	if (!strncmp(parsetext, "exec", 4))
	{
		char *s = parsetext + 4;
		char *execname = COM_Parse(&s);
		if (!execname)
			return false; // Catch case of no text after 'exec'

		uint len = strlen(execname);

		if (len > 1 && execname[len - 1] == ';') // Catch token ending with ;
			len--;

		if (len < 5 || strncmp(execname + len - 4, ".cfg", 4))
		{
			Com_Printf(S_COLOR_YELLOW"%s: server stuffed 'exec' command for non-cfg file\n", __func__);
			return false;
		}

		return true;
	}

	// Code by xian- cycle through list of malicious commands
	for (int i = 0; bad_stuffcmds[i]; i++)
		if (strstr(parsetext, bad_stuffcmds[i]))
			return false;

	return true;
}

// Knightmare- server-controlled fog
// Fog is sent like this:
// gi.WriteByte (svc_fog); // svc_fog = 21
// gi.WriteByte (fog_enable); // 1 = on, 0 = off
// gi.WriteByte (fog_model); // 0, 1, or 2
// gi.WriteByte (fog_density); // 1-100
// gi.WriteShort (fog_near); // >0, <fog_far
// gi.WriteShort (fog_far); // >fog_near-64, <10000
// gi.WriteByte (fog_red); // 0-255
// gi.WriteByte (fog_green); // 0-255
// gi.WriteByte (fog_blue); // 0-255
// gi.unicast (player_ent, true); 

static void CL_ParseFog(void)
{
	const qboolean fogenable = (MSG_ReadByte(&net_message) > 0);
	const int model = MSG_ReadByte(&net_message);
	const int density = MSG_ReadByte(&net_message);
	const int start = MSG_ReadShort(&net_message);
	const int end = MSG_ReadShort(&net_message);
	const int red = MSG_ReadByte(&net_message);
	const int green = MSG_ReadByte(&net_message);
	const int blue = MSG_ReadByte(&net_message);

	R_SetFogVars(fogenable, model, density, start, end, red, green, blue);
}

void CL_ParseServerMessage(void)
{
	// If recording demos, copy the message out
	if (cl_shownet->value == 1)
		Com_Printf("%i ", net_message.cursize);
	else if (cl_shownet->value >= 2)
		Com_Printf("------------------\n");

	// Parse the message
	while (true)
	{
		if (net_message.readcount > net_message.cursize)
		{
			Com_Error(ERR_DROP, "CL_ParseServerMessage: Bad server message");
			break;
		}

		const int cmd = MSG_ReadByte(&net_message);

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			break;
		}

		if (cl_shownet->value >= 2)
		{
			if (!svc_strings[cmd])
				Com_Printf("%3i:BAD CMD %i\n", net_message.readcount - 1, cmd);
			else
				SHOWNET(svc_strings[cmd]);
		}
	
		// Other commands
		switch (cmd)
		{
			default:
				Com_Error(ERR_DROP, "CL_ParseServerMessage: Illegible server message\n");
				break;
			
			case svc_nop:
				break;
			
			case svc_disconnect:
				Com_Error(ERR_DISCONNECT, "Server disconnected\n");
				break;

			case svc_reconnect:
				Com_Printf("Server disconnected, reconnecting\n");
				if (cls.download)
				{
					//ZOID, close download
					fclose(cls.download);
					cls.download = NULL;
				}

				cls.state = ca_connecting;
				cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
				break;

			case svc_print:
			{
				const int i = MSG_ReadByte(&net_message);
				if (i == PRINT_CHAT)
				{
					S_StartLocalSound("misc/talk.wav");
					//con.ormask = 128; // Knightmare- made redundant by color code
					Com_Printf(S_COLOR_ALT"%s", MSG_ReadString(&net_message)); // Knightmare- add green flag
				}
				else
				{
					Com_Printf("%s", MSG_ReadString(&net_message));
				}
				con.ormask = 0;
			} break;
			
			case svc_centerprint:
				SCR_CenterPrint(MSG_ReadString(&net_message));
				break;
			
			case svc_stufftext:
			{
				char *s = MSG_ReadString(&net_message);
				
				// Knightmare- filter malicious stufftext
				if (!CL_FilterStuffText(s))
				{
					Com_Printf(S_COLOR_YELLOW"%s: malicious stufftext from server: '%s'\n", __func__, s);
				}
				else
				{
					Com_DPrintf("stufftext: %s\n", s);
					Cbuf_AddText(s);
				}
			} break;
			
			case svc_serverdata:
				Cbuf_Execute(); // make sure any stuffed commands are done
				CL_ParseServerData();
				break;
			
			case svc_configstring:
				CL_ParseConfigString();
				break;
			
			case svc_sound:
				CL_ParseStartSoundPacket();
				break;
			
			case svc_spawnbaseline:
				CL_ParseBaseline();
				break;

			case svc_temp_entity:
				CL_ParseTEnt();
				break;

			case svc_muzzleflash:
				CL_ParseMuzzleFlash();
				break;

			case svc_muzzleflash2:
				CL_ParseMuzzleFlash2();
				break;

			case svc_download:
				CL_ParseDownload();
				break;

			case svc_frame:
				CL_ParseFrame();
				break;

			case svc_inventory:
				CL_ParseInventory();
				break;

			case svc_fog: // Knightmare added
				CL_ParseFog();
				break;

			case svc_layout:
			{
				char *s = MSG_ReadString(&net_message);
				strncpy(cl.layout, s, sizeof(cl.layout) - 1);
			} break;

			case svc_playerinfo:
			case svc_packetentities:
			case svc_deltapacketentities:
				Com_Error(ERR_DROP, "Out of place frame data");
				break;
		}
	}

	CL_AddNetgraph();

	// We don't know if it is ok to save a demo message until after we have parsed the frame
	if (cls.demorecording && !cls.demowaiting)
		CL_WriteDemoMessage();
}

#pragma endregion 