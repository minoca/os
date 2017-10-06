/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    mman.h

Abstract:

    This header contains definitions for memory management operations.

Author:

    Chris Stevens 6-Mar-2014

--*/

#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

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
// Define protection options for memory mapped files and shared memory objects.
//

//
// Map the file or object so that it cannot be accessed.
//

#define PROT_NONE 0x0000

//
// Map the file or object so that data can be read from it.
//

#define PROT_READ 0x0001

//
// Map the file or object so that data can be written to it.
//

#define PROT_WRITE 0x0002

//
// Map the file or object so that it can be executed.
//

#define PROT_EXEC 0x0004

//
// Define mapping flags for memory mapped files and shared memory objects.
//

//
// Map the file or object so that changes modify the underlying object.
//

#define MAP_SHARED 0x0001

//
// Map the file or object so that changes are only visible to the modifying
// process.
//

#define MAP_PRIVATE 0x0002

//
// Map the file or object at the supplied virtual address.
//

#define MAP_FIXED 0x0004

//
// Create a mapping that is not backed by a file or object. The supplied file
// descriptor and offset are ignored.
//

#define MAP_ANONYMOUS 0x0008
#define MAP_ANON MAP_ANONYMOUS

//
// Define flags use for memory synchronization.
//

//
// Perform the synchronization with asynchronous writes.
//

#define MS_ASYNC 0x0001

//
// Perform the synchronization with synchronous writes.
//

#define MS_SYNC 0x0002

//
// Set this flag to invalidate all cached copies of mapped data that are
// inconsistent with the permanent storage locations such that subsequent
// references obtain data consistent with permanent storage sometime between
// the call to msync and its first subsequent memory reference.
//
// In practice this flag does nothing.
//

#define MS_INVALIDATE 0x0004

//
// Define the value used to indicate a failed mapping.
//

#define MAP_FAILED ((void *)-1)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
void *
mmap (
    void *Address,
    size_t Length,
    int ProtectionFlags,
    int MapFlags,
    int FileDescriptor,
    off_t Offset
    );

/*++

Routine Description:

    This routine maps the given file or memory object into the current process'
    address space.

Arguments:

    Address - Supplies an optional suggested virtual address for the mapping.
        If MAP_FIXED is supplied then the routine attempts to create the
        mapping at the exact address supplied.

    Length - Supplies the length, in bytes, of the region to map.

    ProtectionFlags - Supplies a set of flags ORed together. See PROT_* for
        definitions.

    MapFlags - Supplies a set of flags ORed together. See MAP_* for definitions.

    FileDescriptor - Supplies the file descriptor of the file or memory object
        to map.

    Offset - Supplies the offset, in bytes, within the file or memory object
        where the mapping should begin.

Return Value:

    Returns the address where the mapping was made on sucess.

    MAP_FAILED on failure. The errno variable will be set to indicate the error.

--*/

LIBC_API
int
munmap (
    void *Address,
    size_t Length
    );

/*++

Routine Description:

    This routine removes any mappings in the the current process' address space
    that are within the specified region.

Arguments:

    Address - Supplies the start of the address region to unmap.

    Length - Supplies the size, in bytes, of the region to unmap.

Return Value:

    Returns 0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

LIBC_API
int
mprotect (
    const void *Address,
    size_t Length,
    int ProtectionFlags
    );

/*++

Routine Description:

    This routine changes the memory protection attributes for the given region
    of memory.

Arguments:

    Address - Supplies the starting address (inclusive) to change the memory
        protection for. This must be aligned to a page boundary.

    Length - Supplies the length, in bytes, of the region to change attributes
        for.

    ProtectionFlags - Supplies a bitfield of flags describing the desired
        attributes of the region. See PROT_* definitions.

Return Value:

    0 on success.

    -1 on error, and errno is set to contain more information.

--*/

LIBC_API
int
msync (
    const void *Address,
    size_t Length,
    int Flags
    );

/*++

Routine Description:

    This routine synchronizes a region of the current process' memory address
    space with the permanent storage that backs it. If there is no storage
    backing the supplied region, than this routine has no effect.

Arguments:

    Address - Supplies the start of the address region to synchronize.

    Length - Supplies the size, in bytes, of the region to synchronize.

    Flags - Supplies a set of flags ORed together. See MS_* for definitions.

Return Value:

    Returns 0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

LIBC_API
int
shm_open (
    const char *Name,
    int OpenFlags,
    mode_t Mode
    );

/*++

Routine Description:

    This routine opens a shared memory object and connects it to a file
    descriptor.

Arguments:

    Name - Supplies a pointer to a null terminated string containing the name
        of the shared memory objecet.

    OpenFlags - Supplies a set of flags ORed together. See O_* definitions.

    Mode - Supplies the permissions mask to set if the shared memory object is
        to be created by this call.

Return Value:

    Returns a file descriptor on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

LIBC_API
int
shm_unlink (
    const char *Name
    );

/*++

Routine Description:

    This routine removes the shared memory object as identified by the given
    name from the namespace of shared memory objects.

Arguments:

    Name - Supplies a pointer to the name of the shared memory object to remove.

Return Value:

    Returns 0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

#ifdef __cplusplus

}

#endif
#endif

