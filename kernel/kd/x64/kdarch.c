/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdarch.c

Abstract:

    This module implements AMD64 architectural support for the kernel debugger.

Author:

    Evan Green 2-Jun-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/debug/dbgproto.h>
#include <minoca/kernel/kdebug.h>
#include <minoca/kernel/x64.h>
#include "../kdp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KdpInitializeDebugRegisters (
    VOID
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the machine architecture.
//

ULONG KdMachineType = MACHINE_TYPE_X64;

//
// Store a variable indicating whether freeze request are maskable interrupts
// or NMIs. On PCs, freeze requests are NMIs.
//

BOOL KdFreezesAreMaskable = FALSE;

//
// ------------------------------------------------------------------ Functions
//

VOID
KdpInitializeDebuggingHardware (
    VOID
    )

/*++

Routine Description:

    This routine initializes x86 hardware debug registers.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KdpInitializeDebugRegisters();
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

    TrapFrame->Rflags &= ~IA32_EFLAG_TF;
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

    TrapFrame->Rflags |= IA32_EFLAG_TF;
    return;
}

VOID
KdpInvalidateInstructionCache (
    VOID
    )

/*++

Routine Description:

    This routine invalidates the instruction cache to PoU inner shareable.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

VOID
KdpCleanMemory (
    PVOID Address
    )

/*++

Routine Description:

    This routine cleans memory modified by the kernel debugger, flushing it
    out of the instruciton and data caches.

Arguments:

    Address - Supplies the address whose associated cache line will be
        cleaned.

Return Value:

    None.

--*/

{

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

    PTRAP_FRAME Registers;

    Registers = TrapFrame;
    if ((Registers->Rflags & IA32_EFLAG_VM) != 0) {
        return (PVOID)((Registers->Cs << 4) + Registers->Rip);
    }

    return (PVOID)(Registers->Rip);
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

    return KdpGetInstructionPointer(TrapFrame);
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
        for the debugger. For x86, this is a pointer to an
        X86_GENERAL_REGISTERS structure.

Return Value:

    None.

--*/

{

    PX64_GENERAL_REGISTERS DebuggerRegisters;

    DebuggerRegisters = (PX64_GENERAL_REGISTERS)Registers;
    DebuggerRegisters->Rax = TrapFrame->Rax;
    DebuggerRegisters->Rbx = TrapFrame->Rbx;
    DebuggerRegisters->Rcx = TrapFrame->Rcx;
    DebuggerRegisters->Rdx = TrapFrame->Rdx;
    DebuggerRegisters->Rbp = TrapFrame->Rbp;
    DebuggerRegisters->Rsp = TrapFrame->Rsp;
    DebuggerRegisters->Rsi = TrapFrame->Rsi;
    DebuggerRegisters->Rdi = TrapFrame->Rdi;
    DebuggerRegisters->R8 = TrapFrame->R8;
    DebuggerRegisters->R9 = TrapFrame->R9;
    DebuggerRegisters->R10 = TrapFrame->R10;
    DebuggerRegisters->R11 = TrapFrame->R11;
    DebuggerRegisters->R12 = TrapFrame->R12;
    DebuggerRegisters->R13 = TrapFrame->R13;
    DebuggerRegisters->R14 = TrapFrame->R14;
    DebuggerRegisters->R15 = TrapFrame->R15;
    DebuggerRegisters->Rip = TrapFrame->Rip;
    DebuggerRegisters->Rflags = TrapFrame->Rflags;
    DebuggerRegisters->Cs = (USHORT)TrapFrame->Cs;
    DebuggerRegisters->Ds = (USHORT)TrapFrame->Ds;
    DebuggerRegisters->Es = (USHORT)TrapFrame->Es;
    DebuggerRegisters->Fs = (USHORT)TrapFrame->Fs;
    DebuggerRegisters->Gs = (USHORT)TrapFrame->Gs;
    DebuggerRegisters->Ss = (USHORT)TrapFrame->Ss;
    return;
}

ULONG
KdpGetErrorCode (
    ULONG Exception,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine gets the error code out of the trap frame.

Arguments:

    Exception - Supplies the exception that generated the error code.

    TrapFrame - Supplies a pointer to the current processor state.

Return Value:

    Returns the error code, or 0 if there was no error code.

--*/

{

    return TrapFrame->ErrorCode;
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

    PX64_GENERAL_REGISTERS DebuggerRegisters;

    DebuggerRegisters = (PX64_GENERAL_REGISTERS)Registers;
    TrapFrame->Rax = DebuggerRegisters->Rax;
    TrapFrame->Rbx = DebuggerRegisters->Rbx;
    TrapFrame->Rcx = DebuggerRegisters->Rcx;
    TrapFrame->Rdx = DebuggerRegisters->Rdx;
    TrapFrame->Rsi = DebuggerRegisters->Rsi;
    TrapFrame->Rdi = DebuggerRegisters->Rdi;
    TrapFrame->Rsp = DebuggerRegisters->Rsp;
    TrapFrame->Rbp = DebuggerRegisters->Rbp;
    TrapFrame->R8 = DebuggerRegisters->R8;
    TrapFrame->R9 = DebuggerRegisters->R9;
    TrapFrame->R10 = DebuggerRegisters->R10;
    TrapFrame->R11 = DebuggerRegisters->R11;
    TrapFrame->R12 = DebuggerRegisters->R12;
    TrapFrame->R13 = DebuggerRegisters->R13;
    TrapFrame->R14 = DebuggerRegisters->R14;
    TrapFrame->R15 = DebuggerRegisters->R15;
    TrapFrame->Rip = DebuggerRegisters->Rip;
    TrapFrame->Rflags = DebuggerRegisters->Rflags;
    TrapFrame->Cs = DebuggerRegisters->Cs;
    TrapFrame->Ds = DebuggerRegisters->Ds;
    TrapFrame->Es = DebuggerRegisters->Es;
    TrapFrame->Fs = DebuggerRegisters->Fs;
    TrapFrame->Gs = DebuggerRegisters->Gs;
    TrapFrame->Ss = DebuggerRegisters->Ss;
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

    //
    // The compiler puts all return instructions as the very last instruction
    // of the function, which the debugger already knows is a return
    // instruction, so always return FALSE here.
    //

    return FALSE;
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

    PX86_SPECIAL_REGISTERS IaRegisters;
    TABLE_REGISTER TableRegister;
    ULONG TrRegister;

    IaRegisters = &(SpecialRegisters->Ia);
    IaRegisters->Cr0 = ArGetControlRegister0();
    IaRegisters->Cr2 = (UINTN)ArGetFaultingAddress();
    IaRegisters->Cr3 = ArGetCurrentPageDirectory();
    IaRegisters->Cr4 = ArGetControlRegister4();
    IaRegisters->Dr0 = ArGetDebugRegister0();
    IaRegisters->Dr1 = ArGetDebugRegister1();
    IaRegisters->Dr2 = ArGetDebugRegister2();
    IaRegisters->Dr3 = ArGetDebugRegister3();
    IaRegisters->Dr6 = ArGetDebugRegister6();
    IaRegisters->Dr7 = ArGetDebugRegister7();
    ArStoreIdtr(&TableRegister);
    IaRegisters->Idtr.Limit = TableRegister.Limit;
    IaRegisters->Idtr.Base = TableRegister.Base;
    ArStoreGdtr(&TableRegister);
    IaRegisters->Gdtr.Limit = TableRegister.Limit;
    IaRegisters->Gdtr.Base = TableRegister.Base;
    ArStoreTr(&TrRegister);
    IaRegisters->Tr = TrRegister;
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

    PX86_SPECIAL_REGISTERS New;
    PX86_SPECIAL_REGISTERS Original;
    TABLE_REGISTER TableRegister;

    Original = &(OriginalRegisters->Ia);
    New = &(NewRegisters->Ia);
    if (New->Cr0 != Original->Cr0) {
        ArSetControlRegister0((ULONG)New->Cr0);
    }

    if (New->Cr2 != Original->Cr2) {
        ArSetFaultingAddress((PVOID)(UINTN)New->Cr2);
    }

    if (New->Cr3 != Original->Cr3) {
        ArSetCurrentPageDirectory(New->Cr3);
    }

    if (New->Cr4 != Original->Cr4) {
        ArSetControlRegister4((ULONG)New->Cr4);
    }

    if (New->Dr0 != Original->Dr0) {
        ArSetDebugRegister0((UINTN)New->Dr0);
    }

    if (New->Dr1 != Original->Dr1) {
        ArSetDebugRegister1((UINTN)New->Dr1);
    }

    if (New->Dr2 != Original->Dr2) {
        ArSetDebugRegister2((UINTN)New->Dr2);
    }

    if (New->Dr3 != Original->Dr3) {
        ArSetDebugRegister3((UINTN)New->Dr3);
    }

    if (New->Dr6 != Original->Dr6) {
        ArSetDebugRegister6((UINTN)New->Dr6);
    }

    if (New->Dr7 != Original->Dr7) {
        ArSetDebugRegister7((UINTN)New->Dr7);
    }

    if ((New->Idtr.Limit != Original->Idtr.Limit) ||
        (New->Idtr.Base != Original->Idtr.Base)) {

        TableRegister.Limit = New->Idtr.Limit;
        TableRegister.Base = New->Idtr.Base;
        ArLoadIdtr(&TableRegister);
    }

    if ((New->Gdtr.Limit != Original->Gdtr.Limit) ||
        (New->Gdtr.Base != Original->Gdtr.Base)) {

        TableRegister.Limit = New->Gdtr.Limit;
        TableRegister.Base = New->Gdtr.Base;
        ArLoadGdtr(&TableRegister);
    }

    if (New->Tr != Original->Tr) {
        ArLoadTr(New->Tr);
    }

    return;
}
