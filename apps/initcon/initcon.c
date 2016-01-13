/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    initcon.c

Abstract:

    This module implements the console initialization application. It is
    usually the first user mode process in the system. It is responsible for
    setting up the standard I/O descriptors (which it wires to the local
    video console), setting any environment variables requested via the
    command line, and then launching the first useful application (via the
    remainder of the command line arguments).

Author:

    Evan Green 14-Mar-2013

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <osbase.h>
#include <stdio.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define MAX_ENVIRONMENT_COUNT 50

#define INITCON_RETRY_COUNT 10
#define INITCON_RETRY_DELAY 1000000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
PrintSystemVersionBanner (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the terminal reset sequence that gets written out when the
// application connects to the terminal.
//

CHAR InitconResetSequence[2] = {0x1B, 'c'};

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine implements the test user mode program.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR Argument;
    ULONG ArgumentIndex;
    UINTN BytesCompleted;
    PPROCESS_ENVIRONMENT Environment;
    UINTN EnvironmentVariableCount;
    PSTR EnvironmentVariables[MAX_ENVIRONMENT_COUNT];
    UINTN EnvironmentVariableSize;
    FILE_CONTROL_PARAMETERS_UNION FileControlParameters;
    PSTR Image;
    ULONG ImageLength;
    UINTN Retry;
    HANDLE StandardIn;
    KSTATUS Status;
    PSTR TerminalPath;
    PSTR ThisImage;
    ULONG TotalLength;

    EnvironmentVariableCount = 0;
    EnvironmentVariableSize = 0;
    TerminalPath = LOCAL_TERMINAL_PATH;

    //
    // Add any environment variables.
    //

    while (ArgumentCount > 1) {
        Argument = Arguments[1];
        if (RtlAreStringsEqual(Argument, "-e", sizeof("-e")) != FALSE) {
            Arguments += 1;
            ArgumentCount -= 1;
            if (ArgumentCount > 1) {
                if (EnvironmentVariableCount < MAX_ENVIRONMENT_COUNT) {
                    EnvironmentVariables[EnvironmentVariableCount] =
                                                                  Arguments[1];

                    EnvironmentVariableSize += RtlStringLength(Arguments[1]) +
                                               1;

                    EnvironmentVariableCount += 1;

                } else {
                    RtlDebugPrint("Too many environment variables!\n");
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto mainEnd;
                }

                Arguments += 1;
                ArgumentCount -= 1;

            } else {
                RtlDebugPrint("Expected an argument for -e.\n");
                Status = STATUS_INVALID_PARAMETER;
                goto mainEnd;
            }

        } else if (RtlAreStringsEqual(Argument, "-t", sizeof("-t")) != FALSE) {
            Arguments += 1;
            ArgumentCount -= 1;
            if (ArgumentCount > 1) {
                TerminalPath = Arguments[1];
                Arguments += 1;
                ArgumentCount -= 1;

            } else {
                RtlDebugPrint("Expected an argument for -t.\n");
                Status = STATUS_INVALID_PARAMETER;
                goto mainEnd;
            }

        } else {
            break;
        }
    }

    //
    // Open up the local terminal.
    //

    Status = STATUS_SUCCESS;
    for (Retry = 0; Retry < INITCON_RETRY_COUNT; Retry += 1) {
        Status = OsOpen(INVALID_HANDLE,
                        TerminalPath,
                        RtlStringLength(TerminalPath) + 1,
                        SYS_OPEN_FLAG_READ | SYS_OPEN_FLAG_WRITE,
                        FILE_PERMISSION_NONE,
                        &StandardIn);

        if (KSUCCESS(Status)) {
            break;
        }

        OsDelayExecution(FALSE, INITCON_RETRY_DELAY);
    }

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to open local terminal %s: %x\n",
                      TerminalPath,
                      Status);

        goto mainEnd;
    }

    //
    // Reset the terminal for freshness.
    //

    OsPerformIo(StandardIn,
                0,
                sizeof(InitconResetSequence),
                SYS_IO_FLAG_WRITE,
                1000,
                InitconResetSequence,
                &BytesCompleted);

    //
    // Duplicate the file descriptor for standard out and standard error.
    //

    FileControlParameters.DuplicateDescriptor = (HANDLE)STDOUT_FILENO;
    Status = OsFileControl(StandardIn,
                           FileControlCommandDuplicate,
                           &FileControlParameters);

    if (!KSUCCESS(Status)) {
        goto mainEnd;
    }

    if (FileControlParameters.DuplicateDescriptor != (HANDLE)STDOUT_FILENO) {
        Status = STATUS_INVALID_HANDLE;
        goto mainEnd;
    }

    FileControlParameters.DuplicateDescriptor = (HANDLE)STDERR_FILENO;
    Status = OsFileControl(StandardIn,
                           FileControlCommandDuplicate,
                           &FileControlParameters);

    if (!KSUCCESS(Status)) {
        goto mainEnd;
    }

    if (FileControlParameters.DuplicateDescriptor != (HANDLE)STDERR_FILENO) {
        Status = STATUS_INVALID_HANDLE;
        goto mainEnd;
    }

    //
    // Say hello now that standard out is set up.
    //

    PrintSystemVersionBanner();

    //
    // Piece out the image name, and calculate the total size of all the
    // arguments.
    //

    TotalLength = 0;
    if (ArgumentCount > 1) {
        Image = Arguments[1];
        ImageLength = RtlStringLength(Image) + 1;
        for (ArgumentIndex = 1;
             ArgumentIndex < ArgumentCount;
             ArgumentIndex += 1) {

            TotalLength += RtlStringLength(Arguments[ArgumentIndex]) + 1;
        }

        //
        // Create a new environment.
        //

        Environment = OsCreateEnvironment(Image,
                                          ImageLength,
                                          &(Arguments[1]),
                                          TotalLength,
                                          ArgumentCount - 1,
                                          EnvironmentVariables,
                                          EnvironmentVariableSize,
                                          EnvironmentVariableCount);

        //
        // Execute the image, never to return hopefully.
        //

        Status = OsExecuteImage(Environment);

    } else {
        ThisImage = "InitCon";
        if (ArgumentCount >= 1) {
            ThisImage = Arguments[0];
        }

        printf("%s called without arguments. Nothing to execute!\n", ThisImage);
    }

mainEnd:
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
PrintSystemVersionBanner (
    VOID
    )

/*++

Routine Description:

    This routine prints the system name and version number to standard output.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KSTATUS Status;
    SYSTEM_VERSION_INFORMATION Version;
    CHAR VersionStringBuffer[2048];

    Status = OsGetSystemVersion(&Version, TRUE);
    if (KSUCCESS(Status)) {
        RtlGetSystemVersionString(&Version,
                                  SystemVersionStringBasic,
                                  VersionStringBuffer,
                                  sizeof(VersionStringBuffer));

        puts(VersionStringBuffer);
        fflush(NULL);
    }

    return;
}

