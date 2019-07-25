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
// sv_ents.c

#include "server.h"

#pragma region ======================= Client frame onto the network channel encoding

// Writes a delta update of an entity_state_t list to the message.
static void SV_EmitPacketEntities(client_frame_t *from, client_frame_t *to, sizebuf_t *msg)
{
	entity_state_t *oldent, *newent;
	int oldnum, newnum;

	MSG_WriteByte(msg, svc_packetentities);

	const int from_num_entities = (from ? from->num_entities : 0);

	int newindex = 0;
	int oldindex = 0;
	while (newindex < to->num_entities || oldindex < from_num_entities)
	{
		if (newindex >= to->num_entities)
		{
			newnum = MAX_EDICTS + 1; //mxd. Was 9999
		}
		else
		{
			newent = &svs.client_entities[(to->first_entity + newindex) % svs.num_client_entities];
			newnum = newent->number;
		}

		if (oldindex >= from_num_entities)
		{
			oldnum = MAX_EDICTS + 1; //mxd. Was 9999
		}
		else
		{
			oldent = &svs.client_entities[(from->first_entity + oldindex) % svs.num_client_entities];
			oldnum = oldent->number;
		}

		if (newnum == oldnum)
		{
			// Delta update from old position.
			// Because the force parm is false, this will not result in any bytes being emited if the entity has not changed at all.
			// Note that players are always 'newentities', this updates their oldorigin always and prevents warping.
			MSG_WriteDeltaEntity(oldent, newent, msg, false, newent->number <= maxclients->value);
			oldindex++;
			newindex++;
		}
		else if (newnum < oldnum)
		{
			// This is a new entity, send it from the baseline
			MSG_WriteDeltaEntity(&sv.baselines[newnum], newent, msg, true, true);
			newindex++;
		}
		else // newnum > oldnum
		{
			// The old entity isn't present in the new message
			int bits = U_REMOVE;
			if (oldnum >= 256)
				bits |= U_NUMBER16 | U_MOREBITS1;

			MSG_WriteByte(msg,	bits & 255);
			if (bits & 0x0000ff00)
				MSG_WriteByte(msg, (bits >> 8) & 255);

			if (bits & U_NUMBER16)
				MSG_WriteShort(msg, oldnum);
			else
				MSG_WriteByte(msg, oldnum);

			oldindex++;
		}
	}

	MSG_WriteShort(msg, 0); // End of packetentities
}

static void SV_WritePlayerstateToClient(client_frame_t *from, client_frame_t *to, sizebuf_t *msg)
{
	player_state_t *ops;
	
	player_state_t *ps = &to->ps;
	if (!from)
	{
		player_state_t dummy;
		memset(&dummy, 0, sizeof(dummy));
		ops = &dummy;
	}
	else
	{
		ops = &from->ps;
	}

	// Determine what needs to be sent
	int pflags = 0;

	if (ps->pmove.pm_type != ops->pmove.pm_type)
		pflags |= PS_M_TYPE;

	if (ps->pmove.origin[0] != ops->pmove.origin[0] || ps->pmove.origin[1] != ops->pmove.origin[1] || ps->pmove.origin[2] != ops->pmove.origin[2])
		pflags |= PS_M_ORIGIN;

	if (ps->pmove.velocity[0] != ops->pmove.velocity[0] || ps->pmove.velocity[1] != ops->pmove.velocity[1] || ps->pmove.velocity[2] != ops->pmove.velocity[2])
		pflags |= PS_M_VELOCITY;

	if (ps->pmove.pm_time != ops->pmove.pm_time)
		pflags |= PS_M_TIME;

	if (ps->pmove.pm_flags != ops->pmove.pm_flags)
		pflags |= PS_M_FLAGS;

	if (ps->pmove.gravity != ops->pmove.gravity)
		pflags |= PS_M_GRAVITY;

	if (ps->pmove.delta_angles[0] != ops->pmove.delta_angles[0] || ps->pmove.delta_angles[1] != ops->pmove.delta_angles[1] || ps->pmove.delta_angles[2] != ops->pmove.delta_angles[2])
		pflags |= PS_M_DELTA_ANGLES;

	if (ps->viewoffset[0] != ops->viewoffset[0] || ps->viewoffset[1] != ops->viewoffset[1] || ps->viewoffset[2] != ops->viewoffset[2])
		pflags |= PS_VIEWOFFSET;

	if (ps->viewangles[0] != ops->viewangles[0] || ps->viewangles[1] != ops->viewangles[1] || ps->viewangles[2] != ops->viewangles[2])
		pflags |= PS_VIEWANGLES;

	if (ps->kick_angles[0] != ops->kick_angles[0] || ps->kick_angles[1] != ops->kick_angles[1] || ps->kick_angles[2] != ops->kick_angles[2])
		pflags |= PS_KICKANGLES;

	if (ps->blend[0] != ops->blend[0] || ps->blend[1] != ops->blend[1] || ps->blend[2] != ops->blend[2] || ps->blend[3] != ops->blend[3])
		pflags |= PS_BLEND;

	if (ps->fov != ops->fov)
		pflags |= PS_FOV;

	if (ps->rdflags != ops->rdflags)
		pflags |= PS_RDFLAGS;

	if (ps->gunframe != ops->gunframe)
		pflags |= PS_WEAPONFRAME;

#ifdef NEW_PLAYER_STATE_MEMBERS
	//Knightmare added
	if (ps->gunskin != ops->gunskin)
		pflags |= PS_WEAPONSKIN;

	if (ps->gunframe2 != ops->gunframe2)
		pflags |= PS_WEAPONFRAME2;

	if (ps->gunskin2 != ops->gunskin2)
		pflags |= PS_WEAPONSKIN2;

	// Server-side speed control!
	if (ps->maxspeed != ops->maxspeed)
		pflags |= PS_MAXSPEED;

	if (ps->duckspeed != ops->duckspeed)
		pflags |= PS_DUCKSPEED;

	if (ps->waterspeed != ops->waterspeed)
		pflags |= PS_WATERSPEED;

	if (ps->accel != ops->accel)
		pflags |= PS_ACCEL;

	if (ps->stopspeed != ops->stopspeed)
		pflags |= PS_STOPSPEED;
#endif	//end Knightmare

	pflags |= PS_WEAPONINDEX;
	pflags |= PS_WEAPONINDEX2; //Knightmare added

	// Write it
	MSG_WriteByte(msg, svc_playerinfo);
	MSG_WriteLong(msg, pflags); //Knightmare- write as long

	// Write the pmove_state_t
	if (pflags & PS_M_TYPE)
		MSG_WriteByte(msg, ps->pmove.pm_type);

	if (pflags & PS_M_ORIGIN) // FIXME- map size
	{
#ifdef LARGE_MAP_SIZE
		MSG_WritePMCoordNew(msg, ps->pmove.origin[0]);
		MSG_WritePMCoordNew(msg, ps->pmove.origin[1]);
		MSG_WritePMCoordNew(msg, ps->pmove.origin[2]);
#else
		MSG_WriteShort(msg, ps->pmove.origin[0]);
		MSG_WriteShort(msg, ps->pmove.origin[1]);
		MSG_WriteShort(msg, ps->pmove.origin[2]);
#endif
	}

	if (pflags & PS_M_VELOCITY)
	{
		MSG_WriteShort(msg, ps->pmove.velocity[0]);
		MSG_WriteShort(msg, ps->pmove.velocity[1]);
		MSG_WriteShort(msg, ps->pmove.velocity[2]);
	}

	if (pflags & PS_M_TIME)
		MSG_WriteByte(msg, ps->pmove.pm_time);

	if (pflags & PS_M_FLAGS)
		MSG_WriteByte(msg, ps->pmove.pm_flags);

	if (pflags & PS_M_GRAVITY)
		MSG_WriteShort(msg, ps->pmove.gravity);

	if (pflags & PS_M_DELTA_ANGLES)
	{
		MSG_WriteShort(msg, ps->pmove.delta_angles[0]);
		MSG_WriteShort(msg, ps->pmove.delta_angles[1]);
		MSG_WriteShort(msg, ps->pmove.delta_angles[2]);
	}

	// Write the rest of the player_state_t
	if (pflags & PS_VIEWOFFSET)
	{
		MSG_WriteChar(msg, ps->viewoffset[0] * 4);
		MSG_WriteChar(msg, ps->viewoffset[1] * 4);
		MSG_WriteChar(msg, ps->viewoffset[2] * 4);
	}

	if (pflags & PS_VIEWANGLES)
	{
		MSG_WriteAngle16(msg, ps->viewangles[0]);
		MSG_WriteAngle16(msg, ps->viewangles[1]);
		MSG_WriteAngle16(msg, ps->viewangles[2]);
	}

	if (pflags & PS_KICKANGLES)
	{
		MSG_WriteChar(msg, ps->kick_angles[0] * 4);
		MSG_WriteChar(msg, ps->kick_angles[1] * 4);
		MSG_WriteChar(msg, ps->kick_angles[2] * 4);
	}

	if (pflags & PS_WEAPONINDEX) //Knightmare- 12/23/2001- send as short
		MSG_WriteShort(msg, ps->gunindex);

#ifdef NEW_PLAYER_STATE_MEMBERS	//Knightmare added
	if (pflags & PS_WEAPONINDEX2)
		MSG_WriteShort(msg, ps->gunindex2); //Knightmare- gunindex2 support
#endif		

	if ((pflags & PS_WEAPONFRAME) || (pflags & PS_WEAPONFRAME2))
	{
		if (pflags & PS_WEAPONFRAME)
			MSG_WriteByte(msg, ps->gunframe);

#ifdef NEW_PLAYER_STATE_MEMBERS 	//Knightmare added
		if (pflags & PS_WEAPONFRAME2)
			MSG_WriteByte(msg, ps->gunframe2); //Knightmare- gunframe2 support
#endif

		MSG_WriteChar(msg, ps->gunoffset[0] * 4);
		MSG_WriteChar(msg, ps->gunoffset[1] * 4);
		MSG_WriteChar(msg, ps->gunoffset[2] * 4);
		MSG_WriteChar(msg, ps->gunangles[0] * 4);
		MSG_WriteChar(msg, ps->gunangles[1] * 4);
		MSG_WriteChar(msg, ps->gunangles[2] * 4);
	}

#ifdef NEW_PLAYER_STATE_MEMBERS //Knightmare added
	if (pflags & PS_WEAPONSKIN)
		MSG_WriteShort(msg, ps->gunskin);

	if (pflags & PS_WEAPONSKIN2)
		MSG_WriteShort(msg, ps->gunskin2);

	// Server-side speed control!
	if (pflags & PS_MAXSPEED)
		MSG_WriteShort(msg, ps->maxspeed);

	if (pflags & PS_DUCKSPEED)
		MSG_WriteShort(msg, ps->duckspeed);

	if (pflags & PS_WATERSPEED)
		MSG_WriteShort(msg, ps->waterspeed);

	if (pflags & PS_ACCEL)
		MSG_WriteShort(msg, ps->accel);

	if (pflags & PS_STOPSPEED)
		MSG_WriteShort(msg, ps->stopspeed);
#endif	//end Knightmare

	if (pflags & PS_BLEND)
	{
		MSG_WriteByte(msg, ps->blend[0] * 255);
		MSG_WriteByte(msg, ps->blend[1] * 255);
		MSG_WriteByte(msg, ps->blend[2] * 255);
		MSG_WriteByte(msg, ps->blend[3] * 255);
	}

	if (pflags & PS_FOV)
		MSG_WriteByte(msg, ps->fov);

	if (pflags & PS_RDFLAGS)
		MSG_WriteByte(msg, ps->rdflags);

	// send stats
	int statbits = 0;
	for (int i = 0; i < MAX_STATS; i++) //TODO: (mxd) writing 256 bits into 32-bit int is not such a good idea...
		if (ps->stats[i] != ops->stats[i])
			statbits |= 1 << i;

	MSG_WriteLong(msg, statbits);
	for (int i = 0; i < MAX_STATS; i++)
		if (statbits & (1 << i))
			MSG_WriteShort(msg, ps->stats[i]);
}

void SV_WriteFrameToClient(client_t *client, sizebuf_t *msg)
{
	client_frame_t *oldframe;
	int lastframe;

	// This is the frame we are creating
	client_frame_t *frame = &client->frames[sv.framenum & UPDATE_MASK];

	if (client->lastframe <= 0)
	{
		// Client is asking for a retransmit
		oldframe = NULL;
		lastframe = -1;
	}
	else if (sv.framenum - client->lastframe >= UPDATE_BACKUP - 3)
	{
		// Client hasn't gotten a good message through in a long time
		oldframe = NULL;
		lastframe = -1;
	}
	else
	{
		// We have a valid message to delta from
		oldframe = &client->frames[client->lastframe & UPDATE_MASK];
		lastframe = client->lastframe;
	}

	MSG_WriteByte(msg, svc_frame);
	MSG_WriteLong(msg, sv.framenum);
	MSG_WriteLong(msg, lastframe); // What we are delta'ing from
	MSG_WriteByte(msg, client->surpressCount); // Rate dropped packets
	client->surpressCount = 0;

	// Send over the areabits
	MSG_WriteByte(msg, frame->areabytes);
	SZ_Write(msg, frame->areabits, frame->areabytes);

	// Delta encode the playerstate
	SV_WritePlayerstateToClient(oldframe, frame, msg);

	// Delta encode the entities
	SV_EmitPacketEntities(oldframe, frame, msg);
}

#pragma endregion

#pragma region ======================= Client frame structure building

static byte fatpvs[65536 / 8]; // 32767 is MAX_MAP_LEAFS

// The client will interpolate the view position, so we can't use a single PVS point
static void SV_FatPVS(vec3_t org)
{
	int leafs[64];
	vec3_t mins, maxs;

	for (int i = 0; i < 3; i++)
	{
		mins[i] = org[i] - 8;
		maxs[i] = org[i] + 8;
	}

	const int count = CM_BoxLeafnums(mins, maxs, leafs, 64, NULL);
	if (count < 1)
		Com_Error(ERR_FATAL, "SV_FatPVS: count < 1");

	const int longs = (CM_NumClusters() + 31) >> 5;

	// Convert leafs to clusters
	for (int i = 0; i < count; i++)
		leafs[i] = CM_LeafCluster(leafs[i]);

	memcpy(fatpvs, CM_ClusterPVS(leafs[0]), longs << 2);
	
	// Or in all the other leaf bits
	for (int i = 1; i < count; i++)
	{
		int j;
		for (j = 0; j < i; j++)
			if (leafs[i] == leafs[j])
				break;

		if (j != i)
			continue; // Already have the cluster we want

		byte *src = CM_ClusterPVS(leafs[i]);
		for (j = 0; j < longs; j++)
			((long *)fatpvs)[j] |= ((long *)src)[j];
	}
}

// Decides which entities are going to be visible to the client, and copies off the playerstat and areabits.
void SV_BuildClientFrame(client_t *client)
{
	int i;
	vec3_t org;

	edict_t *clent = client->edict;
	if (!clent->client)
		return; // Not in game yet

	// This is the frame we are creating
	client_frame_t *frame = &client->frames[sv.framenum & UPDATE_MASK];

	frame->senttime = svs.realtime; // Save it for ping calc later

	// Find the client's PVS
	for (i = 0; i < 3; i++)
		org[i] = clent->client->ps.pmove.origin[i] * 0.125f + clent->client->ps.viewoffset[i];

	const int leafnum = CM_PointLeafnum(org);
	const int clientarea = CM_LeafArea(leafnum);
	const int clientcluster = CM_LeafCluster(leafnum);

	// Calculate the visible areas
	frame->areabytes = CM_WriteAreaBits(frame->areabits, clientarea);

	// Grab the current player_state_t
	frame->ps = clent->client->ps;

	SV_FatPVS(org);
	byte *clientphs = CM_ClusterPHS(clientcluster);

	// Build up the list of visible entities
	frame->num_entities = 0;
	frame->first_entity = svs.next_client_entities;

	for (int e = 1; e < ge->num_edicts; e++)
	{
		edict_t *ent = EDICT_NUM(e);

		// Ignore ents without visible models
		if (ent->svflags & SVF_NOCLIENT)
			continue;

		// Ignore ents without visible models unless they have an effect
		if (!ent->s.modelindex && !ent->s.effects && !ent->s.sound && !ent->s.event)
			continue;

		// Ignore if not touching a PV leaf
		if (ent != clent)
		{
			// Check area
			if (!CM_AreasConnected(clientarea, ent->areanum))
			{
				// Doors can legally straddle two areas, so we may need to check another one
				if (!ent->areanum2 || !CM_AreasConnected(clientarea, ent->areanum2))
					continue; // Blocked by a door
			}

			// Beams just check one point for PHS
			if (ent->s.renderfx & RF_BEAM)
			{
				const int cn = ent->clusternums[0];
				if (!(clientphs[cn >> 3] & (1 << (cn & 7))))
					continue;
			}
			else
			{
				// FIXME: if an ent has a model and a sound, but isn't in the PVS, only the PHS, clear the model
				byte *bitvector = fatpvs;

				if (ent->num_clusters == -1)
				{
					// Too many leafs for individual check, go by headnode
					if (!CM_HeadnodeVisible(ent->headnode, bitvector))
						continue;
				}
				else
				{
					// Check individual leafs
					for (i = 0; i < ent->num_clusters; i++)
					{
						const int cn = ent->clusternums[i];
						if (bitvector[cn >> 3] & (1 << (cn & 7)))
							break;
					}

					if (i == ent->num_clusters)
						continue; // Not visible
				}

				if (!ent->s.modelindex)
				{
					// Don't send sounds if they will be attenuated away
					vec3_t delta;
					float maxdist;

#if defined(NEW_ENTITY_STATE_MEMBERS) && defined(LOOP_SOUND_ATTENUATION)
					if (ent->s.attenuation > 0.0f && ent->s.attenuation < ATTN_STATIC)
						maxdist = 1.2f / (ent->s.attenuation * 0.0005f);
					else
#endif
						maxdist = 400;

					VectorSubtract(org, ent->s.origin, delta);
					if (VectorLength(delta) > maxdist) // 400
						continue;
				}
			}
		}

		// Add it to the circular client_entities array
		entity_state_t *state = &svs.client_entities[svs.next_client_entities % svs.num_client_entities];
		if (ent->s.number != e)
		{
			Com_DPrintf("FIXING ENT->S.NUMBER!!!\n");
			ent->s.number = e;
		}
		*state = ent->s;

		// Don't mark players missiles as solid
		if (ent->owner == client->edict)
			state->solid = 0;

		svs.next_client_entities++;
		frame->num_entities++;
	}
}

// Save everything in the world out without deltas.
// Used for recording footage for merged or assembled demos.
void SV_RecordDemoMessage()
{
	if (!svs.demofile)
		return;

	entity_state_t nostate;
	memset(&nostate, 0, sizeof(nostate));

	sizebuf_t buf;
	byte buf_data[32768];
	SZ_Init(&buf, buf_data, sizeof(buf_data));

	// Write a frame message that doesn't contain a player_state_t
	MSG_WriteByte(&buf, svc_frame);
	MSG_WriteLong(&buf, sv.framenum);

	MSG_WriteByte(&buf, svc_packetentities);

	int e = 1;
	edict_t *ent = EDICT_NUM(e);
	while (e < ge->num_edicts) 
	{
		// Ignore ents without visible models unless they have an effect
		if (ent->inuse && ent->s.number && 
			(ent->s.modelindex || ent->s.effects || ent->s.sound || ent->s.event) && 
			!(ent->svflags & SVF_NOCLIENT))
			MSG_WriteDeltaEntity(&nostate, &ent->s, &buf, false, true);

		e++;
		ent = EDICT_NUM(e);
	}

	MSG_WriteShort(&buf, 0); // End of packetentities

	// Now add the accumulated multicast information
	SZ_Write(&buf, svs.demo_multicast.data, svs.demo_multicast.cursize);
	SZ_Clear(&svs.demo_multicast);

	// Now write the entire message to the file, prefixed by the length
	fwrite(&buf.cursize, 4, 1, svs.demofile);
	fwrite(buf.data, buf.cursize, 1, svs.demofile);
}

#pragma endregion