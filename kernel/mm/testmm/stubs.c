/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stubs.c

Abstract:

    This module implements stub routines so the memory manager can be compiled
    in user-mode.

Author:

    Evan Green 30-Sep-2012

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "../mmp.h"
#include "testmm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

PVOID ArpPageFaultHandlerAsm;
ULONG MmDataCacheLineSize;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
MmSyncCacheRegion (
    PVOID Address,
    UINTN Size
    )

/*++

Routine Description:

    This routine unifies the instruction and data caches for the given region,
    probably after a region of executable code was modified. This does not
    necessarily flush data to the point where it's observable to device DMA
    (called the point of coherency).

Arguments:

    Address - Supplies the address to flush.

    Size - Supplies the number of bytes in the region to flush.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if one of the addresses in the given range was not
    valid.

--*/

{

    return STATUS_SUCCESS;
}

VOID
MmpSyncSwapPage (
    PVOID SwapPage,
    ULONG PageSize
    )

/*++

Routine Description:

    This routine cleans the data cache but does not invalidate the instruction
    cache for the given kernel region. It is used by the paging code for a
    temporary mapping that is going to get marked executable, but this mapping
    itself does not need an instruction cache flush.

Arguments:

    SwapPage - Supplies a pointer to the swap page.

    PageSize - Supplies the size of a page.

Return Value:

    None.

--*/

{

    return;
}

BOOL
MmpCopyUserModeMemory (
    PVOID Destination,
    PCVOID Source,
    ULONG ByteCount
    )

/*++

Routine Description:

    This routine copies a section of memory to or from user mode.

Arguments:

    Destination - Supplies a pointer to the buffer where the memory will be
        copied to.

    Source - Supplies a pointer to the buffer to be copied.

    ByteCount - Supplies the number of bytes to copy.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    memcpy(Destination, Source, ByteCount);
    return TRUE;
}

BOOL
MmpZeroUserModeMemory (
    PVOID Buffer,
    ULONG ByteCount
    )

/*++

Routine Description:

    This routine zeroes out a section of user mode memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to clear.

    ByteCount - Supplies the number of bytes to zero out.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    memset(Buffer, 0, ByteCount);
    return TRUE;
}

BOOL
MmpCleanCacheRegion (
    PVOID Address,
    UINTN Size
    )

/*++

Routine Description:

    This routine cleans the given region of virtual address space in the first
    level data cache.

Arguments:

    Address - Supplies the virtual address of the region to clean.

    Size - Supplies the number of bytes to clean.

Return Value:

    TRUE on success.

    FALSE if one of the addresses in the region caused a bad page fault.

--*/

{

    return TRUE;
}

BOOL
MmpCleanCacheLine (
    PVOID Address
    )

/*++

Routine Description:

    This routine flushes a cache line, writing any dirty bits back to the next
    level cache.

Arguments:

    Address - Supplies the address whose associated cache line will be
        cleaned.

Return Value:

    TRUE on success.

    FALSE if the address was a user mode one and accessing it caused a bad
    fault.

--*/

{

    return TRUE;
}

VOID
MmpInitializeCpuCaches (
    VOID
    )

/*++

Routine Description:

    This routine initializes the system's processor cache infrastructure.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

BOOL
MmpInvalidateInstructionCacheRegion (
    PVOID Address,
    ULONG Size
    )

/*++

Routine Description:

    This routine invalidates the given region of virtual address space in the
    instruction cache.

Arguments:

    Address - Supplies the virtual address of the region to invalidate.

    Size - Supplies the number of bytes to invalidate.

Return Value:

    TRUE on success.

    FALSE if one of the addresses in the region caused a bad page fault.

--*/

{

    return TRUE;
}

BOOL
MmpTouchUserModeMemoryForRead (
    PVOID Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine touches each page of a user mode buffer to ensure it can be
    read from.

Arguments:

    Buffer - Supplies a pointer to the buffer to probe.

    Size - Supplies the number of bytes to compare.

Return Value:

    TRUE if the buffers are valid.

    FALSE if the buffers are not valid.

--*/

{

    return TRUE;
}

BOOL
MmpTouchUserModeMemoryForWrite (
    PVOID Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine touches each page of a user mode buffer to ensure it can be
    written to.

Arguments:

    Buffer - Supplies a pointer to the buffer to probe.

    Size - Supplies the number of bytes to compare.

Return Value:

    TRUE if the buffers are valid.

    FALSE if the buffers are not valid.

--*/

{

    return TRUE;
}

BOOL
MmpCheckUserModeCopyRoutines (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine determines if a given fault occurred inside a user mode memory
    manipulation function, and adjusts the instruction pointer if so.

Arguments:

    TrapFrame - Supplies a pointer to the state of the machine when the page
        fault occurred.

Return Value:

    None.

--*/

{

    return FALSE;
}

ULONG
ArGetMultiprocessorIdRegister (
     VOID
     )

/*++

Routine Description:

    This routine gets the Multiprocessor ID register (MPIDR).

Arguments:

    None.

Return Value:

    Returns the value of the MPIDR.

--*/

{

    return 0;
}

VOID
ArSerializeExecution (
    VOID
    )

/*++

Routine Description:

    This routine acts a serializing instruction, preventing the processor
    from speculatively executing beyond this point.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

VOID
KeInitializeSpinLock (
    PKSPIN_LOCK Lock
    )

/*++

Routine Description:

    This routine initializes a spinlock.

Arguments:

    Lock - Supplies a pointer to the lock to initialize.

Return Value:

    None.

--*/

{

    Lock->LockHeld = 0;
    Lock->OwningThread = NULL;
    return;
}

BOOL
KeIsSpinLockHeld (
    PKSPIN_LOCK Lock
    )

/*++

Routine Description:

    This routine determines whether a spin lock is held or free.

Arguments:

    Lock - Supplies a pointer to the lock to check.

Return Value:

    TRUE if the lock has been acquired.

    FALSE if the lock is free.

--*/

{

    return TRUE;
}

PPROCESSOR_BLOCK
KeGetCurrentProcessorBlock (
    VOID
    )

/*++

Routine Description:

    This routine gets the processor state for the currently executing processor.

Arguments:

    None.

Return Value:

    Returns the current processor block.

--*/

{

    return NULL;
}

PPROCESSOR_BLOCK
KeGetCurrentProcessorBlockForDebugger (
    VOID
    )

/*++

Routine Description:

    This routine gets the processor block for the currently executing
    processor. It is intended to be called only by the debugger.

Arguments:

    None.

Return Value:

    Returns the current processor block.

--*/

{

    return NULL;
}

ULONGLONG
KeGetRecentTimeCounter (
    VOID
    )

/*++

Routine Description:

    This routine returns a relatively recent snap of the time counter.

Arguments:

    None.

Return Value:

    Returns the fairly recent snap of the time counter.

--*/

{

    return 0;
}

ULONGLONG
HlQueryTimeCounterFrequency (
    VOID
    )

/*++

Routine Description:

    This routine returns the frequency of the time counter. This frequency will
    never change after it is set on boot.

    This routine can be called at any runlevel.

Arguments:

    None.

Return Value:

    Returns the frequency of the time counter, in Hertz.

--*/

{

    ASSERT(FALSE);

    return 1;
}

VOID
ArInvalidateEntireTlb (
    VOID
    )

/*++

Routine Description:

    This routine invalidates the entire TLB.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

PVOID
ArGetInstructionPointer (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine returns the instruction pointer out of the trap frame.

Arguments:

    TrapFrame - Supplies the trap frame from which the instruction pointer
        will be returned.

Return Value:

    Returns the instruction pointer the trap frame is pointing to.

--*/

{

    return (PVOID)0xDEADBEEF;
}

BOOL
ArIsTrapFrameFromPrivilegedMode (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine determines if the given trap frame occurred in a privileged
    environment or not.

Arguments:

    TrapFrame - Supplies the trap frame.

Return Value:

    TRUE if the execution environment of the trap frame is privileged.

    FALSE if the execution environment of the trap frame is not privileged.

--*/

{

    return TRUE;
}

VOID
PsHandleUserModeFault (
    PVOID VirtualAddress,
    ULONG FaultFlags,
    PTRAP_FRAME TrapFrame,
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine handles a user mode fault where no image section seems to back
    the faulting address.

Arguments:

    VirtualAddress - Supplies the virtual address that caused the fault.

    FaultFlags - Supplies the fault information.

    TrapFrame - Supplies a pointer to the trap frame.

    Process - Supplies the process that caused the fault.

Return Value:

    None.

--*/

{

    return;
}

BOOL
PsDispatchPendingSignalsOnCurrentThread (
    PTRAP_FRAME TrapFrame,
    ULONG SystemCallNumber,
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine dispatches any pending signals that should be run on the
    current thread.

Arguments:

    TrapFrame - Supplies a pointer to the current trap frame. If this trap frame
        is not destined for user mode, this function exits immediately.

    SystemCallNumber - Supplies the number of the system call that is
        attempting to dispatch a pending signal. Supply SystemCallInvalid if
        the caller is not a system call.

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call that is attempting to dispatch a signal. Supply NULL if
        the caller is not a system call.

Return Value:

    FALSE if no signals are pending.

    TRUE if a signal was applied.

--*/

{

    return FALSE;
}

VOID
PsEvaluateRuntimeTimers (
    PKTHREAD Thread
    )

/*++

Routine Description:

    This routine checks the runtime timers for expiration on the current thread.

Arguments:

    Thread - Supplies a pointer to the current thread.

Return Value:

    None.

--*/

{

    return;
}

VOID
ArProcessorYield (
    VOID
    )

/*++

Routine Description:

    This routine executes a short processor yield in hardware.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

ULONG
ArGetTranslationTableBaseRegister0 (
    VOID
    )

/*++

Routine Description:

    This routine gets the translation table base register 0 (TTBR0), used as
    the base for all virtual to physical memory lookups.

Arguments:

    None.

Return Value:

    Returns the contents of TTBR0.

--*/

{

    return 0;
}

ULONG
HlGetDataCacheLineSize (
    VOID
    )

/*++

Routine Description:

    This routine returns the maximum data cache line size out of all registered
    cache controllers.

Arguments:

    None.

Return Value:

    Returns the maximum data cache line size out of all registered cache
    controllers in bytes.

--*/

{

    return 1;
}

VOID
HlFlushCacheRegion (
    PHYSICAL_ADDRESS Address,
    UINTN SizeInBytes,
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes the given cache region for every registered cache
    controller.

Arguments:

    Address - Supplies the starting physical address of the region to flush. It
        must be aligned to the cache line size.

    SizeInBytes - Supplies the number of bytes to flush.

    Flags - Supplies a bitmask of cache flush flags. See HL_CACHE_FLAG_* for
        definitions.

Return Value:

    None.

--*/

{

    return;
}

KSTATUS
HlSendIpi (
    IPI_TYPE IpiType,
    PPROCESSOR_SET Processors
    )

/*++

Routine Description:

    This routine sends an Inter-Processor Interrupt (IPI) to the given set of
    processors.

Arguments:

    IpiType - Supplies the type of IPI to deliver.

    Processors - Supplies the set of processors to deliver the IPI to.

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

PKPROCESS
PsGetCurrentProcess (
    VOID
    )

/*++

Routine Description:

    This routine is a stub to get the MM library to compile in other
    environments.

Arguments:

    BinaryName - Supplies a pointer to the name of the image on disk. This
        memory is not used after this function call, and can be released by the
        caller.

Return Value:

    NULL always.

--*/

{

    return NULL;
}

PKPROCESS
PsGetKernelProcess (
    VOID
    )

/*++

Routine Description:

    This routine returns a pointer to the system process.

Arguments:

    None.

Return Value:

    NULL always.

--*/

{

    return NULL;
}

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

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

RUNLEVEL
KeRaiseRunLevel (
    RUNLEVEL RunLevel
    )

/*++

Routine Description:

    This routine raises the running level of the current processor to the given
    level.

Arguments:

    RunLevel - Supplies the new running level of the current processor.

Return Value:

    Returns the old running level of the processor.

--*/

{

    return RunLevelLow;
}

VOID
KeLowerRunLevel (
    RUNLEVEL RunLevel
    )

/*++

Routine Description:

    This routine lowers the running level of the current processor to the given
    level.

Arguments:

    RunLevel - Supplies the new running level of the current processor.

Return Value:

    None.

--*/

{

    return;
}

RUNLEVEL
KeGetRunLevel (
    VOID
    )

/*++

Routine Description:

    This routine gets the running level for the current processor.

Arguments:

    None.

Return Value:

    Returns the current run level.

--*/

{

    return RunLevelLow;
}

PKTHREAD
KeGetCurrentThread (
    VOID
    )

/*++

Routine Description:

    This routine gets the current thread running on this processor.

Arguments:

    None.

Return Value:

    Returns the current run level.

--*/

{

    return NULL;
}

PQUEUED_LOCK
KeCreateQueuedLock (
    VOID
    )

/*++

Routine Description:

    This routine creates a new queued lock under the current thread. These locks
    can be used at up to dispatch level if non-paged memory is used.

Arguments:

    NonPaged - Supplies a flag indicating that non-paged memory should be used
        for the lock, permitting it to be acquired and released as high as
        dispatch level.

Return Value:

    Returns a pointer to the new lock on success.

    NULL on failure.

--*/

{

    return (PVOID)1;
}

VOID
KeDestroyQueuedLock (
    PQUEUED_LOCK Lock
    )

/*++

Routine Description:

    This routine destroys a queued lock by decrementing its reference count.

Arguments:

    Lock - Supplies a pointer to the queued lock to destroy.

Return Value:

    None. When the function returns, the lock must not be used again.

--*/

{

    return;
}

VOID
KeAcquireQueuedLock (
    PQUEUED_LOCK Lock
    )

/*++

Routine Description:

    This routine acquires the queued lock. If the lock is held, the thread
    blocks until it becomes available.

Arguments:

    Lock - Supplies a pointer to the queued lock to acquire.

Return Value:

    None. When the function returns, the lock will be held.

--*/

{

    return;
}

VOID
KeReleaseQueuedLock (
    PQUEUED_LOCK Lock
    )

/*++

Routine Description:

    This routine releases a queued lock that has been previously acquired.

Arguments:

    Lock - Supplies a pointer to the queued lock to release.

Return Value:

    None.

--*/

{

    return;
}

KERNEL_API
BOOL
KeTryToAcquireQueuedLock (
    PQUEUED_LOCK Lock
    )

/*++

Routine Description:

    This routine attempts to acquire the queued lock. If the lock is busy, it
    does not add this thread to the queue of waiters.

Arguments:

    Lock - Supplies a pointer to a queued lock.

Return Value:

    Returns TRUE if the lock was acquired, or FALSE otherwise.

--*/

{

    return TRUE;
}

BOOL
KeIsQueuedLockHeld (
    PQUEUED_LOCK Lock
    )

/*++

Routine Description:

    This routine determines whether a queued lock is acquired or free.

Arguments:

    Lock - Supplies a pointer to the queued lock.

Return Value:

    TRUE if the queued lock is held.

    FALSE if the queued lock is free.

--*/

{

    return TRUE;
}

KERNEL_API
PSHARED_EXCLUSIVE_LOCK
KeCreateSharedExclusiveLock (
    VOID
    )

/*++

Routine Description:

    This routine creates a shared-exclusive lock.

Arguments:

    None.

Return Value:

    Returns a pointer to a shared-exclusive lock on success, or NULL on failure.

--*/

{

    return (PVOID)1;
}

KERNEL_API
VOID
KeDestroySharedExclusiveLock (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine destroys a shared-exclusive lock.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    None.

--*/

{

    return;
}

KERNEL_API
VOID
KeAcquireSharedExclusiveLockShared (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine acquired the given shared-exclusive lock in shared mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

{

    return;
}

KERNEL_API
VOID
KeReleaseSharedExclusiveLockShared (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine releases the given shared-exclusive lock from shared mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

{

    return;
}

KERNEL_API
VOID
KeAcquireSharedExclusiveLockExclusive (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine acquired the given shared-exclusive lock in exclusive mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

{

    return;
}

KERNEL_API
VOID
KeReleaseSharedExclusiveLockExclusive (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine releases the given shared-exclusive lock from exclusive mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

{

    return;
}

KERNEL_API
BOOL
KeIsSharedExclusiveLockHeld (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine determines whether a shared-exclusive lock is held or free.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    Returns TRUE if the shared-exclusive lock is held, or FALSE if not.

--*/

{

    return TRUE;
}

KERNEL_API
BOOL
KeIsSharedExclusiveLockHeldExclusive (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine determines whether a shared-exclusive lock is held exclusively
    or not.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    Returns TRUE if the shared-exclusive lock is held exclusively, or FALSE
    otherwise.

--*/

{

    return TRUE;
}

KERNEL_API
BOOL
KeIsSharedExclusiveLockHeldShared (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine determines whether a shared-exclusive lock is held shared or
    not.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    Returns TRUE if the shared-exclusive lock is held shared, or FALSE
    otherwise.

--*/

{

    return TRUE;
}

KSTATUS
KeSendIpi (
    PIPI_ROUTINE IpiRoutine,
    PVOID IpiContext,
    PPROCESSOR_SET Processors
    )

/*++

Routine Description:

    This routine runs the given routine at IPI level on the specified set of
    processors. This routine runs synchronously: the routine will have completed
    running on all processors by the time this routine returns. This routine
    must be called at or below dispatch level.

Arguments:

    IpiRoutine - Supplies a pointer to the routine to run at IPI level.

    IpiContext - Supplies the value to pass to the IPI routine as a parameter.

    Processors - Supplies the set of processors to run the IPI on.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

UINTN
ArGetCurrentPageDirectory (
    VOID
    )

/*++

Routine Description:

    This routine returns the active page directory.

Arguments:

    None.

Return Value:

    Returns the page directory currently in use by the system.

--*/

{

    return 0;
}

VOID
ArSetCurrentPageDirectory (
    ULONG Value
    )

/*++

Routine Description:

    This routine sets the CR3 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

{

    return;
}

VOID
ArSwitchTtbr0 (
    ULONG NewValue
    )

/*++

Routine Description:

    This routine performs the proper sequence for changing contexts in TTBR0,
    including the necessary invalidates and barriers.

Arguments:

    NewValue - Supplies the new value to write.

Return Value:

    None.

--*/

{

    return;
}

VOID
ArInvalidateTlbEntry (
    PVOID Address
    )

/*++

Routine Description:

    This routine invalidates one TLB entry corresponding to the given virtual
    address.

Arguments:

    Address - Supplies the virtual address whose associated TLB entry will be
        invalidated.

Return Value:

    None.

--*/

{

    return;
}

ULONG
KeGetCurrentProcessorNumber (
    VOID
    )

/*++

Routine Description:

    This routine gets the processor number for the currently executing
    processor.

Arguments:

    None.

Return Value:

    Returns the current zero-indexed processor number.

--*/

{

    return 0;
}

VOID
KeCrashSystemEx (
    ULONG CrashCode,
    PCSTR CrashCodeString,
    ULONGLONG Parameter1,
    ULONGLONG Parameter2,
    ULONGLONG Parameter3,
    ULONGLONG Parameter4
    )

/*++

Routine Description:

    This routine officially takes the system down after a fatal system error
    has occurred. This function does not return.

Arguments:

    CrashCode - Supplies the reason for the system crash.

    CrashCodeString - Supplies the string corresponding to the given crash
        code. This parameter is generated by the macro, and should not be
        filled in directly.

    Parameter1 - Supplies an optional parameter regarding the crash.

    Parameter2 - Supplies an optional parameter regarding the crash.

    Parameter3 - Supplies an optional parameter regarding the crash.

    Parameter4 - Supplies an optional parameter regarding the crash.

Return Value:

    None. This function does not return.

--*/

{

    printf("**********************************************************"
           "**********************\n"
           "*                                                         "
           "                     *\n"
           "*                            Fatal System Error           "
           "                     *\n"
           "*                                                         "
           "                     *\n"
           "**********************************************************"
           "**********************\n\n"
           "Error Code: %s (0x%x)\n"
           "Parameter1: 0x%08llx\n"
           "Parameter2: 0x%08llx\n"
           "Parameter3: 0x%08llx\n"
           "Parameter4: 0x%08llx\n\n",
           CrashCodeString,
           CrashCode,
           Parameter1,
           Parameter2,
           Parameter3,
           Parameter4);

    exit(1);
}

VOID
KeRegisterCrashDumpFile (
    HANDLE Handle,
    BOOL Register
    )

/*++

Routine Description:

    This routine registers a file for use as a crash dump file.

Arguments:

    Handle - Supplies a handle to the page file to register.

    Register - Supplies a boolean indicating if the page file is registering
        (TRUE) or de-registering (FALSE).

Return Value:

    None.

--*/

{

    return;
}

VOID
KeAcquireSpinLock (
    PKSPIN_LOCK Lock
    )

/*++

Routine Description:

    This routine acquires a kernel spinlock. It must be acquired at or below
    dispatch level. This routine may yield the processor.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

Return Value:

    None.

--*/

{

    ULONG LockValue;

    do {
        LockValue = 1;
        if (Lock->LockHeld == 0) {
            LockValue = 0;
            Lock->LockHeld = 1;
        }

    } while (LockValue != 0);

    return;
}

VOID
KeReleaseSpinLock (
    PKSPIN_LOCK Lock
    )

/*++

Routine Description:

    This routine releases a kernel spinlock.

Arguments:

    Lock - Supplies a pointer to the lock to release.

Return Value:

    None.

--*/

{

    ASSERT(Lock->LockHeld != 0);

    Lock->LockHeld = 0;
    return;
}

ULONG
KeGetActiveProcessorCount (
    VOID
    )

/*++

Routine Description:

    This routine gets the number of processors currently running in the
    system.

Arguments:

    None.

Return Value:

    Returns the number of active processors currently in the system.

--*/

{

    return 1;
}

KERNEL_API
PKEVENT
KeCreateEvent (
    PVOID ParentObject
    )

/*++

Routine Description:

    This routine creates a kernel event. It comes initialized to Not Signaled.

Arguments:

    ParentObject - Supplies an optional parent object to create the event
        under.

Return Value:

    Returns a pointer to the event, or NULL if the event could not be created.

--*/

{

    ASSERT(FALSE);

    return NULL;
}

KERNEL_API
VOID
KeDestroyEvent (
    PKEVENT Event
    )

/*++

Routine Description:

    This routine destroys an event created with KeCreateEvent. The event is no
    longer valid after this call.

Arguments:

    Event - Supplies a pointer to the event to free.

Return Value:

    None.

--*/

{

    ASSERT(FALSE);

    return;
}

KERNEL_API
VOID
KeSignalEvent (
    PKEVENT Event,
    SIGNAL_OPTION Option
    )

/*++

Routine Description:

    This routine sets an event to the given signal state.

Arguments:

    Event - Supplies a pointer to the event to signal or unsignal.

    Option - Supplies the signaling behavior to apply.

Return Value:

    None.

--*/

{

    ASSERT(FALSE);

    return;
}

KERNEL_API
KSTATUS
KeWaitForEvent (
    PKEVENT Event,
    BOOL Interruptible,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine waits until an event enters a signaled state.

Arguments:

    Event - Supplies a pointer to the event to wait for.

    Interruptible - Supplies a boolean indicating whether or not the wait can
        be interrupted if a signal is sent to the process on which this thread
        runs. If TRUE is supplied, the caller must check the return status
        code to find out if the wait was really satisfied or just interrupted.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        objects should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever on these objects.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KERNEL_API
KSTATUS
IoGetDevice (
    PIO_HANDLE Handle,
    PDEVICE *Device
    )

/*++

Routine Description:

    This routine returns the actual device backing the given I/O object. Not
    all I/O objects are actually backed by a single device. For file and
    directory objects, this routine will return a pointer to the volume.

Arguments:

    Handle - Supplies the open file handle.

    Device - Supplies a pointer where the underlying I/O device will be
        returned.

Return Value:

    Status code.

--*/

{

    *Device = NULL;

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KERNEL_API
KSTATUS
IoOpen (
    BOOL FromKernelMode,
    PIO_HANDLE Directory,
    PCSTR Path,
    ULONG PathLength,
    ULONG Access,
    ULONG Flags,
    FILE_PERMISSIONS CreatePermissions,
    PIO_HANDLE *Handle
    )

/*++

Routine Description:

    This routine opens a file, device, pipe, or other I/O object.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    Directory - Supplies an optional pointer to an open handle to a directory
        for relative paths. Supply NULL to use the current working directory.

    Path - Supplies a pointer to the path to open.

    PathLength - Supplies the length of the path buffer in bytes, including the
        null terminator.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    CreatePermissions - Supplies the permissions to apply for a created file.

    Handle - Supplies a pointer where a pointer to the open I/O handle will be
        returned on success.

Return Value:

    Status code.

--*/

{

    *Handle = NULL;

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KSTATUS
IoOpenPageFile (
    PCSTR Path,
    ULONG PathSize,
    ULONG Access,
    ULONG Flags,
    PIO_HANDLE *Handle,
    PULONGLONG FileSize
    )

/*++

Routine Description:

    This routine opens a page file. This routine is to be used only
    internally by MM.

Arguments:

    Path - Supplies a pointer to the string containing the file path to open.

    PathSize - Supplies the length of the path buffer in bytes, including
        the null terminator.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Handle - Supplies a pointer where a pointer to the open I/O handle will be
        returned on success.

    FileSize - Supplies a pointer where the file size in bytes will be returned
        on success.

Return Value:

    Status code.

--*/

{

    *Handle = NULL;

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KERNEL_API
KSTATUS
IoClose (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine closes a file or device.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle returned when the file was
        opened.

Return Value:

    Status code. Close operations can fail if their associated flushes to
    the file system fail.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KERNEL_API
KSTATUS
IoReadAtOffset (
    PIO_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    IO_OFFSET Offset,
    UINTN SizeInBytes,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PUINTN BytesCompleted,
    PIRP Irp
    )

/*++

Routine Description:

    This routine reads from an I/O object at a specific offset.

Arguments:

    Handle - Supplies the open I/O handle.

    IoBuffer - Supplies a pointer to an I/O buffer where the read data will be
        returned on success.

    Offset - Supplies the offset from the beginning of the file or device where
        the I/O should be done.

    SizeInBytes - Supplies the number of bytes to read.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    BytesCompleted - Supplies the a pointer where the number of bytes actually
        read will be returned.

    Irp - Supplies a pointer to the IRP to use for this I/O. This is required
        when doing operations on the page file.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KERNEL_API
KSTATUS
IoWriteAtOffset (
    PIO_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    IO_OFFSET Offset,
    UINTN SizeInBytes,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PUINTN BytesCompleted,
    PIRP Irp
    )

/*++

Routine Description:

    This routine writes to an I/O object at a specific offset.

Arguments:

    Handle - Supplies the open I/O handle.

    IoBuffer - Supplies a pointer to an I/O buffer containing the data to write.

    Offset - Supplies the offset from the beginning of the file or device where
        the I/O should be done.

    SizeInBytes - Supplies the number of bytes to write.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    BytesCompleted - Supplies the a pointer where the number of bytes actually
        written will be returned.

    Irp - Supplies a pointer to the IRP to use for this I/O. This is required
        when doing operations on the page file.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KERNEL_API
KSTATUS
IoSetFileInformation (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
    PSET_FILE_INFORMATION Request
    )

/*++

Routine Description:

    This routine sets the file properties for the given I/O handle.
    Only some properties can be set by this routine.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether the request
        originated from user mode (FALSE) or kernel mode (TRUE). Kernel mode
        requests bypass permission checks.

    Handle - Supplies the open file handle.

    Request - Supplies a pointer to the get/set information request.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KERNEL_API
ULONG
IoGetCacheEntryDataSize (
    VOID
    )

/*++

Routine Description:

    This routine returns the size of data stored in each cache entry.

Arguments:

    None.

Return Value:

    Returns the size of the data stored in each cache entry.

--*/

{

    return MmPageSize();
}

VOID
IoMarkPageCacheEntryDirty (
    PPAGE_CACHE_ENTRY Entry
    )

/*++

Routine Description:

    This routine marks the given page cache entry as dirty.

Arguments:

    Entry - Supplies a pointer to a page cache entry.

Return Value:

    None.

--*/

{

    return;
}

KERNEL_API
KSTATUS
IoGetFileSize (
    PIO_HANDLE Handle,
    PULONGLONG FileSize
    )

/*++

Routine Description:

    This routine returns the current size of the given file or block device.

Arguments:

    Handle - Supplies the open file handle.

    FileSize - Supplies a pointer where the file size will be returned on
        success.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    *FileSize = 0;
    return STATUS_NOT_IMPLEMENTED;
}

VOID
IoIoHandleAddReference (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine increments the reference count on an I/O handle.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle.

Return Value:

    None.

--*/

{

    ASSERT(FALSE);

    return;
}

KSTATUS
IoIoHandleReleaseReference (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine decrements the reference count on an I/O handle. If the
    reference count becomes zero, the I/O handle will be destroyed.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle.

Return Value:

    None.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

PIMAGE_SECTION_LIST
IoGetImageSectionListFromIoHandle (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine gets the image section list for the given I/O handle.

Arguments:

    IoHandle - Supplies a pointer to an I/O handle.

Return Value:

    Returns a pointer to the I/O handles image section list or NULL on failure.

--*/

{

    ASSERT(FALSE);

    return NULL;
}

ULONG
IoGetIoHandleAccessPermissions (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine returns the access permissions for the given I/O handle.

Arguments:

    IoHandle - Supplies a pointer to an I/O handle.

Return Value:

    Returns the access permissions for the given I/O handle.

--*/

{

    ASSERT(FALSE);

    return 0;
}

BOOL
IoIoHandleIsCacheable (
    PIO_HANDLE IoHandle,
    PULONG MapFlags
    )

/*++

Routine Description:

    This routine determines whether or not data for the I/O object specified by
    the given handle is cached in the page cache.

Arguments:

    IoHandle - Supplies a pointer to an I/O handle.

    MapFlags - Supplies an optional pointer where any additional map flags
        needed when mapping sections from this handle will be returned.
        See MAP_FLAG_* definitions.

Return Value:

    Returns TRUE if the I/O handle's object uses the page cache, FALSE
    otherwise.

--*/

{

    ASSERT(FALSE);

    return FALSE;
}

KSTATUS
IoPathAppend (
    PCSTR Prefix,
    ULONG PrefixSize,
    PCSTR Component,
    ULONG ComponentSize,
    ULONG AllocationTag,
    PSTR *AppendedPath,
    PULONG AppendedPathSize
    )

/*++

Routine Description:

    This routine appends a path component to a path.

Arguments:

    Prefix - Supplies the initial path string. This can be null.

    PrefixSize - Supplies the size of the prefix string in bytes including the
        null terminator.

    Component - Supplies a pointer to the component string to add.

    ComponentSize - Supplies the size of the component string in bytes
        including a null terminator.

    AllocationTag - Supplies the tag to use for the combined allocation.

    AppendedPath - Supplies a pointer where the new path will be returned. The
        caller is responsible for freeing this memory..

    AppendedPathSize - Supplies a pointer where the size of the appended bath
        buffer in bytes including the null terminator will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KERNEL_API
KSTATUS
IoFlush (
    PIO_HANDLE Handle,
    IO_OFFSET Offset,
    ULONGLONG Size,
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes I/O data to its appropriate backing device.

Arguments:

    Handle - Supplies an open I/O handle. This parameters is not required if
        the FLUSH_FLAG_ALL flag is set.

    Offset - Supplies the offset from the beginning of the file or device where
        the flush should be done.

    Size - Supplies the size, in bytes, of the region to flush. Supply a value
        of -1 to flush from the given offset to the end of the file.

    Flags - Supplies flags regarding the flush operation. See FLUSH_FLAG_*
        definitions.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KERNEL_API
KSTATUS
IoSeek (
    PIO_HANDLE Handle,
    SEEK_COMMAND SeekCommand,
    IO_OFFSET Offset,
    PIO_OFFSET NewOffset
    )

/*++

Routine Description:

    This routine seeks to the given position in a file. This routine is only
    relevant for normal file or block based devices.

Arguments:

    Handle - Supplies the open I/O handle.

    SeekCommand - Supplies the reference point for the seek offset. Usual
        reference points are the beginning of the file, current file position,
        and the end of the file.

    Offset - Supplies the offset from the reference point to move in bytes.

    NewOffset - Supplies an optional pointer where the file position after the
        move will be returned on success.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KERNEL_API
KSTATUS
IoRead (
    PIO_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    UINTN SizeInBytes,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PUINTN BytesCompleted
    )

/*++

Routine Description:

    This routine reads from an I/O object.

Arguments:

    Handle - Supplies the open I/O handle.

    IoBuffer - Supplies a pointer to an I/O buffer where the read data will be
        returned on success.

    SizeInBytes - Supplies the number of bytes to read.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    BytesCompleted - Supplies the a pointer where the number of bytes actually
        read will be returned.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KERNEL_API
PIRP
IoCreateIrp (
    PDEVICE Device,
    IRP_MAJOR_CODE MajorCode,
    ULONG Flags
    )

/*++

Routine Description:

    This routine creates and initializes an IRP. This routine must be called
    at or below dispatch level.

Arguments:

    Device - Supplies a pointer to the device the IRP will be sent to.

    MajorCode - Supplies the major code of the IRP, which cannot be changed
        once an IRP is allocated (or else disaster ensues).

    Flags - Supplies a bitmask of IRP creation flags. See IRP_FLAG_* for
        definitions.

Return Value:

    Returns a pointer to the newly allocated IRP on success, or NULL on
    failure.

--*/

{

    return NULL;
}

KERNEL_API
VOID
IoDestroyIrp (
    PIRP Irp
    )

/*++

Routine Description:

    This routine destroys an IRP, freeing all memory associated with it. This
    routine must be called at or below dispatch level.

Arguments:

    Irp - Supplies a pointer to the IRP to free.

Return Value:

    None.

--*/

{

    return;
}

VOID
IoPageCacheEntryAddReference (
    PPAGE_CACHE_ENTRY Entry
    )

/*++

Routine Description:

    This routine decrements the reference count on the given page cache entry.

Arguments:

    Entry - Supplies a pointer to the page cache entry whose reference count
        should be incremented.

Return Value:

    None.

--*/

{

    return;
}

VOID
IoPageCacheEntryReleaseReference (
    PPAGE_CACHE_ENTRY Entry
    )

/*++

Routine Description:

    This routine decrements the reference count on the given page cache entry.

Arguments:

    Entry - Supplies a pointer to the page cache entry whose reference count
        should be incremented.

Return Value:

    None.

--*/

{

    return;
}

PHYSICAL_ADDRESS
IoGetPageCacheEntryPhysicalAddress (
    PPAGE_CACHE_ENTRY Entry,
    PULONG MapFlags
    )

/*++

Routine Description:

    This routine returns the physical address of the page cache entry.

Arguments:

    Entry - Supplies a pointer to a page cache entry.

    MapFlags - Supplies an optional pointer to the additional mapping flags
        mandated by the underlying file object.

Return Value:

    Returns the physical address of the given page cache entry.

--*/

{

    return INVALID_PHYSICAL_ADDRESS;
}

PVOID
IoGetPageCacheEntryVirtualAddress (
    PPAGE_CACHE_ENTRY Entry
    )

/*++

Routine Description:

    This routine gets the given page cache entry's virtual address.

Arguments:

    Entry - Supplies a pointer to a page cache entry.

Return Value:

    Returns the virtual address of the given page cache entry.

--*/

{

    return NULL;
}

BOOL
IoSetPageCacheEntryVirtualAddress (
    PPAGE_CACHE_ENTRY Entry,
    PVOID VirtualAddress
    )

/*++

Routine Description:

    This routine attempts to set the virtual address in the given page cache
    entry. It is assumed that the page cache entry's physical address is mapped
    at the given virtual address.

Arguments:

    Entry - Supplies as pointer to the page cache entry.

    VirtualAddress - Supplies the virtual address to set in the page cache
        entry.

Return Value:

    Returns TRUE if the set succeeds or FALSE if another virtual address is
    already set for the page cache entry.

--*/

{

    return FALSE;
}

PVOID
IoReferenceFileObjectForHandle (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine returns an opaque pointer to the file object opened by the
    given handle. It also adds a reference to the file object, which the caller
    is responsible for freeing.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle whose underlying file
        object should be referenced.

Return Value:

    Returns an opaque pointer to the file object, with an incremented reference
    count. The caller is responsible for releasing this reference.

--*/

{

    return NULL;
}

VOID
IoFileObjectReleaseReference (
    PVOID FileObject
    )

/*++

Routine Description:

    This routine releases an external reference on a file object taken by
    referencing the file object for a handle.

Arguments:

    FileObject - Supplies the opaque pointer to the file object.

Return Value:

    None. The caller should not count on this pointer remaining unique after
    this call returns.

--*/

{

    return;
}

KSTATUS
IoNotifyFileMapping (
    PIO_HANDLE Handle,
    BOOL Mapping
    )

/*++

Routine Description:

    This routine is called to notify a file object that it is being mapped
    into memory or unmapped.

Arguments:

    Handle - Supplies the handle being mapped.

    Mapping - Supplies a boolean indicating if a new mapping is being created
        (TRUE) or an old mapping is being destroyed (FALSE).

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

KERNEL_API
KSTATUS
IoGetFileInformation (
    PIO_HANDLE Handle,
    PFILE_PROPERTIES FileProperties
    )

/*++

Routine Description:

    This routine gets the file properties for the given I/O handle.

Arguments:

    Handle - Supplies the open file handle.

    FileProperties - Supplies a pointer where the file properties will be
        returned on success.

Return Value:

    Status code.

--*/

{

    RtlZeroMemory(FileProperties, sizeof(FILE_PROPERTIES));
    return STATUS_SUCCESS;
}

VOID
ObAddReference (
    PVOID Object
    )

/*++

Routine Description:

    This routine increases the reference count on an object by 1. This also
    increases the reference count of the object's parents up the tree.

Arguments:

    Object - Supplies a pointer to the object to add a reference to. This
        structure is presumed to begin with an OBJECT_HEADER.

Return Value:

    None.

--*/

{

    POBJECT_HEADER TypedObject;

    TypedObject = (POBJECT_HEADER)Object;
    TypedObject->ReferenceCount += 1;
    return;
}

VOID
ObReleaseReference (
    PVOID Object
    )

/*++

Routine Description:

    This routine decreases the reference count of an object by 1. If this
    causes the reference count of the object to drop to 0, the object will be
    freed. This also decreases the reference count of the object's parents up
    the tree, which may also cause the parent objects to be freed if this was
    the last reference on them.

Arguments:

    Object - Supplies a pointer to the object to subtract a reference from.
        This structure is presumed to begin with an OBJECT_HEADER structure.

Return Value:

    None.

--*/

{

    POBJECT_HEADER CurrentObject;
    ULONG OldReferenceCount;

    CurrentObject = (POBJECT_HEADER)Object;
    OldReferenceCount = CurrentObject->ReferenceCount;
    CurrentObject->ReferenceCount -= 1;
    if (OldReferenceCount == 1) {
        free(Object);
    }

    return;
}

PVOID
ObCreateObject (
    OBJECT_TYPE Type,
    PVOID Parent,
    PCSTR ObjectName,
    ULONG NameLength,
    ULONG DataSize,
    PDESTROY_OBJECT_ROUTINE DestroyRoutine,
    ULONG Flags,
    ULONG Tag
    )

/*++

Routine Description:

    This routine creates a new system object.

Arguments:

    Type - Supplies the type of object being created.

    Parent - Supplies a pointer to the object that this object is a child under.
        Supply NULL to create an object off the root node.

    ObjectName - Supplies an optional name for the object. A copy of this
        string will be made unless the flags specify otherwise.

    NameLength - Supplies the length of the name string in bytes, including
        the null terminator.

    DataSize - Supplies the size of the object body, *including* the object
        header.

    DestroyRoutine - Supplies an optional pointer to a function to be called
        when the reference count of the object drops to zero (immediately before
        the object is deallocated).

    Flags - Supplies optional flags indicating various properties of the object.
        See OBJECT_FLAG_* definitions.

    Tag - Supplies the pool tag that should be used for the memory allocation.

Return Value:

    Returns a pointer to the new object, on success. The returned structure is
        assumed to start with an OBJECT_HEADER structure.

    NULL if the object could not be allocated, the object already exists, or
        an invalid parameter was passed in.

--*/

{

    POBJECT_HEADER Object;

    Object = malloc(DataSize);
    if (Object != NULL) {
        memset(Object, 0, DataSize);
        Object->ReferenceCount = 1;
    }

    return Object;
}

KSTATUS
ObWaitOnObjects (
    PVOID *ObjectArray,
    ULONG ObjectCount,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PWAIT_BLOCK PreallocatedWaitBlock,
    PVOID *SignalingObject
    )

/*++

Routine Description:

    This routine waits on multiple objects until one (or all in some cases) is
    signaled. The caller is responsible for maintaining references to these
    objects.

Arguments:

    ObjectArray - Supplies an array of object pointers containing the objects
        to wait on. Each object must only be on the list once.

    ObjectCount - Supplies the number of elements in the array.

    Flags - Supplies a bitfield of flags governing the behavior of the wait.
        See WAIT_FLAG_* definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        objects should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever on these objects.

    PreallocatedWaitBlock - Supplies an optional pointer to a pre-allocated
        wait block to use for the wait. This is optional, however if there are
        a large number of objects to wait on and this is a common operation, it
        is a little faster to pre-allocate a wait block and reuse it.

    SignalingObject - Supplies an optional pointer where the object that
        satisfied the wait will be returned on success. If the wait was
        interrupted, this returns NULL. If the WAIT_FLAG_ALL_OBJECTS flag was
        not specified, the first object to be signaled will be returned. If the
        WAIT_FLAG_ALL_OBJECTS was specified, the caller should not depend on
        a particular object being returned here.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

PVOID
ObGetHandleValue (
    PHANDLE_TABLE Table,
    HANDLE Handle,
    PULONG Flags
    )

/*++

Routine Description:

    This routine looks up the given handle and returns the value associated
    with that handle.

Arguments:

    Table - Supplies a pointer to the handle table.

    Handle - Supplies the handle returned when the handle was created.

    Flags - Supplies an optional pointer that receives value of the handle's
        flags.

Return Value:

    Returns the value associated with that handle upon success.

    NULL if the given handle is invalid.

--*/

{

    ASSERT(FALSE);

    return NULL;
}

