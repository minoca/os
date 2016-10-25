/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    info.c

Abstract:

    This module handles getting and setting system information calls.

Author:

    Evan Green 9-Dec-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "hlp.h"
#include "efi.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpGetSetEfiVariable (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlGetSetSystemInformation (
    BOOL FromKernelMode,
    HL_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

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

{

    KSTATUS Status;

    switch (InformationType) {
    case HlInformationEfiVariable:
        Status = HlpGetSetEfiVariable(Data, DataSize, Set);
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        *DataSize = 0;
        break;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpGetSetEfiVariable (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets an EFI variable via runtime services.

Arguments:

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    PHL_EFI_VARIABLE_INFORMATION Information;
    PHL_EFI_VARIABLE_INFORMATION NonPagedInformation;
    KSTATUS Status;
    PVOID VariableData;
    UINT16 *VariableName;

    NonPagedInformation = NULL;
    Status = PsCheckPermission(PERMISSION_SYSTEM_ADMINISTRATOR);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Information = Data;
    if (*DataSize < sizeof(HL_EFI_VARIABLE_INFORMATION)) {
        *DataSize = sizeof(HL_EFI_VARIABLE_INFORMATION);
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    if ((Information->VariableNameSize == 0) ||
        ((Information->VariableNameSize % sizeof(UINT16)) != 0)) {

        Status = STATUS_INVALID_PARAMETER;
        goto GetSetEfiVariableEnd;
    }

    if ((sizeof(HL_EFI_VARIABLE_INFORMATION) + Information->VariableNameSize +
         Information->DataSize) >
        *DataSize) {

        Status = STATUS_INVALID_PARAMETER;
        goto GetSetEfiVariableEnd;
    }

    //
    // Create a copy of the variable information in non-paged pool.
    //

    NonPagedInformation = MmAllocateNonPagedPool(*DataSize, HL_POOL_TAG);
    if (NonPagedInformation == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetSetEfiVariableEnd;
    }

    RtlCopyMemory(NonPagedInformation, Information, *DataSize);
    VariableName = (UINT16 *)(NonPagedInformation + 1);
    VariableData = ((PUCHAR)VariableName) +
                   NonPagedInformation->VariableNameSize;

    if (Set != FALSE) {
        Status = HlpEfiSetVariable(
                                VariableName,
                                (EFI_GUID *)&(NonPagedInformation->VendorGuid),
                                NonPagedInformation->Attributes,
                                NonPagedInformation->DataSize,
                                VariableData);

    } else {
        Status = HlpEfiGetVariable(
                                VariableName,
                                (EFI_GUID *)&(NonPagedInformation->VendorGuid),
                                (UINT32 *)&(NonPagedInformation->Attributes),
                                &(NonPagedInformation->DataSize),
                                VariableData);
    }

    RtlCopyMemory(Information, NonPagedInformation, *DataSize);

GetSetEfiVariableEnd:
    if (NonPagedInformation != NULL) {
        MmFreeNonPagedPool(NonPagedInformation);
    }

    return Status;
}

