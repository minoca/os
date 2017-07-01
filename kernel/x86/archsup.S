/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsup.S

Abstract:

    This module implements x86 processor architecture features not
    implementable in C.

Author:

    Evan Green 18-Jul-2012

Environment:

    Kernel mode

--*/

//
// ------------------------------------------------------------------ Includes
//

#include <minoca/kernel/x86.inc>

//
// ---------------------------------------------------------------------- Code
//

//
// .text specifies that this code belongs in the executable section.
//
// .code32 specifies that this is 32-bit protected mode code.
//

.text
.code32

//
// KERNEL_API
// BOOL
// ArAreInterruptsEnabled (
//     VOID
//     )
//

/*++

Routine Description:

    This routine determines whether or not interrupts are currently enabled
    on the processor.

Arguments:

    None.

Return Value:

    TRUE if interrupts are enabled in the processor.

    FALSE if interrupts are globally disabled.

--*/

PROTECTED_FUNCTION(ArAreInterruptsEnabled)
    pushfl                          # Get Eflags.
    popl    %eax                    # Eflags in eax.
    andl    $IA32_EFLAG_IF, %eax    # Isolate the Interrupt flag.
    jz      ArInterruptsNotEnabled  # If the flag is not set, return FALSE.
    movl    $TRUE, %eax             # Return TRUE.
    jmp ArAreInterruptsEnabledEnd   #

ArInterruptsNotEnabled:
    movl    $FALSE, %eax            # Return FALSE.

ArAreInterruptsEnabledEnd:
    ret                             #

END_FUNCTION(ArAreInterruptsEnabled)

//
// KERNEL_API
// BOOL
// ArDisableInterrupts (
//     VOID
//     )
//

/*++

Routine Description:

    This routine disables all interrupts on the current processor.

Arguments:

    None.

Return Value:

    TRUE if interrupts were previously enabled.

    FALSE if interrupts were previously disabled.

--*/

PROTECTED_FUNCTION(ArDisableInterrupts)
    pushfl                          # Push flags.
    cli                             # Clear the interrupt flag.
    popl    %eax                    # Pop flags into eax.
    andl    $IA32_EFLAG_IF, %eax    # Isolate the Interrupt flag.
    jz      ArDisableInterruptsFalse  # If the flag is not set, return FALSE.
    movl    $TRUE, %eax             # Return TRUE.
    ret                             #

ArDisableInterruptsFalse:
    movl    $FALSE, %eax            # Return FALSE.
    ret

END_FUNCTION(ArDisableInterrupts)

//
// KERNEL_API
// VOID
// ArEnableInterrupts (
//     VOID
//     )
//

/*++

Routine Description:

    This routine enables interrupts on the current processor.

Arguments:

    None.

Return Value:

    None.

--*/

PROTECTED_FUNCTION(ArEnableInterrupts)
    sti                             # Set the interrupt flag.
    ret                             #

END_FUNCTION(ArEnableInterrupts)

//
// VOID
// ArLoadKernelDataSegments (
//     VOID
//     )
//

/*++

Routine Description:

    This routine switches the data segments DS and ES to the kernel data
    segment selectors.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(ArLoadKernelDataSegments)
    LoadKernelDataSegments          # Load the kernel data segments.
    movw    $KERNEL_DS, %ax         # Get the data segment selector,
    mov     %ax, %ss                # and save it into the stack register.
    ret                             #

END_FUNCTION(ArLoadKernelDataSegments)

//
// ULONG
// ArGetProcessorFlags (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the current processor's flags register.

Arguments:

    None.

Return Value:

    Returns the current flags.

--*/

FUNCTION(ArGetProcessorFlags)
    pushfl                          # Push the flags onto the stack.
    popl    %eax                    # Pop them into the return value.
    ret                             #

END_FUNCTION(ArGetProcessorFlags)

//
// VOID
// ArLoadTr (
//     USHORT TssSegment
//     )
//

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

//
// VOID
// ArStoreTr (
//     PULONG TssSegment
//     )
//

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

//
// VOID
// ArLoadIdtr (
//     PVOID IdtBase
//     )
//

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

//
// VOID
// ArStoreIdtr (
//     PTABLE_REGISTER IdtRegister
//     )
//

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

//
// VOID
// ArLoadGdtr (
//     TABLE_REGISTER Gdt
//     )
//

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

//
// Load the GDT and then perform a long jump. The long jump is required for
// the new GDT to actually be loaded.
//

    lgdt    4(%esp)                   # Load the GDT.
    ljmp    $KERNEL_CS, $LoadGdtJump  # Long jump.

LoadGdtJump:
    ret                             # Simply return.

END_FUNCTION(ArLoadGdtr)

//
// VOID
// ArStoreGdtr (
//     PTABLE_REGISTER GdtRegister
//     )
//

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

//
// PVOID
// ArGetFaultingAddress (
//     VOID
//     )
//

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

//
// VOID
// ArSetFaultingAddress (
//     PVOID Value
//     )
//

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
    ret                             #

END_FUNCTION(ArSetFaultingAddress)

//
// UINTN
// ArGetCurrentPageDirectory (
//     VOID
//     )
//

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

//
// VOID
// ArSetCurrentPageDirectory (
//     ULONG Value
//     )
//

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
    ret                             #

END_FUNCTION(ArSetCurrentPageDirectory)

//
// VOID
// ArInvalidateTlbEntry (
//     PVOID Address
//     )
//

/*++

Routine Description:

    This routine invalidates one TLB entry corresponding to the given virtual
    address.

Arguments:

    Address - Supplies the virtual address whose associated TLB entry will be
        invalidated.

Return Value:

    None.

--*/

FUNCTION(ArInvalidateTlbEntry)
    mov     4(%esp), %eax
    invlpg  (%eax)                  # Invalidate the TLB entry.
    ret                             #

END_FUNCTION(ArInvalidateTlbEntry)

//
// VOID
// ArCleanEntireCache (
//     VOID
//     )
//

/*++

Routine Description:

    This routine cleans the entire data cache.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(ArCleanEntireCache)
    wbinvd                          # Write back invalidate cache.
    ret
END_FUNCTION(ArCleanEntireCache)

//
// VOID
// ArInvalidateEntireTlb (
//     VOID
//     )
//

/*++

Routine Description:

    This routine invalidates the entire TLB.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(ArInvalidateEntireTlb)
    mov     %cr3, %eax              # Reloading CR3 causes the entire TLB to
    mov     %eax, %cr3              # be flushed.
    ret

END_FUNCTION(ArInvalidateEntireTlb)

//
// VOID
// ArProcessorYield (
//     VOID
//     )
//

/*++

Routine Description:

    This routine executes a short processor yield in hardware.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(ArProcessorYield)
    pause
    ret

END_FUNCTION(ArProcessorYield)

//
// KERNEL_API
// VOID
// ArWaitForInterrupt (
//     VOID
//     )
//

/*++

Routine Description:

    This routine halts the processor until the next interrupt comes in. This
    routine should be called with interrupts disabled, and will return with
    interrupts enabled.

Arguments:

    None.

Return Value:

    None.

--*/

PROTECTED_FUNCTION(ArWaitForInterrupt)
    sti                             # Enables interrupts one instruction later.
    hlt                             # Simtaneously halt and enable interrupts.
    ret

END_FUNCTION(ArWaitForInterrupt)

//
// VOID
// ArSerializeExecution (
//     VOID
//     )
//

/*++

Routine Description:

    This routine acts a serializing instruction, preventing the processor
    from speculatively executing beyond this point.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(ArSerializeExecution)
    mov     %cr2, %eax              # Control register accesses are
    mov     %eax, %cr2              # cheap (ish) and serializing.
    ret

END_FUNCTION(ArSerializeExecution)

//
// VOID
// ArInvalidateInstructionCache (
//     VOID
//     )
//

/*++

Routine Description:

    This routine invalidate the processor's instruction cache, indicating
    that a page containing code has changed.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(ArInvalidateInstructionCache)
    ret

END_FUNCTION(ArInvalidateInstructionCache)

//
// VOID
// ArCpuid (
//     PULONG Eax,
//     PULONG Ebx,
//     PULONG Ecx,
//     PULONG Edx
//     )
//

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

//
// ULONG
// ArGetControlRegister0 (
//     VOID
//     )
//

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

//
// VOID
// ArSetControlRegister0 (
//     ULONG Value
//     )
//

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

//
// ULONG
// ArGetControlRegister4 (
//     VOID
//     )
//

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

//
// VOID
// ArSetControlRegister4 (
//     ULONG Value
//     )
//

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

//
// ULONG
// ArGetDebugRegister0 (
//     VOID
//     )
//

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

//
// VOID
// ArSetDebugRegister0 (
//     ULONG Value
//     )
//

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

//
// ULONG
// ArGetDebugRegister1 (
//     VOID
//     )
//

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

//
// VOID
// ArSetDebugRegister1 (
//     ULONG Value
//     )
//

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

//
// ULONG
// ArGetDebugRegister2 (
//     VOID
//     )
//

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

//
// VOID
// ArSetDebugRegister2 (
//     ULONG Value
//     )
//

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

//
// ULONG
// ArGetDebugRegister3 (
//     VOID
//     )
//

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

//
// VOID
// ArSetDebugRegister3 (
//     ULONG Value
//     )
//

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

//
// ULONG
// ArGetDebugRegister6 (
//     VOID
//     )
//

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

//
// VOID
// ArSetDebugRegister6 (
//     ULONG Value
//     )
//

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

//
// ULONG
// ArGetDebugRegister7 (
//     VOID
//     )
//

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

//
// VOID
// ArSetDebugRegister7 (
//     ULONG Value
//     )
//

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

//
// VOID
// ArFxSave (
//     PFPU_CONTEXT Buffer
//     )
//

/*++

Routine Description:

    This routine saves the current x87 FPU, MMX, XMM, and MXCSR registers to a
    512 byte memory location.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        saved. This buffer must be 16-byte aligned.

Return Value:

    None.

--*/

FUNCTION(ArFxSave)
    movl    4(%esp), %eax   # Get the buffer parameter.
    addl    $0xF, %eax      # Round up to nearest alignment requirement.
    andl    $0xFFFFFFF0, %eax   # Align.
    clts                    # Clear the TS flag, Enabling FPU access.
    fxsave  (%eax)          # Save the state into there.
    ret

END_FUNCTION(ArFxSave)

//
// VOID
// ArFxRestore (
//     PFPU_CONTEXT Buffer
//     )
//

/*++

Routine Description:

    This routine restores the current x87 FPU, MMX, XMM, and MXCSR registers
    from a 512 byte memory location.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        loaded from. This buffer must be 16-byte aligned.

Return Value:

    None.

--*/

FUNCTION(ArFxRestore)
    movl    4(%esp), %eax   # Get the buffer parameter.
    addl    $0xF, %eax      # Round up to nearest alignment requirement.
    andl    $0xFFFFFFF0, %eax   # Align.
    clts                    # Clear the TS flag, Enabling FPU access.
    fxrstor (%eax)          # Load the state from there.
    ret

END_FUNCTION(ArFxRestore)

//
// VOID
// ArSaveX87State (
//     PFPU_CONTEXT Buffer
//     )
//

/*++

Routine Description:

    This routine saves the current x87 FPU (floating point unit) state.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        saved. This buffer must be 16-byte aligned.

Return Value:

    None.

--*/

FUNCTION(ArSaveX87State)
    movl    4(%esp), %eax   # Get the buffer parameter.
    addl    $0xF, %eax      # Round up to nearest alignment requirement.
    andl    $0xFFFFFFF0, %eax   # Align.
    clts                    # Clear the TS flag, Enabling FPU access.
    fsave  (%eax)           # Save the state into there.
    ret

END_FUNCTION(ArSaveX87State)

//
// VOID
// ArRestoreX87State (
//     PFPU_CONTEXT Buffer
//     )
//

/*++

Routine Description:

    This routine restores the x87 FPU (floating point unit) state.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        loaded from. This buffer must be 16-byte aligned.

Return Value:

    None.

--*/

FUNCTION(ArRestoreX87State)
    movl    4(%esp), %eax   # Get the buffer parameter.
    addl    $0xF, %eax      # Round up to nearest alignment requirement.
    andl    $0xFFFFFFF0, %eax   # Align.
    clts                    # Clear the TS flag, Enabling FPU access.
    frstor (%eax)           # Load the state from there.
    ret

END_FUNCTION(ArRestoreX87State)

//
// VOID
// ArEnableFpu (
//     VOID
//     )
//

/*++

Routine Description:

    This routine clears the TS bit of CR0, allowing access to the FPU.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(ArEnableFpu)
    clts                    # Use the dedicated instruction for this.
    ret                     # Return.

END_FUNCTION(ArEnableFpu)

//
// VOID
// ArDisableFpu (
//     VOID
//     )
//

/*++

Routine Description:

    This routine sets the TS bit of CR0, disallowing access to the FPU.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(ArDisableFpu)
    movl    %cr0, %eax          # Get CR0.
    testl   $CR0_TASK_SWITCHED, %eax    # See if it's already disabled.
    jnz      ArDisableFpuReturn # Jump out without writing if it's already off.
    orl     $CR0_TASK_SWITCHED, %eax   # Turn on that bit.
    movl    %eax, %cr0          # Write CR0.

ArDisableFpuReturn:
    ret                     # Return.

END_FUNCTION(ArDisableFpu)

//
// VOID
// ArInitializeFpu (
//     VOID
//     )
//

/*++

Routine Description:

    This routine resets the FPU state.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(ArInitializeFpu)
    fninit                  # Reset the FPU state.
    ret                     # Return.

END_FUNCTION(ArInitializeFpu)

//
// ULONGLONG
// ArReadTimeStampCounter (
//     VOID
//     )
//

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

//
// ULONGLONG
// ArReadMsr (
//     ULONG Msr
//     )
//

/*++

Routine Description:

    This routine reads the requested Model Specific Register.

Arguments:

    Msr - Supplies the MSR to read.

Return Value:

    Returns the 64-bit MSR value.

--*/

FUNCTION(ArReadMsr)
    movl    4(%esp), %ecx       # Load the MSR number into ECX.
    rdmsr                       # Read the MSR into EDX:EAX.
    ret                         # Return.

END_FUNCTION(ArReadMsr)

//
// VOID
// ArWriteMsr (
//     ULONG Msr,
//     ULONGLONG Value
//     )
//

/*++

Routine Description:

    This routine writes the requested Model Specific Register.

Arguments:

    Msr - Supplies the MSR to write.

    Value - Supplies the 64-bit value to write.

Return Value:

    None.

--*/

FUNCTION(ArWriteMsr)
    movl    4(%esp), %ecx       # Load the MSR number into ECX.
    movl    8(%esp), %eax       # Load the low word.
    movl    12(%esp), %edx      # Load the high word.
    wrmsr                       # Write the MSR.
    ret                         # Return.

END_FUNCTION(ArWriteMsr)

//
// VOID
// ArReloadThreadSegment (
//     VOID
//     )
//

/*++

Routine Description:

    This routine reloads the thread segment register.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(ArReloadThreadSegment)
    movl    $GDT_THREAD, %eax   # Load the segment value into a register.
    movw    %ax, %gs            # Reload GS.
    ret                         # Return.

END_FUNCTION(ArReloadThreadSegment)

//
// KERNEL_API
// VOID
// ArMonitor (
//     PVOID Address,
//     UINTN Ecx,
//     UINTN Edx
//     )
//

/*++

Routine Description:

    This routine arms the monitoring hardware in preparation for an mwait
    instruction.

Arguments:

    Address - Supplies the address pointer to monitor.

    Ecx - Supplies the contents to load into the ECX (RCX in 64-bit) register
        when executing the monitor instruction. These are defined as hints.

    Edx - Supplies the contents to load into the EDX/RDX register. These are
        also hints.

Return Value:

    None.

--*/

PROTECTED_FUNCTION(ArMonitor)
    movl    4(%esp), %eax       # Load the address to eax.
    movl    8(%esp), %ecx       # Load first set of hints.
    movl    12(%esp), %edx      # Load second set of hints.
    monitor                     # Arm the monitoring hardware.
    ret                         # Return.

END_FUNCTION(ArMonitor)

//
// KERNEL_API
// VOID
// ArMwait (
//     UINTN Eax,
//     UINTN Ecx
//     )
//

/*++

Routine Description:

    This routine executes the mwait instruction, which is used to halt the
    processor until a specified memory location is written to. It is also used
    on Intel processors to enter C-states. A monitor instruction must have
    been executed prior to this to set up the monitoring region.

Arguments:

    Eax - Supplies the contents to load into EAX/RAX when executing the mwait
        instruction. This is a set of hints, including which C-state to enter
        on Intel processors.

    Ecx - Supplies the contents to load into the ECX (RCX in 64-bit) register
        when executing the mwait instruction. This is 1 when entering a C-state
        with interrupts disabled to indicate that an interrupt should still
        break out.

Return Value:

    None.

--*/

PROTECTED_FUNCTION(ArMwait)
    movl    4(%esp), %eax       # Load eax.
    movl    8(%esp), %ecx       # Load ecx.
    mwait                       # Go down.
    ret                         # Return.

END_FUNCTION(ArMwait)

//
// KERNEL_API
// VOID
// ArIoReadAndHalt (
//     USHORT IoPort
//     )
//

/*++

Routine Description:

    This routine performs a single 8-bit I/O port read and then halts the
    processor until the next interrupt comes in. This routine should be called
    with interrupts disabled, and will return with interrupts enabled.

Arguments:

    IoPort - Supplies the I/O port to read from.

Return Value:

    None.

--*/

PROTECTED_FUNCTION(ArIoReadAndHalt)
    movl    4(%esp), %edx           # Get the I/O port.
    inb     %dx, %al                # Perform the I/O port read.
    sti                             # Enables interrupts one instruction later.
    hlt                             # Simtaneously halt and enable interrupts.
    ret

END_FUNCTION(ArIoReadAndHalt)

//
// UINTN
// ArSaveProcessorContext (
//     PPROCESSOR_CONTEXT Context
//     )
//

/*++

Routine Description:

    This routine saves the current processor context, including the
    non-volatile general registers and the system level control registers. This
    function appears to return twice, once when the context is saved and then
    again when the context is restored. Because the stack pointer is restored,
    the caller of this function may not return without either abandoning the
    context or calling restore. Returning and then calling restore would almost
    certainly result in stack corruption.

Arguments:

    Context - Supplies a pointer to the context area to save into.

Return Value:

    Returns 0 after the context was successfully saved (first time).

    Returns the value in the context return address register when the restore
    function is called (the second time). By default this value is 1, though it
    can be manipulated after the initial save is complete.

--*/

FUNCTION(ArSaveProcessorContext)
    pushl   %edi                    # Save non-volatile (remember this!)
    movl    8(%esp), %edi           # Get the context base address.
    cld                             # Increment edi upon stosd.
    movl    $1, %eax                # Set default eax (second return value).
    stosl                           # Save it.
    movl    4(%esp), %eax           # Get the return address (eip).
    stosl                           # Save it.
    xor     %eax, %eax              # Zero out top part of eax.
    mov     %cs, %ax                # Get CS.
    stosl                           # Save it.
    pushfl                          # Push the flags.
    popl    %eax                    # Pop them into eax.
    stosl                           # Save it.
    movl    %ebx, (%edi)            # Save ebx.
    movl    %esi, 4(%edi)           # Save esi.
    movl    (%esp), %eax            # Get edi (saved in prologue!)
    movl    %eax, 8(%edi)           # Save it.
    movl    %ebp, 12(%edi)          # Save ebp.
    movl    %esp, %eax              # Get esp.
    addl    $8, %eax                # Pop off saved non-volatile and ret addr.
    movl    %eax, 16(%edi)          # Save it.
    addl    $0x14, %edi             # Advance edi for ebx/esi/edi/ebp/esp.
    movl    %dr7, %eax              # Get dr7.
    stosl                           # Save it.
    movl    %dr6, %eax              # Get dr6.
    stosl                           # Save it.
    movl    %dr0, %eax              # Get debug register.
    stosl                           # Save it.
    movl    %dr1, %eax              # Get dr1.
    stosl                           # Save it.
    movl    %dr2, %eax              # Get dr2.
    stosl                           # Save it.
    movl    %dr3, %eax              # Get dr3.
    stosl                           # Save it.
    movl    %edi, (%edi)            # Save VA.
    addl    $4, %edi                # Advance edi.
    movl    %cr0, %eax              # Get CR0.
    stosl                           # Save it.
    movl    %cr2, %eax              # Get CR2.
    stosl                           # Save it.
    movl    %cr3, %eax              # Get CR3.
    stosl                           # Save it.
    movl    %cr4, %eax              # Get CR4.
    stosl                           # Save it.
    xorl    %eax, %eax              # Zero out eax.
    str     %eax                    # Get task register.
    stosl                           # Save it.
    pushl   %eax                    # Push it as an argument.
    call    ArClearTssBusyBit       # Clear the busy bit from the TSS.
    addl    $4, %esp                # Pop the argument.
    sidt    (%edi)                  # Save interrupt descriptor table.
    addl    $6, %edi                # Advance edi.
    sgdt    (%edi)                  # Save global descriptor table.
    addl    $6, %edi                # Advance edi.
    popl    %edi                    # Restore non-volatile.
    xor     %eax, %eax              # Zero out return value.
    ret

END_FUNCTION(ArSaveProcessorContext)

//
// VOID
// ArRestoreProcessorContext (
//     PPROCESSOR_CONTEXT Context
//     )
//

/*++

Routine Description:

    This routine restores the current processor context, including the
    non-volatile general registers and the system level control registers. This
    function does not return, but instead jumps to the return address from
    the caller of the save context function.

Arguments:

    Context - Supplies a pointer to the context to restore.

Return Value:

    Does not return, at least not conventionally.

--*/

FUNCTION(ArRestoreProcessorContext)

    //
    // This function should be called from protected mode. It uses non-volatile
    // registers without saving them because it never returns.
    //

    movl    4(%esp), %ecx           # Get the context parameter.
    movl    %ecx, %esi              # Move to source register.
    addl    $(PROCESSOR_CONTEXT_SIZE - 6), %esi # Jump to last member.
    std                             # Decrement esi during lodsd.
    lgdt    (%esi)                  # Load the new GDT.
    subl    $6, %esi                # Retreat.
    lidt    (%esi)                  # Load the new IDT.
    ljmp    $KERNEL_CS, $ArRestoreProcessorContextJump # Long jump to load GDT.

ArRestoreProcessorContextJump:
    subl    $4, %esi                # Retreat to tr.
    lodsl                           # Pop a value and retreat.
    ltr     %ax                     # Load the task segment.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %cr4              # Restore CR4.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %cr3              # Restore CR3 (potential page switch).
    lodsl                           # Pop a value and retreat.
    movl    %eax, %cr2              # Restore CR2.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %edx              # Move CR0 value over.
    lodsl                           # Pop the VA and retreat.
    movl    %edx, %cr0              # Restore CR0 (maybe enabling paging).
    movl    %eax, %esi              # Restore from the new VA.
    subl    $4, %esi                # Retreat to dr3.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %dr3              # Restore DR3.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %dr2              # Restore DR2.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %dr1              # Restore DR1.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %dr0              # Restore DR0.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %dr6              # Restore DR6.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %dr7              # Restore DR7.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %esp              # Restore esp.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %ebp              # Restore ebp.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %edi              # Restore edi.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %edx              # Restore esi into edx for now.
    lodsl                           # Pop a value and retreat.
    movl    %eax, %ebx              # Restore ebx.
    lodsl                           # Pop a value and retreat.
    pushl   %eax                    # Push eflags.
    lodsl                           # Pop a value and retreat.
    pushl   %eax                    # Push cs.
    lodsl                           # Pop a value and retreat.
    pushl   %eax                    # Push eip.
    lodsl                           # Pop the return value into eax.
    mov     %edx, %esi              # Restore final esi.
    iret                            # Return, loading eip, cs, and eflags.

END_FUNCTION(ArRestoreProcessorContext)

//
// --------------------------------------------------------- Internal Functions
//

