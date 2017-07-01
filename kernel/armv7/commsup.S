/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    commsup.S

Abstract:

    This module implements ARM processor architecture features not
    implementable in C that are common to all ARM architectures.

Author:

    Chris Stevens 19-Mar-2014

Environment:

    Kernel mode

--*/

//
// ------------------------------------------------------------------ Includes
//

#include <minoca/kernel/arm.inc>

//
// --------------------------------------------------------------- Definitions
//

//
// ---------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER
.fpu vfpv3

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

    TRUE if interrupts were previously enabled on the processor.

    FALSE if interrupts were not previously enabled on the processor.

--*/

PROTECTED_FUNCTION ArDisableInterrupts
    mrs     %r1, CPSR               @ Get the status register.
    cpsid   i                       @ Disable interrupts.
    mov     %r0, #0                 @ Assume interrupts disabled.
    tst     %r1, #PSR_FLAG_IRQ      @ AND the interrupt flag.
    IT(eq)                          @ If the zero flag is set...
    moveq   %r0, #1                 @ Interrupts were enabled.
    bx      %lr                     @ Return.

END_FUNCTION ArDisableInterrupts

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

PROTECTED_FUNCTION ArEnableInterrupts
    cpsie   i                       @ Enable interrupts.
    bx      %lr                     @

END_FUNCTION ArEnableInterrupts

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

PROTECTED_FUNCTION ArAreInterruptsEnabled
    mrs     %r1, CPSR               @ Get the status register.
    mov     %r0, #0                 @ Assume interrupts disabled.
    tst     %r1, #PSR_FLAG_IRQ      @ AND the interrupt flag.
    IT(eq)                          @ If the zero flag is set...
    moveq   %r0, #1                 @ Interrupts were enabled.
    bx      %lr                     @ Return.

END_FUNCTION ArAreInterruptsEnabled

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

FUNCTION ArGetProcessorFlags
    mrs     %r0, CPSR               @ Get the status register.
    bx      %lr                     @

END_FUNCTION ArGetProcessorFlags

//
// ULONG
// ArGetCacheTypeRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine retrives the Cache Type Register (CTR) from the system
    coprocessor.

Arguments:

    None.

Return Value:

    Returns the value of the CTR.

--*/

FUNCTION ArGetCacheTypeRegister
    mrc     p15, 0, %r0, %cr0, %cr0, 1          @ Read the CTR.
    bx      %lr                                 @

END_FUNCTION ArGetCacheTypeRegister

//
// VOID
// ArInvalidateInstructionCache (
//     VOID
//     )
//

/*++

Routine Description:

    This routine invalidate the processor's instruction only cache, indicating
    that a page containing code has changed.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION ArInvalidateInstructionCache
    DSB
    mcr     p15, 0, %r0, c7, c5, 0      @ ICIALLU, Invalidate I-Cache.
    DSB                                 @ Make instructions finish.
    ISB                                 @ Prevent speculative fetching.
    bx      %lr                         @ Return

END_FUNCTION ArInvalidateInstructionCache

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

FUNCTION ArInvalidateTlbEntry
    DSB                                         @ Ensure changes are visible.
    mcr     p15, 0, %r0, %cr8, %cr7, 1          @ Write to TLBIMVA.
    BPIALL                                      @ Write to BPIALL (branch pred).
    DSB                                         @ Data synchronization barrier.
    ISB                                         @ Instruction sync barrier.
    bx      %lr                                 @

END_FUNCTION ArInvalidateTlbEntry

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

FUNCTION ArInvalidateEntireTlb
    DSB                                         @ Ensure changes are visible.
    mcr     p15, 0, %r0, c8, c7, 0              @ TLBIALL (invalidate TLB).
    BPIALL                                      @ Write to BPIALL (branch pred).
    DSB                                         @ Data synchronization barrier.
    ISB                                         @ Instruction sync barrier.
    bx      %lr                                 @

END_FUNCTION ArInvalidateEntireTlb

//
// ULONG
// ArLockTlbEntry (
//     ULONG TlbEntry,
//     PVOID VirtualAddress,
//     ULONG NextTlbEntry
//     )
//

/*++

Routine Description:

    This routine locks a translation in the TLB. This translation will stick
    even across total TLB invalidates.

Arguments:

    TlbEntry - Supplies the base and victim number of the TLB entry to lock.

    VirtualAddress - Supplies the virtual address that should be locked in the
        TLB. The association to physical address will be created by touching
        that address, so the address had better be mapped.

    NextTlbEntry - Supplies the base and victim number to set after locking the
        entry.

Return Value:

    Returns the value of the lockdown register after the TLB miss was forced.
    The lowest bit of this value should be set. If it is not, this indicates
    that TLB lockdown is not supported.

--*/

FUNCTION ArLockTlbEntry
    orr     %r0, %r0, #1                        @ Set lock flag.
    mcr     p15, 0, %r1, %cr8, %cr7, 1          @ Invalidate the TLB entry.
    mcr     p15, 0, %r0, %cr10, %cr0, 0         @ Write lockdown register.
    mcr     p15, 0, %r1, %cr10, %cr1, 0         @ Prefetch data TLB.
    ldr     %r3, [%r1]                          @ Also do standard load.
    mrc     p15, 0, %r0, %cr10, %cr0, 0         @ Read the lockdown register.
    bic     %r2, %r2, #1                        @ Clear lock flag.
    mcr     p15, 0, %r2, %cr10, %cr0, 0         @ Write lockdown register.
    DSB                                         @ Make instructions finish.
    ISB                                         @ Prevent speculative fetching.
    bx      %lr                                 @ Return.

END_FUNCTION ArLockTlbEntry

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

FUNCTION ArSerializeExecution
    DSB
    ISB
    bx      %lr

END_FUNCTION ArSerializeExecution

//
// PVOID
// ArGetDataFaultingAddress (
//     VOID
//     )
//

/*++

Routine Description:

    This routine determines which address caused a data abort.

Arguments:

    None.

Return Value:

    Returns the faulting address.

--*/

FUNCTION ArGetDataFaultingAddress
    mrc     p15, 0, %r0, %cr6, %cr0, 0          @ Get the combined/data FAR.
    bx      %lr                                 @

END_FUNCTION ArGetDataFaultingAddress

//
// VOID
// ArSetDataFaultingAddress (
//     PVOID Value
//     )
//

/*++

Routine Description:

    This routine sets the data faulting address register (DFAR).

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION ArSetDataFaultingAddress
    mcr     p15, 0, %r0, %cr6, %cr0, 0
    bx      %lr

END_FUNCTION ArSetDataFaultingAddress

//
// PVOID
// ArGetInstructionFaultingAddress (
//     VOID
//     )
//

/*++

Routine Description:

    This routine determines which address caused a prefetch abort.

Arguments:

    None.

Return Value:

    Returns the faulting address.

--*/

FUNCTION ArGetInstructionFaultingAddress
    mrc     p15, 0, %r0, %cr6, %cr0, 2          @ Get the IFAR.
    bx      %lr                                 @

END_FUNCTION ArGetInstructionFaultingAddress

//
// VOID
// ArSetInstructionFaultingAddress (
//     PVOID Value
//     )
//

/*++

Routine Description:

    This routine sets the instruction faulting address register (IFAR).

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION ArSetInstructionFaultingAddress
    mcr     p15, 0, %r0, %cr6, %cr0, 2
    bx      %lr

END_FUNCTION ArSetInstructionFaultingAddress

//
// ULONG
// ArGetDataFaultStatus (
//     VOID
//     )
//

/*++

Routine Description:

    This routine determines the reason for the fault by reading the DFSR
    register.

Arguments:

    None.

Return Value:

    Returns the contents of the Data Fault Status Register.

--*/

FUNCTION ArGetDataFaultStatus
    mrc     p15, 0, %r0, %cr5, %cr0, 0          @ Get the DFSR.
    bx      %lr                                 @

END_FUNCTION ArGetDataFaultStatus

//
// VOID
// ArSetDataFaultStatus (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the data fault status register (DFSR).

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION ArSetDataFaultStatus
    mcr     p15, 0, %r0, %cr5, %cr0, 0
    bx      %lr

END_FUNCTION ArSetDataFaultStatus

//
// ULONG
// ArGetInstructionFaultStatus (
//     VOID
//     )
//

/*++

Routine Description:

    This routine determines the reason for the prefetch abort by reading the
    IFAR register.

Arguments:

    None.

Return Value:

    Returns the contents of the Instruction Fault Status Register.

--*/

FUNCTION ArGetInstructionFaultStatus
    mrc     p15, 0, %r0, %cr5, %cr0, 1          @ Get the IFSR.
    bx      %lr                                 @

END_FUNCTION ArGetInstructionFaultStatus

//
// VOID
// ArSetInstructionFaultStatus (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the instruction fault status register (IFSR).

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION ArSetInstructionFaultStatus
    mcr     p15, 0, %r0, %cr5, %cr0, 1
    bx      %lr

END_FUNCTION ArSetInstructionFaultStatus

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

FUNCTION ArProcessorYield
    yield
    bx      %lr

END_FUNCTION ArProcessorYield

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

PROTECTED_FUNCTION ArWaitForInterrupt
    DSB                                         @ Ensure everything is done.
    wfi                                         @ Wait for interrupt.
    cpsie   if                                  @ Enable interrupts.
    bx      %lr

END_FUNCTION ArWaitForInterrupt

//
// VOID
// ArCpuid (
//     PARM_CPUID Features
//     )
//

/*++

Routine Description:

    This routine returns the set of processor features present on the current
    processor.

Arguments:

    Features - Supplies a pointer where the processor feature register values
        will be returned.

Return Value:

    None.

--*/

FUNCTION ArCpuid
    mrc     p15, 0, %r1, c0, c1, 0              @ Get ID_PFR0.
    str     %r1, [%r0], #4                      @ Save it.
    mrc     p15, 0, %r1, c0, c1, 1              @ Get ID_PFR1.
    str     %r1, [%r0], #4                      @ Save it.
    mrc     p15, 0, %r1, c0, c1, 2              @ Get ID_DFR0.
    str     %r1, [%r0], #4                      @ Save it.
    mrc     p15, 0, %r1, c0, c1, 3              @ Get ID_AFR0.
    str     %r1, [%r0], #4                      @ Save it.
    mrc     p15, 0, %r1, c0, c1, 4              @ Get ID_MMFR0.
    str     %r1, [%r0], #4                      @ Save it.
    mrc     p15, 0, %r1, c0, c1, 5              @ Get ID_MMFR1.
    str     %r1, [%r0], #4                      @ Save it.
    mrc     p15, 0, %r1, c0, c1, 6              @ Get ID_MMFR2.
    str     %r1, [%r0], #4                      @ Save it.
    mrc     p15, 0, %r1, c0, c1, 7              @ Get ID_MMFR3.
    str     %r1, [%r0], #4                      @ Save it.
    mrc     p15, 0, %r1, c0, c2, 0              @ Get ID_IDAR0.
    str     %r1, [%r0], #4                      @ Save it.
    mrc     p15, 0, %r1, c0, c2, 1              @ Get ID_IDAR1.
    str     %r1, [%r0], #4                      @ Save it.
    mrc     p15, 0, %r1, c0, c2, 2              @ Get ID_IDAR2.
    str     %r1, [%r0], #4                      @ Save it.
    mrc     p15, 0, %r1, c0, c2, 3              @ Get ID_IDAR3.
    str     %r1, [%r0], #4                      @ Save it.
    mrc     p15, 0, %r1, c0, c2, 4              @ Get ID_IDAR4.
    str     %r1, [%r0], #4                      @ Save it.
    mrc     p15, 0, %r1, c0, c2, 5              @ Get ID_IDAR5.
    str     %r1, [%r0], #4                      @ Save it.
    bx      %lr                                 @ Return!

END_FUNCTION ArCpuid

//
// ULONG
// ArGetSystemControlRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine returns the MMU system control register (SCTLR).

Arguments:

    None.

Return Value:

    Returns the current SCTLR value.

--*/

FUNCTION ArGetSystemControlRegister
    mrc     p15, 0, %r0, %cr1, %cr0, 0          @ Get the SCTLR.
    bx      %lr                                 @ Return.

END_FUNCTION ArGetSystemControlRegister

//
// VOID
// ArSetSystemControlRegister (
//     ULONG NewValue
//     )
//

/*++

Routine Description:

    This routine sets the MMU system control register (SCTLR).

Arguments:

    NewValue - Supplies the value to set as the new MMU SCTLR.

Return Value:

    None.

--*/

FUNCTION ArSetSystemControlRegister
    mcr     p15, 0, %r0, %cr1, %cr0, 0          @ Set the SCTLR.
    bx      %lr                                 @ Return.

END_FUNCTION ArSetSystemControlRegister

//
// ULONG
// ArGetAuxiliaryControlRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine returns the auxiliary system control register (ACTLR).

Arguments:

    None.

Return Value:

    Returns the current value.

--*/

FUNCTION ArGetAuxiliaryControlRegister
    mrc     p15, 0, %r0, %cr1, %cr0, 1
    bx      %lr

END_FUNCTION ArGetAuxiliaryControlRegister

//
// VOID
// ArSetAuxiliaryControlRegister (
//     ULONG NewValue
//     )
//

/*++

Routine Description:

    This routine sets the auxiliary system control register (ACTLR).

Arguments:

    NewValue - Supplies the value to set.

Return Value:

    None.

--*/

FUNCTION ArSetAuxiliaryControlRegister
    mcr     p15, 0, %r0, %cr1, %cr0, 1
    bx      %lr

END_FUNCTION ArSetAuxiliaryControlRegister

//
// PVOID
// ArGetVectorBaseAddress (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the vector base address register (VBAR) which determines
    where the ARM exception vector table starts.

Arguments:

    None.

Return Value:

    Returns the current VBAR.

--*/

FUNCTION ArGetVectorBaseAddress
    mrc     p15, 0, %r0, c12, c0, 0
    bx      %lr

END_FUNCTION ArGetVectorBaseAddress

//
// VOID
// ArSetVectorBaseAddress (
//     PVOID VectorBaseAddress
//     )
//

/*++

Routine Description:

    This routine sets the vector base address register (VBAR) which determines
    where the ARM exception vector table starts.

Arguments:

    VectorBaseAddress - Supplies a pointer to the ARM exception vector base
        address. This value must be 32-byte aligned.

Return Value:

    None.

--*/

FUNCTION ArSetVectorBaseAddress
    mcr     p15, 0, %r0, c12, c0, 0             @ Set VBAR.
    bx      %lr                                 @ Return.

END_FUNCTION ArSetVectorBaseAddress

//
// PVOID
// ArGetProcessorBlockRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the register used to store a pointer to the processor
    block (TPIDRPRW in the ARMARM; Thread and Process ID Registers in the
    ARM1176 TRM).

Arguments:

    None.

Return Value:

    Returns a pointer to the processor block.

--*/

FUNCTION ArGetProcessorBlockRegister
    mrc     p15, 0, %r0, c13, c0, 4             @ Get TPIDRPRW.
    bx      %lr                                 @ Return.

END_FUNCTION ArGetProcessorBlockRegister

//
// PVOID
// ArGetProcessorBlockRegisterForDebugger (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the register used to store a pointer to the processor
    block (TPIDRPRW in the ARMARM; Thread and Process ID Registers in the
    ARM1176 TRM). This routine is called from inside the debugger.

Arguments:

    None.

Return Value:

    Returns a pointer to the processor block.

--*/

FUNCTION ArGetProcessorBlockRegisterForDebugger
    mrc     p15, 0, %r0, c13, c0, 4             @ Get TPIDRPRW.
    bx      %lr                                 @ Return.

END_FUNCTION ArGetProcessorBlockRegisterForDebugger

//
// VOID
// ArSetProcessorBlockRegister (
//     PVOID ProcessorBlockRegisterValue
//     )
//

/*++

Routine Description:

    This routine sets the register used to store a pointer to the processor
    block (TPIDRPRW in the ARMARM; Thread and Process ID Registers in the
    ARM1176 TRM).

Arguments:

    ProcessorBlockRegisterValue - Supplies the value to assign to the register
        used to store the processor block.

Return Value:

    None.

--*/

FUNCTION ArSetProcessorBlockRegister
    mcr     p15, 0, %r0, c13, c0, 4             @ Set TPIDRPRW.
    bx      %lr                                 @ Return.

END_FUNCTION ArSetProcessorBlockRegister

//
// UINTN
// ArDereferenceProcessorBlock (
//     UINTN Offset
//     )
//

/*++

Routine Description:

    This routine performs a native integer read of the processor block plus
    a given offset. The C equivalent of this would be
    *((PUINTN)(ProcessorBlock + Offset)).

Arguments:

    Offset - Supplies the offset into the processor block to read.

Return Value:

    Returns the native integer read at the given address.

--*/

FUNCTION ArDereferenceProcessorBlock
    mrs     %r1, CPSR               @ Get the original status register.
    cpsid   i                       @ Disable interrupts.
    mrc     p15, 0, %r2, c13, c0, 4 @ Get TPIDRPRW.
    ldr     %r0, [%r2, %r0]         @ Read at offset.
    msr     CPSR, %r1               @ Restore interrupts.
    bx      %lr                     @ Return.

END_FUNCTION ArDereferenceProcessorBlock

//
// ULONG
// ArGetTranslationTableBaseRegister0 (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the translation table base register 0 (TTBR0), used as
    the base for all virtual to physical memory lookups.

Arguments:

    None.

Return Value:

    Returns the contents of TTBR0.

--*/

FUNCTION ArGetTranslationTableBaseRegister0
    mrc     p15, 0, %r0, c2, c0, 0              @ Get TTBR0.
    bx      %lr                                 @ Return.

END_FUNCTION ArGetTranslationTableBaseRegister0

//
// VOID
// ArSetTranslationTableBaseRegister0 (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the translation table base register 0 (TTBR0).

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION ArSetTranslationTableBaseRegister0
    mcr     p15, 0, %r0, c2, c0, 0
    bx      %lr

END_FUNCTION ArSetTranslationTableBaseRegister0

//
// ULONG
// ArGetTranslationTableBaseRegister1 (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the translation table base register 1 (TTBR1).

Arguments:

    None.

Return Value:

    Returns the contents of TTBR1.

--*/

FUNCTION ArGetTranslationTableBaseRegister1
    mrc     p15, 0, %r0, c2, c0, 1
    bx      %lr

END_FUNCTION ArGetTranslationTableBaseRegister1

//
// VOID
// ArSetTranslationTableBaseRegister1 (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the translation table base register 1 (TTBR1).

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION ArSetTranslationTableBaseRegister1
    mcr     p15, 0, %r0, c2, c0, 1
    bx      %lr

END_FUNCTION ArSetTranslationTableBaseRegister1

//
// ULONG
// ArGetPrimaryRegionRemapRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the Primary Region Remap Register (PRRR).

Arguments:

    None.

Return Value:

    Returns the contents of the register.

--*/

FUNCTION ArGetPrimaryRegionRemapRegister
    mrc     p15, 0, %r0, c10, c2, 0
    bx      %lr

END_FUNCTION ArGetPrimaryRegionRemapRegister

//
// VOID
// ArSetPrimaryRegionRemapRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the PRRR.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION ArSetPrimaryRegionRemapRegister
    mcr     p15, 0, %r0, c10, c2, 0
    bx      %lr

END_FUNCTION ArSetPrimaryRegionRemapRegister

//
// ULONG
// ArGetNormalMemoryRemapRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the Normal Memory Remap Register (NMRR).

Arguments:

    None.

Return Value:

    Returns the contents of the register.

--*/

FUNCTION ArGetNormalMemoryRemapRegister
    mrc     p15, 0, %r0, c10, c2, 1
    bx      %lr

END_FUNCTION ArGetNormalMemoryRemapRegister

//
// VOID
// ArSetNormalMemoryRemapRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the NMRR.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION ArSetNormalMemoryRemapRegister
    mcr     p15, 0, %r0, c10, c2, 1
    bx      %lr

END_FUNCTION ArSetNormalMemoryRemapRegister

//
// ULONG
// ArGetPhysicalAddressRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the Physical Address Register (PAR).

Arguments:

    None.

Return Value:

    Returns the contents of the register.

--*/

FUNCTION ArGetPhysicalAddressRegister
    mrc     p15, 0, %r0, c7, c4, 0
    bx      %lr

END_FUNCTION ArGetPhysicalAddressRegister

//
// VOID
// ArSetPhysicalAddressRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the Physical Address Register (PAR).

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION ArSetPhysicalAddressRegister
    mcr     p15, 0, %r0, c7, c4, 0
    bx      %lr

END_FUNCTION ArSetPhysicalAddressRegister

//
// VOID
// ArSetPrivilegedReadTranslateRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the Privileged Read address translation command register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION ArSetPrivilegedReadTranslateRegister
    mcr     p15, 0, %r0, c7, c8, 0
    bx      %lr

END_FUNCTION ArSetPrivilegedReadTranslateRegister

//
// VOID
// ArSetPrivilegedWriteTranslateRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the Privileged Write address translation command register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION ArSetPrivilegedWriteTranslateRegister
    mcr     p15, 0, %r0, c7, c8, 1
    bx      %lr

END_FUNCTION ArSetPrivilegedWriteTranslateRegister

//
// VOID
// ArSetUnprivilegedReadTranslateRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the Unrivileged Read address translation command register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION ArSetUnprivilegedReadTranslateRegister
    mcr     p15, 0, %r0, c7, c8, 2
    bx      %lr

END_FUNCTION ArSetUnprivilegedReadTranslateRegister

//
// VOID
// ArSetUnprivilegedWriteTranslateRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the Unprivileged Write address translation command
    register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION ArSetUnprivilegedWriteTranslateRegister
    mcr     p15, 0, %r0, c7, c8, 3
    bx      %lr

END_FUNCTION ArSetUnprivilegedWriteTranslateRegister

//
// ULONG
// ArTranslateVirtualToPhysical (
//     PVOID VirtualAddress
//     )
//

/*++

Routine Description:

    This routine translates a virtual address to its corresponding physical
    address by using the current translation tables.

Arguments:

    VirtualAddress - Supplies the virtual address to translate.

Return Value:

    Returns the physical address that the virtual address corresponds to
    (with some bits at the bottom relating to the cache type).

--*/

FUNCTION ArTranslateVirtualToPhysical
    mrs     %r1, CPSR               @ Get the status register.
    orr     %r2, %r1, #PSR_FLAG_IRQ @ Turn on the interrupt mask bit.
    msr     CPSR_cxsf, %r2          @ Write the status register.
    mcr     p15, 0, %r0, c7, c8, 0  @ Write VA into V2PCWPR
    mrc     p15, 0, %r0, c7, c4, 0  @ Read PAR.
    msr     CPSR_cxsf, %r1          @ Restore interrupts.
    bx      %lr                     @ Return.

END_FUNCTION ArTranslateVirtualToPhysical

//
// VOID
// ArSetThreadPointerUserReadOnly (
//     PVOID NewPointer
//     )
//

/*++

Routine Description:

    This routine sets the TPIDRURO user-mode-read-only thread pointer register.

Arguments:

    NewPointer - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION ArSetThreadPointerUserReadOnly
    mcr     p15, 0, %r0, c13, c0, 3     @ Set the TPIDRURO register.
    bx      %lr

END_FUNCTION ArSetThreadPointerUserReadOnly

//
// ULONG
// ArGetThreadPointerUser (
//     VOID
//     )
//

/*++

Routine Description:

    This routine sets the TPIDRURW user-mode thread pointer register.

Arguments:

    None.

Return Value:

    Returns the current value of the TPIDRURW.

--*/

FUNCTION ArGetThreadPointerUser
    mrc     p15, 0, %r0, c13, c0, 2     @ Get the TPIDRURW register.
    bx      %lr

END_FUNCTION ArGetThreadPointerUser

//
// VOID
// ArSwitchTtbr0 (
//     ULONG NewValue
//     )
//

/*++

Routine Description:

    This routine performs the proper sequence for changing contexts in TTBR0,
    including the necessary invalidates and barriers.

Arguments:

    NewValue - Supplies the new value to write.

Return Value:

    None.

--*/

FUNCTION ArSwitchTtbr0
    mcr     p15, 0, %r0, c2, c0, 0          @ Set TTBR0.
    mov     %r0, #0                         @ Get a zero register.
    DSB                                     @ Ensure everything finished.
    mcr     p15, 0, %r0, c8, c7, 0          @ TLBIALL, Invalidate entire TLB.
    mcr     p15, 0, %r0, c7, c5, 0          @ ICIALLU, Invalidate I-Cache + BTB.
    DSB                                     @ Ensure those operations finished.
    ISB                                     @ Instruction synchronization.
    bx      %lr                             @ Return.

END_FUNCTION ArSwitchTtbr0

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

FUNCTION ArSaveProcessorContext
    stmia   %r0!, {%lr}                 @ Save PC (return address).
    mov     %r1, #1                     @ "Save" 1 to R0.
    stmia   %r0!, {%r1}                 @
    mov     %r1, #0                     @ "Save" 0 to R1, R2, and R3.
    stmia   %r0!, {%r1}                 @
    stmia   %r0!, {%r1}                 @
    stmia   %r0!, {%r1}                 @
    mrs     %r1, CPSR                   @ Save CPSR.
    stmia   %r0!, {%r1}                 @
    mov     %r3, %sp                    @ Put SP in R3 because no pushing SP.
    stmia   %r0!, {%r3-%r11}            @ Save SP and R4-R11.
    mrs     %r1, CPSR                   @ Get the current mode and status.
    cpsid   if, #ARM_MODE_SYSTEM        @ Get to user mode (disable interrupts).
    mov     %r2, %sp                    @ Get banked SP.
    stmia   %r0!, {%r2, %lr}            @ Save banked registers.
    cpsid   if, #ARM_MODE_IRQ           @ Get to interrupt mode.
    mov     %r2, %sp                    @ Get banked SP.
    stmia   %r0!, {%r2, %lr}            @ Save banked registers.
    cpsid   if, #ARM_MODE_FIQ           @ Get to fast interrupt mode.
    mov     %r2, %sp                    @ Get banked SP.
    stmia   %r0!, {%r2, %lr}            @ Save banked registers.
    cpsid   if, #ARM_MODE_ABORT         @ Get to abort mode.
    mov     %r2, %sp                    @ Get banked SP.
    stmia   %r0!, {%r2, %lr}            @ Save banked registers.
    cpsid   if, #ARM_MODE_UNDEF         @ Get to undefined instruction mode.
    mov     %r2, %sp                    @ Get banked SP.
    stmia   %r0!, {%r2, %lr}            @ Save banked registers.
    msr     CPSR, %r1                   @ Restore interrupts and mode.
    stmia   %r0!, {%r0}                 @ Remember the VA right here.
    mrc     p15, 0, %r1, c1, c0, 0      @ Save SCTLR.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c2, c0, 0      @ Save TTBR0.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c2, c0, 1      @ Save TTBR1.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c1, c0, 1      @ Save ACTLR.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c1, c0, 2      @ Save CPACR.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c10, c2, 0     @ Save PRRR.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c10, c2, 1     @ Save NMRR.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c13, c0, 1     @ Save CONTEXTIDR.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c5, c0, 0      @ Save DFSR.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c6, c0, 0      @ Save DFAR
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c5, c0, 1      @ Save IFSR.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c6, c0, 2      @ Save IFAR.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c3, c0, 0      @ Save DACR
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c12, c0, 0     @ Save VBAR.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c13, c0, 4     @ Save TPIDRPRW.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c13, c0, 3     @ Save TPIDRURO.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c13, c0, 2     @ Save TPIDRURW.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c9, c12, 0     @ Save PMCR.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c9, c14, 1     @ Save PMINTENSET.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c9, c14, 0     @ Save PMUSERENR.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c9, c12, 1     @ Save PMCNTENSET.
    stmia   %r0!, {%r1}                 @
    mrc     p15, 0, %r1, c9, c13, 0     @ Save PMCCNTR. Start losing time.
    stmia   %r0!, {%r1}                 @
    mov     %r0, #0                     @ Return 0 initially.
    bx      %lr

END_FUNCTION ArSaveProcessorContext

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

FUNCTION ArRestoreProcessorContext
    add     %r0, %r0, #PROCESSOR_CONTEXT_SIZE   @ Jump to the end.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c9, c13, 0     @ Restore PMCCNTR. Start losing time.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c9, c12, 1     @ Restore PMCNTENSET.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c9, c14, 0     @ Restore PMUSERENR.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c9, c14, 1     @ Restore PMINTENSET.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c9, c12, 0     @ Restore PMCR.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c13, c0, 2     @ Restore TPIDRURW.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c13, c0, 3     @ Restore TPIDRURO.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c13, c0, 4     @ Restore TPIDRPRW.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c12, c0, 0     @ Restore VBAR.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c3, c0, 0      @ Restore DACR
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c6, c0, 2      @ Restore IFAR.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c5, c0, 1      @ Restore IFSR.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c6, c0, 0      @ Restore DFAR
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c5, c0, 0      @ Restore DFSR.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c13, c0, 1     @ Restore CONTEXTIDR.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c10, c2, 1     @ Restore NMRR.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c10, c2, 0     @ Restore PRRR.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c1, c0, 2      @ Restore CPACR.

    //
    // Sometimes the ACTLR cannot be written in non-secure mode, so only try to
    // write it if it appears to need restoring.
    //

    mrc     p15, 0, %r2, c1, c0, 1      @ Get current ACTLR.
    ldmdb   %r0!, {%r1}                 @ Load saved one.
    cmp     %r1, %r2                    @ Compare.
    beq     ArRestoreProcessorContextAfterActlr
    mcr     p15, 0, %r1, c1, c0, 1      @ Restore ACTLR.

ArRestoreProcessorContextAfterActlr:
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c2, c0, 1      @ Restore TTBR1.
    ldmdb   %r0!, {%r1}                 @
    mcr     p15, 0, %r1, c2, c0, 0      @ Restore TTBR0.

    //
    // This next bit is tricky, because paging may suddenly become enabled.
    // This code must be identity mapped, but the context might not be. Load
    // the SCTLR and the virtual address of the next structure member. Then
    // restore SCTLR and continue the restore from the virtual address.
    //

    ldmdb   %r0!, {%r1}                 @ Get SCTLR.
    ldmdb   %r0, {%r2}                  @ Also get the VA of the remainder.
    mov     %r0, %r2                    @ Make it the new current pointer.
    mcr     p15, 0, %r1, c1, c0, 0      @ Restore SCTLR.

    //
    // The paging context may have changed (and observed a wonky ASID in the
    // process), so some invalidations are in order.
    //

    mcr     p15, 0, %r0, c7, c5, 0      @ ICIALLU, Invalidate I-Cache.
    BPIALL                              @ BPIALL, Invalidate Branch Predictor.
    mcr     p15, 0, %r0, c8, c7, 0      @ Clear the TLB, TLBIALL.
    DSB                                 @ Data synchronization barrier.
    ISB                                 @ Instruction syncrhonization barrier.

    //
    // Restore the banked registers.
    //

    cpsid   if, #ARM_MODE_UNDEF         @ Get to undefined instruction mode.
    ldmdb   %r0!, {%r1, %lr}            @ Restore banked registers.
    mov     %sp, %r1                    @ Restore banked SP.
    cpsid   if, #ARM_MODE_ABORT         @ Get to abort mode.
    ldmdb   %r0!, {%r1, %lr}            @ Restore banked registers.
    mov     %sp, %r1                    @ Restore banked SP.
    cpsid   if, #ARM_MODE_FIQ           @ Get to fast interrupt mode.
    ldmdb   %r0!, {%r1, %lr}            @ Restore banked registers.
    mov     %sp, %r1                    @ Restore banked SP.
    cpsid   if, #ARM_MODE_IRQ           @ Get to interrupt mode.
    ldmdb   %r0!, {%r1, %lr}            @ Restore banked registers.
    mov     %sp, %r1                    @ Restore banked SP.
    cpsid   if, #ARM_MODE_SYSTEM        @ Get to user mode (disable interrupts).
    ldmdb   %r0!, {%r1, %lr}            @ Restore banked registers.
    mov     %sp, %r1                    @ Restore banked SP.
    cpsid   if, #ARM_MODE_SVC           @ Get back to SVC mode.

    //
    // Restore the general registers and jump back to the return of the
    // save context function.
    //

    mov     %r12, %r0                   @ Move to a free register.
    ldmdb   %r12!, {%r3-%r11}           @ Restore general context.
    mov     %sp, %r3                    @ Restore stack.
    ldmdb   %r12!, {%r0-%r3,%lr}        @ Restore arguments and CPSR.
    msr     CPSR, %lr                   @ Restore CPSR.
    ldr     %r12, [%r12, #-4]           @ Read the last value: PC.
    mov     %lr, #0                     @ Zero out lr for goodwill.
    bx      %r12                        @ Jump to the return address.

END_FUNCTION ArRestoreProcessorContext

//
// ULONG
// ArGetMainIdRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the Main ID Register (MIDR).

Arguments:

    None.

Return Value:

    Returns the contents of the register.

--*/

FUNCTION ArGetMainIdRegister
    mrc     p15, 0, %r0, c0, c0, 0
    bx      %lr

END_FUNCTION ArGetMainIdRegister

//
// ULONG
// ArGetCoprocessorAccessRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the Coprocessor Access Control Register (CPACR).

Arguments:

    None.

Return Value:

    Returns the contents of the register.

--*/

FUNCTION ArGetCoprocessorAccessRegister
    mrc     p15, 0, %r0, c1, c0, 2
    bx      %lr

END_FUNCTION ArGetCoprocessorAccessRegister

//
// VOID
// ArSetCoprocessorAccessRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the Coprocessor Access Control Register (CPACR).

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION ArSetCoprocessorAccessRegister
    mcr     p15, 0, %r0, c1, c0, 2
    bx      %lr

END_FUNCTION ArSetCoprocessorAccessRegister

//
// ULONG
// ArGetFloatingPointIdRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the Floating Point unit ID register (FPSID).

Arguments:

    None.

Return Value:

    Returns the contents of the register.

--*/

FUNCTION ArGetFloatingPointIdRegister
    vmrs    %r0, FPSID
    bx      %lr

END_FUNCTION ArGetFloatingPointIdRegister

//
// ULONG
// ArGetMvfr0Register (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the floating point extensions identification register
    (MVFR0).

Arguments:

    None.

Return Value:

    Returns the contents of the register.

--*/

FUNCTION ArGetMvfr0Register
    vmrs    %r0, MVFR0
    bx      %lr

END_FUNCTION ArGetMvfr0Register

//
// ULONG
// ArGetVfpExceptionRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the floating point exception control register (FPEXC).

Arguments:

    None.

Return Value:

    Returns the contents of the register.

--*/

FUNCTION ArGetVfpExceptionRegister
    vmrs    %r0, FPEXC
    bx      %lr

END_FUNCTION ArGetVfpExceptionRegister

//
// VOID
// ArSetVfpExceptionRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the floating point exception control register (FPEXC).

Arguments:

    Value - Supplies the new value to set.

Return Value:

    None.

--*/

FUNCTION ArSetVfpExceptionRegister
    vmsr    FPEXC, %r0
    bx      %lr

END_FUNCTION ArSetVfpExceptionRegister

//
// ULONG
// ArGetVfpInstructionRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the floating point instruction register (FPINST).

Arguments:

    None.

Return Value:

    Returns the contents of the register.

--*/

FUNCTION ArGetVfpInstructionRegister
    mrc     p10, 7, %r0, c9, c0, 0
    bx      %lr

END_FUNCTION ArGetVfpInstructionRegister

//
// ULONG
// ArGetFpscr (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the floating point status and control register (FPSCR).

Arguments:

    None.

Return Value:

    Returns the contents of the register.

--*/

FUNCTION ArGetFpscr
    vmrs    %r0, FPSCR                  @ Get FPSCR.
    bx      %lr

END_FUNCTION ArGetFpscr

//
// VOID
// ArSaveVfp (
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

FUNCTION ArSaveVfp
    stc     p11, c0, [%r0], #16*8       @ Save D0-D15 (stmia).
    cmp     %r1, #0                     @ Test for no SIMD support.
    ITE(ne)                             @ If equal then else.
    stclne  p11, c0, [%r0], #16*8       @ Save D16-D31 if SIMD support.
    addeq   %r0, %r0, #16*8             @ Skip those registers if not.
    vmrs    %r2, FPSCR                  @ Get FPSCR.
    str     %r2, [%r0]                  @ Store it.
    bx      %lr                         @ Return.

END_FUNCTION ArSaveVfp

//
// VOID
// ArRestoreVfp (
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

FUNCTION ArRestoreVfp
    ldc     p11, c0, [%r0], #16*8       @ Restore D0-D15 (ldmia).
    cmp     %r1, #0                     @ Test for no SIMD support.
    ITE(ne)                             @ If equal then else.
    ldclne  p11, c0, [%r0], #16*8       @ Restore D16-D31 if SIMD support.
    addeq   %r0, %r0, #16*8             @ Skip those registers if not.
    ldr     %r2, [%r0]                  @ Get FPSCR.
    vmsr    FPSCR, %r2                  @ Restore FPSCR.
    bx      %lr                         @ Return.

END_FUNCTION ArRestoreVfp

//
// --------------------------------------------------------- Internal Functions
//

