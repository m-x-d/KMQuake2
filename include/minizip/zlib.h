//mxd. Fake zlib header, so unzip.c / ioapi.c use miniz instead of zlib...
#pragma once

#define MINIZ_NO_ARCHIVE_WRITING_APIS 1
#include "miniz/miniz.h"

// zlib definitions required by minizip but not provided by the miniz library
#ifndef OF
	#define OF(args) args
#endif

#ifndef ZEXPORT
	#define ZEXPORT
#endif

#ifndef z_off_t
	#define z_off_t long
#endif