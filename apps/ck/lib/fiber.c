/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    fiber.c

Abstract:

    This module implements support for fibers, which are units of concurrency
    in Chalk.

Author:

    Evan Green 17-Jul-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "chalkp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpFiberReset (
    PCK_VM Vm,
    PCK_FIBER Fiber,
    PCK_CLOSURE Closure
    );

BOOL
CkpFiberAbort (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpFiberRun (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpFiberCurrent (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpFiberError (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpFiberIsDone (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpFiberSuspend (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpFiberTransfer (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpFiberTransferError (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpFiberYield (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

VOID
CkpRunFiber (
    PCK_VM Vm,
    PCK_FIBER Fiber,
    PCK_VALUE Arguments,
    BOOL IsCall,
    PSTR Verb
    );

//
// -------------------------------------------------------------------- Globals
//

CK_PRIMITIVE_DESCRIPTION CkFiberPrimitives[] = {
    {"run@1", CkpFiberRun},
    {"error@0", CkpFiberError},
    {"isDone@0", CkpFiberIsDone},
    {"transfer@1", CkpFiberTransfer},
    {"transferError@1", CkpFiberTransferError},
    {NULL, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkFiberStaticPrimitives[] = {
    {"abort@1", CkpFiberAbort},
    {"current@0", CkpFiberCurrent},
    {"suspend@0", CkpFiberSuspend},
    {"yield@1", CkpFiberYield},
    {NULL, NULL}
};

//
// ------------------------------------------------------------------ Functions
//

PCK_FIBER
CkpFiberCreate (
    PCK_VM Vm,
    PCK_CLOSURE Closure
    )

/*++

Routine Description:

    This routine creates a new fiber object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Closure - Supplies a pointer to the closure to execute.

Return Value:

    Returns a pointer to the new fiber on success.

    NULL on allocation failure.

--*/

{

    PCK_CALL_FRAME CallFrames;
    PCK_FIBER Fiber;
    PCK_VALUE Stack;
    UINTN StackCapacity;

    //
    // Allocate the call frames first in case it triggers a garbage collection.
    //

    CallFrames = CkAllocate(Vm, sizeof(CK_CALL_FRAME) * CK_INITIAL_CALL_FRAMES);
    if (CallFrames == NULL) {
        return NULL;
    }

    StackCapacity = CK_INITIAL_CALL_FRAMES;
    if (Closure != NULL) {
        while (StackCapacity < Closure->Function->MaxStack + 1) {
            StackCapacity <<= 1;
        }
    }

    Stack = CkAllocate(Vm, StackCapacity * sizeof(CK_VALUE));
    if (Stack == NULL) {
        CkFree(Vm, CallFrames);
        return NULL;
    }

    Fiber = CkAllocate(Vm, sizeof(CK_FIBER));
    if (Fiber == NULL) {
        CkFree(Vm, CallFrames);
        CkFree(Vm, Stack);
    }

    CkpInitializeObject(Vm, &(Fiber->Header), CkObjectFiber, Vm->Class.Fiber);
    Fiber->Frames = CallFrames;
    Fiber->FrameCapacity = CK_INITIAL_CALL_FRAMES;
    Fiber->Stack = Stack;
    Fiber->StackCapacity = StackCapacity;
    CkpFiberReset(Vm, Fiber, Closure);
    return Fiber;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpFiberReset (
    PCK_VM Vm,
    PCK_FIBER Fiber,
    PCK_CLOSURE Closure
    )

/*++

Routine Description:

    This routine reinitializes a fiber object for fresh execution.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the fiber to reset.

    Closure - Supplies a pointer to the closure to execute.

Return Value:

    None.

--*/

{

    Fiber->StackTop = Fiber->Stack;
    Fiber->OpenUpvalues = NULL;
    Fiber->Caller = NULL;
    Fiber->Error = CK_NULL_VALUE;
    Fiber->FrameCount = 0;
    return;
}

//
// Primitive functions that implement the methods of the Fiber class.
//

BOOL
CkpFiberAbort (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine throws a runtime error in the current fiber.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    Vm->Fiber->Error = Arguments[1];

    //
    // If the caller passed null, then don't actually abort.
    //

    if (CK_IS_NULL(Arguments[1])) {
        return TRUE;
    }

    //
    // Abort.
    //

    return FALSE;
}

BOOL
CkpFiberRun (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine runs or resumes a fiber.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    FALSE always to indicate to the VM to switch fibers.

--*/

{

    CkpRunFiber(Vm, CK_AS_FIBER(Arguments[0]), Arguments, TRUE, "run");
    return FALSE;
}

BOOL
CkpFiberCurrent (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the currently running fiber.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_OBJECT_VALUE(Arguments[0], Vm->Fiber);
    return TRUE;
}

BOOL
CkpFiberError (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the given fiber's error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_FIBER Fiber;

    Fiber = CK_AS_FIBER(Arguments[0]);
    Arguments[0] = Fiber->Error;
    return TRUE;
}

BOOL
CkpFiberIsDone (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns a boolean indicating if the given fiber has completed
    execution or not.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_FIBER Fiber;

    Fiber = CK_AS_FIBER(Arguments[0]);
    if ((Fiber->FrameCount == 0) || (!CK_IS_NULL(Fiber->Error))) {
        Arguments[0] = CkOneValue;

    } else {
        Arguments[0] = CkZeroValue;
    }

    return TRUE;
}

BOOL
CkpFiberSuspend (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine suspends the current fiber's execution, exiting the VM.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    FALSE always to indicate to the VM to stop executing.

--*/

{

    Vm->Fiber = NULL;
    return FALSE;
}

BOOL
CkpFiberTransfer (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine transfers to the given fiber without wiring up its calling
    fiber.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    FALSE always to indicate transferring to a new fiber.

--*/

{

    PCK_FIBER Fiber;

    Fiber = CK_AS_FIBER(Arguments[0]);
    CkpRunFiber(Vm, Fiber, Arguments, FALSE, "transfer to");
    return FALSE;
}

BOOL
CkpFiberTransferError (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine transfers to the given fiber without wiring up its calling
    fiber, and immediately sets that fiber's error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    FALSE always to indicate transferring to a new fiber.

--*/

{

    CK_VALUE Error;
    PCK_FIBER Fiber;

    Fiber = CK_AS_FIBER(Arguments[0]);
    Error = Arguments[1];
    if (CK_IS_OBJECT(Error)) {
        CkpPushRoot(Vm, CK_AS_OBJECT(Error));
    }

    Arguments[1] = CkNullValue;
    CkpRunFiber(Vm, Fiber, Arguments, FALSE, "transfer to");
    if (Vm->Fiber == Fiber) {
        Vm->Fiber->Error = Error;
    }

    if (CK_IS_OBJECT(Error)) {
        CkpPopRoot(Vm);
    }

    return FALSE;
}

BOOL
CkpFiberYield (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine gives up control of the current execution fiber to its caller.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    FALSE always to indicate transferring to a new fiber.

--*/

{

    PCK_FIBER CurrentFiber;

    CurrentFiber = Vm->Fiber;
    Vm->Fiber = CurrentFiber->Caller;
    CurrentFiber->Caller = NULL;
    if (Vm->Fiber != NULL) {

        //
        // Yield the result value to the caller in its return slot.
        //

        CK_ASSERT(Vm->Fiber->StackTop > Vm->Fiber->Stack);

        Vm->Fiber->StackTop[-1] = Arguments[1];

        //
        // Yield has two values on the stack (the receiver and the argument).
        // When control is returned to this yielding fiber, it receives the
        // argument just like the statement above. Pop the extra argument off
        // the stack now so the statement above properly stores the returned
        // value in the return slot.
        //

        Vm->Fiber->StackTop -= 1;
    }

    return FALSE;
}

//
// Support functions for the primitives
//

VOID
CkpRunFiber (
    PCK_VM Vm,
    PCK_FIBER Fiber,
    PCK_VALUE Arguments,
    BOOL IsCall,
    PSTR Verb
    )

/*++

Routine Description:

    This routine changes execution to the given fiber. It is designed to be
    called only by the Fiber class primitives.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the fiber to transfer execution to.

    Arguments - Supplies the beginning of the arguments to the primitive
        function that is transferring control.

    IsCall - Supplies a boolean indicating if this routine should return back
        to the currently running fiber when finished.

    Verb - Supplies a pointer to a verb describing the action in case an error
        needs to be reported.

Return Value:

    None.

--*/

{

    if (IsCall != FALSE) {
        if (Fiber->Caller != NULL) {
            CkpRuntimeError(Vm, "Fiber is already running");
            return;
        }

        //
        // Wire up the fiber to return to after this new fiber finishes.
        //

        Fiber->Caller = Vm->Fiber;
    }

    if (Fiber->FrameCount == 0) {
        CkpRuntimeError(Vm, "Cannot %s to a finished fiber", Verb);
        return;
    }

    if (!CK_IS_NULL(Fiber->Error)) {
        CkpRuntimeError(Vm, "Cannot %s to an aborted fiber", Verb);
    }

    //
    // Pop the argument to the primitive call off the stack, so that other
    // fiber code that manipulates the stack doesn't store returned values in
    // the wrong place.
    //

    Vm->Fiber->StackTop -= 1;

    //
    // If the new fiber had yielded or paused, return the result here to it.
    //

    if (Fiber->StackTop > Fiber->Stack) {
        Fiber->StackTop[-1] = Arguments[1];
    }

    Vm->Fiber = Fiber;
    return;
}

