/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    apstart.S

Abstract:

    This module implements trampoline code necessary to bootstrap application
    processors, which start in 16-bit real mode. This code is copied to one of
    the first pages of physical memory, so it must remain position independent.

Author:

    Evan Green 11-Jun-2017

Environment:

    Kernel mode

--*/

##
## ------------------------------------------------------------------ Includes
##

#include <minoca/kernel/x64.inc>

##
## --------------------------------------------------------------- Definitions
##

#define STUB_ADDRESS 0x1000

##
## ---------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## .globl makes the following labels accessible outside this file.
##

.globl HlpTrampolineCode
.globl HlTrampolineCr3
.globl HlKernelStart
.globl HlKernelStartPointer
.globl HlpTrampolineCodeEnd

##
## .text specifies that this code belongs in the executable section.
## .code16 specifies that this is 16-bit real mode code.
##

.text
.code16

HlpTrampolineCode:

##
## Prepare to switch from 16-bit Real Mode to 32-bit Protected Mode.
## Segment registers turn into descriptors (indices into the Global
## Descriptor Table) which must be set up prior to switching into protected
## mode. Interrupts should be disabled for the switch to protected mode to
## avoid any odd BIOS involvement.
##

    cli                                 # Turn off interrupts.
    mov     $(STUB_ADDRESS + 0xFFC), %esp   # Set up a small temporary stack.
    mov     $(TemporaryGdt - HlpTrampolineCode + STUB_ADDRESS), %ebx
    lgdt    (TemporaryGdt - HlpTrampolineCode + STUB_ADDRESS)   # Load the GDT.
    movl    %cr0, %eax                  # Get the CR0 control register.
    orl     $0x1, %eax                  # Set bit 0 to enable Protected Mode.
    movl    %eax, %cr0                  # Set CR0.
    lidt    (TemporaryIdt - HlpTrampolineCode + STUB_ADDRESS)   # Load IDT.

##
## A long jump is required to complete the switch to protected mode. Long jumps
## specify the segment descriptor and an offset from that segment.
##

    ljmp   $KERNEL64_TRANSITION_CS, \
                        $(ProtectedModeCode - HlpTrampolineCode + STUB_ADDRESS)

##
## .code32 specifies that this is 32-bit protected mode code.
##

.code32

ProtectedModeCode:
    movw    $KERNEL_DS, %ax             # Load the kernel's data descriptor.
    movw    %ax, %ds                    # Set DS, ES, and GS.
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss                    # Set the stack segment as well.

    ##
    ## Enable long mode in EFER. Do this before turning paging on so that
    ## 4-level paging is used rather than 32-bit PAE 3-level paging.
    ##

    movl    $X86_MSR_EFER, %ecx     # Get EFER as MSR register.
    rdmsr                           # Read it.
    orl     $(EFER_LONG_MODE_ENABLE | EFER_NO_EXECUTE_ENABLE), %eax
    wrmsr                           # Write it.

    movl    (HlTrampolineCr3 - HlpTrampolineCode + STUB_ADDRESS), %eax
    movl    %eax, %cr3                  # Set up the kernel page directory.
    movl    %cr0, %eax                  # Read CR0.
    orl     $CR0_OR_MASK, %eax          # Set some bits, including paging.
    andl    $CR0_AND_MASK, %eax         # Clear a couple bits too.
    movl    %eax, %cr0                  # Enable paging.

    ##
    ## The AMD64 manual says the next instruction after enabling long mode
    ## paging must be a branch instruction.
    ##

    ljmp    $KERNEL_CS, $(LongModeCode - HlpTrampolineCode + STUB_ADDRESS)

.code64
LongModeCode:

    ##
    ## Get the pointer to the kernel space return address, which should be the
    ## true address of the label below.
    ##

    movq    $(HlKernelStartPointer - HlpTrampolineCode + STUB_ADDRESS), %rdi
    jmp     *(%rdi)

    ##
    ## From here on in, the code is executing back at the original kernel VA,
    ## rather than the identity stub address.
    ##

HlKernelStart:

    ##
    ## Get the processor context structure.
    ##

    movq    HlProcessorStartContext@GOTPCREL(%rip), %rdi   # Get context.
    call    ArRestoreProcessorContext       # Restore away forever.

    ##
    ## The restore context routine does not return, so this will never be
    ## executed.
    ##

HlpTrampolineImpossibleReturn:
    int     $3
    hlt
    jmp     HlpTrampolineImpossibleReturn

##
## This data must be aligned on an 8 byte boundary.
##

.align 8
.code32

##
## GDT Table. The GDT table sets up the segmentation features of the processor
## and privilege levels. In this case set up two descriptors, a code segment
## and a data segment, both of which simply span the entire address space.
## This way the kernel essentially has full control over the whole address
## space. A GDT entry is 8 bytes long, and looks as follows:
##     USHORT limit_low;       // The lower 16 bits of the limit.
##     USHORT base_low;        // The lower 16 bits of the base.
##     UCHAR base_middle;      // The next 8 bits of the base.
##     UCHAR access;           // Access flags, described below.
##     UCHAR granularity;      // Defined below.
##     UCHAR base_high;        // The high 8 bits of the base.
##
## The granularity byte has the following fields:
##     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
##     |     |     |     |     |                       |
##     |  G  |  D  |  L  |  A  | Segment length 19:16  |
##
##     G - Granularity. 0 = 1 byte, 1 = 1 KByte.
##
##     D - Operand Size. 0 = 16 bit, 1 = 32 bit.
##
##     L - Long mode. 0 = 16/32 bit, 1 = 64 bit.
##
##     A - Available for system use (always zero).
##
## The access byte has the following fields:
##     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
##     |     |           |     |                       |
##     |  P  |    DPL    | DT  |         Type          |
##
##     P - Is segment present (1 = Yes)
##
##     DPL - Descriptor privilege level: Ring 0-3. Zero is the highest
##           privilege, 3 is the lowest (least privileged).
##
##     Type - Segment type: code segment / data segment.
##

TemporaryGdt:
    .word   (4 * 8) - 1                 # GDT table limit
    .long   (TemporaryGdtTable - HlpTrampolineCode + STUB_ADDRESS)  # location

TemporaryGdtTable:
    .long   0x0                         # The first GDT entry is called the
    .long   0x0                         # null descriptor, it is essentially
                                        # unused by the processor.

##
## Kernel 64-bit code segment descriptor
##

    .word   0xFFFF                      # Limit 15:0
    .word   0x0                         # Base 15:0
    .byte   0x0                         # Base 23:16
    .byte   0x9A                        # Access: Present, Ring 0, Code Segment
    .byte   0xAF                        # Granularity: 1Kb, 64-bit operands
    .byte   0x00                        # Base 31:24

##
## Kernel data segment descriptor
##

    .word   0xFFFF                      # Limit 15:0
    .word   0x0                         # Base 15:0
    .byte   0x0                         # Base 23:16
    .byte   0x92                        # Access: Present, Ring 0, Data Segment
    .byte   0xCF                        # Granularity: 1Kb, 32-bit operands
    .byte   0x00                        # Base 31:24

##
## Kernel 32-bit code segment descriptor
##

    .word   0xFFFF                      # Limit 15:0
    .word   0x0                         # Base 15:0
    .byte   0x0                         # Base 23:16
    .byte   0x9A                        # Access: Present, Ring 0, Code Segment
    .byte   0xCF                        # Granularity: 1Kb, 32-bit operands
    .byte   0x00                        # Base 31:24

##
## IDT Table. The IDT table sets up information about where to jump for each
## interrupt vector in the system. IDT entries are 8 bytes long, and contain
## the fields listed below. A stub IDT is necessary because
##
## USHORT LowOffset - Stores the low 16 bits of the routine to jump to.
## USHORT SegmentSelector - Stores the CS to use when this interrupt occurs.
## BYTE Reserved - This byte is all 0 for interrupt gates.
## BYTE Access - Stores access information for this gate.
## USHORT HighOffset - Stores the high 16 bits of the routine to jump to.
##
## The access byte has the following bits:
##     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
##     |     |     |     |     |     |     |     |     |
##     |  P  |    DPL    |  L  |  D  |  1  |  1  |  0  |
##
##     D - Size of gate. 0 = 16 bits. 1 = 32 bits.
##
##     DPL - Descriptor Privilege level. 0 for Ring 0 most privileged
##
##     P - Present flag.
##

TemporaryIdt:
    .word   (4 * 8) - 1                 # IDT limit.
    .long   (TemporaryIdtTable - HlpTrampolineCode + STUB_ADDRESS)  # location

TemporaryIdtTable:
    .long   0                           # Exception 0 - Divide by 0.
    .long   0

                                        # Exception 1 - Debug.
    .word   ((DebugStub - HlpTrampolineCode + STUB_ADDRESS) & 0x0000FFFF)
    .word   KERNEL_CS                   # Kernel CS.
    .byte   0                           # Reserved.
    .byte   0x8E                        # Present, ring 0, 32-bits.
    .word   (((DebugStub - HlpTrampolineCode + STUB_ADDRESS) >> 16) & 0xFFFF)

                                        # Exception 2 - NMI.
    .word   ((NmiStub - HlpTrampolineCode + STUB_ADDRESS) & 0x0000FFFF) # Low.
    .word   KERNEL_CS                   # Kernel CS.
    .byte   0                           # Reserved.
    .byte   0x8E                        # Present, ring 0, 32-bits.
    .word   (((NmiStub - HlpTrampolineCode + STUB_ADDRESS) >> 16) & 0x0000FFFF)

    .word   ((DebugStub - HlpTrampolineCode + STUB_ADDRESS) & 0x0000FFFF)
    .word   KERNEL_CS                   # Kernel CS.
    .byte   0                           # Reserved.
    .byte   0x8E                        # Present, ring 0, 32-bits.
    .word   (((DebugStub - HlpTrampolineCode + STUB_ADDRESS) >> 16) & 0xFFFF)

NmiStub:
    iret                                # Just keep going.

DebugStub:
    iret

##
## Define a global containing the address to put into CR3, the kernel's
## current page directory. This is a temporary CR3 used until the restore
## context routine puts the real one in place.
##

.align 8
HlTrampolineCr3:
    .long   0
    .long   0

##
## Define a slot where the kernel puts the final relocated address of
## HlKernelStart. This is needed because everything in x64 is RIP-relative,
## but this code has been artificially moved, and text relocations are
## unacceptable.
##

HlKernelStartPointer:
    .long   0
    .long   0

##
## This label represents the end of the trampoline code.
##

HlpTrampolineCodeEnd:

