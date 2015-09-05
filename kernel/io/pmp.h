/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    pmp.h

Abstract:

    This header contains internal definitions for the power management
    subsystem.

Author:

    Evan Green 3-Sep-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define PM_ALLOCATION_TAG 0x21727750

//
// Define idle history flags.
//

//
// Set this flag if the idle history will be accessed at or above dispatch
// level or with interrupts disabled.
//

#define IDLE_HISTORY_NON_PAGED 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _DEVICE_POWER_REQUEST {
    DevicePowerRequestInvalid,
    DevicePowerRequestNone,
    DevicePowerRequestIdle,
    DevicePowerRequestSuspend,
    DevicePowerRequestResume,
    DevicePowerRequestMarkActive,
} DEVICE_POWER_REQUEST, *PDEVICE_POWER_REQUEST;

/*++

Structure Description:

    This structure contains the idle history of a device or processor.

Members:

    Shift - Stores the bit shift of the number of buckets. That is, 1 << Shift
        equals the number of buckets in the history.

    NextIndex - Stores the next index to replace.

    Flags - Stores a bitfield of flags about the history. See IDLE_HISTORY_*
        definitions.

    Total - Stores the running total of the data.

    Data - Stores the array of data elements.

--*/

typedef struct _IDLE_HISTORY {
    ULONG Shift;
    ULONG NextIndex;
    ULONG Flags;
    ULONGLONG Total;
    PULONGLONG Data;
} IDLE_HISTORY, *PIDLE_HISTORY;

/*++

Structure Description:

    This structure defines the power management state of a device.

Members:

    State - Stores the current device's power state.

    PreviousState - Stores the previous device power state.

    Request - Stores the current device power request, if the device's power
        state is transitioning.

    ReferenceCount - Stores the number of power references on this device.

    ActiveChildren - Stores the number of active children relying on this
        device. This value has an extra 1 representing the current device's
        reference count.

    TimerQueued - Stores an atomic boolean indicating whether or not the timer
        is currently queued.

    ActiveEvent - Stores a pointer to an event that can be waited on for a
        device to become active.

    IdleTimer - Stores a pointer to a timer used to delay idle transitions.

    IdleDelay - Stores the delay between the last power reference being dropped
        and the idle request being sent.

    IdleTimeout - Stores the absolute timeout, in time counter ticks, when the
        idle timer should expire. This value may occasionally tear, but that's
        an acceptable tradeoff.

    IdleTimerDpc - Stores a pointer to the DPC that's queued when the idle
        timer expires.

    IdleTimerWorkItem - Stores a pointer to the work item that's queued when
        the idle timer DPC runs.

    Irp - Stores a pointer to the IRP used for power requests.

    History - Stores a pointer to the idle history for the device.

    TransitionTime - Stores the time counter when the transition to the current
        state was made.

--*/

struct _DEVICE_POWER {
    DEVICE_POWER_STATE State;
    DEVICE_POWER_STATE PreviousState;
    DEVICE_POWER_REQUEST Request;
    volatile UINTN ReferenceCount;
    volatile UINTN ActiveChildren;
    volatile ULONG TimerQueued;
    PKEVENT ActiveEvent;
    PKTIMER IdleTimer;
    ULONGLONG IdleDelay;
    ULONGLONG IdleTimeout;
    PDPC IdleTimerDpc;
    PWORK_ITEM IdleTimerWorkItem;
    PIRP Irp;
    PIDLE_HISTORY History;
    ULONGLONG TransitionTime;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

VOID
PmpRemoveDevice (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine is called when a device is removed from the system. It cleans
    up the power management state. It is assumed that the device lock is
    already held exclusive.

Arguments:

    Device - Supplies a pointer to the device to remove.

Return Value:

    None.

--*/

VOID
PmpDestroyDevice (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine tears down the power management structures associated with a
    device.

Arguments:

    Device - Supplies a pointer to the device to tear down.

Return Value:

    None.

--*/

VOID
PmpDevicePowerTransition (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine is called by the worker thread to perform a device power
    transition.

Arguments:

    Device - Supplies a pointer to the device to work on.

Return Value:

    None.

--*/

//
// Power optimization functions
//

PIDLE_HISTORY
PmpCreateIdleHistory (
    ULONG Flags,
    ULONG Shift
    );

/*++

Routine Description:

    This routine creates an idle history structure, which tracks the idle
    history of a device or processor.

Arguments:

    Flags - Supplies a bitfield of flags governing the creation and behavior of
        the idle history. See IDLE_HISTORY_* definitions.

    Shift - Supplies the logarithm of the number of history elements to store.
        That is, 1 << Shift will equal the number of history elements stored.

Return Value:

    Returns a pointer to the new history on success.

    NULL on allocation failure.

--*/

VOID
PmpDestroyIdleHistory (
    PIDLE_HISTORY History
    );

/*++

Routine Description:

    This routine destroys an idle history structure.

Arguments:

    History - Supplies a pointer to the idle history to destroy.

Return Value:

    None.

--*/

VOID
PmpIdleHistoryAddDataPoint (
    PIDLE_HISTORY History,
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine adds a datapoint to the running idle history. This routine
    is not synchronized.

Arguments:

    History - Supplies a pointer to the idle history.

    Value - Supplies the new data value to add.

Return Value:

    None.

--*/

ULONGLONG
PmpIdleHistoryGetAverage (
    PIDLE_HISTORY History
    );

/*++

Routine Description:

    This routine returns the running average of the idle history.

Arguments:

    History - Supplies a pointer to the idle history.

Return Value:

    Returns the average idle duration.

--*/

