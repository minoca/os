/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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

##
## ------------------------------------------------------------------ Includes
##

#include <minoca/arm.inc>

##
## --------------------------------------------------------------- Definitions
##

##
## ---------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
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

    TRUE if interrupts were previously enabled on the processor.

    FALSE if interrupts were not previously enabled on the processor.

--*/

FUNCTION ArDisableInterrupts
    mrs     %r1, CPSR               @ Get the status register.
    cpsid   i                       @ Disable interrupts.
    mov     %r0, #0                 @ Assume interrupts disabled.
    tst     %r1, #PSR_FLAG_IRQ      @ AND the interrupt flag.
.if THUMB
    iteq    eq                      @ If the zero flag is set.
.endif
    moveq   %r0, #1                 @ Interrupts were enabled.
    bx      %lr                     @ Return.

END_FUNCTION ArDisableInterrupts

##
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

FUNCTION ArEnableInterrupts
    cpsie   i                       @ Enable interrupts.
    bx      %lr                     @

END_FUNCTION ArEnableInterrupts

##
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

FUNCTION ArAreInterruptsEnabled
    mrs     %r1, CPSR               @ Get the status register.
    mov     %r0, #0                 @ Assume interrupts disabled.
    tst     %r1, #PSR_FLAG_IRQ      @ AND the interrupt flag.
.if THUMB
    iteq    eq                      @ If the zero flag is set.
.endif
    moveq   %r0, #1                 @ Interrupts were enabled.
    bx      %lr                     @ Return.

END_FUNCTION ArAreInterruptsEnabled

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

FUNCTION ArGetProcessorFlags
    mrs     %r0, CPSR               @ Get the status register.
    bx      %lr                     @

END_FUNCTION ArGetProcessorFlags

##
## ULONG
## ArGetCacheTypeRegister (
##     VOID
##     )
##

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

##
## VOID
## ArInvalidateInstructionCacheLine (
##     PVOID Address
##     )
##

/*++

Routine Description:

    This routine invalidates a line in the instruction cache by virtual address.

Arguments:

    Address - Supplies the address whose associated instruction cache line will
        be invalidated.

Return Value:

    None.

--*/

FUNCTION ArInvalidateInstructionCacheLine
    mcr     p15, 0, %r0, %cr7, %cr5, 1          @ Write to ICIMVAU.
    mcr     p15, 0, %r0, %cr7, %cr5, 7          @ Write to BPIMVA
    bx      %lr                                 @ Return.

END_FUNCTION ArInvalidateInstructionCacheLine

##
## PVOID
## ArGetDataFaultingAddress (
##     VOID
##     )
##

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

##
## VOID
## ArSetDataFaultingAddress (
##     PVOID Value
##     )
##

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

##
## PVOID
## ArGetInstructionFaultingAddress (
##     VOID
##     )
##

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

##
## VOID
## ArSetInstructionFaultingAddress (
##     PVOID Value
##     )
##

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

##
## ULONG
## ArGetDataFaultStatus (
##     VOID
##     )
##

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

##
## VOID
## ArSetDataFaultStatus (
##     ULONG Value
##     )
##

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

##
## ULONG
## ArGetInstructionFaultStatus (
##     VOID
##     )
##

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

##
## VOID
## ArSetInstructionFaultStatus (
##     ULONG Value
##     )
##

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

FUNCTION ArProcessorYield
    yield
    bx      %lr

END_FUNCTION ArProcessorYield

##
## VOID
## ArCpuid (
##     PARM_CPUID Features
##     )
##

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

##
## ULONG
## ArGetSystemControlRegister (
##     VOID
##     )
##

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

##
## VOID
## ArSetSystemControlRegister (
##     ULONG NewValue
##     )
##

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

##
## ULONG
## ArGetAuxiliaryControlRegister (
##     VOID
##     )
##

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

##
## VOID
## ArSetAuxiliaryControlRegister (
##     ULONG NewValue
##     )
##

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

##
## PVOID
## ArGetVectorBaseAddress (
##     VOID
##     )
##

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

##
## VOID
## ArSetVectorBaseAddress (
##     PVOID VectorBaseAddress
##     )
##

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

##
## PVOID
## ArGetProcessorBlockRegister (
##     VOID
##     )
##

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

##
## PVOID
## ArGetProcessorBlockRegisterForDebugger (
##     VOID
##     )
##

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

##
## VOID
## ArSetProcessorBlockRegister (
##     PVOID ProcessorBlockRegisterValue
##     )
##

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

##
## UINTN
## ArDereferenceProcessorBlock (
##     UINTN Offset
##     )
##

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

##
## ULONG
## ArGetTranslationTableBaseRegister0 (
##     VOID
##     )
##

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

##
## VOID
## ArSetTranslationTableBaseRegister0 (
##     ULONG Value
##     )
##

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

##
## ULONG
## ArGetTranslationTableBaseRegister1 (
##     VOID
##     )
##

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

##
## VOID
## ArSetTranslationTableBaseRegister1 (
##     ULONG Value
##     )
##

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

##
## ULONG
## ArGetPrimaryRegionRemapRegister (
##     VOID
##     )
##

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

##
## VOID
## ArSetPrimaryRegionRemapRegister (
##     ULONG Value
##     )
##

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

##
## ULONG
## ArGetNormalMemoryRemapRegister (
##     VOID
##     )
##

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

##
## VOID
## ArSetNormalMemoryRemapRegister (
##     ULONG Value
##     )
##

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

##
## ULONG
## ArGetPhysicalAddressRegister (
##     VOID
##     )
##

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

##
## VOID
## ArSetPhysicalAddressRegister (
##     ULONG Value
##     )
##

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

##
## VOID
## ArSetPrivilegedReadTranslateRegister (
##     ULONG Value
##     )
##

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

##
## VOID
## ArSetPrivilegedWriteTranslateRegister (
##     ULONG Value
##     )
##

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

##
## VOID
## ArSetUnprivilegedReadTranslateRegister (
##     ULONG Value
##     )
##

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

##
## VOID
## ArSetUnprivilegedWriteTranslateRegister (
##     ULONG Value
##     )
##

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

##
## ULONG
## ArTranslateVirtualToPhysical (
##     PVOID VirtualAddress
##     )
##

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

##
## VOID
## ArSetThreadPointerUserReadOnly (
##     PVOID NewPointer
##     )
##

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

##
## ULONG
## ArGetThreadPointerUser (
##     VOID
##     )
##

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

##
## --------------------------------------------------------- Internal Functions
##

