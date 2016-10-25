/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    efi.h

Abstract:

    This header contains definitions for the UEFI submodule in the hardware
    library.

Author:

    Evan Green 9-Dec-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/uefi/uefi.h>

//
// ---------------------------------------------------------------- Definitions
//

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
HlpEfiResetSystem (
    SYSTEM_RESET_TYPE ResetType
    );

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

KSTATUS
HlpEfiSetTime (
    EFI_TIME *EfiTime
    );

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

KSTATUS
HlpEfiGetVariable (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 *Attributes,
    UINTN *DataSize,
    VOID *Data
    );

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

KSTATUS
HlpEfiSetVariable (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 Attributes,
    UINTN DataSize,
    VOID *Data
    );

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

