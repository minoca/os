/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    efi.c

Abstract:

    This module implements EFI runtime firmware services support.

Author:

    Evan Green 9-Dec-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/uefi/uefi.h>
#include <minoca/kernel/bootload.h>
#include "hlp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the EFI runtime services table.
//

EFI_RUNTIME_SERVICES *HlEfiRuntimeServices;
KSPIN_LOCK HlFirmwareLock;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpInitializeEfi (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine initializes UEFI runtime support.

Arguments:

    Parameters - Supplies a pointer to the parameters passed from the loader.

Return Value:

    None.

--*/

{

    KeInitializeSpinLock(&HlFirmwareLock);
    HlEfiRuntimeServices = Parameters->EfiRuntimeServices;
    return;
}

KSTATUS
HlpEfiResetSystem (
    SYSTEM_RESET_TYPE ResetType
    )

/*++

Routine Description:

    This routine calls the EFI reset system runtime service.

Arguments:

    ResetType - Supplies the reset type to perform.

Return Value:

    STATUS_NOT_SUPPORTED if the system is not UEFI or the reset system service
    is not filled in.

    Other status codes on failure.

--*/

{

    EFI_RESET_TYPE EfiResetType;
    BOOL Enabled;
    EFI_RUNTIME_SERVICES *RuntimeServices;
    KSTATUS Status;

    Status = STATUS_NOT_SUPPORTED;
    if (HlEfiRuntimeServices != NULL) {
        switch (ResetType) {
        case SystemResetShutdown:
            EfiResetType = EfiResetShutdown;
            break;

        case SystemResetWarm:
            EfiResetType = EfiResetWarm;
            break;

        case SystemResetCold:
            EfiResetType = EfiResetCold;
            break;

        default:

            ASSERT(FALSE);

            EfiResetType = EfiResetCold;
            break;
        }

        RuntimeServices = HlEfiRuntimeServices;
        if (RuntimeServices->ResetSystem != NULL) {

            //
            // Disable interrupts and acquire the high level lock to serialize
            // with other firmware calls.
            //

            Enabled = ArDisableInterrupts();
            KeAcquireSpinLock(&HlFirmwareLock);

            //
            // Ask the firmware to reset.
            //

            RuntimeServices->ResetSystem(EfiResetType, EFI_SUCCESS, 0, NULL);
            KeReleaseSpinLock(&HlFirmwareLock);
            if (Enabled != FALSE) {
                ArEnableInterrupts();
            }

            //
            // Uh oh, still going. Stall for a little while to give the system
            // some time to actually reset.
            //

            HlBusySpin(RESET_SYSTEM_STALL);
            Status = STATUS_UNSUCCESSFUL;
        }
    }

    return Status;
}

KSTATUS
HlpEfiSetTime (
    EFI_TIME *EfiTime
    )

/*++

Routine Description:

    This routine attempts to set the hardware calendar timer using EFI firmware
    calls.

Arguments:

    EfiTime - Supplies a pointer to the new time to set.

Return Value:

    STATUS_NOT_SUPPORTED if the system is not UEFI or the set time service is
    not implemented.

    Status code.

--*/

{

    EFI_STATUS EfiStatus;
    BOOL Enabled;
    EFI_RUNTIME_SERVICES *RuntimeServices;

    RuntimeServices = HlEfiRuntimeServices;
    if ((RuntimeServices == NULL) || (RuntimeServices->SetTime == NULL)) {
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Convert the calendar time to an EFI time.
    //

    Enabled = ArDisableInterrupts();

    //
    // Perform the EFI runtime services call.
    //

    KeAcquireSpinLock(&HlFirmwareLock);
    EfiStatus = RuntimeServices->SetTime(EfiTime);
    KeReleaseSpinLock(&HlFirmwareLock);
    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    if (!EFI_ERROR(EfiStatus)) {
        return STATUS_SUCCESS;
    }

    if (EfiStatus == EFI_UNSUPPORTED) {
        return STATUS_NO_SUCH_DEVICE;
    }

    RtlDebugPrint("EFI SetTime Failed: 0x%x\n", EfiStatus);
    if (EfiStatus == EFI_INVALID_PARAMETER) {
        return STATUS_INVALID_PARAMETER;

    } else if (EfiStatus == EFI_DEVICE_ERROR) {
        return STATUS_DEVICE_IO_ERROR;
    }

    return STATUS_FIRMWARE_ERROR;
}

KSTATUS
HlpEfiGetVariable (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 *Attributes,
    UINTN *DataSize,
    VOID *Data
    )

/*++

Routine Description:

    This routine returns the value of an EFI variable.

Arguments:

    VariableName - Supplies a pointer to a null-terminated string containing
        the name of the vendor's variable.

    VendorGuid - Supplies a pointer to the unique GUID for the vendor.

    Attributes - Supplies an optional pointer where the attribute mask for the
        variable will be returned.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer. On output, the actual size of the data will be returned.

    Data - Supplies a pointer where the variable value will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_SUPPORTED if the firmware type is not EFI or the get variable
    service is not implemented.

    STATUS_NOT_FOUND if the variable was not found.

    STATUS_BUFFER_TOO_SMALL if the supplied buffer is not big enough.

    STATUS_INVALID_PARAMETER if the variable name, vendor GUID, or data size is
    NULL.

    STATUS_DEVICE_ERROR if a hardware error occurred trying to read the
    variable.

    STATUS_FIRMWARE_ERROR on other EFI failures.

--*/

{

    EFI_STATUS EfiStatus;
    BOOL Enabled;
    EFI_RUNTIME_SERVICES *RuntimeServices;

    RuntimeServices = HlEfiRuntimeServices;
    if ((RuntimeServices == NULL) || (RuntimeServices->GetVariable == NULL)) {
        return STATUS_NOT_SUPPORTED;
    }

    Enabled = ArDisableInterrupts();

    //
    // Perform the EFI runtime services call.
    //

    KeAcquireSpinLock(&HlFirmwareLock);
    EfiStatus = RuntimeServices->GetVariable(VariableName,
                                             VendorGuid,
                                             Attributes,
                                             DataSize,
                                             Data);

    KeReleaseSpinLock(&HlFirmwareLock);
    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    if (!EFI_ERROR(EfiStatus)) {
        return STATUS_SUCCESS;
    }

    if (EfiStatus == EFI_NOT_FOUND) {
        return STATUS_NOT_FOUND;
    }

    if (EfiStatus == EFI_BUFFER_TOO_SMALL) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (EfiStatus == EFI_INVALID_PARAMETER) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlDebugPrint("EFI GetVariable Failed: 0x%x\n", EfiStatus);
    if (EfiStatus == EFI_DEVICE_ERROR) {
        return STATUS_DEVICE_IO_ERROR;
    }

    return STATUS_FIRMWARE_ERROR;
}

KSTATUS
HlpEfiSetVariable (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 Attributes,
    UINTN DataSize,
    VOID *Data
    )

/*++

Routine Description:

    This routine sets the value of an EFI variable using runtime services.

Arguments:

    VariableName - Supplies a pointer to a null-terminated string containing
        the name of the vendor's variable. Each variable name is unique for a
        particular vendor GUID. A variable name must be at least one character
        in length.

    VendorGuid - Supplies a pointer to the unique GUID for the vendor.

    Attributes - Supplies the attributes for this variable. See EFI_VARIABLE_*
        definitions.

    DataSize - Supplies the size of the data buffer. Unless the
        EFI_VARIABLE_APPEND_WRITE, EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS, or
        EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS attribute is set, a
        size of zero causes the variable to be deleted. When the
        EFI_VARIABLE_APPEND_WRITE attribute is set, then a set variable call
        with a data size of zero will not cause any change to the variable
        value (the timestamp associated with the variable may be updated
        however even if no new data value is provided,see the description of
        the EFI_VARIABLE_AUTHENTICATION_2 descriptor below. In this case the
        data size will not be zero since the EFI_VARIABLE_AUTHENTICATION_2
        descriptor will be populated).

    Data - Supplies the contents of the variable.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if the variable being updated or deleted was not found.

    STATUS_INVALID_PARAMETER if an invalid combination of attribute bits, name,
    and GUID was suplied, data size exceeds the maximum, or the variable name
    is an empty string.

    STATUS_DEVICE_IO_ERROR if a hardware error occurred trying to access the
    variable.

    STATUS_FIRMWARE_ERROR on other EFI failures.

--*/

{

    EFI_STATUS EfiStatus;
    BOOL Enabled;
    EFI_RUNTIME_SERVICES *RuntimeServices;

    RuntimeServices = HlEfiRuntimeServices;
    if ((RuntimeServices == NULL) || (RuntimeServices->SetVariable == NULL)) {
        return STATUS_NOT_SUPPORTED;
    }

    Enabled = ArDisableInterrupts();

    //
    // Perform the EFI runtime services call.
    //

    KeAcquireSpinLock(&HlFirmwareLock);
    EfiStatus = RuntimeServices->SetVariable(VariableName,
                                             VendorGuid,
                                             Attributes,
                                             DataSize,
                                             Data);

    KeReleaseSpinLock(&HlFirmwareLock);
    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    if (!EFI_ERROR(EfiStatus)) {
        return STATUS_SUCCESS;
    }

    if (EfiStatus == EFI_NOT_FOUND) {
        return STATUS_NOT_FOUND;
    }

    if (EfiStatus == EFI_BUFFER_TOO_SMALL) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (EfiStatus == EFI_INVALID_PARAMETER) {
        return STATUS_INVALID_PARAMETER;
    }

    if (EfiStatus == EFI_WRITE_PROTECTED) {
        return STATUS_ACCESS_DENIED;
    }

    RtlDebugPrint("EFI GetVariable Failed: 0x%x\n", EfiStatus);
    if (EfiStatus == EFI_DEVICE_ERROR) {
        return STATUS_DEVICE_IO_ERROR;
    }

    return STATUS_FIRMWARE_ERROR;
}

//
// --------------------------------------------------------- Internal Functions
//

