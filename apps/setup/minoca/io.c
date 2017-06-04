/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    io.c

Abstract:

    This module implements support for doing I/O on Minoca OS in the setup
    application.

Author:

    Evan Green 11-Apr-2014

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <utime.h>

#include "../setup.h"
#include <minoca/lib/mlibc.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes a handle to an I/O object in the setup app.

Members:

    Handle - Stores the device handle.

--*/

typedef struct _SETUP_OS_HANDLE {
    HANDLE Handle;
} SETUP_OS_HANDLE, *PSETUP_OS_HANDLE;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
SetupOsReadLink (
    PSTR Path,
    PSTR *LinkTarget,
    INT *LinkTargetSize
    )

/*++

Routine Description:

    This routine attempts to read a symbolic link.

Arguments:

    Path - Supplies a pointer to the path to open.

    LinkTarget - Supplies a pointer where an allocated link target will be
        returned on success. The caller is responsible for freeing this memory.

    LinkTargetSize - Supplies a pointer where the size of the link target will
        be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    int Size;

    *LinkTarget = malloc(SETUP_SYMLINK_MAX);
    if (*LinkTarget == NULL) {
        return errno;
    }

    Size = readlink(Path, *LinkTarget, SETUP_SYMLINK_MAX - 1);
    if (Size < 0) {
        free(*LinkTarget);
        *LinkTarget = NULL;
        return errno;
    }

    (*LinkTarget)[Size] = '\0';
    *LinkTargetSize = Size;
    return 0;
}

INT
SetupOsSymlink (
    PSTR Path,
    PSTR LinkTarget,
    INT LinkTargetSize
    )

/*++

Routine Description:

    This routine creates a symbolic link.

Arguments:

    Path - Supplies a pointer to the path of the symbolic link to create.

    LinkTarget - Supplies a pointer to the target of the link.

    LinkTargetSize - Supplies a the size of the link target buffer in bytes.

Return Value:

    Returns the link size on success.

    -1 on failure.

--*/

{

    INT Result;

    //
    // Create the symlink. If it already exists, attempt to unlink that file
    // and create a new one.
    //

    Result = symlink(LinkTarget, Path);
    if ((Result < 0) && (errno == EEXIST)) {
        if (unlink(LinkTarget) == 0) {
            Result = symlink(LinkTarget, Path);
        }
    }

    return Result;
}

PVOID
SetupOsOpenDestination (
    PSETUP_DESTINATION Destination,
    INT Flags,
    INT CreatePermissions
    )

/*++

Routine Description:

    This routine opens a handle to a given destination.

Arguments:

    Destination - Supplies a pointer to the destination to open.

    Flags - Supplies open flags. See O_* definitions.

    CreatePermissions - Supplies optional create permissions.

Return Value:

    Returns a pointer to an opaque context on success.

    NULL on failure.

--*/

{

    PSETUP_OS_HANDLE IoHandle;
    KSTATUS Status;

    IoHandle = malloc(sizeof(SETUP_OS_HANDLE));
    if (IoHandle == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    memset(IoHandle, 0, sizeof(SETUP_OS_HANDLE));
    IoHandle->Handle = INVALID_HANDLE;
    if (Destination->Path != NULL) {
        IoHandle->Handle = (HANDLE)(UINTN)open(Destination->Path,
                                               Flags,
                                               CreatePermissions);

        if (IoHandle->Handle == (HANDLE)-1) {
            free(IoHandle);
            return NULL;
        }

    } else {
        Status = OsOpenDevice(Destination->DeviceId,
                              SYS_OPEN_FLAG_READ | SYS_OPEN_FLAG_WRITE,
                              &(IoHandle->Handle));

        if (!KSUCCESS(Status)) {
            free(IoHandle);
            errno = ClConvertKstatusToErrorNumber(Status);
            return NULL;
        }
    }

    return IoHandle;
}

VOID
SetupOsClose (
    PVOID Handle
    )

/*++

Routine Description:

    This routine closes a handle.

Arguments:

    Handle - Supplies a pointer to the destination to open.

Return Value:

    None.

--*/

{

    PSETUP_OS_HANDLE IoHandle;

    IoHandle = Handle;
    if (IoHandle->Handle != INVALID_HANDLE) {
        OsClose(IoHandle->Handle);
    }

    free(IoHandle);
    return;
}

ssize_t
SetupOsRead (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine reads from an open handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer where the read bytes will be returned.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes read.

    -1 on failure.

--*/

{

    UINTN BytesCompleted;
    PSETUP_OS_HANDLE IoHandle;
    KSTATUS Status;

    IoHandle = Handle;
    Status = OsPerformIo(IoHandle->Handle,
                         IO_OFFSET_NONE,
                         ByteCount,
                         0,
                         SYS_WAIT_TIME_INDEFINITE,
                         Buffer,
                         &BytesCompleted);

    if ((!KSUCCESS(Status)) && (Status != STATUS_END_OF_FILE)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        if (BytesCompleted == 0) {
            BytesCompleted = -1;
        }
    }

    return (ssize_t)BytesCompleted;
}

ssize_t
SetupOsWrite (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine writes data to an open handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer to the bytes to write.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes written.

    -1 on failure.

--*/

{

    UINTN BytesCompleted;
    PSETUP_OS_HANDLE IoHandle;
    KSTATUS Status;

    IoHandle = Handle;
    Status = OsPerformIo(IoHandle->Handle,
                         IO_OFFSET_NONE,
                         ByteCount,
                         SYS_IO_FLAG_WRITE,
                         SYS_WAIT_TIME_INDEFINITE,
                         Buffer,
                         &BytesCompleted);

    if ((!KSUCCESS(Status)) && (Status != STATUS_END_OF_FILE)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        if (BytesCompleted == 0) {
            BytesCompleted = -1;
        }
    }

    return (ssize_t)BytesCompleted;
}

LONGLONG
SetupOsSeek (
    PVOID Handle,
    LONGLONG Offset
    )

/*++

Routine Description:

    This routine seeks in the current file or device.

Arguments:

    Handle - Supplies the handle.

    Offset - Supplies the new offset to set.

Return Value:

    Returns the resulting file offset after the operation.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

{

    PSETUP_OS_HANDLE IoHandle;
    IO_OFFSET NewOffset;
    SEEK_COMMAND SeekCommand;
    KSTATUS Status;

    IoHandle = Handle;
    SeekCommand = SeekCommandFromBeginning;
    Status = OsSeek(IoHandle->Handle, SeekCommand, Offset, &NewOffset);
    if (!KSUCCESS(Status)) {
        goto OsSeekEnd;
    }

OsSeekEnd:
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return NewOffset;
}

LONGLONG
SetupOsTell (
    PVOID Handle
    )

/*++

Routine Description:

    This routine returns the current offset in the given file or device.

Arguments:

    Handle - Supplies the handle.

Return Value:

    Returns the file offset on success.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

{

    PSETUP_OS_HANDLE IoHandle;
    LONGLONG Offset;
    KSTATUS Status;

    IoHandle = Handle;
    Status = OsSeek(IoHandle->Handle, SeekCommandFromCurrentOffset, 0, &Offset);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return Offset;
}

INT
SetupOsFstat (
    PVOID Handle,
    PULONGLONG FileSize,
    time_t *ModificationDate,
    mode_t *Mode
    )

/*++

Routine Description:

    This routine gets details for the given open file.

Arguments:

    Handle - Supplies the handle.

    FileSize - Supplies an optional pointer where the file size will be
        returned on success.

    ModificationDate - Supplies an optional pointer where the file's
        modification date will be returned on success.

    Mode - Supplies an optional pointer where the file's mode information will
        be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSETUP_OS_HANDLE IoHandle;
    int Result;
    struct stat Stat;

    IoHandle = Handle;
    Result = fstat((int)(UINTN)(IoHandle->Handle), &Stat);
    if (Result != 0) {
        return Result;
    }

    if (FileSize != NULL) {
        *FileSize = Stat.st_size;
    }

    if (ModificationDate != NULL) {
        *ModificationDate = Stat.st_mtime;
    }

    if (Mode != NULL) {
        *Mode = Stat.st_mode;
    }

    return Result;
}

INT
SetupOsFtruncate (
    PVOID Handle,
    ULONGLONG NewSize
    )

/*++

Routine Description:

    This routine sets the file size of the given file.

Arguments:

    Handle - Supplies the handle.

    NewSize - Supplies the new file size.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSETUP_OS_HANDLE IoHandle;
    int Result;

    IoHandle = Handle;
    Result = ftruncate((int)(UINTN)(IoHandle->Handle), NewSize);
    if (Result != 0) {
        return Result;
    }

    return Result;
}

INT
SetupOsEnumerateDirectory (
    PVOID Handle,
    PSTR DirectoryPath,
    PSTR *Enumeration
    )

/*++

Routine Description:

    This routine enumerates the contents of a given directory.

Arguments:

    Handle - Supplies the open volume handle.

    DirectoryPath - Supplies a pointer to a string containing the path to the
        directory to enumerate.

    Enumeration - Supplies a pointer where a pointer to a sequence of
        strings will be returned containing the files in the directory. The
        sequence will be terminated by an empty string. The caller is
        responsible for freeing this memory when done.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR Array;
    size_t ArrayCapacity;
    DIR *Directory;
    struct dirent *DirectoryEntry;
    size_t NameSize;
    PVOID NewBuffer;
    size_t NewCapacity;
    INT Result;
    size_t UsedSize;

    Array = NULL;
    ArrayCapacity = 0;
    Directory = NULL;
    UsedSize = 0;
    Directory = opendir(DirectoryPath);
    if (Directory == NULL) {
        Result = errno;
        goto OsEnumerateDirectoryEnd;
    }

    //
    // Loop reading directory entries.
    //

    while (TRUE) {
        errno = 0;
        DirectoryEntry = readdir(Directory);
        if (DirectoryEntry == NULL) {
            if (errno != 0) {
                Result = errno;
                goto OsEnumerateDirectoryEnd;
            }

            NameSize = 1;

        } else {
            if ((strcmp(DirectoryEntry->d_name, ".") == 0) ||
                (strcmp(DirectoryEntry->d_name, "..") == 0)) {

                continue;
            }

            NameSize = strlen(DirectoryEntry->d_name) + 1;
        }

        //
        // Reallocate the array if needed.
        //

        if (ArrayCapacity - UsedSize < NameSize) {
            NewCapacity = ArrayCapacity;
            if (NewCapacity == 0) {
                NewCapacity = 2;
            }

            while (NewCapacity - UsedSize < NameSize) {
                NewCapacity *= 2;
            }

            NewBuffer = realloc(Array, NewCapacity);
            if (NewBuffer == NULL) {
                Result = ENOMEM;
                goto OsEnumerateDirectoryEnd;
            }

            Array = NewBuffer;
            ArrayCapacity = NewCapacity;
        }

        //
        // Copy the entry (or an empty file if this is the end).
        //

        if (DirectoryEntry == NULL) {
            strcpy(Array + UsedSize, "");
            UsedSize += 1;
            break;

        } else {
            strcpy(Array + UsedSize, DirectoryEntry->d_name);
            UsedSize += NameSize;
        }
    }

    Result = 0;

OsEnumerateDirectoryEnd:
    if (Directory != NULL) {
        closedir(Directory);
    }

    if (Result != 0) {
        if (Array != NULL) {
            free(Array);
            Array = NULL;
        }
    }

    *Enumeration = Array;
    return Result;
}

INT
SetupOsCreateDirectory (
    PSTR Path,
    mode_t Permissions
    )

/*++

Routine Description:

    This routine creates a new directory.

Arguments:

    Path - Supplies the path string of the directory to create.

    Permissions - Supplies the permission bits to create the file with.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;

    Result = mkdir(Path, Permissions);
    if (Result != 0) {
        Result = errno;
        if (Result == 0) {
            Result = -1;
        }
    }

    return Result;
}

INT
SetupOsSetAttributes (
    PSTR Path,
    time_t ModificationDate,
    mode_t Permissions
    )

/*++

Routine Description:

    This routine sets attributes on a given path.

Arguments:

    Path - Supplies the path string of the file to modify.

    ModificationDate - Supplies the new modification date to set.

    Permissions - Supplies the new permissions to set.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;
    struct utimbuf Times;

    Times.actime = time(NULL);
    Times.modtime = ModificationDate;
    Result = utime(Path, &Times);
    if (Result != 0) {
        Result = errno;
        return Result;
    }

    Result = chmod(Path, Permissions);
    if (Result != 0) {
        Result = errno;
        return Result;
    }

    return 0;
}

VOID
SetupOsDetermineExecuteBit (
    PVOID Handle,
    PCSTR Path,
    mode_t *Mode
    )

/*++

Routine Description:

    This routine determines whether the open file is executable.

Arguments:

    Handle - Supplies the open file handle.

    Path - Supplies the path the file was opened from (sometimes the file name
        is used as a hint).

    Mode - Supplies a pointer to the current mode bits. This routine may add
        the executable bit to user/group/other if it determines this file is
        executable.

Return Value:

    None.

--*/

{

    //
    // Since Minoca OS has support for executable bits, don't screw with the
    // permissions that are already set.
    //

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

