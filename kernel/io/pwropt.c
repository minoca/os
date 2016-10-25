/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pwropt.c

Abstract:

    This module implements support for power management optimizations.

Author:

    Evan Green 4-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
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

PIDLE_HISTORY
PmpCreateIdleHistory (
    ULONG Flags,
    ULONG Shift
    )

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

{

    UINTN AllocationSize;
    PIDLE_HISTORY History;

    AllocationSize = sizeof(IDLE_HISTORY) + ((1 << Shift) * sizeof(ULONGLONG));
    if ((Flags & IDLE_HISTORY_NON_PAGED) != 0) {
        History = MmAllocateNonPagedPool(AllocationSize, PM_ALLOCATION_TAG);

    } else {
        History = MmAllocatePagedPool(AllocationSize, PM_ALLOCATION_TAG);
    }

    if (History == NULL) {
        return NULL;
    }

    RtlZeroMemory(History, AllocationSize);
    History->Flags = Flags;
    History->Shift = Shift;
    History->Data = (PULONGLONG)(History + 1);
    return History;
}

VOID
PmpDestroyIdleHistory (
    PIDLE_HISTORY History
    )

/*++

Routine Description:

    This routine destroys an idle history structure.

Arguments:

    History - Supplies a pointer to the idle history to destroy.

Return Value:

    None.

--*/

{

    if ((History->Flags & IDLE_HISTORY_NON_PAGED) != 0) {
        MmFreeNonPagedPool(History);

    } else {
        MmFreePagedPool(History);
    }

    return;
}

VOID
PmpIdleHistoryAddDataPoint (
    PIDLE_HISTORY History,
    ULONGLONG Value
    )

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

{

    ULONG NextIndex;

    NextIndex = History->NextIndex;
    History->Total -= History->Data[NextIndex];
    History->Total += Value;
    History->Data[NextIndex] = Value;
    NextIndex += 1;
    if (NextIndex == (1 << History->Shift)) {
        NextIndex = 0;
    }

    History->NextIndex = NextIndex;
    return;
}

ULONGLONG
PmpIdleHistoryGetAverage (
    PIDLE_HISTORY History
    )

/*++

Routine Description:

    This routine returns the running average of the idle history.

Arguments:

    History - Supplies a pointer to the idle history.

Return Value:

    Returns the average idle duration.

--*/

{

    //
    // Return the (rounded) total divided by the number of elements.
    //

    return (History->Total + (1 << (History->Shift - 1))) >> History->Shift;
}

//
// --------------------------------------------------------- Internal Functions
//

