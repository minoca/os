/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dwframe.c

Abstract:

    This module implements support for DWARF stack unwinding.

Author:

    Evan Green 16-Dec-2015

Environment:

    Debug

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/im.h>
#include "dwarfp.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the highest known register number.
//

#define DWARF_MAX_REGISTERS (ArmRegisterD31 + 1)

//
// Define the maximum size of the remember stack.
//

#define DWARF_MAX_REMEMBER_STACK 32

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _DWARF_FRAME_RULE_TYPE {
    DwarfFrameUndefined = 0,
    DwarfFrameSameValue,
    DwarfFrameCfaOffset,
    DwarfFrameCfaOffsetValue,
    DwarfFrameRegister,
    DwarfFrameExpression,
    DwarfFrameExpressionValue,
    DwarfFrameArchitectural
} DWARF_FRAME_RULE_TYPE, *PDWARF_FRAME_RULE_TYPE;

typedef struct _DWARF_FRAME_STACK DWARF_FRAME_STACK, *PDWARF_FRAME_STACK;

/*++

Structure Description:

    This structure stores a DWARF unwinding rule for a particular register.

Members:

    Type - Stores the rule type.

    Operand - Stores the operand to the rule.

    Operand2 - Stores the second operand to the rule.

--*/

typedef struct _DWARF_FRAME_RULE {
    DWARF_FRAME_RULE_TYPE Type;
    ULONGLONG Operand;
    ULONGLONG Operand2;
} DWARF_FRAME_RULE, *PDWARF_FRAME_RULE;

/*++

Structure Description:

    This structure stores an array of DWARF frame rules for every register,
    representing the current frame unwinding state.

Members:

    Cfa - Stores the CFA rule.

    Registers - Stores the array of rules for each register.

--*/

typedef struct _DWARF_FRAME_RULE_SET {
    DWARF_FRAME_RULE Cfa;
    DWARF_FRAME_RULE Registers[DWARF_MAX_REGISTERS];
} DWARF_FRAME_RULE_SET, *PDWARF_FRAME_RULE_SET;

/*++

Structure Description:

    This structure stores a stack entry of remembered rule states.

Members:

    Next - Stores a pointer to the next stack element.

    RuleSet - Stores the set of rules on the stack.

--*/

struct _DWARF_FRAME_STACK {
    PDWARF_FRAME_STACK Next;
    DWARF_FRAME_RULE_SET RuleSet;
};

/*++

Structure Description:

    This structure defines the state for executing frame unwinding.

Members:

    Location - Stores the current location.

    Rules - Stores the current rules set.

    InitialRules - Stores the initial rules after executing the CIE initial
        instructions.

    RememberStack - Stores the stack of states pushed on by the remember
        instruction.

    RememberStackSize - Stores the size of the remember stack in bytes.

    MaxRegister - Stores the maximum register number that has been changed.

    NewValue - Stores the unwound register values.

--*/

typedef struct _DWARF_FRAME_STATE {
    ULONGLONG Location;
    DWARF_FRAME_RULE_SET Rules;
    DWARF_FRAME_RULE_SET InitialRules;
    PDWARF_FRAME_STACK RememberStack;
    UINTN RememberStackSize;
    ULONG MaxRegister;
    ULONGLONG NewValue[DWARF_MAX_REGISTERS];
} DWARF_FRAME_STATE, *PDWARF_FRAME_STATE;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
DwarfpExecuteFde (
    PDWARF_CONTEXT Context,
    ULONGLONG Pc,
    PDWARF_FDE Fde,
    PDWARF_CIE Cie,
    BOOL CfaOnly,
    PSTACK_FRAME Frame
    );

INT
DwarfpExecuteFrameInstructions (
    PDWARF_CONTEXT Context,
    ULONGLONG Pc,
    PDWARF_CIE Cie,
    PUCHAR Instructions,
    PUCHAR End,
    PDWARF_FRAME_STATE State
    );

VOID
DwarfpSetFrameRule (
    PDWARF_CONTEXT Context,
    PDWARF_FRAME_STATE State,
    ULONG Register,
    DWARF_FRAME_RULE_TYPE RuleType,
    ULONGLONG Operand,
    ULONGLONG Operand2
    );

INT
DwarfpGetValueFromRule (
    PDWARF_CONTEXT Context,
    PDWARF_FRAME_STATE State,
    ULONG Register,
    ULONG AddressSize,
    ULONGLONG Cfa,
    PULONGLONG UnwoundValue
    );

INT
DwarfpFindFrameInfo (
    PDWARF_CONTEXT Context,
    ULONGLONG Pc,
    PDWARF_CIE Cie,
    PDWARF_FDE Fde
    );

INT
DwarfpReadCieOrFde (
    PDWARF_CONTEXT Context,
    BOOL EhFrame,
    PUCHAR *Table,
    PUCHAR End,
    PDWARF_CIE Cie,
    PDWARF_FDE Fde,
    PBOOL IsCie
    );

ULONGLONG
DwarfpReadEncodedAddress (
    PDWARF_CONTEXT Context,
    DWARF_ADDRESS_ENCODING Encoding,
    UCHAR AddressSize,
    PUCHAR *Table
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR DwarfCfaEncodingNames[] = {
    "DwarfCfaNop",
    "DwarfCfaSetLoc",
    "DwarfCfaAdvanceLoc1",
    "DwarfCfaAdvanceLoc2",
    "DwarfCfaAdvanceLoc4",
    "DwarfCfaOffsetExtended",
    "DwarfCfaRestoreExtended",
    "DwarfCfaUndefined",
    "DwarfCfaSameValue",
    "DwarfCfaRegister",
    "DwarfCfaRememberState",
    "DwarfCfaRestoreState",
    "DwarfCfaDefCfa",
    "DwarfCfaDefCfaRegister",
    "DwarfCfaDefCfaOffset",
    "DwarfCfaDefCfaExpression",
    "DwarfCfaExpression",
    "DwarfCfaOffsetExtendedSf",
    "DwarfCfaDefCfaSf",
    "DwarfCfaDefCfaOffsetSf",
    "DwarfCfaValOffset",
    "DwarfCfaValOffsetSf",
    "DwarfCfaValExpression",
};

PSTR DwarfFrameRuleNames[] = {
    "Undefined",
    "SameValue",
    "CfaOffset",
    "CfaOffsetValue",
    "Register",
    "Expression",
    "ExpressionValue",
    "Architectural"
};

//
// ------------------------------------------------------------------ Functions
//

INT
DwarfStackUnwind (
    PDEBUG_SYMBOLS Symbols,
    ULONGLONG DebasedPc,
    PSTACK_FRAME Frame
    )

/*++

Routine Description:

    This routine attempts to unwind the stack by one frame.

Arguments:

    Symbols - Supplies a pointer to the debug symbols.

    DebasedPc - Supplies the program counter value, assuming the image were
        loaded at its preferred base address (that is, actual PC minus loaded
        base difference of the module).

    Frame - Supplies a pointer where the basic frame information for this
        frame will be returned.

Return Value:

    0 on success.

    EOF if there are no more stack frames.

    Returns an error code on failure.

--*/

{

    PDWARF_CONTEXT Context;
    INT Status;

    Context = Symbols->SymbolContext;
    Status = DwarfpStackUnwind(Context, DebasedPc, FALSE, Frame);
    if ((Context->Flags & DWARF_CONTEXT_DEBUG_FRAMES) != 0) {
        DbgOut("Unwind %d: %I64x %I64x\n",
               Status,
               Frame->FramePointer,
               Frame->ReturnAddress);
    }

    return Status;
}

INT
DwarfpStackUnwind (
    PDWARF_CONTEXT Context,
    ULONGLONG DebasedPc,
    BOOL CfaOnly,
    PSTACK_FRAME Frame
    )

/*++

Routine Description:

    This routine attempts to unwind the stack by one frame.

Arguments:

    Context - Supplies a pointer to the DWARF symbol context.

    DebasedPc - Supplies the program counter value, assuming the image were
        loaded at its preferred base address (that is, actual PC minus loaded
        base difference of the module).

    CfaOnly - Supplies a boolean indicating whether to only return the
        current Canonical Frame Address and not actually perform any unwinding
        (TRUE) or whether to fully unwind this function (FALSE).

    Frame - Supplies a pointer where the basic frame information for this
        frame will be returned.

Return Value:

    0 on success.

    EOF if there are no more stack frames.

    Returns an error code on failure.

--*/

{

    DWARF_CIE Cie;
    DWARF_FDE Fde;
    INT Status;

    Status = DwarfpFindFrameInfo(Context, DebasedPc, &Cie, &Fde);
    if (Status != 0) {
        return Status;
    }

    Status = DwarfpExecuteFde(Context, DebasedPc, &Fde, &Cie, CfaOnly, Frame);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
DwarfpExecuteFde (
    PDWARF_CONTEXT Context,
    ULONGLONG Pc,
    PDWARF_FDE Fde,
    PDWARF_CIE Cie,
    BOOL CfaOnly,
    PSTACK_FRAME Frame
    )

/*++

Routine Description:

    This routine executes the instructions associated with a DWARF FDE to
    unwind the stack.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Pc - Supplies the (debased) PC value.

    Fde - Supplies a pointer to the FDE containing the instructions.

    Cie - Supplies a pointer to the CIE.

    CfaOnly - Supplies a boolean indicating whether to only return the
        current Canonical Frame Address and not actually perform any unwinding
        (TRUE) or whether to fully unwind this function (FALSE).

    Frame - Supplies a pointer where the basic frame information for this
        frame will be returned.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONGLONG Cfa;
    ULONG DefaultCfaRegister;
    ULONG Index;
    ULONG ReturnRegister;
    DWARF_FRAME_STATE State;
    INT Status;
    ULONGLONG Value;

    assert((Pc >= Fde->InitialLocation) &&
           (Pc < (Fde->InitialLocation + Fde->Range)));

    memset(&State, 0, sizeof(DWARF_FRAME_STATE));
    State.Location = Fde->InitialLocation;

    //
    // Set the return address register so that it keeps its same value.
    //

    ReturnRegister = Cie->ReturnAddressRegister;
    if (ReturnRegister < DWARF_MAX_REGISTERS) {
        State.Rules.Registers[ReturnRegister].Type = DwarfFrameSameValue;
        State.MaxRegister = ReturnRegister;
    }

    //
    // Execute the initial instructions to get the rules set up.
    //

    Status = DwarfpExecuteFrameInstructions(Context,
                                            Pc,
                                            Cie,
                                            Cie->InitialInstructions,
                                            Cie->End,
                                            &State);

    if ((Context->Flags & DWARF_CONTEXT_DEBUG_FRAMES) != 0) {
        DWARF_PRINT("\n");
    }

    DefaultCfaRegister = -1;
    if (State.Rules.Cfa.Type == DwarfFrameRegister) {
        DefaultCfaRegister = State.Rules.Cfa.Operand;
    }

    if (Status != 0) {
        DWARF_ERROR("DWARF: Failed to execute initial CIE instructions.\n");
        goto ExecuteFdeEnd;
    }

    //
    // Copy that into the initial state.
    //

    memcpy(&(State.InitialRules), &(State.Rules), sizeof(DWARF_FRAME_RULE_SET));

    //
    // Now execute the primary rules of the FDE.
    //

    Status = DwarfpExecuteFrameInstructions(Context,
                                            Pc,
                                            Cie,
                                            Fde->Instructions,
                                            Fde->End,
                                            &State);

    if ((Context->Flags & DWARF_CONTEXT_DEBUG_FRAMES) != 0) {
        DWARF_PRINT("\n");
    }

    if (Status != 0) {
        DWARF_ERROR("DWARF: Failed to execute FDE instructions.\n");
        goto ExecuteFdeEnd;
    }

    //
    // Get the CFA value.
    //

    Status = DwarfpGetValueFromRule(Context,
                                    &State,
                                    -1,
                                    Cie->AddressSize,
                                    0,
                                    &Cfa);

    if (Status != 0) {
        DWARF_ERROR("DWARF: Failed to get CFA location.\n");
        goto ExecuteFdeEnd;
    }

    Frame->FramePointer = Cfa;
    Frame->ReturnAddress = 0;
    if (CfaOnly != FALSE) {
        Status = 0;
        goto ExecuteFdeEnd;
    }

    //
    // Now evaluate the rules to get all the new register values.
    //

    for (Index = 0; Index <= State.MaxRegister; Index += 1) {
        if (State.Rules.Registers[Index].Type == DwarfFrameUndefined) {
            continue;
        }

        Status = DwarfpGetValueFromRule(Context,
                                        &State,
                                        Index,
                                        Cie->AddressSize,
                                        Cfa,
                                        &Value);

        if (Status != 0) {
            DWARF_ERROR("DWARF: Failed to get value for register %d.\n", Index);
            goto ExecuteFdeEnd;
        }

        if (Index == ReturnRegister) {
            Frame->ReturnAddress = Value;
        }

        State.NewValue[Index] = Value;
    }

    //
    // Now apply all the new register values. This couldn't be done before
    // because the registers may have depended on each other's old contents.
    // Apply the return address register to the PC first because it's an
    // implicit rule.
    //

    if (ReturnRegister < DWARF_MAX_REGISTERS) {
        if (State.Rules.Registers[ReturnRegister].Type != DwarfFrameUndefined) {
            DwarfTargetWritePc(Context, State.NewValue[ReturnRegister]);
            if ((Context->Flags & DWARF_CONTEXT_VERBOSE_UNWINDING) != 0) {
                DWARF_PRINT("   PC <- %I64x <- r%d (%s) (ReturnAddress)\n",
                            State.NewValue[ReturnRegister],
                            ReturnRegister,
                            DwarfGetRegisterName(Context, ReturnRegister));
            }

        } else {
            DwarfTargetWritePc(Context, 0);
        }
    }

    for (Index = 0; Index <= State.MaxRegister; Index += 1) {
        if (State.Rules.Registers[Index].Type == DwarfFrameUndefined) {
            continue;
        }

        Status = DwarfTargetWriteRegister(Context,
                                          Index,
                                          State.NewValue[Index]);

        if (Status != 0) {
            DWARF_ERROR("DWARF: Failed to set register %d.\n", Index);
            goto ExecuteFdeEnd;
        }
    }

    //
    // Restore the CFA register if it wasn't explicitly restored.
    //

    if (DefaultCfaRegister != (ULONG)-1) {
        if (State.Rules.Registers[DefaultCfaRegister].Type ==
            DwarfFrameUndefined) {

            if ((Context->Flags & DWARF_CONTEXT_VERBOSE_UNWINDING) != 0) {
                DWARF_PRINT("   r%d (%s) <- %I64x <- CFA (implicit)\n",
                            DefaultCfaRegister,
                            DwarfGetRegisterName(Context, DefaultCfaRegister),
                            Cfa);
            }

            Status = DwarfTargetWriteRegister(Context, DefaultCfaRegister, Cfa);
            if (Status != 0) {
                DWARF_ERROR("DWARF: Failed to set CFA register %d.\n", Index);
                goto ExecuteFdeEnd;
            }
        }
    }

    Status = 0;

ExecuteFdeEnd:
    return Status;
}

INT
DwarfpExecuteFrameInstructions (
    PDWARF_CONTEXT Context,
    ULONGLONG Pc,
    PDWARF_CIE Cie,
    PUCHAR Instructions,
    PUCHAR End,
    PDWARF_FRAME_STATE State
    )

/*++

Routine Description:

    This routine executes the instructions associated with a DWARF FDE to
    unwind the stack.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Pc - Supplies the (debased) PC value. Execution will stop if the location
        is ever advanced beyond this value.

    Cie - Supplies a pointer to the CIE associated with this value.

    Instructions - Supplies a pointer to the instructions to execute.

    End - Supplies a pointer just beyond the end of the last instruction to
        execute.

    State - Supplies a pointer where the rule state will be acted upon.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    DWARF_CALL_FRAME_ENCODING Instruction;
    ULONGLONG Operand;
    ULONGLONG Operand2;
    BOOL Print;
    ULONG Register;
    DWARF_FRAME_RULE_TYPE RuleType;
    PDWARF_FRAME_STACK StackEntry;
    INT Status;

    Print = FALSE;
    if ((Context->Flags & DWARF_CONTEXT_DEBUG_FRAMES) != 0) {
        Print = TRUE;
    }

    while (Instructions < End) {
        Instruction = DwarfpRead1(&Instructions);
        if ((Instruction & DwarfCfaHighMask) != 0) {
            Operand = (UCHAR)(Instruction & (~DwarfCfaHighMask));
            switch (Instruction & DwarfCfaHighMask) {

            //
            // Advance the location by the lower 6 bits encoded in the
            // instruction.
            //

            case DwarfCfaAdvanceLoc:
                Operand *= Cie->CodeAlignmentFactor;
                State->Location += Operand;
                if (Print != FALSE) {
                    DWARF_PRINT("   DwarfCfaAdvanceLoc: %I64d to %I64x",
                                Operand,
                                State->Location);
                }

                if (State->Location > Pc) {
                    Status = 0;
                    goto ExecuteFrameInstructionsEnd;
                }

                break;

            //
            // Set the rule for the register encoded in the low 6 bits of the
            // instruction to Offset(N), where N is LEB128 operand.
            //

            case DwarfCfaOffset:
                Operand2 = DwarfpReadLeb128(&Instructions) *
                           Cie->DataAlignmentFactor;

                if (Print != FALSE) {
                    DWARF_PRINT("   DwarfCfaOffset: %I64d %I64d",
                                Operand,
                                Operand2);
                }

                DwarfpSetFrameRule(Context,
                                   State,
                                   Operand,
                                   DwarfFrameCfaOffset,
                                   Operand2,
                                   0);

                break;

            //
            // Chang the rule for the register encoded in the low 6 bits of the
            // instruction back to its initial rule from the CIE.
            //

            case DwarfCfaRestore:
                if (Print != FALSE) {
                    DWARF_PRINT("   DwarfCfaRestore: %I64d", Operand);
                }

                DwarfpSetFrameRule(
                              Context,
                              State,
                              Operand,
                              State->InitialRules.Registers[Operand].Type,
                              State->InitialRules.Registers[Operand].Operand,
                              State->InitialRules.Registers[Operand].Operand2);

                break;

            default:

                assert(FALSE);

                break;
            }

        } else {
            if (Print != FALSE) {
                if ((Instruction >= DwarfCfaLowUser) &&
                    (Instruction <= DwarfCfaHighUser)) {

                    DWARF_PRINT("   DwarfCfaUser%x", Instruction);

                } else if (Instruction <= DwarfCfaValExpression) {
                    DWARF_PRINT("   %s", DwarfCfaEncodingNames[Instruction]);

                } else {
                    DWARF_PRINT("   DwarfCfaUNKNOWN%x", Instruction);
                }
            }

            switch (Instruction) {
            case DwarfCfaNop:
                break;

            //
            // The advance instructions move the current address by a given
            // amount.
            //

            case DwarfCfaSetLoc:
                if (Cie->AddressSize == 8) {
                    Operand = DwarfpRead8(&Instructions);

                } else {

                    assert(Cie->AddressSize == 4);

                    Operand = DwarfpRead4(&Instructions);
                }

                State->Location = Operand;
                if (Print != FALSE) {
                    DWARF_PRINT(": to %I64x", Operand);
                }

                if (State->Location > Pc) {
                    Status = 0;
                    goto ExecuteFrameInstructionsEnd;
                }

                break;

            case DwarfCfaAdvanceLoc1:
            case DwarfCfaAdvanceLoc2:
            case DwarfCfaAdvanceLoc4:
                switch (Instruction) {
                case DwarfCfaAdvanceLoc1:
                    Operand = DwarfpRead1(&Instructions);
                    break;

                case DwarfCfaAdvanceLoc2:
                    Operand = DwarfpRead2(&Instructions);
                    break;

                case DwarfCfaAdvanceLoc4:
                    Operand = DwarfpRead4(&Instructions);
                    break;

                default:

                    assert(FALSE);

                    Operand = 0;
                    break;
                }

                State->Location += Operand * Cie->CodeAlignmentFactor;
                if (Print != FALSE) {
                    DWARF_PRINT(": %I64d to %I64x",
                                Operand,
                                State->Location);
                }

                if (State->Location > Pc) {
                    Status = 0;
                    goto ExecuteFrameInstructionsEnd;
                }

                break;

            //
            // The extended offset instruction sets a register to the offset(N)
            // rule.
            //

            case DwarfCfaOffsetExtended:
            case DwarfCfaOffsetExtendedSf:
                Operand = DwarfpReadLeb128(&Instructions);
                if (Instruction == DwarfCfaOffsetExtendedSf) {
                    Operand2 = DwarfpReadSleb128(&Instructions) *
                               Cie->DataAlignmentFactor;

                } else {
                    Operand2 = DwarfpReadLeb128(&Instructions) *
                               Cie->DataAlignmentFactor;
                }

                if (Print != FALSE) {
                    DWARF_PRINT(": %I64d to %I64x", Operand2, State->Location);
                }

                DwarfpSetFrameRule(Context,
                                   State,
                                   Operand,
                                   DwarfFrameCfaOffset,
                                   Operand2,
                                   0);

                break;

            //
            // The restore extended instruction takes a register operand and
            // restores the current rule to the initial rule.
            //

            case DwarfCfaRestoreExtended:
                Operand = DwarfpReadLeb128(&Instructions);
                if (Print != FALSE) {
                    DWARF_PRINT(": %I64d", Operand);
                }

                DwarfpSetFrameRule(
                              Context,
                              State,
                              Operand,
                              State->InitialRules.Registers[Operand].Type,
                              State->InitialRules.Registers[Operand].Operand,
                              State->InitialRules.Registers[Operand].Operand2);

                break;

            //
            // Set the rule to undefined.
            //

            case DwarfCfaUndefined:
                Operand = DwarfpReadLeb128(&Instructions);
                DwarfpSetFrameRule(Context,
                                   State,
                                   Operand,
                                   DwarfFrameUndefined,
                                   0,
                                   0);

                break;

            //
            // Set the given register operand to the same value rule.
            //

            case DwarfCfaSameValue:
                Operand = DwarfpReadLeb128(&Instructions);
                DwarfpSetFrameRule(Context,
                                   State,
                                   Operand,
                                   DwarfFrameSameValue,
                                   0,
                                   0);

                break;

            //
            // Set the register specified in the first operand to the rule
            // register(R), where R is the second operand.
            //

            case DwarfCfaRegister:
                Operand = DwarfpReadLeb128(&Instructions);
                Operand2 = DwarfpReadLeb128(&Instructions);
                DwarfpSetFrameRule(Context,
                                   State,
                                   Operand,
                                   DwarfFrameRegister,
                                   Operand2,
                                   0);

                break;

            //
            // Save the current frame state for all registers, and push it on
            // a stack.
            //

            case DwarfCfaRememberState:
                if (State->RememberStackSize >= DWARF_MAX_REMEMBER_STACK) {
                    DWARF_ERROR("DWARF: Frame remember stack size too big.\n");
                    Status = ERANGE;
                    goto ExecuteFrameInstructionsEnd;
                }

                StackEntry = malloc(sizeof(DWARF_FRAME_STACK));
                if (StackEntry == NULL) {
                    Status = ENOMEM;
                    goto ExecuteFrameInstructionsEnd;
                }

                memcpy(&(StackEntry->RuleSet),
                       &(State->Rules),
                       sizeof(DWARF_FRAME_RULE_SET));

                StackEntry->Next = State->RememberStack;
                State->RememberStack = StackEntry;
                State->RememberStackSize += 1;
                break;

            //
            // Pop the previously pushed register state and save it as the
            // current row.
            //

            case DwarfCfaRestoreState:
                if (State->RememberStackSize == 0) {
                    DWARF_ERROR("DWARF: Popped empty remember stack.\n");
                    Status = ERANGE;
                    goto ExecuteFrameInstructionsEnd;
                }

                StackEntry = State->RememberStack;
                State->RememberStack = StackEntry->Next;
                State->RememberStackSize -= 1;
                memcpy(&(State->Rules),
                       &(StackEntry->RuleSet),
                       sizeof(DWARF_FRAME_RULE_SET));

                free(StackEntry);
                break;

            //
            // Set the CFA rule to be the given register (operand 1) plus the
            // given offset (operand2). The CFA register rule changes the
            // register but keeps the offset as it is. The CFA offset rule
            // changes the offset but leaves the regsister where it is.
            //

            case DwarfCfaDefCfa:
            case DwarfCfaDefCfaSf:
            case DwarfCfaDefCfaRegister:
            case DwarfCfaDefCfaOffset:
            case DwarfCfaDefCfaOffsetSf:

                //
                // Get the register, which is either the old value or the
                // operand.
                //

                if ((Instruction == DwarfCfaDefCfaOffset) ||
                    (Instruction == DwarfCfaDefCfaOffsetSf)) {

                    Operand = State->Rules.Cfa.Operand;

                } else {
                    Operand = DwarfpReadLeb128(&Instructions);
                }

                //
                // Get the offset, which is either a signed and factored offset,
                // the original value, or an unsigned offset.
                //

                if ((Instruction == DwarfCfaDefCfaSf) ||
                    (Instruction == DwarfCfaDefCfaOffsetSf)) {

                    Operand2 = DwarfpReadSleb128(&Instructions) *
                               Cie->DataAlignmentFactor;

                } else if (Instruction == DwarfCfaDefCfaRegister) {
                    Operand2 = State->Rules.Cfa.Operand2;

                } else {
                    Operand2 = DwarfpReadLeb128(&Instructions);
                }

                if (Print != FALSE) {
                    DWARF_PRINT(": %I64d %I64d", Operand, Operand2);
                }

                DwarfpSetFrameRule(Context,
                                   State,
                                   -1,
                                   DwarfFrameRegister,
                                   Operand,
                                   Operand2);

                break;

            //
            // The CFA or register rule is determined by evaluating the given
            // DWARF expression. This is in the form "exprloc", which is a
            // leb128 length, followed by the expression bytes.
            //

            case DwarfCfaDefCfaExpression:
            case DwarfCfaExpression:
            case DwarfCfaValExpression:
                if (Instruction == DwarfCfaDefCfaExpression) {
                    Register = -1;

                } else {
                    Register = DwarfpReadLeb128(&Instructions);
                }

                Operand = DwarfpReadLeb128(&Instructions);
                Operand2 = (UINTN)(Instructions);
                Instructions += Operand;
                if (Print != FALSE) {
                    DWARF_PRINT(": %d: {", Register);
                    DwarfpPrintExpression(Context,
                                          Cie->AddressSize,
                                          NULL,
                                          (PUCHAR)(UINTN)Operand2,
                                          Operand);

                    DWARF_PRINT("}");
                }

                RuleType = DwarfFrameExpressionValue;
                if (Instruction == DwarfCfaExpression) {
                    RuleType = DwarfFrameExpression;
                }

                DwarfpSetFrameRule(Context,
                                   State,
                                   Register,
                                   RuleType,
                                   Operand,
                                   Operand2);

                break;

            //
            // Set the register rule to the value offset rule.
            //

            case DwarfCfaValOffset:
            case DwarfCfaValOffsetSf:
                Operand = DwarfpReadLeb128(&Instructions);
                if (Instruction == DwarfCfaValOffsetSf) {
                    Operand2 = DwarfpReadSleb128(&Instructions) *
                               Cie->DataAlignmentFactor;

                } else {
                    Operand2 = DwarfpReadLeb128(&Instructions) *
                               Cie->DataAlignmentFactor;
                }

                if (Print != FALSE) {
                    DWARF_PRINT(": %I64d %I64d", Operand, Operand2);
                }

                DwarfpSetFrameRule(Context,
                                   State,
                                   Operand,
                                   DwarfFrameCfaOffsetValue,
                                   Operand2,
                                   0);

                break;

            default:
                Status = EINVAL;
                goto ExecuteFrameInstructionsEnd;
            }
        }

        if (Print != FALSE) {
            DWARF_PRINT("\n");
        }
    }

    Status = 0;

ExecuteFrameInstructionsEnd:

    //
    // Clear any old remember stack entries.
    //

    while (State->RememberStackSize != 0) {
        StackEntry = State->RememberStack;
        State->RememberStack = StackEntry->Next;
        State->RememberStackSize -= 1;
        free(StackEntry);
    }

    assert(State->RememberStack == NULL);

    return Status;
}

VOID
DwarfpSetFrameRule (
    PDWARF_CONTEXT Context,
    PDWARF_FRAME_STATE State,
    ULONG Register,
    DWARF_FRAME_RULE_TYPE RuleType,
    ULONGLONG Operand,
    ULONGLONG Operand2
    )

/*++

Routine Description:

    This routine sets the rule for a given register.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Register - Supplies the register to set the rule for. Supply -1 to set the
        CFA rule.

    State - Supplies a pointer to the frame state machine state.

    RuleType - Supplies the form of the rule.

    Operand - Supplies the rule operand.

    Operand2 - Supplies the second operand.

Return Value:

    None.

--*/

{

    PDWARF_FRAME_RULE Rule;

    if (Register == (ULONG)-1) {
        Rule = &(State->Rules.Cfa);

    } else if (Register < DWARF_MAX_REGISTERS) {
        Rule = &(State->Rules.Registers[Register]);
        if (State->MaxRegister < Register) {
            State->MaxRegister = Register;
        }

    } else {
        DWARF_ERROR("DWARF: Register %d too big.\n", Register);
        return;
    }

    Rule->Type = RuleType;
    Rule->Operand = Operand;
    Rule->Operand2 = Operand2;
    if ((Context->Flags & DWARF_CONTEXT_DEBUG_FRAMES) != 0) {
        DWARF_PRINT("\n    Rule: ");
        if (Register == (ULONG)-1) {
            DWARF_PRINT("CFA");

        } else {
            DWARF_PRINT("r%d (%s)",
                        Register,
                        DwarfGetRegisterName(Context, Register));
        }

        DWARF_PRINT(" %s: %I64x", DwarfFrameRuleNames[RuleType], Operand);
        if (Operand2 != 0) {
            DWARF_PRINT(" %I64x", Operand2);
        }
    }

    return;
}

INT
DwarfpGetValueFromRule (
    PDWARF_CONTEXT Context,
    PDWARF_FRAME_STATE State,
    ULONG Register,
    ULONG AddressSize,
    ULONGLONG Cfa,
    PULONGLONG UnwoundValue
    )

/*++

Routine Description:

    This routine determines the final value by applying a given register rule.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    State - Supplies a pointer to the completed frame state.

    Register - Supplies the register to unwind. Supply -1 to get the CFA.

    AddressSize - Supplies the size of an address/register on the target.

    Cfa - Supplies the canonical frame address value.

    UnwoundValue - Supplies a pointer where the value will be returned on
        success.

Return Value:

    Returns the value of the register by applying the rule.

--*/

{

    ULONGLONG InitialPush;
    DWARF_LOCATION Location;
    PSTR RegisterName;
    PDWARF_FRAME_RULE Rule;
    INT Status;
    ULONGLONG Value;

    Status = 0;
    if (Register == (ULONG)-1) {
        Rule = &(State->Rules.Cfa);
        if ((Context->Flags & DWARF_CONTEXT_VERBOSE_UNWINDING) != 0) {
            DWARF_PRINT("   CFA <- ");
        }

    } else if (Register < DWARF_MAX_REGISTERS) {
        Rule = &(State->Rules.Registers[Register]);
        RegisterName = DwarfGetRegisterName(Context, Register);
        if ((Context->Flags & DWARF_CONTEXT_VERBOSE_UNWINDING) != 0) {
            DWARF_PRINT("   r%d (%s) <- ", Register, RegisterName);
        }

    } else {
        DWARF_ERROR("DWARF: Register %d too big.\n", Register);
        return ERANGE;
    }

    switch (Rule->Type) {
    case DwarfFrameCfaOffset:

        assert(Register != (ULONG)-1);

        Value = 0;
        Status = DwarfTargetRead(Context,
                                 Cfa + Rule->Operand,
                                 AddressSize,
                                 0,
                                 &Value);

        if ((Context->Flags & DWARF_CONTEXT_VERBOSE_UNWINDING) != 0) {
            DWARF_PRINT("%I64x <- [CFA%+I64d]\n", Value, Rule->Operand);
        }

        break;

    case DwarfFrameCfaOffsetValue:

        assert(Register != (ULONG)-1);

        Value = Cfa + Rule->Operand;
        if ((Context->Flags & DWARF_CONTEXT_VERBOSE_UNWINDING) != 0) {
            DWARF_PRINT("%I64x <- [CFA%+I64d]\n", Value, Rule->Operand);
        }

        break;

    case DwarfFrameRegister:
        Value = 0;
        Status = DwarfTargetReadRegister(Context, Rule->Operand, &Value);
        Value += Rule->Operand2;
        if ((Context->Flags & DWARF_CONTEXT_VERBOSE_UNWINDING) != 0) {
            DWARF_PRINT("%I64x <- r%d (%s) + %I64d\n",
                        Value,
                        (ULONG)(Rule->Operand),
                        DwarfGetRegisterName(Context, Rule->Operand),
                        Rule->Operand2);
        }

        break;

    case DwarfFrameExpression:
    case DwarfFrameExpressionValue:
        Value = 0;

        //
        // Evaluate the expression, pushing the CFA address on initially unless
        // this is the CFA rule.
        //

        InitialPush = -1ULL;
        if (Register != (ULONG)-1) {
            InitialPush = Cfa;
        }

        Status = DwarfpEvaluateSimpleExpression(Context,
                                                AddressSize,
                                                NULL,
                                                InitialPush,
                                                (PUCHAR)(UINTN)(Rule->Operand2),
                                                Rule->Operand,
                                                &Location);

        if (Status != 0) {
            DWARF_ERROR("DWARF: Failed to evaluate FDE expression.\n");
            break;
        }

        //
        // Only memory forms are expected.
        //

        if (Location.Form != DwarfLocationMemory) {
            DWARF_ERROR("DWARF: Error: Got simple expression location %d.\n",
                        Location.Form);

            Status = EINVAL;
            break;
        }

        //
        // For expression rules, read the value at the address to get the final
        // unwind value.
        //

        if (Rule->Type == DwarfFrameExpression) {
            Status = DwarfTargetRead(Context,
                                     Location.Value.Address,
                                     AddressSize,
                                     0,
                                     &Value);

            if ((Context->Flags & DWARF_CONTEXT_VERBOSE_UNWINDING) != 0) {
                DWARF_PRINT("%I64x <- [%I64x]\n",
                            Value,
                            Location.Value.Address);
            }

        //
        // For expression value rules, the output of the expression is the
        // unwound value itself.
        //

        } else {
            Value = Location.Value.Address;
            if ((Context->Flags & DWARF_CONTEXT_VERBOSE_UNWINDING) != 0) {
                DWARF_PRINT("%I64x\n", Value);
            }

            Status = 0;
        }

        break;

    case DwarfFrameUndefined:
        if ((Context->Flags & DWARF_CONTEXT_VERBOSE_UNWINDING) != 0) {
            DWARF_PRINT("Undefined\n");
        }

        Value = 0;
        break;

    case DwarfFrameSameValue:

        assert(Register != (ULONG)-1);

        Value = 0;
        Status = DwarfTargetReadRegister(Context, Register, &Value);
        if ((Context->Flags & DWARF_CONTEXT_VERBOSE_UNWINDING) != 0) {
            DWARF_PRINT("%I64x (same)\n", Value);
        }

        break;

    default:

        assert(FALSE);

        Value = 0;
        break;
    }

    *UnwoundValue = Value;
    return Status;
}

INT
DwarfpFindFrameInfo (
    PDWARF_CONTEXT Context,
    ULONGLONG Pc,
    PDWARF_CIE Cie,
    PDWARF_FDE Fde
    )

/*++

Routine Description:

    This routine scans through the .debug_frame or .eh_frame sections to find
    the unwind information for the given PC.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Pc - Supplies the (debased) PC value to match against.

    Cie - Supplies a pointer where the relevant CIE will be returned.

    Fde - Supplies a pointer where the relevant FDE will be returned.

Return Value:

    0 on success.

    ENOENT if no frame information could be found.

    Returns an error number on failure.

--*/

{

    BOOL EhFrame;
    PUCHAR End;
    BOOL IsCie;
    INT Status;
    PUCHAR Table;

    //
    // Get the .debug_frame or .eh_frame sections.
    //

    if (Context->Sections.Frame.Size != 0) {
        Table = Context->Sections.Frame.Data;
        End = Table + Context->Sections.Frame.Size;
        EhFrame = FALSE;

    } else if (Context->Sections.EhFrame.Size != 0) {
        Table = Context->Sections.EhFrame.Data;
        End = Table + Context->Sections.EhFrame.Size;
        EhFrame = TRUE;

    } else {
        Status = ENOENT;
        goto FindFrameInfoEnd;
    }

    memset(Cie, 0, sizeof(DWARF_CIE));
    memset(Fde, 0, sizeof(DWARF_FDE));

    //
    // Loop through the table until the FDE is found that matches the given PC.
    //

    while (Table < End) {
        Status = DwarfpReadCieOrFde(Context,
                                    EhFrame,
                                    &Table,
                                    End,
                                    Cie,
                                    Fde,
                                    &IsCie);

        if (Status != 0) {
            if (Status == EAGAIN) {
                continue;
            }

            goto FindFrameInfoEnd;
        }

        if (IsCie != FALSE) {
            continue;
        }

        if ((Pc >= Fde->InitialLocation) &&
            (Pc < (Fde->InitialLocation + Fde->Range))) {

            Status = 0;
            goto FindFrameInfoEnd;
        }
    }

    //
    // All the FDEs were read and none of them matched.
    //

    Status = ENOENT;

FindFrameInfoEnd:
    return Status;
}

INT
DwarfpReadCieOrFde (
    PDWARF_CONTEXT Context,
    BOOL EhFrame,
    PUCHAR *Table,
    PUCHAR End,
    PDWARF_CIE Cie,
    PDWARF_FDE Fde,
    PBOOL IsCie
    )

/*++

Routine Description:

    This routine reads either a CIE or an FDE.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    EhFrame - Supplies a boolean indicating whether this is an EH frame section
        or a .debug_frame section.

    Table - Supplies a pointer that on input contains a pointer to the start of
        the section. On output this pointer will be advanced past the fields
        scanned.

    End - Supplies a pointer to the end of the section.

    Cie - Supplies a pointer where the CIE will be returned. If an FDE is being
        parsed, then this CIE should be its owning CIE.

    Fde - Supplies a pointer where the FDE will be returned.

    IsCie - Supplies a pointer where a boolean will be returned indicating if a
        CIE was parsed (TRUE) or an FDE (FALSE).

Return Value:

    0 on success.

    EAGAIN if a zero terminator was found.

    Returns an error number on failure.

--*/

{

    PSTR Augmentation;
    LONGLONG CieId;
    BOOL CieIsCie;
    PUCHAR CieStart;
    DWARF_ADDRESS_ENCODING Encoding;
    BOOL Is64Bit;
    PUCHAR Start;
    INT Status;
    PUCHAR UnitEnd;
    ULONGLONG UnitLength;

    Start = *Table;
    DwarfpReadInitialLength(Table, &Is64Bit, &UnitLength);
    if (UnitLength == 0) {
        return EAGAIN;
    }

    UnitEnd = *Table + UnitLength;
    if (UnitEnd > End) {
        return ERANGE;
    }

    CieStart = *Table;
    if (Is64Bit != FALSE) {
        CieId = (LONGLONG)DwarfpRead8(Table);

    } else {
        CieId = (LONG)DwarfpRead4(Table);
    }

    //
    // If the CIE ID is zero or -1, this is a CIE.
    //

    if (((EhFrame != FALSE) && (CieId == 0)) ||
        ((EhFrame == FALSE) && (CieId == -1))) {

        *IsCie = TRUE;
        memset(Cie, 0, sizeof(DWARF_CIE));
        Cie->EhFrame = EhFrame;
        Cie->Is64Bit = Is64Bit;
        Cie->Start = Start;
        Cie->End = UnitEnd;
        Cie->UnitLength = UnitLength;
        Cie->Version = DwarfpRead1(Table);
        Augmentation = (PSTR)(*Table);
        Cie->Augmentation = Augmentation;
        *Table = (PUCHAR)Augmentation + strlen(Augmentation) + 1;
        Cie->AddressSize = 4;
        if (Is64Bit != FALSE) {
            Cie->AddressSize = 8;
        }

        if (EhFrame == FALSE) {
            if (Cie->Version == 4) {
                Cie->AddressSize = DwarfpRead1(Table);
                Cie->SegmentSize = DwarfpRead1(Table);
            }
        }

        Cie->CodeAlignmentFactor = DwarfpReadLeb128(Table);
        Cie->DataAlignmentFactor = DwarfpReadSleb128(Table);
        Cie->ReturnAddressRegister = DwarfpReadLeb128(Table);
        Cie->InitialInstructions = *Table;

        //
        // Read the augmentation format to get the rest of the fields.
        //

        if (*Augmentation == 'z') {
            Augmentation += 1;
            Cie->AugmentationLength = DwarfpReadLeb128(Table);
            Cie->InitialInstructions = *Table + Cie->AugmentationLength;
            while (*Augmentation != '\0') {
                switch (*Augmentation) {

                //
                // L specifies the language specific data area encoding.
                //

                case 'L':
                    Cie->LanguageEncoding = DwarfpRead1(Table);
                    break;

                //
                // P contains two arguments: the first byte is an encoding of
                // the second argument, which is the address of a personality
                // routine handler.
                //

                case 'P':
                    Encoding = DwarfpRead1(Table);
                    DwarfpReadEncodedAddress(Context,
                                             Encoding,
                                             Cie->AddressSize,
                                             Table);

                    break;

                //
                // S indicates this CIE unwinds a signal handler.
                //

                case 'S':
                    break;

                //
                // R contains an argument which is the address encoding of
                // FDE addresses.
                //

                case 'R':
                    Cie->FdeEncoding = DwarfpRead1(Table);
                    break;

                default:
                    DWARF_ERROR("DWARF: Unrecognized augmentation %c in "
                                "string %s.\n",
                                *Augmentation,
                                Cie->Augmentation);

                    Status = EINVAL;
                    goto ReadCieOrFdeEnd;
                }

                Augmentation += 1;
            }
        }

    //
    // Otherwise, this is an FDE.
    //

    } else {
        *IsCie = FALSE;
        memset(Fde, 0, sizeof(DWARF_FDE));
        Fde->Length = UnitLength;
        Fde->CiePointer = CieId;
        Fde->Start = Start;
        Fde->End = UnitEnd;

        //
        // The FDE points at its owning CIE. If that is not the mostly
        // recently read one (as it almost always is), then go read the CIE.
        //

        if (EhFrame != FALSE) {
            CieStart -= CieId;

        } else {
            CieStart = Context->Sections.Frame.Data + (ULONGLONG)CieId;
        }

        if (CieStart != Cie->Start) {
            Status = DwarfpReadCieOrFde(Context,
                                        EhFrame,
                                        &CieStart,
                                        End,
                                        Cie,
                                        Fde,
                                        &CieIsCie);

            if (Status != 0) {
                DWARF_ERROR("DWARF: Could not read alternate CIE.\n");
                goto ReadCieOrFdeEnd;
            }

            assert(CieIsCie != FALSE);
        }

        Fde->InitialLocation = DwarfpReadEncodedAddress(Context,
                                                        Cie->FdeEncoding,
                                                        Cie->AddressSize,
                                                        Table);

        Fde->Range = DwarfpReadEncodedAddress(
                                            Context,
                                            Cie->FdeEncoding & DwarfPeTypeMask,
                                            Cie->AddressSize,
                                            Table);

        Fde->Instructions = *Table;
        if (Cie->Augmentation[0] == 'z') {
            Fde->AugmentationLength = DwarfpReadLeb128(Table);
            Fde->Instructions = *Table + Fde->AugmentationLength;
        }
    }

    Status = 0;

ReadCieOrFdeEnd:
    *Table = UnitEnd;
    return Status;
}

ULONGLONG
DwarfpReadEncodedAddress (
    PDWARF_CONTEXT Context,
    DWARF_ADDRESS_ENCODING Encoding,
    UCHAR AddressSize,
    PUCHAR *Table
    )

/*++

Routine Description:

    This routine reads an encoded target address.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Encoding - Supplies the address encoding format.

    AddressSize - Supplies the size of a target address.

    Table - Supplies a pointer that on input contains a pointer to the start of
        the section. On output this pointer will be advanced past the fields
        scanned.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ULONGLONG Value;

    if (Encoding == DwarfPeOmit) {
        return 0;
    }

    Value = 0;
    switch (Encoding & DwarfPeModifierMask) {
    case DwarfPeAbsolute:
        break;

    //
    // PC-relative is relative to the current table pointer. It's not actually
    // loaded at it's true VA, so get the offset into the .eh_frame section
    // and then add the .eh_frame VA. The .debug_frame sections do not have
    // pointer encodings, so it only applies to .eh_frame.
    //

    case DwarfPePcRelative:
        Value = *Table - (PUCHAR)(Context->Sections.EhFrame.Data) +
                Context->Sections.EhFrameAddress;

        break;

    //
    // Consider supporting other modifiers as needed.
    //

    default:

        assert(FALSE);

        break;
    }

    switch (Encoding & DwarfPeTypeMask) {
    case DwarfPeAbsolute:
        if (AddressSize == 8) {
            Value += DwarfpRead8(Table);

        } else {

            assert(AddressSize == 4);

            Value += DwarfpRead4(Table);
        }

        break;

    case DwarfPeLeb128:
        Value += DwarfpReadLeb128(Table);
        break;

    case DwarfPeUdata2:
        Value += DwarfpRead2(Table);
        break;

    case DwarfPeUdata4:
        Value += DwarfpRead4(Table);
        break;

    case DwarfPeUdata8:
        Value += DwarfpRead8(Table);
        break;

    case DwarfPeSigned:
        if (AddressSize == 8) {
            Value += (LONGLONG)DwarfpRead8(Table);

        } else {

            assert(AddressSize == 4);

            Value += (LONG)DwarfpRead4(Table);
        }

        break;

    case DwarfPeSleb128:
        Value += DwarfpReadSleb128(Table);
        break;

    case DwarfPeSdata2:
        Value += (SHORT)DwarfpRead2(Table);
        break;

    case DwarfPeSdata4:
        Value += (LONG)DwarfpRead4(Table);
        break;

    case DwarfPeSdata8:
        Value += (LONGLONG)DwarfpRead8(Table);
        break;

    default:

        assert(FALSE);

        Value = 0;
        break;
    }

    //
    // This would be where a dereference occurs.
    //

    if ((Encoding & DwarfPeIndirect) != 0) {
        Value = 0;
    }

    return Value;
}

