/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    pminfo.c

Abstract:

    This module implements support for getting and setting power management
    information.

Author:

    Evan Green 9-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "pmp.h"

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
// ------------------------------------------------------------------ Functions
//

KSTATUS
PmGetSetSystemInformation (
    BOOL FromKernelMode,
    PM_INFORMATION_TYPE InformationType,
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
    case PmInformationPerformanceStateHandlers:
        Status = PmpGetSetPerformanceStateHandlers(FromKernelMode,
                                                   Data,
                                                   DataSize,
                                                   Set);

        break;

    case PmInformationIdleStateHandlers:
        Status = PmpGetSetIdleStateHandlers(FromKernelMode,
                                            Data,
                                            DataSize,
                                            Set);

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

