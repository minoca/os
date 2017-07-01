/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kernxfr.S

Abstract:

    This module implements the code necessary to transfer execution from the
    loader to the kernel.

Author:

    Evan Green 29-Jul-2012

Environment:

    Boot (32-bit protected mode, paging on).

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/x86.inc>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------------------------------- Code
//

//
// .text specifies that this code belongs in the executable section.
//
// .code32 specifies that this is 32-bit protected mode code.
//

.text
.code32

//
// VOID
// BoEnablePaging (
//     VOID
//     )
//

/*++

Routine Description:

    This routine turns paging on in the processor.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(BoEnablePaging)

    //
    // Set up CR3.
    //

    movl    BoPageDirectory, %eax  # Get the page directory.
    movl    %eax, %cr3              # Put it in CR3.

    //
    // Set up CR0, enabling paging.
    //

    movl    %cr0, %eax              # Read CR0.
    orl     $CR0_OR_MASK, %eax      # OR in the right bits.
    andl    $CR0_AND_MASK, %eax     # AND out the right bits.
    movl    %eax, %cr0              # Boom, do it.
    ret                             # Return happily.

END_FUNCTION(BoEnablePaging)

//
// VOID
// BoTransferToKernelAsm (
//     PVOID Parameters,
//     PVOID EntryPoint,
//     PVOID StackAddress
//     )

/*++

Routine Description:

    This routine transfers control of execution to the kernel.

Arguments:

    Parameters - Supplies the parameter block to pass on to the kernel for its
        initialization.

    EntryPoint - Supplies the entry point of the kernel. The function will end
        with a jump to here.

    StackAddress - Supplies the top of the kernel stack. The stack pointer will
        get set to this value just before the kernel is launched.

Return Value:

    None. This function does not return.

--*/

//
// Parameters
//

.equ Parameters, 8
.equ EntryPoint, 12
.equ StackAddress, 16

FUNCTION(BoTransferToKernelAsm)
    push    %ebp                # Function prologue.
    movl    %esp, %ebp          #
    pushl   %ebx                # Save registers.
    pushl   %ecx                #
    pushl   %edx                #
    movl    StackAddress(%ebp), %ebx            #
    addl    $-4, %ebx           # Initial address is not beyond stack.
    movl    EntryPoint(%ebp), %eax              # Get the entry point.
    movl    Parameters(%ebp), %ecx              # Get the parameter block.
    movl    %esp, %edx          # Save the stack pointer. If the kernel ever
                                # returned, this would need to be restored.
    subl    $8, %ebx            # Make room for new items on the stack.
    movl    %ecx, 4(%ebx)       # Push the parameter block.
    movl    $0, (%ebx)          # Push a dummy return address.
    movl    %ebx, %esp          # Switch to kernel stack.
    xor     %ebp, %ebp          # Zero ebp.
    jmp     *%eax               # Jump into the kernel.

ImpossibleLoaderReturn:
    movl    %edx, %esp          # Restore the loader stack.
    popl    %edx                # Restore registers.
    popl    %ecx                #
    popl    %ebx                #
    leave                       #
    ret                         #

END_FUNCTION(BoTransferToKernelAsm)
