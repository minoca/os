/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    io.c

Abstract:

    This module implements file I/O related functionality for the Chalk os
    module.

Author:

    Evan Green 11-Jan-2017

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#define _FILE_OFFSET_BITS 64

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#include "osp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpOsOpen (
    PCK_VM Vm
    );

VOID
CkpOsClose (
    PCK_VM Vm
    );

VOID
CkpOsRead (
    PCK_VM Vm
    );

VOID
CkpOsWrite (
    PCK_VM Vm
    );

VOID
CkpOsSeek (
    PCK_VM Vm
    );

VOID
CkpOsFTruncate (
    PCK_VM Vm
    );

VOID
CkpOsIsatty (
    PCK_VM Vm
    );

VOID
CkpOsPathExists (
    PCK_VM Vm
    );

VOID
CkpOsPathLinkExists (
    PCK_VM Vm
    );

VOID
CkpOsIsFile (
    PCK_VM Vm
    );

VOID
CkpOsIsDirectory (
    PCK_VM Vm
    );

VOID
CkpOsIsSymbolicLink (
    PCK_VM Vm
    );

VOID
CkpOsUnlink (
    PCK_VM Vm
    );

VOID
CkpOsLink (
    PCK_VM Vm
    );

VOID
CkpOsSymlink (
    PCK_VM Vm
    );

VOID
CkpOsReadlink (
    PCK_VM Vm
    );

VOID
CkpOsFstat (
    PCK_VM Vm
    );

VOID
CkpOsStat (
    PCK_VM Vm
    );

VOID
CkpOsGetcwd (
    PCK_VM Vm
    );

VOID
CkpOsBasename (
    PCK_VM Vm
    );

VOID
CkpOsDirname (
    PCK_VM Vm
    );

VOID
CkpOsGetenv (
    PCK_VM Vm
    );

VOID
CkpOsSetenv (
    PCK_VM Vm
    );

VOID
CkpOsMkdir (
    PCK_VM Vm
    );

VOID
CkpOsListDirectory (
    PCK_VM Vm
    );

VOID
CkpOsChdir (
    PCK_VM Vm
    );

VOID
CkpOsChroot (
    PCK_VM Vm
    );

VOID
CkpOsUtimes (
    PCK_VM Vm
    );

VOID
CkpOsChmod (
    PCK_VM Vm
    );

VOID
CkpOsChown (
    PCK_VM Vm
    );

VOID
CkpOsCreateStatDict (
    PCK_VM Vm,
    struct stat *Stat
    );

//
// -------------------------------------------------------------------- Globals
//

CK_VARIABLE_DESCRIPTION CkOsIoModuleValues[] = {
    {CkTypeInteger, "O_RDONLY", NULL, O_RDONLY},
    {CkTypeInteger, "O_WRONLY", NULL, O_WRONLY},
    {CkTypeInteger, "O_RDWR", NULL, O_RDWR},
    {CkTypeInteger, "O_ACCMODE", NULL, O_ACCMODE},
    {CkTypeInteger, "O_APPEND", NULL, O_APPEND},
    {CkTypeInteger, "O_EXEC", NULL, O_EXEC},
    {CkTypeInteger, "O_SEARCH", NULL, O_SEARCH},
    {CkTypeInteger, "O_DIRECTORY", NULL, O_DIRECTORY},
    {CkTypeInteger, "O_NOFOLLOW", NULL, O_NOFOLLOW},
    {CkTypeInteger, "O_SYNC", NULL, O_SYNC},
    {CkTypeInteger, "O_DSYNC", NULL, O_DSYNC},
    {CkTypeInteger, "O_RSYNC", NULL, O_RSYNC},
    {CkTypeInteger, "O_CREAT", NULL, O_CREAT},
    {CkTypeInteger, "O_TRUNC", NULL, O_TRUNC},
    {CkTypeInteger, "O_EXCL", NULL, O_EXCL},
    {CkTypeInteger, "O_NOCTTY", NULL, O_NOCTTY},
    {CkTypeInteger, "O_NONBLOCK", NULL, O_NONBLOCK},
    {CkTypeInteger, "O_NOATIME", NULL, O_NOATIME},
    {CkTypeInteger, "O_CLOEXEC", NULL, O_CLOEXEC},
    {CkTypeInteger, "O_PATH", NULL, O_PATH},
    {CkTypeInteger, "O_ASYNC", NULL, O_ASYNC},
    {CkTypeInteger, "O_LARGEFILE", NULL, O_LARGEFILE},
    {CkTypeInteger, "O_TEXT", NULL, O_TEXT},
    {CkTypeInteger, "O_BINARY", NULL, O_BINARY},
    {CkTypeInteger, "OS_SEEK_SET", NULL, SEEK_SET},
    {CkTypeInteger, "OS_SEEK_CUR", NULL, SEEK_CUR},
    {CkTypeInteger, "OS_SEEK_END", NULL, SEEK_END},
    {CkTypeInteger, "S_ISUID", NULL, S_ISUID},
    {CkTypeInteger, "S_ISGID", NULL, S_ISGID},
    {CkTypeInteger, "S_ISVTX", NULL, S_ISVTX},
    {CkTypeInteger, "S_IFBLK", NULL, S_IFBLK},
    {CkTypeInteger, "S_IFCHR", NULL, S_IFCHR},
    {CkTypeInteger, "S_IFDIR", NULL, S_IFDIR},
    {CkTypeInteger, "S_IFIFO", NULL, S_IFIFO},
    {CkTypeInteger, "S_IFREG", NULL, S_IFREG},
    {CkTypeInteger, "S_IFLNK", NULL, S_IFLNK},
    {CkTypeInteger, "S_IFSOCK", NULL, S_IFSOCK},
    {CkTypeInteger, "S_IFMT", NULL, S_IFMT},
    {CkTypeFunction, "open", CkpOsOpen, 3},
    {CkTypeFunction, "close", CkpOsClose, 1},
    {CkTypeFunction, "read", CkpOsRead, 2},
    {CkTypeFunction, "write", CkpOsWrite, 2},
    {CkTypeFunction, "lseek", CkpOsSeek, 3},
    {CkTypeFunction, "ftruncate", CkpOsFTruncate, 2},
    {CkTypeFunction, "isatty", CkpOsIsatty, 1},
    {CkTypeFunction, "exists", CkpOsPathExists, 1},
    {CkTypeFunction, "lexists", CkpOsPathLinkExists, 1},
    {CkTypeFunction, "isfile", CkpOsIsFile, 1},
    {CkTypeFunction, "isdir", CkpOsIsDirectory, 1},
    {CkTypeFunction, "islink", CkpOsIsSymbolicLink, 1},
    {CkTypeFunction, "unlink", CkpOsUnlink, 1},
    {CkTypeFunction, "link", CkpOsLink, 2},
    {CkTypeFunction, "symlink", CkpOsSymlink, 2},
    {CkTypeFunction, "fstat", CkpOsFstat, 1},
    {CkTypeFunction, "stat", CkpOsStat, 1},
    {CkTypeFunction, "getcwd", CkpOsGetcwd, 0},
    {CkTypeFunction, "basename", CkpOsBasename, 1},
    {CkTypeFunction, "dirname", CkpOsDirname, 1},
    {CkTypeFunction, "getenv", CkpOsGetenv, 1},
    {CkTypeFunction, "setenv", CkpOsGetenv, 2},
    {CkTypeFunction, "mkdir", CkpOsMkdir, 2},
    {CkTypeFunction, "listdir", CkpOsListDirectory, 1},
    {CkTypeFunction, "chdir", CkpOsChdir, 1},
    {CkTypeFunction, "chroot", CkpOsChroot, 1},
    {CkTypeFunction, "utimes", CkpOsUtimes, 5},
    {CkTypeFunction, "chown", CkpOsChown, 3},
    {CkTypeFunction, "chmod", CkpOsChmod, 2},
    {CkTypeInvalid, NULL, NULL, 0}
};

//
// ------------------------------------------------------------------ Functions
//

VOID
CkpOsOpen (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the open call. It takes in a path string, flags
    integer, and creation mode integer. It returns a file descriptor integer
    on success, or raises an OsError exception on failure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    INT Descriptor;
    ULONG Flags;
    ULONG Mode;
    PCSTR Path;

    //
    // The function is open(path, flags, mode).
    //

    if (!CkCheckArguments(Vm, 3, CkTypeString, CkTypeInteger, CkTypeInteger)) {
        return;
    }

    Path = CkGetString(Vm, 1, NULL);
    Flags = CkGetInteger(Vm, 2);
    Mode = CkGetInteger(Vm, 3);
    Descriptor = open(Path, Flags, Mode);
    if (Descriptor < 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    CkReturnInteger(Vm, Descriptor);
    return;
}

VOID
CkpOsClose (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the close call. It takes in a file descriptor
    integer. Nothing is returned on success, or an OsError exception is raised
    on error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    INT Result;

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    Result = close(CkGetInteger(Vm, 1));
    CkReturnInteger(Vm, Result);
    return;
}

VOID
CkpOsRead (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the read call. It takes in a file descriptor
    integer and a size, and reads at most that size byte from the descriptor.
    It returns a string containing the bytes read on success, an empty string
    if no bytes were read, or raises an OsError exception on failure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PSTR Buffer;
    ssize_t BytesRead;
    INT Descriptor;
    INT Size;

    if (!CkCheckArguments(Vm, 2, CkTypeInteger, CkTypeInteger)) {
        return;
    }

    Descriptor = CkGetInteger(Vm, 1);
    Size = CkGetInteger(Vm, 2);
    if (Size < 0) {
        Size = 0;
    }

    Buffer = CkPushStringBuffer(Vm, Size);
    if (Buffer == NULL) {
        return;
    }

    do {
        BytesRead = read(Descriptor, Buffer, Size);

    } while ((BytesRead < 0) && (errno == EINTR));

    if (BytesRead < 0) {
        CkFinalizeString(Vm, -1, 0);
        CkpOsRaiseError(Vm);

    } else {
        CkFinalizeString(Vm, -1, BytesRead);
        CkStackReplace(Vm, 0);
    }

    return;
}

VOID
CkpOsWrite (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the write call. It takes in a file descriptor
    integer and a string, and attempts to write that string to the descriptor.
    It returns the number of bytes actually written, which may be less than the
    desired size, or raises an OsError exception on failure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Buffer;
    ssize_t BytesWritten;
    INT Descriptor;
    UINTN Size;

    if (!CkCheckArguments(Vm, 2, CkTypeInteger, CkTypeString)) {
        return;
    }

    Descriptor = CkGetInteger(Vm, 1);
    Buffer = CkGetString(Vm, 2, &Size);
    do {
        BytesWritten = write(Descriptor, Buffer, Size);

    } while ((BytesWritten < 0) && (errno == EINTR));

    if (BytesWritten < 0) {
        CkpOsRaiseError(Vm);
    }

    CkReturnInteger(Vm, BytesWritten);
    return;
}

VOID
CkpOsSeek (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the lseek call. It takes in a file descriptor
    integer, an offset, and a disposition, and seeks to the requested file
    position. On success, the new absolute position is returned, or an OsError
    exception is raised on error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    INT Descriptor;
    off_t Offset;
    INT Whence;

    //
    // The function is lseek(file, offset, whence).
    //

    if (!CkCheckArguments(Vm, 3, CkTypeInteger, CkTypeInteger, CkTypeInteger)) {
        return;
    }

    Descriptor = CkGetInteger(Vm, 1);
    Offset = CkGetInteger(Vm, 2);
    Whence = CkGetInteger(Vm, 3);
    Offset = lseek(Descriptor, Offset, Whence);
    if (Offset < 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    CkReturnInteger(Vm, Offset);
    return;
}

VOID
CkpOsFTruncate (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the ftruncate call. It takes in a file descriptor
    integer and a new file size, and makes the file pointed to by the
    descriptor the desired size. On success, nothing is returned, or an OsError
    exception is raised on error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    INT Descriptor;
    off_t Offset;
    INT Result;

    //
    // The function is ftruncate(file, size).
    //

    if (!CkCheckArguments(Vm, 2, CkTypeInteger, CkTypeInteger)) {
        return;
    }

    Descriptor = CkGetInteger(Vm, 1);
    Offset = CkGetInteger(Vm, 2);
    Result = ftruncate(Descriptor, Offset);
    if (Result < 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    CkReturnNull(Vm);
    return;
}

VOID
CkpOsIsatty (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the isatty call. It takes in a file descriptor
    integer and returns whether or not that descriptor is an interactive
    terminal. On success, nothing is returned, or an OsError exception is
    raised on error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    INT Descriptor;
    INT Result;

    //
    // The function is isatty(file).
    //

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    Descriptor = CkGetInteger(Vm, 1);
    Result = isatty(Descriptor);
    if (Result < 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    if (Result != 0) {
        Result = 1;
    }

    CkReturnInteger(Vm, Result);
    return;
}

VOID
CkpOsPathExists (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine takes an in a path an returns whether or not the given path
    exists. It returns false for broken links and if the caller does not have
    access to traverse the directory tree.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Path;
    INT Result;
    struct stat Stat;

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Result = 0;
    Path = CkGetString(Vm, 1, NULL);
    if (stat(Path, &Stat) == 0) {
        Result = 1;
    }

    CkReturnInteger(Vm, Result);
    return;
}

VOID
CkpOsPathLinkExists (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine takes an in a path an returns whether or not the given path
    exists, without following symbolic links. It may return false if the caller
    does not have access to traverse the directory tree.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Path;
    INT Result;
    struct stat Stat;

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Result = 0;
    Path = CkGetString(Vm, 1, NULL);
    if (lstat(Path, &Stat) == 0) {
        Result = 1;
    }

    CkReturnInteger(Vm, Result);
    return;
}

VOID
CkpOsIsFile (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine takes an in a path an returns whether or not the given object
    is a regular file or not. This follows symbolic links.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Path;
    INT Result;
    struct stat Stat;

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Result = 0;
    Path = CkGetString(Vm, 1, NULL);
    if (stat(Path, &Stat) == 0) {
        if (S_ISREG(Stat.st_mode)) {
            Result = 1;
        }
    }

    CkReturnInteger(Vm, Result);
    return;
}

VOID
CkpOsIsDirectory (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine takes an in a path an returns whether or not the given object
    is a directory or not. This follows symbolic links.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Path;
    INT Result;
    struct stat Stat;

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Result = 0;
    Path = CkGetString(Vm, 1, NULL);
    if (stat(Path, &Stat) == 0) {
        if (S_ISDIR(Stat.st_mode)) {
            Result = 1;
        }
    }

    CkReturnInteger(Vm, Result);
    return;
}

VOID
CkpOsIsSymbolicLink (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine takes an in a path an returns whether or not the given object
    is a symbolic link or not.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Path;
    INT Result;
    struct stat Stat;

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Result = 0;
    Path = CkGetString(Vm, 1, NULL);
    if (stat(Path, &Stat) == 0) {
        if (S_ISLNK(Stat.st_mode)) {
            Result = 1;
        }
    }

    CkReturnInteger(Vm, Result);
    return;
}

VOID
CkpOsUnlink (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine attempts to unlink (delete) a path.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Path;

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Path = CkGetString(Vm, 1, NULL);
    if (unlink(Path) != 0) {
        CkpOsRaiseError(Vm);

    } else {
        CkReturnInteger(Vm, 0);
    }

    return;
}

VOID
CkpOsLink (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine attempts to establish a hard link from one path to another.
    It takes two arguments: a string containing the existing path, and a string
    containing the path to link it to. Returns 0 on success, or raises an
    OSError on failure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR NewLink;
    PCSTR Source;

    if (!CkCheckArguments(Vm, 2, CkTypeString, CkTypeString)) {
        return;
    }

    Source = CkGetString(Vm, 1, NULL);
    NewLink = CkGetString(Vm, 2, NULL);
    if (link(Source, NewLink) != 0) {
        CkpOsRaiseError(Vm);

    } else {
        CkReturnInteger(Vm, 0);
    }

    return;
}

VOID
CkpOsSymlink (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine attempts to establish a symbolic link from one path to another.
    It takes two arguments: a string containing the destination the symbolic
    link points to, and a string containing the location where the symbolic
    link should be created. Returns 0 on success, or raises an OSError on
    failure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR LinkLocation;
    PCSTR LinkTarget;

    if (!CkCheckArguments(Vm, 2, CkTypeString, CkTypeString)) {
        return;
    }

    LinkTarget = CkGetString(Vm, 1, NULL);
    LinkLocation = CkGetString(Vm, 2, NULL);
    if (symlink(LinkTarget, LinkLocation) != 0) {
        CkpOsRaiseError(Vm);

    } else {
        CkReturnInteger(Vm, 0);
    }

    return;
}

VOID
CkpOsReadlink (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine reads the contents of a symbolic link. It takes a single
    argument: a string containing the path to a symbolic link. It returns a
    string containing the contents of the link on success.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR LinkLocation;
    CHAR LinkTarget[4096];
    ssize_t Size;

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    LinkLocation = CkGetString(Vm, 1, NULL);
    Size = readlink(LinkLocation, LinkTarget, sizeof(LinkTarget));
    if (Size < 0) {
        CkpOsRaiseError(Vm);

    } else {
        CkReturnString(Vm, LinkTarget, Size);
    }

    return;
}

VOID
CkpOsFstat (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the fstat call. It takes in a file descriptor
    integer and returns information about that descriptor. On success, returns
    a dictionary of stat information, or an OsError exception is raised on
    error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    INT Descriptor;
    INT Result;
    struct stat Stat;

    //
    // The function is fstat(file).
    //

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    Descriptor = CkGetInteger(Vm, 1);
    Result = fstat(Descriptor, &Stat);
    if (Result < 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    CkpOsCreateStatDict(Vm, &Stat);
    CkStackReplace(Vm, 0);
    return;
}

VOID
CkpOsStat (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the stat call. It takes in a file path string and
    returns information about that descriptor. On success, returns a
    dictionary of stat information, or an OsError exception is raised on error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Path;
    INT Result;
    struct stat Stat;

    //
    // The function is stat(path).
    //

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Path = CkGetString(Vm, 1, NULL);
    Result = stat(Path, &Stat);
    if (Result < 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    CkpOsCreateStatDict(Vm, &Stat);
    CkStackReplace(Vm, 0);
    return;
}

VOID
CkpOsGetcwd (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine returns the current working directory as a string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PSTR Directory;

    Directory = getcwd(NULL, 0);
    if (Directory == NULL) {
        CkpOsRaiseError(Vm);
        return;
    }

    CkPushString(Vm, Directory, strlen(Directory));
    free(Directory);
    CkStackReplace(Vm, 0);
    return;
}

VOID
CkpOsBasename (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the basename call. It takes a path and gets the
    file name portion of that path.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PSTR BaseName;
    PSTR Path;

    //
    // The function takes a string and returns a string.
    //

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Path = strdup(CkGetString(Vm, 1, NULL));
    if (Path == NULL) {
        CkpOsRaiseError(Vm);
        return;
    }

    BaseName = basename(Path);
    if (BaseName == NULL) {
        free(Path);
        CkpOsRaiseError(Vm);
        return;
    }

    CkReturnString(Vm, BaseName, strlen(BaseName));
    free(Path);
    return;
}

VOID
CkpOsDirname (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the dirname call. It takes a path and gets the
    directory portion of it. If the path has no directory portion, "." is
    returned.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PSTR DirName;
    PSTR Path;

    //
    // The function takes a string and returns a string.
    //

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Path = strdup(CkGetString(Vm, 1, NULL));
    if (Path == NULL) {
        CkpOsRaiseError(Vm);
        return;
    }

    DirName = dirname(Path);
    if (DirName == NULL) {
        free(Path);
        CkpOsRaiseError(Vm);
        return;
    }

    CkReturnString(Vm, DirName, strlen(DirName));
    free(Path);
    return;
}

VOID
CkpOsGetenv (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the getenv call. It returns a value from the
    environment.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Name;
    PCSTR Value;

    //
    // The function takes a string and returns a string.
    //

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Name = CkGetString(Vm, 1, NULL);
    Value = getenv(Name);
    if (Value == NULL) {
        CkReturnNull(Vm);

    } else {
        CkReturnString(Vm, Value, strlen(Value));
    }

    return;
}

VOID
CkpOsSetenv (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the setenv call. It sets an environment variable
    value. If the value is null, then this unsets the environment variable.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Name;
    UINTN NameLength;
    UINTN Size;
    PSTR String;
    PCSTR Value;
    UINTN ValueLength;

    //
    // The function takes two strings.
    //

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Name = CkGetString(Vm, 1, &NameLength);
    if (CkIsString(Vm, 2)) {
        Value = CkGetString(Vm, 2, &ValueLength);
        Size = NameLength + ValueLength + 2;
        String = malloc(Size);
        if (String == NULL) {
            CkpOsRaiseError(Vm);
            return;
        }

        snprintf(String, Size, "%s=%s", Name, Value);
        if (putenv(String) != 0) {
            CkpOsRaiseError(Vm);
            return;
        }

    } else {
        Size = NameLength + 2;
        String = malloc(Size);
        if (String == NULL) {
            CkpOsRaiseError(Vm);
            return;
        }

        snprintf(String, Size, "%s=", Name);
        if (putenv(String) != 0) {
            CkpOsRaiseError(Vm);
            return;
        }
    }

    CkReturnNull(Vm);
    return;
}

VOID
CkpOsMkdir (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine creates a directory. It takes two arguments, the path of the
    directory to create and the permissions to apply to the directory.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Path;

    //
    // The function takes a path and a set of permissions.
    //

    if (!CkCheckArguments(Vm, 2, CkTypeString, CkTypeInteger)) {
        return;
    }

    Path = CkGetString(Vm, 1, NULL);
    if (mkdir(Path, CkGetInteger(Vm, 2)) != 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    CkReturnInteger(Vm, 0);
    return;
}

VOID
CkpOsListDirectory (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine lists the contents of the directory specified by the given
    path. It takes a single argument, the path to the directory to enumerate,
    and returns a list of relative directory entries, not including . or ..
    entries.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    This routine does not return. The process exits.

--*/

{

    DIR *Directory;
    struct dirent *Entry;
    UINTN Index;
    PCSTR Path;

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Path = CkGetString(Vm, 1, NULL);
    if (Path == NULL) {
        return;
    }

    Directory = opendir(Path);
    if (Directory == NULL) {
        CkpOsRaiseError(Vm);
        return;
    }

    Index = 0;
    CkPushList(Vm);
    while (TRUE) {
        errno = 0;
        Entry = readdir(Directory);
        if (Entry == NULL) {
            if (errno != 0) {
                closedir(Directory);
                CkpOsRaiseError(Vm);
                return;
            }

            break;
        }

        //
        // Skip . and .. entries.
        //

        if (Entry->d_name[0] == '.') {
            if ((Entry->d_name[1] == '\0') ||
                ((Entry->d_name[1] == '.') && (Entry->d_name[2] == '\0'))) {

                continue;
            }
        }

        CkPushString(Vm, Entry->d_name, strlen(Entry->d_name));
        CkListSet(Vm, -2, Index);
        Index += 1;
    }

    CkStackReplace(Vm, 0);
    return;
}

VOID
CkpOsChdir (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine changes the current working directory to the given directory.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Path;

    //
    // The function takes a path.
    //

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Path = CkGetString(Vm, 1, NULL);
    if (chdir(Path) != 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    CkReturnInteger(Vm, 0);
    return;
}

VOID
CkpOsChroot (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine changes the current root directory to the given directory.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Path;

    //
    // The function takes a path.
    //

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Path = CkGetString(Vm, 1, NULL);
    if (chroot(Path) != 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    CkReturnInteger(Vm, 0);
    return;
}

VOID
CkpOsUtimes (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine changes the access and modification times of the given file.
    The function takes a path, access time, nanoseconds, modification time, and
    nano seconds. The times are integer timestamps.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CK_INTEGER Access;
    CK_INTEGER AccessNano;
    CK_INTEGER Modified;
    CK_INTEGER ModifiedNano;
    PCSTR Path;
    struct timeval Value[2];

    //
    // The function takes a path.
    //

    if (!CkCheckArguments(Vm,
                          5,
                          CkTypeString,
                          CkTypeInteger,
                          CkTypeInteger,
                          CkTypeInteger,
                          CkTypeInteger)) {

        return;
    }

    Path = CkGetString(Vm, 1, NULL);
    Access = CkGetInteger(Vm, 2);
    AccessNano = CkGetInteger(Vm, 3);
    Modified = CkGetInteger(Vm, 4);
    ModifiedNano = CkGetInteger(Vm, 5);
    Value[0].tv_sec = Access;
    Value[0].tv_usec = AccessNano / 1000;
    Value[1].tv_sec = Modified;
    Value[1].tv_usec = ModifiedNano / 1000;
    if ((Value[0].tv_sec != Access) || (Value[1].tv_sec != Modified)) {
        errno = ERANGE;
        CkpOsRaiseError(Vm);
        return;
    }

    if (utimes(Path, Value) != 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    CkReturnInteger(Vm, 0);
    return;
}

VOID
CkpOsChmod (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine changes the permissions of the given path. It takes in a path
    string and a new set of permissions.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    mode_t Mode;
    PCSTR Path;

    //
    // The function takes a path.
    //

    if (!CkCheckArguments(Vm, 2, CkTypeString, CkTypeInteger)) {
        return;
    }

    Path = CkGetString(Vm, 1, NULL);
    Mode = CkGetInteger(Vm, 2);
    if (chmod(Path, Mode) != 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    CkReturnInteger(Vm, 0);
    return;
}

VOID
CkpOsChown (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine changes the ownership of the given path. It takes in a path
    string and a new user and group.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    gid_t Group;
    PCSTR Path;
    uid_t User;

    //
    // The function takes a path.
    //

    if (!CkCheckArguments(Vm, 3, CkTypeString, CkTypeInteger, CkTypeInteger)) {
        return;
    }

    Path = CkGetString(Vm, 1, NULL);
    User = CkGetInteger(Vm, 2);
    Group = CkGetInteger(Vm, 3);
    if (chown(Path, User, Group) != 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    CkReturnInteger(Vm, 0);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpOsCreateStatDict (
    PCK_VM Vm,
    struct stat *Stat
    )

/*++

Routine Description:

    This routine creates a dictionary based off the given stat struct.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Stat - Supplies the stat structure to convert to a dictionary.

Return Value:

    None. The new dictionary is at the top of the stack.

--*/

{

    CkPushDict(Vm);
    CkPushString(Vm, "st_dev", 6);
    CkPushInteger(Vm, Stat->st_dev);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "st_ino", 6);
    CkPushInteger(Vm, Stat->st_ino);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "st_mode", 7);
    CkPushInteger(Vm, Stat->st_mode);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "st_nlink", 8);
    CkPushInteger(Vm, Stat->st_nlink);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "st_uid", 6);
    CkPushInteger(Vm, Stat->st_uid);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "st_gid", 6);
    CkPushInteger(Vm, Stat->st_gid);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "st_rdev", 7);
    CkPushInteger(Vm, Stat->st_rdev);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "st_size", 7);
    CkPushInteger(Vm, Stat->st_size);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "st_atime", 8);
    CkPushInteger(Vm, Stat->st_atime);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "st_mtime", 8);
    CkPushInteger(Vm, Stat->st_mtime);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "st_ctime", 8);
    CkPushInteger(Vm, Stat->st_ctime);
    CkDictSet(Vm, -3);
    return;
}

