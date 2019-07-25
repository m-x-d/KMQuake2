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
// server.h

#pragma once

//#define PARANOID // speed sapping error checking
#include "../qcommon/qcommon.h"
#include "../game/game.h"

#pragma region ======================= Server structs and defines

#define MAX_MASTERS	8 // Max recipients for heartbeat packets

// Some qc commands are only valid before the server has finished
// initializing (precache commands, static sounds / objects, etc.).
typedef enum
{
	ss_dead,	// No map loaded
	ss_loading,	// Spawning level edicts
	ss_game,	// Actively running
	ss_cinematic,
	ss_demo,
	ss_pic
} server_state_t;

typedef struct
{
	server_state_t state; // Precache commands are only valid during load

	qboolean attractloop; // Running cinematics and demos for the local system only
	qboolean loadgame; // Client begins should reuse existing entity

	uint time; // Always sv.framenum * 100 msec
	int framenum;

	char name[MAX_QPATH]; // Map or cinematic name
	struct cmodel_s *models[MAX_MODELS];

	char configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];
	entity_state_t baselines[MAX_EDICTS];

	// The multicast buffer is used to send a message to a set of clients.
	// It is only used to marshall data until SV_Multicast is called.
	sizebuf_t multicast;
	byte multicast_buf[MAX_MSGLEN];

	// Demo server information
	fileHandle_t demofile;
	qboolean timedemo; // Don't time sync
} server_t;

#define EDICT_NUM(n) ((edict_t *)((byte *)ge->edicts + ge->edict_size*(n)))
#define NUM_FOR_EDICT(e) ( ((byte *)(e)-(byte *)ge->edicts ) / ge->edict_size)

typedef enum
{
	cs_free, // Can be reused for a new connection.
	cs_zombie, // Client has been disconnected, but don't reuse connection for a couple seconds.
	cs_connected, // Has been assigned to a client_t, but not in game yet.
	cs_spawned // Client is fully in game.
} client_state_t;

typedef struct
{
	int areabytes;
	byte areabits[MAX_MAP_AREAS/8]; // portalarea visibility bits
	player_state_t ps;
	int num_entities;
	int first_entity; // Into the circular sv_packet_entities[]
	int senttime; // For ping calculations
} client_frame_t;

#define LATENCY_COUNTS	16
#define RATE_MESSAGES	10

// A client can leave the server in one of four ways:
// - dropping properly by quiting or disconnecting;
// - timing out if no valid messages are received for timeout.value seconds;
// - getting kicked off by the server operator;
// - a program error, like an overflowed reliable buffer.
typedef struct client_s
{
	client_state_t state;

	char userinfo[MAX_INFO_STRING]; // Name, etc.

	int lastframe; // For delta compression.
	usercmd_t lastcmd; // For filling in big drops.

	int commandMsec; // Reset every few seconds, if user commands exhaust it, assume time cheating.

	int frame_latency[LATENCY_COUNTS];
	int ping;

	int message_size[RATE_MESSAGES]; // Used to rate drop packets.
	int rate;
	int surpressCount; // Number of messages rate supressed.

	edict_t *edict; // EDICT_NUM(clientnum+1)
	char name[32]; // Extracted from userinfo, high bits masked.
	int messagelevel; // For filtering printed messages.

	// The datagram is written to by sound calls, prints, temp ents, etc.
	// It can be harmlessly overflowed.
	sizebuf_t datagram;
	byte datagram_buf[MAX_MSGLEN];

	client_frame_t frames[UPDATE_BACKUP]; // Updates can be delta'd from here.

	byte *download; // File being downloaded.
	int downloadsize; // Total bytes (can't use EOF because of paks).
	int downloadcount; // Bytes sent.

	int lastmessage; // sv.framenum when packet was last received.
	int lastconnect;

	int challenge; // Challenge of this user, randomly generated.

	netchan_t netchan;
} client_t;

//=============================================================================

// MAX_CHALLENGES is made large to prevent a denial of service attack that could cycle all of them out before legitimate users connected
#define MAX_CHALLENGES	1024

typedef struct
{
	netadr_t adr;
	int challenge;
	int time;
} challenge_t;

typedef struct
{
	qboolean initialized; // sv_init has completed.
	int realtime; // Always increasing, no clamping, etc.

	char mapcmd[MAX_TOKEN_CHARS]; // Ie: *intro.cin+base 

	int spawncount; // Incremented each server start. Used to check late spawns.

	client_t *clients; // [maxclients->value];
	int num_client_entities; // maxclients->value * UPDATE_BACKUP * MAX_PACKET_ENTITIES
	int next_client_entities; // Next client_entity to use.
	entity_state_t *client_entities; // [num_client_entities]

	int last_heartbeat;

	challenge_t challenges[MAX_CHALLENGES]; // To prevent invalid IPs from connecting.

	// serverrecord values
	FILE *demofile;
	sizebuf_t demo_multicast;
	byte demo_multicast_buf[MAX_MSGLEN];
} server_static_t;

#pragma endregion

#pragma region ======================= Server vars & cvars

extern netadr_t master_adr[MAX_MASTERS]; // Address of the master server

extern server_static_t svs; // Persistant server info
extern server_t sv; // Local server

extern client_t *sv_client;
extern edict_t *sv_player;

extern cvar_t *sv_paused;
extern cvar_t *maxclients;
extern cvar_t *sv_noreload; // Don't reload level state when reentering
extern cvar_t *sv_airaccelerate;
extern cvar_t *sv_enforcetime;

extern cvar_t *allow_download;
extern cvar_t *allow_download_players;
extern cvar_t *allow_download_models;
extern cvar_t *allow_download_sounds;
extern cvar_t *allow_download_maps;
extern cvar_t *allow_download_pics;
extern cvar_t *allow_download_textures;
extern cvar_t *allow_download_textures_24bit; // Whether to allow downloading 24-bit textures

extern cvar_t *sv_downloadserver; // From R1Q2

extern cvar_t *sv_baselines_maxlen; // Knightmare- max packet size for connect messages
extern cvar_t *sv_limit_msglen; // Knightmare- whether to use MAX_MSGLEN_MP for multiplayer games

#pragma endregion

#pragma region ======================= Server function declarations

//
// sv_main.c
//
void SV_DropClient(client_t *drop);
void SV_DropClientFromAdr(const netadr_t address); // Knightmare added

int SV_ModelIndex(char *name);
int SV_SoundIndex(char *name);
int SV_ImageIndex(char *name);

void SV_ExecuteUserCommand(char *s);
void SV_InitOperatorCommands();

void Master_Heartbeat();

//
// sv_init.c
//
void SV_InitGame();
void SV_Map(const qboolean attractloop, const char *levelstring, const qboolean loadgame);

//
// sv_send.c
//
typedef enum
{
	RD_NONE,
	RD_CLIENT,
	RD_PACKET
} redirect_t;

void SV_FlushRedirect(int sv_redirected, char *outputbuf);

void SV_DemoCompleted();
void SV_SendClientMessages();

void SV_Multicast(vec3_t origin, multicast_t to);
void SV_StartSound(vec3_t origin, edict_t *entity, int channel, int soundindex, float volume, float attenuation, float timeofs);
void SV_ClientPrintf(client_t *cl, int level, char *fmt, ...);
void SV_BroadcastPrintf(int level, char *fmt, ...);
void SV_BroadcastCommand(char *fmt, ...);

//
// sv_user.c
//
void SV_Nextserver();
void SV_UserinfoChanged(client_t *cl);
void SV_ExecuteClientMessage(client_t *cl);

//
// sv_ccmds.c
//
void SV_ReadLevelFile();

//
// sv_ents.c
//
void SV_WriteFrameToClient(client_t *client, sizebuf_t *msg);
void SV_RecordDemoMessage();
void SV_BuildClientFrame(client_t *client);

//
// sv_game.c
//
extern game_export_t *ge;

void SV_InitGameProgs();
void SV_ShutdownGameProgs();

//
// sv_world.c
//

// Called after the world model has been loaded, before linking any entities
void SV_ClearWorld();

// Call before removing an entity, and before trying to move one, so it doesn't clip against itself
void SV_UnlinkEdict(edict_t *ent);

// Needs to be called any time an entity changes origin, mins, maxs, or solid.
// Automatically unlinks if needed.
// sets ent->v.absmin and ent->v.absmax
// sets ent->leafnums[] for pvs determination even if the entity is not solid
void SV_LinkEdict(edict_t *ent);

// Fills in a table of edict pointers with edicts that have bounding boxes that intersect the given area.
// It is possible for a non-axial bmodel to be returned that doesn't actually intersect the area on an exact test.
// Returns the number of pointers filled in.
// ??? does this always return the world?
int SV_AreaEdicts(vec3_t mins, vec3_t maxs, edict_t **list, const int maxcount, const int areatype);

// Returns the CONTENTS_* value from the world at the given point.
// Quake 2 extends this to also check entities, to allow moving liquids
int SV_PointContents(vec3_t p);

// mins and maxs are relative.
// If the entire move stays in a solid volume, trace.allsolid will be set, trace.startsolid will be set, and trace.fraction will be 0.
// If the starting point is in a solid, it will be allowed to move out to an open area.
// passedict is explicitly excluded from clipping checks (normally NULL).
trace_t SV_Trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passedict, const int contentmask);

#pragma endregion