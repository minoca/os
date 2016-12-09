/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    amlos.c

Abstract:

    This module implements operating system support functions for the ACPI AML
    interpreter and namespace.

Author:

    Evan Green 13-Nov-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "acpiobj.h"
#include "amlos.h"
#include "amlops.h"
#include "namespce.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag for ACPI AMLallocations.
//

#define ACPI_AML_ALLOCATION_TAG 0x696C6D41 // 'ilmA'

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the implementation of an ACPI mutex object.

Members:

    QueuedLock - Stores a pointer to the OS queued lock implementing the
        synchronization primitive.

    OwningContext - Stores a pointer to the execution context (thread) that
        has the lock acquired.

    RecursionCount - Stores the number of acquire calls that have been made.

    SyncLevel - Stores the sync level of this mutex.

    PreviousSyncLevel - Stores the sync level of the execution context
        immediately before the acquire call was made.

--*/

typedef struct _ACPI_MUTEX {
    PQUEUED_LOCK QueuedLock;
    PAML_EXECUTION_CONTEXT OwningContext;
    ULONG RecursionCount;
    ULONG SyncLevel;
    ULONG PreviousSyncLevel;
} ACPI_MUTEX, *PACPI_MUTEX;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

PQUEUED_LOCK AcpiPciLock = NULL;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
AcpipInitializeOperatingSystemAmlSupport (
    VOID
    )

/*++

Routine Description:

    This routine initializes operating system specific support for the AML
    interpreter.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    AcpiPciLock = KeCreateQueuedLock();
    if (AcpiPciLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeOperatingSystemAmlSupportEnd;
    }

    Status = STATUS_SUCCESS;

InitializeOperatingSystemAmlSupportEnd:
    return Status;
}

BOOL
AcpipCheckOsiSupport (
    PCSTR String
    )

/*++

Routine Description:

    This routine determines whether or not a given _OSI request is supported.

Arguments:

    String - Supplies a pointer to the string to check.

Return Value:

    TRUE if the implementation supports this feature.

    FALSE if the request is not supported.

--*/

{

    return FALSE;
}

PVOID
AcpipAllocateMemory (
    ULONG Size
    )

/*++

Routine Description:

    This routine allocates memory from the operating system for the ACPI
    interpreter and namespace.

Arguments:

    Size - Supplies the size of the allocation, in bytes.

Return Value:

    Returns a pointer to the allocated memory on success.

    NULL on failure.

--*/

{

    return MmAllocatePagedPool(Size, ACPI_AML_ALLOCATION_TAG);
}

VOID
AcpipFreeMemory (
    PVOID Allocation
    )

/*++

Routine Description:

    This routine frees memory allocated for the ACPI AML interpreter and
    namespace.

Arguments:

    Allocation - Supplies a pointer to the allocated memory.

Return Value:

    None.

--*/

{

    MmFreePagedPool(Allocation);
    return;
}

VOID
AcpipFatalError (
    ULONGLONG Parameter1,
    ULONGLONG Parameter2,
    ULONGLONG Parameter3,
    ULONGLONG Parameter4
    )

/*++

Routine Description:

    This routine takes the system down as gracefully as possible.

Arguments:

    Parameter1 - Supplies an optional parameter.

    Parameter2 - Supplies an optional parameter.

    Parameter3 - Supplies an optional parameter.

    Parameter4 - Supplies an optional parameter.

Return Value:

    This function does not return.

--*/

{

    KeCrashSystem(CRASH_ACPI_FAILURE,
                  Parameter1,
                  Parameter2,
                  Parameter3,
                  Parameter4);
}

VOID
AcpipSleep (
    ULONG Milliseconds
    )

/*++

Routine Description:

    This routine delays the current thread's execution by at least the given
    number of milliseconds (the delays can be significantly longer). During this
    time, other threads will run.

Arguments:

    Milliseconds - Supplies the minimum number of milliseconds to delay.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeDelayExecution(FALSE, FALSE, Milliseconds * MICROSECONDS_PER_MILLISECOND);
    return;
}

VOID
AcpipBusySpin (
    ULONG Microseconds
    )

/*++

Routine Description:

    This routine stalls the current processor by the given number of
    microseconds. This routine busy spins, unless preemption occurs no other
    threads will run during this delay.

Arguments:

    Microseconds - Supplies the minimum number of microseconds to delay.

Return Value:

    None.

--*/

{

    HlBusySpin(Microseconds);
    return;
}

ULONGLONG
AcpipGetTimerValue (
    VOID
    )

/*++

Routine Description:

    This routine returns a monotomically non-decreasing value representing the
    number of hundred nanosecond units that have elapsed since some epoch in
    the past (could be system boot).

Arguments:

    None.

Return Value:

    Returns the number of hundred nanosecond units (10^-7 seconds) that have
    elapsed.

--*/

{

    ULONGLONG Frequency;
    ULONGLONG Value;

    Frequency = HlQueryTimeCounterFrequency();
    Value = HlQueryTimeCounter();

    //
    // Scale to hundred nanosecond units.
    //

    Value = (Value * (NANOSECONDS_PER_SECOND / 100)) / Frequency;
    return Value;
}

PVOID
AcpipCreateMutex (
    ULONG SyncLevel
    )

/*++

Routine Description:

    This routine creates an operating system mutex object to back an ACPI mutex
    used in the AML interpreter.

Arguments:

    SyncLevel - Supplies the ACPI-defined sync level of the mutex.

Return Value:

    Returns a pointer to the mutex object on success.

    NULL on failure.

--*/

{

    PACPI_MUTEX Mutex;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Mutex = AcpipAllocateMemory(sizeof(ACPI_MUTEX));
    if (Mutex == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateMutexEnd;
    }

    RtlZeroMemory(Mutex, sizeof(ACPI_MUTEX));
    Mutex->SyncLevel = SyncLevel;
    Mutex->QueuedLock = KeCreateQueuedLock();
    if (Mutex->QueuedLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateMutexEnd;
    }

    Status = STATUS_SUCCESS;

CreateMutexEnd:
    if (!KSUCCESS(Status)) {
        if (Mutex != NULL) {
            if (Mutex->QueuedLock != NULL) {
                KeDestroyQueuedLock(Mutex->QueuedLock);
            }

            AcpipFreeMemory(Mutex);
            Mutex = NULL;
        }
    }

    return Mutex;
}

VOID
AcpipDestroyMutex (
    PVOID Mutex
    )

/*++

Routine Description:

    This routine destroys an operating system mutex object.

Arguments:

    Mutex - Supplies a pointer to the OS mutex object returned during the
        create mutex routine.

Return Value:

    None.

--*/

{

    PACPI_MUTEX AcpiMutex;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Mutex != NULL);

    AcpiMutex = (PACPI_MUTEX)Mutex;

    ASSERT(AcpiMutex->OwningContext == NULL);

    KeDestroyQueuedLock(AcpiMutex->QueuedLock);
    AcpipFreeMemory(AcpiMutex);
    return;
}

BOOL
AcpipAcquireMutex (
    PAML_EXECUTION_CONTEXT Context,
    PVOID Mutex,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine attempts to acquire a mutex object.

Arguments:

    Context - Supplies a pointer to the execution context.

    Mutex - Supplies a pointer to the mutex to acquire.

    TimeoutInMilliseconds - Supplies the number of milliseconds to wait before
        returning anyway and timing out (failing the acquire).

Return Value:

    TRUE if the timeout occurred and the mutex was not acquired.

    FALSE if the mutex was successfully acquired.

--*/

{

    PACPI_MUTEX AcpiMutex;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    AcpiMutex = (PACPI_MUTEX)Mutex;

    //
    // ACPI dictates that mutexes must be acquired in order by sync level.
    // This assert indicates bad firmware has attempted to acquire two mutexes
    // in the wrong order.
    //

    ASSERT(Context->SyncLevel <= AcpiMutex->SyncLevel);

    if (AcpiMutex->OwningContext == Context) {
        AcpiMutex->RecursionCount += 1;
        return TRUE;
    }

    if (TimeoutInMilliseconds == ACPI_MUTEX_WAIT_INDEFINITELY) {
        TimeoutInMilliseconds = WAIT_TIME_INDEFINITE;
    }

    Status = KeAcquireQueuedLockTimed(AcpiMutex->QueuedLock,
                                      TimeoutInMilliseconds);

    if (!KSUCCESS(Status)) {
        return FALSE;
    }

    //
    // Save the previous sync level in the mutex and set the sync level to that
    // of the mutex.
    //

    AcpiMutex->OwningContext = Context;
    AcpiMutex->PreviousSyncLevel = Context->SyncLevel;
    Context->SyncLevel = AcpiMutex->SyncLevel;
    return TRUE;
}

VOID
AcpipReleaseMutex (
    PAML_EXECUTION_CONTEXT Context,
    PVOID Mutex
    )

/*++

Routine Description:

    This routine releases an acquired mutex object. This object must have been
    successfully acquired using the acquire routine.

Arguments:

    Context - Supplies a pointer to the execution context.

    Mutex - Supplies a pointer to the mutex to release.

Return Value:

    None.

--*/

{

    PACPI_MUTEX AcpiMutex;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    AcpiMutex = (PACPI_MUTEX)Mutex;

    //
    // This assert fires when ACPI firmware attempts to release a mutex it
    // never acquired (or release more times than it acquired, as the mutex is
    // recursive).
    //

    ASSERT(AcpiMutex->OwningContext == Context);
    ASSERT(Context->SyncLevel == AcpiMutex->SyncLevel);

    //
    // If this is an inner recursive release, just decrement the count and
    // return.
    //

    if (AcpiMutex->RecursionCount != 0) {

        ASSERT(AcpiMutex->RecursionCount < 0x10000000);

        AcpiMutex->RecursionCount -= 1;
        return;
    }

    //
    // Clear the owning context and restore the sync level. Once this routine
    // is out of the mutex structure, drop the real lock that others are
    // blocked on.
    //

    AcpiMutex->OwningContext = NULL;
    Context->SyncLevel = AcpiMutex->PreviousSyncLevel;
    KeReleaseQueuedLock(AcpiMutex->QueuedLock);
    return;
}

PVOID
AcpipCreateEvent (
    VOID
    )

/*++

Routine Description:

    This routine creates an operating system event object to back an ACPI Event
    used in the AML interpreter.

Arguments:

    None.

Return Value:

    Returns a pointer to the event object on success.

    NULL on failure.

--*/

{

    PKEVENT Event;

    Event = KeCreateEvent(NULL);
    return Event;
}

VOID
AcpipDestroyEvent (
    PVOID Event
    )

/*++

Routine Description:

    This routine destroys an operating system event object.

Arguments:

    Event - Supplies a pointer to the OS event object returned during the
        create event routine.

Return Value:

    None.

--*/

{

    KeDestroyEvent(Event);
    return;
}

BOOL
AcpipWaitForEvent (
    PVOID Event,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine waits at least the specified number of milliseconds for the
    given event object.

Arguments:

    Event - Supplies a pointer to the event to wait for.

    TimeoutInMilliseconds - Supplies the number of milliseconds to wait before
        returning anyway and timing out (failing the wait).

Return Value:

    TRUE if the timeout occurred and the event was not acquired.

    FALSE if execution continued because the event was signaled.

--*/

{

    KSTATUS Status;

    Status = KeWaitForEvent(Event, FALSE, TimeoutInMilliseconds);
    if (!KSUCCESS(Status)) {
        return FALSE;
    }

    return TRUE;
}

VOID
AcpipSignalEvent (
    PVOID Event
    )

/*++

Routine Description:

    This routine signals an event, releasing all parties waiting on it.

Arguments:

    Event - Supplies a pointer to the event to signal.

Return Value:

    None.

--*/

{

    KeSignalEvent(Event, SignalOptionSignalAll);
    return;
}

VOID
AcpipResetEvent (
    PVOID Event
    )

/*++

Routine Description:

    This routine resets an event back to its unsignaled state, causing any
    party who subsequently waits on this event to block.

Arguments:

    Event - Supplies a pointer to the event to unsignal.

Return Value:

    None.

--*/

{

    KeSignalEvent(Event, SignalOptionUnsignal);
    return;
}

KSTATUS
AcpipNotifyOperatingSystem (
    PACPI_OBJECT Object,
    ULONGLONG NotificationValue
    )

/*++

Routine Description:

    This routine is called by executing AML code to notify the operating system
    of something.

Arguments:

    Object - Supplies the object generating the notification. This object will
        be of type Processor, Thermal Zone, or Device.

    NotificationValue - Supplies the type of notification being sent.

Return Value:

    Status code.

--*/

{

    RtlDebugPrint("ACPI: OS Notify 0x%I64x!\n", NotificationValue);

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

VOID
AcpipAcquirePciLock (
    VOID
    )

/*++

Routine Description:

    This routine acquires the PCI lock, used to synchronize early access to
    PCI configuration space with the PCI driver actually coming online.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // This routine is expecting only to be called at low run level, as it
    // does not raise to acquire.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(AcpiPciLock);
    return;
}

VOID
AcpipReleasePciLock (
    VOID
    )

/*++

Routine Description:

    This routine releases the PCI lock, used to synchronize early access to
    PCI configuration space with the PCI driver actually coming online.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // This routine is expecting only to be called at low run level, as it
    // does not raise to acquire.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeReleaseQueuedLock(AcpiPciLock);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

