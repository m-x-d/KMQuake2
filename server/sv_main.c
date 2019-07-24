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

#include "server.h"

netadr_t master_adr[MAX_MASTERS]; // Address of group servers

#pragma region ======================= Server cvars

client_t *sv_client; // Current client

cvar_t *sv_paused;
cvar_t *sv_timedemo;

cvar_t *sv_enforcetime;

cvar_t *timeout; // Seconds without any message
cvar_t *zombietime; // Seconds to sink messages after disconnect

cvar_t *rcon_password; // Password for remote server commands

cvar_t *allow_download;
cvar_t *allow_download_players;
cvar_t *allow_download_models;
cvar_t *allow_download_sounds;
cvar_t *allow_download_maps;
cvar_t *allow_download_pics;
cvar_t *allow_download_textures;
cvar_t *allow_download_textures_24bit; // Knightmare- whether to allow downloading 24-bit textures

cvar_t *sv_downloadserver; // From R1Q2

cvar_t *sv_baselines_maxlen; // Knightmare- max packet size for connect messages
cvar_t *sv_limit_msglen; // Knightmare- whether to use MAX_MSGLEN_MP for multiplayer games

cvar_t *sv_airaccelerate;

cvar_t *sv_noreload; // Don't reload level state when reentering

cvar_t *maxclients; // FIXME: rename sv_maxclients
cvar_t *sv_showclamp;

cvar_t *hostname;
cvar_t *public_server; // Should heartbeats be sent

cvar_t *sv_iplimit; // r1ch: max connections from a single IP (prevent DoS)
cvar_t *sv_reconnect_limit; // Minimum seconds between connect messages
cvar_t *sv_entfile; // Whether to use .ent file

cvar_t *musictrackframe; //mxd. Ogg music track frame. Set when saving game, used when loading game

#pragma endregion

#pragma region ======================= Drop / clean client

// Called when the player is totally leaving the server, either willingly or unwillingly.
// This is NOT called if the entire server is quiting or crashing.
void SV_DropClient(client_t *drop)
{
	// Add the disconnect message
	MSG_WriteByte(&drop->netchan.message, svc_disconnect);

	if (drop->state == cs_spawned)
	{
		// Call the prog function for removing a client. This will remove the body, among other things.
		ge->ClientDisconnect(drop->edict);
	}

	if (drop->download)
	{
		FS_FreeFile(drop->download);
		drop->download = NULL;
	}

	// r1ch: fix for mods that don't clean score
	if (drop->edict && drop->edict->client)
		drop->edict->client->ps.stats[STAT_FRAGS] = 0;

	drop->state = cs_zombie; // Become free in a few seconds
	drop->name[0] = 0;
}

//Knightmare added. Given an netadr_t, returns the matching client.
static client_t *SV_GetClientFromAdr(const netadr_t address)
{
	for (int i = 0; i < maxclients->integer; i++)
	{
		client_t *cl = &svs.clients[i];
		if (NET_CompareBaseAdr(cl->netchan.remote_address, address))
			return cl;
	}

	// Don't return non-matching client
	return NULL;
}

//Knightmare added. Calls SV_DropClient, takes netadr_t instead of client pointer.
void SV_DropClientFromAdr(const netadr_t address)
{
	// Adapted Pat Aftermoon's simplified version of this
	client_t *drop = SV_GetClientFromAdr(address);
	
	// Make sure we have a client to drop
	if (drop)
	{
		SV_BroadcastPrintf(PRINT_HIGH, "Dropping client %s.\n", drop->name);
		SV_DropClient(drop);

		drop->state = cs_free; // Don't bother with zombie state 
	}
}

// From R1Q2. r1ch: this does the final cleaning up of a client after zombie state.
static void SV_CleanClient(client_t *drop)
{
	if (drop->download)
	{
		Z_Free(drop->download);
		drop->download = NULL;
	}
}

#pragma endregion

#pragma region ======================= Connectionless commands

// Builds the string that is sent as heartbeats and status replies
static char *SV_StatusString()
{
	static char	status[MAX_MSGLEN - 16];

	Q_strncpyz(status, Cvar_Serverinfo(), sizeof(status));
	Q_strncatz(status, "\n", sizeof(status));
	int statuslength = strlen(status);

	for (int i = 0; i < maxclients->integer; i++)
	{
		client_t *cl = &svs.clients[i];
		if (cl->state == cs_connected || cl->state == cs_spawned)
		{
			char player[1024];
			Com_sprintf(player, sizeof(player), "%i %i \"%s\"\n", cl->edict->client->ps.stats[STAT_FRAGS], cl->ping, cl->name);

			const int playerlength = strlen(player);
			if (statuslength + playerlength >= sizeof(status))
				break; // Can't hold any more

			Q_strncpyz(status + statuslength, player, sizeof(status));
			statuslength += playerlength;
		}
	}

	return status;
}

// Responds with all the info that qplug or qspy can see
static void SVC_Status()
{
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "print\n%s", SV_StatusString());
}

static void SVC_Ack()
{
	Com_Printf("Ping acknowledge from %s\n", NET_AdrToString(net_from));
}

// Responds with short info for broadcast scans.
// The second parameter should be the current protocol version number.
static void SVC_Info()
{
	if (maxclients->integer == 1)
		return; // Ignore in single player

	const int version = atoi(Cmd_Argv(1));

	if (version != PROTOCOL_VERSION)
		return; // According to r1ch, this can be used to make servers endlessly ping each other

	int count = 0;
	for (int i = 0; i < maxclients->integer; i++)
		if (svs.clients[i].state >= cs_connected)
			count++;

	char string[64];
	Com_sprintf(string, sizeof(string), "%16s %8s %2i/%2i\n", hostname->string, sv.name, count, maxclients->integer);
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "info\n%s", string);
}

// Just responds with an acknowledgement
static void SVC_Ping()
{
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "ack");
}

// Returns a challenge number that can be used in a subsequent client_connect command.
// We do this to prevent denial of service attacks that flood the server with invalid connection IPs.
// With a challenge, they must give a valid IP address.
static void SVC_GetChallenge()
{
	int oldest = 0;
	int oldesttime = 0x7fffffff;

	// See if we already have a challenge for this ip
	int i;
	for (i = 0; i < MAX_CHALLENGES; i++)
	{
		if (NET_CompareBaseAdr(net_from, svs.challenges[i].adr))
			break;

		if (svs.challenges[i].time < oldesttime)
		{
			oldesttime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if (i == MAX_CHALLENGES)
	{
		// Overwrite the oldest
		svs.challenges[oldest].challenge = rand() & 0x7fff;
		svs.challenges[oldest].adr = net_from;
		svs.challenges[oldest].time = curtime;
		i = oldest;
	}

	// Send it back
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "challenge %i", svs.challenges[i].challenge);
}

// A connection request that did not come from the master
static void SVC_DirectConnect()
{
	const netadr_t adr = net_from;

	Com_DPrintf("SVC_DirectConnect()\n");

	const int version = atoi(Cmd_Argv(1));
	if (version != PROTOCOL_VERSION)
	{
		Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nServer is version %4.2f.\n", VERSION);
		Com_DPrintf("    rejected connect from version %i\n", version);

		return;
	}

	const int qport = atoi(Cmd_Argv(2));
	const int challenge = atoi(Cmd_Argv(3));

	// r1ch: limit connections from a single IP
	int previousclients = 0;
	client_t *cl = svs.clients;
	for (int i = 0; i < maxclients->integer; i++, cl++)
	{
		if (cl->state == cs_free)
			continue;

		if (NET_CompareBaseAdr(adr, cl->netchan.remote_address))
		{
			// r1ch: zombies are less dangerous
			if (cl->state == cs_zombie)
				previousclients++;
			else
				previousclients += 2;
		}
	}

	if (previousclients >= sv_iplimit->integer * 2)
	{
		Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nToo many connections from your host.\n");
		Com_DPrintf("    too many connections\n");

		return;
	}
	// end r1ch fix

	char userinfo[MAX_INFO_STRING];
	strncpy(userinfo, Cmd_Argv(4), sizeof(userinfo) - 1);
	userinfo[sizeof(userinfo) - 1] = 0;

	// Force the IP key/value pair so the game can filter based on ip
	Info_SetValueForKey(userinfo, "ip", NET_AdrToString(net_from));

	// Attractloop servers are ONLY for local clients
	if (sv.attractloop && !NET_IsLocalAddress(adr))
	{
		Com_Printf("Remote connect in attract loop. Ignored.\n");
		Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nConnection refused.\n");

		return;
	}

	// See if the challenge is valid
	if (!NET_IsLocalAddress(adr))
	{
		int c;
		for (c = 0; c < MAX_CHALLENGES; c++)
		{
			if (NET_CompareBaseAdr(net_from, svs.challenges[c].adr))
			{
				if (challenge == svs.challenges[c].challenge)
					break; // Good

				Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nBad challenge.\n");

				return;
			}
		}

		if (c == MAX_CHALLENGES)
		{
			Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nNo challenge for address.\n");
			return;
		}
	}

	client_t temp;
	client_t *newcl = &temp;
	memset(newcl, 0, sizeof(client_t));

	// If there is already a slot for this ip, reuse it
	qboolean gotnewclient = false; //mxd
	cl = svs.clients;
	for (int i = 0; i < maxclients->integer; i++, cl++)
	{
		if (cl->state == cs_free)
			continue;

		if (NET_CompareBaseAdr(adr, cl->netchan.remote_address)	&& (cl->netchan.qport == qport || adr.port == cl->netchan.remote_address.port))
		{
			if (!NET_IsLocalAddress(adr) && svs.realtime - cl->lastconnect < sv_reconnect_limit->integer * 1000)
			{
				Com_DPrintf("%s:reconnect rejected: too soon\n", NET_AdrToString(adr));
				return;
			}

			// r1ch: !! fix nasty bug where non-disconnected clients (from dropped disconnect packets) could be overwritten!
			if (cl->state != cs_zombie)
			{
				Com_DPrintf("    client already found\n");
				// If we legitly get here, spoofed udp isn't possible (passed challenge) and client addr/port combo
				// is exactly the same, so we can assume its really a dropped/crashed client. I hope...
				Com_Printf("Dropping %s, ghost reconnect\n", cl->name);
				SV_DropClient(cl);
			}
			// end r1ch fix

			Com_Printf("%s:reconnect\n", NET_AdrToString(adr));

			SV_CleanClient(cl); // r1ch: clean up last client data

			newcl = cl;
			gotnewclient = true;
			break;
		}
	}

	if (!gotnewclient)
	{
		// Find a client slot
		newcl = NULL;
		cl = svs.clients;
		for (int i = 0; i < maxclients->integer; i++, cl++)
		{
			if (cl->state == cs_free)
			{
				newcl = cl;
				break;
			}
		}

		if (!newcl)
		{
			Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nServer is full.\n");
			Com_DPrintf("Rejected a connection.\n");
			return;
		}
	}
	
	// Build a new connection. Accept the new client. This is the only place a client_t is ever initialized.
	*newcl = temp;
	sv_client = newcl;
	const int edictnum = newcl - svs.clients + 1;
	edict_t *ent = EDICT_NUM(edictnum);
	newcl->edict = ent;
	newcl->challenge = challenge; // Save challenge for checksumming

	// Get the game a chance to reject this connection or modify the userinfo
	if (!ge->ClientConnect(ent, userinfo))
	{
		if (*Info_ValueForKey(userinfo, "rejmsg")) 
			Netchan_OutOfBandPrint(NS_SERVER, adr, "print\n%s\nConnection refused.\n", Info_ValueForKey(userinfo, "rejmsg"));
		else
			Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nConnection refused.\n");

		Com_DPrintf("Game rejected a connection.\n");

		return;
	}

	// Parse some info from the info strings
	strncpy(newcl->userinfo, userinfo, sizeof(newcl->userinfo) - 1);
	SV_UserinfoChanged(newcl);

	// Send the connect packet to the client
	// r1ch: note we could ideally send this twice but it prints unsightly message on original client.
	if (sv_downloadserver->string[0])
		Netchan_OutOfBandPrint(NS_SERVER, adr, "client_connect dlserver=%s", sv_downloadserver->string);
	else
		Netchan_OutOfBandPrint(NS_SERVER, adr, "client_connect");

	Netchan_Setup(NS_SERVER, &newcl->netchan , adr, qport);

	newcl->state = cs_connected;
	
	SZ_Init(&newcl->datagram, newcl->datagram_buf, sizeof(newcl->datagram_buf));
	newcl->datagram.allowoverflow = true;
	newcl->lastmessage = svs.realtime; // Don't timeout
	newcl->lastconnect = svs.realtime;
}

static int Rcon_Validate()
{
	if (!strlen(rcon_password->string) || strcmp(Cmd_Argv(1), rcon_password->string))
		return 0;

	return 1;
}

#define SV_OUTPUTBUF_LENGTH (MAX_MSGLEN - 16)

// A client issued an rcon command. Shift down the remaining args. Redirect all printfs.
static void SVC_RemoteCommand()
{
	static char sv_outputbuf[SV_OUTPUTBUF_LENGTH]; //mxd. Made local
	
	if (!Rcon_Validate())
		Com_Printf("Bad rcon from %s:\n%s\n", NET_AdrToString(net_from), net_message.data + 4);
	else
		Com_Printf("Rcon from %s:\n%s\n", NET_AdrToString(net_from), net_message.data + 4);

	Com_BeginRedirect(RD_PACKET, sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);

	if (!Rcon_Validate())
	{
		Com_Printf("Bad rcon_password.\n");
	}
	else
	{
		char remaining[1024] = { 0 };

		for (int i = 2; i < Cmd_Argc(); i++)
		{
			strcat(remaining, Cmd_Argv(i));
			strcat(remaining, " ");
		}

		Cmd_ExecuteString(remaining);
	}

	Com_EndRedirect();
}

// A connectionless packet has four leading 0xff characters to distinguish it from a game channel.
// Clients that are in the game can still send connectionless packets.
static void SV_ConnectionlessPacket()
{
	MSG_BeginReading(&net_message);
	MSG_ReadLong(&net_message); // Skip the -1 marker

	char *s = MSG_ReadStringLine(&net_message);
	Cmd_TokenizeString(s, false);

	char *c = Cmd_Argv(0);
	Com_DPrintf("Packet %s : %s\n", NET_AdrToString(net_from), c);

	if (!strcmp(c, "ping"))
		SVC_Ping();
	else if (!strcmp(c, "ack"))
		SVC_Ack();
	else if (!strcmp(c, "status"))
		SVC_Status();
	else if (!strcmp(c, "info"))
		SVC_Info();
	else if (!strcmp(c, "getchallenge"))
		SVC_GetChallenge();
	else if (!strcmp(c, "connect"))
		SVC_DirectConnect();
	else if (!strcmp(c, "rcon"))
		SVC_RemoteCommand();
	else
		Com_Printf("Bad connectionless packet from %s:\n%s\n", NET_AdrToString(net_from), s);
}

#pragma endregion

#pragma region ======================= SV_Frame and its support logic

// Updates the cl->ping variables
static void SV_CalcPings()
{
	for (int i = 0; i < maxclients->integer; i++)
	{
		client_t *cl = &svs.clients[i];
		if (cl->state != cs_spawned)
			continue;

		int total = 0;
		int count = 0;
		for (int j = 0; j < LATENCY_COUNTS; j++)
		{
			if (cl->frame_latency[j] > 0)
			{
				count++;
				total += cl->frame_latency[j];
			}
		}

		cl->ping = (count ? total / count : 0);
		cl->edict->client->ping = cl->ping; // Let the game dll know about the ping
	}
}

// Every few frames, gives all clients an allotment of milliseconds for their command moves. If they exceed it, assume cheating.
static void SV_GiveMsec()
{
	if (sv.framenum & 15)
		return;

	for (int i = 0; i < maxclients->integer; i++)
	{
		client_t *cl = &svs.clients[i];
		if (cl->state != cs_free)
			cl->commandMsec = 1800;	// 1600 + some slop
	}
}

static void SV_ReadPackets()
{
	while (NET_GetPacket(NS_SERVER, &net_from, &net_message))
	{
		// Check for connectionless packet (0xffffffff) first
		if (*(int *)net_message.data == -1)
		{
			SV_ConnectionlessPacket();
			continue;
		}

		// Read the qport out of the message so we can fix up stupid address translating routers
		MSG_BeginReading(&net_message);
		MSG_ReadLong(&net_message); // Sequence number
		MSG_ReadLong(&net_message); // Sequence number
		const int qport = MSG_ReadShort(&net_message) & 0xffff;

		// Check for packets from connected clients
		client_t *cl = svs.clients;
		for (int i = 0; i < maxclients->integer; i++, cl++)
		{
			if (cl->state == cs_free || cl->netchan.qport != qport || !NET_CompareBaseAdr(net_from, cl->netchan.remote_address))
				continue;

			if (cl->netchan.remote_address.port != net_from.port)
			{
				Com_Printf("SV_ReadPackets: fixing up a translated port\n");
				cl->netchan.remote_address.port = net_from.port;
			}

			if (Netchan_Process(&cl->netchan, &net_message))
			{
				// This is a valid, sequenced packet, so process it
				if (cl->state != cs_zombie)
				{
					cl->lastmessage = svs.realtime;	// Don't timeout
					SV_ExecuteClientMessage(cl);
				}
			}

			break;
		}
	}
}

// If a packet has not been received from a client for timeout->value seconds, drop the conneciton.
// Server frames are used instead of realtime to avoid dropping the local client while debugging.
// When a client is normally dropped, the client_t goes into a zombie state for a few seconds to make sure any final reliable message gets resent if necessary.
static void SV_CheckTimeouts()
{
	const int droppoint = svs.realtime - 1000 * timeout->value;
	const int zombiepoint = svs.realtime - 1000 * zombietime->value;

	client_t *cl = svs.clients;
	for (int i = 0; i < maxclients->integer; i++, cl++)
	{
		// Message times may be wrong across a changelevel
		cl->lastmessage = min(svs.realtime, cl->lastmessage);

		if (cl->state == cs_zombie && cl->lastmessage < zombiepoint)
		{
			SV_CleanClient(cl); // r1ch fix: make sure client is cleaned up
			cl->state = cs_free; // Can now be reused

			continue;
		}

		if ((cl->state == cs_connected || cl->state == cs_spawned) && cl->lastmessage < droppoint && cl->netchan.remote_address.type != NA_LOOPBACK) //mxd. Local client should never timeout
		{
			// r1ch fix: only message if they spawned (less spam plz)
			if (cl->state == cs_spawned && cl->name[0])
				SV_BroadcastPrintf(PRINT_HIGH, "%s timed out\n", cl->name);

			SV_DropClient(cl);
			cl->state = cs_free; // Don't bother with zombie state
		}
	}
}

// This has to be done before the world logic, because player processing happens outside RunWorldFrame
static void SV_PrepWorldFrame()
{
	for (int i = 0; i < ge->num_edicts; i++)
	{
		edict_t *ent = EDICT_NUM(i);
		ent->s.event = 0; // Events only last for a single message
	}
}

static void SV_RunGameFrame()
{
	if (host_speeds->integer)
		time_before_game = Sys_Milliseconds();

	// We always need to bump framenum, even if we don't run the world, otherwise the delta
	// compression can get confused when a client has the "current" frame.
	sv.framenum++;
	sv.time = sv.framenum * 100;

	// Don't run if paused
	if (!sv_paused->integer || maxclients->integer > 1)
	{
		ge->RunFrame();

		// Never get more than one tic behind
		if (sv.time < svs.realtime)
		{
			if (sv_showclamp->integer)
				Com_Printf("sv highclamp\n");

			svs.realtime = sv.time;
		}
	}

	if (host_speeds->integer)
		time_after_game = Sys_Milliseconds();
}

void SV_Frame(const int msec)
{
	time_before_game = 0;
	time_after_game = 0;

	// If server is not active, do nothing
	if (!svs.initialized)
		return;

	svs.realtime += msec;

	// Keep the random time dependent
	rand();

	// Check timeouts
	SV_CheckTimeouts();

	// Get packets from clients
	SV_ReadPackets();

	// Move autonomous things around if enough time has passed
	if (!sv_timedemo->integer && svs.realtime < sv.time)
	{
		// Never let the time get too far off
		if (sv.time - svs.realtime > 100)
		{
			if (sv_showclamp->integer)
				Com_Printf("sv lowclamp\n");

			svs.realtime = sv.time - 100;
		}

		NET_Sleep(sv.time - svs.realtime);

		return;
	}

	// Update ping based on the last known frame from all clients
	SV_CalcPings();

	// Give the clients some timeslices
	SV_GiveMsec();

	// Let everything in the world think and move
	SV_RunGameFrame();

	// Send messages back to the clients that had packets read this frame
	SV_SendClientMessages();

	// Save the entire world state if recording a serverdemo
	SV_RecordDemoMessage();

	// Send a heartbeat to the master if needed
	Master_Heartbeat();

	// Clear teleport flags, etc for next frame
	SV_PrepWorldFrame();
}

#pragma endregion

#pragma region ======================= Master heartbeat / shutdown

#define	HEARTBEAT_SECONDS	300

// Send a message to the master every few minutes to let it know we are alive, and log information
void Master_Heartbeat()
{
	// pgm post 3.19 change, cvar pointer not validated before dereferencing
	if (!dedicated || !dedicated->integer)
		return; // Only dedicated servers send heartbeats

	// pgm post 3.19 change, cvar pointer not validated before dereferencing
	if (!public_server || !public_server->integer)
		return; // A private dedicated game

	// Check for time wraparound
	svs.last_heartbeat = min(svs.realtime, svs.last_heartbeat);

	if (svs.realtime - svs.last_heartbeat < HEARTBEAT_SECONDS * 1000)
		return; // Not time to send yet

	svs.last_heartbeat = svs.realtime;

	// Send the same string that we would give for a status OOB command
	char *string = SV_StatusString();

	// Send to group master
	for (int i = 0; i < MAX_MASTERS; i++)
	{
		if (master_adr[i].port)
		{
			Com_Printf("Sending heartbeat to %s.\n", NET_AdrToString(master_adr[i]));
			Netchan_OutOfBandPrint(NS_SERVER, master_adr[i], "heartbeat\n%s", string);
		}
	}
}

// Informs all masters that this server is going down
void Master_Shutdown()
{
	// pgm post3.19 change, cvar pointer not validated before dereferencing
	if (!dedicated || !dedicated->integer)
		return; // Only dedicated servers send heartbeats

	// pgm post3.19 change, cvar pointer not validated before dereferencing
	if (!public_server || !public_server->integer)
		return; // A private dedicated game

	// Send to group master
	for (int i = 0; i < MAX_MASTERS; i++)
	{
		if (master_adr[i].port)
		{
			if (i > 0)
				Com_Printf("Sending heartbeat to %s.\n", NET_AdrToString(master_adr[i]));

			Netchan_OutOfBandPrint(NS_SERVER, master_adr[i], "shutdown");
		}
	}
}

#pragma endregion

#pragma region ======================= Server init / shutdown

// Only called at quake2.exe startup, not for each game
void SV_Init()
{
	SV_InitOperatorCommands();

	rcon_password = Cvar_Get("rcon_password", "", 0);
	Cvar_Get("skill", "1", 0);
	Cvar_Get("deathmatch", "0", CVAR_LATCH);
	Cvar_Get("coop", "0", CVAR_LATCH);
	Cvar_Get("dmflags", va("%i", DF_INSTANT_ITEMS), CVAR_SERVERINFO);
	Cvar_Get("fraglimit", "0", CVAR_SERVERINFO);
	Cvar_Get("timelimit", "0", CVAR_SERVERINFO);
	Cvar_Get("cheats", "0", CVAR_SERVERINFO | CVAR_LATCH);
	Cvar_Get("protocol", va("%i", PROTOCOL_VERSION), CVAR_SERVERINFO | CVAR_NOSET);
	maxclients = Cvar_Get("maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH);
	hostname = Cvar_Get("hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE);
	timeout = Cvar_Get("timeout", "125", 0);
	zombietime = Cvar_Get("zombietime", "2", 0);
	sv_showclamp = Cvar_Get("showclamp", "0", 0);
	sv_paused = Cvar_Get("paused", "0", 0);
	sv_timedemo = Cvar_Get("timedemo", "0", 0);
	sv_enforcetime = Cvar_Get("sv_enforcetime", "0", 0);

	allow_download = Cvar_Get("allow_download", "1", CVAR_ARCHIVE);
	allow_download_players = Cvar_Get("allow_download_players", "0", CVAR_ARCHIVE);
	allow_download_models = Cvar_Get("allow_download_models", "1", CVAR_ARCHIVE);
	allow_download_sounds = Cvar_Get("allow_download_sounds", "1", CVAR_ARCHIVE);
	allow_download_maps = Cvar_Get("allow_download_maps", "1", CVAR_ARCHIVE);
	allow_download_pics = Cvar_Get("allow_download_pics", "1", CVAR_ARCHIVE);
	allow_download_textures = Cvar_Get("allow_download_textures", "1", CVAR_ARCHIVE);
	allow_download_textures_24bit = Cvar_Get("allow_download_textures_24bit", "0", CVAR_ARCHIVE); // Knightmare- whether to allow downloading 24-bit textures
	sv_downloadserver = Cvar_Get("sv_downloadserver", "", 0); // r1ch: http dl server

	sv_baselines_maxlen = Cvar_Get("sv_baselines_maxlen", "1200", 0); // Knightmare- max packet size for connect messages
	sv_limit_msglen = Cvar_Get("sv_limit_msglen", "1", 0); // Knightmare- whether to use MAX_MSGLEN_MP for multiplayer games

	sv_noreload = Cvar_Get("sv_noreload", "0", 0);
	sv_airaccelerate = Cvar_Get("sv_airaccelerate", "0", CVAR_LATCH);
	public_server = Cvar_Get("public", "0", 0);
	
	sv_iplimit = Cvar_Get("sv_iplimit", "3", 0); // r1ch: limit connections per ip address (stop zombie dos/flood)
	sv_reconnect_limit = Cvar_Get("sv_reconnect_limit", "3", CVAR_ARCHIVE);
	
	sv_entfile = Cvar_Get("sv_entfile", "1", CVAR_ARCHIVE); // Whether to use .ent file
	musictrackframe = Cvar_Get("musictrackframe", "-1", CVAR_LATCH); //mxd

	SZ_Init(&net_message, net_message_buffer, sizeof(net_message_buffer));
}

// Used by SV_Shutdown to send a final message to all connected clients before the server goes down.
// The messages are sent immediately, not just stuck on the outgoing message list, 
// because the server is going to totally exit after returning from this function.
static void SV_FinalMessage(const char *message, const qboolean reconnect)
{
	SZ_Clear(&net_message);
	MSG_WriteByte(&net_message, svc_print);
	MSG_WriteByte(&net_message, PRINT_HIGH);
	MSG_WriteString(&net_message, message);

	MSG_WriteByte(&net_message, (reconnect ? svc_reconnect : svc_disconnect));

	// Send it twice. Stagger the packets to crutch operating system limited buffers
	for(int c = 0; c < 2; c++)
	{
		client_t *cl = svs.clients;
		for (int i = 0; i < maxclients->integer; i++, cl++)
			if (cl->state >= cs_connected)
				Netchan_Transmit(&cl->netchan, net_message.cursize, net_message.data);
	}
}

// Called when each game quits, before Sys_Quit or Sys_Error
void SV_Shutdown(const char *finalmsg, const qboolean reconnect)
{
	if (svs.clients)
		SV_FinalMessage(finalmsg, reconnect);

	Master_Shutdown();
	SV_ShutdownGameProgs();

	// Free current level
	if (sv.demofile)
		FS_FCloseFile(sv.demofile);

	memset(&sv, 0, sizeof(sv));
	Com_SetServerState(sv.state);

	// Free server static data
	if (svs.clients)
		Z_Free(svs.clients);

	if (svs.client_entities)
		Z_Free(svs.client_entities);

	if (svs.demofile)
		fclose(svs.demofile);

	memset(&svs, 0, sizeof(svs));
}

#pragma endregion