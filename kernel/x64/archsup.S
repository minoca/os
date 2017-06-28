/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsup.S

Abstract:

    This module implements AMD64 processor architecture features not
    implementable in C.

Author:

    Evan Green 8-Jun-2017

Environment:

    Kernel

--*/

##
## ------------------------------------------------------------------ Includes
##

#include <minoca/kernel/x64.inc>

##
## ---------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## KERNEL_API
## BOOL
## ArAreInterruptsEnabled (
##     VOID
##     )
##

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
    xorl    %eax, %eax              # Clear RAX.
    pushfq                          # Get Rflags.
    popq    %rdi                    # Rflags in rdi.
    andl    $IA32_EFLAG_IF, %edi    # Isolate the Interrupt flag.
    setnz   %al                     # Set eax to 1 if non-zero.
    ret                             #

END_FUNCTION(ArAreInterruptsEnabled)

##
## KERNEL_API
## BOOL
## ArDisableInterrupts (
##     VOID
##     )
##

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
    xorl    %eax, %eax              # Zero eax.
    pushfq                          # Push flags.
    cli                             # Clear the interrupt flag.
    popq    %rdi                    # Pop flags into eax.
    andl    $IA32_EFLAG_IF, %edi    # Isolate the Interrupt flag.
    setnz   %al                     # Set eax to 1 if non-zero.
    ret

END_FUNCTION(ArDisableInterrupts)

##
## KERNEL_API
## VOID
## ArEnableInterrupts (
##     VOID
##     )
##

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

##
## VOID
## ArLoadKernelDataSegments (
##     VOID
##     )
##

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
    LOAD_KERNEL_DATA_SEGMENTS       # Load the kernel data segments.
    ret                             #

END_FUNCTION(ArLoadKernelDataSegments)

##
## ULONG
## ArGetProcessorFlags (
##     VOID
##     )
##

/*++

Routine Description:

    This routine gets the current processor's flags register.

Arguments:

    None.

Return Value:

    Returns the current flags.

--*/

FUNCTION(ArGetProcessorFlags)
    pushfq                          # Push the flags onto the stack.
    popq    %rax                    # Pop them into the return value.
    ret                             #

END_FUNCTION(ArGetProcessorFlags)

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
## ArInvalidateTlbEntry (
##     PVOID Address
##     )
##

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
    invlpg  (%rdi)                  # Invalidate the TLB entry.
    ret                             #

END_FUNCTION(ArInvalidateTlbEntry)

##
## VOID
## ArCleanEntireCache (
##     VOID
##     )
##

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

##
## VOID
## ArInvalidateEntireTlb (
##     VOID
##     )
##

/*++

Routine Description:

    This routine invalidates the entire TLB.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(ArInvalidateEntireTlb)
    movq    %cr3, %rax              # Reloading CR3 causes the entire TLB to
    movq    %rax, %cr3              # be flushed.
    ret

END_FUNCTION(ArInvalidateEntireTlb)

##
## VOID
## ArProcessorYield (
##     VOID
##     )
##

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

##
## KERNEL_API
## VOID
## ArWaitForInterrupt (
##     VOID
##     )
##

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

##
## VOID
## ArSerializeExecution (
##     VOID
##     )
##

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
    movq    %cr2, %rax              # Control register accesses are
    movq    %rax, %cr2              # cheap (ish) and serializing.
    ret

END_FUNCTION(ArSerializeExecution)

##
## VOID
## ArInvalidateInstructionCache (
##     VOID
##     )
##

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
## VOID
## ArFxSave (
##     PFPU_CONTEXT Buffer
##     )
##

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
    addq    $0xF, %rdi      # Round up to nearest alignment requirement.
    andq    $~0xF, %rdi     # Align.
    clts                    # Clear the TS flag, Enabling FPU access.
    fxsave  (%rdi)          # Save the state into there.
    ret

END_FUNCTION(ArFxSave)

##
## VOID
## ArFxRestore (
##     PFPU_CONTEXT Buffer
##     )
##

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
    addq    $0xF, %rdi      # Round up to nearest alignment requirement.
    andq    $~0xF, %rax     # Align.
    clts                    # Clear the TS flag, Enabling FPU access.
    fxrstor (%rdi)          # Load the state from there.
    ret

END_FUNCTION(ArFxRestore)

##
## VOID
## ArEnableFpu (
##     VOID
##     )
##

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

##
## VOID
## ArDisableFpu (
##     VOID
##     )
##

/*++

Routine Description:

    This routine sets the TS bit of CR0, disallowing access to the FPU.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(ArDisableFpu)
    movq    %cr0, %rax          # Get CR0.
    andq    $CR0_TASK_SWITCHED, %rax   # See if it's already disabled.
    jnz      ArDisableFpuReturn # Jump out without writing if it's already off.
    orq     $CR0_TASK_SWITCHED, %rax   # Turn on that bit.
    movq    %rax, %cr0          # Write CR0.

ArDisableFpuReturn:
    ret                     # Return.

END_FUNCTION(ArDisableFpu)

##
## VOID
## ArInitializeFpu (
##     VOID
##     )
##

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
## ULONGLONG
## ArReadMsr (
##     ULONG Msr
##     )
##

/*++

Routine Description:

    This routine reads the requested Model Specific Register.

Arguments:

    Msr - Supplies the MSR to read.

Return Value:

    Returns the 64-bit MSR value.

--*/

FUNCTION(ArReadMsr)
    movq    %rdi, %rcx          # Load the MSR number into ecx.
    xorq    %rax, %rax          # Clear high bits of rax.
    rdmsr                       # Read the MSR into EDX:EAX.
    shlq    $32, %rdx           # Shift rdx into its high word.
    orq     %rdx, %rax          # OR rdx into rax.
    ret                         # Return.

END_FUNCTION(ArReadMsr)

##
## VOID
## ArWriteMsr (
##     ULONG Msr,
##     ULONGLONG Value
##     )
##

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
    movq    %rdi, %rcx          # Load the MSR number into ECX.
    movq    %rsi, %rdx          # Load rdx with the whole 64-bit word.
    shrq    $32, %rdx           # Shift rdx right to put the high bits in edx.
    movl    %esi, %eax          # Put the low bits into eax.
    wrmsr                       # Write the MSR.
    ret                         # Return.

END_FUNCTION(ArWriteMsr)

##
## PVOID
## ArReadFsbase (
##     VOID
##     )
##

/*++

Routine Description:

    This routine reads the fs: base register.

Arguments:

    None.

Return Value:

    Returns the fsbase pointer.

--*/

FUNCTION(ArReadFsbase)
    rdfsbase    %rax            # Read fsbase into rax.
    ret                         # Return.

END_FUNCTION(ArReadFsbase)

##
## VOID
## ArWriteFsbase (
##     PVOID Fsbase
##     )
##

/*++

Routine Description:

    This routine writes the fs: base register.

Arguments:

    Fsbase - Supplies the new fsbase value to write.

Return Value:

    None.

--*/

FUNCTION(ArWriteFsbase)
    wrfsbase    %rdi            # Write fsbase.
    ret                         # Return.

END_FUNCTION(ArWriteFsbase)

##
## PVOID
## ArReadGsbase (
##     VOID
##     )
##

/*++

Routine Description:

    This routine reads the gs: base register.

Arguments:

    None.

Return Value:

    Returns the gsbase pointer.

--*/

FUNCTION(ArReadGsbase)
    rdgsbase    %rax            # Read gsbase into rax.
    ret                         # Return.

END_FUNCTION(ArReadGsbase)

##
## VOID
## ArWriteGsbase (
##     PVOID Gsbase
##     )
##

/*++

Routine Description:

    This routine writes the gs: base register.

Arguments:

    Gsbase - Supplies the new gsbase value to write.

Return Value:

    None.

--*/

FUNCTION(ArWriteGsbase)
    wrgsbase    %rdi            # Write gsbase.
    ret                         # Return.

END_FUNCTION(ArWriteGsbase)

##
## VOID
## ArSwapGs (
##     VOID
##     )
##

/*++

Routine Description:

    This routine exchanges the GS base hidden register with the kernel GS base
    MSR.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(ArSwapGs)
    swapgs                      # Swap gs.
    ret                         # Return.

END_FUNCTION(ArSwapGs)

##
## KERNEL_API
## VOID
## ArMonitor (
##     PVOID Address,
##     UINTN Ecx,
##     UINTN Edx
##     )
##

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
    movq    %rdi, %rax          # Load the address to rax.
    movq    %rsi, %rcx          # Load the first set of hints.
                                # The second set of hints is already in rdx.
    monitor                     # Arm the monitoring hardware.
    ret                         # Return.

END_FUNCTION(ArMonitor)

##
## KERNEL_API
## VOID
## ArMwait (
##     UINTN Eax,
##     UINTN Ecx
##     )
##

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
    movq    %rdi, %rax          # Load eax.
    movq    %rsi, %rcx          # Load ecx.
    mwait                       # Go down.
    ret                         # Return.

END_FUNCTION(ArMwait)

##
## KERNEL_API
## VOID
## ArIoReadAndHalt (
##     USHORT IoPort
##     )
##

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
    movq    %rdi, %rdx              # Move 1st parameter to dx.
    inb     %dx, %al                # Perform the I/O port read.
    sti                             # Enables interrupts one instruction later.
    hlt                             # Simtaneously halt and enable interrupts.
    ret

END_FUNCTION(ArIoReadAndHalt)

##
## UINTN
## ArSaveProcessorContext (
##     PPROCESSOR_CONTEXT Context
##     )
##

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
    int $3                          # TODO: ArSaveProcessorContext for x64.
    jmp     ArSaveProcessorContext  # End of the road.


END_FUNCTION(ArSaveProcessorContext)

##
## VOID
## ArRestoreProcessorContext (
##     PPROCESSOR_CONTEXT Context
##     )
##

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
    int $3                          # TODO: ArRestoreProcessorContext for x64.
    jmp     ArSaveProcessorContext  # End of the road.

END_FUNCTION(ArRestoreProcessorContext)

##
## --------------------------------------------------------- Internal Functions
##

