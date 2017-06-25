/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsupc.c

Abstract:

    This module implements AMD64 processor architecture features.

Author:

    Evan Green 8-Jun-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x64.h>

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

VOID
ArSetUpUserSharedDataFeatures (
    VOID
    )

/*++

Routine Description:

    This routine initialize the user shared data processor specific features.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // TODO: Set up x64 syscall registers. Maybe not here since there's no
    // test needed for them?
    //

    ASSERT(FALSE);

    return;
}

PFPU_CONTEXT
ArAllocateFpuContext (
    ULONG AllocationTag
    )

/*++

Routine Description:

    This routine allocates a buffer that can be used for FPU context.

Arguments:

    AllocationTag - Supplies the pool allocation tag to use for the allocation.

Return Value:

    Returns a pointer to the newly allocated FPU context on success.

    NULL on allocation failure.

--*/

{

    UINTN AllocationSize;
    PFPU_CONTEXT Context;

    AllocationSize = sizeof(FPU_CONTEXT) + FPU_CONTEXT_ALIGNMENT;
    Context = MmAllocateNonPagedPool(AllocationSize, AllocationTag);
    if (Context == NULL) {
        return NULL;
    }

    //
    // Zero out the buffer to avoid leaking kernel pool to user mode.
    //

    RtlZeroMemory(Context, AllocationSize);
    return Context;
}

VOID
ArDestroyFpuContext (
    PFPU_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys a previously allocated FPU context buffer.

Arguments:

    Context - Supplies a pointer to the context to destroy.

Return Value:

    None.

--*/

{

    MmFreeNonPagedPool(Context);
    return;
}

VOID
ArSetThreadPointer (
    PVOID Thread,
    PVOID NewThreadPointer
    )

/*++

Routine Description:

    This routine sets the new thread pointer value.

Arguments:

    Thread - Supplies a pointer to the thread to set the thread pointer for.

    NewThreadPointer - Supplies the new thread pointer value to set.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;
    PKTHREAD TypedThread;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    TypedThread = Thread;
    TypedThread->ThreadPointer = (UINTN)NewThreadPointer;

    //
    // TODO: Set up x64 thread pointer.
    //

    ASSERT(FALSE);

    if (Thread == KeGetCurrentThread()) {
        ArWriteGsbase(NewThreadPointer);
    }

    KeLowerRunLevel(OldRunLevel);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

