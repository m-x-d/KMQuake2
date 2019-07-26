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

#include "qcommon.h"
#include "../include/minizip/unzip.h" //mxd. Use minizip + miniz combo instead of zlib

// Enables faster binary pak searck, still experimental
#define BINARY_PACK_SEARCH

/*
=============================================================================
	QUAKE FILESYSTEM
=============================================================================
*/

/*
All of Quake's data access is through a hierchal file system, but the
contents of the file system can be transparently merged from several
sources.

The "base directory" is the path to the directory holding the
quake.exe and all game directories.  The sys_* files pass this
to host_init in quakeparms_t->basedir.  This can be overridden
with the "+set game" command line parm to allow code debugging
in a different directory.  The base directory is only used
during filesystem initialization.

The "game directory" is the first tree on the search path and directory
that all generated files (savegames, screenshots, demos, config files)
will be saved to.  This can be overridden with the "-game" command line
parameter.  The game directory can never be changed while quake is
executing.  This is a precacution against having a malicious server
instruct clients to write files over areas they shouldn't.
*/

#define BASEDIRNAME		"baseq2"

#define MAX_HANDLES		32
#define MAX_READ		0x10000
#define MAX_WRITE		0x10000
#define MAX_FIND_FILES	0x04000

// Berserk's pk3 file support

typedef struct
{
	char name[MAX_QPATH];
	uint hash; // To speed up searching
	int size;
	int offset; // Ignored in PK3 files
	qboolean ignore; // Whether this file should be ignored
} fsPackFile_t;

typedef struct
{
	char name[MAX_QPATH];
	fsMode_t mode;
	FILE *file;		// Either file or
	unzFile *zip;	// zip will be used
	fsPackFile_t *pak; // Only used for seek/tell in .pak files
} fsHandle_t;

typedef struct fsLink_s
{
	char *from;
	int length;
	char *to;
	struct fsLink_s *next;
} fsLink_t;

typedef struct
{
	char name[MAX_OSPATH];
	FILE *pak;
	unzFile *pk3;
	int numFiles;
	fsPackFile_t *files;
	unsigned int contentFlags;
} fsPack_t;

typedef struct fsSearchPath_s
{
	char path[MAX_OSPATH];	// Only one of path or
	fsPack_t *pack;			// pack will be used
	struct fsSearchPath_s *next;
} fsSearchPath_t;


static fsHandle_t fs_handles[MAX_HANDLES];
static fsLink_t *fs_links;
static fsSearchPath_t *fs_searchPaths;
static fsSearchPath_t *fs_baseSearchPaths;

char fs_gamedir[MAX_OSPATH];
static char fs_currentGame[MAX_QPATH];

static char fs_fileInPath[MAX_OSPATH];
static qboolean fs_fileInPack;

// Set by FS_FOpenFile
qboolean file_from_pak;
char last_pk3_name[MAX_QPATH];

cvar_t *fs_homepath;
cvar_t *fs_basedir;
cvar_t *fs_basegamedir;
cvar_t *fs_basegamedir2;
cvar_t *fs_gamedirvar;
cvar_t *fs_debug;
cvar_t *fs_roguegame;

// Returns the path up to, but not including the last /
static void Com_FilePath(const char *path, char *dst, int dstSize)
{
	const char *last;

	const char *s = last = path + strlen(path);
	while (*s != '/' && *s != '\\' && s != path)
	{
		last = s - 1;
		s--;
	}

	Q_strncpyz(dst, path, dstSize);
	if (last-path < dstSize)
		dst[last-path] = 0;
}

// Returns bit flag based on pak item's extension.
static unsigned int FS_TypeFlagForPakItem(char *itemName)
{
	static const char *type_extensions[] =
	{
		"bsp", "md2", "md3", "sp2", "dm2", 
		"cin", "roq", 
		"wav", "ogg", "pcx", "wal", "tga", "jpg", "png",
		"cfg", "txt", "def", "alias", "arena", "script", "hud",
		0
	};
	
	for (int i = 0; type_extensions[i]; i++) 
		if (!Q_stricmp(COM_FileExtension(itemName), type_extensions[i]))
			return 1 << i;

	return 0;
}

static int FS_FileLength(FILE *f)
{
	const int cur = ftell(f);
	fseek(f, 0, SEEK_END);
	const int end = ftell(f);
	fseek(f, cur, SEEK_SET);

	return end;
}

// Creates any directories needed to store the given filename
void FS_CreatePath(char *path)
{
	FS_DPrintf("FS_CreatePath( %s )\n", path);

	if (strstr(path, "..") || strstr(path, "::") || strstr(path, "\\\\") || strstr(path, "//"))
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: refusing to create relative path '%s'\n", path);
		return;
	}

	for (char *ofs = path + 1; *ofs; ofs++)
	{
		if (*ofs == '/' || *ofs == '\\') // Q2E changed
		{
			// Create the directory
			*ofs = 0;
			Sys_Mkdir(path);
			*ofs = '/';
		}
	}
}

// Psychospaz's mod detector
qboolean FS_ModType(char *name)
{
	for (fsSearchPath_t *search = fs_searchPaths; search; search = search->next)
		if (strstr(search->path, name))
			return true;

	return false;
}

// This enables Rogue menu options for Q2MP4
qboolean FS_RoguePath()
{
	return (FS_ModType("rogue") || fs_roguegame->value);
}

void FS_DPrintf(const char *format, ...)
{
	static char msg[1024]; //mxd. +static
	va_list	argPtr;

	if (!fs_debug->value)
		return;

	va_start(argPtr, format);
	Q_vsnprintf(msg, sizeof(msg), format, argPtr);
	va_end(argPtr);

	Com_Printf("%s", msg);
}

// Called to find where to write a file (demos, savegames, etc...)
char *FS_Gamedir(void)
{
	return fs_gamedir;
}

// TODO: delete tree contents
void FS_DeletePath(char *path)
{
	FS_DPrintf("FS_DeletePath( %s )\n", path);
	Sys_Rmdir(path);
}

// Returns a fsHandle_t * for the given fileHandle_t
static fsHandle_t *FS_GetFileByHandle(fileHandle_t f)
{
	if (f <= 0 || f > MAX_HANDLES)
		Com_Error(ERR_DROP, "FS_GetFileByHandle: out of range");

	return &fs_handles[f - 1];
}

// Returns a FILE * for a fileHandle_t
FILE *FS_FileForHandle(fileHandle_t f)
{
	fsHandle_t *handle = FS_GetFileByHandle(f);

	if (handle->zip)
		Com_Error(ERR_DROP, "FS_FileForHandle: can't get FILE on zip file");

	if (!handle->file)
		Com_Error(ERR_DROP, "FS_FileForHandle: NULL");

	return handle->file;
}

// Finds a free fileHandle_t
static fsHandle_t *FS_HandleForFile(const char *path, fileHandle_t *f)
{
	fsHandle_t *handle = fs_handles;
	for (int i = 0; i < MAX_HANDLES; i++, handle++)
	{
		if (!handle->file && !handle->zip)
		{
			Q_strncpyz(handle->name, path, sizeof(handle->name));
			*f = i + 1;

			return handle;
		}
	}

	// Failed
	Com_Error(ERR_DROP, "FS_HandleForFile: none free");
	return NULL; //mxd. Silences intellisence warning.
}

#ifdef BINARY_PACK_SEARCH

// Performs a binary search by hashed filename to find pack items in a sorted pack
static int FS_FindPackItem(fsPack_t *pack, char *itemName, uint itemHash)
{
	// Catch null pointers
	if (!pack || !itemName)
		return -1;

	int smin = 0;
	int smax = pack->numFiles;

	while (smax - smin > 5)
	{
		const int smidpt = (smax + smin) / 2;

		if (pack->files[smidpt].hash > itemHash) // Before midpoint
			smax = smidpt;
		else if (pack->files[smidpt].hash < itemHash) // After midpoint
			smin = smidpt;
		else
			break;
	}

	for (int i = smin; i < smax; i++)
	{
		// Make sure this entry is not blacklisted & compare filenames
		if (pack->files[i].hash == itemHash && !pack->files[i].ignore && !Q_stricmp(pack->files[i].name, itemName))
			return i;
	}

	return -1;
}

#endif	// BINARY_PACK_SEARCH

// Returns file size or -1 on error
static int FS_FOpenFileAppend(fsHandle_t *handle)
{
	FS_CreatePath(handle->name);

	char path[MAX_OSPATH];
	Com_sprintf(path, sizeof(path), "%s/%s", fs_gamedir, handle->name);
	handle->file = fopen(path, "ab");

	if (handle->file)
	{
		if (fs_debug->value)
			Com_Printf("%s: %s\n", __func__, path);

		return FS_FileLength(handle->file);
	}

	if (fs_debug->value)
		Com_Printf("%s: couldn't open %s\n", __func__, path);

	return -1;
}

// Returns 0 on success, -1 on error
static int FS_FOpenFileWrite(fsHandle_t *handle)
{
	FS_CreatePath(handle->name);

	char path[MAX_OSPATH];
	Com_sprintf(path, sizeof(path), "%s/%s", fs_gamedir, handle->name);
	handle->file = fopen(path, "wb");

	if (handle->file)
	{
		if (fs_debug->value)
			Com_Printf("%s: %s\n", __func__, path);

		return 0;
	}

	if (fs_debug->value)
		Com_Printf("%s: couldn't open %s\n", __func__, path);

	return -1;
}

// Returns file size or -1 if not found.
// Can open separate files as well as files inside pack files (both PAK and PK3).
static int FS_FOpenFileRead(fsHandle_t *handle)
{
	// Knightmare- hack global vars for autodownloads
	file_from_pak = false;

	Com_sprintf(last_pk3_name, sizeof(last_pk3_name), "\0");
	const uint hash = Com_HashFileName(handle->name);
	const unsigned int typeFlag = FS_TypeFlagForPakItem(handle->name);

	// Search through the path, one element at a time
	for (fsSearchPath_t *search = fs_searchPaths; search; search = search->next)
	{
		if (search->pack)
		{
			// Search inside a pack file
			fsPack_t *pack = search->pack;

			// Skip if pack doesn't contain this type of file
			if (typeFlag != 0 && !(pack->contentFlags & typeFlag)) 
				continue;

#ifdef BINARY_PACK_SEARCH
			// Find index of pack item
			const int itemindex = FS_FindPackItem(pack, handle->name, hash);
			
			// found it!
			if (itemindex >= 0 && itemindex < pack->numFiles)
			{
#else
			for (int itemindex = 0; itemindex < pack->numFiles; itemindex++)
			{
				if (pack->files[itemindex].ignore)	// skip blacklisted files
					continue;
				if (hash != pack->files[itemindex].hash)	// compare hash first
					continue;
#endif	//	BINARY_PACK_SEARCH

				if (!Q_stricmp(pack->files[itemindex].name, handle->name)) //TODO: mxd. Remove. FS_FindPackItem already compares the name
				{
					// Found it!
					Com_FilePath(pack->name, fs_fileInPath, sizeof(fs_fileInPath));
					fs_fileInPack = true;

					if (fs_debug->value)
						Com_Printf("%s: found %s in %s\n", __func__, handle->name, pack->name);

					if (pack->pak)
					{
						// PAK
						file_from_pak = true; // Knightmare added
						handle->file = fopen(pack->name, "rb");
						handle->pak = &pack->files[itemindex]; // Set pakfile pointer
						if (handle->file)
						{
							fseek(handle->file, pack->files[itemindex].offset, SEEK_SET);

							return pack->files[itemindex].size;
						}
					}
					else if (pack->pk3)
					{
						// PK3
						file_from_pak = true; // Knightmare added //BUG: mxd. This was file_from_pk3 in KMQ2 and it was never used
						Com_sprintf(last_pk3_name, sizeof(last_pk3_name), strrchr(pack->name, '/') + 1); // Knightmare added
						handle->zip = unzOpen(pack->name);
						if (handle->zip)
						{
							if (unzLocateFile(handle->zip, handle->name, 2) == UNZ_OK && unzOpenCurrentFile(handle->zip) == UNZ_OK)
								return pack->files[itemindex].size;

							unzClose(handle->zip);
						}
					}

					Com_Error(ERR_FATAL, "%s: couldn't reopen %s", __func__, pack->name);
				}
				else
				{
					Com_Printf("%s: different filenames with identical hash (%s, %s)!\n", __func__, pack->files[itemindex].name, handle->name);
				}
			}
		}
		else
		{
			// Search in a directory tree
			char path[MAX_OSPATH];
			Com_sprintf(path, sizeof(path), "%s/%s", search->path, handle->name);

			handle->file = fopen(path, "rb");
			if (handle->file)
			{
				// Found it!
				Q_strncpyz(fs_fileInPath, search->path, sizeof(fs_fileInPath));
				fs_fileInPack = false;

				if (fs_debug->value)
					Com_Printf("%s: found %s in %s\n", __func__, handle->name, search->path);

				return FS_FileLength(handle->file);
			}
		}
	}

	// Not found!
	fs_fileInPath[0] = 0;
	fs_fileInPack = false;

	if (fs_debug->value)
		Com_Printf("%s: couldn't find %s\n", __func__, handle->name);

	return -1;
}

// Opens a file for "mode".
// Returns file size or -1 if an error occurs/not found.
// Can open separate files as well as files inside pack files (both PAK and PK3).
int FS_FOpenFile(const char *name, fileHandle_t *f, fsMode_t mode)
{
	int size = -1;
	fsHandle_t *handle = FS_HandleForFile(name, f);

	Q_strncpyz(handle->name, name, sizeof(handle->name));
	handle->mode = mode;

	switch (mode)
	{
		case FS_READ:	size = FS_FOpenFileRead(handle); break;
		case FS_WRITE:	size = FS_FOpenFileWrite(handle); break;
		case FS_APPEND: size = FS_FOpenFileAppend(handle); break;
		default: Com_Error(ERR_FATAL, "FS_FOpenFile: bad mode (%i)", mode);
	}

	if (size != -1)
		return size;

	// Couldn't open, so free the handle
	memset(handle, 0, sizeof(*handle));

	*f = 0;
	return -1;
}

void FS_FCloseFile(fileHandle_t f)
{
	fsHandle_t *handle = FS_GetFileByHandle(f);

	if (handle->file)
	{
		fclose(handle->file);
	}
	else if (handle->zip)
	{
		unzCloseCurrentFile(handle->zip);
		unzClose(handle->zip);
	}

	memset(handle, 0, sizeof(*handle));
}

// Properly handles partial reads
int FS_Read(void *buffer, int size, fileHandle_t f)
{
	fsHandle_t *handle = FS_GetFileByHandle(f);

	// Read
	int remaining = size;
	byte *buf = (byte *)buffer;

	while (remaining)
	{
		int r;
		if (handle->file)
			r = fread(buf, 1, remaining, handle->file);
		else if (handle->zip)
			r = unzReadCurrentFile(handle->zip, buf, remaining);
		else
			return 0;

		if (r == 0)
		{
			Com_DPrintf(S_COLOR_YELLOW"%s: 0 bytes read from %s\n", __func__, handle->name);
			return size - remaining;
		}

		if (r == -1)
			Com_Error(ERR_FATAL, "%s: -1 bytes read from %s", __func__, handle->name);

		remaining -= r;
		buf += r;
	}

	return size;
}

// Properly handles partial writes
int FS_Write(const void *buffer, int size, fileHandle_t f)
{
	fsHandle_t *handle = FS_GetFileByHandle(f);

	// Write
	int remaining = size;
	byte *buf = (byte *)buffer;

	while (remaining)
	{
		if (handle->file)
		{
			const int w = fwrite(buf, 1, remaining, handle->file);

			if (w == 0)
			{
				Com_Printf(S_COLOR_RED"%s: 0 bytes written to %s\n", __func__, handle->name);
				return size - remaining;
			}

			if (w == -1)
				Com_Error(ERR_FATAL, "%s: -1 bytes written to %s", __func__, handle->name);

			remaining -= w;
			buf += w;
		}
		else if (handle->zip)
		{
			Com_Error(ERR_FATAL, "%s: can't write to zip file %s", __func__, handle->name);
		}
		else
		{
			return 0;
		}
	}

	return size;
}

// Generates a listing of the contents of a pak file
char **FS_ListPak(char *find, int *num)
{
	int nfiles = 0;
	int nfound = 0;

	// Check pak files
	for (fsSearchPath_t	*search = fs_searchPaths; search; search = search->next)
	{
		if (!search->pack)
			continue;

		fsPack_t *pak = search->pack;

		// Find and build list
		for (int i = 0; i < pak->numFiles; i++)
			if (!pak->files[i].ignore)
				nfiles++;
	}

	//mxd. Paranoia check...
	if (nfiles == 0)
	{
		Com_Printf(S_COLOR_YELLOW"No usable files found in PAK files!\n");
		return NULL;
	}

	char **list = malloc(sizeof(char *) * nfiles);
	memset(list, 0, sizeof(char *) * nfiles);

	for (fsSearchPath_t	*search = fs_searchPaths; search; search = search->next)
	{
		if (!search->pack)
			continue;

		fsPack_t *pak = search->pack;

		// Find and build list
		for (int i = 0; i < pak->numFiles; i++)
		{
			if (!pak->files[i].ignore && strstr(pak->files[i].name, find))
			{
				list[nfound] = strdup(pak->files[i].name);
				nfound++;
			}
		}
	}
	
	*num = nfound;

	return list;
}

void FS_Seek(fileHandle_t f, int offset, fsOrigin_t origin)
{
	static byte dummy[0x8000]; //mxd. +static
	fsHandle_t *handle = FS_GetFileByHandle(f);

	if (handle->pak) // Inside .pak file uses offset/size
	{
		switch (origin)
		{
			case FS_SEEK_SET: fseek(handle->file, handle->pak->offset + offset, SEEK_SET); break;
			case FS_SEEK_CUR: fseek(handle->file, offset, SEEK_CUR); break;
			case FS_SEEK_END: fseek(handle->file, handle->pak->offset + handle->pak->size, SEEK_SET); break;
			default: Com_Error(ERR_FATAL, "FS_Seek: bad origin (%i)", origin); break;
		}
	}
	else if (handle->file)
	{
		switch (origin)
		{
			case FS_SEEK_SET: fseek(handle->file, offset, SEEK_SET); break;
			case FS_SEEK_CUR: fseek(handle->file, offset, SEEK_CUR); break;
			case FS_SEEK_END: fseek(handle->file, offset, SEEK_END); break;
			default: Com_Error(ERR_FATAL, "FS_Seek: bad origin (%i)", origin); break;
		}
	}
	else if (handle->zip)
	{
		int remaining;
		
		switch (origin)
		{
			case FS_SEEK_SET:
				remaining = offset;
				break;

			case FS_SEEK_CUR:
				remaining = offset + unztell(handle->zip);
				break;

			case FS_SEEK_END:
			{
				unz_file_info info;
				unzGetCurrentFileInfo(handle->zip, &info, NULL, 0, NULL, 0, NULL, 0);
				remaining = offset + info.uncompressed_size;
			} break;

			default:
				Com_Error(ERR_FATAL, "FS_Seek: bad origin (%i)", origin);
				remaining = 0;
				break;
		}

		// Reopen the file
		unzCloseCurrentFile(handle->zip);
		unzOpenCurrentFile(handle->zip);

		// Skip until the desired offset is reached
		while (remaining)
		{
			const int len = min((int)sizeof(dummy), remaining);
			const int r = unzReadCurrentFile(handle->zip, dummy, len);
			if (r < 1)
				break;

			remaining -= r;
		}
	}
}

// Returns -1 if an error occurs
int FS_Tell(fileHandle_t f)
{
	fsHandle_t *handle = FS_GetFileByHandle(f);

	if (handle->pak)
	{
		// Inside .pak file uses offset/size
		long pos = ftell(handle->file);
		if (pos != -1)
			pos -= handle->pak->offset;

		return pos;
	}

	if (handle->file)
		return ftell(handle->file);

	if (handle->zip)
		return unztell(handle->zip);

	return -1; //mxd. Let's actually return -1 :)
}

// Returns true when file exists and has non-zero size
qboolean FS_FileExists(const char *path)
{
	fileHandle_t f;
	const int size = FS_FOpenFile(path, &f, FS_READ);

	if (f)
	{
		FS_FCloseFile(f);
		return (size > 0); //mxd. Was return true
	}

	return false;
}

qboolean FS_LocalFileExists(char *path)
{
	char realPath[MAX_OSPATH];
	Com_sprintf(realPath, sizeof(realPath), "%s/%s", FS_Gamedir(), path);
	FILE *f = fopen(realPath, "rb");
	
	if (f)
	{
		fclose(f);
		return true;
	}

	return false;
}

void FS_CopyFile(const char *srcPath, const char *dstPath)
{
	Com_DPrintf("%s(%s, %s)\n", __func__, srcPath, dstPath);

	FILE *f1 = fopen(srcPath, "rb");
	if (!f1)
		return;

	FILE *f2 = fopen(dstPath, "wb");
	if (!f2)
	{
		fclose(f1);
		return;
	}

	byte buffer[65536];
	while (true)
	{
		const int len = fread(buffer, 1, sizeof(buffer), f1);
		if (!len)
			break;

		fwrite(buffer, 1, len, f2);
	}

	fclose(f1);
	fclose(f2);
}

void FS_RenameFile(const char *oldPath, const char *newPath)
{
	FS_DPrintf("FS_RenameFile( %s, %s )\n", oldPath, newPath);

	if (rename(oldPath, newPath))
		FS_DPrintf("FS_RenameFile: failed to rename %s to %s\n", oldPath, newPath);
}

void FS_DeleteFile(const char *path)
{
	FS_DPrintf("FS_DeleteFile( %s )\n", path);

	if (remove(path))
		FS_DPrintf("FS_DeleteFile: failed to delete %s\n", path);
}

// "path" is relative to the Quake search path.
// Returns file size or -1 if the file is not found.
// A NULL buffer will just return the file size without loading.
int FS_LoadFile(const char *path, void **buffer)
{
	fileHandle_t f;
	const int size = FS_FOpenFile(path, &f, FS_READ);

	if (size <= 0)
	{
		if (buffer)
			*buffer = NULL;

		return size;
	}

	if (!buffer)
	{
		FS_FCloseFile(f);
		return size;
	}

	byte *buf = Z_Malloc(size);
	*buffer = buf;

	FS_Read(buf, size, f);
	FS_FCloseFile(f);

	return size;
}

void FS_FreeFile(void *buffer)
{
	if (!buffer)
	{
		FS_DPrintf("FS_FreeFile: NULL buffer\n");
		return;
	}

	Z_Free(buffer);
}

// Checks against a blacklist to see if a file should not be loaded from a pak.
static qboolean FS_FileInPakBlacklist(char *filename)
{
	// Some incompetently packaged mods have these files in their paks!
	static char *pakfile_ignore_names[] =
	{
		"save/",
		"scrnshot/",
		"screenshots/", //mxd
		"autoexec.cfg",
		"kmq2config.cfg",
		0
	};

	char *compare = filename;
	if (compare[0] == '/') // Remove leading slash
		compare++;

	for (int i = 0; pakfile_ignore_names[i]; i++)
		if (!Q_strncasecmp(compare, pakfile_ignore_names[i], strlen(pakfile_ignore_names[i])))
			return true;

	return false;
}

#ifdef BINARY_PACK_SEARCH

// Used for sorting pak entries by hash 
static uint *nameHashes = NULL;

static int FS_PakFileCompare(const int *f1, const int *f2)
{
	if (!nameHashes)
		Com_Error(ERR_FATAL, "FS_PakFileCompare: no name hashes!"); //mxd

	if (nameHashes[*f1] > nameHashes[*f2])
		return 1;

	if (nameHashes[*f1] < nameHashes[*f2])
		return -1;

	return 0;
}

#endif

// Takes an explicit (not game tree related) path to a pack file.
// Loads the header and directory, adding the files at the beginning of the list so they override previous pack files.
static fsPack_t *FS_LoadPAK(const char *packPath)
{
	FILE *handle = fopen(packPath, "rb");
	if (!handle)
		return NULL;

	dpackheader_t header;
	fread(&header, 1, sizeof(dpackheader_t), handle);
	
	if (header.ident != IDPAKHEADER)
	{
		fclose(handle);
		Com_Printf(S_COLOR_YELLOW"%s: '%s' is not a valid pack file.\n", __func__, packPath);

		return NULL; //mxd. Handle gracefully instead of throwing an error
	}

	const int numFiles = header.dirlen / sizeof(dpackfile_t);
	if (numFiles == 0)
	{
		fclose(handle);
		Com_Printf(S_COLOR_YELLOW"%s: '%s' has no files.\n", __func__, packPath);

		return NULL; //mxd. Handle gracefully instead of throwing an error
	}

	fsPackFile_t *files = Z_Malloc(numFiles * sizeof(fsPackFile_t));
	dpackfile_t *info = Z_Malloc(numFiles * sizeof(dpackfile_t)); //mxd

	fseek(handle, header.dirofs, SEEK_SET);
	fread(info, 1, header.dirlen, handle);

#ifdef BINARY_PACK_SEARCH

	// Create sort table
	int *sortIndices = Z_Malloc(numFiles * sizeof(int));
	uint *sortHashes = Z_Malloc(numFiles * sizeof(uint));
	nameHashes = sortHashes;

	for (int i = 0; i < numFiles; i++)
	{
		sortIndices[i] = i;
		sortHashes[i] = Com_HashFileName(info[i].name);
	}

	qsort((void *)sortIndices, numFiles, sizeof(int), (int(*)(const void *, const void *))FS_PakFileCompare);

	// Parse the directory
	uint contentFlags = 0;
	for (int i = 0; i < numFiles; i++)
	{
		Q_strncpyz(files[i].name, info[sortIndices[i]].name, sizeof(files[i].name));
		files[i].hash = sortHashes[sortIndices[i]];
		files[i].offset = info[sortIndices[i]].filepos;
		files[i].size = info[sortIndices[i]].filelen;
		files[i].ignore = FS_FileInPakBlacklist(files[i].name); // Check against pak loading blacklist

		if (!files[i].ignore) // Add type flag for this file
			contentFlags |= FS_TypeFlagForPakItem(files[i].name);
	}

	// Free sort table
	Z_Free(sortIndices);
	Z_Free(sortHashes);
	nameHashes = NULL;

#else	// Parse the directory

	for (int i = 0; i < numFiles; i++)
	{
		Q_strncpyz(files[i].name, info[i].name, sizeof(files[i].name));
		files[i].hash = Com_HashFileName(info[i].name, 0, false);	// Added to speed up seaching
		files[i].offset = LittleLong(info[i].filepos);
		files[i].size = LittleLong(info[i].filelen);
		files[i].ignore = FS_FileInPakBlacklist(info[i].name, false); // Check against pak loading blacklist

		if (!files[i].ignore)	// add type flag for this file
			contentFlags |= FS_TypeFlagForPakItem(files[i].name);
	}

#endif	// BINARY_PACK_SEARCH

	//mxd. Free info
	Z_Free(info);

	fsPack_t *pack = Z_Malloc(sizeof(fsPack_t));
	Q_strncpyz(pack->name, packPath, sizeof(pack->name));
	pack->pak = handle;
	pack->pk3 = NULL;
	pack->numFiles = numFiles;
	pack->files = files;
	pack->contentFlags = contentFlags;

	return pack;
}

// Adds a Pak file to the searchpath
void FS_AddPAKFile(const char *packPath)
{
	fsPack_t *pack = FS_LoadPAK(packPath);
	if (!pack)
		return;

	fsSearchPath_t *search = Z_Malloc(sizeof(fsSearchPath_t));
	search->pack = pack;
	search->next = fs_searchPaths;
	fs_searchPaths = search;
}

// Takes an explicit (not game tree related) path to a pack file.
// Loads the header and directory, adding the files at the beginning of the list so they override previous pack files.
static fsPack_t *FS_LoadPK3(const char *packPath)
{
	unz_global_info global;
	unz_file_info info;
	char fileName[MAX_QPATH];

	unzFile *handle = unzOpen(packPath);
	if (!handle)
		return NULL;

	if (unzGetGlobalInfo(handle, &global) != UNZ_OK)
	{
		unzClose(handle);
		Com_Printf(S_COLOR_YELLOW"%s: '%s' is not a valid pk3 file.\n", __func__, packPath);

		return NULL; //mxd. Handle gracefully instead of throwing an error
	}

	const int numFiles = global.number_entry;
	if (numFiles == 0)
	{
		unzClose(handle);
		Com_Printf(S_COLOR_YELLOW"%s: '%s' has no files.\n", __func__, packPath);

		return NULL; //mxd. Handle gracefully instead of throwing an error
	}

	fsPackFile_t *files = Z_Malloc(numFiles * sizeof(fsPackFile_t));

#ifdef BINARY_PACK_SEARCH

	// Create sort table
	fsPackFile_t *tmpFiles = Z_Malloc(numFiles * sizeof(fsPackFile_t));
	int *sortIndices = Z_Malloc(numFiles * sizeof(int));
	uint *sortHashes = Z_Malloc(numFiles * sizeof(uint));
	nameHashes = sortHashes;

	// Parse the directory
	int index = 0;
	uint contentFlags = 0;
	int status = unzGoToFirstFile(handle);
	while (status == UNZ_OK)
	{
		fileName[0] = 0;
		unzGetCurrentFileInfo(handle, &info, fileName, MAX_QPATH, NULL, 0, NULL, 0);
		sortIndices[index] = index;
		Q_strncpyz(tmpFiles[index].name, fileName, sizeof(tmpFiles[index].name));

		tmpFiles[index].hash = sortHashes[index] = Com_HashFileName(fileName); // Added to speed up seaching
		tmpFiles[index].offset = -1; // Not used in ZIP files
		tmpFiles[index].size = info.uncompressed_size;
		tmpFiles[index].ignore = FS_FileInPakBlacklist(fileName); // Check against pak loading blacklist

		if (!tmpFiles[index].ignore) // Add type flag for this file
			contentFlags |= FS_TypeFlagForPakItem(tmpFiles[index].name);

		index++;
		status = unzGoToNextFile(handle);
	}

	// Sort by hash and copy to final file table
	qsort((void *)sortIndices, numFiles, sizeof(int), (int(*)(const void *, const void *))FS_PakFileCompare);
	for (int i = 0; i < numFiles; i++)
	{
		Q_strncpyz(files[i].name, tmpFiles[sortIndices[i]].name, sizeof(files[i].name));
		files[i].hash = tmpFiles[sortIndices[i]].hash;
		files[i].offset = tmpFiles[sortIndices[i]].offset;
		files[i].size = tmpFiles[sortIndices[i]].size;
		files[i].ignore = tmpFiles[sortIndices[i]].ignore;
	}

	// Free sort table
	Z_Free(tmpFiles);
	Z_Free(sortIndices);
	Z_Free(sortHashes);
	nameHashes = NULL;

#else	// Parse the directory

	int index = 0;
	int status = unzGoToFirstFile(handle);
	while (status == UNZ_OK)
	{
		fileName[0] = 0;
		unzGetCurrentFileInfo(handle, &info, fileName, MAX_QPATH, NULL, 0, NULL, 0);

		Q_strncpyz(files[index].name, fileName, sizeof(files[index].name);
		files[index].hash = Com_HashFileName(fileName, 0, false);	// Added to speed up seaching
		files[index].offset = -1;		// Not used in ZIP files
		files[index].size = info.uncompressed_size;
		files[index].ignore = FS_FileInPakBlacklist(fileName, true);	// check against pak loading blacklist

		if (!files[index].ignore)	// add type flag for this file
			contentFlags |= FS_TypeFlagForPakItem(files[index].name);

		index++;

		status = unzGoToNextFile(handle);
	}

#endif	// BINARY_PACK_SEARCH

	fsPack_t *pack = Z_Malloc(sizeof(fsPack_t));
	Q_strncpyz(pack->name, packPath, sizeof(pack->name));
	pack->pak = NULL;
	pack->pk3 = handle;
	pack->numFiles = numFiles;
	pack->files = files;
	pack->contentFlags = contentFlags;

	return pack;
}

// Adds a Pk3 file to the searchpath
void FS_AddPK3File(const char *packPath)
{
	fsPack_t *pack = FS_LoadPK3(packPath);
	if (!pack)
		return;

	fsSearchPath_t *search = Z_Malloc(sizeof(fsSearchPath_t));
	search->pack = pack;
	search->next = fs_searchPaths;
	fs_searchPaths = search;
}

// Sets fs_gameDir, adds the directory to the head of the path, then loads and adds all the pack files found (in alphabetical order).
static void FS_AddGameDirectory(const char *dir)
{
	char packpath[MAX_OSPATH];

	Q_strncpyz(fs_gamedir, dir, sizeof(fs_gamedir));

	// Add pak files in the pak0.pak, pak1.pak... format
	for (int i = 0; i < 100; i++) // Pooy - paks can now go up to 100
	{
		Com_sprintf(packpath, sizeof(packpath), "%s/pak%i.pak", dir, i);
		FS_AddPAKFile(packpath);
	}

	//NeVo. Add pk3 files in the pak0.pk3, pak1.pk3... format 
	for (int i = 0; i < 100; i++) // Pooy - paks can now go up to 100
	{
		Com_sprintf(packpath, sizeof(packpath), "%s/pak%i.pk3", dir, i);
		FS_AddPK3File(packpath);
	}

	// Add pak files not using pak0.pak, pak1.pak... format, then add pk3 files not using pak0.pk3, pak1.pk3... format
	for (int ispak = 0; ispak < 2; ispak++)
	{
		const char *ext = (ispak ?  "pak" : "pk3");

		// Set search mask
		char findname[1024];
		Com_sprintf(findname, sizeof(findname), "%s/*.%s", dir, ext);

		// Use correct slashes...
		char *tmp = findname;
		while (*tmp != 0)
		{
			if (*tmp == '\\')
				*tmp = '/';

			tmp++;
		}

		// Find all .pak/.pk3 files
		int numfiles;
		char **filenames = FS_ListFiles(findname, &numfiles, 0, 0);
		if (filenames == NULL)
			continue;

		// Add the ones, which don't match pakX naming sceme
		for (int i = 0; i < numfiles; i++)
		{
			qboolean numberedpak = false;

			// Skip packs using pakX naming scheme...
			for (int j = 0; j < 100; j++)
			{
				char buf[16];
				Com_sprintf(buf, sizeof(buf), "/pak%i.%s", j, ext);

				if (strstr(filenames[i], buf))
				{
					numberedpak = true;
					break;
				}
			}

			if (numberedpak)
				continue;

			if (strrchr(filenames[i], '/'))
			{
				if (ispak)
					FS_AddPAKFile(filenames[i]);
				else
					FS_AddPK3File(filenames[i]);
			}

			free(filenames[i]);
		}

		free(filenames);
	}

	// Add the directory to the search path //mxd. Add last, so it's checked first
	fsSearchPath_t *search = Z_Malloc(sizeof(fsSearchPath_t));
	Q_strncpyz(search->path, dir, sizeof(search->path));
	search->next = fs_searchPaths;
	fs_searchPaths = search;
}

// Allows enumerating all of the directories in the search path
char *FS_NextPath(const char *prevpath)
{
	char *prev = NULL;
	for (fsSearchPath_t *search = fs_searchPaths; search; search = search->next)
	{
		if (search->pack)
			continue;

		if (prevpath == prev)
			return search->path;

		prev = search->path;
	}

	return NULL;
}

static void FS_Path_f(void)
{
	int totalFiles = 0;

	Com_Printf("Current search path:\n");

	for (fsSearchPath_t *search = fs_searchPaths; search; search = search->next)
	{
		if (search->pack)
		{
			Com_Printf("%s (%i files)\n", search->pack->name, search->pack->numFiles);
			totalFiles += search->pack->numFiles;
		}
		else
		{
			Com_Printf("%s\n", search->path);
		}
	}

	fsHandle_t *handle = fs_handles;
	for (int i = 0; i < MAX_HANDLES; i++, handle++)
		if (handle->file || handle->zip)
			Com_Printf("Handle %i: %s\n", i + 1, handle->name);

	fsLink_t *link = fs_links;
	for (int i = 0; link; i++, link = link->next)
		Com_Printf("Link %i: %s -> %s\n", i, link->from, link->to);

	Com_Printf("-------------------------------------\n");

	Com_Printf("%i files in PAK/PK3 files\n\n", totalFiles);
}

// Creates a filelink_t
static void FS_Link_f(void)
{
	if (Cmd_Argc() != 3)
	{
		Com_Printf("Usage: link <from> <to>\n");
		return;
	}

	// See if the link already exists
	fsLink_t **prev = &fs_links;
	for (fsLink_t *l = fs_links; l; l = l->next)
	{
		if (!strcmp(l->from, Cmd_Argv(1)))
		{
			Z_Free(l->to);
			if (!strlen(Cmd_Argv(2)))
			{
				// Delete it
				*prev = l->next;
				Z_Free(l->from);
				Z_Free(l);

				return;
			}

			l->to = CopyString(Cmd_Argv(2));
			return;
		}

		prev = &l->next;
	}

	// Create a new link
	fsLink_t *newlink = Z_Malloc(sizeof(*newlink));
	newlink->next = fs_links;
	fs_links = newlink;
	newlink->from = CopyString(Cmd_Argv(1));
	newlink->length = strlen(newlink->from);
	newlink->to = CopyString(Cmd_Argv(2));
}

// TODO: close open files for game dir
void FS_Startup(void)
{
	if (!fs_gamedirvar->string[0]
		|| strchr(fs_gamedirvar->string, '.')
		|| strchr(fs_gamedirvar->string, '/')
		|| strchr(fs_gamedirvar->string, '\\')
		|| strchr(fs_gamedirvar->string, ':')) //mxd. strstr -> strchr
	{
		Cvar_ForceSet("game", BASEDIRNAME);
	}

	// Check for game override
	if (Q_stricmp(fs_gamedirvar->string, fs_currentGame))
	{
		// Free up any current game dir info
		while (fs_searchPaths != fs_baseSearchPaths)
		{
			if (fs_searchPaths->pack)
			{
				fsPack_t *pack = fs_searchPaths->pack;

				if (pack->pak)
					fclose(pack->pak);

				if (pack->pk3)
					unzClose(pack->pk3);

				Z_Free(pack->files);
				Z_Free(pack);
			}

			fsSearchPath_t *next = fs_searchPaths->next;
			Z_Free(fs_searchPaths);
			fs_searchPaths = next;
		}

		if (!Q_stricmp(fs_gamedirvar->string, BASEDIRNAME)) // Don't add baseq2 again
			Q_strncpyz(fs_gamedir, fs_basedir->string, sizeof(fs_gamedir));
		else
			FS_AddGameDirectory(va("%s/%s", fs_homepath->string, fs_gamedirvar->string)); // Add the directories
	}

	Q_strncpyz(fs_currentGame, fs_gamedirvar->string, sizeof(fs_currentGame));
	FS_Path_f();
}

extern char *Sys_GetCurrentDirectory(void);

void FS_InitFilesystem(void)
{
	// Register our commands and cvars
	Cmd_AddCommand("path", FS_Path_f);
	Cmd_AddCommand("link", FS_Link_f);
	Cmd_AddCommand("dir", FS_Dir_f);

	Com_Printf("\n----- Filesystem Initialization -----\n");

	// basedir <path>
	// Allows the game to run from outside the data tree
	fs_basedir = Cvar_Get("basedir", ".", CVAR_NOSET);

	// Start up with baseq2 by default
	FS_AddGameDirectory(va("%s/"BASEDIRNAME, fs_basedir->string));

	// Any set gamedirs will be freed up to here
	fs_baseSearchPaths = fs_searchPaths;

	Q_strncpyz(fs_currentGame, BASEDIRNAME, sizeof(fs_currentGame));

	// Check for game override
	fs_homepath = Cvar_Get("homepath", Sys_GetCurrentDirectory(), CVAR_NOSET);
	fs_debug = Cvar_Get("fs_debug", "0", 0);
	fs_roguegame = Cvar_Get("roguegame", "0", CVAR_LATCH);
	fs_basegamedir = Cvar_Get("basegame", "", CVAR_LATCH);
	fs_basegamedir2 = Cvar_Get("basegame2", "", CVAR_LATCH);
	fs_gamedirvar = Cvar_Get("game", "", CVAR_LATCH|CVAR_SERVERINFO);

	// Check and load game directory
	if (fs_gamedirvar->string[0])
		FS_SetGamedir(fs_gamedirvar->string);

	FS_Path_f(); // output path data
}

void FS_Shutdown(void)
{
	Cmd_RemoveCommand("dir");
	Cmd_RemoveCommand("link");
	Cmd_RemoveCommand("path");

	// Close all files
	fsHandle_t *handle = fs_handles;
	for (int i = 0; i < MAX_HANDLES; i++, handle++)
	{
		if (handle->file)
			fclose(handle->file);

		if (handle->zip)
		{
			unzCloseCurrentFile(handle->zip);
			unzClose(handle->zip);
		}
	}

	// Free the search paths
	while (fs_searchPaths)
	{
		if (fs_searchPaths->pack)
		{
			fsPack_t *pack = fs_searchPaths->pack;

			if (pack->pak)
				fclose(pack->pak);
			if (pack->pk3)
				unzClose(pack->pk3);

			Z_Free(pack->files);
			Z_Free(pack);
		}

		fsSearchPath_t *next = fs_searchPaths->next;
		Z_Free(fs_searchPaths);
		fs_searchPaths = next;
	}
}

// Sets the gamedir and path to a different directory.
void FS_SetGamedir(char *dir)
{
	qboolean basegame1_loaded = false;

	if (strstr(dir, "..") || strchr(dir, '/') || strchr(dir, '\\') || strchr(dir, ':')) //mxd. strstr -> strchr
	{
		Com_Printf("Gamedir should be a single filename, not a path\n");
		return;
	}

	// Knightmare- check basegame var
	if (fs_basegamedir->string[0])
	{
		if (strstr(fs_basegamedir->string, "..") || strchr(fs_basegamedir->string, '/') || strchr(fs_basegamedir->string, '\\') || strchr(fs_basegamedir->string, ':')) //mxd. strstr -> strchr
		{
			Cvar_Set("basegame", "");
			Com_Printf("Basegame should be a single filename, not a path\n");
		}

		if (!Q_stricmp(fs_basegamedir->string, BASEDIRNAME) || !Q_stricmp(fs_basegamedir->string, dir))
		{
			Cvar_Set("basegame", "");
			Com_Printf("Basegame should not be the same as "BASEDIRNAME" or gamedir.\n");
		}
	}

	// Knightmare- check basegame2 var
	if (fs_basegamedir2->string[0])
	{
		if (strstr(fs_basegamedir2->string, "..") || strchr(fs_basegamedir2->string, '/') || strchr(fs_basegamedir2->string, '\\') || strchr(fs_basegamedir2->string, ':')) //mxd. strstr -> strchr
		{
			Cvar_Set("basegame2", "");
			Com_Printf("Basegame2 should be a single filename, not a path\n");
		}

		if (!Q_stricmp(fs_basegamedir2->string, BASEDIRNAME) || !Q_stricmp(fs_basegamedir2->string, dir) || !Q_stricmp(fs_basegamedir2->string, fs_basegamedir->string))
		{
			Cvar_Set("basegame2", "");
			Com_Printf("Basegame2 should not be the same as "BASEDIRNAME", gamedir, or basegame.\n");
		}
	}

	// Free up any current game dir info
	while (fs_searchPaths != fs_baseSearchPaths)
	{
		if (fs_searchPaths->pack)
		{
			if (fs_searchPaths->pack->pak)
				fclose(fs_searchPaths->pack->pak);

			if (fs_searchPaths->pack->pk3)
				unzClose(fs_searchPaths->pack->pk3);

			Z_Free(fs_searchPaths->pack->files);
			Z_Free(fs_searchPaths->pack);
		}

		fsSearchPath_t *next = fs_searchPaths->next;
		Z_Free(fs_searchPaths);
		fs_searchPaths = next;
	}

	// Flush all data, so it will be forced to reload
	if (dedicated && !dedicated->value)
		Cbuf_AddText("vid_restart\nsnd_restart\n");

	if (*dir == 0)	// Knightmare- set to basedir if a blank dir is passed
		Com_sprintf(fs_gamedir, sizeof(fs_gamedir), "%s/"BASEDIRNAME, fs_basedir->string);
	else
		Com_sprintf(fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basedir->string, dir);

	if (!strcmp(dir, BASEDIRNAME) || *dir == 0)
	{
		Cvar_FullSet("gamedir", "", CVAR_SERVERINFO | CVAR_NOSET);
		Cvar_FullSet("game", "", CVAR_LATCH | CVAR_SERVERINFO);
	}
	else
	{
		// Check and load base game directory (so mods can be based upon other mods)
		if (fs_basegamedir->string[0])
		{
			FS_AddGameDirectory(va("%s/%s", fs_basedir->string, fs_basegamedir->string));
			basegame1_loaded = true;
		}

		// Second basegame so mods can utilize both Rogue and Xatrix assets
		if (basegame1_loaded && fs_basegamedir2->string[0])
			FS_AddGameDirectory(va("%s/%s", fs_basedir->string, fs_basegamedir2->string));

		Cvar_FullSet("gamedir", dir, CVAR_SERVERINFO | CVAR_NOSET);
		FS_AddGameDirectory(va("%s/%s", fs_basedir->string, dir));
	}
}

void FS_ExecAutoexec()
{
	char *dir = Cvar_VariableString("gamedir");
	if (!*dir)
		dir = BASEDIRNAME;

	char name[MAX_QPATH];
	Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, dir); 

	if (Sys_FindFirst(name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM))
		Cbuf_AddText("exec autoexec.cfg\n");

	Sys_FindClose();
}

//mxd. Returns number of matching files, not number of matching files + 1, like in Vanilla!
char **FS_ListFiles(char *findname, int *numfiles, unsigned musthave, unsigned canthave)
{
	int nfiles = 0;
	
	// Count number of matching files
	char *filename = Sys_FindFirst(findname, musthave, canthave);
	while (filename != NULL)
	{
		// Sys_FindFirst / Sys_FindNext can return a relative path to current ("findname\.") or parent ("findname\..") directory. Skip those.
		if (filename[strlen(filename) - 1] != '.') 
			nfiles++;

		filename = Sys_FindNext(musthave, canthave);
	}
	Sys_FindClose();

	// Store number of matching files
	*numfiles = nfiles;

	if (nfiles == 0)
		return NULL;

	// Allocate list
	const size_t listsize = sizeof(char *) * nfiles;
	char **list = malloc(listsize);
	memset(list, 0, listsize);

	// Fill list with matching filenames
	filename = Sys_FindFirst(findname, musthave, canthave);
	for (int i = 0; filename != NULL;)
	{
		if (filename[strlen(filename) - 1] != '.')
		{
			list[i] = strdup(filename);
#ifdef _WIN32
			strlwr(list[i]);
#endif
			i++;
		}

		filename = Sys_FindNext(musthave, canthave);
	}
	Sys_FindClose();

	// Return result
	return list;
}

void FS_FreeFileList(char **list, const int n)
{
	for (int i = 0; i < n; i++)
	{
		if (list && list[i])
		{
			free(list[i]);
			list[i] = 0;
		}
	}

	free(list);
}

qboolean FS_ItemInList(const char *check, const int num, const char **list)
{
	for (int i = 0; i < num; i++)
		if (!Q_strcasecmp(check, list[i]))
			return true;

	return false;
}

void FS_InsertInList(char **list, const char *insert, const int len, const int start)
{
	if (!list)
		return;

	for (int i = start; i < len; i++)
	{
		if (!list[i])
		{
			list[i] = strdup(insert);
			return;
		}
	}

	list[len] = strdup(insert);
}

static void FS_Dir_f()
{
	char *path = NULL;
	char findname[1024];
	char wildcard[1024] = "*.*";
	int ndirs;

	if (Cmd_Argc() != 1)
		Q_strncpyz(wildcard, Cmd_Argv(1), sizeof(wildcard));

	while ((path = FS_NextPath(path)) != NULL)
	{
		char *tmp = findname;
		Com_sprintf(findname, sizeof(findname), "%s/%s", path, wildcard);

		while (*tmp != 0)
		{
			if (*tmp == '\\') 
				*tmp = '/';

			tmp++;
		}

		Com_Printf("Directory of %s\n", findname);
		Com_Printf("----\n");

		char **dirnames = FS_ListFiles(findname, &ndirs, 0, 0);
		if (dirnames)
		{
			for (int i = 0; i < ndirs; i++)
			{
				if (strrchr(dirnames[i], '/'))
					Com_Printf("%s\n", strrchr(dirnames[i], '/') + 1);
				else
					Com_Printf("%s\n", dirnames[i]);

				free(dirnames[i]);
			}

			free(dirnames);
		}

		Com_Printf("\n");
	}
}