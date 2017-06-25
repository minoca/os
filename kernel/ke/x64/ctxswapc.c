/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ctxswapc.c

Abstract:

    This module implements architecture-specific context swapping support
    routines for the AMD64 architecture.

Author:

    Evan Green 11-Jun-2017

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

    PTSS64 Tss;

    ASSERT((KeGetRunLevel() == RunLevelDispatch) ||
           (ArAreInterruptsEnabled() == FALSE));

    Tss = (PTSS64)(ProcessorBlock->Tss);
    Tss->Rsp[0] = (UINTN)NewThread->KernelStack + NewThread->KernelStackSize;

    //
    // If the thread is using the FPU, save it. If the thread was using the FPU
    // but is now context switching in a system call, then abandon the FPU
    // state, as FPU state is volatile across function calls.
    //

    if ((CurrentThread->FpuFlags & THREAD_FPU_FLAG_IN_USE) != 0) {

        //
        // The FPU context could be NULL if a thread got context swapped while
        // terminating.
        //

        if ((CurrentThread->FpuContext != NULL) &&
            ((CurrentThread->Flags & THREAD_FLAG_IN_SYSTEM_CALL) == 0)) {

            //
            // Save the FPU state if it was used this iteration. A thread may
            // be using the FPU in general but not have used it for its
            // duration on this processor, so it would be bad to save in that
            // case.
            //

            if ((CurrentThread->FpuFlags & THREAD_FPU_FLAG_OWNER) != 0) {
                ArSaveFpuState(CurrentThread->FpuContext);
            }

        //
        // The thread is either dying or in a system call, so abandon the FPU
        // context.
        //

        } else {
            CurrentThread->FpuFlags &= ~THREAD_FPU_FLAG_IN_USE;
        }

        CurrentThread->FpuFlags &= ~THREAD_FPU_FLAG_OWNER;
        ArDisableFpu();
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

