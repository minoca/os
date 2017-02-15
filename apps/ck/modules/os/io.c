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

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
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
    {CkTypeFunction, "open", CkpOsOpen, 3},
    {CkTypeFunction, "close", CkpOsClose, 1},
    {CkTypeFunction, "read", CkpOsRead, 2},
    {CkTypeFunction, "write", CkpOsWrite, 2},
    {CkTypeFunction, "lseek", CkpOsSeek, 3},
    {CkTypeFunction, "ftruncate", CkpOsFTruncate, 2},
    {CkTypeFunction, "isatty", CkpOsIsatty, 1},
    {CkTypeFunction, "fstat", CkpOsFstat, 1},
    {CkTypeFunction, "stat", CkpOsStat, 1},
    {CkTypeFunction, "getcwd", CkpOsGetcwd, 0},
    {CkTypeFunction, "basename", CkpOsBasename, 1},
    {CkTypeFunction, "dirname", CkpOsDirname, 1},
    {CkTypeFunction, "getenv", CkpOsGetenv, 1},
    {CkTypeFunction, "setenv", CkpOsGetenv, 2},
    {CkTypeFunction, "mkdir", CkpOsMkdir, 2},
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
    CkPushInteger(Vm, Stat->st_dev);
    CkPushString(Vm, "st_dev", 6);
    CkDictSet(Vm, -3);
    CkPushInteger(Vm, Stat->st_ino);
    CkPushString(Vm, "st_ino", 6);
    CkDictSet(Vm, -3);
    CkPushInteger(Vm, Stat->st_mode);
    CkPushString(Vm, "st_mode", 7);
    CkDictSet(Vm, -3);
    CkPushInteger(Vm, Stat->st_nlink);
    CkPushString(Vm, "st_nlink", 8);
    CkDictSet(Vm, -3);
    CkPushInteger(Vm, Stat->st_uid);
    CkPushString(Vm, "st_uid", 6);
    CkDictSet(Vm, -3);
    CkPushInteger(Vm, Stat->st_gid);
    CkPushString(Vm, "st_gid", 6);
    CkDictSet(Vm, -3);
    CkPushInteger(Vm, Stat->st_rdev);
    CkPushString(Vm, "st_rdev", 7);
    CkDictSet(Vm, -3);
    CkPushInteger(Vm, Stat->st_size);
    CkPushString(Vm, "st_size", 7);
    CkDictSet(Vm, -3);
    CkPushInteger(Vm, Stat->st_atime);
    CkPushString(Vm, "st_atime", 8);
    CkDictSet(Vm, -3);
    CkPushInteger(Vm, Stat->st_mtime);
    CkPushString(Vm, "st_mtime", 8);
    CkDictSet(Vm, -3);
    CkPushInteger(Vm, Stat->st_ctime);
    CkPushString(Vm, "st_ctime", 8);
    CkDictSet(Vm, -3);
    return;
}

