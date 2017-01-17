/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    init.c

Abstract:

    This module initializes the process and thread subsystem.

Author:

    Evan Green 6-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/lib/bconf.h>
#include <minoca/kernel/bootload.h>
#include "psp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define IDLE_THREAD_NAME_LENGTH 10

//
// Define the initialization file, the contents of which are run as the first
// user mode process.
//

#define INITIALIZATION_COMMAND_FILE "config/init.set"

//
// Define the location of the OS base library, loaded into every user
// application.
//

#define SYSTEM_OS_BASE_LIBRARY_PATH "system/" OS_BASE_LIBRARY

//
// Define the initially enforced maximum number of files.
//

#define INITIAL_MAX_FILE_COUNT 1024

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PspAddIdleThread (
    PKPROCESS KernelProcess,
    PVOID IdleThreadStackBase,
    ULONG IdleThreadStackSize
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

extern LIST_ENTRY PsProcessGroupList;

PROCESS_GROUP PsKernelProcessGroup;

//
// Define the path from the system volume to the system directory. Set it to a
// default in case there is no boot entry (which there should really always be).
//

PSTR PsSystemDirectoryPath = "minoca";

//
// Store the initial resource limits to set.
//

RESOURCE_LIMIT PsInitialResourceLimits[ResourceLimitCount] = {
    {0, RESOURCE_LIMIT_INFINITE}, // ResourceLimitCore
    {RESOURCE_LIMIT_INFINITE, RESOURCE_LIMIT_INFINITE}, // ResourceLimitCpuTime
    {RESOURCE_LIMIT_INFINITE, RESOURCE_LIMIT_INFINITE}, // ResourceLimitData
    {RESOURCE_LIMIT_INFINITE, RESOURCE_LIMIT_INFINITE}, // ResourceLimitFileSize
    {INITIAL_MAX_FILE_COUNT, OB_MAX_HANDLES}, // ResourceLimitFileCount
    {DEFAULT_USER_STACK_SIZE, RESOURCE_LIMIT_INFINITE}, // ResourceLimitStack
    {RESOURCE_LIMIT_INFINITE, RESOURCE_LIMIT_INFINITE}, // AddressSpace
    {RESOURCE_LIMIT_INFINITE, RESOURCE_LIMIT_INFINITE}, // ProcessCount
    {RESOURCE_LIMIT_INFINITE, RESOURCE_LIMIT_INFINITE}, // ResourceLimitSignals
    {0, 0} // ResourceLimitNice
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
PsInitialize (
    ULONG Phase,
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    PVOID IdleThreadStackBase,
    ULONG IdleThreadStackSize
    )

/*++

Routine Description:

    This routine initializes the process and thread subsystem.

Arguments:

    Phase - Supplies the initialization phase. Valid values are 0 and 1.

    Parameters - Supplies an optional pointer to the kernel initialization
        block. It's only required for processor 0.

    IdleThreadStackBase - Supplies the base of the stack for the one thread
        currently running.

    IdleThreadStackSize - Supplies the size of the stack for the one thread
        currently running.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory for the kernel process or thread
        could not be allocated.

--*/

{

    PBOOT_ENTRY BootEntry;
    PSTR KernelBinaryName;
    ULONG KernelBinaryNameSize;
    PSTR KernelNameCopy;
    PKPROCESS KernelProcess;
    RUNLEVEL OldRunLevel;
    ULONG Processor;
    KSTATUS Status;
    UINTN SystemDirectorySize;

    //
    // If this is the boot processor, initialize PS structures.
    //

    Processor = KeGetCurrentProcessorNumber();
    if (Phase == 0) {
        if (Processor == 0) {
            INITIALIZE_LIST_HEAD(&PsProcessListHead);
            PsProcessCount = 0;
            PsProcessListLock = KeCreateQueuedLock();
            if (PsProcessListLock == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto InitializeEnd;
            }

            Status = PspInitializeProcessGroupSupport();
            if (!KSUCCESS(Status)) {
                goto InitializeEnd;
            }

            //
            // Create the process object directory.
            //

            PsProcessDirectory = ObCreateObject(ObjectDirectory,
                                                NULL,
                                                "Process",
                                                sizeof("Process"),
                                                sizeof(OBJECT_HEADER),
                                                NULL,
                                                OBJECT_FLAG_USE_NAME_DIRECTLY,
                                                PS_ALLOCATION_TAG);

            if (PsProcessDirectory == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto InitializeEnd;
            }

            //
            // Create the kernel process.
            //

            KernelBinaryName = Parameters->KernelModule->BinaryName;
            KernelBinaryNameSize = RtlStringLength(KernelBinaryName) + 1;
            KernelNameCopy = MmAllocateNonPagedPool(KernelBinaryNameSize,
                                                    PS_ALLOCATION_TAG);

            if (KernelNameCopy == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto InitializeEnd;
            }

            RtlStringCopy(KernelNameCopy,
                          KernelBinaryName,
                          KernelBinaryNameSize);

            KernelProcess = PspCreateProcess(KernelNameCopy,
                                             KernelBinaryNameSize,
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL);

            if (KernelProcess == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto InitializeEnd;
            }

            Status = PspInitializeUtsRealm(KernelProcess);
            if (!KSUCCESS(Status)) {
                goto InitializeEnd;
            }

            PsKernelProcess = KernelProcess;

            //
            // Copy the system directory path.
            //

            BootEntry = Parameters->BootEntry;
            if ((BootEntry != NULL) && (BootEntry->SystemPath != NULL)) {
                SystemDirectorySize =
                                    RtlStringLength(BootEntry->SystemPath) + 1;

                PsSystemDirectoryPath = MmAllocateNonPagedPool(
                                                           SystemDirectorySize,
                                                           PS_ALLOCATION_TAG);

                if (PsSystemDirectoryPath == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto InitializeEnd;
                }

                RtlStringCopy(PsSystemDirectoryPath,
                              BootEntry->SystemPath,
                              SystemDirectorySize);
            }

            PspInitializeUserLocking();

        } else {
            KernelProcess = PsKernelProcess;
        }

        //
        // Create the idle thread for this processor.
        //

        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        Status = PspAddIdleThread(KernelProcess,
                                  IdleThreadStackBase,
                                  IdleThreadStackSize);

        KeLowerRunLevel(OldRunLevel);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        //
        // Perform one-time thread initialization.
        //

        if (Processor == 0) {
            Status = PspInitializeThreadSupport();
            if (!KSUCCESS(Status)) {
                goto InitializeEnd;
            }

            //
            // Give the kernel process it's own session (and process group).
            //

            ASSERT(KernelProcess->Identifiers.ProcessId == 0);

            PsKernelProcessGroup.ReferenceCount = 1;
            INITIALIZE_LIST_HEAD(&(PsKernelProcessGroup.ProcessListHead));
            INSERT_BEFORE(&(PsKernelProcessGroup.ListEntry),
                          &PsProcessGroupList);
        }

    //
    // In phase 1, set up image support.
    //

    } else {

        ASSERT(Phase == 1);
        ASSERT(Processor == 0);

        Status = PspInitializeImageSupport(
                                       Parameters->KernelModule->LowestAddress,
                                       &(Parameters->ImageList));

        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }
    }

    Status = STATUS_SUCCESS;

InitializeEnd:
    return Status;
}

VOID
PsVolumeArrival (
    PCSTR VolumeName,
    ULONG VolumeNameLength,
    BOOL SystemVolume
    )

/*++

Routine Description:

    This routine implements actions that the process library takes in response
    to a new volume arrival.

Arguments:

    VolumeName - Supplies the full path to the new volume.

    VolumeNameLength - Supplies the length of the volume name buffer, including
        the null terminator, in bytes.

    SystemVolume - Supplies a boolean indicating whether or not this is the
        system volume.

Return Value:

    None.

--*/

{

    UINTN BytesRead;
    PSTR Command;
    PIO_HANDLE File;
    ULONGLONG FileSize;
    IO_BUFFER IoBuffer;
    PKPROCESS Process;
    KSTATUS Status;
    PIO_HANDLE SystemDirectory;
    PIO_HANDLE Volume;

    Command = NULL;
    File = NULL;
    SystemDirectory = NULL;
    Volume = NULL;

    //
    // Do nothing unless this is the system volume.
    //

    if (SystemVolume == FALSE) {
        Status = STATUS_SUCCESS;
        goto VolumeArrivalEnd;
    }

    //
    // Copy the system volume path. Synchronization would be needed if this
    // path changes.
    //

    ASSERT(VolumeNameLength != 0);

    Status = IoOpen(TRUE,
                    NULL,
                    VolumeName,
                    VolumeNameLength,
                    IO_ACCESS_READ,
                    OPEN_FLAG_DIRECTORY,
                    0,
                    &Volume);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to open system volume: %d\n", Status);
        goto VolumeArrivalEnd;
    }

    //
    // Attempt to open the system directory.
    //

    Status = IoOpen(TRUE,
                    Volume,
                    PsSystemDirectoryPath,
                    RtlStringLength(PsSystemDirectoryPath) + 1,
                    IO_ACCESS_READ,
                    OPEN_FLAG_DIRECTORY,
                    0,
                    &SystemDirectory);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to open system directory '%s': %d\n",
                      PsSystemDirectoryPath,
                      Status);

        goto VolumeArrivalEnd;
    }

    //
    // Attempt to open the OS base library.
    //

    Status = IoOpen(TRUE,
                    SystemDirectory,
                    SYSTEM_OS_BASE_LIBRARY_PATH,
                    sizeof(SYSTEM_OS_BASE_LIBRARY_PATH),
                    IO_ACCESS_READ | IO_ACCESS_EXECUTE,
                    0,
                    FILE_PERMISSION_NONE,
                    &PsOsBaseLibrary);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to open OS base library '%s/%s': %d\n",
                      PsSystemDirectoryPath,
                      SYSTEM_OS_BASE_LIBRARY_PATH,
                      Status);

        goto VolumeArrivalEnd;
    }

    //
    // Attempt to open the initialization command file.
    //

    Status = IoOpen(TRUE,
                    SystemDirectory,
                    INITIALIZATION_COMMAND_FILE,
                    sizeof(INITIALIZATION_COMMAND_FILE),
                    IO_ACCESS_READ,
                    0,
                    FILE_PERMISSION_NONE,
                    &File);

    if (!KSUCCESS(Status)) {
        goto VolumeArrivalEnd;
    }

    Status = IoGetFileSize(File, &FileSize);
    if (!KSUCCESS(Status)) {
        goto VolumeArrivalEnd;
    }

    if (FileSize > (UINTN)FileSize) {
        Status = STATUS_BUFFER_OVERRUN;
        goto VolumeArrivalEnd;
    }

    //
    // Create an I/O buffer from paged pool so that a contiguous virtual buffer
    // can be supplied to process creation.
    //

    Command = MmAllocatePagedPool(FileSize, PS_ALLOCATION_TAG);
    if (Command == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto VolumeArrivalEnd;
    }

    Status = MmInitializeIoBuffer(&IoBuffer,
                                  Command,
                                  INVALID_PHYSICAL_ADDRESS,
                                  FileSize,
                                  IO_BUFFER_FLAG_KERNEL_MODE_DATA);

    if (!KSUCCESS(Status)) {
        goto VolumeArrivalEnd;
    }

    Status = IoRead(File,
                    &IoBuffer,
                    (UINTN)FileSize,
                    0,
                    WAIT_TIME_INDEFINITE,
                    &BytesRead);

    if (!KSUCCESS(Status)) {
        goto VolumeArrivalEnd;
    }

    if (BytesRead != FileSize) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto VolumeArrivalEnd;
    }

    Command[FileSize] = '\0';

    //
    // Fire up the process.
    //

    Process = PsCreateProcess(Command,
                              (ULONG)FileSize + 1,
                              NULL,
                              IoGetPathPoint(Volume),
                              NULL);

    if (Process == NULL) {
        RtlDebugPrint("Failed to create initial process: \"%s\"\n",
                      Command);

        goto VolumeArrivalEnd;
    }

    //
    // Release the reference on the process, as no one is waiting around for
    // its completion.
    //

    ObReleaseReference(Process);

VolumeArrivalEnd:
    if (Volume != NULL) {
        IoClose(Volume);
    }

    if (SystemDirectory != NULL) {
        IoClose(SystemDirectory);
    }

    if (File != NULL) {
        IoClose(File);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PspAddIdleThread (
    PKPROCESS KernelProcess,
    PVOID IdleThreadStackBase,
    ULONG IdleThreadStackSize
    )

/*++

Routine Description:

    This routine adds the processor's initial thread to the thread accounting
    system.

Arguments:

    KernelProcess - Supplies a pointer to the system process.

    IdleThreadStackBase - Supplies the base of the stack for the one thread
        currently running on this processor.

    IdleThreadStackSize - Supplies the size of the stack for the one thread
        currently running on this processor.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory for the thread could not be
        allocated.

--*/

{

    PKTHREAD CurrentThread;
    CHAR Name[IDLE_THREAD_NAME_LENGTH];
    PPROCESSOR_BLOCK Processor;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() >= RunLevelDispatch);

    CurrentThread = NULL;
    Processor = KeGetCurrentProcessorBlock();
    RtlPrintToString(Name,
                     IDLE_THREAD_NAME_LENGTH,
                     CharacterEncodingDefault,
                     "Idle%d",
                     Processor->ProcessorNumber);

    //
    // Manually create the idle thread. Locks don't need to be acquired here
    // because preemption has not yet been turned on.
    //

    CurrentThread = ObCreateObject(ObjectThread,
                                   KernelProcess,
                                   Name,
                                   IDLE_THREAD_NAME_LENGTH,
                                   sizeof(KTHREAD),
                                   NULL,
                                   0,
                                   PS_ALLOCATION_TAG);

    if (CurrentThread == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddIdleThreadEnd;
    }

    //
    // Initialize pieces of the thread.
    //

    CurrentThread->OwningProcess = KernelProcess;
    CurrentThread->ThreadId = RtlAtomicAdd32((PULONG)&PsNextThreadId, 1);
    CurrentThread->KernelStack = IdleThreadStackBase;
    CurrentThread->KernelStackSize = IdleThreadStackSize;
    CurrentThread->State = ThreadStateRunning;
    CurrentThread->SchedulerEntry.Type = SchedulerEntryThread;
    CurrentThread->SchedulerEntry.Parent = &(Processor->Scheduler.Group.Entry);
    CurrentThread->ThreadPointer = PsInitialThreadPointer;
    CurrentThread->BuiltinWaitBlock = ObCreateWaitBlock(0);
    if (CurrentThread->BuiltinWaitBlock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddIdleThreadEnd;
    }

    INSERT_BEFORE(&(CurrentThread->ProcessEntry),
                  &(KernelProcess->ThreadListHead));

    KernelProcess->ThreadCount += 1;

    //
    // Make this initial thread all-powerful.
    //

    CurrentThread->Permissions.Limit = PERMISSION_SET_FULL;
    CurrentThread->Permissions.Permitted = PERMISSION_SET_FULL;
    CurrentThread->Permissions.Inheritable = PERMISSION_SET_FULL;
    CurrentThread->Permissions.Effective = PERMISSION_SET_FULL;
    RtlCopyMemory(&(CurrentThread->Limits),
                  PsInitialResourceLimits,
                  sizeof(CurrentThread->Limits));

    //
    // Again, it's okay not to raise the run-level because preemption is not
    // yet enabled in the system.
    //

    Processor->RunningThread = CurrentThread;

    //
    // Set this thread as the idle thread.
    //

    Processor->IdleThread = CurrentThread;
    Status = STATUS_SUCCESS;

AddIdleThreadEnd:
    if (!KSUCCESS(Status)) {
        if (CurrentThread != NULL) {
            if (CurrentThread->BuiltinTimer != NULL) {
                KeDestroyTimer(CurrentThread->BuiltinTimer);
            }

            if (CurrentThread->BuiltinWaitBlock != NULL) {
                ObDestroyWaitBlock(CurrentThread->BuiltinWaitBlock);
            }

            ObReleaseReference(CurrentThread);
        }
    }

    return Status;
}

