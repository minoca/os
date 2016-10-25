/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtlib.h

Abstract:

    This header contains internal definitions for the UEFI runtime library.

Author:

    Evan Green 18-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "shortcut.h"

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

//
// Variable services
//

EFIAPI
EFI_STATUS
EfiCoreSetVariable (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 Attributes,
    UINTN DataSize,
    VOID *Data
    );

/*++

Routine Description:

    This routine sets the value of a variable.

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

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the variable being updated or deleted was not found.

    EFI_INVALID_PARAMETER if an invalid combination of attribute bits, name,
    and GUID was suplied, data size exceeds the maximum, or the variable name
    is an empty string.

    EFI_DEVICE_ERROR if a hardware error occurred trying to access the variable.

    EFI_WRITE_PROTECTED if the variable is read-only or cannot be deleted.

    EFI_SECURITY_VIOLATION if variable could not be written due to
    EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS or
    EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACESS being set, but the
    authorization information does NOT pass the validation check carried out by
    the firmware.

--*/

EFIAPI
EFI_STATUS
EfiCoreGetNextVariableName (
    UINTN *VariableNameSize,
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid
    );

/*++

Routine Description:

    This routine enumerates the current variable names.

Arguments:

    VariableNameSize - Supplies a pointer that on input contains the size of
        the variable name buffer. On output, will contain the size of the
        variable name.

    VariableName - Supplies a pointer that on input contains the last variable
        name that was returned. On output, returns the null terminated string
        of the current variable.

    VendorGuid - Supplies a pointer that on input contains the last vendor GUID
        returned by this routine. On output, returns the vendor GUID of the
        current variable.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the next variable was not found.

    EFI_BUFFER_TOO_SMALL if the supplied buffer is not big enough.

    EFI_INVALID_PARAMETER if the variable name, vendor GUID, or data size is
    NULL.

    EFI_DEVICE_ERROR if a hardware error occurred trying to read the variable.

--*/

EFIAPI
EFI_STATUS
EfiCoreGetVariable (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 *Attributes,
    UINTN *DataSize,
    VOID *Data
    );

/*++

Routine Description:

    This routine returns the value of a variable.

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

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the variable was not found.

    EFI_BUFFER_TOO_SMALL if the supplied buffer is not big enough.

    EFI_INVALID_PARAMETER if the variable name, vendor GUID, or data size is
    NULL.

    EFI_DEVICE_ERROR if a hardware error occurred trying to read the variable.

    EFI_SECURITY_VIOLATION if the variable could not be retrieved due to an
    authentication failure.

--*/

EFIAPI
EFI_STATUS
EfiCoreQueryVariableInfo (
    UINT32 Attributes,
    UINT64 *MaximumVariableStorageSize,
    UINT64 *RemainingVariableStorageSize,
    UINT64 *MaximumVariableSize
    );

/*++

Routine Description:

    This routine returns information about EFI variables.

Arguments:

    Attributes - Supplies a bitmask of attributes specifying the type of
        variables on which to return information.

    MaximumVariableStorageSize - Supplies a pointer where the maximum size of
        storage space for EFI variables with the given attributes will be
        returned.

    RemainingVariableStorageSize - Supplies a pointer where the remaining size
        of the storage space available for EFI variables associated with the
        attributes specified will be returned.

    MaximumVariableSize - Supplies a pointer where the maximum size of an
        individual variable will be returned on success.

Return Value:

    EFI_SUCCESS if a valid answer was returned.

    EFI_UNSUPPORTED if the attribute is not supported on this platform.

    EFI_INVALID_PARAMETER if an invalid combination of attributes was supplied.

--*/

EFI_STATUS
EfipCoreInitializeVariableServices (
    VOID
    );

/*++

Routine Description:

    This routine initialize core variable services.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

VOID
EfipCoreVariableHandleExitBootServices (
    VOID
    );

/*++

Routine Description:

    This routine is called when leaving boot services.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipCoreVariableHandleVirtualAddressChange (
    VOID
    );

/*++

Routine Description:

    This routine is called to change from physical to virtual mode.

Arguments:

    None.

Return Value:

    None.

--*/

//
// Utility functions
//

EFIAPI
VOID
EfiCoreCopyMemory (
    VOID *Destination,
    VOID *Source,
    UINTN Length
    );

/*++

Routine Description:

    This routine copies the contents of one buffer to another.

Arguments:

    Destination - Supplies a pointer to the destination of the copy.

    Source - Supplies a pointer to the source of the copy.

    Length - Supplies the number of bytes to copy.

Return Value:

    None.

--*/

EFIAPI
VOID
EfiCoreSetMemory (
    VOID *Buffer,
    UINTN Size,
    UINT8 Value
    );

/*++

Routine Description:

    This routine fills a buffer with a specified value.

Arguments:

    Buffer - Supplies a pointer to the buffer to fill.

    Size - Supplies the size of the buffer in bytes.

    Value - Supplies the value to fill the buffer with.

Return Value:

    None.

--*/

INTN
EfiCoreCompareMemory (
    VOID *FirstBuffer,
    VOID *SecondBuffer,
    UINTN Length
    );

/*++

Routine Description:

    This routine compares the contents of two buffers for equality.

Arguments:

    FirstBuffer - Supplies a pointer to the first buffer to compare.

    SecondBuffer - Supplies a pointer to the second buffer to compare.

    Length - Supplies the number of bytes to compare.

Return Value:

    0 if the buffers are identical.

    Returns the first mismatched byte as
    First[MismatchIndex] - Second[MismatchIndex].

--*/

BOOLEAN
EfiCoreCompareGuids (
    EFI_GUID *FirstGuid,
    EFI_GUID *SecondGuid
    );

/*++

Routine Description:

    This routine compares two GUIDs.

Arguments:

    FirstGuid - Supplies a pointer to the first GUID.

    SecondGuid - Supplies a pointer to the second GUID.

Return Value:

    TRUE if the GUIDs are equal.

    FALSE if the GUIDs are different.

--*/

UINTN
EfiCoreStringLength (
    CHAR16 *String
    );

/*++

Routine Description:

    This routine returns the length of the given string, in characters (not
    bytes).

Arguments:

    String - Supplies a pointer to the string.

Return Value:

    Returns the number of characters in the string.

--*/

EFIAPI
EFI_STATUS
EfiCoreCalculateCrc32 (
    VOID *Data,
    UINTN DataSize,
    UINT32 *Crc32
    );

/*++

Routine Description:

    This routine computes the 32-bit CRC for a data buffer.

Arguments:

    Data - Supplies a pointer to the buffer to compute the CRC on.

    DataSize - Supplies the size of the data buffer in bytes.

    Crc32 - Supplies a pointer where the 32-bit CRC will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if any parameter is NULL, or the data size is zero.

--*/

