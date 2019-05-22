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
// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "client.h"
#include "snd_loc.h"

//mxd. Minimum and maximum possible values of a signed short
#define SHORT_MIN	-32768
#define SHORT_MAX	32767

#define PAINTBUFFER_SIZE	2048
static portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];

static int snd_scaletable[32][256];

static int *snd_p;
static int snd_linear_count;
static int snd_vol;
static short *snd_out;

#pragma region ======================= Utility functions

static void S_WriteLinearBlastStereo16(void)
{
	for (int i = 0; i < snd_linear_count; i += 2)
	{
		int val = snd_p[i] >> 8;
		snd_out[i] = clamp(val, SHORT_MIN, SHORT_MAX); //mxd

		val = snd_p[i + 1] >> 8;
		snd_out[i + 1] = clamp(val, SHORT_MIN, SHORT_MAX); //mxd
	}
}

static void S_TransferStereo16(unsigned long *pbuf, int endtime)
{
	snd_p = (int *)paintbuffer;
	int lpaintedtime = paintedtime;

	while (lpaintedtime < endtime)
	{
		// Handle recirculating buffer issues
		const int lpos = lpaintedtime & ((dma.samples >> 1) - 1);

		snd_out = (short *)pbuf + (lpos << 1);

		snd_linear_count = (dma.samples >> 1) - lpos;
		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

		// Write a linear blast of samples
		S_WriteLinearBlastStereo16();

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count >> 1);
	}
}

static void S_TransferPaintBuffer(int endtime)
{
	ulong *pbuf = (ulong *)dma.buffer;

	if (s_testsound->integer)
	{
		// Write a fixed sine wave
		const int delta = (endtime - paintedtime);
		for (int i = 0; i < delta; i++)
		{
			paintbuffer[i].left = sin((paintedtime + i) * 0.1) * 20000 * 256;
			paintbuffer[i].right = paintbuffer[i].left;
		}
	}

	if (dma.samplebits == 16 && dma.channels == 2)
	{
		// Optimized case
		S_TransferStereo16(pbuf, endtime);
	}
	else
	{
		// General case
		int *p = (int *)paintbuffer;
		int count = (endtime - paintedtime) * dma.channels;
		const int out_mask = dma.samples - 1; 
		int out_idx = paintedtime * dma.channels & out_mask;
		const int step = 3 - dma.channels;

		if (dma.samplebits == 16)
		{
			short *out = (short *)pbuf;
			while (count--)
			{
				const int val = *p >> 8;
				p+= step;

				out[out_idx] = clamp(val, SHORT_MIN, SHORT_MAX); //mxd
				out_idx = (out_idx + 1) & out_mask;
			}
		}
		else if (dma.samplebits == 8)
		{
			byte *out = (byte *)pbuf;
			while (count--)
			{
				int val = *p >> 8;
				p+= step;

				val = clamp(val, SHORT_MIN, SHORT_MAX); //mxd
				out[out_idx] = (val + 32768) >> 8; //mxd. Let's apply offset before byte-shifting, so the val is always positive...
				out_idx = (out_idx + 1) & out_mask;
			}
		}
	}
}

#pragma endregion

#pragma region ======================= Channel mixing

static void S_PaintChannelFrom8(channel_t *ch, sfxcache_t *sc, int count, int offset)
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

static void S_PaintChannelFrom16(channel_t *ch, sfxcache_t *sc, int count, int offset)
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

void S_PaintChannels(int endtime)
{
	snd_vol = s_volume->value * 256;

	while (paintedtime < endtime)
	{
		// If paintbuffer is smaller than DMA buffer
		int end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

		// Start any playsounds
		while (true)
		{
			playsound_t *ps = s_pendingplays.next;
			if (ps == &s_pendingplays)
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

		// Clear the paint buffer
		if (s_rawend < paintedtime)
		{
			memset(paintbuffer, 0, (end - paintedtime) * sizeof(portable_samplepair_t));
		}
		else
		{
			const int stop = min(end, s_rawend);
			int i;

			for (i = paintedtime; i < stop; i++)
			{
				const int s = i & (MAX_RAW_SAMPLES - 1);
				paintbuffer[i - paintedtime] = s_rawsamples[s];
			}

			for (; i < end; i++)
			{
				paintbuffer[i - paintedtime].left = 0;
				paintbuffer[i - paintedtime].right = 0;
			}
		}

		// Paint in the channels.
		channel_t *ch = channels;
		for (int i = 0; i < MAX_CHANNELS; i++, ch++)
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

				if (count > 0 && ch->sfx != NULL) //mxd. V560 A part of conditional expression is always true: ch->sfx.
				{	
					if (sc->width == 1)// FIXME; 8 bit asm is wrong now
						S_PaintChannelFrom8(ch, sc, count,  ltime - paintedtime);
					else
						S_PaintChannelFrom16(ch, sc, count, ltime - paintedtime);
	
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

		// Transfer out according to DMA format
		S_TransferPaintBuffer(end);
		paintedtime = end;
	}
}

void S_InitScaletable(void)
{
	s_volume->modified = false;

	for (int i = 0; i < 32; i++)
	{
		const int scale = (int)(i * 8 * 256 * s_volume->value);
		for (int j = 0; j < 256; j++)
			snd_scaletable[i][j] = (j < 128 ? j : j - 255) * scale;
	}
}

#pragma endregion