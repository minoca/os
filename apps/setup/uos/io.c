/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    io.c

Abstract:

    This module implements support for doing I/O on generic POSIX systems in
    the setup application.

Author:

    Evan Green 19-Jan-2016

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#define _FILE_OFFSET_BITS 64

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#if defined(__APPLE__) || defined(__FreeBSD__)

#include <sys/disk.h>

#endif

#if defined(__linux__)

#include <sys/mount.h>

#endif

#include "../setup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SetupOsGetBlockDeviceSize (
    INT Descriptor,
    PULONGLONG Size
    );

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

    int Descriptor;

    if (Destination->Path == NULL) {
        fprintf(stderr, "Error: Device ID paths not supported.\n");
        errno = ENOENT;
        return NULL;
    }

    Descriptor = open(Destination->Path, Flags, CreatePermissions);
    if (Descriptor == -1) {
        return NULL;
    }

    return (PVOID)(INTN)Descriptor;
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

    close((INTN)Handle);
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

    ssize_t BytesRead;

    do {
        BytesRead = read((INTN)Handle, Buffer, ByteCount);

    } while ((BytesRead < 0) && (errno == EINTR));

    return BytesRead;
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

    ssize_t BytesWritten;

    do {
        BytesWritten = write((INTN)Handle, Buffer, ByteCount);

    } while ((BytesWritten < 0) && (errno == EINTR));

    return BytesWritten;
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

    return lseek((INTN)Handle, Offset, SEEK_SET);
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

    return lseek((INTN)Handle, 0, SEEK_CUR);
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

    int Result;
    struct stat Stat;

    Result = fstat((INTN)Handle, &Stat);
    if (Result != 0) {
        return Result;
    }

    if (FileSize != NULL) {
        if (S_ISBLK(Stat.st_mode)) {
            Result = SetupOsGetBlockDeviceSize((INTN)Handle, FileSize);
            if (Result != 0) {
                return Result;
            }

        } else {
            *FileSize = Stat.st_size;
        }
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

    int Result;

    Result = ftruncate((INTN)Handle, NewSize);
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
    // Since POSIX systems have support for executable bits, don't screw with
    // the permissions that are already set.
    //

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SetupOsGetBlockDeviceSize (
    INT Descriptor,
    PULONGLONG Size
    )

/*++

Routine Description:

    This routine gets the size of the open block device size.

Arguments:

    Descriptor - Supplies the open file descriptor.

    Size - Supplies a pointer where the block device size will be returned on
        success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    //
    // The IOCTL for Apple devices.
    //

#ifdef DKIOCGETBLOCKCOUNT

    if (ioctl(Descriptor, DKIOCGETBLOCKCOUNT, Size) >= 0) {
        *Size *= 512;
        return 0;
    }

#endif

    //
    // The IOCTLs for Linux devices.
    //

#ifdef BLKGETSIZE64

    if (ioctl(Descriptor, BLKGETSIZE64, Size) >= 0) {
        return 0;
    }

#endif

#ifdef BLKGETSIZE

    if (ioctl(Descriptor, BLKGETSIZE, Size) >= 0) {
        *Size *= 512;
        return 0;
    }

#endif

    //
    // The IOCTL for FreeBSD.
    //

#ifdef DIOCGMEDIASIZE

    if (ioctl(Descriptor, DIOCGMEDIASIZE, Size) >= 0) {
        return 0;
    }

#endif

    return ENOSYS;
}

