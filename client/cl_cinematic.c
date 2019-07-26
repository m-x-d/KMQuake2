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

#include "client.h"

#define MAX_CINEMATICS		8

#define CIN_SYSTEM			1	// A cinematic handled by the client system
#define CIN_LOOPED			2	// Looped playback
#define CIN_SILENT			4	// Don't play audio

#define RoQ_INFO			0x1001
#define RoQ_QUAD_CODEBOOK	0x1002
#define RoQ_QUAD_VQ			0x1011
#define RoQ_SOUND_MONO		0x1020
#define RoQ_SOUND_STEREO	0x1021

#define RoQ_ID_MOT			0x0000
#define RoQ_ID_FCC			0x0001
#define RoQ_ID_SLD			0x0002
#define RoQ_ID_CCC			0x0003

typedef struct
{
	unsigned short	id;
	unsigned int	size;
	unsigned short	argument;
} roqChunk_t;

typedef struct
{
	byte y[4];
	byte u;
	byte v;
} roqCell_t;

typedef struct
{
	byte idx[4];
} roqQCell_t;

typedef struct
{
	char			name[MAX_QPATH];
	qboolean		playing;
	fileHandle_t	file;
	int				size;
	int				start;
	int				remaining;
	qboolean		isRoQ;
	int				rate;
	
	int				x;
	int				y;
	int				w;
	int				h;
	int				flags;

	int				sndRate;
	int				sndWidth;
	int				sndChannels;

	int				vidWidth;
	int				vidHeight;
	byte			*vidBuffer;

	int				frameCount;
	int				frameTime;

	unsigned		palette[256];

	// PCX stuff
	byte			*pcxBuffer;

	// Order 1 Huffman stuff
	int				*hNodes1;
	int				hNumNodes1[256];

	int				hUsed[512];
	int				hCount[512];

	byte			*hBuffer;

	// RoQ stuff
	roqChunk_t		roqChunk;
	roqCell_t		roqCells[256];
	roqQCell_t		roqQCells[256];

	short			roqSndSqrTable[256];

	byte			*roqBuffer;
	byte			*roqBufferPtr[2];
} cinematic_t;

static cinematic_t cinematics[MAX_CINEMATICS];

// If path doesn't have a / or \\, append newPath (newPath should not include the /)
static void DefaultPath(char *path, int maxSize, const char *newPath)
{
	char oldPath[MAX_OSPATH];
	char *s = path;

	while (*s)
	{
		if (*s == '/' || *s == '\\')
			return; // It has a path
		
		s++;
	}

	Q_strncpyz(oldPath, path, sizeof(oldPath));
	Com_sprintf(path, maxSize, "%s/%s", newPath, oldPath);
}

//=============================================================

static void CIN_Skip(cinematic_t *cin, int count)
{
	FS_Seek(cin->file, count, FS_SEEK_CUR);
	cin->remaining -= count;
}

static void CIN_SoundSqrTableInit(cinematic_t *cin)
{
	for (int i = 0; i < 128; i++)
	{
		cin->roqSndSqrTable[i] = i * i;
		cin->roqSndSqrTable[i + 128] = -(i * i);
	}
}

static void CIN_ReadChunk(cinematic_t *cin)
{
	roqChunk_t *chunk = &cin->roqChunk;

	FS_Read(&chunk->id, sizeof(chunk->id), cin->file);
	FS_Read(&chunk->size, sizeof(chunk->size), cin->file);
	FS_Read(&chunk->argument, sizeof(chunk->argument), cin->file);

	cin->remaining -= sizeof(roqChunk_t);
}

static void CIN_ReadInfo(cinematic_t *cin)
{
	short data[4];

	FS_Read(data, sizeof(data), cin->file);
	cin->remaining -= sizeof(data);

	cin->vidWidth = data[0];
	cin->vidHeight = data[1];

	if (cin->roqBuffer)
		Z_Free(cin->roqBuffer);

	cin->roqBuffer = Z_Malloc(cin->vidWidth * cin->vidHeight * 8);

	cin->roqBufferPtr[0] = cin->roqBuffer;
	cin->roqBufferPtr[1] = cin->roqBuffer + cin->vidWidth * cin->vidHeight * 4;
}

static void CIN_ReadCodebook(cinematic_t *cin)
{
	roqChunk_t *chunk = &cin->roqChunk;

	unsigned int nv1 = (chunk->argument >> 8) & 0xff; //mxd. int -> unsigned int
	if (!nv1)
		nv1 = 256;

	unsigned int nv2 = chunk->argument & 0xff; //mxd. int -> unsigned int
	if (!nv2 && (nv1 * 6 < chunk->size))
		nv2 = 256;

	FS_Read(cin->roqCells, sizeof(roqCell_t) * nv1, cin->file);
	FS_Read(cin->roqQCells, sizeof(roqQCell_t) * nv2, cin->file);
	cin->remaining -= chunk->size;
}

static void CIN_DecodeBlock(byte *dst0, byte *dst1, const byte *src0, const byte *src1, float u, float v)
{
	int rgb[3];

	// Convert YCbCr to RGB
	rgb[0] = (int)(1.402f * v);
	rgb[1] = (int)(-0.34414f * u - 0.71414f * v);
	rgb[2] = (int)(1.772f * u);

	// 1st pixel
	dst0[0] = clamp(rgb[0] + src0[0], 0, 255);
	dst0[1] = clamp(rgb[1] + src0[0], 0, 255);
	dst0[2] = clamp(rgb[2] + src0[0], 0, 255);
	dst0[3] = 255;

	// 2nd pixel
	dst0[4] = clamp(rgb[0] + src0[1], 0, 255);
	dst0[5] = clamp(rgb[1] + src0[1], 0, 255);
	dst0[6] = clamp(rgb[2] + src0[1], 0, 255);
	dst0[7] = 255;

	// 3rd pixel
	dst1[0] = clamp(rgb[0] + src1[0], 0, 255);
	dst1[1] = clamp(rgb[1] + src1[0], 0, 255);
	dst1[2] = clamp(rgb[2] + src1[0], 0, 255);
	dst1[3] = 255;

	// 4th pixel
	dst1[4] = clamp(rgb[0] + src1[1], 0, 255);
	dst1[5] = clamp(rgb[1] + src1[1], 0, 255);
	dst1[6] = clamp(rgb[2] + src1[1], 0, 255);
	dst1[7] = 255;
}

static void CIN_ApplyVector2x2(cinematic_t *cin, int x, int y, const roqCell_t *cell)
{
	byte *dst0 = cin->roqBufferPtr[0] + (y * cin->vidWidth + x) * 4;
	byte *dst1 = dst0 + cin->vidWidth * 4;

	CIN_DecodeBlock(dst0, dst1, cell->y, cell->y + 2, (float)((int)cell->u - 128), (float)((int)cell->v - 128));
}

static void CIN_ApplyVector4x4(cinematic_t *cin, int x, int y, const roqCell_t *cell)
{
	byte yp[4];

	const float u = (float)((int)cell->u - 128);
	const float v = (float)((int)cell->v - 128);

	yp[0] = yp[1] = cell->y[0];
	yp[2] = yp[3] = cell->y[1];

	byte *dst0 = cin->roqBufferPtr[0] + (y * cin->vidWidth + x) * 4;
	byte *dst1 = dst0 + cin->vidWidth * 4;

	CIN_DecodeBlock(dst0, dst0 + 8, yp, yp + 2, u, v);
	CIN_DecodeBlock(dst1, dst1 + 8, yp, yp + 2, u, v);

	yp[0] = yp[1] = cell->y[2];
	yp[2] = yp[3] = cell->y[3];

	dst0 += cin->vidWidth * 8;
	dst1 += cin->vidWidth * 8;

	CIN_DecodeBlock(dst0, dst0 + 8, yp, yp + 2, u, v);
	CIN_DecodeBlock(dst1, dst1 + 8, yp, yp + 2, u, v);
}

static void CIN_ApplyMotion4x4(cinematic_t *cin, int x, int y, byte mv, char meanX, char meanY)
{
	const int x1 = x + 8 - (mv >> 4) - meanX;
	const int y1 = y + 8 - (mv & 15) - meanY;

	byte *src = cin->roqBufferPtr[1] + (y1 * cin->vidWidth + x1) * 4;
	byte *dst = cin->roqBufferPtr[0] + (y * cin->vidWidth + x) * 4;

	for (int i = 0; i < 4; i++)
	{
		memcpy(dst, src, 4 * 4);

		src += cin->vidWidth * 4;
		dst += cin->vidWidth * 4;
	}
}

static void CIN_ApplyMotion8x8(cinematic_t *cin, int x, int y, byte mv, char meanX, char meanY)
{
	const int x1 = x + 8 - (mv >> 4) - meanX;
	const int y1 = y + 8 - (mv & 15) - meanY;

	byte *src = cin->roqBufferPtr[1] + (y1 * cin->vidWidth + x1) * 4;
	byte *dst = cin->roqBufferPtr[0] + (y * cin->vidWidth + x) * 4;

	for (int i = 0; i < 8; i++)
	{
		memcpy(dst, src, 8 * 4);

		src += cin->vidWidth * 4;
		dst += cin->vidWidth * 4;
	}
}

static int CIN_SmallestNode1(cinematic_t *cin, int numNodes)
{
	int best = 99999999;
	int bestNode = -1;

	for (int i = 0; i < numNodes; i++)
	{
		if (cin->hUsed[i] || !cin->hCount[i])
			continue;

		if (cin->hCount[i] < best)
		{
			best = cin->hCount[i];
			bestNode = i;
		}
	}

	if (bestNode == -1)
		return -1;

	cin->hUsed[bestNode] = true;
	return bestNode;
}

// Reads the 64k counts table and initializes the node trees
static void CIN_Huff1TableInit(cinematic_t *cin)
{
	byte counts[256];

	if (!cin->hNodes1)
		cin->hNodes1 = Z_Malloc(256 * 256 * 4 * 2);

	for (int prev = 0; prev < 256; prev++)
	{
		memset(cin->hCount, 0, sizeof(cin->hCount));
		memset(cin->hUsed, 0, sizeof(cin->hUsed));

		// Read a row of counts
		FS_Read(counts, sizeof(counts), cin->file);
		cin->remaining -= sizeof(counts);

		for (int j = 0; j < 256; j++)
			cin->hCount[j] = counts[j];

		// Build the nodes
		int numNodes = 256;
		int *nodeBase = cin->hNodes1 + prev * 512;

		while (numNodes != 511)
		{
			int *node = nodeBase + (numNodes - 256) * 2;

			// Pick two lowest counts
			node[0] = CIN_SmallestNode1(cin, numNodes);
			if (node[0] == -1)
				break;

			node[1] = CIN_SmallestNode1(cin, numNodes);
			if (node[1] == -1)
				break;

			cin->hCount[numNodes] = cin->hCount[node[0]] + cin->hCount[node[1]];
			numNodes++;
		}

		cin->hNumNodes1[prev] = numNodes - 1;
	}
}

static void CIN_Huff1Decompress(cinematic_t *cin, const byte *data, int size)
{
	if (!cin->hBuffer)
		cin->hBuffer = Z_Malloc(cin->vidWidth * cin->vidHeight * 4);

	// Get decompressed count
	int count = data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
	const byte *input = data + 4;
	unsigned *out = (unsigned *)cin->hBuffer;

	// Read bits
	int *nodesBase = cin->hNodes1 - 512; // Nodes 0-255 aren't stored
	int *nodes = nodesBase;
	int nodeNum = cin->hNumNodes1[0];

	while (count)
	{
		int in = *input++;
		
		for(int i = 0; i < 8; i++)
		{
			if (nodeNum < 256)
			{
				nodes = nodesBase + (nodeNum << 9);
				*out++ = cin->palette[nodeNum];

				if (!--count)
					break;

				nodeNum = cin->hNumNodes1[nodeNum];
			}

			nodeNum = nodes[nodeNum * 2 + (in & 1)];
			in >>= 1;
		}
	}

	if (input - data != size && input - data != size + 1)
		Com_Error(ERR_DROP, "CIN_Huff1Decompress: decompression overread by %i", (input - data) - size);
}

static void CIN_ReadPalette(cinematic_t *cin)
{
	byte palette[768];

	FS_Read(palette, sizeof(palette), cin->file);
	cin->remaining -= sizeof(palette);

	byte *pal = (byte *)cin->palette;

	for (int i = 0; i < 256; i++)
	{
		pal[i * 4 + 0] = palette[i * 3 + 0];
		pal[i * 4 + 1] = palette[i * 3 + 1];
		pal[i * 4 + 2] = palette[i * 3 + 2];
		pal[i * 4 + 3] = 255;
	}
}

static void CIN_ReadVideoFrame(cinematic_t *cin)
{
	if (!cin->isRoQ)
	{
		byte compressed[0x20000];
		int	size;

		FS_Read(&size, sizeof(size), cin->file);
		cin->remaining -= sizeof(size);

		if (size < 1 || size > sizeof(compressed))
			Com_Error(ERR_DROP, "CIN_ReadVideoFrame: bad compressed frame size (%i)", size);

		FS_Read(compressed, size, cin->file);
		cin->remaining -= size;

		CIN_Huff1Decompress(cin, compressed, size);

		cin->vidBuffer = cin->hBuffer;
	}
	else
	{
		roqChunk_t	*chunk = &cin->roqChunk;
		roqQCell_t	*qcell;
		byte		c[4];

		short vqFlg = 0;
		int vqFlgPos = -1;

		int xPos = 0;
		int yPos = 0;
		int pos = chunk->size;

		while (pos > 0)
		{
			for (int yp = yPos; yp < yPos + 16; yp += 8)
			{
				for (int xp = xPos; xp < xPos + 16; xp += 8)
				{
					if (vqFlgPos < 0)
					{
						FS_Read(&vqFlg, sizeof(vqFlg), cin->file);
						pos -= sizeof(vqFlg);
						vqFlgPos = 7;
					}

					int vqId = (vqFlg >> (vqFlgPos * 2)) & 0x3;
					vqFlgPos--;
				
					switch (vqId)
					{
					case RoQ_ID_MOT:
						break;

					case RoQ_ID_FCC:
						FS_Read(c, 1, cin->file);
						pos--;
						CIN_ApplyMotion8x8(cin, xp, yp, c[0], (char)((chunk->argument >> 8) & 0xff), (char)(chunk->argument & 0xff));
						break;

					case RoQ_ID_SLD:
						FS_Read(c, 1, cin->file);
						pos--;
						qcell = cin->roqQCells + c[0];
						CIN_ApplyVector4x4(cin, xp, yp, cin->roqCells + qcell->idx[0]);
						CIN_ApplyVector4x4(cin, xp + 4, yp, cin->roqCells + qcell->idx[1]);
						CIN_ApplyVector4x4(cin, xp, yp + 4, cin->roqCells + qcell->idx[2]);
						CIN_ApplyVector4x4(cin, xp + 4, yp + 4, cin->roqCells + qcell->idx[3]);
						break;

					case RoQ_ID_CCC:
						for (int i = 0; i < 4; i++)
						{
							int x = xp;
							int y = yp;

							if (i & 0x01)
								x += 4;
							if (i & 0x02)
								y += 4;
						
							if (vqFlgPos < 0)
							{
								FS_Read(&vqFlg, sizeof(vqFlg), cin->file);
								pos -= sizeof(vqFlg);
								vqFlgPos = 7;
							}

							vqId = (vqFlg >> (vqFlgPos * 2)) & 0x3;
							vqFlgPos--;

							switch (vqId)
							{
							case RoQ_ID_MOT:
								break;

							case RoQ_ID_FCC:
								FS_Read(c, 1, cin->file);
								pos--;
								CIN_ApplyMotion4x4(cin, x, y, c[0], (char)((chunk->argument >> 8) & 0xff), (char)(chunk->argument & 0xff));
								break;

							case RoQ_ID_SLD:
								FS_Read(c, 1, cin->file);
								pos--;
								qcell = cin->roqQCells + c[0];
								CIN_ApplyVector2x2(cin, x, y, cin->roqCells + qcell->idx[0]);
								CIN_ApplyVector2x2(cin, x + 2, y, cin->roqCells + qcell->idx[1]);
								CIN_ApplyVector2x2(cin, x, y + 2, cin->roqCells + qcell->idx[2]);
								CIN_ApplyVector2x2(cin, x + 2, y + 2, cin->roqCells + qcell->idx[3]);
								break;

							case RoQ_ID_CCC:
								FS_Read(&c, 4, cin->file);
								pos -= 4;
								CIN_ApplyVector2x2(cin, x, y, cin->roqCells + c[0]);
								CIN_ApplyVector2x2(cin, x + 2, y, cin->roqCells + c[1]);
								CIN_ApplyVector2x2(cin, x, y + 2, cin->roqCells + c[2]);
								CIN_ApplyVector2x2(cin, x + 2, y + 2, cin->roqCells + c[3]);
								break;
							}
						}
						break;

					default:
						Com_Error(ERR_DROP, "CIN_ReadVideoFrame: unknown VQ code (%i)",  vqId);
					}
				}
			}
			
			xPos += 16;

			if (xPos >= cin->vidWidth)
			{
				xPos -= cin->vidWidth;
				yPos += 16;
			}

			if (yPos >= cin->vidHeight && pos)
			{
				CIN_Skip(cin, pos);
				break;
			}
		}

		cin->remaining -= (chunk->size - pos);
	
		if (cin->frameCount == 0)
		{
			memcpy(cin->roqBufferPtr[1], cin->roqBufferPtr[0], cin->vidWidth * cin->vidHeight * 4);
		}
		else
		{
			byte *tmp = cin->roqBufferPtr[0];
			cin->roqBufferPtr[0] = cin->roqBufferPtr[1];
			cin->roqBufferPtr[1] = tmp;
		}

		cin->vidBuffer = cin->roqBufferPtr[1];
	}
}

static void CIN_ReadAudioFrame(cinematic_t *cin)
{
	byte data[0x40000];
	int samples;

	if (!cin->isRoQ)
	{
		byte *p;

		const int start = cin->frameCount * cin->sndRate / 14;
		const int end = (cin->frameCount + 1) * cin->sndRate / 14;
		samples = end - start;
		const int len = samples * cin->sndWidth * cin->sndChannels;

		if (cin->flags & CIN_SILENT)
		{
			CIN_Skip(cin, len);
			return;
		}

		// HACK: gross hack to keep cinematic audio sync'ed using OpenAL
		if (cin->frameCount == 0)
		{
			samples += 4096;

			const int val = (cin->sndWidth == 2 ? 0x00 : 0x80);
			memset(data, val, 4096 * cin->sndWidth * cin->sndChannels);

			p = data + (4096 * cin->sndWidth * cin->sndChannels);
		}
		else
		{
			p = data;
		}

		FS_Read(p, len, cin->file);
		cin->remaining -= len;
	}
	else
	{
		roqChunk_t *chunk = &cin->roqChunk;
		
		if (cin->flags & CIN_SILENT)
		{
			CIN_Skip(cin, cin->roqChunk.size);
			return;
		}

		byte compressed[0x20000];
		FS_Read(compressed, chunk->size, cin->file);
		cin->remaining -= chunk->size;

		if (chunk->id == RoQ_SOUND_MONO)
		{
			cin->sndChannels = 1;

			short l = chunk->argument;

			for (unsigned i = 0; i < chunk->size; i++)
			{
				l += cin->roqSndSqrTable[compressed[i]];
				((short *)&data)[i] = l;
			}

			samples = chunk->size;
		}
		else if (chunk->id == RoQ_SOUND_STEREO)
		{
			cin->sndChannels = 2;

			short l = chunk->argument & 0xff00;
			short r = (chunk->argument & 0xff) << 8;

			for (unsigned i = 0; i < chunk->size; i += 2)
			{
				l += cin->roqSndSqrTable[compressed[i + 0]];
				r += cin->roqSndSqrTable[compressed[i + 1]];

				((short *)&data)[i + 0] = l;
				((short *)&data)[i + 1] = r;
			}

			samples = chunk->size / 2;
		}
		else
		{
			//mxd. Silence "potentially uninitialized samples/data" PVS warnings.
			Com_Printf(S_COLOR_YELLOW"Unsupported chunk id: %i\n", chunk->id);
			CIN_Skip(cin, cin->roqChunk.size);

			return;
		}
	}

	// Send sound to mixer
	S_RawSamples(samples, cin->sndRate, cin->sndWidth, cin->sndChannels, data, Cvar_VariableValue("s_volume"));
}

static qboolean CIN_ReadNextFrame(cinematic_t *cin)
{
	if (!cin->isRoQ)
	{
		if (cin->remaining > 0)
		{
			int command;
			FS_Read(&command, sizeof(command), cin->file);
			cin->remaining -= sizeof(command);

			if (cin->remaining <= 0 || command == 2)
				return false; // Done

			if (command == 1)
				CIN_ReadPalette(cin);

			CIN_ReadVideoFrame(cin);
			CIN_ReadAudioFrame(cin);

			cin->frameCount++;
			cl.cinematicframe = cin->frameCount;

			return true;
		}

		return false;
	}

	roqChunk_t *chunk = &cin->roqChunk;

	while (cin->remaining > 0)
	{
		CIN_ReadChunk(cin);

		if (cin->remaining <= 0 || (int)chunk->size > cin->remaining)
			return false; // Done

		if (chunk->size == 0)
			continue;

		if (chunk->id == RoQ_INFO)
		{
			CIN_ReadInfo(cin);
		}
		else if (chunk->id == RoQ_QUAD_CODEBOOK)
		{
			CIN_ReadCodebook(cin);
		}
		else if (chunk->id == RoQ_QUAD_VQ)
		{
			CIN_ReadVideoFrame(cin);

			cin->frameCount++;
			cl.cinematicframe = cin->frameCount;
			return true;
		}
		else if (chunk->id == RoQ_SOUND_MONO || chunk->id == RoQ_SOUND_STEREO)
		{
			CIN_ReadAudioFrame(cin);
		}
		else
		{
			CIN_Skip(cin, cin->roqChunk.size);
		}
	}

	return false;
}

static qboolean CIN_StaticCinematic(cinematic_t *cin, const char *name)
{
	byte *buffer;
	byte *out;
	int runLength;
	byte palette[768];

	// Load the file
	const int len = FS_LoadFile((char *)name, (void **)&buffer);
	if (!buffer)
		return false;

	// Parse the PCX file
	pcx_t *pcx = (pcx_t *)buffer;
	byte *in = &pcx->data;

	if (pcx->manufacturer != 0x0A || pcx->version != 5 || pcx->encoding != 1)
	{
		FS_FreeFile(buffer);
		Com_Error(ERR_DROP, "CIN_StaticCinematic: invalid PCX header");
	}

	if (pcx->bits_per_pixel != 8 || pcx->color_planes != 1)
	{
		FS_FreeFile(buffer);
		Com_Error(ERR_DROP, "CIN_StaticCinematic: only 8 bit PCX images supported");
	}
		
	if (pcx->xmax >= 640 || pcx->ymax >= 480 || pcx->xmax <= 0 || pcx->ymax <= 0)
	{
		FS_FreeFile(buffer);
		Com_Error(ERR_DROP, "CIN_StaticCinematic: bad PCX file (%i x %i)", pcx->xmax, pcx->ymax);
	}

	memcpy(palette, (byte *)buffer + len - 768, 768);

	byte *pal = (byte *)cin->palette;
	for (int i = 0; i < 256; i++)
	{
		pal[i * 4 + 0] = palette[i * 3 + 0];
		pal[i * 4 + 1] = palette[i * 3 + 1];
		pal[i * 4 + 2] = palette[i * 3 + 2];
		pal[i * 4 + 3] = 255;
	}

	cin->vidWidth = pcx->xmax + 1;
	cin->vidHeight = pcx->ymax + 1;

	cin->pcxBuffer = out = Z_Malloc(cin->vidWidth * cin->vidHeight * 4);

	for (int y = 0; y <= pcx->ymax; y++)
	{
		for (int x = 0; x <= pcx->xmax; )
		{
			int dataByte = *in++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *in++;
			}
			else
			{
				runLength = 1;
			}

			while (runLength-- > 0)
			{
				*(unsigned *)out = cin->palette[dataByte];

				out += 4;
				x++;
			}
		}
	}

	if (in - buffer > len)
	{
		FS_FreeFile(buffer);
		Z_Free(cin->pcxBuffer);
		cin->pcxBuffer = NULL;
		Com_Error(ERR_DROP, "CIN_StaticCinematic: PCX file was malformed");
	}

	FS_FreeFile(buffer);

	cin->vidBuffer = cin->pcxBuffer;

	cin->frameCount = -1;
	cin->frameTime = cls.realtime;
	cl.cinematicframe = cin->frameCount;
	cl.cinematictime = cin->frameTime;

	cin->playing = true;

	return true;
}

static cinematic_t *CIN_HandleForVideo(cinHandle_t *handle)
{
	cinematic_t	*cin = cinematics;
	for (int i = 0; i < MAX_CINEMATICS; i++, cin++)
	{
		if (cin->playing)
			continue;

		*handle = i + 1;
		return cin;
	}

	Com_Error(ERR_DROP, "CIN_HandleForVideo: none free\n");
	return NULL;
}

static cinematic_t *CIN_GetVideoByHandle(cinHandle_t handle)
{
	if (handle <= 0 || handle > MAX_CINEMATICS)
		Com_Error(ERR_DROP, "CIN_GetVideoByHandle: out of range");

	return &cinematics[handle - 1];
}

static qboolean CIN_RunCinematic(cinHandle_t handle)
{
	cinematic_t *cin = CIN_GetVideoByHandle(handle);

	if (!cin->playing)
		return false; // Not running
	
	if (cin->frameCount == -1)
		return true; // Static image

	/*if (cls.key_dest != key_game)
	{
		// pause if menu or console is up
		cin->frameTime = cls.realtime - cin->frameTime * 1000 / 14;
		cl.cinematictime = cin->frameTime;
		return true;
	}*/

	const int frame = (cls.realtime - cin->frameTime) * cin->rate / 1000;
	if (frame <= cin->frameCount)
		return true;

	if (frame > cin->frameCount + 1)
		cin->frameTime = cls.realtime - cin->frameCount * 1000 / cin->rate;

	if (!CIN_ReadNextFrame(cin))
	{
		if (cin->flags & CIN_LOOPED)
		{
			// Restart the cinematic
			FS_Seek(cin->file, 0, FS_SEEK_SET);
			cin->remaining = cin->size;

			// Skip over the header
			CIN_Skip(cin, cin->start);

			cin->frameCount = 0;
			cin->frameTime = cls.realtime;
			cl.cinematicframe = cin->frameCount;
			cl.cinematictime = cin->frameTime;

			return true;
		}

		return false;	// Finished
	}

	return true;
}

static void CIN_SetExtents(cinHandle_t handle, int x, int y, int w, int h)
{
	cinematic_t *cin = CIN_GetVideoByHandle(handle);

	if (!cin->playing)
		return; // Not running

	float realx = (float)x;
	float realy = (float)y;
	float realw = (float)w;
	float realh = (float)h;

	SCR_AdjustFrom640(&realx, &realy, &realw, &realh, ALIGN_CENTER);

	cin->x = (int)realx;
	cin->y = (int)realy;
	cin->w = (int)realw;
	cin->h = (int)realh;
}

static void CIN_DrawCinematic(cinHandle_t handle)
{
	cinematic_t *cin = CIN_GetVideoByHandle(handle);

	if (!cin->playing) // Not running
		return;

	if (cin->frameCount == -1) // Knightmare- HACK to show JPG endscreens
	{
		char picname[MAX_QPATH] = "/";
		float x = 0;
		float y = 0;
		float w = 640;
		float h = 480;

		Q_strncatz(picname, cin->name, sizeof(picname));
		SCR_AdjustFrom640(&x, &y, &w, &h, ALIGN_CENTER);

		if (w < viddef.width || h < viddef.height)
			R_DrawFill(0, 0, viddef.width, viddef.height, 0, 0, 0, 255);

		R_DrawStretchPic((int)x, (int)y, (int)w, (int)h, picname, 1.0f);

		return;
	}

	if (cin->w < viddef.width || cin->h < viddef.height)
		R_DrawFill(0, 0, viddef.width, viddef.height, 0, 0, 0, 255);

	R_DrawStretchRaw(cin->x, cin->y, cin->w, cin->h, cin->vidBuffer, cin->vidWidth, cin->vidHeight);
}

static cinHandle_t CIN_PlayCinematic(const char *name, int x, int y, int w, int h, int flags)
{
	cinHandle_t	handle;

	// See if already playing this cinematic
	cinematic_t *cin = cinematics;
	for (int i = 0; i < MAX_CINEMATICS; i++, cin++)
	{
		if (!cin->playing)
			continue;

		if (!Q_stricmp(cin->name, (char *)name))
			return i + 1;
	}

	const char *ext = COM_FileExtension(name);

	if (!Q_stricmp(ext, "cin")) // RoQ autoreplace hack
	{
		char s[MAX_QPATH];
		const int len = strlen(name);
		Q_strncpyz(s, name, sizeof(s));

		s[len - 3] = 'r';
		s[len - 2] = 'o';
		s[len - 1] = 'q';

		handle = CIN_PlayCinematic(s, x, y , w, h, flags);
		if (handle)
			return handle;
	}

	// Find a free handle
	cin = CIN_HandleForVideo(&handle);

	// Fill it in
	Q_strncpyz(cin->name, name, sizeof(cin->name));

	float realx = (float)x;
	float realy = (float)y;
	float realw = (float)w;
	float realh = (float)h;

	SCR_AdjustFrom640(&realx, &realy, &realw, &realh, ALIGN_CENTER);

	cin->x = (int)realx;
	cin->y = (int)realy;
	cin->w = (int)realw;
	cin->h = (int)realh;
	cin->flags = flags;

	if (cin->flags & CIN_SYSTEM)
	{
		S_StopAllSounds(); // Make sure sound isn't playing
		UI_ForceMenuOff(); // Close the menu
	}

	if (!Q_stricmp(ext, "pcx"))
	{
		// Static PCX image
		if (!CIN_StaticCinematic(cin, name))
			return 0;

		return handle;
	}

	if (!Q_stricmp(ext, "roq"))
	{
		cin->isRoQ = true;
		cin->rate = 30;
	}
	else if (!Q_stricmp(ext, "cin"))
	{
		cin->isRoQ = false;
		cin->rate = 14;
	}
	else
	{
		return 0;
	}

	// Open the cinematic file
	cin->size = FS_FOpenFile(name, &cin->file, FS_READ);
	if (!cin->file)
		return 0;

	cin->remaining = cin->size;
	
	// Read the header
	if (!cin->isRoQ)
	{
		FS_Read(&cin->vidWidth, sizeof(cin->vidWidth), cin->file);
		FS_Read(&cin->vidHeight, sizeof(cin->vidHeight), cin->file);
		FS_Read(&cin->sndRate, sizeof(cin->sndRate), cin->file);
		FS_Read(&cin->sndWidth, sizeof(cin->sndWidth), cin->file);
		FS_Read(&cin->sndChannels, sizeof(cin->sndChannels), cin->file);

		cin->remaining -= 20;

		CIN_Huff1TableInit(cin);

		cin->start = FS_Tell(cin->file); //mxd. FS_FTell -> FS_Tell
		if (cin->start == -1) //mxd
			return 0;
	}
	else
	{
		cin->sndRate = 22050;
		cin->sndWidth = 2;

		CIN_ReadChunk(cin);
		CIN_SoundSqrTableInit(cin);

		cin->start = FS_Tell(cin->file); //mxd. FS_FTell -> FS_Tell
		if (cin->start == -1) //mxd
			return 0;
	}

	cin->frameCount = 0;
	cin->frameTime = cls.realtime;
	cl.cinematicframe = cin->frameCount;
	cl.cinematictime = cin->frameTime;

	cin->playing = true;

	// Read the first frame
	CIN_ReadNextFrame(cin);

	return handle;
}

static void CIN_StopCinematic(cinHandle_t handle)
{
	if (!handle)
		return;

	cinematic_t *cin = CIN_GetVideoByHandle(handle);

	if (!cin->playing)
		return;	// Not running

	if (cin->pcxBuffer)
		Z_Free(cin->pcxBuffer);

	if (cin->hNodes1)
		Z_Free(cin->hNodes1);

	if (cin->hBuffer)
		Z_Free(cin->hBuffer);

	if (cin->roqBuffer)
		Z_Free(cin->roqBuffer);

	if (cin->file)
		FS_FCloseFile(cin->file);

	memset(cin, 0, sizeof(*cin));
}

#pragma region ======================= Global playback functions 

void SCR_PlayCinematic(char *name)
{
	char filename[MAX_QPATH];

	// If currently playing another, stop it
	SCR_StopCinematic();

	Com_DPrintf("SCR_PlayCinematic( %s )\n", name);

	cl.cinematicframe = 0;
	if (!Q_stricmp(name + strlen(name) - 4, ".pcx"))
	{
		Q_strncpyz(filename, name, sizeof(filename));
		DefaultPath(filename, sizeof(filename), "pics");
		cl.cinematicframe = -1;
		cl.cinematictime = 1;
		SCR_EndLoadingPlaque();
		cls.state = ca_active;
	}
	else
	{
		Q_strncpyz(filename, name, sizeof(filename));
		DefaultPath(filename, sizeof(filename), "video");
		COM_DefaultExtension(filename, sizeof(filename), ".cin");
	}

	cls.cinematicHandle = CIN_PlayCinematic(filename, 0, 0, 640, 480, CIN_SYSTEM);
	if (!cls.cinematicHandle)
	{
		Com_Printf("Cinematic %s not found\n", filename);
		cl.cinematictime = 0; // done
		SCR_FinishCinematic();
	}
	else
	{
		SCR_EndLoadingPlaque();
		cls.state = ca_active;
		cl.cinematicframe = 0;
		cl.cinematictime = Sys_Milliseconds();
	}
}

void SCR_StopCinematic(void)
{
	if (!cls.cinematicHandle)
		return;

	Com_DPrintf("SCR_StopCinematic()\n");

	CIN_StopCinematic(cls.cinematicHandle);
	cls.cinematicHandle = 0;
}

// Called when either the cinematic completes, or it is aborted
void SCR_FinishCinematic(void)
{
	// Tell the server to advance to the next map / cinematic
	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
	SZ_Print(&cls.netchan.message, va("nextserver %i\n", cl.servercount));
}

qboolean SCR_DrawCinematic(void)
{
	if (cl.cinematictime <= 0 || !cls.cinematicHandle)
		return false;

	if (!CIN_RunCinematic(cls.cinematicHandle))
	{
		SCR_StopCinematic();
		SCR_FinishCinematic();

		return false;
	}

	CIN_SetExtents(cls.cinematicHandle, 0, 0, 640, 480);
	CIN_DrawCinematic(cls.cinematicHandle);

	return true;
}

#pragma endregion