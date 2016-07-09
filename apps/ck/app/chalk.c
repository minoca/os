/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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

#include <assert.h>
#include <errno.h>
#include <getopt.h>
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
    "  --debug-gc -- Stress the garbage collector.\n"                          \
    "  --debug-compiler -- Print the compiled bytecode.\n"                     \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define CHALK_OPTIONS_STRING "hV"

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
    PSTR FileName
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

    ULONG ArgumentIndex;
    CK_APP_CONTEXT Context;
    PSTR FileBuffer;
    INT Option;
    int Status;

    FileBuffer = NULL;
    Status = ChalkInitializeContext(&Context);
    if (Status != 0) {
        fprintf(stderr, "Failed to initialize: %s.\n", strerror(Status));
        Status = 2;
        goto MainEnd;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
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
        case CHALK_OPTION_DEBUG_GC:
            Context.Configuration.Flags |= CK_CONFIGURATION_GC_STRESS;
            break;

        case CHALK_OPTION_DEBUG_COMPILER:
            Context.Configuration.Flags |= CK_CONFIGURATION_DEBUG_COMPILER;
            break;

        case 'V':
            printf("Chalk version %d.%d.%d. Copyright 2016 Minoca Corp. "
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

    Context.Vm = CkCreateVm(&(Context.Configuration));
    if (Context.Vm == NULL) {
        fprintf(stderr, "Error: Failed to create VM\n");
        Status = 2;
        goto MainEnd;
    }

    ArgumentIndex = optind;
    if (ArgumentIndex < ArgumentCount) {
        FileBuffer = ChalkLoadFile(Arguments[ArgumentIndex]);
        if (FileBuffer == NULL) {
            fprintf(stderr,
                    "Error: Failed to load file %s: %s\n",
                    Arguments[ArgumentIndex],
                    strerror(errno));

            Status = 2;
            goto MainEnd;
        }

        ArgumentIndex += 1;

        //
        // TODO: Execute the file here.
        //

        Status = 2;

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
    PSTR FileName
    )

/*++

Routine Description:

    This routine loads a file, returning its contents.

Arguments:

    FileName - Supplies a pointer to the file path to load.

Return Value:

    Returns a pointer to the contents of the file, null terminated.

--*/

{

    PSTR Buffer;
    FILE *File;
    struct stat Stat;
    INT Status;

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

    if (fread(Buffer, 1, Stat.st_size, File) != Stat.st_size) {
        fprintf(stderr,
                "chalk: Unable to read %s: %s\n",
                FileName,
                strerror(errno));

        free(Buffer);
        Buffer = NULL;
        goto LoadFileEnd;
    }

    Buffer[Stat.st_size] = '\0';

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

    CK_ERROR_TYPE Error;
    INT Status;

    printf(" _      _\n|_ |-| /-\\ |_ |<  Chalk %d.%d.%d\n",
           CHALK_VERSION_MAJOR,
           CHALK_VERSION_MINOR,
           CHALK_VERSION_REVISION);

    Status = 0;
    while (TRUE) {
        printf("%d> ", Context->LineNumber);
        Status = ChalkReadLine(Context);
        if (Status == EOF) {
            Status = 0;
            break;

        } else if (Status != 0) {
            break;
        }

        Error = CkInterpret(Context->Vm, Context->Line, strlen(Context->Line));
        if (Error != CkSuccess) {
            switch (Error) {
            case CkErrorNoMemory:
                fprintf(stderr, "Allocation failure\n");
                break;

            case CkErrorCompile:
                fprintf(stderr, "Compile error\n");
                break;

            case CkErrorRuntime:
                fprintf(stderr, "Runtime error\n");
                break;

            case CkErrorStackTrace:
                fprintf(stderr, "Error: Stack trace\n");
                break;

            default:
                break;
            }
        }
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

    return errno;
}

