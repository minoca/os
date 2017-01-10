/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
CkpFiberInit (
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
    {"__init@1", 1, CkpFiberInit},
    {"run@1", 1, CkpFiberRun},
    {"error@0", 0, CkpFiberError},
    {"isDone@0", 0, CkpFiberIsDone},
    {"transfer@1", 1, CkpFiberTransfer},
    {"transferError@1", 1, CkpFiberTransferError},
    {NULL, 0, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkFiberStaticPrimitives[] = {
    {"abort@1", 1, CkpFiberAbort},
    {"current@0", 0, CkpFiberCurrent},
    {"suspend@0", 0, CkpFiberSuspend},
    {"yield@1", 1, CkpFiberYield},
    {NULL, 0, NULL}
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

    StackCapacity = CK_INITIAL_STACK;
    if (Closure != NULL) {
        if (Closure->Type == CkClosureBlock) {
            while (StackCapacity < Closure->U.Block.Function->MaxStack + 1) {
                StackCapacity <<= 1;
            }
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
    Fiber->TryStack = NULL;
    Fiber->TryCapacity = 0;
    Fiber->ForeignCalls = 0;
    CkpPushRoot(Vm, &(Fiber->Header));
    CkpFiberReset(Vm, Fiber, Closure);
    CkpPopRoot(Vm);
    return Fiber;
}

VOID
CkpFiberDestroy (
    PCK_VM Vm,
    PCK_FIBER Fiber
    )

/*++

Routine Description:

    This routine destroys a fiber object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the fiber to destroy.

Return Value:

    None.

--*/

{

    if (Fiber->Stack != NULL) {
        CkFree(Vm, Fiber->Stack);
        Fiber->Stack = NULL;
    }

    if (Fiber->Frames != NULL) {
        CkFree(Vm, Fiber->Frames);
        Fiber->Frames = NULL;
    }

    if (Fiber->TryStack != NULL) {
        CkFree(Vm, Fiber->TryStack);
        Fiber->TryStack = NULL;
    }

    return;
}

VOID
CkpAppendCallFrame (
    PCK_VM Vm,
    PCK_FIBER Fiber,
    PCK_CLOSURE Closure,
    PCK_VALUE Stack
    )

/*++

Routine Description:

    This routine adds a new call frame onto the given fiber.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the fiber to run on.

    Closure - Supplies a pointer to the closure to execute.

    Stack - Supplies a pointer to the base of the stack. The receiver and
        arguments should already be set up after this pointer.

Return Value:

    None. On allocation failure, the runtime error will be set.

--*/

{

    PCK_CALL_FRAME Frame;
    PVOID NewBuffer;
    UINTN NewCapacity;

    //
    // Reallocate the frame stack if needed.
    //

    if (Fiber->FrameCount >= Fiber->FrameCapacity) {
        CkpPushRoot(Vm, &(Closure->Header));
        NewCapacity = Fiber->FrameCapacity * 2;
        NewBuffer = CkpReallocate(Vm,
                                  Fiber->Frames,
                                  Fiber->FrameCapacity * sizeof(CK_CALL_FRAME),
                                  NewCapacity * sizeof(CK_CALL_FRAME));

        CkpPopRoot(Vm);
        if (NewBuffer == NULL) {
            return;
        }

        Fiber->Frames = NewBuffer;
        Fiber->FrameCapacity = NewCapacity;
    }

    Frame = &(Fiber->Frames[Fiber->FrameCount]);
    Fiber->FrameCount += 1;
    if (Closure->Type == CkClosureBlock) {
        Frame->Ip = Closure->U.Block.Function->Code.Data;

    } else {
        Frame->Ip = NULL;
    }

    Frame->Closure = Closure;
    Frame->StackStart = Stack;
    Frame->TryCount = Fiber->TryCount;
    return;
}

VOID
CkpPushTryBlock (
    PCK_VM Vm,
    PCK_IP ExceptionHandler
    )

/*++

Routine Description:

    This routine pushes a try block onto the current fiber's try stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ExceptionHandler - Supplies a pointer to the exception handler code.

Return Value:

    None. On allocation failure, the runtime error will be set.

--*/

{

    PCK_FIBER Fiber;
    PVOID NewBuffer;
    UINTN NewCapacity;
    PCK_TRY_BLOCK TryBlock;

    Fiber = Vm->Fiber;
    if (Fiber->TryCount >= Fiber->TryCapacity) {
        NewCapacity = Fiber->TryCapacity * 2;
        if (NewCapacity == 0) {
            NewCapacity = CK_MIN_TRY_STACK;
        }

        CK_ASSERT(NewCapacity >= Fiber->TryCapacity);

        NewBuffer = CkpReallocate(Vm,
                                  Fiber->TryStack,
                                  Fiber->TryCapacity * sizeof(CK_TRY_BLOCK),
                                  NewCapacity * sizeof(CK_TRY_BLOCK));

        if (NewBuffer == NULL) {
            return;
        }

        Fiber->TryStack = NewBuffer;
        Fiber->TryCapacity = NewCapacity;
    }

    TryBlock = &(Fiber->TryStack[Fiber->TryCount]);
    Fiber->TryCount += 1;
    TryBlock->Ip = ExceptionHandler;
    TryBlock->Stack = Fiber->StackTop;
    TryBlock->FrameCount = Fiber->FrameCount;
    return;
}

VOID
CkpEnsureStack (
    PCK_VM Vm,
    PCK_FIBER Fiber,
    UINTN Size
    )

/*++

Routine Description:

    This routine ensures that the stack is at least the given size.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the currently executing fiber.

    Size - Supplies the required size of the stack.

Return Value:

    None. The fiber error will be set on failure.

--*/

{

    UINTN Index;
    UINTN NewCapacity;
    PCK_VALUE NewStack;
    UINTN Offset;
    PCK_UPVALUE Upvalue;

    if (Fiber->StackCapacity >= Size) {
        return;
    }

    if (Size >= CK_MAX_STACK) {
        CkpRuntimeError(Vm, "RuntimeError", "Stack overflow");
        return;
    }

    NewCapacity = Fiber->StackCapacity * 2;
    while (NewCapacity < Size) {
        NewCapacity *= 2;
    }

    NewStack = CkpReallocate(Vm,
                             Fiber->Stack,
                             Fiber->StackCapacity * sizeof(CK_VALUE),
                             NewCapacity * sizeof(CK_VALUE));

    if (NewStack == NULL) {
        return;
    }

    Fiber->StackCapacity = NewCapacity;

    //
    // If the stack buffer changed, adjust all the pointers into the stack.
    //

    if (Fiber->Stack != NewStack) {
        Offset = CK_POINTER_DIFFERENCE(NewStack, Fiber->Stack);
        Fiber->Stack = NewStack;
        Fiber->StackTop = CK_POINTER_ADD(Fiber->StackTop, Offset);

        //
        // Adjust each call frame, which points into the stack.
        //

        for (Index = 0; Index < Fiber->FrameCount; Index += 1) {
            Fiber->Frames[Index].StackStart =
                       CK_POINTER_ADD(Fiber->Frames[Index].StackStart, Offset);
        }

        //
        // Adjust the try blocks.
        //

        for (Index = 0; Index < Fiber->TryCount; Index += 1) {
            Fiber->TryStack[Index].Stack =
                          CK_POINTER_ADD(Fiber->TryStack[Index].Stack, Offset);
        }

        //
        // Adjust the open upvalues.
        //

        Upvalue = Fiber->OpenUpvalues;
        while (Upvalue != NULL) {
            Upvalue->Value = CK_POINTER_ADD(Upvalue->Value, Offset);
            Upvalue = Upvalue->Next;
        }
    }

    return;
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

    CK_ASSERT(Fiber->ForeignCalls == 0);

    Fiber->StackTop = Fiber->Stack;
    Fiber->OpenUpvalues = NULL;
    Fiber->Caller = NULL;
    Fiber->Error = CK_NULL_VALUE;
    Fiber->FrameCount = 0;
    Fiber->TryCount = 0;
    if (Closure != NULL) {

        CK_ASSERT(Closure->Type == CkClosureBlock);

        CkpAppendCallFrame(Vm, Fiber, Closure, Fiber->Stack);
        CkpEnsureStack(Vm, Fiber, Closure->U.Block.Function->MaxStack);
    }

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
CkpFiberInit (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine initializes a new fiber.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PCK_CLOSURE Closure;
    PCK_FIBER Fiber;

    if (!CK_IS_CLOSURE(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a closure");
        return FALSE;
    }

    Fiber = CK_AS_FIBER(Arguments[0]);
    Closure = CK_AS_CLOSURE(Arguments[1]);
    if (Fiber == Vm->Fiber) {
        CkpRuntimeError(Vm, "ValueError", "Cannot initialize running fiber");
        return FALSE;
    }

    if (CkpGetFunctionArity(Closure) != 1) {
        CkpRuntimeError(Vm,
                        "TypeError",
                        "Fiber functions take exactly one argument");

        return FALSE;
    }

    CkpFiberReset(Vm, Fiber, CK_AS_CLOSURE(Arguments[1]));

    //
    // The first two stack slots are null for the receiver ("this"), and null
    // for the argument (which gets filled in later).
    //

    if (Fiber->StackTop == Fiber->Stack) {
        Fiber->Stack[0] = CkNullValue;
        Fiber->Stack[1] = CkNullValue;
        Fiber->StackTop += 2;
    }

    return TRUE;
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

    //
    // If the fiber call stack and the C call stack are linked, then it is
    // not possible to suspend the current fiber (since it would return from
    // the C stack but not the Fiber stack).
    //

    if (Vm->Fiber->ForeignCalls + Vm->ForeignCalls > 0) {
        CkpRuntimeError(Vm,
                        "RuntimeError",
                        "Cannot suspend with foreign calls on stack");

        return FALSE;
    }

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

    //
    // If the fiber call stack and the C call stack are linked, then it is
    // not possible to transfer to a new fiber (since the C stack cannot also
    // simply be put on ice and restored later).
    //

    if (Vm->Fiber->ForeignCalls + Vm->ForeignCalls > 0) {
        CkpRuntimeError(Vm,
                        "RuntimeError",
                        "Cannot transfer with foreign calls on stack");

        return FALSE;
    }

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

    //
    // For the same reason as the main transfer function, fiber transfers
    // cannot happen if a foreign function is on the call stack.
    //

    if (Vm->Fiber->ForeignCalls + Vm->ForeignCalls > 0) {
        CkpRuntimeError(Vm,
                        "RuntimeError",
                        "Cannot transfer with foreign calls on stack");

        return FALSE;
    }

    Fiber = CK_AS_FIBER(Arguments[0]);
    Error = Arguments[1];
    if (CK_IS_OBJECT(Error)) {
        CkpPushRoot(Vm, CK_AS_OBJECT(Error));
    }

    Arguments[1] = CkNullValue;
    CkpRunFiber(Vm, Fiber, Arguments, FALSE, "transfer to");
    if (Vm->Fiber == Fiber) {
        CkpRaiseException(Vm, Error, 0);
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

    //
    // The current fiber cannot yield if there is a foreign function on the
    // stack, since it would get the C stack and the fiber stack out of sync.
    //

    if (CurrentFiber->ForeignCalls != 0) {
        CkpRuntimeError(Vm,
                        "RuntimeError",
                        "Cannot yield with a foreign function on the stack");

        return FALSE;
    }

    Vm->Fiber = CurrentFiber->Caller;
    CurrentFiber->Caller = NULL;
    if (Vm->Fiber != NULL) {

        //
        // If the caller had foreign functions in progress that were added to
        // the VM total when it was switched, subtract those out now.
        //

        Vm->ForeignCalls -= Vm->Fiber->ForeignCalls;

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

        CurrentFiber->StackTop -= 1;
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

    //
    // It should not be possible to run a fiber that already has foreign calls
    // in progress.
    //

    CK_ASSERT(Fiber->ForeignCalls == 0);

    if (IsCall != FALSE) {
        if (Fiber->Caller != NULL) {
            CkpRuntimeError(Vm, "RuntimeError", "Fiber is already running");
            return;
        }

        //
        // Wire up the fiber to return to after this new fiber finishes.
        //

        Fiber->Caller = Vm->Fiber;

        //
        // Remember if this fiber, which is now no longer the running fiber,
        // has foreign functions on the call stack.
        //

        Vm->ForeignCalls += Vm->Fiber->ForeignCalls;
    }

    if (Fiber->FrameCount == 0) {
        CkpRuntimeError(Vm,
                        "RuntimeError",
                        "Cannot %s to a finished fiber",
                        Verb);

        return;
    }

    if (!CK_IS_NULL(Fiber->Error)) {
        CkpRuntimeError(Vm,
                        "RuntimeError",
                        "Cannot %s to an aborted fiber",
                        Verb);
    }

    //
    // Pop the argument to the primitive call off the stack, so that other
    // fiber code that manipulates the stack doesn't store returned values in
    // the wrong place.
    //

    Vm->Fiber->StackTop -= 1;

    //
    // Save the argument into either the primary argument, or the return value
    // of yield or pause. The only time there is not a slot waiting for the
    // argument is the first run of a module fiber.
    //

    if (Fiber->StackTop > Fiber->Stack) {
        Fiber->StackTop[-1] = Arguments[1];
    }

    Vm->Fiber = Fiber;
    return;
}

