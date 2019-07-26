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
// snd_ogg.h -- Ogg Vorbis streaming functions

#pragma once

void S_UpdateBackgroundTrack();
void S_StartBackgroundTrack(const char *introTrack, const char *loopTrack, int startframe);
void S_StopBackgroundTrack();
void S_StartStreaming();
void S_StopStreaming();
int S_GetBackgroundTrackFrame(); //mxd
void S_OGG_Init();
void S_OGG_Shutdown();
void S_OGG_Restart();
void S_OGG_LoadFileList();
void S_OGG_ParseCmd();