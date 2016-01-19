/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    memory.c

Abstract:

    This module implements memory management routines.

Author:

    Chris Stevens 6-Mar-2014

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
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
    )

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

{

    PVOID MappedAddress;
    ULONG OsMapFlags;
    KSTATUS Status;

    MappedAddress = MAP_FAILED;
    OsMapFlags = 0;

    //
    // Convert the protection flags to the OS flags.
    //

    if ((ProtectionFlags & PROT_READ) != 0) {
        OsMapFlags |= SYS_MAP_FLAG_READ;
    }

    if ((ProtectionFlags & PROT_WRITE) != 0) {
        OsMapFlags |= SYS_MAP_FLAG_WRITE;
    }

    if ((ProtectionFlags & PROT_EXEC) != 0) {
        OsMapFlags |= SYS_MAP_FLAG_EXECUTE;
    }

    //
    // Convert the mapping flags to OS flags. One of MAP_SHARED or MAP_PRIVATE
    // must be supplied, but not both.
    //

    if ((MapFlags & MAP_SHARED) != 0) {
        if ((MapFlags & MAP_PRIVATE) != 0) {
            errno = EINVAL;
            goto mmapEnd;
        }

        OsMapFlags |= SYS_MAP_FLAG_SHARED;

    } else if ((MapFlags & MAP_PRIVATE) == 0) {
        errno = EINVAL;
        goto mmapEnd;
    }

    if ((MapFlags & MAP_FIXED) != 0) {
        OsMapFlags |= SYS_MAP_FLAG_FIXED;
    }

    if ((MapFlags & MAP_ANONYMOUS) != 0) {
        OsMapFlags |= SYS_MAP_FLAG_ANONYMOUS;
    }

    Status = OsMemoryMap((HANDLE)(UINTN)FileDescriptor,
                         Offset,
                         Length,
                         OsMapFlags,
                         &Address);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        goto mmapEnd;
    }

    MappedAddress = Address;

mmapEnd:
    return MappedAddress;
}

LIBC_API
int
munmap (
    void *Address,
    size_t Length
    )

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

{

    KSTATUS Status;

    Status = OsMemoryUnmap(Address, Length);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
mprotect (
    const void *Address,
    size_t Length,
    int ProtectionFlags
    )

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

{

    ULONG Flags;
    KSTATUS Status;

    //
    // Convert the protection flags to the OS flags.
    //

    Flags = 0;
    if ((ProtectionFlags & PROT_READ) != 0) {
        Flags |= SYS_MAP_FLAG_READ;
    }

    if ((ProtectionFlags & PROT_WRITE) != 0) {
        Flags |= SYS_MAP_FLAG_WRITE;
    }

    if ((ProtectionFlags & PROT_EXEC) != 0) {
        Flags |= SYS_MAP_FLAG_EXECUTE;
    }

    Status = OsSetMemoryProtection((PVOID)Address, Length, Flags);
    if (!KSUCCESS(Status)) {

        //
        // This routine is supposed to return EAGAIN if there insufficient
        // kernel resources.
        //

        if (Status == STATUS_INSUFFICIENT_RESOURCES) {
            Status = STATUS_TRY_AGAIN;
        }

        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
msync (
    const void *Address,
    size_t Length,
    int Flags
    )

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

{

    ULONG OsFlags;
    KSTATUS Status;

    OsFlags = 0;
    if ((Flags & MS_ASYNC) != 0) {
        if ((Flags & MS_SYNC) != 0) {
            errno = EINVAL;
            return -1;
        }

        OsFlags |= SYS_MAP_FLUSH_FLAG_ASYNC;
    }

    Status = OsMemoryFlush((PVOID)Address, Length, OsFlags);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
shm_open (
    const char *Name,
    int OpenFlags,
    mode_t Mode
    )

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

{

    FILE_PERMISSIONS CreatePermissions;
    HANDLE FileHandle;
    ULONG NameLength;
    ULONG OsOpenFlags;
    KSTATUS Status;

    CreatePermissions = 0;
    NameLength = RtlStringLength((PSTR)Name) + 1;
    OsOpenFlags = 0;

    //
    // Set the access mask.
    //

    if ((OpenFlags & O_ACCMODE) == O_RDONLY) {
        OsOpenFlags |= SYS_OPEN_FLAG_READ;

    } else if ((OpenFlags & O_ACCMODE) == O_RDWR) {
        OsOpenFlags |= SYS_OPEN_FLAG_READ | SYS_OPEN_FLAG_WRITE;

    } else {
        errno = EINVAL;
        return -1;
    }

    if ((OpenFlags & O_TRUNC) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_TRUNCATE;
    }

    //
    // Set other flags.
    //

    if ((OpenFlags & O_CREAT) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_CREATE;
        if ((OpenFlags & O_EXCL) != 0) {
            OsOpenFlags |= SYS_OPEN_FLAG_FAIL_IF_EXISTS;
        }

        ASSERT_FILE_PERMISSIONS_EQUIVALENT();

        CreatePermissions = Mode;
    }

    OsOpenFlags |= SYS_OPEN_FLAG_SHARED_MEMORY;
    Status = OsOpen(INVALID_HANDLE,
                    (PSTR)Name,
                    NameLength,
                    OsOpenFlags,
                    CreatePermissions,
                    &FileHandle);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return (int)(UINTN)FileHandle;
}

LIBC_API
int
shm_unlink (
    const char *Name
    )

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

{

    KSTATUS Status;

    Status = OsDelete(INVALID_HANDLE,
                      (PSTR)Name,
                      strlen(Name) + 1,
                      SYS_DELETE_FLAG_SHARED_MEMORY);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions

