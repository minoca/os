/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    statvfs.h

Abstract:

    This header contains definitions for getting information about the file
    system.

Author:

    Evan Green 15-Jan-2015

--*/

#ifndef _SYS_STATVFS_H
#define _SYS_STATVFS_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/types.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define flags for the f_flag field of the statvfs structure.
//

//
// This bit is set if the file system is mounted read-only.
//

#define ST_RDONLY 0x00000001

//
// This bit is set if setuid and setgid bits are ignored.
//

#define ST_NOSUID 0x00000002

//
// This bit is set if device special files cannot be accessed.
//

#define ST_NODEV 0x00000004

//
// This bit is set if programs cannot be executed on this file system.
//

#define ST_NOEXEC 0x00000008

//
// This bit is set if writes are synchronized immediately.
//

#define ST_SYNCHRONOUS 0x00000010

//
// This bit is set to indicate access times are not updated.
//

#define ST_NOATIME 0x00000020

//
// This bit is set to indicate directory access times are not update.
//

#define ST_NODIRATIME 0x00000040

//
// This bit is set to indicate that access time is updated only in relation to
// modified and changed time.
//

#define ST_RELATIME 0x00000080

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines file system information.

Members:

    f_bsize - Stores the file system block size.

    f_frsize - Stores the fundamental file system block size.

    f_blocks - Stores the number of blocks on the file system in units of
        f_frsize.

    f_bfree - Stores the total number of free blocks.

    f_bavail - Stores the number of free blocks available to non-privileged
        processes.

    f_files - Stores the total number of file serial numbers.

    f_ffree - Stores the number of free file serial numbers.

    f_favail - Stores the number of free file serila numbers available to
        non-privileged processes.

    f_fsid - Stores the file system ID.

    f_flag - Stores the bitmask of flag values.

    f_namemax - Stores the maximum file name length.

--*/

struct statvfs {
    unsigned long f_bsize;
    unsigned long f_frsize;
    fsblkcnt_t f_blocks;
    fsblkcnt_t f_bfree;
    fsblkcnt_t f_bavail;
    fsfilcnt_t f_files;
    fsfilcnt_t f_ffree;
    fsfilcnt_t f_favail;
    unsigned long f_fsid;
    unsigned long f_flag;
    unsigned long f_namemax;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
statvfs (
    const char *Path,
    struct statvfs *Information
    );

/*++

Routine Description:

    This routine returns information about the file system containing the
    given path.

Arguments:

    Path - Supplies a pointer to a null terminated string containing the path
        to a file.

    Information - Supplies a pointer where the file system information will
        be returned.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

LIBC_API
int
fstatvfs (
    int FileDescriptor,
    struct statvfs *Information
    );

/*++

Routine Description:

    This routine returns information about the file system containing the
    given file descriptor.

Arguments:

    FileDescriptor - Supplies an open file descriptor whose file system
        properties are desired.

    Information - Supplies a pointer where the file system information will
        be returned.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

#ifdef __cplusplus

}

#endif
#endif

