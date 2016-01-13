/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    ctxswapc.c

Abstract:

    This module implements context swapping support routines.

Author:

    Evan Green 25-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/arm.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
KepArchPrepareForContextSwap (
    PPROCESSOR_BLOCK ProcessorBlock,
    PKTHREAD CurrentThread,
    PKTHREAD NewThread
    )

/*++

Routine Description:

    This routine performs any architecture specific work before context swapping
    between threads. This must be called at dispatch level.

Arguments:

    ProcessorBlock - Supplies a pointer to the processor block of the current
        processor.

    CurrentThread - Supplies a pointer to the current (old) thread.

    NewThread - Supplies a pointer to the thread that's about to be switched to.

Return Value:

    None.

--*/

{

    PULONG StorageAddress;

    ASSERT((KeGetRunLevel() == RunLevelDispatch) ||
           (ArAreInterruptsEnabled() == FALSE));

    //
    // Store the user read/write thread pointer in the upper 32-bits.
    //

    StorageAddress = ((PULONG)&(CurrentThread->ThreadPointer)) + 1;
    *StorageAddress = ArGetThreadPointerUser();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

