/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    osbasea.S

Abstract:

    This module implements assembly support for the OS Base library.

Author:

    Evan Green 17-Jan-2015

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/x64.inc>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// OS_API
// INTN
// OsForkProcess (
//     ULONG Flags,
//     PVOID FrameRestoreBase
//     )
//

/*++

Routine Description:

    This routine forks the current process into two separate processes. The
    child process begins executing in the middle of this function.

Arguments:

    Flags - Supplies a bitfield of flags governing the behavior of the newly
        forked process. See FORK_FLAG_* definitions.

    FrameRestoreBase - Supplies an optional pointer to a region of recent
        stack. On vfork operations, the kernel will copy the stack region from
        the supplied pointer up to the current stack pointer into a temporary
        buffer. After the child execs or exits, the kernel will copy that
        region back into the parent process' stack. This is needed so that the
        stack can be used in between the C library and the final system call.

Return Value:

    In the child, returns 0 indicating success.

    In the parent, returns the process ID of the child on success, which is
    always a positive value.

    On failure, returns a KSTATUS code, which is a negative value.

--*/

EXPORTED_FUNCTION(OsForkProcess)

    //
    // Save non-volatiles, since in the child process they're all zeroed out.
    //

    pushq   %rbp                # Preserve non-volatile.
    pushq   %rbx                # Preserve non-volatile.
    pushq   %r12                # Preserve non-volatile.
    pushq   %r13                # Preserve non-volatile.
    pushq   %r14                # Preserve non-volatile.
    pushq   %r15                # Preserve non-volatile.

    //
    // Create the SYSTEM_CALL_FORK structure, and pass a pointer to it.
    //

    pushq   %rsi                # Push frame restore base parameter.
    pushq   %rdi                # Push flags parameter.
    movq    %rsp, %rsi          # Pass pointer to "structure" as parameter 2.
    subq    $8, %rsp            # Align stack.
    CFI_ADJUST_CFA_OFFSET(72)   # Let the debugger know about the stack change.
    movl    $SystemCallForkProcess, %edi    # Pass system call number param 1.
    callq   OsSystemCall        # Perform the system call.
    addq    $24, %rsp           # Pop structure and alignment.
    CFI_ADJUST_CFA_OFFSET(-24)  # Let the debugger know about the stack change.
    popq    %r15                # Restore non-volatile.
    popq    %r14                # Restore non-volatile.
    popq    %r13                # Restore non-volatile.
    popq    %r12                # Restore non-volatile.
    popq    %rbx                # Restore non-volatile.
    popq    %rbp                # Restore non-volatile.
    retq

END_FUNCTION(OsForkProcess)

//
// INTN
// OsSystemCall (
//     ULONG SystemCallNumber,
//     PVOID SystemCallParameter
//     )
//

/*++

Routine Description:

    This routine executes a regular system call.

Arguments:

    SystemCallNumber - Supplies the system call number.

    SystemCallParameter - Supplies the system call parameter.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

FUNCTION(OsSystemCall)
    syscall                     # Just do that system call, params in rdi, rsi.
    ret                         # Return.

END_FUNCTION(OsSystemCall)

//
// VOID
// OspSignalHandler (
//     )
//

/*++

Routine Description:

    This routine is called directly by the kernel when a signal occurs. It
    marshals the parameters and calls the C routine for handling the signal.
    The parameters are stored on the stack with the signal parameters followed
    by the signal context.

Arguments:

    None. The parameters are stored in registers, but do not conform to any C
        calling convention.

Return Value:

    None.

--*/

FUNCTION(OspSignalHandler)
    pushq   %rsi                # Save signal context, align stack.
    CFI_ADJUST_CFA_OFFSET(8)    # Account for the push.
    call    OspProcessSignal    # Call the processing routine.
    movl    $SystemCallRestoreContext, %edi     # Set system call number.
    popq    %rsi                # Pop signal context as parameter two.
    CFI_ADJUST_CFA_OFFSET(-8)   # Account for the popl.
    callq   OsSystemCall        # Restore to before the signal handler.
    int     $3                  # Execution should never get back here.

END_FUNCTION(OspSignalHandler)

//
// PTHREAD_CONTROL_BLOCK
// OspGetThreadControlBlock (
//     VOID
//     )
//

/*++

Routine Description:

    This routine returns a pointer to the thread control block, a structure
    unique to each thread.

Arguments:

    None.

Return Value:

    Returns a pointer to the current thread's control block.

--*/

FUNCTION(OspGetThreadControlBlock)
    movq    %fs:(0), %rax       # Load the Self pointer.
    ret                         # Return.

END_FUNCTION(OspGetThreadControlBlock)

//
// VOID
// OspImArchResolvePltEntry (
//     PLOADED_IMAGE Image,
//     ULONG RelocationOffset
//     )
//

/*++

Routine Description:

    This routine implements the slow path for a Procedure Linkable Table entry
    that has not yet been resolved to its target function address. This routine
    is only called once for each PLT entry, as subsequent calls jump directly
    to the destination function address.

Arguments:

    Image - Supplies a pointer to the loaded image whose PLT needs resolution.
        This is really whatever pointer is in GOT + 4.

    RelocationOffset - Supplies the byte offset from the start of the
        relocation section where the relocation for this PLT entry resides.

Return Value:

    None. Control jumps directly to the destination function, rather than
    returning.

--*/

FUNCTION(OspImArchResolvePltEntry)
    popq    %r10                # Pop GOT[1] (image), pushed last.
    popq    %r11                # Pop relocation offset.
    pushq   %rax                # Save rax in case of ___tls_get_addr.
    pushq   %rcx                # Save rcx argument.
    pushq   %rdx                # Save rdx argument.
    pushq   %rdi                # Save rdi argument.
    pushq   %rsi                # Save rsi argument.
    pushq   %r8                 # Save r8 argument.
    pushq   %r9                 # Save r9 argument.
    movq    %r10, %rdi          # Move image into 1st argument.
    movq    %r11, %rsi          # Move offset into second argument.
    CFI_ADJUST_CFA_OFFSET(56)   # Account for the pushes.
    call    OspImResolvePltEntry    # Call the C handler
    movq    %rax, %r10          # Save the address to return to in a volatile.
    popq    %r9                 # Restore r9 argument.
    popq    %r8                 # Restore r8 argument.
    popq    %rsi                # Restore rsi argument.
    popq    %rdi                # Restore rdi argument.
    popq    %rdx                # Restore rdx argument.
    popq    %rcx                # Restore rcx argument.
    popq    %rax                # Restore rax.
    CFI_ADJUST_CFA_OFFSET(-56)  # Account for the pops.
    jmp     *%r10               # Off to the final destination...

END_FUNCTION(OspImArchResolvePltEntry)

