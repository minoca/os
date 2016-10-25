/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    contextc.c

Abstract:

    This module implements C support for working with ucontext structures in
    the C library.

Author:

    Evan Green 9-Sep-2016

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../libcp.h"
#include <alloca.h>
#include <errno.h>
#include <signal.h>
#include <ucontext.h>
#include <minoca/kernel/arm.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

__NO_RETURN
VOID
ClpContextEnd (
    ucontext_t *Context
    );

__NO_RETURN
void
ClpContextStart (
    void (*StartFunction)(),
    ...
    );

VOID
ClpSaveVfp (
    PFPU_CONTEXT Context,
    BOOL SimdSupport
    );

VOID
ClpRestoreVfp (
    PFPU_CONTEXT Context,
    BOOL SimdSupport
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
void
makecontext (
    ucontext_t *Context,
    void (*StartFunction)(),
    int ArgumentCount,
    ...
    )

/*++

Routine Description:

    This routine modifies an initialized context to call the function provided
    with the given arguments.

Arguments:

    Context - Supplies a pointer to the context.

    StartFunction - Supplies a pointer to the function to call when the
        context is restored.

    ArgumentCount - Supplies the number of int sized arguments supplied.

    ... - Supplies the remaining arguments to pass to the function.

Return Value:

    None.

--*/

{

    PUINTN Argument;
    va_list ArgumentList;
    UINTN Index;
    UINTN Minimum;
    PVOID StackTop;
    PTRAP_FRAME TrapFrame;

    if (Context == NULL) {
        return;
    }

    //
    // Create a stack that looks like this (starting with the last pushed):
    // ClpContextStart
    // StartFunction
    // Argument1 (16 byte aligned)
    // ...
    // ArgumentN
    // Context.
    //

    StackTop = (PVOID)(Context->uc_stack.ss_sp + Context->uc_stack.ss_size -
                       sizeof(UINTN));

    //
    // At a minimum, push arguments to account for all the register passed
    // arguments.
    //

    Minimum = 4;
    if (ArgumentCount > Minimum) {
        Minimum = ArgumentCount;
    }

    StackTop -= (Minimum + 1) * sizeof(UINTN);
    StackTop = ALIGN_POINTER_DOWN(StackTop, 16);
    StackTop -= 2 * sizeof(UINTN);
    Argument = (PUINTN)StackTop;
    *Argument = (UINTN)ClpContextStart;
    Argument += 1;
    *Argument = (UINTN)StartFunction;
    Argument += 1;
    va_start(ArgumentList, ArgumentCount);
    for (Index = 0; Index < ArgumentCount; Index += 1) {
        *Argument = va_arg(ArgumentList, UINTN);
        Argument += 1;
    }

    va_end(ArgumentList);

    //
    // If there are fewer argumens than passed via register, pad it out.
    //

    while (Index < Minimum) {
        *Argument = 0;
        Argument += 1;
        Index += 1;
    }

    //
    // Make sure the stack is aligned.
    //

    if ((Index & 0x1) != 0) {
        *Argument = 0;
        Argument += 1;
    }

    *Argument = (UINTN)Context;

    //
    // Set the registers to point at the top of the stack.
    //

    TrapFrame = (PTRAP_FRAME)&(Context->uc_mcontext.gregs);
    TrapFrame->R4 = (UINTN)Argument;
    TrapFrame->R11 = 0;
    TrapFrame->R7 = 0;
    TrapFrame->UserSp = (UINTN)StackTop + sizeof(PVOID);
    TrapFrame->Pc = (UINTN)ClpContextStart;
    TrapFrame->Cpsr &= ~(PSR_FLAG_IT_STATE | PSR_FLAG_THUMB);
    if ((TrapFrame->Pc & ARM_THUMB_BIT) != 0) {
        TrapFrame->Cpsr |= PSR_FLAG_THUMB;
    }

    return;
}

int
ClpGetContext (
    ucontext_t *Context,
    void *StackPointer
    )

/*++

Routine Description:

    This routine stores the current FPU and general context into the given
    structure. The assembly code that calls this routine is responsible for
    saving the general registers.

Arguments:

    Context - Supplies a pointer to the context.

    StackPointer - Supplies the current stack pointer.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    BOOL Aligned;
    int Error;
    PFPU_CONTEXT FpuContext;
    BOOL SimdSupport;
    void *StackBase;
    size_t StackSize;
    pthread_attr_t ThreadAttribute;

    Error = pthread_getattr_np(pthread_self(), &ThreadAttribute);
    if (Error != 0) {
        errno = Error;
        return -1;
    }

    Error = pthread_attr_getstack(&ThreadAttribute, &StackBase, &StackSize);
    if (Error != 0) {
        errno = Error;
        return -1;
    }

    if (StackBase == NULL) {
        StackBase = StackPointer;
    }

    Context->uc_flags = 0;
    Context->uc_stack.ss_sp = StackPointer;
    Context->uc_stack.ss_flags = 0;
    Context->uc_stack.ss_size = StackSize;

    //
    // TODO: Enable this when sigaltstack is implemented.
    //

#if 0

    //
    // If currently on the signal stack, then the thread parameters aren't
    // correct.
    //

    if (sigaltstack(NULL, &SignalStack) == 0) {
        if ((SignalStack.ss_flags & SS_ONSTACK) != 0) {
            Context->uc_stack = SignalStack;
        }
    }

#endif

    sigprocmask(0, NULL, &(Context->uc_sigmask));

    //
    // Save the floating point context if it exists. If it's not aligned,
    // it will have to be saved into an aligned buffer and then copied.
    //

    if (OsTestProcessorFeature(OsArmVfp) != FALSE) {
        Context->uc_flags |= SIGNAL_CONTEXT_FLAG_FPU_VALID;
        SimdSupport = OsTestProcessorFeature(OsArmNeon32);
        Aligned = IS_POINTER_ALIGNED(&(Context->uc_mcontext.fpregs),
                                     __FPSTATE_ALIGNMENT);

        if (Aligned != FALSE) {
            FpuContext = (PFPU_CONTEXT)&(Context->uc_mcontext.fpregs);

        } else {
            FpuContext = alloca(__FPSTATE_SIZE + __FPSTATE_ALIGNMENT);
            FpuContext = ALIGN_POINTER_UP(FpuContext, __FPSTATE_ALIGNMENT);
        }

        //
        // Save the floating point state.
        //

        ClpSaveVfp(FpuContext, SimdSupport);
        if (FpuContext != (PFPU_CONTEXT)&(Context->uc_mcontext.fpregs)) {
            memcpy(&(Context->uc_mcontext.fpregs),
                   FpuContext,
                   sizeof(Context->uc_mcontext.fpregs));
        }
    }

    pthread_attr_destroy(&ThreadAttribute);
    return 0;
}

void
ClpSetContext (
    const ucontext_t *Context
    )

/*++

Routine Description:

    This routine restores the user context set in the given structure.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    None.

--*/

{

    PFPU_CONTEXT FpuContext;
    BOOL SimdSupport;

    //
    // Restore the floating point support if it exists.
    //

    if (((Context->uc_flags & SIGNAL_CONTEXT_FLAG_FPU_VALID) != 0) &&
        (OsTestProcessorFeature(OsArmVfp) != FALSE)) {

        SimdSupport = OsTestProcessorFeature(OsArmNeon32);

        //
        // If the structure causes the floating point context not to be aligned,
        // allocate a temporary structure, align it, and copy the data in.
        //

        FpuContext = (PFPU_CONTEXT)&(Context->uc_mcontext.fpregs);
        if (!IS_POINTER_ALIGNED(FpuContext, __FPSTATE_ALIGNMENT)) {
            FpuContext = alloca(__FPSTATE_SIZE + __FPSTATE_ALIGNMENT);
            FpuContext = ALIGN_POINTER_UP(FpuContext, __FPSTATE_ALIGNMENT);
            memcpy(FpuContext,
                   &(Context->uc_mcontext.fpregs),
                   sizeof(FPU_CONTEXT));
        }

        //
        // Restore the floating point context using the appropriate mechanism.
        //

        ClpRestoreVfp(FpuContext, SimdSupport);
    }

    sigprocmask(SIG_SETMASK, &(Context->uc_sigmask), NULL);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

