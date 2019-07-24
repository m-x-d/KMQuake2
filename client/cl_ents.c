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
// cl_ents.c -- entity parsing and management

#include "client.h"

// Knightmare- var for saving speed controls
player_state_t *clientstate;

// Knightmare- backup of client angles
vec3_t old_viewangles;

#pragma region ======================= Frame parsing

// Returns the entity number and the header bits
int CL_ParseEntityBits(uint *bits)
{
	uint total = MSG_ReadByte(&net_message);

	if (total & U_MOREBITS1)
	{
		const uint b = MSG_ReadByte(&net_message);
		total |= b << 8;
	}

	if (total & U_MOREBITS2)
	{
		const uint b = MSG_ReadByte(&net_message);
		total |= b << 16;
	}

	if (total & U_MOREBITS3)
	{
		const uint b = MSG_ReadByte(&net_message);
		total |= b << 24;
	}

	int number;
	if (total & U_NUMBER16)
		number = MSG_ReadShort(&net_message);
	else
		number = MSG_ReadByte(&net_message);

	*bits = total;

	return number;
}

// Can go from either a baseline or a previous packet_entity
void CL_ParseDelta(entity_state_t *from, entity_state_t *to, const int number, const uint bits)
{
	// Set everything to the state we are delta'ing from
	*to = *from;

	VectorCopy(from->origin, to->old_origin);
	to->number = number;

	// Knightmare- read deltas the old way if playing old demos or connected to server using old protocol
	if (LegacyProtocol())
	{
		if (bits & U_MODEL)
			to->modelindex = MSG_ReadByte(&net_message);
		if (bits & U_MODEL2)
			to->modelindex2 = MSG_ReadByte(&net_message);
		if (bits & U_MODEL3)
			to->modelindex3 = MSG_ReadByte(&net_message);
		if (bits & U_MODEL4)
			to->modelindex4 = MSG_ReadByte(&net_message);

		if (bits & U_FRAME8)
			to->frame = MSG_ReadByte(&net_message);
		if (bits & U_FRAME16)
			to->frame = MSG_ReadShort(&net_message);

		if ((bits & U_SKIN8) && (bits & U_SKIN16)) // Used for laser colors
			to->skinnum = MSG_ReadLong(&net_message);
		else if (bits & U_SKIN8)
			to->skinnum = MSG_ReadByte(&net_message);
		else if (bits & U_SKIN16)
			to->skinnum = MSG_ReadShort(&net_message);

		if ((bits & (U_EFFECTS8 | U_EFFECTS16)) == (U_EFFECTS8 | U_EFFECTS16))
			to->effects = MSG_ReadLong(&net_message);
		else if (bits & U_EFFECTS8)
			to->effects = MSG_ReadByte(&net_message);
		else if (bits & U_EFFECTS16)
			to->effects = MSG_ReadShort(&net_message);

		if ((bits & (U_RENDERFX8 | U_RENDERFX16)) == (U_RENDERFX8 | U_RENDERFX16))
			to->renderfx = MSG_ReadLong(&net_message);
		else if (bits & U_RENDERFX8)
			to->renderfx = MSG_ReadByte(&net_message);
		else if (bits & U_RENDERFX16)
			to->renderfx = MSG_ReadShort(&net_message);

		if (bits & U_ORIGIN1)
			to->origin[0] = MSG_ReadCoord(&net_message);
		if (bits & U_ORIGIN2)
			to->origin[1] = MSG_ReadCoord(&net_message);
		if (bits & U_ORIGIN3)
			to->origin[2] = MSG_ReadCoord(&net_message);

		if (bits & U_ANGLE1)
			to->angles[0] = MSG_ReadAngle(&net_message);
		if (bits & U_ANGLE2)
			to->angles[1] = MSG_ReadAngle(&net_message);
		if (bits & U_ANGLE3)
			to->angles[2] = MSG_ReadAngle(&net_message);

		if (bits & U_OLDORIGIN)
			MSG_ReadPos(&net_message, to->old_origin);

		if (bits & U_SOUND)
			to->sound = MSG_ReadByte(&net_message);

		if (bits & U_EVENT)
			to->event = MSG_ReadByte(&net_message);
		else
			to->event = 0;

		if (bits & U_SOLID)
			to->solid = MSG_ReadShort(&net_message);
		// End old CL_ParseDelta code
	}	
	else // New CL_ParseDelta code
	{
		// Knightmare- 12/23/2001- read model indices as shorts 
		if (bits & U_MODEL)
			to->modelindex = MSG_ReadShort(&net_message);
		if (bits & U_MODEL2)
			to->modelindex2 = MSG_ReadShort(&net_message);
		if (bits & U_MODEL3)
			to->modelindex3 = MSG_ReadShort(&net_message);
		if (bits & U_MODEL4)
			to->modelindex4 = MSG_ReadShort(&net_message);

		// 1/18/2002- extra model indices
#ifdef NEW_ENTITY_STATE_MEMBERS
		if (bits & U_MODEL5)
			to->modelindex5 = MSG_ReadShort(&net_message);
		if (bits & U_MODEL6)
			to->modelindex6 = MSG_ReadShort(&net_message);
#else // We need to read and ignore this for eraser client compatibility
		if (bits & U_MODEL5)
			MSG_ReadShort(&net_message);
		if (bits & U_MODEL6)
			MSG_ReadShort(&net_message);
#endif // NEW_ENTITY_STATE_MEMBERS
		if (bits & U_FRAME8)
			to->frame = MSG_ReadByte(&net_message);
		if (bits & U_FRAME16)
			to->frame = MSG_ReadShort(&net_message);

		if ((bits & U_SKIN8) && (bits & U_SKIN16)) // Used for laser colors
			to->skinnum = MSG_ReadLong(&net_message);
		else if (bits & U_SKIN8)
			to->skinnum = MSG_ReadByte(&net_message);
		else if (bits & U_SKIN16)
			to->skinnum = MSG_ReadShort(&net_message);

		if ((bits & (U_EFFECTS8 | U_EFFECTS16)) == (U_EFFECTS8 | U_EFFECTS16))
			to->effects = MSG_ReadLong(&net_message);
		else if (bits & U_EFFECTS8)
			to->effects = MSG_ReadByte(&net_message);
		else if (bits & U_EFFECTS16)
			to->effects = MSG_ReadShort(&net_message);

		if ((bits & (U_RENDERFX8 | U_RENDERFX16)) == (U_RENDERFX8 | U_RENDERFX16))
			to->renderfx = MSG_ReadLong(&net_message);
		else if (bits & U_RENDERFX8)
			to->renderfx = MSG_ReadByte(&net_message);
		else if (bits & U_RENDERFX16)
			to->renderfx = MSG_ReadShort(&net_message);

		if (bits & U_ORIGIN1)
			to->origin[0] = MSG_ReadCoord(&net_message);
		if (bits & U_ORIGIN2)
			to->origin[1] = MSG_ReadCoord(&net_message);
		if (bits & U_ORIGIN3)
			to->origin[2] = MSG_ReadCoord(&net_message);

		//mxd. Read more precise angles (MSG_ReadAngle -> MSG_ReadAngle16). Fixes jerky movement of func_rotating with slow speed.
		if (bits & U_ANGLE1)
			to->angles[0] = MSG_ReadAngle16(&net_message);
		if (bits & U_ANGLE2)
			to->angles[1] = MSG_ReadAngle16(&net_message);
		if (bits & U_ANGLE3)
			to->angles[2] = MSG_ReadAngle16(&net_message);

		if (bits & U_OLDORIGIN)
			MSG_ReadPos(&net_message, to->old_origin);

		// 5/11/2002- added alpha
		if (bits & U_ALPHA)
#ifdef NEW_ENTITY_STATE_MEMBERS
			to->alpha = MSG_ReadByte(&net_message) / 255.0f;
#else // We need to read and ignore this for eraser client compatibility
			MSG_ReadByte(&net_message);
#endif

		// 12/23/2001- read sound indices as shorts
		if (bits & U_SOUND)
			to->sound = MSG_ReadShort(&net_message);

#ifdef LOOP_SOUND_ATTENUATION
		if (bits & U_ATTENUAT)
#ifdef NEW_ENTITY_STATE_MEMBERS
			to->attenuation = MSG_ReadByte(&net_message) / 64.0f;
#else // We need to read and ignore this for eraser client compatibility
			MSG_ReadByte(&net_message);
#endif
#endif

		if (bits & U_EVENT)
			to->event = MSG_ReadByte(&net_message);
		else
			to->event = 0;

		if (bits & U_SOLID)
			to->solid = MSG_ReadShort(&net_message);
	} // End new CL_ParseDelta code
}

// Parses deltas from the given base and adds the resulting entity to the current frame
static void CL_DeltaEntity(frame_t *frame, const int newnum, entity_state_t *old, const uint bits)
{
	centity_t *ent = &cl_entities[newnum];
	entity_state_t *state = &cl_parse_entities[cl.parse_entities & (MAX_PARSE_ENTITIES - 1)];

	cl.parse_entities++;
	frame->num_entities++;

	CL_ParseDelta(old, state, newnum, bits);

	// Some data changes will force no lerping
	if (state->modelindex != ent->current.modelindex
		|| state->modelindex2 != ent->current.modelindex2
		|| state->modelindex3 != ent->current.modelindex3
		|| state->modelindex4 != ent->current.modelindex4
#ifdef NEW_ENTITY_STATE_MEMBERS
		// 1/18/2002- extra model indices
		|| state->modelindex5 != ent->current.modelindex5
		|| state->modelindex6 != ent->current.modelindex6
#endif
		|| abs(state->origin[0] - ent->current.origin[0]) > 512
		|| abs(state->origin[1] - ent->current.origin[1]) > 512
		|| abs(state->origin[2] - ent->current.origin[2]) > 512
		|| state->event == EV_PLAYER_TELEPORT
		|| state->event == EV_OTHER_TELEPORT)
	{
		ent->serverframe = -99;
	}

	if (ent->serverframe != cl.frame.serverframe - 1)
	{
		// Wasn't in last update, so initialize some things
		ent->trailcount = 1024; // For diminishing rocket / grenade trails
		
		// Duplicate the current state so lerping doesn't hurt anything
		ent->prev = *state;

		if (state->event == EV_OTHER_TELEPORT)
		{
			VectorCopy(state->origin, ent->prev.origin);
			VectorCopy(state->origin, ent->lerp_origin);
		}
		else
		{
			VectorCopy(state->old_origin, ent->prev.origin);
			VectorCopy(state->old_origin, ent->lerp_origin);
		}
	}
	else
	{
		// Shuffle the last state to previous
		ent->prev = ent->current;
	}

	ent->serverframe = cl.frame.serverframe;
	ent->current = *state;
}

// An svc_packetentities has just been parsed, deal with the rest of the data stream.
static void CL_ParsePacketEntities(frame_t *oldframe, frame_t *newframe)
{
	uint bits;
	entity_state_t *oldstate = NULL;

	newframe->parse_entities = cl.parse_entities;
	newframe->num_entities = 0;

	// Delta from the entities present in oldframe
	int oldindex = 0;
	int oldnum = 99999;

	if (oldframe && oldindex < oldframe->num_entities)
	{
		oldstate = &cl_parse_entities[(oldframe->parse_entities + oldindex) & (MAX_PARSE_ENTITIES - 1)];
		oldnum = oldstate->number;
	}

	while (true)
	{
		const int newnum = CL_ParseEntityBits(&bits);
		if (newnum >= MAX_EDICTS)
			Com_Error(ERR_DROP, "CL_ParsePacketEntities: bad number: %i", newnum);

		if (net_message.readcount > net_message.cursize)
			Com_Error(ERR_DROP, "CL_ParsePacketEntities: end of message");

		if (!newnum)
			break;

		while (oldnum < newnum)
		{
			// One or more entities from the old packet are unchanged
			if (cl_shownet->value == 3)
				Com_Printf("   unchanged: %i\n", oldnum);

			CL_DeltaEntity(newframe, oldnum, oldstate, 0);
			
			oldindex++;

			if (oldindex >= oldframe->num_entities)
			{
				oldnum = 99999;
			}
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities + oldindex) & (MAX_PARSE_ENTITIES - 1)];
				oldnum = oldstate->number;
			}
		}

		if (bits & U_REMOVE)
		{
			// The entity present in oldframe is not in the current frame
			if (cl_shownet->value == 3)
				Com_Printf("   remove: %i\n", newnum);

			if (oldnum != newnum)
				Com_Printf(S_COLOR_YELLOW"U_REMOVE: oldnum != newnum!\n");

			oldindex++;

			if (oldindex >= oldframe->num_entities)
			{
				oldnum = 99999;
			}
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities + oldindex) & (MAX_PARSE_ENTITIES - 1)];
				oldnum = oldstate->number;
			}

			continue;
		}

		if (oldnum == newnum)
		{
			// Delta from previous state
			if (cl_shownet->value == 3)
				Com_Printf("   delta: %i\n", newnum);

			CL_DeltaEntity(newframe, newnum, oldstate, bits);

			oldindex++;

			if (oldindex >= oldframe->num_entities)
			{
				oldnum = 99999;
			}
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities + oldindex) & (MAX_PARSE_ENTITIES - 1)];
				oldnum = oldstate->number;
			}

			continue;
		}

		if (oldnum > newnum)
		{
			// Delta from baseline
			if (cl_shownet->value == 3)
				Com_Printf("   baseline: %i\n", newnum);

			CL_DeltaEntity(newframe, newnum, &cl_entities[newnum].baseline, bits);
		}
	}

	// Any remaining entities in the old frame are copied over
	while (oldnum != 99999)
	{
		// One or more entities from the old packet are unchanged
		if (cl_shownet->value == 3)
			Com_Printf("   unchanged: %i\n", oldnum);

		CL_DeltaEntity(newframe, oldnum, oldstate, 0);
		
		oldindex++;

		if (oldindex >= oldframe->num_entities)
		{
			oldnum = 99999;
		}
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities + oldindex) & (MAX_PARSE_ENTITIES - 1)];
			oldnum = oldstate->number;
		}
	}
}

static void CL_ParsePlayerstate(frame_t *oldframe, frame_t *newframe)
{
	player_state_t *state = &newframe->playerstate;

	// Clear to old value before delta parsing
	if (oldframe)
		*state = oldframe->playerstate;
	else
		memset(state, 0, sizeof(*state));

	// Knightmare- read player state info the old way if playing old demos or connected to server using old protocol
	if (LegacyProtocol())
	{
		const int flags = MSG_ReadShort(&net_message);

		// Parse the pmove_state_t
		if (flags & PS_M_TYPE)
			state->pmove.pm_type = MSG_ReadByte(&net_message);

		if (flags & PS_M_ORIGIN) // FIXME- map size
			for (int c = 0; c < 3; c++)
				state->pmove.origin[c] = MSG_ReadShort(&net_message);

		if (flags & PS_M_VELOCITY)
			for (int c = 0; c < 3; c++)
				state->pmove.velocity[c] = MSG_ReadShort(&net_message);

		if (flags & PS_M_TIME)
			state->pmove.pm_time = MSG_ReadByte(&net_message);

		if (flags & PS_M_FLAGS)
			state->pmove.pm_flags = MSG_ReadByte(&net_message);

		if (flags & PS_M_GRAVITY)
			state->pmove.gravity = MSG_ReadShort(&net_message);

		if (flags & PS_M_DELTA_ANGLES)
			for (int c = 0; c < 3; c++)
				state->pmove.delta_angles[c] = MSG_ReadShort(&net_message);

		if (cl.attractloop)
			state->pmove.pm_type = PM_FREEZE; // Demo playback

		// Parse the rest of the player_state_t
		if (flags & PS_VIEWOFFSET)
			for (int c = 0; c < 3; c++)
				state->viewoffset[c] = MSG_ReadChar(&net_message) * 0.25f;

		if (flags & PS_VIEWANGLES)
			for (int c = 0; c < 3; c++)
				state->viewangles[c] = MSG_ReadAngle16(&net_message);

		if (flags & PS_KICKANGLES)
			for (int c = 0; c < 3; c++)
				state->kick_angles[c] = MSG_ReadChar(&net_message) * 0.25f;

		if (flags & PS_WEAPONINDEX)
			state->gunindex = MSG_ReadByte(&net_message);

		if (flags & PS_WEAPONFRAME)
		{
			state->gunframe = MSG_ReadByte(&net_message);

			for (int c = 0; c < 3; c++)
				state->gunoffset[c] = MSG_ReadChar(&net_message) * 0.25f;

			for (int c = 0; c < 3; c++)
				state->gunangles[c] = MSG_ReadChar(&net_message) * 0.25f;
		}

		if (flags & PS_BLEND)
			for (int c = 0; c < 4; c++)
				state->blend[c] = MSG_ReadByte(&net_message) * DIV255;

		if (flags & PS_FOV)
			state->fov = MSG_ReadByte(&net_message);

		if (flags & PS_RDFLAGS)
			state->rdflags = MSG_ReadByte(&net_message);

		// parse stats
		const int statbits = MSG_ReadLong(&net_message);
		for (int i = 0; i < OLD_MAX_STATS; i++) //Knightmare- use old max_stats
			if (statbits & (1 << i))
				state->stats[i] = MSG_ReadShort(&net_message);

		// End old CL_ParsePlayerstate code
	}	
	else // New CL_ParsePlayerstate code
	{
		// Knightmare 4/5/2002- read as long
		const int flags = MSG_ReadLong(&net_message);

		// Parse the pmove_state_t
		if (flags & PS_M_TYPE)
			state->pmove.pm_type = MSG_ReadByte(&net_message);

		if (flags & PS_M_ORIGIN)
		{
			for (int c = 0; c < 3; c++)
#ifdef LARGE_MAP_SIZE
				state->pmove.origin[c] = MSG_ReadPMCoordNew(&net_message);
#else
				state->pmove.origin[c] = MSG_ReadShort(&net_message);
#endif
		}

		if (flags & PS_M_VELOCITY)
			for (int c = 0; c < 3; c++)
				state->pmove.velocity[c] = MSG_ReadShort(&net_message);

		if (flags & PS_M_TIME)
			state->pmove.pm_time = MSG_ReadByte(&net_message);

		if (flags & PS_M_FLAGS)
			state->pmove.pm_flags = MSG_ReadByte(&net_message);

		if (flags & PS_M_GRAVITY)
			state->pmove.gravity = MSG_ReadShort(&net_message);

		if (flags & PS_M_DELTA_ANGLES)
			for (int c = 0; c < 3; c++)
				state->pmove.delta_angles[c] = MSG_ReadShort(&net_message);

		if (cl.attractloop)
			state->pmove.pm_type = PM_FREEZE; // Demo playback

		// Parse the rest of the player_state_t
		if (flags & PS_VIEWOFFSET)
			for (int c = 0; c < 3; c++)
				state->viewoffset[c] = MSG_ReadChar(&net_message) * 0.25f;

		if (flags & PS_VIEWANGLES)
			for (int c = 0; c < 3; c++)
				state->viewangles[c] = MSG_ReadAngle16(&net_message);

		if (flags & PS_KICKANGLES)
			for (int c = 0; c < 3; c++)
				state->kick_angles[c] = MSG_ReadChar(&net_message) * 0.25f;

		//Knightmare 4/5/2002- read as short
		if (flags & PS_WEAPONINDEX)
			state->gunindex = MSG_ReadShort(&net_message);

	#ifdef NEW_PLAYER_STATE_MEMBERS	// Knightmare- gunindex2 support
		if (flags & PS_WEAPONINDEX2)
			state->gunindex2 = MSG_ReadShort(&net_message);
	#endif

	// Knightmare- gunframe2 support
	#ifdef NEW_PLAYER_STATE_MEMBERS
		if ((flags & PS_WEAPONFRAME) || (flags & PS_WEAPONFRAME2))
		{
			if (flags & PS_WEAPONFRAME)
				state->gunframe = MSG_ReadByte(&net_message);
			if (flags & PS_WEAPONFRAME2)
				state->gunframe2 = MSG_ReadByte(&net_message);

			for (int c = 0; c < 3; c++)
				state->gunoffset[0] = MSG_ReadChar(&net_message) * 0.25f;

			for (int c = 0; c < 3; c++)
				state->gunangles[c] = MSG_ReadChar(&net_message) * 0.25f;
		}
	#else
		if (flags & PS_WEAPONFRAME)
		{
			state->gunframe = MSG_ReadByte(&net_message);

			for (int c = 0; c < 3; c++)
				state->gunoffset[c] = MSG_ReadChar(&net_message) * 0.25f;

			for (int c = 0; c < 3; c++)
				state->gunangles[c] = MSG_ReadChar(&net_message) * 0.25f;
		}
	#endif

	#ifdef NEW_PLAYER_STATE_MEMBERS
		if (flags & PS_WEAPONSKIN) // Knightmare- gunskin support
			state->gunskin = MSG_ReadShort(&net_message);

		if (flags & PS_WEAPONSKIN2)
			state->gunskin2 = MSG_ReadShort(&net_message);

		// Server-side speed control!
		if (flags & PS_MAXSPEED)
			state->maxspeed = MSG_ReadShort(&net_message);

		if (flags & PS_DUCKSPEED)
			state->duckspeed = MSG_ReadShort(&net_message);

		if (flags & PS_WATERSPEED)
			state->waterspeed = MSG_ReadShort(&net_message);

		if (flags & PS_ACCEL)
			state->accel = MSG_ReadShort(&net_message);

		if (flags & PS_STOPSPEED)
			state->stopspeed = MSG_ReadShort(&net_message);
	#endif // end Knightmare

		if (flags & PS_BLEND)
			for(int c = 0; c < 4; c++)
				state->blend[c] = MSG_ReadByte(&net_message) / 255.0f;

		if (flags & PS_FOV)
			state->fov = MSG_ReadByte(&net_message);

		if (flags & PS_RDFLAGS)
			state->rdflags = MSG_ReadByte(&net_message);

		// Parse stats
		const int statbits = MSG_ReadLong(&net_message);
		for (int i = 0; i < MAX_STATS; i++) //TODO: (mxd) reading 256 bits from 32-bit int is not such a good idea...
			if (statbits & (1 << i))
				state->stats[i] = MSG_ReadShort(&net_message);
	}
}

static void CL_FireEntityEvents(frame_t *frame)
{
	for (int pnum = 0; pnum < frame->num_entities; pnum++)
	{
		const int num = (frame->parse_entities + pnum) & (MAX_PARSE_ENTITIES - 1);
		entity_state_t *s1 = &cl_parse_entities[num];
		if (s1->event)
			CL_EntityEvent(s1);

		// EF_TELEPORTER acts like an event, but is not cleared each frame
		if (s1->effects & EF_TELEPORTER)
			CL_TeleporterParticles(s1);
	}
}

void CL_ParseFrame()
{
	frame_t *old;

	memset(&cl.frame, 0, sizeof(cl.frame));

	cl.frame.serverframe = MSG_ReadLong(&net_message);
	cl.frame.deltaframe = MSG_ReadLong(&net_message);
	cl.frame.servertime = cl.frame.serverframe * 100;

	// BIG HACK to let old demos continue to work
	if (cls.serverProtocol != 26)
		cl.surpressCount = MSG_ReadByte(&net_message);

	if (cl_shownet->value == 3)
		Com_Printf("   frame: %i  delta: %i\n", cl.frame.serverframe, cl.frame.deltaframe);

	// If the frame is delta compressed from data that we no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed message 
	if (cl.frame.deltaframe <= 0)
	{
		cl.frame.valid = true; // Uncompressed frame
		old = NULL;
		cls.demowaiting = false; // We can start recording now
	}
	else
	{
		old = &cl.frames[cl.frame.deltaframe & UPDATE_MASK];
		if (!old->valid) // Should never happen
			Com_Printf(S_COLOR_YELLOW"Delta from invalid frame (not supposed to happen!).\n");

		if (old->serverframe != cl.frame.deltaframe)
			Com_Printf(S_COLOR_YELLOW"Delta frame too old.\n"); // The frame that the server did the delta from is too old, so we can't reconstruct it properly.
		else if (cl.parse_entities - old->parse_entities > MAX_PARSE_ENTITIES - 128)
			Com_Printf(S_COLOR_YELLOW"Delta parse_entities too old.\n");
		else
			cl.frame.valid = true; // Valid delta parse
	}

	// Clamp time
	cl.time = clamp(cl.time, cl.frame.servertime - 100, cl.frame.servertime);

	// Read areabits
	const int len = MSG_ReadByte(&net_message);
	MSG_ReadData(&net_message, &cl.frame.areabits, len);

	// Read playerinfo
	int cmd = MSG_ReadByte(&net_message);
	SHOWNET(svc_strings[cmd]);

	if (cmd != svc_playerinfo)
		Com_Error(ERR_DROP, "CL_ParseFrame: not playerinfo");

	CL_ParsePlayerstate(old, &cl.frame);

	// Knightmare- set pointer to player state for movement code
	clientstate = &cl.frame.playerstate;

	// Read packet entities
	cmd = MSG_ReadByte(&net_message);
	SHOWNET(svc_strings[cmd]);

	if (cmd != svc_packetentities)
		Com_Error(ERR_DROP, "CL_ParseFrame: not packetentities");

	CL_ParsePacketEntities(old, &cl.frame);

	// Save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverframe & UPDATE_MASK] = cl.frame;

	if (cl.frame.valid)
	{
		// Getting a valid frame message ends the connection process
		if (cls.state != ca_active)
		{
			cls.state = ca_active;
			cl.force_refdef = true;

			for(int c = 0; c < 3; c++)
				cl.predicted_origin[c] = cl.frame.playerstate.pmove.origin[c] * 0.125f;

			VectorCopy(cl.frame.playerstate.viewangles, cl.predicted_angles);

			if (cls.disable_servercount != cl.servercount && cl.refresh_prepped)
				SCR_EndLoadingPlaque(); // Get rid of loading plaque
		}

		cl.sound_prepped = true; // Can start mixing ambient sounds
	
		// Fire entity events
		CL_FireEntityEvents(&cl.frame);
		CL_CheckPredictionError();
	}
}

#pragma endregion

#pragma region ======================= CL_AddPacketEntities

// Knightmare- save off current player weapon model for player config menu
extern char *currentweaponmodel;
extern vec3_t clientorigin;

static void CL_AddPacketEntities(frame_t *frame)
{
	entity_t ent;
	clientinfo_t *ci;

	// Bonus items rotate at a fixed rate
	const float autorotate = anglemod(cl.time / 10.0f);

	// Brush models can auto animate their frames
	const int autoanim = 2 * cl.time / 1000;

	memset(&ent, 0, sizeof(ent));

	// Knightmare added
	VectorCopy(vec3_origin, clientorigin);

	// Knightmare- reset current weapon model
	currentweaponmodel = NULL;

	for (int pnum = 0; pnum < frame->num_entities; pnum++)
	{
		qboolean isclientviewer = false;
		qboolean drawent = true; //Knightmare added

		entity_state_t *s1 = &cl_parse_entities[(frame->parse_entities + pnum) & (MAX_PARSE_ENTITIES - 1)];

		centity_t *cent = &cl_entities[s1->number];

		uint effects = s1->effects;
		uint renderfx = s1->renderfx;

		// If i want to multiply alpha, then I'd better do this...
		ent.alpha = 1.0f;

		// Reset this
		ent.renderfx = 0;

		// Set frame
		if (effects & EF_ANIM01)
			ent.frame = autoanim & 1;
		else if (effects & EF_ANIM23)
			ent.frame = 2 + (autoanim & 1);
		else if (effects & EF_ANIM_ALL)
			ent.frame = autoanim;
		else if (effects & EF_ANIM_ALLFAST)
			ent.frame = cl.time / 100;
		else
			ent.frame = s1->frame;

		// Ents that cast light don't give off shadows
		if (effects & (EF_BLASTER | EF_HYPERBLASTER | EF_BLUEHYPERBLASTER | EF_TRACKER | EF_ROCKET | EF_PLASMA | EF_IONRIPPER))
			renderfx |= RF_NOSHADOW;

		// Quad and pent can do different things on client
		if (effects & EF_PENT)
		{
			effects &= ~EF_PENT;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_RED;
		}

		if (effects & EF_QUAD)
		{
			effects &= ~EF_QUAD;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_BLUE;
		}

		if (effects & EF_DOUBLE) // PMM
		{
			effects &= ~EF_DOUBLE;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_DOUBLE;
		}

		if (effects & EF_HALF_DAMAGE) // PMM
		{
			effects &= ~EF_HALF_DAMAGE;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_HALF_DAM;
		}

		ent.oldframe = cent->prev.frame;
		ent.backlerp = 1.0f - cl.lerpfrac;

		if (renderfx & (RF_FRAMELERP | RF_BEAM)) //TODO: (mxd) check beam interpolation
		{
			// Step origin discretely, because the frames do the animation properly
			VectorCopy(cent->current.origin, ent.origin);
			VectorCopy(cent->current.old_origin, ent.oldorigin);
		}
		else
		{
			// Interpolate origin
			for (int i = 0; i < 3; i++)
				ent.origin[i] = ent.oldorigin[i] = cent->prev.origin[i] + cl.lerpfrac * (cent->current.origin[i] - cent->prev.origin[i]);
		}

		// Create a new entity
	
		// Tweak the color of beams
		if (renderfx & RF_BEAM)
		{
			// The four beam colors are encoded in 32 bits of skinnum (hack)
			ent.alpha = 0.3f;
			ent.skinnum = (s1->skinnum >> ((rand() % 4) * 8)) & 0xff;
			ent.model = NULL;
		}
		else
		{
			// Set skin
			if (s1->modelindex == MAX_MODELS - 1 || (LegacyProtocol() && s1->modelindex == OLD_MAX_MODELS - 1)) //Knightmare- GROSS HACK for old demos, use modelindex 255
			{
				// Use custom player skin
				ent.skinnum = 0;
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				ent.skin = ci->skin;
				ent.model = ci->model;

				if (!ent.skin || !ent.model)
				{
					ent.skin = cl.baseclientinfo.skin;
					ent.model = cl.baseclientinfo.model;
				}

				// Knightmare- catch NULL player skin
				if (renderfx & RF_USE_DISGUISE && ent.skin != NULL && ((char *)ent.skin)[0])
				{
					if(!strncmp((char *)ent.skin, "players/male", 12))
					{
						ent.skin = R_RegisterSkin("players/male/disguise.pcx");
						ent.model = R_RegisterModel("players/male/tris.md2");
					}
					else if(!strncmp((char *)ent.skin, "players/female", 14))
					{
						ent.skin = R_RegisterSkin("players/female/disguise.pcx");
						ent.model = R_RegisterModel("players/female/tris.md2");
					}
					else if(!strncmp((char *)ent.skin, "players/cyborg", 14))
					{
						ent.skin = R_RegisterSkin("players/cyborg/disguise.pcx");
						ent.model = R_RegisterModel("players/cyborg/tris.md2");
					}
				}
			}
			else
			{
				ent.skinnum = s1->skinnum;
				ent.skin = NULL;
				ent.model = cl.model_draw[s1->modelindex];
			}
		}
		
		// MODEL / EFFECT SWAPPING ETC - per gametype...
		if (ent.model && !(effects & EF_BLASTER) && r_particle_mode->integer > 0) //mxd. Not when classic particles are enabled
		{
			if (!Q_strcasecmp((char *)ent.model, "models/objects/laser/tris.md2"))
			{
				// Replace the bolt with a particle glow
				CL_HyperBlasterEffect(cent->lerp_origin, ent.origin, s1->angles, tv(255, 150, 50), tv(0, -90, -30), 10, 3);
				drawent = false;
			}
			else if (!Q_strcasecmp((char *)ent.model, "models/proj/laser2/tris.md2")
				  || !Q_strcasecmp((char *)ent.model, "models/objects/laser2/tris.md2")
				  || !Q_strcasecmp((char *)ent.model, "models/objects/glaser/tris.md2"))
			{
				// Give the bolt a green particle glow
				CL_HyperBlasterEffect(cent->lerp_origin, ent.origin, s1->angles, tv(50, 235, 50), tv(-10, 0, -10), 10, 3);
				drawent = false;
			}
			else if (!Q_strcasecmp((char *)ent.model, "models/objects/blaser/tris.md2"))
			{
				// Give the bolt a blue particle glow
				CL_HyperBlasterEffect(cent->lerp_origin, ent.origin, s1->angles, tv(50, 50, 235), tv(0, -10, 0), -10, 3);
				drawent = false;
			}
			else if (!Q_strcasecmp((char *)ent.model, "models/objects/rlaser/tris.md2"))
			{
				// Give the bolt a red particle glow
				CL_HyperBlasterEffect(cent->lerp_origin, ent.origin, s1->angles, tv(235, 50, 50), tv(0, -90, -30), -10, 3);
				drawent = false;
			}
		}

		// Only used for black hole model right now, FIXME: do better
		if (renderfx & RF_TRANSLUCENT)
			ent.alpha = 0.7f;

		// Render effects (fullbright, translucent, etc)
		if (effects & EF_COLOR_SHELL)
			ent.flags = 0; // renderfx go on color shell entity
		else
			ent.flags = renderfx;

		// Calculate angles
		if (effects & EF_ROTATE)
		{
			// Some bonus items auto-rotate
			ent.angles[0] = 0;
			ent.angles[1] = autorotate;
			ent.angles[2] = 0;

			// Bobbing items by QuDos
			if (cl_item_bobbing->integer)
			{
				const float	bob_scale = (0.005f + s1->number * 0.00001f) * 0.5f;
				const float	bob = cosf((cl.time + 1000) * bob_scale) * 5;
				ent.oldorigin[2] += bob;
				ent.origin[2] += bob;
			}
		}
		else if (effects & EF_SPINNINGLIGHTS) // RAFAEL
		{
			ent.angles[0] = 0;
			ent.angles[1] = anglemod(cl.time / 2.0f) + s1->angles[1];
			ent.angles[2] = 180;

			vec3_t start, forward;
			AngleVectors(ent.angles, forward, NULL, NULL);
			VectorMA(ent.origin, 64, forward, start);
			V_AddLight(start, 100, 1, 0, 0);
		}
		else
		{
			// Interpolate angles
			for (int i = 0; i < 3; i++)
			{
				const float a1 = cent->current.angles[i];
				const float a2 = cent->prev.angles[i];
				ent.angles[i] = LerpAngle(a2, a1, cl.lerpfrac);
			}
		}

		// Process player entity
		if (s1->number == cl.playernum + 1)
		{
			ent.flags |= RF_VIEWERMODEL; // Only draw from mirrors
			isclientviewer = true;

			// EF_FLAG1|EF_FLAG2 is a special case for EF_FLAG3...  plus de fromage!
			if (effects & EF_FLAG1)
			{
				if (effects & EF_FLAG2)
					V_AddLight(ent.origin, 255, 0.1f, 1.0f, 0.1f);
				else
					V_AddLight(ent.origin, 225, 1.0f, 0.1f, 0.1f);
			}
			else if (effects & EF_FLAG2)
			{
				V_AddLight(ent.origin, 225, 0.1f, 0.1f, 1.0f);
			}
			else if (effects & EF_TAGTRAIL)	//PGM
			{
				V_AddLight(ent.origin, 225, 1.0f, 1.0f, 0.0f);
			}
			else if (effects & EF_TRACKERTRAIL)	//PGM
			{
				V_AddLight(ent.origin, 225, -1.0f, -1.0f, -1.0f);
			}

			// Knightmare- save off current player weapon model for player config menu
			if (s1->modelindex2 == MAX_MODELS - 1 || (LegacyProtocol() && s1->modelindex2 == OLD_MAX_MODELS - 1))
			{
				int index = (s1->skinnum >> 8); // 0 is default weapon model
				if (!cl_vwep->integer || index > MAX_CLIENTWEAPONMODELS - 1)
					index = 0;

				currentweaponmodel = cl_weaponmodels[index];
			}

			// Fix for third-person in demos
			if (!(cg_thirdperson->integer && !(cl.attractloop && !(cl.cinematictime > 0 && cls.realtime - cl.cinematictime > 1000))))
				continue;
		}

		// If set to invisible, skip
		if (!s1->modelindex)
			continue;

		if (effects & EF_BFG)
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.3f;
		}

		if (effects & EF_PLASMA) // RAFAEL
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.6f;
		}

		if (effects & EF_SPHERETRANS)
		{
			ent.flags |= RF_TRANSLUCENT;

			// PMM - *sigh*  yet more EF overloading
			if (effects & EF_TRACKERTRAIL)
				ent.alpha = 0.6f;
			else
				ent.alpha = 0.3f;
		}

// Knightmare- read server-assigned alpha, overriding effects flags
#ifdef NEW_ENTITY_STATE_MEMBERS
		if (s1->alpha > 0.0f && s1->alpha < 1.0f)
		{
			// Lerp alpha
			ent.alpha = cent->prev.alpha + cl.lerpfrac * (cent->current.alpha - cent->prev.alpha);
			ent.flags |= RF_TRANSLUCENT;
		}
#endif
		
		// Knightmare added for mirroring
		if (renderfx & RF_MIRRORMODEL)
			ent.flags |= RF_MIRRORMODEL;

		// Add to refresh list
		if (drawent) //Knightmare added
			V_AddEntity(&ent);

		// Color shells generate a seperate entity for the main model
		if (effects & EF_COLOR_SHELL && (!isclientviewer || (cg_thirdperson->integer && !(cl.attractloop && !(cl.cinematictime > 0 && cls.realtime - cl.cinematictime > 1000)))))
		{
			// PMM - at this point, all of the shells have been handled.
			// If we're in the rogue pack, set up the custom mixing, otherwise just keep going.
			// Knightmare 6/06/2002
			if (FS_RoguePath())
			{
				// All of the solo colors are fine. We need to catch any of the combinations that look bad
				// (double & half) and turn them into the appropriate color, and make double/quad something special
				if (renderfx & RF_SHELL_HALF_DAM)
				{
					// Ditch the half damage shell if any of red, blue, or double are on
					if (renderfx & (RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE))
						renderfx &= ~RF_SHELL_HALF_DAM;
				}

				if (renderfx & RF_SHELL_DOUBLE)
				{
					// Lose the yellow shell if we have a red, blue, or green shell
					if (renderfx & (RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_GREEN))
						renderfx &= ~RF_SHELL_DOUBLE;

					// If we have a red shell, turn it to purple by adding blue
					if (renderfx & RF_SHELL_RED)
					{
						renderfx |= RF_SHELL_BLUE;
					}
					// If we have a blue shell (and not a red shell), turn it to cyan by adding green
					else if (renderfx & RF_SHELL_BLUE)
					{
						// Go to green if it's on already, otherwise do cyan (flash green)
						if (renderfx & RF_SHELL_GREEN)
							renderfx &= ~RF_SHELL_BLUE;
						else
							renderfx |= RF_SHELL_GREEN;
					}
				}
			}

			ent.flags = renderfx | RF_TRANSLUCENT;
			ent.alpha = 0.3f;
			V_AddEntity(&ent);
		}

		ent.skin = NULL; // Never use a custom skin on others
		ent.skinnum = 0;
		ent.flags = 0;
		ent.alpha = 1.0f;

		// Duplicate for linked models
		if (s1->modelindex2)
		{
			if (s1->modelindex2 == MAX_MODELS - 1 || (LegacyProtocol() && s1->modelindex2 == OLD_MAX_MODELS - 1)) //Knightmare- GROSS HACK for old demos, use modelindex 255
			{
				// Custom weapon
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				int index = (s1->skinnum >> 8); // 0 is default weapon model
				if (!cl_vwep->integer || index > MAX_CLIENTWEAPONMODELS - 1)
					index = 0;

				ent.model = ci->weaponmodel[index];

				if (!ent.model)
				{
					if (index != 0)
						ent.model = ci->weaponmodel[0];

					if (!ent.model)
						ent.model = cl.baseclientinfo.weaponmodel[0];
				}
			}
			else
			{
				ent.model = cl.model_draw[s1->modelindex2];
			}

			// PMM - check for the defender sphere shell .. make it translucent.
			// Replaces the previous version which used the high bit on modelindex2 to determine transparency.
			if (!Q_strcasecmp(cl.configstrings[CS_MODELS + (s1->modelindex2)], "models/items/shell/tris.md2"))
			{
				ent.alpha = 0.32f;
				ent.flags = RF_TRANSLUCENT;
			}

			// Knightmare added for Psychospaz's chasecam
			if (isclientviewer)
				ent.flags |= RF_VIEWERMODEL;

			// Knightmare added for mirroring
			if (renderfx & RF_MIRRORMODEL)
				ent.flags |= RF_MIRRORMODEL;

			V_AddEntity(&ent);

			//PGM - make sure these get reset.
			ent.flags = 0;
			ent.alpha = 1.0f;
		}

		if (s1->modelindex3)
		{
			// Knightmare added for Psychospaz's chasecam
			if (isclientviewer)
				ent.flags |= RF_VIEWERMODEL;

			// Knightmare added for mirroring
			if (renderfx & RF_MIRRORMODEL)
				ent.flags |= RF_MIRRORMODEL;

			ent.model = cl.model_draw[s1->modelindex3];
			V_AddEntity(&ent);
		}

		if (s1->modelindex4)
		{
			// Knightmare added for Psychospaz's chasecam
			if (isclientviewer)
				ent.flags |= RF_VIEWERMODEL;

			// Knightmare added for mirroring
			if (renderfx & RF_MIRRORMODEL)
				ent.flags |= RF_MIRRORMODEL;

			ent.model = cl.model_draw[s1->modelindex4];
			V_AddEntity(&ent);
		}

#ifdef NEW_ENTITY_STATE_MEMBERS
		// 1/18/2002- extra model indices
		if (s1->modelindex5)
		{
			// Knightmare added for Psychospaz's chasecam
			if (isclientviewer)
				ent.flags |= RF_VIEWERMODEL;

			// Knightmare added for mirroring
			if (renderfx & RF_MIRRORMODEL)
				ent.flags |= RF_MIRRORMODEL;

			ent.model = cl.model_draw[s1->modelindex5];
			V_AddEntity(&ent);
		}

		if (s1->modelindex6)
		{
			// Knightmare added for Psychospaz's chasecam
			if (isclientviewer)
				ent.flags |= RF_VIEWERMODEL;

			// Knightmare added for mirroring
			if (renderfx & RF_MIRRORMODEL)
				ent.flags |= RF_MIRRORMODEL;

			ent.model = cl.model_draw[s1->modelindex6];
			V_AddEntity(&ent);
		}
#endif
		
		if (effects & EF_POWERSCREEN)
		{
			ent.model = clMedia.mod_powerscreen;
			ent.oldframe = 0;
			ent.frame = 0;
			ent.flags |= (RF_TRANSLUCENT | RF_SHELL_GREEN);
			ent.alpha = 0.3f;

			V_AddEntity(&ent);
		}

		// Add automatic particle trails
		if (!(effects & EF_ROTATE))
		{
			if (effects & EF_ROCKET)
			{
				CL_RocketTrail(cent->lerp_origin, ent.origin, cent);
				V_AddLight(ent.origin, 200, 1.0f, 1.0f, 0.0f);
			}
			// PGM - Do not reorder EF_BLASTER and EF_HYPERBLASTER. EF_BLASTER | EF_TRACKER is a special case for EF_BLASTER2... Cheese!
			else if (effects & EF_BLASTER)
			{
				if (effects & EF_TRACKER) // Lame... problematic?
				{
					CL_BlasterTrail(cent->lerp_origin, ent.origin, tv(50, 235, 50), tv(-10, 0, -10));
					V_AddLight(ent.origin, 200, 0.15f, 1.0f, 0.15f);
				}
				//Knightmare- behold, the power of cheese!!
				else if (effects & EF_BLUEHYPERBLASTER) // EF_BLUEBLASTER
				{
					CL_BlasterTrail(cent->lerp_origin, ent.origin, tv(50, 50, 235), tv(-10, 0, -10));
					V_AddLight(ent.origin, 200, 0.15f, 0.15f, 1.0f);
				}
				//Knightmare- behold, the power of cheese!!
				else if (effects & EF_IONRIPPER) // EF_REDBLASTER
				{
					CL_BlasterTrail(cent->lerp_origin, ent.origin, tv(235, 50, 50), tv(0, -90, -30));
					V_AddLight(ent.origin, 200, 1.0f, 0.15f, 0.15f);
				}
				else
				{
					if ((effects & EF_GREENGIB) && cl_blood->integer >= 1) // EF_BLASTER|EF_GREENGIB effect
						CL_DiminishingTrail(cent->lerp_origin, ent.origin, cent, effects);
					else
						CL_BlasterTrail(cent->lerp_origin, ent.origin, tv(255, 150, 50), tv(0, -90, -30));
						
					V_AddLight(ent.origin, 200, 1.0f, 1.0f, 0.15f);
				}
			}
			else if (effects & EF_HYPERBLASTER)
			{
				if (effects & EF_TRACKER) // PGM overloaded for hyperblaster2
					V_AddLight(ent.origin, 200, 0.15f, 1.0f, 0.15f);
				else if (effects & EF_IONRIPPER) // Overloaded for EF_REDHYPERBLASTER
					V_AddLight(ent.origin, 200, 1.0f, 0.15f, 0.15f);
				else // PGM
					V_AddLight(ent.origin, 200, 1.0f, 1.0f, 0.15f);
			}
			else if (effects & EF_GIB)
			{
				if (cl_blood->integer >= 1)
					CL_DiminishingTrail(cent->lerp_origin, ent.origin, cent, effects);
			}
			else if (effects & EF_GRENADE)
			{
				CL_DiminishingTrail(cent->lerp_origin, ent.origin, cent, effects);
			}
			else if (effects & EF_FLIES)
			{
				CL_FlyEffect(cent, ent.origin);
			}
			else if (effects & EF_BFG)
			{
				static int bfg_lightramp[] = { 300, 400, 600, 300, 150, 75 };

				int intensity;
				if (effects & EF_ANIM_ALLFAST)
				{
					CL_BfgParticles(&ent);
					intensity = 200;
				}
				else
				{
					intensity = bfg_lightramp[s1->frame];
				}

				V_AddLight(ent.origin, intensity, 0.0f, 1.0f, 0.0f);
			}
			else if (effects & EF_TRAP) // RAFAEL
			{
				ent.origin[2] += 32;
				CL_TrapParticles(&ent);
				const int intensity = (rand() % 100) + 100;
				V_AddLight(ent.origin, intensity, 1.0f, 0.8f, 0.1f);
			}
			else if (effects & EF_FLAG1)
			{
				// Knightmare 1/3/2002
				// EF_FLAG1|EF_FLAG2 is a special case for EF_FLAG3...  More cheese!
				if (effects & EF_FLAG2)
				{
					//Knightmare- Psychospaz's enhanced particle code
					CL_FlagTrail(cent->lerp_origin, ent.origin, false, true);
					V_AddLight(ent.origin, 255, 0.1f, 1.0f, 0.1f);
				}
				else
				{
					//Knightmare- Psychospaz's enhanced particle code
					CL_FlagTrail(cent->lerp_origin, ent.origin, true, false);
					V_AddLight(ent.origin, 225, 1.0f, 0.1f, 0.1f);
				}
			}
			else if (effects & EF_FLAG2)
			{
				//Knightmare- Psychospaz's enhanced particle code
				CL_FlagTrail(cent->lerp_origin, ent.origin, false, false);
				V_AddLight(ent.origin, 225, 0.1f, 0.1f, 1.0f);
			}
			else if (effects & EF_TAGTRAIL) //ROGUE
			{
				CL_TagTrail(cent->lerp_origin, ent.origin, 220);
				V_AddLight(ent.origin, 225, 1.0f, 1.0f, 0.0f);
			}
			else if (effects & EF_TRACKERTRAIL) //ROGUE
			{
				if (effects & EF_TRACKER)
				{
					const float intensity = 50 + (500 * (sinf(cl.time / 500.0f) + 1.0f));
					V_AddLight(ent.origin, intensity, -1.0f, -1.0f, -1.0f);
				}
				else
				{
					CL_Tracker_Shell(cent->lerp_origin);
					V_AddLight(ent.origin, 155, -1.0f, -1.0f, -1.0f);
				}
			}
			else if (effects & EF_TRACKER) //ROGUE
			{
				//Knightmare- this is replaced for Psychospaz's enhanced particle code
				CL_TrackerTrail(cent->lerp_origin, ent.origin);
				V_AddLight(ent.origin, 200, -1.0f, -1.0f, -1.0f);
			}
			else if (effects & EF_GREENGIB) // RAFAEL
			{
				if (cl_blood->integer >= 1) // Disable blood option
					CL_DiminishingTrail(cent->lerp_origin, ent.origin, cent, effects);
			}
			else if (effects & EF_IONRIPPER) // RAFAEL
			{
				CL_IonripperTrail(cent->lerp_origin, ent.origin);
				V_AddLight(ent.origin, 100, 1.0f, 0.5f, 0.5f);
			}
			else if (effects & EF_BLUEHYPERBLASTER) // RAFAEL
			{
				V_AddLight(ent.origin, 200, 0.0f, 0.0f, 1.0f);
			}
			else if (effects & EF_PLASMA) // RAFAEL
			{
				if (effects & EF_ANIM_ALLFAST)
					CL_BlasterTrail(cent->lerp_origin, ent.origin, tv(255, 150, 50), tv(0, -90, -30));

				V_AddLight(ent.origin, 130, 1.0f, 0.5f, 0.5f);
			}
		}

		VectorCopy(ent.origin, cent->lerp_origin);
	}
}

#pragma endregion

#pragma region ======================= View and player weapon updating

static void CL_AddViewWeapon(player_state_t *ps, player_state_t *ops)
{
	entity_t gun; // View model

#ifdef NEW_PLAYER_STATE_MEMBERS //Knightmare- second gun model
	entity_t gun2; // View model
#endif

	// Don't draw if outside body...
	if (cg_thirdperson->integer && !(cl.attractloop && !(cl.cinematictime > 0 && cls.realtime - cl.cinematictime > 1000)))
		return;

	// Allow the gun to be completely removed
	if (!cl_gun->integer)
		return;

	// Don't draw gun if in wide angle view
	if (ps->fov > 180) //Knightmare 1/4/2002 - was 90
		return;

	memset(&gun, 0, sizeof(gun));
#ifdef NEW_PLAYER_STATE_MEMBERS //Knightmare- second gun model
	memset(&gun2, 0, sizeof(gun2));
#endif

	if (gun_model)
		gun.model = gun_model; // Development tool
	else
		gun.model = cl.model_draw[ps->gunindex];

#ifdef NEW_PLAYER_STATE_MEMBERS //Knightmare- second gun model
	gun2.model = cl.model_draw[ps->gunindex2];
	if (!gun.model && !gun2.model)
		return;
#else
	if (!gun.model)
		return;
#endif

	if (gun.model)
	{
		// Set up gun position
		for (int i = 0; i < 3; i++)
		{
			gun.origin[i] = cl.refdef.vieworg[i] + ops->gunoffset[i] + cl.lerpfrac * (ps->gunoffset[i] - ops->gunoffset[i]);
			gun.angles[i] = cl.refdef.viewangles[i] + LerpAngle(ops->gunangles[i], ps->gunangles[i], cl.lerpfrac);
		}

		if (gun_frame)
		{
			gun.frame = gun_frame; // Development tool
			gun.oldframe = gun_frame; // Development tool
		}
		else
		{
			gun.frame = ps->gunframe;
			gun.oldframe = (gun.frame == 0 ? 0 : ops->gunframe); // gun.frame == 0 : just changed weapons, don't lerp from old
		}

		//Knightmare- added changeable skin
#ifdef NEW_PLAYER_STATE_MEMBERS
		if (ps->gunskin)
			gun.skinnum = ops->gunskin;
#endif

		gun.flags = (RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL);
		gun.backlerp = 1.0f - cl.lerpfrac;
		VectorCopy(gun.origin, gun.oldorigin); // Don't lerp at all
		V_AddEntity(&gun);

		// Add shells for viewweaps (all of em!)
		if (cl_weapon_shells->value)
		{
			const int oldeffects = gun.flags;

			for (int pnum = 0; pnum < cl.frame.num_entities; pnum++)
			{
				entity_state_t *s1 = &cl_parse_entities[(cl.frame.parse_entities + pnum) & (MAX_PARSE_ENTITIES - 1)];
				
				if (s1->number == cl.playernum + 1)
				{
					if (s1->effects & (EF_PENT | EF_QUAD | EF_DOUBLE | EF_HALF_DAMAGE))
					{
						gun.flags = 0;

						if (s1->effects & EF_QUAD)
							gun.flags |= (oldeffects | RF_TRANSLUCENT | RF_SHELL_BLUE);

						if (s1->effects & EF_PENT)
							gun.flags |= (oldeffects | RF_TRANSLUCENT | RF_SHELL_RED);

						if ((s1->effects & EF_DOUBLE) && !(s1->effects & (EF_PENT | EF_QUAD)))
							gun.flags |= (oldeffects | RF_TRANSLUCENT | RF_SHELL_DOUBLE);

						if ((s1->effects & EF_HALF_DAMAGE) && !(s1->effects & (EF_PENT | EF_QUAD | EF_DOUBLE)))
							gun.flags |= (oldeffects | RF_TRANSLUCENT | RF_SHELL_HALF_DAM);

						gun.flags |= RF_TRANSLUCENT;
						gun.alpha = 0.3f;

						V_AddEntity(&gun);
					}

					break; // Early termination
				}
			}

			gun.flags = oldeffects;
		}
	}

	//Knightmare- second gun model
#ifdef NEW_PLAYER_STATE_MEMBERS
	if (gun2.model)
	{
		// set up gun2 position
		for (int i = 0; i < 3; i++)
		{
			gun2.origin[i] = cl.refdef.vieworg[i] + ops->gunoffset[i] + cl.lerpfrac * (ps->gunoffset[i] - ops->gunoffset[i]);
			gun2.angles[i] = cl.refdef.viewangles[i] + LerpAngle(ops->gunangles[i], ps->gunangles[i], cl.lerpfrac);
		}

		gun2.frame = ps->gunframe2;
		gun2.oldframe = (gun2.frame == 0 ? 0 : ops->gunframe2); // gun2.frame == 0 : just changed weapons, don't lerp from old

		if (ps->gunskin2)
			gun2.skinnum = ops->gunskin2;

		gun2.flags = (RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL);
		gun2.backlerp = 1.0f - cl.lerpfrac;
		VectorCopy(gun2.origin, gun2.oldorigin); // Don't lerp at all
		V_AddEntity(&gun2);

		// Add shells for viewweaps (all of em!)
		if (cl_weapon_shells->value)
		{
			const int oldeffects = gun2.flags;
			
			for (int pnum = 0; pnum < cl.frame.num_entities; pnum++)
			{
				entity_state_t *s1 = &cl_parse_entities[(cl.frame.parse_entities + pnum) & (MAX_PARSE_ENTITIES - 1)];
				
				if (s1->number == cl.playernum + 1)
				{

					if (s1->effects & (EF_PENT | EF_QUAD | EF_DOUBLE | EF_HALF_DAMAGE))
					{
						gun2.flags = 0;

						if (s1->effects & EF_QUAD)
							gun2.flags |= (oldeffects | RF_TRANSLUCENT | RF_SHELL_BLUE);

						if (s1->effects & EF_PENT)
							gun2.flags |= (oldeffects | RF_TRANSLUCENT | RF_SHELL_RED);

						if ((s1->effects & EF_DOUBLE) && !(s1->effects & (EF_PENT | EF_QUAD)))
							gun2.flags |= (oldeffects | RF_TRANSLUCENT | RF_SHELL_DOUBLE);

						if ((s1->effects & EF_HALF_DAMAGE) && !(s1->effects & (EF_PENT | EF_QUAD | EF_DOUBLE)))
							gun2.flags |= (oldeffects | RF_TRANSLUCENT | RF_SHELL_HALF_DAM);

						gun2.flags |= RF_TRANSLUCENT;
						gun2.alpha = 0.3f;

						V_AddEntity(&gun2);
					}

					break; // Early termination
				}
			}

			gun2.flags = oldeffects;
		}
	}
#endif
}

extern void CalcViewerCamTrans(float dist);
extern void ClipCam(vec3_t start, vec3_t end, vec3_t newpos);

static void SetUpCamera()
{
	if (cg_thirdperson->integer && !(cl.attractloop && !(cl.cinematictime > 0 && cls.realtime - cl.cinematictime > 1000)))
	{
		vec3_t end, oldorg, camposition, camforward;

		if (cg_thirdperson_dist->value < 0)
			Cvar_SetValue("cg_thirdperson_dist", 0);

		// Knightmare- backup old viewangles
		VectorCopy(cl.refdef.viewangles, old_viewangles);

		// And who said trig was pointless?
		const float angle = M_PI * cg_thirdperson_angle->value / 180.0f;
		const float dist_up = cg_thirdperson_dist->value * sinf(angle);
		const float dist_back = cg_thirdperson_dist->value * cosf(angle);

		VectorCopy(cl.refdef.vieworg, oldorg);
		if (cg_thirdperson_chase->value)
		{
			VectorMA(cl.refdef.vieworg, -dist_back, cl.v_forward, end);
			VectorMA(end, dist_up, cl.v_up, end);

			// Move back so looking straight down will make us look towards ourself
			vec3_t temp;
			vectoangles(cl.v_forward, temp);
			temp[PITCH] = 0;
			temp[ROLL] = 0;

			vec3_t temp2;
			AngleVectors(temp, temp2, NULL, NULL);
			VectorMA(end, -(dist_back / 1.8f), temp2, end);
		}
		else
		{
			vec3_t temp;
			vectoangles(cl.v_forward, temp);
			temp[PITCH] = 0;
			temp[ROLL] = 0;

			vec3_t viewForward, viewUp;
			AngleVectors(temp, viewForward, NULL, viewUp);

			VectorScale(viewForward, dist_up * 0.5f, camforward);

			VectorMA(cl.refdef.vieworg, -dist_back, viewForward, end);
			VectorMA(end, dist_up, viewUp, end);
		}

		ClipCam(cl.refdef.vieworg, end, camposition);
		VectorSubtract(camposition, oldorg, end);
		CalcViewerCamTrans(VectorLength(end));

		if (cg_thirdperson_chase->value) // Now aim at where ever client is...
		{
			if (cg_thirdperson_adjust->value)
			{
				vec3_t newDir, dir;
				VectorMA(cl.refdef.vieworg, 8000, cl.v_forward, dir);
				ClipCam(cl.refdef.vieworg, dir, newDir);

				VectorSubtract(newDir, camposition, dir);
				VectorNormalize(dir);
				vectoangles(dir, newDir);

				AngleVectors(newDir, cl.v_forward, cl.v_right, cl.v_up);
				VectorCopy(newDir, cl.refdef.viewangles);
			}

			VectorCopy(camposition, cl.refdef.vieworg);
		}
		else
		{
			vec3_t newDir[2], newPos;

			VectorSubtract(cl.predicted_origin, camposition, newDir[0]);
			VectorNormalize(newDir[0]);
			vectoangles(newDir[0], newDir[1]);
			VectorCopy(newDir[1], cl.refdef.viewangles);

			VectorAdd(camforward, cl.refdef.vieworg, newPos);
			ClipCam(cl.refdef.vieworg, newPos, cl.refdef.vieworg);
		}
	}
}

// Sets cl.refdef view values
static void CL_CalcViewValues()
{
	// Find the previous frame to interpolate from
	const int frame = (cl.frame.serverframe - 1) & UPDATE_MASK;
	frame_t *oldframe = &cl.frames[frame];

	if (oldframe->serverframe != cl.frame.serverframe - 1 || !oldframe->valid)
		oldframe = &cl.frame; // Previous frame was dropped or invalid

	player_state_t *ops = &oldframe->playerstate;

	// See if the player entity was teleported this frame
	player_state_t *ps = &cl.frame.playerstate;

	for(int i = 0; i < 3; i++)
	{
		if (abs(ops->pmove.origin[i] - ps->pmove.origin[i]) > 256 * 8)
		{
			ops = ps; // Don't interpolate
			break;
		}
	}

	const float lerp = cl.lerpfrac;

	// Calculate the origin
	if (cl_predict->integer && !(cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION) && !cl.attractloop) // Jay Dolan fix- so long as we're not viewing a demo 
	{
		const float backlerp = 1.0f - lerp;

		for (int i = 0; i < 3; i++)
		{
			cl.refdef.vieworg[i] = cl.predicted_origin[i] + ops->viewoffset[i] + cl.lerpfrac * (ps->viewoffset[i] - ops->viewoffset[i]) - backlerp * cl.prediction_error[i];

			// This smoothes out platform riding
			cl.predicted_origin[i] -= backlerp * cl.prediction_error[i];
		}

		// Smooth out stair climbing
		const uint delta = cls.realtime - cl.predicted_step_time;
		if (delta < 100)
		{
			cl.refdef.vieworg[2] -= cl.predicted_step * (100 - delta) * 0.01f;
			cl.predicted_origin[2] -= cl.predicted_step * (100 - delta) * 0.01f;
		}
	}
	else
	{
		// Just use interpolated values
		for (int i = 0; i < 3; i++)
		{
			cl.refdef.vieworg[i] = ops->pmove.origin[i] * 0.125f + ops->viewoffset[i]
									+ lerp * (ps->pmove.origin[i] * 0.125f + ps->viewoffset[i]
									- (ops->pmove.origin[i] * 0.125f + ops->viewoffset[i]));
		}

		// Knightmare- set predicted origin anyway, it's needed for the chasecam
		VectorCopy(cl.refdef.vieworg, cl.predicted_origin);
		cl.predicted_origin[2] -= ops->viewoffset[2]; 
	}

	// If not running a demo or on a locked frame, add the local angle movement
	if (cl.frame.playerstate.pmove.pm_type < PM_DEAD)
	{
		// Use predicted values
		for (int i = 0; i < 3; i++)
			cl.refdef.viewangles[i] = cl.predicted_angles[i];
	}
	else
	{
		// Just use interpolated values
		for (int i = 0; i < 3; i++)
			cl.refdef.viewangles[i] = LerpAngle(ops->viewangles[i], ps->viewangles[i], lerp);
	}

	// Add kick angles
	for (int i = 0; i < 3; i++)
		cl.refdef.viewangles[i] += LerpAngle(ops->kick_angles[i], ps->kick_angles[i], lerp);

	AngleVectors(cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up);

	// Interpolate field of view
	cl.refdef.fov_x = cl.base_fov = ops->fov + lerp * (ps->fov - ops->fov);

	// Don't interpolate blend color
	for (int i = 0; i < 4; i++)
		cl.refdef.blend[i] = ps->blend[i];

	// Add the weapon
	CL_AddViewWeapon(ps, ops);

	// Set up chase cam
	SetUpCamera();
}

#pragma endregion

// Emits all entities, particles, and lights to the refresh
void CL_AddEntities()
{
	if (cls.state != ca_active)
		return;

	if (cl.time > cl.frame.servertime)
	{
		if (cl_showclamp->integer)
			Com_Printf("High clamp %i\n", cl.time - cl.frame.servertime);

		cl.time = cl.frame.servertime;
		cl.lerpfrac = 1.0f;
	}
	else if (cl.time < cl.frame.servertime - 100)
	{
		if (cl_showclamp->integer)
			Com_Printf("Low clamp %i\n", cl.frame.servertime - 100 - cl.time);

		cl.time = cl.frame.servertime - 100;
		cl.lerpfrac = 0.0f;
	}
	else
	{
		cl.lerpfrac = 1.0f - (cl.frame.servertime - cl.time) * 0.01f;
	}

	if (cl_timedemo->integer)
		cl.lerpfrac = 1.0f;

	CL_CalcViewValues();

	// PMM - moved this here so the heat beam has the right values for the vieworg, and can lock the beam to the gun
	CL_AddPacketEntities(&cl.frame);

#ifdef LOC_SUPPORT // Xile/NiceAss LOC
	CL_AddViewLocs();
#endif

	CL_AddTEnts();
	CL_AddParticles();
	CL_AddDLights();
	CL_AddLightStyles();
}

// Called to get the sound spatialization origin
void CL_GetEntitySoundOrigin(const int entindex, vec3_t org)
{
	if (entindex < 0 || entindex >= MAX_EDICTS)
		Com_Error(ERR_DROP, "CL_GetEntitySoundOrigin: bad ent");

	centity_t *ent = &cl_entities[entindex];

	//mxd. Find correct origin for bmodels without origin brushes
	if(ent->current.solid == 31)
	{
		cmodel_t* cmodel = cl.model_clip[ent->current.modelindex];
		if (cmodel)
		{
			for (int i = 0; i < 3; i++)
				org[i] = ent->current.origin[i] + 0.5f * (cmodel->mins[i] + cmodel->maxs[i]);

			return;
		}
	}

	VectorCopy(ent->lerp_origin, org);
}