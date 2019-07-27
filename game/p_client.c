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
#include "m_player.h"

#define MUD1BASE	0.2f
#define MUD1AMP		0.08f
#define MUD3		0.08f

#pragma region ======================= Gross, ugly, disgustuing hack section

// This function is an ugly as hell hack to fix some map flaws.
// The coop spawn spots on some maps are SNAFU. There are coop spots with the wrong targetname as well as spots with no name at all.
// We use carnal knowledge of the maps to fix the coop spot targetnames to match that of the nearest named single player spot.
void SP_FixCoopSpots(edict_t *self)
{
	edict_t *spot = NULL;

	while (true)
	{
		spot = G_Find(spot, FOFS(classname), "info_player_start");
		if (!spot)
			return;

		if (!spot->targetname)
			continue;

		vec3_t d;
		VectorSubtract(self->s.origin, spot->s.origin, d);
		if (VectorLength(d) < 384)
		{
			if (!self->targetname || Q_stricmp(self->targetname, spot->targetname) != 0)
				self->targetname = spot->targetname;

			return;
		}
	}
}

// Now if that one wasn't ugly enough for you then try this one on for size.
// Some maps don't have any coop spots at all, so we need to create them where they should have been.
void SP_CreateCoopSpots(edict_t *self)
{
	if (Q_stricmp(level.mapname, "security") == 0)
	{
		edict_t *spot = G_Spawn();
		spot->classname = "info_player_coop";
		VectorSet(spot->s.origin, 188 - 64, -164, 80);
		spot->targetname = "jail3";
		spot->s.angles[1] = 90;

		spot = G_Spawn();
		spot->classname = "info_player_coop";
		VectorSet(spot->s.origin, 188 + 64, -164, 80);
		spot->targetname = "jail3";
		spot->s.angles[1] = 90;

		spot = G_Spawn();
		spot->classname = "info_player_coop";
		VectorSet(spot->s.origin, 188 + 128, -164, 80);
		spot->targetname = "jail3";
		spot->s.angles[1] = 90;
	}
}

// QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
// The normal starting point for a level.
void SP_info_player_start(edict_t *self)
{
	if (coop->integer && Q_stricmp(level.mapname, "security") == 0)
	{
		// Invoke one of our gross, ugly, disgusting hacks
		self->think = SP_CreateCoopSpots;
		self->nextthink = level.time + FRAMETIME;
	}
}

extern void SP_misc_teleporter_dest(edict_t *ent);

// QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32)
// Potential spawning position for deathmatch games.
void SP_info_player_deathmatch(edict_t *self)
{
	if (!deathmatch->integer)
	{
		G_FreeEdict(self);
		return;
	}

	SP_misc_teleporter_dest(self);
}

// QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 32)
// Potential spawning position for coop games.
void SP_info_player_coop(edict_t *self)
{
	if (!coop->integer)
	{
		G_FreeEdict(self);
		return;
	}

	if (Q_stricmp(level.mapname, "jail2") == 0    ||
	   (Q_stricmp(level.mapname, "jail4") == 0)   ||
	   (Q_stricmp(level.mapname, "mine1") == 0)   ||
	   (Q_stricmp(level.mapname, "mine2") == 0)   ||
	   (Q_stricmp(level.mapname, "mine3") == 0)   ||
	   (Q_stricmp(level.mapname, "mine4") == 0)   ||
	   (Q_stricmp(level.mapname, "lab") == 0)     ||
	   (Q_stricmp(level.mapname, "boss1") == 0)   ||
	   (Q_stricmp(level.mapname, "fact3") == 0)   ||
	   (Q_stricmp(level.mapname, "biggun") == 0)  ||
	   (Q_stricmp(level.mapname, "space") == 0)   ||
	   (Q_stricmp(level.mapname, "command") == 0) ||
	   (Q_stricmp(level.mapname, "power2") == 0)  ||
	   (Q_stricmp(level.mapname, "strike") == 0))
	{
		// Invoke one of our gross, ugly, disgusting hacks
		self->think = SP_FixCoopSpots;
		self->nextthink = level.time + FRAMETIME;
	}
}

// QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32) LETTERBOX
// The deathmatch intermission point will be at one of these.
// Use 'angles' instead of 'angle', so you can set pitch or roll as well as yaw.
void SP_info_player_intermission() { }

#pragma endregion

void player_pain(edict_t *self, edict_t *other, float kick, int damage)
{
	// Player pain is handled at the end of the frame in P_DamageFeedback
}

static qboolean IsFemale(edict_t *ent)
{
	if (!ent->client)
		return false;

	char *info = Info_ValueForKey(ent->client->pers.userinfo, "gender");
	return (info[0] == 'f' || info[0] == 'F' || strstr(info, "crakhor")); // Knightmare: +crakhor
}

static qboolean IsNeutral(edict_t *ent)
{
	if (!ent->client)
		return false;

	char *info = Info_ValueForKey(ent->client->pers.userinfo, "gender");
	return (info[0] != 'f' && info[0] != 'F' && info[0] != 'm' && info[0] != 'M' && !strstr(info, "crakhor")); // Knightmare: +crakhor
}

static void ClientObituary(edict_t *self, edict_t *attacker)
{
	if (coop->integer && attacker->client)
		meansOfDeath |= MOD_FRIENDLY_FIRE;

	if (deathmatch->integer || coop->integer)
	{
		const qboolean ff = meansOfDeath & MOD_FRIENDLY_FIRE;
		const int mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;
		char *message = NULL;
		char *message2 = "";

		switch (mod)
		{
			case MOD_SUICIDE:
				message = "suicides";
				break;

			case MOD_FALLING:
				message = "cratered";
				break;

			case MOD_CRUSH:
				message = "was squished";
				break;

			case MOD_WATER:
				message = "sank like a rock";
				break;

			case MOD_SLIME:
				message = "melted";
				break;

			case MOD_LAVA:
				message = "does a back flip into the lava";
				break;

			case MOD_EXPLOSIVE:
			case MOD_BARREL:
				message = "blew up";
				break;

			case MOD_EXIT:
				message = "found a way out";
				break;

			case MOD_TARGET_LASER:
				message = "saw the light";
				break;

			case MOD_TARGET_BLASTER:
				message = "got blasted";
				break;

			case MOD_BOMB:
			case MOD_SPLASH:
			case MOD_TRIGGER_HURT:
			case MOD_VEHICLE:
				message = "was in the wrong place";
				break;
		}

		if (attacker == self)
		{
			switch (mod)
			{
			case MOD_HELD_GRENADE:
				message = "tried to put the pin back in";
				break;

			case MOD_HG_SPLASH:
			case MOD_G_SPLASH:
				if (IsNeutral(self))
					message = "tripped on its own grenade";
				else if (IsFemale(self))
					message = "tripped on her own grenade";
				else
					message = "tripped on his own grenade";
				break;

			case MOD_R_SPLASH:
				if (IsNeutral(self))
					message = "blew itself up";
				else if (IsFemale(self))
					message = "blew herself up";
				else
					message = "blew himself up";
				break;

			case MOD_BFG_BLAST:
				message = "should have used a smaller gun";
				break;

			default:
				if (IsNeutral(self))
					message = "killed itself";
				else if (IsFemale(self))
					message = "killed herself";
				else
					message = "killed himself";
				break;
			}
		}

		if (message)
		{
			safe_bprintf(PRINT_MEDIUM, "%s %s.\n", self->client->pers.netname, message);
			if (deathmatch->integer)
				self->client->resp.score--;
			self->enemy = NULL;

			return;
		}

		self->enemy = attacker;
		if (attacker && attacker->client)
		{
			switch (mod)
			{
				case MOD_BLASTER:
					message = "was blasted by";
					break;

				case MOD_SHOTGUN:
					message = "was gunned down by";
					break;

				case MOD_SSHOTGUN:
					message = "was blown away by";
					message2 = "'s super shotgun";
					break;

				case MOD_MACHINEGUN:
					message = "was machinegunned by";
					break;

				case MOD_CHAINGUN:
					message = "was cut in half by";
					message2 = "'s chaingun";
					break;

				case MOD_GRENADE:
					message = "was popped by";
					message2 = "'s grenade";
					break;

				case MOD_G_SPLASH:
					message = "was shredded by";
					message2 = "'s shrapnel";
					break;

				case MOD_ROCKET:
					message = "ate";
					message2 = "'s rocket";
					break;

				case MOD_R_SPLASH:
					message = "almost dodged";
					message2 = "'s rocket";
					break;

				case MOD_HYPERBLASTER:
					message = "was melted by";
					message2 = "'s hyperblaster";
					break;

				case MOD_RAILGUN:
					message = "was railed by";
					break;

				case MOD_BFG_LASER:
					message = "saw the pretty lights from";
					message2 = "'s BFG";
					break;

				case MOD_BFG_BLAST:
					message = "was disintegrated by";
					message2 = "'s BFG blast";
					break;

				case MOD_BFG_EFFECT:
					message = "couldn't hide from";
					message2 = "'s BFG";
					break;

				case MOD_HANDGRENADE:
					message = "caught";
					message2 = "'s handgrenade";
					break;

				case MOD_HG_SPLASH:
					message = "didn't see";
					message2 = "'s handgrenade";
					break;

				case MOD_HELD_GRENADE:
					message = "feels";
					message2 = "'s pain";
					break;

				case MOD_TELEFRAG:
					message = "tried to invade";
					message2 = "'s personal space";
					break;

				case MOD_GRAPPLE: //ZOID
					message = "was caught by";
					message2 = "'s grapple";
					break;

				case MOD_VEHICLE: //ZOID
					message = "was splattered by";
					message2 = "'s vehicle";
					break;

				case MOD_KICK:
					message = "was booted by";
					message2 = "'s foot";
					break;
			}

			if (message)
			{
				safe_bprintf(PRINT_MEDIUM,"%s %s %s%s\n", self->client->pers.netname, message, attacker->client->pers.netname, message2);
				if (deathmatch->integer)
				{
					if (ff)
						attacker->client->resp.score--;
					else
						attacker->client->resp.score++;
				}

				return;
			}
		}
		//Knightmare- Single-player obits
		if (attacker->svflags & SVF_MONSTER)
		{
			// Light Guard
			if (!strcmp(attacker->classname, "monster_soldier_light"))
			{
				message = "was blasted by a";
			}
			// Shotgun Guard
			else if (!strcmp(attacker->classname, "monster_soldier"))
			{
				message = "was gunned down by a";
			}
			// Machinegun Guard
			else if (!strcmp(attacker->classname, "monster_soldier_ss"))
			{
				message = "was machinegunned by a";
			}
			// Enforcer
			else if (!strcmp(attacker->classname, "monster_infantry"))
			{
				if (mod == MOD_HIT)
					message = "was bludgened by an";
				else
					message = "was pumped full of lead by an";
			}
			// Gunner
			else if (!strcmp(attacker->classname, "monster_gunner"))
			{
				if (mod == MOD_GRENADE)
				{
					message = "was popped by a";
					message2 = "'s grenade";
				}
				else if (mod == MOD_G_SPLASH)
				{
					message = "was shredded by a";
					message2 = "'s shrapnel";
				}
				else
				{
					message = "was machinegunned by a";
				}
			}
			// Berserker
			else if (!strcmp(attacker->classname, "monster_berserk"))
			{
				message = "was smashed by a";
			}
			// Gladiator
			else if (!strcmp(attacker->classname, "monster_gladiator"))
			{
				if (mod == MOD_RAILGUN)
				{
					message = "was railed by a";
				}
				else
				{
					message = "was mangled by a";
					message2 = "'s claw";
				}
			}
			// Medic
			else if (!strcmp(attacker->classname, "monster_medic"))
			{
				message = "was blasted by a";
			}
			// Icarus
			else if (!strcmp(attacker->classname, "monster_hover"))
			{
				message = "was blasted by an";
			}
			// Iron Maiden
			else if (!strcmp(attacker->classname, "monster_chick"))
			{
				if (mod == MOD_ROCKET)
				{
					message = "ate an";
					message2 = "'s rocket";
				}
				else if (mod == MOD_R_SPLASH)
				{
					message = "almost dodged an";
					message2 = "'s rocket";
				}
				else if (mod == MOD_HIT)
				{
					message = "was bitch-slapped by an";
				}
			}
			// Parasite
			else if (!strcmp(attacker->classname, "monster_parasite"))
			{
				message = "was exsanguiated by a";
			}
			// Brain
			else if (!strcmp(attacker->classname, "monster_brain"))
			{
				message = "was torn up by a";
				message2 = "'s tentacles";
			}
			// Flyer
			else if (!strcmp(attacker->classname, "monster_flyer"))
			{
				if (mod == MOD_BLASTER)
				{
					message = "was blasted by a";
				}
				else if (mod == MOD_HIT)
				{
					message = "was cut up by a";
					message2 = "'s sharp wings";
				}
			}
			// Technician
			else if (!strcmp(attacker->classname, "monster_floater"))
			{
				if (mod == MOD_BLASTER)
				{
					message = "was blasted by a";
				}
				else if (mod == MOD_HIT)
				{
					message = "was gouged to death by a";
					message2 = "'s utility claw";
				}
			}
			// Tank/Tank Commander
			else if (!strcmp(attacker->classname, "monster_tank") || !strcmp(attacker->classname, "monster_tank_commander"))
			{
				if (mod == MOD_BLASTER)
				{
					message = "was blasted by a";
				}
				else if (mod == MOD_ROCKET)
				{
					message = "ate a";
					message2 = "'s rocket";
				}
				else if (mod == MOD_R_SPLASH)
				{
					message = "almost dodged a";
					message2 = "'s rocket";
				}
				else
				{
					message = "was pumped full of lead by a";
				}
			}
			// Supertank
			else if (!strcmp(attacker->classname, "monster_supertank"))
			{
				if (mod == MOD_ROCKET)
				{
					message = "ate a";
					message2 = "'s rocket";
				}
				else if (mod == MOD_R_SPLASH)
				{
					message = "almost dodged a";
					message2 = "'s rocket";
				}
				else
				{
					message = "was chaingunned by a";
				}
			}
			// Hornet
			else if (!strcmp(attacker->classname, "monster_boss2"))
			{
				if (mod == MOD_ROCKET)
				{
					message = "ate a";
					message2 = "'s rocket";
				}
				else if (mod == MOD_R_SPLASH)
				{
					message = "almost dodged a";
					message2 = "'s rockets";
				}
				else
				{
					message = "was chaingunned by a";
				}
			}
			// Jorg
			else if (!strcmp(attacker->classname, "monster_jorg"))
			{
				if (mod == MOD_BFG_LASER)
				{
					message = "saw the pretty lights from the";
					message2 = "'s BFG";
				}
				else if (mod == MOD_BFG_BLAST)
				{
					message = "was disintegrated by the";
					message2 = "'s BFG blast";
				}
				else if (mod == MOD_BFG_EFFECT)
				{
					message = "couldn't hide from the";
					message2 = "'s BFG";
				}
				else
				{
					message = "was shredded by the";
					message2 = "'s chain-cannons";
				}
			}
			// Makron
			else if (!strcmp(attacker->classname, "monster_makron"))
			{
				if (mod == MOD_BLASTER)
				{
					message = "was melted by the";
					message2 = "'s hyperblaster";
				}
				else if (mod == MOD_RAILGUN)
					message = "was railed by the";
				else if (mod == MOD_BFG_LASER)
				{
					message = "saw the pretty lights from the";
					message2 = "'s BFG";
				}
				else if (mod == MOD_BFG_BLAST)
				{
					message = "was disintegrated by the";
					message2 = "'s BFG blast";
				}
				else if (mod == MOD_BFG_EFFECT)
				{
					message = "couldn't hide from the";
					message2 = "'s BFG";
				}
			}
			// Barracuda Shark
			else if (!strcmp(attacker->classname, "monster_flipper"))
			{
				message = "was chewed up by a";
			}
			// Mutant
			else if (!strcmp(attacker->classname, "monster_mutant"))
			{
				message = "was clawed by a";
			}
		}

		if (message)
		{
			safe_bprintf (PRINT_MEDIUM,"%s %s %s%s\n", self->client->pers.netname, message, attacker->common_name, message2);
			if (coop->integer)
				self->client->resp.score--;
			self->enemy = NULL;

			return;
		}
	}

	safe_bprintf(PRINT_MEDIUM,"%s died.\n", self->client->pers.netname);
	if (deathmatch->integer)
		self->client->resp.score--;
}

static void TossClientWeapon(edict_t *self)
{
	if (!deathmatch->integer)
		return;

	gitem_t *item = self->client->pers.weapon;

	if (!self->client->pers.inventory[self->client->ammo_index])
		item = NULL;

	if (item && (strcmp(item->pickup_name, "Blaster") == 0))
		item = NULL;

	if (item && (strcmp(item->pickup_name, "Grapple") == 0))
		item = NULL;

	if (item && (strcmp(item->pickup_name, "No Weapon") == 0))
		item = NULL;

	// Knightmare- don't drop homing rocket launcher (null model error), drop rocket launcher instead
	if (item && (strcmp(item->pickup_name, "Homing Rocket Launcher") == 0))
		item = FindItem("Rocket Launcher");

	qboolean quad;
	if (!(dmflags->integer & DF_QUAD_DROP))
		quad = false;
	else
		quad = (self->client->quad_framenum > (level.framenum + 10));

	const float spread = (item && quad ? 22.5f : 0.0f);

	if (item)
	{
		self->client->v_angle[YAW] -= spread;

		edict_t *drop = Drop_Item(self, item);
		self->client->v_angle[YAW] += spread;
		drop->spawnflags = DROPPED_PLAYER_ITEM;
	}

	if (quad)
	{
		self->client->v_angle[YAW] += spread;

		edict_t *drop = Drop_Item(self, FindItemByClassname("item_quad"));
		self->client->v_angle[YAW] -= spread;
		drop->spawnflags |= DROPPED_PLAYER_ITEM;

		drop->touch = Touch_Item;
		drop->nextthink = level.time + (self->client->quad_framenum - level.framenum) * FRAMETIME;
		drop->think = G_FreeEdict;
	}
}

static void LookAtKiller(edict_t *self, edict_t *inflictor, edict_t *attacker)
{
	vec3_t dir;

	if (attacker && attacker != world && attacker != self)
	{
		VectorSubtract(attacker->s.origin, self->s.origin, dir);
	}
	else if (inflictor && inflictor != world && inflictor != self)
	{
		VectorSubtract(inflictor->s.origin, self->s.origin, dir);
	}
	else
	{
		self->client->killer_yaw = self->s.angles[YAW];
		return;
	}

	if (dir[0])
		self->client->killer_yaw = 180 / M_PI * atan2f(dir[1], dir[0]);
	else
		self->client->killer_yaw = 90 * sign(dir[1]);

	if (self->client->killer_yaw < 0)
		self->client->killer_yaw += 360;
}

void player_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	// tpp
	if (self->client->chasetoggle)
	{
		ChasecamRemove(self, OPTION_OFF);
		self->client->pers.chasetoggle = 1;
	}
	else
	{
		self->client->pers.chasetoggle = 0;
	}

	self->client->pers.spawn_landmark = false; // Paranoia check
	self->client->pers.spawn_levelchange = false;
	SetLazarusCrosshair(self); // Backup crosshair
	self->client->zooming = 0;
	self->client->zoomed = false;
	SetSensitivities(self, true);

	if (self->client->spycam)
		camera_off(self);

	if (self->turret)
		turret_disengage(self->turret);

	if (self->client->textdisplay)
		Text_Close(self);

	VectorClear(self->avelocity);

	self->takedamage = DAMAGE_YES;
	self->movetype = MOVETYPE_TOSS;

	self->s.modelindex2 = 0; // Remove linked weapon model
	self->s.modelindex3 = 0; //ZOID. Remove linked ctf flag

	self->s.angles[0] = 0;
	self->s.angles[2] = 0;

	self->s.sound = 0;
	self->client->weapon_sound = 0;

	self->maxs[2] = -8;

	self->svflags |= SVF_DEADMONSTER;

	if (!self->deadflag)
	{
		self->client->respawn_time = level.time + 1.0f;
		LookAtKiller(self, inflictor, attacker);
		self->client->ps.pmove.pm_type = PM_DEAD;
		ClientObituary(self, attacker);

		//ZOID
		if (ctf->integer)
		{
			// If at start and same team, clear
			if (meansOfDeath == MOD_TELEFRAG && self->client->resp.ctf_state < 2 && self->client->resp.ctf_team == attacker->client->resp.ctf_team)
			{
				attacker->client->resp.score--;
				self->client->resp.ctf_state = 0;
			}

			CTFFragBonuses(self, inflictor, attacker);
		}

		TossClientWeapon(self);

		//ZOID
		if (ctf->integer)
		{
			CTFPlayerResetGrapple(self);
			CTFDeadDropFlag(self);
		}

		CTFDeadDropTech(self); //ZOID
	
		// Knightmare added- drop ammogen backpack
		if (!OnSameTeam(self, attacker))
			CTFApplyAmmogen(attacker, self);

		if (deathmatch->integer)
			Cmd_Help_f(self); // Show scores

		// Clear inventory. This is kind of ugly, but it's how we want to handle keys in coop.
		for (int n = 0; n < game.num_items; n++)
		{
			if (coop->integer && itemlist[n].flags & IT_KEY)
				self->client->resp.coop_respawn.inventory[n] = self->client->pers.inventory[n];

			self->client->pers.inventory[n] = 0;
		}
	}

	// Remove powerups
	self->client->quad_framenum = 0;
	self->client->invincible_framenum = 0;
	self->client->breather_framenum = 0;
	self->client->enviro_framenum = 0;
	self->flags &= ~(FL_POWER_SHIELD | FL_POWER_SCREEN);
	self->client->flashlight = false;

	// Turn off alt-fire mode if on
	self->client->pers.fire_mode = 0;

	if (self->health < player_gib_health->value)
	{
		// Spawn gibs
		int num_giblets = 4;

		if (deathmatch->integer && self->health < player_gib_health->value * 2)
			num_giblets = 8;

		gi.sound(self, CHAN_BODY, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);

		for (int n = 0; n < num_giblets; n++)
			ThrowGib(self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);

		if (mega_gibs->integer)
		{
			ThrowGib(self, "models/objects/gibs/arm/tris.md2", damage, GIB_ORGANIC);
			ThrowGib(self, "models/objects/gibs/arm/tris.md2", damage, GIB_ORGANIC);
			ThrowGib(self, "models/objects/gibs/leg/tris.md2", damage, GIB_ORGANIC);
			ThrowGib(self, "models/objects/gibs/leg/tris.md2", damage, GIB_ORGANIC);
			ThrowGib(self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			ThrowGib(self, "models/objects/gibs/bone2/tris.md2", damage, GIB_ORGANIC);
		}

		ThrowClientHead(self, damage);

		//ZOID
		self->client->anim_priority = ANIM_DEATH;
		self->client->anim_end = 0;

		self->takedamage = DAMAGE_NO;
	}
	else
	{
		// Normal death
		if (!self->deadflag)
		{
			static int i;

			i = (i + 1) % 3;

			// Start a death animation
			self->client->anim_priority = ANIM_DEATH;

			if (self->client->ps.pmove.pm_flags & PMF_DUCKED)
			{
				self->s.frame = FRAME_crdeath1 - 1;
				self->client->anim_end = FRAME_crdeath5;
			}
			else 
			{
				switch (i)
				{
					case 0:
						self->s.frame = FRAME_death101 - 1;
						self->client->anim_end = FRAME_death106;
						break;

					case 1:
						self->s.frame = FRAME_death201 - 1;
						self->client->anim_end = FRAME_death206;
						break;

					case 2:
						self->s.frame = FRAME_death301 - 1;
						self->client->anim_end = FRAME_death308;
						break;
				}
			}

			gi.sound(self, CHAN_VOICE, gi.soundindex(va("*death%i.wav", (rand() % 4) + 1)), 1, ATTN_NORM, 0);
		}
	}

#ifdef JETPACK_MOD
	if (self->client->jetpack)
	{
		Jet_BecomeExplosion(self, damage);

		// Stop jetting when dead
		self->client->jetpack_framenum = 0;
		self->client->jetpack = false;

		// DWH: force player to gib
		self->health = player_gib_health->value - 1;
	}
#endif

	self->deadflag = DEAD_DEAD;
	gi.linkentity(self);
}

//=======================================================================

static void SwitchToBestStartWeapon(gclient_t *client)
{
	if (!client)
		return;

	if (client->pers.inventory[slugs_index] && client->pers.inventory[ITEM_INDEX(FindItem("railgun"))])
		client->pers.weapon = FindItem("railgun");
	else if (client->pers.inventory[cells_index] && client->pers.inventory[ITEM_INDEX(FindItem("hyperblaster"))])
		client->pers.weapon = FindItem("hyperblaster");
	else if (client->pers.inventory[bullets_index] && client->pers.inventory[ITEM_INDEX(FindItem("chaingun"))])
		client->pers.weapon = FindItem("chaingun");
	else if (client->pers.inventory[bullets_index] && client->pers.inventory[ITEM_INDEX(FindItem("machinegun"))])
		client->pers.weapon = FindItem("machinegun");
	else if (client->pers.inventory[shells_index] > 1 && client->pers.inventory[ITEM_INDEX(FindItem("super shotgun"))])
		client->pers.weapon = FindItem("super shotgun");
	else if (client->pers.inventory[shells_index] && client->pers.inventory[ITEM_INDEX(FindItem("shotgun"))])
		client->pers.weapon = FindItem("shotgun");
	else if (client->pers.inventory[ITEM_INDEX(FindItem("blaster"))]) // DWH: Dude may not HAVE a blaster
		client->pers.weapon = FindItem("blaster");
	else
		client->pers.weapon = FindItem("No Weapon");
}

static void SelectStartWeapon(gclient_t *client, const int style)
{
	gitem_t *item;

	// Lazarus: We allow choice of weapons (or no weapon) at startup.
	// If style is non-zero, first clear player inventory of all weapons and ammo that might have been passed over 
	// through target_changelevel or acquired when previously called by InitClientPersistant
	if (style)
	{
		for (int n = 0; n < MAX_ITEMS; n++)
			if (itemlist[n].flags & IT_WEAPON)
				client->pers.inventory[n] = 0;

		client->pers.inventory[shells_index] = 0;
		client->pers.inventory[bullets_index] = 0;
		client->pers.inventory[grenades_index] = 0;
		client->pers.inventory[rockets_index] = 0;
		client->pers.inventory[cells_index] = 0;
		client->pers.inventory[slugs_index] = 0;
		client->pers.inventory[homing_index] = 0;
	}

	switch (style)
	{
		case -1: item = FindItem("No Weapon"); break;
		case -2: case  2: item = FindItem("Shotgun"); break;
		case -3: case  3: item = FindItem("Super Shotgun"); break;
		case -4: case  4: item = FindItem("Machinegun"); break;
		case -5: case  5: item = FindItem("Chaingun"); break;
		case -6: case  6: item = FindItem("Grenade Launcher"); break;
		case -7: case  7: item = FindItem("Rocket Launcher"); break;
		case -8: case  8: item = FindItem("HyperBlaster"); break;
		case -9: case  9: item = FindItem("Railgun"); break;
		case -10: case 10: item = FindItem("BFG10K"); break;
		default: item = FindItem("Blaster"); break;
	}

	client->pers.selected_item = ITEM_INDEX(item);
	client->pers.inventory[client->pers.selected_item] = 1;
	client->pers.weapon = item;

	//ZOID
	if (ctf->integer)
	{
		client->pers.lastweapon = item;
		item = FindItem("Grapple");
		client->pers.inventory[ITEM_INDEX(item)] = 1;
	}

	// Lazarus: If default weapon is NOT "No Weapon", then give player a blaster
	if (style > 1)
		client->pers.inventory[ITEM_INDEX(FindItem("Blaster"))] = 1;

	// Knightmare- player always has null weapon to allow holstering
	client->pers.inventory[ITEM_INDEX(FindItem("No Weapon"))] = 1;

	// And give him standard ammo
	if (item->ammo)
	{
		gitem_t *ammo = FindItem(item->ammo);

		if (deathmatch->integer && (dmflags->integer & DF_INFINITE_AMMO))
			client->pers.inventory[ITEM_INDEX(ammo)] += 1000;
		else
			client->pers.inventory[ITEM_INDEX(ammo)] += ammo->quantity;
	}

	// Knightmare- DM start values
	if (deathmatch->integer)
	{
		client->pers.inventory[ITEM_INDEX(FindItem("Shells"))] = sk_dm_start_shells->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("Bullets"))] = sk_dm_start_bullets->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("Rockets"))] = sk_dm_start_rockets->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("Homing Rockets"))] = sk_dm_start_homing->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("Grenades"))] = sk_dm_start_grenades->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("Cells"))] = sk_dm_start_cells->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("Slugs"))] = sk_dm_start_slugs->integer;

		client->pers.inventory[ITEM_INDEX(FindItem("Shotgun"))] = sk_dm_start_shotgun->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("Super Shotgun"))] = sk_dm_start_sshotgun->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("Machinegun"))] = sk_dm_start_machinegun->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("Chaingun"))] = sk_dm_start_chaingun->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("Grenade Launcher"))] = sk_dm_start_grenadelauncher->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("Rocket Launcher"))] = sk_dm_start_rocketlauncher->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("Homing Rocket Launcher"))] = sk_dm_start_rocketlauncher->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("HyperBlaster"))] = sk_dm_start_hyperblaster->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("Railgun"))] = sk_dm_start_railgun->integer;
		client->pers.inventory[ITEM_INDEX(FindItem("BFG10K"))] = sk_dm_start_bfg->integer;

		SwitchToBestStartWeapon(client);
	}
}

// This is only called when the game first initializes in single player, but is called after each death and level change in deathmatch.
void InitClientPersistant(gclient_t *client, const int style)
{
	memset(&client->pers, 0, sizeof(client->pers));

	client->homing_rocket = NULL;
	SelectStartWeapon(client, style);

	client->pers.health = 100;
	if (deathmatch->integer)
		client->pers.max_health	= sk_max_health_dm->integer;
	else
		client->pers.max_health	= sk_max_health->integer;

	client->pers.max_bullets = sk_max_bullets->integer;
	client->pers.max_shells = sk_max_shells->integer;
	client->pers.max_rockets = sk_max_rockets->integer;
	client->pers.max_grenades = sk_max_grenades->integer;
	client->pers.max_cells = sk_max_cells->integer;
	client->pers.max_slugs = sk_max_slugs->integer;
	client->pers.max_fuel = sk_max_fuel->integer;
	client->pers.max_homing_rockets = sk_max_rockets->integer;
	client->pers.fire_mode = 0; // Lazarus alternate fire mode

	client->pers.connected = true;

	//Default chasecam to tpp setting
	client->pers.chasetoggle = tpp->integer;

	// Lazarus
	client->zooming = 0;
	client->zoomed = false;
	client->spycam = NULL;
	client->pers.spawn_landmark = false;
	client->pers.spawn_levelchange = false;
}

void InitClientResp(gclient_t *client)
{
	//ZOID
	const int ctf_team = client->resp.ctf_team;
	const qboolean id_state = client->resp.id_state;
	
	memset(&client->resp, 0, sizeof(client->resp));

	//ZOID
	client->resp.ctf_team = ctf_team;
	client->resp.id_state = id_state;

	client->resp.enterframe = level.framenum;
	client->resp.coop_respawn = client->pers;

	//ZOID
	if (ctf->integer && client->resp.ctf_team < CTF_TEAM1)
		CTFAssignTeam(client);
}

// Some information that should be persistant, like health, is still stored in the edict structure, 
// so it needs to be mirrored out to the client structure before all the edicts are wiped.
void SaveClientData()
{
	for (int i = 0; i < game.maxclients; i++)
	{
		edict_t *ent = &g_edicts[i + 1];
		if (!ent->inuse)
			continue;

		game.clients[i].pers.chasetoggle = ent->client->pers.chasetoggle; // tpp
		game.clients[i].pers.newweapon = ent->client->newweapon;
		game.clients[i].pers.health = ent->health;
		game.clients[i].pers.max_health = ent->max_health;
		game.clients[i].pers.savedFlags = (ent->flags & (FL_GODMODE | FL_NOTARGET | FL_POWER_SHIELD | FL_POWER_SCREEN));

		if (coop->integer)
			game.clients[i].pers.score = ent->client->resp.score;
	}
}

void FetchClientEntData(edict_t *ent)
{
	ent->health = ent->client->pers.health;
	ent->gib_health = player_gib_health->value; // was -40
	ent->max_health = ent->client->pers.max_health;
	ent->flags |= ent->client->pers.savedFlags;

	if (coop->integer)
		ent->client->resp.score = ent->client->pers.score;
}

#pragma region ======================= Spawn point selection

// Returns the distance to the nearest player from the given spot.
float PlayersRangeFromSpot(edict_t *spot)
{
	float bestplayerdistance = 9999999;

	for (int n = 1; n <= maxclients->integer; n++)
	{
		edict_t *player = &g_edicts[n];

		if (!player->inuse || player->health <= 0)
			continue;

		vec3_t v;
		VectorSubtract(spot->s.origin, player->s.origin, v);
		const float playerdistance = VectorLength(v);

		bestplayerdistance = min(playerdistance, bestplayerdistance);
	}

	return bestplayerdistance;
}

// Go to a random point, but NOT the two points closest to other players.
edict_t *SelectRandomDeathmatchSpawnPoint()
{
	float range1 = 99999;
	float range2 = 99999;

	edict_t *spot = NULL;
	edict_t *spot1 = NULL;
	edict_t *spot2 = NULL;

	int count = 0;
	while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL)
	{
		count++;
		const float range = PlayersRangeFromSpot(spot);
		if (range < range1)
		{
			range1 = range;
			spot1 = spot;
		}
		else if (range < range2)
		{
			range2 = range;
			spot2 = spot;
		}
	}

	if (!count)
		return NULL;

	if (count <= 2)
	{
		spot1 = NULL;
		spot2 = NULL;
	}
	else
	{
		if (spot1)
			count--;

		if (spot2)
			count--;
	}

	int selection = rand() % count;
	spot = NULL;

	do
	{
		spot = G_Find(spot, FOFS(classname), "info_player_deathmatch");
		if (spot == spot1 || spot == spot2)
			selection++;
	} while (selection--);

	return spot;
}

edict_t *SelectFarthestDeathmatchSpawnPoint()
{
	edict_t *spot = NULL;
	edict_t *bestspot = NULL;
	float bestdistance = 0;

	while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL)
	{
		const float bestplayerdistance = PlayersRangeFromSpot(spot);

		if (bestplayerdistance > bestdistance)
		{
			bestspot = spot;
			bestdistance = bestplayerdistance;
		}
	}

	if (bestspot)
		return bestspot;

	// If there is a player just spawned on each and every start spot we have no choice to turn one into a telefrag meltdown...
	return G_Find(NULL, FOFS(classname), "info_player_deathmatch");
}

static edict_t *SelectDeathmatchSpawnPoint()
{
	if (dmflags->integer & DF_SPAWN_FARTHEST)
		return SelectFarthestDeathmatchSpawnPoint();

	return SelectRandomDeathmatchSpawnPoint();
}

static edict_t *SelectCoopSpawnPoint(edict_t *ent)
{
	int index = ent->client - game.clients;

	// Player 0 starts in normal player spawn point
	if (!index)
		return NULL;

	edict_t *spot = NULL;

	// Assume there are four coop spots at each spawnpoint.
	while (true)
	{
		spot = G_Find(spot, FOFS(classname), "info_player_coop");
		if (!spot)
			return NULL; // We didn't have enough...

		if (spot->targetname && !Q_stricmp(game.spawnpoint, spot->targetname) && !--index) // This is a coop spawn point for one of the clients here
			return spot; // This is it
	}
}

// Chooses a player start, deathmatch start, coop start, etc.
void SelectSpawnPoint(edict_t *ent, vec3_t origin, vec3_t angles, int *style, int *health)
{
	edict_t *spot = NULL;

	if (deathmatch->integer)
	{
		if (ctf->integer) //ZOID
			spot = SelectCTFSpawnPoint(ent);
		else
			spot = SelectDeathmatchSpawnPoint();
	}
	else if (coop->integer)
	{
		spot = SelectCoopSpawnPoint(ent);
	}

	// Find a single player start spot
	if (!spot)
	{
		while ((spot = G_Find(spot, FOFS(classname), "info_player_start")) != NULL)
		{
			if (!game.spawnpoint[0] && !spot->targetname)
				break;

			if (!game.spawnpoint[0] || !spot->targetname)
				continue;

			if (Q_stricmp(game.spawnpoint, spot->targetname) == 0)
				break;
		}

		if (!spot)
		{
			if (!game.spawnpoint[0])
				spot = G_Find(spot, FOFS(classname), "info_player_start"); // There wasn't a spawnpoint without a target, so use any.

			if (!spot)
				gi.error("Couldn't find spawn point %s\n", game.spawnpoint);
		}
	}

	if (style)
		*style = spot->style;

	if (health)
		*health = spot->health;

	VectorCopy(spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy(spot->s.angles, angles);

	if (!deathmatch->integer && !coop->integer)
	{
		spot->count--;
		if (!spot->count)
		{
			spot->think = G_FreeEdict;
			spot->nextthink = level.time + 1;
		}
	}
}

#pragma endregion

void InitBodyQue()
{
	// DWH: bodyque isn't used in SP, so why reserve space for it?
	if (deathmatch->integer || coop->integer)
	{
		level.body_que = 0;
		for (int i = 0; i < BODY_QUEUE_SIZE; i++)
		{
			edict_t *ent = G_Spawn();
			ent->classname = "bodyque";
		}
	}
}

void body_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	if (self->health < player_gib_health->value)
	{
		int num_giblets = 4;

		if (deathmatch->value && self->health < player_gib_health->value * 2)
			num_giblets = 8;

		gi.sound(self, CHAN_BODY, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);

		for (int n = 0; n < num_giblets; n++)
			ThrowGib(self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);

		if (mega_gibs->integer)
		{
			ThrowGib(self, "models/objects/gibs/arm/tris.md2", damage, GIB_ORGANIC);
			ThrowGib(self, "models/objects/gibs/arm/tris.md2", damage, GIB_ORGANIC);
			ThrowGib(self, "models/objects/gibs/leg/tris.md2", damage, GIB_ORGANIC);
			ThrowGib(self, "models/objects/gibs/leg/tris.md2", damage, GIB_ORGANIC);
			ThrowGib(self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
			ThrowGib(self, "models/objects/gibs/bone2/tris.md2", damage, GIB_ORGANIC);
		}

		self->s.origin[2] -= 48;
		ThrowClientHead(self, damage);
		self->takedamage = DAMAGE_NO;
	}
}

void CopyToBodyQue(edict_t *ent)
{
	// Grab a body que and cycle to the next one.
	edict_t *body = &g_edicts[maxclients->integer + level.body_que + 1];
	level.body_que = (level.body_que + 1) % BODY_QUEUE_SIZE;

	// FIXME: send an effect on the removed body
	gi.unlinkentity(ent);

	gi.unlinkentity(body);
	body->s = ent->s;
	body->s.number = body - g_edicts;

	body->svflags = ent->svflags;
	VectorCopy(ent->mins, body->mins);
	VectorCopy(ent->maxs, body->maxs);
	VectorCopy(ent->absmin, body->absmin);
	VectorCopy(ent->absmax, body->absmax);
	VectorCopy(ent->size, body->size);
	body->solid = ent->solid;
	body->clipmask = ent->clipmask;
	body->owner = ent->owner;
	body->movetype = ent->movetype;

	body->die = body_die;
	body->takedamage = DAMAGE_YES;

	gi.linkentity(body);
}

void respawn(edict_t *self)
{
	// tpp
	if (self->crosshair)
		G_FreeEdict(self->crosshair);
	self->crosshair = NULL;

	if (self->client->oldplayer)
		G_FreeEdict(self->client->oldplayer);
	self->client->oldplayer = NULL;

	if (self->client->chasecam)
		G_FreeEdict(self->client->chasecam);
	self->client->chasecam = NULL;

	if (deathmatch->integer || coop->integer)
	{
		if (self->is_bot) // ACEBOT_ADD special respawning code
		{
			ACESP_Respawn(self);
			return;
		}

		// Spectator's don't leave bodies
		if (self->movetype != MOVETYPE_NOCLIP)
			CopyToBodyQue(self);

		self->svflags &= ~SVF_NOCLIENT;
		PutClientInServer(self);

		// Add a teleportation effect
		self->s.event = EV_PLAYER_TELEPORT;

		// Hold in place briefly
		self->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
		self->client->ps.pmove.pm_time = 14;

		self->client->respawn_time = level.time;

		return;
	}

	// Restart the entire server
	gi.AddCommandString("menu_loadgame\n");
}

// Only called when pers.spectator changes.
// Note that resp.spectator should be the opposite of pers.spectator here.
static void SpectatorRespawn(edict_t *ent)
{
	// If the user wants to become a spectator, make sure he doesn't exceed max_spectators.
	if (ent->client->pers.spectator)
	{
		char *value = Info_ValueForKey(ent->client->pers.userinfo, "spectator");
		
		if (*spectator_password->string && strcmp(spectator_password->string, "none") && strcmp(spectator_password->string, value))
		{
			safe_cprintf(ent, PRINT_HIGH, "Spectator password incorrect.\n");
			ent->client->pers.spectator = false;
			gi.WriteByte(svc_stufftext);
			gi.WriteString("spectator 0\n");
			gi.unicast(ent, true);

			return;
		}

		// Count spectators
		int numspec = 0;
		for (int i = 1; i <= maxclients->integer; i++)
			if (g_edicts[i].inuse && g_edicts[i].client->pers.spectator)
				numspec++;

		if (numspec >= maxspectators->integer)
		{
			safe_cprintf(ent, PRINT_HIGH, "Server spectator limit is full.");
			ent->client->pers.spectator = false;

			// Reset his spectator var
			gi.WriteByte(svc_stufftext);
			gi.WriteString("spectator 0\n");
			gi.unicast(ent, true);

			return;
		}
	}
	else
	{
		// He was a spectator and wants to join the game. He must have the right password
		char *value = Info_ValueForKey(ent->client->pers.userinfo, "password");

		if (*password->string && strcmp(password->string, "none") && strcmp(password->string, value))
		{
			safe_cprintf(ent, PRINT_HIGH, "Password incorrect.\n");
			ent->client->pers.spectator = true;
			gi.WriteByte(svc_stufftext);
			gi.WriteString("spectator 1\n");
			gi.unicast(ent, true);

			return;
		}
	}

	// Clear client on respawn
	ent->client->resp.score = ent->client->pers.score = 0;

	ent->svflags &= ~SVF_NOCLIENT;
	PutClientInServer(ent);

	// Add a teleportation effect
	if (!ent->client->pers.spectator)
	{
		// Send effect
		gi.WriteByte(svc_muzzleflash);
		gi.WriteShort(ent-g_edicts);
		gi.WriteByte(MZ_LOGIN);
		gi.multicast(ent->s.origin, MULTICAST_PVS);

		// Hold in place briefly
		ent->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
		ent->client->ps.pmove.pm_time = 14;
	}

	ent->client->respawn_time = level.time;

	if (ent->client->pers.spectator) 
		safe_bprintf(PRINT_HIGH, "%s has moved to the sidelines\n", ent->client->pers.netname);
	else
		safe_bprintf(PRINT_HIGH, "%s joined the game\n", ent->client->pers.netname);
}

//==============================================================

// Called when a player connects to a server or respawns in deathmatch.
void PutClientInServer(edict_t *ent)
{
	// Find a spawn point. Do it before setting health back up, so farthest ranging doesn't count this client.
	vec3_t spawn_origin, spawn_angles;
	int spawn_style, spawn_health;
	SelectSpawnPoint(ent, spawn_origin, spawn_angles, &spawn_style, &spawn_health);

	vec3_t spawn_viewangles;
	int spawn_pm_flags = 0;
	const int index = ent - g_edicts - 1;
	gclient_t *client = ent->client;
	const int chasetoggle = client->pers.chasetoggle; // tpp
	gitem_t *newweapon = client->pers.newweapon;
	const int spawn_gunframe = client->pers.spawn_gunframe;
	const int spawn_modelframe = client->pers.spawn_modelframe;
	const int spawn_anim_end = client->pers.spawn_anim_end;

	const qboolean spawn_landmark = client->pers.spawn_landmark;
	client->pers.spawn_landmark = false;

	const qboolean spawn_levelchange = client->pers.spawn_levelchange;
	client->pers.spawn_levelchange = false;

	if (spawn_landmark)
	{
		spawn_origin[2] -= 9;
		VectorAdd(spawn_origin, client->pers.spawn_offset, spawn_origin);
		VectorCopy(client->pers.spawn_angles, spawn_angles);
		VectorCopy(client->pers.spawn_viewangles, spawn_viewangles);
		VectorCopy(client->pers.spawn_velocity, ent->velocity);
		spawn_pm_flags = client->pers.spawn_pm_flags;
	}

	// Deathmatch wipes most client data every spawn
	client_respawn_t resp;
	if (deathmatch->integer)
	{
		resp = client->resp;

		char userinfo[MAX_INFO_STRING];
		memcpy(userinfo, client->pers.userinfo, sizeof(userinfo));
		InitClientPersistant(client, spawn_style);
		ClientUserinfoChanged(ent, userinfo);
	}
	else if (coop->integer)
	{
		resp = client->resp;

		char userinfo[MAX_INFO_STRING];
		memcpy(userinfo, client->pers.userinfo, sizeof(userinfo));
		resp.coop_respawn.game_helpchanged = client->pers.game_helpchanged;
		resp.coop_respawn.helpchanged = client->pers.helpchanged;
		client->pers = resp.coop_respawn;
		ClientUserinfoChanged(ent, userinfo);

		if (resp.score > client->pers.score)
			client->pers.score = resp.score;
	}
	else
	{
		memset(&resp, 0, sizeof(resp));

		// tpp. A bug in Q2 that you couldn't see without thirdpp
		char userinfo[MAX_INFO_STRING];
		memcpy(userinfo, client->pers.userinfo, sizeof(userinfo));
		ClientUserinfoChanged(ent, userinfo);
	}

	// Clear everything but the persistant data
	const client_persistant_t saved = client->pers;
	memset(client, 0, sizeof(*client));
	client->pers = saved;

	if (client->pers.health <= 0)
		InitClientPersistant(client, spawn_style);
	else if (spawn_style)
		SelectStartWeapon(client, spawn_style);

	client->resp = resp;
	client->pers.chasetoggle = chasetoggle; // tpp
	client->pers.newweapon = newweapon;

	// Copy some data from the client to the entity
	FetchClientEntData(ent);

	// Lazarus: Starting health < max. Presumably player was hurt in a crash.
	if (spawn_health > 0 && !deathmatch->integer && !coop->integer)
		ent->health = min(ent->health, spawn_health);

	// Clear entity values
	ent->groundentity = NULL;
	ent->client = &game.clients[index];
	ent->takedamage = DAMAGE_AIM;
	ent->movetype = MOVETYPE_WALK;
	ent->viewheight = 22;
	ent->inuse = true;
	ent->classname = "player";
	ent->mass = 200;
	ent->solid = SOLID_BBOX;
	ent->deadflag = DEAD_NO;
	ent->air_finished = level.time + 12;
	ent->clipmask = MASK_PLAYERSOLID;
	ent->model = "players/male/tris.md2";
	ent->pain = player_pain;
	ent->die = player_die;
	ent->waterlevel = 0;
	ent->watertype = 0;
	ent->flags &= ~FL_NO_KNOCKBACK;
	ent->svflags &= ~SVF_DEADMONSTER;
	ent->svflags &= ~SVF_NOCLIENT; // tpp
	ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION; // tpp. Turn on prediction
	ent->client->spycam = NULL;
	ent->client->camplayer = NULL;

	// ACEBOT_ADD
	ent->is_bot = false;
	ent->last_node = -1;
	ent->is_jumping = false;

	VectorSet(ent->mins, -16, -16, -24);
	VectorSet(ent->maxs, 16, 16, 32);

	if (!spawn_landmark)
		VectorClear(ent->velocity);

	// Clear playerstate values
	memset(&ent->client->ps, 0, sizeof(client->ps));

	if (spawn_landmark)
		client->ps.pmove.pm_flags = spawn_pm_flags;

	for (int i = 0; i < 3; i++)
		client->ps.pmove.origin[i] = spawn_origin[i] * 8;

	if (deathmatch->integer && (dmflags->integer & DF_FIXED_FOV))
	{
		client->ps.fov = 90;
	}
	else
	{
		client->ps.fov = atoi(Info_ValueForKey(client->pers.userinfo, "fov"));
		if (client->ps.fov < 1)
			client->ps.fov = 90;
		else if (client->ps.fov > 160)
			client->ps.fov = 160;
	}
	
	client->original_fov  = client->ps.fov; // DWH
	client->ps.gunindex = gi.modelindex(client->pers.weapon->view_model);

	// Server-side speed control stuff
#ifdef KMQUAKE2_ENGINE_MOD
	client->ps.maxspeed = player_max_speed->integer;
	client->ps.duckspeed = player_crouch_speed->integer;
	client->ps.accel = player_accel->integer;
	client->ps.stopspeed = player_stopspeed->integer;
#endif

	// Clear entity state values
	ent->s.effects = 0;
	ent->s.modelindex = MAX_MODELS - 1; // Will use the skin specified model

	if (ITEM_INDEX(client->pers.weapon) == noweapon_index)
		ent->s.modelindex2 = 0;
	else
		ent->s.modelindex2 = MAX_MODELS - 1; // Custom gun model

	// sknum is player num and weapon number.
	// Weapon number will be added in changeweapon.
	ent->s.skinnum = ent - g_edicts - 1;

	ent->s.frame = 0;
	VectorCopy(spawn_origin, ent->s.origin);
	ent->s.origin[2] += 1; // Make sure off ground
	VectorCopy(ent->s.origin, ent->s.old_origin);

	// Set the delta angle
	for (int i = 0; i < 3; i++)
		client->ps.pmove.delta_angles[i] = ANGLE2SHORT(spawn_angles[i] - client->resp.cmd_angles[i]);

	VectorSet(ent->s.angles, 0, spawn_angles[YAW], 0);

	if (spawn_landmark)
		VectorCopy(spawn_viewangles, client->ps.viewangles);
	else
		VectorCopy(ent->s.angles, client->ps.viewangles);

	VectorCopy(client->ps.viewangles, client->v_angle);

	// Spawn a spectator
	if (client->pers.spectator)
	{
		client->chase_target = NULL;
		client->resp.spectator = true;

		ent->movetype = MOVETYPE_NOCLIP;
		ent->solid = SOLID_NOT;
		ent->svflags |= SVF_NOCLIENT;
		ent->client->ps.gunindex = 0;
		gi.linkentity(ent);

		return;
	}
	
	client->resp.spectator = false;

	// DWH:
	client->flashlight = false;
	client->secs_per_frame = 0.025f; // Assumed 40 fps until we know better
	client->fps_time_start = level.time;

	KillBox(ent);

	if (ctf->integer && CTFStartClient(ent)) //ZOID
		return;

	gi.linkentity(ent);

	client->chasetoggle = 0; // tpp

	// tpp. If chasetoggle set then turn on (delayed start of 5 frames - 0.5s)
	if (client->pers.chasetoggle)
		client->delayedstart = 5;

	if (spawn_levelchange && !client->pers.chasetoggle && !client->pers.newweapon)
	{
		client->pers.lastweapon = client->pers.weapon;
		client->newweapon = NULL;
		client->machinegun_shots = 0;

		const int i = ((client->pers.weapon->weapmodel & 0xff) << 8);
		ent->s.skinnum = (ent - g_edicts - 1) | i;

		if (client->pers.weapon->ammo)
			client->ammo_index = ITEM_INDEX(FindItem(client->pers.weapon->ammo));
		else
			client->ammo_index = 0;

		client->weaponstate = WEAPON_READY;
		client->ps.gunframe = 0;
		client->ps.gunindex = gi.modelindex(client->pers.weapon->view_model);
		client->ps.gunframe = spawn_gunframe;
		ent->s.frame = spawn_modelframe;
		client->anim_end = spawn_anim_end;
	}
	else
	{
		// Force the current weapon up
		client->newweapon = client->pers.weapon;
		ChangeWeapon(ent);
	}

	// Paril's fix for this getting reset after map changes
	ent->client->pers.connected = true;
}

// A client has just connected to the server in deathmatch mode, so clear everything out before starting them.
static void ClientBeginDeathmatch(edict_t *ent)
{
	G_InitEdict(ent);
	InitClientResp(ent->client);
	ACEIT_PlayerAdded(ent); // ACEBOT_ADD

	// Locate ent at a spawn point
	PutClientInServer(ent);

	if (level.intermissiontime)
	{
		MoveClientToIntermission(ent);
	}
	else
	{
		// Send effect
		gi.WriteByte(svc_muzzleflash);
		gi.WriteShort(ent - g_edicts);
		gi.WriteByte(MZ_LOGIN);
		gi.multicast(ent->s.origin, MULTICAST_PVS);
	}

	safe_bprintf(PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname);

	// ACEBOT_ADD
	safe_centerprintf(ent, "\n======================================\nACE Bot II Mod\n\n'sv addbot' to add a new bot.\n'sv removebot <name>' to remove bot.\n'sv dmpause' to pause the game.\n'sv savenodes' to save level path data.\n======================================\n\n");

	// Make sure all view stuff is valid.
	ClientEndServerFrame(ent);
}

// Called when a client has finished connecting, and is ready to be placed into the game. This will happen every level load.
void ClientBegin(edict_t *ent)
{
	ent->client = game.clients + (ent - g_edicts - 1);
	
	if (deathmatch->integer)
	{
		ClientBeginDeathmatch(ent);
		return;
	}

	Fog(ent); //mxd. Was Fog_Off(). Fixes no fog rendered for the first server frame after loading a save of a map, which has fog enabled.

	stuffcmd(ent, "alias +zoomin zoomin;alias -zoomin zoominstop\n");
	stuffcmd(ent, "alias +zoomout zoomout;alias -zoomout zoomoutstop\n");
	stuffcmd(ent, "alias +zoom zoomon;alias -zoom zoomoff\n");

	// If there is already a body waiting for us (a loadgame), just take it, otherwise spawn one from scratch.
	if (ent->inuse)
	{
		// The client has cleared the client side viewangles upon connecting to the server, which is different than the
		// state when the game is saved, so we need to compensate with deltaangles.
		for (int i = 0; i < 3; i++)
			ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(ent->client->ps.viewangles[i]);
	}
	else
	{
		// A spawn point will completely reinitialize the entity except for the 
		// persistant data that was initialized at ClientConnect() time.
		G_InitEdict(ent);
		ent->classname = "player";
		InitClientResp(ent->client);
		PutClientInServer(ent);
	}

	if (level.intermissiontime)
	{
		MoveClientToIntermission(ent);
	}
	else
	{
		// Send effect if in a multiplayer game
		if (game.maxclients > 1)
		{
			gi.WriteByte(svc_muzzleflash);
			gi.WriteShort(ent - g_edicts);
			gi.WriteByte(MZ_LOGIN);
			gi.multicast(ent->s.origin, MULTICAST_PVS);

			safe_bprintf(PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname);
		}
	}

	// DWH
	SetLazarusCrosshair(ent); // Backup crosshair
	SetSensitivities(ent, true);

	if (game.maxclients == 1)
	{
		// For SP games, check for monsters who were mad at player in previous level and have changed levels with the player.
		for (int i = 2; i < globals.num_edicts; i++)
		{
			edict_t *monster = &g_edicts[i];

			if (!monster->inuse || monster->health <= 0 || !(monster->svflags & SVF_MONSTER))
				continue;

			if (monster->monsterinfo.aiflags & AI_RESPAWN_FINDPLAYER)
			{
				monster->monsterinfo.aiflags &= ~AI_RESPAWN_FINDPLAYER;
				if (!monster->enemy)
				{
					monster->enemy = ent;
					FoundTarget(monster);
				}
			}
		}
	}

	// Make sure all view stuff is valid
	ClientEndServerFrame(ent);
}

// Called whenever the player updates a userinfo variable.
// The game can override any of the settings in place (forcing skins or names, etc.) before copying it off.
void ClientUserinfoChanged(edict_t *ent, char *userinfo)
{
	// Check for malformed or illegal info strings
	if (!Info_Validate(userinfo))
		Q_strncpyz(userinfo, "\\name\\badinfo\\skin\\male/grunt", MAX_INFO_STRING * sizeof(char)); // userinfo length is always MAX_INFO_STRING

	// Set name
	char *s = Info_ValueForKey(userinfo, "name");
	strncpy(ent->client->pers.netname, s, sizeof(ent->client->pers.netname) - 1);

	// Set spectator
	s = Info_ValueForKey(userinfo, "spectator");
	ent->client->pers.spectator = (deathmatch->integer && *s && strcmp(s, "0")); // Spectators are only supported in deathmatch.

	// Set skin
	s = Info_ValueForKey(userinfo, "skin");

	const int playernum = ent - g_edicts - 1;

	// Combine name and skin into a configstring
	if (ctf->integer) //ZOID
		CTFAssignSkin(ent, s);
	else
		gi.configstring(CS_PLAYERSKINS + playernum, va("%s\\%s", ent->client->pers.netname, s));

	//ZOID. Set player name field (used in id_state view)
	gi.configstring(CS_GENERAL + playernum, ent->client->pers.netname);

	// Fov
	if (deathmatch->integer && (dmflags->integer & DF_FIXED_FOV))
	{
		ent->client->ps.fov = 90;
		ent->client->original_fov = ent->client->ps.fov;
	}
	else
	{
		float new_fov = atoi(Info_ValueForKey(userinfo, "fov"));

		if (new_fov < 1)
			new_fov = 90;
		else if (new_fov > 160)
			new_fov = 160;

		if (new_fov != ent->client->original_fov)
		{
			ent->client->ps.fov = new_fov;
			ent->client->original_fov = new_fov;
		}
	}

	// Handedness
	s = Info_ValueForKey(userinfo, "hand");
	if (strlen(s))
		ent->client->pers.hand = atoi(s);

	// Save off the userinfo in case we want to check something later.
	strncpy(ent->client->pers.userinfo, userinfo, sizeof(ent->client->pers.userinfo) - 1);
}

// Called when a player begins connecting to the server.
// The game can refuse entrance to a client by returning false.
// If the client is allowed, the connection process will continue and eventually get to ClientBegin().
// Changing levels will NOT cause this to be called again, but loadgames will.
qboolean ClientConnect(edict_t *ent, char *userinfo)
{
	// Check to see if they are on the banned IP list
	char *value = Info_ValueForKey(userinfo, "ip");
	if (SV_FilterPacket(value))
	{
		Info_SetValueForKey(userinfo, "rejmsg", "Banned.");
		return false;
	}

	// Check for a spectator
	value = Info_ValueForKey(userinfo, "spectator");
	if (deathmatch->integer && *value && strcmp(value, "0"))
	{
		if (*spectator_password->string && strcmp(spectator_password->string, "none") && strcmp(spectator_password->string, value))
		{
			Info_SetValueForKey(userinfo, "rejmsg", "Spectator password required or incorrect.");
			return false;
		}

		// Count spectators
		int numspec = 0;
		for (int i = 0; i < maxclients->integer; i++)
			if (g_edicts[i + 1].inuse && g_edicts[i + 1].client->pers.spectator)
				numspec++;

		if (numspec >= maxspectators->integer)
		{
			Info_SetValueForKey(userinfo, "rejmsg", "Server spectator limit is full.");
			return false;
		}
	}
	else
	{
		// Check for a password
		value = Info_ValueForKey(userinfo, "password");
		if (*password->string && strcmp(password->string, "none") && strcmp(password->string, value))
		{
			Info_SetValueForKey(userinfo, "rejmsg", "Password required or incorrect.");
			return false;
		}
	}

	// They can connect
	ent->client = game.clients + (ent - g_edicts - 1);

	// If there is already a body waiting for us (a loadgame), just take it, otherwise spawn one from scratch.
	if (!ent->inuse)
	{
		//ZOID -- force team join
		if (ctf->integer)
		{
			ent->client->resp.ctf_team = -1;
			ent->client->resp.id_state = true; 
		}

		// Clear the respawning variables
		InitClientResp(ent->client);
		if (!game.autosaved || !ent->client->pers.weapon)
			InitClientPersistant(ent->client, world->style);
	}

	ClientUserinfoChanged(ent, userinfo);

	if (game.maxclients > 1)
		gi.dprintf("%s connected\n", ent->client->pers.netname);

	ent->svflags = 0; // Make sure we start with known default
	ent->client->pers.connected = true;

	return true;
}

// Called when a player drops from the server. Will not be called between levels.
void ClientDisconnect(edict_t *ent)
{
	if (!ent->client)
		return;

	if (ent->client->chasetoggle) // tpp
		ChasecamRemove(ent, OPTION_OFF);

	// DWH
	SetLazarusCrosshair(ent); // Backup crosshair
	ent->client->zooming = 0;
	ent->client->zoomed = false;
	SetSensitivities(ent, true);

	if (ent->client->textdisplay)
		Text_Close(ent);

	safe_bprintf(PRINT_HIGH, "%s disconnected\n", ent->client->pers.netname);

	ACEIT_PlayerRemoved(ent); // ACEBOT_ADD

	CTFDeadDropFlag(ent); //ZOID
	CTFDeadDropTech(ent); //ZOID

	// Send effect
	gi.WriteByte(svc_muzzleflash);
	gi.WriteShort(ent - g_edicts);
	gi.WriteByte(MZ_LOGOUT);
	gi.multicast(ent->s.origin, MULTICAST_PVS);

	gi.unlinkentity(ent);

	ent->s.modelindex = 0;
	ent->solid = SOLID_NOT;
	ent->inuse = false;
	ent->classname = "disconnected";
	ent->client->pers.connected = false;

	if (ent->client->spycam)
		camera_off(ent);

	const int playernum = ent - g_edicts - 1;
	gi.configstring(CS_PLAYERSKINS + playernum, "");
}

//==============================================================

static edict_t *pm_passent;

// pmove doesn't need to know about passent and contentmask
trace_t PM_trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	const int mask = (pm_passent->health > 0 ? MASK_PLAYERSOLID : MASK_DEADSOLID);
	return gi.trace(start, mins, maxs, end, pm_passent, mask);
}

static uint CheckBlock(void *b, const int c)
{
	int v = 0;
	for (int i = 0; i < c; i++)
		v += ((byte *)b)[i];

	return v;
}

static void RemovePush(edict_t *ent)
{
	ent->client->push->s.sound = 0;
	ent->client->push->activator = NULL;
	ent->client->push = NULL;
	ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;

	// If tpp is NOT always on, and auto-switch for func_pushables IS on, and we're currently in third-person view, switch it off.
	// Knightmare- don't autoswitch if client-side chasecam is on.
#ifdef KMQUAKE2_ENGINE_MOD
	if (!tpp->integer && tpp_auto->integer && (!cl_thirdperson->integer || deathmatch->integer || coop->integer) && ent->client->chasetoggle)
#else
	if (!tpp->integer && tpp_auto->integer && ent->client->chasetoggle)
#endif
		Cmd_Chasecam_Toggle(ent);
}

static void ClientPushPushable(edict_t *ent)
{
	edict_t *box = ent->client->push;

	vec3_t vbox;
	VectorAdd(box->absmax, box->absmin, vbox);
	VectorScale(vbox, 0.5f, vbox);

	if (point_infront(ent, vbox))
	{
		vec3_t new_origin;
		VectorSubtract(ent->s.origin, box->offset, new_origin);

		vec3_t v;
		VectorSubtract(new_origin, box->s.origin, v);
		v[2] = 0;

		const float dist = VectorLength(v);

		if (dist > 8)
		{
			// func_pushable got hung up somehow. Break off contact
			RemovePush(ent);
		}
		else if (dist > 0)
		{
			if (!box->speaker)
				box->s.sound = box->noise_index;

			box_walkmove(box, vectoyaw(v), dist);
		}
		else
		{
			box->s.sound = 0;
		}
	}
	else
	{
		RemovePush(ent);
	}
}

static void ClientSpycam(edict_t *ent)
{
	gclient_t *client = ent->client;
	edict_t *camera = ent->client->spycam;

	if (client->ucmd.sidemove && level.time > ent->last_move_time + 1)
	{
		camera->flags &= ~FL_ROBOT;

		if (camera->viewer == ent)
			camera->viewer = NULL;

		if (client->ucmd.sidemove > 0)
			camera = G_FindNextCamera(camera, client->monitor);
		else
			camera = G_FindPrevCamera(camera, client->monitor);
		
		if (camera)
		{
			if (!camera->viewer)
				camera->viewer = ent;

			client->spycam = camera;
			VectorAdd(camera->s.origin, camera->move_origin, ent->s.origin);

			if (camera->viewmessage)
				safe_centerprintf(ent, camera->viewmessage);

			ent->last_move_time = level.time;
		}
		else
		{
			camera = client->spycam;
		}

		if (camera->monsterinfo.aiflags & AI_ACTOR)
		{
			camera->flags |= FL_ROBOT;

			if (camera->monsterinfo.aiflags & AI_FOLLOW_LEADER)
			{
				camera->monsterinfo.aiflags &= ~AI_FOLLOW_LEADER;
				camera->monsterinfo.old_leader = NULL;
				camera->monsterinfo.leader = NULL;
				camera->movetarget = camera->goalentity = NULL;
				camera->monsterinfo.stand(camera);
			}
		}
	}

	if (camera->enemy && (camera->enemy->deadflag || !camera->enemy->inuse))
		camera->enemy = NULL;

	vec3_t forward, left, up;
	AngleVectors(camera->s.angles, forward, left, up);

	const qboolean is_actor = (camera->svflags & SVF_MONSTER) && (camera->monsterinfo.aiflags & AI_ACTOR);

	if (is_actor && !camera->enemy)
	{
		// Walk/run
		if (abs(client->ucmd.forwardmove) > 199 && camera->groundentity)
		{
			vec3_t end;
			VectorMA(camera->s.origin, WORLD_SIZE, forward, end); // Was 8192
			const trace_t tr = gi.trace(camera->s.origin, camera->mins, camera->maxs, end, camera, MASK_SOLID);

			float dist;

			if (client->ucmd.forwardmove < 0)
			{
				VectorMA(camera->s.origin, -WORLD_SIZE, forward, end); // Was -8192
				const trace_t back = gi.trace(camera->s.origin, camera->mins, camera->maxs, end, camera, MASK_SOLID);
				VectorSubtract(back.endpos, camera->s.origin, end);
				dist = VectorLength(end);
				VectorCopy(tr.endpos, end);
			}
			else
			{
				VectorSubtract(tr.endpos, camera->s.origin, end);
				dist = VectorLength(end) - 8;
				VectorMA(camera->s.origin, dist, forward, end);
			}

			edict_t *thing = camera->vehicle;

			if (dist > 8)
			{
				if (!thing || !thing->inuse || Q_stricmp(thing->classname, "thing"))
					thing = camera->vehicle = SpawnThing();

				thing->touch_debounce_time = level.time + 5.0f;
				thing->target_ent = camera;
				VectorCopy(end, thing->s.origin);
				ED_CallSpawn(thing);

				camera->monsterinfo.aiflags |= AI_CHASE_THING;
				camera->monsterinfo.aiflags &= ~(AI_CHICKEN | AI_STAND_GROUND);
				camera->monsterinfo.pausetime = 0;
				camera->movetarget = camera->goalentity = thing;
				camera->monsterinfo.old_leader = NULL;
				camera->monsterinfo.leader = thing;

				vec3_t dir;
				VectorSubtract(thing->s.origin, camera->s.origin, dir);
				camera->ideal_yaw = vectoyaw(dir);

				if (client->ucmd.forwardmove > 300)
					actor_run(camera);
				else if (client->ucmd.forwardmove > 199)
					actor_walk(camera);
				else if (client->ucmd.forwardmove < -300)
					actor_run_back(camera);
				else
					actor_walk_back(camera);
			}
			else if (thing)
			{
				camera->monsterinfo.aiflags &= ~AI_CHASE_THING;
				camera->movetarget = camera->goalentity = NULL;
				G_FreeEdict(thing);
				camera->vehicle = NULL;
				actor_stand(camera);
			}
		}

		// Stop
		if (client->ucmd.forwardmove == 0 && camera->groundentity && camera->vehicle)
		{
			camera->monsterinfo.aiflags &= ~AI_CHASE_THING;
			camera->movetarget = camera->goalentity = NULL;
			G_FreeEdict(camera->vehicle);
			camera->vehicle = NULL;
			actor_stand(camera);
		}
		
		if (client->ucmd.upmove)
		{
			if (client->ucmd.upmove > 0 && camera->groundentity && !camera->waterlevel)
			{
				// Jump
				if (client->ucmd.forwardmove > 300)
					VectorScale(forward, 400, camera->velocity);
				else if (client->ucmd.forwardmove > 199)
					VectorScale(forward, 200, camera->velocity);
				else if (client->ucmd.forwardmove < -300)
					VectorScale(forward, -400, camera->velocity);
				else if (client->ucmd.forwardmove < -199)
					VectorScale(forward, -200, camera->velocity);

				camera->velocity[2] = 250;
				camera->monsterinfo.savemove = camera->monsterinfo.currentmove;
				actor_jump(camera);
				camera->groundentity = NULL;
			}
			else if (client->ucmd.upmove < 0 && camera->groundentity && !(camera->monsterinfo.aiflags & AI_CROUCH))
			{
				// Crouch
				qboolean docrouch = false; //mxd

				if (camera->monsterinfo.currentmove == &actor_move_walk ||
					camera->monsterinfo.currentmove == &actor_move_run ||
					camera->monsterinfo.currentmove == &actor_move_run_bad)
				{
					camera->monsterinfo.currentmove = &actor_move_crouchwalk;
					docrouch = true;
				}
				else if (camera->monsterinfo.currentmove == &actor_move_walk_back ||
						 camera->monsterinfo.currentmove == &actor_move_run_back)
				{
					camera->monsterinfo.currentmove = &actor_move_crouchwalk_back;
					docrouch = true;
				}
				else if (camera->monsterinfo.currentmove == &actor_move_stand)
				{
					camera->monsterinfo.currentmove = &actor_move_crouch;
					docrouch = true;
				}

				//mxd
				if (docrouch)
				{
					camera->maxs[2] -= 28;
					camera->viewheight -= 28;
					camera->move_origin[2] -= 28;
					camera->monsterinfo.aiflags |= AI_CROUCH;
				}
			}
		}

		// Come out of crouch
		if (client->ucmd.upmove >= 0 && (camera->monsterinfo.aiflags & AI_CROUCH))
		{
			camera->maxs[2] += 28;
			camera->viewheight += 28;
			camera->move_origin[2] += 28;
			camera->monsterinfo.aiflags &= ~AI_CROUCH;

			if (camera->monsterinfo.currentmove == &actor_move_crouchwalk)
				actor_walk(camera);
			else if (camera->monsterinfo.currentmove == &actor_move_crouchwalk_back)
				actor_walk_back(camera);
			else if (camera->monsterinfo.currentmove == &actor_move_crouch)
				actor_stand(camera);
		}
	}

	client->ps.pmove.pm_type = PM_FREEZE;

	if (camera->viewer == ent)
	{
		if (client->old_owner_angles[0] != client->ucmd.angles[0] ||
			client->old_owner_angles[1] != client->ucmd.angles[1])
		{
			// Give game a bit of time to catch up after player. Causes ucmd pitch angle to roll over... 
			// Otherwise we'll hit on the above test even though player hasn't hit +lookup/+lookdown.
			const float delta = level.time - camera->touch_debounce_time;

			if (delta < 0.0f || delta > 1.0f)
			{
				if (is_actor)
				{
					float diff = SHORT2ANGLE(client->ucmd.angles[1] - client->old_owner_angles[1]);
					if (diff < -180)
						diff += 360;
					else if (diff > 180)
						diff -= 360;

					camera->ideal_yaw += diff;

					if (abs(diff) > 100 && camera->vehicle)
					{
						vec3_t angles;
						VectorSet(angles, 0, camera->ideal_yaw, 0);

						vec3_t f;
						AngleVectors(angles, f, NULL, NULL);

						vec3_t end;
						VectorMA(camera->s.origin, WORLD_SIZE, f, end); // Was 8192

						const trace_t tr = gi.trace(camera->s.origin, camera->mins, camera->maxs, end, camera, MASK_SOLID);

						VectorCopy(tr.endpos, camera->vehicle->s.origin);
						camera->vehicle->touch_debounce_time = level.time + 5.0f;
						gi.linkentity(camera->vehicle);
					}

					ai_turn(camera, 0.0f);
					diff = SHORT2ANGLE(client->ucmd.angles[0] - client->old_owner_angles[0]);
					if (diff < -180)
						diff += 360;
					if (diff > 180)
						diff -= 360;

					camera->move_angles[0] += diff;
					client->old_owner_angles[0] = client->ucmd.angles[0];
					client->old_owner_angles[1] = client->ucmd.angles[1];
				}
			}
		}

		if (client->ucmd.buttons & BUTTON_ATTACK && camera->sounds >= 0)
		{
			if (level.time >= camera->monsterinfo.attack_finished)
			{
				client->latched_buttons &= ~BUTTON_ATTACK;

				if (!Q_stricmp(camera->classname, "turret_breach"))
				{
					if (camera->sounds == 5 || camera->sounds == 6)
						camera->monsterinfo.attack_finished = level.time;
					else
						camera->monsterinfo.attack_finished = level.time + 1.0f;

					turret_breach_fire(camera);
				}
				else if (is_actor)
				{
					if (!camera->enemy)
					{
						edict_t *target = LookingAt(ent, 0, NULL, NULL);

						if (target && target->takedamage && target != client->camplayer)
						{
							if (camera->vehicle)
							{
								// Currently following "thing" - turn that off
								camera->monsterinfo.aiflags &= ~AI_CHASE_THING;
								camera->movetarget = camera->goalentity = NULL;
								G_FreeEdict(camera->vehicle);
								camera->vehicle = NULL;
							}

							camera->enemy = target;
							actor_fire(camera);
							camera->enemy = NULL;

							if (camera->monsterinfo.aiflags & AI_HOLD_FRAME)
								camera->monsterinfo.attack_finished = level.time + FRAMETIME;
							else
								camera->monsterinfo.attack_finished = level.time + 1.0f;
						}
					}
				}
			}
		}

		if (client->zoomed)
			camera->touch_debounce_time = max(camera->touch_debounce_time, level.time + 1.0f);
	}

	vec3_t start;
	VectorMA(camera->s.origin, camera->move_origin[0], forward, start);
	VectorMA(start,           -camera->move_origin[1], left,    start);
	VectorMA(start,            camera->move_origin[2], up,      start);

	const trace_t tr = gi.trace(camera->s.origin, NULL, NULL, start, camera, MASK_SOLID);

	if (tr.fraction < 1.0f)
	{
		vec3_t dir;
		VectorSubtract(tr.endpos,camera->s.origin, dir);
		const float dist = VectorNormalize(dir) - 2;
		VectorMA(camera->s.origin, max(0, dist), dir, start);
	}

	VectorCopy(start, ent->s.origin);
	VectorCopy(camera->velocity, ent->velocity);
	
	for (int i = 0; i < 3; i++)
		client->resp.cmd_angles[i] = SHORT2ANGLE(client->ucmd.angles[i]);
	
	pmove_t pm;
	memset(&pm, 0, sizeof(pm));
	pm.s = client->ps.pmove;

	for (int i = 0; i < 3; i++)
	{
		pm.s.origin[i] = ent->s.origin[i] * 8;
		client->ps.pmove.delta_angles[i] = ANGLE2SHORT(client->ps.viewangles[i] - client->resp.cmd_angles[i]);
	}

	if (memcmp(&client->old_pmove, &pm.s, sizeof(pm.s)))
		pm.snapinitial = true;

	pm.cmd = client->ucmd;
	pm.trace = PM_trace; // Adds default parms
	pm.pointcontents = gi.pointcontents;
	
	gi.Pmove(&pm);
	gi.linkentity(ent);
	
	G_TouchTriggers(ent); // We'll only allow touching trigger_look with "Cam Owner" SF
}

// This will be called once for each client frame, which will usually be a couple times for each server frame.
void ClientThink(edict_t *ent, usercmd_t *ucmd)
{
	// Knightmare- dm pause
	if (paused && deathmatch->integer)
	{
		safe_centerprintf(ent, "PAUSED\n\n(type \"sv dmpause\" to resume)");
		ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

		return;
	}

	level.current_entity = ent;
	gclient_t *client = ent->client;

	// Lazarus: Copy latest usercmd stuff for use in other routines
	client->ucmd = *ucmd;

	vec3_t oldorigin, oldvelocity;
	VectorCopy(ent->s.origin, oldorigin);
	VectorCopy(ent->velocity, oldvelocity);

	edict_t *ground = ent->groundentity;

	float ground_speed;
	if (ground && (ground->movetype == MOVETYPE_PUSH) && ground != world && ground->turn_rider)
		ground_speed = VectorLength(ground->velocity);
	else
		ground_speed = 0;

	if (ent->in_mud || ent->client->push || ent->vehicle || ent->client->chasetoggle || ent->turret || ent->client->spycam || ground_speed > 0)
		ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
	else
		ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;

	// Server-side speed control stuff
#ifdef KMQUAKE2_ENGINE_MOD
	client->ps.maxspeed = player_max_speed->value;
	client->ps.duckspeed = player_crouch_speed->value;
	client->ps.accel = player_accel->value;
	client->ps.stopspeed = player_stopspeed->value;
#endif

	if (client->startframe == 0)
		client->startframe = level.framenum;

	client->fps_frames++;
	if (client->fps_frames >= 100)
	{
		client->secs_per_frame = (level.time - client->fps_time_start) / 100;
		client->fps_frames = 0;
		client->fps_time_start = level.time;
		client->frame_zoomrate = zoomrate->value * client->secs_per_frame;
	}

	Fog(ent);

	// MUD - get mud level
	if (level.mud_puddles)
	{
		ent->in_mud = 0;
		for (int i = game.maxclients + 1; i < globals.num_edicts && !ent->in_mud; i++)
		{
			edict_t *mud = &g_edicts[i];

			if (!mud->inuse || !(mud->svflags & SVF_MUD))
				continue;

			// Check if bboxes intersect. //TODO: mxd. There sould be a fuction for this?
			if (ent->absmin[0] > mud->absmax[0]) continue;
			if (ent->absmin[1] > mud->absmax[1]) continue;
			if (ent->absmin[2] > mud->absmax[2]) continue;
			if (ent->absmax[0] < mud->absmin[0]) continue;
			if (ent->absmax[1] < mud->absmin[1]) continue;
			if (ent->absmax[2] < mud->absmin[2]) continue;
			
			if (ent->s.origin[2] + ent->viewheight < mud->absmax[2])
				ent->in_mud = 3;
			else if (ent->s.origin[2] < mud->absmax[2])
				ent->in_mud = 2;
			else
				ent->in_mud = 1;
		}
	}

	// USE - special actions taken when +use is pressed
	if (!client->use && (ucmd->buttons & BUTTON_USE))
	{
		// Use key was NOT pressed, but now is.
		client->use = 1;
		if (client->spycam)
		{
			camera_off(ent);
		}
		else
		{
			vec3_t intersect;
			float range;
			edict_t *viewing = LookingAt(ent, 0, intersect, &range);

			if (viewing && viewing->classname)
			{
				if (!Q_stricmp(viewing->classname, "crane_control") && range <= 100)
				{
					crane_control_action(viewing, ent, intersect);
				}
				else if (!Q_stricmp(viewing->classname, "target_lock_digit") && range <= 100)
				{
					lock_digit_increment(viewing, ent);
				}
				else if (!Q_stricmp(viewing->classname, "func_trainbutton") && (viewing->spawnflags & 1) && range <= 64)
				{
					trainbutton_use(viewing, ent, ent);
				}
				// Knightmare- different range for chasecam
				else if (!Q_stricmp(viewing->classname, "func_monitor") && (range <= 100 || (client->chasetoggle && range <= client->zoom + 160.0f)))
				{
					use_camera(viewing, ent, ent);

					if (client->spycam && client->spycam->viewer == ent)
					{
						client->old_owner_angles[0] = ucmd->angles[0];
						client->old_owner_angles[1] = ucmd->angles[1];
					}
				}

				if (viewing->monsterinfo.aiflags & AI_ACTOR)
				{
					if (viewing->monsterinfo.aiflags & AI_FOLLOW_LEADER)
					{
						viewing->monsterinfo.aiflags &= ~AI_FOLLOW_LEADER;
						viewing->monsterinfo.old_leader = NULL;
						viewing->monsterinfo.leader = NULL;
						viewing->movetarget = NULL;
						viewing->goalentity = NULL;
						viewing->monsterinfo.stand(viewing);
					}
					else
					{
						viewing->monsterinfo.aiflags |= AI_FOLLOW_LEADER;
						viewing->monsterinfo.leader = ent;

						vec3_t dir;
						VectorSubtract(ent->s.origin, viewing->s.origin, dir);
						viewing->ideal_yaw = vectoyaw(dir);

						if (fabsf(viewing->s.angles[YAW] - viewing->ideal_yaw) < 90)
							actor_salute(viewing);
					}
				}
			}
		}
	}

	client->use = (ucmd->buttons & BUTTON_USE);

	if (client->push)
	{
		// Currently pushing or pulling a func_pushable
		if (!client->use)
		{
			RemovePush(ent); // USE key was released.
		}
		else if (!ent->groundentity && (ent->waterlevel == 0 || client->push->waterlevel == 0))
		{
			RemovePush(ent); // Player is falling down
		}
		else
		{
			// Scale client velocity by mass of func_pushable
			const float vellen = VectorLength(ent->velocity);
			if (vellen > client->maxvelocity)
				VectorScale(ent->velocity, client->maxvelocity / vellen, ent->velocity);

			client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

			const float massscaler = 200.0f / client->push->mass;
			ucmd->forwardmove *= massscaler;
			ucmd->sidemove *= massscaler;
		}
	}

	if (ent->turret && ucmd->upmove > 10)
		turret_disengage(ent->turret);

	// INTERMISSION
	if (level.intermissiontime)
	{
		// tpp
		if (client->chasetoggle)
			ChasecamRemove(ent, OPTION_OFF);

		// Lazarus spycam
		if (client->spycam)
			camera_off(ent);

		client->ps.pmove.pm_type = PM_FREEZE;

		// Can exit intermission after five seconds.
		if (level.time > level.intermissiontime + 5.0f && (ucmd->buttons & BUTTON_ANY))
			level.exitintermission = true;

		return;
	}

	if (ent->target_ent && !Q_stricmp(ent->target_ent->classname, "target_monitor"))
	{
		edict_t	*monitor = ent->target_ent;
		if (monitor->target_ent && monitor->target_ent->inuse)
		{
			if (monitor->spawnflags & 2)
			{
				VectorCopy(monitor->target_ent->s.angles, client->ps.viewangles);
			}
			else
			{
				vec3_t dir;
				VectorSubtract(monitor->target_ent->s.origin, monitor->s.origin, dir);
				vectoangles(dir, client->ps.viewangles);
			}
		}
		else
		{
			VectorCopy(monitor->s.angles, client->ps.viewangles);
		}

		VectorCopy(monitor->s.origin, ent->s.origin);
		client->ps.pmove.pm_type = PM_FREEZE;

		return;
	}

	// THIRDPERSON VIEW in/out
	// If NOT pushing something AND in third person AND use key is pressed, move viewpoint in/out.
	if (client->chasetoggle && !client->push)
	{
		vec3_t intersect;
		float range;
		edict_t *viewing = LookingAt(ent, 0, intersect, &range);

		if (!(viewing && range <= 100 && viewing->classname && (Q_stricmp(viewing->classname, "func_monitor") || Q_stricmp(viewing->classname, "func_pushable"))))
		{
			if ((ucmd->buttons & BUTTON_USE) && !deathmatch->integer)
			{
				client->use = 1;

				if (ucmd->forwardmove < 0 && client->zoom < 100)
					client->zoom++;
				else if (ucmd->forwardmove > 0 && client->zoom > -40)
					client->zoom--;

				ucmd->forwardmove = 0;
				ucmd->sidemove = 0;
			}
			else if (client->use)
			{
				if (client->oldplayer)
				{
					// Set angles
					for (int i = 0; i < 3; i++)
						ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(ent->client->oldplayer->s.angles[i] - ent->client->resp.cmd_angles[i]);
				}

				client->use = 0;
			}
		}
	}

	// ZOOM
	if (client->zooming)
	{
		client->pers.hand = 2;

		if (client->zooming > 0)
		{
			if (client->ps.fov > 5)
				client->ps.fov = max(5, client->ps.fov - client->frame_zoomrate);
			else
				client->ps.fov = 5;

			client->zoomed = true;
		}
		else
		{
			if (client->ps.fov < client->original_fov)
			{
				client->ps.fov += client->frame_zoomrate;

				if (client->ps.fov > client->original_fov)
				{
					client->ps.fov = client->original_fov;
					client->zoomed = false;
				}
				else
				{
					client->zoomed = true;
				}
			}
			else
			{
				client->ps.fov = client->original_fov;
				client->zoomed = false;
			}
		}
	}

	// SPYCAM
	if (client->spycam)
	{
		ClientSpycam(ent);

		// No movement while in spycam mode.
		return;
	}

	pm_passent = ent;

	// Lazarus: developer item movement
	if (client->use && client->shift_dir)
		ShiftItem(ent, client->shift_dir);

	if (client->chase_target)
	{
		for (int i = 0; i < 3; i++)
			client->resp.cmd_angles[i] = SHORT2ANGLE(ucmd->angles[i]);
	}
	else
	{
		// Set up for pmove
		pmove_t pm;
		memset(&pm, 0, sizeof(pm));

		if (ent->movetype == MOVETYPE_NOCLIP)
			client->ps.pmove.pm_type = PM_SPECTATOR;
		else if (ent->s.modelindex != MAX_MODELS - 1)
			client->ps.pmove.pm_type = PM_GIB;
		else if (ent->deadflag)
			client->ps.pmove.pm_type = PM_DEAD;
		else
			client->ps.pmove.pm_type = PM_NORMAL;

		if (level.time > ent->gravity_debounce_time)
			client->ps.pmove.gravity = sv_gravity->value;
		else
			client->ps.pmove.gravity = 0;

#ifdef JETPACK_MOD
		if (client->jetpack)
		{
			qboolean jetpack_thrusting = false; //mxd

			if (ucmd->upmove != 0 || ucmd->forwardmove != 0 || ucmd->sidemove != 0)
			{
				if (ucmd->upmove > 0 || !ent->groundentity)
				{
					if (!client->jetpack_thrusting)
					{
						gi.sound(ent, CHAN_AUTO, gi.soundindex("jetpack/rev.wav"), 1, ATTN_NORM, 0);
						client->jetpack_start_thrust = level.framenum;
					}

					jetpack_thrusting = true;
				}
			}

			client->jetpack_thrusting = jetpack_thrusting;

			if (client->jetpack_framenum + client->pers.inventory[fuel_index] > level.framenum)
			{
				if (jetpack_weenie->value)
				{
					Jet_ApplyJet(ent, ucmd);
					if (client->jetpack_framenum < level.framenum)
					{
						if (!client->jetpack_infinite)
							client->pers.inventory[fuel_index] -= 10;

						client->jetpack_framenum = level.framenum + 10;
					}
				}
				else
				{
					if (client->jetpack_thrusting)
						Jet_ApplyJet(ent, ucmd);

					if (client->jetpack_framenum <= level.framenum)
					{
						if (client->jetpack_thrusting)
						{
							if (!client->jetpack_infinite)
								client->pers.inventory[fuel_index] -= 11;

							client->jetpack_framenum = level.framenum + 10;
						}
						else
						{
							if (!client->jetpack_infinite)
								client->pers.inventory[fuel_index]--;

							client->jetpack_framenum = level.framenum + 10;
						}
					}

					if (ucmd->upmove == 0)
					{
						// Accelerate to 75% gravity in 2 seconds
						const float g_max = 0.75f * sv_gravity->value;
						const float gravity = g_max * (level.framenum - client->jetpack_last_thrust) / 20;

						client->ps.pmove.gravity = (short)min(g_max, gravity);
					}
					else
					{
						client->jetpack_last_thrust = level.framenum;
					}
				}
			}
			else
			{
				client->jetpack = false;
				ent->s.frame = FRAME_jump2;	// Reset from stand to avoid goofiness
			}
		}
#endif // JETPACK_MOD

		pm.s = client->ps.pmove;

		for (int i = 0; i < 3; i++)
		{
			pm.s.origin[i] = ent->s.origin[i] * 8;
			pm.s.velocity[i] = ent->velocity[i] * 8;
		}

		if (memcmp(&client->old_pmove, &pm.s, sizeof(pm.s)))
			pm.snapinitial = true;

		pm.cmd = *ucmd;

		pm.trace = PM_trace; // Adds default parms
		pm.pointcontents = gi.pointcontents;

		if (ent->vehicle)
			pm.s.pm_flags |= PMF_ON_GROUND;

		// Perform a pmove
		gi.Pmove(&pm);

		// Save results of pmove
		client->ps.pmove = pm.s;
		client->old_pmove = pm.s;

		for (int i = 0; i < 3; i++)
		{
			ent->s.origin[i] = pm.s.origin[i] * 0.125f;
			ent->velocity[i] = pm.s.velocity[i] * 0.125f;

			client->resp.cmd_angles[i] = SHORT2ANGLE(ucmd->angles[i]);
		}

		VectorCopy(pm.mins, ent->mins);
		VectorCopy(pm.maxs, ent->maxs);

#ifdef JETPACK_MOD
		if (client->jetpack && jetpack_weenie->value && pm.groundentity && Jet_AvoidGround(ent)) // have jetpack && jetpack_weenie (???) && on ground && liftoff succeeded
			pm.groundentity = NULL; // Now we are no longer on ground
#endif

		// MUD - "correct" Pmove physics
		if (pm.waterlevel && ent->in_mud)
		{
			pm.watertype |= CONTENTS_MUD;
			ent->in_mud = pm.waterlevel;

			vec3_t deltapos, deltavel;
			VectorSubtract(ent->s.origin, oldorigin, deltapos);
			VectorSubtract(ent->velocity, oldvelocity, deltavel);

			if (pm.waterlevel == 1)
			{
				const float frac = MUD1BASE + MUD1AMP * sinf((level.framenum % 10) / 10.0f * M_PI2);

				ent->s.origin[0] = oldorigin[0] + frac * deltapos[0];
				ent->s.origin[1] = oldorigin[1] + frac * deltapos[1];
				ent->s.origin[2] = oldorigin[2] + 0.75f * deltapos[2];

				ent->velocity[0] = oldvelocity[0] + frac * deltavel[0];
				ent->velocity[1] = oldvelocity[1] + frac * deltavel[1];
				ent->velocity[2] = oldvelocity[2] + 0.75f * deltavel[2];
			}
			else if (pm.waterlevel == 2)
			{
				vec3_t point;
				VectorCopy(oldorigin, point);
				point[2] += ent->maxs[2];

				vec3_t end = { point[0], point[1], oldorigin[2] + ent->mins[2] };
				const trace_t tr = gi.trace(point, NULL, NULL, end, ent, CONTENTS_WATER);
				const float dist = point[2] - tr.endpos[2];

				// frac = waterlevel 1 frac at dist=32 or more,
				//      = waterlevel 3 frac at dist=10 or less
				float frac;
				if (dist <= 10)
					frac = MUD3;
				else
					frac = MUD3 + (dist - 10) / 22.0f * (MUD1BASE - MUD3);

				for (int i = 0; i < 3; i++)
				{
					ent->s.origin[i] = oldorigin[i] + frac * deltapos[i];
					ent->velocity[i] = oldvelocity[i] + frac * deltavel[i];
				}

				if (!ent->groundentity)
				{
					// Player can't possibly move up
					ent->s.origin[2] = min(oldorigin[2], ent->s.origin[2]);
					ent->velocity[2] = min(oldvelocity[2], ent->velocity[2]);
					ent->velocity[2] = min(-10, ent->velocity[2]);
				}
			}
			else
			{
				ent->s.origin[0] = oldorigin[0] + MUD3 * deltapos[0];
				ent->s.origin[1] = oldorigin[1] + MUD3 * deltapos[1];

				ent->velocity[0] = oldvelocity[0] + MUD3 * deltavel[0];
				ent->velocity[1] = oldvelocity[1] + MUD3 * deltavel[1];

				if (ent->groundentity)
				{
					ent->s.origin[2] = oldorigin[2]   + MUD3 * deltapos[2];
					ent->velocity[2] = oldvelocity[2] + MUD3 * deltavel[2];
				}
				else
				{
					ent->s.origin[2] = min(oldorigin[2], ent->s.origin[2]);
					ent->velocity[2] = min(oldvelocity[2], 0);
				}
			}
		}
		else
		{
			ent->in_mud = 0;
		}

		// Player has just jumped.
		if (ent->groundentity && !pm.groundentity && pm.cmd.upmove >= 10 && pm.waterlevel == 0 && !client->jetpack)
		{
			// Knightmare- allow disabling of STUPID grunting when jumping
			if ((deathmatch->integer || player_jump_sounds->integer) && !ent->vehicle)
			{
				gi.sound(ent, CHAN_VOICE, gi.soundindex("*jump1.wav"), 1, ATTN_NORM, 0);
				PlayerNoise(ent, ent->s.origin, PNOISE_SELF);
			}

			// Paril's vehicle targeting
			if (ent->vehicle)
				G_UseTargets(ent->vehicle, ent);

			// Lazarus: temporarily match velocities with entity we just jumped from.
			VectorAdd(ent->groundentity->velocity, ent->velocity, ent->velocity);
		}

		if (ent->groundentity && !pm.groundentity && pm.cmd.upmove >= 10 && pm.waterlevel == 0)
			ent->client->jumping = 1;

		if (ent->deadflag != DEAD_FROZEN)
			ent->viewheight = pm.viewheight;

		ent->waterlevel = pm.waterlevel;
		ent->watertype = pm.watertype;
		ent->groundentity = pm.groundentity;

		if (pm.groundentity)
			ent->groundentity_linkcount = pm.groundentity->linkcount;

		// Lazarus - lie about ground when driving a vehicle. Pmove apparently doesn't think the ground can be "owned".
		if (ent->vehicle && !ent->groundentity)
		{
			ent->groundentity = ent->vehicle;
			ent->groundentity_linkcount = ent->vehicle->linkcount;
		}

		// Apply view angles
		if (ent->deadflag)
		{
			if (ent->deadflag != DEAD_FROZEN)
			{
				client->ps.viewangles[ROLL] = 40;
				client->ps.viewangles[PITCH] = -15;
				client->ps.viewangles[YAW] = client->killer_yaw;
			}
		}
		else
		{
			VectorCopy(pm.viewangles, client->v_angle);
			VectorCopy(pm.viewangles, client->ps.viewangles);
		}

#ifdef JETPACK_MOD
		if (client->jetpack && !(ucmd->buttons & BUTTONS_ATTACK))
			ent->s.frame = FRAME_stand20;
#endif

		//ZOID
		if (client->ctf_grapple)
			CTFGrapplePull(client->ctf_grapple);

		gi.linkentity(ent);

		if (ent->movetype != MOVETYPE_NOCLIP)
			G_TouchTriggers(ent);

		// Perform kick attack?
		if ((world->effects & FX_WORLDSPAWN_JUMPKICK) && ent->client->jumping && ent->solid != SOLID_NOT)
			kick_attack(ent);

		// Touch other objects
		if (!level.freeze) // Lazarus: but NOT if game is frozen
		{
			for (int i = 0; i < pm.numtouch; i++)
			{
				edict_t *other = pm.touchents[i];

				int j;
				for (j = 0; j < i; j++)
					if (pm.touchents[j] == other)
						break;

				if (j == i && other->touch) // Duplicated when j != i
					other->touch(other, ent, NULL, NULL);
			}
		}
	}

	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;
	client->latched_buttons |= client->buttons & ~client->oldbuttons;

	// Save light level the player is standing on for monster sighting AI
	ent->light_level = ucmd->lightlevel;

	// CDawg
	ent->client->backpedaling = (ucmd->forwardmove < -1);

	// Fire weapon from final position if needed
	if (client->latched_buttons & BUTTONS_ATTACK && !(ctf->integer && ent->movetype == MOVETYPE_NOCLIP))
	{
		if (client->resp.spectator)
		{
			client->latched_buttons = 0;

			if (client->chase_target)
			{
				client->chase_target = NULL;
				client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
			}
			else
			{
				GetChaseTarget(ent);
			}
		}
		else if (!client->weapon_thunk)
		{
			client->weapon_thunk = true;
			Think_Weapon(ent);
		}
	}

	// ACEBOT_ADD
	if (!ent->is_bot && !ent->deadflag && !ent->client->resp.spectator)
		ACEND_PathMap(ent);

	if (client->resp.spectator)
	{
		if (ucmd->upmove >= 10)
		{
			if (!(client->ps.pmove.pm_flags & PMF_JUMP_HELD))
			{
				client->ps.pmove.pm_flags |= PMF_JUMP_HELD;

				if (client->chase_target)
					ChaseNext(ent);
				else
					GetChaseTarget(ent);
			}
		}
		else
		{
			client->ps.pmove.pm_flags &= ~PMF_JUMP_HELD;
		}
	}

	//ZOID. Regen tech
	CTFApplyRegeneration(ent);

	// Update chase cam if being followed
	for (int i = 1; i <= maxclients->integer; i++)
	{
		edict_t *other = g_edicts + i;
		if (other->inuse && other->client->chase_target == ent)
			UpdateChaseCam(other);
	}

	// Push the pushable?
	if (client->push != NULL)
	{
		if (client->use && (ucmd->forwardmove != 0 || ucmd->sidemove != 0))
			ClientPushPushable(ent);
		else
			client->push->s.sound = 0;
	}
}

// This will be called once for each server frame, before running any other entities in the world.
void ClientBeginServerFrame(edict_t *ent)
{
	if (level.intermissiontime)
		return;

	gclient_t *client = ent->client;

	// DWH
	if (client->spycam)
		client = client->camplayer->client;

	// tpp
	if (client->delayedstart > 0)
		client->delayedstart--;

	if (client->delayedstart == 1)
		ChasecamStart(ent);

	if (deathmatch->integer && client->pers.spectator != client->resp.spectator && level.time - client->respawn_time >= 5)
	{
		SpectatorRespawn(ent);
		return;
	}

	// Run weapon animations if it hasn't been done by a ucmd_t.
	if (!client->weapon_thunk && !client->resp.spectator && !(ctf->integer && ent->movetype == MOVETYPE_NOCLIP))
		Think_Weapon(ent);
	else
		client->weapon_thunk = false;

	if (ent->deadflag)
	{
		// Wait for any button just going down
		if (level.time > client->respawn_time)
		{
			// tpp
			if (ent->crosshair)
				G_FreeEdict(ent->crosshair);
			ent->crosshair = NULL;

			if (ent->client->oldplayer)
				G_FreeEdict(ent->client->oldplayer);
			ent->client->oldplayer = NULL;

			if (ent->client->chasecam)
				G_FreeEdict(ent->client->chasecam);
			ent->client->chasecam = NULL;

			// In deathmatch, only wait for attack button
			const int buttonmask = (deathmatch->integer ? BUTTONS_ATTACK : -1);
			if ((client->latched_buttons & buttonmask) || (deathmatch->integer && (dmflags->integer & DF_FORCE_RESPAWN)))
			{
				respawn(ent);
				client->latched_buttons = 0;
			}
		}

		return;
	}

	// Add player trail so monsters can follow. DWH: Don't add player trail for players in camera.
	if (!deathmatch->integer && !client->spycam && !visible(ent, PlayerTrail_LastSpot()))
		PlayerTrail_Add(ent->s.old_origin);

	client->latched_buttons = 0;
}