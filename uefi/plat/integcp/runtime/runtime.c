/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    runtime.c

Abstract:

    This module implements platform-specific runtime code for the Integrator/CP
    system.

Author:

    Evan Green 7-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "../integfw.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
VOID
EfipIntegratorResetSystem (
    EFI_RESET_TYPE ResetType,
    EFI_STATUS ResetStatus,
    UINTN DataSize,
    VOID *ResetData
    );

//
// -------------------------------------------------------------------- Globals
//

VOID *EfiIntegratorCmBase = (VOID *)INTEGRATOR_CM_BASE;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfiPlatformRuntimeInitialize (
    VOID
    )

/*++

Routine Description:

    This routine performs platform-specific firmware initialization in the
    runtime core driver. The runtime routines are in a separate binary from the
    firmware core routines as they need to be relocated for runtime. This
    routine should perform platform-specific initialization needed to provide
    the core runtime services.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    Status = EfipIntegratorInitializeRtc();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Take over the runtime services. The runtime library recomputes the
    // CRC so there's no need to do it here.
    //

    EfiRuntimeServices->GetTime = EfipIntegratorGetTime;
    EfiRuntimeServices->SetTime = EfipIntegratorSetTime;
    EfiRuntimeServices->GetWakeupTime = EfipIntegratorGetWakeupTime;
    EfiRuntimeServices->SetWakeupTime = EfipIntegratorSetWakeupTime;
    EfiRuntimeServices->ResetSystem = EfipIntegratorResetSystem;
    return EFI_SUCCESS;
}

EFI_STATUS
EfiPlatformReadNonVolatileData (
    VOID *Data,
    UINTN DataSize
    )

/*++

Routine Description:

    This routine reads the EFI variable data from non-volatile storage.

Arguments:

    Data - Supplies a pointer where the platform returns the non-volatile
        data.

    DataSize - Supplies the size of the data to return.

Return Value:

    EFI_SUCCESS if some data was successfully loaded.

    EFI_UNSUPPORTED if the platform does not have non-volatile storage. In this
    case the firmware core saves the non-volatile variables to a file on the
    EFI system partition, and the variable library hopes to catch the same
    variable buffer on reboots to see variable writes that happened at
    runtime.

    EFI_DEVICE_IO_ERROR if a device error occurred during the operation.

    Other error codes on other failures.

--*/

{

    return EFI_UNSUPPORTED;
}

EFI_STATUS
EfiPlatformWriteNonVolatileData (
    VOID *Data,
    UINTN DataSize
    )

/*++

Routine Description:

    This routine writes the EFI variable data to non-volatile storage.

Arguments:

    Data - Supplies a pointer to the data to write.

    DataSize - Supplies the size of the data to write, in bytes.

Return Value:

    EFI_SUCCESS if some data was successfully loaded.

    EFI_UNSUPPORTED if the platform does not have non-volatile storage. In this
    case the firmware core saves the non-volatile variables to a file on the
    EFI system partition, and the variable library hopes to catch the same
    variable buffer on reboots to see variable writes that happened at
    runtime.

    EFI_DEVICE_IO_ERROR if a device error occurred during the operation.

    Other error codes on other failures.

--*/

{

    return EFI_UNSUPPORTED;
}

VOID
EfiPlatformRuntimeExitBootServices (
    VOID
    )

/*++

Routine Description:

    This routine is called in the runtime core driver when the firmware is in
    the process of terminating boot services. The platform can do any work it
    needs to prepare for the imminent termination of boot services.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

VOID
EfiPlatformRuntimeVirtualAddressChange (
    VOID
    )

/*++

Routine Description:

    This routine is called in the runtime core driver when the firmware is
    converting to virtual address mode. It should convert any pointers it's
    got. This routine is called after ExitBootServices, so no EFI boot services
    are available.

Arguments:

    None.

Return Value:

    None.

--*/

{

    EfipIntegratorRtcVirtualAddressChange();
    EfiConvertPointer(0, (VOID **)&EfiIntegratorCmBase);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
VOID
EfipIntegratorResetSystem (
    EFI_RESET_TYPE ResetType,
    EFI_STATUS ResetStatus,
    UINTN DataSize,
    VOID *ResetData
    )

/*++

Routine Description:

    This routine resets the entire platform.

Arguments:

    ResetType - Supplies the type of reset to perform.

    ResetStatus - Supplies the status code for this reset.

    DataSize - Supplies the size of the reset data.

    ResetData - Supplies an optional pointer for reset types of cold, warm, or
        shutdown to a null-terminated string, optionally followed by additional
        binary data.

Return Value:

    None. This routine does not return.

--*/

{

    volatile UINT32 *ControlRegister;

    //
    // Attempt to flush non-volatile variable data out to storage.
    //

    EfiCoreFlushVariableData();
    ControlRegister = EfiIntegratorCmBase + INTEGRATOR_CM_CONTROL;

    //
    // Write the control register with the reset value, even though this
    // causes Qemu to crash.
    //

    *ControlRegister = INTEGRATOR_CM_CONTROL_RESET;
    while (TRUE) {
        if ((*ControlRegister & INTEGRATOR_CM_CONTROL_RESET) != 0) {
            break;
        }
    }

    return;
}

