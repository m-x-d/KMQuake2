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
// cl_view.c -- player rendering positioning

#include "client.h"

// Development tools for weapons
int gun_frame;
struct model_s *gun_model;

// Added for Psychospaz's chasecam
vec3_t clientorigin; // Lerped org of client for server->client side effects

cvar_t *cl_testparticles;
cvar_t *cl_testentities;
cvar_t *cl_testlights;
cvar_t *cl_testblend;

cvar_t *cl_stats;

cvar_t *info_hand;

static int r_numdlights;
static dlight_t r_dlights[MAX_DLIGHTS];

static int r_numentities;
static entity_t r_entities[MAX_ENTITIES];

static int r_numparticles;
static particle_t r_particles[MAX_PARTICLES];

static int r_numdecalfrags;
static particle_t r_decalfrags[MAX_DECAL_FRAGS];

static lightstyle_t r_lightstyles[MAX_LIGHTSTYLES];

int num_cl_weaponmodels;
char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];

static void V_ClearScene(void)
{
	r_numdlights = 0;
	r_numentities = 0;
	r_numparticles = 0;
	r_numdecalfrags = 0;
}

#pragma region ======================= TPS camera -psychospaz

static float viewermodelalpha;

void ClipCam(vec3_t start, vec3_t end, vec3_t newpos)
{
	const trace_t tr = CL_Trace(start, end, 5, MASK_SOLID);
	for (int i = 0; i < 3; i++)
		newpos[i] = tr.endpos[i];
}

static void AddViewerEntAlpha(entity_t *ent)
{
	if (viewermodelalpha == 1 || !cg_thirdperson_alpha->value)
		return;

	ent->alpha *= viewermodelalpha;
	if (ent->alpha < 1.0f)
		ent->flags |= RF_TRANSLUCENT;
}

void CalcViewerCamTrans(float distance)
{
	const float alphacalc = max(1, cg_thirdperson_dist->value); // No div by 0
	viewermodelalpha = min(1, distance / alphacalc);
}

#pragma endregion

#pragma region ======================= Add entity / particle / decal / light / spotlight / lightstyle

void V_AddEntity(entity_t *ent)
{
	// Knightmare- added Psychospaz's chasecam
	if (ent->flags & RF_VIEWERMODEL) // Here is our client
	{
		// What was i thinking before!?
		for (int i = 0; i < 3; i++)
			clientorigin[i] = ent->oldorigin[i] = ent->origin[i] = cl.predicted_origin[i];

		if (info_hand->value == 1) // Lefthanded
			ent->flags |= RF_MIRRORMODEL;

		if (cg_thirdperson->value && !(cl.attractloop && !(cl.cinematictime > 0 && cls.realtime - cl.cinematictime > 1000)))
		{
			AddViewerEntAlpha(ent);
			ent->flags &= ~RF_VIEWERMODEL;
			ent->renderfx |= RF2_CAMERAMODEL;
		}
	}

	// end Knightmare
	if (r_numentities < MAX_ENTITIES)
		r_entities[r_numentities++] = *ent;
}

//Knightmare- Psychospaz's enhanced particle code
void V_AddParticle(vec3_t org, vec3_t angle, vec3_t color, float alpha, int alpha_src, int alpha_dst, float size, int image, int flags)
{
	if (r_numparticles >= MAX_PARTICLES)
		return;

	particle_t *p = &r_particles[r_numparticles++];

	VectorCopy(org, p->origin); //mxd
	VectorCopy(angle, p->angle); //mxd
	p->red = color[0];
	p->green = color[1];
	p->blue = color[2];
	p->alpha = alpha;
	p->image = image;
	p->flags = flags;
	p->size  = size;
	p->decal = NULL;
	p->blendfunc_src = alpha_src;
	p->blendfunc_dst = alpha_dst;
}
//end Knightmare

void V_AddDecal(vec3_t org, vec3_t angle, vec3_t color, float alpha, int alpha_src, int alpha_dst, float size, int image, int flags, decalpolys_t *decal)
{
	if (r_numdecalfrags >= MAX_DECAL_FRAGS)
		return;

	particle_t *d = &r_decalfrags[r_numdecalfrags++];

	VectorCopy(org, d->origin); //mxd
	VectorCopy(angle, d->angle); //mxd
	d->red = color[0];
	d->green = color[1];
	d->blue = color[2];
	d->alpha = alpha;
	d->image = image;
	d->flags = flags;
	d->size  = size;
	d->decal = decal;

	d->blendfunc_src = alpha_src;
	d->blendfunc_dst = alpha_dst;
}

void V_AddLight(vec3_t org, float intensity, float r, float g, float b)
{
	if (r_numdlights >= MAX_DLIGHTS)
		return;

	dlight_t *dl = &r_dlights[r_numdlights++];
	VectorCopy(org, dl->origin);
	dl->intensity = intensity;
	VectorSet(dl->color, r, g, b);
	VectorCopy(vec3_origin, dl->direction);
	dl->spotlight = false;
}

void V_AddSpotLight(vec3_t org, vec3_t direction, float intensity, float r, float g, float b)
{
	if (r_numdlights >= MAX_DLIGHTS)
		return;

	dlight_t *dl = &r_dlights[r_numdlights++];
	VectorCopy(org, dl->origin);
	VectorCopy(direction, dl->direction);
	VectorSet(dl->color, r, g, b);
	dl->intensity = intensity;
	dl->spotlight = true;
}

void V_AddLightStyle(int style, float r, float g, float b)
{
	if (style < 0 || style > MAX_LIGHTSTYLES)
		Com_Error(ERR_DROP, "Bad light style %i", style);

	lightstyle_t *ls = &r_lightstyles[style];

	ls->white = r + g + b;
	VectorSet(ls->rgb, r, g, b);
}

#pragma endregion

#pragma region ======================= Test particles / entities / lights

// If cl_testparticles is set, create 4096 particles in the view
static void V_TestParticles()
{
	r_numparticles = 4096;
	for (int i = 0; i < r_numparticles; i++)
	{
		const float d = i * 0.25f;
		const float r = 4 * ((i & 7) - 3.5f);
		const float u = 4 * (((i >> 3) & 7) - 3.5f);
		particle_t *p = &r_particles[i];

		for (int j = 0; j < 3; j++)
			p->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j] * d + cl.v_right[j] * r + cl.v_up[j] * u;

		p->color = 8;
		p->alpha = cl_testparticles->value;
	}
}

// If cl_testentities is set, create 32 player models
static void V_TestEntities()
{
	r_numentities = 32;
	memset(r_entities, 0, sizeof(r_entities));

	for (int i = 0; i < r_numentities; i++)
	{
		entity_t *ent = &r_entities[i];

		const float r = 64 * ((i % 4) - 1.5f);
		const float f = 64 * (i / 4.0f) + 128;

		for (int j = 0; j < 3; j++)
			ent->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j] * f + cl.v_right[j] * r;

		ent->model = cl.baseclientinfo.model;
		ent->skin = cl.baseclientinfo.skin;
	}
}

// If cl_testlights is set, create 32 lights models
static void V_TestLights()
{
	r_numdlights = 32;
	memset(r_dlights, 0, sizeof(r_dlights));

	for (int i = 0; i < r_numdlights; i++)
	{
		dlight_t *dl = &r_dlights[i];

		const float r = 64 * ((i % 4) - 1.5f);
		const float f = 64 * (i / 4.0f) + 128;

		for (int j = 0; j < 3; j++)
			dl->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j] * f + cl.v_right[j] * r;
		
		dl->color[0] =  ((i % 6) + 1) & 1;
		dl->color[1] = (((i % 6) + 1) & 2) >> 1;
		dl->color[2] = (((i % 6) + 1) & 4) >> 2;
		dl->intensity = 200;
		dl->spotlight = false;
	}
}

#pragma endregion

#pragma region ======================= CL_PrepRefresh - map loading

extern int scr_draw_loading; //mxd

// Called before entering a new level, or after changing dlls
void CL_PrepRefresh()
{
	char mapname[64];
	char pname[MAX_QPATH];

	if (!cl.configstrings[CS_MODELS + 1][0])
		return; // No map loaded

	// Use new loading plaque?
	if (!cls.disable_screen || !scr_draw_loading)
		SCR_BeginLoadingPlaque(NULL);

	// Knightmare- for Psychospaz's map loading screen
	Com_sprintf(loadingMessages, sizeof(loadingMessages), S_COLOR_ALT"Loading %s", cl.configstrings[CS_MODELS + 1]);
	loadingPercent = 0.0f;
	// end Knightmare

	// Let the render dll load the map
	Q_strncpyz(mapname, cl.configstrings[CS_MODELS + 1] + 5, sizeof(mapname)); // Skip "maps/"
	mapname[strlen(mapname) - 4] = 0; // Cut off ".bsp"

	// Register models, pics, and skins
	Com_Printf("Map: %s\r", mapname);
	SCR_UpdateScreen();
	R_BeginRegistration(mapname);
	Com_Printf("                                     \r");

	// Knightmare- for Psychospaz's map loading screen
	Com_sprintf(loadingMessages, sizeof(loadingMessages), S_COLOR_ALT"Loading models...");
	loadingPercent += 20.0f;
	// end Knightmare

	// Precache status bar pics
	Com_Printf("pics\r");
	SCR_UpdateScreen();
	SCR_TouchPics();
	Com_Printf("                                     \r");

	CL_RegisterTEntModels();

	num_cl_weaponmodels = 1;
	Q_strncpyz(cl_weaponmodels[0], "weapon.md2", sizeof(cl_weaponmodels[0]));

	// Knightmare- for Psychospaz's map loading screen
	int max = 0;
	for (int i = 1; i < MAX_MODELS && cl.configstrings[CS_MODELS + i][0]; i++)
		max++;

	for (int i = 1; i < MAX_MODELS && cl.configstrings[CS_MODELS + i][0]; i++)
	{
		Q_strncpyz(pname, cl.configstrings[CS_MODELS + i], sizeof(pname));
		pname[37] = 0; // Never go beyond one line
		if (pname[0] != '*')
		{
			Com_Printf("%s\r", pname); 

			// Knightmare- for Psychospaz's map loading screen. Only make max of 40 chars long
			if (i > 1)
				Com_sprintf(loadingMessages, sizeof(loadingMessages), S_COLOR_ALT"Loading %s", (strlen(pname) > 40 ? &pname[strlen(pname) - 40] : pname));
		}

		SCR_UpdateScreen();
		IN_Update(); // Pump message loop
		if (pname[0] == '#')
		{
			// Special player weapon model
			if (num_cl_weaponmodels < MAX_CLIENTWEAPONMODELS)
			{
				strncpy(cl_weaponmodels[num_cl_weaponmodels], cl.configstrings[CS_MODELS + i] + 1, sizeof(cl_weaponmodels[num_cl_weaponmodels]) - 1);
				num_cl_weaponmodels++;
			}
		}
		else
		{
			cl.model_draw[i] = R_RegisterModel(cl.configstrings[CS_MODELS + i]);
			if (pname[0] == '*')
				cl.model_clip[i] = CM_InlineModel(cl.configstrings[CS_MODELS + i]);
			else
				cl.model_clip[i] = NULL;
		}

		if (pname[0] != '*')
			Com_Printf("                                     \r");

		// Knightmare- for Psychospaz's map loading screen
		loadingPercent += 40.0f / (float)max;
	}

	// Knightmare- for Psychospaz's map loading screen
	Com_sprintf(loadingMessages, sizeof(loadingMessages), S_COLOR_ALT"Loading pics...");
	Com_Printf("images\r");
	SCR_UpdateScreen();

	// Knightmare- BIG UGLY HACK for connected to server using old protocol
	// Changed configstrings require different parsing
	const int maximages = (LegacyProtocol() ? OLD_MAX_IMAGES : MAX_IMAGES); //mxd
	const int csimages = (LegacyProtocol() ? OLD_CS_IMAGES : CS_IMAGES); //mxd

	// Knightmare- for Psychospaz's map loading screen
	max = 0;
	for (int i = 1; i < maximages && cl.configstrings[csimages + i][0]; i++)
		max++;

	for (int i = 1; i < maximages && cl.configstrings[csimages + i][0]; i++)
	{
		cl.image_precache[i] = R_DrawFindPic(cl.configstrings[csimages + i]);
		IN_Update(); // Pump message loop
		
		// Knightmare- for Psychospaz's map loading screen
		loadingPercent += 20.0f / (float)max;
	}

	// Knightmare- for Psychospaz's map loading screen
	Com_sprintf(loadingMessages, sizeof(loadingMessages), S_COLOR_ALT"Loading players...");
	Com_Printf("                                     \r");

	// Knightmare- for Psychospaz's map loading screen
	const int playerskinsoffset = (LegacyProtocol() ? OLD_CS_PLAYERSKINS : CS_PLAYERSKINS); //mxd
	max = 0;
	for (int i = 1; i < MAX_CLIENTS; i++)
		if (cl.configstrings[playerskinsoffset + i][0]) //mxd
			max++;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		// Knightmare- BIG UGLY HACK for old connected to server using old protocol
		// Changed configstrings require different parsing
		if (!cl.configstrings[playerskinsoffset + i][0]) //mxd
			continue;
		
		Com_Printf("client %i\r", i);
		SCR_UpdateScreen();
		IN_Update(); // Pump message loop
		CL_ParseClientinfo(i);
		Com_Printf("                                     \r");

		// Knightmare- for Psychospaz's map loading screen
		loadingPercent += 20.0f / (float)max;
	}

	// Knightmare- for Psychospaz's map loading screen
	Com_sprintf(loadingMessages, sizeof(loadingMessages), S_COLOR_ALT"Loading players... done");
	//hack hack hack - psychospaz
	loadingPercent = 100.0f;

	// Knightmare - Vics fix to get rid of male/grunt flicker. Knightmare- make this single-player only
	if (!cl.configstrings[CS_MAXCLIENTS][0] || !strcmp(cl.configstrings[CS_MAXCLIENTS], "1"))
		CL_LoadClientinfo(&cl.baseclientinfo, va("unnamed\\%s", info_skin->string));
	else
		CL_LoadClientinfo(&cl.baseclientinfo, "unnamed\\male/grunt");

	// Knightmare- refresh the player model/skin info
	userinfo_modified = true;

	// Set sky textures and speed
	Com_Printf("sky\r"); 
	SCR_UpdateScreen();

	vec3_t axis;
	const float rotate = atof(cl.configstrings[CS_SKYROTATE]);
	sscanf(cl.configstrings[CS_SKYAXIS], "%f %f %f", &axis[0], &axis[1], &axis[2]);
	R_SetSky(cl.configstrings[CS_SKY], rotate, axis);
	Com_Printf("                                     \r");

	// The renderer can now free unneeded stuff
	R_EndRegistration();

#ifdef LOC_SUPPORT // Xile/NiceAss LOC
	CL_LoadLoc();
#endif	// LOC_SUPPORT 

	// Clear any lines of console text
	Con_ClearNotify();

	SCR_UpdateScreen();
	cl.refresh_prepped = true;
	cl.force_refdef = true;	// Make sure we have a valid refdef

	// Start the cd track
	CL_PlayBackgroundTrack();

	// Knightmare- for Psychospaz's map loading screen
	loadingMessages[0] = 0;
	// Knightmare- close loading screen as soon as done
	cls.disable_screen = false;

	// Knightmare- don't start map with game paused
	if (cls.key_dest != key_menu)
		Cvar_Set("paused", "0");
}

#pragma endregion

#pragma region ======================= Gun frame debugging functions

static void V_Gun_Next_f(void)
{
	gun_frame++;
	Com_Printf("frame %i\n", gun_frame);
}

static void V_Gun_Prev_f(void)
{
	gun_frame--;
	gun_frame = max(0, gun_frame);
	Com_Printf("frame %i\n", gun_frame);
}

static void V_Gun_Model_f(void)
{
	char name[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		gun_model = NULL;
		return;
	}

	Com_sprintf(name, sizeof(name), "models/%s/tris.md2", Cmd_Argv(1));
	gun_model = R_RegisterModel(name);
}

#pragma endregion

#pragma region ======================= View rendering

float CalcFov(float fov_x, float width, float height)
{
	if (fov_x < 1 || fov_x > 179)
		Com_Error(ERR_DROP, "Bad fov: %f", fov_x);

	const float x = width / tan(fov_x / 360 * M_PI);
	float a = atanf(height / x);
	a *= 360 / M_PI;

	return a;
}

static int entitycmpfnc(const entity_t *a, const entity_t *b)
{
	// All other models are sorted by model then skin
	if (a->model == b->model)
		return (int)a->skin - (int)b->skin;

	return (int)a->model - (int)b->model;
}

void V_RenderView(float stereo_separation)
{
	if (cls.state != ca_active)
		return;

	if (!cl.refresh_prepped)
		return; // Still loading

	if (cl_timedemo->value)
	{
		if (!cl.timedemo_start)
			cl.timedemo_start = Sys_Milliseconds();

		cl.timedemo_frames++;
	}

	// An invalid frame will just use the exact previous refdef.
	// We can't use the old frame if the video mode has changed, though...
	if (cl.frame.valid && (cl.force_refdef || !cl_paused->value))
	{
		cl.force_refdef = false;

		V_ClearScene();

		// Build a refresh entity list and calc cl.sim*
		// This also calls CL_CalcViewValues which loads v_forward, etc.
		CL_AddEntities();

		if (cl_testparticles->integer)
			V_TestParticles();

		if (cl_testentities->integer)
			V_TestEntities();

		if (cl_testlights->integer)
			V_TestLights();

		if (cl_testblend->integer)
		{
			cl.refdef.blend[0] = 1.0f;
			cl.refdef.blend[1] = 0.5f;
			cl.refdef.blend[2] = 0.25f;
			cl.refdef.blend[3] = 0.5f;
		}

		// Offset vieworg appropriately if we're doing stereo separation
		if (stereo_separation != 0)
		{
			vec3_t tmp;
			VectorScale(cl.v_right, stereo_separation, tmp);
			VectorAdd(cl.refdef.vieworg, tmp, cl.refdef.vieworg);
		}

		// Never let it sit exactly on a node line, because a water plane can
		// dissapear when viewed with the eye exactly on it.
		// The server protocol only specifies to 1/8 pixel, so add 1/16 in each axis
		for(int c = 0; c < 3; c++)
			cl.refdef.vieworg[c] += 1.0f / 16;

		cl.refdef.x = 0;
		cl.refdef.y = 0;
		cl.refdef.width = viddef.width;
		cl.refdef.height = viddef.height;

		// Adjust fov for wide aspect ratio
		if (cl_widescreen_fov->value)
		{
			const float aspectRatio = (float)cl.refdef.width / (float)cl.refdef.height;
			
			// Changed to improved algorithm by Dopefish
			if (aspectRatio > STANDARD_ASPECT_RATIO)
				cl.refdef.fov_x = RAD2DEG(2 * atanf((aspectRatio / STANDARD_ASPECT_RATIO) * tanf(DEG2RAD(cl.refdef.fov_x) * 0.5f)));

			cl.refdef.fov_x = min(cl.refdef.fov_x, 160);
		}

		cl.refdef.fov_y = CalcFov(cl.refdef.fov_x, cl.refdef.width, cl.refdef.height);
		cl.refdef.time = cl.time * 0.001f;

		// Barnes- Warp if underwater ala q3a :-)
		if (cl.refdef.rdflags & RDF_UNDERWATER)
		{
			const float f = sinf(cl.time * 0.001f * 0.4f * (M_PI * 2.7f));
			cl.refdef.fov_x += f * (cl.refdef.fov_x / 90.0f); // Knightmare- scale to fov
			cl.refdef.fov_y -= f * (cl.refdef.fov_y / 90.0f); // Knightmare- scale to fov
		}

		cl.refdef.areabits = cl.frame.areabits;

		if (!cl_add_entities->integer)
			r_numentities = 0;

		if (!cl_add_particles->integer)
			r_numparticles = 0;

		if (!cl_add_lights->integer)
			r_numdlights = 0;

		if (!cl_add_blend->integer)
			VectorClear(cl.refdef.blend);

		cl.refdef.num_entities = r_numentities;
		cl.refdef.entities = r_entities;
		cl.refdef.num_particles = r_numparticles;
		cl.refdef.particles = r_particles;

		cl.refdef.num_decals = r_numdecalfrags;
		cl.refdef.decals = r_decalfrags;

		cl.refdef.num_dlights = r_numdlights;
		cl.refdef.dlights = r_dlights;
		cl.refdef.lightstyles = r_lightstyles;

		cl.refdef.rdflags = cl.frame.playerstate.rdflags;
		qsort(cl.refdef.entities, cl.refdef.num_entities, sizeof(cl.refdef.entities[0]), (int (*)(const void *, const void *))entitycmpfnc);
	}

	R_RenderFrame(&cl.refdef);

	if (cl_stats->integer)
		Com_Printf("ent:%i  lt:%i  part:%i\n", r_numentities, r_numdlights, r_numparticles);

	if (log_stats->integer && log_stats_file != 0)
		fprintf(log_stats_file, "%i,%i,%i,", r_numentities, r_numdlights, r_numparticles);
}

#pragma endregion

#pragma region ======================= Console commands

static void V_Viewpos_f(void)
{
	Com_Printf("(%i %i %i) : %i\n",
		(int)cl.refdef.vieworg[0],
		(int)cl.refdef.vieworg[1],
		(int)cl.refdef.vieworg[2],
		(int)cl.refdef.viewangles[YAW]);
}

// Knightmare- diagnostic commands from Lazarus
static void V_Texture_f(void)
{
	//mxd. Flag names
	static int flags[] =
	{
		SURF_LIGHT, SURF_SLICK, SURF_SKY, SURF_WARP, SURF_TRANS33, SURF_TRANS66, SURF_FLOWING,
		SURF_METAL, SURF_DIRT, SURF_VENT, SURF_GRATE, SURF_TILE, SURF_GRASS, SURF_SNOW, SURF_FORCE, SURF_GRAVEL, SURF_ICE, SURF_SAND, SURF_WOOD, SURF_STANDARD,
		SURF_NOLIGHTENV, SURF_ALPHATEST
	};

	static char *flagnames[] =
	{
		"LIGHT", "SLICK", "SKY", "WARP", "TRANS33", "TRANS66", "FLOWING",
		"METAL", "DIRT", "VENT", "GRATE", "TILE", "GRASS", "SNOW", "FORCE", "GRAVEL", "ICE", "SAND", "WOOD", "STANDARD",
		"NOLIGHTENV", "ALPHATEST"
	};

	static int flagscount = sizeof(flags) / sizeof(flags[0]);
	
	vec3_t forward, start, end;

	VectorCopy(cl.refdef.vieworg, start);
	AngleVectors(cl.refdef.viewangles, forward, NULL, NULL);
	VectorMA(start, WORLD_SIZE, forward, end); // Was 8192
	const trace_t tr = CL_PMSurfaceTrace(cl.playernum + 1, start, NULL, NULL, end, MASK_SOLID | MASK_WATER); //mxd. Was MASK_ALL

	Con_ClearNotify(); //mxd

	if (!tr.ent)
	{
		Com_Printf("Nothing hit...\n");
	}
	else if (!tr.surface || !tr.surface->name[0]) //mxd. We may get a nullsurface here
	{
		Com_Printf("Not a brush!\n");
	}
	else
	{
		Com_Printf("Texture:     %s\n", tr.surface->name);
		Com_Printf("Surf. flags: 0x%08x", tr.surface->flags); // Don't add newline yet

		//mxd. Let's actually print em...
		int numflags = 0;
		if(tr.surface->flags != 0)
		{
			for (int i = 0; i < flagscount; i++)
				if (tr.surface->flags & flags[i])
					numflags++;

			if(numflags > 0)
			{
				char buffer[1024];
				Q_snprintfz(buffer, sizeof(buffer), " [");

				int usedflags = 0;
				for (int i = 0; i < flagscount; i++)
				{
					if (tr.surface->flags & flags[i])
					{
						char *format = (usedflags < numflags - 1 ? "%s, " : "%s");
						Q_strncatz(buffer, va(format, flagnames[i]), sizeof(buffer));
						usedflags++;
					}
				}

				Q_strncatz(buffer, "]\n", sizeof(buffer));
				Com_Printf("%s", buffer);
			}
		}

		if(numflags == 0)
			Com_Printf("\n"); // Add newline now

		Com_Printf("Light value: %i\n", tr.surface->value);
	}
}

static void V_Surf_f(void)
{
	vec3_t forward, start, end;

	if (!developer->value) // Only works in developer mode
		return;

	// Disable this in multiplayer
	if (cl.configstrings[CS_MAXCLIENTS][0] && strcmp(cl.configstrings[CS_MAXCLIENTS], "1"))
		return;

	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: surf <flags>: assigns given integer as surface flags\n");
		return;
	}

	const int s = atoi(Cmd_Argv(1));

	VectorCopy(cl.refdef.vieworg, start);
	AngleVectors(cl.refdef.viewangles, forward, NULL, NULL);
	VectorMA(start, WORLD_SIZE, forward, end); // Was 8192
	const trace_t tr = CL_PMSurfaceTrace(cl.playernum + 1, start, NULL, NULL, end, MASK_SOLID | MASK_WATER); //mxd. Was MASK_ALL

	if (!tr.ent)
		Com_Printf("Nothing hit...\n");
	else if (!tr.surface || !tr.surface->name[0]) //mxd. We may get a nullsurface here
		Com_Printf("Not a brush!\n");
	else
		tr.surface->flags = s;
}

#pragma endregion

void V_Init(void)
{
	Cmd_AddCommand("gun_next", V_Gun_Next_f);
	Cmd_AddCommand("gun_prev", V_Gun_Prev_f);
	Cmd_AddCommand("gun_model", V_Gun_Model_f);

	Cmd_AddCommand("viewpos", V_Viewpos_f);

	// Knightmare- diagnostic commands from Lazarus
	Cmd_AddCommand("texture", V_Texture_f);
	Cmd_AddCommand("surf", V_Surf_f);

	info_hand = Cvar_Get("hand", "0", CVAR_ARCHIVE);

	cl_testblend = Cvar_Get("cl_testblend", "0", 0);
	cl_testparticles = Cvar_Get("cl_testparticles", "0", 0);
	cl_testentities = Cvar_Get("cl_testentities", "0", 0);
	cl_testlights = Cvar_Get("cl_testlights", "0", CVAR_CHEAT);

	cl_stats = Cvar_Get("cl_stats", "0", 0);
}