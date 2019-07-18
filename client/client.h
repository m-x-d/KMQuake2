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
// client.h -- primary header for client

#pragma once

//#define PARANOID // Speed sapping error checking

#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "ref.h"

#include "vid.h"
#include "screen.h"
#include "sound.h"
#include "input.h"
#include "keys.h"
#include "console.h"

// HTTP downloading from R1Q2
#ifdef USE_CURL
	#ifdef _WIN32
		#define CURL_STATICLIB
		#define CURL_HIDDEN_SYMBOLS
		#define CURL_EXTERN_SYMBOL
		#define CURL_CALLING_CONVENTION __cdecl
	#endif

	#define CURL_STATICLIB
	#include "../include/curl/curl.h"
	#define CURL_ERROR(x)	curl_easy_strerror(x)
#endif

typedef int cinHandle_t; // ROQ support

//=============================================================================

typedef struct
{
	qboolean valid; // False if delta parsing was invalid
	int serverframe;
	int servertime; // Server time the message is valid for (in msec)
	int deltaframe;
	byte areabits[MAX_MAP_AREAS / 8]; // Portalarea visibility bits
	player_state_t playerstate;
	int num_entities;
	int parse_entities; // Non-masked index into cl_parse_entities array
} frame_t;

typedef struct
{
	entity_state_t baseline; // Delta from this if not from a previous frame
	entity_state_t current;
	entity_state_t prev; // Always valid, but might just be a copy of current

	int serverframe; // If not current, this ent isn't in the frame

	int trailcount; // For diminishing grenade trails
	vec3_t lerp_origin; // For trails (variable hz)

	int fly_stoptime; // Corpse files effect timing
} centity_t;

#define MAX_CLIENTWEAPONMODELS	64 // PGM -- upped from 16 to fit the chainfist vwep // 12/23/2001- increased this from 20

typedef struct
{
	char name[MAX_QPATH];
	char cinfo[MAX_QPATH];

	struct image_s *skin;
	struct image_s *icon;
	char iconname[MAX_QPATH];

	struct model_s *model;
	struct model_s *weaponmodel[MAX_CLIENTWEAPONMODELS];
} clientinfo_t;

extern char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
extern int num_cl_weaponmodels;

#define CMD_BACKUP	64 // Allow a lot of command backups for very fast systems

#ifdef USE_CURL	// HTTP downloading from R1Q2

void CL_CancelHTTPDownloads(qboolean permKill);
void CL_InitHTTPDownloads(void);
qboolean CL_QueueHTTPDownload(const char *quakePath);
void CL_RunHTTPDownloads(void);
qboolean CL_PendingHTTPDownloads(void);
void CL_SetHTTPServer(const char *URL);
void CL_HTTP_Cleanup(qboolean fullShutdown);
void CL_HTTP_ResetMapAbort(void);	// Knightmare added

typedef enum
{
	DLQ_STATE_NOT_STARTED,
	DLQ_STATE_RUNNING,
	DLQ_STATE_DONE
} dlq_state;

typedef struct dlqueue_s
{
	struct dlqueue_s *next;
	char quakePath[MAX_QPATH];
	qboolean isPak; // Knightmare added
	dlq_state state;
} dlqueue_t;

typedef struct dlhandle_s
{
	CURL *curl;
	char filePath[MAX_OSPATH];
	FILE *file;
	dlqueue_t *queueEntry;
	size_t fileSize;
	size_t position;
	double speed;
	char URL[576];
	char *tempBuffer;
} dlhandle_t;

#endif	// USE_CURL

// The client_state_t structure is wiped completely at every server map change
typedef struct
{
	int timeoutcount;

	int timedemo_frames;
	int timedemo_start;

	qboolean refresh_prepped; // False if on new level or new ref dll
	qboolean sound_prepped; // Ambient sounds can start
	qboolean force_refdef; // Vid has changed, so we can't use a paused refdef

	int parse_entities; // Index (not anded off) into cl_parse_entities[]

	usercmd_t cmds[CMD_BACKUP]; // Each mesage will send several old cmds
	int cmd_time[CMD_BACKUP]; // Time sent, for calculating pings

#ifdef LARGE_MAP_SIZE // Larger precision needed
	int predicted_origins[CMD_BACKUP][3]; // For debug comparing against server
#else
	short predicted_origins[CMD_BACKUP][3]; // For debug comparing against server
#endif

	float predicted_step; // For stair up smoothing
	uint predicted_step_time;

	// Generated by CL_PredictMovement
	vec3_t predicted_origin;
	vec3_t predicted_angles;
	vec3_t prediction_error;

	frame_t frame; // Received from server
	int surpressCount; // Number of messages rate supressed
	frame_t frames[UPDATE_BACKUP];

	// The client maintains its own idea of view angles, which are sent to the server each frame.
	// It is cleared to 0 upon entering each level.
	// The server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects, and teleport direction changes.
	vec3_t viewangles;

	int time; // This is the time value that the client is rendering at. Always <= cls.realtime
	float lerpfrac; // Fraction between oldframe and frame

	float base_fov; // The fov set by the game code, unaltered by widescreen scaling or other effects

	refdef_t refdef;

	// Set when refdef.angles is set
	vec3_t v_forward;
	vec3_t v_right;
	vec3_t v_up;

	// Transient data from server
	char layout[1024]; // General 2D overlay
	int inventory[MAX_ITEMS];

	// Non-gameserver infornamtion
	// FIXME: move this cinematic stuff into the cin_t structure
	int cinematictime; // cls.realtime for first cinematic frame
	int cinematicframe;

	// Server state information
	qboolean attractloop; // Running the attract loop, any key will open main menu
	int servercount; // Server identification for prespawns
	char gamedir[MAX_QPATH];
	int playernum;
	int maxclients; // From R1Q2

	char configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];

	// Locally derived information from server state
	struct model_s *model_draw[MAX_MODELS];
	struct cmodel_s *model_clip[MAX_MODELS];

	struct sfx_s *sound_precache[MAX_SOUNDS];
	struct image_s *image_precache[MAX_IMAGES];

	clientinfo_t clientinfo[MAX_CLIENTS];
	clientinfo_t baseclientinfo;
} client_state_t;

extern client_state_t cl;

typedef enum
{
	ca_uninitialized,
	ca_disconnected, // Not talking to a server
	ca_connecting, // Sending request packets to the server
	ca_connected, // netchan_t established, waiting for svc_serverdata
	ca_active // Game view should be displayed
} connstate_t;

typedef enum
{
	dl_none,
	dl_model,
	dl_sound,
	dl_skin,
	dl_single
} dltype_t; // Download type

typedef enum
{
	key_game,
	key_console,
	key_message,
	key_menu
} keydest_t;

// The client_static_t structure is persistant through an arbitrary number of server connections.
typedef struct
{
	connstate_t state;
	keydest_t key_dest;

	qboolean consoleActive;

	int framecount;
	int realtime; // Always increasing, no clamping, etc.
	float netFrameTime; // Seconds since last packet frame.
	float renderFrameTime; // Seconds since last refresh frame.

	// Screen rendering information
	int disable_screen; // Showing loading plaque between levels or changing rendering dlls. If time gets > 30 seconds ahead, break it.
	int disable_servercount; // When we receive a frame and cl.servercount > cls.disable_servercount, clear disable_screen.

	// Connection information
	char servername[MAX_OSPATH]; // Name of server from original connect
	float connect_time; // For connection retransmits

	int quakePort; // A 16 bit value that allows quake servers to work around address translating routers

	netchan_t netchan;
	int serverProtocol; // In case we are doing some kind of version hack

	int challenge; // From the server to use for connecting

	qboolean forcePacket; // Forces a packet to be sent the next frame

	FILE *download; // File transfer from server
	char downloadtempname[MAX_OSPATH];
	char downloadname[MAX_OSPATH];
	int downloadnumber;
	dltype_t downloadtype;
	size_t downloadposition; // Added for HTTP downloads
	int downloadpercent;
	float downloadrate; // Knightmare- to display KB/s

	// Demo recording info must be here, so it isn't cleared on level change
	qboolean demorecording;
	qboolean demowaiting; // Don't record until a non-delta message is received
	FILE *demofile;

	// Cinematic information (ROQ support)
	cinHandle_t cinematicHandle;

#ifdef USE_CURL	// HTTP downloading from R1Q2
	dlqueue_t downloadQueue; // Queue of paths we need
	
	dlhandle_t HTTPHandles[4]; // Actual download handles
	// Don't raise this!  I use a hardcoded maximum of 4 simultaneous connections to avoid overloading the server.

	char downloadServer[512]; // Base url prefix to download from
	
	// FS: Added because Whale's Weapons HTTP server rejects you after a lot of 404s. Then you lose HTTP until a hard reconnect.
	char downloadServerRetry[512];
	char downloadReferer[32]; // Libcurl requires a static string :(
#endif // USE_CURL
} client_static_t;

extern client_static_t cls;

#pragma region ======================= cvars

extern cvar_t *cl_stereo_separation;
extern cvar_t *cl_stereo;

extern cvar_t *cl_gun;
extern cvar_t *cl_weapon_shells;
extern cvar_t *cl_add_blend;
extern cvar_t *cl_add_lights;
extern cvar_t *cl_add_particles;
extern cvar_t *cl_add_entities;
extern cvar_t *cl_predict;
extern cvar_t *cl_footsteps;
extern cvar_t *cl_noskins;

// Reduction factor for particle effects
extern cvar_t *cl_particle_scale;

// Whether to adjust fov for wide aspect rattio
extern cvar_t *cl_widescreen_fov;

extern cvar_t *con_alpha; // Psychospaz's transparent console
extern cvar_t *con_newconback; // Whether to use new console background
extern cvar_t *con_oldconbar; // Whether to draw bottom bar on old console

// Psychospaz's chasecam
extern cvar_t *cg_thirdperson;
extern cvar_t *cg_thirdperson_angle;
extern cvar_t *cg_thirdperson_chase;
extern cvar_t *cg_thirdperson_dist;
extern cvar_t *cg_thirdperson_alpha;
extern cvar_t *cg_thirdperson_adjust;

extern cvar_t *cl_blood;
extern cvar_t *cl_old_explosions; // Option for old explosions
extern cvar_t *cl_plasma_explo_sound; // Option for unique plasma explosion sound
extern cvar_t *cl_item_bobbing; // Option for bobbing items

// Psychospaz's changeable rail code
extern cvar_t *cl_railred;
extern cvar_t *cl_railgreen;
extern cvar_t *cl_railblue;
extern cvar_t *cl_railtype;
extern cvar_t *cl_rail_length;
extern cvar_t *cl_rail_space;

// Whether to use texsurfs.txt footstep sounds
extern cvar_t *cl_footstep_override;

extern cvar_t *r_decals; // Decal control
extern cvar_t *r_decal_life; // Decal duration in seconds

extern cvar_t *r_particle_mode; // mxd

extern cvar_t *con_font_size;
extern cvar_t *alt_text_color;

//Knightmare 12/28/2001- BramBo's FPS counter
extern cvar_t *cl_drawfps;

extern cvar_t *cl_rogue_music; // Whether to play Rogue tracks
extern cvar_t *cl_xatrix_music; // Whether to play Xatrix tracks
// end Knightmare

extern cvar_t *cl_servertrick;

extern cvar_t *cl_upspeed;
extern cvar_t *cl_forwardspeed;
extern cvar_t *cl_sidespeed;

extern cvar_t *cl_yawspeed;
extern cvar_t *cl_pitchspeed;

extern cvar_t *cl_run;

extern cvar_t *cl_anglespeedkey;

extern cvar_t *cl_shownet;
extern cvar_t *cl_showmiss;
extern cvar_t *cl_showclamp;

extern cvar_t *lookspring;
extern cvar_t *lookstrafe;
extern cvar_t *sensitivity;
extern cvar_t *menu_sensitivity;
extern cvar_t *menu_rotate;
extern cvar_t *menu_alpha;
extern cvar_t *hud_scale;
extern cvar_t *hud_width;
extern cvar_t *hud_height;
extern cvar_t *hud_alpha;

extern cvar_t *m_pitch;
extern cvar_t *m_yaw;
extern cvar_t *m_forward;
extern cvar_t *m_side;

extern cvar_t *freelook;

extern cvar_t *cl_lightlevel; // FIXME HACK

extern cvar_t *cl_paused;
extern cvar_t *cl_timedemo;

#ifdef CLIENT_SPLIT_NETFRAME
extern cvar_t *cl_async;
#endif

// Knighthare added
extern cvar_t *info_password;
extern cvar_t *info_spectator;
extern cvar_t *info_name;
extern cvar_t *info_skin;
extern cvar_t *info_rate;
extern cvar_t *info_fov;
extern cvar_t *info_msg;
extern cvar_t *info_hand;
extern cvar_t *info_gender;
extern cvar_t *info_gender_auto;
// end Knightmare

extern cvar_t *cl_vwep;

// for the server to tell which version the client is
extern cvar_t *cl_engine;
extern cvar_t *cl_engine_version;

#ifdef LOC_SUPPORT	// Xile/NiceAss LOC
extern cvar_t *cl_drawlocs;
extern cvar_t *loc_here;
extern cvar_t *loc_there;
#endif // LOC_SUPPORT

#ifdef USE_CURL	// HTTP downloading from R1Q2
extern cvar_t *cl_http_downloads;
extern cvar_t *cl_http_filelists;
extern cvar_t *cl_http_proxy;
extern cvar_t *cl_http_max_connections;
#endif

#pragma endregion

typedef struct
{
	int key; // So entities can reuse same entry
	vec3_t color;
	vec3_t origin;
	float radius;
	float die; // Stop lighting after this time
	float decay; // Drop this each second
	float minlight; // Don't add when contributing less
} cdlight_t;

extern centity_t cl_entities[MAX_EDICTS];

// The cl_parse_entities must be large enough to hold UPDATE_BACKUP frames of entities,
// so that when a delta compressed message arives from the server it can be un-deltad from the original.
#define MAX_PARSE_ENTITIES	4096 //was 16384

extern entity_state_t cl_parse_entities[MAX_PARSE_ENTITIES];

//=============================================================================

// For use with the alt_text_color cvar
void TextColor(int colornum, int *red, int *green, int *blue);
qboolean StringSetParams(char modifier, int *red, int *green, int *blue, qboolean *bold, qboolean *shadow, qboolean *italic, qboolean *reset);
qboolean IsColoredString(const char *s); //mxd
int CL_UnformattedStringLength(const char *string); //mxd
char *CL_UnformattedString(const char *string); //mxd
int CL_StringLengthExtra(const char *string); //mxd
void Con_DrawString(int x, int y, char *string, int alpha);
void DrawStringGeneric(int x, int y, const char *string, int alpha, textscaletype_t scaleType, qboolean altBit);

//ROGUE
typedef struct cl_sustain
{
	int id;
	int type;
	int endtime;
	int nextthink;
	int thinkinterval;
	vec3_t org;
	vec3_t dir;
	int color;
	int count;
	int magnitude;
	void (*think)(struct cl_sustain *self);
} cl_sustain_t;

void CL_ParticleSteamEffect2(cl_sustain_t *self);

void CL_TeleporterParticles(entity_state_t *ent);
void CL_ParticleEffect(const vec3_t org, const vec3_t dir, const int color8, const int count);
void CL_ParticleEffect2(const vec3_t org, const vec3_t dir, const int color8, const int count, const qboolean invertgravity); //mxd. +invertgravity

void CL_ParticleEffectSplash(const vec3_t org, const vec3_t dir, const int color8, const int count);
void CL_ElectricParticles(const vec3_t org, const vec3_t dir, const int count);
void CL_GunSmokeEffect(const vec3_t org, const vec3_t dir);
void CL_LogoutEffect(const vec3_t org, const int type);

// Psychospaz's mod detector
qboolean FS_ModType(char *name);
qboolean FS_RoguePath();

// Utility function for protocol version
qboolean LegacyProtocol();

#pragma region ======================= Psychospaz enhanced particle code

typedef struct
{
	qboolean isactive;

	vec3_t lightcol;
	float light;
	float lightvel;
} cplight_t;

#define P_LIGHTS_MAX 8

typedef struct particle_s
{
	struct particle_s *next;

	cplight_t lights[P_LIGHTS_MAX];

	float start;
	float time;

	vec3_t org;
	vec3_t vel;
	vec3_t accel;

	vec3_t color;
	vec3_t colorvel;

	int blendfunc_src;
	int blendfunc_dst;

	float alpha;
	float alphavel;

	float size;
	float sizevel;

	vec3_t angle;
	
	int image;
	int flags;

	vec3_t oldorg;
	float temp;
	int src_ent;
	int dst_ent;

	int decalnum;
	decalpolys_t *decal;

	struct particle_s *link;

	void (*think)(struct particle_s *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time);
	qboolean thinknext;
} cparticle_t;

#define PARTICLE_GRAVITY		40
#define BLASTER_PARTICLE_COLOR	0xe0
#define INSTANT_PARTICLE	-10000.0
#define MIN_RAIL_LENGTH		1024
#define DEFAULT_RAIL_LENGTH	2048
#define DEFAULT_RAIL_SPACE	1
#define MIN_DECAL_LIFE 5

#pragma endregion

void CL_ClearTEnts();

int CL_ParseEntityBits(uint *bits);
void CL_ParseDelta(entity_state_t *from, entity_state_t *to, const int number, const uint bits);
void CL_ParseFrame();

void CL_ParseTEnt();
void CL_ParseConfigString();
void CL_PlayBackgroundTrack(); // Knightmare added
void CL_ParseMuzzleFlash();
void CL_ParseMuzzleFlash2();

void CL_SetLightstyle(const int i);

void CL_RunDLights();
void CL_RunLightStyles();

void CL_AddEntities();
void CL_AddDLights();
void CL_AddTEnts();
void CL_AddLightStyles();

void CL_PrepRefresh();
void CL_RegisterSounds();

void CL_Quit_f();

#pragma region ======================= Imported renderer functions

// Called when the renderer is loaded
qboolean R_Init(char *reason);

// Called to clear rendering state (error recovery, etc.)
void R_ClearState(void);

// Called before the renderer is unloaded
void R_Shutdown(void);

// All data that will be used in a level should be registered before rendering any frames to prevent disk hits,
// but they can still be registered at a later time if necessary.

// EndRegistration will free any remaining data that wasn't registered.
// Any model_s or skin_s pointers from before the BeginRegistration are no longer valid after EndRegistration.

// Skins and images need to be differentiated, because skins are flood filled to eliminate mip map edge errors, and pics have
// an implicit "pics/" prepended to the name (a pic name that starts with a slash will not use the "pics/" prefix or the ".pcx" postfix).

void R_BeginRegistration(char *map);
struct model_s *R_RegisterModel(char *name);
struct image_s *R_RegisterSkin(char *name);
struct image_s *R_DrawFindPic(char *name);

void R_FreePic(char *name); // Knightmare added
void R_SetSky(char *name, float rotate, vec3_t axis);
void R_EndRegistration(void);
int R_GetRegistartionSequence(struct model_s *model); //mxd

void R_RenderFrame(refdef_t *fd);

void R_SetParticlePicture(int num, char *name); // Knightmare added

void R_DrawGetPicSize(int *w, int *h, char *name);	// will return 0 0 if not found
void R_DrawPic(int x, int y, char *name);
void R_DrawStretchPic(int x, int y, int w, int h, char *name, float alpha); // added alpha for Psychospaz's transparent console
void R_DrawScaledPic(int x, int y, float scale, float alpha, char *name);
void R_DrawChar(float x, float y, int c, float scale, int red, int green, int blue, int alpha, qboolean italic, qboolean last); // added char scaling from Quake2Max
void R_DrawFill(int x, int y, int w, int h, int red, int green, int blue, int alpha);
void R_DrawCameraEffect(void);

void R_GrabScreen(void); // Screenshots for savegames
void R_ScaledScreenshot(char *filename); // Screenshots for savegames

int R_MarkFragments(const vec3_t origin, const vec3_t axis[3], float radius, int maxPoints, vec3_t *points, int maxFragments, markFragment_t *fragments);

void R_SetFogVars(qboolean enable, int model, int density, int start, int end, int red, int green, int blue);

// Draw images for cinematic rendering.
void R_DrawStretchRaw(int x, int y, int w, int h, const byte *raw, int width, int height);

// Video mode and refresh state management entry points
void R_BeginFrame(float camera_separation);
void GLimp_EndFrame(void);

#pragma endregion

#pragma region ======================= Client-side logic

//
// cl_main.c
//
void CL_FixUpGender();
void CL_Disconnect();
void CL_Disconnect_f();
void CL_PingServers_f();
void CL_Snd_Restart_f();
void CL_WriteConfig_f();

void CL_WriteDemoMessage();
void CL_Stop_f();
void CL_Record_f();

//
// cl_screen.c
//
typedef struct
{
	float x;
	float y;
	float avg;
} hudscale_t;

hudscale_t hudScale;

float ScaledHud(float param);
float HudScale();
void InitHudScale();
void CL_AddNetgraph();

//
// cl_input.c
//
typedef enum //mxd
{
	KEYSTATE_UNSET = 0,
	KEYSTATE_DOWN = 1,
	KEYSTATE_IMPULSE_DOWN = 2,
	KEYSTATE_IMPULSE_UP = 4,
} kstate_t;

typedef struct
{
	int down[2]; // Key nums holding it down
	uint downtime; // Msec timestamp
	uint msec; // Msec down this frame
	kstate_t state; //mxd. Was int
} kbutton_t;

extern kbutton_t in_mlook;
extern kbutton_t in_klook;
extern kbutton_t in_strafe;
extern kbutton_t in_speed;

void CL_InitInput();
void CL_SendCmd(qboolean async); //mxd. Merged with CL_SendCmd_Async()

#ifdef CLIENT_SPLIT_NETFRAME
void CL_RefreshCmd();
void CL_RefreshMove();
#endif

void CL_ClearState();
void CL_ReadPackets();
void CL_BaseMove(usercmd_t *cmd);
void IN_CenterView();
float CL_KeyState(kbutton_t *key);
char *Key_KeynumToString(int keynum);

//
// cl_parse.c
//
extern char *svc_strings[256];

void CL_ParseServerMessage(void);
void CL_LoadClientinfo(clientinfo_t *ci, char *s);
void SHOWNET(char *s);
void CL_ParseClientinfo(int player);

//
// cl_download.c
//
void CL_RequestNextDownload(void);
qboolean CL_CheckOrDownloadFile(char *filename);
void CL_Download_f(void);
void CL_ParseDownload(void);
void CL_Download_Reset_KBps_counter(void);
void CL_Download_Calculate_KBps(int byteDistance, int totalSize);

//
// cl_view.c
//
extern int gun_frame;
extern struct model_s *gun_model;

char loadingMessages[96];
float loadingPercent;

void V_Init(void);
float CalcFov(float fov_x, float width, float height);
void V_RenderView(float stereo_separation);
void V_AddEntity(entity_t *ent);

// Psychospaz's enhanced particle code
void V_AddParticle(vec3_t org, vec3_t angle, vec3_t color, float alpha, int alpha_src, int alpha_dst, float size, int image, int flags);
void V_AddDecal(vec3_t org, vec3_t angle, vec3_t color, float alpha, int alpha_src, int alpha_dst, float size, int image, int flags, decalpolys_t *decal);

void V_AddLight(vec3_t org, float intensity, float r, float g, float b);
void V_AddLightStyle(int style, float r, float g, float b);

//
// cl_tempent.c
//
typedef struct
{
	struct sfx_s *sfx_ric[3];
	struct sfx_s *sfx_lashit;
	struct sfx_s *sfx_spark[3];
	struct sfx_s *sfx_railg;
	struct sfx_s *sfx_rockexp;
	struct sfx_s *sfx_grenexp;
	struct sfx_s *sfx_watrexp;
	struct sfx_s *sfx_plasexp;
	struct sfx_s *sfx_lightning;
	struct sfx_s *sfx_disrexp;
	struct sfx_s *sfx_shockhit;
	struct sfx_s *sfx_footsteps[4];
	struct sfx_s *sfx_metal_footsteps[4];
	struct sfx_s *sfx_dirt_footsteps[4];
	struct sfx_s *sfx_vent_footsteps[4];
	struct sfx_s *sfx_grate_footsteps[4];
	struct sfx_s *sfx_tile_footsteps[4];
	struct sfx_s *sfx_grass_footsteps[4];
	struct sfx_s *sfx_snow_footsteps[4];
	struct sfx_s *sfx_carpet_footsteps[4];
	struct sfx_s *sfx_force_footsteps[4];
	struct sfx_s *sfx_gravel_footsteps[4];
	struct sfx_s *sfx_ice_footsteps[4];
	struct sfx_s *sfx_sand_footsteps[4];
	struct sfx_s *sfx_wood_footsteps[4];
	struct sfx_s *sfx_slosh[4];
	struct sfx_s *sfx_wade[4];
	struct sfx_s *sfx_mud_wade[2];
	struct sfx_s *sfx_ladder[4];

	struct model_s *mod_explode;
	struct model_s *mod_smoke;
	struct model_s *mod_flash;
	struct model_s *mod_parasite_segment;
	struct model_s *mod_grapple_cable;
	struct model_s *mod_parasite_tip;
	struct model_s *mod_explo;
	struct model_s *mod_bfg_explo;
	struct model_s *mod_powerscreen;
	struct model_s *mod_plasmaexplo;
	struct model_s *mod_lightning;
	struct model_s *mod_heatbeam;
	struct model_s *mod_monster_heatbeam;
	struct model_s *mod_explo_big;
	struct model_s *mod_shocksplash;
} clientMedia_t;

extern clientMedia_t clMedia;

void CL_RegisterTEntSounds();
void CL_RegisterTEntModels();
void CL_SmokeAndFlash(const vec3_t origin);

//
// cl_pred.c
//
void CL_PredictMovement();
void CL_CheckPredictionError();
//Knightmare added
trace_t CL_Trace(const vec3_t start, const vec3_t end, const float size, const int contentmask);
trace_t CL_BrushTrace(const vec3_t start, const vec3_t end, const float size, const int contentmask);
trace_t CL_PMTrace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
trace_t CL_PMSurfaceTrace(const int playernum, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, const int contentmask);

//
// cl_lights.c
//
cdlight_t *CL_AllocDlight(const int key);
void CL_ClearDlights();
void CL_ClearLightStyles();

//
// cl_particle.c
//
extern cparticle_t *active_particles;
extern cparticle_t *free_particles;

int CL_GetRandomBloodParticle();
void CL_ClipDecal(cparticle_t *part, float radius, float orient, vec3_t origin, vec3_t dir);
float CL_NewParticleTime();

/*color = 255, 255, 255
image = particle_generic
blendfunc_src = GL_SRC_ALPHA
blendfunc_dst = GL_ONE
p->alpha = 1
p->size = 1
p->time = cl.time
The rest is 0 (mxd). */
cparticle_t *CL_InitParticle();
cparticle_t *CL_InitParticle2(const int flags);

//TODO: (mxd) get rid of this abomination
/*angle X Y Z
origin X Y Z
velocity X Y Z
acceleration X Y Z
color R G B
color verlocity R G B
alpha, alpha velocity
blendfunc_src, blendfunc_dst
size, size velocity
image
flags
think, thinknext */
cparticle_t *CL_SetupParticle(
			float angle0,		float angle1,		float angle2,
			float org0,			float org1,			float org2,
			float vel0,			float vel1,			float vel2,
			float accel0,		float accel1,		float accel2,
			float color0,		float color1,		float color2,
			float colorvel0,	float colorvel1,	float colorvel2,
			float alpha,		float alphavel,
			int	blendfunc_src,	int blendfunc_dst,
			float size,			float sizevel,
			int	image,
			int flags,
			void (*think)(cparticle_t *p, vec3_t p_org, vec3_t p_angle, float *p_alpha, float *p_size, int *p_image, float *p_time),
			qboolean thinknext);

void CL_AddParticleLight(cparticle_t *p, const float light, const float lightvel, const float lcol0, const float lcol1, const float lcol2);

void CL_CalcPartVelocity(cparticle_t *p, const float scale, const float time, vec3_t velocity);
void CL_ParticleBounceThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time);
void CL_ParticleRotateThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time);
void CL_DecalAlphaThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, int *image, float *time);
void CL_AddParticles();
void CL_ClearEffects();
qboolean CL_UnclipDecals(); //mxd. void -> qboolean
void CL_ReclipDecals();

//
// cl_effects.c
//
void CL_BigTeleportParticles(const vec3_t org);
void CL_RocketTrail(const vec3_t start, const vec3_t end, centity_t *old);
void CL_DiminishingTrail(const vec3_t start, const vec3_t end, centity_t *old, const int flags);
void CL_FlyEffect(centity_t *ent, vec3_t origin);
void CL_BfgParticles(const entity_t *ent);
void CL_EntityEvent(entity_state_t *ent);
void CL_TrapParticles(entity_t *ent); // RAFAEL
void CL_BlasterTrail(const vec3_t start, const vec3_t end, const int red, const int green, const int blue, const int reddelta, const int greendelta, const int bluedelta);
void CL_HyperBlasterEffect(const vec3_t start, const vec3_t end, const vec3_t angle, const int red, const int green, const int blue, const int reddelta, const int greendelta, const int bluedelta, const float len, const float size);
void CL_BlasterTracer(const vec3_t origin, const vec3_t angle, const int red, const int green, const int blue, const float len, const float size);
void CL_BlasterParticles(const vec3_t org, const vec3_t dir, const int count, const float size, const int red, const int green, const int blue, const int reddelta, const int greendelta, const int bluedelta);

void CL_QuadTrail(const vec3_t start, const vec3_t end);
void CL_RailTrail(const vec3_t start, const vec3_t end, const qboolean isred);
void CL_BubbleTrail(const vec3_t start, const vec3_t end);
void CL_FlagTrail(const vec3_t start, const vec3_t end, const qboolean isred, const qboolean isgreen);
void CL_IonripperTrail(const vec3_t start, const vec3_t end); // RAFAEL

void CL_TeleportParticles(const vec3_t org);
void CL_ItemRespawnParticles(const vec3_t org);
void CL_ExplosionParticles(const vec3_t org);

// PGM
void CL_DebugTrail(const vec3_t start, const vec3_t end);
void CL_Flashlight(const int ent, const vec3_t pos);
void CL_ForceWall(const vec3_t start, const vec3_t end, const int color);
void CL_BubbleTrail2(const vec3_t start, const vec3_t end, const int dist);
void CL_HeatbeamParticles(const vec3_t start, const vec3_t forward);
void CL_ParticleSteamEffect(const vec3_t org, const vec3_t dir, const int red, const int green, const int blue, const int reddelta, const int greendelta, const int bluedelta, const int count, const int magnitude);

void CL_TrackerTrail(const vec3_t start, const vec3_t end);
void CL_Tracker_Explode(const vec3_t origin);
void CL_TagTrail(const vec3_t start, const vec3_t end, const int color8);
void CL_ColorFlash(const vec3_t pos, const int ent, const int intensity, const float r, const float g, const float b);
void CL_Tracker_Shell(const vec3_t origin);
void CL_MonsterPlasma_Shell(const vec3_t origin);
void CL_ColorExplosionParticles(const vec3_t org, const int color8, const int run);
void CL_ParticleSmokeEffect(const vec3_t org, const vec3_t dir, const float size);
void CL_ClassicParticleSmokeEffect(const vec3_t org, const vec3_t dir, const int color, const int count, const int magnitude); //mxd
void CL_Widowbeamout(const cl_sustain_t *self);
void CL_Nukeblast(const cl_sustain_t *self);
void CL_WidowSplash(const vec3_t org);

//
// cl_utils.c
//
int color8red(int color8);
int color8green(int color8);
int color8blue(int color8);
void color8_to_vec3(int color8, vec3_t v); //mxd
void vectoangles(vec3_t vec, vec3_t angles);

#ifdef LOC_SUPPORT	// Xile/NiceAss LOC
//
// cl_loc.c
//
void CL_LoadLoc();
void CL_LocPlace();
void CL_AddViewLocs();
void CL_LocDelete();
void CL_LocAdd(char *name);
void CL_LocWrite();
void CL_LocHelp_f();

#endif // LOC_SUPPORT

//
// menus
//
void UI_Init(void);
void UI_Keydown(int key);
void UI_Draw(void);
void UI_ForceMenuOff(void);
void UI_AddToServerList(netadr_t adr, char *info);
void M_Menu_Main_f(void);

//
// cl_inv.c
//
void CL_ParseInventory();
void CL_DrawInventory();

#pragma endregion