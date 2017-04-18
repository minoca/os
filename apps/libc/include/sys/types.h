/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    types.h

Abstract:

    This header contains common abstract data types used throughout the C
    library.

Author:

    Evan Green 5-Mar-2013

--*/

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <stddef.h>
#include <stdint.h>

//
// ------------------------------------------------------ Data Type Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define a type used for file block counts.
//

typedef long long blkcnt_t;

//
// Define a type used for block sizes.
//

typedef unsigned long long blksize_t;

//
// Define a type for file system block counts.
//

typedef unsigned long long fsblkcnt_t;

//
// Define a type for the file system file counts.
//

typedef unsigned long long fsfilcnt_t;

//
// Define a type used for system clock ticks.
//

typedef unsigned long long clock_t;

//
// Define the type used for clock timer types (the CLOCK_* definitions).
//

typedef unsigned int clockid_t;

//
// Define a type used for device IDs.
//

typedef unsigned long long dev_t;

//
// Define the size of a generic identifier, which can be used to contain at
// least a pid_t, uid_t, or gid_t.
//

typedef unsigned int id_t;

//
// Define a group identifier type.
//

typedef unsigned int gid_t;

//
// Define a type used for file serial numbers.
//

typedef unsigned long long ino_t;

//
// Define a type used for file mode bits.
//

typedef unsigned int mode_t;

//
// Define a type used for link count (both hard and symbolic).
//

typedef int nlink_t;

//
// Define the size of file offsets.
//

typedef long long off_t;

//
// Define the size for 64 bit file offsets.
//

typedef off_t off64_t;

//
// Define a process identifier type.
//

#ifndef pid_t

typedef int pid_t;

#endif

//
// Define a type used for a count of bytes or error indication.
//

#ifndef ssize_t

typedef long ssize_t;

#endif

//
// Define the type used for timer handles.
//

typedef long timer_t;

//
// Define a user identifier type.
//

typedef unsigned int uid_t;

//
// Define a signed type used for time in microseconds.
//

typedef long suseconds_t;

//
// Define a type used for time in seconds.
//

typedef signed long long time_t;

//
// Define a type used for time in microseconds.
//

typedef unsigned int useconds_t;

//
// Define the key type used for interprocess communication.
//

typedef int key_t;

//
// Define some pthread types.
//

//
// Define the type of a thread identifier.
//

typedef long pthread_t;

//
// Define the type used for a "once" object.
//

typedef int pthread_once_t;

//
// Define the type used for a key object.
//

typedef int pthread_key_t;

//
// Define pthread structures. Their internals are not exposed.
//

typedef union {
    char Data[16];
    long int AlignMember;
} pthread_mutex_t;

typedef union {
    char Data[16];
    long int AlignMember;
} pthread_mutexattr_t;

typedef union {
    char Data[16];
    long int AlignMember;
} pthread_cond_t;

typedef union {
    char Data[16];
    long int AlignMember;
} pthread_condattr_t;

typedef union {
    char Data[48];
    long int AlignMember;
} pthread_rwlock_t;

typedef union {
    char Data[16];
    long int AlignMember;
} pthread_rwlockattr_t;

typedef union {
    char Data[64];
    long int AlignMember;
} pthread_attr_t;

typedef union {
    char Data[32];
    long int AlignMember;
} pthread_barrier_t;

typedef union {
    char Data[16];
    long int AlignMember;
} pthread_barrierattr_t;

//
// Define some old BSD types.
//

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;

typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;
typedef unsigned long long u_int64_t;

typedef char *caddr_t;
typedef long daddr_t;

//
// Define some old C types.
//

typedef unsigned long int ulong;
typedef unsigned short int ushort;
typedef unsigned int uint;

#ifdef __cplusplus

}

#endif
#endif

