/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    chalk.c

Abstract:

    This module implements the Chalk interactive interpreter.

Author:

    Evan Green 26-May-2016

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/chalk.h>
#include <minoca/lib/chalk/app.h>
#include <minoca/lib/chalk/bundle.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define CHALK_USAGE                                                            \
    "usage: chalk [options] [file] [arguments...]\n"                           \
    "Chalk is a nifty scripting language. It's designed to be intuitive, \n"   \
    "small, and easily embeddable. Options are:\n"                             \
    "  -c \"expr\" -- Execute the given expression and exit.\n"                \
    "  --debug-gc -- Stress the garbage collector.\n"                          \
    "  --debug-compiler -- Print the compiled bytecode.\n"                     \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define CHALK_OPTIONS_STRING "+c:chV"

#define CHALK_LINE_MAX 2048

#define CHALK_OPTION_DEBUG_GC 257
#define CHALK_OPTION_DEBUG_COMPILER 258

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the context for the Chalk interactive interpreter.

Members:

    Configuration - Stores the VM configuration.

    Vm - Stores a pointer to the virtual machine.

    LineNumber - Stores the next line number to be read.

    Line - Stores the line input buffer.

--*/

typedef struct _CK_APP_CONTEXT {
    CK_CONFIGURATION Configuration;
    PCK_VM Vm;
    INT LineNumber;
    PSTR Line;
} CK_APP_CONTEXT, *PCK_APP_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ChalkSetupModulePath (
    PVOID Vm,
    PSTR Script
    );

INT
ChalkInitializeContext (
    PCK_APP_CONTEXT Context
    );

VOID
ChalkDestroyContext (
    PCK_APP_CONTEXT Context
    );

PSTR
ChalkLoadFile (
    PSTR FileName,
    PUINTN FileSize
    );

INT
ChalkRunInteractiveInterpreter (
    PCK_APP_CONTEXT Context
    );

INT
ChalkReadLine (
    PCK_APP_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

struct option ChalkLongOptions[] = {
    {"debug-gc", no_argument, 0, CHALK_OPTION_DEBUG_GC},
    {"debug-compiler", no_argument, 0, CHALK_OPTION_DEBUG_COMPILER},
    {"help", no_argument, 0, 'h'},
    {"verbose", no_argument, 0, 'v'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the chalk interactive interpreter.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    BOOL AppIsBundle;
    PSTR ArgumentCopy;
    ULONG ArgumentIndex;
    PSTR BaseName;
    CK_APP_CONTEXT Context;
    PCSTR Expression;
    PSTR FileBuffer;
    UINTN FileSize;
    INT Option;
    PSTR ScriptPath;
    int Status;

    ArgumentIndex = 1;
    AppIsBundle = FALSE;
    Expression = NULL;
    FileBuffer = NULL;
    ScriptPath = NULL;
    Status = ChalkInitializeContext(&Context);
    if (Status != 0) {
        fprintf(stderr, "Failed to initialize: %s.\n", strerror(Status));
        Status = 2;
        goto MainEnd;
    }

    //
    // Figure out early if this executable is a bundle. If it is, don't do
    // regular argument parsing.
    //

    if (ArgumentCount == 0) {
        fprintf(stderr, "Arg0 required.\n");
        Status = 2;
        goto MainEnd;
    }

    ArgumentCopy = strdup(Arguments[0]);
    if (ArgumentCopy != NULL) {
        BaseName = basename(Arguments[0]);
        if (BaseName != NULL) {
            if (strncasecmp(BaseName, "chalk", 5) != 0) {
                AppIsBundle = TRUE;
            }
        }

        free(ArgumentCopy);
    }

    CkAppArgv = Arguments;
    CkAppArgc = ArgumentCount;

    //
    // Process the control arguments if this is the Chalk app acting as the
    // Chalk app.
    //

    if (AppIsBundle == FALSE) {
        while (TRUE) {
            optarg = NULL;
            Option = getopt_long(ArgumentCount,
                                 Arguments,
                                 CHALK_OPTIONS_STRING,
                                 ChalkLongOptions,
                                 NULL);

            if (Option == -1) {
                break;
            }

            if ((Option == '?') || (Option == ':')) {
                Status = 2;
                goto MainEnd;
            }

            switch (Option) {
            case 'c':
                Expression = optarg;
                break;

            case CHALK_OPTION_DEBUG_GC:
                Context.Configuration.Flags |= CK_CONFIGURATION_GC_STRESS;
                break;

            case CHALK_OPTION_DEBUG_COMPILER:
                Context.Configuration.Flags |= CK_CONFIGURATION_DEBUG_COMPILER;
                break;

            case 'V':
                printf("Chalk version %d.%d.%d. Copyright 2017 Minoca Corp. "
                       "All Rights Reserved.\n",
                       CHALK_VERSION_MAJOR,
                       CHALK_VERSION_MINOR,
                       CHALK_VERSION_REVISION);

                return 1;

            case 'h':
                printf(CHALK_USAGE);
                return 2;

            default:

                assert(FALSE);

                Status = 2;
                goto MainEnd;
            }
        }

        ArgumentIndex = optind;
        if (ArgumentIndex < ArgumentCount) {
            ScriptPath = Arguments[ArgumentIndex];
            CkAppArgc = ArgumentCount - ArgumentIndex;
            CkAppArgv = Arguments + ArgumentIndex;
        }
    }

    Context.Vm = CkCreateVm(&(Context.Configuration));
    if (Context.Vm == NULL) {
        fprintf(stderr, "Error: Failed to create VM.\n");
        Status = 2;
        goto MainEnd;
    }

    if ((!CkPreloadAppModule(Context.Vm, Arguments[0])) ||
        (!CkPreloadBundleModule(Context.Vm))) {

        fprintf(stderr, "Error: Failed to preload builtin modules.\n");
        Status = 2;
        goto MainEnd;
    }

    //
    // Set up the module search path. Two stack slots are needed: one for the
    // module search list, and one for a new string being appended.
    //

    if (!CkEnsureStack(Context.Vm, 2)) {
        fprintf(stderr, "Warning: Failed to initialize module search path.\n");
        Status = 2;
        goto MainEnd;
    }

    CkPushModulePath(Context.Vm);
    ChalkSetupModulePath(Context.Vm, ScriptPath);
    CkStackPop(Context.Vm);

    //
    // If the app doesn't start with the name chalk, try to load a bundle from
    // the app.
    //

    if (AppIsBundle != FALSE) {
        Status = CkBundleThaw(Context.Vm);
        goto MainEnd;
    }

    //
    // Run the expression if there was one.
    //

    if (Expression != NULL) {
        Status = CkInterpret(Context.Vm,
                             NULL,
                             Expression,
                             strlen(Expression),
                             1,
                             FALSE);

        if (Status != CkSuccess) {
            Status = 1;
        }

    //
    // Run the script if there was one.
    //

    } else if (ScriptPath != NULL) {
        FileBuffer = ChalkLoadFile(ScriptPath, &FileSize);
        if (FileBuffer == NULL) {
            fprintf(stderr,
                    "Error: Failed to load file %s: %s\n",
                    Arguments[ArgumentIndex],
                    strerror(errno));

            Status = 2;
            goto MainEnd;
        }

        CkAppArgc = ArgumentCount - ArgumentIndex;
        CkAppArgv = Arguments + ArgumentIndex;
        Status = CkInterpret(Context.Vm,
                             ScriptPath,
                             FileBuffer,
                             FileSize,
                             1,
                             FALSE);

    //
    // With no arguments, run the interactive interpreter.
    //

    } else {
        Status = ChalkRunInteractiveInterpreter(&Context);
    }

MainEnd:
    ChalkDestroyContext(&Context);
    if (FileBuffer != NULL) {
        free(FileBuffer);
    }

    return Status;
}

VOID
ChalkAddSearchPath (
    PVOID Vm,
    PSTR Directory,
    PSTR ChalkDirectory
    )

/*++

Routine Description:

    This routine adds a library search path. It assumes the module list is
    already pushed at the top of the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Directory - Supplies the base directory path to add.

    ChalkDirectory - Supplies the directory to tack on to the base. If this
        is supplied, the major version number will be appended to it.

Return Value:

    None.

--*/

{

    CHAR NewPath[PATH_MAX];
    INT NewPathSize;

    if (ChalkDirectory != NULL) {
        NewPathSize = snprintf(NewPath,
                               PATH_MAX,
                               "%s/%s%d",
                               Directory,
                               ChalkDirectory,
                               CHALK_VERSION_MAJOR);

    } else {
        NewPathSize = snprintf(NewPath, PATH_MAX, "%s", Directory);
    }

    if (NewPathSize <= 0) {
        return;
    }

    CkPushString(Vm, NewPath, NewPathSize);
    CkListSet(Vm, -2, CkListSize(Vm, -2));
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ChalkInitializeContext (
    PCK_APP_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes the Chalk application context.

Arguments:

    Context - Supplies a pointer to the context to load.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    INT Status;

    memset(Context, 0, sizeof(CK_APP_CONTEXT));
    Context->LineNumber = 1;
    Context->Line = malloc(CHALK_LINE_MAX);
    if (Context->Line == NULL) {
        Status = ENOMEM;
        goto InitializeEnd;
    }

    CkInitializeConfiguration(&(Context->Configuration));
    Status = 0;

InitializeEnd:
    return Status;
}

VOID
ChalkDestroyContext (
    PCK_APP_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys the Chalk application context.

Arguments:

    Context - Supplies a pointer to the context to tear down.

Return Value:

    None.

--*/

{

    if (Context->Line != NULL) {
        free(Context->Line);
    }

    if (Context->Vm != NULL) {
        CkDestroyVm(Context->Vm);
    }

    return;
}

PSTR
ChalkLoadFile (
    PSTR FileName,
    PUINTN FileSize
    )

/*++

Routine Description:

    This routine loads a file, returning its contents.

Arguments:

    FileName - Supplies a pointer to the file path to load.

    FileSize - Supplies a pointer where the size of the file, not including
        the null terminator, will be returned on success.

Return Value:

    Returns a pointer to the contents of the file, null terminated.

    NULL on failure.

--*/

{

    PSTR Buffer;
    ssize_t BytesRead;
    FILE *File;
    struct stat Stat;
    INT Status;
    size_t TotalRead;

    Buffer = NULL;
    Status = stat(FileName, &Stat);
    if (Status != 0) {
        fprintf(stderr,
                "chalk: Unable to load file %s: %s\n",
                FileName,
                strerror(errno));

        return NULL;
    }

    File = fopen(FileName, "r");
    if (File == NULL) {
        fprintf(stderr,
                "chalk: Unable to load file %s: %s\n",
                FileName,
                strerror(errno));

        return NULL;
    }

    if (Stat.st_size > MAX_UINTN) {
        fprintf(stderr, "chalk: File too big: %s\n", FileName);
        goto LoadFileEnd;
    }

    Buffer = malloc(Stat.st_size + 1);
    if (Buffer == NULL) {
        goto LoadFileEnd;
    }

    TotalRead = 0;
    while (TotalRead < Stat.st_size) {
        BytesRead = fread(Buffer + TotalRead,
                          1,
                          Stat.st_size - TotalRead,
                          File);

        if (BytesRead == 0) {
            break;
        }

        if (BytesRead <= 0) {
            fprintf(stderr,
                    "chalk: Unable to read %s: %s\n",
                    FileName,
                    strerror(errno));

            free(Buffer);
            Buffer = NULL;
            goto LoadFileEnd;
        }

        TotalRead += BytesRead;
    }

    Buffer[TotalRead] = '\0';
    *FileSize = TotalRead;

LoadFileEnd:
    if (File != NULL) {
        fclose(File);
    }

    return Buffer;
}

INT
ChalkRunInteractiveInterpreter (
    PCK_APP_CONTEXT Context
    )

/*++

Routine Description:

    This routine implements the main loop for the interactive interpreter.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    INT Line;
    INT Status;

    printf(" _      _\n|_ |-| /-\\ |_ |<  Chalk %d.%d.%d\n",
           CHALK_VERSION_MAJOR,
           CHALK_VERSION_MINOR,
           CHALK_VERSION_REVISION);

    Status = 0;
    while (TRUE) {
        Line = Context->LineNumber;
        printf("%d> ", Line);
        fflush(NULL);
        Status = ChalkReadLine(Context);
        if (Status == EOF) {
            Status = 0;
            break;

        } else if (Status != 0) {
            break;
        }

        CkInterpret(Context->Vm,
                    NULL,
                    Context->Line,
                    strlen(Context->Line),
                    Line,
                    TRUE);
    }

    return Status;
}

INT
ChalkReadLine (
    PCK_APP_CONTEXT Context
    )

/*++

Routine Description:

    This routine reads a line in the interpreter.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    EOF on end of file.

    Returns an error number on failure.

--*/

{

    PSTR Buffer;
    UINTN Length;

    do {
        errno = 0;
        Buffer = fgets(Context->Line, CHALK_LINE_MAX, stdin);

    } while ((Buffer == NULL) && (errno == EINTR));

    if (Buffer != NULL) {
        Length = strlen(Buffer);
        if (Buffer[Length - 1] == '\n') {
            Context->LineNumber += 1;
        }

        return 0;
    }

    if ((feof(stdin)) || (errno == 0)) {
        return EOF;
    }

    fprintf(stderr, "Failed to read line: %s", strerror(errno));
    return errno;
}

