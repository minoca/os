/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    userio.c

Abstract:

    This module implements I/O support functionality for the Swiss common
    library.

Author:

    Evan Green 2-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../swlib.h"
#include "version.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the initial line allocation size.
//

#define SWISS_READ_LINE_INITIAL_SIZE 256

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
SwpParseOctalPermissionsString (
    PSTR String,
    mode_t *Mode,
    PSTR *NextString
    );

BOOL
SwpParseFilePermissionsClause (
    PSTR String,
    BOOL IsDirectory,
    mode_t *Mode,
    PSTR *NextString
    );

VOID
SwpPrintDirectoryMessage (
    PSTR Path,
    int Error,
    BOOL Verbose
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the string containing the current application name.
//

PSTR SwCurrentApplication;

//
// ------------------------------------------------------------------ Functions
//

PSTR
SwGetCurrentApplicationName (
    VOID
    )

/*++

Routine Description:

    This routine returns the current application name.

Arguments:

    None.

Return Value:

    Returns the current application name. The caller must not modify this
    buffer.

--*/

{

    return SwCurrentApplication;
}

PSTR
SwSetCurrentApplicationName (
    PSTR ApplicationName
    )

/*++

Routine Description:

    This routine changes the application name prefix that is printed in all
    error messages.

Arguments:

    ApplicationName - Supplies a pointer to the new application name to use.
        This string is expected to stay valid, this routine will not attempt to
        copy or modify this string.

Return Value:

    Returns a pointer to the original application name, which the caller should
    restore when finished.

--*/

{

    PSTR OriginalName;

    OriginalName = SwCurrentApplication;
    SwCurrentApplication = ApplicationName;
    return OriginalName;
}

VOID
SwPrintError (
    INT ErrorNumber,
    PSTR QuotedString,
    PSTR Format,
    ...
    )

/*++

Routine Description:

    This routine prints a formatted message to standard error. The message
    generally takes the form:
        "<appname>: Formatted message 'QuotedString': Error description.\n"

    The error description and quoted string can be omitted by passing special
    values. The app name portion can be omitted by passing NULL to the set
    current application name value.

Arguments:

    ErrorNumber - Supplies an error number to print a description for. Supply
        zero to skip printing the final colon and error description.

    QuotedString - Supplies an optional pointer to a string to quote, which
        will be printed immediately after the formatted message. Supply null
        to skip printing a quoted string.

    Format - Supplies the printf style format string to print to standard error.

    ... - Supplies the remaining printf arguments.

Return Value:

    None.

--*/

{

    va_list ArgumentList;

    va_start(ArgumentList, Format);
    SwPrintErrorVaList(ErrorNumber, QuotedString, Format, ArgumentList);
    va_end(ArgumentList);
    return;
}

VOID
SwPrintErrorVaList (
    INT ErrorNumber,
    PSTR QuotedString,
    PSTR Format,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine prints a formatted message to standard error. The message
    generally takes the form:
        "<appname>: Formatted message 'QuotedString': Error description.\n"

    The error description and quoted string can be omitted by passing special
    values. The app name portion can be omitted by passing NULL to the set
    current application name value.

Arguments:

    ErrorNumber - Supplies an error number to print a description for. Supply
        zero to skip printing the final colon and error description.

    QuotedString - Supplies an optional pointer to a string to quote, which
        will be printed immediately after the formatted message. Supply null
        to skip printing a quoted string.

    Format - Supplies the printf style format string to print to standard error.

    ArgumentList - Supplies the argument list to print with the format.

Return Value:

    None.

--*/

{

    PSTR QuotedArgument;

    if (SwCurrentApplication != NULL) {
        fprintf(stderr, "%s: ", SwCurrentApplication);
    }

    vfprintf(stderr, Format, ArgumentList);
    if (QuotedString != NULL) {
        QuotedArgument = SwQuoteArgument(QuotedString);
        fprintf(stderr, " '%s'", QuotedArgument);
        if (QuotedArgument != QuotedString) {
            free(QuotedArgument);
        }
    }

    if (ErrorNumber != 0) {
        fprintf(stderr, ": %s", strerror(ErrorNumber));
    }

    fprintf(stderr, ".\n");
    return;
}

VOID
SwPrintVersion (
    ULONG MajorVersion,
    ULONG MinorVersion
    )

/*++

Routine Description:

    This routine prints an application version number.

Arguments:

    MajorVersion - Supplies the major version number.

    MinorVersion - Supplies the minor version number.

Return Value:

    None.

--*/

{

    printf("Minoca %s version %d.%d.%d\n"
           "%s\n"
           "Copyright (c) 2013-2016 Minoca Corp. %s\n\n",
           SwCurrentApplication,
           MajorVersion,
           MinorVersion,
           VERSION_SERIAL,
           VERSION_BUILD_STRING,
           VERSION_LICENSE);

    return;
}

INT
SwGetSerialVersion (
    VOID
    )

/*++

Routine Description:

    This routine returns the serial version number, an ever-increasing version
    number with each revision.

Arguments:

    MajorVersion - Supplies the major version number.

    MinorVersion - Supplies the minor version number.

Return Value:

    Returns the serial version number.

--*/

{

    return VERSION_SERIAL;
}

INT
SwGetYesNoAnswer (
    PBOOL Answer
    )

/*++

Routine Description:

    This routine gets a yes or no answer from the user.

Arguments:

    Answer - Supplies a pointer where the answer will be returned on success.

Return Value:

    0 if an answer was successfully retrieved.

    Returns an error number on failure.

--*/

{

    INT Character;

    Character = getchar();
    if (Character == -1) {
        return errno;
    }

    *Answer = FALSE;
    if ((Character == 'y') || (Character == 'Y')) {
        *Answer = TRUE;
    }

    //
    // Also swallow any remaining gobbledegook the user may have input.
    //

    while ((Character != '\n') &&
           (Character != '\0') && (Character != -1)) {

        Character = getchar();
    }

    return 0;
}

INT
SwStat (
    PSTR Path,
    BOOL FollowLink,
    struct stat *Stat
    )

/*++

Routine Description:

    This routine stats a file, potentially following a symlink if found.

Arguments:

    Path - Supplies a pointer to a string containing the path to stat.

    FollowLink - Supplies a boolean indicating whether a symbolic link should
        be followed.

    Stat - Supplies a pointer to the stat structure where the information will
        be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure (the value from errno).

--*/

{

    return SwOsStat(Path, FollowLink, Stat);
}

BOOL
SwParseFilePermissionsString (
    PSTR String,
    BOOL IsDirectory,
    mode_t *Mode
    )

/*++

Routine Description:

    This routine parses a file permissions string.

Arguments:

    String - Supplies a pointer to the string.

    IsDirectory - Supplies a boolean indicating if the permission bits are for
        a directory. If they are, then the X permission flips on or off all
        execute (search) bits no matter if one is set. If this is not a
        directory, then the X permission only flips on all execute bits if one
        is already set.

    Mode - Supplies a pointer that on input contains the initial mode. This
        mode will be modified to reflect the directives in the mode string.

Return Value:

    TRUE on success.

    FALSE if the mode string was not valid.

--*/

{

    PSTR NextString;
    BOOL Result;

    //
    // Loop parsing clauses separated by commas.
    //

    while (TRUE) {
        if ((*String >= '0') && (*String <= '7')) {
            Result = SwpParseOctalPermissionsString(String,
                                                    Mode,
                                                    &NextString);

        } else {
            Result = SwpParseFilePermissionsClause(String,
                                                   IsDirectory,
                                                   Mode,
                                                   &NextString);
        }

        if (Result == FALSE) {
            return FALSE;
        }

        if (*NextString == '\0') {
            break;
        }

        if (*NextString != ',') {
            return FALSE;
        }

        String = NextString + 1;
        if (*String == '\0') {
            break;
        }
    }

    return TRUE;
}

INT
SwParseUserAndGroupString (
    PSTR String,
    uid_t *User,
    gid_t *Group
    )

/*++

Routine Description:

    This routine parses a user and group string in the form user:group, where
    user and group can be names or numbers, and the :group part is optional.

Arguments:

    String - Supplies a pointer to the string to parse. This string may be
        modified in place.

    User - Supplies a pointer where the user ID will be returned on success, or
        -1 if the user was not supplied.

    Group - Supplies a pointer where the group ID will be returned on success.
        If a group is not supplied, -1 will be returned here.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR AfterScan;
    PSTR GroupName;
    INT Result;
    PSTR Separator;

    *User = (uid_t)-1;
    *Group = (gid_t)-1;
    GroupName = NULL;
    Separator = strchr(String, ':');
    if (Separator != NULL) {
        *Separator = '\0';
        GroupName = Separator + 1;
    }

    if (*String != '\0') {
        Result = SwGetUserIdFromName(String, User);
        if (Result != 0) {
            *User = strtoul(String, &AfterScan, 10);
            if (*AfterScan != '\0') {
                return EINVAL;
            }
        }
    }

    if (GroupName != NULL) {
        Result = SwGetGroupIdFromName(GroupName, Group);
        if (Result != 0) {
            *Group = strtoul(String, &AfterScan, 10);
            if (*AfterScan != '\0') {
                return EINVAL;
            }
        }
    }

    return 0;
}

INT
SwParseGroupList (
    PSTR String,
    gid_t **GroupList,
    size_t *ListSize
    )

/*++

Routine Description:

    This routine parses a group list in the form group0,group1,...,groupN.
    Each group can either be a name or a number.

Arguments:

    String - Supplies a pointer to the group list string. This may be modified
        in place.

    GroupList - Supplies a pointer where an array of group IDs will be returned
        on success. It is the caller's responsibility to free this memory when
        finished.

    ListSize - Supplies a pointer where the number of elements in the returned
        group list will be returned on success.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR AfterScan;
    gid_t *Array;
    size_t Count;
    PSTR GroupName;
    size_t Index;
    INT Result;
    PSTR Separator;

    *GroupList = NULL;
    *ListSize = 0;

    //
    // Count the number of commas to figure out the number of groups.
    //

    Count = 1;
    GroupName = String;
    while (TRUE) {
        Separator = strchr(GroupName, ',');
        if (Separator == NULL) {
            break;
        }

        Count += 1;
        GroupName = Separator + 1;
    }

    Array = malloc(sizeof(id_t) * Count);
    if (Array == NULL) {
        return ENOMEM;
    }

    memset(Array, 0, sizeof(id_t) * Count);
    Index = 0;
    GroupName = String;
    while (Index < Count) {
        Separator = strchr(GroupName, ',');
        if (Separator != NULL) {
            *Separator = '\0';
        }

        Result = SwGetGroupIdFromName(GroupName, &(Array[Index]));
        if (Result != 0) {
            Array[Index] = strtoul(String, &AfterScan, 10);
            if (*AfterScan != '\0') {
                free(Array);
                return EINVAL;
            }
        }

        Index += 1;
        if (Separator == NULL) {
            break;
        }

        *Separator = ',';
        GroupName = Separator + 1;
    }

    assert(Index <= Count);

    *GroupList = Array;
    *ListSize = Index;
    return 0;
}

ULONGLONG
SwParseFileSize (
    PSTR String
    )

/*++

Routine Description:

    This routine parses a file size, allowing multipliers of b for 512, kB for
    1000, K for 1024, MB for 1000 * 1000, M for 1024 * 1024, and so on.

Arguments:

    String - Supplies the size string.

Return Value:

    -1ULL on failure.

    Returns the size on success.

--*/

{

    PSTR AfterScan;
    ULONGLONG Kilo;
    ULONGLONG Multiplier;
    ULONGLONG Size;
    CHAR Suffix;
    CHAR Suffix2;

    Size = strtoull(String, &AfterScan, 10);
    if (String == AfterScan) {
        return -1ULL;
    }

    String = AfterScan;
    if (*String == '\0') {
        return Size;
    }

    Suffix = tolower(*String);
    String += 1;
    Suffix2 = *String;
    String += 1;
    Kilo = 1024;
    if ((Suffix2 == 'B') || (Suffix2 == 'b')) {
        Kilo = 1000;

    } else if (Suffix2 != '\0') {
        return -1ULL;
    }

    switch (Suffix) {
    case 'b':
        Multiplier = 512;
        break;

    case 'k':
        Multiplier = Kilo;
        break;

    case 'm':
        Multiplier = Kilo * Kilo;
        break;

    case 'g':
        Multiplier = Kilo * Kilo * Kilo;
        break;

    case 't':
        Multiplier = Kilo * Kilo * Kilo * Kilo;
        break;

    default:
        return -1ULL;
    }

    return Size * Multiplier;
}

INT
SwReadLine (
    FILE *Input,
    PSTR *Line
    )

/*++

Routine Description:

    This routine reads a new line into a string.

Arguments:

    Input - Supplies the input to read a line from.

    Line - Supplies a pointer where the allocated line will be returned on
        success. It is the caller's responsibility to free this buffer. This
        may be NULL if EOF is hit and no characters were seen first.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    UINTN Capacity;
    INT Character;
    UINTN NewCapacity;
    PSTR NewString;
    UINTN Size;
    INT Status;
    PSTR String;

    //
    // Loop reading characters until a line is in there.
    //

    Capacity = 0;
    String = NULL;
    Size = 0;
    while (TRUE) {

        //
        // Reallocate if needed.
        //

        if (Size + 2 > Capacity) {
            NewCapacity = Capacity * 2;
            if (NewCapacity == 0) {
                NewCapacity = SWISS_READ_LINE_INITIAL_SIZE;
            }

            assert(Size + 2 < NewCapacity);

            NewString = realloc(String, NewCapacity);
            if (NewString == NULL) {
                Status = ENOMEM;
                goto ReadLineEnd;
            }

            String = NewString;
            Capacity = NewCapacity;
        }

        Character = fgetc(Input);
        if (Character == EOF) {
            if (ferror(Input) != 0) {
                Status = errno;
                goto ReadLineEnd;
            }

            //
            // If there was nothing in this line, return success with the
            // string as NULL.
            //

            if (Size == 0) {
                free(String);
                String = NULL;
                Status = 0;
                goto ReadLineEnd;
            }

            break;
        }

        if (Character == '\n') {

            //
            // Get rid of an \r immediately preceding a \n.
            //

            if ((Size != 0) && (String[Size - 1] == '\r')) {
                Size -= 1;
            }

            break;
        }

        String[Size] = Character;
        Size += 1;
    }

    assert(Size < Capacity);

    String[Size] = '\0';
    Size += 1;
    Status = 0;

ReadLineEnd:
    if (Status != 0) {
        if (String != NULL) {
            free(String);
        }
    }

    *Line = String;
    return Status;
}

INT
SwCreateDirectoryCommand (
    PSTR Path,
    BOOL CreateIntermediateDirectories,
    BOOL Verbose,
    mode_t CreatePermissions
    )

/*++

Routine Description:

    This routine performs the work of the mkdir command, complete with options.

Arguments:

    Path - Supplies a pointer to the null terminated path of the directory to
        create.

    CreateIntermediateDirectories - Supplies a boolean indicating whether or
        not intermediate directories that do not exist should also be created.

    Verbose - Supplies a boolean indicating if a message should be printed out
        for each directory created.

    CreatePermissions - Supplies the permission bits to set for each new
        directory created.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSTR CurrentSeparator;
    mode_t OriginalMask;
    PSTR PathCopy;
    ULONG PathSize;
    int Result;

    //
    // If the option to create intermediate directories is not supplied, this
    // is quite easy.
    //

    OriginalMask = umask(0);
    if ((CreateIntermediateDirectories == FALSE) ||
        (strchr(Path, '/') == NULL)) {

        errno = 0;
        Result = SwMakeDirectory(Path, CreatePermissions);
        if ((Result != 0) &&
            (CreateIntermediateDirectories != FALSE) && (errno == EEXIST)) {

            errno = 0;
            Result = 0;

        } else {
            SwpPrintDirectoryMessage(Path, errno, Verbose);
        }

        goto CreateDirectoryCommandEnd;
    }

    //
    // Create a copy of the path so it can be modified.
    //

    PathSize = strlen(Path) + 1;
    PathCopy = malloc(PathSize);
    if (PathCopy == NULL) {
        Result = ENOMEM;
        goto CreateDirectoryCommandEnd;
    }

    memcpy(PathCopy, Path, PathSize);

    //
    // Skip the pointless system call to try to create the root directory.
    //

    CurrentSeparator = PathCopy;
    while (*CurrentSeparator == '/') {
        CurrentSeparator += 1;
    }

    //
    // Loop creating each component of the directory.
    //

    while (TRUE) {
        errno = 0;
        if (*CurrentSeparator == '\0') {
            break;
        }

        CurrentSeparator = strchr(CurrentSeparator, '/');

        //
        // If there's a separator, create the intermediate directory.
        //

        if (CurrentSeparator != NULL) {
            *CurrentSeparator = '\0';
            Result = SwMakeDirectory(PathCopy,
                                     CreatePermissions | S_IWUSR | S_IXUSR);

            if ((Result == -1) && (errno != EEXIST)) {
                SwpPrintDirectoryMessage(PathCopy, errno, Verbose);
                break;

            } else if (Result == 0) {
                SwpPrintDirectoryMessage(PathCopy, 0, Verbose);
            }

            *CurrentSeparator = '/';
            while (*CurrentSeparator == '/') {
                CurrentSeparator += 1;
            }

        //
        // This is the final component. If creating intermediate directories,
        // don't fail if it already exists.
        //

        } else {
            Result = SwMakeDirectory(PathCopy, CreatePermissions);
            if ((Result != 0) &&
                (CreateIntermediateDirectories != FALSE) && (errno == EEXIST)) {

                errno = 0;

            } else {
                SwpPrintDirectoryMessage(PathCopy, errno, Verbose);
            }

            break;
        }
    }

    if (PathCopy != NULL) {
        free(PathCopy);
    }

    Result = errno;

CreateDirectoryCommandEnd:
    umask(OriginalMask);
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
SwpParseOctalPermissionsString (
    PSTR String,
    mode_t *Mode,
    PSTR *NextString
    )

/*++

Routine Description:

    This routine parses a file permissions string that takes the form of an
    octal number.

Arguments:

    String - Supplies a pointer to the string.

    Mode - Supplies a pointer that on input contains the initial mode. This
        mode will be set to the mode in the octal string.

    NextString - Supplies a pointer where a pointer to the string after the
        number will be returned.

Return Value:

    TRUE on success.

    FALSE if the mode string was not valid.

--*/

{

    mode_t Mask;
    long OctalMode;

    OctalMode = strtol(String, NextString, 8);
    if (*NextString == String) {
        return FALSE;
    }

    //
    // Loop through and set all the corresponding bits, as they don't
    // necessarily line up with the command line format.
    //

    Mask = 0;
    if ((OctalMode & 0x0001) != 0) {
        Mask |= S_IXOTH;
        OctalMode &= ~0x0001;
    }

    if ((OctalMode & 0x0002) != 0) {
        Mask |= S_IWOTH;
        OctalMode &= ~0x0002;
    }

    if ((OctalMode & 0x0004) != 0) {
        Mask |= S_IROTH;
        OctalMode &= ~0x0004;
    }

    if ((OctalMode & 0x0008) != 0) {
        Mask |= S_IXGRP;
        OctalMode &= ~0x0008;
    }

    if ((OctalMode & 0x0010) != 0) {
        Mask |= S_IWGRP;
        OctalMode &= ~0x0010;
    }

    if ((OctalMode & 0x0020) != 0) {
        Mask |= S_IRGRP;
        OctalMode &= ~0x0020;
    }

    if ((OctalMode & 0x0040) != 0) {
        Mask |= S_IXUSR;
        OctalMode &= ~0x0040;
    }

    if ((OctalMode & 0x0080) != 0) {
        Mask |= S_IWUSR;
        OctalMode &= ~0x0080;
    }

    if ((OctalMode & 0x0100) != 0) {
        Mask |= S_IRUSR;
        OctalMode &= ~0x0100;
    }

    if ((OctalMode & 0x0200) != 0) {
        Mask |= S_ISVTX;
        OctalMode &= ~0x200;
    }

    if ((OctalMode & 0x0400) != 0) {
        Mask |= S_ISGID;
        OctalMode &= ~0x0400;
    }

    if ((OctalMode & 0x0800) != 0) {
        Mask |= S_ISUID;
        OctalMode &= ~0x0800;
    }

    //
    // Fail if there are any bits left over.
    //

    if (OctalMode != 0) {
        return FALSE;
    }

    *Mode = Mask;
    return TRUE;
}

BOOL
SwpParseFilePermissionsClause (
    PSTR String,
    BOOL IsDirectory,
    mode_t *Mode,
    PSTR *NextString
    )

/*++

Routine Description:

    This routine parses a file permissions clause (multiple clauses can be
    joined together with a comma, this parses only the first).

Arguments:

    String - Supplies a pointer to the string.

    IsDirectory - Supplies a boolean indicating if the permission bits are for
        a directory. If they are, then the X permission flips on or off all
        execute (search) bits no matter if one is set. If this is not a
        directory, then the X permission only flips on all execute bits if one
        is already set.

    Mode - Supplies a pointer that on input contains the initial mode. This
        mode will be modified to reflect the directives in the mode string.

    NextString - Supplies a pointer where a pointer to the string after the
        parsed parts will be returned.

Return Value:

    TRUE on success.

    FALSE if the mode string was not valid.

--*/

{

    BOOL Execute;
    mode_t ExecuteMask;
    BOOL Group;
    mode_t MaskToClear;
    mode_t MaskToSet;
    CHAR Operator;
    BOOL Other;
    BOOL Read;
    mode_t ReadMask;
    BOOL Search;
    BOOL Set;
    BOOL Sticky;
    BOOL User;
    mode_t WorkingMask;
    BOOL Write;
    mode_t WriteMask;

    Execute = FALSE;
    Group = FALSE;
    MaskToClear = 0;
    MaskToSet = 0;
    Other = FALSE;
    Read = FALSE;
    Search = FALSE;
    Set = FALSE;
    Sticky = FALSE;
    User = FALSE;
    WorkingMask = 0;
    Write = FALSE;

    //
    // Parse an optional who list.
    //

    while (TRUE) {
        if (*String == 'u') {
            User = TRUE;

        } else if (*String == 'g') {
            Group = TRUE;

        } else if (*String == 'o') {
            Other = TRUE;

        } else if (*String == 'a') {
            User = TRUE;
            Group = TRUE;
            Other = TRUE;

        } else {
            break;
        }

        String += 1;
    }

    //
    // If nothing was specified, then this applies to all three.
    //

    if ((User == FALSE) && (Group == FALSE) && (Other == FALSE)) {
        User = TRUE;
        Group = TRUE;
        Other = TRUE;
    }

    //
    // Parse an operator.
    //

    if ((*String != '+') && (*String != '-') && (*String != '=')) {
        return FALSE;
    }

    Operator = *String;
    String += 1;

    //
    // Parse either a permission list (rwxXst) or a permission copy (ugo),
    // figuring out which permissions are being modified.
    //

    if ((*String == 'u') || (*String == 'g') || (*String == 'o')) {
        if (*String == 'u') {
            ReadMask = S_IRUSR;
            WriteMask = S_IWUSR;
            ExecuteMask = S_IXUSR;

        } else if (*String == 'g') {
            ReadMask = S_IRGRP;
            WriteMask = S_IWGRP;
            ExecuteMask = S_IXGRP;

        } else {
            ReadMask = S_IROTH;
            WriteMask = S_IWOTH;
            ExecuteMask = S_IXOTH;
        }

        if ((*Mode & ReadMask) != 0) {
            Read = TRUE;
        }

        if ((*Mode & WriteMask) != 0) {
            Write = TRUE;
        }

        if ((*Mode & ExecuteMask) != 0) {
            Execute = TRUE;
        }

        String += 1;

    } else {
        while (TRUE) {
            if (*String == 'r') {
                Read = TRUE;

            } else if (*String == 'w') {
                Write = TRUE;

            } else if (*String == 'x') {
                Execute = TRUE;

            } else if (*String == 'X') {
                Search = TRUE;

            } else if (*String == 's') {
                Set = TRUE;

            } else if (*String == 't') {
                Sticky = TRUE;

            } else {
                break;
            }

            String += 1;
        }

        //
        // Fail if none are set.
        //

        if ((Read == FALSE) && (Write == FALSE) && (Execute == FALSE) &&
            (Search == FALSE) && (Set == FALSE) && (Sticky == FALSE)) {

            return FALSE;
        }
    }

    //
    // Search is execute but only if the file is a directory or already has
    // execute permission for some user.
    //

    if (Search != FALSE) {
        if ((IsDirectory != FALSE) ||
            ((*Mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0)) {

            Execute = TRUE;
        }
    }

    //
    // Loop through and set up the bits being discussed.
    //

    WorkingMask = 0;
    if (User != FALSE) {
        if (Read != FALSE) {
            WorkingMask |= S_IRUSR;
        }

        if (Write != FALSE) {
            WorkingMask |= S_IWUSR;
        }

        if (Execute != FALSE) {
            WorkingMask |= S_IXUSR;
        }

        if (Set != FALSE) {
            WorkingMask |= S_ISUID;
        }
    }

    if (Group != FALSE) {
        if (Read != FALSE) {
            WorkingMask |= S_IRGRP;
        }

        if (Write != FALSE) {
            WorkingMask |= S_IWGRP;
        }

        if (Execute != FALSE) {
            WorkingMask |= S_IXGRP;
        }

        if (Set != FALSE) {
            WorkingMask |= S_ISGID;
        }
    }

    if (Other != FALSE) {
        if (Read != FALSE) {
            WorkingMask |= S_IROTH;
        }

        if (Write != FALSE) {
            WorkingMask |= S_IWOTH;
        }

        if (Execute != FALSE) {
            WorkingMask |= S_IXOTH;
        }
    }

    if (Sticky != FALSE) {
        WorkingMask |= S_ISVTX;
    }

    //
    // Now, set up the set/clear masks depending on what the operator is.
    //

    if (Operator == '+') {
        MaskToSet = WorkingMask;

    } else if (Operator == '-') {
        MaskToClear = WorkingMask;

    } else if (Operator == '=') {
        MaskToSet = WorkingMask;
        if (User != FALSE) {
            MaskToClear = S_IRUSR | S_IWUSR | S_IXUSR | S_ISUID;
        }

        if (Group != FALSE) {
            MaskToClear = S_IRGRP | S_IWGRP | S_IXGRP | S_ISGID;
        }

        if (Other != FALSE) {
            MaskToClear = S_IROTH | S_IWOTH | S_IXOTH;
        }

        if (Sticky != FALSE) {
            MaskToClear |= S_ISVTX;
        }
    }

    //
    // Finally, modify the mask.
    //

    *Mode &= ~MaskToClear;
    *Mode |= MaskToSet;
    *NextString = String;
    return TRUE;
}

VOID
SwpPrintDirectoryMessage (
    PSTR Path,
    int Error,
    BOOL Verbose
    )

/*++

Routine Description:

    This routine prints a message indicating if the directory creation
    succeeded or failed.

Arguments:

    Path - Supplies the path of the directory that was created.

    Error - Supplies the error number if the creation failed, or 0 if it
        succeeded.

    Verbose - Supplies the value of the verbose flag. If clear, then success
        messages will not be printed.

Return Value:

    None.

--*/

{

    PSTR QuotedPath;

    if ((Error == 0) && (Verbose == FALSE)) {
        return;
    }

    QuotedPath = SwQuoteArgument(Path);
    if (Error == 0) {
        printf("mkdir: Created directory '%s'.\n", QuotedPath);

    } else {
        SwPrintError(errno,
                     NULL,
                     "Failed to create directory '%s'",
                     QuotedPath);
    }

    if (QuotedPath != Path) {
        free(QuotedPath);
    }

    return;
}

