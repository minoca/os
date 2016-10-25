/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdarch.c

Abstract:

    This module implements ARM architectural support for the kernel debugger.

Author:

    Evan Green 11-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/debug/dbgproto.h>
#include <minoca/kernel/kdebug.h>
#include <minoca/kernel/arm.h>
#include "../kdp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// This macro determines whether or not a given instruction will break into the
// debugger.
//

#define IS_BREAKING_INSTRUCTION_ARM(_Instruction) \
    (((_Instruction) == ARM_BREAK_INSTRUCTION) || \
     ((_Instruction) == ARM_SINGLE_STEP_INSTRUCTION))

#define IS_BREAKING_INSTRUCTION_THUMB(_Instruction)         \
    (((_Instruction) == THUMB_BREAK_INSTRUCTION) ||         \
     ((_Instruction) == THUMB_DEBUG_SERVICE_INSTRUCTION) || \
     ((_Instruction) == THUMB_SINGLE_STEP_INSTRUCTION))

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
KdpGetNextPcReadMemory (
    PVOID Address,
    ULONG Size,
    PVOID Data
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Set this boolean to TRUE to have the target print out each "next PC"
// prediction it makes.
//

BOOL KdPrintNextPcPredictions = FALSE;

//
// Store a variable indicating whether freeze request are maskable interrupts
// or NMIs. On ARM, freeze requests are just regular IPIs.
//

BOOL KdFreezesAreMaskable = TRUE;

//
// Single step mode is actually implemented by decoding the next instruction,
// predicting the value of the PC, and then putting a software breakpoint there.
// These variables contain accounting information for that work.
//

PVOID KdSingleStepAddress = NULL;
ULONG KdSingleStepContents;

//
// ------------------------------------------------------------------ Functions
//

VOID
KdpInitializeDebuggingHardware (
    VOID
    )

/*++

Routine Description:

    This routine initializes ARM hardware debug registers. Currently hardware
    debug registers are not supported.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

VOID
KdpClearSingleStepMode (
    PULONG Exception,
    PTRAP_FRAME TrapFrame,
    PVOID *PreviousSingleStepAddress
    )

/*++

Routine Description:

    This routine turns off single step mode.

Arguments:

    Exception - Supplies the type of exception that this function is handling.

    TrapFrame - Supplies a pointer to the state of the machine immediately
        before the debug exception occurred. Also returns the possibly modified
        machine state.

    PreviousSingleStepAddress - Supplies a pointer where the address the
        single step breakpoint was set will be returned, if a software-based
        single step mechanism is in use.

Return Value:

    None.

--*/

{

    PVOID BreakingAddress;
    ULONG Length;
    PVOID SingleStepAddress;
    ULONG ValidBytes;
    BOOL Writable;

    //
    // Take a look at the instruction to determine if it was significant. All
    // thumb break instructions are 16 bits.
    //

    BreakingAddress = (PVOID)REMOVE_THUMB_BIT(TrapFrame->Pc);
    if ((TrapFrame->Cpsr & PSR_FLAG_THUMB) != 0) {
        BreakingAddress -= THUMB16_INSTRUCTION_LENGTH;
        ValidBytes = KdpValidateMemoryAccess(BreakingAddress,
                                             THUMB16_INSTRUCTION_LENGTH,
                                             NULL);

        if (ValidBytes != THUMB16_INSTRUCTION_LENGTH) {
            *Exception = EXCEPTION_ACCESS_VIOLATION;

        } else {
            switch (*((PUSHORT)BreakingAddress)) {
            case THUMB_DEBUG_SERVICE_INSTRUCTION:
                break;

            case THUMB_BREAK_INSTRUCTION:
                *Exception = EXCEPTION_BREAK;
                break;

            case THUMB_SINGLE_STEP_INSTRUCTION:
                *Exception = EXCEPTION_SINGLE_STEP;
                break;

            default:
                break;
            }
        }

    } else {
        BreakingAddress -= ARM_INSTRUCTION_LENGTH;
        ValidBytes = KdpValidateMemoryAccess(BreakingAddress,
                                             ARM_INSTRUCTION_LENGTH,
                                             NULL);

        if ((ValidBytes != ARM_INSTRUCTION_LENGTH) ||
            (ALIGN_RANGE_DOWN((UINTN)BreakingAddress, 4) !=
             (UINTN)BreakingAddress)) {

            *Exception = EXCEPTION_ACCESS_VIOLATION;

        } else {
            switch (*((PULONG)BreakingAddress)) {
            case ARM_BREAK_INSTRUCTION:
                *Exception = EXCEPTION_BREAK;
                break;

            case ARM_SINGLE_STEP_INSTRUCTION:
                *Exception = EXCEPTION_SINGLE_STEP;
                break;

            default:
                break;
            }
        }
    }

    //
    // Attempt to clear the single step address, which may not necessarily
    // be the same as the PC.
    //

    if (KdSingleStepAddress != NULL) {
        SingleStepAddress = (PVOID)REMOVE_THUMB_BIT((UINTN)KdSingleStepAddress);
        Length = ARM_INSTRUCTION_LENGTH;
        if (((UINTN)KdSingleStepAddress & ARM_THUMB_BIT) != 0) {
            Length = THUMB16_INSTRUCTION_LENGTH;
        }

        ValidBytes = KdpValidateMemoryAccess(SingleStepAddress,
                                             Length,
                                             &Writable);

        if (ValidBytes == Length) {

            //
            // If the debugger broke in because of the single step
            // breakpoint, set the PC back so the correct instruction gets
            // executed.
            //

            if (SingleStepAddress == BreakingAddress) {
                TrapFrame->Pc -= Length;
            }

            *PreviousSingleStepAddress = KdSingleStepAddress;

            //
            // Make sure the address is writable.
            //

            if (Writable == FALSE) {
                KdpModifyAddressMapping(SingleStepAddress, TRUE, &Writable);
            }

            if (Length == THUMB16_INSTRUCTION_LENGTH) {
                *((PUSHORT)SingleStepAddress) = KdSingleStepContents;

            } else {
                *((PULONG)SingleStepAddress) = KdSingleStepContents;
            }

            KdpCleanMemory(SingleStepAddress);
            if (Writable == FALSE) {
                KdpModifyAddressMapping(SingleStepAddress, FALSE, &Writable);
            }

            KdSingleStepAddress = NULL;
            KdSingleStepContents = 0;

        } else {
            KdpInternalPrint("Warning: Could not clear old single step "
                             "break at 0x%08x!\n",
                             (ULONG)KdSingleStepAddress);
        }
    }

    return;
}

VOID
KdpSetSingleStepMode (
    ULONG Exception,
    PTRAP_FRAME TrapFrame,
    PVOID SingleStepAddress
    )

/*++

Routine Description:

    This routine turns on single step mode.

Arguments:

    Exception - Supplies the type of exception that this function is handling.

    TrapFrame - Supplies a pointer to the state of the machine immediately
        before the debug exception occurred. Also returns the possibly modified
        machine state.

    SingleStepAddress - Supplies an optional pointer where the breakpoint
        should be set. This is only used by software based single step
        mechanisms to restore a previously unset single step breakpoint. If
        this is NULL, then the next instruction will be calculated from the
        current trap frame.

Return Value:

    None.

--*/

{

    PVOID Address;
    BOOL FunctionReturning;
    UINTN NextPc;
    ULONG ValidBytes;
    BOOL Writable;

    if (KdSingleStepAddress == NULL) {
        if (SingleStepAddress != NULL) {
            NextPc = (UINTN)SingleStepAddress;
            FunctionReturning = FALSE;

        } else {
            ArGetNextPc(TrapFrame,
                        KdpGetNextPcReadMemory,
                        &FunctionReturning,
                        (PVOID *)&NextPc);
        }

        if (KdPrintNextPcPredictions != FALSE) {
            if (FunctionReturning != FALSE) {
                KdpInternalPrint("Next: 0x%08x %x, TRUE\n",
                                 NextPc,
                                 TrapFrame->Cpsr);

            } else {
                KdpInternalPrint("Next: 0x%08x %x\n", NextPc, TrapFrame->Cpsr);
            }
        }

        if ((NextPc & ARM_THUMB_BIT) != 0) {
            Address = (PVOID)REMOVE_THUMB_BIT(NextPc);
            ValidBytes = KdpValidateMemoryAccess(Address,
                                                 THUMB16_INSTRUCTION_LENGTH,
                                                 &Writable);

            if ((ValidBytes == THUMB16_INSTRUCTION_LENGTH) &&
                (!IS_BREAKING_INSTRUCTION_THUMB(*((PUSHORT)Address)))) {

                KdSingleStepAddress = (PVOID)NextPc;
                KdSingleStepContents = *((PUSHORT)Address);
                if (Writable == FALSE) {
                    KdpModifyAddressMapping(Address, TRUE, &Writable);
                }

                *(PUSHORT)Address = THUMB_SINGLE_STEP_INSTRUCTION;
                KdpCleanMemory(Address);
                if (Writable == FALSE) {
                    KdpModifyAddressMapping(Address, FALSE, &Writable);
                }
            }

        } else {
            Address = (PVOID)NextPc;
            ValidBytes = KdpValidateMemoryAccess(Address,
                                                 ARM_INSTRUCTION_LENGTH,
                                                 &Writable);

            if ((ValidBytes == ARM_INSTRUCTION_LENGTH) &&
                (!IS_BREAKING_INSTRUCTION_ARM(*((PULONG)Address)))) {

                KdSingleStepAddress = (PVOID)NextPc;
                KdSingleStepContents = *((PULONG)Address);
                if (Writable == FALSE) {
                    KdpModifyAddressMapping(KdSingleStepAddress,
                                            TRUE,
                                            &Writable);
                }

                *(PULONG)KdSingleStepAddress = ARM_SINGLE_STEP_INSTRUCTION;
                KdpCleanMemory(Address);
                if (Writable == FALSE) {
                    KdpModifyAddressMapping(KdSingleStepAddress,
                                            FALSE,
                                            &Writable);
                }
            }
        }
    }

    return;
}

PVOID
KdpGetInstructionPointer (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine returns the instruction pointer in the trap frame.

Arguments:

    TrapFrame - Supplies a pointer to the state of the machine immediately
        before the debug exception occurred.

Return Value:

    Returns the current instruction pointer.

--*/

{

    ULONG Pc;

    Pc = TrapFrame->Pc;
    if ((TrapFrame->Cpsr & PSR_FLAG_THUMB) != 0) {
        Pc |= ARM_THUMB_BIT;
    }

    return (PVOID)Pc;
}

PVOID
KdpGetInstructionPointerAddress (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine returns the memory address corresponding to the current
    instruction pointer.

Arguments:

    TrapFrame - Supplies a pointer to the state of the machine immediately
        before the debug exception occurred.

Return Value:

    Returns the current instruction pointer address.

--*/

{

    ULONG Pc;

    Pc = REMOVE_THUMB_BIT(TrapFrame->Pc);
    return (PVOID)Pc;
}

VOID
KdpGetRegisters (
    PTRAP_FRAME TrapFrame,
    PVOID Registers
    )

/*++

Routine Description:

    This routine writes the register values from the trap frame into the
    debugger packet.

Arguments:

    TrapFrame - Supplies a pointer to the current processor state.

    Registers - Supplies a pointer where the register values will be written in
        for the debugger. For ARM, this is a pointer to an
        ARM_GENERAL_REGISTERS structure.

Return Value:

    None.

--*/

{

    PARM_GENERAL_REGISTERS DebuggerRegisters;

    if (TrapFrame == NULL) {
        return;
    }

    DebuggerRegisters = (PARM_GENERAL_REGISTERS)Registers;
    DebuggerRegisters->R0 = TrapFrame->R0;
    DebuggerRegisters->R1 = TrapFrame->R1;
    DebuggerRegisters->R2 = TrapFrame->R2;
    DebuggerRegisters->R3 = TrapFrame->R3;
    DebuggerRegisters->R4 = TrapFrame->R4;
    DebuggerRegisters->R5 = TrapFrame->R5;
    DebuggerRegisters->R6 = TrapFrame->R6;
    DebuggerRegisters->R7 = TrapFrame->R7;
    DebuggerRegisters->R8 = TrapFrame->R8;
    DebuggerRegisters->R9 = TrapFrame->R9;
    DebuggerRegisters->R10 = TrapFrame->R10;
    DebuggerRegisters->R11Fp = TrapFrame->R11;
    DebuggerRegisters->R12Ip = TrapFrame->R12;

    //
    // Select the banked SP and LR based on the running mode, which assumed to
    // be SVC if it's not user. Getting and setting these registers in other
    // modes is not supported.
    //

    if ((TrapFrame->Cpsr & ARM_MODE_MASK) == ARM_MODE_USER) {
        DebuggerRegisters->R13Sp = TrapFrame->UserSp;
        DebuggerRegisters->R14Lr = TrapFrame->UserLink;

    } else if ((TrapFrame->Cpsr & ARM_MODE_MASK) == ARM_MODE_SVC) {
        DebuggerRegisters->R13Sp = TrapFrame->SvcSp;
        DebuggerRegisters->R14Lr = TrapFrame->SvcLink;

    } else {
        DebuggerRegisters->R13Sp = MAX_ULONG;
        DebuggerRegisters->R14Lr = MAX_ULONG;
    }

    DebuggerRegisters->R15Pc = TrapFrame->Pc;
    DebuggerRegisters->Cpsr = TrapFrame->Cpsr;
    return;
}

ULONG
KdpGetErrorCode (
    ULONG Exception,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine gets the error code out of the trap frame. On ARM, there is no
    concept of an error code.

Arguments:

    Exception - Supplies the exception that generated the error code.

    TrapFrame - Supplies a pointer to the current processor state.

Return Value:

    Returns the error code, or 0 if there was no error code.

--*/

{

    return 0;
}

VOID
KdpSetRegisters (
    PTRAP_FRAME TrapFrame,
    PVOID Registers
    )

/*++

Routine Description:

    This routine writes the register values from the debugger to the trap
    frame.

Arguments:

    TrapFrame - Supplies a pointer to the current processor state.

    Registers - Supplies a pointer to the new register values to use.

Return Value:

    None.

--*/

{

    PARM_GENERAL_REGISTERS DebuggerRegisters;

    DebuggerRegisters = (PARM_GENERAL_REGISTERS)Registers;
    TrapFrame->R0 = DebuggerRegisters->R0;
    TrapFrame->R1 = DebuggerRegisters->R1;
    TrapFrame->R2 = DebuggerRegisters->R2;
    TrapFrame->R3 = DebuggerRegisters->R3;
    TrapFrame->R4 = DebuggerRegisters->R4;
    TrapFrame->R5 = DebuggerRegisters->R5;
    TrapFrame->R6 = DebuggerRegisters->R6;
    TrapFrame->R7 = DebuggerRegisters->R7;
    TrapFrame->R8 = DebuggerRegisters->R8;
    TrapFrame->R9 = DebuggerRegisters->R9;
    TrapFrame->R10 = DebuggerRegisters->R10;
    TrapFrame->R11 = DebuggerRegisters->R11Fp;
    TrapFrame->R12 = DebuggerRegisters->R12Ip;

    //
    // Select the banked SP and LR based on the running mode, which assumed to
    // be SVC if it's not User mode. Getting and setting these registers in
    // other modes is not supported.
    //

    if ((TrapFrame->Cpsr & ARM_MODE_MASK) == ARM_MODE_USER) {
        TrapFrame->UserSp = DebuggerRegisters->R13Sp;
        TrapFrame->UserLink = DebuggerRegisters->R14Lr;

    } else if ((TrapFrame->Cpsr & ARM_MODE_MASK) == ARM_MODE_SVC) {
        TrapFrame->SvcSp = DebuggerRegisters->R13Sp;
        TrapFrame->SvcLink = DebuggerRegisters->R14Lr;
    }

    TrapFrame->Pc = DebuggerRegisters->R15Pc;
    TrapFrame->Cpsr = DebuggerRegisters->Cpsr;
    return;
}

BOOL
KdpIsFunctionReturning (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine will determine if the current instruction (the instruction
    about to get executed) is going to return from the current function.

Arguments:

    TrapFrame - Supplies a pointer to the current machine state.

Return Value:

    Returns TRUE if the function is about to return, or FALSE if this is not
    a return instruction.

--*/

{

    BOOL IsFunctionReturning;
    PVOID NextPc;

    IsFunctionReturning = FALSE;
    ArGetNextPc(TrapFrame,
                KdpGetNextPcReadMemory,
                &IsFunctionReturning,
                &NextPc);

    return IsFunctionReturning;
}

VOID
KdpGetSpecialRegisters (
    PSPECIAL_REGISTERS_UNION SpecialRegisters
    )

/*++

Routine Description:

    This routine retrieves the special registers from the current processor.

Arguments:

    SpecialRegisters - Supplies a pointer where the contents of the special
        registers will be returned.

Return Value:

    None.

--*/

{

    PARM_SPECIAL_REGISTERS ArmRegisters;

    ArmRegisters = &(SpecialRegisters->Arm);
    ArmRegisters->Sctlr = ArGetSystemControlRegister();
    ArmRegisters->Actlr = ArGetAuxiliaryControlRegister();
    ArmRegisters->Ttbr0 = ArGetTranslationTableBaseRegister0();
    ArmRegisters->Ttbr1 = ArGetTranslationTableBaseRegister1();
    ArmRegisters->Dfsr = ArGetDataFaultStatus();
    ArmRegisters->Ifsr = ArGetInstructionFaultStatus();
    ArmRegisters->Dfar = (UINTN)ArGetDataFaultingAddress();
    ArmRegisters->Ifar = (UINTN)ArGetInstructionFaultingAddress();
    ArmRegisters->Prrr = ArGetPrimaryRegionRemapRegister();
    ArmRegisters->Nmrr = ArGetNormalMemoryRemapRegister();
    ArmRegisters->Vbar = (UINTN)ArGetVectorBaseAddress();
    ArmRegisters->Par = ArGetPhysicalAddressRegister();
    ArmRegisters->Tpidrprw = (UINTN)ArGetProcessorBlockRegister();
    ArmRegisters->Ats1Cpr = 0;
    ArmRegisters->Ats1Cpw = 0;
    ArmRegisters->Ats1Cur = 0;
    ArmRegisters->Ats1Cuw = 0;
    return;
}

VOID
KdpSetSpecialRegisters (
    PSPECIAL_REGISTERS_UNION OriginalRegisters,
    PSPECIAL_REGISTERS_UNION NewRegisters
    )

/*++

Routine Description:

    This routine sets the special registers from the current processor.

Arguments:

    OriginalRegisters - Supplies a pointer to the current special register
        context.

    NewRegisters - Supplies a pointer to the values to write. Only values
        different from the original registers will actually be written.

Return Value:

    None.

--*/

{

    PARM_SPECIAL_REGISTERS New;
    PARM_SPECIAL_REGISTERS Original;

    Original = &(OriginalRegisters->Arm);
    New = &(NewRegisters->Arm);
    if (New->Sctlr != Original->Sctlr) {
        ArSetSystemControlRegister(New->Sctlr);
    }

    if (New->Actlr != Original->Actlr) {
        ArSetAuxiliaryControlRegister(New->Actlr);
    }

    if (New->Ttbr0 != Original->Ttbr0) {
        ArSetTranslationTableBaseRegister0(New->Ttbr0);
    }

    if (New->Ttbr1 != Original->Ttbr1) {
        ArSetTranslationTableBaseRegister1(New->Ttbr1);
    }

    if (New->Dfsr != Original->Dfsr) {
        ArSetDataFaultStatus(New->Dfsr);
    }

    if (New->Ifsr != Original->Ifsr) {
        ArSetInstructionFaultStatus(New->Ifsr);
    }

    if (New->Dfar != Original->Dfar) {
        ArSetDataFaultingAddress((PVOID)(UINTN)(New->Dfar));
    }

    if (New->Ifar != Original->Ifar) {
        ArSetInstructionFaultingAddress((PVOID)(UINTN)(New->Ifar));
    }

    if (New->Prrr != Original->Prrr) {
        ArSetPrimaryRegionRemapRegister(New->Prrr);
    }

    if (New->Nmrr != Original->Nmrr) {
        ArSetNormalMemoryRemapRegister(New->Nmrr);
    }

    if (New->Vbar != Original->Vbar) {
        ArSetVectorBaseAddress((PVOID)(UINTN)(New->Vbar));
    }

    if (New->Tpidrprw != Original->Tpidrprw) {
        ArSetProcessorBlockRegister((PVOID)(UINTN)(New->Tpidrprw));
    }

    if (New->Par != Original->Par) {
        ArSetPhysicalAddressRegister(New->Par);
    }

    if (New->Ats1Cpr != Original->Ats1Cpr) {
        ArSetPrivilegedReadTranslateRegister(New->Ats1Cpr);
    }

    if (New->Ats1Cpw != Original->Ats1Cpw) {
        ArSetPrivilegedWriteTranslateRegister(New->Ats1Cpw);
    }

    if (New->Ats1Cur != Original->Ats1Cur) {
        ArSetUnprivilegedReadTranslateRegister(New->Ats1Cur);
    }

    if (New->Ats1Cuw != Original->Ats1Cuw) {
        ArSetUnprivilegedWriteTranslateRegister(New->Ats1Cuw);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
KdpGetNextPcReadMemory (
    PVOID Address,
    ULONG Size,
    PVOID Data
    )

/*++

Routine Description:

    This routine attempts to read memory on behalf of the function trying to
    figure out what the next instruction will be.

Arguments:

    Address - Supplies the virtual address that needs to be read.

    Size - Supplies the number of bytes to be read.

    Data - Supplies a pointer to the buffer where the read data will be
        returned on success.

Return Value:

    Status code. STATUS_SUCCESS will only be returned if all the requested
    bytes could be read.

--*/

{

    ULONG ValidBytes;

    ValidBytes = KdpValidateMemoryAccess(Address, Size, NULL);
    if (ValidBytes == Size) {
        RtlCopyMemory(Data, Address, Size);
        return STATUS_SUCCESS;
    }

    return STATUS_DATA_PAGED_OUT;
}

