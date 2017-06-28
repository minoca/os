/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsup.S

Abstract:

    This module implements assembly-based architecture support routines for the
    AMD64 platform.

Author:

    Evan Green 6-Jun-2017

Environment:

    Boot

--*/

##
## ------------------------------------------------------------------- Includes
##

#include <minoca/kernel/x64.inc>

##
## -------------------------------------------------------------------- Macros
##

##
## ---------------------------------------------------------------- Definitions
##

##
## -------------------------------------------------------------------- Globals
##

##
## ----------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## VOID
## BoBreakExceptionHandlerAsm (
##     VOID
##     )
##

/*++

Routine Description:

    This routine is called directly when an debug exception occurs. It sets up
    the parameters and calls a C routine to handle the break. It then restores
    machine state to return from the exception. The arguments to this function
    are pushed by the hardware.

Arguments:

    None. RIP, CS, RFLAGS, RSP, and SS have been pushed onto the stack by the
    processor, and the stack was 16-byte aligned before the pushes.

Return Value:

    None.

--*/

FUNCTION(BoBreakExceptionHandlerAsm)
    pushq   $0                      # Push a dummy error code.
    call    BoGenerateTrapFrame     # Create a local trap frame.
    movq    %rsp, %rdx              # 3rd parameter is trap frame pointer.
    xorq    %rsi, %rsi              # 2nd parameter is NULL.
    movq    $EXCEPTION_BREAK, %rdi  # 1st parameter is the exception.
    call    KdDebugExceptionHandler # Call the main exception handler.
    call    BoRestoreTrapFrame      # Restore the trap frame
    iretq                           # Return from the exception.

END_FUNCTION(BoBreakExceptionHandlerAsm)

##
## VOID
## BoSingleStepExceptionHandlerAsm (
##     VOID
##     )
##

/*++

Routine Description:

    This routine is called directly when an debug exception occurs. It sets up
    the parameters and calls the executive to dispatch the trap.

Arguments:

    None. RIP, CS, RFLAGS, RSP, and SS have been pushed onto the stack by the
    processor, and the stack was 16-byte aligned before the pushes.

Return Value:

    None.

--*/

FUNCTION(BoSingleStepExceptionHandlerAsm)
    pushq   $0                      # Push a dummy error code.
    call    BoGenerateTrapFrame     # Create a local trap frame.
    movq    %rsp, %rdx              # 3rd parameter is trap frame pointer.
    xorq    %rsi, %rsi              # 2nd parameter is NULL.
    movq    $EXCEPTION_SINGLE_STEP, %rdi  # 1st parameter is the exception.
    call    KdDebugExceptionHandler # Call the main exception handler.
    call    BoRestoreTrapFrame      # Restore the trap frame
    iretq                           # Return from the exception.

END_FUNCTION(BoSingleStepExceptionHandlerAsm)

##
## VOID
## BoDebugServiceHandlerAsm (
##     VOID
##     )
##

/*++

Routine Description:

    This routine is entered via an IDT entry to request debug service. It sets
    up the parameters and calls KdDebugExceptionHandler, and then restores
    machine state to return from the exception. The arguments to this function
    are pushed by the hardware. Upon Entry, the first argument is the debug
    service request, and the second argument is the parameter to the request.

Arguments:

    None. RIP, CS, RFLAGS, RSP, and SS have been pushed onto the stack by the
    processor, and the stack was 16-byte aligned before the pushes.

Return Value:

    None.

--*/

FUNCTION(BoDebugServiceHandlerAsm)
    pushq   $0                      # Push a dummy error code.
    call    BoGenerateTrapFrame     # Create a local trap frame.
    movq    %rsp, %rdx              # 3rd parameter is trap frame pointer.
    movq    TRAP_RSI(%rsp), %rsi    # Move 2nd parameter (parameter) into place.
    movq    TRAP_RDI(%rsp), %rdi    # Move 1st parameter (exception) into place.
    call    KdDebugExceptionHandler # Call the main exception handler.
    call    BoRestoreTrapFrame      # Restore the trap frame
    iretq                           # Return from the exception.

END_FUNCTION(BoDebugServiceHandlerAsm)

##
## VOID
## BoDivideByZeroExceptionHandlerAsm (
##     VOID
##     )
##

/*++

Routine Description:

    This routine is called directly when a divide by zero exception occurs.

Arguments:

    None. RIP, CS, RFLAGS, RSP, and SS have been pushed onto the stack by the
    processor, and the stack was 16-byte aligned before the pushes.

Return Value:

    None.

--*/

FUNCTION(BoDivideByZeroExceptionHandlerAsm)
    pushq   $0                      # Push a dummy error code.
    call    BoGenerateTrapFrame     # Create a local trap frame.
    movq    %rsp, %rdi              # 1st parameter is trap frame pointer.
    call    BoDivideByZeroHandler   # Call the main exception handler.
    call    BoRestoreTrapFrame      # Restore the trap frame
    iretq                           # Return from the exception.

END_FUNCTION(BoDivideByZeroExceptionHandlerAsm)

##
## VOID
## BoProtectionFaultHandlerAsm (
##     VOID
##     )
##

/*++

Routine Description:

    This routine is called directly when a general protection fault occurs.
    It's job is to prepare the trap frame, call the appropriate handler, and
    then restore the trap frame.

Arguments:

    None. RIP, CS, RFLAGS, RSP, and SS have been pushed onto the stack by the
    processor, and the stack was 16-byte aligned before the pushes.

Return Value:

    None.

--*/

FUNCTION(BoProtectionFaultHandlerAsm)
    call    BoGenerateTrapFrame     # Create a local trap frame.
    movq    %rsp, %rdx              # 3rd parameter is trap frame pointer.
    xorq    %rsi, %rsi              # 2nd parameter is NULL.
    movq    $EXCEPTION_ACCESS_VIOLATION, %rdi  # 1st parameter is the exception.
    call    KdDebugExceptionHandler # Call the main exception handler.
    call    BoRestoreTrapFrame      # Restore the trap frame
    iretq                           # Return from the exception.

END_FUNCTION(BoProtectionFaultHandlerAsm)

##
## VOID
## BoPageFaultHandlerAsm (
##     VOID
##     )
##

/*++

Routine Description:

    This routine is called directly when a page fault occurs.

Arguments:

    None. RIP, CS, RFLAGS, RSP, and SS have been pushed onto the stack by the
    processor, and the stack was 16-byte aligned before the pushes.

Return Value:

    None.

--*/

FUNCTION(BoPageFaultHandlerAsm)
    call    BoGenerateTrapFrame     # Create a local trap frame.
    movq    %rsp, %rsi              # 2nd parameter is trap frame pointer.
    movq    %cr2, %rdi              # 1st parameter is faulting address.
    xorq    %rax, %rax              # Create a zero register.
    movq    %rax, %cr2              # Clear cr2.
    call    BoPageFaultHandler      # Call the main exception handler.
    call    BoRestoreTrapFrame      # Restore the trap frame
    iretq                           # Return from the exception.

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
    LOAD_KERNEL_DATA_SEGMENTS       # Load up kernel data segments.
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
    ltr      %di                    # Load the Task Register.
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
    str     (%rdi)                  # Store the TR register.
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
    lidt     (%rdi)                 # Load the IDT register.
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
    sidt     (%rdi)                 # Store the IDT register.
    ret                             # Return politely.

END_FUNCTION(ArStoreIdtr)

##
## VOID
## ArLoadGdtr (
##     PTABLE_REGISTER Gdt
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
    lgdt    (%rdi)                    # Load the GDT.

    ##
    ## In order to load the new GDT, a long jump of some kind is needed. Use a
    ## far return for this purpose, returning from this routine in the process.
    ##

    popq    %rax                      # Pop the return address into a register.
    pushq   $KERNEL_CS                # Push the return segemnt.
    pushq   %rax                      # Push the return address.
    retfq                             # Do a 64-bit far return, loading the GDT.

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
    sgdt     (%rdi)                 # Store the GDT register.
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
    movq    %cr2, %rax              # Return CR2.
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
    movq    %rdi, %cr2
    ret

END_FUNCTION(ArSetFaultingAddress)

##
## UINTN
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
    movq    %cr3, %rax              # Return CR3.
    ret                             #

END_FUNCTION(ArGetCurrentPageDirectory)

##
## VOID
## ArSetCurrentPageDirectory (
##     UINTN Value
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
    movq    %rdi, %cr3
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
    pushq   %rbx                    # Save the only non-volatile involved.
    movq    %rdx, %r8               # Save rcx into R8
    movq    %rcx, %r9               # Save rdx into R9.
    movl    (%rdi), %eax            # Dereference to get eax.
    movl    (%rsi), %ebx            # Dereference to get ebx.
    movl    (%r8), %ecx             # Dereference to get ecx.
    movl    (%r9), %edx             # Dereference to get edx.
    cpuid                           # Fire off the CPUID instruction.
    movl    %edx, (%r9)             # Save the resulting edx.
    movl    %ecx, (%r8)             # Save the resulting ecx.
    movl    %ebx, (%rsi)            # Save the resulting ebx.
    movl    %eax, (%rdi)            # Save the resulting eax.
    popq    %rbx                    # Restore the non-volatile.
    ret

END_FUNCTION(ArCpuid)

##
## UINTN
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
    movq    %cr0, %rax
    ret

END_FUNCTION(ArGetControlRegister0)

##
## VOID
## ArSetControlRegister0 (
##     UINTN Value
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
    movq    %rdi, %cr0
    ret

END_FUNCTION(ArSetControlRegister0)

##
## UINTN
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
    movq    %cr4, %rax
    ret

END_FUNCTION(ArGetControlRegister4)

##
## VOID
## ArSetControlRegister4 (
##     UINTN Value
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
    movq    %rdi, %cr4
    ret

END_FUNCTION(ArSetControlRegister4)

##
## UINTN
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
    movq    %dr0, %rax
    ret

END_FUNCTION(ArGetDebugRegister0)

##
## VOID
## ArSetDebugRegister0 (
##     UINTN Value
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
    movq    %rdi, %dr0
    ret

END_FUNCTION(ArSetDebugRegister0)

##
## UINTN
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
    movq    %dr1, %rax
    ret

END_FUNCTION(ArGetDebugRegister1)

##
## VOID
## ArSetDebugRegister1 (
##     UINTN Value
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
    movq    %rdi, %dr1
    ret

END_FUNCTION(ArSetDebugRegister1)

##
## UINTN
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
    movq    %dr2, %rax
    ret

END_FUNCTION(ArGetDebugRegister2)

##
## VOID
## ArSetDebugRegister2 (
##     UINTN Value
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
    movq    %rdi, %dr2
    ret

END_FUNCTION(ArSetDebugRegister2)

##
## UINTN
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
    movq    %dr3, %rax
    ret

END_FUNCTION(ArGetDebugRegister3)

##
## VOID
## ArSetDebugRegister3 (
##     UINTN Value
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
    movq    %rdi, %dr3
    ret

END_FUNCTION(ArSetDebugRegister3)

##
## UINTN
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
    movq    %dr6, %rax
    ret

END_FUNCTION(ArGetDebugRegister6)

##
## VOID
## ArSetDebugRegister6 (
##     UINTN Value
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
    movq    %rdi, %dr6
    ret

END_FUNCTION(ArSetDebugRegister6)

##
## UINTN
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
    movq    %dr7, %rax
    ret

END_FUNCTION(ArGetDebugRegister7)

##
## VOID
## ArSetDebugRegister7 (
##     UINTN Value
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
    movq    %rdi, %dr7
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
    processor and prepares the machine for an iretq back to the code that
    generated this trap frame. It's not really a function because it assumes
    a specific stack layout and modifies data that technically belongs to the
    caller. It should only be called immediately before returning from an
    exception or interrupt. This routine will pop up to and including the error
    code.

Arguments:

    TrapFrame - Supplies the trap frame to restore.

Return Value:

    Upon return, the trap frame will have been popped off the stack, and the
    machine will be in the same state as right after the exception happened.

--*/

BoRestoreTrapFrame:
    popq    %rax                            # Pop return address.
    movq    %rax, TRAP_ERRORCODE(%rsp)      # Save into convenient return slot.
    movl    TRAP_DS(%rsp), %ecx             # Restore ds.
    movw    %cx, %ds                        #
    movl    TRAP_ES(%rsp), %ecx             # Restore es.
    movw    %cx, %es                        #
    movq    TRAP_RAX(%rsp), %rax            # Restore general registers.
    movq    TRAP_RBX(%rsp), %rbx            #
    movq    TRAP_RCX(%rsp), %rcx            #
    movq    TRAP_RDX(%rsp), %rdx            #
    movq    TRAP_RSI(%rsp), %rsi            #
    movq    TRAP_RDI(%rsp), %rdi            #
    movq    TRAP_RBP(%rsp), %rbp            #
    movq    TRAP_R8(%rsp), %r8              #
    movq    TRAP_R9(%rsp), %r9              #
    movq    TRAP_R10(%rsp), %r10            #
    movq    TRAP_R11(%rsp), %r11            #
    movq    TRAP_R12(%rsp), %r12            #
    movq    TRAP_R13(%rsp), %r13            #
    movq    TRAP_R14(%rsp), %r14            #
    movq    TRAP_R15(%rsp), %r15            #
    addq    $TRAP_ERRORCODE, %rsp           # Pop off non-hardware portion.
    ret                                     # Pop error code to return.

##
## TRAP_FRAME
## BoGenerateTrapFrame (
##     ULONGLONG ErrorCode,
##     ULONGLONG ReturnRip,
##     ULONGLONG ReturnCs,
##     ULONGLONG ReturnRflags,
##     ULONGLONG ReturnRsp,
##     ULONGLONG ReturnSs
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

    ErrorCode - Supplies the error code that generated the fault, or a dummy
        error code should be pushed if this was not an exception where the
        hardware would push it.

    ReturnRip - Supplies the instruction that generated the exception.

    ReturnCs - Supplies the code selector of the code that generated the
        exception.

    ReturnRflags - Supplies the flags of the code that generated the
        exception.

    ReturnRsp - Supplies the stack pointer of the code that generated the
        exception.

    ReturnSs - Supplies the stack segment of the code that generated the
        exception.

Return Value:

    Upon return, a TRAP_FRAME will be on the top of the stack.

--*/

BoGenerateTrapFrame:

    ##
    ## Allocate room on the stack for the trap frame, minus the original
    ## return address, minus the fields that have already been pushed by
    ## hardware.
    ##

    subq    $TRAP_R15, %rsp             # Allocate remaining trap frame space.
    movq    %rax, TRAP_RAX(%rsp)        # Save RAX to free it up.
    movq    TRAP_R15(%rsp), %rax        # Get the return address.
    movq    %rbx, TRAP_RBX(%rsp)        # Save the general registers.
    movq    %rcx, TRAP_RCX(%rsp)        #
    movq    %rdx, TRAP_RDX(%rsp)        #
    movq    %rsi, TRAP_RSI(%rsp)        #
    movq    %rdi, TRAP_RDI(%rsp)        #
    movq    %rbp, TRAP_RBP(%rsp)        #
    movq    %r8, TRAP_R8(%rsp)          #
    movq    %r9, TRAP_R9(%rsp)          #
    movq    %r10, TRAP_R10(%rsp)        #
    movq    %r11, TRAP_R11(%rsp)        #
    movq    %r12, TRAP_R12(%rsp)        #
    movq    %r13, TRAP_R13(%rsp)        #
    movq    %r14, TRAP_R14(%rsp)        #
    movq    %r15, TRAP_R15(%rsp)        #
    movq    %ds, %rcx                   #
    movl    %ecx, TRAP_DS(%rsp)         # Save DS.
    movq    %es, %rcx                   #
    movl    %ecx, TRAP_ES(%rsp)         # Save ES.
    movq    %fs, %rcx                   #
    movl    %ecx, TRAP_FS(%rsp)         # Save FS.
    movq    %gs, %rcx                   #
    movl    %ecx, TRAP_GS(%rsp)         # Save GS.
    jmp     *%rax                       # Return

