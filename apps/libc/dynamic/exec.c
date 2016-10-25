/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    exec.c

Abstract:

    This module implements the exec() family of functions.

Author:

    Evan Green 3-Apr-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
execl (
    const char *Path,
    const char *Argument0,
    ...
    )

/*++

Routine Description:

    This routine replaces the current process image with a new image.

Arguments:

    Path - Supplies a pointer to a string containing the fully specified path
        of the file to execute.

    Argument0 - Supplies the first argument to execute, usually the same as
        the command name.

    ... - Supplies the arguments to the program. The argument list must be
        terminated with a NULL.

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    char *Argument;
    char **ArgumentArray;
    ULONG ArgumentCount;
    ULONG ArgumentIndex;
    va_list ArgumentList;
    int Result;

    //
    // Determine the number of arguments.
    //

    ArgumentCount = 1;
    va_start(ArgumentList, Argument0);
    Argument = va_arg(ArgumentList, char *);
    while (Argument != NULL) {
        ArgumentCount += 1;
        Argument = va_arg(ArgumentList, char *);
    }

    va_end(ArgumentList);

    //
    // Create an array for them.
    //

    ArgumentArray = malloc((ArgumentCount + 1) * sizeof(char *));
    if (ArgumentArray == NULL) {
        errno = ENOMEM;
        return -1;
    }

    //
    // Copy the arguments into the array.
    //

    ArgumentIndex = 1;
    ArgumentArray[0] = (char *)Argument0;
    va_start(ArgumentList, Argument0);
    Argument = va_arg(ArgumentList, char *);
    while (Argument != NULL) {
        ArgumentArray[ArgumentIndex] = Argument;
        ArgumentIndex += 1;
        Argument = va_arg(ArgumentList, char *);
    }

    ArgumentArray[ArgumentIndex] = NULL;

    //
    // The environment is one more along.
    //

    Result = execve(Path,
                    (char *const *)ArgumentArray,
                    (char *const *)environ);

    free(ArgumentArray);
    return Result;
}

LIBC_API
int
execv (
    const char *Path,
    char *const Arguments[]
    )

/*++

Routine Description:

    This routine replaces the current process image with a new image.

Arguments:

    Path - Supplies a pointer to a string containing the fully specified path
        of the file to execute.

    Arguments - Supplies an array of pointers to strings containing the
        arguments to pass to the program.

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    return execve(Path, Arguments, (char *const *)environ);
}

LIBC_API
int
execle (
    const char *Path,
    const char *Argument0,
    ...
    )

/*++

Routine Description:

    This routine replaces the current process image with a new image. The
    parameters to this function also include the environment variables to use.

Arguments:

    Path - Supplies a pointer to a string containing the fully specified path
        of the file to execute.

    Argument0 - Supplies the first argument to the program, usually the same
        as the program name.

    ... - Supplies the arguments to the program. The argument list must be
        terminated with a NULL. After the NULL an array of strings representing
        the environment is expected (think of it like a final argument after
        the NULL in the form const char *envp[]).

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    char *Argument;
    char **ArgumentArray;
    ULONG ArgumentCount;
    ULONG ArgumentIndex;
    va_list ArgumentList;
    char *const *Environment;
    int Result;

    //
    // Determine the number of arguments.
    //

    ArgumentCount = 1;
    va_start(ArgumentList, Argument0);
    Argument = va_arg(ArgumentList, char *);
    while (Argument != NULL) {
        ArgumentCount += 1;
        Argument = va_arg(ArgumentList, char *);
    }

    va_end(ArgumentList);

    //
    // Create an array for them.
    //

    ArgumentArray = malloc((ArgumentCount + 1) * sizeof(char *));
    if (ArgumentArray == NULL) {
        errno = ENOMEM;
        return -1;
    }

    //
    // Copy the arguments into the array.
    //

    ArgumentIndex = 1;
    ArgumentArray[0] = (char *)Argument0;
    va_start(ArgumentList, Argument0);
    Argument = va_arg(ArgumentList, char *);
    while (Argument != NULL) {
        ArgumentArray[ArgumentIndex] = Argument;
        ArgumentIndex += 1;
        Argument = va_arg(ArgumentList, char *);
    }

    ArgumentArray[ArgumentIndex] = NULL;

    //
    // The environment is one more along.
    //

    Environment = va_arg(ArgumentList, char *const *);
    Result = execve(Path, (char *const *)ArgumentArray, Environment);
    free(ArgumentArray);
    return Result;
}

LIBC_API
int
execve (
    const char *Path,
    char *const Arguments[],
    char *const Environment[]
    )

/*++

Routine Description:

    This routine replaces the current process image with a new image. The
    parameters to this function also include the environment variables to use.

Arguments:

    Path - Supplies a pointer to a string containing the fully specified path
        of the file to execute.

    Arguments - Supplies an array of pointers to strings containing the
        arguments to pass to the program.

    Environment - Supplies an array of pointers to strings containing the
        environment variables to pass to the program.

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    UINTN ArgumentCount;
    UINTN ArgumentIndex;
    UINTN ArgumentValuesTotalLength;
    PSTR End;
    UINTN EnvironmentCount;
    UINTN EnvironmentValuesTotalLength;
    UINTN ExtraArguments;
    PSTR Interpreter;
    PSTR InterpreterArgument;
    UINTN InterpreterArgumentLength;
    UINTN InterpreterLength;
    PSTR Line;
    UINTN PathLength;
    PPROCESS_ENVIRONMENT ProcessEnvironment;
    FILE *Script;
    char **ShellArguments;
    KSTATUS Status;

    fflush(NULL);
    ArgumentCount = 0;
    ArgumentValuesTotalLength = 0;
    EnvironmentCount = 0;
    EnvironmentValuesTotalLength = 0;
    Script = NULL;
    ShellArguments = NULL;
    Line = NULL;
    PathLength = strlen(Path) + 1;
    while (Arguments[ArgumentCount] != NULL) {
        ArgumentValuesTotalLength += strlen(Arguments[ArgumentCount]) + 1;
        ArgumentCount += 1;
    }

    if (Environment != NULL) {
        while (Environment[EnvironmentCount] != NULL) {
            EnvironmentValuesTotalLength +=
                                     strlen(Environment[EnvironmentCount]) + 1;

            EnvironmentCount += 1;
        }
    }

    ProcessEnvironment = OsCreateEnvironment((PSTR)Path,
                                             PathLength,
                                             (PSTR *)Arguments,
                                             ArgumentValuesTotalLength,
                                             ArgumentCount,
                                             (PSTR *)Environment,
                                             EnvironmentValuesTotalLength,
                                             EnvironmentCount);

    if (ProcessEnvironment == NULL) {
        errno = ENOMEM;
        goto execveEnd;
    }

    Status = OsExecuteImage(ProcessEnvironment);
    if ((!KSUCCESS(Status)) && (Status != STATUS_UNKNOWN_IMAGE_FORMAT)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        goto execveEnd;
    }

    //
    // Take a peek at the file. If it begins with #!, then interpret it as a
    // shell script.
    //

    Script = fopen(Path, "r");
    if (Script == NULL) {
        goto execveEnd;
    }

    Line = malloc(PATH_MAX + 6);
    if (Line == NULL) {
        errno = ENOMEM;
        goto execveEnd;
    }

    if (fgets(Line, PATH_MAX + 6, Script) == NULL) {
        goto execveEnd;
    }

    if ((Line[0] != '#') || (Line[1] != '!')) {
        errno = ENOEXEC;
        goto execveEnd;
    }

    Interpreter = Line + 2;
    while (isblank(*Interpreter) != 0) {
        Interpreter += 1;
    }

    if (*Interpreter == '\0') {
        errno = ENOEXEC;
        goto execveEnd;
    }

    //
    // Find the first space, newline, or ending and terminate the interpreter
    // path.
    //

    End = Interpreter;
    while ((*End != '\0') && (!isspace(*End))) {
        End += 1;
    }

    ExtraArguments = 1;
    InterpreterArgument = NULL;
    InterpreterArgumentLength = 0;

    //
    // If there's more to the line, then there's an argument. Treat the
    // remainder of the line as one big argument.
    //

    if ((*End != '\0') && (*End != '\n') && (*End != '\r')) {
        ExtraArguments += 1;
        *End = '\0';
        End += 1;
        while (isblank(*End)) {
            End += 1;
        }

        if ((*End != '\0') && (*End != '\n') && (*End != '\r')) {
            InterpreterArgument = End;
            while ((*End != '\r') && (*End != '\n') && (*End != '\0')) {
                End += 1;
            }

            InterpreterArgumentLength =
                                   (UINTN)End - (UINTN)InterpreterArgument + 1;

            *End = '\0';
        }

    //
    // No argument, just terminate the interpreter.
    //

    } else {
        *End = '\0';
    }

    //
    // Perform some basic (but not nearly foolproof) infinite loop detection
    // by looking to see if the interpreter is the same as the file itself.
    //

    if (strcmp(Interpreter, Path) == 0) {
        fprintf(stderr, "Error: Infinite exec loop for '%s'.\n", Path);
        errno = ENOEXEC;
        goto execveEnd;
    }

    if (access(Interpreter, X_OK) != 0) {
        goto execveEnd;
    }

    //
    // Create the arguments for a shell interpreter.
    //

    OsDestroyEnvironment(ProcessEnvironment);
    ProcessEnvironment = NULL;
    ShellArguments = malloc(
                        sizeof(char *) * (ArgumentCount + ExtraArguments + 1));

    if (ShellArguments == NULL) {
        errno = ENOMEM;
        return -1;
    }

    InterpreterLength = strlen(Interpreter) + 1;
    ShellArguments[0] = Interpreter;
    ArgumentValuesTotalLength += InterpreterLength;
    if (ArgumentCount == 0) {
        ArgumentCount = 1;

    } else {
        ArgumentValuesTotalLength -= strlen(Arguments[0]) + 1;
    }

    if (InterpreterArgument != NULL) {
        ShellArguments[1] = InterpreterArgument;
        ArgumentValuesTotalLength += InterpreterArgumentLength;
    }

    ShellArguments[ExtraArguments] = (char *)Path;
    ArgumentValuesTotalLength += PathLength;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        ShellArguments[ArgumentIndex + ExtraArguments] =
                                                      Arguments[ArgumentIndex];
    }

    ShellArguments[ArgumentIndex + ExtraArguments] = NULL;

    //
    // Add the extras for the interpreter and possibly its parameter.
    //

    ArgumentCount += ExtraArguments;

    //
    // Create the environment and try to execute this puppy. The interpreter
    // line cannot itself point to a script, that would be downright silly.
    //

    ProcessEnvironment = OsCreateEnvironment(Interpreter,
                                             InterpreterLength,
                                             (PSTR *)ShellArguments,
                                             ArgumentValuesTotalLength,
                                             ArgumentCount,
                                             (PSTR *)Environment,
                                             EnvironmentValuesTotalLength,
                                             EnvironmentCount);

    if (ProcessEnvironment == NULL) {
        errno = ENOMEM;
        goto execveEnd;
    }

    Status = OsExecuteImage(ProcessEnvironment);
    errno = ClConvertKstatusToErrorNumber(Status);

execveEnd:
    if (ProcessEnvironment != NULL) {
        OsDestroyEnvironment(ProcessEnvironment);
    }

    if (Script != NULL) {
        fclose(Script);
    }

    if (Line != NULL) {
        free(Line);
    }

    if (ShellArguments != NULL) {
        free(ShellArguments);
    }

    return -1;
}

LIBC_API
int
execlp (
    const char *File,
    const char *Argument0,
    ...
    )

/*++

Routine Description:

    This routine replaces the current process image with a new image. If the
    given file is found but of an unrecognized binary format, then a shell
    interpreter will be launched and passed the file.

Arguments:

    File - Supplies a pointer to a string containing the name of the executable,
        which will be searched for on the PATH if the string does not contain a
        slash.

    Argument0 - Supplies the first argument to the program. Additional arguments
        follow in the ellipses. The argument list must be terminated with a
        NULL.

    ... - Supplies any remaining arguments.

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    char *Argument;
    char **ArgumentArray;
    ULONG ArgumentCount;
    ULONG ArgumentIndex;
    va_list ArgumentList;
    int Result;

    //
    // Determine the number of arguments.
    //

    ArgumentCount = 1;
    va_start(ArgumentList, Argument0);
    Argument = va_arg(ArgumentList, char *);
    while (Argument != NULL) {
        ArgumentCount += 1;
        Argument = va_arg(ArgumentList, char *);
    }

    va_end(ArgumentList);

    //
    // Create an array for them.
    //

    ArgumentArray = malloc((ArgumentCount + 1) * sizeof(char *));
    if (ArgumentArray == NULL) {
        errno = ENOMEM;
        return -1;
    }

    //
    // Copy the arguments into the array.
    //

    ArgumentIndex = 1;
    ArgumentArray[0] = (char *)Argument0;
    va_start(ArgumentList, Argument0);
    Argument = va_arg(ArgumentList, char *);
    while (Argument != NULL) {
        ArgumentArray[ArgumentIndex] = Argument;
        ArgumentIndex += 1;
        Argument = va_arg(ArgumentList, char *);
    }

    ArgumentArray[ArgumentIndex] = NULL;
    Result = execvpe(File, (char *const *)ArgumentArray, environ);
    free(ArgumentArray);
    return Result;
}

LIBC_API
int
execvp (
    const char *File,
    char *const Arguments[]
    )

/*++

Routine Description:

    This routine replaces the current process image with a new image. If the
    given file is found but of an unrecognized binary format, then a shell
    interpreter will be launched and passed the file.

Arguments:

    File - Supplies a pointer to a string containing the name of the executable,
        which will be searched for on the PATH if the string does not contain a
        slash.

    Arguments - Supplies an array of pointers to strings containing the
        arguments to pass to the program.

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    return execvpe(File, Arguments, (char *const *)environ);
}

LIBC_API
int
execvpe (
    const char *File,
    char *const Arguments[],
    char *const Environment[]
    )

/*++

Routine Description:

    This routine replaces the current process image with a new image. The
    parameters to this function also include the environment variables to use.
    If the given file is found but of an unrecognized binary format, then a
    shell interpreter will be launched and passed the file.

Arguments:

    File - Supplies a pointer to a string containing the name of the executable,
        which will be searched for on the PATH if the string does not contain a
        slash.

    Arguments - Supplies an array of pointers to strings containing the
        arguments to pass to the program.

    Environment - Supplies an array of pointers to strings containing the
        environment variables to pass to the program.

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    PSTR CombinedPath;
    size_t FileLength;
    int OriginalError;
    PSTR PathCopy;
    PSTR PathEntry;
    size_t PathEntryLength;
    PSTR PathVariable;
    char *Token;

    PathVariable = getenv("PATH");

    //
    // If the path contains a slash, use it directly without searching the
    // PATH variable.
    //

    if ((strchr(File, '/') != NULL) || (PathVariable == NULL) ||
        (*PathVariable == '\0')) {

        return execve(File, Arguments, Environment);
    }

    //
    // The path doesn't have a slash and there's a path variable, so try
    // searching the path.
    //

    PathCopy = strdup(PathVariable);
    if (PathCopy == NULL) {
        errno = ENOMEM;
        return -1;
    }

    FileLength = strlen(File);
    PathEntry = strtok_r(PathCopy, ":", &Token);
    while (PathEntry != NULL) {
        PathEntryLength = strlen(PathEntry);
        if (PathEntryLength == 0) {
            PathEntry = ".";
            PathEntryLength = 1;
        }

        if (PathEntry[PathEntryLength - 1] == '/') {
            PathEntryLength -= 1;
        }

        CombinedPath = malloc(PathEntryLength + FileLength + 2);
        if (CombinedPath == NULL) {
            errno = ENOMEM;
            break;
        }

        memcpy(CombinedPath, PathEntry, PathEntryLength);
        CombinedPath[PathEntryLength] = '/';
        strcpy(CombinedPath + PathEntryLength + 1, File);

        //
        // Recurse, though this won't recurse further because now there's a
        // slash in the path.
        //

        OriginalError = errno;
        if (access(CombinedPath, X_OK) == 0) {
            execvpe(CombinedPath, Arguments, Environment);

        } else {
            errno = OriginalError;
        }

        free(CombinedPath);

        //
        // Move to the next path entry.
        //

        PathEntry = strtok_r(NULL, ":", &Token);
    }

    if (errno == 0) {
        errno = ENOENT;
    }

    free(PathCopy);
    return -1;
}

//
// --------------------------------------------------------- Internal Functions
//

