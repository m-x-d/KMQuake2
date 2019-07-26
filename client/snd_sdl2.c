/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2010, 2013 Yamagi Burmeister
 * Copyright (C) 2005 Ryan C. Gordon
 * Copyright (C) 2019 MaxED
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * =======================================================================
 *
 * SDL sound backend. Since SDL is just an API for sound playback, we
 * must caculate everything in software: mixing, resampling, stereo
 * spartializations, etc. Therefore this file is rather complex. :)
 * Samples are read from the cache (see the upper layer of the sound
 * system), manipulated and written into sound.buffer. sound.buffer is
 * passed to SDL (in fact requested by SDL via the callback) and played
 * with a platform dependend SDL driver. Parts of this file are based
 * on ioQuake3s snd_sdl.c.
 *
 * =======================================================================
 */

#include <SDL2/SDL.h>
#include "client.h"
#include "snd_loc.h"
#include "snd_ogg.h"

 // Defines
#define SDL_PAINTBUFFER_SIZE	2048
#define SDL_FULLVOLUME			80
#define SDL_LOOPATTENUATE		0.003f

//mxd. Minimum and maximum possible values of a signed short
#define SHORT_MIN	-32768
#define SHORT_MAX	32767

 // Global vars
static cvar_t *s_sdldriver;
static sound_t *backend;
static portable_samplepair_t paintbuffer[SDL_PAINTBUFFER_SIZE];
static int playpos = 0;
static int samplesize = 0;
static qboolean snd_inited = false;
static int snd_scaletable[32][256];
static int soundtime;

#pragma region ======================= Channel "painting"

// Transfers a mixed "paint buffer" to the SDL output buffer and places it at the appropriate position.
static void SDL_TransferPaintBuffer(const int endtime)
{
	byte *pbuf = sound.buffer;
	int *snd_p = (int *)paintbuffer;

	if (s_testsound->integer)
	{
		// Write a fixed sine wave
		const int delta = (endtime - paintedtime);

		for (int i = 0; i < delta; i++)
		{
			paintbuffer[i].left = (int)(sinf((paintedtime + i) * 0.1f) * 20000 * 256);
			paintbuffer[i].right = paintbuffer[i].left;
		}
	}

	if (sound.samplebits == 16 && sound.channels == 2)
	{
		// Optimized case
		int ls_paintedtime = paintedtime;

		while (ls_paintedtime < endtime)
		{
			// Handle recirculating buffer issues
			const int lpos = ls_paintedtime & ((sound.samples >> 1) - 1);
			short *snd_out = (short *)pbuf + (lpos << 1);
			int snd_linear_count = (sound.samples >> 1) - lpos;

			if (ls_paintedtime + snd_linear_count > endtime)
				snd_linear_count = endtime - ls_paintedtime;

			snd_linear_count <<= 1;

			for (int i = 0; i < snd_linear_count; i += 2)
			{
				int val = snd_p[i] >> 8;
				snd_out[i] = clamp(val, SHORT_MIN, SHORT_MAX); //mxd

				val = snd_p[i + 1] >> 8;
				snd_out[i + 1] = clamp(val, SHORT_MIN, SHORT_MAX); //mxd
			}

			snd_p += snd_linear_count;
			ls_paintedtime += (snd_linear_count >> 1);
		}
	}
	else
	{
		int count = (endtime - paintedtime) * sound.channels;
		const int out_mask = sound.samples - 1;
		int out_idx = paintedtime * sound.channels & out_mask;
		const int step = 3 - sound.channels;

		if (sound.samplebits == 16)
		{
			short *out = (short *)pbuf;

			while (count--)
			{
				const int val = *snd_p >> 8;
				out[out_idx] = clamp(val, SHORT_MIN, SHORT_MAX); //mxd
				out_idx = (out_idx + 1) & out_mask;
				snd_p += step;
			}
		}
		else if (sound.samplebits == 8)
		{
			byte *out = pbuf;

			while (count--)
			{
				int val = *snd_p >> 8;
				val = clamp(val, SHORT_MIN, SHORT_MAX); //mxd
				out[out_idx] = (val >> 8) + 128;
				out_idx = (out_idx + 1) & out_mask;
				snd_p += step;
			}
		}
	}
}

// Mixes an 8 bit sample into a channel.
static void SDL_PaintChannelFrom8(channel_t *ch, sfxcache_t *sc, const int count, const int offset)
{
	ch->leftvol = min(255, ch->leftvol);
	ch->rightvol = min(255, ch->rightvol);

	//ZOID-- >>11 has been changed to >>3, >>11 didn't make much sense as it would always be zero.
	int *lscale = snd_scaletable[ch->leftvol >> 3];
	int *rscale = snd_scaletable[ch->rightvol >> 3];
	byte *sfx = sc->data + ch->pos;

	portable_samplepair_t *samp = &paintbuffer[offset];

	for (int i = 0; i < count; i++, samp++)
	{
		const int data = sfx[i];
		samp->left += lscale[data];
		samp->right += rscale[data];
	}

	ch->pos += count;
}

// Mixes an 16 bit sample into a channel
static void SDL_PaintChannelFrom16(channel_t *ch, sfxcache_t *sc, const int count, const int offset, const int snd_vol)
{
	const int leftvol = ch->leftvol * snd_vol;
	const int rightvol = ch->rightvol * snd_vol;
	short *sfx = (short *)sc->data + ch->pos;

	portable_samplepair_t *samp = &paintbuffer[offset];

	for (int i = 0; i < count; i++, samp++)
	{
		const int data = sfx[i];
		samp->left += (data * leftvol) >> 8;
		samp->right += (data * rightvol) >> 8;
	}

	ch->pos += count;
}

// Mixes all pending sounds into the available output channels.
static void SDL_PaintChannels(const int endtime)
{
	const int snd_vol = (int)(s_volume->value * 256);

	while (paintedtime < endtime)
	{
		// If paintbuffer is smaller than SDL buffer
		int end = endtime;
		if (endtime - paintedtime > SDL_PAINTBUFFER_SIZE)
			end = paintedtime + SDL_PAINTBUFFER_SIZE;

		// Start any playsounds
		while (true)
		{
			playsound_t* ps = s_pendingplays.next;
			if (ps == NULL || ps == &s_pendingplays)
				break; // No more pending sounds

			if (ps->begin <= paintedtime)
			{
				S_IssuePlaysound(ps);
				continue;
			}

			if (ps->begin < end)
				end = ps->begin; // Stop here

			break;
		}

		memset(paintbuffer, 0, (end - paintedtime) * sizeof(portable_samplepair_t));

		// Paint in the channels.
		channel_t *ch = channels;
		for (int i = 0; i < s_numchannels; i++, ch++)
		{
			int ltime = paintedtime;

			while (ltime < end)
			{
				if (!ch->sfx || (!ch->leftvol && !ch->rightvol))
					break;

				// Max painting is to the end of the buffer
				int count = end - ltime;

				// Might be stopped by running out of data
				if (ch->end - ltime < count)
					count = ch->end - ltime;

				sfxcache_t *sc = S_LoadSound(ch->sfx);
				if (!sc)
					break;

				if (count > 0)
				{
					if (sc->width == 1)
						SDL_PaintChannelFrom8(ch, sc, count, ltime - paintedtime);
					else
						SDL_PaintChannelFrom16(ch, sc, count, ltime - paintedtime, snd_vol);

					ltime += count;
				}

				// If at end of loop, restart
				if (ltime >= ch->end)
				{
					if (ch->autosound)
					{
						// Autolooping sounds always go back to start
						ch->pos = 0;
						ch->end = ltime + sc->length;
					}
					else if (sc->loopstart >= 0)
					{
						ch->pos = sc->loopstart;
						ch->end = ltime + sc->length - ch->pos;
					}
					else
					{
						// Channel just stopped
						ch->sfx = NULL;
					}
				}
			}
		}

		if (s_rawend >= paintedtime)
		{
			const int stop = min(end, s_rawend);
			for (int i = paintedtime; i < stop; i++)
			{
				const int s = i & (MAX_RAW_SAMPLES - 1);
				paintbuffer[i - paintedtime].left += s_rawsamples[s].left;
				paintbuffer[i - paintedtime].right += s_rawsamples[s].right;
			}
		}

		// Transfer out according to SDL format
		SDL_TransferPaintBuffer(end);
		paintedtime = end;
	}
}

#pragma endregion

#pragma region ======================= Spatialization

// Spatialize a sound effect based on it's origin.
static void SDL_SpatializeOrigin(const vec3_t origin, const float master_vol, const float dist_mult, int *left_vol, int *right_vol) // S_SpatializeOrigin in KMQ2
{
	if (cls.state != ca_active)
	{
		*left_vol = 255;
		*right_vol = 255;

		return;
	}

	// Calculate stereo seperation and distance attenuation
	vec3_t source_vec;
	VectorSubtract(origin, listener_origin, source_vec);

	vec_t dist = VectorNormalize(source_vec);
	dist -= SDL_FULLVOLUME;
	dist = max(0, dist); // Close enough to be at full volume
	dist *= dist_mult;

	float lscale, rscale;
	if (sound.channels == 1 || dist_mult == 0)
	{
		rscale = 1.0f;
		lscale = 1.0f;
	}
	else
	{
		const float dot = DotProduct(listener_right, source_vec);
		rscale = 0.5f * (1.0f + dot);
		lscale = 0.5f * (1.0f - dot);
	}

	// Add in distance effect
	float scale = (1.0f - dist) * rscale;
	*right_vol = (int)(master_vol * scale);
	*right_vol = max(0, *right_vol);

	scale = (1.0f - dist) * lscale;
	*left_vol = (int)(master_vol * scale);
	*left_vol = max(0, *left_vol);
}

// Spatializes a channel.
void SDL_Spatialize(channel_t *ch) // S_Spatialize in KMQ2
{
	// Anything coming from the view entity will always be full volume
	if (ch->entnum == cl.playernum + 1)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;

		return;
	}

	vec3_t origin;
	if (ch->fixed_origin)
		VectorCopy(ch->origin, origin);
	else
		CL_GetEntitySoundOrigin(ch->entnum, origin);

	SDL_SpatializeOrigin(origin, (float)ch->master_vol, ch->dist_mult, &ch->leftvol, &ch->rightvol);
}

#pragma endregion

#pragma region ======================= Utility methods

// Entities with a "sound" field will generate looped sounds that are automatically
// started, stopped and merged together as the entities are sent to the client.
static void SDL_AddLoopSounds()
{
	int sounds[MAX_EDICTS];
	float dist_mult;
	
	if (cl_paused->integer || cls.state != ca_active || !cl.sound_prepped)
		return;

	memset(&sounds, 0, sizeof(int) * MAX_EDICTS);

	// Build sounds list
	for (int i = 0; i < cl.frame.num_entities; i++)
	{
		if (i >= MAX_EDICTS)
			break;
		
		const int num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		entity_state_t *ent = &cl_parse_entities[num];
		sounds[i] = ent->sound;
	}

	for (int i = 0; i < cl.frame.num_entities; i++)
	{
		if (!sounds[i])
			continue;

		sfx_t *sfx = cl.sound_precache[sounds[i]];
		if (!sfx)
			continue; // Bad sound effect

		sfxcache_t* sc = sfx->cache;
		if (!sc)
			continue;

		int num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		entity_state_t *ent = &cl_parse_entities[num];

#if defined(NEW_ENTITY_STATE_MEMBERS) && defined(LOOP_SOUND_ATTENUATION)
		if (ent->attenuation <= 0.0f || ent->attenuation == ATTN_STATIC)
			dist_mult = SDL_LOOPATTENUATE;
		else
			dist_mult = ent->attenuation * 0.0005f;
#else
		dist_mult = SDL_LOOPATTENUATE;
#endif

		vec3_t origin;
		CL_GetEntitySoundOrigin(ent->number, origin);

		// Find the total contribution of all sounds of this type
		int left_total, right_total;
		SDL_SpatializeOrigin(origin, 255.0f, dist_mult, &left_total, &right_total);

		for (int j = i + 1; j < cl.frame.num_entities; j++)
		{
			if (sounds[j] != sounds[i])
				continue;

			sounds[j] = 0; // Don't check this again later
			num = (cl.frame.parse_entities + j) & (MAX_PARSE_ENTITIES - 1);
			ent = &cl_parse_entities[num];

			int left, right;
			CL_GetEntitySoundOrigin(ent->number, origin); //mxd
			SDL_SpatializeOrigin(origin, 255.0f, dist_mult, &left, &right);

			left_total += left;
			right_total += right;
		}

		if (left_total == 0 && right_total == 0)
			continue; // Not audible

		// Allocate a channel
		channel_t *ch = S_PickChannel(0, 0);
		if (!ch)
			return;

		ch->leftvol = min(255, left_total);
		ch->rightvol = min(255, right_total);
		ch->autosound = true; // Remove next frame
		ch->sfx = sfx;

		// Sometimes, the sc->length argument can become 0, and in that case we get a SIGFPE in the next modulo operation.
		if (sc->length == 0)
		{
			ch->pos = 0;
			ch->end = 0;
		}
		else
		{
			ch->pos = paintedtime % sc->length;
			ch->end = paintedtime + sc->length - ch->pos;
		}
	}
}

// Clears the playback buffer so that all playback stops.
void SDL_ClearBuffer()
{
	if (!sound_started)
		return;

	s_rawend = 0;

	SDL_LockAudio();

	if (sound.buffer)
	{
		const int clear = (sound.samplebits == 8 ? 0x80 : 0);
		byte *ptr = sound.buffer;
		int i = sound.samples * sound.samplebits / 8;

		while (i--)
		{
			*ptr = clear;
			ptr++;
		}
	}

	SDL_UnlockAudio();
}

// Calculates the absolute timecode of current playback.
static void SDL_UpdateSoundtime()
{
	static int buffers;
	static int oldsamplepos;

	const int fullsamples = sound.samples / sound.channels;

	// It is possible to miscount buffers if it has wrapped twice between calls to SDL_Update. Oh well. This a hack around that.
	if (playpos < oldsamplepos)
	{
		buffers++; // buffer wrapped

		if (paintedtime > 0x40000000)
		{
			// Time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds();
		}
	}

	oldsamplepos = playpos;
	soundtime = buffers * fullsamples + playpos / sound.channels;
}

// Updates the volume scale table based on current volume setting.
static void SDL_UpdateScaletable() // S_InitScaletable in KMQ2
{
	if (s_volume->value > 2.0f)
		Cvar_Set("s_volume", "2");
	else if (s_volume->value < 0)
		Cvar_Set("s_volume", "0");

	s_volume->modified = false;

	for (int i = 0; i < 32; i++)
	{
		const int scale = (int)(i * 8 * 256 * s_volume->value);
		for (int j = 0; j < 256; j++)
			snd_scaletable[i][j] = (j < 128 ? j : j - 255) * scale;
	}
}

// Saves a sound sample into cache. Modified version of ResampleSfx from KMQ2.
qboolean SDL_Cache(sfx_t *sfx, const wavinfo_t *info, const byte *data)
{
	const float stepscale = (float)info->rate / sound.speed;
	const int len = (int)(info->samples / stepscale);

	if (info->samples == 0 || len == 0)
	{
		Com_Printf(S_COLOR_YELLOW"%s: zero length sound encountered: '%s'.\n", __func__, sfx->name);
		return false;
	}

	sfx->cache = Z_Malloc((len * info->width * info->channels) + sizeof(sfxcache_t));
	sfxcache_t* sc = sfx->cache;

	if (!sc)
	{
		Com_Printf(S_COLOR_YELLOW"%s: failed to allocate sfx cache for sound '%s'.\n", __func__, sfx->name); //mxd
		return false;
	}

	sc->loopstart = info->loopstart;
	sc->stereo = info->channels; //mxd. 0 in YQ2
	sc->length = len;

	if (sc->loopstart != -1)
		sc->loopstart = (int)(sc->loopstart / stepscale);

	sc->width = (s_loadas8bit->integer ? 1 : info->width);

	// Resample / decimate to the current source rate
	if (stepscale == 1 && info->width == 1 && sc->width == 1)
	{
		// Fast special case
		for (int i = 0; i < sc->length / stepscale; i++)
			((signed char *)sc->data)[i] = (int)(data[i] - 128);
	}
	else
	{
		// General case
		uint samplefrac = 0;
		const int fracstep = (int)(stepscale * 256);

		for (int i = 0; i < len; i++)
		{
			const int srcsample = samplefrac >> 8;
			samplefrac += fracstep;

			int sample;
			if (info->width == 2)
				sample = ((short *)data)[srcsample];
			else
				sample = (int)((byte)(data[srcsample]) - 128) << 8;

			if (sc->width == 2)
				((short *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}

	return true;
}

// Playback of "raw samples", e.g. samples without an origin entity. Used for music and cinematic playback.
void SDL_RawSamples(const int samples, const int rate, const int width, const int channels, const byte *data, const float volume)
{
	const float scale = (float)rate / sound.speed;
	int intVolume = (int)(clamp(volume, 0.0f, 1.0f) * 256);

	if (channels == 2 && width == 2)
	{
		for (int i = 0; ; i++)
		{
			const int src = (int)(i * scale);
			if (src >= samples)
				break;

			const int dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left =  ((short *)data)[src * 2 + 0] * intVolume;
			s_rawsamples[dst].right = ((short *)data)[src * 2 + 1] * intVolume;
		}
	}
	else if (channels == 1 && width == 2)
	{
		for (int i = 0; ; i++)
		{
			const int src = (int)(i * scale);
			if (src >= samples)
				break;

			const int dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left =  ((short *)data)[src] * intVolume;
			s_rawsamples[dst].right = ((short *)data)[src] * intVolume;
		}
	}
	else if (channels == 2 && width == 1)
	{
		intVolume *= 256;

		for (int i = 0; ; i++)
		{
			const int src = (int)(i * scale);
			if (src >= samples)
				break;

			const int dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left =  (((byte *)data)[src * 2 + 0] - 128) * intVolume;
			s_rawsamples[dst].right = (((byte *)data)[src * 2 + 1] - 128) * intVolume;
		}
	}
	else if (channels == 1 && width == 1)
	{
		intVolume *= 256;

		for (int i = 0; ; i++)
		{
			const int src = (int)(i * scale);
			if (src >= samples)
				break;

			const int dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left =  (((byte *)data)[src] - 128) * intVolume;
			s_rawsamples[dst].right = (((byte *)data)[src] - 128) * intVolume;
		}
	}
}

#pragma endregion

#pragma region ======================= Update

// Runs every frame, handles all necessary sound calculations and fills the playback buffer.
void SDL_Update()
{
	// If the loading plaque is up, clear everything out to make sure we aren't looping a dirty SDL buffer while loading
	if (cls.disable_screen)
	{
		SDL_ClearBuffer();
		return;
	}

	// Rebuild scale tables if volume was modified
	if (s_volume->modified)
		SDL_UpdateScaletable();

	// Update spatialization for dynamic sounds
	channel_t *ch = channels;
	for (int i = 0; i < s_numchannels; i++, ch++)
	{
		if (!ch->sfx)
			continue;

		if (ch->autosound)
		{
			// Autosounds are regenerated fresh each frame
			memset(ch, 0, sizeof(*ch));
			continue;
		}

		// Respatialize channel
		SDL_Spatialize(ch);

		if (!ch->leftvol && !ch->rightvol)
			memset(ch, 0, sizeof(*ch));
	}

	// Add loopsounds
	SDL_AddLoopSounds();

	// Debugging output
	if (s_show->integer)
	{
		int total = 0;
		ch = channels;
		for (int i = 0; i < s_numchannels; i++, ch++)
		{
			if (ch->sfx && (ch->leftvol || ch->rightvol))
			{
				Com_Printf("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}
		}

		Com_Printf("----(%i)---- painted: %i\n", total, paintedtime);
	}

	// Stream music
	S_UpdateBackgroundTrack();

	if (!sound.buffer)
		return;

	// Mix the samples
	SDL_LockAudio();

	// Updates SDL time
	SDL_UpdateSoundtime();

	if (!soundtime)
		return;

	// Check to make sure that we haven't overshot
	if (paintedtime < soundtime)
	{
		Com_DPrintf(S_COLOR_RED"%s: overflow\n", __func__);
		paintedtime = soundtime;
	}

	// Mix ahead of current position
	int endtime = (int)(soundtime + s_mixahead->value * sound.speed);

	// Mix to an even submission block size
	endtime = (endtime + sound.submission_chunk - 1) & ~(sound.submission_chunk - 1);
	const int samps = sound.samples >> (sound.channels - 1);

	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	SDL_PaintChannels(endtime);
	SDL_UnlockAudio();
}

#pragma endregion

#pragma region ======================= Init / shutdown

// Callback funktion for SDL. Writes sound data to SDL when requested.
static void SDL_Callback(void *userdata, byte *stream, int length)
{
	int pos = (playpos * (backend->samplebits / 8));

	if (pos >= samplesize)
	{
		playpos = 0;
		pos = 0;
	}

	// This can't happen!
	if (!snd_inited)
	{
		memset(stream, '\0', length);
		return;
	}

	const int tobufferend = samplesize - pos;

	int length1, length2;
	if (length > tobufferend)
	{
		length1 = tobufferend;
		length2 = length - length1;
	}
	else
	{
		length1 = length;
		length2 = 0;
	}

	memcpy(stream, backend->buffer + pos, length1);

	// Set new position
	if (length2 <= 0)
	{
		playpos += length1 / (backend->samplebits / 8);
	}
	else
	{
		memcpy(stream + length1, backend->buffer, length2);
		playpos = length2 / (backend->samplebits / 8);
	}

	if (playpos >= samplesize)
		playpos = 0;
}

// Initializes the SDL sound backend and sets up SDL.
qboolean SDL_BackendInit()
{
	// This should never happen, but this is Quake 2 ... 
	if (snd_inited)
		return true;

	// Users are stupid
	if (s_sndbits->integer != 8 && s_sndbits->integer != 16)
	{
		Cvar_SetInteger("s_sndbits", 16);
		s_sndbits->modified = false;
	}

	s_sdldriver = (Cvar_Get("s_sdldriver", "directsound", CVAR_ARCHIVE));

	char reqdriver[128];
	snprintf(reqdriver, sizeof(reqdriver), "%s=%s", "SDL_AUDIODRIVER", s_sdldriver->string);
	putenv(reqdriver);

	Com_Printf("Starting SDL audio backend.\n");

	if (!SDL_WasInit(SDL_INIT_AUDIO))
	{
		if (SDL_Init(SDL_INIT_AUDIO) == -1)
		{
			Com_Printf(S_COLOR_YELLOW"Failed to initialize SDL audio backend: %s.\n", SDL_GetError());
			return false;
		}
	}

	SDL_AudioSpec desired;
	SDL_AudioSpec obtained;
	memset(&desired, '\0', sizeof(desired));
	memset(&obtained, '\0', sizeof(obtained));

	switch(s_khz->integer)
	{
		default:
		case 48: desired.freq = 48000; break;
		case 44: desired.freq = 44100; break;
		case 22: desired.freq = 22050; break;
		case 11: desired.freq = 11025; break;
	}

	desired.format = (s_sndbits->integer == 16 ? AUDIO_S16SYS : AUDIO_U8);

	if (desired.freq <= 11025)
		desired.samples = 256;
	else if (desired.freq <= 22050)
		desired.samples = 512;
	else if (desired.freq <= 44100)
		desired.samples = 1024;
	else
		desired.samples = 2048;

	desired.channels = max(2, s_sndchannels->integer);
	desired.callback = SDL_Callback;
	desired.userdata = NULL;

	// Open the audio device
	if (SDL_OpenAudio(&desired, &obtained) == -1)
	{
		Com_Printf(S_COLOR_RED"SDL_OpenAudio() failed: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return false;
	}

	// This points to the frontend
	backend = &sound;

	playpos = 0;
	backend->samplebits = (obtained.format & 0xff); // First byte of format is bits
	backend->channels = obtained.channels;

	int tmp = obtained.samples * obtained.channels * 10;

	if (tmp & (tmp - 1))
	{	
		// Make it a power of two
		int val = 1;
		while (val < tmp)
			val <<= 1;

		tmp = val;
	}

	backend->samples = tmp;

	backend->submission_chunk = 1;
	backend->speed = obtained.freq;
	samplesize = (backend->samples * (backend->samplebits / 8));
	backend->buffer = calloc(1, samplesize);
	s_numchannels = MAX_CHANNELS;

	//mxd. Print sound info...
	Com_Printf("SDL audio spec  : %d Hz, %d samples, %d channels.\n", obtained.freq, obtained.samples, obtained.channels);

	const char *driver = SDL_GetCurrentAudioDriver();
	const char *device = SDL_GetAudioDeviceName(0, SDL_FALSE);

	char drivername[128];
	Q_snprintfz(drivername, sizeof(drivername), "%s - %s",
		(driver != NULL ? driver : "(UNKNOWN)"),
		(device != NULL ? device : "(UNKNOWN)"));

	Com_Printf("SDL audio driver: %s, %i bytes buffer.\n", drivername, samplesize);

	SDL_UpdateScaletable();
	SDL_PauseAudio(0);

	Com_Printf("SDL audio initialized.\n");

	soundtime = 0;
	snd_inited = true;

	return true;
}

// Shuts the SDL backend down.
void SDL_BackendShutdown()
{
	Com_Printf("Closing SDL audio device...\n");
	SDL_PauseAudio(1);
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	free(backend->buffer);
	backend->buffer = NULL;
	playpos = 0;
	samplesize = 0;
	snd_inited = false;
	Com_Printf("SDL audio device shut down.\n");
}

#pragma endregion