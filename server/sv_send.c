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
// sv_main.c -- server main program

#include "server.h"

#pragma region ======================= Com_Printf redirection

void SV_FlushRedirect(int sv_redirected, char *outputbuf)
{
	if (sv_redirected == RD_PACKET)
	{
		Netchan_OutOfBandPrint(NS_SERVER, net_from, "print\n%s", outputbuf);
	}
	else if (sv_redirected == RD_CLIENT)
	{
		MSG_WriteByte(&sv_client->netchan.message, svc_print);
		MSG_WriteByte(&sv_client->netchan.message, PRINT_HIGH);
		MSG_WriteString(&sv_client->netchan.message, outputbuf);
	}
}

#pragma endregion

#pragma region ======================= EVENT MESSAGES

// Sends text across to be displayed if the level passes
void SV_ClientPrintf(client_t *cl, int level, char *fmt, ...)
{
	va_list	argptr;
	static char string[1024]; //mxd. +static
	
	if (level < cl->messagelevel)
		return;
	
	va_start(argptr, fmt);
	Q_vsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);
	
	MSG_WriteByte(&cl->netchan.message, svc_print);
	MSG_WriteByte(&cl->netchan.message, level);
	MSG_WriteString(&cl->netchan.message, string);
}

// Sends text to all active clients
void SV_BroadcastPrintf(int level, char *fmt, ...)
{
	va_list argptr;
	static char string[2048]; //mxd. +static

	va_start(argptr, fmt);
	Q_vsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);
	
	// Echo to console
	if (dedicated->value)
	{
		char copy[1024];
		int c;
		
		// Mask off high bits
		for (c = 0; c < 1023 && string[c]; c++)
			copy[c] = string[c] & 127;

		copy[c] = 0;
		Com_Printf("%s", copy);
	}

	client_t *cl = svs.clients;
	for (int i = 0; i < maxclients->value; i++, cl++)
	{
		if (level < cl->messagelevel || cl->state != cs_spawned)
			continue;

		MSG_WriteByte(&cl->netchan.message, svc_print);
		MSG_WriteByte(&cl->netchan.message, level);
		MSG_WriteString(&cl->netchan.message, string);
	}
}

// Sends text to all active clients
void SV_BroadcastCommand(char *fmt, ...)
{
	va_list	argptr;
	static char string[1024]; //mxd. +static
	
	if (!sv.state)
		return;

	va_start(argptr, fmt);
	Q_vsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);

	MSG_WriteByte(&sv.multicast, svc_stufftext);
	MSG_WriteString(&sv.multicast, string);
	SV_Multicast(NULL, MULTICAST_ALL_R);
}

// Sends the contents of sv.multicast to a subset of the clients, then clears sv.multicast.
// MULTICAST_ALL - same as broadcast (origin can be NULL)
// MULTICAST_PVS - send to clients potentially visible from org
// MULTICAST_PHS - send to clients potentially hearable from org
void SV_Multicast(vec3_t origin, multicast_t to)
{
	byte *mask;
	int leafnum, cluster;
	int area1;

	qboolean reliable = false;

	if (to != MULTICAST_ALL_R && to != MULTICAST_ALL)
	{
		leafnum = CM_PointLeafnum(origin);
		area1 = CM_LeafArea(leafnum);
	}
	else
	{
		area1 = 0;
	}

	// If doing a serverrecord, store everything
	if (svs.demofile)
		SZ_Write(&svs.demo_multicast, sv.multicast.data, sv.multicast.cursize);
	
	switch (to)
	{
		case MULTICAST_ALL_R:
			reliable = true;	// Intentional fallthrough
		case MULTICAST_ALL:
			mask = NULL;
			break;

		case MULTICAST_PHS_R:
			reliable = true;	// Intentional fallthrough
		case MULTICAST_PHS:
			leafnum = CM_PointLeafnum(origin);
			cluster = CM_LeafCluster(leafnum);
			mask = CM_ClusterPHS(cluster);
			break;

		case MULTICAST_PVS_R:
			reliable = true;	// Intentional fallthrough
		case MULTICAST_PVS:
			leafnum = CM_PointLeafnum(origin);
			cluster = CM_LeafCluster(leafnum);
			mask = CM_ClusterPVS(cluster);
			break;

		default:
			mask = NULL;
			Com_Error(ERR_FATAL, "SV_Multicast: bad to:%i", to);
			break;
	}

	// Send the data to all relevent clients
	client_t* client = svs.clients;
	for (int i = 0; i < maxclients->value; i++, client++)
	{
		if (client->state == cs_free || client->state == cs_zombie || (client->state != cs_spawned && !reliable))
			continue;

		if (mask)
		{
			leafnum = CM_PointLeafnum(client->edict->s.origin);
			cluster = CM_LeafCluster(leafnum);
			const int area2 = CM_LeafArea(leafnum);

			if (!CM_AreasConnected(area1, area2))
				continue;
			if (!(mask[cluster >> 3] & 1 << (cluster & 7)))
				continue;
		}

		if (reliable)
			SZ_Write(&client->netchan.message, sv.multicast.data, sv.multicast.cursize);
		else
			SZ_Write(&client->datagram, sv.multicast.data, sv.multicast.cursize);
	}

	SZ_Clear(&sv.multicast);
}

// Each entity can have eight independant sound sources, like voice, weapon, feet, etc.
// If cahnnel & 8, the sound will be sent to everyone, not just things in the PHS.

// FIXME: if entity isn't in PHS, they must be forced to be sent or have the origin explicitly sent.

// Channel 0 is an auto-allocate channel, the others override anything already running on that entity/channel pair.

// An attenuation of 0 will play full volume everywhere in the level.
// Larger attenuations will drop off (max 4 attenuation).

// Timeofs can range from 0.0 to 0.1 to cause sounds to be started later in the frame than they normally would.

// If origin is NULL, the origin is determined from the entity origin or the midpoint of the entity box for bmodels.
void SV_StartSound(vec3_t origin, edict_t *entity, int channel, int soundindex, float volume, float attenuation, float timeofs)
{
	vec3_t origin_v;
	
	if (volume < 0 || volume > 1.0)
		Com_Error(ERR_FATAL, "SV_StartSound: volume = %f", volume);

	if (attenuation < 0 || attenuation > 4)
		Com_Error(ERR_FATAL, "SV_StartSound: attenuation = %f", attenuation);

	if (timeofs < 0 || timeofs > 0.255f)
		Com_Error(ERR_FATAL, "SV_StartSound: timeofs = %f", timeofs);

	const int ent = NUM_FOR_EDICT(entity);
	qboolean use_phs = !(channel & 8); // No PHS flag
	
	if(!use_phs)
		channel &= 7;

	const int sendchan = (ent << 3) | (channel & 7);

	int flags = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		flags |= SND_VOLUME;

	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		flags |= SND_ATTENUATION;

	// The client doesn't know that bmodels have weird origins
	// The origin can also be explicitly set
	if (entity->svflags & SVF_NOCLIENT || entity->solid == SOLID_BSP || origin)
		flags |= SND_POS;

	// Always send the entity number for channel overrides
	flags |= SND_ENT;

	if (timeofs)
		flags |= SND_OFFSET;

	// Use the entity origin unless it is a bmodel or explicitly specified
	if (!origin)
	{
		origin = origin_v;
		if (entity->solid == SOLID_BSP)
		{
			for (int i = 0; i < 3; i++)
				origin_v[i] = entity->s.origin[i] + 0.5f * (entity->mins[i] + entity->maxs[i]);
		}
		else
		{
			VectorCopy(entity->s.origin, origin_v);
		}
	}

	MSG_WriteByte(&sv.multicast, svc_sound);
	MSG_WriteByte(&sv.multicast, flags);

	//Knightmare- 12/23/2001- changed to short
	MSG_WriteShort(&sv.multicast, soundindex);
		
	if (flags & SND_VOLUME)
		MSG_WriteByte(&sv.multicast, volume * 255);
	if (flags & SND_ATTENUATION)
		MSG_WriteByte(&sv.multicast, attenuation * 64);
	if (flags & SND_OFFSET)
		MSG_WriteByte(&sv.multicast, timeofs * 1000);

	if (flags & SND_ENT)
		MSG_WriteShort(&sv.multicast, sendchan);

	if (flags & SND_POS)
		MSG_WritePos(&sv.multicast, origin);

	// If the sound doesn't attenuate, send it to everyone (global radio chatter, voiceovers, etc)
	if (attenuation == ATTN_NONE)
		use_phs = false;

	if (channel & CHAN_RELIABLE)
		SV_Multicast(origin, (use_phs ? MULTICAST_PHS_R : MULTICAST_ALL_R));
	else
		SV_Multicast(origin, (use_phs ? MULTICAST_PHS : MULTICAST_ALL));
}

#pragma endregion

#pragma region ======================= FRAME UPDATES

void SV_SendClientDatagram(client_t *client)
{
	static byte msg_buf[MAX_MSGLEN]; //mxd. +static
	sizebuf_t msg;

	SV_BuildClientFrame(client);

	SZ_Init(&msg, msg_buf, sizeof(msg_buf));

	// Knightmare- limit message size to 2800 for non-local clients in multiplayer
	if (maxclients->value > 1 && client->netchan.remote_address.type != NA_LOOPBACK && sv_limit_msglen->integer != 0)
		msg.maxsize = MAX_MSGLEN_MP;

	msg.allowoverflow = true;

	// Send over all the relevant entity_state_t and the player_state_t
	SV_WriteFrameToClient(client, &msg);

	// Copy the accumulated multicast datagram for this client out to the message.
	// It is necessary for this to be after the WriteEntities so that entity references will be current
	if (client->datagram.overflowed)
		Com_Printf(S_COLOR_YELLOW"WARNING: datagram overflowed for %s\n", client->name);
	else
		SZ_Write(&msg, client->datagram.data, client->datagram.cursize);
	SZ_Clear(&client->datagram);

	if (msg.overflowed)
	{
		// Must have room left for the packet header
		Com_Printf(S_COLOR_YELLOW"WARNING: msg overflowed for %s\n", client->name);
		SZ_Clear(&msg);
	}

	// Send the datagram
	Netchan_Transmit(&client->netchan, msg.cursize, msg.data);

	// Record the size for rate estimation
	client->message_size[sv.framenum % RATE_MESSAGES] = msg.cursize;
}

void SV_DemoCompleted()
{
	if (sv.demofile)
	{
		FS_FCloseFile(sv.demofile);
		sv.demofile = 0; // Clear the file handle
	}

	SV_Nextserver();
}

// Returns true if the client is over its current bandwidth estimation and should not be sent another packet.
qboolean SV_RateDrop(client_t *c)
{
	// Never drop over the loopback
	if (c->netchan.remote_address.type == NA_LOOPBACK)
		return false;

	int total = 0;

	for (int i = 0; i < RATE_MESSAGES; i++)
		total += c->message_size[i];

	if (total > c->rate)
	{
		c->surpressCount++;
		c->message_size[sv.framenum % RATE_MESSAGES] = 0;

		return true;
	}

	return false;
}

void SV_SendClientMessages()
{
	static byte msgbuf[MAX_MSGLEN]; //mxd. +static
	int msglen = 0;

	// Read the next demo message if needed
	if (sv.state == ss_demo && sv.demofile && !sv_paused->integer)
	{
		// Get the next message
		int r = FS_Read(&msglen, 4, sv.demofile);
		if (r != 4)
		{
			SV_DemoCompleted();
			return;
		}

		if (msglen == -1)
		{
			SV_DemoCompleted();
			return;
		}

		if (msglen > MAX_MSGLEN)
			Com_Error(ERR_DROP, "SV_SendClientMessages: msglen > MAX_MSGLEN");

		r = FS_Read(msgbuf, msglen, sv.demofile);
		if (r != msglen)
		{
			SV_DemoCompleted();
			return;
		}
	}

	// Send a message to each connected client
	client_t *c = svs.clients;
	for (int i = 0; i < maxclients->value; i++, c++)
	{
		if (!c->state)
			continue;

		// If the reliable message overflowed, drop the client
		if (c->netchan.message.overflowed)
		{
			SZ_Clear(&c->netchan.message);
			SZ_Clear(&c->datagram);
			SV_BroadcastPrintf(PRINT_HIGH, "%s overflowed\n", c->name);
			SV_DropClient(c);
		}

		if (sv.state == ss_cinematic || sv.state == ss_demo || sv.state == ss_pic)
		{
			Netchan_Transmit(&c->netchan, msglen, msgbuf);
		}
		else if (c->state == cs_spawned)
		{
			// Don't overrun bandwidth
			if (SV_RateDrop(c))
				continue;

			SV_SendClientDatagram(c);
		}
		else
		{
			// Just update reliable	if needed
			if (c->netchan.message.cursize || curtime - c->netchan.last_sent > 1000)
				Netchan_Transmit(&c->netchan, 0, NULL);
		}
	}
}

#pragma endregion