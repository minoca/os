/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    realmexe.S

Abstract:

    This module implements the real and protected mode code necessary to call
    BIOS services.

Author:

    Evan Green 18-Jul-2012

Environment:

    Boot

--*/

##
## ------------------------------------------------------------------ Includes
##

#include <minoca/kernel/x86.inc>

##
## --------------------------------------------------------------- Definitions
##

##
## REAL_MODE_CONTEXT structure definition.
##

.equ CODEPAGE,  0x0
.equ DATAPAGE,  0xc
.equ STACKPAGE, 0x18
.equ EAX,       0x24
.equ EBX,       0x28
.equ ECX,       0x2C
.equ EDX,       0x30
.equ ESI,       0x34
.equ EDI,       0x38
.equ ESP,       0x3C
.equ EBP,       0x40
.equ EIP,       0x44
.equ EFLAGS,    0x48
.equ CS,        0x4C
.equ DS,        0x50
.equ ES,        0x54
.equ FS,        0x58
.equ GS,        0x5C
.equ SS,        0x60

##
## ----------------------------------------------------------------------- Code
##

##
## .text specifies that this code belongs in the executable section.
##
## .code32 specifies that this is 32-bit protected mode code.
##

.text
.code32

##
## .globl allows this label to be visible to the linker.
##

.globl FwpRealModeBiosCallTemplate
.globl FwpRealModeBiosCallTemplateLongJump
.globl FwpRealModeBiosCallTemplateLongJump2
.globl FwpRealModeBiosCallTemplateLongJump3
.globl FwpRealModeBiosCallTemplateIntInstruction
.globl FwpRealModeBiosCallTemplateEnd

##
## This code represents the real-mode code run to call BIOS code. It is 16-bit
## real-mode code. SI contains a pointer to the context structure.
##

FwpRealModeBiosCallTemplate:

    ##
    ## Save the GDT, IDT, and CR0.
    ##

    subl    $0x8, %esp
    sgdt    (%esp)
    subl    $0x8, %esp
    sidt    (%esp)
    movl    %cr0, %ecx
    push    %ecx
    andl    $0xFFFFFFFE, %ecx
    pushl   %esi

    ##
    ## Push the registers from the context onto the stack.
    ##

    movl    EFLAGS(%esi), %eax
    pushl   %eax
    movl    EAX(%esi), %eax
    pushl   %eax
    movl    ECX(%esi), %eax
    pushl   %eax
    movl    EDX(%esi), %eax
    pushl   %eax
    movl    EBX(%esi), %eax
    pushl   %eax
    movl    EDI(%esi), %eax
    pushl   %eax
    movl    ESI(%esi), %eax
    pushl   %eax
    movl    DS(%esi), %eax
    pushl   %eax
    movl    ES(%esi), %eax
    pushl   %eax
    movl    FS(%esi), %eax
    pushl   %eax
    movl    GS(%esi), %eax
    pushl   %eax

    ##
    ## Load 16-bit protected mode GDT and IDT registers.
    ##

    lgdt    Fwp16BitGdt
    lidt    Fwp16BitIdt

    ##
    ## Jump to consummate the transition to 16-bit protected mode.
    ##

FwpRealModeBiosCallTemplateLongJump:
    ljmp    $KERNEL_CS, $0x3456

    ##
    ## This is now 16-bit protected mode code (a weird combination). Remove the
    ## protected mode bit to get to real mode.
    ##

.code16

    movl    %ecx, %cr0

    ##
    ## Perform a long jump to get back to 16 bit real mode. This assumes that
    ## the code is located below 64k. The actual constants will be patched up,
    ## this constant is a dummy.
    ##

FwpRealModeBiosCallTemplateLongJump2:
    ljmp    $0x12, $0x3456

    ##
    ## Reset the stack segment, and pop the registers into place.
    ##

    xorw    %ax, %ax
    movw    %ax, %ss
    popl    %eax
    movw    %ax, %gs
    popl    %eax
    movw    %ax, %fs
    popl    %eax
    movw    %ax, %es
    popl    %eax
    movw    %ax, %ds
    popl    %esi
    popl    %edi
    popl    %ebx
    popl    %edx
    popl    %ecx
    popl    %eax
    popfl

    ##
    ## Blast off. This 0x1B is a dummy value, the setup code will have
    ## modified that to be the correct vector.
    ##

FwpRealModeBiosCallTemplateIntInstruction:
    int     $0x1B

    ##
    ## Push the registers onto the stack. This is still 16 bit code.
    ##

    pushfl
    cli
    pushl   %esi
    pushl   %edi
    pushl   %ebx
    pushl   %edx
    pushl   %ecx
    pushl   %eax
    xorl    %eax, %eax
    movw    %gs, %ax
    pushl   %eax
    movw    %fs, %ax
    pushl   %eax
    movw    %es, %ax
    pushl   %eax
    movw    %ds, %ax
    pushl   %eax

    ##
    ## Restore back to 32-bit protected mode by loading up the GDT and IDT,
    ## then applying the original CR0.
    ##

    xorw    %ax, %ax
    movw    %ax, %ds
    leal    0x34(%esp), %eax
    lidt    (%eax)
    addl    $0x8, %eax
    lgdt    (%eax)
    movl    0x30(%esp), %eax

    ##
    ## Restore protected mode, and perform a long jump to make it apply.
    ##

    movl    %eax, %cr0

FwpRealModeBiosCallTemplateLongJump3:
    ljmp    $KERNEL_CS, $0x3456

.code32

    ##
    ## Restore the protected mode segment registers.
    ##

    movw    $KERNEL_DS, %dx
    movw    %dx, %ds
    movw    %dx, %es
    movw    %dx, %fs
    movw    %dx, %gs
    movw    %dx, %ss

    ##
    ## Safely back in 32-bit land, get the address of the context structure,
    ## and save the registers saved onto the stack into the context structure.
    ##

    movl    0x2C(%esp), %esi
    popl    %eax
    movl    %eax, DS(%esi)
    popl    %eax
    movl    %eax, ES(%esi)
    popl    %eax
    movl    %eax, FS(%esi)
    popl    %eax
    movl    %eax, GS(%esi)
    popl    %eax
    movl    %eax, EAX(%esi)
    popl    %eax
    movl    %eax, ECX(%esi)
    popl    %eax
    movl    %eax, EDX(%esi)
    popl    %eax
    movl    %eax, EBX(%esi)
    popl    %eax
    movl    %eax, EDI(%esi)
    popl    %eax
    movl    %eax, ESI(%esi)
    popl    %eax
    movl    %eax, EFLAGS(%esi)
    addl    $0x18, %esp
    movl    $FwpRealModeExecuteRestore, %eax
    jmp     *%eax

##
## That was the end of the code. Now define a 16-bit protected mode GDT.
## The GDT must be aligned to 8 bytes.
##

.align 8

Fwp16BitGdt:
    .word   (3 * 8) - 1                 # GDT table limit
    .long   Fwp16BitGdtTable            # GDT table location

Fwp16BitGdtTable:
    .long   0x0                         # The first GDT entry is called the
    .long   0x0                         # null descriptor, it is essentially
                                        # unused by the processor.

##
## Define the code segment descriptor.
##

    .word   0xFFFF                      # Limit 15:0
    .word   0x0                         # Base 15:0
    .byte   0x0                         # Base 23:16
    .byte   0x9A                        # Access: Present, Ring 0, Code Segment
    .byte   0x8F                        # Granularity: 1Kb, 16-bit mode
    .byte   0x00                        # Base 31:24

##
## Define the dat segment descriptor.
##

    .word   0xFFFF                      # Limit 15:0
    .word   0x0                         # Base 15:0
    .byte   0x0                         # Base 23:16
    .byte   0x92                        # Access: Present, Ring 0, Data Segment
    .byte   0x8F                        # Granularity: 1kB, 16-bit mode
    .byte   0x00                        # Base 31:24

##
## Also define a 16-bit protected mode IDT.
##

Fwp16BitIdt:
    .word   0x3FF                       # IDT Table Limit
    .long   0x0                         # IDT Table base

##
## This label marks the end of the template code. It is useful for determining
## the size of the template code.
##

FwpRealModeBiosCallTemplateEnd:
    nop

##
## This function is 32-bit protected mode code.
##

.code32

##
## VOID
## FwpRealModeExecute (
##     PREAL_MODE_CONTEXT Context
##     )
##

/*++

Routine Description:

    This routine executes 16-bit real mode code by switching the processor back
    to real mode.

Arguments:

    Context - Supplies a pointer to the context structure that will be
        executed. On return, this will contain the executed context.

Return Value:

    None.

--*/

FUNCTION(FwpRealModeExecute)
    push    %ebp
    movl    %esp, %ebp

    ##
    ## Save the non-volatile registers and flags.
    ##

    push    %ebx
    push    %esi
    push    %edi
    push    %ebp
    pushfl
    cli

    ##
    ## Load the context parameter into ESI, get EIP, and jump to it.
    ##

    movl    8(%ebp), %esi
    movl    CS(%esi), %eax
    shll    $4, %eax
    movl    EIP(%esi), %edx
    add     %edx, %eax
    jmp     *%eax

    ##
    ## This code is jumped to by the end of the BIOS call template.
    ##

FwpRealModeExecuteRestore:

    ##
    ## Restore the non-volatile registers and flags.
    ##

    popfl
    popl    %ebp
    popl    %edi
    popl    %esi
    popl    %ebx
    leave
    ret

END_FUNCTION(FwpRealModeExecute)

