/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    amlos.h

Abstract:

    This header contains definitions for operating system support functions
    provided to the ACPI AML interpreter and namespace.

Author:

    Evan Green 13-Nov-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define ACPI parameter 1 crash codes.
//

#define ACPI_CRASH_FATAL_INSTRUCTION   0x00000001
#define ACPI_CRASH_GLOBAL_LOCK_FAILURE 0x00000002

//
// Define the acquire mutex wait value that specifies to wait indefinitely.
//

#define ACPI_MUTEX_WAIT_INDEFINITELY 0xFFFF

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
AcpipInitializeOperatingSystemAmlSupport (
    VOID
    );

/*++

Routine Description:

    This routine initializes operating system specific support for the AML
    interpreter.

Arguments:

    None.

Return Value:

    Status code.

--*/

BOOL
AcpipCheckOsiSupport (
    PCSTR String
    );

/*++

Routine Description:

    This routine determines whether or not a given _OSI request is supported.

Arguments:

    String - Supplies a pointer to the string to check.

Return Value:

    TRUE if the implementation supports this feature.

    FALSE if the request is not supported.

--*/

PVOID
AcpipAllocateMemory (
    ULONG Size
    );

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

VOID
AcpipFreeMemory (
    PVOID Allocation
    );

/*++

Routine Description:

    This routine frees memory allocated for the ACPI AML interpreter and
    namespace.

Arguments:

    Allocation - Supplies a pointer to the allocated memory.

Return Value:

    None.

--*/

VOID
AcpipFatalError (
    ULONGLONG Parameter1,
    ULONGLONG Parameter2,
    ULONGLONG Parameter3,
    ULONGLONG Parameter4
    );

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

VOID
AcpipSleep (
    ULONG Milliseconds
    );

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

VOID
AcpipBusySpin (
    ULONG Microseconds
    );

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

ULONGLONG
AcpipGetTimerValue (
    VOID
    );

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

PVOID
AcpipCreateMutex (
    ULONG SyncLevel
    );

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

VOID
AcpipDestroyMutex (
    PVOID Mutex
    );

/*++

Routine Description:

    This routine destroys an operating system mutex object.

Arguments:

    Mutex - Supplies a pointer to the OS mutex object returned during the
        create mutex routine.

Return Value:

    None.

--*/

BOOL
AcpipAcquireMutex (
    PAML_EXECUTION_CONTEXT Context,
    PVOID Mutex,
    ULONG TimeoutInMilliseconds
    );

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

VOID
AcpipReleaseMutex (
    PAML_EXECUTION_CONTEXT Context,
    PVOID Mutex
    );

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

PVOID
AcpipCreateEvent (
    VOID
    );

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

VOID
AcpipDestroyEvent (
    PVOID Event
    );

/*++

Routine Description:

    This routine destroys an operating system event object.

Arguments:

    Event - Supplies a pointer to the OS event object returned during the
        create event routine.

Return Value:

    None.

--*/

BOOL
AcpipWaitForEvent (
    PVOID Event,
    ULONG TimeoutInMilliseconds
    );

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

VOID
AcpipSignalEvent (
    PVOID Event
    );

/*++

Routine Description:

    This routine signals an event, releasing all parties waiting on it.

Arguments:

    Event - Supplies a pointer to the event to signal.

Return Value:

    None.

--*/

VOID
AcpipResetEvent (
    PVOID Event
    );

/*++

Routine Description:

    This routine resets an event back to its unsignaled state, causing any
    party who subsequently waits on this event to block.

Arguments:

    Event - Supplies a pointer to the event to unsignal.

Return Value:

    None.

--*/

KSTATUS
AcpipNotifyOperatingSystem (
    PACPI_OBJECT Object,
    ULONGLONG NotificationValue
    );

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

VOID
AcpipAcquirePciLock (
    VOID
    );

/*++

Routine Description:

    This routine acquires the PCI lock, used to synchronize early access to
    PCI configuration space with the PCI driver actually coming online.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
AcpipReleasePciLock (
    VOID
    );

/*++

Routine Description:

    This routine releases the PCI lock, used to synchronize early access to
    PCI configuration space with the PCI driver actually coming online.

Arguments:

    None.

Return Value:

    None.

--*/

