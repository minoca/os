/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    thread.c

Abstract:

    This module implements support for threads in the kernel.

Author:

    Evan Green 6-Aug-2012

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

//
// Define the number of times to try and get the thread list.
//

#define THREAD_LIST_TRIES 100

//
// Define the fudge factor to add to the reported allocation to account for
// new threads sneaking in between calls.
//

#define THREAD_LIST_FUDGE_FACTOR 2

//
// Define the number of threads the reaper attempts to clean up in one pass.
//

#define THREAD_DEFAULT_REAP_COUNT 16

//
// Define the number of dead threads that are allowed to sit on the dead
// threads list before thread creation starts to kick in helping to destroy
// threads.
//

#define THREAD_CREATE_DEAD_THREAD_THRESHOLD 50

//
// Define the number of threads that thread creation will reap if the number of
// dead threads has exceeded the threshold.
//

#define THREAD_CREATE_REAP_COUNT 2

//
// ----------------------------------------------- Internal Function Prototypes
//

PKTHREAD
PspCreateThread (
    PKPROCESS OwningProcess,
    ULONG KernelStackSize,
    PTHREAD_ENTRY_ROUTINE ThreadRoutine,
    PVOID ThreadParameter,
    PCSTR Name,
    ULONG Flags
    );

VOID
PspReaperThread (
    PVOID Parameter
    );

VOID
PspReapThreads (
    UINTN ReapCount
    );

VOID
PspDestroyThread (
    PVOID ThreadObject
    );

KSTATUS
PspGetThreadList (
    PROCESS_ID ProcessId,
    PVOID Buffer,
    PULONG BufferSize
    );

KSTATUS
PspGetThreadInformation (
    PKTHREAD Thread,
    PTHREAD_INFORMATION Buffer,
    PULONG BufferSize
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Globals related to thread manipulation.
//

//
// Stores the ID for the next thread to be created.
//

volatile THREAD_ID PsNextThreadId = 0;

//
// Stores the list of exited threads waiting to be cleaned up.
//

KSPIN_LOCK PsDeadThreadsLock;
LIST_ENTRY PsDeadThreadsListHead;
UINTN PsDeadThreadsCount;
PKEVENT PsDeadThreadsEvent;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
PsCreateKernelThread (
    PTHREAD_ENTRY_ROUTINE ThreadRoutine,
    PVOID ThreadParameter,
    PCSTR Name
    )

/*++

Routine Description:

    This routine creates and launches a new kernel thread with default
    parameters.

Arguments:

    ThreadRoutine - Supplies the entry point to the thread.

    ThreadParameter - Supplies the parameter to pass to the entry point
        routine.

    Name - Supplies an optional name to identify the thread.

Return Value:

    Returns a pointer to the new thread on success, or NULL on failure.

--*/

{

    THREAD_CREATION_PARAMETERS Parameters;

    RtlZeroMemory(&Parameters, sizeof(THREAD_CREATION_PARAMETERS));
    Parameters.Name = Name;
    if (Name != NULL) {
        Parameters.NameSize = RtlStringLength(Name) + 1;
    }

    Parameters.ThreadRoutine = ThreadRoutine;
    Parameters.Parameter = ThreadParameter;
    return PsCreateThread(&Parameters);
}

KERNEL_API
KSTATUS
PsCreateThread (
    PTHREAD_CREATION_PARAMETERS Parameters
    )

/*++

Routine Description:

    This routine creates and initializes a new thread, and adds it to the
    ready list for execution.

Arguments:

    Parameters - Supplies a pointer to the thread creation parameters.

Return Value:

    Status code.

--*/

{

    PKTHREAD CurrentThread;
    UINTN KernelStackSize;
    PKTHREAD NewThread;
    BOOL ParameterIsStack;
    KSTATUS Status;
    PPROCESS_ENVIRONMENT UserEnvironment;

    KernelStackSize = 0;
    if ((Parameters->Flags & THREAD_FLAG_USER_MODE) == 0) {
        KernelStackSize = Parameters->StackSize;
    }

    NewThread = PspCreateThread(Parameters->Process,
                                KernelStackSize,
                                Parameters->ThreadRoutine,
                                Parameters->Parameter,
                                Parameters->Name,
                                Parameters->Flags);

    if (NewThread == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto CreateThreadEnd;
    }

    if (Parameters->ThreadIdPointer != NULL) {
        if ((PVOID)(Parameters->ThreadIdPointer) >= KERNEL_VA_START) {
            *(Parameters->ThreadIdPointer) = NewThread->ThreadId;

        } else {

            //
            // Save the new ID to user mode, and remember this as the thread
            // ID pointer.
            //

            MmUserWrite32(Parameters->ThreadIdPointer, NewThread->ThreadId);
            NewThread->ThreadIdPointer = Parameters->ThreadIdPointer;
        }
    }

    ArSetThreadPointer(NewThread, Parameters->ThreadPointer);

    //
    // Copy the thread permissions and identity from the current thread.
    //

    CurrentThread = KeGetCurrentThread();
    Status = PspCopyThreadCredentials(NewThread, CurrentThread);
    if (!KSUCCESS(Status)) {
        goto CreateThreadEnd;
    }

    //
    // Create the user mode stack if needed.
    //

    ParameterIsStack = FALSE;
    if ((Parameters->Flags & THREAD_FLAG_USER_MODE) != 0) {
        if (Parameters->UserStack == NULL) {
            NewThread->Flags |= THREAD_FLAG_FREE_USER_STACK;
            if (Parameters->StackSize == 0) {
                Parameters->StackSize =
                                 NewThread->Limits[ResourceLimitStack].Current;
            }

            Status = PspSetThreadUserStackSize(NewThread,
                                               Parameters->StackSize);

            if (!KSUCCESS(Status)) {
                goto CreateThreadEnd;
            }

            Parameters->UserStack = NewThread->UserStack;

        } else {
            NewThread->UserStack = Parameters->UserStack;
            NewThread->UserStackSize = Parameters->StackSize;
        }

        //
        // Copy the signal mask from the current thread.
        //

        NewThread->BlockedSignals = CurrentThread->BlockedSignals;

        //
        // Set up the environment if there is one.
        //

        if (Parameters->Environment != NULL) {
            ParameterIsStack = TRUE;
            Parameters->Environment->StartData->StackBase =
                                                          NewThread->UserStack;

            Status = PsCopyEnvironment(Parameters->Environment,
                                       &UserEnvironment,
                                       FALSE,
                                       NewThread,
                                       NULL,
                                       0);

            if (!KSUCCESS(Status)) {
                goto CreateThreadEnd;
            }
        }
    }

    PspPrepareThreadForFirstRun(NewThread, NULL, ParameterIsStack);

    //
    // Insert the thread onto the ready list.
    //

    KeSetThreadReady(NewThread);
    Status = STATUS_SUCCESS;

CreateThreadEnd:
    if (!KSUCCESS(Status)) {
        if (NewThread != NULL) {
            PspSetThreadUserStackSize(NewThread, 0);
            PspDestroyCredentials(NewThread);
            KeAcquireQueuedLock(NewThread->OwningProcess->QueuedLock);
            LIST_REMOVE(&(NewThread->ProcessEntry));
            NewThread->ProcessEntry.Next = NULL;
            NewThread->OwningProcess->ThreadCount -= 1;

            ASSERT(NewThread->OwningProcess->ThreadCount != 0);

            KeReleaseQueuedLock(NewThread->OwningProcess->QueuedLock);
            ObReleaseReference(NewThread);
        }
    }

    return Status;
}

KSTATUS
PsGetThreadList (
    PROCESS_ID ProcessId,
    ULONG AllocationTag,
    PVOID *Buffer,
    PULONG BufferSize
    )

/*++

Routine Description:

    This routine returns information about the active processes in the system.

Arguments:

    ProcessId - Supplies the identifier of the process to get thread
        information for.

    AllocationTag - Supplies the allocation tag to use for the allocation
        this routine will make on behalf of the caller.

    Buffer - Supplies a pointer where a non-paged pool buffer will be returned
        containing the array of thread information. The caller is responsible
        for freeing this memory from non-paged pool. The type returned here
        will be an array (where each element may be a different size) of
        THREAD_INFORMATION structures.

    BufferSize - Supplies a pointer where the size of the buffer in bytes
        will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
    information buffer.

    STATUS_BUFFER_TOO_SMALL if the thread list is so volatile that it cannot
    be sized. This is only returned in extremely rare cases, as this routine
    makes multiple attempts.

--*/

{

    PVOID Allocation;
    ULONG Size;
    KSTATUS Status;
    ULONG Try;

    Allocation = NULL;
    Size = 0;
    Status = STATUS_BUFFER_TOO_SMALL;
    for (Try = 0; Try < THREAD_LIST_TRIES; Try += 1) {
        Status = PspGetThreadList(ProcessId, NULL, &Size);
        if (!KSUCCESS(Status)) {
            goto GetThreadListEnd;
        }

        ASSERT(Size != 0);

        Size = Size * THREAD_LIST_FUDGE_FACTOR;
        Allocation = MmAllocateNonPagedPool(Size, AllocationTag);
        if (Allocation == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetThreadListEnd;
        }

        Status = PspGetThreadList(ProcessId, Allocation, &Size);
        if (KSUCCESS(Status)) {
            break;
        }

        MmFreeNonPagedPool(Allocation);
        Allocation = NULL;
    }

GetThreadListEnd:
    if (!KSUCCESS(Status)) {
        if (Allocation != NULL) {
            MmFreeNonPagedPool(Allocation);
            Allocation = NULL;
        }

        Size = 0;
    }

    *Buffer = Allocation;
    *BufferSize = Size;
    return Status;
}

KSTATUS
PsGetThreadInformation (
    PROCESS_ID ProcessId,
    THREAD_ID ThreadId,
    PTHREAD_INFORMATION Buffer,
    PULONG BufferSize
    )

/*++

Routine Description:

    This routine returns information about a given thread.

Arguments:

    ProcessId - Supplies the process ID owning the thread.

    ThreadId - Supplies the ID of the thread to get information about.

    Buffer - Supplies an optional pointer to a buffer to write the data into.
        This must be non-paged memory if the thread requested belongs to the
        kernel process.

    BufferSize - Supplies a pointer that on input contains the size of the
        input buffer. On output, returns the size needed to contain the data.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_PROCESS if no process with the given identifier exists.

    STATUS_NO_SUCH_THREAD if the no thread with the given identifier exists.

    STATUS_BUFFER_TOO_SMALL if a buffer was supplied but was not big enough to
    contain all the information.

--*/

{

    PKPROCESS Process;
    KSTATUS Status;
    PKTHREAD Thread;

    Thread = NULL;
    Process = PspGetProcessById(ProcessId);
    if (Process == NULL) {
        Status = STATUS_NO_SUCH_PROCESS;
        goto GetThreadInformationEnd;
    }

    Thread = PspGetThreadById(Process, ThreadId);
    if (Thread == NULL) {
        Status = STATUS_NO_SUCH_THREAD;
        goto GetThreadInformationEnd;
    }

    Status = PspGetThreadInformation(Thread, Buffer, BufferSize);
    if (!KSUCCESS(Status)) {
        goto GetThreadInformationEnd;
    }

GetThreadInformationEnd:
    if (Process != NULL) {
        ObReleaseReference(Process);
    }

    if (Thread != NULL) {
        ObReleaseReference(Thread);
    }

    return Status;
}

INTN
PsSysCreateThread (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine creates a new thread for the current process.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PKPROCESS CurrentProcess;
    PSTR Name;
    PSYSTEM_CALL_CREATE_THREAD Parameters;
    KSTATUS Status;
    THREAD_CREATION_PARAMETERS ThreadParameters;

    CurrentProcess = PsGetCurrentProcess();

    ASSERT(CurrentProcess != PsGetKernelProcess());

    Name = NULL;
    Parameters = (PSYSTEM_CALL_CREATE_THREAD)SystemCallParameter;
    if ((Parameters->Name != NULL) && (Parameters->NameBufferLength != 0)) {
        Status = MmCreateCopyOfUserModeString(Parameters->Name,
                                              Parameters->NameBufferLength,
                                              PS_ALLOCATION_TAG,
                                              &Name);

        if (!KSUCCESS(Status)) {
            goto SysCreateThreadEnd;
        }
    }

    //
    // Enable locking on the handle table, which will exist for the remainder
    // of the process lifetime.
    //

    Status = ObEnableHandleTableLocking(CurrentProcess->HandleTable);
    if (!KSUCCESS(Status)) {
        goto SysCreateThreadEnd;
    }

    //
    // Create and launch the thread.
    //

    RtlZeroMemory(&ThreadParameters, sizeof(THREAD_CREATION_PARAMETERS));
    ThreadParameters.Name = Parameters->Name;
    ThreadParameters.NameSize = Parameters->NameBufferLength;
    ThreadParameters.ThreadRoutine = Parameters->ThreadRoutine;
    ThreadParameters.Parameter = Parameters->Parameter;
    ThreadParameters.UserStack = Parameters->StackBase;
    ThreadParameters.StackSize = Parameters->StackSize;
    ThreadParameters.Flags = THREAD_FLAG_USER_MODE;
    ThreadParameters.ThreadPointer = Parameters->ThreadPointer;
    ThreadParameters.ThreadIdPointer = Parameters->ThreadId;
    if ((PVOID)(ThreadParameters.ThreadIdPointer) >= KERNEL_VA_START) {
        Status = STATUS_ACCESS_VIOLATION;
        goto SysCreateThreadEnd;
    }

    Status = PsCreateThread(&ThreadParameters);
    if (!KSUCCESS(Status)) {
        goto SysCreateThreadEnd;
    }

    //
    // Update the stack base and size on output.
    //

    Parameters->StackBase = ThreadParameters.UserStack;
    Parameters->StackSize = ThreadParameters.StackSize;

    //
    // Null out the name parameters as that memory is now owned by the object
    // manager.
    //

    Name = NULL;
    Status = STATUS_SUCCESS;

SysCreateThreadEnd:
    if (Name != NULL) {
        MmFreePagedPool(Name);
    }

    return Status;
}

INTN
PsSysExitThread (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine terminates the current thread.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    Does not return. Eventually exits by killing the thread.

--*/

{

    PSYSTEM_CALL_EXIT_THREAD Parameters;
    PKTHREAD Thread;
    PVOID ThreadIdPointer;

    Parameters = SystemCallParameter;

    //
    // Before killing the thread, unmap a region if requested. This is used by
    // the user-modes thread library to clean up the stack for the thread that
    // just exited.
    //

    if ((Parameters->UnmapSize != 0) && (Parameters->UnmapAddress != NULL)) {

        //
        // Clear the thread ID pointer if it's in the unmap region. This saves
        // the C library a system call.
        //

        Thread = KeGetCurrentThread();
        ThreadIdPointer = Thread->ThreadIdPointer;
        if ((ThreadIdPointer >= Parameters->UnmapAddress) &&
            (ThreadIdPointer <
             Parameters->UnmapAddress + Parameters->UnmapSize)) {

            Thread->ThreadIdPointer = NULL;
        }

        MmUnmapFileSection(PsGetCurrentProcess(),
                           Parameters->UnmapAddress,
                           Parameters->UnmapSize,
                           NULL);
    }

    PspThreadTermination();

    //
    // Execution should never get here.
    //

    ASSERT(FALSE);

    return STATUS_UNSUCCESSFUL;
}

INTN
PsSysSetThreadPointer (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine sets the thread pointer for the current thread.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This is supplies the thread pointer directly, which is
        passed from user mode via a register.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    ArSetThreadPointer(KeGetCurrentThread(), SystemCallParameter);
    return STATUS_SUCCESS;
}

INTN
PsSysSetThreadIdPointer (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine sets the thread ID pointer for the current thread.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This is supplies the thread ID pointer directly, which
        is passed from user mode via a register.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PVOID Pointer;
    PKTHREAD Thread;

    Pointer = SystemCallParameter;
    Thread = KeGetCurrentThread();
    if (Pointer < KERNEL_VA_START) {
        Thread->ThreadIdPointer = Pointer;

        //
        // As a convenience, also set the thread ID if the pointer is being set
        // to a new value. This is useful when the executable becomes
        // multithreaded and the main thread needs to catch up setting up a
        // thread structure.
        //

        if (Pointer != NULL) {
            MmUserWrite32(Pointer, Thread->ThreadId);
        }
    }

    return STATUS_SUCCESS;
}

VOID
PsQueueThreadCleanup (
    PKTHREAD Thread
    )

/*++

Routine Description:

    This routine queues the work item that cleans up a dead thread. This routine
    must not be executed by the thread being destroyed! This routine must be
    called at dispatch level.

Arguments:

    Thread - Supplies a pointer to the thread to clean up.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;

    ASSERT(KeGetCurrentThread() != Thread);

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&PsDeadThreadsLock);

    ASSERT(Thread->SchedulerEntry.ListEntry.Next == NULL);

    INSERT_AFTER(&(Thread->SchedulerEntry.ListEntry), &PsDeadThreadsListHead);
    PsDeadThreadsCount += 1;
    KeSignalEvent(PsDeadThreadsEvent, SignalOptionSignalAll);
    KeReleaseSpinLock(&PsDeadThreadsLock);
    KeLowerRunLevel(OldRunLevel);
    return;
}

KSTATUS
PspSetThreadUserStackSize (
    PKTHREAD Thread,
    UINTN NewStackSize
    )

/*++

Routine Description:

    This routine changes the given thread's user mode stack size.

Arguments:

    Thread - Supplies a pointer to the thread whose stack size should be
        changed.

    NewStackSize - Supplies the new stack size to set. If 0 is supplied, the
        user mode stack will be destroyed.

Return Value:

    Status code.

--*/

{

    ULONG Flags;
    PVOID MaxMapRegion;
    ULONG PageSize;
    KSTATUS Status;
    VM_ALLOCATION_PARAMETERS VaRequest;

    PageSize = MmPageSize();
    if (NewStackSize > USER_STACK_MAX) {
        NewStackSize = USER_STACK_MAX;
    }

    NewStackSize = ALIGN_RANGE_UP(NewStackSize, PageSize);

    //
    // Shrink or destroy the stack if requested. This whole routine assumes the
    // stack grows down.
    //

    if (NewStackSize <= Thread->UserStackSize) {
        Status = STATUS_SUCCESS;
        if ((Thread->UserStackSize != 0) &&
            (NewStackSize != Thread->UserStackSize)) {

            Status = MmUnmapFileSection(Thread->OwningProcess,
                                        Thread->UserStack,
                                        Thread->UserStackSize - NewStackSize,
                                        NULL);

            if (NewStackSize != 0) {
                Thread->UserStack += Thread->UserStackSize - NewStackSize;
            }

            Thread->UserStackSize = NewStackSize;
        }

        if (NewStackSize == 0) {
            Thread->UserStack = NULL;
            Thread->UserStackSize = 0;
        }

    //
    // Create or grow the stack.
    //

    } else {
        VaRequest.Address = NULL;
        VaRequest.Size = NewStackSize;
        VaRequest.Strategy = AllocationStrategyHighestAddress;
        if (Thread->UserStackSize != 0) {
            VaRequest.Strategy = AllocationStrategyFixedAddress;
            VaRequest.Address = Thread->UserStack + Thread->UserStackSize -
                                NewStackSize;

            if (VaRequest.Address > Thread->UserStack) {
                Status = STATUS_INTEGER_OVERFLOW;
                goto SetThreadUserStackSizeEnd;
            }

            VaRequest.Size = NewStackSize - Thread->UserStackSize;
        }

        ASSERT(Thread->UserStackSize == 0);
        ASSERT(Thread->UserStack == NULL);

        //
        // Check against the current resource limit.
        //

        if (NewStackSize > Thread->Limits[ResourceLimitStack].Current) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetThreadUserStackSizeEnd;
        }

        Flags = IMAGE_SECTION_READABLE | IMAGE_SECTION_WRITABLE;
        VaRequest.Alignment = 0;
        VaRequest.Min = 0;
        VaRequest.Max = MAX_ADDRESS;
        VaRequest.MemoryType = MemoryTypeReserved;
        Status = MmMapFileSection(INVALID_HANDLE,
                                  0,
                                  &VaRequest,
                                  Flags,
                                  FALSE,
                                  NULL);

        if (!KSUCCESS(Status)) {
            goto SetThreadUserStackSizeEnd;
        }

        //
        // If this was the first time the stack size has been set, save the
        // upper limit for memory mapped regions. This gives the stack some
        // breathing room to grow.
        //

        if ((Thread->UserStackSize == 0) &&
            (Thread->OwningProcess->AddressSpace->MaxMemoryMap == 0)) {

            MaxMapRegion = VaRequest.Address;
            if (NewStackSize < USER_STACK_HEADROOM) {
                MaxMapRegion = VaRequest.Address + NewStackSize -
                               USER_STACK_HEADROOM;
            }

            Thread->OwningProcess->AddressSpace->MaxMemoryMap = MaxMapRegion;
        }

        //
        // Don't free the stack for the first thread, as it contains the
        // environment and arguments. Do free it for all the other threads
        // that used the kernel to allocate a stack.
        //

        if (Thread->OwningProcess->ThreadCount > 1) {
            Thread->Flags |= THREAD_FLAG_FREE_USER_STACK;
        }

        Thread->UserStack = VaRequest.Address;
        Thread->UserStackSize = NewStackSize;
        Status = STATUS_SUCCESS;
    }

SetThreadUserStackSizeEnd:
    return Status;
}

VOID
PspKernelThreadStart (
    VOID
    )

/*++

Routine Description:

    This routine performs common initialization for all kernel mode threads, and
    executes the primary thread routine.

Arguments:

    None.

Return Value:

    Does not return. Eventually exits by killing the thread.

--*/

{

    PTHREAD_ENTRY_ROUTINE Entry;
    PKTHREAD Thread;

    //
    // Run the thread.
    //

    Thread = KeGetCurrentThread();
    Entry = Thread->ThreadRoutine;
    Entry(Thread->ThreadParameter);

    //
    // The thread returned, so exit.
    //

    PspThreadTermination();
    return;
}

KSTATUS
PspInitializeThreadSupport (
    VOID
    )

/*++

Routine Description:

    This routine performs one-time system initialization for thread support.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    ASSERT(KeGetCurrentProcessorNumber() == 0);
    ASSERT(PsDeadThreadsCount == 0);

    KeInitializeSpinLock(&PsDeadThreadsLock);
    INITIALIZE_LIST_HEAD(&PsDeadThreadsListHead);
    PsDeadThreadsEvent = KeCreateEvent(NULL);
    if (PsDeadThreadsEvent == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeThreadSupportEnd;
    }

    //
    // Create the reaper thread.
    //

    Status = PsCreateKernelThread(PspReaperThread, NULL, "PspReaperThread");
    if (!KSUCCESS(Status)) {
        goto InitializeThreadSupportEnd;
    }

    Status = STATUS_SUCCESS;

InitializeThreadSupportEnd:
    return Status;
}

PKTHREAD
PspCloneThread (
    PKPROCESS DestinationProcess,
    PKTHREAD Thread,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine clones a user mode thread from another process into the
    destination thread. This routine is designed to support the fork process
    system call.

Arguments:

    DestinationProcess - Supplies a pointer to the process the new thread
        should be created under.

    Thread - Supplies a pointer to the thread to clone.

    TrapFrame - Supplies a pointer to the trap frame to set initial thread
        state to. A copy of this trap frame will be made.

Return Value:

    Returns a pointer to the new thread on success, or NULL on failure.

--*/

{

    PKTHREAD NewThread;
    KSTATUS Status;

    NewThread = PspCreateThread(DestinationProcess,
                                Thread->KernelStackSize,
                                Thread->ThreadRoutine,
                                Thread->ThreadParameter,
                                Thread->Header.Name,
                                Thread->Flags & THREAD_FLAG_CREATION_MASK);

    if (NewThread == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto CloneThreadEnd;
    }

    //
    // Copy the existing thread's credentials to the new thread.
    //

    Status = PspCopyThreadCredentials(NewThread, Thread);
    if (!KSUCCESS(Status)) {
        goto CloneThreadEnd;
    }

    Status = PspArchCloneThread(Thread, NewThread);
    if (!KSUCCESS(Status)) {
        goto CloneThreadEnd;
    }

    //
    // The user stack is presumed to be set up in the new process at the same
    // place.
    //

    NewThread->BlockedSignals = Thread->BlockedSignals;
    NewThread->UserStack = Thread->UserStack;
    NewThread->UserStackSize = Thread->UserStackSize;
    PspPrepareThreadForFirstRun(NewThread, TrapFrame, FALSE);
    NewThread->ThreadPointer = Thread->ThreadPointer;
    NewThread->ThreadIdPointer = Thread->ThreadIdPointer;

    //
    // Insert the thread onto the ready list.
    //

    KeSetThreadReady(NewThread);
    Status = STATUS_SUCCESS;

CloneThreadEnd:
    if (!KSUCCESS(Status)) {
        if (NewThread != NULL) {

            ASSERT(NewThread->SupplementaryGroups == NULL);

            ObReleaseReference(NewThread);
            NewThread = NULL;
        }
    }

    return NewThread;
}

KSTATUS
PspResetThread (
    PKTHREAD Thread,
    PTRAP_FRAME TrapFrame,
    PINTN ReturnValue
    )

/*++

Routine Description:

    This routine resets a user mode thread. It assumes that the user mode stack
    was freed out from under it, and sets up a new stack.

Arguments:

    Thread - Supplies a pointer to the thread to reset. The thread must be a
        user mode thread. A new user mode stack will be allocated for it, the
        old one will not be freed. Commonly, this parameter will be a pointer
        to the currently running thread.

    TrapFrame - Supplies a pointer to the initial trap frame to reset the thread
        to.

    ReturnValue - Supplies a pointer that receives the value that the reset
        user mode thread should return when exiting back to user mode.

Return Value:

    Status code.

--*/

{

    PPROCESS_ENVIRONMENT Environment;
    KSTATUS Status;
    PPROCESS_ENVIRONMENT UserEnvironment;

    //
    // Create the user mode stack.
    //

    ASSERT((Thread->Flags & THREAD_FLAG_USER_MODE) != 0);

    Thread->ThreadIdPointer = NULL;
    Status = PspSetThreadUserStackSize(
                                   Thread,
                                   Thread->Limits[ResourceLimitStack].Current);

    if (!KSUCCESS(Status)) {
        goto CreateThreadEnd;
    }

    Thread->ThreadParameter = NULL;
    Environment = Thread->OwningProcess->Environment;
    Environment->StartData->StackBase = Thread->UserStack;
    Status = PsCopyEnvironment(Environment,
                               &UserEnvironment,
                               FALSE,
                               Thread,
                               NULL,
                               0);

    if (!KSUCCESS(Status)) {
        goto CreateThreadEnd;
    }

    *ReturnValue = PspArchResetThreadContext(Thread, TrapFrame);
    Status = STATUS_SUCCESS;

CreateThreadEnd:
    if (!KSUCCESS(Status)) {
        PspSetThreadUserStackSize(Thread, 0);
    }

    return Status;
}

PKTHREAD
PspGetThreadById (
    PKPROCESS Process,
    THREAD_ID ThreadId
    )

/*++

Routine Description:

    This routine returns the thread with the given thread ID under the given
    process. This routine also increases the reference count of the returned
    thread.

Arguments:

    Process - Supplies a pointer to the process to search under.

    ThreadId - Supplies the thread ID to search for.

Return Value:

    Returns a pointer to the thread with the corresponding ID. The reference
    count will be increased by one.

    NULL if no such thread could be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PKTHREAD FoundThread;
    PKTHREAD Thread;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    FoundThread = NULL;
    KeAcquireQueuedLock(Process->QueuedLock);
    CurrentEntry = Process->ThreadListHead.Next;
    while (CurrentEntry != &(Process->ThreadListHead)) {
        Thread = LIST_VALUE(CurrentEntry, KTHREAD, ProcessEntry);
        if (Thread->ThreadId == ThreadId) {
            FoundThread = Thread;
            ObAddReference(FoundThread);
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseQueuedLock(Process->QueuedLock);
    return FoundThread;
}

VOID
PspThreadTermination (
    VOID
    )

/*++

Routine Description:

    This routine is called when a thread finishes execution, it performs some
    cleanup and calls the scheduler to exit the thread. This routine runs in
    the context of the thread itself.

Arguments:

    None.

Return Value:

    Does not return. Eventually exits by killing the thread.

--*/

{

    PFPU_CONTEXT FpuContext;
    BOOL LastThread;
    PKPROCESS Process;
    PKTHREAD Thread;
    SYSTEM_CALL_USER_LOCK WakeOperation;

    LastThread = FALSE;
    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;

    //
    // Mark that the thread is exiting so that it does not get chosen for any
    // new signals.
    //

    Thread->Flags |= THREAD_FLAG_EXITING;

    //
    // Free the user mode stack before decrementing the thread count.
    //

    PspSetThreadUserStackSize(Thread, 0);

    //
    // Decrement the thread count. If this is the last thread, unload all
    // images in the process.
    //

    KeAcquireQueuedLock(Process->QueuedLock);

    ASSERT((Process->ThreadCount != 0) && (Process->ThreadCount < 0x10000000));

    Process->ThreadCount -= 1;
    if (Process->ThreadCount == 0) {

        //
        // The last thread shouldn't be exiting without having first set the
        // exit flags.
        //

        ASSERT((Process == PsGetKernelProcess()) || (Process->ExitReason != 0));

        LastThread = TRUE;
    }

    //
    // If a stop was requested and this thread happened to be the last one
    // being waited for, signal the all stopped event.
    //

    if ((Process->DebugData != NULL) &&
        (Process->ThreadCount != 0) &&
        (Process->StoppedThreadCount == Process->ThreadCount)) {

        KeSignalEvent(Process->DebugData->AllStoppedEvent,
                      SignalOptionSignalAll);
    }

    //
    // The thread may have been responsible for dispatching some signals. Pass
    // those on to other threads.
    //

    PspCleanupThreadSignals();
    KeReleaseQueuedLock(Process->QueuedLock);

    //
    // Wake any threads waiting on the thread ID address.
    //

    if ((LastThread == FALSE) && (Thread->ThreadIdPointer != NULL)) {

        ASSERT((PVOID)(Thread->ThreadIdPointer) < KERNEL_VA_START);

        ASSERT(sizeof(THREAD_ID) == sizeof(ULONG));

        MmUserWrite32(Thread->ThreadIdPointer, 0);
        WakeOperation.Address = (PULONG)(Thread->ThreadIdPointer);
        WakeOperation.Value = 1;
        WakeOperation.Operation = UserLockWake;
        WakeOperation.TimeoutInMilliseconds = 0;
        PspUserLockWake(&WakeOperation);
    }

    PspDestroyCredentials(Thread);

    //
    // Free up the FPU context. The thread could still get context swapped here,
    // which is why it's NULLed and then freed. The context swap code watches
    // out for this case where the using FPU flag is set but the context is
    // gone.
    //

    FpuContext = Thread->FpuContext;
    if (FpuContext != NULL) {
        Thread->FpuContext = NULL;
        ArDestroyFpuContext(FpuContext);
    }

    //
    // If this was the last thread in the process, clean up the dying process.
    //

    if (LastThread != FALSE) {
        PspProcessTermination(Process);
    }

    KeRaiseRunLevel(RunLevelDispatch);
    KeSchedulerEntry(SchedulerReasonThreadExiting);

    //
    // Execution should never get here.
    //

    KeCrashSystem(CRASH_THREAD_ERROR,
                  (UINTN)Thread,
                  Thread->State,
                  0,
                  0);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PKTHREAD
PspCreateThread (
    PKPROCESS OwningProcess,
    ULONG KernelStackSize,
    PTHREAD_ENTRY_ROUTINE ThreadRoutine,
    PVOID ThreadParameter,
    PCSTR Name,
    ULONG Flags
    )

/*++

Routine Description:

    This routine creates and initializes a new thread. It will not create a
    user mode stack.

Arguments:

    OwningProcess - Supplies a pointer to the process responsible for creating
        this thread.

    KernelStackSize - Supplies the initial size of the kernel mode stack, in
        bytes. Supply 0 to use a default size.

    ThreadRoutine - Supplies the entry point to thread.

    ThreadParameter - Supplies the parameter to pass to the entry point routine.

    Name - Supplies an optional name to identify the thread.

    Flags - Supplies a set of flags governing the behavior and characteristics
        of the thread. See THREAD_FLAG_* definitions.

Return Value:

    Returns a pointer to the new thread on success, or NULL on failure.

--*/

{

    PKTHREAD CurrentThread;
    ULONG NameLength;
    PKTHREAD NewThread;
    ULONG ObjectFlags;
    KSTATUS Status;
    BOOL UserMode;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((Flags & ~THREAD_FLAG_CREATION_MASK) == 0);

    //
    // Before creating a new thread, make sure there aren't too many dead
    // threads hanging around. If there are dead threads, attempt to help the
    // system out be reaping some before creating a new thread.
    //

    if (PsDeadThreadsCount > THREAD_CREATE_DEAD_THREAD_THRESHOLD) {
        PspReapThreads(THREAD_CREATE_REAP_COUNT);
    }

    CurrentThread = KeGetCurrentThread();
    UserMode = FALSE;
    if ((Flags & THREAD_FLAG_USER_MODE) != 0) {
        UserMode = TRUE;
    }

    if (KernelStackSize == 0) {
        KernelStackSize = DEFAULT_KERNEL_STACK_SIZE;
    }

    if (OwningProcess == NULL) {
        OwningProcess = CurrentThread->OwningProcess;
        if (UserMode == FALSE) {
            OwningProcess = PsKernelProcess;
        }
    }

    ASSERT((ThreadRoutine == NULL) ||
           ((UserMode == FALSE) && ((PVOID)ThreadRoutine >= KERNEL_VA_START)) ||
           ((UserMode != FALSE) && ((PVOID)ThreadRoutine < KERNEL_VA_START)));

    NameLength = 0;
    if (Name != NULL) {
        NameLength = RtlStringLength(Name) + 1;
    }

    //
    // Allocate the new thread's structure.
    //

    ObjectFlags = OBJECT_FLAG_USE_NAME_DIRECTLY;
    NewThread = ObCreateObject(ObjectThread,
                               OwningProcess,
                               Name,
                               NameLength,
                               sizeof(KTHREAD),
                               PspDestroyThread,
                               ObjectFlags,
                               PS_ALLOCATION_TAG);

    if (NewThread == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateThreadEnd;
    }

    INITIALIZE_LIST_HEAD(&(NewThread->SignalListHead));
    NewThread->OwningProcess = OwningProcess;
    NewThread->State = ThreadStateFirstTime;
    NewThread->KernelStackSize = KernelStackSize;
    NewThread->ThreadRoutine = ThreadRoutine;
    NewThread->ThreadParameter = ThreadParameter;
    NewThread->Flags = Flags;
    NewThread->SignalPending = ThreadNoSignalPending;
    NewThread->SchedulerEntry.Type = SchedulerEntryThread;
    NewThread->SchedulerEntry.Parent = CurrentThread->SchedulerEntry.Parent;
    NewThread->ThreadPointer = PsInitialThreadPointer;

    //
    // Allocate a kernel stack.
    //

    NewThread->KernelStack = MmAllocateKernelStack(KernelStackSize);
    if (NewThread->KernelStack == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateThreadEnd;
    }

    //
    // Create a timer to be used for most operations that can time out.
    //

    NewThread->BuiltinTimer = KeCreateTimer(PS_ALLOCATION_TAG);
    if (NewThread->BuiltinTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateThreadEnd;
    }

    //
    // Create a built in wait block for the thread.
    //

    NewThread->BuiltinWaitBlock = ObCreateWaitBlock(0);
    if (NewThread->BuiltinWaitBlock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateThreadEnd;
    }

    //
    // Update the page directory of the owning process to ensure the new stack
    // is visible to the process.
    //

    MmUpdatePageDirectory(OwningProcess->AddressSpace,
                          NewThread->KernelStack,
                          KernelStackSize);

    //
    // Additionally, if the owning process is not the current process, then
    // make sure the thread structure is visible to the new process. If the
    // owner is the current process then the thread was faulted in when it was
    // zero-initialized above.
    //

    if (OwningProcess != CurrentThread->OwningProcess) {
        MmUpdatePageDirectory(OwningProcess->AddressSpace,
                              NewThread,
                              sizeof(KTHREAD));
    }

    //
    // Give the thread a unique ID.
    //

    NewThread->ThreadId = RtlAtomicAdd32((PULONG)&PsNextThreadId, 1);

    //
    // Add the thread to the process.
    //

    KeAcquireQueuedLock(OwningProcess->QueuedLock);
    INSERT_BEFORE(&(NewThread->ProcessEntry), &(OwningProcess->ThreadListHead));
    OwningProcess->ThreadCount += 1;
    KeReleaseQueuedLock(OwningProcess->QueuedLock);
    SpProcessNewThread(OwningProcess->Identifiers.ProcessId,
                       NewThread->ThreadId);

    Status = STATUS_SUCCESS;

CreateThreadEnd:
    if (!KSUCCESS(Status)) {
        if (NewThread != NULL) {
            ObReleaseReference(NewThread);
            NewThread = NULL;
        }
    }

    return NewThread;
}

VOID
PspReaperThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine waits on the dead thread event and when the event is signaled
    it attempts to reap the default number of threads until the event is no
    longer signaled.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread. This
        parameter is not used.

Return Value:

    None. This thread never exits.

--*/

{

    while (TRUE) {
        KeWaitForEvent(PsDeadThreadsEvent, FALSE, WAIT_TIME_INDEFINITE);
        PspReapThreads(THREAD_DEFAULT_REAP_COUNT);
    }

    return;
}

VOID
PspReapThreads (
    UINTN TargetReapCount
    )

/*++

Routine Description:

    This routine checks for any threads that need to be cleaned up, dequeues
    them, and then frees the threads and all associated memory. This routine
    runs at low level.

Arguments:

    TargetReapCount - Supplies the maximum number of threads to reap.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    LIST_ENTRY ListHead;
    RUNLEVEL OldRunLevel;
    UINTN ReapCount;
    PKTHREAD Thread;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Acquire the lock and move up to the requested number of threads to the
    // local list.
    //

    ReapCount = 0;
    INITIALIZE_LIST_HEAD(&ListHead);
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&PsDeadThreadsLock);
    while ((LIST_EMPTY(&PsDeadThreadsListHead) == FALSE) &&
           (ReapCount < TargetReapCount)) {

        CurrentEntry = PsDeadThreadsListHead.Next;
        LIST_REMOVE(CurrentEntry);
        INSERT_BEFORE(CurrentEntry, &ListHead);
        ReapCount += 1;
    }

    PsDeadThreadsCount -= ReapCount;

    //
    // Only unsignal the event if there are no threads left to reap.
    //

    if (LIST_EMPTY(&PsDeadThreadsListHead) != FALSE) {
        KeSignalEvent(PsDeadThreadsEvent, SignalOptionUnsignal);
    }

    KeReleaseSpinLock(&PsDeadThreadsLock);
    KeLowerRunLevel(OldRunLevel);

    //
    // Now that execution is running back at passive, calmly walk the local
    // list, signal anyone waiting on the thread exiting, and destroy the
    // threads.
    //

    while (LIST_EMPTY(&ListHead) == FALSE) {
        Thread = LIST_VALUE(ListHead.Next,
                            KTHREAD,
                            SchedulerEntry.ListEntry);

        LIST_REMOVE(&(Thread->SchedulerEntry.ListEntry));
        Thread->SchedulerEntry.ListEntry.Next = NULL;

        //
        // Remove the thread from the process before the reference count
        // drops to zero so that acquiring the process lock and adding
        // a reference synchronizes against the thread destroying itself
        // during or after that process lock is released.
        //

        KeAcquireQueuedLock(Thread->OwningProcess->QueuedLock);
        LIST_REMOVE(&(Thread->ProcessEntry));
        Thread->ProcessEntry.Next = NULL;

        //
        // The thread has been removed from the process's thread list. Add
        // its resource usage to the process' counts. This is where a process'
        // max resident set is updated (by setting it in the thread and then
        // updating to the parent).
        //

        Thread->ResourceUsage.MaxResidentSet =
                           Thread->OwningProcess->AddressSpace->MaxResidentSet;

        PspAddResourceUsages(&(Thread->OwningProcess->ResourceUsage),
                             &(Thread->ResourceUsage));

        KeReleaseQueuedLock(Thread->OwningProcess->QueuedLock);

        //
        // Signal everyone waiting on the thread to die.
        //

        ObSignalObject(Thread, SignalOptionSignalAll);
        ObReleaseReference(Thread);
    }

    return;
}

VOID
PspDestroyThread (
    PVOID ThreadObject
    )

/*++

Routine Description:

    This routine frees all memory associated with a thread. It is assumed that
    the thread has already been unlinked from any queues or ready lists.

Arguments:

    ThreadObject - Supplies the thread to free.

Return Value:

    None.

--*/

{

    BOOL DestroyProcess;
    BOOL LastThread;
    PKPROCESS Process;
    BOOL SignalQueued;
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry;
    PKTHREAD Thread;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeGetCurrentThread() != ThreadObject);

    Thread = (PKTHREAD)ThreadObject;

    ASSERT((Thread->State == ThreadStateExited) ||
           (Thread->State == ThreadStateFirstTime));

    ASSERT(Thread->Header.ReferenceCount == 0);
    ASSERT(Thread->ProcessEntry.Next == NULL);
    ASSERT(Thread->SupplementaryGroups == NULL);

    //
    // Clean up any queued signals that snuck on while the thread was dying.
    //

    while (LIST_EMPTY(&(Thread->SignalListHead)) == FALSE) {
        SignalQueueEntry = LIST_VALUE(Thread->SignalListHead.Next,
                                      SIGNAL_QUEUE_ENTRY,
                                      ListEntry);

        LIST_REMOVE(&(SignalQueueEntry->ListEntry));
        SignalQueueEntry->ListEntry.Next = NULL;
        if (SignalQueueEntry->CompletionRoutine != NULL) {
            SignalQueueEntry->CompletionRoutine(SignalQueueEntry);
        }
    }

    DestroyProcess = FALSE;

    //
    // Destroy the built in timer.
    //

    if (Thread->BuiltinTimer != NULL) {
        KeDestroyTimer(Thread->BuiltinTimer);
    }

    //
    // Destroy the built in wait block.
    //

    if (Thread->BuiltinWaitBlock != NULL) {
        ObDestroyWaitBlock(Thread->BuiltinWaitBlock);
    }

    Process = Thread->OwningProcess;

    //
    // If the thread never got a chance to run, remove it from the owning
    // process's list and if this is the last thread, make sure the process has
    // an exit status before proceeding.
    //

    if (Thread->State == ThreadStateFirstTime) {
        LastThread = FALSE;
        if (Thread->ProcessEntry.Next != NULL) {
            KeAcquireQueuedLock(Process->QueuedLock);
            LIST_REMOVE(&(Thread->ProcessEntry));
            Process->ThreadCount -= 1;
            if (Process->ThreadCount == 0) {
                LastThread = TRUE;
            }

            KeReleaseQueuedLock(Process->QueuedLock);
            Thread->ProcessEntry.Next = NULL;

        } else if (Process->ThreadCount == 0) {
            LastThread = TRUE;
        }

        if ((LastThread != FALSE) && (Process->ExitReason == 0)) {
            PspSetProcessExitStatus(Process,
                                    CHILD_SIGNAL_REASON_KILLED,
                                    SIGNAL_ABORT);
        }
    }

    if (LIST_EMPTY(&(Process->ThreadListHead)) != FALSE) {
        DestroyProcess = TRUE;
    }

    //
    // Free the kernel stack.
    //

    if (Thread->KernelStack != NULL) {
        MmFreeKernelStack(Thread->KernelStack, Thread->KernelStackSize);
        Thread->KernelStack = NULL;
    }

    //
    // Remove the thread from its scheduling group.
    //

    if (Thread->State != ThreadStateFirstTime) {
        KeUnlinkSchedulerEntry(&(Thread->SchedulerEntry));
    }

    //
    // Potentially clean up the process if the last thread just exited. This
    // will clean up all blocked signals.
    //

    if (DestroyProcess != FALSE) {

        //
        // Send the child signal to the parent.
        //

        SignalQueued = PspQueueChildSignalToParent(Process,
                                                   Process->ExitStatus,
                                                   Process->ExitReason);

        ObSignalObject(Process, SignalOptionSignalAll);

        //
        // If the parent was not signaled, then just remove the process from
        // the global list.
        //

        if (SignalQueued == FALSE) {
            PspRemoveProcessFromLists(Process);
        }

        //
        // Clean up any queued signals that snuck on while the process was
        // dying.
        //

        while (LIST_EMPTY(&(Process->SignalListHead)) == FALSE) {
            SignalQueueEntry = LIST_VALUE(Process->SignalListHead.Next,
                                          SIGNAL_QUEUE_ENTRY,
                                          ListEntry);

            LIST_REMOVE(&(SignalQueueEntry->ListEntry));
            SignalQueueEntry->ListEntry.Next = NULL;
            if (SignalQueueEntry->CompletionRoutine != NULL) {
                SignalQueueEntry->CompletionRoutine(SignalQueueEntry);
            }
        }

        //
        // Also clean up any blocked signals.
        //

        while (LIST_EMPTY(&(Process->UnreapedChildList)) == FALSE) {
            SignalQueueEntry = LIST_VALUE(Process->UnreapedChildList.Next,
                                          SIGNAL_QUEUE_ENTRY,
                                          ListEntry);

            ASSERT(IS_CHILD_SIGNAL(SignalQueueEntry));

            LIST_REMOVE(&(SignalQueueEntry->ListEntry));
            SignalQueueEntry->ListEntry.Next = NULL;
            if (SignalQueueEntry->CompletionRoutine != NULL) {
                SignalQueueEntry->CompletionRoutine(SignalQueueEntry);
            }
        }
    }

    return;
}

KSTATUS
PspGetThreadList (
    PROCESS_ID ProcessId,
    PVOID Buffer,
    PULONG BufferSize
    )

/*++

Routine Description:

    This routine returns information about the threads in a given process.

Arguments:

    ProcessId - Supplies the ID of the process to get thread information for.

    Buffer - Supplies an optional pointer to a buffer to write the data into.

    BufferSize - Supplies a pointer that on input contains the size of the
        input buffer. On output, returns the size needed to contain the data.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_PROCESS if the supplied process ID does not correspond to
    any active process.

    STATUS_BUFFER_TOO_SMALL if a buffer was supplied but was not big enough to
    contain all the information.

--*/

{

    PKPROCESS Process;
    ULONG RemainingSize;
    ULONG Size;
    KSTATUS Status;
    PKTHREAD Thread;
    PLIST_ENTRY ThreadEntry;
    ULONG ThreadSize;
    KSTATUS ThreadStatus;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Process = PspGetProcessById(ProcessId);
    if (Process == NULL) {
        return STATUS_NO_SUCH_PROCESS;
    }

    RemainingSize = *BufferSize;
    Size = 0;
    Status = STATUS_SUCCESS;
    KeAcquireQueuedLock(Process->QueuedLock);
    ThreadEntry = Process->ThreadListHead.Next;
    while (ThreadEntry != &(Process->ThreadListHead)) {
        Thread = LIST_VALUE(ThreadEntry, KTHREAD, ProcessEntry);
        ThreadEntry = ThreadEntry->Next;
        ThreadSize = RemainingSize;
        ThreadStatus = PspGetThreadInformation(Thread, Buffer, &ThreadSize);
        if (!KSUCCESS(ThreadStatus)) {
            Status = ThreadStatus;

        } else if (RemainingSize >= ThreadSize) {
            Buffer += ThreadSize;
            RemainingSize -= ThreadSize;
        }

        Size += ThreadSize;
    }

    KeReleaseQueuedLock(Process->QueuedLock);
    ObReleaseReference(Process);
    *BufferSize = Size;
    return Status;
}

KSTATUS
PspGetThreadInformation (
    PKTHREAD Thread,
    PTHREAD_INFORMATION Buffer,
    PULONG BufferSize
    )

/*++

Routine Description:

    This routine returns information about a given thread.

Arguments:

    Thread - Supplies a pointer to the thread.

    Buffer - Supplies an optional pointer to a buffer to write the data into.

    BufferSize - Supplies a pointer that on input contains the size of the
        input buffer. On output, returns the size needed to contain the data.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if a buffer was supplied but was not big enough to
    contain all the information.

--*/

{

    KSTATUS Status;
    ULONG ThreadSize;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Status = STATUS_SUCCESS;
    ThreadSize = sizeof(THREAD_INFORMATION);
    if (Thread->Header.NameLength != 0) {
        ThreadSize += Thread->Header.NameLength -
                      (ANYSIZE_ARRAY * sizeof(CHAR));
    }

    if (*BufferSize >= ThreadSize) {
        Buffer->StructureSize = ThreadSize;
        Buffer->ThreadId = Thread->ThreadId;
        PspGetThreadResourceUsage(Thread, &(Buffer->ResourceUsage));
        Buffer->Name[0] = '\0';
        if (Thread->Header.NameLength != 0) {
            RtlStringCopy(Buffer->Name,
                          Thread->Header.Name,
                          Thread->Header.NameLength);
        }

    } else if (Buffer != NULL) {
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    *BufferSize = ThreadSize;
    return Status;
}

