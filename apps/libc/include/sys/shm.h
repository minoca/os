/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    shm.h

Abstract:

    This header contains definitions for the older SystemV style shared memory
    objects.

Author:

    Evan Green 24-Mar-2017

--*/

#ifndef _SYS_SHM_H
#define _SYS_SHM_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <sys/ipc.h>
#include <time.h>

//
// Include unistd.h for sysconf.
//

#include <unistd.h>

//
// --------------------------------------------------------------------- Macros
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ---------------------------------------------------------------- Definitions
//

//
// Define flags for shmat.
//

//
// Set this flag to map the region read-only. The caller must have read
// permissions on the object. If this is not set, the caller must have read and
// write permissions on the object.
//

#define SHM_RDONLY 0x00001000

//
// Set this bit to round the attachment address down to SHMLBA if it is not
// page aligned.
//

#define SHM_RND 0x00002000

//
// Set this bit to map the region as executable. The caller must have execute
// permission on the region.
//

#define SHM_EXEC 0x00004000

//
// This bit is set if the shared memory object is scheduled for deletion after
// the last handle is closed.
//

#define SHM_DEST 0x00010000

//
// Define the rounding granularity used when SHM_RND is set.
//

#define SHMLBA sysconf(_SC_PAGE_SIZE)

//
// Define the surrogate structure members for the traditional names in
// struct shmid_ds.
//

#define shm_atime shm_atim.tv_sec
#define shm_dtime shm_dtim.tv_sec
#define shm_ctime shm_ctim.tv_sec

//
// ------------------------------------------------------ Data Type Definitions
//

typedef unsigned long shmatt_t;

/*++

Structure Description:

    This structure defines the properties of a shared memory object.

Members:

    shm_perm - Stores the permission information for the object.

    shm_segsz - Stores the segment size in bytes.

    shm_atim - Stores the last time a process attached to the segment.

    shm_dtim - Stores the last time a process detached from the segment.

    shm_ctim - Stores the last time a process changed the segment using shmctl.

    shm_cpid - Stores the creator process ID.

    shm_lpid - Stores the process ID of the last process to attach or detach.

    shm_nattch - Stores the number of attachments.

--*/

struct shmid_ds {
    struct ipc_perm shm_perm;
    off_t shm_segsz;
    struct timespec shm_atim;
    struct timespec shm_dtim;
    struct timespec shm_ctim;
    pid_t shm_cpid;
    pid_t shm_lpid;
    shmatt_t shm_nattch;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
shmget (
    key_t Key,
    size_t Size,
    int Flags
    );

/*++

Routine Description:

    This routine creates or opens a shared memory object.

Arguments:

    Key - Supplies the key associated with the new or existing object to open.
        Supply IPC_PRIVATE to always create a new object.

    Size - Supplies the minimum number of bytes in the region.

    Flags - Supplies a set of flags governing how the region is created. The
        bottom nine bits contain permission bits for the region. See IPC_*
        definitions for additional flags that can be passed like IPC_CREAT and
        IPC_EXCL.

Return Value:

    Returns an integer representing the new or existing shared memory object
    on success.

    -1 on failure, and errno will contain more information.

--*/

LIBC_API
void *
shmat (
    int SharedMemoryObject,
    const void *Address,
    int Flags
    );

/*++

Routine Description:

    This routine attaches the current process to the given shared memory object,
    and maps it into the process' address space.

Arguments:

    SharedMemoryObject - Supplies the value returned from shmget identifying
        the shared memory object.

    Address - Supplies an optional pointer to the address to map the object at.
        Supply NULL to allow the kernel to choose an address. If SHM_RND is
        supplied in the flags, this address may be rounded down to the nearest
        page. Otherwise, this address must be page aligned.

    Flags - Supplies a bitfield of flags governing the mapping.

Return Value:

    Returns a pointer to the mapped region on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
shmdt (
    const void *Address
    );

/*++

Routine Description:

    This routine detaches the current process from the shared memory object
    mapped at the given address, and unmaps the address.

Arguments:

    Address - Supplies a pointer to the base address the shared memory object
        is mapped at.

Return Value:

    0 on success. The mapping will no longer be valid.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
shmctl (
    int SharedMemoryObject,
    int Command,
    struct shmid_ds *Buffer
    );

/*++

Routine Description:

    This routine performs a control function on the given shared memory object.

Arguments:

    SharedMemoryObject - Supplies the identifier returned by shmget.

    Command - Supplies the control command to execute. See IPC_* definitions.

    Buffer - Supplies a pointer to the shared memory information buffer.

Return Value:

    0 on success.

    -1 on error, and errno will be set to the stat error information for the
    given file path.

--*/

#ifdef __cplusplus

}

#endif
#endif

