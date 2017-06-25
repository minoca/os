/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archprof.c

Abstract:

    This module implements system profiling routines specific to the AMD64
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
#include <minoca/kernel/x64.h>

//
// ---------------------------------------------------------------- Definitions
//

#define X86_FUNCTION_PROLOGUE_MASK 0x00FFFFFF
#define X86_FUNCTION_PROLOGUE 0x00E58955

#define IS_FUNCTION_PROLOGUE(_Instruction) \
    ((_Instruction & X86_FUNCTION_PROLOGUE_MASK) == X86_FUNCTION_PROLOGUE)

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

    PUINTN BasePointer;
    ULONG CallStackIndex;
    ULONG CallStackLength;
    PUINTN InstructionPointer;
    PUINTN ReturnAddress;
    KSTATUS Status;
    PKTHREAD Thread;
    UINTN TopOfStack;

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

    InstructionPointer = (PUINTN)TrapFrame->Rip;
    if ((PVOID)InstructionPointer < KERNEL_VA_START) {
        Status = STATUS_OUT_OF_BOUNDS;
        goto GetKernelStackDataEnd;
    }

    CallStack[CallStackIndex] = (PVOID)InstructionPointer;
    CallStackIndex += 1;

    //
    // Determine if the current instruction is that of the function prologue.
    // If yes, then the return address is on the bottom of the stack.
    //

    if (IS_FUNCTION_PROLOGUE(*InstructionPointer) != FALSE) {
        if (CallStackIndex >= CallStackLength) {
            goto GetKernelStackDataEnd;
        }

        //
        // If the stack is a user mode pointer, do not bother to read it.
        // Return the call stack with just the instruction pointer.
        //

        if (TrapFrame->Rsp < (UINTN)KERNEL_VA_START) {
            goto GetKernelStackDataEnd;
        }

        ReturnAddress = (PVOID)*((PUINTN)TrapFrame->Rsp);
        if (ReturnAddress == 0) {
            goto GetKernelStackDataEnd;
        }

        CallStack[CallStackIndex] = ReturnAddress;
        CallStackIndex += 1;
    }

    //
    // Trace back through the stack. The two values on the stack before the
    // base pointer are the next base pointer and the return address. Save the
    // return address and carry on up the call stack. This loop makes sure that
    // user mode addresses will not be dereferenced as it quites once the base
    // pointer goes beyond the bounds of the kernel stack.
    //

    BasePointer = (PUINTN)TrapFrame->Rbp;
    TopOfStack = (UINTN)Thread->KernelStack + Thread->KernelStackSize;
    while (BasePointer != 0) {

        //
        // If the base pointer is beyond the bounds of the kernel stack, exit.
        //

        if (((UINTN)BasePointer >= TopOfStack) ||
            ((UINTN)BasePointer < (UINTN)Thread->KernelStack)) {

            break;
        }

        //
        // If the return address is zero, break out of the loop.
        //

        ReturnAddress = (PVOID)*(BasePointer + 1);
        if (ReturnAddress == 0) {
            break;
        }

        //
        // The return address on the frame that has a zero base pointer is
        // invalid. Skip it.
        //

        BasePointer = (PUINTN)*BasePointer;
        if (BasePointer == 0) {
            break;
        }

        //
        // Don't go beyond the bounds of the data array.
        //

        if (CallStackIndex >= CallStackLength) {
            break;
        }

        CallStack[CallStackIndex] = ReturnAddress;
        CallStackIndex += 1;
    }

GetKernelStackDataEnd:
    *CallStackSize = CallStackIndex * sizeof(UINTN);
    return Status;
}

