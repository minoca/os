/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    teatdata.S

Abstract:

    This module contains instructions designed to exercise the disassembler.
    Do not try to execute any code in this module.

Author:

    Evan Green 21-Jun-2012

Environment:

    Test

--*/

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------------------------------- Code
//

//
// .text specifies that this code belongs in the executable section.
//
// .code32 specifies that this is 16-bit real mode code.
//

.text
.code32

//
// .globl allows this label to be visible to the linker. _Start is the entry
// point to the code, so it needs to be globally visible.
//

.globl _Start
_Start:

    mov     %eax, %cr2
    mov     %cr2, %eax
    lidt    -0x18(%ebp)
    addl    $0x15, %eax
    addb    $0x51, %al
    addb   $0x61, %ah
    addw   $0x71, %bx
    addl    %ebp, %edx
    addl    0x32(%esp), %ecx
    addl    %esp, 0x40(%ecx)
    pushw   %es
    popw    %es
    addw    %cx, %bp
    addw    %bx, %bx
    addl    %eax, %edx
    addb    %dh, 0x30
    addl    %edi, 0x1234
    addl    0x12345678, %esi
    addl    %ecx, 16(%eax, %edx, 4)   # EAX + EDX*4 + 16
    addl    20(%ebx, %ebp, 1), %esi

.long 0
.long 0

//
// --------------------------------------------------------- Internal Functions
//


//
// ---------------------------------------------------------------- Definitions
//

