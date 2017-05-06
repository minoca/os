/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    arm.inc

Abstract:

    This module contains common definitions for the ARM architecture.

Author:

    Evan Green 11-Aug-2012

Environment:

    Kernel mode

--*/

##
## --------------------------------------------------------------- Definitions
##

##
## Set this to 1 to enable Thumb-2 instructions.
##

#ifdef __thumb__
#define THUMB 1
#else
#define THUMB 0
#endif

#define NULL 0x0

##
## Processor flags
##

##
## Program Status Register flags.
##

#define PSR_FLAG_NEGATIVE   0x80000000
#define PSR_FLAG_ZERO       0x40000000
#define PSR_FLAG_CARRY      0x20000000
#define PSR_FLAG_OVERFLOW   0x10000000
#define PSR_FLAG_SATURATION 0x08000000
#define PSR_FLAG_JAZELLE    0x01000000
#define PSR_FLAG_THUMB      0x00000020
#define PSR_FLAG_IRQ        0x00000080
#define PSR_FLAG_FIQ        0x00000040

##
## Processor modes
##

#define ARM_MODE_USER   0x00000010
#define ARM_MODE_FIQ    0x00000011
#define ARM_MODE_IRQ    0x00000012
#define ARM_MODE_SVC    0x00000013
#define ARM_MODE_MON    0x00000016
#define ARM_MODE_ABORT  0x00000017
#define ARM_MODE_HYP    0x0000001A
#define ARM_MODE_UNDEF  0x0000001B
#define ARM_MODE_SYSTEM 0x0000001F
#define ARM_MODE_MASK   0x0000001F

##
## Basic constants.
##

#define FALSE 0
#define TRUE 1

##
## Kernel constants.
##

#define EXCEPTION_BREAK             0x03
#define EXCEPTION_SINGLE_STEP       0x04
#define EXCEPTION_ACCESS_VIOLATION  0x05
#define EXCEPTION_ASSERTION_FAILURE 0x07

##
## Translation table base register address mask.
##
## Bit definitions are tricky for this register because they change based on
## whether or not the Multiprocessing Extensions are supported on the CPU.
##

#define TTBR_ADDRESS_MASK                0x00003FFF

##
## MMU Control bits (SCTLR, CP15, register 1).
##

#define MMU_ENABLED                     0x00000001
#define MMU_ALIGNMENT_FAULT_ENABLED     0x00000002
#define MMU_DCACHE_ENABLED              0x00000004
#define MMU_WRITE_BUFFER_ENABLED        0x00000008
#define MMU_ENDIANNESS                  0x00000080
#define MMU_SYSTEM_PROTECTION           0x00000100
#define MMU_ROM_PROTECTION              0x00000200
#define MMU_BRANCH_PREDICTION_ENABLED   0x00000800
#define MMU_ICACHE_ENABLED              0x00001000
#define MMU_HIGH_EXCEPTION_VECTORS      0x00002000
#define MMU_PREDICTABLE_REPLACEMENT     0x00004000
#define MMU_DISABLE_THUMB_DEPRECATED    0x00008000
#define MMU_FAST_INTERRUPTS             0x00200000
#define MMU_UNALIGNED_ACCESS_ENABLED    0x00400000
#define MMU_VMSA6_ENABLED               0x00800000
#define MMU_VECTORED_INTERRUPTS_ENABLED 0x01000000
#define MMU_EXCEPTION_ENDIAN            0x02000000
#define MMU_THUMB_EXCEPTIONS            0x40000000

#define MMU_CONTROL_DEFAULT_VALUE       0x00C0180D

##
## ARMv6 auxiliary control register bits (ACTLR).
##

#define ARMV6_AUX_16K_CACHE_SIZE 0x00000040

##
## Cortex A17 auxiliary control register bits (ACTLR).
##

#define CORTEX_A17_AUX_SMP_ENABLE 0x00000040

##
## Nonsecure Access Control Register bits (CP15, register 1, opcode2 2).
##

#define MMU_TLB_LOCKING_ENABLE          0x00020000

##
## Secure Configuration Register bits (SCR).
##

#define SCR_NON_SECURE                            0x00000001
#define SCR_MONITOR_MODE_IRQ                      0x00000002
#define SCR_MONITOR_MODE_FIQ                      0x00000004
#define SCR_MONITOR_MODE_EXTERNAL_ABORT           0x00000008
#define SCR_CPSR_FIQ_WRITABLE                     0x00000010
#define SCR_CPSR_ASYNC_ABORT_WRITABLE             0x00000020
#define SCR_EARLY_TERMINATION_DISABLED            0x00000040
#define SCR_NON_SECURE_SMC_DISABLED               0x00000080
#define SCR_NON_SECURE_HVC_ENABLED                0x00000100
#define SCR_NON_SECURE_INSTRUCTION_FETCH_DISABLED 0x00000200

##
## Definition for the structure on the exception stacks.
##

#define EXCEPTION_STACK_R0   0
#define EXCEPTION_STACK_CPSR 4

##
## Processor block offsets.
##

#define PROCESSOR_BLOCK_RUNNING_THREAD 0x14

##
## Define the offsets for members of the TRAP_FRAME structure.
##

#define TRAP_SVCSP 0
#define TRAP_USERSP 4
#define TRAP_USERLR 8
#define TRAP_R0 12
#define TRAP_EXCEPTION_CPSR 16
#define TRAP_R1 20
#define TRAP_R2 24
#define TRAP_R3 28
#define TRAP_R4 32
#define TRAP_R5 36
#define TRAP_R6 40
#define TRAP_R7 44
#define TRAP_R8 48
#define TRAP_R9 52
#define TRAP_R10 56
#define TRAP_R11 60
#define TRAP_R12 64
#define TRAP_SVCLR 68
#define TRAP_PC 72
#define TRAP_CPSR 76
#define TRAP_FRAME_SIZE 80

##
## Define the size of the common SIGNAL_CONTEXT structure.
##

#define SIGNAL_CONTEXT_SIZE 32

##
## Define the size of the PROCESSOR_CONTEXT structure.
##

#define PROCESSOR_CONTEXT_SIZE 192

##
## Thread structure offsets.
##

#define THREAD_SIGNAL_PENDING 84

##
## Define the thread signal pending state options.
##

#define ThreadSignalPending 2

##
## Define the instructions for DSB/ISB, which are different in ARMv6 vs ARMv7.
##

#if __ARM_ARCH == 6

#define DMB mcr     p15, 0, %r0, %cr7, %cr10, 5
#define DSB mcr     p15, 0, %r0, %cr7, %cr10, 4
#define ISB mcr     p15, 0, %r0, %cr7, %cr5, 4
#define BPIALL

#elif __ARM_ARCH == 7

#define DMB dmb
#define DSB dsb
#define ISB isb
#define BPIALL mcr     p15, 0, %r0, %cr7, %cr5, 6

#else

#error Unsupported ARM architecture version

#endif

##
## Define the system call number for resuming after a signal.
##

#define SystemCallRestoreContext 1
#define SIGNAL_PARAMETERS_SIZE 24
#define SIGNAL_CONTEXT_SIZE 32

##
## -------------------------------------------------------------------- Macros
##

##
## Define .cfi directives, macroed so they can be excised if unneeded.
##

#define CFI_DEF_CFA(_Register, _Offset) .cfi_def_cfa _Register, _Offset
#define CFI_DEF_CFA_OFFSET(_Offset) .cfi_def_cfa_offset _Offset
#define CFI_ADJUST_CFA_OFFSET(_Amount) .cfi_adjust_cfa_offset _Amount
#define CFI_OFFSET(_Register, _Offset) .cfi_offset _Register, _Offset
#define CFI_UNDEFINED(_Register) .cfi_undefined _Register
#define CFI_SAME_VALUE(_Register) .cfi_same_value _Register

##
## This macro emits a thumb if-then instruction, if compiling in Thumb mode.
##

#define IT(_Condition) \
    .if THUMB ; \
        it     _Condition ; \
    .endif

#define ITE(_Condition) \
    .if THUMB ; \
        ite    _Condition ; \
    .endif

#define ITTE(_Condition) \
    .if THUMB ; \
        itte   _Condition ; \
    .endif

#define ITTTE(_Condition) \
    .if THUMB ; \
        ittte  _Condition ; \
    .endif

##
## This macro goes at the top of an assembly file.
## .text specifies that this code belongs in the executable section.
## .thumb specifies this is Thumb code (not ARM code).
## .syntax unified turns on Unified Assembly Language which is required to
## enable Thumb-2 features.
## .align 4 ensures functions are emitted on proper boundaries.
##

.macro ASSEMBLY_FILE_HEADER
    .text
    .syntax unified
    .cfi_sections .debug_frame
    .if THUMB
        .thumb
    .else
        .arm
    .endif
#if __ARM_ARCH == 6
    .cpu arm1176jz-s
#elif __ARM_ARCH == 7
    .arch armv7-a
    .arch_extension sec
#else
#error Unsupported ARM architecture version
#endif
.endm

##
## This macro defines a function, callable from C code within the current
## module.
##

.macro FUNCTION FunctionName
    .hidden \FunctionName
    EXPORTED_FUNCTION \FunctionName
.endm

##
## This macro defines a function, callable from C code in any module, but
## always called locally in the current module.
##

.macro PROTECTED_FUNCTION FunctionName
    .protected \FunctionName
    EXPORTED_FUNCTION \FunctionName
.endm

##
## This macro defines a function, callable from C code in any module, and
## capable of being overridden by other modules.
##

.macro EXPORTED_FUNCTION FunctionName
    .func \FunctionName
    .type \FunctionName, %function
    .if THUMB
        .thumb_func
    .endif
    .globl \FunctionName
    .cfi_startproc
    .cfi_def_cfa r13, 0
    \FunctionName:
.endm

##
## This macro defines the end of a function.
##

.macro END_FUNCTION FunctionName
    .ltorg
    .size \FunctionName, .-\FunctionName
    .cfi_endproc
    .endfunc
.endm

##
## Define a macro used to generate the debugger break instruction.
##

.macro DEBUGGER_BREAK
    .if THUMB
        .hword 0xDE20
    .else
        .word 0xE7F000F3
    .endif
.endm

##
## Define a macro used to generate the debug service instruction.
##

.macro DEBUG_SERVICE
    .if THUMB
        .hword 0xDE24
    .else
        .word 0xE7F000F4
    .endif
.endm

##
## This macro can be run immediately after an exception or interrupt. It
## switches back to SVC mode and creates a trap frame. This uses several
## instructions from ARMv6T2, including srs, cpsid #mode, and clrex.
##

.macro ARM_ENTER_INTERRUPT
    srsdb   %sp!, #ARM_MODE_SVC                 @ Push lr and spsr to SVC.
    mrs     %lr, cpsr                           @ Get exception mode CPSR.
    stmdb   %sp, {%r0, %lr}                     @ Push R0 and exception CPSR.
    sub     %r0, %sp, #8                        @ Remember exception stack.
    cpsid   i, #ARM_MODE_SVC                    @ Switch to SVC mode.
    stmdb   %sp!, {%r1-%r12, %lr}               @ Push general registers.
    ldmia   %r0, {%r1, %r2}                     @ Get R0 and exception CPSR.
    stmdb   %sp!, {%r1, %r2}                    @ Save onto trap frame.
    mov     %r0, %sp                            @ Get SVC stack.
    sub     %sp, %sp, #12                       @ Allocate space.
    cpsid   i, #ARM_MODE_SYSTEM                 @ Move to system mode.
    str     %lr, [%r0, #-4]                     @ Save usermode SP.
    str     %sp, [%r0, #-8]                     @ Save usermode LR.
    cpsid   i, #ARM_MODE_SVC                    @ Switch back to SVC mode.
    str     %sp, [%sp]                          @ Save SVC stack pointer.
    clrex                                       @ Clear exclusive monitors.

    CFI_OFFSET(r0, TRAP_R0)
    CFI_OFFSET(r1, TRAP_R1)
    CFI_OFFSET(r2, TRAP_R2)
    CFI_OFFSET(r3, TRAP_R3)
    CFI_OFFSET(r4, TRAP_R4)
    CFI_OFFSET(r5, TRAP_R5)
    CFI_OFFSET(r6, TRAP_R6)
    CFI_OFFSET(r7, TRAP_R7)
    CFI_OFFSET(r8, TRAP_R8)
    CFI_OFFSET(r9, TRAP_R9)
    CFI_OFFSET(r10, TRAP_R10)
    CFI_OFFSET(r11, TRAP_R11)
    CFI_OFFSET(r12, TRAP_R12)
    CFI_OFFSET(sp, TRAP_SVCSP)                 @ Assume it came from SVC mode.
    CFI_OFFSET(lr, TRAP_SVCLR)
    CFI_OFFSET(pc, TRAP_PC)

.endm

##
## This macro can be used for an interrupt or exception that was entered with
## ARM_INTERRUPT_ENTER. It restores the state in the trap frame and returns
## from the exception.
##

.macro ARM_EXIT_INTERRUPT

    ##
    ## Restore the stack pointer. The rest of the context better be at the new
    ## stack location. Then restore the user mode sp and lr.
    ##

    ldr     %sp, [%sp]                          @ Restore stack pointer.
    add     %sp, %sp, #4                        @ Pop stack pointer.

    ##
    ## Restore the user mode stack and link registers.
    ##

    mov     %r0, %sp                            @ Get SVC stack pointer.
    cpsid   i, #ARM_MODE_SYSTEM                 @ Switch to system mode.
    ldr     %sp, [%r0]                          @ Restore usermode SP.
    ldr     %lr, [%r0, #4]                      @ Resotre usermode LR.
    cpsid   i, #ARM_MODE_SVC                    @ Switch back to svc mode.
    ldr     %r0, [%sp, #8]                      @ Restore R0.
    add     %sp, %sp, #16                       @ Pop up to R1.
    ldmia   %sp!, {%r1-%r12, %lr}               @ Restore general registers.

    CFI_SAME_VALUE(r0)
    CFI_SAME_VALUE(r1)
    CFI_SAME_VALUE(r2)
    CFI_SAME_VALUE(r3)
    CFI_SAME_VALUE(r4)
    CFI_SAME_VALUE(r5)
    CFI_SAME_VALUE(r6)
    CFI_SAME_VALUE(r7)
    CFI_SAME_VALUE(r8)
    CFI_SAME_VALUE(r9)
    CFI_SAME_VALUE(r10)
    CFI_SAME_VALUE(r11)
    CFI_SAME_VALUE(r12)
    CFI_OFFSET(sp, 8)
    CFI_OFFSET(lr, 0)

    rfeia   %sp!                                @ Restore PC and CPSR.

.endm

##
## This macro can be used to get the current thread out of the processor block.
## It is used by the system call assembly routines and purposely avoids R0.
##

.macro ARM_GET_CURRENT_THREAD
    mrs     %r2, CPSR               @ Get the original status register.
    cpsid   i                       @ Disable interrupts.
    mrc     p15, 0, %r1, c13, c0, 4 @ Get TPIDRPRW.
    ldr     %r1, [%r1, #PROCESSOR_BLOCK_RUNNING_THREAD] @ Read at offset.
    msr     CPSR, %r2               @ Restore interrupts.

.endm

