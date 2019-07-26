/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2001-2003 pat@aftermoon.net for modif flanked by <serverping>

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

// cl_main.c  -- client main loop

#include "client.h"

//mxd. Some particle-related defines formerly stored in client.h...
#define DEFAULT_RAIL_LENGTH		2048
#define MIN_RAIL_LENGTH			1024
#define DEFAULT_RAIL_SPACE		1
#define MIN_DECAL_LIFE			5

#pragma region ======================= cvars

cvar_t *freelook;

cvar_t *adr0;
cvar_t *adr1;
cvar_t *adr2;
cvar_t *adr3;
cvar_t *adr4;
cvar_t *adr5;
cvar_t *adr6;
cvar_t *adr7;
cvar_t *adr8;
cvar_t *adr9;
cvar_t *adr10;
cvar_t *adr11;

cvar_t *cl_stereo_separation;
cvar_t *cl_stereo;

cvar_t *rcon_client_password;
cvar_t *rcon_address;

cvar_t *cl_noskins;
cvar_t *cl_footsteps;
cvar_t *cl_timeout;
cvar_t *cl_predict;
cvar_t *cl_maxfps;

#ifdef CLIENT_SPLIT_NETFRAME
cvar_t *cl_async;
cvar_t *net_maxfps;
cvar_t *r_maxfps;
#endif

cvar_t *cl_sleep;

// Whether to trick version 34 servers that this is a version 34 client
cvar_t *cl_servertrick;

cvar_t *cl_gun;
cvar_t *cl_weapon_shells;

// Reduction factor for particle effects
cvar_t *cl_particle_scale;

// Whether to adjust fov for wide aspect rattio
cvar_t *cl_widescreen_fov;

// Psychospaz's chasecam
cvar_t *cg_thirdperson;
cvar_t *cg_thirdperson_angle;
cvar_t *cg_thirdperson_chase;
cvar_t *cg_thirdperson_dist;
cvar_t *cg_thirdperson_alpha;
cvar_t *cg_thirdperson_adjust;

cvar_t *cl_blood;
cvar_t *cl_old_explosions; // Option for old explosions
cvar_t *cl_plasma_explo_sound; // Option for unique plasma explosion sound
cvar_t *cl_item_bobbing; // Option for bobbing items

// Psychospaz's rail code
cvar_t *cl_railred;
cvar_t *cl_railgreen;
cvar_t *cl_railblue;
cvar_t *cl_railtype;
cvar_t *cl_rail_length;
cvar_t *cl_rail_space;

// Whether to use texsurfs.txt footstep sounds
cvar_t *cl_footstep_override;

cvar_t *r_decals; // Decal quantity
cvar_t *r_decal_life; // Decal duration in seconds

cvar_t *con_font_size;
cvar_t *alt_text_color;

cvar_t *cl_rogue_music; // Whether to play Rogue tracks
cvar_t *cl_xatrix_music; // Whether to play Xatrix tracks


cvar_t *cl_add_particles;
cvar_t *cl_add_lights;
cvar_t *cl_add_entities;
cvar_t *cl_add_blend;

cvar_t *cl_shownet;
cvar_t *cl_showmiss;
cvar_t *cl_showclamp;

cvar_t *cl_paused;
cvar_t *cl_timedemo;


cvar_t *lookspring;
cvar_t *lookstrafe;
cvar_t *sensitivity;
cvar_t *menu_sensitivity;
cvar_t *menu_rotate;
cvar_t *menu_alpha;

cvar_t *m_pitch;
cvar_t *m_yaw;
cvar_t *m_forward;
cvar_t *m_side;

cvar_t *cl_lightlevel;

// Userinfo
cvar_t *info_password;
cvar_t *info_spectator;
cvar_t *info_name;
cvar_t *info_skin;
cvar_t *info_rate;
cvar_t *info_fov;
cvar_t *info_msg;
cvar_t *info_hand;
cvar_t *info_gender;
cvar_t *info_gender_auto;

cvar_t *cl_vwep;

// For the server to tell which version the client is
cvar_t *cl_engine;
cvar_t *cl_engine_version;

#ifdef LOC_SUPPORT	// Xile/NiceAss LOC
cvar_t *cl_drawlocs;
cvar_t *loc_here;
cvar_t *loc_there;
#endif

#pragma endregion

client_static_t	cls;
client_state_t cl;

centity_t cl_entities[MAX_EDICTS];
entity_state_t cl_parse_entities[MAX_PARSE_ENTITIES];

#pragma region ======================= Demo recording

// Dumps the current net message, prefixed by the length
void CL_WriteDemoMessage()
{
	// The first eight bytes are just packet sequencing stuff
	const int len = net_message.cursize - 8;
	fwrite(&len, 4, 1, cls.demofile);
	fwrite(net_message.data + 8, len, 1, cls.demofile);
}

// Stop recording a demo
void CL_Stop_f()
{
	if (!cls.demorecording)
	{
		Com_Printf("Not recording a demo.\n");
		return;
	}

	// Finish up
	int len = -1;
	fwrite(&len, 4, 1, cls.demofile);
	fclose(cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Com_Printf("Stopped demo.\n");
}

// record <demoname>
// Begins recording a demo from the current position
void CL_Record_f()
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("record <demoname>\n");
		return;
	}

	if (cls.demorecording)
	{
		Com_Printf("Already recording.\n");
		return;
	}

	if (cls.state != ca_active)
	{
		Com_Printf("You must be in a level to record.\n");
		return;
	}

	// Open the demo file
	char name[MAX_OSPATH];
	Com_sprintf(name, sizeof(name), "%s/demos/%s.dm2", FS_Gamedir(), Cmd_Argv(1));

	Com_Printf("recording to %s.\n", name);
	FS_CreatePath(name);
	cls.demofile = fopen(name, "wb");

	if (!cls.demofile)
	{
		Com_Printf("ERROR: couldn't open.\n");
		return;
	}

	cls.demorecording = true;

	// Don't start saving messages until a non-delta compressed message is received
	cls.demowaiting = true;

	// Write out messages to hold the startup information
	sizebuf_t buf;
	char buf_data[MAX_MSGLEN];
	SZ_Init(&buf, buf_data, sizeof(buf_data));

	// Send the serverdata
	MSG_WriteByte(&buf, svc_serverdata);
	MSG_WriteLong(&buf, PROTOCOL_VERSION);
	MSG_WriteLong(&buf, 0x10000 + cl.servercount);
	MSG_WriteByte(&buf, 1);	// Demos are always attract loops
	MSG_WriteString(&buf, cl.gamedir);
	MSG_WriteShort(&buf, cl.playernum);

	MSG_WriteString(&buf, cl.configstrings[CS_NAME]);

	// Configstrings
	for (int i = 0; i < MAX_CONFIGSTRINGS; i++)
	{
		if (cl.configstrings[i][0])
		{
			if (buf.cursize + (int)strlen(cl.configstrings[i]) + 32 > buf.maxsize)
			{
				// Write it out
				fwrite(&buf.cursize, 4, 1, cls.demofile);
				fwrite(buf.data, buf.cursize, 1, cls.demofile);
				buf.cursize = 0;
			}

			MSG_WriteByte(&buf, svc_configstring);
			MSG_WriteShort(&buf, i);
			MSG_WriteString(&buf, cl.configstrings[i]);
		}
	}

	// Baselines
	entity_state_t nullstate;
	memset(&nullstate, 0, sizeof(nullstate));
	for (int i = 0; i < MAX_EDICTS; i++)
	{
		entity_state_t *ent = &cl_entities[i].baseline;
		if (!ent->modelindex)
			continue;

		if (buf.cursize + 64 > buf.maxsize)
		{
			// Write it out
			fwrite(&buf.cursize, 4, 1, cls.demofile);
			fwrite(buf.data, buf.cursize, 1, cls.demofile);
			buf.cursize = 0;
		}

		MSG_WriteByte(&buf, svc_spawnbaseline);
		MSG_WriteDeltaEntity(&nullstate, &cl_entities[i].baseline, &buf, true, true);
	}

	MSG_WriteByte(&buf, svc_stufftext);
	MSG_WriteString(&buf, "precache\n");

	// Write it to the demo file
	fwrite(&buf.cursize, 4, 1, cls.demofile);
	fwrite(buf.data, buf.cursize, 1, cls.demofile);

	// The rest of the demo file will be individual frames
}

#pragma endregion 

// Adds the current command line as a clc_stringcmd to the client message.
// Things like godmode, noclip, etc, are commands directed to the server, so when they are typed in at the console, they will need to be forwarded.
void Cmd_ForwardToServer()
{
	char *cmd = Cmd_Argv(0);
	if (cls.state <= ca_connected || *cmd == '-' || *cmd == '+')
	{
		Com_Printf("Unknown command \"%s\"\n", cmd);
		return;
	}

	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
	SZ_Print(&cls.netchan.message, cmd);

	if (Cmd_Argc() > 1)
	{
		SZ_Print(&cls.netchan.message, " ");
		SZ_Print(&cls.netchan.message, Cmd_Args());
	}

	cls.forcePacket = true;
}

static void CL_Setenv_f()
{
	const int argc = Cmd_Argc();

	if (argc > 2)
	{
		char buffer[1000];

		Q_strncpyz(buffer, Cmd_Argv(1), sizeof(buffer));
		Q_strncatz(buffer, "=", sizeof(buffer));

		for (int i = 2; i < argc; i++)
		{
			Q_strncatz(buffer, Cmd_Argv(i), sizeof(buffer));
			Q_strncatz(buffer, " ", sizeof(buffer));
		}

		putenv(buffer);
	}
	else if (argc == 2)
	{
		char *env = getenv(Cmd_Argv(1));

		if (env)
			Com_Printf("%s=%s\n", Cmd_Argv(1), env);
		else
			Com_Printf("%s undefined\n", Cmd_Argv(1), env);
	}
}

static void CL_ForwardToServer_f()
{
	if (cls.state != ca_connected && cls.state != ca_active)
	{
		Com_Printf("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	// Don't forward the first argument
	if (Cmd_Argc() > 1)
	{
		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		SZ_Print(&cls.netchan.message, Cmd_Args());
		cls.forcePacket = true;
	}
}

static void CL_Pause_f()
{
	// Never pause in multiplayer
	if (Cvar_VariableValue("maxclients") > 1 || !Com_ServerState())
	{
		Cvar_SetValue("paused", 0);
		return;
	}

	Cvar_SetValue("paused", !cl_paused->value);
}

void CL_Quit_f()
{
	CL_Disconnect();
	Com_Quit();
}

// Called after an ERR_DROP was thrown
void CL_Drop()
{
	if (cls.state == ca_uninitialized)
		return;

	// If an error occurs during initial load or during game start, drop loading plaque
	if (cls.disable_servercount != -1 || cls.key_dest == key_game)
		SCR_EndLoadingPlaque(); // Get rid of loading plaque

	if (cls.state == ca_disconnected)
		return;

	CL_Disconnect();
}

// We have gotten a challenge from the server, so try and connect.
static void CL_SendConnectPacket()
{
	netadr_t adr;

	if (!NET_StringToAdr(cls.servername, &adr))
	{
		Com_Printf("Bad server address\n");
		cls.connect_time = 0;
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort(PORT_SERVER);

	const int port = Cvar_VariableInteger("qport");
	userinfo_modified = false;

	// If in compatibility mode, lie to server about this client's protocol, but exclude localhost for this.
	const int protocolversion = ((cl_servertrick->integer && strcmp(cls.servername, "localhost")) ? OLD_PROTOCOL_VERSION : PROTOCOL_VERSION); //mxd
	Netchan_OutOfBandPrint(NS_CLIENT, adr, "connect %i %i %i \"%s\"\n", protocolversion, port, cls.challenge, Cvar_Userinfo());
}

// Resend a connect message if the last one has timed out
static void CL_CheckForResend()
{
	// If the local server is running and we aren't, then connect
	if (cls.state == ca_disconnected && Com_ServerState() )
	{
		cls.state = ca_connecting;
		strncpy(cls.servername, "localhost", sizeof(cls.servername) - 1);

		// We don't need a challenge on the localhost
		CL_SendConnectPacket();

		return;
	}

	// Resend if we haven't gotten a reply yet
	if (cls.state != ca_connecting)
		return;

	if (cls.realtime - cls.connect_time < 3000)
		return;

	netadr_t adr;
	if (!NET_StringToAdr(cls.servername, &adr))
	{
		Com_Printf("Bad server address\n");
		cls.state = ca_disconnected;
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort(PORT_SERVER);

	cls.connect_time = (float)cls.realtime; // For retransmit requests

	Com_Printf("Connecting to %s...\n", cls.servername);

	Netchan_OutOfBandPrint(NS_CLIENT, adr, "getchallenge\n");
}

static void CL_Connect_f()
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: connect <server>\n");
		return;	
	}
	
	if (Com_ServerState())
		SV_Shutdown("Server quit\n", false); // If running a local server, kill it and reissue
	else
		CL_Disconnect();

	char *server = Cmd_Argv(1);
	NET_Config(true); // Allow remote
	CL_Disconnect();

	cls.state = ca_connecting;
	strncpy(cls.servername, server, sizeof(cls.servername) - 1);
	cls.connect_time = -99999; // CL_CheckForResend() will fire immediately
}

// Send the rest of the command line over as an unconnected command.
static void CL_Rcon_f()
{
	netadr_t to;

	if (!rcon_client_password->string)
	{
		Com_Printf("You must set 'rcon_password' before\nissuing an rcon command.\n");
		return;
	}

	NET_Config(true); // Allow remote

	char message[1024] = { 255, 255, 255, 255, 0 };
	Q_strncatz(message, "rcon ", sizeof(message));
	Q_strncatz(message, rcon_client_password->string, sizeof(message));
	Q_strncatz(message, " ", sizeof(message));

	for (int i = 1; i < Cmd_Argc(); i++)
	{
		Q_strncatz(message, Cmd_Argv(i), sizeof(message));
		Q_strncatz(message, " ", sizeof(message));
	}

	if (cls.state >= ca_connected)
	{
		to = cls.netchan.remote_address;
	}
	else
	{
		if (!strlen(rcon_address->string))
		{
			Com_Printf("You must either be connected,\nor set the 'rcon_address' cvar\nto issue rcon commands\n");
			return;
		}

		NET_StringToAdr(rcon_address->string, &to);
		if (to.port == 0)
			to.port = BigShort(PORT_SERVER);
	}
	
	NET_SendPacket(NS_CLIENT, strlen(message) + 1, message, to);
}

void CL_ClearState()
{
	S_StopAllSounds();
	CL_ClearEffects();
	CL_ClearTEnts();
	R_ClearState();

	// Wipe the entire cl structure
	memset(&cl, 0, sizeof(cl));
	memset(&cl_entities, 0, sizeof(cl_entities));

	//mxd. Initialize with unreachable value (fixes incorrect view interpolation after loading a game when the player is within 32 map units from [0,0,0] coords).
	for (int i = 0; i < CMD_BACKUP; i++)
		cl.predicted_origins[i][2] = MAX_WORLD_COORD * 2;

	cl.maxclients = MAX_CLIENTS; // From R1Q2
	SZ_Clear(&cls.netchan.message);
}

// Goes from a connected state to full screen console state.
// Sends a disconnect message to the server.
// This is also called on Com_Error, so it shouldn't cause any errors.
extern char *currentweaponmodel;
void CL_Disconnect()
{
	byte final[32];

	if (cls.state == ca_disconnected)
		return;

	if (cl_timedemo && cl_timedemo->value)
	{
		const int time = Sys_Milliseconds() - cl.timedemo_start;
		if (time > 0)
			Com_Printf("%i frames, %3.1f seconds: %3.1f fps\n", cl.timedemo_frames, time / 1000.0f, cl.timedemo_frames * 1000.0f / time);
	}

	VectorClear(cl.refdef.blend);
	UI_ForceMenuOff();
	cls.connect_time = 0;
	SCR_StopCinematic();

	if (cls.demorecording)
		CL_Stop_f();

	// Send a disconnect message to the server
	final[0] = clc_stringcmd;
	Q_strncpyz((char *)final + 1, "disconnect", sizeof(final) - 1);
	Netchan_Transmit(&cls.netchan, strlen(final), final);
	Netchan_Transmit(&cls.netchan, strlen(final), final);
	Netchan_Transmit(&cls.netchan, strlen(final), final);

	CL_ClearState();

	// Stop download
	if (cls.download)
	{
		fclose(cls.download);
		cls.download = NULL;
	}

#ifdef USE_CURL	// HTTP downloading from R1Q2
	CL_CancelHTTPDownloads(true);
	cls.downloadReferer[0] = 0;
	cls.downloadname[0] = 0;
	cls.downloadposition = 0;
#endif

	cls.state = ca_disconnected;

	// Reset current weapon model
	currentweaponmodel = NULL;
}

void CL_Disconnect_f()
{
	Com_Error(ERR_DROP, "Disconnected from server");
}

// Just sent as a hint to the client that they should drop to full console
static void CL_Changing_f()
{
	// ZOID. If we are downloading, we don't change! This so we don't suddenly stop downloading a map
	SCR_BeginLoadingPlaque(NULL); // Knightmare moved here

	if (cls.download)
		return;

	cls.state = ca_connected; // Not active anymore, but not disconnected
	Com_Printf("\nChanging map...\n");

#ifdef USE_CURL
	// FS: Added because Whale's Weapons HTTP server rejects you after a lot of 404s.  Then you lose HTTP until a hard reconnect.
	if (cls.downloadServerRetry[0] != 0)
		CL_SetHTTPServer(cls.downloadServerRetry);
#endif
}

// The server is changing levels
static void CL_Reconnect_f()
{
	// ZOID. If we are downloading, we don't change! This so we don't suddenly stop downloading a map
	if (cls.download)
		return;

	S_StopAllSounds();
	if (cls.state == ca_connected)
	{
		Com_Printf("reconnecting...\n");
		cls.state = ca_connected;
		MSG_WriteChar(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, "new");
		cls.forcePacket = true;

		return;
	}

	if (*cls.servername)
	{
		if (cls.state >= ca_connected)
		{
			CL_Disconnect();
			cls.connect_time = (float)cls.realtime - 1500;
		}
		else
		{
			cls.connect_time = -99999; // Fire immediately
		}

		cls.state = ca_connecting;
		Com_Printf("reconnecting...\n");
	}
}

// Handle a reply from a ping
static void CL_ParseStatusMessage()
{
	char *s = MSG_ReadString(&net_message);
	Com_Printf("%s\n", s);
	UI_AddToServerList(net_from, s);
}

// <serverping> Added code for compute ping time of server broadcasted
extern int global_udp_server_time;
extern int global_ipx_server_time;
extern int global_adr_server_time[16];
extern netadr_t global_adr_server_netadr[16];

void CL_PingServers_f()
{
	netadr_t adr;
	char name[32];
	const int protocolversion = (cl_servertrick->integer ? OLD_PROTOCOL_VERSION : PROTOCOL_VERSION); //mxd

	NET_Config(true); // Allow remote

	// Send a broadcast packet
	Com_Printf("pinging broadcast...\n");

	// Send a packet to each address book entry
	for (int i = 0; i < 16; i++)
	{
		const size_t adrsize = sizeof(global_adr_server_netadr[0]); //mxd
		memset(&global_adr_server_netadr[i], 0, adrsize);
		global_adr_server_time[i] = Sys_Milliseconds();

		Com_sprintf(name, sizeof(name), "adr%i", i);
		char *adrstring = Cvar_VariableString(name);
		if (!adrstring || !adrstring[0])
			continue;

		Com_Printf("pinging %s...\n", adrstring);
		if (!NET_StringToAdr(adrstring, &adr))
		{
			Com_Printf("Bad address: %s\n", adrstring);
			continue;
		}

		if (!adr.port)
			adr.port = BigShort(PORT_SERVER);

		memcpy(&global_adr_server_netadr[i], &adr, adrsize);

		// If the server is using the old protocol, lie to it about this client's protocol
		Netchan_OutOfBandPrint(NS_CLIENT, adr, va("info %i", protocolversion)); //mxd
	}

	cvar_t *noudp = Cvar_Get("noudp", "0", CVAR_NOSET);
	if (!noudp->value)
	{
		global_udp_server_time = Sys_Milliseconds();
		adr.type = NA_BROADCAST;
		adr.port = BigShort(PORT_SERVER);

		// If the server is using the old protocol, lie to it about this client's protocol
		Netchan_OutOfBandPrint(NS_CLIENT, adr, va("info %i", protocolversion)); //mxd
	}

	cvar_t *noipx = Cvar_Get("noipx", "0", CVAR_NOSET);
	if (!noipx->value)
	{
		global_ipx_server_time = Sys_Milliseconds();
		adr.type = NA_BROADCAST_IPX;
		adr.port = BigShort(PORT_SERVER);
		
		// If the server is using the old protocol, lie to it about this client's protocol
		Netchan_OutOfBandPrint(NS_CLIENT, adr, va("info %i", protocolversion)); //mxd
	}
}

// Load or download any custom player skins and models
static void CL_Skins_f()
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		// BIG UGLY HACK for old connected to server using old protocol
		// Changed config strings require different parsing
		const int csplayerskins = (LegacyProtocol() ? OLD_CS_PLAYERSKINS : CS_PLAYERSKINS); //mxd
		if (!cl.configstrings[csplayerskins + i][0])
			continue;

		Com_Printf("client %i: %s\n", i, cl.configstrings[csplayerskins + i]);
		SCR_UpdateScreen();
		IN_Update(); // Pump message loop
		CL_ParseClientinfo(i);
	}
}

static void CL_AACSkey_f()
{
	Com_Printf("AACS processing keys: 09 F9 11 02 9D 74 E3 5B D8 41 56 C5 63 56 88 C0\n");
	Com_Printf("                      45 5F E1 04 22 CA 29 C4 93 3F 95 05 2B 79 2A B2\n");
}

// Responses to broadcasts, etc
static void CL_ConnectionlessPacket()
{
	MSG_BeginReading(&net_message);
	MSG_ReadLong(&net_message); // Skip the -1

	char *s = MSG_ReadStringLine(&net_message);
	Cmd_TokenizeString(s, false);

	char *c = Cmd_Argv(0);
	Com_Printf("%s: %s\n", NET_AdrToString(net_from), c);

	// Server connection
	if (!strcmp(c, "client_connect"))
	{
		if (cls.state == ca_connected)
		{
			Com_Printf("Dup connect received. Ignored.\n");
			return;
		}

		Netchan_Setup(NS_CLIENT, &cls.netchan, net_from, cls.quakePort);

		// HTTP downloading from R1Q2
		char *buff = NET_AdrToString(cls.netchan.remote_address);
		for (int i = 1; i < Cmd_Argc(); i++)
		{
			char *p = Cmd_Argv(i);
			if (!strncmp(p, "dlserver=", 9))
			{
#ifdef USE_CURL
				p += 9;
				Com_sprintf(cls.downloadReferer, sizeof(cls.downloadReferer), "quake2://%s", buff);
				CL_SetHTTPServer(p);

				if (cls.downloadServer[0])
					Com_Printf("HTTP downloading enabled, URL: %s\n", cls.downloadServer);
#else
				Com_Printf("HTTP downloading supported by server but this client was built without USE_CURL, too bad.\n");
#endif	// USE_CURL
			}
		}
		// end HTTP downloading from R1Q2

		MSG_WriteChar(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, "new");	
		cls.forcePacket = true;
		cls.state = ca_connected;

		return;
	}

	// Server responding to a status broadcast
	if (!strcmp(c, "info"))
	{
		CL_ParseStatusMessage();
		return;
	}

	// Remote command from gui front end
	if (!strcmp(c, "cmd"))
	{
		if (!NET_IsLocalAddress(net_from))
		{
			Com_Printf("Command packet from remote host. Ignored.\n");
			return;
		}

		GLimp_AppActivate(); //mxd
		s = MSG_ReadString(&net_message);
		Cbuf_AddText(s);
		Cbuf_AddText("\n");

		return;
	}

	// Print command from somewhere
	if (!strcmp(c, "print"))
	{
		s = MSG_ReadString(&net_message);
		Com_Printf("%s", s);

		return;
	}

	// Ping from somewhere
	if (!strcmp(c, "ping"))
	{
		Netchan_OutOfBandPrint(NS_CLIENT, net_from, "ack");
		return;
	}

	// Challenge from the server we are connecting to
	if (!strcmp(c, "challenge"))
	{
		cls.challenge = atoi(Cmd_Argv(1));
		CL_SendConnectPacket();
		return;
	}

	// Echo request from server
	if (!strcmp(c, "echo"))
	{
		Netchan_OutOfBandPrint(NS_CLIENT, net_from, "%s", Cmd_Argv(1) );
		return;
	}

	Com_Printf("Unknown command.\n");
}

void CL_ReadPackets()
{
	while (NET_GetPacket(NS_CLIENT, &net_from, &net_message))
	{
		// Remote command packet
		if (*(int *)net_message.data == -1)
		{
			CL_ConnectionlessPacket();
			continue;
		}

		if (cls.state == ca_disconnected || cls.state == ca_connecting)
			continue; // Dump it if not connected

		if (net_message.cursize < 8)
		{
			Com_Printf("%s: runt packet\n", NET_AdrToString(net_from));
			continue;
		}

		// Packet from server
		if (!NET_CompareAdr(net_from, cls.netchan.remote_address))
		{
			Com_DPrintf ("%s: sequenced packet without connection\n", NET_AdrToString(net_from));
			continue;
		}

		if (!Netchan_Process(&cls.netchan, &net_message))
			continue; // Wasn't accepted for some reason

		CL_ParseServerMessage();
	}

	// Check timeout
	if (cls.state >= ca_connected && cls.realtime - cls.netchan.last_received > cl_timeout->value * 1000)
	{
		if (++cl.timeoutcount > 5) // Timeoutcount saves debugger
		{
			Com_Printf("\nServer connection timed out.\n");
			CL_Disconnect();
		}
	}
	else
	{
		cl.timeoutcount = 0;
	}
}

//=============================================================================

void CL_FixUpGender()
{
	if (!info_gender_auto->integer)
		return;

	if (info_gender->modified)
	{
		// Was set directly, don't override the user
		info_gender->modified = false;
		return;
	}

	char sk[80];
	strncpy(sk, info_skin->string, sizeof(sk) - 1);
	char *p = strchr(sk, '/');

	if (p != NULL)
		*p = 0;

	if (Q_stricmp(sk, "male") == 0 || Q_stricmp(sk, "cyborg") == 0)
		Cvar_Set("gender", "male");
	else if (Q_stricmp(sk, "female") == 0 || Q_stricmp(sk, "crackhor") == 0)
		Cvar_Set("gender", "female");
	else
		Cvar_Set("gender", "none");

	info_gender->modified = false;
}

static void CL_Userinfo_f()
{
	Com_Printf("User info settings:\n");
	Info_Print(Cvar_Userinfo());
}

// Restart the sound subsystem so it can pick up new parameters and flush all sounds
void CL_Snd_Restart_f()
{
	S_Shutdown();
	S_Init();
	CL_RegisterSounds();
}

extern int precache_check; // For autodownload of precache items
extern int precache_spawncount;
extern int precache_model_skin;
extern byte *precache_model; // Used for skin checking in alias models
extern int precache_pak; // Knightmare added

void CL_ResetPrecacheCheck()
{
	precache_check = CS_MODELS;
	precache_model = 0;
	precache_model_skin = 0;
	precache_pak = 0; // Knightmare added
}

// The server will send this command right before allowing the client into the server
static void CL_Precache_f()
{
	// Yet another hack to let old demos work the old precache sequence
	if (Cmd_Argc() < 2)
	{
		uint map_checksum; // For detecting cheater maps

		CM_LoadMap(cl.configstrings[CS_MODELS + 1], true, &map_checksum);
		CL_RegisterSounds();
		CL_PrepRefresh();

		return;
	}

	precache_check = CS_MODELS;
	precache_spawncount = atoi(Cmd_Argv(1));
	precache_model = 0;
	precache_model_skin = 0;
	precache_pak = 0;	// Knightmare added

#ifdef USE_CURL	// HTTP downloading from R1Q2
	CL_HTTP_ResetMapAbort(); // Knightmare- reset the map abort flag
#endif

	CL_RequestNextDownload();
}

#ifdef LOC_SUPPORT // Xile/NiceAss LOC
static void CL_AddLoc_f()
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: loc_add <label/description>\n");
		return;
	}

	CL_LocAdd(Cmd_Argv(1));
}

static void CL_DeleteLoc_f()
{
	CL_LocDelete();
}

static void CL_SaveLoc_f()
{
	CL_LocWrite();
}
#endif // LOC_SUPPORT

static void CL_InitLocal()
{
	cls.state = ca_disconnected;
	cls.realtime = Sys_Milliseconds();

	CL_InitInput();

	adr0 = Cvar_Get("adr0", "", CVAR_ARCHIVE);
	adr1 = Cvar_Get("adr1", "", CVAR_ARCHIVE);
	adr2 = Cvar_Get("adr2", "", CVAR_ARCHIVE);
	adr3 = Cvar_Get("adr3", "", CVAR_ARCHIVE);
	adr4 = Cvar_Get("adr4", "", CVAR_ARCHIVE);
	adr5 = Cvar_Get("adr5", "", CVAR_ARCHIVE);
	adr6 = Cvar_Get("adr6", "", CVAR_ARCHIVE);
	adr7 = Cvar_Get("adr7", "", CVAR_ARCHIVE);
	adr8 = Cvar_Get("adr8", "", CVAR_ARCHIVE);
	adr9 = Cvar_Get("adr9", "", CVAR_ARCHIVE);
	adr10 = Cvar_Get("adr10", "", CVAR_ARCHIVE);
	adr11 = Cvar_Get("adr11", "", CVAR_ARCHIVE);

	// Register our variables
	cl_stereo_separation = Cvar_Get("cl_stereo_separation", "0.4", CVAR_ARCHIVE);
	cl_stereo = Cvar_Get("cl_stereo", "0", 0);

	cl_add_blend = Cvar_Get("cl_blend", "1", 0);
	cl_add_lights = Cvar_Get("cl_lights", "1", 0);
	cl_add_particles = Cvar_Get("cl_particles", "1", 0);
	cl_add_entities = Cvar_Get("cl_entities", "1", 0);
	cl_gun = Cvar_Get("cl_gun", "1", 0);
	cl_weapon_shells = Cvar_Get("cl_weapon_shells", "1", CVAR_ARCHIVE);
	cl_footsteps = Cvar_Get("cl_footsteps", "1", 0);

	// Reduction factor for particle effects
	cl_particle_scale = Cvar_Get("cl_particle_scale", "1", CVAR_ARCHIVE);

	// Whether to adjust fov for wide aspect rattio
	cl_widescreen_fov = Cvar_Get("cl_widescreen_fov", "1", CVAR_ARCHIVE);

	cl_noskins = Cvar_Get("cl_noskins", "0", 0);
	cl_predict = Cvar_Get("cl_predict", "1", 0);
	cl_maxfps = Cvar_Get("cl_maxfps", "90", 0);

#ifdef CLIENT_SPLIT_NETFRAME
	cl_async = Cvar_Get("cl_async", "1", 0);
	net_maxfps = Cvar_Get("net_maxfps", "60", 0);
	r_maxfps = Cvar_Get("r_maxfps", "125", 0);
#endif

	cl_sleep = Cvar_Get("cl_sleep", "1", 0);

	// Whether to trick version 34 servers that this is a version 34 client
	cl_servertrick = Cvar_Get("cl_servertrick", "0", CVAR_ARCHIVE);

	// Psychospaz's chasecam
	cg_thirdperson = Cvar_Get("cg_thirdperson", "0", CVAR_ARCHIVE);
	cg_thirdperson_angle = Cvar_Get("cg_thirdperson_angle", "10", CVAR_ARCHIVE);
	cg_thirdperson_dist = Cvar_Get("cg_thirdperson_dist", "50", CVAR_ARCHIVE);
	cg_thirdperson_alpha = Cvar_Get("cg_thirdperson_alpha", "0", CVAR_ARCHIVE);
	cg_thirdperson_chase = Cvar_Get("cg_thirdperson_chase", "1", CVAR_ARCHIVE);
	cg_thirdperson_adjust = Cvar_Get("cg_thirdperson_adjust", "1", CVAR_ARCHIVE);

	cl_blood = Cvar_Get("cl_blood", "2", CVAR_ARCHIVE);

	// Option for old explosions
	cl_old_explosions = Cvar_Get("cl_old_explosions", "0", CVAR_ARCHIVE);
	
	// Option for unique plasma explosion sound
	cl_plasma_explo_sound = Cvar_Get("cl_plasma_explo_sound", "0", CVAR_ARCHIVE);
	cl_item_bobbing = Cvar_Get("cl_item_bobbing", "0", CVAR_ARCHIVE);

	// Psychospaz's changeable rail code
	cl_railred = Cvar_Get("cl_railred", "20", CVAR_ARCHIVE);
	cl_railgreen = Cvar_Get("cl_railgreen", "50", CVAR_ARCHIVE);
	cl_railblue = Cvar_Get("cl_railblue", "175", CVAR_ARCHIVE);
	cl_railtype = Cvar_Get("cl_railtype", "0", CVAR_ARCHIVE);
	cl_rail_length = Cvar_Get("cl_rail_length", va("%i", DEFAULT_RAIL_LENGTH), CVAR_ARCHIVE);
	cl_rail_space = Cvar_Get("cl_rail_space", va("%i", DEFAULT_RAIL_SPACE), CVAR_ARCHIVE);

	// Whether to use texsurfs.txt footstep sounds
	cl_footstep_override = Cvar_Get("cl_footstep_override", "1", CVAR_ARCHIVE);

	// Decal control
	r_decals = Cvar_Get("r_decals", "500", CVAR_ARCHIVE);
	r_decal_life = Cvar_Get("r_decal_life", "1000", CVAR_ARCHIVE);

	//con_font_size = Cvar_Get("con_font_size", "8", CVAR_ARCHIVE); //mxd. Moved to Con_Init()
	alt_text_color = Cvar_Get("alt_text_color", "2", CVAR_ARCHIVE);

	cl_rogue_music = Cvar_Get("cl_rogue_music", "0", CVAR_ARCHIVE);
	cl_xatrix_music = Cvar_Get("cl_xatrix_music", "0", CVAR_ARCHIVE);

	cl_upspeed = Cvar_Get("cl_upspeed", "200", 0);
	cl_forwardspeed = Cvar_Get("cl_forwardspeed", "200", 0);
	cl_sidespeed = Cvar_Get("cl_sidespeed", "200", 0);
	cl_yawspeed = Cvar_Get("cl_yawspeed", "140", 0);
	cl_pitchspeed = Cvar_Get("cl_pitchspeed", "150", 0);
	cl_anglespeedkey = Cvar_Get("cl_anglespeedkey", "1.5", 0);

	cl_run = Cvar_Get("cl_run", "0", CVAR_ARCHIVE);
	freelook = Cvar_Get("freelook", "1", CVAR_ARCHIVE); // Knightmare changed, was 0
	lookspring = Cvar_Get("lookspring", "0", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get("lookstrafe", "0", CVAR_ARCHIVE);
	sensitivity = Cvar_Get("sensitivity", "3", CVAR_ARCHIVE);
	menu_sensitivity = Cvar_Get("menu_sensitivity", "1", CVAR_ARCHIVE);
	menu_rotate = Cvar_Get("menu_rotate", "0", CVAR_ARCHIVE);
	menu_alpha = Cvar_Get("menu_alpha", "0.6", CVAR_ARCHIVE);

	m_pitch = Cvar_Get("m_pitch", "0.022", CVAR_ARCHIVE);
	m_yaw = Cvar_Get("m_yaw", "0.022", 0);
	m_forward = Cvar_Get("m_forward", "1", 0);
	m_side = Cvar_Get("m_side", "1", 0);

	cl_shownet = Cvar_Get("cl_shownet", "0", 0);
	cl_showmiss = Cvar_Get("cl_showmiss", "0", 0);
	cl_showclamp = Cvar_Get("showclamp", "0", 0);
	cl_timeout = Cvar_Get("cl_timeout", "120", 0);
	cl_paused = Cvar_Get("paused", "0", CVAR_CHEAT);
	cl_timedemo = Cvar_Get("timedemo", "0", CVAR_CHEAT);

	rcon_client_password = Cvar_Get("rcon_password", "", 0);
	rcon_address = Cvar_Get("rcon_address", "", 0);

	cl_lightlevel = Cvar_Get("r_lightlevel", "0", 0);

	// Userinfo
	info_password = Cvar_Get("password", "", CVAR_USERINFO);
	info_spectator = Cvar_Get("spectator", "0", CVAR_USERINFO);
	info_name = Cvar_Get("name", "unnamed", CVAR_USERINFO | CVAR_ARCHIVE);
	info_skin = Cvar_Get("skin", "male/grunt", CVAR_USERINFO | CVAR_ARCHIVE);
	info_rate = Cvar_Get("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE); // FIXME
	info_msg = Cvar_Get("msg", "1", CVAR_USERINFO | CVAR_ARCHIVE);
	info_hand = Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	info_fov = Cvar_Get("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE);
	info_gender = Cvar_Get("gender", "male", CVAR_USERINFO | CVAR_ARCHIVE);
	info_gender_auto = Cvar_Get("gender_auto", "1", CVAR_ARCHIVE);
	info_gender->modified = false; // Clear this so we know when user sets it manually

	cl_vwep = Cvar_Get("cl_vwep", "1", CVAR_ARCHIVE);

	// For the server to tell which version the client is
	cl_engine = Cvar_Get("cl_engine", ENGINE_NAME, CVAR_USERINFO | CVAR_NOSET | CVAR_LATCH); //mxd. "KMQuake2" -> ENGINE_NAME
	cl_engine_version = Cvar_Get("cl_engine_version", va("%4.2f", VERSION), CVAR_USERINFO | CVAR_NOSET | CVAR_LATCH);

#ifdef LOC_SUPPORT	// Xile/NiceAss LOC
	cl_drawlocs = Cvar_Get("cl_drawlocs", "0", 0);
	loc_here = Cvar_Get("loc_here", "", CVAR_NOSET);
	loc_there = Cvar_Get("loc_there", "", CVAR_NOSET);
#endif

#ifdef USE_CURL	// HTTP downloading from R1Q2
	cl_http_proxy = Cvar_Get("cl_http_proxy", "", 0);
	cl_http_filelists = Cvar_Get("cl_http_filelists", "1", 0);
	cl_http_downloads = Cvar_Get("cl_http_downloads", "1", CVAR_ARCHIVE);
	cl_http_max_connections = Cvar_Get("cl_http_max_connections", "4", 0);
#endif

	// Register our commands
	Cmd_AddCommand("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand("pause", CL_Pause_f);
	Cmd_AddCommand("pingservers", CL_PingServers_f);
	Cmd_AddCommand("skins", CL_Skins_f);

	Cmd_AddCommand("userinfo", CL_Userinfo_f);
	Cmd_AddCommand("snd_restart", CL_Snd_Restart_f);

	Cmd_AddCommand("changing", CL_Changing_f);
	Cmd_AddCommand("disconnect", CL_Disconnect_f);
	Cmd_AddCommand("record", CL_Record_f);
	Cmd_AddCommand("stop", CL_Stop_f);

	Cmd_AddCommand("quit", CL_Quit_f);

	Cmd_AddCommand("connect", CL_Connect_f);
	Cmd_AddCommand("reconnect", CL_Reconnect_f);

	Cmd_AddCommand("rcon", CL_Rcon_f);
	Cmd_AddCommand("setenv", CL_Setenv_f);
	Cmd_AddCommand("precache", CL_Precache_f);
	Cmd_AddCommand("download", CL_Download_f);
	Cmd_AddCommand("writeconfig", CL_WriteConfig_f);
	Cmd_AddCommand("aacskey", CL_AACSkey_f);

#ifdef LOC_SUPPORT	// Xile/NiceAss LOC
	Cmd_AddCommand("loc_add", CL_AddLoc_f);
	Cmd_AddCommand("loc_del", CL_DeleteLoc_f);
	Cmd_AddCommand("loc_save", CL_SaveLoc_f);
	Cmd_AddCommand("loc_help", CL_LocHelp_f);
#endif

	// Forward to server commands

	// The only thing this does is allow command completion to work -- all unknown commands are automatically forwarded to the server
	Cmd_AddCommand("wave", NULL);
	Cmd_AddCommand("inven", NULL);
	Cmd_AddCommand("kill", NULL);
	Cmd_AddCommand("use", NULL);
	Cmd_AddCommand("drop", NULL);
	Cmd_AddCommand("say", NULL);
	Cmd_AddCommand("say_team", NULL);
	Cmd_AddCommand("info", NULL);
	Cmd_AddCommand("prog", NULL);
	Cmd_AddCommand("give", NULL);
	Cmd_AddCommand("god", NULL);
	Cmd_AddCommand("notarget", NULL);
	Cmd_AddCommand("noclip", NULL);
	Cmd_AddCommand("invuse", NULL);
	Cmd_AddCommand("invprev", NULL);
	Cmd_AddCommand("invnext", NULL);
	Cmd_AddCommand("invdrop", NULL);
	Cmd_AddCommand("weapnext", NULL);
	Cmd_AddCommand("weapprev", NULL);
}

// Writes key bindings and archived cvars to config.cfg
static qboolean CL_WriteConfiguration(char *cfgName)
{
	if (!cfgName || !*cfgName || cls.state == ca_uninitialized)
		return false;

	// Knightmare changed- use separate config for better cohabitation
	char path[MAX_QPATH];
	Com_sprintf(path, sizeof(path), "%s/%s.cfg", FS_Gamedir(), cfgName);
	FILE *f = fopen(path, "w");
	if (!f)
	{
		// Knightmare changed- use separate config for better cohabitation
		Com_Printf("Couldn't write %s.cfg.\n", cfgName);
		return false;
	}

	fprintf(f, "// This file is generated by "ENGINE_NAME", do not modify.\n");
	fprintf(f, "// Use autoexec.cfg for adding custom settings.\n");
	Key_WriteBindings(f);
	fclose(f);

	Cvar_WriteVariables(path);

	return true;
}

void CL_WriteConfig_f()
{
	if (Cmd_Argc() == 1 || Cmd_Argc() == 2)
	{
		char cfgName[MAX_QPATH];
		
		if (Cmd_Argc() == 1)
			Com_sprintf(cfgName, sizeof(cfgName), "kmq2config");
		else
			Q_strncpyz(cfgName, Cmd_Argv(1), sizeof(cfgName));

		if(CL_WriteConfiguration(cfgName))
			Com_Printf("Wrote config file %s/%s.cfg.\n", FS_Gamedir(), cfgName);
	}
	else
	{
		Com_Printf("Usage: writeconfig <name>\n");
	}
}

//============================================================================

static void CL_FixCvarCheats()
{
	// Allow cheats in singleplayer, don't allow in multiplayer
	const qboolean singleplayer = (!cl.configstrings[CS_MAXCLIENTS][0] || !strcmp(cl.configstrings[CS_MAXCLIENTS], "1"));
	Cvar_FixCheatVars(singleplayer);
}


#ifdef CLIENT_SPLIT_NETFRAME

static void CL_RefreshInputs()
{
	// Fetch results from server
	CL_ReadPackets();

	// Get new key events
	IN_Update();

	// Process console commands
	Cbuf_Execute();

	// Fix any cheating cvars
	CL_FixCvarCheats();

	// Update usercmd state
	if (cls.state > ca_connecting)
		CL_RefreshCmd();
	else
		CL_RefreshMove();
}

static void CL_SendCommand_Async()
{
	// Send intentions now
	CL_SendCmd(true);

	// Resend a connection request if necessary
	CL_CheckForResend();
}

#define FRAMETIME_MAX 0.5f // was 0.2

void CL_Frame_Async(int msec)
{
	static int packetDelta = 0;
	static int renderDelta = 0;
	static int miscDelta = 0;
	static int lasttimecalled;

	qboolean packetFrame = true;
	qboolean renderFrame = true;
	qboolean miscFrame = true;

	// Don't allow setting maxfps too low or too high
	if (net_maxfps->value < 10)
		Cvar_SetValue("net_maxfps", 10);
	else if (net_maxfps->value > 90)
		Cvar_SetValue("net_maxfps", 90);

	if (r_maxfps->value < 10)
		Cvar_SetValue("r_maxfps", 10);
	else if (r_maxfps->value > 1000)
		Cvar_SetValue("r_maxfps", 1000);

	packetDelta += msec;
	renderDelta += msec;
	miscDelta += msec;

	// Decide the simulation time
	cls.netFrameTime = packetDelta * 0.001f;
	cls.renderFrameTime = renderDelta * 0.001f;
	cl.time += msec;
	cls.realtime = curtime;

	// Don't extrapolate too far ahead
	cls.netFrameTime = min(FRAMETIME_MAX, cls.netFrameTime);
	cls.renderFrameTime = min(FRAMETIME_MAX, cls.renderFrameTime);

	// If in the debugger last frame, don't timeout
	if (msec > 5000) //TODO: mxd. This doesn't work for me...
		cls.netchan.last_received = Sys_Milliseconds();

	if (!cl_timedemo->value)
	{	
		// Don't flood packets out while connecting
		if (cls.state == ca_connected && packetDelta < 100)
			packetFrame = false;

		if (packetDelta < 1000.0 / net_maxfps->value)
			packetFrame = false;
		else if (cls.netFrameTime == cls.renderFrameTime)
			packetFrame = false;

		if (renderDelta < 1000.0 / r_maxfps->value)
			renderFrame = false;

		// Stuff that only needs to run at 10FPS
		if (miscDelta < 100)
			miscFrame = false;
		
		if (!packetFrame && !renderFrame && !cls.forcePacket && !userinfo_modified)
		{	
			// Pooy's CPU usage fix
			if (cl_sleep->integer)
			{
				const int temptime = (int)min(1000.0f / net_maxfps->value - packetDelta, 1000.0f / r_maxfps->value - renderDelta);
				if (temptime > 1)
					Sys_Sleep(1);
			} // end CPU usage fix

			return;
		}
		
	}
	else if (msec < 1)	// Don't exceed 1000 fps in timedemo mode (fixes hang)
	{
		return;
	}

#ifdef USE_CURL	// HTTP downloading from R1Q2
	if (cls.state == ca_connected)
		CL_RunHTTPDownloads(); // downloads run full speed when connecting
#endif	// USE_CURL

	// Update the inputs (keyboard, mouse, console)
	if (packetFrame || renderFrame)
		CL_RefreshInputs();

	if (cls.forcePacket || userinfo_modified)
	{
		packetFrame = true;
		cls.forcePacket = false;
	}

	// Send a new command message to the server
	if (packetFrame)
	{
		packetDelta = 0;
		CL_SendCommand_Async();

#ifdef USE_CURL	// HTTP downloading from R1Q2
		CL_RunHTTPDownloads(); // Downloads run less often in game
#endif
	}
	
	if (renderFrame)
	{
		renderDelta = 0;

		if (miscFrame)
		{
			miscDelta = 0;

			// Let the mouse activate or deactivate
			IN_Update();

			// Allow rendering DLL change
			VID_CheckChanges();
		}

		// Predict all unacknowledged movements
		CL_PredictMovement();

		if (!cl.refresh_prepped && cls.state == ca_active)
			CL_PrepRefresh();

		// Update the screen
		if (host_speeds->value)
			time_before_ref = Sys_Milliseconds();

		SCR_UpdateScreen();

		if (host_speeds->value)
			time_after_ref = Sys_Milliseconds();

		// Update audio
		S_Update(cl.refdef.vieworg, cl.v_forward, cl.v_right, cl.v_up);
		
		// Advance local effects for next frame
		CL_RunDLights();
		CL_RunLightStyles();
		SCR_RunConsole();
		SCR_RunLetterbox();

		cls.framecount++;

		if (log_stats->value)
		{
			if (cls.state == ca_active)
			{
				if (!lasttimecalled)
				{
					lasttimecalled = Sys_Milliseconds();
					if (log_stats_file)
						fprintf(log_stats_file, "0\n");
				}
				else
				{
					const int now = Sys_Milliseconds();

					if (log_stats_file)
						fprintf(log_stats_file, "%d\n", now - lasttimecalled);
					lasttimecalled = now;
				}
			}
		}
	}
}
#endif // CLIENT_SPLIT_NETFRAME

static void CL_SendCommand()
{
	// Get new key events
	IN_Update();

	// Process console commands
	Cbuf_Execute();

	// Fix any cheating cvars
	CL_FixCvarCheats();

	// Send intentions now
	CL_SendCmd(false);

	// Resend a connection request if necessary
	CL_CheckForResend();
}

void CL_Frame(const int msec)
{
	static int extratime;
	static int lasttimecalled;

	if (dedicated->value)
		return;

#ifdef CLIENT_SPLIT_NETFRAME
	if (cl_async->value && !cl_timedemo->value)
	{
		CL_Frame_Async(msec);
		return;
	}
#endif

	extratime += msec;

	// Don't allow setting maxfps too low (or game could stop responding). Don't allow too high, either
	if (cl_maxfps->value < 10)
		Cvar_SetValue("cl_maxfps", 10);
	else if (cl_maxfps->value > 500)
		Cvar_SetValue("cl_maxfps", 500);

	if (!cl_timedemo->value)
	{
		if (cls.state == ca_connected && extratime < 100)
			return; // don't flood packets out while connecting

		if (extratime < 1000.0f / cl_maxfps->value)
		{	
			// Pooy's CPU usage fix
			if (cl_sleep->integer)
			{
				const int temptime = (int)(1000 / cl_maxfps->value - extratime);
				if (temptime > 1)
					Sys_Sleep(1);
			} // end CPU usage fix

			return; // framerate is too high
		}
	}
	else if (extratime < 1)	// Don't exceed 1000 fps in timedemo mode (fixes hang)
	{
		return;
	}

	// Let the mouse activate or deactivate
	IN_Update();

	// Decide the simulation time
	cls.netFrameTime = extratime / 1000.0f;
	cl.time += extratime;
	cls.realtime = curtime;

	extratime = 0;

	cls.netFrameTime = min(0.2f, cls.netFrameTime);
	cls.renderFrameTime = cls.netFrameTime;
		
	// Clamp this to acceptable values (don't allow infinite particles)
	if (cl_particle_scale->value < 1.0f)
		Cvar_SetValue("cl_particle_scale", 1);

	// Clamp this to acceptable minimum length
	if (cl_rail_length->value < MIN_RAIL_LENGTH)
		Cvar_SetValue("cl_rail_length", MIN_RAIL_LENGTH);

	// Clamp this to acceptable minimum duration
	if (r_decal_life->value < MIN_DECAL_LIFE)
		Cvar_SetValue("r_decal_life", MIN_DECAL_LIFE);

	// If in the debugger last frame, don't timeout
	if (msec > 5000) //TODO: mxd. This doesn't work for me...
		cls.netchan.last_received = Sys_Milliseconds();

#ifdef USE_CURL	// HTTP downloading from R1Q2
	CL_RunHTTPDownloads();
#endif

	// Fetch results from server
	CL_ReadPackets();

	// Send a new command message to the server
	CL_SendCommand();

	// Predict all unacknowledged movements
	CL_PredictMovement();

	// Allow rendering DLL change
	VID_CheckChanges();
	if (!cl.refresh_prepped && cls.state == ca_active)
		CL_PrepRefresh();

	// Update the screen
	if (host_speeds->value)
		time_before_ref = Sys_Milliseconds();

	SCR_UpdateScreen();

	if (host_speeds->value)
		time_after_ref = Sys_Milliseconds();

	// Update audio
	S_Update(cl.refdef.vieworg, cl.v_forward, cl.v_right, cl.v_up);
	
	// Advance local effects for next frame
	CL_RunDLights();
	CL_RunLightStyles();
	SCR_RunConsole();
	SCR_RunLetterbox();

	cls.framecount++;

	if (log_stats->integer)
	{
		if (cls.state == ca_active)
		{
			if (!lasttimecalled)
			{
				lasttimecalled = Sys_Milliseconds();
				if (log_stats_file)
					fprintf(log_stats_file, "0\n");
			}
			else
			{
				const int now = Sys_Milliseconds();

				if (log_stats_file)
					fprintf(log_stats_file, "%d\n", now - lasttimecalled);
				lasttimecalled = now;
			}
		}
	}
}

#pragma region ======================= Init / Shutdown

void CL_Init()
{
	if (dedicated->integer)
		return; // Nothing running on the client

	// All archived variables will now be loaded
	Con_Init();	
	S_Init();
	VID_Init();
	V_Init();
	
	net_message.data = net_message_buffer;
	net_message.maxsize = sizeof(net_message_buffer);

	UI_Init();

	SCR_Init();
	cls.disable_screen = true; // Don't draw yet

	CL_InitLocal();
	IN_Init();

#ifdef USE_CURL	// HTTP downloading from R1Q2
	CL_InitHTTPDownloads();
#endif	// USE_CURL

	//Cbuf_AddText("exec autoexec.cfg\n");
	FS_ExecAutoexec();
	Cbuf_Execute();
}

// FIXME: this is a callback from Sys_Quit and Com_Error.
// It would be better to run quit through here before the final handoff to the sys code.
void CL_Shutdown()
{
	static qboolean isdown = false;
	int sec, base; // zaphster's delay variables

	if (isdown)
	{
		Com_Printf(S_COLOR_RED"%s: recursive shutdown\n", __func__);
		return;
	}
	isdown = true;

#ifdef USE_CURL	// HTTP downloading from R1Q2
	CL_HTTP_Cleanup(true);
#endif	// USE_CURL

	CL_WriteConfiguration("kmq2config");

	// Added delay
	sec = base = Sys_Milliseconds();
	while (sec - base < 200)
		sec = Sys_Milliseconds();
	// end delay

	S_Shutdown();

	// Added delay
	sec = base = Sys_Milliseconds();
	while (sec - base < 200)
		sec = Sys_Milliseconds();
	// end delay

	IN_Shutdown();
	VID_Shutdown();
	Con_Shutdown(); //mxd
}