/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgrcomm.c

Abstract:

    This module implements much of the debugger protocol communication for the
    debugger client.

Author:

    Evan Green 3-Jul-2012

Environment:

    Debugger client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dbgrtl.h"
#include <minoca/debug/spproto.h>
#include <minoca/lib/im.h>
#include <minoca/debug/dbgext.h>
#include "disasm.h"
#include "dbgapi.h"
#include "dbgrprof.h"
#include "console.h"
#include "symbols.h"
#include "dbgrcomm.h"
#include "dbgsym.h"
#include "extsp.h"
#include "remsrv.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

//
// ---------------------------------------------------------------- Definitions
//

#define DEFAULT_DISASSEMBLED_INSTRUCTIONS 10
#define DEFAULT_RECURSION_DEPTH 3
#define BYTES_PER_INSTRUCTION 15
#define DEFAULT_MEMORY_PRINT_ROWS 10
#define DEFAULT_DUMP_POINTERS_ROWS 100

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
DbgrpSetBreakpointAtAddress (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PULONG OriginalValue
    );

INT
DbgrpClearBreakpointAtAddress (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG OriginalValue
    );

ULONG
DbgrpHandleBreakpoints (
    PDEBUGGER_CONTEXT Context
    );

INT
DbgrpAdjustInstructionPointerForBreakpoint (
    PDEBUGGER_CONTEXT Context,
    ULONG OriginalValue
    );

INT
DbgrpProcessBreakNotification (
    PDEBUGGER_CONTEXT Context
    );

INT
DbgrpRangeStep (
    PDEBUGGER_CONTEXT Context,
    PRANGE_STEP RangeStep
    );

INT
DbgrpValidateLoadedModules (
    PDEBUGGER_CONTEXT Context,
    ULONG ModuleCount,
    ULONGLONG Signature,
    BOOL ForceReload
    );

VOID
DbgrpUnloadModule (
    PDEBUGGER_CONTEXT Context,
    PDEBUGGER_MODULE Module,
    BOOL Verbose
    );

VOID
DbgrpUnloadAllModules (
    PDEBUGGER_CONTEXT Context,
    BOOL Verbose
    );

INT
DbgrpPrintDisassembly (
    PDEBUGGER_CONTEXT Context,
    PBYTE InstructionStream,
    ULONGLONG InstructionPointer,
    ULONG InstructionCount,
    PULONG BytesDecoded
    );

PDATA_SYMBOL
DbgpGetLocal (
    PFUNCTION_SYMBOL Function,
    PSTR LocalName,
    ULONGLONG ExecutionAddress
    );

PSTR
DbgrpCreateFullPath (
    PSOURCE_FILE_SYMBOL Source
    );

INT
DbgrpPrintMemory (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    BOOL VirtualAddress,
    ULONG TypeSize,
    ULONG Columns,
    ULONG TotalValues,
    BOOL PrintCharacters
    );

VOID
DbgrpProcessShutdown (
    PDEBUGGER_CONTEXT Context
    );

PDEBUGGER_MODULE
DbgpLoadModule (
    PDEBUGGER_CONTEXT Context,
    PSTR BinaryName,
    PSTR FriendlyName,
    ULONGLONG Size,
    ULONGLONG LowestAddress,
    ULONGLONG Timestamp,
    ULONG Process
    );

INT
DbgrpResolveDumpType (
    PDEBUGGER_CONTEXT Context,
    PTYPE_SYMBOL *Type,
    PVOID *Data,
    PUINTN DataSize,
    PULONGLONG Address
    );

INT
DbgrpSetFrame (
    PDEBUGGER_CONTEXT Context,
    ULONG FrameNumber
    );

INT
DbgrpEnableBreakPoint (
    PDEBUGGER_CONTEXT Context,
    LONG BreakPointIndex,
    BOOL Enable
    );

VOID
DbgrpDestroySourcePath (
    PDEBUGGER_SOURCE_PATH SourcePath
    );

INT
DbgrpLoadFile (
    PSTR Path,
    PVOID *Contents,
    PULONGLONG Size
    );

VOID
DbgrpPrintEflags (
    ULONGLONG Eflags
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the global context, which is needed for requesting
// break-ins.
//

extern PDEBUGGER_CONTEXT DbgConsoleContext;

//
// ------------------------------------------------------------------ Functions
//

INT
DbgrInitialize (
    PDEBUGGER_CONTEXT Context,
    DEBUG_CONNECTION_TYPE ConnectionType
    )

/*++

Routine Description:

    This routine initializes data structures for common debugger functionality.

Arguments:

    Context - Supplies a pointer to the application context.

    ConnectionType - Supplies the type of debug connection to set the debugger
        up in.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    INT Status;

    Status = EINVAL;
    Context->Flags |= DEBUGGER_FLAG_SOURCE_LINE_STEPPING;

    //
    // Initialize the loaded modules list.
    //

    if (Context->ModuleList.ModuleCount == 0) {
        Context->ModuleList.Signature = 0;
        INITIALIZE_LIST_HEAD(&(Context->ModuleList.ModulesHead));
    }

    if (Context->BreakpointList.Next == NULL) {
        INITIALIZE_LIST_HEAD(&(Context->BreakpointList));
    }

    //
    // Initialize the profiler.
    //

    Status = DbgrProfilerInitialize(Context);
    if (Status != 0) {
        goto InitializeEnd;
    }

    Status = DbgInitialize(Context, ConnectionType);
    if (Status != 0) {
        goto InitializeEnd;
    }

    Status = 0;

InitializeEnd:
    return Status;
}

VOID
DbgrDestroy (
    PDEBUGGER_CONTEXT Context,
    DEBUG_CONNECTION_TYPE ConnectionType
    )

/*++

Routine Description:

    This routine destroys any data structures used for common debugger
    functionality.

Arguments:

    Context - Supplies a pointer to the application context.

    ConnectionType - Supplies the type of debug connection the debugger was set
        up in.

Return Value:

    None.

--*/

{

    //
    // Destroy the profiler structures.
    //

    DbgrProfilerDestroy(Context);
    DbgDestroy(Context, ConnectionType);
    return;
}

INT
DbgrConnect (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine establishes a link with the target debuggee. It is assumed that
    the underlying communication layer has already been established (COM ports
    have been opened and initialized, etc).

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR Architecture;
    PSTR BuildDebugString;
    PSTR BuildString;
    CHAR BuildTime[50];
    PCONNECTION_RESPONSE ConnectionResponse;
    BOOL InitialBreak;
    PSTR ProductName;
    INT Result;
    ULONG Size;
    time_t Time;
    struct tm *TimeStructure;

    InitialBreak = FALSE;
    if ((Context->Flags & DEBUGGER_FLAG_INITIAL_BREAK) != 0) {
        InitialBreak = TRUE;
    }

    //
    // Connect to the target.
    //

    DbgOut("Waiting to connect...\n");
    Result = DbgKdConnect(Context, InitialBreak, &ConnectionResponse, &Size);
    if (Result != 0) {
        DbgOut("Error: Unable to connect.\n");
        return Result;
    }

    //
    // A connection was successfully established. Print the banner and
    // return.
    //

    Context->MachineType = ConnectionResponse->Machine;
    switch (ConnectionResponse->Machine) {
    case MACHINE_TYPE_X86:
        Architecture = "x86";
        break;

    case MACHINE_TYPE_ARM:
        Architecture = "ARM";
        break;

    case MACHINE_TYPE_X64:
        Architecture = "x64";
        break;

    default:
        Architecture = "Unknown";
        break;
    }

    ProductName = "Unknown Target";
    if ((ConnectionResponse->ProductNameOffset != 0) &&
        (ConnectionResponse->ProductNameOffset < Size)) {

        ProductName = (PSTR)ConnectionResponse +
                      ConnectionResponse->ProductNameOffset;
    }

    BuildString = "";
    if ((ConnectionResponse->BuildStringOffset != 0) &&
        (ConnectionResponse->BuildStringOffset < Size)) {

        BuildString = (PSTR)ConnectionResponse +
                      ConnectionResponse->BuildStringOffset;
    }

    BuildDebugString = RtlGetBuildDebugLevelString(
                                    ConnectionResponse->SystemBuildDebugLevel);

    DbgOut("Connected to %s on %s\nSystem Version %d.%d.%d.%I64d %s %s %s\n",
           ProductName,
           Architecture,
           ConnectionResponse->SystemMajorVersion,
           ConnectionResponse->SystemMinorVersion,
           ConnectionResponse->SystemRevision,
           ConnectionResponse->SystemSerialVersion,
           RtlGetReleaseLevelString(ConnectionResponse->SystemReleaseLevel),
           BuildDebugString,
           BuildString);

    Time = ConnectionResponse->SystemBuildTime + SYSTEM_TIME_TO_EPOCH_DELTA;
    TimeStructure = localtime(&Time);
    strftime(BuildTime,
             sizeof(BuildTime),
             "%a %b %d, %Y %I:%M %p",
             TimeStructure);

    DbgOut("Built on %s.\n", BuildTime);
    free(ConnectionResponse);
    return 0;
}

INT
DbgrQuit (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine exits the local debugger.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    DbgOut("\n*** Exiting ***\n");
    Context->Flags |= DEBUGGER_FLAG_EXITING;
    return 0;
}

INT
DbgrGo (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine interprets the "go" command from the user.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONGLONG Evaluation;
    PSTR GoUntilAddress;
    INT Result;

    GoUntilAddress = NULL;
    if (ArgumentCount > 1) {
        GoUntilAddress = Arguments[1];
    }

    //
    // If no argument was specified, send the unconditional go.
    //

    if (GoUntilAddress == NULL) {
        Result = DbgrContinue(Context, FALSE, 0);
        return Result;
    }

    //
    // Evaluate the argument. If it fails, print a message, and do not send the
    // command.
    //

    Result = DbgEvaluate(Context, GoUntilAddress, &Evaluation);
    if (Result != 0) {
        DbgOut("Error: Unable to evaluate \"%s\".\n", GoUntilAddress);
        return Result;
    }

    //
    // Send the command with a one-time breakpoint.
    //

    Result = DbgrContinue(Context, TRUE, Evaluation);
    return Result;
}

INT
DbgrSingleStep (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine steps the target by a single instruction.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INT Result;
    ULONG SignalToDeliver;

    SignalToDeliver = DbgGetSignalToDeliver(Context);
    Result = DbgSingleStep(Context, SignalToDeliver);
    return Result;
}

INT
DbgrGetSetRegisters (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine prints or modifies the target machine's registers.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PARM_GENERAL_REGISTERS ArmRegisters;
    ULONG Psr;
    PSTR RegisterString;
    INT Result;
    ULONGLONG Value;
    PSTR ValueString;
    PX64_GENERAL_REGISTERS X64Registers;
    PX86_GENERAL_REGISTERS X86Registers;

    RegisterString = NULL;
    Result = 0;
    ValueString = NULL;
    if (ArgumentCount >= 2) {
        RegisterString = Arguments[1];
        if (ArgumentCount >= 3) {
            ValueString = Arguments[2];
        }
    }

    assert(Context->CurrentEvent.Type == DebuggerEventBreak);

    ArmRegisters = &(Context->CurrentEvent.BreakNotification.Registers.Arm);
    X64Registers = &(Context->CurrentEvent.BreakNotification.Registers.X64);
    X86Registers = &(Context->CurrentEvent.BreakNotification.Registers.X86);
    if (Context->CurrentFrame != 0) {
        ArmRegisters = &(Context->FrameRegisters.Arm);
        X64Registers = &(Context->FrameRegisters.X64);
        X86Registers = &(Context->FrameRegisters.X86);
    }

    //
    // If the first parameter is not NULL, find the register the user is
    // talking about.
    //

    if (RegisterString != NULL) {

        //
        // If no other parameter was specified, print out the value of the
        // specified register.
        //

        if (ValueString == NULL) {
            if (EvalGetRegister(Context, RegisterString, &Value) == FALSE) {
                DbgOut("Error: Invalid Register \"%s\".\n", RegisterString);
                goto GetSetRegistersEnd;
            }

            DbgOut("%0*llx\n", DbgGetTargetPointerSize(Context) * 2, Value);
            goto GetSetRegistersEnd;

        //
        // A value to write into the register was supplied. Attempt to evaluate
        // and write the register.
        //

        } else {
            if (Context->CurrentFrame != 0) {
                DbgOut("Error: Registers can only be set in frame 0.\n");
                goto GetSetRegistersEnd;
            }

            Result = DbgEvaluate(Context, ValueString, &Value);
            if (Result != 0) {
                DbgOut("Error: Unable to evaluate \"%s\".\n", ValueString);
                goto GetSetRegistersEnd;
            }

            if (EvalSetRegister(Context, RegisterString, Value) == FALSE) {
                DbgOut("Error: Invalid Register \"%s\".\n", RegisterString);
                goto GetSetRegistersEnd;
            }

            Result = DbgSetRegisters(
                         Context,
                         &(Context->CurrentEvent.BreakNotification.Registers));

            goto GetSetRegistersEnd;
        }

    //
    // No parameters were specified, just dump all the register contents.
    //

    } else {
        if (Context->CurrentFrame != 0) {
            DbgOut("Frame %d Registers:\n", Context->CurrentFrame);
        }

        switch (Context->MachineType) {

        //
        // Dump x86 registers.
        //

        case MACHINE_TYPE_X86:
            DbgOut("eax=%08I64x ebx=%08I64x ecx=%08I64x edx=%08I64x "
                   "eip=%08I64x\n"
                   "esi=%08I64x edi=%08I64x ebp=%08I64x esp=%08I64x "
                   "eflags=%08I64x\n",
                   X86Registers->Eax,
                   X86Registers->Ebx,
                   X86Registers->Ecx,
                   X86Registers->Edx,
                   X86Registers->Eip,
                   X86Registers->Esi,
                   X86Registers->Edi,
                   X86Registers->Ebp,
                   X86Registers->Esp,
                   X86Registers->Eflags);

            DbgOut("cs=%04x ds=%04x es=%04x fs=%04x gs=%04x ss=%04x\n",
                   X86Registers->Cs,
                   X86Registers->Ds,
                   X86Registers->Es,
                   X86Registers->Fs,
                   X86Registers->Gs,
                   X86Registers->Ss);

            DbgrpPrintEflags(X86Registers->Eflags);
            DbgOut("\n");
            break;

        case MACHINE_TYPE_X64:
            DbgOut("rax=%016llx rdx=%016llx rcx=%016llx\n"
                   "rbx=%016llx rsi=%016llx rdi=%016llx\n"
                   "r8 =%016llx r9 =%016llx r10=%016llx\n"
                   "r11=%016llx r12=%016llx r13=%016llx\n"
                   "r14=%016llx r15=%016llx rbp=%016llx\n"
                   "rip=%016llx rsp=%016llx\n",
                   X64Registers->Rax,
                   X64Registers->Rdx,
                   X64Registers->Rcx,
                   X64Registers->Rbx,
                   X64Registers->Rsi,
                   X64Registers->Rdi,
                   X64Registers->R8,
                   X64Registers->R9,
                   X64Registers->R10,
                   X64Registers->R11,
                   X64Registers->R12,
                   X64Registers->R13,
                   X64Registers->R14,
                   X64Registers->R15,
                   X64Registers->Rbp,
                   X64Registers->Rip,
                   X64Registers->Rsp);

            DbgOut("cs=%04x ds=%04x es=%04x fs=%04x gs=%04x ss=%04x\n"
                   "rflags=%016llx ",
                   X64Registers->Cs,
                   X64Registers->Ds,
                   X64Registers->Es,
                   X64Registers->Fs,
                   X64Registers->Gs,
                   X64Registers->Ss,
                   X64Registers->Rflags);

            DbgrpPrintEflags(X64Registers->Rflags);
            DbgOut("\n");
            break;

        //
        // Dump ARM registers.
        //

        case MACHINE_TYPE_ARM:
            DbgOut("r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x\n"
                   "r6=%08x r7=%08x r8=%08x r9=%08x r10=%08x fp=%08x\n"
                   "ip=%08x sp=%08x lr=%08x pc=%08x cpsr=%08x\n",
                   ArmRegisters->R0,
                   ArmRegisters->R1,
                   ArmRegisters->R2,
                   ArmRegisters->R3,
                   ArmRegisters->R4,
                   ArmRegisters->R5,
                   ArmRegisters->R6,
                   ArmRegisters->R7,
                   ArmRegisters->R8,
                   ArmRegisters->R9,
                   ArmRegisters->R10,
                   ArmRegisters->R11Fp,
                   ArmRegisters->R12Ip,
                   ArmRegisters->R13Sp,
                   ArmRegisters->R14Lr,
                   ArmRegisters->R15Pc,
                   ArmRegisters->Cpsr);

            DbgOut("Mode: ");
            Psr = ArmRegisters->Cpsr;
            switch (Psr & ARM_MODE_MASK) {
            case ARM_MODE_ABORT:
                DbgOut("Abort");
                break;

            case ARM_MODE_FIQ:
                DbgOut("FIQ");
                break;

            case ARM_MODE_IRQ:
                DbgOut("IRQ");
                break;

            case ARM_MODE_SVC:
                DbgOut("SVC");
                break;

            case ARM_MODE_SYSTEM:
                DbgOut("System");
                break;

            case ARM_MODE_UNDEF:
                DbgOut("Undefined Instruction");
                break;

            case ARM_MODE_USER:
                DbgOut("User");
                break;

            default:
                DbgOut("*** Unknown ***");
                break;
            }

            if ((Psr & PSR_FLAG_NEGATIVE) != 0) {
                DbgOut(" N");
            }

            if ((Psr & PSR_FLAG_ZERO) != 0) {
                DbgOut(" Z");
            }

            if ((Psr & PSR_FLAG_CARRY) != 0) {
                DbgOut(" C");
            }

            if ((Psr & PSR_FLAG_OVERFLOW) != 0) {
                DbgOut(" V");
            }

            if ((Psr & PSR_FLAG_SATURATION) != 0) {
                DbgOut(" Q");
            }

            if ((Psr & PSR_FLAG_JAZELLE) != 0) {
                DbgOut(" Jazelle");
            }

            if ((Psr & PSR_FLAG_THUMB) != 0) {
                DbgOut(" Thumb");
            }

            if ((Psr & PSR_FLAG_FIQ) != 0) {
                DbgOut(" FIQ");
            }

            if ((Psr & PSR_FLAG_IRQ) != 0) {
                DbgOut(" IRQ");
            }

            DbgOut("\n");
            break;

        default:
            DbgOut("Error: Unknown machine type %d.\n", Context->MachineType);
            goto GetSetRegistersEnd;
        }
    }

GetSetRegistersEnd:
    return Result;
}

INT
DbgrGetSetSpecialRegisters (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine prints or modifies the target machine's special registers.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSPECIAL_REGISTERS_UNION Original;
    PVOID Register;
    PULONG Register2;
    ULONG RegisterSize;
    PSTR RegisterString;
    INT Result;
    SET_SPECIAL_REGISTERS SetCommand;
    ULONGLONG Value;
    ULONGLONG Value2;
    PSTR ValueCopy;
    PSTR ValueString;
    PSTR ValueString2;

    Register = NULL;
    RegisterString = NULL;
    Register2 = NULL;
    ValueCopy = NULL;
    ValueString = NULL;
    if (ArgumentCount >= 2) {
        RegisterString = Arguments[1];
        if (ArgumentCount >= 3) {
            ValueString = Arguments[2];
        }
    }

    //
    // Fill in the new registers as if they were the originals so they can be
    // modified in place.
    //

    Original = &(SetCommand.New);
    Result = DbgGetSpecialRegisters(Context, Original);
    if (Result != 0) {
        goto GetSetSpecialRegistersEnd;
    }

    //
    // A specific register is being read or written to.
    //

    if (RegisterString != NULL) {
        switch (Context->MachineType) {
        case MACHINE_TYPE_X86:
        case MACHINE_TYPE_X64:
            RegisterSize = 8;
            if (strcasecmp(RegisterString, "cr0") == 0) {
                Register = &(Original->Ia.Cr0);

            } else if (strcasecmp(RegisterString, "cr2") == 0) {
                Register = &(Original->Ia.Cr2);

            } else if (strcasecmp(RegisterString, "cr3") == 0) {
                Register = &(Original->Ia.Cr3);

            } else if (strcasecmp(RegisterString, "cr4") == 0) {
                Register = &(Original->Ia.Cr4);

            } else if (strcasecmp(RegisterString, "dr0") == 0) {
                Register = &(Original->Ia.Dr0);

            } else if (strcasecmp(RegisterString, "dr1") == 0) {
                Register = &(Original->Ia.Dr1);

            } else if (strcasecmp(RegisterString, "dr2") == 0) {
                Register = &(Original->Ia.Dr2);

            } else if (strcasecmp(RegisterString, "dr3") == 0) {
                Register = &(Original->Ia.Dr3);

            } else if (strcasecmp(RegisterString, "dr6") == 0) {
                Register = &(Original->Ia.Dr6);

            } else if (strcasecmp(RegisterString, "dr7") == 0) {
                Register = &(Original->Ia.Dr7);

            } else if (strcasecmp(RegisterString, "idtr") == 0) {
                RegisterSize = 4;
                Register = &(Original->Ia.Idtr.Base);
                Register2 = &(Original->Ia.Idtr.Limit);

            } else if (strcasecmp(RegisterString, "gdtr") == 0) {
                RegisterSize = 4;
                Register = &(Original->Ia.Gdtr.Base);
                Register2 = &(Original->Ia.Gdtr.Limit);

            } else if (strcasecmp(RegisterString, "tr") == 0) {
                RegisterSize = 2;
                Register = &(Original->Ia.Tr);

            } else {
                DbgOut("Error: Unknown register '%s'.\n", RegisterString);
                Result = EINVAL;
                goto GetSetSpecialRegistersEnd;
            }

            break;

        case MACHINE_TYPE_ARM:
            RegisterSize = 4;
            if (strcasecmp(RegisterString, "sctlr") == 0) {
                Register = &(Original->Arm.Sctlr);

            } else if (strcasecmp(RegisterString, "actlr") == 0) {
                Register = &(Original->Arm.Actlr);

            } else if (strcasecmp(RegisterString, "ttbr0") == 0) {
                Register = &(Original->Arm.Ttbr0);

            } else if (strcasecmp(RegisterString, "ttbr1") == 0) {
                Register = &(Original->Arm.Ttbr1);

            } else if (strcasecmp(RegisterString, "dfsr") == 0) {
                Register = &(Original->Arm.Dfsr);

            } else if (strcasecmp(RegisterString, "ifsr") == 0) {
                Register = &(Original->Arm.Ifsr);

            } else if (strcasecmp(RegisterString, "dfar") == 0) {
                Register = &(Original->Arm.Dfar);

            } else if (strcasecmp(RegisterString, "ifar") == 0) {
                Register = &(Original->Arm.Ifar);

            } else if (strcasecmp(RegisterString, "prrr") == 0) {
                Register = &(Original->Arm.Prrr);

            } else if (strcasecmp(RegisterString, "nmrr") == 0) {
                Register = &(Original->Arm.Nmrr);

            } else if (strcasecmp(RegisterString, "vbar") == 0) {
                Register = &(Original->Arm.Vbar);

            } else if (strcasecmp(RegisterString, "par") == 0) {
                Register = &(Original->Arm.Par);

            } else if (strcasecmp(RegisterString, "ats1cpr") == 0) {
                Register = &(Original->Arm.Ats1Cpr);

            } else if (strcasecmp(RegisterString, "ats1cpw") == 0) {
                Register = &(Original->Arm.Ats1Cpw);

            } else if (strcasecmp(RegisterString, "ats1cur") == 0) {
                Register = &(Original->Arm.Ats1Cur);

            } else if (strcasecmp(RegisterString, "ats1cuw") == 0) {
                Register = &(Original->Arm.Ats1Cuw);

            } else if (strcasecmp(RegisterString, "tpidrprw") == 0) {
                Register = &(Original->Arm.Tpidrprw);

            } else {
                DbgOut("Error: Unknown register '%s'.\n", RegisterString);
                Result = EINVAL;
                goto GetSetSpecialRegistersEnd;
            }

            break;

        default:
            DbgOut("GetSetSpecialRegisters: Unknown architecture.\n");
            Result = EINVAL;
            goto GetSetSpecialRegistersEnd;
        }

        //
        // Set a register.
        //

        if (ValueString != NULL) {
            ValueCopy = strdup(ValueString);
            if (ValueCopy == NULL) {
                Result = ENOMEM;
                goto GetSetSpecialRegistersEnd;
            }

            ValueString2 = strchr(ValueCopy, ',');
            if (ValueString2 != NULL) {
                *ValueString2 = '\0';
                ValueString2 += 1;
            }

            Result = DbgEvaluate(Context, ValueCopy, &Value);
            if (Result != 0) {
                DbgOut("Failed to evaluate '%s'.\n", ValueCopy);
                goto GetSetSpecialRegistersEnd;
            }

            if (ValueString2 != NULL) {
                Result = DbgEvaluate(Context, ValueString2, &Value2);
                if (Result != 0) {
                    DbgOut("Failed to evaluate '%s'.\n", ValueCopy);
                    goto GetSetSpecialRegistersEnd;
                }
            }

            if ((ValueString2 != NULL) && (Register2 == NULL)) {
                DbgOut("Error: %s takes only one argument.\n", RegisterString);
                Result = EINVAL;
                goto GetSetSpecialRegistersEnd;

            } else if ((ValueString2 == NULL) && (Register2 != NULL)) {
                DbgOut("Error: %s takes two arguments (in the form "
                       "'base,limit').\n",
                       RegisterString);

                Result = EINVAL;
                goto GetSetSpecialRegistersEnd;
            }

            //
            // Set the register. Copy the originals to the originals position
            // first.
            //

            memcpy(&(SetCommand.Original),
                   &(SetCommand.New),
                   sizeof(SPECIAL_REGISTERS_UNION));

            memcpy(Register, &Value, RegisterSize);
            if (Register2 != NULL) {
                memcpy(Register2, &Value2, sizeof(ULONG));
            }

            Result = DbgSetSpecialRegisters(Context, &SetCommand);
            if (Result != 0) {
                goto GetSetSpecialRegistersEnd;
            }

        } else {
            Value = 0;
            memcpy(&Value, Register, RegisterSize);
            if (Register2 != NULL) {
                DbgOut("%I64x,%x\n", Value, *Register2);

            } else {
                DbgOut("%I64x\n", Value);
            }
        }

    //
    // Just print all the registers.
    //

    } else {
        switch (Context->MachineType) {
        case MACHINE_TYPE_X86:
        case MACHINE_TYPE_X64:
            DbgOut("cr0=%08I64x cr2=%08I64x cr3=%08I64x cr4=%08I64x tr=%04x\n"
                   "dr0=%08I64x dr1=%08I64x dr2=%08I64x dr3=%08I64x\n"
                   "dr6=%08I64x dr7=%08I64x\n"
                   "idtr=%08x,%04x gdtr=%08x,%04x\n",
                   Original->Ia.Cr0,
                   Original->Ia.Cr2,
                   Original->Ia.Cr3,
                   Original->Ia.Cr4,
                   Original->Ia.Tr,
                   Original->Ia.Dr0,
                   Original->Ia.Dr1,
                   Original->Ia.Dr2,
                   Original->Ia.Dr3,
                   Original->Ia.Dr6,
                   Original->Ia.Dr7,
                   Original->Ia.Idtr.Base,
                   Original->Ia.Idtr.Limit,
                   Original->Ia.Gdtr.Base,
                   Original->Ia.Gdtr.Limit);

            break;

        case MACHINE_TYPE_ARM:
            DbgOut("Not shown: ats1cpr, ats1cpw, ats1cur, ats1cuw\n"
                   "sctlr=%08x actlr=%08x ttbr0=%08I64x ttbr1=%08I64x\n"
                   " dfsr=%08x  dfar=%08I64x  ifsr=%08x  ifar=%08I64x\n"
                   " prrr=%08x  nmrr=%08x  vbar=%08x   par=%08I64x\n"
                   "tpidrprw=%08I64x\n",
                   Original->Arm.Sctlr,
                   Original->Arm.Actlr,
                   Original->Arm.Ttbr0,
                   Original->Arm.Ttbr1,
                   Original->Arm.Dfsr,
                   Original->Arm.Dfar,
                   Original->Arm.Ifsr,
                   Original->Arm.Ifar,
                   Original->Arm.Prrr,
                   Original->Arm.Nmrr,
                   Original->Arm.Vbar,
                   Original->Arm.Par,
                   Original->Arm.Tpidrprw);

            break;

        default:
            DbgOut("GetSetSpecialRegisters: Unknown architecture.\n");
            Result = EINVAL;
            goto GetSetSpecialRegistersEnd;
        }
    }

    Result = 0;

GetSetSpecialRegistersEnd:
    if (ValueCopy != NULL) {
        free(ValueCopy);
    }

    return Result;
}

INT
DbgrPrintCallStack (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine prints the current call stack.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONGLONG BasePointer;
    ULONGLONG InstructionPointer;
    REGISTERS_UNION LocalRegisters;
    BOOL PrintFrameNumbers;
    PREGISTERS_UNION Registers;
    INT Result;
    ULONGLONG StackPointer;

    Registers = NULL;

    assert(Context->CurrentEvent.Type == DebuggerEventBreak);

    if ((ArgumentCount != 1) && (ArgumentCount != 4)) {
        DbgOut("Usage: k [<InstructionPointer> <StackPointer> "
               "<BasePointer>]\n");

        return EINVAL;
    }

    PrintFrameNumbers = FALSE;
    if (strcasecmp(Arguments[0], "kn") == 0) {
        PrintFrameNumbers = TRUE;

    } else {

        assert(strcasecmp(Arguments[0], "k") == 0);
    }

    if (ArgumentCount == 4) {
        RtlCopyMemory(&LocalRegisters,
                      &(Context->CurrentEvent.BreakNotification.Registers),
                      sizeof(REGISTERS_UNION));

        Registers = &LocalRegisters;
        Result = DbgEvaluate(Context, Arguments[1], &InstructionPointer);
        if (Result != 0) {
            DbgOut("Failed to evaluate \"%s\".\n", Arguments[1]);
            goto PrintCallStackEnd;
        }

        Result = DbgEvaluate(Context, Arguments[2], &StackPointer);
        if (Result != 0) {
            DbgOut("Failed to evaluate \"%s\".\n", Arguments[2]);
            goto PrintCallStackEnd;
        }

        Result = DbgEvaluate(Context, Arguments[3], &BasePointer);
        if (Result != 0) {
            DbgOut("Failed to evaluate \"%s\".\n", Arguments[3]);
            goto PrintCallStackEnd;
        }

        switch (Context->MachineType) {
        case MACHINE_TYPE_X86:
            LocalRegisters.X86.Eip = InstructionPointer;
            LocalRegisters.X86.Esp = StackPointer;
            LocalRegisters.X86.Ebp = BasePointer;
            break;

        case MACHINE_TYPE_ARM:
            LocalRegisters.Arm.R15Pc = InstructionPointer;
            LocalRegisters.Arm.R13Sp = StackPointer;
            if ((LocalRegisters.Arm.Cpsr & PSR_FLAG_THUMB) != 0) {
                LocalRegisters.Arm.R7 = BasePointer;

            } else {
                LocalRegisters.Arm.R11Fp = BasePointer;
            }

            break;

        case MACHINE_TYPE_X64:
            LocalRegisters.X64.Rip = InstructionPointer;
            LocalRegisters.X64.Rsp = StackPointer;
            LocalRegisters.X64.Rbp = BasePointer;
            break;
        }
    }

    Result = DbgPrintCallStack(Context, Registers, PrintFrameNumbers);
    if (Result != 0) {
        goto PrintCallStackEnd;
    }

    Result = 0;

PrintCallStackEnd:
    return Result;
}

INT
DbgrSetFrame (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine changes the current stack frame, so that local variables may
    come from a different function in the call stack.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR AfterScan;
    ULONG FrameNumber;
    PSTR FrameNumberString;
    INT Status;

    if (ArgumentCount < 2) {
        DbgOut("Usage: frame <N>\nSets the current call stack frame, where "
               "N is a number between 0 and the number of stack frames (use "
               "kn to dump numbered frames).\n");

        Status = EINVAL;
        goto SetFrameEnd;
    }

    FrameNumberString = Arguments[1];
    FrameNumber = strtoul(FrameNumberString, &AfterScan, 0);
    if (FrameNumberString == AfterScan) {
        DbgOut("Failed to convert '%s' to a number.\n", FrameNumberString);
        Status = EINVAL;
        goto SetFrameEnd;
    }

    Status = DbgrpSetFrame(Context, FrameNumber);

SetFrameEnd:
    return Status;
}

INT
DbgrDisassemble (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine disassembles instructions from the target.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONGLONG ActualAddress;
    ULONG BufferSize;
    ULONG BytesConsumed;
    ULONG BytesRead;
    ULONG InstructionCount;
    PBYTE InstructionStream;
    ULONGLONG NewAddress;
    BOOL Result;

    InstructionCount = DEFAULT_DISASSEMBLED_INSTRUCTIONS;
    BufferSize =  InstructionCount * BYTES_PER_INSTRUCTION;
    InstructionStream = NULL;

    //
    // If an address string was supplied, parse that. Otherwise, print from
    // where disassembly left off.
    //

    if (ArgumentCount >= 2) {
        Result = DbgEvaluate(Context, Arguments[1], &NewAddress);
        if (Result != 0) {
            DbgOut("Error: Unable to parse address '%s'.\n", Arguments[1]);
            goto DisassembleEnd;

        } else {
            Context->DisassemblyAddress = NewAddress;
        }
    }

    //
    // Allocate memory to hold the binary instructions.
    //

    InstructionStream = malloc(BufferSize);
    if (InstructionStream == NULL) {
        Result = ENOMEM;
        goto DisassembleEnd;
    }

    memset(InstructionStream, 0, BufferSize);

    //
    // Read the memory from the target.
    //

    ActualAddress = Context->DisassemblyAddress;
    if (Context->MachineType == MACHINE_TYPE_ARM) {
        ActualAddress &= ~ARM_THUMB_BIT;
    }

    Result = DbgReadMemory(Context,
                           TRUE,
                           ActualAddress,
                           BufferSize,
                           InstructionStream,
                           &BytesRead);

    if (Result != 0) {
        goto DisassembleEnd;
    }

    if (BytesRead < BufferSize) {
        BufferSize = BytesRead;
    }

    //
    // Print out the disassembly and advance the disassembly address.
    //

    Result = DbgrpPrintDisassembly(Context,
                                   InstructionStream,
                                   Context->DisassemblyAddress,
                                   BufferSize / BYTES_PER_INSTRUCTION,
                                   &BytesConsumed);

    if (Result != 0) {
        goto DisassembleEnd;
    }

    Result = 0;
    Context->DisassemblyAddress += BytesConsumed;

DisassembleEnd:
    if (InstructionStream != NULL) {
        free(InstructionStream);
    }

    return Result;
}

INT
DbgrWaitForEvent (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine gets an event from the target, such as a break event or other
    exception.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    0 on success,

    Returns an error code on failure.

--*/

{

    INT Result;

    while ((Context->TargetFlags & DEBUGGER_TARGET_RUNNING) != 0) {
        Result = DbgWaitForEvent(Context);
        if (Result != 0) {
            DbgOut("Error: Failed to get next debugging event.\n");
            return Result;
        }

        switch (Context->CurrentEvent.Type) {
        case DebuggerEventBreak:
            Result = DbgrpProcessBreakNotification(Context);
            if (Result != 0) {
                goto WaitForEventEnd;
            }

            break;

        case DebuggerEventShutdown:
            DbgrpProcessShutdown(Context);
            break;

        case DebuggerEventProfiler:
            DbgrProcessProfilerNotification(Context);
            break;

        default:

            //
            // The target sent an unknown command.
            //

            DbgOut("Unknown event received: 0x%x\n",
                   Context->CurrentEvent.Type);

            break;
        }
    }

    Result = 0;

WaitForEventEnd:
    return Result;
}

INT
DbgrSearchSymbols (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine searches for symbols. Wildcards are accepted. If the search
    string is preceded by "modulename!" then only that module will be searched.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONGLONG Address;
    PDEBUGGER_MODULE CurrentModule;
    PLIST_ENTRY CurrentModuleEntry;
    PSTR ModuleEnd;
    ULONG ModuleLength;
    ULONGLONG Pc;
    INT Result;
    PSYMBOL_SEARCH_RESULT ResultValid;
    SYMBOL_SEARCH_RESULT SearchResult;
    PSTR SearchString;
    PDEBUGGER_MODULE UserModule;

    UserModule = NULL;
    if (ArgumentCount != 2) {
        DbgOut("Usage: x <query>\nThe x command searches for a symbol with "
               "the given name. Wildcards are accepted.");

        Result = EINVAL;
        goto SearchSymbolsEnd;
    }

    SearchString = Arguments[1];

    //
    // If an exclamation point exists, then the module was specified. Find that
    // module.
    //

    ModuleEnd = strchr(SearchString, '!');
    if (ModuleEnd != NULL) {
        ModuleLength = (UINTN)ModuleEnd - (UINTN)SearchString;
        UserModule = DbgpGetModule(Context, SearchString, ModuleLength);
        if (UserModule == NULL) {
            DbgOut("Module %s not found.\n", SearchString);
            Result = ENOENT;
            goto SearchSymbolsEnd;
        }

        //
        // Move the search string and initialize the list entry.
        //

        SearchString = ModuleEnd + 1;
        CurrentModuleEntry = &(UserModule->ListEntry);

    //
    // If a module was not specified, simply start with the first one.
    //

    } else {
        CurrentModuleEntry = Context->ModuleList.ModulesHead.Next;
    }

    //
    // Loop over all modules.
    //

    while (CurrentModuleEntry != &(Context->ModuleList.ModulesHead)) {
        CurrentModule = LIST_VALUE(CurrentModuleEntry,
                                   DEBUGGER_MODULE,
                                   ListEntry);

        CurrentModuleEntry = CurrentModuleEntry->Next;
        if (!IS_MODULE_IN_CURRENT_PROCESS(Context, CurrentModule)) {
            if (UserModule != NULL) {
                break;
            }

            continue;
        }

        //
        // Loop over all symbol search results.
        //

        SearchResult.Variety = SymbolResultInvalid;
        while (TRUE) {

            //
            // Perform the search. If it fails, break out of this loop.
            //

            ResultValid = DbgpFindSymbolInModule(CurrentModule->Symbols,
                                                 SearchString,
                                                 &SearchResult);

            if (ResultValid == NULL) {
                break;
            }

            //
            // Print out the result.
            //

            Result = TRUE;
            switch (SearchResult.Variety) {
            case SymbolResultFunction:
                Address = SearchResult.U.FunctionResult->StartAddress +
                          CurrentModule->BaseDifference;

                DbgPrintFunctionPrototype(SearchResult.U.FunctionResult,
                                          CurrentModule->ModuleName,
                                          Address);

                DbgOut("\n");
                break;

            case SymbolResultData:
                Pc = DbgGetPc(Context, &(Context->FrameRegisters)) -
                     CurrentModule->BaseDifference;

                Result = DbgGetDataSymbolAddress(Context,
                                                 CurrentModule->Symbols,
                                                 SearchResult.U.DataResult,
                                                 Pc,
                                                 &Address);

                if (Result == 0) {
                    DbgOut("%s!%s @ 0x%08x\n",
                           CurrentModule->ModuleName,
                           SearchResult.U.DataResult->Name,
                           Address + CurrentModule->BaseDifference);
                }

                Result = TRUE;
                break;

            case SymbolResultType:
                DbgOut("%s!%s\n",
                       CurrentModule->ModuleName,
                       SearchResult.U.TypeResult->Name);

                break;

            default:
                DbgOut("ERROR: Unknown search result type %d returned!",
                       SearchResult.U.TypeResult);

                Result = EINVAL;
                goto SearchSymbolsEnd;
            }

            if (Result == FALSE) {
                Result = ENOENT;
                goto SearchSymbolsEnd;
            }
        }

        //
        // If a specific user module was specified, do not loop over more
        // modules.
        //

        if (UserModule != NULL) {
            break;
        }
    }

    Result = 0;

SearchSymbolsEnd:
    return Result;
}

INT
DbgrDumpTypeCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine prints information about a type description or value. If only
    a type is specified, the type format will be printed. If an address is
    passed as a second parameter, then the values will be dumped. If a global
    or local variable is passed as the first parameter, the values will also be
    dumped.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INT Result;

    if (ArgumentCount < 2) {
        DbgOut("Usage: dt <type name> [<address...> | <variable name>]\n");
        Result = EINVAL;
        goto DumpTypeCommandEnd;
    }

    Result = DbgrDumpType(Context, Arguments + 1, ArgumentCount - 1, NULL, 0);
    if (Result != 0) {
        goto DumpTypeCommandEnd;
    }

    DbgOut("\n");
    Result = 0;

DumpTypeCommandEnd:
    return Result;
}

INT
DbgrDumpMemory (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine prints the contents of debuggee memory to the screen.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONGLONG Address;
    PSTR Argument;
    ULONG ArgumentIndex;
    ULONGLONG Columns;
    PSTR MemoryType;
    BOOL PrintCharacters;
    INT Status;
    ULONGLONG TotalValues;
    ULONG TypeSize;
    BOOL VirtualAddress;

    Columns = 0;
    PrintCharacters = TRUE;
    TotalValues = 0;
    VirtualAddress = TRUE;
    MemoryType = Arguments[0];

    //
    // Get the type size.
    //

    if (strcasecmp(MemoryType, "db") == 0) {
        TypeSize = 1;

    } else if (strcasecmp(MemoryType, "dc") == 0) {
        TypeSize = 1;

    } else if (strcasecmp(MemoryType, "dw") == 0) {
        TypeSize = 2;

    } else if (strcasecmp(MemoryType, "dd") == 0) {
        TypeSize = 4;

    } else if (strcasecmp(MemoryType, "dq") == 0) {
        TypeSize = 8;

    } else {
        DbgOut("Error: unrecognized command. Valid dump commands are db (byte),"
               " dc (char), dw (word), dd (double-word), dq (quad-word), and "
               "dt (type).\n");

        Status = EINVAL;
        goto DebuggerDumpMemoryEnd;
    }

    //
    // Go through the arguments.
    //

    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];

        assert(Argument != NULL);

        if (Argument[0] == '-') {
            Argument += 1;

            //
            // 'c' specifies the number of columns.
            //

            if (Argument[0] == 'c') {
                Status = DbgEvaluate(Context, Argument + 1, &Columns);
                if (Status != 0) {
                    DbgOut("Error: Invalid column argument \"%s\". The correct "
                           "form looks something like \"c4\".\n",
                           Argument);

                    goto DebuggerDumpMemoryEnd;
                }
            }

            //
            // 'l' specifies the number of values to print.
            //

            if (Argument[0] == 'l') {
                Status = DbgEvaluate(Context, Argument + 1, &TotalValues);
                if (Status != 0) {
                    DbgOut("Error: Invalid total values argument \"%s\". The "
                           "correct form looks something like \"l8\".\n",
                           Argument);

                    goto DebuggerDumpMemoryEnd;
                }
            }

            //
            // 'p' specifies physical addressing.
            //

            if (Argument[0] == 'p') {
                VirtualAddress = FALSE;
            }
        }

        //
        // The last argument is the address to dump.
        //

        if (ArgumentIndex == ArgumentCount - 1) {
            Status = DbgEvaluate(Context, Argument, &Address);
            if (Status != 0) {
                DbgOut("Error: unable to parse address \"%s\".\n",  Argument);
                goto DebuggerDumpMemoryEnd;
            }
        }
    }

    //
    // If the argument count is 0, continue from the previous dump or print the
    // default dump.
    //

    if (ArgumentCount <= 1) {
        Address = Context->LastMemoryDump.NextAddress;
        VirtualAddress = Context->LastMemoryDump.Virtual;
        Columns = Context->LastMemoryDump.Columns;
        TotalValues = Context->LastMemoryDump.TotalValues;
        PrintCharacters = Context->LastMemoryDump.PrintCharacters;

    //
    // Save the current dump parameters.
    //

    } else {
        Context->LastMemoryDump.NextAddress = Address +
                                              (TypeSize * TotalValues);

        Context->LastMemoryDump.Virtual = VirtualAddress;
        Context->LastMemoryDump.Columns = Columns;
        Context->LastMemoryDump.TotalValues = TotalValues;
        Context->LastMemoryDump.PrintCharacters = PrintCharacters;
    }

    //
    // Update the last dump address.
    //

    if (TotalValues == 0) {
        Context->LastMemoryDump.NextAddress += 16 * DEFAULT_MEMORY_PRINT_ROWS;

    } else {
        Context->LastMemoryDump.NextAddress += TotalValues * TypeSize;
    }

    //
    // All the information has been collected. Attempt to print the memory.
    //

    Status = DbgrpPrintMemory(Context,
                              Address,
                              VirtualAddress,
                              TypeSize,
                              Columns,
                              TotalValues,
                              PrintCharacters);

    if (Status != 0) {
        goto DebuggerDumpMemoryEnd;
    }

    Status = 0;

DebuggerDumpMemoryEnd:
    return Status;
}

INT
DbgrDumpList (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine interates over a linked list and prints out the structure
    information for each entry. It also performs basic validation on the list,
    checking for bad previous pointers.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG BytesRead;
    ULONG Count;
    ULONGLONG CurrentAddress;
    ULONG FieldOffset;
    ULONG FieldSize;
    ULONGLONG ListEntry[2];
    PSTR ListEntryName;
    ULONG ListEntrySize;
    ULONGLONG ListHeadAddress;
    PSTR ListHeadAddressString;
    ULONG PointerSize;
    ULONGLONG PreviousAddress;
    PTYPE_SYMBOL ResolvedType;
    BOOL Result;
    SYMBOL_SEARCH_RESULT SearchResult;
    INT Status;
    ULONGLONG StructureAddress;
    PVOID StructureBuffer;
    PDATA_TYPE_STRUCTURE StructureData;
    ULONG StructureSize;
    PSTR TypeNameString;

    Status = EINVAL;

    //
    // Argument validation.
    //

    if (ArgumentCount < 3) {
        DbgOut("Usage: dl <list head address> <type name> "
               "[<list entry name>]\n");

        goto DumpListEnd;
    }

    ListHeadAddressString = Arguments[1];
    TypeNameString = Arguments[2];

    //
    // Evaluate the first argument, converting it to the list head address.
    //

    Status = DbgEvaluate(Context, ListHeadAddressString, &ListHeadAddress);
    if (Status != 0) {
        DbgOut("Error: Could not evaluate address from string %s\n",
               Arguments[0]);

        goto DumpListEnd;
    }

    //
    // Serach through all modules to find the supplied symbol.
    //

    SearchResult.Variety = SymbolResultType;
    Result = DbgpFindSymbol(Context, TypeNameString, &SearchResult);
    if (Result == FALSE) {
        DbgOut("Error: Unknown type name %s\n", TypeNameString);
        goto DumpListEnd;
    }

    //
    // Validate that the given symbol is a structure. It must at least be a
    // type.
    //

    if (SearchResult.Variety != SymbolResultType) {
        DbgOut("Error: %s is not a structure.\n", TypeNameString);
        goto DumpListEnd;
    }

    //
    // If the symbol is a relation type, then test to see if it resolves to a
    // structure type.
    //

    if (SearchResult.U.TypeResult->Type == DataTypeRelation) {
        ResolvedType = DbgSkipTypedefs(SearchResult.U.TypeResult);
        if ((ResolvedType == NULL) ||
            (ResolvedType->Type != DataTypeStructure)) {

            DbgOut("Error: %s could not be resolved as a structure.\n",
                   TypeNameString);

            goto DumpListEnd;
        }

    //
    // If the symbol is not a structure type, then this is an error.
    //

    } else if (SearchResult.U.TypeResult->Type != DataTypeStructure) {
        DbgOut("Error: %s is not a structure.\n", TypeNameString);
        goto DumpListEnd;

    } else {
        ResolvedType = SearchResult.U.TypeResult;
    }

    //
    // If the list entry name is not supplied, assume the field is called
    // "ListEntry".
    //

    ListEntryName = "ListEntry";
    if (ArgumentCount > 3) {
        ListEntryName = Arguments[3];
    }

    //
    // Get the offset and size of the list entry field.
    //

    Status = DbgGetMemberOffset(ResolvedType,
                                ListEntryName,
                                &FieldOffset,
                                &FieldSize);

    if (Status != 0) {
        DbgOut("Error: Unknown structure member %s\n", ListEntryName);
        goto DumpListEnd;
    }

    if ((FieldOffset % BITS_PER_BYTE) != 0) {
        DbgOut("Error: Structure member %s is not byte align\n", ListEntryName);
        goto DumpListEnd;
    }

    //
    // Read the Next and Previous pointers from the list head.
    //

    PointerSize = DbgGetTargetPointerSize(Context);
    ListEntrySize = PointerSize * 2;
    Status = DbgReadMemory(Context,
                           TRUE,
                           ListHeadAddress,
                           ListEntrySize,
                           ListEntry,
                           &BytesRead);

    if ((Status != 0) || (BytesRead != ListEntrySize)) {
        if (Status == 0) {
            Status = EINVAL;
        }

        DbgOut("Error: Unable to read data at address 0x%I64x\n",
               ListHeadAddress);

        goto DumpListEnd;
    }

    //
    // If the target's pointer size is 32-bits modify the ListEntry array to
    // hold a list pointer in each index.
    //

    if (PointerSize == sizeof(ULONG)) {
        ListEntry[1] = ListEntry[0] >> (sizeof(ULONG) * BITS_PER_BYTE);
        ListEntry[0] &= MAX_ULONG;
    }

    //
    // If the list is empty validate the Previous pointer and exit.
    //

    if (ListEntry[0] == ListHeadAddress) {
        DbgOut("Empty List\n");
        if (ListEntry[1] != ListHeadAddress) {
            DbgOut("Error: Corrupted empty list head Previous.\n"
                   "\tExpected Value: 0x%I64x\n"
                   "\tActual Value: 0x%I64x\n",
                   ListHeadAddress,
                   ListEntry[1]);
        }

        goto DumpListEnd;
    }

    //
    // Get the given data structure's size and allocate a buffer for reading
    // each structure entry in the list.
    //

    StructureData = &(ResolvedType->U.Structure);
    StructureSize = StructureData->SizeInBytes;
    StructureBuffer = malloc(StructureSize);
    if (StructureBuffer == NULL) {
        DbgOut("Error: Failed to allocate %d bytes\n", StructureSize);
        goto DumpListEnd;
    }

    //
    // Loop through the list, printing each element.
    //

    Count = 0;
    CurrentAddress = ListEntry[0];
    PreviousAddress = ListHeadAddress;
    while (CurrentAddress != ListHeadAddress) {
        if (CurrentAddress == (UINTN)NULL) {
            DbgOut("Error: Found NULL list entry Next pointer\n");
            break;
        }

        //
        // Calculate the structure's base pointer and read it from memory.
        //

        StructureAddress = CurrentAddress - (FieldOffset / BITS_PER_BYTE);
        Status = DbgReadMemory(Context,
                               TRUE,
                               StructureAddress,
                               StructureSize,
                               StructureBuffer,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != StructureSize)) {
            if (Status == 0) {
                Status = EINVAL;
            }

            DbgOut("Error: Unable to read %d bytes at address 0x%I64x\n",
                   StructureSize,
                   StructureAddress);

            goto DumpListEnd;
        }

        //
        // Print the structure's contents.
        //

        DbgOut("----------------------------------------\n");
        DbgOut("List Entry %d at address 0x%I64x\n", Count, StructureAddress);
        DbgOut("----------------------------------------\n");
        DbgPrintType(Context,
                     ResolvedType,
                     StructureBuffer,
                     StructureSize,
                     1,
                     DEFAULT_RECURSION_DEPTH);

        DbgOut("\n");

        //
        // Read the current structure's list entry data.
        //

        Status = DbgReadMemory(Context,
                               TRUE,
                               CurrentAddress,
                               ListEntrySize,
                               ListEntry,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != ListEntrySize)) {
            if (Status == 0) {
                Status = EINVAL;
            }

            DbgOut("Error: Unable to read data at address 0x%I64x\n",
                   CurrentAddress);

            goto DumpListEnd;
        }

        //
        // Perform the same pointer magic as we did above for the head.
        //

        if (PointerSize == sizeof(ULONG)) {
            ListEntry[1] = ListEntry[0] >> (sizeof(ULONG) * BITS_PER_BYTE);
            ListEntry[0] &= MAX_ULONG;
        }

        //
        // Validate that the current list entry's Previous field points to the
        // previous element in the list.
        //

        if (PreviousAddress != ListEntry[1]) {
            DbgOut("Error: Corrupted previous pointer:\n"
                   "\tExpected Value: 0x%I64x\n"
                   "\tActual Value: 0x%I64x\n",
                   PreviousAddress,
                   ListEntry[1]);

            goto DumpListEnd;
        }

        PreviousAddress = CurrentAddress;
        CurrentAddress = ListEntry[0];
        Count += 1;
    }

    Status = 0;

DumpListEnd:
    return Status;
}

INT
DbgrEditMemory (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine writes to the target memory space.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONGLONG Address;
    PSTR Argument;
    ULONG ArgumentIndex;
    ULONG BufferSize;
    ULONG BytesWritten;
    PVOID CurrentValue;
    PVOID DataBuffer;
    PSTR MemoryType;
    INT Status;
    ULONG TypeSize;
    ULONGLONG Value;
    BOOL VirtualAddress;

    Address = 0;
    DataBuffer = NULL;
    MemoryType = Arguments[0];
    VirtualAddress = TRUE;

    //
    // Get the type size.
    //

    if (strcasecmp(MemoryType, "eb") == 0) {
        TypeSize = 1;

    } else if (strcasecmp(MemoryType, "ew") == 0) {
        TypeSize = 2;

    } else if (strcasecmp(MemoryType, "ed") == 0) {
        TypeSize = 4;

    } else if (strcasecmp(MemoryType, "eq") == 0) {
        TypeSize = 8;

    } else {
        DbgOut("Error: unrecognized command. Valid edit commands are eb "
               "(byte), ew (word), ed (double-word), and eq (quad-word).\n");

        Status = EINVAL;
        goto DebuggerEditMemoryEnd;
    }

    //
    // Go through the argument options.
    //

    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];

        assert(Argument != NULL);

        //
        // 'p' specifies physical addressing.
        //

        if ((Argument[0] == 'p') && (Argument[1] == '\0')) {
            VirtualAddress = FALSE;
            continue;
        }

        break;
    }

    //
    // The next argument is the address to edit.
    //

    if (ArgumentIndex == ArgumentCount) {
        DbgOut("Error: Not enough arguments.\n");
        Status = EINVAL;
        goto DebuggerEditMemoryEnd;
    }

    Status = DbgEvaluate(Context, Arguments[ArgumentIndex], &Address);
    if (Status != 0) {
        DbgOut("Error: unable to parse address \"%s\".\n",
               Arguments[ArgumentIndex]);

        goto DebuggerEditMemoryEnd;
    }

    ArgumentIndex += 1;
    if (ArgumentIndex == ArgumentCount) {
        DbgOut("Error: Not enough arguments!\n");
        Status = EINVAL;
        goto DebuggerEditMemoryEnd;
    }

    //
    // All other arguments are values to write, sequentially. Start by
    // allocating space for the data to be written.
    //

    BufferSize = (ArgumentCount - ArgumentIndex) * TypeSize;
    DataBuffer = malloc(BufferSize);
    if (DataBuffer == NULL) {
        DbgOut("Error: Unable to allocate %d bytes.\n", BufferSize);
        Status = ENOMEM;
        goto DebuggerEditMemoryEnd;
    }

    CurrentValue = DataBuffer;
    for (NOTHING; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Status = DbgEvaluate(Context, Arguments[ArgumentIndex], &Value);
        if (Status != 0) {
            DbgOut("Error: Unable to parse value \"%s\".\n",
                   Arguments[ArgumentIndex]);

            goto DebuggerEditMemoryEnd;
        }

        memcpy(CurrentValue, &Value, TypeSize);
        CurrentValue += TypeSize;
    }

    //
    // Attempt to write the values to memory.
    //

    Status = DbgWriteMemory(Context,
                            VirtualAddress,
                            Address,
                            BufferSize,
                            DataBuffer,
                            &BytesWritten);

    if (Status != 0) {
        goto DebuggerEditMemoryEnd;
    }

    if (BytesWritten != BufferSize) {
        DbgOut("Only %d of %d bytes written.\n", BytesWritten, BufferSize);
    }

    Context->LastMemoryDump.NextAddress = Address;
    Context->LastMemoryDump.Virtual = VirtualAddress;
    Status = 0;

DebuggerEditMemoryEnd:
    if (DataBuffer != NULL) {
        free(DataBuffer);
    }

    return Status;
}

INT
DbgrEvaluate (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine evaluates a numerical expression and prints it out in both
    decimal and hexadecimal.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONGLONG Result;
    BOOL Status;

    if (ArgumentCount < 2) {
        DbgOut("Usage: %s <expr>.\nExpressions can be numeric (3+4) or \n"
               "symbolic (DbgSymbolTable+(0x10*4)).\n",
               Arguments[0]);

        return EINVAL;
    }

    Status = DbgEvaluate(Context, Arguments[1], &Result);
    if (Status != 0) {
        DbgOut("Syntax error in expression.\n");
        goto EvaluateEnd;
    }

    DbgOut(" 0x%I64x = %I64d\n", Result, Result);
    Status = 0;

EvaluateEnd:
    return Status;
}

INT
DbgrPrintLocals (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine prints the values of the local variables inside the currently
    selected stack frame.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PDATA_SYMBOL BestLocal;
    PDATA_SYMBOL CurrentLocal;
    PLIST_ENTRY CurrentLocalEntry;
    PFUNCTION_SYMBOL Function;
    ULONGLONG InstructionPointer;
    PDEBUGGER_MODULE Module;
    BOOL ParameterPrinted;
    PSYMBOL_SEARCH_RESULT ResultValid;
    SYMBOL_SEARCH_RESULT SearchResult;
    INT Status;

    //
    // Attempt to get the module this address is in. If one cannot be found,
    // then there is no useful information to print, so exit.
    //

    InstructionPointer = DbgGetPc(Context, &(Context->FrameRegisters));
    Module = DbgpFindModuleFromAddress(Context,
                                       InstructionPointer,
                                       &InstructionPointer);

    if (Module == NULL) {
        DbgOut("Error: Execution is not in any module!\n");
        Status = ENOENT;
        goto PrintLocalsEnd;
    }

    //
    // Attempt to find the current function symbol in the module.
    //

    SearchResult.Variety = SymbolResultInvalid;
    ResultValid = NULL;
    if (Module->Symbols != NULL) {
        ResultValid = DbgFindFunctionSymbol(Module->Symbols,
                                            NULL,
                                            InstructionPointer,
                                            &SearchResult);

    } else {
        DbgOut("Error: Module %s has no symbols loaded for it!\n",
               Module->ModuleName);

        Status = ESRCH;
        goto PrintLocalsEnd;
    }

    //
    // If a function could not be found, bail.
    //

    if ((ResultValid == NULL) ||
        (SearchResult.Variety != SymbolResultFunction)) {

        DbgOut("Error: Function symbol could not be found in module %s!\n",
               Module->ModuleName);

        Status = ENOENT;
        goto PrintLocalsEnd;
    }

    Function = SearchResult.U.FunctionResult;

    //
    // Print all function parameters.
    //

    ParameterPrinted = FALSE;
    CurrentLocalEntry = Function->ParametersHead.Next;
    while (CurrentLocalEntry != &(Function->ParametersHead)) {
        CurrentLocal = LIST_VALUE(CurrentLocalEntry, DATA_SYMBOL, ListEntry);
        CurrentLocalEntry = CurrentLocalEntry->Next;
        Status = DbgPrintDataSymbol(Context,
                                    Module->Symbols,
                                    CurrentLocal,
                                    InstructionPointer,
                                    4,
                                    DEFAULT_RECURSION_DEPTH);

        if (Status != ENOENT) {
            if (Status == 0) {
                ParameterPrinted = TRUE;
            }

            DbgOut("\n");
        }
    }

    if (ParameterPrinted != FALSE) {
        DbgOut("\n");
    }

    //
    // Loop through every local in the function.
    //

    CurrentLocalEntry = Function->LocalsHead.Next;
    while (CurrentLocalEntry != &(Function->LocalsHead)) {
        CurrentLocal = LIST_VALUE(CurrentLocalEntry, DATA_SYMBOL, ListEntry);
        CurrentLocalEntry = CurrentLocalEntry->Next;
        if (CurrentLocal->MinimumValidExecutionAddress != 0) {

            //
            // Skip this local if it's not yet valid.
            //

            if (InstructionPointer <
                CurrentLocal->MinimumValidExecutionAddress) {

                continue;
            }

            //
            // Attempt to find the most updated version of this local. Skip
            // this one if a different local is determined to be the most up to
            // date.
            //

            BestLocal = DbgpGetLocal(Function,
                                     CurrentLocal->Name,
                                     InstructionPointer);

            //
            // The function should definitely not fail to find any local, since
            // this function found it.
            //

            assert(BestLocal != NULL);

            if (BestLocal != CurrentLocal) {
                continue;
            }
        }

        //
        // Print out this local.
        //

        Status = DbgPrintDataSymbol(Context,
                                    Module->Symbols,
                                    CurrentLocal,
                                    InstructionPointer,
                                    4,
                                    DEFAULT_RECURSION_DEPTH);

        if (Status != ENOENT) {
            DbgOut("\n");
        }
    }

    Status = 0;

PrintLocalsEnd:
    return Status;
}

INT
DbgrShowSourceAtAddressCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine shows the source file for the provided address and highlights
    the specific line associated with the address.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONGLONG Address;
    PSTR AddressString;
    INT Result;

    if (ArgumentCount != 2) {
        DbgOut("Usage: so <address>.\nThis command displays the current "
               "source file and line for the given address.\n");

        return EINVAL;
    }

    AddressString = Arguments[1];
    Result = DbgEvaluate(Context, AddressString, &Address);
    if (Result != 0) {
        DbgOut("Error: Unable to parse address %s.\n", Address);
        goto ShowSourceAtAddressEnd;
    }

    DbgrShowSourceAtAddress(Context, Address);
    Result = 0;

ShowSourceAtAddressEnd:
    return Result;
}

VOID
DbgrUnhighlightCurrentLine (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine restores the currently executing line to the normal background
    color in the source window.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    //
    // Remove the highlight on the previous line.
    //

    DbgrpHighlightExecutingLine(Context, 0);
    return;
}

INT
DbgrListBreakPoints (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine lists all valid breakpoints in the target.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PDEBUGGER_BREAK_POINT Breakpoint;
    PLIST_ENTRY CurrentEntry;
    INT Status;

    DbgOut("Breakpoints: \n");
    if (LIST_EMPTY(&(Context->BreakpointList)) != FALSE) {
        DbgOut("(None)\n");
        return 0;
    }

    //
    // Loop and print every breakpoint.
    //

    CurrentEntry = Context->BreakpointList.Next;
    while (CurrentEntry != &(Context->BreakpointList)) {
        Breakpoint = LIST_VALUE(CurrentEntry, DEBUGGER_BREAK_POINT, ListEntry);

        //
        // Check that this is a valid entry.
        //

        if (Breakpoint->Type == BreakpointTypeInvalid) {
            DbgOut("Error: Invalid breakpoint type!\n");
            Status = EINVAL;
            goto ListBreakPointsEnd;
        }

        //
        // Print the breakpoint index and whether or not the breakpoint is
        // disabled.
        //

        DbgOut("%d: ", Breakpoint->Index);
        if (Breakpoint->Enabled == FALSE) {
            DbgOut("(Disabled) ");
        }

        //
        // Print the breakpoint address, with symbol information if possible.
        //

        DbgOut("%08I64x ", Breakpoint->Address);
        Status = DbgPrintAddressSymbol(Context, Breakpoint->Address);
        if (Status == 0) {
            DbgOut(" ");
        }

        //
        // If it's a break on access, print out the access type and size.
        //

        if (Breakpoint->Type == BreakpointTypeRead) {
            DbgOut("Read ");

        } else if (Breakpoint->Type == BreakpointTypeWrite) {
            DbgOut("Write ");

        } else if (Breakpoint->Type == BreakpointTypeReadWrite) {
            DbgOut("Read/Write ");
        }

        if ((Breakpoint->Type == BreakpointTypeRead) ||
            (Breakpoint->Type == BreakpointTypeWrite) ||
            (Breakpoint->Type == BreakpointTypeReadWrite)) {

            DbgOut("%d Bytes", Breakpoint->AccessSize);
        }

        //
        // Advance to the next breakpoint.
        //

        DbgOut("\n");
        CurrentEntry = CurrentEntry->Next;
    }

    Status = 0;

ListBreakPointsEnd:
    return Status;
}

INT
DbgrEnableBreakPoint (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine lists all valid breakpoints in the target.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR AfterScan;
    BOOL Enable;
    LONG Number;
    PSTR NumberString;
    INT Status;

    if (ArgumentCount < 2) {
        DbgOut("Usage: %s <N>\nEnable or disable the break point with the "
               "given number N. Use bl to list all breakpoints.\n",
               Arguments[0]);

        Status = EINVAL;
        goto EnableBreakPointEnd;
    }

    Enable = FALSE;
    if (strcasecmp(Arguments[0], "be") == 0) {
        Enable = TRUE;

    } else {

        assert(strcasecmp(Arguments[0], "bd") == 0);
    }

    NumberString = Arguments[1];

    //
    // A star specifies all breakpoints.
    //

    if (strcmp(NumberString, "*") == 0) {
        Number = -1;

    } else {
        Number = strtol(NumberString, &AfterScan, 0);
        if (AfterScan == NumberString) {
            DbgOut("Failed to convert '%s' into a number.\n", NumberString);
            Status = EINVAL;
            goto EnableBreakPointEnd;
        }
    }

    Status = DbgrpEnableBreakPoint(Context, Number, Enable);
    if (Status != 0) {
        goto EnableBreakPointEnd;
    }

EnableBreakPointEnd:
    return Status;
}

INT
DbgrDeleteBreakPoint (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine deletes a breakpoint from the target.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR AfterScan;
    PDEBUGGER_BREAK_POINT Breakpoint;
    PLIST_ENTRY CurrentEntry;
    BOOL Found;
    LONG Number;
    PSTR NumberString;
    INT Status;

    if (ArgumentCount < 2) {
        DbgOut("Usage: %s <N>\nDelete a breakpoint with the given number N. "
               "Use * for all breakpoints. Use bl to list all breakpoints.\n",
               Arguments[0]);

        Status = EINVAL;
        goto DeleteBreakPointEnd;
    }

    NumberString = Arguments[1];

    //
    // A star specifies all breakpoints.
    //

    if (strcmp(NumberString, "*") == 0) {
        Number = -1;

    } else {
        Number = strtol(NumberString, &AfterScan, 0);
        if (AfterScan == NumberString) {
            DbgOut("Failed to convert '%s' into a number.\n", NumberString);
            Status = EINVAL;
            goto DeleteBreakPointEnd;
        }
    }

    //
    // Loop through looking for the breakpoint in the list.
    //

    Found = FALSE;
    CurrentEntry = Context->BreakpointList.Next;
    while (CurrentEntry != &(Context->BreakpointList)) {
        Breakpoint = LIST_VALUE(CurrentEntry, DEBUGGER_BREAK_POINT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((Breakpoint->Index == Number) || (Number == -1)) {
            Found = TRUE;
            if (Context->BreakpointToRestore == Breakpoint) {
                Context->BreakpointToRestore = NULL;
            }

            if (Breakpoint->Enabled != FALSE) {
                DbgrpClearBreakpointAtAddress(Context,
                                              Breakpoint->Address,
                                              Breakpoint->OriginalValue);
            }

            LIST_REMOVE(&(Breakpoint->ListEntry));
            free(Breakpoint);
            if (Number != -1) {
                break;
            }
        }
    }

    if (Found == FALSE) {
        DbgOut("Breakpoint %d not found.\n", Number);
        Status = ESRCH;
        goto DeleteBreakPointEnd;
    }

    Status = 0;

DeleteBreakPointEnd:
    return Status;
}

INT
DbgrCreateBreakPoint (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine creates a new breakpoint in the debuggee.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR AccessType;
    PDEBUGGER_BREAK_POINT Breakpoint;
    PSTR BreakPointAddress;
    PDEBUGGER_BREAK_POINT BreakpointAfter;
    PDEBUGGER_BREAK_POINT CurrentBreakpoint;
    PLIST_ENTRY CurrentEntry;
    LONG Index;
    INT Status;

    AccessType = NULL;
    Breakpoint = NULL;
    if ((ArgumentCount <= 1) || (ArgumentCount > 3)) {
        DbgOut("Usage: bp [<access>] <address>.\n"
               "Set a new breakpoint. The access takes the form "
               "<type><width>, where type is 'r' for read, 'w' for write, or "
               "'x' for execute, and width is 1, 2, 4, or 8. The address is "
               "where to set the breakpoint.\n"
               "Example: \"bp w2 0x1004\" -- Breaks in when a two-byte write "
               "occurs to address 0x1004. If no access type is specified, a "
               "regular software execution breakpoint is created.\n");

        Status = EINVAL;
        goto CreateBreakPointEnd;
    }

    if (ArgumentCount > 2) {
        AccessType = Arguments[1];
        BreakPointAddress = Arguments[2];

    } else {

        assert(ArgumentCount == 2);

        BreakPointAddress = Arguments[1];
    }

    Breakpoint = malloc(sizeof(DEBUGGER_BREAK_POINT));
    if (Breakpoint == NULL) {
        DbgOut("Error: Failed to allocate space for a breakpoint.\n");
        Status = ENOMEM;
        goto CreateBreakPointEnd;
    }

    RtlZeroMemory(Breakpoint, sizeof(DEBUGGER_BREAK_POINT));

    //
    // Parse the access type.
    //

    if (AccessType != NULL) {

        //
        // It's a break on read or break on read/write.
        //

        if (*AccessType == 'r') {
            AccessType += 1;
            if (*AccessType == 'w') {
                AccessType += 1;
                Breakpoint->Type = BreakpointTypeReadWrite;

            } else {
                Breakpoint->Type = BreakpointTypeRead;
            }

        //
        // It's a break on write.
        //

        } else if (*AccessType == 'w') {
            AccessType += 1;
            Breakpoint->Type = BreakpointTypeWrite;

        //
        // It's an invalid specification.
        //

        } else {
            DbgOut("Error: Invalid access type specified. Valid values are "
                   "r, w, and rw, but not %c.\n",
                   *AccessType);

            Status = EINVAL;
            goto CreateBreakPointEnd;
        }

        //
        // Get the access size.
        //

        Breakpoint->AccessSize = strtoul(AccessType, NULL, 10);

        //
        // Check the validity of the result.
        //

        if ((Breakpoint->AccessSize != 1) &&
            (Breakpoint->AccessSize != 2) &&
            (Breakpoint->AccessSize != 4) &&
            (Breakpoint->AccessSize != 8) &&
            (Breakpoint->AccessSize != 16)) {

            DbgOut("Error: Invalid access size specified. Valid values are "
                   "1, 2, 4, 8, and 16.\n");

            Status = EINVAL;
            goto CreateBreakPointEnd;
        }

    //
    // The access type parameter was NULL, so this must be a standard execution
    // breakpoint.
    //

    } else {
        Breakpoint->Type = BreakpointTypeExecution;
    }

    //
    // Parse the address parameter.
    //

    Status = DbgEvaluate(Context, BreakPointAddress, &(Breakpoint->Address));
    if (Status != 0) {
        DbgOut("Error: Unable to parse breakpoint address.\n");
        goto CreateBreakPointEnd;
    }

    //
    // TODO: Enable hardware breakpoints.
    //

    if (Breakpoint->Type != BreakpointTypeExecution) {
        DbgOut("Error: Break on access is currently not implemented.\n");
        Status = ENOSYS;
        goto CreateBreakPointEnd;
    }

    //
    // Loop through once and ensure there's not the same breakpoint already in
    // there (for software breakpoints only).
    //

    if (Breakpoint->Type == BreakpointTypeExecution) {
        CurrentEntry = Context->BreakpointList.Next;
        Index = 0;
        while (CurrentEntry != &(Context->BreakpointList)) {
            CurrentBreakpoint = LIST_VALUE(CurrentEntry,
                                           DEBUGGER_BREAK_POINT,
                                           ListEntry);

            CurrentEntry = CurrentEntry->Next;
            if ((CurrentBreakpoint->Type == Breakpoint->Type) &&
                (CurrentBreakpoint->Address == Breakpoint->Address)) {

                //
                // If the existing breakpoint is currently disabled, enable it.
                //

                if (CurrentBreakpoint->Enabled == FALSE) {
                    Status = DbgrpEnableBreakPoint(Context,
                                                   CurrentBreakpoint->Index,
                                                   TRUE);

                    if (Status != 0) {
                        DbgOut("Error: Failed to re-enable existing breakpoint "
                               "%d at %I64x.\n",
                               CurrentBreakpoint->Index,
                               CurrentBreakpoint->Address);

                        goto CreateBreakPointEnd;
                    }
                }

                Status = 0;
                goto CreateBreakPointEnd;
            }
        }
    }

    //
    // Find an index and location in the list for this breakpoint. The list is
    // always in sorted order by index.
    //

    BreakpointAfter = NULL;
    CurrentEntry = Context->BreakpointList.Next;
    Index = 0;
    while (CurrentEntry != &(Context->BreakpointList)) {
        BreakpointAfter = LIST_VALUE(CurrentEntry,
                                     DEBUGGER_BREAK_POINT,
                                     ListEntry);

        //
        // If the entry here is bigger than the index, then a free slot was
        // found.
        //

        if (BreakpointAfter->Index > Index) {
            break;
        }

        //
        // The index must be equal to this entrie's index. Move up to the next
        // slot.
        //

        Index += 1;
        CurrentEntry = CurrentEntry->Next;
    }

    //
    // If the list is empty or entirely in order, then just put this one at
    // the back of the list.
    //

    Breakpoint->Index = Index;
    if (CurrentEntry == &(Context->BreakpointList)) {
        INSERT_BEFORE(&(Breakpoint->ListEntry), &(Context->BreakpointList));

    } else {
        INSERT_BEFORE(&(Breakpoint->ListEntry), &(BreakpointAfter->ListEntry));
    }

    Breakpoint->Enabled = FALSE;
    DbgrpEnableBreakPoint(Context, Breakpoint->Index, TRUE);
    Breakpoint = NULL;
    Status = 0;

CreateBreakPointEnd:
    if (Breakpoint != NULL) {
        free(Breakpoint);
    }

    return Status;
}

INT
DbgrStep (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine performs a source or assembly line step in the debugger.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONGLONG BaseDifference;
    PFUNCTION_SYMBOL CurrentFunction;
    PDEBUGGER_MODULE CurrentModule;
    PSOURCE_FILE_SYMBOL CurrentSource;
    ULONGLONG DebasedInstructionPointer;
    ULONGLONG FunctionEndAddress;
    SYMBOL_SEARCH_RESULT FunctionSearch;
    ULONGLONG InstructionPointer;
    ULONGLONG LineEndAddress;
    RANGE_STEP RangeStep;
    INT Result;
    PSYMBOL_SEARCH_RESULT ResultValid;
    PSOURCE_LINE_SYMBOL SourceLine;
    BOOL StepInto;

    BaseDifference = 0;
    CurrentFunction = NULL;
    CurrentSource = NULL;
    FunctionEndAddress = 0;
    InstructionPointer =
                    Context->CurrentEvent.BreakNotification.InstructionPointer;

    DebasedInstructionPointer = InstructionPointer;
    SourceLine = NULL;
    StepInto = FALSE;
    if (strcasecmp(Arguments[0], "t") == 0) {
        StepInto = TRUE;

    } else {

        assert(strcasecmp(Arguments[0], "p") == 0);
    }

    //
    // Attempt to get the currently executing source line and function.
    //

    CurrentModule = DbgpFindModuleFromAddress(Context,
                                              InstructionPointer,
                                              &DebasedInstructionPointer);

    if (CurrentModule != NULL) {
        BaseDifference = CurrentModule->BaseDifference;
        SourceLine = DbgLookupSourceLine(CurrentModule->Symbols,
                                         DebasedInstructionPointer);

        if (SourceLine != NULL) {
            CurrentSource = SourceLine->ParentSource;
        }

        FunctionSearch.Variety = SymbolResultInvalid;
        ResultValid = DbgFindFunctionSymbol(CurrentModule->Symbols,
                                            NULL,
                                            DebasedInstructionPointer,
                                            &FunctionSearch);

        if (ResultValid != NULL) {

            assert(FunctionSearch.Variety == SymbolResultFunction);

            CurrentFunction = FunctionSearch.U.FunctionResult;
        }
    }

    //
    // If the source line or current function could not be found, or source
    // stepping is disabled, fall back to stepping over the current instruction.
    //

    if ((SourceLine == NULL) ||
        ((Context->Flags & DEBUGGER_FLAG_SOURCE_LINE_STEPPING) == 0)) {

        //
        // If stepping into, just execute a single step.
        //

        if (StepInto != FALSE) {
            Result = DbgrSingleStep(Context);

        //
        // Attempt to step over one instruction. Symbols will be helpful here.
        // Without them, this basically does a single step.
        //

        } else {

            //
            // Start with a default value that basically represents a single
            // step.
            //

            RangeStep.BreakRangeMinimum = 0;
            RangeStep.BreakRangeMaximum = MAX_ULONGLONG;

            //
            // If there is a current function symbol, then set the range to
            // break anywhere in this function, unless this is the last
            // instruction in the function.
            //

            if (CurrentFunction != NULL) {
                if ((DebasedInstructionPointer +
                     Context->BreakInstructionLength) <
                    CurrentFunction->EndAddress) {

                    RangeStep.BreakRangeMinimum =
                                CurrentFunction->StartAddress + BaseDifference;

                    RangeStep.BreakRangeMaximum =
                                  CurrentFunction->EndAddress + BaseDifference;
                }

            //
            // There's not a function symbol, so check to see if there's at
            // least a source symbol. If there is, set the range to break
            // anywhere in this file.
            //

            } else if (CurrentSource != NULL) {
                if ((DebasedInstructionPointer +
                     Context->BreakInstructionLength) <
                    CurrentSource->EndAddress) {

                    RangeStep.BreakRangeMinimum =
                                  CurrentSource->StartAddress + BaseDifference;

                    RangeStep.BreakRangeMaximum =
                                    CurrentSource->EndAddress + BaseDifference;
                }
            }

            RangeStep.RangeHoleMinimum = InstructionPointer;
            RangeStep.RangeHoleMaximum = InstructionPointer + 1;
            Result = DbgrpRangeStep(Context, &RangeStep);
        }

        goto StepEnd;
    }

    //
    // Set a "range" breakpoint, which essentially puts the debuggee into single
    // step mode. The debuggee will break when it is inside the break range (ie
    // the current function), but not inside the range hole (ie the current
    // source line). Start by getting the addresses of the beginning and end of
    // the source line.
    //

    if ((CurrentFunction == NULL) || (SourceLine == NULL)) {
        LineEndAddress = 0;
        RangeStep.RangeHoleMinimum = 0;
        RangeStep.RangeHoleMaximum = 0;

    } else {
        LineEndAddress = SourceLine->End + BaseDifference;
        RangeStep.RangeHoleMinimum = SourceLine->Start + BaseDifference;
        RangeStep.RangeHoleMaximum = LineEndAddress;
    }

    //
    // If stepping into the source line or this is the last line of the
    // function (ie it's about to return), just set the break range to be the
    // entire address space.
    //

    if (CurrentFunction != NULL) {
        FunctionEndAddress = CurrentFunction->EndAddress + BaseDifference;
    }

    if ((StepInto != FALSE) ||
        (CurrentFunction == NULL) ||
        (LineEndAddress == FunctionEndAddress)) {

        RangeStep.BreakRangeMinimum = 0;
        RangeStep.BreakRangeMaximum = MAX_ULONGLONG;

    //
    // The command was step over and it's not the last line of the function, so
    // only break anywhere in the function.
    //

    } else {
        RangeStep.BreakRangeMinimum =
                                CurrentFunction->StartAddress + BaseDifference;

        RangeStep.BreakRangeMaximum = FunctionEndAddress;
    }

    Result = DbgrpRangeStep(Context, &RangeStep);

StepEnd:
    return Result;
}

INT
DbgrSetSourceStepping (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine turns source line stepping on or off.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR Argument;

    if (ArgumentCount != 2) {
        DbgOut("Error: Use \"ss on\" or \"ss off\" to enable or disable source "
               "line stepping.\n");

        return EINVAL;
    }

    Argument = Arguments[1];
    if ((strcasecmp(Argument, "on") == 0) ||
        (strcasecmp(Argument, "yes") == 0) ||
        (strcasecmp(Argument, "1") == 0)) {

        Context->Flags |= DEBUGGER_FLAG_SOURCE_LINE_STEPPING;
    }

    if ((strcasecmp(Argument, "off") == 0) ||
        (strcasecmp(Argument, "no") == 0) ||
        (strcasecmp(Argument, "0") == 0)) {

        Context->Flags &= ~DEBUGGER_FLAG_SOURCE_LINE_STEPPING;
    }

    if ((Context->Flags & DEBUGGER_FLAG_SOURCE_LINE_STEPPING) != 0) {
        DbgOut("Stepping by source line is now enabled.\n");

    } else {
        DbgOut("Stepping by source line is now disabled.\n");
    }

    return 0;
}

INT
DbgrSetSourceLinePrinting (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine turns on or off the option to print the source file and line
    next to every text address.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR Argument;

    if (ArgumentCount != 2) {
        DbgOut("Error: Use \"sl on\" or \"sl off\" to enable or disable source "
               "line printing.\n");

        return EINVAL;
    }

    Argument = Arguments[1];
    if ((strcasecmp(Argument, "on") == 0) ||
        (strcasecmp(Argument, "yes") == 0) ||
        (strcasecmp(Argument, "1") == 0)) {

        Context->Flags |= DEBUGGER_FLAG_PRINT_LINE_NUMBERS;
    }

    if ((strcasecmp(Argument, "off") == 0) ||
        (strcasecmp(Argument, "no") == 0) ||
        (strcasecmp(Argument, "0") == 0)) {

        Context->Flags &= ~DEBUGGER_FLAG_PRINT_LINE_NUMBERS;
    }

    if ((Context->Flags & DEBUGGER_FLAG_PRINT_LINE_NUMBERS) != 0) {
        DbgOut("Printing of source line numbers is now enabled.\n");

    } else {
        DbgOut("Printing of source line numbers is now disabled.\n");
    }

    return 0;
}

INT
DbgrReturnToCaller (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine interprets the "go" command from the user.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG BytesRead;
    ULONG FirstInstruction;
    ULONG FirstInstructionAddress;
    STACK_FRAME Frame;
    ULONG FrameCount;
    ULONGLONG InstructionPointer;
    INT Result;
    ULONGLONG ReturnAddress;

    ReturnAddress = 0;

    assert(Context->CurrentEvent.Type == DebuggerEventBreak);

    InstructionPointer =
                    Context->CurrentEvent.BreakNotification.InstructionPointer;

    //
    // For ARM machines, the compiler doesn't generate a stack frame for
    // leaf functions (functions that call no other functions). If this is the
    // case the link register is actually the return value, not the frame.
    // Detect this case by reading the first instruction of the function. If
    // it's not "mov ip, sp", then this is a leaf function.
    //

    if (Context->MachineType == MACHINE_TYPE_ARM) {
        FirstInstructionAddress =
                      DbgpGetFunctionStartAddress(Context, InstructionPointer);

        if (FirstInstructionAddress != 0) {
            Result = DbgReadMemory(Context,
                                   TRUE,
                                   FirstInstructionAddress,
                                   ARM_INSTRUCTION_LENGTH,
                                   &FirstInstruction,
                                   &BytesRead);

            if ((Result == 0) && (BytesRead == ARM_INSTRUCTION_LENGTH) &&
                ((FirstInstruction != ARM_FUNCTION_PROLOGUE) ||
                 (InstructionPointer == FirstInstructionAddress))) {

                ReturnAddress =
                    Context->CurrentEvent.BreakNotification.Registers.Arm.R14Lr;

                Result = 0;
                goto ReturnToCallerEnd;
            }
        }
    }

    FrameCount = 1;
    Result = DbgGetCallStack(Context, NULL, &Frame, &FrameCount);
    if ((Result != 0) || (FrameCount == 0)) {
        DbgOut("Error: Unable to get call stack.\n");
        Result = EINVAL;
        goto ReturnToCallerEnd;
    }

    ReturnAddress = Frame.ReturnAddress;
    Result = 0;

ReturnToCallerEnd:

    //
    // If the return address was successfully retrieved, then send the go
    // command.
    //

    if (Result == 0) {
        Result = DbgrContinue(Context, TRUE, ReturnAddress);
    }

    return Result;
}

INT
DbgrSetSymbolPathCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine sets or updates the symbol search path.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    BOOL Append;
    ULONG ArgumentIndex;
    ULONG PathIndex;
    INT Status;
    INT TotalStatus;

    //
    // The sympath+ command augments the current symbol path.
    //

    if (strcasecmp(Arguments[0], "sympath+") == 0) {
        Append = TRUE;

    //
    // The sympath command command either prints the current symbol path with
    // no arguments or sets a new one.
    //

    } else {

        assert(strcasecmp(Arguments[0], "sympath") == 0);

        Append = FALSE;
        if (ArgumentCount == 1) {
            for (PathIndex = 0;
                 PathIndex < Context->SymbolPathCount;
                 PathIndex += 1) {

                DbgOut("%s\n", Context->SymbolPath[PathIndex]);
            }

            return 0;
        }
    }

    //
    // Loop adding or replacing the symbol path.
    //

    TotalStatus = 0;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Status = DbgrSetSymbolPath(Context, Arguments[ArgumentIndex], Append);
        if (Status != 0) {
            TotalStatus = Status;
        }

        //
        // Assume that even if the user didn't specify sympath+ but did add
        // multiple arguments, they want all the arguments in the search path.
        //

        Append = TRUE;
    }

    Status = TotalStatus;
    return Status;
}

INT
DbgrSetSourcePathCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine sets or updates the source search path.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG ArgumentIndex;
    PLIST_ENTRY CurrentEntry;
    PDEBUGGER_SOURCE_PATH Entry;
    INT FinalResult;
    INT Result;

    if (strcasecmp(Arguments[0], "srcpath+") != 0) {

        //
        // If it's just srcpath by itself, print the current source path.
        //

        if (ArgumentCount == 1) {
            CurrentEntry = Context->SourcePathList.Next;
            while (CurrentEntry != &(Context->SourcePathList)) {
                Entry = LIST_VALUE(CurrentEntry,
                                   DEBUGGER_SOURCE_PATH,
                                   ListEntry);

                CurrentEntry = CurrentEntry->Next;
                if (Entry->PrefixLength != 0) {
                    DbgOut("%s -> %s\n", Entry->Prefix, Entry->Path);

                } else {
                    DbgOut("%s\n", Entry->Path);
                }
            }

        } else {
            DbgrpDestroyAllSourcePaths(Context);
        }
    }

    //
    // Add all source paths in the arguments.
    //

    FinalResult = 0;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Result = DbgrpAddSourcePath(Context, Arguments[ArgumentIndex]);
        if (Result != 0) {
            DbgOut("Failed to add source path %s: Error %s.\n",
                   Arguments[ArgumentIndex],
                   strerror(Result));

            FinalResult = Result;
        }
    }

    return FinalResult;
}

INT
DbgrReloadSymbols (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine unloads and reloads all symbols from the search path.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INT Result;

    DbgrpUnloadAllModules(Context, FALSE);
    Result = DbgrpValidateLoadedModules(
                 Context,
                 Context->CurrentEvent.BreakNotification.LoadedModuleCount,
                 Context->CurrentEvent.BreakNotification.LoadedModuleSignature,
                 TRUE);

    return Result;
}

INT
DbgrSetSymbolPath (
    PDEBUGGER_CONTEXT Context,
    PSTR Path,
    BOOL Append
    )

/*++

Routine Description:

    This routine sets or updates the symbol search path.

Arguments:

    Context - Supplies a pointer to the application context.

    Path - Supplies a pointer to the new symbol path. This could contain
        multiple symbol paths if separated by semicolons.

    Append - Supplies a boolean indicating whether the new path should replace
        or append the existing path.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR CurrentString;
    PSTR *NewArray;
    PSTR NewPath;
    ULONG NewPathCount;
    PSTR Next;
    ULONG Offset;
    ULONG PathCount;
    ULONG PathIndex;
    ULONG PathLength;
    INT Status;

    NewArray = NULL;

    //
    // Loop once to count the number of semicolons.
    //

    NewPathCount = 1;
    CurrentString = Path;
    while (TRUE) {
        CurrentString = strchr(CurrentString, ';');
        if (CurrentString == NULL) {
            break;
        }

        NewPathCount += 1;
        CurrentString += 1;
    }

    //
    // Allocate the new array.
    //

    PathCount = NewPathCount;
    if (Append != FALSE) {
        PathCount += Context->SymbolPathCount;
    }

    NewArray = malloc(sizeof(PSTR) * PathCount);
    if (NewArray == NULL) {
        Status = ENOMEM;
        goto SetSymbolPathEnd;
    }

    memset(NewArray, 0, sizeof(PSTR) * PathCount);

    //
    // If copying, move the existing entries over now.
    //

    Offset = 0;
    if (Append != FALSE) {
        for (Offset = 0; Offset < Context->SymbolPathCount; Offset += 1) {
            NewArray[Offset] = Context->SymbolPath[Offset];
        }

    //
    // If not copying, free up the existing entries.
    //

    } else {
        for (PathIndex = 0;
             PathIndex < Context->SymbolPathCount;
             PathIndex += 1) {

            free(Context->SymbolPath[PathIndex]);
        }
    }

    if (Context->SymbolPath != NULL) {
        free(Context->SymbolPath);
    }

    //
    // Now create the new array entries based on the parameter.
    //

    CurrentString = Path;
    for (PathIndex = 0; PathIndex < NewPathCount; PathIndex += 1) {
        Next = strchr(CurrentString, ';');
        if (Next != NULL) {
            PathLength = (UINTN)Next - (UINTN)CurrentString;

        } else {
            PathLength = strlen(CurrentString);
        }

        NewPath = malloc(PathLength + 1);
        if (NewPath == NULL) {
            Status = ENOMEM;
            goto SetSymbolPathEnd;
        }

        memcpy(NewPath, CurrentString, PathLength);
        NewPath[PathLength] = '\0';
        NewArray[PathIndex + Offset] = NewPath;
        CurrentString = Next + 1;
    }

    Context->SymbolPath = NewArray;
    Context->SymbolPathCount = PathCount;
    Status = 0;

SetSymbolPathEnd:
    if (Status != 0) {
        if (NewArray != NULL) {
            for (PathIndex = 0; PathIndex < NewPathCount; PathIndex += 1) {
                if (NewArray[PathIndex] != NULL) {
                    free(NewArray[PathIndex]);
                }
            }

            free(NewArray);
        }
    }

    return Status;
}

INT
DbgrLoadExtension (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine loads or unloads a debugger extension.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG ArgumentIndex;
    PSTR Name;
    INT Status;
    INT TotalStatus;

    //
    // The load command fires up an extension.
    //

    if (strcasecmp(Arguments[0], "load") == 0) {
        if (ArgumentCount < 2) {
            DbgOut("Usage: load <path>\nLoads a debugger extension at the "
                   "given path.\n");

            Status = EINVAL;
            goto LoadExtensionEnd;
        }

        TotalStatus = 0;
        for (ArgumentIndex = 1;
             ArgumentIndex < ArgumentCount;
             ArgumentIndex += 1) {

            Name = Arguments[ArgumentIndex];
            Status = DbgLoadExtension(Context, Name);
            if (Status != 0) {
                DbgOut("Failed to load extension '%s'.\n", Name);
                TotalStatus = Status;
            }
        }

        Status = TotalStatus;

    //
    // Unload an extension, or * for all extensions.
    //

    } else {

        assert(strcasecmp(Arguments[0], "unload") == 0);

        if (ArgumentCount < 2) {
            DbgOut("Usage: unload <path>\nUnloads a debugger extension at the "
                   "given path. Use 'unload *' to unload all extensions.\n");

            Status = EINVAL;
            goto LoadExtensionEnd;
        }

        TotalStatus = 0;
        for (ArgumentIndex = 1;
             ArgumentIndex < ArgumentCount;
             ArgumentIndex += 1) {

            Name = Arguments[ArgumentIndex];
            if (strcmp(Name, "*") == 0) {
                DbgOut("Unloading all extensions.\n");
                DbgUnloadAllExtensions(Context);
                Status = 0;
                break;

            } else {
                DbgUnloadExtension(Context, Name);
            }
        }

        Status = TotalStatus;
    }

LoadExtensionEnd:
    return Status;
}

INT
DbgrSwitchProcessor (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine switches the debugger to another processor in kernel mode or
    thread in user mode.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR AfterScan;
    ULONG Count;
    PULONG Ids;
    ULONG ProcessorNumber;
    PSTR ProcessorNumberString;
    INT Result;
    ULONG ThreadIndex;

    Ids = NULL;

    assert(Arguments[0][0] == '~');

    if (Arguments[0][1] == '\0') {
        ProcessorNumber = -1;

    } else {
        ProcessorNumberString = Arguments[0] + 1;
        ProcessorNumber = strtoul(ProcessorNumberString, &AfterScan, 0);
        if (AfterScan == ProcessorNumberString) {
            DbgOut("Failed to convert '%s' to a number.\n",
                   ProcessorNumberString);

            Result = EINVAL;
            goto SwitchProcessorsEnd;
        }
    }

    //
    // If no processor number was supplied, list the processors.
    //

    if (ProcessorNumber == -1) {
        Result = DbgGetThreadList(Context, &Count, &Ids);
        if (Result != 0) {
            DbgOut("Error: Failed to get processor/thread list.\n");
            goto SwitchProcessorsEnd;
        }

        if (Context->ConnectionType == DebugConnectionKernel) {
            if (Count == 1) {
                DbgOut("There is 1 processor in the system.\n");

            } else {
                DbgOut("There are %d processors in the system.\n", Count);
            }

            Result = 0;

        } else if (Context->ConnectionType == DebugConnectionUser) {
            if (Count == 1) {
                DbgOut("There is 1 thread in the process.\n");

            } else {
                DbgOut("There are %d threads in the process:\n", Count);
                for (ThreadIndex = 0; ThreadIndex < Count; ThreadIndex += 1) {
                    DbgOut("%x\n", Ids[ThreadIndex]);
                }
            }

        } else {
            DbgOut("Error: Unknown connection type %d.\n",
                    Context->ConnectionType);

            Result = EINVAL;
        }

        goto SwitchProcessorsEnd;
    }

    //
    // The user cannot switch to the same processor.
    //

    if (ProcessorNumber ==
        Context->CurrentEvent.BreakNotification.ProcessorOrThreadNumber) {

        Result = 0;
        goto SwitchProcessorsEnd;
    }

    //
    // The user cannot switch to a processor that's out of range.
    //

    Count = Context->CurrentEvent.BreakNotification.ProcessorOrThreadCount;
    if ((Context->ConnectionType == DebugConnectionKernel) &&
        (ProcessorNumber >= Count)) {

        if (Count == 1) {
            DbgOut("Error: There is only one processor in the system.\n");

        } else {
            DbgOut("Error: There are only %d processors in the system!\n",
                   Count);
        }

        Result = 0;
        goto SwitchProcessorsEnd;
    }

    //
    // Send the switch command.
    //

    Result = DbgSwitchProcessors(Context, ProcessorNumber);
    if (Result != 0) {
        DbgOut("Error: Failed to switch processors.\n");
        goto SwitchProcessorsEnd;
    }

    //
    // Reset the frame as well.
    //

    DbgrpSetFrame(Context, 0);
    Context->LastMemoryDump.NextAddress =
                    Context->CurrentEvent.BreakNotification.InstructionPointer;

    Context->LastMemoryDump.Virtual = TRUE;
    Context->DisassemblyAddress = Context->LastMemoryDump.NextAddress;
    Result = 0;

SwitchProcessorsEnd:
    if (Ids != NULL) {
        free(Ids);
    }

    return Result;
}

INT
DbgrPrintProcessorBlock (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine prints the contents of the current processor block.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONGLONG Address;
    UINTN Size;
    INT Status;
    PSTR TypeString;

    if (ArgumentCount > 1) {
        Size = strlen(Arguments[1]) + strlen("PROCESSOR_BLOCK") + 2;
        TypeString = malloc(Size);
        if (TypeString == NULL) {
            return ENOMEM;
        }

        snprintf(TypeString, Size, "%s.%s", "PROCESSOR_BLOCK", Arguments[1]);

    } else {
        TypeString = "PROCESSOR_BLOCK";
    }

    Address = Context->CurrentEvent.BreakNotification.ProcessorBlock;
    Status = EFAULT;
    if (Address != 0) {
        Status = DbgPrintTypeByName(Context,
                                    Address,
                                    TypeString,
                                    0,
                                    DEFAULT_RECURSION_DEPTH);

        DbgOut("\n");
    }

    if (ArgumentCount > 1) {
        free(TypeString);
    }

    return Status;
}

VOID
DbgrRequestBreakIn (
    VOID
    )

/*++

Routine Description:

    This routine sends a break-in request to the target.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (DbgConsoleContext->ConnectionType == DebugConnectionRemote) {
        DbgrpClientRequestBreakIn(DbgConsoleContext);

    } else {
        DbgRequestBreakIn(DbgConsoleContext);
    }

    return;
}

INT
DbgrDumpPointerSymbols (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine dumps memory at the provided address and attempts to match
    symbols at the dumped memory addresses.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONGLONG Address;
    PSTR AddressString;
    PULONG Buffer;
    ULONG BufferSize;
    ULONG BytesRead;
    INT Index;
    INT Result;

    Buffer = NULL;
    AddressString = NULL;
    if (ArgumentCount >= 2) {
        AddressString = Arguments[1];
        Result = DbgEvaluate(Context, AddressString, &Address);
        if (Result != 0) {
            DbgOut("Failed to evaluate address '%s'.\n", AddressString);
            goto DumpPointerSymbolsEnd;
        }

    } else {
        Address = Context->LastMemoryDump.NextAddress;
    }

    BufferSize = sizeof(ULONG) * DEFAULT_DUMP_POINTERS_ROWS;
    Buffer = malloc(BufferSize);
    if (Buffer == NULL) {
        Result = ENOMEM;
        goto DumpPointerSymbolsEnd;
    }

    Result = DbgReadMemory(Context,
                           TRUE,
                           Address,
                           BufferSize,
                           Buffer,
                           &BytesRead);

    if (Result != 0) {
        goto DumpPointerSymbolsEnd;
    }

    for (Index = 0; Index < DEFAULT_DUMP_POINTERS_ROWS; Index += 1) {
        DbgOut("%08I64x ", Address);
        Address += sizeof(ULONG);
        if (((Index + 1) * sizeof(ULONG)) <= BytesRead) {
            DbgOut("%08x ", Buffer[Index]);
            DbgPrintAddressSymbol(Context, Buffer[Index]);

        } else {
            DbgOut("????????");
        }

        DbgOut("\n");
    }

    Context->LastMemoryDump.NextAddress = Address;
    Context->LastMemoryDump.Virtual = TRUE;
    Result = 0;

DumpPointerSymbolsEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    return Result;
}

INT
DbgrProfileCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine handles the profile command. It essentially just forwards on
    to the profile handler.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INT Result;

    Result = DbgrDispatchProfilerCommand(Context,
                                         Arguments + 1,
                                         ArgumentCount - 1);

    return Result;
}

INT
DbgrRebootCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine handles the profile command. It essentially just forwards on
    to the profile handler.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    BOOL PrintUsage;
    DEBUG_REBOOT_TYPE RebootType;

    PrintUsage = FALSE;
    RebootType = DebugRebootWarm;
    if (ArgumentCount > 2) {
        PrintUsage = TRUE;

    } else if (ArgumentCount > 1) {
        if (strcasecmp(Arguments[1], "-s") == 0) {
            RebootType = DebugRebootShutdown;

        } else if (strcasecmp(Arguments[1], "-w") == 0) {
            RebootType = DebugRebootWarm;

        } else if (strcasecmp(Arguments[1], "-c") == 0) {
            RebootType = DebugRebootCold;

        } else {
            PrintUsage = TRUE;
        }
    }

    if (PrintUsage != FALSE) {
        DbgOut("Usage: reboot [-s|-w|-c]\n"
               "This command forcefully reboots the target machine. If the \n"
               "target does not support the given option, a cold reboot is \n"
               "performed. Options are:\n"
               "  -s -- Shut down the machine.\n"
               "  -w -- Warm reset the machine (default).\n"
               "  -c -- Cold reset the machine.\n\n");

        return 1;
    }

    return DbgReboot(Context, RebootType);
}

INT
DbgrContinue (
    PDEBUGGER_CONTEXT Context,
    BOOL SetOneTimeBreak,
    ULONGLONG Address
    )

/*++

Routine Description:

    This routine sends the "go" command to the target, signaling to continue
    execution.

Arguments:

    Context - Supplies a pointer to the application context.

    SetOneTimeBreak - Supplies a flag indicating whether to go unconditionally
        (FALSE) or with a one-time breakpoint (TRUE).

    Address - Supplies the address of the one-time breakpoint if one was
        specified.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PDEBUGGER_BREAK_POINT Breakpoint;
    PLIST_ENTRY CurrentEntry;
    INT Result;
    ULONG SignalToDeliver;

    //
    // Look to see if there's already an enabled breakpoint at that address,
    // and do nothing if there is.
    //

    CurrentEntry = Context->BreakpointList.Next;
    while ((SetOneTimeBreak != FALSE) &&
           (CurrentEntry != &(Context->BreakpointList))) {

        Breakpoint = LIST_VALUE(CurrentEntry, DEBUGGER_BREAK_POINT, ListEntry);
        if ((Breakpoint->Enabled != FALSE) &&
            (Breakpoint->Type == BreakpointTypeExecution) &&
            (Breakpoint->Address == Address)) {

            SetOneTimeBreak = FALSE;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Set the one time break point if requested.
    //

    if (SetOneTimeBreak != FALSE) {
        Result = DbgrpSetBreakpointAtAddress(
                                        Context,
                                        Address,
                                        &(Context->OneTimeBreakOriginalValue));

        Context->OneTimeBreakAddress = Address;
        if (Result != 0) {
            DbgOut("Error: Failed to set breakpoint at %I64x.\n", Address);
            return Result;
        }

        Context->OneTimeBreakValid = TRUE;
    }

    //
    // If there's a breakpoint to restore, then do a single step, restore the
    // breakpoint, and then continue.
    //

    if (Context->BreakpointToRestore != NULL) {
        Result = DbgrSingleStep(Context);
        if (Result != 0) {
            DbgOut("Error: Failed to single step.\n");
            return Result;
        }

        Result = DbgWaitForEvent(Context);
        if (Result != 0) {
            DbgOut("Error: Failed to wait for a response after single step.\n");
            return Result;
        }

        if (Context->CurrentEvent.Type != DebuggerEventBreak) {
            DbgOut("Failed to get a break after a single step.\n");
            return EINVAL;
        }

        Result = DbgrpSetBreakpointAtAddress(
                               Context,
                               Context->BreakpointToRestore->Address,
                               &(Context->BreakpointToRestore->OriginalValue));

        if (Result != 0) {
            DbgOut("Failed to restore breakpoint %d at %I64x.\n",
                   Context->BreakpointToRestore->Index,
                   Context->BreakpointToRestore->Address);

            return Result;
        }

        Context->BreakpointToRestore = NULL;
    }

    SignalToDeliver = DbgGetSignalToDeliver(Context);
    Result = DbgContinue(Context, SignalToDeliver);
    if (Result != 0) {
        return Result;
    }

    Result = 0;
    return Result;
}

VOID
DbgrShowSourceAtAddress (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address
    )

/*++

Routine Description:

    This routine loads the source file and highlights the source line
    corresponding to the given target address.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the target's executing virtual address.

Return Value:

    None.

--*/

{

    PDEBUGGER_MODULE CurrentModule;
    ULONGLONG DebasedAddress;
    BOOL Result;
    PSOURCE_LINE_SYMBOL SourceLine;
    PSTR SourcePath;

    //
    // Acquire the standard out lock to synchronize with remote threads trying
    // to send updated source information.
    //

    AcquireDebuggerLock(Context->StandardOut.Lock);
    DbgrUnhighlightCurrentLine(Context);
    SourcePath = NULL;
    CurrentModule = DbgpFindModuleFromAddress(Context,
                                              Address,
                                              &DebasedAddress);

    if (CurrentModule != NULL) {
        SourceLine = DbgLookupSourceLine(CurrentModule->Symbols,
                                         DebasedAddress);

        if (SourceLine != NULL) {
            SourcePath = DbgrpCreateFullPath(SourceLine->ParentSource);
            if (SourcePath == NULL) {
                goto ShowSourceAtAddressEnd;
            }

            //
            // If the source file is different than what was previously
            // displayed, load the new source file.
            //

            if ((Context->SourceFile.Path == NULL) ||
                (strcmp(Context->SourceFile.Path, SourcePath) != 0)) {

                if (Context->SourceFile.Path != NULL) {
                    free(Context->SourceFile.Path);
                    Context->SourceFile.Path = NULL;
                }

                if (Context->SourceFile.ActualPath != NULL) {
                    free(Context->SourceFile.ActualPath);
                    Context->SourceFile.ActualPath = NULL;
                }

                if (Context->SourceFile.Contents != NULL) {
                    free(Context->SourceFile.Contents);
                    Context->SourceFile.Contents = 0;
                }

                Context->SourceFile.Path = SourcePath;
                SourcePath = NULL;
                Context->SourceFile.LineNumber = 0;
                Result = DbgrpLoadSourceFile(Context,
                                             Context->SourceFile.Path,
                                             &(Context->SourceFile.ActualPath),
                                             &(Context->SourceFile.Contents),
                                             &(Context->SourceFile.Size));

                if (Result == 0) {
                    Result = UiLoadSourceFile(Context->SourceFile.ActualPath,
                                              Context->SourceFile.Contents,
                                              Context->SourceFile.Size);

                    if (Result != FALSE) {
                        DbgrpHighlightExecutingLine(Context,
                                                    SourceLine->LineNumber);
                    }

                //
                // The file load failed. Clear the screen. Do notify the
                // remotes too, maybe they'll have better luck loading the
                // file on their own.
                //

                } else {
                    UiLoadSourceFile(NULL, NULL, 0);
                    Context->SourceFile.LineNumber = SourceLine->LineNumber;
                    DbgrpServerNotifyClients(Context);
                }

            //
            // It's the same file as before, just highlight a different line.
            //

            } else {
                DbgrpHighlightExecutingLine(Context, SourceLine->LineNumber);
            }
        }
    }

ShowSourceAtAddressEnd:
    ReleaseDebuggerLock(Context->StandardOut.Lock);
    if (SourcePath != NULL) {
        free(SourcePath);
    }

    return;
}

INT
DbgrDumpType (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount,
    PVOID RawDataStream,
    ULONG RawDataStreamSizeInBytes
    )

/*++

Routine Description:

    This routine prints information about a type description or value. If only
    a type is specified, the type format will be printed. If an address is
    passed as a second parameter, then the values will be dumped. If a global
    or local variable is passed as the first parameter, the values will also be
    dumped.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies a pointer to an array of argument strings.

    ArgumentCount - Supplies the number of arguments in the argument array.

    RawDataStream - Supplies the actual memory to dump in the given type form.
        If this parameter is non-NULL, then the AddressString parameter is
        ignored. The size of this buffer must be non-zero.

    RawDataStreamSizeInBytes - Supplies the size of the raw data stream buffer,
        in bytes.

Return Value:

    0 if the information was printed successfully.

    Returns an error code on failure.

--*/

{

    ULONGLONG Address;
    ULONG AddressIndex;
    ULONG AddressStartIndex;
    ULONG BytesRead;
    PDATA_SYMBOL DataResult;
    PVOID DataStream;
    PDATA_SYMBOL Local;
    ULONGLONG Pc;
    INT Result;
    SYMBOL_SEARCH_RESULT SearchResult;
    PDEBUG_SYMBOLS Symbols;
    PSTR SymbolString;
    PTYPE_SYMBOL Type;
    UINTN TypeSize;

    Address = 0;
    AddressStartIndex = 1;
    DataStream = NULL;
    Type = NULL;

    assert((Arguments != NULL) && (ArgumentCount != 0));

    //
    // Test to see if the first argument is a local variable name. Local
    // variables must be tested first because a local variable may match a
    // structure name, at which point the wrong data would be printed. Do not
    // perform this test when raw data is supplied.
    //

    if ((ArgumentCount == 1) &&
        ((RawDataStream == NULL) || (RawDataStreamSizeInBytes == 0))) {

        Result = DbgpFindLocal(Context,
                               &(Context->FrameRegisters),
                               Arguments[0],
                               &Symbols,
                               &Local,
                               &Pc);

        if (Result == 0) {

            //
            // In order to dump the local variable, the local data symbol must
            // be evaluated for type information.
            //

            Result = DbgGetDataSymbolTypeInformation(Local, &Type, &TypeSize);
            if (Result == FALSE) {
                DbgOut("Error: unable to get type information for the local "
                       "variable %s\n",
                       Arguments[0]);

                Result = EINVAL;
                goto DumpTypeEnd;
            }

            //
            // Allocate memory to collect the data for the data symbol.
            //

            DataStream = malloc(TypeSize);
            if (DataStream == NULL) {
                DbgOut("Error: unable to allocate %ld bytes of memory\n",
                       TypeSize);

                Result = ENOMEM;
                goto DumpTypeEnd;
            }

            //
            // Read the symbol data from the local data symbol.
            //

            Result = DbgGetDataSymbolData(Context,
                                          Symbols,
                                          Local,
                                          Pc,
                                          DataStream,
                                          TypeSize,
                                          NULL,
                                          0);

            if (Result == 0) {

                //
                // Resolve the data into something useful to dump.
                //

                Address = 0;
                Result = DbgrpResolveDumpType(Context,
                                              &Type,
                                              &DataStream,
                                              &TypeSize,
                                              &Address);

                if (Result != 0) {
                    DbgOut("Error: could not resolve dump type %s.\n",
                           Type->Name);

                    goto DumpTypeEnd;
                }

                if (Address != 0) {
                    DbgOut("Dumping memory at 0x%08x\n", (ULONG)Address);
                }

                Result = DbgPrintType(Context,
                                      Type,
                                      DataStream,
                                      TypeSize,
                                      0,
                                      DEFAULT_RECURSION_DEPTH);

                goto DumpTypeEnd;

            //
            // If failed with something other than not found (for not currently
            // active locals), then bail to the end.
            //

            } else if (Result != ENOENT) {
                goto DumpTypeEnd;
            }
        }
    }

    //
    // If a local type was not found, search symbols for the first argument.
    // It should either by a type or a global variable.
    //

    SymbolString = Arguments[0];
    SearchResult.Variety = SymbolResultInvalid;
    Result = DbgpFindSymbol(Context, SymbolString, &SearchResult);
    if (Result == FALSE) {
        DbgOut("Error: Invalid type or global variable %s\n", SymbolString);
        Result = EINVAL;
        goto DumpTypeEnd;
    }

    switch (SearchResult.Variety) {
    case SymbolResultType:
        Type = SearchResult.U.TypeResult;
        AddressStartIndex = 1;
        break;

    case SymbolResultData:
        DataResult = SearchResult.U.DataResult;
        Type = DbgGetType(DataResult->TypeOwner, DataResult->TypeNumber);

        //
        // This argument was an address itself.
        //

        AddressStartIndex = 0;
        break;

    default:
        DbgOut("Error: Invalid symbol type %d for argument 1: %s\n",
               SearchResult.Variety,
               SymbolString);

        break;
    }

    if (Type != NULL) {
        Type = DbgSkipTypedefs(Type);
    }

    //
    // If a type was not found, print the error and exit.
    //

    if (Type == NULL) {
        DbgOut("Error: could not find type %s.\n", SymbolString);
        goto DumpTypeEnd;
    }

    //
    // If a raw data stream was supplied, print the contents of that in terms
    // of the type.
    //

    TypeSize = DbgGetTypeSize(Type, 0);
    if ((RawDataStream != NULL) && (RawDataStreamSizeInBytes != 0)) {
        if (RawDataStreamSizeInBytes < TypeSize) {
            DbgOut("Error: Supplied buffer of size %d is not big enough to "
                   "print type of size %ld.\n",
                   RawDataStreamSizeInBytes,
                   TypeSize);

            Result = EINVAL;
            goto DumpTypeEnd;
        }

        Result = DbgPrintType(Context,
                              Type,
                              RawDataStream,
                              RawDataStreamSizeInBytes,
                              0,
                              DEFAULT_RECURSION_DEPTH);

        if (Result != 0) {
            goto DumpTypeEnd;
        }

    //
    // If an address was specified, print the type's contents.
    //

    } else if (ArgumentCount - AddressStartIndex != 0) {
        DataStream = malloc(TypeSize);
        if (DataStream == NULL) {
            Result = ENOMEM;
            goto DumpTypeEnd;
        }

        for (AddressIndex = AddressStartIndex;
             AddressIndex < ArgumentCount;
             AddressIndex += 1) {

            //
            // Evaluate the address. Failure here indicates a syntax error, but
            // is not a fatal error.
            //

            Result = DbgEvaluate(Context, Arguments[AddressIndex], &Address);
            if (Result != 0) {
                DbgOut("Syntax error in address parameter!\n");
                goto DumpTypeEnd;
            }

            Result = DbgReadMemory(Context,
                                   TRUE,
                                   Address,
                                   TypeSize,
                                   DataStream,
                                   &BytesRead);

            if (Result != 0) {
                DbgOut("Error reading virtual memory, only read %ld bytes, "
                       "type is %d bytes!\n",
                       TypeSize,
                       BytesRead);

                goto DumpTypeEnd;
            }

            //
            // Resolve the type to something useful to dump.
            //

            Result = DbgrpResolveDumpType(Context,
                                          &Type,
                                          &DataStream,
                                          &TypeSize,
                                          &Address);

            if (Result != 0) {
                DbgOut("Error: could not resolve dump type %s.\n", Type->Name);
                goto DumpTypeEnd;
            }

            //
            // Print the values.
            //

            DbgOut("Dumping memory at 0x%08x\n", (ULONG)Address);
            Result = DbgPrintType(Context,
                                  Type,
                                  DataStream,
                                  TypeSize,
                                  0,
                                  DEFAULT_RECURSION_DEPTH);

            if (AddressIndex != ArgumentCount - 1) {
                DbgOut("\n");
            }

            if (Result != 0) {
                goto DumpTypeEnd;
            }
        }

    //
    // No address was specified, so print the type description.
    //

    } else {
        DbgPrintTypeName(Type);
        DbgOut(" = ");
        DbgPrintTypeDescription(Type, 0, DEFAULT_RECURSION_DEPTH);
    }

    Result = 0;

DumpTypeEnd:
    if (DataStream != NULL) {
        free(DataStream);
    }

    return Result;
}

INT
DbgrpHighlightExecutingLine (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG LineNumber
    )

/*++

Routine Description:

    This routine highlights the currently executing source line and scrolls to
    it, or removes the highlight.

Arguments:

    Context - Supplies the application context.

    LineNumber - Supplies the one-based line number to highlight, or 0 to
        disable highlighting.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    BOOL Result;

    if (Context->SourceFile.LineNumber == LineNumber) {
        return 0;
    }

    //
    // Unhighlight the current line first.
    //

    if (Context->SourceFile.LineNumber != 0) {
        UiHighlightExecutingLine(Context->SourceFile.LineNumber, FALSE);
    }

    //
    // Set the new line number and notify the connected clients.
    //

    Context->SourceFile.LineNumber = LineNumber;
    DbgrpServerNotifyClients(Context);

    //
    // If a new line is being highlighted, set it in the UI.
    //

    if (LineNumber != 0) {
        Result = UiHighlightExecutingLine(Context->SourceFile.LineNumber, TRUE);
        if (Result == FALSE) {
            return -1;
        }
    }

    return 0;
}

INT
DbgrpLoadSourceFile (
    PDEBUGGER_CONTEXT Context,
    PSTR Path,
    PSTR *FoundPath,
    PVOID *Contents,
    PULONGLONG Size
    )

/*++

Routine Description:

    This routine loads a source file into memory.

Arguments:

    Context - Supplies a pointer to the application context.

    Path - Supplies a pointer to the path to load.

    FoundPath - Supplies a pointer where a pointer to the path of the actual
        file loaded will be returned. The path may be modified by the source
        path list, so this represents the final found path.

    Contents - Supplies a pointer where a pointer to the loaded file will be
        returned on success. The caller is responsible for freeing this memory.

    Size - Supplies a pointer where the size of the file will be returned on
        success.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDEBUGGER_SOURCE_PATH Entry;
    size_t PathLength;
    PSTR Potential;
    size_t PotentialSize;
    INT Result;

    *FoundPath = NULL;
    *Contents = NULL;
    *Size = 0;
    PathLength = strlen(Path);
    Potential = NULL;
    PotentialSize = 0;

    //
    // Loop over all the source paths trying to find a file path that
    // exists.
    //

    CurrentEntry = Context->SourcePathList.Next;
    while (CurrentEntry != &(Context->SourcePathList)) {
        Entry = LIST_VALUE(CurrentEntry, DEBUGGER_SOURCE_PATH, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // If the prefix is empty or matches this path, chop the prefix off,
        // replace it with the path, and try to load that file.
        //

        if ((Entry->PrefixLength == 0) ||
            (strncmp(Path, Entry->Prefix, Entry->PrefixLength) == 0)) {

            PotentialSize = Entry->PathLength +
                            (PathLength - Entry->PrefixLength) + 1;

            Potential = malloc(PotentialSize);
            if (Potential == NULL) {
                continue;
            }

            memcpy(Potential, Entry->Path, Entry->PathLength);
            memcpy(Potential + Entry->PathLength,
                   Path + Entry->PrefixLength,
                   PathLength - Entry->PrefixLength);

            Potential[PotentialSize - 1] = '\0';
            Result = DbgrpLoadFile(Potential, Contents, Size);
            if ((Context->Flags & DEBUGGER_FLAG_PRINT_SOURCE_LOADS) != 0) {

                //
                // Use printf directly here and not DbgOut as the standard out
                // lock is held already.
                //

                printf("Load %s: %s\n", Potential, strerror(Result));
            }

            if (Result == 0) {
                *FoundPath = Potential;
                return 0;
            }

            free(Potential);
        }
    }

    //
    // Finally, try the source by itself.
    //

    PotentialSize = PathLength + 1;
    Potential = strdup(Path);
    if (Potential == NULL) {
        return ENOMEM;
    }

    Result = DbgrpLoadFile(Potential, Contents, Size);
    if ((Context->Flags & DEBUGGER_FLAG_PRINT_SOURCE_LOADS) != 0) {

        //
        // Use printf directly here and not DbgOut as the standard out lock is
        // held already.
        //

        printf("Load %s: %s\n", Potential, strerror(Result));
    }

    if (Result == 0) {
        *FoundPath = Potential;
        return 0;
    }

    return Result;
}

INT
DbgrpAddSourcePath (
    PDEBUGGER_CONTEXT Context,
    PSTR PathString
    )

/*++

Routine Description:

    This routine adds a source path entry to the given application context.

Arguments:

    Context - Supplies a pointer to the application context.

    PathString - Supplies a pointer to the path string, which takes the form
        prefix=path. If there is no equals sign, then the prefix is assumed to
        be empty.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PDEBUGGER_SOURCE_PATH Entry;
    PSTR Equals;
    PSTR Path;
    UINTN PathLength;
    PSTR Prefix;
    UINTN PrefixLength;
    INT Status;

    if (*PathString == '\0') {
        return 0;
    }

    Entry = NULL;
    Prefix = NULL;
    PrefixLength = 0;
    Path = NULL;
    PathLength = 0;

    //
    // Split on the equals.
    //

    Equals = strchr(PathString, '=');
    if (Equals != NULL) {
        if (Equals != PathString) {
            PrefixLength = (UINTN)Equals - (UINTN)PathString;
            Prefix = malloc(PrefixLength + 1);
            if (Prefix == NULL) {
                Status = ENOMEM;
                goto AddSourcePathEnd;
            }

            memcpy(Prefix, PathString, PrefixLength);
            Prefix[PrefixLength] = '\0';
        }

        PathLength = strlen(Equals + 1);
        Path = strdup(Equals + 1);

    } else {
        PathLength = strlen(PathString);
        Path = strdup(PathString);
    }

    if (Path == NULL) {
        Status = ENOMEM;
        goto AddSourcePathEnd;
    }

    //
    // Don't bother adding dumb entries.
    //

    if ((PathLength == 0) && (PrefixLength == 0)) {
        Status = 0;
        goto AddSourcePathEnd;
    }

    //
    // Create the entry and add it to the end.
    //

    Entry = malloc(sizeof(DEBUGGER_SOURCE_PATH));
    if (Entry == NULL) {
        Status = ENOMEM;
        goto AddSourcePathEnd;
    }

    Entry->Prefix = Prefix;
    Entry->PrefixLength = PrefixLength;
    Entry->Path = Path;
    Entry->PathLength = PathLength;
    Path = NULL;
    Prefix = NULL;
    INSERT_BEFORE(&(Entry->ListEntry), &(Context->SourcePathList));
    Status = 0;

AddSourcePathEnd:
    if (Path != NULL) {
        free(Path);
    }

    if (Prefix != NULL) {
        free(Prefix);
    }

    return Status;
}

VOID
DbgrpDestroyAllSourcePaths (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys all source path entries in the given application
    context.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PDEBUGGER_SOURCE_PATH SourcePathEntry;

    while (LIST_EMPTY(&(Context->SourcePathList)) == FALSE) {
        SourcePathEntry = LIST_VALUE(Context->SourcePathList.Next,
                                     DEBUGGER_SOURCE_PATH,
                                     ListEntry);

        LIST_REMOVE(&(SourcePathEntry->ListEntry));
        DbgrpDestroySourcePath(SourcePathEntry);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
DbgrpSetBreakpointAtAddress (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PULONG OriginalValue
    )

/*++

Routine Description:

    This routine modifies the instruction stream to set a breakpoint at the
    given address.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the target address to change to a breakpoint instruction.

    OriginalValue - Supplies a pointer that receives the original contents
        of memory that the breakpoint instruction replaced.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG BreakInstruction;
    ULONG BytesComplete;
    ULONGLONG MemoryAddress;
    INT Result;
    ULONG Size;

    MemoryAddress = Address;
    if ((Context->MachineType == MACHINE_TYPE_X86) ||
        (Context->MachineType == MACHINE_TYPE_X64)) {

        BreakInstruction = X86_BREAK_INSTRUCTION;
        Size = X86_BREAK_INSTRUCTION_LENGTH;

    } else if (Context->MachineType == MACHINE_TYPE_ARM) {
        if ((Address & ARM_THUMB_BIT) != 0) {
            BreakInstruction = THUMB_BREAK_INSTRUCTION;
            Size = THUMB_BREAK_INSTRUCTION_LENGTH;
            MemoryAddress = Address & ~ARM_THUMB_BIT;

        } else {
            BreakInstruction = ARM_BREAK_INSTRUCTION;
            Size = ARM_BREAK_INSTRUCTION_LENGTH;
        }

    } else {
        DbgOut("Unknown machine type %d.\n", Context->MachineType);
        return EINVAL;
    }

    //
    // Read the original contents.
    //

    *OriginalValue = 0;
    Result = DbgReadMemory(Context,
                           TRUE,
                           MemoryAddress,
                           Size,
                           OriginalValue,
                           &BytesComplete);

    if ((Result != 0) || (BytesComplete != Size)) {
        if (Result == 0) {
            Result = EINVAL;
        }

        return Result;
    }

    //
    // Write out the breakpoint instruction.
    //

    Result = DbgWriteMemory(Context,
                            TRUE,
                            MemoryAddress,
                            Size,
                            &BreakInstruction,
                            &BytesComplete);

    if ((Result != 0) || (BytesComplete != Size)) {
        if (Result == 0) {
            Result = EINVAL;
        }

        //
        // Attempt to restore the original value.
        //

        DbgrpClearBreakpointAtAddress(Context, Address, *OriginalValue);
        return Result;
    }

    return 0;
}

INT
DbgrpClearBreakpointAtAddress (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG OriginalValue
    )

/*++

Routine Description:

    This routine restores an instruction stream to its original form before a
    breakpoint was inserted in it.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the target address to change to a breakpoint instruction.

    OriginalValue - Supplies the original value returned when the breakpoint
        was set.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG BreakInstruction;
    ULONG BytesComplete;
    ULONG CurrentValue;
    ULONGLONG MemoryAddress;
    INT Result;
    ULONG Size;

    MemoryAddress = Address;
    if ((Context->MachineType == MACHINE_TYPE_X86) ||
        (Context->MachineType == MACHINE_TYPE_X64)) {

        BreakInstruction = X86_BREAK_INSTRUCTION;
        Size = X86_BREAK_INSTRUCTION_LENGTH;

    } else if (Context->MachineType == MACHINE_TYPE_ARM) {
        if ((Address & ARM_THUMB_BIT) != 0) {
            BreakInstruction = THUMB_BREAK_INSTRUCTION;
            Size = THUMB_BREAK_INSTRUCTION_LENGTH;
            MemoryAddress = Address & ~ARM_THUMB_BIT;

        } else {
            BreakInstruction = ARM_BREAK_INSTRUCTION;
            Size = ARM_BREAK_INSTRUCTION_LENGTH;
        }

    } else {
        DbgOut("Unknown machine type %d.\n", Context->MachineType);
        return EINVAL;
    }

    //
    // Read what's there. If it's not a break instruction, warn the user that
    // perhaps something's wrong.
    //

    CurrentValue = 0;
    Result = DbgReadMemory(Context,
                           TRUE,
                           MemoryAddress,
                           Size,
                           &CurrentValue,
                           &BytesComplete);

    if ((Result != 0) || (BytesComplete != Size)) {
        if (Result == 0) {
            Result = EINVAL;
        }

        goto ClearBreakpointAtAddressEnd;
    }

    if (CurrentValue == OriginalValue) {
        goto ClearBreakpointAtAddressEnd;
    }

    //
    // Write out the original instruction.
    //

    Result = DbgWriteMemory(Context,
                            TRUE,
                            MemoryAddress,
                            Size,
                            &OriginalValue,
                            &BytesComplete);

    if ((Result != 0) || (BytesComplete != Size)) {
        if (Result == 0) {
            Result = EINVAL;
        }

        goto ClearBreakpointAtAddressEnd;
    }

ClearBreakpointAtAddressEnd:
    if ((Result != 0) &&
        (CurrentValue != BreakInstruction) &&
        (CurrentValue != OriginalValue)) {

        DbgOut("Warning: Clearing a breakpoint at address %I64x, but instead "
               "of finding the breakpoint instruction %x at that address, %x "
               "was found instead.\n",
               Address,
               BreakInstruction,
               CurrentValue);
    }

    return Result;
}

ULONG
DbgrpHandleBreakpoints (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine is called when a new break notification comes in. It looks to
    see if any breakpoints were hit, and if they were, backs up the instruction
    pointer and fixes the instruction.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    Returns the breakpoint number of the breakpoint that was hit, or -1 if no
    breakpoint was hit.

--*/

{

    PDEBUGGER_BREAK_POINT Breakpoint;
    ULONG BreakpointNumber;
    PLIST_ENTRY CurrentEntry;
    BOOL Result;
    ULONG Size;

    BreakpointNumber = -1;

    //
    // If there's a breakpoint to restore, do it. This code may restore the
    // breakpoint (as opposed to the code in Continue) if the user requests a
    // single step.
    //

    if (Context->BreakpointToRestore != NULL) {
        Result = DbgrpSetBreakpointAtAddress(
                               Context,
                               Context->BreakpointToRestore->Address,
                               &(Context->BreakpointToRestore->OriginalValue));

        if (Result != 0) {
            DbgOut("Failed to restore breakpoint %d at %I64x.\n",
                   Context->BreakpointToRestore->Index,
                   Context->BreakpointToRestore->Address);

            return -1;
        }

        Context->BreakpointToRestore = NULL;
    }

    //
    // Check the breakpoint list to see if that's the cause of the break, and
    // clean up after it if it is.
    //

    CurrentEntry = Context->BreakpointList.Next;
    while (CurrentEntry != &(Context->BreakpointList)) {
        Breakpoint = LIST_VALUE(CurrentEntry,
                                DEBUGGER_BREAK_POINT,
                                ListEntry);

        if ((Context->MachineType == MACHINE_TYPE_X86) ||
            (Context->MachineType == MACHINE_TYPE_X64)) {

            Size = X86_BREAK_INSTRUCTION_LENGTH;

        } else if (Context->MachineType == MACHINE_TYPE_ARM) {
            if ((Breakpoint->Address & ARM_THUMB_BIT) != 0) {
                Size = THUMB_BREAK_INSTRUCTION_LENGTH;

            } else {
                Size = ARM_BREAK_INSTRUCTION_LENGTH;
            }

        } else {
            DbgOut("Unknown machine type %d.\n", Context->MachineType);
            return -1;
        }

        //
        // If the instruction pointer is exactly after the breakpoint address,
        // then it must have stopped for the breakpoint. Hide the breakpoint
        // from the user.
        //

        if ((Breakpoint->Enabled != FALSE) &&
            (Breakpoint->Type == BreakpointTypeExecution) &&
            (Context->CurrentEvent.BreakNotification.InstructionPointer ==
                                                 Breakpoint->Address + Size)) {

            BreakpointNumber = Breakpoint->Index;
            Result = DbgrpAdjustInstructionPointerForBreakpoint(
                                                    Context,
                                                    Breakpoint->OriginalValue);

            if (Result != 0) {
                DbgOut("Unable to adjust instruction pointer for breakpoint "
                       "%d.\n",
                       Breakpoint->Index);

                goto HandleBreakpointsEnd;
            }

            //
            // Put the right instruction back in memory.
            //

            Result = DbgrpClearBreakpointAtAddress(Context,
                                                   Breakpoint->Address,
                                                   Breakpoint->OriginalValue);

            if (Result != 0) {
                DbgOut("Error: Unable to temporarily clear breakpoint at "
                       "%08x.\n",
                       Breakpoint->Address);

                goto HandleBreakpointsEnd;
            }

            //
            // Mark this breakpoint as needing to be restored.
            //

            Context->BreakpointToRestore = Breakpoint;
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Check the one-time break point.
    //

    if (Context->OneTimeBreakValid != FALSE) {
        if ((Context->MachineType == MACHINE_TYPE_X86) ||
            (Context->MachineType == MACHINE_TYPE_X64)) {

            Size = X86_BREAK_INSTRUCTION_LENGTH;

        } else if (Context->MachineType == MACHINE_TYPE_ARM) {
            if ((Context->OneTimeBreakAddress & ARM_THUMB_BIT) != 0) {
                Size = THUMB_BREAK_INSTRUCTION_LENGTH;

            } else {
                Size = ARM_BREAK_INSTRUCTION_LENGTH;
            }

        } else {
            DbgOut("Unknown machine type %d.\n", Context->MachineType);
            return -1;
        }

        if (Context->CurrentEvent.BreakNotification.InstructionPointer ==
            Context->OneTimeBreakAddress + Size) {

            Result = DbgrpAdjustInstructionPointerForBreakpoint(
                                           Context,
                                           Context->OneTimeBreakOriginalValue);

            if (Result != 0) {
                DbgOut("Error: Failed to adjust instruction pointer for one "
                       "time break.");

                goto HandleBreakpointsEnd;
            }
        }

        //
        // Remove the one time breakpoint.
        //

        Result = DbgrpClearBreakpointAtAddress(
                                           Context,
                                           Context->OneTimeBreakAddress,
                                           Context->OneTimeBreakOriginalValue);

        if (Result != 0) {
            DbgOut("Error: Failed to clear one time break point.\n");
            goto HandleBreakpointsEnd;
        }

        Context->OneTimeBreakValid = FALSE;
    }

HandleBreakpointsEnd:
    return BreakpointNumber;
}

INT
DbgrpAdjustInstructionPointerForBreakpoint (
    PDEBUGGER_CONTEXT Context,
    ULONG OriginalValue
    )

/*++

Routine Description:

    This routine potentially moves the instruction pointer back and hides a
    breakpoint instruction.

Arguments:

    Context - Supplies a pointer to the application context.

    OriginalValue - Supplies the original instruction stream contents at that
        address.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PBREAK_NOTIFICATION BreakNotification;
    ULONG ByteIndex;
    PUSHORT Pointer16;
    PULONG Pointer32;
    BOOL Result;
    ULONG Size;

    assert(Context->CurrentEvent.Type == DebuggerEventBreak);

    BreakNotification = &(Context->CurrentEvent.BreakNotification);
    if (Context->MachineType == MACHINE_TYPE_X86) {
        Size = X86_BREAK_INSTRUCTION_LENGTH;

        //
        // On x86, debug breaks are traps, meaning the instruction pointer
        // points to the next instruction. Move the instruction pointer back
        // and replace the original value in the first position.
        //

        BreakNotification->InstructionPointer -= Size;
        BreakNotification->Registers.X86.Eip -= Size;
        for (ByteIndex = sizeof(BreakNotification->InstructionStream) - 1;
             ByteIndex > 0;
             ByteIndex -= 1) {

            BreakNotification->InstructionStream[ByteIndex] =
                           BreakNotification->InstructionStream[ByteIndex - 1];
        }

        BreakNotification->InstructionStream[0] = (UCHAR)OriginalValue;

    } else if (Context->MachineType == MACHINE_TYPE_X64) {
        Size = X86_BREAK_INSTRUCTION_LENGTH;

        //
        // x64 is just like x86.
        //

        BreakNotification->InstructionPointer -= Size;
        BreakNotification->Registers.X64.Rip -= Size;
        for (ByteIndex = sizeof(BreakNotification->InstructionStream) - 1;
             ByteIndex > 0;
             ByteIndex -= 1) {

            BreakNotification->InstructionStream[ByteIndex] =
                           BreakNotification->InstructionStream[ByteIndex - 1];
        }

        BreakNotification->InstructionStream[0] = (UCHAR)OriginalValue;

    } else if (Context->MachineType == MACHINE_TYPE_ARM) {
        Size = ARM_BREAK_INSTRUCTION_LENGTH;
        if ((BreakNotification->Registers.Arm.Cpsr & PSR_FLAG_THUMB) != 0) {
            Size = THUMB_BREAK_INSTRUCTION_LENGTH;
            Pointer16 = (PUSHORT)(&(BreakNotification->InstructionStream[0]));
            Pointer16[1] = Pointer16[0];
            Pointer16[0] = OriginalValue;

        } else {
            Pointer32 = (PULONG)(&(BreakNotification->InstructionStream[0]));
            *Pointer32 = OriginalValue;
        }

        //
        // On ARM, the break instruction also points to the next one. In this
        // case though the instruction size is known, so there's no need to
        // carefully shift the whole instruction stream buffer.
        //

        BreakNotification->InstructionPointer -= Size;
        BreakNotification->Registers.Arm.R15Pc -= Size;

    } else {
        DbgOut("Unknown machine type %d.\n", Context->MachineType);
        return FALSE;
    }

    //
    // Reflect the register change in the target as well.
    //

    Result = DbgSetRegisters(Context, &(BreakNotification->Registers));
    if (Result != 0) {
        DbgOut("Error adjusting EIP on breakpoint instruction.\n");
        return Result;
    }

    return 0;
}

INT
DbgrpProcessBreakNotification (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine is called when a new break notification comes in.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG BreakpointNumber;
    PBREAK_NOTIFICATION CurrentBreak;
    BOOL ForceModuleUpdate;
    BOOL InRange;
    ULONGLONG InstructionPointer;
    INT Result;

    Context->TargetFlags &= ~DEBUGGER_TARGET_RUNNING;

    //
    // Synchronize symbols with the target.
    //

    CurrentBreak = &(Context->CurrentEvent.BreakNotification);
    ForceModuleUpdate = FALSE;
    if (Context->PreviousProcess != CurrentBreak->Process) {
        ForceModuleUpdate = TRUE;
    }

    Context->PreviousProcess = CurrentBreak->Process;
    Result = DbgrpValidateLoadedModules(Context,
                                        CurrentBreak->LoadedModuleCount,
                                        CurrentBreak->LoadedModuleSignature,
                                        ForceModuleUpdate);

    if (Result != 0) {
        DbgOut("Failed to validate loaded modules.\n");
    }

    //
    // Handle any breakpoint stuff.
    //

    BreakpointNumber = DbgrpHandleBreakpoints(Context);
    InstructionPointer = CurrentBreak->InstructionPointer;

    //
    // Print the exception.
    //

    switch (CurrentBreak->Exception) {
    case ExceptionDebugBreak:
    case ExceptionSingleStep:
    case ExceptionSignal:
        if (CurrentBreak->Exception == ExceptionSignal) {
            if (Context->CurrentEvent.SignalParameters.SignalNumber !=
                SIGNAL_TRAP) {

                DbgOut("Caught signal %d.\n",
                       Context->CurrentEvent.SignalParameters.SignalNumber);
            }
        }

        if (BreakpointNumber != -1) {
            DbgOut("Breakpoint %d hit!\n", BreakpointNumber);

        //
        // If the range step is valid, then only break if the address qualifies.
        //

        } else if (Context->RangeStepValid != FALSE) {
            InRange = FALSE;
            if ((InstructionPointer >=
                 Context->RangeStepParameters.BreakRangeMinimum) &&
                (InstructionPointer <
                 Context->RangeStepParameters.BreakRangeMaximum)) {

                InRange = TRUE;
                if ((InstructionPointer >=
                     Context->RangeStepParameters.RangeHoleMinimum) &&
                    (InstructionPointer <
                     Context->RangeStepParameters.RangeHoleMaximum)) {

                    InRange = FALSE;
                }
            }

            if (InRange == FALSE) {
                Result = DbgrSingleStep(Context);
                if (Result != 0) {
                    DbgOut("Failed to single step over %I64x.\n",
                           InstructionPointer);

                    goto ProcessBreakNotificationEnd;
                }

                Context->TargetFlags |= DEBUGGER_TARGET_RUNNING;
                goto ProcessBreakNotificationEnd;
            }
        }

        break;

    case ExceptionAssertionFailure:
        break;

    case ExceptionAccessViolation:
        DbgOut("\n *** Access violation: Error code 0x%08x ***\n",
               CurrentBreak->ErrorCode);

        break;

    case ExceptionDoubleFault:
        DbgOut("\n *** Double Fault ***\n");
        break;

    case ExceptionInvalid:
        DbgOut("Error: Invalid exception received!\n");
        break;

    case ExceptionIllegalInstruction:
        DbgOut("\n *** Illegal Instruction ***\n");
        break;

    case ExceptionUnknown:
        DbgOut("Error: Unknown exception received!\n");
        break;

    default:
        DbgOut("Error: Unknown exception %d received!\n",
               CurrentBreak->Exception);

        break;
    }

    //
    // This break is really going to the user. Turn off any range stepping.
    //

    Context->RangeStepValid = FALSE;

    //
    // Set the globals indicating where to disassemble from and where
    // the current frame is.
    //

    Context->DisassemblyAddress = InstructionPointer;
    RtlCopyMemory(&(Context->FrameRegisters),
                  &(Context->CurrentEvent.BreakNotification.Registers),
                  sizeof(REGISTERS_UNION));

    Context->CurrentFrame = 0;
    Context->LastMemoryDump.Virtual = TRUE;
    Context->LastMemoryDump.NextAddress = InstructionPointer;
    Context->LastMemoryDump.Columns = 0;
    Context->LastMemoryDump.TotalValues = 0;
    Context->LastMemoryDump.PrintCharacters = TRUE;

    //
    // Load up the source file in the source window.
    //

    DbgrShowSourceAtAddress(Context, InstructionPointer);

    //
    // Print the instruction that's about to execute.
    //

    DbgrpPrintDisassembly(Context,
                          CurrentBreak->InstructionStream,
                          InstructionPointer,
                          1,
                          &(Context->BreakInstructionLength));

    Result = 0;

ProcessBreakNotificationEnd:
    return Result;
}

INT
DbgrpRangeStep (
    PDEBUGGER_CONTEXT Context,
    PRANGE_STEP RangeStep
    )

/*++

Routine Description:

    This routine continues execution until a range of execution addresses is
    reached.

Arguments:

    Context - Supplies a pointer to the application context.

    RangeStep - Supplies a pointer to the range to go to.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    BOOL Result;
    ULONG SignalToDeliver;

    //
    // First attempt to use the direct API method. The kernel debugger for
    // instance has support for doing range stepping on its own so it doesn't
    // have to send break notifications back and forth for every singls step.
    //

    SignalToDeliver = DbgGetSignalToDeliver(Context);
    Result = DbgRangeStep(Context, RangeStep, SignalToDeliver);
    if (Result == 0) {
        return 0;
    }

    //
    // The API is unavailable, so it's going to have to be done the old
    // fashioned way.
    //

    RtlCopyMemory(&(Context->RangeStepParameters),
                  RangeStep,
                  sizeof(RANGE_STEP));

    Context->RangeStepValid = TRUE;
    return DbgrSingleStep(Context);
}

INT
DbgrpValidateLoadedModules (
    PDEBUGGER_CONTEXT Context,
    ULONG ModuleCount,
    ULONGLONG Signature,
    BOOL ForceReload
    )

/*++

Routine Description:

    This routine validates that the debugger's list of loaded modules is in sync
    with the target. This routine may load or unload symbols.

Arguments:

    Context - Supplies a pointer to the application context.

    ModuleCount - Supplies the number of modules loaded in the target.

    Signature - Supplies the total of the timestamps and loaded addresses of
        all loaded modules. This is used esseentially as a quick way to
        determine whether or not the list is in sync without transmitting much
        data.

    ForceReload - Supplies a boolean indicating if the module list should be
        reloaded no matter what.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    BOOL AlreadyPrinted;
    ULONG BinaryNameLength;
    PLIST_ENTRY CurrentEntry;
    PLOADED_MODULE_ENTRY CurrentTargetModule;
    PDEBUGGER_MODULE ExistingModule;
    PSTR FriendlyName;
    ULONG FriendlyNameLength;
    PSTR FriendlyNameStart;
    PMODULE_LIST_HEADER ModuleListHeader;
    ULONG RemoteModuleCount;
    INT Result;

    AlreadyPrinted = FALSE;
    ModuleListHeader = NULL;

    //
    // If the two checksum totals match, then the debugger symbols are in sync.
    // No action is required.
    //

    if ((Signature == Context->ModuleList.Signature) &&
        (ModuleCount == Context->ModuleList.ModuleCount) &&
        (ForceReload == FALSE)) {

        Result = 0;
        goto ValidateLoadedModulesEnd;
    }

    //
    // If the signature hasn't changed since the last time it wasn't in sync,
    // don't bother going through all that again.
    //

    if ((Signature == Context->RemoteModuleListSignature) &&
        (ForceReload == FALSE)) {

        AlreadyPrinted = TRUE;
        Result = 0;
        goto ValidateLoadedModulesEnd;
    }

    //
    // Request the loaded modules list header to determine how many modules
    // are loaded in the target.
    //

    Result = DbgGetLoadedModuleList(Context, &ModuleListHeader);
    if (Result != 0) {
        DbgOut("Error: Failed to get loaded module list.\n");
        goto ValidateLoadedModulesEnd;
    }

    Signature = ModuleListHeader->Signature;
    RemoteModuleCount = ModuleListHeader->ModuleCount;
    CurrentTargetModule = (PLOADED_MODULE_ENTRY)(ModuleListHeader + 1);

    //
    // Mark all modules as unloaded.
    //

    CurrentEntry = Context->ModuleList.ModulesHead.Next;
    while (CurrentEntry != &(Context->ModuleList.ModulesHead)) {
        ExistingModule = LIST_VALUE(CurrentEntry,
                                    DEBUGGER_MODULE,
                                    ListEntry);

        ExistingModule->Loaded = FALSE;
        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Get all modules. For each module, attempt to match it to an existing
    // loaded module. If none is found, attempt to load the module.
    //

    while (RemoteModuleCount != 0) {

        //
        // Create a friendly name from the binary name by chopping off the
        // extension and any path stuff at the beginning.
        //

        assert(CurrentTargetModule->StructureSize >=
               sizeof(LOADED_MODULE_ENTRY));

        BinaryNameLength = CurrentTargetModule->StructureSize -
                           sizeof(LOADED_MODULE_ENTRY) +
                           (ANYSIZE_ARRAY * sizeof(CHAR));

        DbgpGetFriendlyName(CurrentTargetModule->BinaryName,
                            BinaryNameLength,
                            &FriendlyNameStart,
                            &FriendlyNameLength);

        FriendlyName = malloc(FriendlyNameLength + 1);
        if (FriendlyName == NULL) {
            DbgOut("Error: Failed to allocate %d bytes for friendly name.\n",
                   FriendlyNameLength + 1);

            Result = ENOMEM;
            goto ValidateLoadedModulesEnd;
        }

        RtlStringCopy(FriendlyName, FriendlyNameStart, FriendlyNameLength + 1);
        FriendlyName[FriendlyNameLength] = '\0';
        ExistingModule = DbgpFindModuleFromEntry(Context, CurrentTargetModule);
        if (ExistingModule != NULL) {
            ExistingModule->Loaded = TRUE;

        } else {
            DbgpLoadModule(Context,
                           CurrentTargetModule->BinaryName,
                           FriendlyName,
                           CurrentTargetModule->Size,
                           CurrentTargetModule->LowestAddress,
                           CurrentTargetModule->Timestamp,
                           CurrentTargetModule->Process);
        }

        free(FriendlyName);
        RemoteModuleCount -= 1;
        CurrentTargetModule =
            (PLOADED_MODULE_ENTRY)((PUCHAR)CurrentTargetModule +
                                   CurrentTargetModule->StructureSize);
    }

    //
    // Unload any modules no longer in the list.
    //

    CurrentEntry = Context->ModuleList.ModulesHead.Next;
    while (CurrentEntry != &(Context->ModuleList.ModulesHead)) {
        ExistingModule = LIST_VALUE(CurrentEntry,
                                    DEBUGGER_MODULE,
                                    ListEntry);

        CurrentEntry = CurrentEntry->Next;
        if (ExistingModule->Loaded == FALSE) {
            DbgrpUnloadModule(Context, ExistingModule, TRUE);
        }
    }

    Context->RemoteModuleListSignature = Signature;
    Result = 0;

ValidateLoadedModulesEnd:
    if (Result == 0) {
        if ((Context->ModuleList.Signature != Signature) &&
            (AlreadyPrinted == FALSE)) {

            DbgOut("*** Module signatures don't match after synchronization. "
                   "***\nDebugger: 0x%I64x, Target: 0x%I64x\n",
                   Context->ModuleList.Signature,
                   Signature);
        }
    }

    if (ModuleListHeader != NULL) {
        free(ModuleListHeader);
    }

    return Result;
}

VOID
DbgrpUnloadModule (
    PDEBUGGER_CONTEXT Context,
    PDEBUGGER_MODULE Module,
    BOOL Verbose
    )

/*++

Routine Description:

    This routine unloads a module, removing its binary and symbol information.

Arguments:

    Context - Supplies a pointer to the application context.

    Module - Supplies a pointer to the loaded module to unload. Once this
        function is called, this pointer will no longer be valid.

    Verbose - Supplies a flag indicating whether or not this unload should be
        printed to the user.

Return Value:

    None.

--*/

{

    //
    // Subtract the checksum out of the checksum total, and remove the module
    // from the list.
    //

    Context->ModuleList.Signature -= Module->Timestamp + Module->LowestAddress;
    Context->ModuleList.ModuleCount -= 1;
    LIST_REMOVE(&(Module->ListEntry));
    if (Verbose != FALSE) {
        DbgOut("Module unloaded: %s.\n", Module->ModuleName);
    }

    //
    // Free all the symbols.
    //

    if (Module->Symbols != NULL) {
        DbgUnloadSymbols(Module->Symbols);
        Module->Symbols = NULL;
    }

    if (Module->Filename != NULL) {
        free(Module->Filename);
    }

    if (Module->ModuleName != NULL) {
        free(Module->ModuleName);
    }

    free(Module);
    return;
}

VOID
DbgrpUnloadAllModules (
    PDEBUGGER_CONTEXT Context,
    BOOL Verbose
    )

/*++

Routine Description:

    This routine unloads all modules and symbols from the debugger.

Arguments:

    Context - Supplies a pointer to the application context.

    Verbose - Supplies a flag indicating whether or not this unload should be
        printed to the user.

Return Value:

    Returns TRUE if information was printed, or FALSE if module information
    could not be found for the address.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDEBUGGER_MODULE CurrentModule;

    if (Context->ModuleList.ModuleCount == 0) {

        assert(Context->ModuleList.Signature == 0);
        assert(Context->RemoteModuleListSignature == 0);

        return;
    }

    CurrentEntry = Context->ModuleList.ModulesHead.Next;
    while (CurrentEntry != &(Context->ModuleList.ModulesHead)) {
        CurrentModule = LIST_VALUE(CurrentEntry, DEBUGGER_MODULE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        DbgrpUnloadModule(Context, CurrentModule, Verbose);
    }

    assert(Context->ModuleList.ModuleCount == 0);
    assert(Context->ModuleList.Signature == 0);

    Context->RemoteModuleListSignature = 0;
    return;
}

INT
DbgrpPrintDisassembly (
    PDEBUGGER_CONTEXT Context,
    PBYTE InstructionStream,
    ULONGLONG InstructionPointer,
    ULONG InstructionCount,
    PULONG BytesDecoded
    )

/*++

Routine Description:

    This routine prints the disassembly of one or more instructions.

Arguments:

    Context - Supplies a pointer to the application context.

    InstructionStream - Supplies a pointer to the binary instruction stream to
        disassemble. It is assumed that this buffer contains enough bytes to
        completely disassemble the specified number of instructions.

    InstructionPointer - Supplies the location in the target's memory where
        these instructions reside.

    InstructionCount - Supplies the number of instructions to disassemble.

    BytesDecoded - Supplies a pointer where the number of bytes decoded from the
        instruction stream is returned.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG CurrentInstructionByte;
    DISASSEMBLED_INSTRUCTION Disassembly;
    PSTR DisassemblyBuffer;
    MACHINE_LANGUAGE Language;
    ULONGLONG OperandAddress;
    INT Result;

    if (BytesDecoded != NULL) {
        *BytesDecoded = 0;
    }

    DisassemblyBuffer = malloc(200);
    if (DisassemblyBuffer == NULL) {
        Result = ENOMEM;
        goto PrintDisassemblyEnd;
    }

    switch (Context->MachineType) {
    case MACHINE_TYPE_X86:
        Language = MachineLanguageX86;
        break;

    case MACHINE_TYPE_ARM:
        Language = MachineLanguageArm;
        if ((InstructionPointer & ARM_THUMB_BIT) != 0) {
            Language = MachineLanguageThumb2;
        }

        break;

    case MACHINE_TYPE_X64:
        Language = MachineLanguageX64;
        break;

    default:
        DbgOut("Error: Unknown machine type %d.\n", Context->MachineType);
        Result = EINVAL;
        goto PrintDisassemblyEnd;
    }

    Result = DbgPrintAddressSymbol(Context, InstructionPointer);
    if (Result == 0) {
        DbgOut(":\n");
    }

    while (InstructionCount > 0) {

        //
        // Print the instruction pointer and attempt to decode the instruction.
        //

        DbgOut("%08I64x ", InstructionPointer);
        Result = DbgDisassemble(InstructionPointer,
                                InstructionStream,
                                DisassemblyBuffer,
                                200,
                                &Disassembly,
                                Language);

        if (Result == FALSE) {
            DbgOut("*** Error decoding instruction ***\n");
            Result = EINVAL;
            goto PrintDisassemblyEnd;
        }

        if ((Language == MachineLanguageArm) ||
            (Language == MachineLanguageThumb2)) {

            if (Disassembly.BinaryLength == 2) {
                DbgOut("%04x      ", *((PUSHORT)InstructionStream));

            } else {

                assert(Disassembly.BinaryLength == 4);

                if (Language == MachineLanguageThumb2) {
                    DbgOut("%04x %04x ",
                           *((PUSHORT)InstructionStream),
                           *((PUSHORT)InstructionStream + 1));

                } else {
                    DbgOut("%08x ", *((PULONG)InstructionStream));
                }
            }
        }

        DbgOut("%s\t", Disassembly.Mnemonic);

        //
        // Print the first (destination) operand if one exists.
        //

        if (Disassembly.DestinationOperand != NULL) {
            if ((Disassembly.AddressIsDestination != FALSE) &&
                (Disassembly.AddressIsValid != FALSE)) {

                OperandAddress = Disassembly.OperandAddress;
                Result = DbgPrintAddressSymbol(Context, OperandAddress);
                if (Result == 0) {
                    DbgOut(" ");
                }

                if (Disassembly.DestinationOperand[0] == '[') {
                    DbgOut("%s", Disassembly.DestinationOperand);

                } else {
                    DbgOut("%s (0x%08I64x)",
                           Disassembly.DestinationOperand,
                           OperandAddress);
                }

            } else {
                DbgOut("%s", Disassembly.DestinationOperand);
            }
        }

        //
        // Print the second (source) operand if one exists.
        //

        if (Disassembly.SourceOperand != NULL) {
            DbgOut(", ");
            if ((Disassembly.AddressIsDestination == FALSE) &&
                (Disassembly.AddressIsValid != FALSE)) {

                OperandAddress = Disassembly.OperandAddress;
                Result = DbgPrintAddressSymbol(Context, OperandAddress);
                if (Result == 0) {
                    DbgOut(" ");
                }

                if (Disassembly.SourceOperand[0] == '[') {
                    DbgOut("%s", Disassembly.SourceOperand);

                } else {
                    DbgOut("%s (0x%08I64x)",
                           Disassembly.SourceOperand,
                           OperandAddress);
                }

            } else {
                DbgOut("%s", Disassembly.SourceOperand);
            }
        }

        //
        // Print the third operand if one exists. This operand will not have a
        // symbol associated with it.
        //

        if (Disassembly.ThirdOperand != NULL) {
            DbgOut(", %s", Disassembly.ThirdOperand);
        }

        //
        // Print the fourth operand if one exists. This one also will not have
        // a symbol associated with it.
        //

        if (Disassembly.FourthOperand != NULL) {
            DbgOut(", %s", Disassembly.FourthOperand);
        }

        //
        // For x86 disassembly, print out the bytes of the actual instruction.
        //

        if ((Language == MachineLanguageX86) ||
            (Language == MachineLanguageX64)) {

            DbgOut("\t; ");
            for (CurrentInstructionByte = 0;
                 CurrentInstructionByte < Disassembly.BinaryLength;
                 CurrentInstructionByte += 1) {

                DbgOut("%02x", *InstructionStream);
                InstructionStream += 1;
            }

        } else {
            InstructionStream += Disassembly.BinaryLength;
        }

        DbgOut("\n");

        //
        // Update the variables and loop.
        //

        if (BytesDecoded != NULL) {
            *BytesDecoded += Disassembly.BinaryLength;
        }

        InstructionPointer += Disassembly.BinaryLength;
        InstructionCount -= 1;
    }

    Result = 0;

PrintDisassemblyEnd:
    if (DisassemblyBuffer != NULL) {
        free(DisassemblyBuffer);
    }

    return Result;
}

PSTR
DbgrpCreateFullPath (
    PSOURCE_FILE_SYMBOL Source
    )

/*++

Routine Description:

    This routine makes a full source file path from the given source file. The
    caller must remember to free memory allocated here.

Arguments:

    Source - Supplies a pointer to the source line symbol.

Return Value:

    Returns a pointer to string containing the full path, or NULL if there was
    an error. This memory has been allocated in the function, and must be freed
    by the caller.

--*/

{

    UINTN DirectoryLength;
    UINTN FileLength;
    ULONG Index;
    CHAR LastCharacter;
    BOOL NeedsSlash;
    PSTR Path;
    ULONG PathLength;

    //
    // Validate parameters.
    //

    if ((Source == NULL) || (Source->SourceFile == NULL)) {
        return NULL;
    }

    DirectoryLength = 0;
    FileLength = strlen(Source->SourceFile);
    PathLength = FileLength;
    NeedsSlash = FALSE;

    //
    // Get the length of the full path, depending on whether or not a directory
    // was specified.
    //

    if (Source->SourceDirectory != NULL) {
        DirectoryLength = strlen(Source->SourceDirectory);
        PathLength += DirectoryLength;
        if (DirectoryLength != 0) {
            LastCharacter = Source->SourceDirectory[DirectoryLength - 1];
            if ((LastCharacter != '/') && (LastCharacter != '\\')) {
                NeedsSlash = TRUE;
                PathLength += 1;
            }
        }
    }

    //
    // Allocate the buffer.
    //

    Path = malloc(PathLength + 1);
    if (Path == NULL) {
        return NULL;
    }

    if (Source->SourceDirectory != NULL) {
        memcpy(Path, Source->SourceDirectory, DirectoryLength);
        if (NeedsSlash != FALSE) {
            Path[DirectoryLength] = '/';
            DirectoryLength += 1;
        }
    }

    memcpy(Path + DirectoryLength, Source->SourceFile, FileLength + 1);

    //
    // Change any backslashes to forward slashes.
    //

    for (Index = 0; Index < PathLength; Index += 1) {
        if (Path[Index] == '\\') {
            Path[Index] = '/';
        }
    }

    return Path;
}

INT
DbgrpPrintMemory (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    BOOL VirtualAddress,
    ULONG TypeSize,
    ULONG Columns,
    ULONG TotalValues,
    BOOL PrintCharacters
    )

/*++

Routine Description:

    This routine prints the contents of memory in a formatted way.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address of the memory in the debuggee to print.

    VirtualAddress - Supplies a flag indicating whether or not Address is a
        virtual address (TRUE) or physical address (FALSE).

    TypeSize - Supplies the unit of integers to print, in bytes. Valid values
        are 1, 2, 4, and 8.

    Columns - Supplies the number of columns to print for each line. To use a
        default value, set this parameter to 0.

    TotalValues - Supplies the total number of integers to print (the total
        amount of memory will depend on the type size as well). To use a default
        value, set this parameter to 0.

    PrintCharacters - Supplies a flag indicating whether or not to print
        characters alongside the integers.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PUCHAR Buffer;
    PUCHAR CharacterPointer;
    ULONG ColumnIndex;
    ULONGLONG CurrentAddress;
    PUCHAR CurrentByte;
    PUCHAR InvalidBytes;
    ULONG ItemsPrinted;
    INT Result;
    ULONG ValidBytes;

    Buffer = NULL;
    ItemsPrinted = 0;

    //
    // If the number of columns was 0, then pick a default number of columns to
    // print.
    //

    if (Columns == 0) {
        Columns = 2;
        if (TypeSize == 4) {
            Columns *= 2;
        }

        if (TypeSize == 2) {
            Columns *= 4;
        }

        if (TypeSize == 1) {
            Columns *= 8;
        }
    }

    //
    // If the number of items was 0, then pick a default number of items to
    // print.
    //

    if (TotalValues == 0) {
        TotalValues = Columns * DEFAULT_MEMORY_PRINT_ROWS;
    }

    //
    // Allocate a buffer big enough to hold all the values.
    //

    Buffer = malloc(TotalValues * TypeSize);
    if (Buffer == NULL) {
        Result = ENOMEM;
        goto PrintMemoryEnd;
    }

    //
    // Read the memory in from the debuggee.
    //

    Result = DbgReadMemory(Context,
                           VirtualAddress,
                           Address,
                           TotalValues * TypeSize,
                           Buffer,
                           &ValidBytes);

    if (Result != 0) {
        DbgOut("Error retrieving memory!\n");
        goto PrintMemoryEnd;
    }

    InvalidBytes = Buffer + ValidBytes;

    //
    // Print every value.
    //

    ColumnIndex = 0;
    CurrentAddress = Address;
    CurrentByte = Buffer;
    CharacterPointer = Buffer;
    for (ItemsPrinted = 0; ItemsPrinted < TotalValues; ItemsPrinted += 1) {

        //
        // If this is the beginning of a new column, print the address.
        //

        if (ColumnIndex == 0) {
            DbgOut("%08I64x: ", CurrentAddress);
        }

        //
        // Depending on the size, print the value.
        //

        if (TypeSize == 8) {
            if (CurrentByte + 7 >= InvalidBytes) {
                DbgOut("????????`????????  ");

            } else {
                DbgOut("%08x`%08x  ",
                       *((PULONG)CurrentByte + 1),
                       *((PULONG)CurrentByte));
            }

        } else if (TypeSize == 4) {
            if (CurrentByte + 3 >= InvalidBytes) {
                DbgOut("???????? ");

            } else {
                DbgOut("%08x ", *((PULONG)CurrentByte));
            }

        } else if (TypeSize == 2) {
            if (CurrentByte + 1 >= InvalidBytes) {
                DbgOut("???? ");

            } else {
                DbgOut("%04x ", *((PUSHORT)CurrentByte));
            }

        } else if (TypeSize == 1) {
            if (ColumnIndex == 7) {
                if (CurrentByte >= InvalidBytes) {
                    DbgOut("?""?-");

                } else {
                    DbgOut("%02x-", *CurrentByte);
                }

            } else {
                if (CurrentByte >= InvalidBytes) {
                    DbgOut("?? ");

                } else {
                    DbgOut("%02x ", *CurrentByte);
                }
            }
        }

        //
        // Advance all the pointers and whatnot.
        //

        CurrentByte += TypeSize;
        ColumnIndex += 1;
        CurrentAddress += TypeSize;

        //
        // If this is the last column in the row, and characters are to be
        // printed, print them.
        //

        if (ColumnIndex == Columns) {
            if (PrintCharacters != FALSE) {
                DbgOut(" ");
                for (ColumnIndex = 0;
                     ColumnIndex < (Columns * TypeSize);
                     ColumnIndex += 1) {

                    //
                    // If the character is printable, print it. Otherwise, print
                    // a dot. If the memory doesn't exist, print a question
                    // mark.
                    //

                    if (CharacterPointer >= InvalidBytes) {
                        DbgOut("?");

                    } else if (*CharacterPointer < 0x20) {
                        DbgOut(".");

                    } else {
                        DbgOut("%c", *CharacterPointer);
                    }

                    CharacterPointer += 1;
                }

                assert(CharacterPointer == CurrentByte);

            }

            ColumnIndex = 0;
            DbgOut("\n");
        }
    }

    //
    // Print one more newline if a column was not complete.
    //

    if (ColumnIndex != 0) {
        DbgOut("\n");
    }

    Result = 0;

PrintMemoryEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    return Result;
}

VOID
DbgrpProcessShutdown (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine processes a shutdown event coming from the debuggee.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    assert(Context->CurrentEvent.Type == DebuggerEventShutdown);

    switch (Context->CurrentEvent.ShutdownNotification.ShutdownType) {
    case ShutdownTypeTransition:
        DbgOut("Target disconnected.\n");
        DbgrConnect(Context);
        break;

    case ShutdownTypeExit:
        DbgOut("Process %x exited with status %d.\n",
               Context->CurrentEvent.ShutdownNotification.Process,
               Context->CurrentEvent.ShutdownNotification.ExitStatus);

        break;

    case ShutdownTypeSynchronizationLost:
        DbgOut("Resynchronizing...\n");
        DbgrConnect(Context);
        break;

    default:
        DbgOut("Shutdown occurred, unknown reason %d.\n",
               Context->CurrentEvent.ShutdownNotification.ShutdownType);

        break;
    }

    if (Context->CurrentEvent.ShutdownNotification.UnloadAllSymbols != FALSE) {
        DbgrpUnloadAllModules(Context, TRUE);
    }

    return;
}

PDEBUGGER_MODULE
DbgpLoadModule (
    PDEBUGGER_CONTEXT Context,
    PSTR BinaryName,
    PSTR FriendlyName,
    ULONGLONG Size,
    ULONGLONG LowestAddress,
    ULONGLONG Timestamp,
    ULONG Process
    )

/*++

Routine Description:

    This routine loads a new module and adds it to the debugger's loaded module
    list.

Arguments:

    Context - Supplies a pointer to the application context.

    BinaryName - Supplies the name of the binary to load. This routine will use
        the known symbol path to try to find this binary.

    FriendlyName - Supplies the friendly name of the module.

    Size - Supplies the size of the module.

    LowestAddress - Supplies the lowest address of the module in memory.

    Timestamp - Supplies the timestamp of the module.

    Checksum - Supplies the required checksum of the module.

    Process - Supplies the ID of the process this module is specific to.

Return Value:

    Returns a pointer to the loaded module on success.

    NULL on failure.

--*/

{

    PSTR BackupPotential;
    ULONGLONG BackupPotentialTimestamp;
    PSTR CurrentPath;
    ULONG CurrentPathLength;
    ULONGLONG Delta;
    IMAGE_MACHINE_TYPE ImageMachineType;
    UINTN LastIndex;
    ULONG NameIndex;
    ULONG NameLength;
    PDEBUGGER_MODULE NewModule;
    PSTR OriginalBinaryName;
    ULONG PathCount;
    ULONG PathIndex;
    PSTR PotentialBinary;
    ULONGLONG PotentialTimestamp;
    BOOL Result;
    struct stat Stat;
    INT Status;
    time_t Time;
    char *TimeString;
    struct tm *TimeStructure;

    OriginalBinaryName = BinaryName;
    BackupPotential = NULL;
    BackupPotentialTimestamp = 0;

    //
    // Determine the image machine type.
    //

    switch (Context->MachineType) {
    case MACHINE_TYPE_X86:
        ImageMachineType = ImageMachineTypeX86;
        break;

    case MACHINE_TYPE_ARM:
        ImageMachineType = ImageMachineTypeArm32;
        break;

    case MACHINE_TYPE_X64:
        ImageMachineType = ImageMachineTypeX64;
        break;

    default:
        ImageMachineType = ImageMachineTypeUnknown;
        break;
    }

    //
    // Create an entry for the module.
    //

    NewModule = malloc(sizeof(DEBUGGER_MODULE));
    if (NewModule == NULL) {
        Result = FALSE;
        goto LoadModuleEnd;
    }

    memset(NewModule, 0, sizeof(DEBUGGER_MODULE));
    NameLength = strlen(BinaryName);
    if (NameLength == 0) {
        Result = FALSE;
        goto LoadModuleEnd;
    }

    PathCount = Context->SymbolPathCount;

    //
    // Find the base name to stick on the path.
    //

    NameIndex = NameLength - 1;
    while ((NameIndex != 0) && (BinaryName[NameIndex - 1] != '/') &&
           (BinaryName[NameIndex - 1] != '\\')) {

        NameIndex -= 1;
    }

    BinaryName += NameIndex;
    NameLength -= NameIndex;

    //
    // Attempt to load the binary using each path in the symbol path.
    //

    for (PathIndex = 0; PathIndex < PathCount; PathIndex += 1) {
        CurrentPath = Context->SymbolPath[PathIndex];
        CurrentPathLength = strlen(CurrentPath);

        //
        // Create the full binary path.
        //

        PotentialBinary = malloc(CurrentPathLength + NameLength + 2);
        if (PotentialBinary == NULL) {
            DbgOut("Error: Could not allocate memory for potential binary.\n");
            Result = FALSE;
            goto LoadModuleEnd;
        }

        if (CurrentPathLength != 0) {
            memcpy(PotentialBinary, CurrentPath, CurrentPathLength);
            PotentialBinary[CurrentPathLength] = '/';
            CurrentPathLength += 1;
        }

        strcpy(PotentialBinary + CurrentPathLength, BinaryName);
        Status = stat(PotentialBinary, &Stat);
        if (Status == 0) {

            //
            // Compare the timestamps. Allow for a difference of one because
            // some file systems like FAT only store modification date to
            // two second granules.
            //

            PotentialTimestamp = Stat.st_mtime - SYSTEM_TIME_TO_EPOCH_DELTA;
            if ((Timestamp == 0) ||
                (PotentialTimestamp == Timestamp) ||
                (PotentialTimestamp + 1 == Timestamp) ||
                (PotentialTimestamp - 1 == Timestamp)) {

                //
                // The file name and timestamps match, try to load symbols in
                // this file.
                //

                Status = DbgLoadSymbols(PotentialBinary,
                                        ImageMachineType,
                                        Context,
                                        &(NewModule->Symbols));

                if (Status == 0) {
                    NewModule->Timestamp = Timestamp;
                    NewModule->Filename = PotentialBinary;
                    break;
                }

            //
            // The name matches, but the timestamp doesn't. If nothing better
            // is found keep this around to try at the end.
            //

            } else if (BackupPotential == NULL) {
                BackupPotential = PotentialBinary;
                BackupPotentialTimestamp = Stat.st_mtime -
                                           SYSTEM_TIME_TO_EPOCH_DELTA;

                PotentialBinary = NULL;
            }
        }

        if (PotentialBinary != NULL) {
            free(PotentialBinary);
        }
    }

    //
    // Attempt to load the binary without any symbol path.
    //

    if (NewModule->Symbols == NULL) {
        Status = stat(OriginalBinaryName, &Stat);
        if (Status == 0) {
            PotentialTimestamp = Stat.st_mtime - SYSTEM_TIME_TO_EPOCH_DELTA;
            NewModule->Filename = strdup(OriginalBinaryName);
            if (NewModule->Filename == NULL) {
                DbgOut("Error: Unable to allocate space for filename.\n");
                Result = FALSE;
                goto LoadModuleEnd;
            }

            //
            // Attempt to load symbols if the timestamps match.
            //

            if ((Timestamp == 0) ||
                (PotentialTimestamp == Timestamp) ||
                (PotentialTimestamp + 1 == Timestamp) ||
                (PotentialTimestamp - 1 == Timestamp)) {

                Status = DbgLoadSymbols(NewModule->Filename,
                                        ImageMachineType,
                                        Context,
                                        &(NewModule->Symbols));

                if (Status == 0) {

                    //
                    // A module was successfully loaded this way, so skip the
                    // symbol path searching.
                    //

                    PathCount = 0;
                    NewModule->Timestamp = Timestamp;

                } else {
                    free(NewModule->Filename);
                    NewModule->Filename = NULL;
                }

            //
            // The timestamps don't match but the name does, save this as a
            // backup if there's nothing better.
            //

            } else if (BackupPotential == NULL) {
                BackupPotential = NewModule->Filename;
                BackupPotentialTimestamp = PotentialTimestamp;
                NewModule->Filename = NULL;
            }
        }
    }

    //
    // If nothing was found but there's a backup, try the backup.
    //

    if ((NewModule->Symbols == NULL) && (BackupPotential != NULL)) {
        Status = DbgLoadSymbols(BackupPotential,
                                ImageMachineType,
                                Context,
                                &(NewModule->Symbols));

        if (Status == 0) {

            //
            // Warn the user that a module with a different timestamp is being
            // loaded, as it could easily mean the symbols are out of sync.
            // Don't bother warning them for timestamps that are different by 2
            // seconds or less, as some file systems (FAT) and compression
            // formats (ZIP) only have 2-second resolution.
            //

            Delta = BackupPotentialTimestamp - Timestamp;
            if (Timestamp > BackupPotentialTimestamp) {
                Delta = Timestamp - BackupPotentialTimestamp;
            }

            if (Delta > 2) {
                Time = Timestamp + SYSTEM_TIME_TO_EPOCH_DELTA;
                TimeStructure = localtime(&Time);
                TimeString = asctime(TimeStructure);
                LastIndex = strlen(TimeString);
                if (LastIndex != 0) {
                    if (TimeString[LastIndex - 1] == '\n') {
                        TimeString[LastIndex - 1] = '\0';
                    }
                }

                DbgOut("Warning: Target timestamp for %s is %s\n",
                       FriendlyName,
                       TimeString);

                Time = BackupPotentialTimestamp + SYSTEM_TIME_TO_EPOCH_DELTA;
                TimeStructure = localtime(&Time);
                TimeString = asctime(TimeStructure);
                LastIndex = strlen(TimeString);
                if (LastIndex != 0) {
                    if (TimeString[LastIndex - 1] == '\n') {
                        TimeString[LastIndex - 1] = '\0';
                    }
                }

                DbgOut("but file '%s' has timestamp %s.\n",
                       BackupPotential,
                       TimeString);
            }

            NewModule->Filename = BackupPotential;
            NewModule->Timestamp = Timestamp;
            BackupPotential = NULL;
        }
    }

    //
    // Populate the other fields of the module.
    //

    NameLength = strlen(FriendlyName);
    if (NameLength == 0) {
        Result = FALSE;
        goto LoadModuleEnd;
    }

    NewModule->ModuleName = malloc(NameLength + 1);
    if (NewModule->ModuleName == NULL) {
        Result = FALSE;
        goto LoadModuleEnd;
    }

    strcpy(NewModule->ModuleName, FriendlyName);
    NewModule->LowestAddress = LowestAddress;
    NewModule->Size = Size;
    NewModule->Process = Process;
    NewModule->Loaded = TRUE;
    INSERT_BEFORE(&(NewModule->ListEntry), &(Context->ModuleList.ModulesHead));
    NewModule->BaseDifference = LowestAddress;
    if (NewModule->Symbols != NULL) {
        NewModule->BaseDifference = LowestAddress -
                                    NewModule->Symbols->ImageBase;
    }

    DbgOut("Module loaded 0x%08I64x: %s -> ",
           NewModule->BaseDifference,
           NewModule->ModuleName);

    if (NewModule->Symbols == NULL) {
        DbgOut(" *** Error: Symbols could not be loaded. ***\n");

    } else {
        DbgOut("%s\n", NewModule->Filename);
    }

    //
    // Update the total checksum.
    //

    Context->ModuleList.Signature += NewModule->Timestamp +
                                     NewModule->LowestAddress;

    Context->ModuleList.ModuleCount += 1;
    Result = TRUE;

LoadModuleEnd:
    if (BackupPotential != NULL) {
        free(BackupPotential);
    }

    if (Result == FALSE) {
        if (NewModule != NULL) {
            if (NewModule->Filename != NULL) {
                free(NewModule->Filename);
            }

            if (NewModule->ModuleName != NULL) {
                free(NewModule->ModuleName);
            }

            if (NewModule->Symbols != NULL) {
                DbgUnloadSymbols(NewModule->Symbols);
                NewModule->Symbols = NULL;
            }

            free(NewModule);
            NewModule = NULL;
        }
    }

    return NewModule;
}

INT
DbgrpResolveDumpType (
    PDEBUGGER_CONTEXT Context,
    PTYPE_SYMBOL *Type,
    PVOID *Data,
    PUINTN DataSize,
    PULONGLONG Address
    )

/*++

Routine Description:

    This routine resolves a dump type and data to something valuable that can
    be dumped. For example, it will follow pointers until a structure type and
    data is found.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    Type - Supplies a pointer that receives the resolved type. It also supplies
        thie initial type.

    Data - Supplies a pointer that receives the data to be dumped. It also
        supplies the dump data for the given type.

    DataSize - Supplies a pointer that on input contains the size of the
        existing data buffer. This will be updated on output.

    Address - Supplies a pointer that receives the address of the final
        data if pointers are followed.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG BytesRead;
    PVOID CurrentData;
    ULONG CurrentSize;
    PTYPE_SYMBOL CurrentType;
    ULONGLONG PointerValue;
    PDATA_TYPE_RELATION RelationData;
    ULONG RelativeSize;
    PTYPE_SYMBOL RelativeType;
    INT Result;
    SYMBOL_SEARCH_RESULT SearchResult;

    CurrentData = *Data;
    CurrentSize = *DataSize;
    CurrentType = *Type;
    while (TRUE) {
        Result = FALSE;

        //
        // Resolve the current type until a void, pointer, array, function, or
        // non-relation type is found.
        //

        CurrentType = DbgSkipTypedefs(CurrentType);
        if (CurrentType == NULL) {
            Result = EINVAL;
            goto ResolveDumpTypeEnd;
        }

        //
        // If the type resolved to a non-relation type, then exit successfully.
        //

        if (CurrentType->Type != DataTypeRelation) {
            break;
        }

        RelationData = &(CurrentType->U.Relation);
        RelativeType = DbgGetType(RelationData->OwningFile,
                                  RelationData->TypeNumber);

        //
        // If the resolved type is a void, an array, or a function, then there
        // is nothing more to resolve.
        //

        if ((RelativeType == CurrentType) ||
            (RelationData->Array.Minimum != RelationData->Array.Maximum) ||
            (RelationData->Function != FALSE)) {

            break;
        }

        //
        // If the relative type is a structure with zero size, search for a
        // structure with the same name and a non-zero size.
        //

        RelativeSize = DbgGetTypeSize(RelativeType, 0);
        if ((RelativeType->Type == DataTypeStructure) && (RelativeSize == 0)) {
            SearchResult.Variety = SymbolResultType;
            Result = DbgpFindSymbol(Context,
                                    RelativeType->Name,
                                    &SearchResult);

            if (Result != FALSE) {
                RelativeType = SearchResult.U.TypeResult;
                RelativeSize = DbgGetTypeSize(RelativeType, 0);
            }
        }

        //
        // Follow pointers, reading the relative type data from the pointer.
        // The pointer value is stored in the current data and the size of the
        // current data should not be bigger than a pointer size.
        //

        assert(RelationData->Pointer != 0);

        CurrentSize = DbgGetTypeSize(CurrentType, 0);
        if (CurrentSize > sizeof(ULONGLONG)) {
            DbgOut("Pointer for type %s is of size %d.\n",
                   CurrentType->Name,
                   CurrentSize);

            Result = EINVAL;
            goto ResolveDumpTypeEnd;
        }

        assert((CurrentData != *Data) || (*DataSize == CurrentSize));

        //
        // Make sure to not follow a NULL pointer.
        //

        PointerValue = 0;
        memcpy(&PointerValue, CurrentData, CurrentSize);
        if (PointerValue == 0) {
            DbgOut("Pointer is NULL.\n", CurrentType->Name);
            Result = FALSE;
            goto ResolveDumpTypeEnd;
        }

        //
        // Allocate a new buffer and read the type data.
        //

        free(CurrentData);
        CurrentSize = RelativeSize;
        CurrentData = malloc(CurrentSize);
        if (CurrentData == NULL) {
            DbgOut("Error unable to allocate %d bytes of memory.\n",
                   RelativeSize);

            Result = ENOMEM;
            goto ResolveDumpTypeEnd;
        }

        *Address = PointerValue;
        Result = DbgReadMemory(Context,
                               TRUE,
                               PointerValue,
                               RelativeSize,
                               CurrentData,
                               &BytesRead);

        if ((Result != 0) || (BytesRead != RelativeSize)) {
            if (Result == 0) {
                Result = EINVAL;
            }

            DbgOut("Error reading memory at 0x%I64x. Expected %d bytes and "
                   "read %d bytes\n",
                   PointerValue,
                   RelativeSize,
                   BytesRead);

            goto ResolveDumpTypeEnd;
        }

        CurrentType = RelativeType;
    }

    Result = 0;

ResolveDumpTypeEnd:
    if (Result != 0) {
        if (CurrentData != NULL) {
            free(CurrentData);
            CurrentData = NULL;
        }

        CurrentSize = 0;
    }

    *Type = CurrentType;
    *Data = CurrentData;
    *DataSize = CurrentSize;
    return Result;
}

INT
DbgrpSetFrame (
    PDEBUGGER_CONTEXT Context,
    ULONG FrameNumber
    )

/*++

Routine Description:

    This routine changes the current frame, so that local variables may come
    from a different function in the call stack.

Arguments:

    Context - Supplies a pointer to the application context.

    FrameNumber - Supplies the frame number. Zero represents the currently
        executing function, 1 represents 0's caller, 2 represents 1's caller,
        etc.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    STACK_FRAME Frame;
    ULONG FrameIndex;
    REGISTERS_UNION Registers;
    INT Status;
    BOOL Unwind;

    assert(Context->CurrentEvent.Type == DebuggerEventBreak);

    Status = 0;

    //
    // Attempt to unwind to the given frame.
    //

    RtlCopyMemory(&Registers,
                  &(Context->CurrentEvent.BreakNotification.Registers),
                  sizeof(REGISTERS_UNION));

    //
    // Set the return address to the current PC so that if it's frame 0, the
    // highlighted line returns to the PC.
    //

    Frame.ReturnAddress = DbgGetPc(Context, &Registers);

    //
    // Unwind the desired number of frames.
    //

    Unwind = TRUE;
    for (FrameIndex = 0; FrameIndex < FrameNumber; FrameIndex += 1) {
        Status = DbgStackUnwind(Context, &Registers, &Unwind, &Frame);
        if (Status == EOF) {
            DbgOut("Error: Only %d frames on the stack.\n", FrameIndex);
            break;

        } else if (Status != 0) {
            DbgOut("Error: Failed to unwind stack: %s.\n",
                   strerror(Status));

            break;
        }
    }

    //
    // If the stack was successfully unwound to the given frame, set that
    // as the current information.
    //

    if (Status == 0) {
        RtlCopyMemory(&(Context->FrameRegisters),
                      &Registers,
                      sizeof(REGISTERS_UNION));

        Context->CurrentFrame = FrameNumber;

        //
        // Load and highlight the source line of the new frame.
        //

        DbgrShowSourceAtAddress(Context, Frame.ReturnAddress);
    }

    return Status;
}

INT
DbgrpEnableBreakPoint (
    PDEBUGGER_CONTEXT Context,
    LONG BreakPointIndex,
    BOOL Enable
    )

/*++

Routine Description:

    This routine enables or disables a breakpoint identified by its zero-based
    index.

Arguments:

    Context - Supplies a pointer to the application context.

    BreakPointIndex - Supplies the zero-based index of the breakpoint to enable
        or disable. To specify all breakpoints, set Index to -1.

    Enable - Supplies a flag indicating whether or not to enable (TRUE) or
        disable (FALSE) this breakpoint.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PDEBUGGER_BREAK_POINT Breakpoint;
    PLIST_ENTRY CurrentEntry;
    INT Status;

    //
    // Loop through looking for the breakpoint in the list.
    //

    CurrentEntry = Context->BreakpointList.Next;
    while (CurrentEntry != &(Context->BreakpointList)) {
        Breakpoint = LIST_VALUE(CurrentEntry, DEBUGGER_BREAK_POINT, ListEntry);
        if ((Breakpoint->Index == BreakPointIndex) || (BreakPointIndex == -1)) {

            //
            // Attempt to enable the breakpoint if it's not already enabled.
            //

            if (Enable != FALSE) {
                if (Breakpoint->Enabled == FALSE) {
                    Status = DbgrpSetBreakpointAtAddress(
                                                 Context,
                                                 Breakpoint->Address,
                                                 &(Breakpoint->OriginalValue));

                    if (Status != 0) {
                        goto EnableBreakPointEnd;
                    }

                    Breakpoint->Enabled = TRUE;
                }

            //
            // Disable the breakpoint if it's not already disabled.
            //

            } else {
                if (Breakpoint->Enabled != FALSE) {
                    if (Context->BreakpointToRestore == Breakpoint) {
                        Context->BreakpointToRestore = NULL;
                    }

                    Status = DbgrpClearBreakpointAtAddress(
                                                    Context,
                                                    Breakpoint->Address,
                                                    Breakpoint->OriginalValue);

                    if (Status != 0) {
                        goto EnableBreakPointEnd;
                    }

                    Breakpoint->Enabled = FALSE;
                }
            }

            if (BreakPointIndex != -1) {
                break;
            }
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if ((CurrentEntry == &(Context->BreakpointList)) &&
        (BreakPointIndex != -1)) {

        DbgOut("Breakpoint %d not found.\n", BreakPointIndex);
        Status = EINVAL;
        goto EnableBreakPointEnd;
    }

    Status = 0;

EnableBreakPointEnd:
    return Status;
}

VOID
DbgrpDestroySourcePath (
    PDEBUGGER_SOURCE_PATH SourcePath
    )

/*++

Routine Description:

    This routine destroys a source path entry. It is assumed the entry is
    already removed from its list.

Arguments:

    SourcePath - Supplies a pointer to the source path to destroy.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    if (SourcePath->Prefix != NULL) {
        free(SourcePath->Prefix);
    }

    if (SourcePath->Path != NULL) {
        free(SourcePath->Path);
    }

    free(SourcePath);
    return;
}

INT
DbgrpLoadFile (
    PSTR Path,
    PVOID *Contents,
    PULONGLONG Size
    )

/*++

Routine Description:

    This routine loads a file into memory.

Arguments:

    Path - Supplies a pointer to the path to load.

    Contents - Supplies a pointer where a pointer to the loaded file will be
        returned on success. The caller is responsible for freeing this memory.

    Size - Supplies a pointer where the size of the file will be returned on
        success.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PVOID Buffer;
    FILE *File;
    size_t Read;
    INT Result;
    struct stat Stat;

    *Contents = NULL;
    *Size = 0;
    Result = stat(Path, &Stat);
    if (Result != 0) {
        return errno;
    }

    Buffer = malloc(Stat.st_size);
    if (Buffer == NULL) {
        return ENOMEM;
    }

    File = fopen(Path, "rb");
    if (File == NULL) {
        Result = errno;
        goto LoadFileEnd;
    }

    Read = fread(Buffer, 1, Stat.st_size, File);
    fclose(File);
    if (Read != Stat.st_size) {
        Result = errno;
        goto LoadFileEnd;
    }

    Result = 0;

LoadFileEnd:
    if (Result != 0) {
        if (Buffer != NULL) {
            free(Buffer);
            Buffer = NULL;
        }
    }

    *Contents = Buffer;
    *Size = Stat.st_size;
    return Result;
}

VOID
DbgrpPrintEflags (
    ULONGLONG Eflags
    )

/*++

Routine Description:

    This routine prints the broken down x86 eflags register.

Arguments:

    Eflags - Supplies the eflags value.

Return Value:

    None.

--*/

{

    ULONG Iopl;

    Iopl = (Eflags & IA32_EFLAG_IOPL_MASK) >> IA32_EFLAG_IOPL_SHIFT;
    DbgOut("Iopl: %d Flags: ", Iopl);
    if (((Eflags & IA32_EFLAG_ALWAYS_0) != 0) ||
        ((Eflags & IA32_EFLAG_ALWAYS_1) != IA32_EFLAG_ALWAYS_1)) {

        DbgOut("*** WARNING: Invalid Flags!! ***");
    }

    if ((Eflags & IA32_EFLAG_CF) != 0) {
        DbgOut("cf ");
    }

    if ((Eflags & IA32_EFLAG_PF) != 0) {
        DbgOut("pf ");
    }

    if ((Eflags & IA32_EFLAG_AF) != 0) {
        DbgOut("af ");
    }

    if ((Eflags & IA32_EFLAG_ZF) != 0) {
        DbgOut("zf ");
    }

    if ((Eflags & IA32_EFLAG_SF) != 0) {
        DbgOut("sf ");
    }

    if ((Eflags & IA32_EFLAG_TF) != 0) {
        DbgOut("tf ");
    }

    if ((Eflags & IA32_EFLAG_IF) != 0) {
        DbgOut("if ");
    }

    if ((Eflags & IA32_EFLAG_DF) != 0) {
        DbgOut("df ");
    }

    if ((Eflags & IA32_EFLAG_OF) != 0) {
        DbgOut("of ");
    }

    if ((Eflags & IA32_EFLAG_NT) != 0) {
        DbgOut("nt ");
    }

    if ((Eflags & IA32_EFLAG_RF) != 0) {
        DbgOut("rf ");
    }

    if ((Eflags & IA32_EFLAG_VM) != 0) {
        DbgOut("vm ");
    }

    if ((Eflags & IA32_EFLAG_AC) != 0) {
        DbgOut("ac ");
    }

    if ((Eflags & IA32_EFLAG_VIF) != 0) {
        DbgOut("vif ");
    }

    if ((Eflags & IA32_EFLAG_VIP) != 0) {
        DbgOut("vip ");
    }

    if ((Eflags & IA32_EFLAG_ID) != 0) {
        DbgOut("id ");
    }

    return;
}

