/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dirio.c

Abstract:

    This module implements directory enumeration functionality.

Author:

    Evan Green 11-Mar-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

//
// ---------------------------------------------------------------- Definitions
//

#define DIRECTORY_BUFFER_SIZE 4096

//
// Define the initial guess for the current working directory buffer length.
//

#define WORKING_DIRECTORY_BUFFER_SIZE 256

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about about an open directory.

Members:

    Descriptor - Stores the file descriptor number.

    Buffer - Stores a pointer to the buffer used to store several enumeration
        entries.

    ValidBufferSize - Stores the number of bytes in the buffer that actually
        contain valid data.

    CurrentPosition - Stores the offset within the buffer where the next entry
        will come from.

    AtEnd - Stores a boolean indicating if the end of the current buffer
        represents the end of the entire directory.

    Entry - Stores a pointer to the dirent structure expected at the final
        output.

--*/

typedef struct _DIR {
    HANDLE Descriptor;
    PVOID Buffer;
    ULONG ValidBufferSize;
    ULONG CurrentPosition;
    BOOL AtEnd;
    struct dirent Entry;
} *PDIR;

//
// ----------------------------------------------- Internal Function Prototypes
//

PDIR
ClpCreateDirectoryStructure (
    VOID
    );

KSTATUS
ClpDestroyDirectoryStructure (
    PDIR Directory
    );

//
// -------------------------------------------------------------------- Globals
//

CHAR ClDirectoryEntryTypeConversions[IoObjectTypeCount] = {
    DT_UNKNOWN,
    DT_DIR,
    DT_REG,
    DT_BLK,
    DT_CHR,
    DT_FIFO,
    DT_DIR,
    DT_SOCK,
    DT_CHR,
    DT_CHR,
    DT_REG,
    DT_LNK
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
DIR *
opendir (
    const char *DirectoryName
    )

/*++

Routine Description:

    This routine opens a directory for reading.

Arguments:

    DirectoryName - Supplies a pointer to a null terminated string containing
        the name of the directory to open.

Return Value:

    Returns a pointer to the directory on success.

    NULL on failure.

--*/

{

    PDIR Directory;
    ULONG FilePathLength;
    ULONG Flags;
    KSTATUS Status;

    Directory = ClpCreateDirectoryStructure();
    if (Directory == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto opendirEnd;
    }

    FilePathLength = RtlStringLength((PSTR)DirectoryName);
    Flags = SYS_OPEN_FLAG_DIRECTORY | SYS_OPEN_FLAG_READ;
    Status = OsOpen(INVALID_HANDLE,
                    (PSTR)DirectoryName,
                    FilePathLength + 1,
                    Flags,
                    FILE_PERMISSION_NONE,
                    &(Directory->Descriptor));

    if (!KSUCCESS(Status)) {
        goto opendirEnd;
    }

opendirEnd:
    if (!KSUCCESS(Status)) {
        if (Directory != NULL) {
            ClpDestroyDirectoryStructure(Directory);
            Directory = NULL;
        }

        errno = ClConvertKstatusToErrorNumber(Status);
    }

    return Directory;
}

LIBC_API
DIR *
fdopendir (
    int FileDescriptor
    )

/*++

Routine Description:

    This routine opens a directory based on an already open file descriptor to
    a directory.

Arguments:

    FileDescriptor - Supplies a pointer to the open handle to the directory.

Return Value:

    0 on success.

    -1 on failure, and more details will be provided in errno.

--*/

{

    PDIR Directory;
    KSTATUS Status;

    Directory = ClpCreateDirectoryStructure();
    if (Directory == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto fdopendirEnd;
    }

    Status = OsFileControl((HANDLE)(UINTN)FileDescriptor,
                           FileControlCommandSetDirectoryFlag,
                           NULL);

    if (!KSUCCESS(Status)) {
        goto fdopendirEnd;
    }

    Directory->Descriptor = (HANDLE)(UINTN)FileDescriptor;
    Status = STATUS_SUCCESS;

fdopendirEnd:
    if (!KSUCCESS(Status)) {
        if (Directory != NULL) {
            ClpDestroyDirectoryStructure(Directory);
            Directory = NULL;
        }

        errno = ClConvertKstatusToErrorNumber(Status);
    }

    return Directory;
}

LIBC_API
int
closedir (
    DIR *Directory
    )

/*++

Routine Description:

    This routine closes an open directory.

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

Return Value:

    0 on success.

    -1 on failure, and more details will be provided in errno.

--*/

{

    KSTATUS Status;

    if (Directory == NULL) {
        Status = STATUS_SUCCESS;
        goto closedirEnd;
    }

    Status = ClpDestroyDirectoryStructure(Directory);

closedirEnd:
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
readdir_r (
    DIR *Directory,
    struct dirent *Buffer,
    struct dirent **Result
    )

/*++

Routine Description:

    This routine reads from a directory in a reentrant manner.

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

    Buffer - Supplies the buffer where the next directory entry will be
        returned.

    Result - Supplies a pointer that will either be set to the Buffer pointer
        if there are more entries, or NULL if there are no more entries in the
        directory.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    UINTN BytesRead;
    PDIRECTORY_ENTRY Entry;
    INT Error;
    UINTN NextEntryOffset;
    KSTATUS Status;

    *Result = NULL;
    if (Directory == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto readdir_rEnd;
    }

    //
    // If more data needs to be read, perform the underlying read.
    //

    if (Directory->CurrentPosition + sizeof(DIRECTORY_ENTRY) >
        Directory->ValidBufferSize) {

        //
        // If this is the end, return a null entry and success.
        //

        if (Directory->AtEnd != FALSE) {
            Status = STATUS_SUCCESS;
            goto readdir_rEnd;
        }

        Status = OsPerformIo(Directory->Descriptor,
                             IO_OFFSET_NONE,
                             DIRECTORY_BUFFER_SIZE,
                             0,
                             SYS_WAIT_TIME_INDEFINITE,
                             Directory->Buffer,
                             &BytesRead);

        if (!KSUCCESS(Status)) {
            goto readdir_rEnd;
        }

        if (BytesRead == 0) {
            Directory->AtEnd = TRUE;
            goto readdir_rEnd;
        }

        Directory->ValidBufferSize = (ULONG)BytesRead;
        Directory->CurrentPosition = 0;

        //
        // Make sure there is enough space for a new directory entry.
        //

        if (Directory->CurrentPosition + sizeof(DIRECTORY_ENTRY) >
            Directory->ValidBufferSize) {

            Status = STATUS_BUFFER_OVERRUN;
            goto readdir_rEnd;
        }
    }

    //
    // Grab the next directory entry.
    //

    Entry = (PDIRECTORY_ENTRY)(Directory->Buffer + Directory->CurrentPosition);
    NextEntryOffset = Directory->CurrentPosition + Entry->Size;
    if (NextEntryOffset > Directory->ValidBufferSize) {
        Status = STATUS_BUFFER_OVERRUN;
        goto readdir_rEnd;
    }

    if (Entry->Size - sizeof(DIRECTORY_ENTRY) > NAME_MAX) {
        Status = STATUS_BUFFER_OVERRUN;
        goto readdir_rEnd;
    }

    Buffer->d_ino = Entry->FileId;
    Buffer->d_off = Entry->NextOffset;
    Buffer->d_reclen = Entry->Size;

    //
    // Please update the array (and this assert) if a new I/O object type is
    // added.
    //

    assert(IoObjectSymbolicLink + 1 == IoObjectTypeCount);

    Buffer->d_type = ClDirectoryEntryTypeConversions[Entry->Type];
    RtlStringCopy((PSTR)&(Buffer->d_name), (PSTR)(Entry + 1), NAME_MAX);

    assert(strchr(Buffer->d_name, '/') == NULL);

    //
    // Move on to the next entry.
    //

    Directory->CurrentPosition = NextEntryOffset;
    Status = STATUS_SUCCESS;
    *Result = Buffer;

readdir_rEnd:
    if (!KSUCCESS(Status)) {
        Error = ClConvertKstatusToErrorNumber(Status);

    } else {
        Error = 0;
    }

    return Error;
}

LIBC_API
struct dirent *
readdir (
    DIR *Directory
    )

/*++

Routine Description:

    This routine reads the next directory entry from the open directory
    stream.

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

Return Value:

    Returns a pointer to the next directory entry on success.

    NULL on failure or when the end of the directory is reached. On failure,
        errno is set. If the end of the directory is reached, errno is not
        changed.

--*/

{

    int Error;
    struct dirent *NextEntry;

    Error = readdir_r(Directory, &(Directory->Entry), &NextEntry);
    if (Error != 0) {
        errno = Error;
    }

    return NextEntry;
}

LIBC_API
void
seekdir (
    DIR *Directory,
    long Location
    )

/*++

Routine Description:

    This routine seeks directory to the given location. The location must have
    been returned from a previous call to telldir, otherwise the results are
    undefined.

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

    Location - Supplies the location within the directory to seek to as given
        by the telldir function.

Return Value:

    None. No errors are defined.

--*/

{

    OsSeek(Directory->Descriptor,
           SeekCommandFromBeginning,
           Location,
           NULL);

    Directory->ValidBufferSize = 0;
    Directory->CurrentPosition = 0;
    Directory->AtEnd = FALSE;
    return;
}

LIBC_API
long
telldir (
    DIR *Directory
    )

/*++

Routine Description:

    This routine returns the current position within a directory. This position
    can be seeked to later (in fact, the return value from this function is the
    only valid parameter to pass to seekdir).

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

Return Value:

    Returns the current location of the specified directory stream.

--*/

{

    IO_OFFSET Offset;
    KSTATUS Status;

    Status = OsSeek(Directory->Descriptor,
                    SeekCommandNop,
                    0,
                    &Offset);

    if (!KSUCCESS(Status)) {
        return 0;
    }

    return (long)Offset;
}

LIBC_API
void
rewinddir (
    DIR *Directory
    )

/*++

Routine Description:

    This routine rewinds a directory back to the beginning.

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

Return Value:

    None.

--*/

{

    seekdir(Directory, 0);
    return;
}

LIBC_API
int
dirfd (
    DIR *Directory
    )

/*++

Routine Description:

    This routine returns the file descriptor backing the given directory.

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

Return Value:

    None.

--*/

{

    if ((Directory == NULL) || (Directory->Descriptor == INVALID_HANDLE)) {
        errno = EINVAL;
        return -1;
    }

    return (int)(UINTN)(Directory->Descriptor);
}

LIBC_API
int
rmdir (
    const char *Path
    )

/*++

Routine Description:

    This routine attempts to unlink a directory. The directory must be empty or
    the operation will fail.

Arguments:

    Path - Supplies a pointer to a null terminated string containing the path
        of the directory to remove.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. In failure
    cases, the directory will not be removed.

--*/

{

    return unlinkat(AT_FDCWD, Path, AT_REMOVEDIR);
}

LIBC_API
char *
getcwd (
    char *Buffer,
    size_t BufferSize
    )

/*++

Routine Description:

    This routine returns a pointer to a null terminated string containing the
    path to the current working directory.

Arguments:

    Buffer - Supplies a pointer to a buffer where the string should be returned.
        If NULL is supplied, then malloc will be used to allocate a buffer of
        the appropriate size, and it is therefore the caller's responsibility
        to free this memory.

    BufferSize - Supplies the size of the buffer, in bytes.

Return Value:

    Returns a pointer to a string containing the current working directory on
    success.

    NULL on failure. Errno will contain more information.

--*/

{

    PSTR Directory;
    UINTN DirectorySize;
    KSTATUS Status;

    Status = OsGetCurrentDirectory(FALSE, &Directory, &DirectorySize);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return NULL;
    }

    //
    // If a buffer was supplied, try to use it.
    //

    if (Buffer != NULL) {

        //
        // The size argument is required if a buffer was supplied.
        //

        if (BufferSize == 0) {
            errno = EINVAL;
            Buffer = NULL;

        //
        // If a size was supplied but is too small, notify the caller.
        //

        } else if (BufferSize < DirectorySize) {
            errno = ERANGE;
            Buffer = NULL;

        //
        // Otherwise copy the current directory into the supplied buffer.
        //

        } else {
            memcpy(Buffer, Directory, DirectorySize);
        }

    //
    // If there is no provided buffer, then allocate a buffer of the
    // appropriate size and copy the directory into it. Ignore the supplied
    // buffer size.
    //

    } else {
        Buffer = malloc(DirectorySize);
        if (Buffer != NULL) {
            memcpy(Buffer, Directory, DirectorySize);

        } else {
            errno = ENOMEM;
        }
    }

    //
    // Free the allocated directory and return the user's buffer.
    //

    if (Directory != NULL) {
        OsHeapFree(Directory);
        Directory = NULL;
    }

    return Buffer;
}

LIBC_API
int
chdir (
    const char *Path
    )

/*++

Routine Description:

    This routine changes the current working directory (the starting point for
    all paths that don't begin with a path separator).

Arguments:

    Path - Supplies a pointer to the null terminated string containing the
        path of the new working directory.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. On failure,
    the current working directory will not be changed.

--*/

{

    ULONG PathSize;
    KSTATUS Status;

    if (Path == NULL) {
        errno = EINVAL;
        return -1;
    }

    PathSize = strlen((PSTR)Path) + 1;
    Status = OsChangeDirectory(FALSE, (PSTR)Path, PathSize);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
fchdir (
    int FileDescriptor
    )

/*++

Routine Description:

    This routine changes the current working directory (the starting point for
    all paths that don't begin with a path separator) using an already open
    file descriptor to that directory.

Arguments:

    FileDescriptor - Supplies the open file handle to the directory to change
        to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. On failure,
    the current working directory will not be changed.

--*/

{

    KSTATUS Status;

    Status = OsChangeDirectoryHandle(FALSE, (HANDLE)(UINTN)FileDescriptor);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
chroot (
    const char *Path
    )

/*++

Routine Description:

    This routine changes the current root directory. The working directory is
    not changed. The caller must have sufficient privileges to change root
    directories.

Arguments:

    Path - Supplies a pointer to the null terminated string containing the
        path of the new root directory.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. On failure,
    the current root directory will not be changed. Errno may be set to the
    following values, among others:

    EACCES if search permission is denied on a component of the path prefix.

    EPERM if the caller has insufficient privileges.

--*/

{

    ULONG PathSize;
    KSTATUS Status;

    //
    // This is a Minoca extension. If the caller passes NULL, then this routine
    // will try to escape the current root. This is only possible if the caller
    // has the permission to escape roots.
    //

    if (Path == NULL) {
        Status = OsChangeDirectory(TRUE, NULL, 0);

    } else {
        PathSize = strlen((PSTR)Path) + 1;
        Status = OsChangeDirectory(TRUE, (PSTR)Path, PathSize);
    }

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
fchroot (
    int FileDescriptor
    )

/*++

Routine Description:

    This routine changes the current root directory using an already open file
    descriptor to that directory. The caller must have sufficient privileges
    to change root directories.

Arguments:

    FileDescriptor - Supplies the open file handle to the directory to change
        to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. On failure,
    the current root directory will not be changed.

--*/

{

    KSTATUS Status;

    Status = OsChangeDirectoryHandle(TRUE, (HANDLE)(UINTN)FileDescriptor);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

PDIR
ClpCreateDirectoryStructure (
    VOID
    )

/*++

Routine Description:

    This routine creates a directory structure.

Arguments:

    None.

Return Value:

    Returns a pointer to the directory structure on success.

    NULL on allocation failure.

--*/

{

    PDIR Directory;
    KSTATUS Status;

    Status = STATUS_INSUFFICIENT_RESOURCES;
    Directory = malloc(sizeof(DIR));
    if (Directory == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDirectoryStructureEnd;
    }

    RtlZeroMemory(Directory, sizeof(DIR));
    Directory->Descriptor = INVALID_HANDLE;
    Directory->Buffer = malloc(DIRECTORY_BUFFER_SIZE + 1);
    if (Directory->Buffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDirectoryStructureEnd;
    }

    Status = STATUS_SUCCESS;

CreateDirectoryStructureEnd:
    if (!KSUCCESS(Status)) {
        if (Directory != NULL) {
            ClpDestroyDirectoryStructure(Directory);
            Directory = NULL;
        }
    }

    return Directory;
}

KSTATUS
ClpDestroyDirectoryStructure (
    PDIR Directory
    )

/*++

Routine Description:

    This routine destroys a directory structure.

Arguments:

    Directory - Supplies a pointer to the directory structure to destroy.

Return Value:

    Returns the resulting status code from the close function.

--*/

{

    KSTATUS Status;

    Status = STATUS_INVALID_HANDLE;
    if (Directory->Descriptor != INVALID_HANDLE) {
        Status = OsClose(Directory->Descriptor);
    }

    if (Directory->Buffer != NULL) {
        free(Directory->Buffer);
    }

    free(Directory);
    return Status;
}

