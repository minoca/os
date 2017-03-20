/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    init.c

Abstract:

    This module is responsible for initializing the Kernel Executive subsystem.

Author:

    Evan Green 6-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "keinit.h"
#include <minoca/lib/bconf.h>
#include "kep.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
KepInitializeUserSharedData (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

KSTATUS
KepInitializeCommandLine (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

KSTATUS
KepInitializeEntropy (
    VOID
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the initial array of processor blocks, which is just an array of 1
// pointing to P0's processor blocks.
//

PPROCESSOR_BLOCK KeP0ProcessorBlockArray = NULL;

//
// Structures that store the processor blocks and total number of processors.
//

PPROCESSOR_BLOCK *KeProcessorBlocks = &KeP0ProcessorBlockArray;
ULONG KeProcessorBlockArraySize = 1;
volatile ULONG KeProcessorCount = 1;
volatile ULONG KeActiveProcessorCount = 0;

//
// Queued lock directory where all queued locks are stored. This is primarily
// done to keep the root directory tidy.
//

extern POBJECT_HEADER KeQueuedLockDirectory;

//
// Directory for events with no parent, used primarily to keep the root
// directory uncluttered.
//

extern POBJECT_HEADER KeEventDirectory;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
KeInitialize (
    ULONG Phase,
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine initializes the Kernel Executive subsystem. There is no
    synchronization in this routine, it is assumed that processors do not
    run through this routine concurrently.

Arguments:

    Phase - Supplies the initialization phase. Valid values are 0 through 3.

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    Status code.

--*/

{

    PPROCESSOR_BLOCK ProcessorBlock;
    KSTATUS Status;

    //
    // Initialize the processor block in phase 0. Phase 0 is called on all
    // processors.
    //

    if (Phase == 0) {
        ProcessorBlock = KeGetCurrentProcessorBlock();
        ProcessorBlock->RunLevel = RunLevelLow;
        KeInitializeSpinLock(&(ProcessorBlock->IpiListLock));
        INITIALIZE_LIST_HEAD(&(ProcessorBlock->IpiListHead));
        INITIALIZE_LIST_HEAD(&(ProcessorBlock->DpcList));
        KeInitializeSpinLock(&(ProcessorBlock->DpcLock));
        ProcessorBlock->CyclePeriodAccount = CycleAccountKernel;
        KepInitializeScheduler(ProcessorBlock);

        ASSERT(ProcessorBlock->ProcessorNumber < KeProcessorBlockArraySize);

        //
        // Add the current processor to the array of processor blocks.
        //

        KeProcessorBlocks[ProcessorBlock->ProcessorNumber] = ProcessorBlock;

        //
        // Initialize the system resource manager.
        //

        if (ProcessorBlock->ProcessorNumber == 0) {
            KeSystemFirmwareType = Parameters->FirmwareType;
            Status = KepInitializeSystemResources(Parameters, 0);
            if (!KSUCCESS(Status)) {
                goto InitializeEnd;
            }
        }

        //
        // Do architecture dependent initialization.
        //

        Status = KepArchInitialize(Parameters, Phase);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        RtlAtomicAdd32(&KeActiveProcessorCount, 1);

        //
        // Fire up the built in base video library.
        //

        Status = KepInitializeBaseVideo(Parameters);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

    //
    // Set up directories for events and queued locks in phase 1.
    //

    } else if (Phase == 1) {
        ProcessorBlock = KeGetCurrentProcessorBlock();
        if (ProcessorBlock->ProcessorNumber == 0) {
            Status = ArFinishBootProcessorInitialization();
            if (!KSUCCESS(Status)) {
                goto InitializeEnd;
            }

            Status = KepInitializeCommandLine(Parameters);
            if (!KSUCCESS(Status)) {
                goto InitializeEnd;
            }

            Status = KepInitializeSystemResources(NULL, 1);
            if (!KSUCCESS(Status)) {
                goto InitializeEnd;
            }

            KeQueuedLockDirectory = ObCreateObject(
                                                 ObjectDirectory,
                                                 NULL,
                                                 "QueuedLocks",
                                                 sizeof("QueuedLocks"),
                                                 sizeof(OBJECT_HEADER),
                                                 NULL,
                                                 OBJECT_FLAG_USE_NAME_DIRECTLY,
                                                 KE_ALLOCATION_TAG);

            if (KeQueuedLockDirectory == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto InitializeEnd;
            }

            KeEventDirectory = ObCreateObject(ObjectDirectory,
                                              NULL,
                                              "Events",
                                              sizeof("Events"),
                                              sizeof(OBJECT_HEADER),
                                              NULL,
                                              OBJECT_FLAG_USE_NAME_DIRECTLY,
                                              KE_ALLOCATION_TAG);

            if (KeEventDirectory == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto InitializeEnd;
            }

            //
            // Initialize system crash support.
            //

            Status = KepInitializeCrashDumpSupport();
            if (!KSUCCESS(Status)) {
                goto InitializeEnd;
            }
        }

        //
        // Initialize the clock information, run on all processors.
        //

        KepInitializeClock(ProcessorBlock);

        //
        // Create the timer queue for the processor.
        //

        ProcessorBlock->TimerData = KepCreateTimerData();
        if (ProcessorBlock->TimerData == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeEnd;
        }

        //
        // Perform architecture-specific setup for the user shared data page.
        //

        ArSetUpUserSharedDataFeatures();
        Status = STATUS_SUCCESS;

    //
    // Create the worker threads in phase 2.
    //

    } else if (Phase == 2) {
        ProcessorBlock = KeGetCurrentProcessorBlock();

        //
        // Call the initialize clock routine again (only on processor 0) now
        // that the true time counter has been established.
        //

        KepInitializeClock(ProcessorBlock);
        Status = KepInitializeSystemWorkQueue();
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        Status= KepInitializeUserSharedData(Parameters);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

    //
    // Phase 3 occurs after I/O has started up.
    //

    } else {

        ASSERT(Phase == 3);

        Status = KepInitializeEntropy();
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }
    }

InitializeEnd:
    return Status;
}

PPROCESSOR_START_BLOCK
KePrepareForProcessorLaunch (
    VOID
    )

/*++

Routine Description:

    This routine prepares the kernel's internal structures for a new processor
    coming online.

Arguments:

    None.

Return Value:

    Returns a pointer to an allocated and filled out processor start block
    structure. At this point the kernel will be ready for this processor to
    come online at any time.

    NULL on failure.

--*/

{

    ULONG ArraySizeInBytes;
    PPROCESSOR_BLOCK *NewProcessorBlockArray;
    ULONG NewProcessorBlockArraySize;
    PPROCESSOR_BLOCK *OldArray;
    ULONG ProcessorNumber;
    PPROCESSOR_START_BLOCK StartBlock;
    KSTATUS Status;

    StartBlock = NULL;

    //
    // Get the next processor number.
    //

    ProcessorNumber = RtlAtomicAdd32(&KeProcessorCount, 1);

    //
    // If needed, expand the the processor block pointer array to accomodate
    // this new processor.
    //

    if (ProcessorNumber >= KeProcessorBlockArraySize) {
        NewProcessorBlockArraySize = KeProcessorBlockArraySize * 2;

        ASSERT(NewProcessorBlockArraySize > ProcessorNumber);

        ArraySizeInBytes = NewProcessorBlockArraySize *
                           sizeof(PPROCESSOR_BLOCK);

        NewProcessorBlockArray = MmAllocateNonPagedPool(ArraySizeInBytes,
                                                        KE_ALLOCATION_TAG);

        if (NewProcessorBlockArray == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto PrepareForProcessorLaunchEnd;
        }

        OldArray = KeProcessorBlocks;
        RtlCopyMemory(NewProcessorBlockArray,
                      OldArray,
                      KeProcessorBlockArraySize * sizeof(PVOID));

        //
        // Assign the new array, assign the new array size, then free the old
        // array.
        //

        KeProcessorBlocks = NewProcessorBlockArray;
        KeProcessorBlockArraySize = NewProcessorBlockArraySize;
        if (OldArray != &KeP0ProcessorBlockArray) {
            MmFreeNonPagedPool(OldArray);
        }
    }

    //
    // Allocate the start block structure.
    //

    StartBlock = MmAllocateNonPagedPool(sizeof(PROCESSOR_START_BLOCK),
                                        KE_ALLOCATION_TAG);

    if (StartBlock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PrepareForProcessorLaunchEnd;
    }

    RtlZeroMemory(StartBlock, sizeof(PROCESSOR_START_BLOCK));

    //
    // Allocate basic processor structures.
    //

    StartBlock->ProcessorNumber = ProcessorNumber;
    StartBlock->ProcessorStructures =
                                ArAllocateProcessorStructures(ProcessorNumber);

    if (StartBlock->ProcessorStructures == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PrepareForProcessorLaunchEnd;
    }

    //
    // MM has some structures to create as well.
    //

    Status = MmPrepareForProcessorLaunch(StartBlock);
    if (!KSUCCESS(Status)) {
        goto PrepareForProcessorLaunchEnd;
    }

    Status = STATUS_SUCCESS;

PrepareForProcessorLaunchEnd:
    if (!KSUCCESS(Status)) {
        if (StartBlock != NULL) {
            MmDestroyProcessorStartBlock(StartBlock);
            if (StartBlock->ProcessorStructures != NULL) {
                ArFreeProcessorStructures(StartBlock->ProcessorStructures);
            }

            MmFreeNonPagedPool(StartBlock);
            StartBlock = NULL;
        }
    }

    return StartBlock;
}

VOID
KeFreeProcessorStartBlock (
    PPROCESSOR_START_BLOCK StartBlock,
    BOOL FreeResourcesInside
    )

/*++

Routine Description:

    This routine frees a processor start block structure.

Arguments:

    StartBlock - Supplies a pointer to the start block structure to free.

    FreeResourcesInside - Supplies a boolean indicating whether or not to free
        the resources contained inside the start block.

Return Value:

    None.

--*/

{

    if (FreeResourcesInside != FALSE) {
        MmDestroyProcessorStartBlock(StartBlock);
        if (StartBlock->ProcessorStructures != NULL) {
            ArFreeProcessorStructures(StartBlock->ProcessorStructures);
        }
    }

    MmFreeNonPagedPool(StartBlock);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
KepInitializeUserSharedData (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine initializes the shared user data area.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    Status code.

--*/

{

    CALENDAR_TIME CalendarTime;
    KSTATUS Status;
    SYSTEM_TIME SystemTime;
    PUSER_SHARED_DATA UserSharedData;

    UserSharedData = MmGetUserSharedData();
    UserSharedData->EncodedSystemVersion = KeEncodedVersion;
    UserSharedData->SystemVersionSerial = KeVersionSerial;
    UserSharedData->BuildTime = KeBuildTime;
    UserSharedData->TimeCounterFrequency = HlQueryTimeCounterFrequency();
    UserSharedData->ProcessorCounterFrequency =
                                            HlQueryProcessorCounterFrequency();

    //
    // If no calendar services are around, set this to the boot time and go
    // from there.
    //

    if (UserSharedData->TimeOffset.Seconds == 0) {
        Status = KepSetTimeOffset(&(Parameters->BootTime), NULL);
        if (!KSUCCESS(Status)) {
            goto InitializeUserSharedDataEnd;
        }
    }

    //
    // Print the boot time out to the debugger.
    //

    KeGetSystemTime(&SystemTime);
    RtlSystemTimeToGmtCalendarTime(&(Parameters->BootTime), &CalendarTime);
    RtlDebugPrint("Boot time: %02d/%02d/%04d %02d:%02d:%02d GMT\n",
                  CalendarTime.Month + 1,
                  CalendarTime.Day,
                  CalendarTime.Year,
                  CalendarTime.Hour,
                  CalendarTime.Minute,
                  CalendarTime.Second);

    Status = STATUS_SUCCESS;

InitializeUserSharedDataEnd:
    return Status;
}

KSTATUS
KepInitializeCommandLine (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine initializes and parses the kernel command line parameters.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PKERNEL_ARGUMENT Argument;
    UINTN ArgumentCount;
    PBOOT_ENTRY BootEntry;
    CHAR Character;
    PSTR Component;
    PSTR Current;
    PKERNEL_COMMAND_LINE Information;
    BOOL InQuote;
    PSTR Line;
    PSTR LineCopy;
    PSTR Name;
    PCSTR OriginalString;
    ULONG Pass;
    UINTN StringSize;
    ULONG ValueIndex;

    BootEntry = Parameters->BootEntry;
    if (BootEntry == NULL) {
        return STATUS_SUCCESS;
    }

    OriginalString = BootEntry->KernelArguments;
    StringSize = RtlStringLength(OriginalString) + 1;
    if (StringSize > KERNEL_MAX_COMMAND_LINE) {
        StringSize = KERNEL_MAX_COMMAND_LINE;
        RtlDebugPrint("Truncated kernel command line.\n");
    }

    Line = MmAllocateNonPagedPool(StringSize, KE_ALLOCATION_TAG);
    if (Line == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (StringSize != 1) {
        RtlCopyMemory(Line, OriginalString, StringSize);
    }

    Line[StringSize - 1] = '\0';
    Current = Line;
    Argument = NULL;
    ArgumentCount = 0;
    Information = NULL;

    //
    // Parse the arguments in the form component.name=value1,value2,...
    //

    for (Pass = 0; Pass < 2; Pass += 1) {
        while (*Current != '\0') {
            Component = NULL;
            Name = NULL;

            //
            // Skip leading spaces.
            //

            while (RtlIsCharacterBlank(*Current) != FALSE) {
                Current += 1;
            }

            if (*Current == '\0') {
                break;
            }

            Component = Current;
            while ((*Current != '\0') &&
                   (*Current != '.') &&
                   (RtlIsCharacterSpace(*Current) == FALSE)) {

                Current += 1;
            }

            if (*Current != '.') {
                if (Argument != NULL) {
                    RtlDebugPrint("Ignoring argument starting at: %s\n",
                                  Component);
                }

                continue;
            }

            if (Argument != NULL) {
                *Current = '\0';
            }

            Current += 1;
            Name = Current;
            while ((*Current != '\0') &&
                   (*Current != '=') &&
                   (RtlIsCharacterBlank(*Current) == FALSE)) {

                Current += 1;
            }

            if (Argument != NULL) {
                Argument->Component = Component;
                Argument->Name = Name;
            }

            //
            // If the argument contains no equals, it's just a component and
            // name.
            //

            Character = *Current;
            if (Argument != NULL) {
                *Current = '\0';
            }

            Current += 1;
            if (Character != '=') {
                if (Information != NULL) {
                    Information->ArgumentCount += 1;
                }

                continue;
            }

            for (ValueIndex = 0;
                 ValueIndex < KERNEL_MAX_ARGUMENT_VALUES;
                 ValueIndex += 1) {

                if (Argument != NULL) {
                    Argument->Values[ValueIndex] = Current;
                }

                InQuote = FALSE;
                while (*Current != '\0') {
                    if (*Current == '"') {
                        InQuote = !InQuote;

                    } else if (InQuote == FALSE) {
                        if ((*Current == ',') ||
                            (RtlIsCharacterBlank(*Current) != FALSE)) {

                            break;
                        }
                    }

                    Current += 1;
                }

                if (*Current != ',') {
                    ValueIndex += 1;
                    break;
                }

                if (ValueIndex == KERNEL_MAX_ARGUMENT_VALUES - 1) {
                    if (Argument != NULL) {
                        RtlDebugPrint("Combining argument values starting at "
                                      "%s\n",
                                      Current);
                    }

                } else {
                    if (Argument != NULL) {
                        *Current = '\0';
                    }

                    Current += 1;
                }
            }

            if (Argument != NULL) {
                Argument->ValueCount = ValueIndex;
            }

            //
            // Get past any remaining non-blanks.
            //

            while ((*Current != '\0') &&
                   (RtlIsCharacterBlank(*Current) == FALSE)) {

                Current += 1;
            }

            if (Argument != NULL) {
                *Current = '\0';
                Argument += 1;
            }

            ArgumentCount += 1;
        }

        if (Pass != 0) {
            break;
        }

        //
        // Allocate the complete structure, which includes the main structure,
        // element for each argument, and a complete copy of the string which
        // will be chopped up.
        //

        AllocationSize = sizeof(KERNEL_COMMAND_LINE) +
                         (ArgumentCount * sizeof(KERNEL_ARGUMENT)) +
                         StringSize;

        Information = MmAllocateNonPagedPool(AllocationSize, KE_ALLOCATION_TAG);
        if (Information == NULL) {
            MmFreeNonPagedPool(Line);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(Information, AllocationSize);
        Information->Line = Line;
        Information->LineSize = StringSize;
        Information->Arguments = (PKERNEL_ARGUMENT)(Information + 1);
        Information->ArgumentCount = ArgumentCount;
        LineCopy = (PSTR)(Information->Arguments + ArgumentCount);
        RtlCopyMemory(LineCopy, Line, StringSize);
        Argument = &(Information->Arguments[0]);
        Current = LineCopy;
        ArgumentCount = 0;
    }

    ASSERT(ArgumentCount == Information->ArgumentCount);

    KeCommandLine = Information;
    return STATUS_SUCCESS;
}

