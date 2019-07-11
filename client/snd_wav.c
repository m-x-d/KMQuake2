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

// snd_wav.c: Wave sound loading

#include "client.h"
#include "snd_loc.h"

#pragma region ======================= WAV loading

static byte *data_p;
static byte *iff_end;
static byte *last_chunk;
static byte *iff_data;
static int iff_chunk_len;

static short GetLittleShort(void)
{
	short val = *data_p;
	val += *(data_p + 1) << 8;
	data_p += 2;

	return val;
}

static int GetLittleLong(void)
{
	int val = *data_p;
	val += *(data_p + 1) << 8;
	val += *(data_p + 2) << 16;
	val += *(data_p + 3) << 24;
	data_p += 4;

	return val;
}

static void FindNextChunk(char *name)
{
	while (true)
	{
		data_p = last_chunk;

		if (data_p >= iff_end)
		{
			// Didn't find the chunk
			data_p = NULL;
			return;
		}
		
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0)
		{
			data_p = NULL;
			return;
		}

		data_p -= 8;
		last_chunk = data_p + 8 + ((iff_chunk_len + 1) & ~1);

		if (!strncmp((char *)data_p, name, 4))
			return;
	}
}

static void FindChunk(char *name)
{
	last_chunk = iff_data;
	FindNextChunk(name);
}

wavinfo_t S_GetWavInfo(char *name, byte *wav, int wavlength)
{
	wavinfo_t info;
	memset(&info, 0, sizeof(info));

	if (!wav)
		return info;
		
	iff_data = wav;
	iff_end = wav + wavlength;

	// Find "RIFF" chunk
	FindChunk("RIFF");
	if (!(data_p && !strncmp((char *)data_p + 8, "WAVE", 4)))
	{
		Com_Printf(S_COLOR_YELLOW"%s: sound '%s' is missing RIFF/WAVE chunks!\n", __func__, name);
		return info;
	}

	// Get "fmt " chunk
	iff_data = data_p + 12;

	FindChunk("fmt ");
	if (!data_p)
	{
		Com_Printf(S_COLOR_YELLOW"%s: sound '%s' is missing fmt chunk!\n", __func__, name);
		return info;
	}

	data_p += 8;
	const int format = GetLittleShort();
	if (format != 1)
	{
		Com_Printf(S_COLOR_YELLOW"%s: sound '%s' uses unsupported format (%i). Only Microsoft PCM format is supported.\n", __func__, name, format);
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 6;
	info.width = GetLittleShort() / 8;

	// Get cue chunk
	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info.loopstart = GetLittleLong();

		// If the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk("LIST");
		if (data_p)
		{
			if (!strncmp((char *)data_p + 28, "mark", 4))
			{
				// This is not a proper parse, but it works with cooledit...
				data_p += 24;
				const int i = GetLittleLong(); // Samples in loop
				info.samples = info.loopstart + i;
			}
		}
	}
	else
	{
		info.loopstart = -1;
	}

	// Find data chunk
	FindChunk("data");
	if (!data_p)
	{
		Com_Printf(S_COLOR_YELLOW"%s: sound '%s' is missing 'data' chunk!\n", __func__, name);
		return info;
	}

	data_p += 4;
	const int samples = GetLittleLong() / info.width;

	if (info.samples)
	{
		if (samples < info.samples)
			Com_Error(ERR_DROP, "%s: sound '%s' has bad loop length!", __func__, name);
	}
	else
	{
		info.samples = samples;
	}

	info.dataofs = data_p - wav;
	
	return info;
}

#pragma endregion