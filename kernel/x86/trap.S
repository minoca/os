/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    trap.S

Abstract:

    This module implements interrupt and exception trap management, such as
    saving and restoring registers.

Author:

    Evan Green 3-Jul-2012

Environment:

    Kernel mode

--*/

##
## ------------------------------------------------------------------- Includes
##

#include <minoca/kernel/x86.inc>

##
## ---------------------------------------------------------------- Definitions
##

##
## -------------------------------------------------------------------- Globals
##

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

.globl HlVectorStart
.globl HlVectorEnd
.hidden HlVectorStart
.hidden HlVectorEnd

##
## VOID
## ArBreakExceptionHandlerAsm (
##     ULONG ReturnEip,
##     ULONG ReturnCodeSelector,
##     ULONG ReturnEflags
##     )
##

/*++

Routine Description:

    This routine is called directly when an debug exception occurs. It sets up
    the parameters and calls a C routine to handle the break. It then restores
    machine state to return from the exception. The arguments to this function
    are pushed by the hardware.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

FUNCTION(ArBreakExceptionHandlerAsm)
    pushl   $0                      # Push a dummy error code.
    call    ArGenerateTrapFrame     # Create a local trap frame.
    CFI_TRAP_FRAME_PUSHED           # Set unwind info for the debugger.
    pushl   %ebx                    # Push a pointer to it as a parameter.
    call    KeDispatchBreakPointTrap  # Call the main exception handler.
    addl    $0x4, %esp              # Pop the parameter.
    call    ArRestoreTrapFrame      # Restore the trap frame
    CFI_TRAP_FRAME_POPPED           # Let the debugger know.
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(ArBreakExceptionHandlerAsm)

##
## VOID
## KdNmiHandlerAsm (
##     VOID
##     )
##

/*++

Routine Description:

    This routine is called directly when an NMI occurs. Since it is a hardware
    task switch, no registers need to be saved.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

FUNCTION(KdNmiHandlerAsm)
    LoadKernelDataSegments          # Load valid data segments.
    call    KeDispatchNmiTrap       # Call to the C routine to handle this mess.
    iret                            # Return from the exception.
    jmp     KdNmiHandlerAsm         # The next NMI starts here, jump back up.

END_FUNCTION(KdNmiHandlerAsm)

##
## VOID
## ArSingleStepExceptionHandlerAsm (
##     ULONG ReturnEip,
##     ULONG ReturnCodeSelector,
##     ULONG ReturnEflags
##     )
##

/*++

Routine Description:

    This routine is called directly when an debug exception occurs. It sets up
    the parameters and calls the executive to dispatch the trap.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

FUNCTION(ArSingleStepExceptionHandlerAsm)
    pushl   $0                      # Push a dummy error code.
    call    ArGenerateTrapFrame     # Create a local trap frame.
    CFI_TRAP_FRAME_PUSHED           # Set unwind info for the debugger.
    pushl   %ebx                    # Push a pointer to it as a parameter.
    call    KeDispatchSingleStepTrap  # Call the main exception handler.
    addl    $0x4, %esp              # Pop the parameter.
    call    ArRestoreTrapFrame      # Restore the trap frame
    CFI_TRAP_FRAME_POPPED           # Let the debugger know.
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(ArSingleStepExceptionHandlerAsm)

##
## VOID
## KdDebugServiceHandlerAsm (
##     ULONG ReturnEip,
##     ULONG ReturnCodeSelector,
##     ULONG ReturnEflags
##     )
##

/*++

Routine Description:

    This routine is entered via an IDT entry to request debug service. It sets
    up the parameters and calls KdDebugExceptionHandler, and then restores
    machine state to return from the exception. The arguments to this function
    are pushed by the hardware. Upon Entry:

        eax - Supplies the debug service request.

        ecx - Supplies the parameter to the request.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

FUNCTION(KdDebugServiceHandlerAsm)
    pushl   $0                      # Push a dummy error code.
    call    ArGenerateTrapFrame     # Create a local trap frame.
    CFI_TRAP_FRAME_PUSHED           # Set unwind info for the debugger.
    pushl   %ebx                    # Push a pointer to the trap frame.
    call    KeDispatchDebugServiceTrap  # Call the main exception handler.
    addl    $0x4, %esp              # Pop the parameter.
    call    ArRestoreTrapFrame      # Restore the trap frame
    CFI_TRAP_FRAME_POPPED           # Let the debugger know.
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(KdDebugServiceHandlerAsm)

##
## VOID
## ArDivideByZeroExceptionHandlerAsm (
##     ULONG ReturnEip,
##     ULONG ReturnCodeSelector,
##     ULONG ReturnEflags
##     )
##

/*++

Routine Description:

    This routine is called directly when a divide by zero exception occurs.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

FUNCTION(ArDivideByZeroExceptionHandlerAsm)
    pushl   $0                      # Push a dummy error code.
    call    ArGenerateTrapFrame     # Create a local trap frame.
    CFI_TRAP_FRAME_PUSHED           # Set unwind info for the debugger.
    pushl   %ebx                    # Push a pointer to it as a parameter.
    call    KeDispatchDivideByZeroTrap  # Call the main exception handler.
    addl    $0x4, %esp              # Pop the parameters.
    call    ArRestoreTrapFrame      # Restore the trap frame
    CFI_TRAP_FRAME_POPPED           # Let the debugger know.
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(ArDivideByZeroExceptionHandlerAsm)

##
## VOID
## ArFpuAccessExceptionHandlerAsm (
##     ULONG ReturnEip,
##     ULONG ReturnCodeSelector,
##     ULONG ReturnEflags
##     )
##

/*++

Routine Description:

    This routine is called directly when floating point access occurs and the
    TS bit in CR0 is

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

FUNCTION(ArFpuAccessExceptionHandlerAsm)
    pushl   $0                      # Push a dummy error code.
    call    ArGenerateTrapFrame     # Create a local trap frame.
    CFI_TRAP_FRAME_PUSHED           # Set unwind info for the debugger.
    pushl   %ebx                    # Push a pointer to it as a parameter.
    call    KeDispatchFpuAccessTrap # Call the main exception handler.
    addl    $0x4, %esp              # Pop the parameters.
    call    ArRestoreTrapFrame      # Restore the trap frame
    CFI_TRAP_FRAME_POPPED           # Let the debugger know.
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(ArFpuAccessExceptionHandlerAsm)

##
## VOID
## ArDoubleFaultHandlerAsm (
##     VOID
##     )
##

/*++

Routine Description:

    This routine is entered via an IDT entry when a double fault exception
    occurs. Double faults are non-recoverable. This machine loops attempting
    to enter the debugger indefinitely.

Arguments:

    None.

Return Value:

    None, this routine does not return.

--*/

FUNCTION(ArDoubleFaultHandlerAsm)
    LoadKernelDataSegments         # Load valid data segments.
    call    ArpHandleDoubleFault   # Call to the C routine to handle this mess.
    nop

END_FUNCTION(ArDoubleFaultHandlerAsm)

##
## VOID
## ArProtectionFaultHandlerAsm (
##     ULONG ReturnEip,
##     ULONG ReturnCodeSelector,
##     ULONG ReturnEflags
##     )
##

/*++

Routine Description:

    This routine is called directly when a general protection fault occurs.
    It's job is to prepare the trap frame, call the appropriate handler, and
    then restore the trap frame.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

FUNCTION(ArProtectionFaultHandlerAsm)
    call    ArGenerateTrapFrame     # Create a local trap frame.
    CFI_TRAP_FRAME_PUSHED           # Set unwind info for the debugger.
    pushl   %ebx                    # Push a pointer to it as a parameter.
    call    KeDispatchProtectionFault # Call the main handler.
    addl    $0x4, %esp              # Pop the parameter.
    call    ArRestoreTrapFrame      # Restore the trap frame
    CFI_TRAP_FRAME_POPPED           # Let the debugger know.
    addl    $4, %esp
    iret                            # Return from the exception.

END_FUNCTION(ArProtectionFaultHandlerAsm)

##
## VOID
## ArMathFaultHandlerAsm (
##     ULONG ReturnEip,
##     ULONG ReturnCodeSelector,
##     ULONG ReturnEflags
##     )
##

/*++

Routine Description:

    This routine is called directly when a x87 FPU fault occurs.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

FUNCTION(ArMathFaultHandlerAsm)
    pushl   $0                      # Push a dummy error code.
    call    ArGenerateTrapFrame     # Create a local trap frame.
    CFI_TRAP_FRAME_PUSHED           # Set unwind info for the debugger.
    pushl   %ebx                    # Push a pointer to it as a parameter.
    call    KeDispatchMathFault     # Call the main handler.
    addl    $0x4, %esp              # Pop the parameter.
    call    ArRestoreTrapFrame      # Restore the trap frame
    CFI_TRAP_FRAME_POPPED           # Let the debugger know.
    addl    $4, %esp                # Pop dummy error code.
    iret                            # Return from the exception.

END_FUNCTION(ArMathFaultHandlerAsm)

##
## VOID
## ArTrapSystemCallHandlerAsm (
##     ULONG ReturnEip,
##     ULONG ReturnCodeSelector,
##     ULONG ReturnEflags
##     )
##

/*++

Routine Description:

    This routine is entered when the sysenter routine is entered with the TF
    flag set. It performs a normal save and sets the TF.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

FUNCTION(ArTrapSystemCallHandlerAsm)

    ##
    ## ESP is currently pointing at the processor double fault stack, which
    ## is also the main TSS. Switch to the thread stack.
    ##

    movl    TSS_ESP0(%esp), %esp    # Load the stack.
    sti                             # Re-enable interrupts.

    ##
    ## Fake a user mode exception by pushing SS, Esp, Eflags, CS and Eip.
    ##

    pushl   $USER_DS                # Push user mode SS.
    pushl   %eax                    # Push user ESP supplied by sysenter.
    pushfl                          # Push the Eflags.
    pushl   $USER_CS                # Push user CS.
    pushl   %ebx                    # Push return address supplied by sysenter.
    pushl   $0                      # Push a dummy error code.
    call    ArGenerateTrapFrame     # Create a local trap frame.
    CFI_TRAP_FRAME_PUSHED           # Set unwind info for the debugger.
    orl     $IA32_EFLAG_TF, TRAP_EFLAGS(%ebx) # Set the trap flag.
    jmp     ArSystemCallHandlerAfterTrapSave  # Go to the regular path.

END_FUNCTION(ArTrapSystemCallHandlerAsm)

##
## INTN
## ArSystemCallHandlerAsm (
##     ULONG ReturnEip,
##     ULONG ReturnCodeSelector,
##     ULONG ReturnEflags
##     )
##

/*++

Routine Description:

    This routine is entered via an IDT entry to service a user mode request.
    Ecx contains the system call number, and Edx contains the argument.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

FUNCTION(ArSystemCallHandlerAsm)
    pushl   $0                      # Push a dummy error code.
    call    ArGenerateTrapFrame     # Create a local trap frame.
    CFI_TRAP_FRAME_PUSHED           # Set unwind info for the debugger.

ArSystemCallHandlerAfterTrapSave:
    mov     TRAP_ECX(%ebx), %ecx    # Get ecx, containing the call number.
    mov     TRAP_EDX(%ebx), %edx    # Get edx, containing the parameter.

    ##
    ## Dispatching the signal takes the system call number, system call
    ## parameter and trap frame as arguments. This means that the system call
    ## number and parameter need to be saved across the call to
    ## KeSystemCallHandler.
    ##

    pushl   %edx                    # Push system call parameter for dispatch.
    pushl   %ecx                    # Push system call number for dispatch.

    ##
    ## Push the parameters for the system call handler. The compiler is free to
    ## modify the parameters on the stack in the callee, so this second push is
    ## necessary.
    ##

    pushl   %ebx                    # Push a pointer to the trap frame.
    pushl   %edx                    # Push system call parameter.
    pushl   %ecx                    # Push system call number.
    CFI_ADJUST_CFA_OFFSET(20)       # Tell the debugger.
    call    KeSystemCallHandler     # Call the main exception handler.
    addl    $0xC, %esp              # Pop the parameters.
    CFI_ADJUST_CFA_OFFSET(-12)      # Tell the debugger.
    movl    %eax, TRAP_EAX(%ebx)    # Save the return value in the trap frame.

    ##
    ## If a signal is pending, then go attempt to dispatch it as the trap frame
    ## is already complete.
    ##

    movl    %fs:(PROCESSOR_BLOCK_RUNNING_THREAD), %ecx # Get current thread.
    cmpl    $ThreadSignalPending, THREAD_SIGNAL_PENDING(%ecx) # Signal test.
    jnz     ArSystemCallHandlerExit

    ##
    ## Push the trap frame as the first parameter. The system call number and
    ## system call parameter were saved on the stack earlier for this call.
    ##

    pushl   %ebx                    # Push trap frame.
    CFI_ADJUST_CFA_OFFSET(4)        # Notify about the push.
    call    PsDispatchPendingSignalsOnCurrentThread # Dispatch signals.
    addl    $0x4, %esp              # Pop the trap frame.
    CFI_ADJUST_CFA_OFFSET(-4)       # Notify about the pop.

ArSystemCallHandlerExit:
    addl    $0x8, %esp              # Pop the system call number and parameter.
    CFI_ADJUST_CFA_OFFSET(-8)       # Notify about the pop.
    call    ArRestoreTrapFrame      # Restore the trap frame
    CFI_TRAP_FRAME_POPPED           # Let the debugger know.
    addl    $0x4, %esp              # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(ArSystemCallHandlerAsm)

##
## INTN
## ArSysenterHandlerAsm (
##     VOID
##     )
##

/*++

Routine Description:

    This routine is executed when user mode invokes the SYSENTER instruction.
    Upon entry, CS, EIP, and ESP are set to predefined values set in MSRs.

Arguments:

    None.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

FUNCTION(ArSysenterHandlerAsm)

    ##
    ## ESP is currently pointing at the processor double fault stack, which
    ## is also the main TSS. Switch to the thread stack.
    ##

    movl    TSS_ESP0(%esp), %esp    # Load the stack.

    ##
    ## Make a fake trap frame but fill in the bare minimum: Eip and Esp.
    ## These are needed so they can be saved if a signal is dispatched. CS is
    ## also needed so kernel code can determine whether the trap frame is
    ## from user or kernel mode.
    ##

    pushl   %eax                    # Save user ESP in trap frame.
    LoadKernelDataSegments          # Load kernel data segments (using EAX).
    sti                             # Re-enable interrupts.
    subl    $(TRAP_FRAME_SIZE - 4), %esp  # Allocate the rest of the trap frame.
    movl    %ebx, TRAP_EIP(%esp)    # EBX contains the return address.
    movl    %esp, %ebx              # Save the trap frame in EBX.

    ##
    ## There's a trap frame at ebx, which is nice since it won't require
    ## maintenance over all these upcoming push/pops of the stack.
    ##

    CFI_DEF_CFA(%ebx, 0)
    CFI_OFFSET(%eip, TRAP_EIP)
    CFI_OFFSET(%esp, TRAP_ESP)

    ##
    ## Move user DS (rather than user CS) into the trap frame to indicate
    ## 1) this is a user mode trap frame and 2) it's incomplete.
    ##

    movl    $USER_DS, TRAP_CS(%ebx) # Indicate a user mode trap frame.

    ##
    ## Dispatching the signal takes the system call number, system call
    ## parameter and trap frame as arguments. This means that the system call
    ## number and parameter need to be saved across the call to
    ## KeSystemCallHandler.
    ##

    pushl   %edx                    # Push system call parameter for dispatch.
    pushl   %ecx                    # Push system call number for dispatch.

    ##
    ## Push the parameters for the system call handler and execute it.
    ##

    pushl   %ebx                    # Push a pointer to the sort-of trap frame.
    pushl   %edx                    # Push EDX, the system call parameter.
    pushl   %ecx                    # Push saved ECX, the system call number.
    call    KeSystemCallHandler     # Call out to the main service handler.
    addl    $0xC, %esp              # Pop the system call parameter.

    ##
    ## If a signal is pending, then save the full trap frame and attempt to
    ## dispatch the signal. Note that the trap frame was allocated on the
    ## kernel stack and may be copied out to user mode. Don't leak kernel stack
    ## values.
    ##

    movl    %fs:(PROCESSOR_BLOCK_RUNNING_THREAD), %ecx # Get current thread.
    cmpl    $ThreadSignalPending, THREAD_SIGNAL_PENDING(%ecx) # Signal test.
    jnz     ArSysenterHandlerFastRestore # Exit via the fast restore path.

    ##
    ## Save the default user mode segment selectors. Update user CS as the trap
    ## frame is about to become full.
    ##

    movl    $USER_CS, TRAP_CS(%ebx)    # Save CS.
    movl    $USER_DS, TRAP_DS(%ebx)    # Save DS.
    movl    $USER_DS, TRAP_ES(%ebx)    # Save ES.
    movl    $GDT_THREAD, TRAP_FS(%ebx) # Save FS.
    movl    $GDT_THREAD, TRAP_GS(%ebx) # Save GS.
    movl    $USER_DS, TRAP_SS(%ebx)    # Save SS.

    ##
    ## Save the registers.
    ##

    movl    %eax, TRAP_EAX(%ebx) # Save the return value in the trap frame.
    movl    $0x0, TRAP_EBX(%ebx) # Zero EBX. It holds the kernel trap pointer.
    movl    $0x0, TRAP_ECX(%ebx) # Zero ECX. It is volatile.
    movl    $0x0, TRAP_EDX(%ebx) # Zero EDX. It is volatile.
    movl    %esi, TRAP_ESI(%ebx) # Save ESI. It is non-volatile.
    movl    %edi, TRAP_EDI(%ebx) # Save EDI. It is non-volatile.
    movl    %ebp, TRAP_EBP(%ebx) # Save EBP. It is non-volatile.
    movl    $0x0, TRAP_ERRORCODE(%ebx) # Scrub the error code.
    movl    $0x0, TRAP_EFLAGS(%ebx) # Scrub the EFLAGS.

    ##
    ## Push the trap frame as the first parameter. The system call number and
    ## system call paramter were saved on the stack earlier for this call.
    ##

    pushl   %ebx                    # Pass the trap frame.
    call    PsDispatchPendingSignalsOnCurrentThread # Dispatch signals.
    addl    $0xC, %esp              # Pop all three parameters.

    ##
    ## Restore the non-volatile registers in case they were modified by the
    ## trap frame.
    ##

    movl    TRAP_ESI(%esp), %esi
    movl    TRAP_EDI(%esp), %edi
    movl    TRAP_EBP(%esp), %ebp

    ##
    ## Restore EAX in case the return value was modified. The other volatiles
    ## (ECX and EDX) are used specifically by sysexit to restore the stack and
    ## instruction pointer.
    ##

    movl    TRAP_EAX(%esp), %eax

    ##
    ## Restore the segment registers. SS and CS are restored by sysexit. Once
    ## DS is restored then only memory accesses through ESP are allowed.
    ##

    movl    TRAP_DS(%esp), %ecx  # Restore DS.
    movw    %cx, %ds             #
    movl    TRAP_ES(%esp), %ecx  # Restore ES.
    movw    %cx, %es             #
    movl    TRAP_FS(%esp), %ecx  # Restore FS.
    movw    %cx, %fs             #
    movl    TRAP_GS(%esp), %ecx  # Restore GS.
    movw    %cx, %gs             #

    ##
    ## Upon sysenter ebx held the return address, making it effectively
    ## volatile.
    ##

    xorl    %ebx, %ebx
    jmp     ArSysenterHandlerExit

    ##
    ## Reset the segment registers to user mode and return.
    ##

ArSysenterHandlerFastRestore:
    mov     $USER_DS, %cx           # Get the user mode DS.
    mov     %cx, %ds                # Move to DS.
    mov     %cx, %es                # Move to ES.
    mov     $GDT_THREAD, %cx        # Get the user-mode GS.
    mov     %cx, %fs                # Move to FS.
    mov     %cx, %gs                # Move to GS.
    addl    $0x8, %esp              # Pop the system call number and parameter.

ArSysenterHandlerExit:
    CFI_DEF_CFA(%esp, 0)            # The trap frame is at ESP, EBX is gone.

    ##
    ## Restore some portions of the pseudo trap frame. Do not zero EAX as it
    ## holds the return value from the system call.
    ##

    movl    TRAP_ESP(%esp), %ecx    # Sysexit moves ECX to ESP.
    movl    TRAP_EIP(%esp), %edx    # Sysexit moves EDX to EIP.
    sysexit                         # Return to user mode, slickly.

END_FUNCTION(ArSysenterHandlerAsm)

##
## VOID
## ArpPageFaultHandlerAsm (
##     ULONG ReturnEip,
##     ULONG ReturnCodeSelector,
##     ULONG ReturnEflags
##     )
##

/*++

Routine Description:

    This routine is called directly when a page fault occurs.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the
        fault.

    ReturnCodeSelector - Supplies the code selector the code that faulted was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the fault.

Return Value:

    None.

--*/

FUNCTION(ArpPageFaultHandlerAsm)
    call    ArGenerateTrapFrame     # Create a local trap frame.
    CFI_TRAP_FRAME_PUSHED           # Set unwind info for the debugger.
    movl    %cr2, %ecx              # Get the faulting address.
    xor     %edx, %edx              # Zero edx.
    movl    %edx, %cr2              # Clear CR2.
    sti                             # Re-enable interrupts.
    pushl   %ebx                    # Push a pointer to to the trap frame.
    pushl   %ecx                    # Push CR2.
    CFI_ADJUST_CFA_OFFSET(8)        # Tell the debugger.
    call    KeDispatchPageFault     # Call the main exception handler.
    addl    $8, %esp                # Pop the parameters.
    CFI_ADJUST_CFA_OFFSET(-8)       # Pop them in the debugger.
    call    ArRestoreTrapFrame      # Restore the trap frame
    CFI_TRAP_FRAME_POPPED           # Let the debugger know.
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(ArpPageFaultHandlerAsm)

##
## VOID
## HlSpuriousInterruptHandlerAsm (
##     ULONG ReturnEip,
##     ULONG ReturnCodeSelector,
##     ULONG ReturnEflags
##     )
##

/*++

Routine Description:

    This routine handles spurious interrupts. It does not require an EOI or
    other interrupt acknowledgement.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

FUNCTION(HlSpuriousInterruptHandlerAsm)
    pushl   $0                      # Push a dummy error code.
    call    ArGenerateTrapFrame     # Create a local trap frame.
    CFI_TRAP_FRAME_PUSHED           # Set unwind info for the debugger.
    addl    $1, HlSpuriousInterruptCount      # Count interrupts
    call    ArRestoreTrapFrame      # Restore the trap frame
    CFI_TRAP_FRAME_POPPED           # Let the debugger know.
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(HlSpuriousInterruptHandlerAsm)

##
## VOID
## ArRestoreTrapFrame (
##     PTRAP_FRAME TrapFrame
##     )
##

/*++

Routine Description:

    This routine restores information contained in a trap frame to the
    processor and prepares the machine for an iret back to the code that
    generated this trap frame. It's not really a function because it assumes
    a specific stack layout and modifies data that technically belongs to the
    caller. It should only be called immediately before returning from an
    exception or interrupt.

Arguments:

    TrapFrame - Supplies the trap frame to restore, in ebx.

Return Value:

    Upon return, the trap frame will have been popped off the stack, and the
    machine will be in the same state as right after the exception happened.

--*/

FUNCTION(ArRestoreTrapFrame)
    mov     %ebx, %ecx                      # Move trap frame to ecx.
    addl    $TRAP_FRAME_SIZE+16, %ebx       # Compute the pre-exception stack.
    movl    TRAP_CS(%ecx), %edx             # Get the destination CS.

##
## The exception is returning to either kernel or user mode. Either way restore
## the common data segment registers. Hold off on DS, as this routine will
## make a couple more DS: accesses. Save it in ESI.
##

    movl    TRAP_DS(%ecx), %esi             # Save DS into ESI for now.
    movl    TRAP_ES(%ecx), %eax             # Restore ES.
    movw    %ax, %es                        #
    movl    TRAP_FS(%ecx), %eax             # Restore FS.
    movw    %ax, %fs                        #
    movl    TRAP_GS(%ecx), %eax             # Restore GS.
    movw    %ax, %gs                        #

##
## Restore the remaining registers based on the destination mode.
##

    movl    %edx, %eax                      # Get CS (loaded above).
    andl    $SEGMENT_PRIVILEGE_MASK, %eax   # AND out the privilege.
    jz      RestoreTrapFrameToKernelMode    # Jump over if not.

##
## The exception is going to jump back into user mode, so put the stack
## pointer and segments back into the exception-generated part of the stack.
##

    movl    TRAP_ESP(%ecx), %eax            # Restore Esp.
    movl    %eax, TRAP_RET_ESP(%ecx)        #
    movl    TRAP_SS(%ecx), %eax             # Restore SS.
    movl    %eax, TRAP_RET_SS(%ecx)         #
    jmp     RestoreTrapFrameGeneralRegisters        # Jump to the end.

RestoreTrapFrameToKernelMode:

##
## The exception came from kernel mode, so restore the stack segment register.
##

    movl    TRAP_SS(%ecx), %eax      # Restore SS. If this doesn't allow access
    movw    %ax, %ss                 # to the current stack, this will be bad.

##
## Build the iret return. The parameters going on the new stack are Ebx, Return
## Address, Error Code, Eip, CS, and Eflags.
##
## Note that if the stack pointer doesn't change, the Ebx and Return address
## values destroy data that was on the stack there (immediately after the
## Eflags, CS, Eip). This happens to be the last two values in the trap frame
## structure. Luckily those members are Esp and Eflags, which are restored
## immediately before their values are destroyed.
##

    movl    TRAP_ESP(%ecx), %ebx     # Get the kernel Esp.

RestoreTrapFrameGeneralRegisters:
    subl    $24, %ebx                # Make room for the new parameters.
    movl    TRAP_EIP(%ecx), %eax     # Restore Eip.
    movl    %eax, 12(%ebx)           #
    movl    %edx, 16(%ebx)           # Restore CS.
    movl    TRAP_EFLAGS(%ecx), %eax  # Restore Eflags.
    movl    %eax, 20(%ebx)           #
    movl    TRAP_EBX(%ecx), %eax     # Save Ebx.
    movl    %eax, (%ebx)             #
    movl    (%esp), %eax             # Save this function's return address.
    movl    %eax, 4(%ebx)            #

##
## Now that all DS: accesses are finished, restore DS.
##

    movw    %si, %ds

##
## Move the trap frame pointer to the stack, popping everything up until then,
## including the extended state.
##

    movl    %ecx, %esp               # Pop up to the trap frame.

##
## Restore the general registers.
##

    movl    TRAP_EAX(%esp), %eax     #
    movl    TRAP_ECX(%esp), %ecx     #
    movl    TRAP_EDX(%esp), %edx     #
    movl    TRAP_ESI(%esp), %esi     #
    movl    TRAP_EDI(%esp), %edi     #
    movl    TRAP_EBP(%esp), %ebp     #

##
## Transition to the new kernel mode stack pointer, pop Ebx, and return.
##

    movl    %ebx, %esp              # Move stacks!
    popl    %ebx                    # Restore Ebx.
    ret

END_FUNCTION(ArRestoreTrapFrame)

##
## --------------------------------------------------------- Internal Functions
##

##
## This macro stamps out the assembly dispatch code necessary for interrupts
## received at each vector. It will create code for all vectors between
## MinimumVector and MaximumVector.
##

.macro InterruptVector _Vector

    ##
    ## 0x6A xx is the instruction for push imm8, except the immediate is sign
    ## extended. The assembler will use the longer form for numbers >= 0x80
    ## since those should not be sign extended. Use the shorter form directly
    ## here to save space, and deal with it using a cast in the C code.
    ##

    .byte   0x6A
    .byte   (\_Vector)
    jmp     KeInterruptEntry

.endm

.macro InterruptVectors16 _Vector
    InterruptVector (\_Vector)
    InterruptVector (\_Vector + 1)
    InterruptVector (\_Vector + 2)
    InterruptVector (\_Vector + 3)
    InterruptVector (\_Vector + 4)
    InterruptVector (\_Vector + 5)
    InterruptVector (\_Vector + 6)
    InterruptVector (\_Vector + 7)
    InterruptVector (\_Vector + 8)
    InterruptVector (\_Vector + 9)
    InterruptVector (\_Vector + 10)
    InterruptVector (\_Vector + 11)
    InterruptVector (\_Vector + 12)
    InterruptVector (\_Vector + 13)
    InterruptVector (\_Vector + 14)
    InterruptVector (\_Vector + 15)

.endm

##
## Now actually instantiate the macro to create the vector code.
##

HlVectorStart:

InterruptVectors16 0x30
InterruptVectors16 0x40
InterruptVectors16 0x50
InterruptVectors16 0x60
InterruptVectors16 0x70
InterruptVectors16 0x80
InterruptVectors16 0x90
InterruptVectors16 0xA0
InterruptVectors16 0xB0
InterruptVectors16 0xC0
InterruptVectors16 0xD0
InterruptVectors16 0xE0
InterruptVectors16 0xF0

HlVectorEnd:

##
## PTRAP_FRAME
## ArGenerateTrapFrame (
##     ULONG ReturnEip,
##     ULONG ReturnCs,
##     ULONG ReturnEflags,
##     ...
##     )
##

/*++

Routine Description:

    This routine generates a trap frame based on the data pushed onto the
    stack by the processor after an exception. It is not really a function
    in that it assumes a certain stack layout and will modify data that
    belongs to the caller. This function should only be called immediately
    after an interrupt/exception.

Arguments:

    ReturnEip - Supplies the instruction that generated the exception.

    ReturnCs - Supplies the code selector of the code that generated the
        exception.

    ReturnEflags - Supplies the flags of the code that generated the
        exception.

Return Value:

    Returns a pointer to the trap frame in ebx.

--*/

FUNCTION(ArGenerateTrapFrame)

##
## Allocate room on the stack for the trap frame plus the return address,
## minus the original return address.
##

    subl    $TRAP_FRAME_SIZE, %esp  #
    pushl   %eax                    # Save eax for a moment while the return
    movl    TRAP_FRAME_SIZE+4(%esp), %eax     # address is moved.
    movl    %eax, 4(%esp)           #
    popl    %eax                    # Restore eax
    movl    %eax, TRAP_EAX+4(%esp)  # Save the general registers.
    movl    %ebx, TRAP_EBX+4(%esp)  #
    movl    %ecx, TRAP_ECX+4(%esp)  #
    movl    %edx, TRAP_EDX+4(%esp)  #
    movl    %esi, TRAP_ESI+4(%esp)  #
    movl    %edi, TRAP_EDI+4(%esp)  #
    movl    %ebp, TRAP_EBP+4(%esp)  #
    movl    TRAP_RET_ERRORCODE+4(%esp), %eax  # Save the error code.
    movl    %eax, TRAP_ERRORCODE+4(%esp)      #
    movl    TRAP_RET_EIP+4(%esp), %eax        # Save the return address.
    movl    %eax, TRAP_EIP+4(%esp)            #
    movl    TRAP_RET_CS+4(%esp), %ebx         # Save the return CS.
    movl    %ebx, TRAP_CS+4(%esp)             #
    movl    TRAP_RET_EFLAGS+4(%esp), %eax     # Save eflags.
    movl    %eax, TRAP_EFLAGS+4(%esp)

##
## Figure out if a ring change occurred.
##

    andl    $SEGMENT_PRIVILEGE_MASK, %ebx   # exception had a ring change.
    jz      GenerateTrapFrameFromKernelMode # Jump over if not.

##
## The exception caused a privilege level change, so the stack contains the
## following ULONGs: Eip, CS, Eflags, Esp, and SS. Get the other segment
## selectors from their current values.
##

    movl    TRAP_RET_ESP+4(%esp), %eax      # Save Esp.
    movl    %eax, TRAP_ESP+4(%esp)          #
    movl    TRAP_RET_SS+4(%esp), %eax       # Save SS.
    movl    %eax, TRAP_SS+4(%esp)           #
    xorl    %eax, %eax                      # Zero out eax.
    movw    %ds, %ax                        # Save DS.
    movl    %eax, TRAP_DS+4(%esp)           #
    movw    %es, %ax                        # Save ES.
    movl    %eax, TRAP_ES+4(%esp)           #
    movw    %fs, %ax                        # Save FS.
    movl    %eax, TRAP_FS+4(%esp)           #
    movw    %gs, %ax                        # Save GS.
    movl    %eax, TRAP_GS+4(%esp)           #
    jmp     GenerateTrapFrameEnd            # All done.

GenerateTrapFrameFromKernelMode:

##
## The exception came from kernel mode, so the only things pushed on the stack
## by the processor are Eip, CS, and Eflags. The data segments also don't need
## to be saved. Get the data segments from their current values. Since there
## was no stack change, the Esp is simply this current one except all the
## stuff pushed by the exception, plus the error code.
##

    movl    %esp, %eax                      # Save Esp.
    addl    $TRAP_FRAME_SIZE+20, %eax       # Remove exception stack items.
    movl    %eax, TRAP_ESP+4(%esp)          #
    xorl    %eax, %eax                      # Zero out eax.
    movw    %ds, %ax                        # Save DS.
    movl    %eax, TRAP_DS+4(%esp)           #
    movw    %es, %ax                        # Save ES.
    movl    %eax, TRAP_ES+4(%esp)           #
    movw    %fs, %ax                        # Save FS.
    movl    %eax, TRAP_FS+4(%esp)           #
    movw    %gs, %ax                        # Save GS.
    movl    %eax, TRAP_GS+4(%esp)           #
    movw    %ss, %ax                        # Save SS.
    movl    %eax, TRAP_SS+4(%esp)           #

GenerateTrapFrameEnd:
    LoadKernelDataSegments                  # Load valid data segments.
    popl    %edi                            # Pop the return address.
    movl    %esp, %ebx                      # Return the trap pointer.
    jmp     *%edi                           # Return

END_FUNCTION(ArGenerateTrapFrame)

##
## Define the common interrupt entry code. At this point the vector number has
## been pushed into the error code slot, but nothing else has been done. Note
## that this code needs to be far enough away from the vectors themselves so
## that none of the jumps in the vectors turn into shorter instructions
## (distance >= 0x100).
##

FUNCTION(KeInterruptEntry)
    call    ArGenerateTrapFrame     # Create a local trap frame.
    CFI_TRAP_FRAME_PUSHED           # Set unwind info for the debugger.
    pushl   %ebx                    # Push a pointer to it as a parameter.
    CFI_ADJUST_CFA_OFFSET(4)
    call    KeDispatchInterrupt     # Dispatch the interrupt.
    addl    $4, %esp                # Pop the parameters.
    CFI_ADJUST_CFA_OFFSET(-4)
    call    ArRestoreTrapFrame      # Restore state.
    CFI_TRAP_FRAME_POPPED           # Let the debugger know.
    addl    $4, %esp                # Pop the error code.
    CFI_OFFSET(%eip, 0)             # The return address is right there.
    iret                            # Return from the exception.

END_FUNCTION(KeInterruptEntry)

