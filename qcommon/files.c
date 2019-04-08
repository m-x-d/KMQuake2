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
#include "../include/zlib/unzip.h"

// enables faster binary pak searck, still experimental
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

#define BASEDIRNAME				"baseq2"

#define MAX_HANDLES				32
#define MAX_READ				0x10000
#define MAX_WRITE				0x10000
#define MAX_FIND_FILES			0x04000


//
// in memory
//

//
// Berserk's pk3 file support
//

typedef struct
{
	char			name[MAX_QPATH];
	fsMode_t		mode;
	FILE			*file;				// Only one of file or
	unzFile			*zip;				// zip will be used
} fsHandle_t;

typedef struct fsLink_s
{
	char			*from;
	int				length;
	char			*to;
	struct fsLink_s	*next;
} fsLink_t;

typedef struct
{
	char			name[MAX_QPATH];
	long			hash;				// To speed up searching
	int				size;
	int				offset;				// This is ignored in PK3 files
	qboolean		ignore;				// Whether this file should be ignored
} fsPackFile_t;

typedef struct
{
	char			name[MAX_OSPATH];
	FILE			*pak;
	unzFile			*pk3;
	int				numFiles;
	fsPackFile_t	*files;
	unsigned int	contentFlags;
} fsPack_t;

typedef struct fsSearchPath_s
{
	char			path[MAX_OSPATH];	// Only one of path or
	fsPack_t		*pack;				// pack will be used
	struct fsSearchPath_s	*next;
} fsSearchPath_t;

fsHandle_t		fs_handles[MAX_HANDLES];
fsLink_t		*fs_links;
fsSearchPath_t	*fs_searchPaths;
fsSearchPath_t	*fs_baseSearchPaths;

char			fs_gamedir[MAX_OSPATH];
static char		fs_currentGame[MAX_QPATH];

static char				fs_fileInPath[MAX_OSPATH];
static qboolean			fs_fileInPack;

int		file_from_pak = 0;		// This is set by FS_FOpenFile
int		file_from_pk3 = 0;		// This is set by FS_FOpenFile
char	last_pk3_name[MAX_QPATH];	// This is set by FS_FOpenFile

cvar_t	*fs_homepath;
cvar_t	*fs_basedir;
cvar_t	*fs_cddir;
cvar_t	*fs_basegamedir;
cvar_t	*fs_basegamedir2;
cvar_t	*fs_gamedirvar;
cvar_t	*fs_debug;
cvar_t	*fs_roguegame;


void CDAudio_Stop(void);


/*
=================
Com_FilePath

Returns the path up to, but not including the last /
=================
*/
void Com_FilePath (const char *path, char *dst, int dstSize)
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


char *type_extensions[] =
{
	"bsp",
	"md2",
	"md3",
	"sp2",
	"dm2",
	"cin",
	"roq",
	"wav",
	"ogg",
	"pcx",
	"wal",
	"tga",
	"jpg",
	"png",
	"cfg",
	"txt",
	"def",
	"alias",
	"arena",
	"script",
//	"shader",
	"hud",
//	"menu",
//	"efx",
	0
};

/*
=================
FS_TypeFlagForPakItem
Returns bit flag based on pak item's extension.
=================
*/
unsigned int FS_TypeFlagForPakItem (char *itemName)
{
	for (int i = 0; type_extensions[i]; i++) 
		if (!Q_stricmp(COM_FileExtension(itemName), type_extensions[i]))
			return 1 << i;

	return 0;
}


/*
=================
FS_FileLength
=================
*/
int FS_FileLength (FILE *f)
{
	const int cur = ftell(f);
	fseek(f, 0, SEEK_END);
	const int end = ftell(f);
	fseek(f, cur, SEEK_SET);

	return end;
}


/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
void FS_CreatePath (char *path)
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
			// create the directory
			*ofs = 0;
			Sys_Mkdir(path);
			*ofs = '/';
		}
	}
}


// Psychospaz's mod detector
qboolean modType (char *name)
{
	for (fsSearchPath_t *search = fs_searchPaths; search; search = search->next)
		if (strstr(search->path, name))
			return true;

	return false;
}


// This enables Rogue menu options for Q2MP4
qboolean roguepath()
{
	return (modType("rogue") || fs_roguegame->value);
}


/*
=================
FS_DPrintf
=================
*/
void FS_DPrintf (const char *format, ...)
{
	char	msg[1024];
	va_list	argPtr;

	if (!fs_debug->value)
		return;

	va_start(argPtr, format);
	Q_vsnprintf(msg, sizeof(msg), format, argPtr);
	va_end(argPtr);

	Com_Printf("%s", msg);
}


/*
=================
FS_GameDir

Called to find where to write a file (demos, savegames, etc...)
=================
*/
char *FS_Gamedir (void)
{
	return fs_gamedir;
}


/*
=================
FS_DeletePath

TODO: delete tree contents
=================
*/
void FS_DeletePath (char *path)
{
	FS_DPrintf("FS_DeletePath( %s )\n", path);
	Sys_Rmdir(path);
}


/*
=================
FS_FileForHandle

Returns a FILE * for a fileHandle_t
=================
*/
fsHandle_t *FS_GetFileByHandle(fileHandle_t f);

FILE *FS_FileForHandle (fileHandle_t f)
{
	fsHandle_t *handle = FS_GetFileByHandle(f);

	if (handle->zip)
		Com_Error(ERR_DROP, "FS_FileForHandle: can't get FILE on zip file");

	if (!handle->file)
		Com_Error(ERR_DROP, "FS_FileForHandle: NULL");

	return handle->file;
}


/*
=================
FS_HandleForFile

Finds a free fileHandle_t
=================
*/
fsHandle_t *FS_HandleForFile (const char *path, fileHandle_t *f)
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


/*
=================
FS_GetFileByHandle

Returns a fsHandle_t * for the given fileHandle_t
=================
*/
fsHandle_t *FS_GetFileByHandle (fileHandle_t f)
{
	if (f <= 0 || f > MAX_HANDLES)
		Com_Error(ERR_DROP, "FS_GetFileByHandle: out of range");

	return &fs_handles[f - 1];
}

#ifdef BINARY_PACK_SEARCH

/*
=================
FS_FindPackItem

Performs a binary search by hashed filename to find pack items in a sorted pack
=================
*/
int FS_FindPackItem (fsPack_t *pack, char *itemName, long itemHash)
{
	// catch null pointers
	if (!pack || !itemName)
		return -1;

	int smin = 0;
	int smax = pack->numFiles;

	while (smax - smin > 5)
	{
		const int smidpt = (smax + smin) / 2;

		if (pack->files[smidpt].hash > itemHash)	// before midpoint
			smax = smidpt;
		else if (pack->files[smidpt].hash < itemHash)	// after midpoint
			smin = smidpt;
		else
			break;
	}

	for (int i = smin; i < smax; i++)
	{
		// make sure this entry is not blacklisted & compare filenames
		if (pack->files[i].hash == itemHash && !pack->files[i].ignore && !Q_stricmp(pack->files[i].name, itemName))
			return i;
	}

	return -1;
}

#endif	// BINARY_PACK_SEARCH

/*
=================
FS_FOpenFileAppend

Returns file size or -1 on error
=================
*/
int FS_FOpenFileAppend (fsHandle_t *handle)
{
	char path[MAX_OSPATH];

	FS_CreatePath(handle->name);

	Com_sprintf(path, sizeof(path), "%s/%s", fs_gamedir, handle->name);

	handle->file = fopen(path, "ab");
	if (handle->file)
	{
		if (fs_debug->value)
			Com_Printf("FS_FOpenFileAppend: %s\n", path);

		return FS_FileLength(handle->file);
	}

	if (fs_debug->value)
		Com_Printf("FS_FOpenFileAppend: couldn't open %s\n", path);

	return -1;
}


/*
=================
FS_FOpenFileWrite

Always returns 0 or -1 on error
=================
*/
int FS_FOpenFileWrite (fsHandle_t *handle)
{
	char path[MAX_OSPATH];

	FS_CreatePath(handle->name);

	Com_sprintf(path, sizeof(path), "%s/%s", fs_gamedir, handle->name);

	handle->file = fopen(path, "wb");
	if (handle->file)
	{
		if (fs_debug->value)
			Com_Printf("FS_FOpenFileWrite: %s\n", path);

		return 0;
	}

	if (fs_debug->value)
		Com_Printf("FS_FOpenFileWrite: couldn't open %s\n", path);

	return -1;
}


/*
=================
FS_FOpenFileRead

Returns file size or -1 if not found.
Can open separate files as well as files inside pack files (both PAK and PK3).
=================
*/
int FS_FOpenFileRead (fsHandle_t *handle)
{
	char path[MAX_OSPATH];
	int i;

	// Knightmare- hack global vars for autodownloads
	file_from_pak = 0;
	file_from_pk3 = 0;
	Com_sprintf(last_pk3_name, sizeof(last_pk3_name), "\0");
	const long hash = Com_HashFileName(handle->name, 0, false);
	const unsigned int typeFlag = FS_TypeFlagForPakItem(handle->name);

	// Search through the path, one element at a time
	for (fsSearchPath_t *search = fs_searchPaths; search; search = search->next)
	{
		if (search->pack)
		{
			// Search inside a pack file
			fsPack_t *pack = search->pack;

			// skip if pack doesn't contain this type of file
			if (typeFlag != 0 && !(pack->contentFlags & typeFlag)) 
				continue;

#ifdef BINARY_PACK_SEARCH

			// find index of pack item
			i = FS_FindPackItem(pack, handle->name, hash);
			
			// found it!
			if (i >= 0 && i < pack->numFiles )
			{

#else
			for (i = 0; i < pack->numFiles; i++)
			{
				if (pack->files[i].ignore)	// skip blacklisted files
					continue;
				if (hash != pack->files[i].hash)	// compare hash first
					continue;

#endif	// 	BINARY_PACK_SEARCH
				if (!Q_stricmp(pack->files[i].name, handle->name))
				{
					// Found it!
					Com_FilePath(pack->name, fs_fileInPath, sizeof(fs_fileInPath));
					fs_fileInPack = true;

					if (fs_debug->value)
						Com_Printf("FS_FOpenFileRead: %s (found in %s)\n", handle->name, pack->name);

					if (pack->pak)
					{
						// PAK
						file_from_pak = 1; // Knightmare added
						handle->file = fopen(pack->name, "rb");
						if (handle->file)
						{
							fseek(handle->file, pack->files[i].offset, SEEK_SET);

							return pack->files[i].size;
						}
					}
					else if (pack->pk3)
					{
						// PK3
						file_from_pk3 = 1; // Knightmare added
						Com_sprintf(last_pk3_name, sizeof(last_pk3_name), strrchr(pack->name, '/') + 1); // Knightmare added
						handle->zip = unzOpen(pack->name);
						if (handle->zip)
						{
							if (unzLocateFile(handle->zip, handle->name, 2) == UNZ_OK && unzOpenCurrentFile(handle->zip) == UNZ_OK)
								return pack->files[i].size;

							unzClose(handle->zip);
						}
					}

					Com_Error(ERR_FATAL, "Couldn't reopen %s", pack->name);
				}
				else
				{
					Com_Printf("FS_FOpenFileRead: different filenames with identical hash (%s, %s)!\n", pack->files[i].name, handle->name);
				}
			}
		}
		else
		{
			// Search in a directory tree
			Com_sprintf(path, sizeof(path), "%s/%s", search->path, handle->name);

			handle->file = fopen(path, "rb");
			if (handle->file)
			{
				// Found it!
				Q_strncpyz(fs_fileInPath, search->path, sizeof(fs_fileInPath));
				fs_fileInPack = false;

				if (fs_debug->value)
					Com_Printf("FS_FOpenFileRead: %s (found in %s)\n", handle->name, search->path);

				return FS_FileLength(handle->file);
			}
		}
	}

	// Not found!
	fs_fileInPath[0] = 0;
	fs_fileInPack = false;

	if (fs_debug->value)
		Com_Printf("FS_FOpenFileRead: couldn't find %s\n", handle->name);

	return -1;
}

/*
=================
FS_FOpenFile

Opens a file for "mode".
Returns file size or -1 if an error occurs/not found.
Can open separate files as well as files inside pack files (both PAK and PK3).
=================
*/
int FS_FOpenFile (const char *name, fileHandle_t *f, fsMode_t mode)
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


/*
=================
FS_FCloseFile
=================
*/
void FS_FCloseFile (fileHandle_t f)
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

/*
=================
FS_Read

Properly handles partial reads
=================
*/
int FS_Read (void *buffer, int size, fileHandle_t f)
{
	int r;

	qboolean tried = false;
	fsHandle_t *handle = FS_GetFileByHandle(f);

	// Read
	int remaining = size;
	byte *buf = (byte *)buffer;

	while (remaining)
	{
		if (handle->file)
			r = fread(buf, 1, remaining, handle->file);
		else if (handle->zip)
			r = unzReadCurrentFile(handle->zip, buf, remaining);
		else
			return 0;

		if (r == 0)
		{
			if (!tried)
			{
				// We might have been trying to read from a CD
				CDAudio_Stop();
				tried = true;
			}
			else
			{
				// Already tried once
				Com_DPrintf(S_COLOR_YELLOW"FS_Read: 0 bytes read from %s\n", handle->name);
				return size - remaining;
			}
		}
		else if (r == -1)
		{
			Com_Error(ERR_FATAL, "FS_Read: -1 bytes read from %s", handle->name);
		}

		remaining -= r;
		buf += r;
	}

	return size;
}


/*
=================
FS_FRead

Properly handles partial reads of size up to count times
No error if it can't read
=================
*/
int FS_FRead (void *buffer, int size, int count, fileHandle_t f)
{
	int r;

	qboolean tried = false;
	fsHandle_t *handle = FS_GetFileByHandle(f);

	// Read
	int loops = count;
	byte *buf = (byte *)buffer;

	while (loops)
	{
		// Read in chunks
		int remaining = size;
		while (remaining)
		{
			if (handle->file)
				r = fread(buf, 1, remaining, handle->file);
			else if (handle->zip)
				r = unzReadCurrentFile(handle->zip, buf, remaining);
			else
				return 0;

			if (r == 0)
			{
				if (!tried)
				{
					// We might have been trying to read from a CD
					CDAudio_Stop();
					tried = true;
				}
				else
				{
					return size - remaining;
				}
			}
			else if (r == -1)
			{
				Com_Error(ERR_FATAL, "FS_FRead: -1 bytes read from %s", handle->name);
			}

			remaining -= r;
			buf += r;
		}

		loops--;
	}

	return size;
}

/*
=================
FS_Write

Properly handles partial writes
=================
*/
int FS_Write (const void *buffer, int size, fileHandle_t f)
{
	int w = -1;
	fsHandle_t *handle = FS_GetFileByHandle(f);

	// Write
	int remaining = size;
	byte *buf = (byte *)buffer;

	while (remaining)
	{
		if (handle->file)
			w = fwrite(buf, 1, remaining, handle->file);
		else if (handle->zip)
			Com_Error(ERR_FATAL, "FS_Write: can't write to zip file %s", handle->name);
		else
			return 0;

		if (w == 0)
		{
			Com_Printf(S_COLOR_RED"FS_Write: 0 bytes written to %s\n", handle->name);
			return size - remaining;
		}

		if (w == -1)
			Com_Error(ERR_FATAL, "FS_Write: -1 bytes written to %s", handle->name);

		remaining -= w;
		buf += w;
	}

	return size;
}

/*
=================
FS_FTell
=================
*/
int FS_FTell (fileHandle_t f)
{
	fsHandle_t *handle = FS_GetFileByHandle(f);

	if (handle->file)
		return ftell(handle->file);

	if (handle->zip)
		return unztell(handle->zip);

	return 0;
}

/*
=================
FS_ListPak

Generates a listing of the contents of a pak file
=================
*/
char **FS_ListPak (char *find, int *num)
{
	int nfiles = 0;
	int nfound = 0;

	// now check pak files
	for (fsSearchPath_t	*search = fs_searchPaths; search; search = search->next)
	{
		if (!search->pack)
			continue;

		fsPack_t *pak = search->pack;

		// now find and build list
		for (int i = 0; i < pak->numFiles; i++)
			if (!pak->files[i].ignore)
				nfiles++;
	}

	char **list = malloc(sizeof(char *) * nfiles);
	memset(list, 0, sizeof(char *) * nfiles);

	for (fsSearchPath_t	*search = fs_searchPaths; search; search = search->next)
	{
		if (!search->pack)
			continue;

		fsPack_t *pak = search->pack;

		// now find and build list
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

/*
=================
FS_Seek
=================
*/
void FS_Seek (fileHandle_t f, int offset, fsOrigin_t origin)
{
	byte dummy[0x8000];
	fsHandle_t *handle = FS_GetFileByHandle(f);

	if (handle->file)
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
		}
			break;

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

/*
=================
FS_Tell

Returns -1 if an error occurs
=================
*/
int FS_Tell (fileHandle_t f)
{
	fsHandle_t *handle = FS_GetFileByHandle(f);

	if (handle->file)
		return ftell(handle->file);

	if (handle->zip)
		return unztell(handle->zip);

	return -1; //mxd. Let's actually return -1 :)
}

/*
=================
FS_FileExists
================
*/
qboolean FS_FileExists (char *path)
{
	fileHandle_t f;
	FS_FOpenFile(path, &f, FS_READ);

	if (f)
	{
		FS_FCloseFile(f);
		return true;
	}

	return false;
}

/*
=================
FS_LocalFileExists
================
*/
qboolean FS_LocalFileExists (char *path)
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

/*
=================
FS_RenameFile
=================
*/
void FS_RenameFile (const char *oldPath, const char *newPath)
{
	FS_DPrintf("FS_RenameFile( %s, %s )\n", oldPath, newPath);

	if (rename(oldPath, newPath))
		FS_DPrintf("FS_RenameFile: failed to rename %s to %s\n", oldPath, newPath);
}

/*
=================
FS_DeleteFile
=================
*/
void FS_DeleteFile (const char *path)
{
	FS_DPrintf("FS_DeleteFile( %s )\n", path);

	if (remove(path))
		FS_DPrintf("FS_DeleteFile: failed to delete %s\n", path);
}

/*
=================
FS_LoadFile

"path" is relative to the Quake search path.
Returns file size or -1 if the file is not found.
A NULL buffer will just return the file size without loading.
=================
*/
int FS_LoadFile (char *path, void **buffer)
{
	fileHandle_t f;
	const int size = FS_FOpenFile(path, &f, FS_READ);

	if (size == -1 || size == 0)
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

/*
=================
FS_FreeFile
=================
*/
void FS_FreeFile (void *buffer)
{
	if (!buffer)
	{
		FS_DPrintf("FS_FreeFile: NULL buffer\n");
		return;
	}

	Z_Free(buffer);
}

// Some incompetently packaged mods have these files in their paks!
char *pakfile_ignore_names[] =
{
	"save/",
	"scrnshot/",
	"screenshots/", //mxd
	"autoexec.cfg",
	"kmq2config.cfg",
	0
};


/*
=================
FS_FileInPakBlacklist

Checks against a blacklist to see if a file should not be loaded from a pak.
=================
*/
qboolean FS_FileInPakBlacklist (char *filename, qboolean isPk3)
{
	qboolean ignore = false;

	char *compare = filename;
	if (compare[0] == '/')	// remove leading slash
		compare++;

	for (int i = 0; pakfile_ignore_names[i]; i++)
	{
		if (!Q_strncasecmp(compare, pakfile_ignore_names[i], strlen(pakfile_ignore_names[i])))
			ignore = true;

		// Ogg files can't load from .paks
		if (!isPk3 && !Q_stricmp(COM_FileExtension(compare), "ogg"))
			ignore = true;
	}

	return ignore;
}


#ifdef BINARY_PACK_SEARCH

/*
=================
FS_PakFileCompare
 
Used for sorting pak entries by hash
=================
*/
long *nameHashes = NULL;

int FS_PakFileCompare (const void *f1, const void *f2)
{
	if (!nameHashes)
		return 1;

	return (nameHashes[*((int *)f1)] - nameHashes[*((int *)f2)]);
}

#endif	// BINARY_PACK_SEARCH


/*
=================
FS_LoadPAK
 
Takes an explicit (not game tree related) path to a pack file.
Loads the header and directory, adding the files at the beginning of the list so they override previous pack files.
=================
*/
fsPack_t *FS_LoadPAK (const char *packPath)
{
	dpackheader_t	header;
	dpackfile_t		info[MAX_FILES_IN_PACK];
	unsigned		contentFlags = 0;

	FILE *handle = fopen(packPath, "rb");
	if (!handle)
		return NULL;

	fread(&header, 1, sizeof(dpackheader_t), handle);
	
	if (LittleLong(header.ident) != IDPAKHEADER)
	{
		fclose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPAK: %s is not a pack file", packPath);
	}

	header.dirofs = LittleLong(header.dirofs);
	header.dirlen = LittleLong(header.dirlen);

	const int numFiles = header.dirlen / sizeof(dpackfile_t);
	if (numFiles > MAX_FILES_IN_PACK || numFiles == 0)
	{
		fclose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPAK: %s has %i files", packPath, numFiles);
	}

	fsPackFile_t *files = Z_Malloc(numFiles * sizeof(fsPackFile_t));

	fseek(handle, header.dirofs, SEEK_SET);
	fread(info, 1, header.dirlen, handle);

#ifdef BINARY_PACK_SEARCH

	// create sort table
	int *sortIndices = Z_Malloc(numFiles * sizeof(int));
	long *sortHashes = Z_Malloc(numFiles * sizeof(unsigned));
	nameHashes = sortHashes;

	for (int i = 0; i < numFiles; i++)
	{
		sortIndices[i] = i;
		sortHashes[i] = Com_HashFileName(info[i].name, 0, false);
	}

	qsort((void *)sortIndices, numFiles, sizeof(int), FS_PakFileCompare);

	// Parse the directory
	for (int i = 0; i < numFiles; i++)
	{
		Q_strncpyz(files[i].name, info[sortIndices[i]].name, sizeof(files[i].name));
		files[i].hash = sortHashes[sortIndices[i]];
		files[i].offset = LittleLong(info[sortIndices[i]].filepos);
		files[i].size = LittleLong(info[sortIndices[i]].filelen);
		files[i].ignore = FS_FileInPakBlacklist(files[i].name, false);	// check against pak loading blacklist

		if (!files[i].ignore)	// add type flag for this file
			contentFlags |= FS_TypeFlagForPakItem(files[i].name);
	}

	// free sort table
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
		files[i].ignore = FS_FileInPakBlacklist(info[i].name, false);	// check against pak loading blacklist

		if (!files[i].ignore)	// add type flag for this file
			contentFlags |= FS_TypeFlagForPakItem(files[i].name);
	}

#endif	// BINARY_PACK_SEARCH

	fsPack_t *pack = Z_Malloc(sizeof(fsPack_t));
	Q_strncpyz(pack->name, packPath, sizeof(pack->name));
	pack->pak = handle;
	pack->pk3 = NULL;
	pack->numFiles = numFiles;
	pack->files = files;
	pack->contentFlags = contentFlags;

	return pack;
}

/*
=================
FS_AddPAKFile

Adds a Pak file to the searchpath
=================
*/
void FS_AddPAKFile (const char *packPath)
{
	fsPack_t *pack = FS_LoadPAK(packPath);
	if (!pack)
		return;

	fsSearchPath_t *search = Z_Malloc(sizeof(fsSearchPath_t));
	search->pack = pack;
	search->next = fs_searchPaths;
	fs_searchPaths = search;
}

/*
=================
FS_LoadPK3

Takes an explicit (not game tree related) path to a pack file.
Loads the header and directory, adding the files at the beginning of the list so they override previous pack files.
=================
*/
fsPack_t *FS_LoadPK3 (const char *packPath)
{
	unz_global_info	global;
	unz_file_info	info;
	unsigned		contentFlags = 0;
	char			fileName[MAX_QPATH];

	unzFile *handle = unzOpen(packPath);
	if (!handle)
		return NULL;

	if (unzGetGlobalInfo(handle, &global) != UNZ_OK)
	{
		unzClose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPK3: %s is not a pack file", packPath);
	}

	const int numFiles = global.number_entry;
	if (numFiles > MAX_FILES_IN_PACK || numFiles == 0)
	{
		unzClose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPK3: %s has %i files", packPath, numFiles);
	}

	fsPackFile_t *files = Z_Malloc(numFiles * sizeof(fsPackFile_t));

#ifdef BINARY_PACK_SEARCH

	// create sort table
	fsPackFile_t *tmpFiles = Z_Malloc(numFiles * sizeof(fsPackFile_t));
	int *sortIndices = Z_Malloc(numFiles * sizeof(int));
	long *sortHashes = Z_Malloc(numFiles * sizeof(unsigned));
	nameHashes = sortHashes;

	// Parse the directory
	int index = 0;
	int status = unzGoToFirstFile(handle);
	while (status == UNZ_OK)
	{
		fileName[0] = 0;
		unzGetCurrentFileInfo(handle, &info, fileName, MAX_QPATH, NULL, 0, NULL, 0);
		sortIndices[index] = index;
		Q_strncpyz(tmpFiles[index].name, fileName, sizeof(tmpFiles[index].name));

		tmpFiles[index].hash = sortHashes[index] = Com_HashFileName(fileName, 0, false);	// Added to speed up seaching
		tmpFiles[index].offset = -1;	// Not used in ZIP files
		tmpFiles[index].size = info.uncompressed_size;
		tmpFiles[index].ignore = FS_FileInPakBlacklist(fileName, true);	// check against pak loading blacklist

		if (!tmpFiles[index].ignore)	// add type flag for this file
			contentFlags |= FS_TypeFlagForPakItem(tmpFiles[index].name);

		index++;
		status = unzGoToNextFile(handle);
	}

	// sort by hash and copy to final file table
	qsort((void *)sortIndices, numFiles, sizeof(int), FS_PakFileCompare);
	for (int i = 0; i < numFiles; i++)
	{
		Q_strncpyz(files[i].name, tmpFiles[sortIndices[i]].name, sizeof(files[i].name));
		files[i].hash = tmpFiles[sortIndices[i]].hash;
		files[i].offset = tmpFiles[sortIndices[i]].offset;
		files[i].size = tmpFiles[sortIndices[i]].size;
		files[i].ignore = tmpFiles[sortIndices[i]].ignore;
	}

	// free sort table
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

/*
=================
FS_AddPK3File

Adds a Pk3 file to the searchpath
=================
*/
void FS_AddPK3File (const char *packPath)
{
	fsPack_t *pack = FS_LoadPK3(packPath);
	if (!pack)
		return;

	fsSearchPath_t *search = Z_Malloc(sizeof(fsSearchPath_t));
	search->pack = pack;
	search->next = fs_searchPaths;
	fs_searchPaths = search;
}

/*
=================
FS_AddGameDirectory

Sets fs_gameDir, adds the directory to the head of the path, then loads and adds all the pack files found (in alphabetical order).
PK3 files are loaded later so they override PAK files.
=================
*/
void FS_AddGameDirectory (const char *dir)
{
	char packPath[MAX_OSPATH];

	// VoiD -S- *.pak support
	char findname[1024];
	int ndirs;
	// VoiD -E- *.pak support

	Q_strncpyz(fs_gamedir, dir, sizeof(fs_gamedir));

	//
	// Add the directory to the search path
	//
	fsSearchPath_t *search = Z_Malloc(sizeof(fsSearchPath_t));
	Q_strncpyz(search->path, dir, sizeof(search->path));
	search->path[sizeof(search->path) - 1] = 0;
	search->next = fs_searchPaths;
	fs_searchPaths = search;

	//
	// add any pak files in the format pak0.pak pak1.pak, ...
	//
	for (int i = 0; i < 100; i++)    // Pooy - paks can now go up to 100
	{
		Com_sprintf(packPath, sizeof(packPath), "%s/pak%i.pak", dir, i);
		FS_AddPAKFile(packPath);
	}

	//
	// NeVo - pak3's!
	// add any pk3 files in the format pak0.pk3 pak1.pk3, ...
	//
	for (int i = 0; i < 100; i++)    // Pooy - paks can now go up to 100
	{
		Com_sprintf(packPath, sizeof(packPath), "%s/pak%i.pk3", dir, i);
		FS_AddPK3File(packPath);
	}

	for (int i = 0; i < 2; i++)
	{
		// NeVo - Set filetype
		if(i == 0)
			Com_sprintf(findname, sizeof(findname), "%s/%s", dir, "*.pak"); // Standard Quake II pack file '.pak'
		else
			Com_sprintf(findname, sizeof(findname), "%s/%s", dir, "*.pk3"); // Quake III pack file '.pk3'

		// VoiD -S- *.pack support
		char *tmp = findname;
		while (*tmp != 0)
		{
			if (*tmp == '\\')
				*tmp = '/';

			tmp++;
		}

		char **dirnames = FS_ListFiles(findname, &ndirs, 0, 0);
		if (dirnames)
		{
			for (int j = 0; j < ndirs - 1; j++)
			{
				char buf[16];
				char buf2[16];
				qboolean numberedpak = false;

				for (int k = 0; k < 100; k++)
				{
					Com_sprintf(buf, sizeof(buf), "/pak%i.pak", k);
					Com_sprintf(buf2, sizeof(buf2), "/pak%i.pk3", k);

					if (strstr(dirnames[j], buf) || strstr(dirnames[j], buf2))
					{
						numberedpak = true;
						break;
					}
				}

				if (numberedpak)
					continue;

 				if (strrchr(dirnames[j], '/' ))
				{
					if (i == 1)
						FS_AddPK3File(dirnames[j]);
					else
						FS_AddPAKFile(dirnames[j]);
				}

				free(dirnames[j]);
			}

			free(dirnames);
		}
		// VoiD -E- *.pack support
	}
}

/*
=================
FS_NextPath

Allows enumerating all of the directories in the search path
=================
*/
char *FS_NextPath (char *prevPath)
{
	if (!prevPath)
		return fs_gamedir;

	char *prev = fs_gamedir;
	for (fsSearchPath_t *search = fs_searchPaths; search; search = search->next)
	{
		if (search->pack)
			continue;

		if (prevPath == prev)
			return search->path;

		prev = search->path;
	}

	return NULL;
}


/*
=================
FS_Path_f
=================
*/
void FS_Path_f (void)
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

/*
=================
FS_Startup

TODO: close open files for game dir
=================
*/
void FS_Startup (void)
{
	if (!fs_gamedirvar->string[0] || strstr(fs_gamedirvar->string, ".") || strstr(fs_gamedirvar->string, "/")
		|| strstr(fs_gamedirvar->string, "\\") || strstr(fs_gamedirvar->string, ":"))
	{
		Cvar_ForceSet("game", BASEDIRNAME);
	}

	// Check for game override
	if (stricmp(fs_gamedirvar->string, fs_currentGame))
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

		if (!stricmp(fs_gamedirvar->string, BASEDIRNAME))	// Don't add baseq2 again
			Q_strncpyz(fs_gamedir, fs_basedir->string, sizeof(fs_gamedir));
		else
			FS_AddGameDirectory(va("%s/%s", fs_homepath->string, fs_gamedirvar->string)); // Add the directories
	}

	Q_strncpyz(fs_currentGame, fs_gamedirvar->string, sizeof(fs_currentGame));
	FS_Path_f();
}

/*
=================
FS_Init
=================
*/
void FS_Link_f(void);
char *Sys_GetCurrentDirectory(void);

void FS_InitFilesystem (void)
{
	// Register our commands and cvars
	Cmd_AddCommand("path", FS_Path_f);
	Cmd_AddCommand("link", FS_Link_f);
	Cmd_AddCommand("dir", FS_Dir_f);

	Com_Printf("\n----- Filesystem Initialization -----\n");

	// basedir <path>
	// allows the game to run from outside the data tree
	fs_basedir = Cvar_Get("basedir", ".", CVAR_NOSET);

	// cddir <path>
	// Logically concatenates the cddir after the basedir to allow the game to run from outside the data tree
	fs_cddir = Cvar_Get("cddir", "", CVAR_NOSET);
	if (fs_cddir->string[0])
		FS_AddGameDirectory(va("%s/"BASEDIRNAME, fs_cddir->string));

	// start up with baseq2 by default
	FS_AddGameDirectory(va("%s/"BASEDIRNAME, fs_basedir->string));

	// any set gamedirs will be freed up to here
	fs_baseSearchPaths = fs_searchPaths;

	Q_strncpyz(fs_currentGame, BASEDIRNAME, sizeof(fs_currentGame));

	// check for game override
	fs_homepath = Cvar_Get("homepath", Sys_GetCurrentDirectory(), CVAR_NOSET);
	fs_debug = Cvar_Get("fs_debug", "0", 0);
	fs_roguegame = Cvar_Get("roguegame", "0", CVAR_LATCH);
	fs_basegamedir = Cvar_Get("basegame", "", CVAR_LATCH);
	fs_basegamedir2 = Cvar_Get("basegame2", "", CVAR_LATCH);
	fs_gamedirvar = Cvar_Get("game", "", CVAR_LATCH|CVAR_SERVERINFO);

	// check and load game directory
	if (fs_gamedirvar->string[0])
		FS_SetGamedir(fs_gamedirvar->string);

	FS_Path_f(); // output path data
}

/*
=================
FS_Shutdown
=================
*/
void FS_Shutdown (void)
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


/*
================
FS_SetGamedir

Sets the gamedir and path to a different directory.
================
*/
void FS_SetGamedir (char *dir)
{
	qboolean basegame1_loaded = false;

	if (strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\") || strstr(dir, ":") )
	{
		Com_Printf("Gamedir should be a single filename, not a path\n");
		return;
	}

	// Knightmare- check basegame var
	if (fs_basegamedir->string[0])
	{
		if (strstr(fs_basegamedir->string, "..") || strstr(fs_basegamedir->string, "/") || strstr(fs_basegamedir->string, "\\") || strstr(fs_basegamedir->string, ":"))
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
		if (strstr(fs_basegamedir2->string, "..") || strstr(fs_basegamedir2->string, "/") || strstr(fs_basegamedir2->string, "\\") || strstr(fs_basegamedir2->string, ":"))
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

	//
	// free up any current game dir info
	//
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

	//
	// flush all data, so it will be forced to reload
	//
	if (dedicated && !dedicated->value)
		Cbuf_AddText("vid_restart\nsnd_restart\n");

	if (*dir == 0)	// Knightmare- set to basedir if a blank dir is passed
		Com_sprintf(fs_gamedir, sizeof(fs_gamedir), "%s/"BASEDIRNAME, fs_basedir->string);
	else
		Com_sprintf(fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basedir->string, dir);

	if (!strcmp(dir, BASEDIRNAME) || *dir == 0)
	{
		Cvar_FullSet("gamedir", "", CVAR_SERVERINFO|CVAR_NOSET);
		Cvar_FullSet("game", "", CVAR_LATCH|CVAR_SERVERINFO);
	}
	else
	{
		// check and load base game directory (so mods can be based upon other mods)
		if (fs_basegamedir->string[0])
		{
			if (fs_cddir->string[0])
				FS_AddGameDirectory(va("%s/%s", fs_cddir->string, fs_basegamedir->string));

			FS_AddGameDirectory(va("%s/%s", fs_basedir->string, fs_basegamedir->string));
			basegame1_loaded = true;
		}

		// second basegame so mods can utilize both Rogue and Xatrix assets
		if (basegame1_loaded && fs_basegamedir2->string[0])
		{
			if (fs_cddir->string[0])
				FS_AddGameDirectory(va("%s/%s", fs_cddir->string, fs_basegamedir2->string));

			FS_AddGameDirectory(va("%s/%s", fs_basedir->string, fs_basegamedir2->string));
		}

		Cvar_FullSet("gamedir", dir, CVAR_SERVERINFO | CVAR_NOSET);

		if (fs_cddir->string[0])
			FS_AddGameDirectory(va("%s/%s", fs_cddir->string, dir));

		FS_AddGameDirectory(va("%s/%s", fs_basedir->string, dir));
	}
}


/*
================
FS_Link_f

Creates a filelink_t
================
*/
void FS_Link_f (void)
{
	if (Cmd_Argc() != 3)
	{
		Com_Printf("USAGE: link <from> <to>\n");
		return;
	}

	// see if the link already exists
	fsLink_t **prev = &fs_links;
	for (fsLink_t *l = fs_links; l; l = l->next)
	{
		if (!strcmp(l->from, Cmd_Argv(1)))
		{
			Z_Free(l->to);
			if (!strlen(Cmd_Argv(2)))
			{
				// delete it
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

	// create a new link
	fsLink_t *newlink = Z_Malloc(sizeof(*newlink));
	newlink->next = fs_links;
	fs_links = newlink;
	newlink->from = CopyString(Cmd_Argv(1));
	newlink->length = strlen(newlink->from);
	newlink->to = CopyString(Cmd_Argv(2));
}


/*
=============
FS_ExecAutoexec
=============
*/
void FS_ExecAutoexec (void)
{
	char name [MAX_QPATH];

	char *dir = Cvar_VariableString("gamedir");
	if (*dir)
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, dir); 
	else
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, BASEDIRNAME);

	if (Sys_FindFirst(name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM))
		Cbuf_AddText("exec autoexec.cfg\n");

	Sys_FindClose();
}

/*
================
FS_ListFiles
================
*/
char **FS_ListFiles (char *findname, int *numfiles, unsigned musthave, unsigned canthave)
{
	int nfiles = 0;

	char *s = Sys_FindFirst(findname, musthave, canthave);
	while (s)
	{
		if (s[strlen(s) - 1] != '.')
			nfiles++;

		s = Sys_FindNext(musthave, canthave);
	}

	Sys_FindClose();

	if (!nfiles)
	{
		*numfiles = 0;
		return NULL;
	}

	nfiles++; // add space for a guard
	*numfiles = nfiles;

	char **list = malloc(sizeof( char * ) * nfiles);
	memset(list, 0, sizeof(char *) * nfiles);

	s = Sys_FindFirst(findname, musthave, canthave);
	nfiles = 0;
	while (s)
	{
		if (s[strlen(s) - 1] != '.')
		{
			list[nfiles] = strdup(s);
#ifdef _WIN32
			strlwr(list[nfiles]);
#endif
			nfiles++;
		}

		s = Sys_FindNext(musthave, canthave);
	}

	Sys_FindClose();

	return list;
}

/*
=================
FS_FreeFileList
=================
*/
void FS_FreeFileList (char **list, int n)
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

/*
=================
FS_ItemInList
=================
*/
qboolean FS_ItemInList (char *check, int num, char **list)
{
	for (int i = 0; i < num; i++)
		if (!Q_strcasecmp(check, list[i]))
			return true;

	return false;
}

/*
=================
FS_InsertInList
=================
*/
void FS_InsertInList (char **list, char *insert, int len, int start)
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

/*
================
FS_Dir_f
================
*/
void FS_Dir_f (void)
{
	char	*path = NULL;
	char	findname[1024];
	char	wildcard[1024] = "*.*";
	int		ndirs;

	if (Cmd_Argc() != 1)
		Q_strncpyz(wildcard, Cmd_Argv(1), sizeof(wildcard));

	while ((path = FS_NextPath(path)) != NULL)
	{
		char *tmp = findname;
		Com_sprintf(findname, sizeof(findname), "%s/%s", path, wildcard);

		while (*tmp != 0)
		{
			if ( *tmp == '\\' ) 
				*tmp = '/';

			tmp++;
		}

		Com_Printf("Directory of %s\n", findname);
		Com_Printf("----\n");

		char **dirnames = FS_ListFiles(findname, &ndirs, 0, 0);
		if (dirnames)
		{
			for (int i = 0; i < ndirs-1; i++)
			{
				if (strrchr(dirnames[i], '/' ))
					Com_Printf("%s\n", strrchr(dirnames[i], '/') + 1 );
				else
					Com_Printf("%s\n", dirnames[i]);

				free(dirnames[i]);
			}

			free(dirnames);
		}

		Com_Printf("\n");
	}
}