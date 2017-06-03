/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsup.S

Abstract:

    This module implements assembly-based architecture support routines for the
    x86 platform.

Author:

    Evan Green 7-Aug-2013

Environment:

    Boot

--*/

##
## ------------------------------------------------------------------- Includes
##

#include <minoca/kernel/x86.inc>

##
## -------------------------------------------------------------------- Macros
##

##
## This macro switches the DS and ES data segments to the kernel's data
## segments.
##

.macro LoadBootDataSegments
    movw    $KERNEL_DS, %ax         # Get the data segment selector,
    mov     %ax, %ds                # and save it into the data segments.
    mov     %ax, %es                #
.endm

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

##
## VOID
## BoBreakExceptionHandlerAsm (
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

FUNCTION(BoBreakExceptionHandlerAsm)
    pushl   $0                      # Push a dummy error code.
    call    BoGenerateTrapFrame     # Create a local trap frame.
    pushl   %esp                    # Push a pointer to it as a parameter.
    pushl   $0                      # Push a NULL parameter.
    pushl   $EXCEPTION_BREAK        # Push the break parameter.
    call    KdDebugExceptionHandler # Call the main exception handler.
    addl    $0xC, %esp              # Pop the parameters.
    call    BoRestoreTrapFrame      # Restore the trap frame
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(BoBreakExceptionHandlerAsm)

##
## VOID
## BoSingleStepExceptionHandlerAsm (
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

FUNCTION(BoSingleStepExceptionHandlerAsm)
    pushl   $0                      # Push a dummy error code.
    call    BoGenerateTrapFrame     # Create a local trap frame.
    pushl   %esp                    # Push a pointer to it as a parameter.
    pushl   $0                      # Push a NULL parameter.
    pushl   $EXCEPTION_SINGLE_STEP  # Push the exception parameter.
    call    KdDebugExceptionHandler # Call the main exception handler.
    addl    $0xC, %esp              # Pop the parameters.
    call    BoRestoreTrapFrame      # Restore the trap frame
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(BoSingleStepExceptionHandlerAsm)

##
## VOID
## BoDebugServiceHandlerAsm (
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

FUNCTION(BoDebugServiceHandlerAsm)
    pushl   $0                      # Push a dummy error code.
    call    BoGenerateTrapFrame     # Create a local trap frame.
    mov     TRAP_EAX(%esp), %eax    # Get eax, containing the service request.
    mov     TRAP_ECX(%esp), %ecx    # Get ecx, containing the parameter.
    pushl   %esp                    # Push a pointer to the trap frame.
    pushl   %ecx                    # Push exception parameter.
    pushl   %eax                    # Push exception type.
    call    KdDebugExceptionHandler # Call the main exception handler.
    addl    $0xc, %esp              # Pop the parameters.
    call   BoRestoreTrapFrame       # Restore the trap frame
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(BoDebugServiceHandlerAsm)

##
## VOID
## BoDivideByZeroExceptionHandlerAsm (
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

FUNCTION(BoDivideByZeroExceptionHandlerAsm)
    pushl   $0                      # Push a dummy error code.
    call    BoGenerateTrapFrame     # Create a local trap frame.
    pushl   %esp                    # Push a pointer to it as a parameter.
    call    BoDivideByZeroHandler   # Call the main exception handler.
    addl    $0x4, %esp              # Pop the parameters.
    call    BoRestoreTrapFrame      # Restore the trap frame
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(BoDivideByZeroExceptionHandlerAsm)

##
## VOID
## BoProtectionFaultHandlerAsm (
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

FUNCTION(BoProtectionFaultHandlerAsm)
    call    BoGenerateTrapFrame     # Create a local trap frame.
    pushl   %esp                    # Push a pointer to it as a parameter.
    pushl   $0                      # Push dummy parameter.
    pushl   $EXCEPTION_ACCESS_VIOLATION  # Push exception type parameter.
    call    KdDebugExceptionHandler      # Let the debugger handle this.
    addl    $0xC, %esp              # Pop the parameter.
    call    BoRestoreTrapFrame      # Restore the trap frame
    addl    $4, %esp
    iret                            # Return from the exception.

END_FUNCTION(BoProtectionFaultHandlerAsm)

##
## VOID
## BoPageFaultHandlerAsm (
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

FUNCTION(BoPageFaultHandlerAsm)
    call    BoGenerateTrapFrame     # Create a local trap frame.
    movl    %cr2, %eax              # Get the faulting address.
    xor     %edx, %edx              # Zero edx.
    movl    %edx, %cr2              # Clear CR2.
    sti                             # Re-enable interrupts.
    pushl   %esp                    # Push a pointer to it as a parameter.
    pushl   %eax                    #
    call    BoPageFaultHandler      # Call the main exception handler.
    addl    $8, %esp                # Pop the parameters.
    call    BoRestoreTrapFrame      # Restore the trap frame
    addl    $4, %esp                # Pop the error code.
    iret                            # Return from the exception.

END_FUNCTION(BoPageFaultHandlerAsm)

##
## VOID
## BoLoadBootDataSegments (
##     VOID
##     )
##

/*++

Routine Description:

    This routine switches the data segments DS and ES to the boot data
    segment selectors.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(BoLoadBootDataSegments)
    LoadBootDataSegments            # Load the boot data segments.
    movw    $KERNEL_DS, %ax         # Get the data segment selector,
    mov     %ax, %ss                # and save it into the stack register.
    mov     %ax, %fs                # Load it.
    mov     %ax, %gs                # Load it in GS as well.
    ret                             #

END_FUNCTION(BoLoadBootDataSegments)

##
## VOID
## ArLoadTr (
##     USHORT TssSegment
##     )
##

/*++

Routine Description:

    This routine loads a TSS (Task Selector State).

Arguments:

    TssSegment - Supplies the segment selector in the GDT that describes the
        TSS.

Return Value:

    None.

--*/

FUNCTION(ArLoadTr)
    ltr     4(%esp)                 # Load the Task Register.
    ret                             # That's it!

END_FUNCTION(ArLoadTr)

##
## VOID
## ArStoreTr (
##     PULONG TssSegment
##     )
##

/*++

Routine Description:

    This routine retrieves the current TSS (Task Selector State) register.

Arguments:

    TssSegment - Supplies a pointer where the current TSS segment register will
        be returned.

Return Value:

    None.

--*/

FUNCTION(ArStoreTr)
    movl     4(%esp), %eax          # Get the address parameter.
    str     (%eax)                  # Store the TR register into it.
    ret                             # Return

END_FUNCTION(ArStoreTr)

##
## VOID
## ArLoadIdtr (
##     PVOID IdtBase
##     )
##

/*++

Routine Description:

    This routine loads the given Interrupt Descriptor Table.

Arguments:

    IdtBase - Supplies a pointer to the base of the IDT.

Return Value:

    None.

--*/

FUNCTION(ArLoadIdtr)
    movl     4(%esp), %eax          # Get the base parameter.
    lidt     (%eax)                 # Load the IDT register.
    ret                             # That's it!

END_FUNCTION(ArLoadIdtr)

##
## VOID
## ArStoreIdtr (
##     PTABLE_REGISTER IdtRegister
##     )
##

/*++

Routine Description:

    This routine stores the interrupt descriptor table register into the given
    value.

Arguments:

    IdtRegister - Supplies a pointer that will receive the value.

Return Value:

    None.

--*/

FUNCTION(ArStoreIdtr)
    movl     4(%esp), %eax          # Get the address parameter.
    sidt     (%eax)                 # Store the IDT register into it.
    ret                             # Return politely.

END_FUNCTION(ArStoreIdtr)

##
## VOID
## ArLoadGdtr (
##     TABLE_REGISTER Gdt
##     )
##

/*++

Routine Description:

    This routine loads a global descriptor table.

Arguments:

    Gdt - Supplies a pointer to the Gdt pointer, which contains the base and
        limit for the GDT.

Return Value:

    None.

--*/

FUNCTION(ArLoadGdtr)

##
## Load the GDT and then perform a long jump. The long jump is required for
## the new GDT to actually be loaded.
##

    lgdt    4(%esp)                   # Load the GDT.
    ljmp    $KERNEL_CS, $LoadGdtJump  # Long jump.

LoadGdtJump:
    ret                             # Simply return.

END_FUNCTION(ArLoadGdtr)

##
## VOID
## ArStoreGdtr (
##     PTABLE_REGISTER GdtRegister
##     )
##

/*++

Routine Description:

    This routine stores the GDT register into the given value.

Arguments:

    GdtRegister - Supplies a pointer that will receive the value.

Return Value:

    None.

--*/

FUNCTION(ArStoreGdtr)
    movl     4(%esp), %eax          # Get the address parameter.
    sgdt     (%eax)                 # Store the GDT register into it.
    ret                             # Return politely.

END_FUNCTION(ArStoreGdtr)

##
## PVOID
## ArGetFaultingAddress (
##     VOID
##     )
##

/*++

Routine Description:

    This routine determines which address caused a page fault.

Arguments:

    None.

Return Value:

    Returns the faulting address.

--*/

FUNCTION(ArGetFaultingAddress)
    movl    %cr2, %eax              # Return CR2.
    ret                             #

END_FUNCTION(ArGetFaultingAddress)

##
## VOID
## ArSetFaultingAddress (
##     PVOID Value
##     )
##

/*++

Routine Description:

    This routine sets the CR2 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION(ArSetFaultingAddress)
    movl    4(%esp), %eax
    movl    %eax, %cr2
    ret

END_FUNCTION(ArSetFaultingAddress)

##
## ULONG
## ArGetCurrentPageDirectory (
##     VOID
##     )
##

/*++

Routine Description:

    This routine returns the active page directory.

Arguments:

    None.

Return Value:

    Returns the page directory currently in use by the system.

--*/

FUNCTION(ArGetCurrentPageDirectory)
    movl    %cr3, %eax              # Return CR3.
    ret                             #

END_FUNCTION(ArGetCurrentPageDirectory)

##
## VOID
## ArSetCurrentPageDirectory (
##     ULONG Value
##     )
##

/*++

Routine Description:

    This routine sets the CR3 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION(ArSetCurrentPageDirectory)
    movl    4(%esp), %eax
    movl    %eax, %cr3
    ret

END_FUNCTION(ArSetCurrentPageDirectory)

##
## VOID
## ArCpuid (
##     PULONG Eax,
##     PULONG Ebx,
##     PULONG Ecx,
##     PULONG Edx
##     )
##

/*++

Routine Description:

    This routine executes the CPUID instruction to get processor architecture
    information.

Arguments:

    Eax - Supplies a pointer to the value that EAX should be set to when the
        CPUID instruction is executed. On output, contains the contents of
        EAX immediately after the CPUID instruction.

    Ebx - Supplies a pointer to the value that EBX should be set to when the
        CPUID instruction is executed. On output, contains the contents of
        EAX immediately after the CPUID instruction.

    Ecx - Supplies a pointer to the value that ECX should be set to when the
        CPUID instruction is executed. On output, contains the contents of
        EAX immediately after the CPUID instruction.

    Edx - Supplies a pointer to the value that EDX should be set to when the
        CPUID instruction is executed. On output, contains the contents of
        EAX immediately after the CPUID instruction.

Return Value:

    None.

--*/

FUNCTION(ArCpuid)
    push    %ebx                    # Save the non-volatile register.
    push    %esi                    # Save the non-volatile register.
    movl    12(%esp), %esi          # Load Eax parameter.
    movl    (%esi), %eax            # Dereference to load EAX.
    movl    16(%esp), %esi          # Load Ebx parameter.
    movl    (%esi), %ebx            # Dereference to load EBX.
    movl    20(%esp), %esi          # Load Ecx parameter.
    movl    (%esi), %ecx            # Dereference to load ECX.
    movl    24(%esp), %esi          # Load Edx parameter.
    movl    (%esi), %edx            # Dereference to load EDX.
    cpuid                           # Fire off the CPUID instruction.
    movl    24(%esp), %esi          # Load Edx parameter.
    movl    %edx, (%esi)            # Save EDX into there.
    movl    20(%esp), %esi          # Load Ecx parameter.
    movl    %ecx, (%esi)            # Save ECX into there.
    movl    16(%esp), %esi          # Load Ebx parameter.
    movl    %ebx, (%esi)            # Save EBX into there.
    movl    12(%esp), %esi          # Load Eax parameter.
    movl    %eax, (%esi)            # Save EAX into there.
    pop     %esi                    # Resotre the non-volatile register.
    pop     %ebx                    # Restore the non-volatile register.
    ret

END_FUNCTION(ArCpuid)

##
## ULONG
## ArGetControlRegister0 (
##     VOID
##     )
##

/*++

Routine Description:

    This routine returns the current value of CR0.

Arguments:

    None.

Return Value:

    Returns CR0.

--*/

FUNCTION(ArGetControlRegister0)
    movl    %cr0, %eax
    ret

END_FUNCTION(ArGetControlRegister0)

##
## VOID
## ArSetControlRegister0 (
##     ULONG Value
##     )
##

/*++

Routine Description:

    This routine sets the CR0 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION(ArSetControlRegister0)
    movl    4(%esp), %eax
    movl    %eax, %cr0
    ret

END_FUNCTION(ArSetControlRegister0)

##
## ULONG
## ArGetControlRegister4 (
##     VOID
##     )
##

/*++

Routine Description:

    This routine returns the current value of CR4.

Arguments:

    None.

Return Value:

    Returns CR4.

--*/

FUNCTION(ArGetControlRegister4)
    movl    %cr4, %eax
    ret

END_FUNCTION(ArGetControlRegister4)

##
## VOID
## ArSetControlRegister4 (
##     ULONG Value
##     )
##

/*++

Routine Description:

    This routine sets the CR4 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION(ArSetControlRegister4)
    movl    4(%esp), %eax
    movl    %eax, %cr4
    ret

END_FUNCTION(ArSetControlRegister4)

##
## ULONG
## ArGetDebugRegister0 (
##     VOID
##     )
##

/*++

Routine Description:

    This routine returns the current value of DR0.

Arguments:

    None.

Return Value:

    Returns DR0.

--*/

FUNCTION(ArGetDebugRegister0)
    movl    %dr0, %eax
    ret

END_FUNCTION(ArGetDebugRegister0)

##
## VOID
## ArSetDebugRegister0 (
##     ULONG Value
##     )
##

/*++

Routine Description:

    This routine sets the DR0 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION(ArSetDebugRegister0)
    movl    4(%esp), %eax
    movl    %eax, %dr0
    ret

END_FUNCTION(ArSetDebugRegister0)

##
## ULONG
## ArGetDebugRegister1 (
##     VOID
##     )
##

/*++

Routine Description:

    This routine returns the current value of DR1.

Arguments:

    None.

Return Value:

    Returns DR1.

--*/

FUNCTION(ArGetDebugRegister1)
    movl    %dr1, %eax
    ret

END_FUNCTION(ArGetDebugRegister1)

##
## VOID
## ArSetDebugRegister1 (
##     ULONG Value
##     )
##

/*++

Routine Description:

    This routine sets the DR1 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION(ArSetDebugRegister1)
    movl    4(%esp), %eax
    movl    %eax, %dr1
    ret

END_FUNCTION(ArSetDebugRegister1)

##
## ULONG
## ArGetDebugRegister2 (
##     VOID
##     )
##

/*++

Routine Description:

    This routine returns the current value of DR2.

Arguments:

    None.

Return Value:

    Returns DR2.

--*/

FUNCTION(ArGetDebugRegister2)
    movl    %dr2, %eax
    ret

END_FUNCTION(ArGetDebugRegister2)

##
## VOID
## ArSetDebugRegister2 (
##     ULONG Value
##     )
##

/*++

Routine Description:

    This routine sets the DR2 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION(ArSetDebugRegister2)
    movl    4(%esp), %eax
    movl    %eax, %dr2
    ret

END_FUNCTION(ArSetDebugRegister2)

##
## ULONG
## ArGetDebugRegister3 (
##     VOID
##     )
##

/*++

Routine Description:

    This routine returns the current value of DR3.

Arguments:

    None.

Return Value:

    Returns DR3.

--*/

FUNCTION(ArGetDebugRegister3)
    movl    %dr3, %eax
    ret

END_FUNCTION(ArGetDebugRegister3)

##
## VOID
## ArSetDebugRegister3 (
##     ULONG Value
##     )
##

/*++

Routine Description:

    This routine sets the DR3 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION(ArSetDebugRegister3)
    movl    4(%esp), %eax
    movl    %eax, %dr3
    ret

END_FUNCTION(ArSetDebugRegister3)

##
## ULONG
## ArGetDebugRegister6 (
##     VOID
##     )
##

/*++

Routine Description:

    This routine returns the current value of DR6.

Arguments:

    None.

Return Value:

    Returns DR6.

--*/

FUNCTION(ArGetDebugRegister6)
    movl    %dr6, %eax
    ret

END_FUNCTION(ArGetDebugRegister6)

##
## VOID
## ArSetDebugRegister6 (
##     ULONG Value
##     )
##

/*++

Routine Description:

    This routine sets the DR6 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION(ArSetDebugRegister6)
    movl    4(%esp), %eax
    movl    %eax, %dr6
    ret

END_FUNCTION(ArSetDebugRegister6)

##
## ULONG
## ArGetDebugRegister7 (
##     VOID
##     )
##

/*++

Routine Description:

    This routine returns the current value of DR7.

Arguments:

    None.

Return Value:

    Returns DR7.

--*/

FUNCTION(ArGetDebugRegister7)
    movl    %dr7, %eax
    ret

END_FUNCTION(ArGetDebugRegister7)

##
## VOID
## ArSetDebugRegister7 (
##     ULONG Value
##     )
##

/*++

Routine Description:

    This routine sets the DR7 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION(ArSetDebugRegister7)
    movl    4(%esp), %eax
    movl    %eax, %dr7
    ret

END_FUNCTION(ArSetDebugRegister7)

##
## ULONGLONG
## ArReadTimeStampCounter (
##     VOID
##     )
##

/*++

Routine Description:

    This routine reads the time stamp counter from the current processor. It
    is essential that callers of this function understand that this returns
    instruction cycles, which does not always translate directly into units
    of time. For example, some processors halt the timestamp counter during
    performance and CPU idle state transitions. In other cases, the timestamp
    counters of all processors are not in sync, so as execution of a thread
    bounces unpredictably from one core to another, different timelines may be
    observed. Additionally, one must understand that this intrinsic is not a
    serializing instruction to the hardware, so the processor may decide to
    execute any number of instructions after this one before actually snapping
    the timestamp counter. To all those who choose to continue to use this
    primitive to measure time, you have been warned.

Arguments:

    None.

Return Value:

    Returns the current instruction cycle count since the processor was started.

--*/

FUNCTION(ArReadTimeStampCounter)
    rdtsc                       # Store the timestamp counter in EDX:EAX.
    ret                         # And return!

END_FUNCTION(ArReadTimeStampCounter)

##
## --------------------------------------------------------- Internal Functions
##

##
## VOID
## BoRestoreTrapFrame (
##     TRAP_FRAME TrapFrame
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

    TrapFrame - Supplies the trap frame to restore. It is assumed that the
        IOPL and VM fields of the Eflags register have remained unchanged from
        when the exception was generated.

Return Value:

    Upon return, the trap frame will have been popped off the stack, and the
    machine will be in the same state as right after the exception happened.

--*/

BoRestoreTrapFrame:

##
## Restore the common data segment registers. Hold off on DS, as this routine
## will make a couple more DS: accesses. Save it in ESI.
##

    movl    TRAP_DS+4(%esp), %esi           # Save DS into ESI for now.
    movl    TRAP_ES+4(%esp), %eax           # Restore ES.
    movw    %ax, %es                        #
    movl    TRAP_FS+4(%esp), %eax           # Restore FS.
    movw    %ax, %fs                        #
    movl    TRAP_GS+4(%esp), %eax           # Restore GS.
    movw    %ax, %gs                        #

RestoreTrapFrameToKernelMode:

##
## The exception came from kernel mode, so restore the stack segment register.
##

    movl    TRAP_SS+4(%esp), %eax    # Restore SS. If this doesn't allow access
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

    movl    TRAP_ESP+4(%esp), %ebx   # Get the kernel Esp.

RestoreTrapFrameGeneralRegisters:
    subl    $24, %ebx                # Make room for the new parameters.
    movl    TRAP_EIP+4(%esp), %ecx   # Restore Eip.
    movl    %ecx, 12(%ebx)           #
    movl    TRAP_CS+4(%esp), %ecx    # Restore CS.
    movl    %ecx, 16(%ebx)           #

    movl    TRAP_EFLAGS+4(%esp), %ecx  # Restore Eflags.
    movl    %ecx, 20(%ebx)           #
    movl    TRAP_EBX+4(%esp), %ecx   # Save Ebx.
    movl    %ecx, (%ebx)             #
    movl    (%esp), %ecx             # Save this function's return address.
    movl    %ecx, 4(%ebx)            #

##
## Now that all DS: accesses are finished, restore DS.
##

    movw    %si, %ds
##
## Restore the general registers.
##

    movl    TRAP_EAX+4(%esp), %eax   #
    movl    TRAP_ECX+4(%esp), %ecx   #
    movl    TRAP_EDX+4(%esp), %edx   #
    movl    TRAP_ESI+4(%esp), %esi   #
    movl    TRAP_EDI+4(%esp), %edi   #
    movl    TRAP_EBP+4(%esp), %ebp   #

##
## Transition to the new kernel mode stack pointer, pop Ebx, and return.
##

    movl    %ebx, %esp              # Move stacks!
    popl    %ebx                    # Restore Ebx.
    ret

##
## TRAP_FRAME
## BoGenerateTrapFrame (
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

    Upon return, a TRAP_FRAME will be on the top of the stack.

--*/

BoGenerateTrapFrame:

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
    movl    TRAP_RET_CS+4(%esp), %eax         # Save the return CS.
    movl    %eax, TRAP_CS+4(%esp)             #
    movl    TRAP_RET_EFLAGS+4(%esp), %eax     # Save eflags.
    movl    %eax, TRAP_EFLAGS+4(%esp)

##
## The exception came from ring 0, so the only things pushed on the stack
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
    LoadBootDataSegments                    # Load valid data segments.
    ret                                     #

