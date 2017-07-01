/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    contexta.S

Abstract:

    This module implements functionality for manipulating ucontext structures.

Author:

    Evan Green 9-Sep-2016

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/arm.inc>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER
.fpu vfpv3

//
// LIBC_API
// int
// getcontext (
//     ucontext_t *Context
//     )
//

/*++

Routine Description:

    This routine saves the current user context into the given structure,
    including the machine registers, signal mask, and execution stack pointer.
    If restored, the returned context will appear to execute at the return from
    this function.

Arguments:

    Context - Supplies a pointer where the current context is saved.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

EXPORTED_FUNCTION getcontext
    str     %r1, [%r0, #(SIGNAL_CONTEXT_SIZE + TRAP_R1)]    @ Save R1.
    str     %r2, [%r0, #(SIGNAL_CONTEXT_SIZE + TRAP_R2)]    @ Save R2.
    str     %r3, [%r0, #(SIGNAL_CONTEXT_SIZE + TRAP_R3)]    @ Save R3.
    mov     %r2, %r0                        @ Copy R0 into R2.
    add     %r0, %r0, #SIGNAL_CONTEXT_SIZE  @ Get to the TRAP_FRAME part.
    eor     %r1, %r1, %r1                   @ Clear R1.
    mov     %r3, %sp                        @ Get SP in a regular register.
    stmia   %r0!, {%r1, %r3, %lr}           @ Save SVC Sp, User SP, and User LR.
    stmia   %r0!, {%r1}                     @ Save a zeroed R0.
    stmia   %r0!, {%r1}                     @ Save a zeroed exception CPSR.
    add     %r0, %r0, #12                   @ Skip R1-R3.
    stmia   %r0!, {%r4-%r12}                @ Save R4-R12.
    stmia   %r0!, {%r1, %lr}                @ Save zeroed SVC link, and PC.
    mrs     %r1, CPSR                       @ Get the flags.
    stmia   %r0!, {%r1}                     @ Save the CPSR.
    mov     %r0, %r2                        @ Get the context back.
    mov     %r1, %sp                        @ Stack pointer as second argument.
    b       ClpGetContext                   @ Tail call to clpgetcontext.

END_FUNCTION getcontext

//
// LIBC_API
// int
// setcontext (
//     const ucontext_t *Context
//     )
//

/*++

Routine Description:

    This routine restores a previous execution context into the current
    processor.

Arguments:

    Context - Supplies a pointer to the previously saved context to restore.

Return Value:

    Does not return on success, as execution continues from the new context.

    -1 on failure, and errno will be set to contain more information.

--*/

EXPORTED_FUNCTION setcontext
    stmdb   %sp!, {%r0}         @ Save R0.
    bl      ClpSetContext       @ Call the C helper.
    ldmia   %sp!, {%r0}         @ Restore R0.
    add     %r2, %r0, #(SIGNAL_CONTEXT_SIZE + TRAP_USERSP) @ Get to user SP.
    ldmia   %r2!, {%r3-%r7}     @ Get User SP, lr, R0, exception CPSR, and R1.
    mov     %sp, %r3            @ Restore SP.
    mov     %lr, %r4            @ Restore LR.
    mov     %r0, %r5            @ Restore R0.
    mov     %r1, %r7            @ Restore R1.
    add     %r2, %r2, #8        @ Skip R2 and R3.
    ldmia   %r2!, {%r4-%r12}    @ Restore R4-R12.
    add     %r2, %r2, #4        @ Skip Svc link.
    ldmia   %r2!, {%r3}         @ Restore PC into R3. Ignore CPSR.
    bx      %r3                 @ Jump to PC.

END_FUNCTION setcontext

//
// __NO_RETURN
// void
// ClpContextStart (
//     void (*StartFunction)(),
//     ...
//     )
//

/*++

Routine Description:

    This routine is a small trampoline that calls the function specified in
    makecontext.

Arguments:

    StartFunction - Supplies a pointer to the function to call.

    ... - Supplies the arguments the start function takes.

Return Value:

    This routine does not return.

--*/

FUNCTION ClpContextStart
    ldmia   %sp!, {%r12}            @ Get the function to call.
    ldmia   %sp!, {%r0-%r3}         @ Pop the register arguments.
    blx     %r12                    @ Jump to the function to call.
    mov     %sp, %r4                @ Pop the function and arguments.
    ldmia   %sp!, {%r0}             @ Pop the context pointer.
    bl      ClpContextEnd           @ Call the C helper to switch contexts.
    DEBUGGER_BREAK                  @ Execution should never reach here.

END_FUNCTION ClpContextStart

//
// VOID
// ClpSaveVfp (
//     PFPU_CONTEXT Context,
//     BOOL SimdSupport
//     )
//

/*++

Routine Description:

    This routine saves the Vector Floating Point unit state.

Arguments:

    Context - Supplies a pointer where the context will be saved.

    SimdSupport - Supplies a boolean indicating whether the VFP unit contains
        32 64-bit registers (TRUE) or 16 64-bit registers (FALSE).

Return Value:

    None.

--*/

FUNCTION ClpSaveVfp
    stc     p11, c0, [%r0], #16*8       @ Save D0-D15 (stmia).
    cmp     %r1, #0                     @ Test for no SIMD support.
    ITE(ne)                             @ If equal then else.
    stclne  p11, c0, [%r0], #16*8       @ Save D16-D31 if SIMD support.
    addeq   %r0, %r0, #16*8             @ Skip those registers if not.
    vmrs    %r2, FPSCR                  @ Get FPSCR.
    str     %r2, [%r0]                  @ Store it.
    bx      %lr                         @ Return.

END_FUNCTION ClpSaveVfp

//
// VOID
// ClpRestoreVfp (
//     PFPU_CONTEXT Context,
//     BOOL SimdSupport
//     )
//

/*++

Routine Description:

    This routine restores the Vector Floating Point unit state into the
    hardware.

Arguments:

    Context - Supplies a pointer to the context to restore.

    SimdSupport - Supplies a boolean indicating whether the VFP unit contains
        32 64-bit registers (TRUE) or 16 64-bit registers (FALSE).

Return Value:

    None.

--*/

FUNCTION ClpRestoreVfp
    ldc     p11, c0, [%r0], #16*8       @ Restore D0-D15 (ldmia).
    cmp     %r1, #0                     @ Test for no SIMD support.
    ITE(ne)                             @ If equal then else.
    ldclne  p11, c0, [%r0], #16*8       @ Restore D16-D31 if SIMD support.
    addeq   %r0, %r0, #16*8             @ Skip those registers if not.
    ldr     %r2, [%r0]                  @ Get FPSCR.
    vmsr    FPSCR, %r2                  @ Restore FPSCR.
    bx      %lr                         @ Return.

END_FUNCTION ClpRestoreVfp

