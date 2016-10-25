/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    swlib.h

Abstract:

    This header contains definitions for the Swiss common library.

Author:

    Evan Green 2-Jul-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "swlibos.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define some default values.
//

#define USER_FALLBACK_SHELL "/bin/sh"
#define USER_DEFAULT_PATH "/bin:/usr/bin:/usr/local/bin"
#define SUPERUSER_DEFAULT_PATH \
    "/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin"

#define USER_DEFAULT_LOGIN_SHELL "-/bin/sh"
#define MKDIR_DEFAULT_PERMISSIONS \
    (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | \
     S_IROTH | S_IWOTH | S_IXOTH)

//
// Copy options
//

//
// Set this option to unlink files that could not be truncated.
//

#define COPY_OPTION_UNLINK 0x00000001

//
// Set this option to prompt before overwriting anything.
//

#define COPY_OPTION_INTERACTIVE 0x00000002

//
// Set this option to recursively copy the file hierarchy.
//

#define COPY_OPTION_RECURSIVE 0x00000004

//
// Set this option to follow symbolic links in operands only.
//

#define COPY_OPTION_FOLLOW_OPERAND_LINKS 0x00000008

//
// Set this option to follow all symbolic links.
//

#define COPY_OPTION_FOLLOW_LINKS 0x00000010

//
// Set this option to print out what's going on.
//

#define COPY_OPTION_VERBOSE 0x00000020

//
// Set this option to preserve permissions in the destination.
//

#define COPY_OPTION_PRESERVE_PERMISSIONS 0x00000040

//
// Define delete options.
//

//
// Set this option to disable all prompts.
//

#define DELETE_OPTION_FORCE 0x00000001

//
// Set this option to set prompts for all files.
//

#define DELETE_OPTION_INTERACTIVE 0x00000002

//
// Set this option to recurse down to other subdirectories.
//

#define DELETE_OPTION_RECURSIVE 0x00000004

//
// Set this option to print each file that's deleted.
//

#define DELETE_OPTION_VERBOSE 0x00000008

//
// This internal option is set if standard in is a terminal device.
//

#define DELETE_OPTION_STDIN_IS_TERMINAL 0x00000010

//
// Define chown options.
//

//
// Set this option to print each file processed.
//

#define CHOWN_OPTION_VERBOSE 0x00000001

//
// Set this option to print only changed files.
//

#define CHOWN_OPTION_PRINT_CHANGES 0x00000002

//
// Set this option to be quiet.
//

#define CHOWN_OPTION_QUIET 0x00000004

//
// Set this option to affect symbolic links rather than their destinations.
//

#define CHOWN_OPTION_AFFECT_SYMBOLIC_LINKS 0x00000008

//
// Set this option to be recursive through directories.
//

#define CHOWN_OPTION_RECURSIVE 0x00000010

//
// Set this option to traverse symbolic links to directories on the command
// line.
//

#define CHOWN_OPTION_SYMBOLIC_DIRECTORY_ARGUMENTS 0x00000020

//
// Set this option to traverse all symbolic directories.
//

#define CHOWN_OPTION_SYMBOLIC_DIRECTORIES 0x00000040

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the context for a chown operation.

Members:

    Options - Stores the CHOWN_OPTION_* flags.

    User - Stores the user ID to change files to, or -1 to leave them alone.

    Group - Stores the group ID to change files to -1 to leave them alone.

    FromUser - Stores the user ID to match on to perform a change, or -1 to
        match any user.

    FromGroup - Stores the group ID to match on to perform a change, or -1 to
        match any group.

--*/

typedef struct _CHOWN_CONTEXT {
    ULONG Options;
    uid_t User;
    gid_t Group;
    uid_t FromUser;
    gid_t FromGroup;
} CHOWN_CONTEXT, *PCHOWN_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// User I/O functionality.
//

PSTR
SwGetCurrentApplicationName (
    VOID
    );

/*++

Routine Description:

    This routine returns the current application name.

Arguments:

    None.

Return Value:

    Returns the current application name. The caller must not modify this
    buffer.

--*/

PSTR
SwSetCurrentApplicationName (
    PSTR ApplicationName
    );

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

VOID
SwPrintError (
    INT ErrorNumber,
    PSTR QuotedString,
    PSTR Format,
    ...
    );

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

VOID
SwPrintErrorVaList (
    INT ErrorNumber,
    PSTR QuotedString,
    PSTR Format,
    va_list ArgumentList
    );

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

VOID
SwPrintVersion (
    ULONG MajorVersion,
    ULONG MinorVersion
    );

/*++

Routine Description:

    This routine prints an application version number.

Arguments:

    MajorVersion - Supplies the major version number.

    MinorVersion - Supplies the minor version number.

Return Value:

    None.

--*/

INT
SwGetSerialVersion (
    VOID
    );

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

INT
SwGetYesNoAnswer (
    PBOOL Answer
    );

/*++

Routine Description:

    This routine gets a yes or no answer from the user.

Arguments:

    Answer - Supplies a pointer where the answer will be returned on success.

Return Value:

    0 if an answer was successfully retrieved.

    Returns an error number on failure.

--*/

INT
SwStat (
    PSTR Path,
    BOOL FollowLink,
    struct stat *Stat
    );

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

BOOL
SwParseFilePermissionsString (
    PSTR String,
    BOOL IsDirectory,
    mode_t *Mode
    );

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

INT
SwParseUserAndGroupString (
    PSTR String,
    uid_t *User,
    gid_t *Group
    );

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

INT
SwParseGroupList (
    PSTR String,
    gid_t **GroupList,
    size_t *ListSize
    );

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

ULONGLONG
SwParseFileSize (
    PSTR String
    );

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

INT
SwReadLine (
    FILE *Input,
    PSTR *Line
    );

/*++

Routine Description:

    This routine reads a new line into a string.

Arguments:

    Input - Supplies the input to read a line from.

    Line - Supplies a pointer where the allocated line will be returned on
        success. It is the caller's responsibility to free this buffer.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
SwCreateDirectoryCommand (
    PSTR Path,
    BOOL CreateIntermediateDirectories,
    BOOL Verbose,
    mode_t CreatePermissions
    );

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

//
// Copy file functionality.
//

INT
SwCopy (
    ULONG Options,
    PSTR Source,
    PSTR Destination
    );

/*++

Routine Description:

    This routine performs a copy of the source file or directory to the
    destination.

Arguments:

    Options - Supplies a bitfield of options governing the behavior of the
        copy.

    Source - Supplies a pointer to the string describing the source path to
        copy.

    Destination - Supplies a pointer to the string describing the destination of
        the copy.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

//
// Delete file functionality.
//

INT
SwDelete (
    INT Options,
    PSTR Argument
    );

/*++

Routine Description:

    This routine is the workhorse behind the rm application. It removes one
    file or directory.

Arguments:

    Options - Supplies the application options.

    Argument - Supplies the object to remove.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

INT
ChownChangeOwnership (
    PCHOWN_CONTEXT Context,
    PSTR Path,
    ULONG RecursionDepth
    );

/*++

Routine Description:

    This routine executes the body of the chown utility action on a single
    argument.

Arguments:

    Context - Supplies a pointer to the chown context.

    Path - Supplies the path to change.

    RecursionDepth - Supplies the recursion depth. Supply 0 initially.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

//
// String functionality.
//

BOOL
SwAppendPath (
    PSTR Prefix,
    ULONG PrefixSize,
    PSTR Component,
    ULONG ComponentSize,
    PSTR *AppendedPath,
    PULONG AppendedPathSize
    );

/*++

Routine Description:

    This routine appends a path component to a path.

Arguments:

    Prefix - Suppiles the initial path string. This can be null.

    PrefixSize - Supplies the size of the prefix string in bytes including the
        null terminator.

    Component - Supplies a pointer to the component string to add.

    ComponentSize - Supplies the size of the component string in bytes
        including a null terminator.

    AppendedPath - Supplies a pointer where the new path will be returned. The
        caller is responsible for freeing this memory..

    AppendedPathSize - Supplies a pointer where the size of the appended bath
        buffer in bytes including the null terminator will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

PSTR
SwQuoteArgument (
    PSTR Argument
    );

/*++

Routine Description:

    This routine puts a backslash before every single quote and backslash in
    the given argument.

Arguments:

    Argument - Supplies a pointer to the argument to quote.

Return Value:

    Returns the original argument if it needs no quoting.

    Returns a newly allocated string if the argument needs quoting. The caller
    is responsible for freeing this string.

--*/

PSTR
SwStringDuplicate (
    PSTR String,
    UINTN StringSize
    );

/*++

Routine Description:

    This routine copies a string.

Arguments:

    String - Supplies a pointer to the string to copy.

    StringSize - Supplies the size of the string buffer in bytes.

Return Value:

    Returns a pointer to the new string on success.

    NULL on failure.

--*/

BOOL
SwStringReplaceRegion (
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    UINTN SourceRegionBegin,
    UINTN SourceRegionEnd,
    PSTR Replacement,
    UINTN ReplacementSize
    );

/*++

Routine Description:

    This routine replaces a portion of the given string.

Arguments:

    StringBufferAddress - Supplies a pointer to the address of the allocated
        string buffer. This value may be changed if the string is reallocated.

    StringBufferSize - Supplies a pointer that on input contains the size of
        the current string in bytes, including the null terminator. On output,
        it will contain the updated size of the string in bytes.

    StringBufferCapacity - Supplies a pointer that on input supplies the total
        size of the buffer. It will contain the updated size of the buffer
        allocation on output.

    SourceRegionBegin - Supplies the index into the buffer where the
        replacement occurs.

    SourceRegionEnd - Supplies the index into the buffer where the expansion
        ends, exclusive.

    Replacement - Supplies a pointer to the string to replace that region with.

    ReplacementSize - Supplies the size of the replacement string in bytes
        including the null terminator.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

VOID
SwStringRemoveRegion (
    PSTR String,
    PUINTN StringSize,
    UINTN RemoveIndex,
    UINTN RemoveLength
    );

/*++

Routine Description:

    This routine removes a portion of the given string.

Arguments:

    String - Supplies a pointer to the string to remove a portion of.

    StringSize - Supplies a pointer that on input contains the size of the
        string in bytes including the null terminator. On output this value
        will be updated to reflect the removal.

    RemoveIndex - Supplies the starting index to remove.

    RemoveLength - Supplies the number of characters to remove.

Return Value:

    None.

--*/

BOOL
SwRotatePointerArray (
    PVOID *Array,
    ULONG ColumnCount,
    ULONG RowCount
    );

/*++

Routine Description:

    This routine rotates an array by row and column, so that an array that
    used to read 1 2 3 4 / 5 6 7 8 will now read 1 3 5 7 / 2 4 6 8. The row
    and column counts don't change, but after this transformation the elements
    can be read down the column instead of across the row.

Arguments:

    Array - Supplies the array of pointers to rotate.

    ColumnCount - Supplies the number of columns that the array is
        represented in.

    RowCount - Supplies the number of rows that the array is represented in.

Return Value:

    TRUE on success.

    FALSE on temporary allocation failure.

--*/

INT
SwGetSignalNumberFromName (
    PSTR SignalName
    );

/*++

Routine Description:

    This routine parses a signal number. This can either be a numerical value,
    a string like TERM, or a string like SIGTERM. The string is matched without
    regard for case.

Arguments:

    SignalName - Supplies a pointer to the string to convert to a signal
        number.

Return Value:

    Returns the corresponding signal number on success.

    -1 on failure.

--*/

PSTR
SwGetSignalNameFromNumber (
    INT SignalNumber
    );

/*++

Routine Description:

    This routine returns a pointer to a constant string for the given signal
    number. This string does not have the SIG prefix appended to it.

Arguments:

    SignalNumber - Supplies the signal number to get the string for.

Return Value:

    Returns a pointer to a string containing the name of the signal on success.

    NULL if the signal number is invalid.

--*/

BOOL
SwDoesPathPatternMatch (
    PSTR Path,
    UINTN PathSize,
    PSTR Pattern,
    UINTN PatternSize
    );

/*++

Routine Description:

    This routine determines if a given path matches a given pattern. This
    routine assumes it's only comparing path components, and does not do any
    splitting or other special processing on slashes.

Arguments:

    Path - Supplies a pointer to the path component string.

    PathSize - Supplies the size of the path component in bytes including the
        null terminator.

    Pattern - Supplies the pattern string to match against.

    PatternSize - Supplies the size of the pattern string in bytes including
        the null terminator if there is one.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
SwDoesPatternMatch (
    PSTR Input,
    UINTN InputSize,
    PSTR Pattern,
    UINTN PatternSize
    );

/*++

Routine Description:

    This routine determines if the given input matches the given pattern.

Arguments:

    Input - Supplies a pointer to the input string to test.

    InputSize - Supplies the size of the input string in bytes including the
        null terminator.

    Pattern - Supplies a pointer to the pattern string to test against.

    PatternSize - Supplies the size of the pattern string in bytes including
        the null terminator.

Return Value:

    TRUE if the input matches the pattern.

    FALSE if the input does not match the pattern.

--*/

INT
SwPwdCommand (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the pwd (print working directory)
    utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    0 on success.

    Non-zero error on failure.

--*/

