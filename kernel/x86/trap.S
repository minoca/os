/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

#include <minoca/x86.inc>

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
    pushl   %ebx                    # Push a pointer to it as a parameter.
    call    KeDispatchBreakPointTrap  # Call the main exception handler.
    addl    $0x4, %esp              # Pop the parameter.
    call    ArRestoreTrapFrame      # Restore the trap frame
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
    pushl   %ebx                    # Push a pointer to it as a parameter.
    call    KeDispatchSingleStepTrap  # Call the main exception handler.
    addl    $0x4, %esp              # Pop the parameter.
    call    ArRestoreTrapFrame      # Restore the trap frame
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
    pushl   %ebx                    # Push a pointer to the trap frame.
    call    KeDispatchDebugServiceTrap  # Call the main exception handler.
    addl    $0x4, %esp              # Pop the parameter.
    call   ArRestoreTrapFrame       # Restore the trap frame
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
    pushl   %ebx                    # Push a pointer to it as a parameter.
    call    KeDispatchDivideByZeroTrap  # Call the main exception handler.
    addl    $0x4, %esp              # Pop the parameters.
    call    ArRestoreTrapFrame      # Restore the trap frame
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
    pushl   %ebx                    # Push a pointer to it as a parameter.
    call    KeDispatchFpuAccessTrap # Call the main exception handler.
    addl    $0x4, %esp              # Pop the parameters.
    call    ArRestoreTrapFrame      # Restore the trap frame
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
    pushl   %ebx                    # Push a pointer to it as a parameter.
    call    KeDispatchProtectionFault # Call the main handler.
    addl    $0x4, %esp              # Pop the parameter.
    call    ArRestoreTrapFrame      # Restore the trap frame
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
    pushl   %ebx                    # Push a pointer to it as a parameter.
    call    KeDispatchMathFault     # Call the main handler.
    addl    $0x4, %esp              # Pop the parameter.
    call    ArRestoreTrapFrame      # Restore the trap frame
    addl    $4, %esp                # Pop dummy error code.
    iret                            # Return from the exception.

END_FUNCTION(ArMathFaultHandlerAsm)

##
## VOID
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

    None.

--*/

FUNCTION(ArSystemCallHandlerAsm)
    pushl   $0                      # Push a dummy error code.
    call    ArGenerateTrapFrame     # Create a local trap frame.
    mov     TRAP_ECX(%ebx), %ecx    # Get ecx, containing the call number.
    mov     TRAP_EDX(%ebx), %edx    # Get edx, containing the parameter.
    pushl   %ebx                    # Push a pointer to the trap frame.
    pushl   %edx                    # Push system call parameter.
    pushl   %ecx                    # Push system call number.
    call    KeSystemCallHandler     # Call the main exception handler.
    addl    $0xc, %esp              # Pop the parameters.
    call    ArRestoreTrapFrame      # Restore the trap frame
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(ArSystemCallHandlerAsm)

##
## VOID
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

    None.

--*/

FUNCTION(ArSysenterHandlerAsm)

    ##
    ## To avoid having to set the MSR on context switches when using sysenter,
    ## ESP is left at 0. So the first step is to set up the stack.
    ##

    LoadKernelDataSegments
    movl    %fs:(PROCESSOR_BLOCK_TSS), %eax  # Get the PROCESSOR_BLOCK.Tss.
    movl    TSS_ESP0(%eax), %esp    # Load the stack.
    sti                             # Re-enable interrupts.

    ##
    ## Make a fake trap frame and fill in the registers that need to be saved.
    ## This is needed because if a signal comes in these may be changed.
    ##

    subl    $TRAP_FRAME_SIZE, %esp  # Allocate trap frame space.
    movl    %edx, TRAP_EIP(%esp)    # EDX contains the return address.
    movl    %ecx, TRAP_ESP(%esp)    # ECX contains return stack.
    movl    $0x0, TRAP_EAX(%esp)    # Save 0 to EAX.
    movl    %ebx, TRAP_EBX(%esp)    # Save EBX.
    movl    %esi, TRAP_ESI(%esp)    # Save ESI.
    movl    %edi, TRAP_EDI(%esp)    # Save EDI.
    movl    %ebp, TRAP_EBP(%esp)    # Save EBP.
    pushfl                          # Push eflags.
    popl    %eax                    # Get eflags in register.
    movl    %eax, TRAP_EFLAGS(%esp) # Save eflags into trap frame.
    pushl   %esp                    # Push a pointer to the sort-of trap frame.
    pushl   %edi                    # Push EDI, the system call parameter.
    pushl   %esi                    # Push ESI, the system call number.
    call    KeSystemCallHandler     # Call out to the main service handler.
    addl    $0xC, %esp              # Pop the parameters.

    ##
    ## Restore some portions of the pseudo trap frame. The signal application
    ## routine may have adjusted them.
    ##

    movl    TRAP_EBX(%esp), %ebx    # Restore EBX.
    movl    TRAP_ESI(%esp), %esi    # Restore ESI.
    movl    TRAP_EDI(%esp), %edi    # Restore EDI.
    movl    TRAP_EBP(%esp), %ebp    # Restore EBP.
    movl    TRAP_ESP(%esp), %ecx    # Sysexit moves ECX to ESP.
    movl    TRAP_EIP(%esp), %edx    # Sysexit moves EDX to EIP.

    ##
    ## Take a look at eflags. If it's got the trap flag set, then an iret
    ## return is required.
    ##

    movl    TRAP_EFLAGS(%esp), %eax # Get eflags.
    test    $IA32_EFLAG_TF, %eax    # test trap flag.
    jnz     ArSysenterHandlerAsmIretReturn  # Return with iret if TF set.

    ##
    ## Ordinarily this would be the place to pop the trap frame off the stack,
    ## but since sysenter always moves it back to the same place there's no
    ## need. Just jump back to user mode. Replace EAX as it has been used
    ## locally and a signal handler may be looking to use the value stored in
    ## the trap frame.
    ##

    mov     $USER_DS, %ax           # Get the user mode DS.
    mov     %ax, %ds                # Move to DS.
    mov     %ax, %es                # Move to ES.
    mov     $GDT_THREAD, %ax        # Get the user-mode GS.
    mov     %ax, %fs                # Move to FS.
    mov     %ax, %gs                # Move to GS.
    movl    TRAP_EAX(%esp), %eax    # Restore EAX.
    sysexit                         # Return to user mode, slickly.

    ##
    ## Set up and return using an iret since eflags needs to be popped. Again,
    ## the stack doesn't need to be fully popped because the next interrupt or
    ## system call starts from the fully popped place. Again, restore EAX. A
    ## signal handler may need it and it has been used locally.
    ##

ArSysenterHandlerAsmIretReturn:
    pushl   $GDT_THREAD             # Push GS.
    pushl   $GDT_THREAD             # Push FS.
    pushl   $USER_DS                # Push ES.
    pushl   $USER_DS                # Push DS
    pushl   $USER_DS                # Push SS.
    pushl   %ecx                    # Push ESP.
    pushl   %eax                    # Push eflags.
    pushl   $USER_CS                # Push user-mode CS.
    pushl   %edx                    # Push destination EIP.
    movl    TRAP_EAX(%esp), %eax    # Restore EAX.
    iret                            # Return back to user mode, old school.

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
    movl    %cr2, %ecx              # Get the faulting address.
    xor     %edx, %edx              # Zero edx.
    movl    %edx, %cr2              # Clear CR2.
    sti                             # Re-enable interrupts.
    pushl   %ebx                    # Push a pointer to to the trap frame.
    pushl   %ecx                    # Push CR2.
    call    KeDispatchPageFault     # Call the main exception handler.
    addl    $8, %esp                # Pop the parameters.
    call    ArRestoreTrapFrame      # Restore the trap frame
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
    addl    $1, HlSpuriousInterruptCount      # Count interrupts
    call    ArRestoreTrapFrame      # Restore the trap frame
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
## Build the iret return on the value The parameters going on the new stack are
## Ebx, Return Address, Error Code, Eip, CS, and Eflags.
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

KeInterruptEntry:
    call    ArGenerateTrapFrame     # Create a local trap frame.
    pushl   %ebx                    # Push a pointer to it as a parameter.
    call    KeDispatchInterrupt     # Dispatch the interrupt.
    addl    $4, %esp                # Pop the parameters.
    call    ArRestoreTrapFrame      # Restore state.
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

