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

