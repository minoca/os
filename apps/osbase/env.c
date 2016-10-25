/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    env.c

Abstract:

    This module implements environment support for user mode programs.

Author:

    Evan Green 7-Mar-2013

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "osbasep.h"

//
// ---------------------------------------------------------------- Definitions
//

#define ENVIRONMENT_ALLOCATION_TAG 0x21766E45

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
// Store a pointer to the environment.
//

PPROCESS_ENVIRONMENT OsEnvironment = NULL;

//
// ------------------------------------------------------------------ Functions
//

OS_API
PPROCESS_ENVIRONMENT
OsCreateEnvironment (
    PSTR ImagePath,
    ULONG ImagePathLength,
    PSTR *ArgumentValues,
    ULONG ArgumentValuesTotalLength,
    ULONG ArgumentCount,
    PSTR *EnvironmentValues,
    ULONG EnvironmentValuesTotalLength,
    ULONG EnvironmentCount
    )

/*++

Routine Description:

    This routine creates an environment that can be passed to the kernel for
    execution of an image. This routine will use the heap.

Arguments:

    ImagePath - Supplies a pointer to the name of the image that will be
        executed.

    ImagePathLength - Supplies the length of the image path buffer in bytes,
        including the null terminator.

    ArgumentValues - Supplies an array of pointers to arguments to pass to the
        image.

    ArgumentValuesTotalLength - Supplies the total length of all arguments, in
        bytes, including their null terminators.

    ArgumentCount - Supplies the number of arguments in the argument values
        array.

    EnvironmentValues - Supplies an array of pointers to environment variables
        to set in the new environment. Environment variables take the form
        "name=value".

    EnvironmentValuesTotalLength - Supplies the total length of all environment
        variables, in bytes, including their null terminators.

    EnvironmentCount - Supplies the number of environment variable strings
        present in the environment values array. This should not include any
        null terminating entry in the array.

Return Value:

    Returns a pointer to a heap allocated environment, suitable for sending to
    the execute image system call.

    NULL on failure.

--*/

{

    ULONG AllocationSize;
    ULONG BufferSize;
    PSTR CurrentBuffer;
    ULONG ElementIndex;
    PPROCESS_ENVIRONMENT Environment;
    BOOL Result;
    ULONG StringSize;
    ULONG TerminatedEnvironmentCount;

    Result = FALSE;
    TerminatedEnvironmentCount = EnvironmentCount;
    if ((EnvironmentCount == 0) ||
        (EnvironmentValues[EnvironmentCount - 1] != NULL)) {

        TerminatedEnvironmentCount += 1;
    }

    //
    // Allocate the beast. There's a null pointer at the end of the list too
    // (hence the argument count plus one).
    //

    AllocationSize = sizeof(PROCESS_ENVIRONMENT) +
                     ALIGN_RANGE_UP(ImagePathLength, sizeof(PVOID)) +
                     ((ArgumentCount + 1) * sizeof(PSTR)) +
                     ALIGN_RANGE_UP(ArgumentValuesTotalLength, sizeof(PVOID)) +
                     (TerminatedEnvironmentCount * sizeof(PSTR)) +
                     EnvironmentValuesTotalLength;

    Environment = OsHeapAllocate(AllocationSize, ENVIRONMENT_ALLOCATION_TAG);
    if (Environment == NULL) {
        return NULL;
    }

    RtlZeroMemory(Environment, sizeof(PROCESS_ENVIRONMENT));

    //
    // Copy the image path.
    //

    Environment->ImageName = (PSTR)(Environment + 1);
    RtlStringCopy(Environment->ImageName, ImagePath, ImagePathLength);
    Environment->ImageNameLength = ImagePathLength;

    //
    // Make room for the arguments and its buffer.
    //

    Environment->Arguments =
                      (PSTR *)((PVOID)Environment->ImageName +
                               ALIGN_RANGE_UP(ImagePathLength, sizeof(PVOID)));

    Environment->ArgumentCount = ArgumentCount;
    Environment->ArgumentsBuffer = (PVOID)Environment->Arguments +
                                   ((ArgumentCount + 1) * sizeof(PSTR));

    Environment->ArgumentsBufferLength = ArgumentValuesTotalLength;

    //
    // Make room for the environment and its buffer.
    //

    Environment->Environment =
            (PSTR *)(Environment->ArgumentsBuffer +
                     ALIGN_RANGE_UP(ArgumentValuesTotalLength, sizeof(PVOID)));

    Environment->EnvironmentCount = EnvironmentCount;
    Environment->EnvironmentBuffer =
                                   (PVOID)Environment->Environment +
                                   (TerminatedEnvironmentCount * sizeof(PSTR));

    Environment->EnvironmentBufferLength = EnvironmentValuesTotalLength;

    //
    // Copy over each argument. The first is always the image name.
    //

    BufferSize = ArgumentValuesTotalLength;
    CurrentBuffer = Environment->ArgumentsBuffer;
    for (ElementIndex = 0; ElementIndex < ArgumentCount; ElementIndex += 1) {
        StringSize = RtlStringCopy(CurrentBuffer,
                                   ArgumentValues[ElementIndex],
                                   BufferSize);

        if (StringSize == 0) {
            goto CreateEnvironmentEnd;
        }

        Environment->Arguments[ElementIndex] = CurrentBuffer;
        CurrentBuffer += StringSize;
        BufferSize -= StringSize;
    }

    //
    // Copy over each environment variable.
    //

    BufferSize = EnvironmentValuesTotalLength;
    CurrentBuffer = Environment->EnvironmentBuffer;
    for (ElementIndex = 0; ElementIndex < EnvironmentCount; ElementIndex += 1) {

        //
        // Protect against a supplied environment that was null-terminated.
        //

        if ((ElementIndex == (EnvironmentCount - 1)) &&
            (EnvironmentValues[ElementIndex] == NULL)) {

            break;
        }

        StringSize = RtlStringCopy(CurrentBuffer,
                                   EnvironmentValues[ElementIndex],
                                   BufferSize);

        if (StringSize == 0) {
            goto CreateEnvironmentEnd;
        }

        Environment->Environment[ElementIndex] = CurrentBuffer;
        CurrentBuffer += StringSize;
        BufferSize -= StringSize;
    }

    //
    // NULL out the last one.
    //

    ASSERT(ElementIndex == TerminatedEnvironmentCount - 1);

    Environment->Environment[ElementIndex] = NULL;
    Result = TRUE;

CreateEnvironmentEnd:
    if (Result == FALSE) {
        if (Environment != NULL) {
            OsHeapFree(Environment);
            Environment = NULL;
        }
    }

    return Environment;
}

OS_API
VOID
OsDestroyEnvironment (
    PPROCESS_ENVIRONMENT Environment
    )

/*++

Routine Description:

    This routine destroys an environment created with the create environment
    function.

Arguments:

    Environment - Supplies a pointer to the environment to destroy.

Return Value:

    None.

--*/

{

    OsHeapFree(Environment);
    return;
}

OS_API
PPROCESS_ENVIRONMENT
OsGetCurrentEnvironment (
    VOID
    )

/*++

Routine Description:

    This routine gets the environment for the current process.

Arguments:

    None.

Return Value:

    Returns a pointer to the current environment. This is shared memory, and
    should not be altered by the caller.

--*/

{

    return OsEnvironment;
}

//
// --------------------------------------------------------- Internal Functions
//

