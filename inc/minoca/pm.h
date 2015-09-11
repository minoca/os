/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    pm.h

Abstract:

    This header contains definitions for the power management subsystem.

Author:

    Evan Green 3-Sep-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define performance state interface flags.
//

//
// Set this flag if the interface is per-processor, in which case target
// changes will be called at dispatch level. If this flag is not set, then
// P-state management is assumed to be global and will get called at low-level
// to affect changes.
//

#define PM_PERFORMANCE_STATE_PER_PROCESSOR 0x00000001

//
// Define the total weight of all the states.
//

#define PM_PERFORMANCE_STATE_WEIGHT_TOTAL 1024
#define PM_PERFORMANCE_STATE_WEIGHT_SHIFT 10

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _DEVICE_POWER_STATE {
    DevicePowerStateInvalid,
    DevicePowerStateActive,
    DevicePowerStateTransitioning,
    DevicePowerStateIdle,
    DevicePowerStateSuspended,
    DevicePowerStateRemoved
} DEVICE_POWER_STATE, *PDEVICE_POWER_STATE;

typedef enum _PM_INFORMATION_TYPE {
    PmInformationInvalid,
    PmInformationPerformanceStateHandlers,
} PM_INFORMATION_TYPE, *PPM_INFORMATION_TYPE;

typedef struct _PM_PERFORMANCE_STATE_INTERFACE
    PM_PERFORMANCE_STATE_INTERFACE, *PPM_PERFORMANCE_STATE_INTERFACE;

/*++

Structure Description:

    This structure defines a particular processor performance state. It is
    assumed that all CPUs in the system can switch to this state if performance
    state is per-CPU.

Members:

    Frequency - Stores the CPU frequency of this state in kilo Hertz.

    Weight - Stores the weight to associate with this state. That is, how
        much of the range of possible loads fall into this state. The total of
        all possible weights should equal 1024 (or close to it). For example,
        if there are 4 possible performance states, and they all have equal
        weights, then performance state 1 will be used at a load of < 25%,
        2 at < 50%, 3 at < 75%, and 4 otherwise. If they have weights of 170,
        170, 172, and 512, then state 4 will be used for any load above 50%.

--*/

typedef struct _PM_PERFORMANCE_STATE {
    ULONG Frequency;
    ULONG Weight;
} PM_PERFORMANCE_STATE, *PPM_PERFORMANCE_STATE;

typedef
KSTATUS
(*PPM_SET_PERFORMANCE_STATE) (
    PPM_PERFORMANCE_STATE_INTERFACE Interface,
    ULONG State
    );

/*++

Routine Description:

    This routine prototype represents a function that is called to change the
    current performance state. If the performance state interface is
    per-processor, then this routine is called at dispatch level on the
    processor to change. If performance state changes are global, then this
    routine is called at low level (and therefore on any processor).

Arguments:

    Interface - Supplies a pointer to the interface.

    State - Supplies the new state index to change to.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure defines the kernel performance state interface.

Members:

    Flags - Stores a bitfield of flags about the performance state interface.
        See PM_PERFORMANCE_STATE_* definitions.

    MinimumPeriod - Stores the minimum period in time counter ticks to
        re-evaluate performance state data. This should be set to about the
        amount of time it takes to affect a performance state change.

    States - Stores a pointer to an array of possible performance states. The
        interface must not modify this pointer or array without synchronizing
        with the kernel.

    StateCount - Stores the number of states in the array.

    SetPerformanceState - Stores a pointer to a function used to change to a
        new performance state.

    Context - Stores a pointer's worth of context that the interface provider
        can use to get back to its data structures.

--*/

struct _PM_PERFORMANCE_STATE_INTERFACE {
    ULONG Flags;
    ULONGLONG MinimumPeriod;
    PPM_PERFORMANCE_STATE States;
    ULONG StateCount;
    PPM_SET_PERFORMANCE_STATE SetPerformanceState;
    PVOID Context;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KERNEL_API
KSTATUS
PmInitialize (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine initializes power management infrastructure for a given
    device.

Arguments:

    Device - Supplies a pointer to the device to prepare to do power management
        calls on.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
PmDeviceAddReference (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine adds a power management reference on the given device,
    and waits for the device to transition to the active state.

Arguments:

    Device - Supplies a pointer to the device to add a power reference to.

Return Value:

    Status code. On failure, the caller will not have a reference on the
    device, and should not assume that the device or its parent lineage is
    active.

--*/

KERNEL_API
KSTATUS
PmDeviceAddReferenceAsynchronous (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine adds a power management reference on the given device,
    preventing the device from idling until the reference is released.

Arguments:

    Device - Supplies a pointer to the device to add a power reference to.

Return Value:

    Status code indicating if the request was successfully queued. On failure,
    the caller will not have the reference on the device.

--*/

KERNEL_API
KSTATUS
PmDeviceReleaseReference (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine releases a power management reference on a device.

Arguments:

    Device - Supplies a pointer to the device to subtract a power reference
        from.

Return Value:

    Status code indicating if the idle timer was successfully queued. The
    reference itself is always dropped, even on failure.

--*/

KERNEL_API
KSTATUS
PmDeviceSetState (
    PDEVICE Device,
    DEVICE_POWER_STATE PowerState
    );

/*++

Routine Description:

    This routine sets a new power state for the device. This can be used to
    clear an error. It should not be called from a power IRP.

Arguments:

    Device - Supplies a pointer to the device to set the power state for.

    PowerState - Supplies the new power management state to set.

Return Value:

    Status code.

--*/

KSTATUS
PmGetSetSystemInformation (
    BOOL FromKernelMode,
    PM_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets system information.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    InformationType - Supplies the information type.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

