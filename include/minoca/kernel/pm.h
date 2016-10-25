/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
// Define the maximum length of an idle state name.
//

#define PM_IDLE_STATE_NAME_LENGTH 8

//
// Define the invalid state used to indicate the CPU is active.
//

#define PM_IDLE_STATE_NONE (-1)
#define PM_IDLE_STATE_HALT (-2)

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
    PmInformationIdleStateHandlers,
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
// CPU idle state data types
//

typedef struct _PM_IDLE_STATE_INTERFACE
    PM_IDLE_STATE_INTERFACE, *PPM_IDLE_STATE_INTERFACE;

/*++

Structure Description:

    This structure defines a single CPU idle state that a processor can enter.

Members:

    Name - Stores the name of this idle state, for display purposes.

    Flags - Stores a bitfield of flags describing this state. See
        PM_IDLE_STATE_* definitions.

    Context - Stores a pointer's worth of context that the driver can use to
        store additional data about this state.

    ExitLatency - Stores the amount of time needed to exit this idle state once
        entered, in time counter ticks.

    TargetResidency - Stores the minimum duration to be in this idle state to
        make it worth it to enter, in time counter ticks.

--*/

typedef struct _PM_IDLE_STATE {
    CHAR Name[PM_IDLE_STATE_NAME_LENGTH];
    ULONG Flags;
    PVOID Context;
    ULONGLONG ExitLatency;
    ULONGLONG TargetResidency;
} PM_IDLE_STATE, *PPM_IDLE_STATE;

/*++

Structure Description:

    This structure defines the per processor CPU idle information.

Members:

    States - Stores a pointer to an array of idle states. The CPU idle driver
        fills this in upon initialization.

    StateCount - Stores the number of states in the array. The CPU idle driver
        fills this in upon initialization.

    ProcessorNumber - Stores the software index of this processor. The boot
        processor will always be zero.

    Context - Stores a per processor context pointer the CPU idle driver can
        use to store additional state.

    CurrentState - Stores the current state of the processor. This will be
        initialized to the desired state upon calling enter, and will be
        cleared to PM_IDLE_STATE_NONE when the CPU is active.

--*/

typedef struct _PM_IDLE_PROCESSOR_STATE {
    PPM_IDLE_STATE States;
    ULONG StateCount;
    ULONG ProcessorNumber;
    PVOID Context;
    ULONG CurrentState;
} PM_IDLE_PROCESSOR_STATE, *PPM_IDLE_PROCESSOR_STATE;

typedef
KSTATUS
(*PPM_INITIALIZE_IDLE_STATES) (
    PPM_IDLE_STATE_INTERFACE Interface,
    PPM_IDLE_PROCESSOR_STATE Processor
    );

/*++

Routine Description:

    This routine prototype represents a function that is called to go set up
    idle state information on the current processor. It should set the states
    and state count in the given processor idle information structure. This
    routine is called once on every processor. It runs at dispatch level.

Arguments:

    Interface - Supplies a pointer to the interface.

    Processor - Supplies a pointer to the context for this processor.

Return Value:

    Status code.

--*/

typedef
VOID
(*PPM_ENTER_IDLE_STATE) (
    PPM_IDLE_PROCESSOR_STATE Processor,
    ULONG State
    );

/*++

Routine Description:

    This routine prototype represents a function that is called to go into a
    given idle state on the current processor. This routine is called with
    interrupts disabled, and should return with interrupts disabled.

Arguments:

    Processor - Supplies a pointer to the information for the current processor.

    State - Supplies the new state index to change to.

Return Value:

    None. It is assumed when this function returns that the idle state was
    entered and then exited.

--*/

/*++

Structure Description:

    This structure defines the kernel CPU idle state interface.

Members:

    Flags - Stores a bitfield of flags about the performance state interface.
        See PM_IDLE_STATE_INTERFACE_* definitions.

    InitializeIdleStates - Stores a pointer to a function called on each
        active processor that initializes processor idle state support.

    EnterIdleState - Stores a pointer to a function used to enter an idle state.

    Context - Stores a pointer's worth of context that the interface provider
        can use to get back to its data structures.

--*/

struct _PM_IDLE_STATE_INTERFACE {
    ULONG Flags;
    PPM_INITIALIZE_IDLE_STATES InitializeIdleStates;
    PPM_ENTER_IDLE_STATE EnterIdleState;
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
VOID
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

    None.

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
PmInitializeLibrary (
    VOID
    );

/*++

Routine Description:

    This routine performs global initialization for the power management
    library. It is called towards the end of I/O initialization.

Arguments:

    None.

Return Value:

    None.

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

VOID
PmIdle (
    PPROCESSOR_BLOCK Processor
    );

/*++

Routine Description:

    This routine is called on a processor to go into a low power idle state. If
    no processor idle driver has been registered or processor idle states have
    been disabled, then the processor simply halts waiting for an interrupt.
    This routine is called with interrupts disabled and returns with interrupts
    enabled. This routine should only be called internally by the idle thread.

Arguments:

    Processor - Supplies a pointer to the processor going idle.

Return Value:

    None. This routine returns from the idle state with interrupts enabled.

--*/

