/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archprof.c

Abstract:

    This module implements system profiling routines specific to the ARM
    architecture.

Author:

    Chris Stevens 1-Jul-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
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

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
SppArchGetKernelStackData (
    PTRAP_FRAME TrapFrame,
    PVOID *CallStack,
    PULONG CallStackSize
    )

/*++

Routine Description:

    This routine retrieves the kernel stack and its size in the given data
    fields.

Arguments:

    TrapFrame - Supplies a pointer to the trap frame.

    CallStack - Supplies a pointer that receives an array of return addresses
        in the call stack.

    CallStackSize - Supplies a pointer to the size of the given call stack
        array. On return, it contains the size of the produced call stack, in
        bytes.

Return Value:

    Status code.

--*/

{

    PULONG BasePointer;
    ULONG CallStackIndex;
    ULONG CallStackLength;
    PULONG InstructionPointer;
    PVOID ReturnAddress;
    KSTATUS Status;
    PKTHREAD Thread;
    ULONG TopOfStack;

    ASSERT(CallStack != NULL);
    ASSERT((CallStackSize != NULL) && (*CallStackSize != 0));

    //
    // If the current thread information has not been initialized, exit.
    //

    Thread = KeGetCurrentThread();
    if (Thread == NULL) {
        return STATUS_NOT_READY;
    }

    //
    // Put the instruction pointer as the first entry in the call stack unless
    // it is a user mode pointer.
    //

    Status = STATUS_SUCCESS;
    CallStackIndex = 0;
    CallStackLength = *CallStackSize / sizeof(PVOID);
    if (CallStackIndex >= CallStackLength) {
        goto GetKernelStackDataEnd;
    }

    InstructionPointer = (PULONG)TrapFrame->Pc;
    if ((PVOID)InstructionPointer < KERNEL_VA_START) {
        Status = STATUS_OUT_OF_BOUNDS;
        goto GetKernelStackDataEnd;
    }

    CallStack[CallStackIndex] = (PVOID)InstructionPointer;
    CallStackIndex += 1;

    //
    // Trace back through the stack. The two values on the stack before the
    // base pointer are the next base pointer and the return address. Save the
    // return address and carry on up the call stack.
    //

    if ((TrapFrame->Cpsr & PSR_FLAG_THUMB) != 0) {
        BasePointer = (PVOID)TrapFrame->R7;

    } else {
        BasePointer = (PVOID)TrapFrame->R11;
    }

    TopOfStack = (UINTN)Thread->KernelStack + Thread->KernelStackSize;
    while (BasePointer != 0) {

        //
        // If the base pointer is beyond the bounds of the kernel stack, exit.
        //

        if (((ULONG)BasePointer >= TopOfStack) ||
            ((ULONG)BasePointer < (ULONG)Thread->KernelStack)) {

            break;
        }

        //
        // If the return address equals zero, then break out of the loop.
        //

        ReturnAddress = (PVOID)*BasePointer;
        if (ReturnAddress == 0) {
            break;
        }

        //
        // The return address on the frame that has a zero base pointer is
        // invalid. Skip it.
        //

        BasePointer = (PULONG)*(BasePointer - 1);
        if (BasePointer == 0) {
            break;
        }

        if (CallStackIndex >= CallStackLength) {
            break;
        }

        CallStack[CallStackIndex] = ReturnAddress;
        CallStackIndex += 1;
    }

GetKernelStackDataEnd:
    *CallStackSize = CallStackIndex * sizeof(ULONG);
    return Status;
}

