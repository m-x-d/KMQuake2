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

#pragma once

struct sfx_s;

void S_Init(void);
void S_Shutdown(void);

// If origin is NULL, the sound will be dynamically sourced from the entity
void S_StartSound(const vec3_t origin, int entnum, int entchannel, struct sfx_s *sfx, float fvol,  float attenuation, float timeofs);
void S_StartLocalSound(char *s);

void S_RawSamples(const int samples, const int rate, const int width, const int channels, const byte *data, const float volume); //mxd. -music, +volume

void S_StopAllSounds(void);
void S_Update(const vec3_t origin, const vec3_t forward, const vec3_t right, const vec3_t up); //mxd. Only origin and up are actually used

void S_BeginRegistration(void);
struct sfx_s *S_RegisterSound(char *name);
void S_EndRegistration(void);

struct sfx_s *S_FindName(char *name, qboolean create);

// The sound code makes callbacks to the client for entitiy position information, so entities can be dynamically re-spatialized.
void CL_GetEntitySoundOrigin(const int entindex, vec3_t org);