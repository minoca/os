/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    param.h

Abstract:

    This header contains old system parameter definitions. This header is
    included only for application compatibility. New applications should use
    alternate methods to get at the information defined here.

Author:

    Evan Green 12-Jan-2015

--*/

#ifndef _SYS_PARAM_H
#define _SYS_PARAM_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <endian.h>
#include <limits.h>

//
// --------------------------------------------------------------------- Macros
//

//
// Define macros for manipulating bitmaps.
//

#define setbit(_Array, _Bit) \
    (((unsigned char *)(_Array))[(_Bit) / NBBY] |= 1 << ((_Bit) % NBBY))

#define clrbit(_Array, _Bit) \
    (((unsigned char *)(_Array))[(_Bit) / NBBY] &= ~(1 << ((_Bit) % NBBY)))

#define isset(_Array, _Bit) \
    (((const unsigned char *)(_Array))[(_Bit) / NBBY] & (1 << ((_Bit) % NBBY)))

#define isclr(_Array, _Bit) (!isset(_Array, _Bit))

//
// Define macros for counting and rounding.
//

#define howmany(_Value, _Divisor) (((_Value) + ((_Divisor) - 1)) / (_Divisor))
#define nitems(_Array) (sizeof((_Array)) / sizeof((_Array)[0])
#define rounddown(_Value, _Round) (((_Value) / (_Round)) * (_Round))
#define roundup(_Value, _Round) \
    ((((_Value) + ((_Round) - 1)) / (_Round)) * (_Round))

//
// Round down or up if the rounding value is a power of two.
//

#define rounddown2(_Value, _Round) ((_Value) & (~((_Round) - 1)))
#define roundup2(_Value, _Round) \
    (((_Value) + ((_Round) - 1)) & (~((_Round) - 1)))

#define powerof2(_Value) ((((_Value) - 1) & (_Value)) == 0)

//
// Define macros for MIN and MAX.
//

#define MIN(_Value1, _Value2) (((_Value1) < (_Value2)) ? (_Value1) : (_Value2))
#define MAX(_Value1, _Value2) (((_Value1) > (_Value2)) ? (_Value1) : (_Value2))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of bits in a byte.
//

#define NBBY CHAR_BIT

//
// Define the maximum number of user groups.
//

#define NGROUPS NGROUPS_MAX

//
// Define the maximum number of symbolic links that can be expanded in a path.
//

#define MAXSYMLINKS 8

//
// Define the maximum number of argument bytes when calling an exec function.
//

#define NCARGS _POSIX_ARG_MAX

//
// Define the default maximum number of files that can be open per process. The
// actual number is much greater than this, but this value is included for
// compatibility with older applications only.
//

#define NOFILE 256

//
// Define the value for an empty group set.
//

#define NOGROUP 65535

//
// Define the maximum host name size.
//

#define MAXHOSTNAMELEN 256

//
// Define the maximum domain name size.
//

#define MAXDOMNAMELEN 256

//
// Define the maximum size of a path after symlink expansion.
//

#define MAXPATHLEN PATH_MAX

//
// Define the unit of st_blocks in the stat structure.
//

#define DEV_BSIZE 512

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#endif

