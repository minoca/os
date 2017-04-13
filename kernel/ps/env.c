/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    env.c

Abstract:

    This module implements environment support for processes.

Author:

    Evan Green 7-Mar-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "psp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define ENVIRONMENT_ALLOCATION_TAG 0x50766E45 // 'PvnE'

//
// Define the arbitrary maximum size for any environment.
//

#define MAX_ENVIRONMENT_SIZE (_1MB)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PSTR
PspFindNextArgument (
    PSTR PreviousArgument,
    PULONG BufferLength
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
PsCopyEnvironment (
    PPROCESS_ENVIRONMENT Source,
    PPROCESS_ENVIRONMENT *Destination,
    BOOL FromUserMode,
    PKTHREAD DestinationThread,
    PSTR OverrideImageName,
    UINTN OverrideImageNameSize
    )

/*++

Routine Description:

    This routine creates a copy of a pre-existing environment.

Arguments:

    Source - Supplies a pointer to the source environment to copy.

    Destination - Supplies a pointer where a pointer to the newly created
        environment will be returned.

    FromUserMode - Supplies a boolean indicating whether the environment exists
        in user mode or not.

    DestinationThread - Supplies an optional pointer to the user mode thread
        to copy the environment into. Supply NULL to copy the environment to
        a new kernel mode buffer.

    OverrideImageName - Supplies an optional pointer to an image name to use as
        an override of the image name in the source environment. The override
        is assumed to be a kernel mode buffer.

    OverrideImageNameSize - Supplies the size of the override image name,
        including the null terminator.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    UINTN ArgumentsBufferLength;
    ULONG BufferSize;
    PVOID CurrentBuffer;
    ULONG ElementCount;
    ULONG ElementIndex;
    UINTN EnvironmentBufferLength;
    PSTR ImageName;
    UINTN ImageNameBufferLength;
    UINTN ImageNameLength;
    PPROCESS_ENVIRONMENT NewEnvironment;
    UINTN Offset;
    PKPROCESS Process;
    KSTATUS Status;
    UINTN TerminatedEnvironmentCount;

    //
    // Environments cannot be copied directly from user mode to user mode.
    //

    ASSERT((FromUserMode == FALSE) || (DestinationThread == NULL));

    NewEnvironment = NULL;
    Process = PsGetCurrentProcess();

    //
    // If copying to or from user mode, this had better not be the kernel
    // process.
    //

    ASSERT(((FromUserMode == FALSE) && (DestinationThread == NULL)) ||
           (Process != PsGetKernelProcess()));

    //
    // Make sure the environment can end in a null.
    //

    TerminatedEnvironmentCount = Source->EnvironmentCount;
    if ((TerminatedEnvironmentCount == 0) ||
        (Source->Environment[TerminatedEnvironmentCount - 1] != NULL)) {

        TerminatedEnvironmentCount += 1;
    }

    ImageName = Source->ImageName;
    ImageNameLength = Source->ImageNameLength;
    if (OverrideImageName != NULL) {
        ImageName = OverrideImageName;
        ImageNameLength = OverrideImageNameSize;
    }

    if ((ImageName == NULL) || (ImageNameLength <= 1)) {
        Status = STATUS_INVALID_PARAMETER;
        goto CopyEnvironmentEnd;
    }

    //
    // Allocate space for the entirety of the new environment. The array of
    // pointers to arguments ends in a null pointer, hence the extra 1.
    //

    ImageNameBufferLength = ALIGN_RANGE_UP(ImageNameLength, sizeof(PVOID));
    ArgumentsBufferLength = ALIGN_RANGE_UP(Source->ArgumentsBufferLength,
                                           sizeof(PVOID));

    EnvironmentBufferLength = ALIGN_RANGE_UP(Source->EnvironmentBufferLength,
                                             sizeof(PVOID));

    AllocationSize = sizeof(PROCESS_ENVIRONMENT) + ImageNameBufferLength +
                     ((Source->ArgumentCount + 1) * sizeof(PSTR)) +
                     ArgumentsBufferLength +
                     (TerminatedEnvironmentCount * sizeof(PSTR)) +
                     EnvironmentBufferLength +
                     sizeof(PROCESS_START_DATA);

    if (DestinationThread != NULL) {
        AllocationSize += sizeof(PROCESS_START_DATA);
    }

    if (AllocationSize > MAX_ENVIRONMENT_SIZE) {
        Status = STATUS_OUT_OF_BOUNDS;
        goto CopyEnvironmentEnd;
    }

    if (DestinationThread != NULL) {

        //
        // If copying to user mode, then the user stack had better be set up,
        // and this had better be the only thread (otherwise sudden unmappings
        // could cause bad faults in kernel mode).
        //

        ASSERT((DestinationThread->UserStackSize != 0) &&
               (DestinationThread->OwningProcess == Process) &&
               (DestinationThread->ThreadParameter == NULL));

        //
        // Don't allow the environment to cover too much of the stack.
        //

        if (AllocationSize > DestinationThread->UserStackSize / 2) {
            RtlDebugPrint("Environment too large!\n");
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CopyEnvironmentEnd;
        }

        NewEnvironment = DestinationThread->UserStack +
                         DestinationThread->UserStackSize -
                         ALIGN_RANGE_UP(AllocationSize, STACK_ALIGNMENT);

        //
        // Set the thread parameter to point at the environment.
        //

        DestinationThread->ThreadParameter = NewEnvironment;

    } else {
        NewEnvironment = MmAllocatePagedPool(AllocationSize,
                                             ENVIRONMENT_ALLOCATION_TAG);

        if (NewEnvironment == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CopyEnvironmentEnd;
        }
    }

    AllocationSize -= sizeof(PROCESS_ENVIRONMENT);
    NewEnvironment->ImageName = (PVOID)(NewEnvironment + 1);

    //
    // Copy over the image name.
    //

    CurrentBuffer = NewEnvironment->ImageName;
    NewEnvironment->ImageNameLength = ImageNameLength;
    if (ImageNameLength != 0) {

        ASSERT(AllocationSize >= ImageNameLength);

        BufferSize = ImageNameLength;
        if ((FromUserMode != FALSE) && (ImageName == Source->ImageName)) {
            Status = MmCopyFromUserMode(NewEnvironment->ImageName,
                                        ImageName,
                                        BufferSize);

            if (!KSUCCESS(Status)) {
                goto CopyEnvironmentEnd;
            }

        } else {
            RtlCopyMemory(NewEnvironment->ImageName,
                          ImageName,
                          BufferSize);
        }

        NewEnvironment->ImageName[BufferSize - 1] = '\0';

        //
        // Move beyond the image name buffer and realign to a pointer boundary.
        //

        CurrentBuffer += ImageNameBufferLength;
        AllocationSize -= ImageNameBufferLength;
    }

    //
    // Copy over the arguments.
    //

    ElementCount = Source->ArgumentCount;
    NewEnvironment->ArgumentCount = ElementCount;

    ASSERT(AllocationSize >= ElementCount * sizeof(PSTR));

    NewEnvironment->Arguments = CurrentBuffer;

    //
    // Start by just copying over the array of pointers, to get them safely
    // over. They'll be fixed up in a moment.
    //

    if (FromUserMode != FALSE) {
        Status = MmCopyFromUserMode(NewEnvironment->Arguments,
                                    Source->Arguments,
                                    ElementCount * sizeof(PSTR));

        if (!KSUCCESS(Status)) {
            goto CopyEnvironmentEnd;
        }

    } else {
        RtlCopyMemory(NewEnvironment->Arguments,
                      Source->Arguments,
                      ElementCount * sizeof(PSTR));
    }

    CurrentBuffer += (ElementCount + 1) * sizeof(PSTR);
    AllocationSize -= (ElementCount + 1) * sizeof(PSTR);

    ASSERT(AllocationSize >= Source->ArgumentsBufferLength);

    BufferSize = Source->ArgumentsBufferLength;
    NewEnvironment->ArgumentsBufferLength = BufferSize;
    if (Source->ArgumentsBufferLength != 0) {
        NewEnvironment->ArgumentsBuffer = CurrentBuffer;
        CurrentBuffer += ArgumentsBufferLength;
        AllocationSize -= ArgumentsBufferLength;
        if (FromUserMode != FALSE) {
            Status = MmCopyFromUserMode(NewEnvironment->ArgumentsBuffer,
                                        Source->ArgumentsBuffer,
                                        BufferSize);

            if (!KSUCCESS(Status)) {
                goto CopyEnvironmentEnd;
            }

        } else {
            RtlCopyMemory(NewEnvironment->ArgumentsBuffer,
                          Source->ArgumentsBuffer,
                          BufferSize);
        }

        ((PSTR)NewEnvironment->ArgumentsBuffer)[BufferSize - 1] = '\0';
    }

    //
    // Recreate every argument string pointer in the new environment.
    //

    ASSERT((ElementCount == 1) ||
           (NewEnvironment->ArgumentsBuffer != NULL));

    for (ElementIndex = 0; ElementIndex < ElementCount; ElementIndex += 1) {

        //
        // Handle element zero specially as it may point to the image name,
        // not point inside the arguments buffer.
        //

        if ((ElementIndex == 0) &&
            (NewEnvironment->Arguments[0] == Source->ImageName)) {

            NewEnvironment->Arguments[0] = NewEnvironment->ImageName;
            continue;
        }

        //
        // The source argument points a certain way through the arguments
        // buffer. Find that offset and apply it to the destination buffer.
        //

        Offset = (UINTN)NewEnvironment->Arguments[ElementIndex] -
                 (UINTN)Source->ArgumentsBuffer;

        ASSERT(Offset < Source->ArgumentsBufferLength);

        if (Offset >= Source->ArgumentsBufferLength) {
            Status = STATUS_INVALID_PARAMETER;
            goto CopyEnvironmentEnd;
        }

        NewEnvironment->Arguments[ElementIndex] =
                          (PSTR)(NewEnvironment->ArgumentsBuffer + Offset);
    }

    //
    // A null pointer goes on the end of the list.
    //

    NewEnvironment->Arguments[ElementIndex] = NULL;

    //
    // Copy over the environment variables.
    //

    ElementCount = Source->EnvironmentCount;

    ASSERT(AllocationSize >= TerminatedEnvironmentCount * sizeof(PSTR));

    NewEnvironment->Environment = CurrentBuffer;
    NewEnvironment->EnvironmentCount = Source->EnvironmentCount;
    NewEnvironment->EnvironmentBuffer = NULL;
    NewEnvironment->EnvironmentBufferLength = 0;
    CurrentBuffer += TerminatedEnvironmentCount * sizeof(PSTR);
    AllocationSize -= TerminatedEnvironmentCount * sizeof(PSTR);
    if (ElementCount != 0) {

        //
        // Again, just copy over the array of pointers in one go, then fix them
        // up.
        //

        if (FromUserMode != FALSE) {
            Status = MmCopyFromUserMode(NewEnvironment->Environment,
                                        Source->Environment,
                                        ElementCount * sizeof(PSTR));

            if (!KSUCCESS(Status)) {
                goto CopyEnvironmentEnd;
            }

        } else {
            RtlCopyMemory(NewEnvironment->Environment,
                          Source->Environment,
                          ElementCount * sizeof(PSTR));
        }

        ASSERT(AllocationSize >= Source->EnvironmentBufferLength);
        ASSERT(Source->EnvironmentBufferLength != 0);

        BufferSize = Source->EnvironmentBufferLength;
        NewEnvironment->EnvironmentBuffer = CurrentBuffer;
        CurrentBuffer += EnvironmentBufferLength;
        AllocationSize -= EnvironmentBufferLength;
        NewEnvironment->EnvironmentBufferLength = BufferSize;
        if (FromUserMode != FALSE) {
            Status = MmCopyFromUserMode(NewEnvironment->EnvironmentBuffer,
                                        Source->EnvironmentBuffer,
                                        BufferSize);

            if (!KSUCCESS(Status)) {
                goto CopyEnvironmentEnd;
            }

        } else {
            RtlCopyMemory(NewEnvironment->EnvironmentBuffer,
                          Source->EnvironmentBuffer,
                          BufferSize);
        }

        ((PSTR)NewEnvironment->EnvironmentBuffer)[BufferSize - 1] = '\0';

        //
        // Recreate every environment variable string pointer in the new
        // environment. Note that the environment currently holds an array of
        // the source's pointers.
        //

        for (ElementIndex = 0; ElementIndex < ElementCount; ElementIndex += 1) {

            //
            // The source variable points a certain way through the environment
            // buffer. Find that offset and apply it to the destination buffer.
            //

            Offset = (UINTN)NewEnvironment->Environment[ElementIndex] -
                     (UINTN)Source->EnvironmentBuffer;

            ASSERT(Offset < Source->EnvironmentBufferLength);

            if (Offset >= Source->EnvironmentBufferLength) {
                Status = STATUS_INVALID_PARAMETER;
                goto CopyEnvironmentEnd;
            }

            NewEnvironment->Environment[ElementIndex] =
                            (PSTR)(NewEnvironment->EnvironmentBuffer + Offset);
        }
    }

    NewEnvironment->Environment[TerminatedEnvironmentCount - 1] = NULL;

    ASSERT(AllocationSize >= sizeof(PROCESS_START_DATA));

    if (DestinationThread != NULL) {

        ASSERT(Source->StartData != NULL);

        NewEnvironment->StartData = CurrentBuffer;
        RtlCopyMemory(NewEnvironment->StartData,
                      Source->StartData,
                      sizeof(PROCESS_START_DATA));

    } else {
        NewEnvironment->StartData = NULL;
    }

    Status = STATUS_SUCCESS;

CopyEnvironmentEnd:
    if (!KSUCCESS(Status)) {
        if (NewEnvironment != NULL) {
            MmFreePagedPool(NewEnvironment);
            NewEnvironment = NULL;
        }
    }

    *Destination = NewEnvironment;
    return Status;
}

KSTATUS
PsCreateEnvironment (
    PCSTR CommandLine,
    ULONG CommandLineSize,
    PSTR *EnvironmentVariables,
    ULONG EnvironmentVariableCount,
    PPROCESS_ENVIRONMENT *NewEnvironment
    )

/*++

Routine Description:

    This routine creates a new environment based on a command line.

Arguments:

    CommandLine - Supplies a pointer to a string containing the command line,
        including the image and any arguments.

    CommandLineSize - Supplies the size of the command line buffer in bytes,
        including the null terminator character.

    EnvironmentVariables - Supplies an optional pointer to an array of
        environment variables in the form name=value.

    EnvironmentVariableCount - Supplies the number of valid entries in the
        environment variables array. For example, 1 if there is a single
        environment variable.

    NewEnvironment - Supplies a pointer where a pointer to the newly created
        environment will be returned on success. The caller is responsible
        for freeing this memory from paged pool.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    ULONG ArgumentCount;
    ULONG ArgumentIndex;
    PSTR *ArgumentPointers;
    PVOID ArgumentsBuffer;
    PVOID Buffer;
    PSTR CurrentCommand;
    ULONG CurrentSize;
    PSTR CurrentVariable;
    PPROCESS_ENVIRONMENT Environment;
    UINTN EnvironmentBufferLength;
    UINTN EnvironmentIndex;
    PSTR NextArgument;
    KSTATUS Status;
    UINTN VariableLength;

    Environment = NULL;

    ASSERT((CommandLineSize != 0) && (CommandLine != NULL));
    ASSERT((PVOID)CommandLine > KERNEL_VA_START);

    //
    // Get past any initial spaces.
    //

    while (*CommandLine == ' ') {
        CommandLine += 1;
        CommandLineSize -= 1;
    }

    //
    // Discount any spaces at the end.
    //

    CurrentCommand = (PSTR)CommandLine + CommandLineSize - 1;
    while (*CurrentCommand == ' ') {
        CommandLineSize -= 1;
        CurrentCommand -= 1;
    }

    //
    // Loop through once to get the number of arguments.
    //

    CurrentCommand = (PSTR)CommandLine;
    CurrentSize = CommandLineSize;
    ArgumentCount = 0;
    while (TRUE) {
        ArgumentCount += 1;
        NextArgument = PspFindNextArgument(CurrentCommand, &CurrentSize);
        if (NextArgument == NULL) {
            break;
        }

        CurrentCommand = NextArgument + 1;
        while (RtlIsCharacterSpace(*CurrentCommand) != FALSE) {
            CurrentCommand += 1;
        }

        if (*CurrentCommand == '\0') {
            break;
        }
    }

    //
    // Also compute the size of the environment variables.
    //

    EnvironmentBufferLength = 0;
    for (EnvironmentIndex = 0;
         EnvironmentIndex < EnvironmentVariableCount;
         EnvironmentIndex += 1) {

        EnvironmentBufferLength +=
                   RtlStringLength(EnvironmentVariables[EnvironmentIndex]) + 1;
    }

    //
    // Allocate the buffer. Add room for a null entry on the end of the
    // pointer array.
    //

    AllocationSize = sizeof(PROCESS_ENVIRONMENT) +
                     ALIGN_RANGE_UP(CommandLineSize, sizeof(PVOID)) +
                     ((ArgumentCount + 1) * sizeof(PSTR));

    AllocationSize += ((EnvironmentVariableCount + 1) * sizeof(PSTR)) +
                      EnvironmentBufferLength;

    if (AllocationSize > MAX_ENVIRONMENT_SIZE) {
        Status = STATUS_OUT_OF_BOUNDS;
        goto CreateEnvironmentEnd;
    }

    Environment = MmAllocatePagedPool(AllocationSize,
                                      ENVIRONMENT_ALLOCATION_TAG);

    if (Environment == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateEnvironmentEnd;
    }

    RtlZeroMemory(Environment, sizeof(PROCESS_ENVIRONMENT));
    Buffer = Environment + 1;

    //
    // Copy the arguments over.
    //

    CurrentCommand = Buffer;
    RtlCopyMemory(CurrentCommand, CommandLine, CommandLineSize);

    //
    // Terminate the command line, both for safety and to truncate in case
    // trailing spaces were removed.
    //

    CurrentCommand[CommandLineSize - 1] = '\0';

    //
    // Loop through and assign pointers to all the arguments.
    //

    ArgumentsBuffer = NULL;
    ArgumentIndex = 0;
    CurrentCommand = Buffer;
    ArgumentPointers = Buffer + ALIGN_RANGE_UP(CommandLineSize, sizeof(PVOID));
    CurrentSize = CommandLineSize;
    while (TRUE) {

        ASSERT(ArgumentIndex < ArgumentCount);
        ASSERT(*CurrentCommand != ' ');

        //
        // Assign this argument pointer and get the next one.
        //

        ArgumentPointers[ArgumentIndex] = CurrentCommand;
        ArgumentIndex += 1;
        NextArgument = PspFindNextArgument(CurrentCommand, &CurrentSize);
        if (NextArgument == NULL) {
            break;
        }

        //
        // If there is a next argument, null terminate this argument.
        //

        *NextArgument = '\0';
        CurrentCommand = NextArgument + 1;
        while (RtlIsCharacterSpace(*CurrentCommand) != FALSE) {
            CurrentCommand += 1;
        }

        if (*CurrentCommand == '\0') {
            break;
        }

        //
        // Store a pointer to the second argument.
        //

        if (ArgumentsBuffer == NULL) {
            ArgumentsBuffer = CurrentCommand;
        }
    }

    ASSERT(ArgumentIndex == ArgumentCount);

    ArgumentPointers[ArgumentIndex] = NULL;

    //
    // Copy the environment variables over.
    //

    Environment->Environment = &(ArgumentPointers[ArgumentIndex + 1]);
    Environment->EnvironmentCount = EnvironmentVariableCount;
    Environment->EnvironmentBuffer = (PSTR)(Environment->Environment +
                                            (EnvironmentVariableCount + 1));

    Environment->EnvironmentBufferLength = EnvironmentBufferLength;
    CurrentVariable = Environment->EnvironmentBuffer;
    for (EnvironmentIndex = 0;
         EnvironmentIndex < EnvironmentVariableCount;
         EnvironmentIndex += 1) {

        Environment->Environment[EnvironmentIndex] = CurrentVariable;
        VariableLength =
                   RtlStringLength(EnvironmentVariables[EnvironmentIndex]) + 1;

        RtlCopyMemory(CurrentVariable,
                      EnvironmentVariables[EnvironmentIndex],
                      VariableLength);

        CurrentVariable += VariableLength;
    }

    Environment->Environment[EnvironmentIndex] = NULL;
    Environment->ImageName = Buffer;
    Environment->ImageNameLength = RtlStringLength(Buffer) + 1;
    Environment->Arguments = ArgumentPointers;
    Environment->ArgumentsBuffer = ArgumentsBuffer;
    Environment->ArgumentCount = ArgumentCount;
    Environment->ArgumentsBufferLength = CommandLineSize;
    Status = STATUS_SUCCESS;

CreateEnvironmentEnd:
    if (!KSUCCESS(Status)) {
        if (Environment != NULL) {
            MmFreePagedPool(Environment);
            Environment = NULL;
        }
    }

    *NewEnvironment = Environment;
    return Status;
}

VOID
PsDestroyEnvironment (
    PPROCESS_ENVIRONMENT Environment
    )

/*++

Routine Description:

    This routine destroys an environment and frees all resources associated with
    it. This routine can only be called on environments created in kernel space.

Arguments:

    Environment - Supplies a pointer to the environment to tear down.

Return Value:

    None.

--*/

{

    MmFreePagedPool(Environment);
    return;
}

VOID
PspInitializeProcessStartData (
    PPROCESS_START_DATA StartData,
    PLOADED_IMAGE OsBaseLibrary,
    PLOADED_IMAGE Executable,
    PLOADED_IMAGE Interpreter
    )

/*++

Routine Description:

    This routine initializes a process start data structure.

Arguments:

    StartData - Supplies a pointer to the start data structure to initialize.

    OsBaseLibrary - Supplies a pointer to the OS base library, loaded into
        every address space.

    Executable - Supplies a pointer to the executable image.

    Interpreter - Supplies an optional pointer to the interpreter image.

Return Value:

    None.

--*/

{

    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    StartData->Version = PROCESS_START_DATA_VERSION;
    StartData->PageSize = MmPageSize();
    RtlCopyMemory(&(StartData->Identity),
                  &(Thread->Identity),
                  sizeof(THREAD_IDENTITY));

    KeGetRandomBytes(StartData->Random, PROCESS_START_DATA_RANDOM_SIZE);
    if (Interpreter != NULL) {
        StartData->EntryPoint = Interpreter->EntryPoint;
        StartData->InterpreterBase = Interpreter->LoadedImageBuffer;

    } else {
        StartData->EntryPoint = Executable->EntryPoint;
        StartData->InterpreterBase = NULL;
    }

    StartData->ExecutableBase = Executable->LoadedImageBuffer;
    StartData->OsLibraryBase = OsBaseLibrary->LoadedImageBuffer;
    StartData->StackBase = NULL;
    StartData->IgnoredSignals = Thread->OwningProcess->IgnoredSignals;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PSTR
PspFindNextArgument (
    PSTR PreviousArgument,
    PULONG BufferLength
    )

/*++

Routine Description:

    This routine gets the next argument in a command string, taking into account
    double quotes.

Arguments:

    PreviousArgument - Supplies a pointer to the previous argument in the
        command string.

    BufferLength - Supplies a pointer that on input supplies the total length of
        the buffer, including the null terminator. On output, this variable will
        be moved forward by the size of the previous argument.

Return Value:

    Returns a pointer to the next argument on success.

    NULL if there are no more arguments in the string.

--*/

{

    PSTR CurrentString;
    BOOL InQuotes;
    ULONG Length;

    CurrentString = PreviousArgument;
    InQuotes = FALSE;
    Length = *BufferLength;

    //
    // Get past any spaces at the beginning.
    //

    while ((Length != 0) && (RtlIsCharacterSpace(*CurrentString) != FALSE)) {
        Length -= 1;
        CurrentString += 1;
    }

    //
    // Find the first space not surrounded in quotes. Watch out for the end of
    // the line.
    //

    while (Length != 0) {
        if (*CurrentString == '\0') {
            break;
        }

        if (InQuotes != FALSE) {
            if (*CurrentString == '"') {
                InQuotes = FALSE;
            }

        } else {
            if (*CurrentString == '"') {
                InQuotes = TRUE;
            }

            if (RtlIsCharacterSpace(*CurrentString) != FALSE) {
                break;
            }
        }

        Length -= 1;
        CurrentString += 1;
    }

    //
    // Return unsuccessfully if the end was hit, or return a pointer to the
    // first character of the next argument.
    //

    if ((Length == 0) || (*CurrentString == '\0')) {
        return NULL;
    }

    *BufferLength = Length;
    return CurrentString;
}

