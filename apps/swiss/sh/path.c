/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    path.c

Abstract:

    This module implements path traversal and other path utilities for the
    shell.

Author:

    Evan Green 11-Jun-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "sh.h"
#include "../swlib.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro determines if the given path string starts with a relative
// component, a dot or a dot dot.
//

#define PATH_IS_RELATIVE_TO_CURRENT(_Path)              \
    (((_Path)[0] == '.') &&                             \
     (((_Path)[1] == '\0') || ((_Path)[1] == '/') ||    \
      (((_Path)[1] == '.') &&                           \
       (((_Path)[2] == '\0') || ((_Path)[2] == '/')))))

//
// ---------------------------------------------------------------- Definitions
//

#define SHELL_DIRECTORY_NAMES_INITIAL_LENGTH 256
#define SHELL_DIRECTORY_INITIAL_ELEMENT_COUNT 16
#define SHELL_INITIAL_PATH_BUFFER_SIZE 256
#define SHELL_INITIAL_PATH_LIST_SIZE 16

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
ShExpandPath (
    PSHELL Shell,
    PSTR Prefix,
    ULONG PrefixSize,
    PSTR Field,
    PSTR *FilesStringBuffer,
    PULONG FilesStringBufferSize,
    PSTR **FilesArray,
    PULONG FilesArrayCount
    );

BOOL
ShArePatternCharactersInPath (
    PSTR Path,
    ULONG PathSize
    );

BOOL
ShPathCombineLists (
    PSTR *ListBuffer,
    PULONG ListBufferSize,
    PULONG ListBufferCapacity,
    PSTR **List,
    PULONG ListSize,
    PULONG ListCapacity,
    PSTR SecondListBuffer,
    ULONG SecondListBufferSize,
    PSTR *SecondList,
    ULONG SecondListSize
    );

BOOL
ShLocateDirectoryOnCdPath (
    PSHELL Shell,
    PSTR Directory,
    UINTN DirectorySize,
    PSTR *FullDirectoryPath,
    PUINTN FullDirectoryPathSize
    );

INT
ShCleanLogicalDirectoryPath (
    PSTR PathString,
    UINTN PathStringSize,
    PSTR *CleanedPathString,
    PUINTN CleanedPathStringSize
    );

PSTR
ShPathGetNextComponent (
    PSTR Field,
    PBOOL HasMetaCharacters
    );

int
ShPathCompareStrings (
    const void *LeftString,
    const void *RightString
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

BOOL
ShGetCurrentDirectory (
    PSTR *Directory,
    PUINTN DirectorySize
    )

/*++

Routine Description:

    This routine gets a listing of the files in the current directory.

Arguments:

    Directory - Supplies a pointer where the current directory path will be
        returned. The caller is responsible for freeing this memory.

    DirectorySize - Supplies a pointer where the size of the directory string
        including the null terminator will be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR Buffer;
    UINTN Capacity;
    PSTR NewBuffer;
    unsigned long PathSize;

    *Directory = NULL;
    *DirectorySize = 0;
    Capacity = SHELL_INITIAL_PATH_BUFFER_SIZE;
    Buffer = malloc(Capacity);
    if (Buffer == NULL) {
        return FALSE;
    }

    while (TRUE) {
        if (getcwd(Buffer, Capacity) != Buffer) {
            if (errno != ERANGE) {
                free(Buffer);
                return FALSE;
            }

            Capacity *= 2;
            NewBuffer = realloc(Buffer, Capacity);
            if (NewBuffer == NULL) {

                assert(Capacity != 0);

                free(Buffer);
                return FALSE;
            }

            Buffer = NewBuffer;

        } else {
            *Directory = Buffer;
            PathSize = strlen(Buffer) + 1;
            ShFixUpPath(Directory, &PathSize);
            *DirectorySize = PathSize;
            return TRUE;
        }
    }

    return FALSE;
}

BOOL
ShGetDirectoryListing (
    PSTR DirectoryPath,
    PSTR *FileNamesBuffer,
    PSHELL_DIRECTORY_ENTRY *Elements,
    PULONG ElementCount
    )

/*++

Routine Description:

    This routine gets a listing of the files in the current directory.

Arguments:

    DirectoryPath - Supplies a pointer to the string containing the directory
        to list.

    FileNamesBuffer - Supplies a pointer where a pointer to the files names
        will be returned on success. The elements array will contain pointers
        into the buffer. The caller is responsible for freeing this memory. A
        size is not returned because the caller is not expected to dereference
        into this memory directly.

    Elements - Supplies a pointer where the array of file names will be
        returned on success. The caller is responsible for freeing this buffer,
        which can be accomplished by freeing the first element.

    ElementCount - Supplies a pointer where the number of elements in the array
        will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR CurrentFileName;
    DIR *Directory;
    PSHELL_DIRECTORY_ENTRY Entries;
    struct dirent *Entry;
    UINTN EntryCapacity;
    UINTN EntrySize;
    PSTR FileNames;
    UINTN  FileNamesCapacity;
    UINTN FileNamesSize;
    ULONG FixIndex;
    UINTN NameSize;
    PVOID NewBuffer;
    UINTN NewBufferSize;
    UINTN OriginalFileNames;
    BOOL Result;

    Entries = NULL;
    EntryCapacity = 0;
    EntrySize = 0;
    FileNames = NULL;
    FileNamesSize = 0;
    FileNamesCapacity = 0;

    //
    // Use the current directory given no other information.
    //

    if (DirectoryPath == NULL) {
        DirectoryPath = ".";
    }

    //
    // Open up the directory and loop reading entries.
    //

    Directory = opendir(DirectoryPath);
    if (Directory == NULL) {
        Result = FALSE;
        goto GetDirectoryListingEnd;
    }

    while (TRUE) {
        Entry = readdir(Directory);
        if (Entry == NULL) {
            break;
        }

        //
        // Write the string to the big buffer, expanding the buffer if needed.
        //

        NameSize = strlen(Entry->d_name) + 1;
        if (FileNamesSize + NameSize > FileNamesCapacity) {
            NewBufferSize = FileNamesCapacity;
            if (NewBufferSize == 0) {

                assert(FileNames == NULL);

                NewBufferSize = SHELL_DIRECTORY_NAMES_INITIAL_LENGTH;
            }

            while (NewBufferSize < FileNamesSize + NameSize) {
                NewBufferSize *= 2;
            }

            OriginalFileNames = (UINTN)FileNames;
            NewBuffer = realloc(FileNames, NewBufferSize);
            if (NewBuffer == NULL) {
                Result = FALSE;
                goto GetDirectoryListingEnd;
            }

            FileNames = NewBuffer;
            FileNamesCapacity = NewBufferSize;

            //
            // Fix up all the entries so far.
            //

            for (FixIndex = 0; FixIndex < EntrySize; FixIndex += 1) {
                Entries[FixIndex].Name = FileNames +
                                         ((UINTN)(Entries[FixIndex].Name) -
                                          OriginalFileNames);
            }
        }

        CurrentFileName = FileNames + FileNamesSize;
        strcpy(CurrentFileName, Entry->d_name);
        FileNamesSize += NameSize;

        assert(FileNamesSize <= FileNamesCapacity);

        //
        // Write the entry to the array, expanding it if needed.
        //

        if (EntrySize + 1 > EntryCapacity) {
            NewBufferSize = EntryCapacity * 2;
            if (NewBufferSize == 0) {

                assert(Entries == NULL);

                NewBufferSize = SHELL_DIRECTORY_INITIAL_ELEMENT_COUNT;
            }

            NewBuffer = realloc(Entries,
                                NewBufferSize * sizeof(SHELL_DIRECTORY_ENTRY));

            if (NewBuffer == NULL) {
                Result = FALSE;
                goto GetDirectoryListingEnd;
            }

            Entries = NewBuffer;
            EntryCapacity = NewBufferSize;
        }

        Entries[EntrySize].Name = CurrentFileName;
        Entries[EntrySize].NameSize = NameSize;
        EntrySize += 1;
    }

    Result = TRUE;

GetDirectoryListingEnd:
    if (Directory != NULL) {
        closedir(Directory);
    }

    if (Result == FALSE) {
        if (Entries != NULL) {
            free(Entries);
            Entries = NULL;
        }

        EntryCapacity = 0;
        EntrySize = 0;
        if (FileNames != NULL) {
            free(FileNames);
            FileNames = NULL;
        }

        FileNamesSize = 0;
        FileNamesCapacity = 0;
    }

    *FileNamesBuffer = FileNames;
    *Elements = Entries;
    *ElementCount = EntrySize;
    return Result;
}

BOOL
ShPerformPathExpansions (
    PSHELL Shell,
    PSTR *StringBuffer,
    PUINTN StringBufferSize,
    PSTR **FieldArray,
    PULONG FieldArrayCount
    )

/*++

Routine Description:

    This routine performs pathname expansion on the fields in the given field
    array.

Arguments:

    Shell - Supplies a pointer to the shell.

    StringBuffer - Supplies a pointer where the address of the fields string
        buffer is on input. On output, this may contain a different buffer that
        all the fields point into.

    StringBufferSize - Supplies a pointer that contains the size of the fields
        string buffer. This value will be updated to reflect the new size.

    FieldArray - Supplies a pointer to the array of string pointers of the
        fields on input. This array may get replaced if more elements need to
        be added for paths.

    FieldArrayCount - Supplies a pointer that contains the number of field
        elements on input. This value will be updated to reflect the number of
        fields after pathname expansion.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG Delta;
    ULONG FieldCapacity;
    ULONG FieldCount;
    ULONG FieldEnd;
    ULONG FieldIndex;
    ULONG FieldOffset;
    PSTR *Fields;
    ULONG FileCount;
    ULONG FileOffset;
    PSTR *Files;
    PSTR FilesString;
    ULONG FilesStringSize;
    ULONG FixIndex;
    PVOID NewBuffer;
    UINTN OriginalStringAddress;
    BOOL Result;
    PSTR String;
    UINTN StringCapacity;
    UINTN StringSize;

    FieldCount = *FieldArrayCount;
    FieldCapacity = FieldCount;
    Fields = *FieldArray;
    Files = NULL;
    FilesString = NULL;
    String = *StringBuffer;
    StringSize = *StringBufferSize;
    StringCapacity = StringSize;
    for (FieldIndex = 0; FieldIndex < FieldCount; FieldIndex += 1) {

        //
        // Get the array of files that expand out under this path.
        //

        Result = ShExpandPath(Shell,
                              NULL,
                              0,
                              Fields[FieldIndex],
                              &FilesString,
                              &FilesStringSize,
                              &Files,
                              &FileCount);

        if (Result == FALSE) {
            ShPrintTrace(Shell,
                         "Failed to expand path '%s'",
                         Fields[FieldIndex]);

            goto PerformPathExpansionsEnd;
        }

        //
        // If this expands out to zero files, leave the field alone and move
        // on.
        //

        if (FileCount == 0) {
            continue;
        }

        //
        // Sort the array. Scripts have come to rely on this behavior.
        //

        qsort(Files, FileCount, sizeof(char *), ShPathCompareStrings);

        //
        // Replace the portion of the string.
        //

        OriginalStringAddress = (UINTN)String;
        FieldOffset = (UINTN)(Fields[FieldIndex]) - OriginalStringAddress;
        FieldEnd = FieldOffset + strlen(Fields[FieldIndex]);
        Result = SwStringReplaceRegion(&String,
                                       &StringSize,
                                       &StringCapacity,
                                       FieldOffset,
                                       FieldEnd,
                                       FilesString,
                                       FilesStringSize);

        if (Result == FALSE) {
            goto PerformPathExpansionsEnd;
        }

        FilesStringSize -= 1;

        //
        // Expand the capacity of the fields if needed.
        //

        if (FieldCount + FileCount > FieldCapacity) {
            while (FieldCount + FileCount + 1 > FieldCapacity) {
                FieldCapacity *= 2;
            }

            NewBuffer = realloc(Fields, FieldCapacity * sizeof(PSTR));
            if (Fields == NULL) {
                Result = FALSE;
                goto PerformPathExpansionsEnd;
            }

            Fields = NewBuffer;
            memset(Fields + FieldCount,
                   0,
                   (FieldCapacity - FieldCount) * sizeof(PSTR));
        }

        //
        // Fix up the field pointers in three parts. The first part is fields
        // after the expansion, which need to be both shifted out in index and
        // adjusted to the new string and new size of that string.
        //

        Delta = FilesStringSize - (FieldEnd - FieldOffset);
        FieldCount += FileCount - 1;
        for (FixIndex = FieldCount - 1;
             FixIndex >= FieldIndex + FileCount;
             FixIndex -= 1) {

            FieldOffset = (UINTN)(Fields[FixIndex - (FileCount - 1)]) -
                          OriginalStringAddress;

            Fields[FixIndex] = String + FieldOffset + Delta;
        }

        //
        // Fix up the fields that came before the expansion, which only really
        // have to watch out for being in a new buffer potentially.
        //

        for (FixIndex = 0; FixIndex < FieldIndex; FixIndex += 1) {
            FieldOffset = (UINTN)(Fields[FixIndex]) - OriginalStringAddress;
            Fields[FixIndex] = String + FieldOffset;
        }

        //
        // Fix up fields in the expansion.
        //

        FieldOffset = (UINTN)(Fields[FieldIndex]) - OriginalStringAddress;
        for (FixIndex = FieldIndex;
             FixIndex < FieldIndex + FileCount;
             FixIndex += 1) {

            //
            // Get the offset of this file from the file string buffer.
            //

            FileOffset = ((UINTN)(Files[FixIndex - FieldIndex]) -
                         (UINTN)FilesString);

            //
            // The offset into the big string is the original offset of this
            // field plus the offset into the file buffer that got spliced in.
            //

            Fields[FixIndex] = String + FieldOffset + FileOffset;
        }

        //
        // Set the next field index to look at to just after this new rash of
        // things.
        //

        FieldIndex += FileCount - 1;
    }

    Result = TRUE;

PerformPathExpansionsEnd:
    if (FilesString != NULL) {
        free(FilesString);
    }

    if (Files != NULL) {
        free(Files);
    }

    *StringBuffer = String;
    *StringBufferSize = StringSize;
    *FieldArray = Fields;
    *FieldArrayCount = FieldCount;
    return Result;
}

BOOL
ShLocateCommand (
    PSHELL Shell,
    PSTR Command,
    ULONG CommandSize,
    BOOL MustBeExecutable,
    PSTR *FullCommand,
    PULONG FullCommandSize,
    PINT ReturnValue
    )

/*++

Routine Description:

    This routine locates a command using the PATH environment variable.

Arguments:

    Shell - Supplies a pointer to the shell.

    Command - Supplies a pointer to the command as seen from the command line.

    CommandSize - Supplies the size of the command string in bytes.

    MustBeExecutable - Supplies a boolean indicating if the given file must be
        executable or not.

    FullCommand - Supplies a pointer where a pointer to the full command string
        will be returned on success. If this is not the same pointer as the
        command string then the caller is responsible for freeing this buffer.

    FullCommandSize - Supplies a pointer where the size of the full command
        string will be returned.

    ReturnValue - Supplies a pointer where a premature return value will be
        returned. If this is not zero, then it contains the value that should
        be returned without trying to execute the command. On success, this
        variable will not be touched.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR CompletePath;
    ULONG CompletePathSize;
    PSTR CurrentPath;
    ULONG CurrentPathSize;
    PSTR ExtendedPath;
    PSTR Extension;
    ULONG ExtensionIndex;
    ULONG ExtensionLength;
    PSTR *ExtensionList;
    unsigned int ExtensionListCount;
    CHAR ListSeparator;
    PSTR NextListSeparator;
    PSTR Path;
    UINTN PathSize;
    BOOL Result;
    struct stat Stat;
    INT Status;

    *FullCommand = NULL;
    *FullCommandSize = 0;
    *ReturnValue = 0;
    CompletePath = NULL;
    ExtendedPath = NULL;
    ShGetExecutableExtensions(&ExtensionList, &ExtensionListCount);
    ListSeparator = PATH_LIST_SEPARATOR;
    if (ShExecutableBitSupported == 0) {
        MustBeExecutable = FALSE;
    }

    //
    // If there command has a slash, then don't use the path variable, just go
    // for it directly.
    //

    if (SwDoesPathHaveSeparators(Command) != 0) {
        Status = SwStat(Command, TRUE, &Stat);
        if ((Status == 0) && (S_ISREG(Stat.st_mode)) &&
            ((MustBeExecutable == FALSE) || ((Stat.st_mode & S_IXUSR) != 0))) {

            *FullCommand = Command;
            *FullCommandSize = CommandSize;
            Result = TRUE;
            goto LocateCommandEnd;
        }

        //
        // Fail if the file is there but not executable.
        //

        if ((Status == 0) && (MustBeExecutable != FALSE) &&
            ((Stat.st_mode & S_IXUSR) == 0)) {

            *ReturnValue = SHELL_ERROR_EXECUTE;
            Result = TRUE;
            goto LocateCommandEnd;
        }

        //
        // Try that same thing with all the different extensions on it.
        //

        for (ExtensionIndex = 0;
             ExtensionIndex < ExtensionListCount;
             ExtensionIndex += 1) {

            Extension = ExtensionList[ExtensionIndex];
            ExtensionLength = strlen(Extension);
            ExtendedPath = malloc(CommandSize + ExtensionLength);
            if (ExtendedPath == NULL) {
                Result = FALSE;
                goto LocateCommandEnd;
            }

            memcpy(ExtendedPath, Command, CommandSize - 1);
            memcpy(ExtendedPath + CommandSize - 1, Extension, ExtensionLength);
            ExtendedPath[CommandSize + ExtensionLength - 1] = '\0';
            Status = SwStat(ExtendedPath, TRUE, &Stat);
            if ((Status == 0) && (S_ISREG(Stat.st_mode)) &&
                ((MustBeExecutable == FALSE) ||
                 ((Stat.st_mode & S_IXUSR) != 0))) {

                *FullCommand = ExtendedPath;
                *FullCommandSize = CommandSize + ExtensionLength;
                ExtendedPath = NULL;
                Result = TRUE;
                goto LocateCommandEnd;
            }

            free(ExtendedPath);
            ExtendedPath = NULL;
        }

        *ReturnValue = SHELL_ERROR_OPEN;
        Result = TRUE;
        goto LocateCommandEnd;
    }

    //
    // Get the PATH environment variable.
    //

    Result = ShGetVariable(Shell,
                           SHELL_PATH,
                           sizeof(SHELL_PATH),
                           &Path,
                           &PathSize);

    //
    // If the path variable couldn't be found or is empty, then just return.
    //

    if ((Result == FALSE) || (Path == NULL) || (PathSize <= 1)) {
        Status = SwStat(Command, TRUE, &Stat);
        if ((Status < 0) || (!S_ISREG(Stat.st_mode))) {
            *ReturnValue = SHELL_ERROR_OPEN;

        } else if ((MustBeExecutable != FALSE) &&
                   ((Stat.st_mode & S_IXUSR) == 0)) {

            *ReturnValue = SHELL_ERROR_EXECUTE;

        } else {
            *FullCommand = Command;
            *FullCommandSize = CommandSize;
        }

        Result = TRUE;
        goto LocateCommandEnd;
    }

    //
    // Loop through each entry in the path.
    //

    CurrentPath = Path;
    NextListSeparator = strchr(CurrentPath, ListSeparator);
    while (TRUE) {
        if (NextListSeparator == NULL) {
            CurrentPathSize = PathSize - ((UINTN)CurrentPath - (UINTN)Path);

        } else {
            CurrentPathSize = (UINTN)NextListSeparator - (UINTN)CurrentPath;
        }

        if (CurrentPathSize == 0) {
            CurrentPath = ".";
            CurrentPathSize = sizeof(".");
        }

        //
        // Make a complete command path out of this path entry and the command.
        //

        Result = SwAppendPath(CurrentPath,
                              CurrentPathSize,
                              Command,
                              CommandSize,
                              &CompletePath,
                              &CompletePathSize);

        if (Result == FALSE) {
            goto LocateCommandEnd;
        }

        //
        // Figure out if this is something legit.
        //

        Status = SwStat(CompletePath, TRUE, &Stat);
        if ((Status == 0) && (S_ISREG(Stat.st_mode)) &&
            ((MustBeExecutable == FALSE) ||
             ((Stat.st_mode & S_IXUSR) != 0))) {

            *FullCommand = CompletePath;
            *FullCommandSize = CompletePathSize;
            CompletePath = NULL;
            Result = TRUE;
            goto LocateCommandEnd;
        }

        //
        // Try that same thing with all the different extensions on it.
        //

        for (ExtensionIndex = 0;
             ExtensionIndex < ExtensionListCount;
             ExtensionIndex += 1) {

            Extension = ExtensionList[ExtensionIndex];
            ExtensionLength = strlen(Extension);
            ExtendedPath = malloc(CompletePathSize + ExtensionLength);
            if (ExtendedPath == NULL) {
                Result = FALSE;
                goto LocateCommandEnd;
            }

            memcpy(ExtendedPath, CompletePath, CompletePathSize - 1);
            memcpy(ExtendedPath + CompletePathSize - 1,
                   Extension,
                   ExtensionLength);

            ExtendedPath[CompletePathSize + ExtensionLength - 1] = '\0';
            Status = SwStat(ExtendedPath, TRUE, &Stat);
            if ((Status == 0) && (S_ISREG(Stat.st_mode)) &&
                ((MustBeExecutable == FALSE) ||
                 ((Stat.st_mode & S_IXUSR) != 0))) {

                *FullCommand = ExtendedPath;
                *FullCommandSize = CompletePathSize + ExtensionLength;
                ExtendedPath = NULL;
                Result = TRUE;
                goto LocateCommandEnd;
            }

            free(ExtendedPath);
            ExtendedPath = NULL;
        }

        free(CompletePath);
        CompletePath = NULL;

        //
        // If this was the last entry, stop.
        //

        if (NextListSeparator == NULL) {
            break;
        }

        //
        // Move to the next path.
        //

        CurrentPath = NextListSeparator + 1;
        NextListSeparator = strchr(CurrentPath, ListSeparator);
    }

    //
    // Nothing was found.
    //

    *ReturnValue = SHELL_ERROR_OPEN;
    Result = TRUE;

LocateCommandEnd:
    if (CompletePath != NULL) {
        free(CompletePath);
    }

    if (ExtendedPath != NULL) {
        free(ExtendedPath);
    }

    return Result;
}

INT
ShBuiltinPwd (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin pwd (print working directory) command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    return SwPwdCommand(ArgumentCount, Arguments);
}

INT
ShBuiltinCd (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin cd (change directory) command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR Argument;
    ULONG ArgumentIndex;
    ULONG ArgumentSize;
    PSTR CdPathDirectory;
    UINTN CdPathDirectorySize;
    ULONG CharacterIndex;
    PSTR CleanedDirectory;
    UINTN CleanedDirectorySize;
    PSTR CurrentDirectory;
    UINTN CurrentDirectorySize;
    PSTR Destination;
    UINTN DestinationSize;
    PSTR FullDirectory;
    ULONG FullDirectorySize;
    BOOL LogicalMode;
    PSTR NewOldCurrentDirectory;
    UINTN NewOldCurrentDirectorySize;
    BOOL RelativeToCurrent;
    BOOL Result;
    INT ReturnValue;
    BOOL UseOldWorkingDirectory;

    CdPathDirectory = NULL;
    CleanedDirectory = NULL;
    CurrentDirectory = NULL;
    FullDirectory = NULL;
    FullDirectorySize = 0;
    NewOldCurrentDirectory = NULL;
    ReturnValue = 1;

    //
    // Parse the arguments.
    //

    LogicalMode = TRUE;
    UseOldWorkingDirectory = FALSE;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];
        ArgumentSize = strlen(Argument);
        if (Argument[0] != '-') {
            break;
        }

        if (strcmp(Argument, "--") == 0) {
            break;
        }

        if (strcmp(Argument, "-") == 0) {
            UseOldWorkingDirectory = TRUE;
            continue;
        }

        for (CharacterIndex = 1;
             CharacterIndex < ArgumentSize;
             CharacterIndex += 1) {

            switch (Argument[CharacterIndex]) {
            case 'L':
                LogicalMode = TRUE;
                break;

            case 'P':
                LogicalMode = FALSE;
                break;

            default:
                PRINT_ERROR("cd: invalid option -%c.\n",
                            Argument[CharacterIndex]);

                goto BuiltinCdEnd;
            }
        }
    }

    Destination = NULL;
    DestinationSize = 0;

    //
    // Use the old working directory if - was supplied.
    //

    if (UseOldWorkingDirectory != FALSE) {
        Result = ShGetVariable(Shell,
                               SHELL_OLDPWD,
                               sizeof(SHELL_OLDPWD),
                               &Destination,
                               &DestinationSize);

        if ((Result != FALSE) && (Destination != NULL)) {
            FullDirectory = SwStringDuplicate(Destination, DestinationSize);
            if (FullDirectory == NULL) {
                ReturnValue = ENOMEM;
                goto BuiltinCdEnd;
            }

            FullDirectorySize = DestinationSize;
            Destination = FullDirectory;
        }
    }

    //
    // Get the current directory as the future old directory.
    //

    ShGetVariable(Shell,
                  SHELL_PWD,
                  sizeof(SHELL_PWD),
                  &NewOldCurrentDirectory,
                  &NewOldCurrentDirectorySize);

    //
    // If there's no directory operand, use the value of HOME.
    //

    if ((Destination == NULL) && (ArgumentIndex == ArgumentCount)) {
        Result = ShGetVariable(Shell,
                               SHELL_HOME,
                               sizeof(SHELL_HOME),
                               &Destination,
                               &DestinationSize);

        if ((Result == FALSE) || (Destination == NULL)) {
            goto BuiltinCdEnd;
        }

    } else if (Destination == NULL) {
        Destination = Arguments[ArgumentIndex];
        DestinationSize = strlen(Destination) + 1;
    }

    assert(Destination != NULL);

    //
    // Perform some work on relative paths. Detect both paths that start with
    // slash, and the C: format of Windows.
    //

    if ((Destination[0] != '/') && (Destination[0] != '\0') &&
        (Destination[1] != ':')) {

        //
        // If the first component is a dot or dot dot, then it's a relative
        // directory, so ignore CDPATH.
        //

        FullDirectory = NULL;
        FullDirectorySize = 0;
        RelativeToCurrent = FALSE;
        if (PATH_IS_RELATIVE_TO_CURRENT(Destination)) {
            RelativeToCurrent = TRUE;
        }

        //
        // If the pathname does not begin with a slash or a dot, it's relative,
        // so try the paths in CDPATH.
        //

        if (RelativeToCurrent == FALSE) {
            Result = ShLocateDirectoryOnCdPath(Shell,
                                               Destination,
                                               DestinationSize,
                                               &CdPathDirectory,
                                               &CdPathDirectorySize);

            assert(((Result == FALSE) && (CdPathDirectory == NULL)) ||
                   ((Result != FALSE) && (CdPathDirectory != NULL)));

            assert(CdPathDirectory != Destination);
        }

        //
        // If the path is relative to the current directory specifically or
        // CDPATH didn't turn up anything, append the current directory.
        //

        if (((CdPathDirectory == NULL) ||
             (PATH_IS_RELATIVE_TO_CURRENT(CdPathDirectory))) &&
            (NewOldCurrentDirectory != NULL)) {

            if (CdPathDirectory != NULL) {
                Destination = CdPathDirectory;
                DestinationSize = CdPathDirectorySize;
            }

            Result = SwAppendPath(NewOldCurrentDirectory,
                                  NewOldCurrentDirectorySize,
                                  Destination,
                                  DestinationSize,
                                  &FullDirectory,
                                  &FullDirectorySize);

            CurrentDirectory = NULL;
            if (Result == FALSE) {
                goto BuiltinCdEnd;
            }

            Destination = FullDirectory;
            DestinationSize = FullDirectorySize;

        //
        // If CDPATH did come up with something absolute, use it.
        //

        } else if (CdPathDirectory != NULL) {
            Destination = CdPathDirectory;
            DestinationSize = CdPathDirectorySize;
        }
    }

    //
    // If logical mode is on, clean up the path, removing dot components, dot-
    // dot components, and extra random slashes.
    //

    if (LogicalMode != FALSE) {
        ReturnValue = ShCleanLogicalDirectoryPath(Destination,
                                                  DestinationSize,
                                                  &CleanedDirectory,
                                                  &CleanedDirectorySize);

        if (ReturnValue != 0) {
            goto BuiltinCdEnd;
        }

        Destination = CleanedDirectory;
        DestinationSize = CleanedDirectorySize;
    }

    //
    // Ok, let's change directories.
    //

    if (chdir(Destination) == -1) {
        PRINT_ERROR("cd: Failed to cd to '%s': %s.\n",
                    Destination,
                    strerror(errno));

        ReturnValue = errno;
        goto BuiltinCdEnd;
    }

    //
    // If in physical mode, ask the system where this all landed.
    //

    if (LogicalMode != FALSE) {
        CurrentDirectory = SwStringDuplicate(Destination, DestinationSize);
        if (CurrentDirectory == NULL) {
            PRINT_ERROR("cd: Allocation failure.\n");
            ReturnValue = 1;
            goto BuiltinCdEnd;
        }

        CurrentDirectorySize = DestinationSize;

    } else {
        Result = ShGetCurrentDirectory(&CurrentDirectory,
                                       &CurrentDirectorySize);

        if (Result == FALSE) {
            PRINT_ERROR("cd: Failed to get current directory after cd to %s.\n",
                        Destination);

            ReturnValue = 1;
            goto BuiltinCdEnd;
        }
    }

    //
    // Update the old PWD variable. Make sure to do this before updating the
    // PWD variable.
    //

    ShSetVariable(Shell,
                  SHELL_OLDPWD,
                  sizeof(SHELL_OLDPWD),
                  NewOldCurrentDirectory,
                  NewOldCurrentDirectorySize);

    //
    // Update the PWD variable.
    //

    Result = ShSetVariable(Shell,
                           SHELL_PWD,
                           sizeof(SHELL_PWD),
                           CurrentDirectory,
                           CurrentDirectorySize);

    if (Result == FALSE) {
        ReturnValue = 1;
        goto BuiltinCdEnd;
    }

    //
    // For whatever reason, when this argument is used print out the new
    // current directory.
    //

    if (UseOldWorkingDirectory != FALSE) {
        printf("%s\n", CurrentDirectory);
    }

    ReturnValue = 0;

BuiltinCdEnd:
    if (CdPathDirectory != NULL) {
        free(CdPathDirectory);
    }

    if (CleanedDirectory != NULL) {
        free(CleanedDirectory);
    }

    if (CurrentDirectory != NULL) {
        free(CurrentDirectory);
    }

    if (FullDirectory != NULL) {
        free(FullDirectory);
    }

    return ReturnValue;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ShExpandPath (
    PSHELL Shell,
    PSTR Prefix,
    ULONG PrefixSize,
    PSTR Field,
    PSTR *FilesStringBuffer,
    PULONG FilesStringBufferSize,
    PSTR **FilesArray,
    PULONG FilesArrayCount
    )

/*++

Routine Description:

    This routine expands a path pattern.

Arguments:

    Shell - Supplies a pointer to the shell.

    Prefix - Supplies an optional pointer to the string containing the
        expanded path so far.

    PrefixSize - Supplies the size of the prefix in bytes including the null
        terminator.

    Field - Supplies a pointer to the string containing the path to expand.

    FilesStringBuffer - Supplies a pointer where the string will be returned
        containing all the matches. The caller is responsible for freeing this
        memory.

    FilesStringBufferSize - Supplies a pointer where the size of the files
        string buffer will be returned on success.

    FilesArray - Supplies a pointer where the array of pointers to the files
        matching the expansion will be returned on success. The caller is
        responsible for freeing this memory.

    FilesArrayCount - Supplies a pointer where the number of files matching
        will be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR CompletePath;
    ULONG CompletePathSize;
    PSTR CompletePrefix;
    PSTR Component;
    UINTN ComponentLength;
    PSTR FieldCopy;
    UINTN FieldCopySize;
    ULONG FileCount;
    PSTR *Files;
    ULONG FilesCapacity;
    BOOL HasMetaCharacters;
    PSTR InitialFieldCopy;
    PSTR ListingBuffer;
    ULONG ListingCount;
    ULONG ListingIndex;
    PSHELL_DIRECTORY_ENTRY Listings;
    BOOL Match;
    BOOL MustBeDirectory;
    PSTR NextComponent;
    PSTR RecursedBuffer;
    ULONG RecursedBufferSize;
    ULONG RecursedFileCount;
    PSTR *RecursedFiles;
    BOOL Result;
    struct stat Stat;
    int Status;
    PSTR String;
    ULONG StringCapacity;
    ULONG StringSize;

    CompletePath = NULL;
    CompletePrefix = NULL;
    FieldCopy = NULL;
    FileCount = 0;
    Files = NULL;
    FilesCapacity = 0;
    InitialFieldCopy = NULL;
    ListingBuffer = NULL;
    Listings = NULL;
    String = NULL;
    StringCapacity = 0;
    StringSize = 0;

    //
    // If there's no prefix and it starts with an absolute path, advance beyond
    // the slashes.
    //

    if (Prefix == NULL) {
        FieldCopySize = strlen(Field) + 1;
        if (ShArePatternCharactersInPath(Field, FieldCopySize) == FALSE) {
            Result = TRUE;
            goto ExpandPathEnd;
        }

        //
        // Create a copy of the string and dequote it for path expansions.
        //

        InitialFieldCopy = strdup(Field);
        if (InitialFieldCopy == NULL) {
            Result = FALSE;
            goto ExpandPathEnd;
        }

        FieldCopySize = strlen(Field);
        ShStringDequote(InitialFieldCopy,
                        FieldCopySize + 1,
                        SHELL_DEQUOTE_FOR_PATTERN_MATCHING,
                        &FieldCopySize);

        Field = InitialFieldCopy;

        //
        // If it's absolute, set the prefix and make it relative.
        //

        if (*Field == '/') {
            while (*Field == '/') {
                Field += 1;
            }

            Prefix = "/";
            PrefixSize = sizeof("/");
        }
    }

    //
    // Determine where to split the remaining path.
    //

    NextComponent = ShPathGetNextComponent(Field, &HasMetaCharacters);

    //
    // There is a component, but if there is no prefix, figure one out.
    //

    if (Prefix == NULL) {
        if (HasMetaCharacters == FALSE) {

            assert(NextComponent != NULL);

            FieldCopySize = NextComponent - Field;

            assert(FieldCopySize != 0);

            FieldCopy = strdup(Field);
            if (FieldCopy == NULL) {
                Result = FALSE;
                goto ExpandPathEnd;
            }

            FieldCopy[FieldCopySize] = '\0';
            FieldCopySize += 1;
            Prefix = FieldCopy;
            PrefixSize = FieldCopySize;
            Field = NextComponent;

            //
            // Remove all the escaping backslashes from the prefix.
            //

            while (*Prefix != '\0') {
                if (*Prefix == '\\') {
                    memmove(Prefix,
                            Prefix + 1,
                            &(FieldCopy[FieldCopySize]) - (Prefix + 1));

                    PrefixSize -= 1;
                }

                Prefix += 1;
            }

            Prefix = FieldCopy;

            //
            // This component is now the prefix, so get the next component.
            //

            while (*Field == '/') {
                Field += 1;
            }

            NextComponent = ShPathGetNextComponent(Field,
                                                   &HasMetaCharacters);

            assert(HasMetaCharacters != FALSE);
        }
    }

    //
    // If there are no metacharacters in this component, glom it on to the
    // prefix and get the next component, which should have metacharacters.
    //

    if (HasMetaCharacters == FALSE) {
        if (NextComponent == NULL) {
            ComponentLength = strlen(Field);

        } else {
            ComponentLength = NextComponent - Field;
        }

        Result = SwAppendPath(Prefix,
                              PrefixSize,
                              Field,
                              ComponentLength + 1,
                              &CompletePrefix,
                              &PrefixSize);

        if (Result == FALSE) {
            goto ExpandPathEnd;
        }

        Prefix = CompletePrefix;

        //
        // If there is no next component, then just see if this file exists.
        //

        if (NextComponent == NULL) {
            Status = SwStat(CompletePrefix, FALSE, &Stat);
            if (Status == 0) {
                Result = ShPathCombineLists(&String,
                                            &StringSize,
                                            &StringCapacity,
                                            &Files,
                                            &FileCount,
                                            &FilesCapacity,
                                            CompletePrefix,
                                            PrefixSize,
                                            &CompletePrefix,
                                            1);

                if (Result == FALSE) {
                    goto ExpandPathEnd;
                }
            }

            Result = TRUE;
            goto ExpandPathEnd;
        }

        Field = NextComponent;
        while (*Field == '/') {
            Field += 1;
        }

        NextComponent = ShPathGetNextComponent(Field, &HasMetaCharacters);

        assert(HasMetaCharacters != FALSE);
    }

    Component = Field;
    while (*Component == '/') {
        Component += 1;
    }

    MustBeDirectory = FALSE;
    if (NextComponent != NULL) {
        ComponentLength = NextComponent - Component;
        MustBeDirectory = TRUE;
        while (*NextComponent == '/') {
            NextComponent += 1;
        }

    } else {
        ComponentLength = strlen(Component);
    }

    if (ComponentLength == 0) {
        Result = TRUE;
        goto ExpandPathEnd;
    }

    //
    // Get the directory contents of the prefix.
    //

    Result = ShGetDirectoryListing(Prefix,
                                   &ListingBuffer,
                                   &Listings,
                                   &ListingCount);

    if (Result == FALSE) {
        Result = TRUE;
        goto ExpandPathEnd;
    }

    for (ListingIndex = 0; ListingIndex < ListingCount; ListingIndex += 1) {

        //
        // If the listing doesn't match, just continue.
        //

        Match = SwDoesPathPatternMatch(Listings[ListingIndex].Name,
                                       Listings[ListingIndex].NameSize,
                                       Component,
                                       ComponentLength + 1);

        if (Match == FALSE) {
            continue;
        }

        //
        // Create the appended path.
        //

        Result = SwAppendPath(Prefix,
                              PrefixSize,
                              Listings[ListingIndex].Name,
                              Listings[ListingIndex].NameSize,
                              &CompletePath,
                              &CompletePathSize);

        if (Result == FALSE) {
            goto ExpandPathEnd;
        }

        if (MustBeDirectory != FALSE) {
            Status = SwStat(CompletePath, TRUE, &Stat);
            if (Status != 0) {
                free(CompletePath);
                CompletePath = NULL;
                continue;
            }

            if (S_ISDIR(Stat.st_mode)) {

                //
                // It's a directory. If there's another path component,
                // recurse to get all the files in that directory matching the
                // pattern, then combine those results with the answer.
                //

                if ((NextComponent != NULL) && (*NextComponent != '\0')) {
                    Result = ShExpandPath(Shell,
                                          CompletePath,
                                          CompletePathSize,
                                          NextComponent,
                                          &RecursedBuffer,
                                          &RecursedBufferSize,
                                          &RecursedFiles,
                                          &RecursedFileCount);

                    if (Result == FALSE) {
                        goto ExpandPathEnd;
                    }

                    Result = ShPathCombineLists(&String,
                                                &StringSize,
                                                &StringCapacity,
                                                &Files,
                                                &FileCount,
                                                &FilesCapacity,
                                                RecursedBuffer,
                                                RecursedBufferSize,
                                                RecursedFiles,
                                                RecursedFileCount);

                    free(RecursedBuffer);
                    free(RecursedFiles);
                    if (Result == FALSE) {
                        goto ExpandPathEnd;
                    }

                //
                // There are no more components, so add this directory to
                // the list of results.
                //

                } else {
                    Result = ShPathCombineLists(&String,
                                                &StringSize,
                                                &StringCapacity,
                                                &Files,
                                                &FileCount,
                                                &FilesCapacity,
                                                CompletePath,
                                                CompletePathSize,
                                                &CompletePath,
                                                1);

                    if (Result == FALSE) {
                        goto ExpandPathEnd;
                    }
                }
            }

        //
        // This doesn't have to be a directory, doesn't much matter what it is.
        //

        } else {

            //
            // It doesn't have to be a directory, which must mean there are no
            // more components. Add this value to the list.
            //

            assert(NextComponent == NULL);

            Result = ShPathCombineLists(&String,
                                        &StringSize,
                                        &StringCapacity,
                                        &Files,
                                        &FileCount,
                                        &FilesCapacity,
                                        CompletePath,
                                        CompletePathSize,
                                        &CompletePath,
                                        1);

            if (Result == FALSE) {
                goto ExpandPathEnd;
            }
        }

        free(CompletePath);
        CompletePath = NULL;
    }

ExpandPathEnd:
    if (InitialFieldCopy != NULL) {
        free(InitialFieldCopy);
    }

    if (FieldCopy != NULL) {
        free(FieldCopy);
    }

    if (CompletePath != NULL) {
        free(CompletePath);
    }

    if (CompletePrefix != NULL) {
        free(CompletePrefix);
    }

    if (Listings != NULL) {
        free(Listings);
    }

    if (ListingBuffer != NULL) {
        free(ListingBuffer);
    }

    if (Result == FALSE) {
        if (Files != NULL) {
            free(Files);
            Files = NULL;
        }

        FileCount = 0;
        if (String != NULL) {
            free(String);
            String = NULL;
        }

        StringSize = 0;
    }

    *FilesStringBuffer = String;
    *FilesStringBufferSize = StringSize;
    *FilesArray = Files;
    *FilesArrayCount = FileCount;
    return Result;
}

BOOL
ShArePatternCharactersInPath (
    PSTR Path,
    ULONG PathSize
    )

/*++

Routine Description:

    This routine determines if the given string contains any special pattern
    characters: * ? or [ that not quoted by a backslash.

Arguments:

    Path - Supplies a pointer to the path string. If a null terminator is
        encountered, the search will stop.

    PathSize - Supplies the size of the path string in bytes.

Return Value:

    TRUE if the string contains any special character (which may or may not be
    quoted).

    FALSE if the string contains no special characters.

--*/

{

    while (PathSize != 0) {
        if (*Path == SHELL_CONTROL_ESCAPE) {

            assert(PathSize >= 2);

            Path += 2;
            PathSize -= 2;
            continue;
        }

        if ((*Path == '?') || (*Path == '*') || (*Path == '[')) {
            return TRUE;
        }

        Path += 1;
        PathSize -= 1;
    }

    return FALSE;
}

BOOL
ShPathCombineLists (
    PSTR *ListBuffer,
    PULONG ListBufferSize,
    PULONG ListBufferCapacity,
    PSTR **List,
    PULONG ListSize,
    PULONG ListCapacity,
    PSTR SecondListBuffer,
    ULONG SecondListBufferSize,
    PSTR *SecondList,
    ULONG SecondListSize
    )

/*++

Routine Description:

    This routine appends a path component to a path.

Arguments:

    ListBuffer - Supplies a pointer that on input contains the pointer to the
        string buffer all the list elements point to. This buffer may be
        updated on output.

    ListBufferSize - Supplies a pointer that on input contains the size of the
        list buffer. This may be updated on output.

    ListBufferCapacity - Supplies a pointer that on input contains the total
        allocation size of the list buffer. This may be updated on output.

    List - Supplies a pointer to the array of element pointers on input. This
        will be updated on output to contain the updated list.

    ListSize - Supplies a pointer that on input contains the number of elements
        in the list. This will be updated on output.

    ListCapacity - Supplies a pointer that on input contains the maximum
        number of elements that could currently go in the list. This value may
        be updated on output if the allocation is expanded.

    SecondListBuffer - Supplies a pointer to the second list's string buffer.

    SecondListBufferSize - Supplies the size of the second list's buffer in
        bytes including the last null terminator.

    SecondList - Supplies the second list to append to the first.

    SecondListSize - Supplies the number of elements in the second list.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG ListIndex;
    PSTR NewBuffer;
    ULONG NewBufferCapacity;
    ULONG NewBufferSize;
    PSTR *NewList;
    ULONG NewListCapacity;
    ULONG NewListSize;
    UINTN OriginalBufferAddress;
    PSTR *ReallocatedList;
    BOOL Result;
    ULONG SizeNeeded;

    OriginalBufferAddress = (UINTN)*ListBuffer;
    NewBufferCapacity = *ListBufferCapacity;
    NewBufferSize = *ListBufferSize;
    NewList = *List;
    NewListSize = *ListSize;
    NewListCapacity = *ListCapacity;
    Result = FALSE;

    //
    // Create a new string containing both strings.
    //

    SizeNeeded = *ListBufferSize + SecondListBufferSize;
    if (NewBufferCapacity == 0) {
        NewBufferCapacity = SHELL_INITIAL_PATH_BUFFER_SIZE;
    }

    while (NewBufferCapacity < SizeNeeded) {
        NewBufferCapacity *= 2;
    }

    if (NewBufferCapacity > *ListBufferCapacity) {
        NewBuffer = realloc(*ListBuffer, NewBufferCapacity);
        if (NewBuffer == NULL) {
            goto PathCombineListsEnd;
        }

    } else {
        NewBuffer = *ListBuffer;
    }

    memcpy(NewBuffer + *ListBufferSize, SecondListBuffer, SecondListBufferSize);
    NewBufferSize = SizeNeeded;

    //
    // Create the new combined array.
    //

    SizeNeeded = NewListSize + SecondListSize;
    if (NewListCapacity == 0) {
        NewListCapacity = SHELL_INITIAL_PATH_LIST_SIZE;
    }

    while (NewListCapacity < SizeNeeded) {
        NewListCapacity *= 2;
    }

    if (NewListCapacity > *ListCapacity) {
        ReallocatedList = realloc(NewList, NewListCapacity * sizeof(PSTR));
        if (ReallocatedList == NULL) {
            goto PathCombineListsEnd;
        }

        NewList = ReallocatedList;
    }

    NewListSize = SizeNeeded;

    //
    // Fix up all the original pointers to point at the new buffer if the
    // buffer was reallocated.
    //

    if (NewBufferCapacity != *ListBufferCapacity) {
        for (ListIndex = 0; ListIndex < *ListSize; ListIndex += 1) {
            NewList[ListIndex] = NewBuffer +
                                 ((UINTN)(NewList[ListIndex]) -
                                  OriginalBufferAddress);
        }
    }

    //
    // Add in all the new pointers.
    //

    for (ListIndex = *ListSize; ListIndex < NewListSize; ListIndex += 1) {
        NewList[ListIndex] = NewBuffer + *ListBufferSize +
                             ((UINTN)SecondList[ListIndex - *ListSize] -
                              (UINTN)SecondListBuffer);
    }

    Result = TRUE;

PathCombineListsEnd:
    if (Result == FALSE) {
        if (NewBuffer != NULL) {
            free(NewBuffer);
            NewBuffer = NULL;
        }

        NewBufferSize = 0;
        NewBufferCapacity = 0;
        if (NewList != NULL) {
            free(NewList);
            NewList = NULL;
        }

        NewListSize = 0;
        NewListCapacity = 0;
    }

    *ListBuffer = NewBuffer;
    *ListBufferCapacity = NewBufferCapacity;
    *ListBufferSize = NewBufferSize;
    *List = NewList;
    *ListSize = NewListSize;
    *ListCapacity = NewListCapacity;
    return Result;
}

BOOL
ShLocateDirectoryOnCdPath (
    PSHELL Shell,
    PSTR Directory,
    UINTN DirectorySize,
    PSTR *FullDirectoryPath,
    PUINTN FullDirectoryPathSize
    )

/*++

Routine Description:

    This routine locates a directory using the CDPATH environment variable.

Arguments:

    Shell - Supplies a pointer to the shell.

    Directory - Supplies a pointer to the directory string.

    DirectorySize - Supplies the size of the directory string in bytes.

    FullDirectoryPath - Supplies a pointer where a pointer to the full directory
        path will be returned on success. If this is not the same pointer as the
        command string then the caller is responsible for freeing this buffer.

    FullDirectoryPathSize - Supplies a pointer where the size of the full
        command string will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR CompletePath;
    ULONG CompletePathSize;
    PSTR CurrentPath;
    UINTN CurrentPathSize;
    CHAR ListSeparator;
    PSTR NextListSeparator;
    PSTR Path;
    UINTN PathSize;
    BOOL Result;
    struct stat Stat;
    INT Status;

    *FullDirectoryPath = NULL;
    *FullDirectoryPathSize = 0;
    ListSeparator = PATH_LIST_SEPARATOR;

    //
    // Get the CDPATH environment variable.
    //

    Result = ShGetVariable(Shell,
                           SHELL_CDPATH,
                           sizeof(SHELL_CDPATH),
                           &Path,
                           &PathSize);

    //
    // If the path variable couldn't be found or is empty, then just return.
    //

    if ((Result == FALSE) || (Path == NULL) || (PathSize <= 1)) {
        Result = FALSE;
        goto LocateDirectoryOnCdPathEnd;
    }

    //
    // Loop through each entry in the path.
    //

    CurrentPath = Path;
    NextListSeparator = strchr(CurrentPath, ListSeparator);
    while (TRUE) {
        if (NextListSeparator == NULL) {
            CurrentPathSize = PathSize - ((UINTN)CurrentPath - (UINTN)Path);

        } else {
            CurrentPathSize = (UINTN)NextListSeparator - (UINTN)CurrentPath;
        }

        if (CurrentPathSize == 0) {
            CurrentPath = ".";
            CurrentPathSize = sizeof(".");
        }

        //
        // Make a complete command path out of this path entry and the command.
        //

        Result = SwAppendPath(CurrentPath,
                              CurrentPathSize,
                              Directory,
                              DirectorySize,
                              &CompletePath,
                              &CompletePathSize);

        if (Result == FALSE) {
            goto LocateDirectoryOnCdPathEnd;
        }

        //
        // Figure out if this is something legit.
        //

        Status = SwStat(CompletePath, TRUE, &Stat);
        if ((Status == 0) && (S_ISDIR(Stat.st_mode))) {
            *FullDirectoryPath = CompletePath;
            *FullDirectoryPathSize = CompletePathSize;
            Result = TRUE;
            goto LocateDirectoryOnCdPathEnd;
        }

        free(CompletePath);

        //
        // If this was the last entry, stop.
        //

        if (NextListSeparator == NULL) {
            break;
        }

        //
        // Move to the next path.
        //

        CurrentPath = NextListSeparator + 1;
        NextListSeparator = strchr(CurrentPath, ListSeparator);
    }

    //
    // Nothing was found.
    //

    Result = FALSE;

LocateDirectoryOnCdPathEnd:
    return Result;
}

INT
ShCleanLogicalDirectoryPath (
    PSTR PathString,
    UINTN PathStringSize,
    PSTR *CleanedPathString,
    PUINTN CleanedPathStringSize
    )

/*++

Routine Description:

    This routine cleans up a logical directory path. It removes unnecessary
    slashes, removes dot components, and performs logical splicing of dot-dot
    components, validating that each path referenced along the way points to
    a directory.

Arguments:

    PathString - Supplies a pointer to the path string.

    PathStringSize - Supplies the size of the path string in bytes including
        the null terminator.

    CleanedPathString - Supplies a pointer where a new string will be returned
        containing the cleaned path. The caller is responsible for freeing this
        memory.

    CleanedPathStringSize - Supplies a pointer where the size in bytes of the
        returned cleaned string in bytes including the null terminator will
        be returned.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    UINTN ComponentSize;
    PSTR CurrentOutput;
    PSTR NextSeparator;
    PSTR OriginalPathString;
    PSTR Output;
    UINTN OutputCapacity;
    UINTN OutputSize;
    UINTN RemainingSize;
    INT Result;
    struct stat Stat;

    OriginalPathString = PathString;

    //
    // The cleaned path only gets smaller, except that it may add a trailing
    // space.
    //

    OutputCapacity = PathStringSize + 2;
    Output = malloc(OutputCapacity);
    if (Output == NULL) {
        Result = ENOMEM;
        goto CleanLogicalDirectoryPathEnd;
    }

    CurrentOutput = Output;
    OutputSize = 0;
    if (*PathString == '/') {
        *CurrentOutput = '/';
        CurrentOutput += 1;
        OutputSize += 1;
    }

    while (PathStringSize != 0) {

        //
        // Get past any separators. Add a single separator to the output if
        // there isn't one already.
        //

        if (*PathString == '/') {
            if ((OutputSize != 0) && (*(CurrentOutput - 1) != '/')) {
                *CurrentOutput = '/';
                CurrentOutput += 1;
                OutputSize += 1;
            }

            while ((PathStringSize != 0) && (*PathString == '/')) {
                PathString += 1;
                PathStringSize -= 1;
            }
        }

        //
        // Find the next separator.
        //

        RemainingSize = PathStringSize;
        NextSeparator = PathString;
        while ((RemainingSize != 0) && (*NextSeparator != '/') &&
               (*NextSeparator != '\0')) {

            NextSeparator += 1;
            RemainingSize -= 1;
        }

        ComponentSize = PathStringSize - RemainingSize;

        //
        // Skip any dot components.
        //

        if ((ComponentSize == 1) && (*PathString == '.')) {
            PathStringSize = RemainingSize;
            PathString = NextSeparator;
            continue;
        }

        //
        // If it's a dot-dot component, then test the path so far. If it's
        // not a valid directory (following symlinks), then complain and exit.
        //

        if ((ComponentSize == 2) && (PathString[0] == '.') &&
            (PathString[1] == '.')) {

            //
            // Terminate the string and check the output so far.
            //

            *CurrentOutput = '\0';
            Result = SwStat(Output, TRUE, &Stat);
            if ((Result == 0) && (!S_ISDIR(Stat.st_mode))) {
                Result = ENOTDIR;
            }

            if (Result != 0) {
                PRINT_ERROR("cd: %s: %s\n",
                            OriginalPathString,
                            strerror(Result));

                goto CleanLogicalDirectoryPathEnd;
            }

            //
            // Attempt to remove the most recently added component. Start by
            // backing up over the separator, unless it's the very first root
            // separator.
            //

            if ((OutputSize > 1) && (*(CurrentOutput - 1) == '/')) {
                OutputSize -= 1;
                CurrentOutput -= 1;

                //
                // Now back up until a separator or the beginning of the
                // string is found.
                //

                while ((OutputSize > 0) && (*(CurrentOutput - 1) != '/')) {
                    CurrentOutput -= 1;
                    OutputSize -= 1;
                }

                //
                // Also remove the separator before that, unless it's the first
                // one.
                //

                if ((OutputSize > 1) && (*(CurrentOutput - 1) == '/')) {
                    OutputSize -= 1;
                    CurrentOutput -= 1;
                }
            }

            //
            // Move along.
            //

            PathStringSize = RemainingSize;
            PathString = NextSeparator;
            continue;

        } else if (*PathString == '\0') {
            break;
        }

        //
        // It's a regular path component, jam it on there.
        //

        memcpy(CurrentOutput, PathString, ComponentSize);
        CurrentOutput += ComponentSize;
        OutputSize += ComponentSize;
        PathStringSize = RemainingSize;
        PathString = NextSeparator;
    }

    //
    // Trim a trailing slash.
    //

    if ((OutputSize > 1) && (*(CurrentOutput - 1) == '/')) {
        OutputSize -= 1;
        CurrentOutput -= 1;
    }

    //
    // Terminate the path.
    //

    *CurrentOutput = '\0';
    OutputSize += 1;
    Result = 0;

CleanLogicalDirectoryPathEnd:
    if (Result != 0) {
        if (Output != NULL) {
            free(Output);
            Output = NULL;
        }

        OutputSize = 0;
    }

    *CleanedPathString = Output;
    *CleanedPathStringSize = OutputSize;
    return Result;
}

PSTR
ShPathGetNextComponent (
    PSTR Field,
    PBOOL HasMetaCharacters
    )

/*++

Routine Description:

    This routine determines the next path component of the given path, honoring
    backslashes.

Arguments:

    Field - Supplies a pointer to the field to get the next path component of.

    HasMetaCharacters - Supplies a pointer where a boolean will be returned
        indicating if this component has metacharacters or not.

Return Value:

    Returns a pointer to the path separator right before a component with a
    metacharacter if there are no metacharacters in the first component.

    Returns a pointer to the next path separator if this component has a meta-
    character.

    NULL if no metacharacters were found in this field region.

--*/

{

    BOOL FoundMeta;
    BOOL FoundSeparator;
    PSTR LastSeparator;
    BOOL WasBackslash;

    *HasMetaCharacters = FALSE;
    FoundMeta = FALSE;
    LastSeparator = NULL;
    WasBackslash = FALSE;
    while (*Field != '\0') {
        FoundSeparator = FALSE;
        if (WasBackslash == FALSE) {
            if ((*Field == '*') || (*Field == '?') || (*Field == '[')) {
                FoundMeta = TRUE;
            }
        }

        if (*Field == '/') {
            FoundSeparator = TRUE;
        }

        if (*Field == '\\') {
            WasBackslash = !WasBackslash;

        } else {
            WasBackslash = FALSE;
        }

        //
        // Handle a path separator if one was found.
        //

        if (FoundSeparator != FALSE) {

            //
            // If there was a metacharacter and there was a previous component,
            // return the previous component.
            //

            if (FoundMeta != FALSE) {
                if (LastSeparator != NULL) {
                    return LastSeparator;
                }

                //
                // This is the first component, so return the next
                // metacharacter.
                //

                *HasMetaCharacters = TRUE;
                return Field;
            }

            LastSeparator = Field;
        }

        Field += 1;
    }

    if (FoundMeta != FALSE) {
        if (LastSeparator != NULL) {
            return LastSeparator;
        }

        *HasMetaCharacters = TRUE;
    }

    return NULL;
}

int
ShPathCompareStrings (
    const void *LeftPointer,
    const void *RightPointer
    )

/*++

Routine Description:

    This routine compares two strings, using a function prototype compatible
    with the qsort function.

Arguments:

    LeftPointer - Supplies a pointer to a pointer to the left side of the
        string comparison.

    RightPointer - Supplies a pointer to a pointer to the right side of the
        string comparison.

Return Value:

    < 0 if the left is less than the right.

    0 if the strings are equal.

    > 0 if the left side is greater than the right.

--*/

{

    char **LeftStringPointer;
    int Result;
    char **RightStringPointer;

    LeftStringPointer = (char **)LeftPointer;
    RightStringPointer = (char **)RightPointer;
    Result = strcmp(*LeftStringPointer, *RightStringPointer);
    return Result;
}

