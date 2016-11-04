/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    io.c

Abstract:

    This module implements support for doing I/O on a Windows host in the setup
    application.

Author:

    Evan Green 8-Oct-2014

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Use CRT 6.1 for stat64.
//

#define __MSVCRT_VERSION__ 0x0601

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <unistd.h>

#include "../setup.h"
#include "win32sup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define some executable file magic words.
//

#define ELF_MAGIC 0x464C457F
#define IMAGE_DOS_SIGNATURE 0x5A4D
#define SCRIPT_SHEBANG 0x2123

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes a handle to an I/O object in the setup app.

Members:

    Handle - Stores the device handle.

    WinHandle - Stores the Windows handle.

--*/

typedef struct _SETUP_OS_HANDLE {
    int Handle;
    void *WinHandle;
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

    return ENOSYS;
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

    return -1;
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
    PSTR PathCopy;
    size_t PathLength;
    struct stat Stat;

    IoHandle = malloc(sizeof(SETUP_OS_HANDLE));
    if (IoHandle == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    IoHandle->Handle = -1;
    IoHandle->WinHandle = NULL;
    if (Destination->Path != NULL) {
        IoHandle->Handle = open(Destination->Path,
                                Flags | O_BINARY,
                                CreatePermissions);

        if (IoHandle->Handle < 0) {

            //
            // Windows doesn't allow opening directories. Make the error
            // unambiguous if this is a directory.
            //

            if ((stat(Destination->Path, &Stat) == 0) &&
                (S_ISDIR(Stat.st_mode))) {

                errno = EISDIR;

            } else {

                //
                // Windows doesn't allow opening a path with a slash on the
                // end. If the non-slash version works, then the error is it's
                // a directory.
                //

                PathLength = strlen(Destination->Path);
                if ((PathLength != 0) &&
                    (Destination->Path[PathLength - 1] == '/')) {

                    PathCopy = strdup(Destination->Path);
                    if (PathCopy == NULL) {
                        free(IoHandle);
                        return NULL;
                    }

                    while ((PathLength > 1) &&
                           (PathCopy[PathLength - 1] == '/')) {

                        PathCopy[PathLength - 1] = '\0';
                        PathLength -= 1;
                    }

                    if ((stat(PathCopy, &Stat) == 0) &&
                        (S_ISDIR(Stat.st_mode))) {

                        errno = EISDIR;
                    }

                    free(PathCopy);
                }
            }

            free(IoHandle);
            return NULL;
        }

    } else {
        IoHandle->WinHandle = SetupWin32OpenDeviceId(Destination->DeviceId);
        if (IoHandle->WinHandle == NULL) {
            free(IoHandle);
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
    if (IoHandle->WinHandle != NULL) {
        SetupWin32Close(IoHandle->WinHandle);
    }

    if (IoHandle->Handle >= 0) {
        close(IoHandle->Handle);
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

    ssize_t BytesCompleted;
    PSETUP_OS_HANDLE IoHandle;
    ssize_t TotalBytesRead;

    IoHandle = Handle;
    if (IoHandle->WinHandle != NULL) {
        return SetupWin32Read(IoHandle->WinHandle, Buffer, ByteCount);
    }

    TotalBytesRead = 0;
    while (ByteCount != 0) {
        BytesCompleted = read(IoHandle->Handle, Buffer, ByteCount);
        if (BytesCompleted <= 0) {
            break;
        }

        Buffer += BytesCompleted;
        TotalBytesRead += BytesCompleted;
        ByteCount -= BytesCompleted;
    }

    return TotalBytesRead;
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

    ssize_t BytesCompleted;
    PSETUP_OS_HANDLE IoHandle;
    ssize_t TotalBytesWritten;

    IoHandle = Handle;
    if (IoHandle->WinHandle != NULL) {
        return SetupWin32Write(IoHandle->WinHandle, Buffer, ByteCount);
    }

    TotalBytesWritten = 0;
    while (ByteCount != 0) {
        BytesCompleted = write(IoHandle->Handle, Buffer, ByteCount);
        if (BytesCompleted <= 0) {
            perror("Write failed");
            break;
        }

        Buffer += BytesCompleted;
        TotalBytesWritten += BytesCompleted;
        ByteCount -= BytesCompleted;
    }

    return TotalBytesWritten;
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
    LONGLONG NewOffset;

    IoHandle = Handle;
    if (IoHandle->WinHandle != NULL) {
        return SetupWin32Seek(IoHandle->WinHandle, Offset);
    }

    NewOffset = _lseeki64(IoHandle->Handle, Offset, SEEK_SET);
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
    ULONGLONG NewOffset;

    IoHandle = Handle;
    if (IoHandle->WinHandle != NULL) {
        return SetupWin32Tell(IoHandle->WinHandle);
    }

    NewOffset = _lseeki64(IoHandle->Handle, 0, SEEK_CUR);
    return NewOffset;
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
    struct __stat64 Stat;

    IoHandle = Handle;
    if (IoHandle->WinHandle != NULL) {
        if ((ModificationDate != NULL) || (Mode != NULL)) {
            fprintf(stderr,
                    "Error: Modification date and mode cannot be read from a "
                    "device.\n");

            return ENOSYS;
        }

        if (FileSize == NULL) {
            return 0;
        }

        return SetupWin32FileStat(IoHandle->WinHandle, FileSize);
    }

    Result = _fstat64(IoHandle->Handle, &Stat);
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
    Result = ftruncate(IoHandle->Handle, NewSize);
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
    size_t NameSize;
    PVOID NewBuffer;
    size_t NewCapacity;
    INT Result;
    struct dirent *ResultPointer;
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
        ResultPointer = readdir(Directory);
        if (ResultPointer == NULL) {
            NameSize = 1;

        } else {
            if ((strcmp(ResultPointer->d_name, ".") == 0) ||
                (strcmp(ResultPointer->d_name, "..") == 0)) {

                continue;
            }

            NameSize = strlen(ResultPointer->d_name) + 1;
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

        if (ResultPointer == NULL) {
            strcpy(Array + UsedSize, "");
            UsedSize += 1;
            break;

        } else {
            strcpy(Array + UsedSize, ResultPointer->d_name);
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

    Result = mkdir(Path);
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

    ssize_t BytesRead;
    BOOL Executable;
    PSETUP_OS_HANDLE IoHandle;
    PSTR LastDot;
    off_t Offset;
    ULONG Word;

    Executable = FALSE;
    IoHandle = Handle;

    //
    // First try to guess based on the name.
    //

    LastDot = strrchr(Path, '.');
    if (LastDot != NULL) {
        LastDot += 1;
        if ((strcmp(LastDot, "sh") == 0) ||
            (strcmp(LastDot, "exe") == 0)) {

            Executable = TRUE;
        }
    }

    if (Executable == FALSE) {

        //
        // Go to the beginning of the file and read the first word.
        //

        Offset = lseek(IoHandle->Handle, 0, SEEK_CUR);
        lseek(IoHandle->Handle, 0, SEEK_SET);
        Word = 0;
        do {
            BytesRead = read(IoHandle->Handle, &Word, sizeof(Word));

        } while ((BytesRead < 0) && (errno == EINTR));

        if (BytesRead > 0) {
            if (Word == ELF_MAGIC) {
                Executable = TRUE;

            } else {

                //
                // Now just look at the first two bytes.
                //

                Word &= 0x0000FFFF;
                if ((Word == IMAGE_DOS_SIGNATURE) ||
                    (Word == SCRIPT_SHEBANG)) {

                    Executable = TRUE;
                }
            }
        }

        //
        // Restore the previous offset.
        //

        lseek(IoHandle->Handle, Offset, SEEK_SET);
    }

    if (Executable != FALSE) {
        *Mode |= FILE_PERMISSION_ALL_EXECUTE;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

