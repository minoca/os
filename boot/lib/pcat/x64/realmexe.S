/*++

Copyright (c) 2017 Minoca Corp.

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

    Evan Green 6-Jun-2017

Environment:

    Boot

--*/

##
## ------------------------------------------------------------------ Includes
##

#include <minoca/kernel/x64.inc>

##
## --------------------------------------------------------------- Definitions
##

##
## REAL_MODE_CONTEXT structure definition.
##

.equ CODEPAGE,  0x0
.equ DATAPAGE,  0x10
.equ STACKPAGE, 0x20
.equ EAX,       0x30
.equ EBX,       0x34
.equ ECX,       0x38
.equ EDX,       0x3C
.equ ESI,       0x40
.equ EDI,       0x44
.equ ESP,       0x48
.equ EBP,       0x4C
.equ EIP,       0x50
.equ EFLAGS,    0x54
.equ CS,        0x58
.equ DS,        0x5C
.equ ES,        0x60
.equ FS,        0x64
.equ GS,        0x68
.equ SS,        0x6C

##
## ----------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

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
## This code represents the trampoline code that gets down to 16-bit real mode
## and then makes an int 0xN BIOS call. It starts as 32-bit protected mode
## code. When this code is actually run it will not be at its original location.
## ESI contains a pointer to the context.
##

.code32
FwpRealModeBiosCallTemplate:

    ##
    ## Save the GDT, IDT, and CR0.
    ##

    subl    $0x8, %esp
    sgdt    (%esp)
    subl    $0x8, %esp
    sidt    (%esp)
    movl    %cr0, %ecx

    ##
    ## Disable paging, and save CR0 with the enable paging flag clear.
    ##

    andl    $~CR0_PAGING_ENABLE, %ecx
    movl    %ecx, %cr0
    pushl   %ecx
    pushl   %esi

    ##
    ## Disable long mode.
    ##

    movl    $X86_MSR_EFER, %ecx     # Get EFER as MSR register.
    rdmsr                           # Read it.
    andl    $~EFER_LONG_MODE_ENABLE, %eax    # Disable long mode.
    wrmsr                           # Write it.

    ##
    ## Set up a real mode cr0, but don't switch to it yet.
    ##

    movl    %cr0, %ecx
    andl    $~CR0_PROTECTED_MODE, %ecx

    ##
    ## Load 16-bit protected mode GDT and IDT registers.
    ##

    lgdt    24(%esp)
    lidt    32(%esp)

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
    lidt    0x34(%esp)
    lgdt    0x3C(%esp)
    movl    0x30(%esp), %eax

    ##
    ## Restore protected mode, and perform a long jump to make it apply.
    ##

    movl    %eax, %cr0

FwpRealModeBiosCallTemplateLongJump3:
    ljmp    $KERNEL64_TRANSITION_CS, $0x3456

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
    addl    $0x28, %esp

    ##
    ## Enable long mode.
    ##

    movl    $X86_MSR_EFER, %ecx     # Get EFER as MSR register.
    rdmsr                           # Read it.
    orl     $EFER_LONG_MODE_ENABLE, %eax    # Enable long mode.
    wrmsr                           # Write it.

    ##
    ## Enable paging.
    ##

    movl    %cr0, %ecx
    orl     $CR0_PAGING_ENABLE, %ecx
    movl    %ecx, %cr0

    ##
    ## Far return back to long mode.
    ##

    retf

##
## That was the end of the code. Now define a 16-bit protected mode GDT.
## The GDT must be aligned to 8 bytes.
##

.align 8

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
## Define the data segment descriptor.
##

    .word   0xFFFF                      # Limit 15:0
    .word   0x0                         # Base 15:0
    .byte   0x0                         # Base 23:16
    .byte   0x92                        # Access: Present, Ring 0, Data Segment
    .byte   0x8F                        # Granularity: 1kB, 16-bit mode
    .byte   0x00                        # Base 31:24

##
## This label marks the end of the template code. It is useful for determining
## the size of the template code.
##

FwpRealModeBiosCallTemplateEnd:
    nop

##
## This function is 64-bit long mode code.
##

.code64

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
    pushq   %rbp
    movq    %rsp, %rbp

    ##
    ## Save the non-volatile registers and flags. Theoretically r8 and higher
    ## shouldn't be touched by 16-bit real mode code, or even 32-bit protected
    ## mode code. Save them on the off chance that the BIOS is flipping into
    ## long mode somewhere behind the scenes.
    ##

    pushq   %rbx
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
    pushq   %rbp
    pushfq
    cli

    ##
    ## Save FSBase and GSbase.
    ##

    movl    $X86_MSR_FSBASE, %ecx
    rdmsr
    pushq   %rdx
    pushq   %rax
    movl    $X86_MSR_GSBASE, %ecx
    rdmsr
    pushq   %rdx
    pushq   %rax

    ##
    ## Push the parameters for a far return back to the long mode restore code.
    ##

    subq    $8, %rsp
    movl    $KERNEL_CS, 4(%rsp)
    movq    FwpRealModeExecuteRestore@GOTPCREL(%rip), %rax
    movl    %eax, (%rsp)

    ##
    ## Get the linear EIP.
    ##

    movq    %rdi, %rsi
    movl    CS(%rsi), %eax
    shll    $4, %eax
    movl    EIP(%rsi), %edx
    add     %edx, %eax

    ##
    ## Push the 16-bit IDT (empty) and GDT addresses.
    ##

    subq    $16, %rsp
    movl    $0x0, 12(%rsp)
    movl    $0x3FF, 8(%rsp)
    movq    $(Fwp16BitGdtTable - FwpRealModeBiosCallTemplate), %rcx
    addq    %rax, %rcx
    movw    $((3 * 8) - 1), (%rsp)
    movl    %ecx, 2(%rsp)

    ##
    ## Push parameters for a far return, and use it to get into 32-bit code.
    ##

    pushq   $KERNEL64_TRANSITION_CS
    pushq   %rax
    retfq

    ##
    ## This code is jumped to by the end of the BIOS call template.
    ##

FwpRealModeExecuteRestore:

    ##
    ## Restore GSBase and FSBase
    ##

    popq    %rax
    popq    %rdx
    movl    $X86_MSR_GSBASE, %ecx
    wrmsr
    popq    %rax
    popq    %rdx
    movl    $X86_MSR_FSBASE, %ecx
    wrmsr

    ##
    ## Restore the non-volatile registers and flags.
    ##

    popfq
    popq    %rbp
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx
    leave
    ret

END_FUNCTION(FwpRealModeExecute)

