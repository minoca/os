/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
#include <paths.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
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

KSTATUS
ClpOpenSharedMemoryObject (
    HANDLE Directory,
    PCSTR Path,
    ULONG PathLength,
    ULONG Flags,
    FILE_PERMISSIONS CreatePermissions,
    PHANDLE Handle
    );

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

    if (Length == 0) {
        errno = EINVAL;
        goto mmapEnd;
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

LIBC_API
int
shmget (
    key_t Key,
    size_t Size,
    int Flags
    )

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

{

    FILE_PERMISSIONS CreatePermissions;
    FILE_CONTROL_PARAMETERS_UNION FileControl;
    HANDLE FileHandle;
    FILE_PROPERTIES FileProperties;
    CHAR Name[14];
    INT NameLength;
    ULONG OsOpenFlags;
    KSTATUS Status;
    struct timespec Time;

    FileHandle = INVALID_HANDLE;
    if (Key == IPC_PRIVATE) {
        clock_gettime(CLOCK_MONOTONIC, &Time);
        Key = Time.tv_sec ^ Time.tv_nsec;
        NameLength = snprintf(Name, sizeof(Name), "shmp_%08x", Key);

    } else {
        NameLength = snprintf(Name, sizeof(Name), "shm_%08x", Key);
    }

    CreatePermissions = 0;
    NameLength = RtlStringLength((PSTR)Name) + 1;
    CreatePermissions = Flags & FILE_PERMISSION_ALL;
    OsOpenFlags = SYS_OPEN_FLAG_SHARED_MEMORY |
                  SYS_OPEN_FLAG_READ | SYS_OPEN_FLAG_WRITE |
                  SYS_OPEN_FLAG_EXECUTE;

    if ((Flags & IPC_CREAT) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_CREATE;
    }

    if ((Flags & IPC_EXCL) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_FAIL_IF_EXISTS;
    }

    ASSERT_FILE_PERMISSIONS_EQUIVALENT();

    Status = ClpOpenSharedMemoryObject(INVALID_HANDLE,
                                       Name,
                                       NameLength,
                                       OsOpenFlags,
                                       CreatePermissions,
                                       &FileHandle);

    if (!KSUCCESS(Status)) {
        goto shmgetEnd;
    }

    //
    // Get the file information in order to set the size. The size may be
    // non-zero if the object already existed, in which case validated that it
    // is at least the requested size.
    //

    FileControl.SetFileInformation.FileProperties = &FileProperties;
    Status = OsFileControl(FileHandle,
                           FileControlCommandGetFileInformation,
                           &FileControl);

    if (!KSUCCESS(Status)) {
        goto shmgetEnd;
    }

    //
    // Size the new shared memory object.
    //

    if ((FileProperties.Size == 0) && (Size != 0)) {
        FileProperties.Size = Size;
        FileControl.SetFileInformation.FieldsToSet =
                                                 FILE_PROPERTY_FIELD_FILE_SIZE;

        Status = OsFileControl(FileHandle,
                               FileControlCommandSetFileInformation,
                               &FileControl);

        if (!KSUCCESS(Status)) {
            goto shmgetEnd;
        }

    } else if (FileProperties.Size < Size) {
        Status = STATUS_INVALID_PARAMETER;
        goto shmgetEnd;
    }

shmgetEnd:
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        if (FileHandle != INVALID_HANDLE) {
            OsDelete(INVALID_HANDLE,
                     Name,
                     NameLength,
                     SYS_DELETE_FLAG_SHARED_MEMORY);

            OsClose(FileHandle);
        }

        return -1;
    }

    return (int)(UINTN)FileHandle;
}

LIBC_API
void *
shmat (
    int SharedMemoryObject,
    const void *Address,
    int Flags
    )

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

{

    PVOID MappedAddress;
    ULONG OsMapFlags;
    UINTN PageSize;
    KSTATUS Status;

    MappedAddress = (PVOID)-1;
    OsMapFlags = SYS_MAP_FLAG_READ | SYS_MAP_FLAG_SHARED;
    if (((Flags & SHM_RND) != 0) && (Address != NULL)) {
        PageSize = SHMLBA;
        Address = ALIGN_POINTER_DOWN(Address, PageSize);
    }

    //
    // Convert the flags to protection flags.
    //

    if ((Flags & SHM_EXEC) != 0) {
        OsMapFlags |= SYS_MAP_FLAG_EXECUTE;
    }

    if ((Flags & SHM_RDONLY) == 0) {
        OsMapFlags |= SYS_MAP_FLAG_WRITE;
    }

    if (Address != NULL) {
        OsMapFlags |= SYS_MAP_FLAG_FIXED;
    }

    Status = OsMemoryMap((HANDLE)(UINTN)SharedMemoryObject,
                         0,
                         0,
                         OsMapFlags,
                         (PVOID *)&Address);

    if (!KSUCCESS(Status)) {
        goto shmatEnd;
    }

    MappedAddress = (PVOID)Address;

shmatEnd:
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
    }

    return MappedAddress;
}

LIBC_API
int
shmdt (
    const void *Address
    )

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

{

    return munmap((void *)Address, 0);
}

LIBC_API
int
shmctl (
    int SharedMemoryObject,
    int Command,
    struct shmid_ds *Buffer
    )

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

{

    KSTATUS Status;

    Status = OsUserControl((HANDLE)(UINTN)SharedMemoryObject,
                           Command,
                           Buffer,
                           sizeof(struct shmid_ds));

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
key_t
ftok (
    const char *Path,
    int ProjectId
    )

/*++

Routine Description:

    This routine uses the identify of the given file and the least significant
    8 bits of the given project identifier to create a key suitable for use in
    the shmget, semget, or msgget functions.

Arguments:

    Path - Supplies a pointer to the path to the file whose identity should be
        involved in the key ID.

    ProjectId - Supplies an identifier whose least significant 8 bits will be
        worked into the result.

Return Value:

    Returns a key value suitable for use as a parameter to shmget, semget, or
    msgget.

    -1 on error, and errno will be set to the stat error information for the
    given file path.

--*/

{

    key_t Result;
    struct stat Stat;

    if (stat(Path, &Stat) != 0) {
        return -1;
    }

    Result = (Stat.st_ino & 0xFFFF) |
             ((Stat.st_dev & 0xFF) << 16) |
             ((ProjectId & 0xFF) << 24);

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
ClpOpenSharedMemoryObject (
    HANDLE Directory,
    PCSTR Path,
    ULONG PathLength,
    ULONG Flags,
    FILE_PERMISSIONS CreatePermissions,
    PHANDLE Handle
    )

/*++

Routine Description:

    This routine opens a shared memory object. It may try several times to get
    the best possible set of permissions.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to a string containing the path of the object
        to open.

    PathLength - Supplies the length of the path buffer, in bytes, including
        the null terminator.

    Flags - Supplies flags associated with the open operation. See
        SYS_OPEN_FLAG_* definitions.

    CreatePermissions - Supplies the permissions for create operations.

    Handle - Supplies a pointer where a handle will be returned on success.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Open with full permissions. This routine isn't very fast, but then again
    // SystemV shared memory objects are pretty crusty and old, and usually
    // someone attempting to open them has the proper access.
    //

    Status = OsOpen(INVALID_HANDLE,
                    Path,
                    PathLength,
                    Flags,
                    CreatePermissions,
                    Handle);

    if (Status == STATUS_ACCESS_DENIED) {

        //
        // Try opening without the new and more obscure execute permissions.
        //

        Flags &= ~SYS_OPEN_FLAG_EXECUTE;
        Status = OsOpen(INVALID_HANDLE,
                        Path,
                        PathLength,
                        Flags,
                        CreatePermissions,
                        Handle);

        if (Status == STATUS_ACCESS_DENIED) {

            //
            // Okay, things are getting desperate. Try to just open for read.
            //

            Flags &= ~SYS_OPEN_FLAG_WRITE;
            Status = OsOpen(INVALID_HANDLE,
                            Path,
                            PathLength,
                            Flags,
                            CreatePermissions,
                            Handle);
        }
    }

    return Status;
}

