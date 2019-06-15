/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2000-2002 Mr. Hyde and Mad Dog

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

#include "g_local.h"

// Fog is sent to engine like this:
// gi.WriteByte (svc_fog); // svc_fog = 21
// gi.WriteByte (fog_enable); // 1 = on, 0 = off
// gi.WriteByte (fog_model); // 0, 1, or 2
// gi.WriteByte (fog_density); // 1-100
// gi.WriteShort (fog_near); // >0, <fog_far
// gi.WriteShort (fog_far); // >fog_near-64, < 5000
// gi.WriteByte (fog_red); // 0-255
// gi.WriteByte (fog_green); // 0-255
// gi.WriteByte (fog_blue); // 0-255
// gi.unicast (player_ent, true);

#define MAX_FOGS	16 //mxd. Moved from g_local.h

#define GL_LINEAR	0x2601
#define GL_EXP		0x0800
#define GL_EXP2		0x0801

static fog_t gfogs[MAX_FOGS];

static fog_t trig_fade_fog;
static fog_t fade_fog;
static fog_t *pfog;

static qboolean InTriggerFog;

static int GLModels[] = { GL_LINEAR, GL_EXP, GL_EXP2 };
static char* glmodelnames[] = { "Linear", "Exponential", "Exponential squared" }; //mxd

static int last_fog_model;
static int last_fog_density;
static int last_fog_near;
static int last_fog_far;
static int last_fog_color[3];

#define FOG_ON       1
#define FOG_TOGGLE   2
#define FOG_TURNOFF  4
#define FOG_STARTOFF 8

#define COLOR(r,g,b) ((((BYTE)(b)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(r))<<16)))

void fog_fade(edict_t *self);

#pragma region ======================= Helper methods (mxd)

static void SetDefaultDensity(fog_t *fog)
{
	fog->Near = 4999.0f;
	fog->Far = 5000.0f;
	fog->Density = 0.0f;
	fog->Density1 = 0.0f;
	fog->Density2 = 0.0f;
}

static void CopyDensity(fog_t *fog, const edict_t *from)
{
	fog->Near = from->fog_near;
	fog->Far = from->fog_far;
	fog->Density = from->fog_density;
	fog->Density1 = from->fog_density;
	fog->Density2 = from->density;
}

static void ApplyFade(fog_t *fog, edict_t *self)
{
	const int index = self->fog_index - 1;
	const float frames = self->goal_frame - level.framenum + 1;

	if (fog->Model == 0)
	{
		fog->Near += (gfogs[index].Near - fog->Near) / frames;
		fog->Far += (gfogs[index].Far - fog->Far) / frames;
	}
	else
	{
		fog->Density += (gfogs[index].Density - fog->Density) / frames;
		fog->Density1 += (gfogs[index].Density1 - fog->Density1) / frames;
		fog->Density2 += (gfogs[index].Density2 - fog->Density2) / frames;
	}

	fog->Color[0] += (gfogs[index].Color[0] - fog->Color[0]) / frames;
	fog->Color[1] += (gfogs[index].Color[1] - fog->Color[1]) / frames;
	fog->Color[2] += (gfogs[index].Color[2] - fog->Color[2]) / frames;

	fog->GL_Model = GLModels[fog->Model];
}

#pragma endregion

// This routine is ONLY called for console fog commands
void Fog_ConsoleFog(void)
{
	if (deathmatch->integer || coop->integer || !level.active_fog)
		return;

	memcpy(&level.fog, &gfogs[0], sizeof(fog_t));
	pfog = &level.fog;

	// Force sensible values for linear fog
	if (pfog->Model == 0 && pfog->Near == 0.0f && pfog->Far == 0.0f)
	{
		pfog->Near = 64.0f;
		pfog->Far = 1024.0f;
	}
}

qboolean Fog_ProcessCommand()
{
	fog_t* fog = &gfogs[0];
	char* cmd = gi.argv(0);

	char *parm;
	if (gi.argc() < 2)
		parm = NULL;
	else
		parm = gi.argv(1);

	if (Q_stricmp(cmd, "fog_help") == 0)
	{
		gi.dprintf("Fog parameters for console only.\n"
				   "Use fog_active to see parameters of currently active fog.\n"
				   "Use fog_list to see parameters of all fogs.\n"
				   "Use fog [1/0] to turn fog on/off (currently %s).\n\n", (level.active_fog > 0 ? "on" : "off"));

		gi.dprintf("fog_red   = red color in 0 - 1 range.\n"
				   "fog_green = green color in 0 - 1 range.\n"
				   "fog_blue  = blue color in 0 - 1 range.\n"
				   "fog_model = 0 (linear), 1 (exponential), 2 (exponential squared).\n\n"
				   "Linear parameters:\n"
				   "fog_near = fog start distance (>0 and < Fog_Far).\n"
				   "fog_far  = distance where objects are completely fogged (<5000 and > Fog_Near).\n\n"
				   "Exponential parameters:\n"
				   "fog_density = best results with values < 100.\n\n"
				   "Command without a value will show current setting\n");
	}
	else if (Q_stricmp(cmd, "fog_active") == 0)
	{
		if (level.active_fog)
		{
			gi.dprintf("Active fog:\n"
				"  Color: %g, %g, %g\n"
				"  Model: %s\n",
				level.fog.Color[0], level.fog.Color[1], level.fog.Color[2],
				glmodelnames[level.fog.Model]);

			if (level.fog.Model)
			{
				gi.dprintf("Density: %g\n", level.fog.Density);
			}
			else
			{
				gi.dprintf("   Near: %g\n", level.fog.Near);
				gi.dprintf("    Far: %g\n", level.fog.Far);
			}
		}
		else
		{
			gi.dprintf("No fogs currently active\n");
		}
	}
	/*else if (Q_stricmp(cmd, "fog_stuff") == 0)
	{
		gi.dprintf("active_fog: %d, last_active_fog: %d\n", level.active_fog, level.last_active_fog);
	}*/
	else if (Q_stricmp(cmd, "fog") == 0)
	{
		if (parm)
		{
			const int on = atoi(parm);
			level.active_fog = (on ? 1 : 0);
			level.active_target_fog = level.active_fog;
			Fog_ConsoleFog();
		}

		gi.dprintf("Fog is %s.\n", (level.active_fog ? "on" : "off"));
	}
	else if (Q_stricmp(cmd, "fog_red") == 0)
	{
		if (!parm)
		{
			gi.dprintf("%s: %g.\n", cmd, fog->Color[0]);
		}
		else
		{
			level.active_fog = 1;
			level.active_target_fog = level.active_fog;
			fog->Color[0] = clamp((float)atof(parm), 0.0f, 1.0f);
			Fog_ConsoleFog();
		}
	}
	else if (Q_stricmp(cmd, "fog_green") == 0)
	{
		if (!parm)
		{
			gi.dprintf("%s: %g.\n", cmd, fog->Color[1]);
		}
		else
		{
			level.active_fog = 1;
			level.active_target_fog = level.active_fog;
			fog->Color[1] = clamp((float)atof(parm), 0.0f, 1.0f);
			Fog_ConsoleFog();
		}
	}
	else if (Q_stricmp(cmd, "fog_blue") == 0)
	{
		if (!parm)
		{
			gi.dprintf("%s: %g.\n", cmd, fog->Color[2]);
		}
		else
		{
			level.active_fog = 1;
			level.active_target_fog = level.active_fog;
			fog->Color[2] = clamp((float)atof(parm), 0.0f, 1.0f);
			Fog_ConsoleFog();
		}
	}
	else if (Q_stricmp(cmd, "fog_near") == 0)
	{
		if (!parm)
		{
			gi.dprintf("%s: %g.\n", cmd, fog->Near);
		}
		else
		{
			level.active_fog = 1;
			level.active_target_fog = level.active_fog;
			fog->Near = (float)atof(parm);
			Fog_ConsoleFog();
		}
	}
	else if (Q_stricmp(cmd, "fog_far") == 0)
	{
		if (!parm)
		{
			gi.dprintf("%s: %g.\n", cmd, fog->Far);
		}
		else
		{
			level.active_fog = 1;
			level.active_target_fog = level.active_fog;
			fog->Far = (float)atof(parm);
			Fog_ConsoleFog();
		}
	}
	else if (Q_stricmp(cmd, "fog_model") == 0)
	{
		if (!parm)
		{
			gi.dprintf("%s: %d (%s).\n", cmd, fog->Model, glmodelnames[fog->Model]);
		}
		else
		{
			level.active_fog = 1;
			level.active_target_fog = level.active_fog;
			fog->Model = clamp(atoi(parm), 0, (int)(sizeof(GLModels) / sizeof(GLModels[0])));
			fog->GL_Model = min(fog->Model, 2);
			Fog_ConsoleFog();
		}
	}
	else if (Q_stricmp(cmd, "fog_density") == 0)
	{
		if (!parm)
		{
			gi.dprintf("%s: %g.\n", cmd, fog->Density);
		}
		else
		{
			level.active_fog = 1;
			level.active_target_fog = level.active_fog;

			const float density = (float)atof(parm);
			fog->Density = density;
			fog->Density1 = density;
			fog->Density2 = density;

			Fog_ConsoleFog();
		}
	}
	else if (Q_stricmp(cmd, "fog_list") == 0)
	{
		gi.dprintf("level.fogs: %d\n", level.fogs);
		gi.dprintf("level.trigger_fogs: %d\n", level.trigger_fogs);

		for (int i = 0; i < level.fogs; i++)
		{
			fog_t *f = &gfogs[i];
			gi.dprintf("\n"S_COLOR_GREEN"Fog #%d:\n", i + 1);
			gi.dprintf("Trigger: %s\n", (f->Trigger ? "true" : "false"));
			gi.dprintf("Model:   %i (%s)\n", f->Model, glmodelnames[f->Model]);
			gi.dprintf("Near:    %g\n", f->Near);
			gi.dprintf("Far:     %g\n", f->Far);
			gi.dprintf("Density: %g\n", f->Density);
			gi.dprintf("Color:   %g, %g, %g\n", f->Color[0], f->Color[1], f->Color[2]);
			gi.dprintf("Targetname: %s\n", (f->ent ? f->ent->targetname : "[none]"));
		}
	}
	else
	{
		return false;
	}

	return true;
}

void GLFog(void)
{
	edict_t	*player_ent = &g_edicts[1];

	if (!player_ent->client || player_ent->is_bot)
		return;

	int fog_model;
	if (pfog->GL_Model == GL_EXP)
		fog_model = 1;
	else if (pfog->GL_Model == GL_EXP2)
		fog_model = 2;
	else // GL_LINEAR
		fog_model = 0;

	const int fog_density = (int)pfog->Density;
	const int fog_near = (int)pfog->Near;
	const int fog_far = (int)pfog->Far;

	int fog_color[] =
	{
		(int)(pfog->Color[0] * 255),
		(int)(pfog->Color[1] * 255),
		(int)(pfog->Color[2] * 255)
	};

	// Check for change in fog state before updating
	if (fog_model == last_fog_model
		&& fog_density == last_fog_density
		&& fog_near == last_fog_near
		&& fog_far == last_fog_far
		&& fog_color[0] == last_fog_color[0]
		&& fog_color[1] == last_fog_color[1]
		&& fog_color[2] == last_fog_color[2])
	{
		return;
	}

	gi.WriteByte(svc_fog);		// svc_fog = 21
	gi.WriteByte(1);			// enable message
	gi.WriteByte(fog_model);	// model 0, 1, or 2
	gi.WriteByte(fog_density);	// density 1-100
	gi.WriteShort(fog_near);	// near >0, <fog_far
	gi.WriteShort(fog_far);		// far >fog_near-64, < 5000
	gi.WriteByte(fog_color[0]);	// red	0-255
	gi.WriteByte(fog_color[1]);	// green	0-255
	gi.WriteByte(fog_color[2]);	// blue	0-255
	gi.unicast(player_ent, true);

	// Write to last fog state
	last_fog_model = fog_model;
	last_fog_density = fog_density;
	last_fog_near = fog_near;
	last_fog_far = fog_far;
	VectorCopy(fog_color, last_fog_color);
}

void trig_fog_fade(edict_t *self)
{
	if (!InTriggerFog)
	{
		self->nextthink = 0;
		return;
	}

	if (level.framenum <= self->goal_frame)
	{
		ApplyFade(&trig_fade_fog, self); //mxd
		self->nextthink = level.time + FRAMETIME;

		gi.linkentity(self);
	}
}

void init_trigger_fog_delay(edict_t *self)
{
	const int index = self->fog_index - 1;

	// Scan for other trigger_fog's that are currently "thinking", iow
	// the trigger_fog has a delay and is ramping. If found, stop the ramp for those fogs.
	edict_t *e = g_edicts + 1;
	for (int i = 1; i < globals.num_edicts; i++, e++)
	{
		if (!e->inuse || e == self)
			continue;

		if (e->think == trig_fog_fade || e->think == fog_fade)
		{
			e->think = NULL;
			e->nextthink = 0;

			gi.linkentity(e);
		}
	}

	self->spawnflags |= FOG_ON;
	if (!level.active_fog)
	{
		// Fog isn't currently on
		memcpy(&level.fog, &gfogs[index], sizeof(fog_t));
		SetDefaultDensity(&level.fog); //mxd
	}

	VectorCopy(self->fog_color, gfogs[index].Color);
	CopyDensity(&gfogs[index], self); //mxd

	self->goal_frame = level.framenum + self->delay * 10 + 1;
	self->think = trig_fog_fade;
	self->nextthink = level.time + FRAMETIME;

	memcpy(&trig_fade_fog, &level.fog, sizeof(fog_t));
	level.active_fog = self->fog_index;
}

void Fog(edict_t *ent)
{
	if (deathmatch->integer || coop->integer)
		return;

	edict_t	*player = ent;

	if (!player->client || player->is_bot)
		return;

	vec3_t viewpoint;
	VectorCopy(player->s.origin, viewpoint);
	viewpoint[2] += ent->viewheight;

	/*if (Q_stricmp(vid_ref->string, "gl"))
	{
		last_software_frame = level.framenum;
		level.active_fog = 0;

		return;
	}*/

	InTriggerFog = false;

	if (level.trigger_fogs)
	{
		int trigger = 0;

		for (int i = 1; i < level.fogs; i++)
		{
			if (!gfogs[i].Trigger)
				continue;

			if (!gfogs[i].ent->inuse)
				continue;

			if (!(gfogs[i].ent->spawnflags & FOG_ON))
				continue;

			if (viewpoint[0] < gfogs[i].ent->absmin[0] || viewpoint[0] > gfogs[i].ent->absmax[0])
				continue;

			if (viewpoint[1] < gfogs[i].ent->absmin[1] || viewpoint[1] > gfogs[i].ent->absmax[1])
				continue;

			if (viewpoint[2] < gfogs[i].ent->absmin[2] || viewpoint[2] > gfogs[i].ent->absmax[2])
				continue;

			trigger = i;
			break;
		}

		if (trigger)
		{
			InTriggerFog = true;
			edict_t* triggerfog = gfogs[trigger].ent;

			if (level.last_active_fog != trigger + 1)
			{
				if (triggerfog->delay)
					init_trigger_fog_delay(triggerfog);
				else
					memcpy(&level.fog, &gfogs[trigger], sizeof(fog_t));

				level.active_fog = trigger + 1;
			}
			else if (triggerfog->delay)
			{
				memcpy(&level.fog, &trig_fade_fog, sizeof(fog_t));
			}
		}
		else
		{
			InTriggerFog = false;
			level.active_fog = level.active_target_fog;

			// If we are just coming out of a trigger_fog, force level.fog to last active target_fog values
			if (level.active_fog && level.last_active_fog && gfogs[level.last_active_fog - 1].Trigger)
			{
				edict_t	*fogent = gfogs[level.active_fog - 1].ent;
				if (fogent && fogent->think == fog_fade)
					fogent->think(fogent);
				else
					memcpy(&level.fog, &gfogs[level.active_fog - 1], sizeof(fog_t));
			}
		}
	}
	
	if (!level.active_fog)
	{
		if (level.last_active_fog)
			Fog_Off();

		level.last_active_fog = 0;

		return;
	}
	
	pfog = &level.fog;
	if (pfog->Density1 != pfog->Density2 && game.maxclients == 1 && pfog->Model > 0)
	{
		vec3_t vp;
		AngleVectors(player->client->ps.viewangles, vp, NULL, NULL);

		const float dp = DotProduct(pfog->Dir, vp) + 1.0f;
		pfog->Density = ((pfog->Density1 * dp) + (pfog->Density2 * (2.0f - dp))) / 2.0f;
	}

	GLFog();

	level.last_active_fog = level.active_fog;
}

void Fog_Off(void)
{
	if (deathmatch->integer || coop->integer)
		return;

	edict_t	*player_ent = &g_edicts[1];

	if (!player_ent->client || player_ent->is_bot)
		return;

	gi.WriteByte(svc_fog); // svc_fog = 21
	gi.WriteByte(0); // Disable message, remaining paramaters are ignored
	gi.WriteByte(0);
	gi.WriteByte(0);
	gi.WriteShort(0);
	gi.WriteShort(0);
	gi.WriteByte(0);
	gi.WriteByte(0);
	gi.WriteByte(0);
	gi.unicast(player_ent, true);

	// Write to last fog state
	last_fog_model = 0;
	last_fog_density = 0;
	last_fog_near = 0;
	last_fog_far = 0;

	VectorSet(last_fog_color, 255, 255, 255);
}

void Fog_Init(void)
{
	VectorSetAll(gfogs[0].Color, 0.5f); //mxd
	gfogs[0].Model = 1;
	gfogs[0].GL_Model = GLModels[1];
	gfogs[0].Density = 20.0f;
	gfogs[0].Trigger = false;

	// Write to last fog state
	last_fog_model = 0;
	last_fog_density = 0;
	last_fog_near = 0;
	last_fog_far = 0;

	VectorSet(last_fog_color, 255, 255, 255);
}

void fog_fade(edict_t *self)
{
	if (level.framenum <= self->goal_frame)
	{
		ApplyFade(&fade_fog, self); //mxd
		self->nextthink = level.time + FRAMETIME;

		if (!InTriggerFog)
			memcpy(&level.fog, &fade_fog, sizeof(fog_t));

		gi.linkentity(self);
	}
	else if (self->spawnflags & FOG_TURNOFF)
	{
		level.active_fog = 0;
		level.active_target_fog = 0;
	}
}

/*QUAKED target_fog (1 0 0) (-8 -8 -8) (8 8 8) ADDITIVE NEGATIVE
Change the fog effects.

ADDITIVE	: adds the target_fog settings to the current settings
NEGATIVE	: subtracts the target_fog settings from the current settings

fog_color   : The colour of the fog, the colour picker box can (and should IMO) be used.
fog_density	: The density of the fog, dens*10K (exp&exp2) Default=20
fog_model   : Choices are:
   0 :Linear (Default)
   1 :Exponential
   2 :Exponential2
fog_near    : How close the player must get before he sees the fog. Default=64
fog_far     : How far the player can see into the fog. Default=1024
density     : Specifies how player see fog. Direction is in degrees. Default=0
delay       : Ramp time in seconds
count       : Number of times it can be used
*/
void target_fog_use(edict_t *self, edict_t *other, edict_t *activator)
{
	self->count--;
	if (self->count == 0)
	{
		self->think = G_FreeEdict;
		self->nextthink = level.time + self->delay + 1;
	}

	if ((self->spawnflags & FOG_ON) && (self->spawnflags & FOG_TOGGLE))
	{
		self->spawnflags &= ~FOG_ON;
		return;
	}

	self->spawnflags |= FOG_ON;

	const int index = self->fog_index - 1;
	InTriggerFog = false;

	// Scan for other target_fog's that are currently "thinking", iow
	// the target_fog has a delay and is ramping. If found, stop the ramp for those fogs
	edict_t *e = g_edicts + 1;
	for (int i = 1; i < globals.num_edicts; i++, e++)
	{
		if (e->inuse && e->think == fog_fade)
		{
			e->nextthink = 0;
			gi.linkentity(e);
		}
	}

	if (self->spawnflags & FOG_TURNOFF)
	{
		// Fog is "turn off" only
		if (self->delay && level.active_fog)
		{
			/*gfogs[index].Far = 5000.0f;
			gfogs[index].Near = 4999.0f;
			gfogs[index].Density = 0.0f;
			gfogs[index].Density1 = 0.0f;
			gfogs[index].Density2 = 0.0f;*/
			SetDefaultDensity(&gfogs[index]); //mxd

			VectorCopy(level.fog.Color, gfogs[index].Color);

			self->goal_frame = level.framenum + self->delay * 10 + 1;
			self->think = fog_fade;
			self->nextthink = level.time + FRAMETIME;
			level.active_fog = self->fog_index;
			level.active_target_fog = self->fog_index;

			memcpy(&fade_fog, &level.fog, sizeof(fog_t));
		}
		else
		{
			level.active_fog = 0;
			level.active_target_fog = level.active_fog;
		}
	}
	else
	{
		if (self->delay)
		{
			if (!level.active_fog)
			{
				// Fog isn't currently on
				memcpy(&level.fog, &gfogs[index], sizeof(fog_t));
				SetDefaultDensity(&level.fog); //mxd
			}

			VectorCopy(self->fog_color, gfogs[index].Color);
			CopyDensity(&gfogs[index], self); //mxd

			self->goal_frame = level.framenum + self->delay * 10 + 1;
			self->think = fog_fade;
			self->nextthink = level.time + FRAMETIME;

			memcpy(&fade_fog, &level.fog, sizeof(fog_t));
		}
		else
		{
			memcpy(&level.fog, &gfogs[index], sizeof(fog_t));
		}

		level.active_fog = self->fog_index;
		level.active_target_fog = self->fog_index;
	}
}

void SP_target_fog(edict_t *self)
{
	if (!allow_fog->integer || deathmatch->value || coop->value)
	{
		G_FreeEdict(self);
		return;
	}

	self->class_id = ENTITY_TARGET_FOG;

	if (!level.fogs)
		level.fogs = 1; // 1st fog reserved for console commands

	if (level.fogs >= MAX_FOGS)
	{
		gi.dprintf("Maximum number of fogs exceeded (%i / %i)!\n", level.fogs, MAX_FOGS - 1);
		G_FreeEdict(self);

		return;
	}

	self->delay = max(self->delay, 0.0f);
	self->fog_index = level.fogs + 1;

	fog_t* fog = &gfogs[level.fogs];
	fog->Trigger = false;
	fog->Model = self->fog_model;
	if (fog->Model < 0 || fog->Model > 2)
		fog->Model = 0;

	fog->GL_Model = GLModels[fog->Model];
	VectorCopy(self->fog_color, fog->Color);

	if (self->spawnflags & FOG_TURNOFF)
	{
		SetDefaultDensity(fog); //mxd
	}
	else
	{
		if (self->density == 0.0f)
			self->density = self->fog_density;
		else if (self->density < 0.0f)
			self->density = 0.0f;

		CopyDensity(fog, self); //mxd
	}

	AngleVectors(self->s.angles, fog->Dir, NULL, NULL);
	fog->ent = self;
	level.fogs++;
	self->use = target_fog_use;
	gi.linkentity(self);

	if (self->spawnflags & FOG_ON)
	{
		self->spawnflags &= ~FOG_ON;
		target_fog_use(self,NULL,NULL);
	}
}

/*QUAKED trigger_fog (1 0 0) ? x Toggle x StartOff 
Fog field

"fog_color" specify an RGB color: Default = .5 .5 .5
"fog_model" default = 1
   0 :Linear
   1 :Exp
   2 :Exp2

"fog_near" Starting distance from player. Default =  64
"fog_far" How far the player can see into the fog. Default = 1024
"fog_density" Default = 20
"density" at 180 degrees; Default=0
"delay" ramp time in seconds
"count" number of times it can be used
*/
void trigger_fog_use(edict_t *self, edict_t *other, edict_t *activator)
{
	if ((self->spawnflags & FOG_ON) && (self->spawnflags & FOG_TOGGLE))
	{
		self->spawnflags &= ~FOG_ON;
		self->count--;

		if (self->count == 0)
		{
			self->think = G_FreeEdict;
			self->nextthink = level.time + FRAMETIME;
		}
	}
	else
	{
		self->spawnflags |= FOG_ON;
	}
}

void SP_trigger_fog(edict_t *self)
{
	if (!allow_fog->integer || deathmatch->integer || coop->integer)
	{
		G_FreeEdict(self);
		return;
	}

	self->class_id = ENTITY_TRIGGER_FOG;

	if (!level.fogs)
		level.fogs = 1; // 1st fog reserved for console commands

	if (level.fogs >= MAX_FOGS)
	{
		gi.dprintf("Maximum number of fogs exceeded (%i / %i)!\n", level.fogs, MAX_FOGS - 1);
		G_FreeEdict(self);

		return;
	}

	self->fog_index = level.fogs + 1;

	fog_t* fog = &gfogs[level.fogs];
	fog->Trigger = true;
	fog->Model = self->fog_model;
	if (fog->Model < 0 || fog->Model > 2)
		fog->Model = 0;

	fog->GL_Model = GLModels[fog->Model];
	VectorCopy(self->fog_color, fog->Color);

	if (self->spawnflags & FOG_TURNOFF)
	{
		SetDefaultDensity(fog); //mxd
	}
	else
	{
		if (self->density == 0.0f)
			self->density = self->fog_density;
		else if (self->density < 0.0f)
			self->density = 0.0f;

		CopyDensity(fog, self); //mxd
	}

	if (!(self->spawnflags & FOG_STARTOFF))
		self->spawnflags |= FOG_ON;

	AngleVectors(self->s.angles, fog->Dir, NULL, NULL);
	VectorClear(self->s.angles);
	fog->ent = self;
	level.fogs++;
	level.trigger_fogs++;

	self->movetype = MOVETYPE_NONE;
	self->svflags |= SVF_NOCLIENT;
	self->solid = SOLID_NOT;

	gi.setmodel(self, self->model);
	gi.linkentity(self);
}

/*QUAKED trigger_fog_bbox (.5 .5 .5) (-8 -8 -8) (8 8 8) x Toggle x StartOff 
Fog field

"fog_color" specify an RGB color: Default = .5 .5 .5
"fog_model" default = 1
   0 :Linear
   1 :Exp
   2 :Exp2

"fog_near" Starting distance from player. Default =  64
"fog_far" How far the player can see into the fog. Default = 1024
"fog_density" Default = 20
"density" at 180 degrees; Default=0
"delay" ramp time in seconds
"count" number of times it can be used

bleft Min b-box coords XYZ. Default = -16 -16 -16
tright Max b-box coords XYZ. Default = 16 16 16
*/
void SP_trigger_fog_bbox(edict_t *self)
{
	if (!allow_fog->integer || deathmatch->integer || coop->integer)
	{
		G_FreeEdict(self);
		return;
	}

	self->class_id = ENTITY_TRIGGER_FOG;

	if (!level.fogs)
		level.fogs = 1; // 1st fog reserved for console commands

	if (level.fogs >= MAX_FOGS)
	{
		gi.dprintf("Maximum number of fogs exceeded (%i / %i)!\n", level.fogs, MAX_FOGS - 1);
		G_FreeEdict(self);

		return;
	}

	self->fog_index = level.fogs + 1;

	fog_t* fog = &gfogs[level.fogs];
	fog->Trigger = true;
	fog->Model = self->fog_model;
	if (fog->Model < 0 || fog->Model > 2)
		fog->Model = 0;

	fog->GL_Model = GLModels[fog->Model];
	VectorCopy(self->fog_color, fog->Color);

	if (self->spawnflags & FOG_TURNOFF)
	{
		SetDefaultDensity(fog); //mxd
	}
	else
	{
		if (self->density == 0.0f)
			self->density = self->fog_density;
		else if (self->density < 0.0f)
			self->density = 0.0f;

		CopyDensity(fog, self); //mxd
	}

	if (!(self->spawnflags & FOG_STARTOFF))
		self->spawnflags |= FOG_ON;

	AngleVectors(self->s.angles, fog->Dir, NULL, NULL);
	VectorClear(self->s.angles);
	fog->ent = self;

	level.fogs++;
	level.trigger_fogs++;

	self->movetype = MOVETYPE_NONE;
	self->svflags |= SVF_NOCLIENT;
	self->solid = SOLID_NOT;

	if (!VectorLength(self->bleft) && !VectorLength(self->tright))
	{
		VectorSet(self->bleft, -16, -16, -16);
		VectorSet(self->tright, 16, 16, 16);
	}

	VectorCopy(self->bleft, self->mins);
	VectorCopy(self->tright, self->maxs);

	gi.linkentity(self);
}