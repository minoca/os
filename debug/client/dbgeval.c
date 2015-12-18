/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    dbgeval.c

Abstract:

    This module implements arithmetic expression evaluation for the debugger.

Author:

    Evan Green 11-Jul-2012

Environment:

    Debug client.

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dbgrtl.h"
#include <minoca/spproto.h>
#include <minoca/im.h>
#include "dbgext.h"
#include "symbols.h"
#include "dbgapi.h"
#include "dbgrprof.h"
#include "dbgrcomm.h"
#include "dbgsym.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _EVALUATION_OPERATOR {
    OperatorInvalid,
    OperatorValue,
    OperatorAdd,
    OperatorSubtract,
    OperatorMultiply,
    OperatorDivide,
    OperatorOpenParentheses,
    OperatorCloseParentheses
} EVALUATION_OPERATOR, *PEVALUATION_OPERATOR;

typedef struct _STACK_ELEMENT STACK_ELEMENT, *PSTACK_ELEMENT;
struct _STACK_ELEMENT {
    PLIST_ENTRY ListHead;
    PSTACK_ELEMENT NextElement;
};

typedef struct _EVALUATION_ELEMENT {
    LIST_ENTRY ListEntry;
    EVALUATION_OPERATOR Operator;
    ULONGLONG Value;
} EVALUATION_ELEMENT, *PEVALUATION_ELEMENT;

typedef struct _EVALUATION_DATA {
    PSTACK_ELEMENT StackTop;
    PSTR InitialString;
    PSTR String;
} EVALUATION_DATA, *PEVALUATION_DATA;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
EvalGetNextToken (
    PDEBUGGER_CONTEXT Context,
    PEVALUATION_DATA State,
    PEVALUATION_OPERATOR Operator,
    PULONGLONG Value
    );

BOOL
EvalEvaluateBasicList (
    PLIST_ENTRY ListHead,
    PULONGLONG Result
    );

INT
EvalGetAddressFromSymbol (
    PDEBUGGER_CONTEXT Context,
    PSTR SymbolName,
    PULONGLONG Address
    );

//
// -------------------------------------------------------------------- Globals
//

//
// --------------------------------------------------------------------- Macros
//

#define MALLOC(_x) malloc(_x)
#define FREE(_x) free(_x)

//
// ------------------------------------------------------------------ Functions
//

INT
DbgEvaluate (
    PDEBUGGER_CONTEXT Context,
    PSTR String,
    PULONGLONG Result
    )

/*++

Routine Description:

    This routine evaluates a mathematical expression. The following operators
    are supported: +, -, *, /, (, ). No spaces are permitted. Module symbols
    are permitted and will be translated into their corresponding address.

Arguments:

    Context - Supplies a pointer to the debugger application context.

    String - Supplies the string to evaluate.

    Result - Supplies a pointer to the 64-bit unsigned integer where the result
        will be stored.

Return Value:

    0 if the expression was successfully evaluated.

    Returns an error code on failure.

--*/

{

    ULONGLONG ComputationValue;
    PEVALUATION_ELEMENT CurrentElement;
    PLIST_ENTRY CurrentElementEntry;
    BOOL CurrentListEmpty;
    PLIST_ENTRY CurrentListHead;
    PSTACK_ELEMENT CurrentStackElement;
    EVALUATION_DATA Evaluation;
    PEVALUATION_ELEMENT NewElement;
    PSTACK_ELEMENT NewStackElement;
    PSTACK_ELEMENT NextStackElement;
    EVALUATION_OPERATOR Operator;
    PSTACK_ELEMENT PoppedStackElement;
    INT Status;
    ULONG StringLength;
    ULONGLONG Value;

    CurrentListHead = NULL;
    Evaluation.StackTop = NULL;
    Evaluation.InitialString = NULL;

    //
    // Parameter check and put a default value in the result.
    //

    if ((String == NULL) || (Result == NULL)) {
        Status = EINVAL;
        goto EvaluateEnd;
    }

    *Result = 0;

    //
    // Make a copy of the string that can be modified.
    //

    StringLength = RtlStringLength(String);
    Evaluation.InitialString = MALLOC(StringLength + 1);
    if (Evaluation.InitialString == NULL) {
        Status = EINVAL;
        goto EvaluateEnd;
    }

    RtlStringCopy(Evaluation.InitialString, String, StringLength + 1);
    Evaluation.String = Evaluation.InitialString;
    CurrentListHead = MALLOC(sizeof(LIST_ENTRY));
    if (CurrentListHead == NULL) {
        Status = EINVAL;
        goto EvaluateEnd;
    }

    INITIALIZE_LIST_HEAD(CurrentListHead);

    //
    // This is the main processing loop.
    //

    CurrentListEmpty = TRUE;
    while (TRUE) {
        Status = EvalGetNextToken(Context, &Evaluation, &Operator, &Value);
        if (Status != 0) {
            goto EvaluateEnd;
        }

        //
        // If it's the end of the string, break out of the processing loop.
        //

        if (Operator == OperatorInvalid) {
            break;
        }

        //
        // If it's an open parentheses, push the current list onto the stack,
        // and start a new list.
        //

        if (Operator == OperatorOpenParentheses) {
            NewStackElement = MALLOC(sizeof(STACK_ELEMENT));
            if (NewStackElement == NULL) {
                Status = EINVAL;
                goto EvaluateEnd;
            }

            NewStackElement->ListHead = CurrentListHead;
            NewStackElement->NextElement = Evaluation.StackTop;
            Evaluation.StackTop = NewStackElement;
            CurrentListHead = MALLOC(sizeof(LIST_ENTRY));
            if (CurrentListHead == NULL) {
                Status = ENOMEM;
                goto EvaluateEnd;
            }

            INITIALIZE_LIST_HEAD(CurrentListHead);
            CurrentListEmpty = TRUE;

        //
        // If it's a close parentheses, the idea is to evaluate the current
        // list, pop the last list, and append the result to that popped list.
        //

        } else if (Operator == OperatorCloseParentheses) {

            //
            // If nothing was evaluated in between the parentheses, that's an
            // error.
            //

            if (CurrentListEmpty != FALSE) {
                Status = EINVAL;
                goto EvaluateEnd;
            }

            //
            // Evaluate the current list. If successful, all elements except the
            // list head will be freed automatically.
            //

            Status = EvalEvaluateBasicList(CurrentListHead, &ComputationValue);
            if (Status == FALSE) {
                Status = EINVAL;
                goto EvaluateEnd;
            }

            //
            // Free the current list head, and pop the last list off the stack.
            //

            FREE(CurrentListHead);
            PoppedStackElement = Evaluation.StackTop;
            CurrentListHead = PoppedStackElement->ListHead;
            Evaluation.StackTop = Evaluation.StackTop->NextElement;
            FREE(PoppedStackElement);

            //
            // Create a new value element and insert it into the popped list.
            //

            NewElement = MALLOC(sizeof(EVALUATION_ELEMENT));
            if (NewElement == NULL) {
                Status = ENOMEM;
                goto EvaluateEnd;
            }

            memset(NewElement, 0, sizeof(EVALUATION_ELEMENT));
            NewElement->Operator = OperatorValue;
            NewElement->Value = ComputationValue;
            INSERT_BEFORE(&(NewElement->ListEntry), CurrentListHead);
            CurrentListEmpty = FALSE;

        } else {

            //
            // It must just be a normal operator or a value. Add it to the list.
            //

            NewElement = MALLOC(sizeof(EVALUATION_ELEMENT));
            if (NewElement == NULL) {
                Status = ENOMEM;
                goto EvaluateEnd;
            }

            memset(NewElement, 0, sizeof(EVALUATION_ELEMENT));
            NewElement->Operator = Operator;
            NewElement->Value = Value;
            INSERT_BEFORE(&(NewElement->ListEntry), CurrentListHead);
            CurrentListEmpty = FALSE;
        }
    }

    Status = EvalEvaluateBasicList(CurrentListHead, Result);
    if (Status == FALSE) {
        Status = EINVAL;
        goto EvaluateEnd;
    }

    Status = 0;

EvaluateEnd:

    //
    // Clean up the current list, if one exists.
    //

    if (CurrentListHead != NULL) {
        CurrentElementEntry = CurrentListHead->Next;
        while (CurrentElementEntry != CurrentListHead) {
            CurrentElement = LIST_VALUE(CurrentElementEntry,
                                        EVALUATION_ELEMENT,
                                        ListEntry);

            CurrentElementEntry = CurrentElementEntry->Next;
            FREE(CurrentElement);
        }

        FREE(CurrentListHead);
    }

    //
    // Clean up any leftover elements on the stack.
    //

    CurrentStackElement = Evaluation.StackTop;
    while (CurrentStackElement != NULL) {

        //
        // Free the list associated with this stack element.
        //

        CurrentElementEntry = CurrentStackElement->ListHead->Next;
        while (CurrentElementEntry != CurrentStackElement->ListHead) {
            CurrentElement = LIST_VALUE(CurrentElementEntry,
                                        EVALUATION_ELEMENT,
                                        ListEntry);

            CurrentElementEntry = CurrentElementEntry->Next;
            FREE(CurrentElement);
        }

        FREE(CurrentStackElement->ListHead);
        NextStackElement = CurrentStackElement->NextElement;
        FREE(CurrentStackElement);
        CurrentStackElement = NextStackElement;
    }

    //
    // Free the string copy.
    //

    if (Evaluation.InitialString != NULL) {
        FREE(Evaluation.InitialString);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
EvalEvaluateBasicList (
    PLIST_ENTRY ListHead,
    PULONGLONG Result
    )

/*++

Routine Description:

    This routine evaluates a simple arithmetic expression. It supports
    +, -, *, and / operators, but not parentheses.

Arguments:

    ListHead - Supplies a pointer to the beginning of a list of
        EVALUATION_ELEMENTs. If completely successful, this list will have no
        members in it upon return, the members will have been freed. Upon
        failure, some of the members may have been freed, and any remaining ones
        will still be in the list.

    Result - Supplies a pointer where the resulting number will be returned.

Return Value:

    Returns TRUE if the function was successful, or NULL if there was a syntax
    error.

--*/

{

    PEVALUATION_ELEMENT ComputationResult;
    PEVALUATION_ELEMENT CurrentElement;
    PLIST_ENTRY CurrentEntry;
    EVALUATION_OPERATOR CurrentPassOperator1;
    EVALUATION_OPERATOR CurrentPassOperator2;
    PEVALUATION_ELEMENT LeftOperand;
    ULONG Pass;
    PEVALUATION_ELEMENT RightOperand;

    *Result = 0;

    //
    // The first pass handles * and / operators only. The second pass handles
    // + and -. This accomplishes order of operations. Not allowing parentheses
    // makes this simplification possible.
    //

    for (Pass = 0; Pass < 2; Pass += 1) {

        //
        // Set the operators being looked at for the current pass.
        //

        if (Pass == 0) {
            CurrentPassOperator1 = OperatorMultiply;
            CurrentPassOperator2 = OperatorDivide;

        } else {
            CurrentPassOperator1 = OperatorAdd;
            CurrentPassOperator2 = OperatorSubtract;
        }

        CurrentEntry = ListHead->Next;
        while (CurrentEntry != ListHead) {
            CurrentElement = LIST_VALUE(CurrentEntry,
                                        EVALUATION_ELEMENT,
                                        ListEntry);

            //
            // Only basic operators and values are supported.
            //

            if ((CurrentElement->Operator == OperatorInvalid) ||
                (CurrentElement->Operator == OperatorOpenParentheses) ||
                (CurrentElement->Operator == OperatorCloseParentheses)) {

                return FALSE;
            }

            if ((CurrentElement->Operator == CurrentPassOperator1) ||
                (CurrentElement->Operator == CurrentPassOperator2)) {

                //
                // This had better not be the first or the last value in the
                // list.
                //

                if ((CurrentElement->ListEntry.Next == ListHead) ||
                    (CurrentElement->ListEntry.Previous == ListHead)) {

                    return FALSE;
                }

                //
                // Get the operands on either side of this one. They had better
                // be values (not other operands).
                //

                LeftOperand = LIST_VALUE(CurrentElement->ListEntry.Previous,
                                         EVALUATION_ELEMENT,
                                         ListEntry);

                RightOperand = LIST_VALUE(CurrentElement->ListEntry.Next,
                                          EVALUATION_ELEMENT,
                                          ListEntry);

                if ((LeftOperand->Operator != OperatorValue) ||
                    (RightOperand->Operator != OperatorValue)) {

                    return FALSE;
                }

                //
                // Based on the operator, compute the result and store it into
                // the operator's value. This will become the result.
                //

                if (CurrentElement->Operator == OperatorMultiply) {
                    CurrentElement->Value = LeftOperand->Value *
                                            RightOperand->Value;

                } else if (CurrentElement->Operator == OperatorDivide) {
                    CurrentElement->Value = LeftOperand->Value /
                                            RightOperand->Value;

                } else if (CurrentElement->Operator == OperatorAdd) {
                    CurrentElement->Value = LeftOperand->Value +
                                            RightOperand->Value;

                } else if (CurrentElement->Operator == OperatorSubtract) {
                    CurrentElement->Value = LeftOperand->Value -
                                            RightOperand->Value;

                } else {
                    return FALSE;
                }

                CurrentElement->Operator = OperatorValue;

                //
                // Remove the left and right operands from the list and free
                // their data structures.
                //

                LIST_REMOVE(&(LeftOperand->ListEntry));
                LIST_REMOVE(&(RightOperand->ListEntry));
                FREE(LeftOperand);
                FREE(RightOperand);
            }

            CurrentEntry = CurrentEntry->Next;
        }
    }

    //
    // Both passes have completed. In the end one lone value should remain. If
    // something else is there, there was a syntax error.
    //

    ComputationResult = LIST_VALUE(ListHead->Next,
                                   EVALUATION_ELEMENT,
                                   ListEntry);

    if (ComputationResult->ListEntry.Next != ListHead) {
        return FALSE;
    }

    if (ComputationResult->Operator != OperatorValue) {
        return FALSE;
    }

    *Result = ComputationResult->Value;
    LIST_REMOVE(&(ComputationResult->ListEntry));
    FREE(ComputationResult);
    return TRUE;
}

INT
EvalGetNextToken (
    PDEBUGGER_CONTEXT Context,
    PEVALUATION_DATA State,
    PEVALUATION_OPERATOR Operator,
    PULONGLONG Value
    )

/*++

Routine Description:

    This routine retrieves the next token from the evaluation string.

Arguments:

    Context - Supplies a pointer to the application context.

    State - Supplies a pointer to the current evaluation state.

    Operator - Supplies a pointer that will receive the operator of the next
        token. If the end of the string is reached, the operator will be set to
        OperatorInvalid.

    Value - Supplies a pointer that will receieve the value of the token if it's
        not an operator.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PARM_GENERAL_REGISTERS ArmRegisters;
    ULONG Base;
    PSTR CurrentPosition;
    UCHAR FirstCharacter;
    INT Result;
    UCHAR SecondCharacter;
    PSTR SymbolEnd;
    UCHAR TerminatorValue;
    PX86_GENERAL_REGISTERS X86Registers;

    Result = 0;
    *Operator = OperatorInvalid;
    *Value = 0;
    FirstCharacter = *(State->String);
    CurrentPosition = State->String + 1;
    SymbolEnd = State->String + 1;

    //
    // If it's end of the string, end now.
    //

    if (FirstCharacter == '\0') {
        goto GetNextTokenEnd;
    }

    //
    // Find the end of the symbol.
    //

    while ((*SymbolEnd != '\0') &&
           (*SymbolEnd != '+') &&
           (*SymbolEnd != '-') &&
           (*SymbolEnd != '*') &&
           (*SymbolEnd != '/') &&
           (*SymbolEnd != '(') &&
           (*SymbolEnd != ')') &&
           (*SymbolEnd != '@')) {

        SymbolEnd += 1;
    }

    //
    // Check the standard operators.
    //

    if (FirstCharacter == '+') {
        *Operator = OperatorAdd;

    } else if (FirstCharacter == '-') {
        *Operator = OperatorSubtract;

    } else if (FirstCharacter == '*') {
        *Operator = OperatorMultiply;

    } else if (FirstCharacter == '/') {
        *Operator = OperatorDivide;

    } else if (FirstCharacter == '(') {
        *Operator = OperatorOpenParentheses;

    } else if (FirstCharacter == ')') {
        *Operator = OperatorCloseParentheses;

    //
    // If it's a digit between 1 and 9, it's clearly a number. Treat it as hex.
    //

    } else if ((FirstCharacter >= '1') && (FirstCharacter <= '9')) {
        *Value = strtoull(State->String, (PCHAR *)&CurrentPosition, 16);
        if (CurrentPosition == State->String) {
            Result = EINVAL;
            goto GetNextTokenEnd;
        }

        *Operator = OperatorValue;

    //
    // If the first character is 0, it could be a hex or decimal number. Check
    // the second digit to determine which one. Default to hex if nothing is
    // specified.
    //

    } else if (FirstCharacter == '0') {
        SecondCharacter = *(State->String + 1);
        Base = 16;
        if (SecondCharacter == 'x') {
            State->String += 2;
            Base = 16;

        } else if (SecondCharacter == 'n') {
            State->String += 2;
            Base = 10;
        }

        *Value = strtoll(State->String, (PCHAR *)&CurrentPosition, Base);
        if (CurrentPosition == State->String) {
            Result = EINVAL;
            goto GetNextTokenEnd;
        }

        *Operator = OperatorValue;

    //
    // The first character is an @, so the value is a register.
    //
    //

    } else if (FirstCharacter == '@') {

        assert(Context->CurrentEvent.Type == DebuggerEventBreak);

        X86Registers = &(Context->FrameRegisters.X86);
        ArmRegisters = &(Context->FrameRegisters.Arm);

        //
        // Assume success and set the operator. It will be set back if the
        // lookup fails. terminate the symbol string and attempt to look up the
        // register.
        //

        *Operator = OperatorValue;
        TerminatorValue = *SymbolEnd;
        *SymbolEnd = '\0';
        switch (Context->MachineType) {

        //
        // Get x86 registers.
        //

        case MACHINE_TYPE_X86:

            //
            // Start with the 32 bit registers.
            //

            if (strcasecmp(CurrentPosition, "eax") == 0) {
                *Value = X86Registers->Eax & 0xFFFFFFFF;

            } else if (strcasecmp(CurrentPosition, "ebx") == 0) {
                *Value = X86Registers->Ebx & 0xFFFFFFFF;

            } else if (strcasecmp(CurrentPosition, "ecx") == 0) {
                *Value = X86Registers->Ecx & 0xFFFFFFFF;

            } else if (strcasecmp(CurrentPosition, "edx") == 0) {
                *Value = X86Registers->Edx & 0xFFFFFFFF;

            } else if (strcasecmp(CurrentPosition, "esi") == 0) {
                *Value = X86Registers->Esi & 0xFFFFFFFF;

            } else if (strcasecmp(CurrentPosition, "edi") == 0) {
                *Value = X86Registers->Edi & 0xFFFFFFFF;

            } else if (strcasecmp(CurrentPosition, "esp") == 0) {
                *Value = X86Registers->Esp & 0xFFFFFFFF;

            } else if (strcasecmp(CurrentPosition, "ebp") == 0) {
                *Value = X86Registers->Ebp & 0xFFFFFFFF;

            } else if (strcasecmp(CurrentPosition, "eip") == 0) {
                *Value = X86Registers->Eip & 0xFFFFFFFF;

            } else if (strcasecmp(CurrentPosition, "eflags") == 0) {
                *Value = X86Registers->Eflags & 0xFFFFFFFF;

            //
            // Check the 8 bit registers.
            //

            } else if (strcasecmp(CurrentPosition, "al") == 0) {
                *Value = X86Registers->Eax & 0x00FF;

            } else if (strcasecmp(CurrentPosition, "ah") == 0) {
                *Value = X86Registers->Eax & 0xFF00;

            } else if (strcasecmp(CurrentPosition, "bl") == 0) {
                *Value = X86Registers->Ebx & 0x00FF;

            } else if (strcasecmp(CurrentPosition, "bh") == 0) {
                *Value = X86Registers->Ebx & 0xFF00;

            } else if (strcasecmp(CurrentPosition, "cl") == 0) {
                *Value = X86Registers->Ecx & 0x00FF;

            } else if (strcasecmp(CurrentPosition, "ch") == 0) {
                *Value = X86Registers->Ecx & 0xFF00;

            } else if (strcasecmp(CurrentPosition, "dl") == 0) {
                *Value = X86Registers->Edx & 0x00FF;

            } else if (strcasecmp(CurrentPosition, "dh") == 0) {
                *Value = X86Registers->Edx & 0xFF00;

            //
            // Finally, check the 16 bit registers.
            //

            } else if (strcasecmp(CurrentPosition, "ax") == 0) {
                *Value = X86Registers->Eax & 0xFFFF;

            } else if (strcasecmp(CurrentPosition, "bx") == 0) {
                *Value = X86Registers->Ebx & 0xFFFF;

            } else if (strcasecmp(CurrentPosition, "cx") == 0) {
                *Value = X86Registers->Ecx & 0xFFFF;

            } else if (strcasecmp(CurrentPosition, "dx") == 0) {
                *Value = X86Registers->Edx & 0xFFFF;

            } else if (strcasecmp(CurrentPosition, "si") == 0) {
                *Value = X86Registers->Esi & 0xFFFF;

            } else if (strcasecmp(CurrentPosition, "di") == 0) {
                *Value = X86Registers->Edi & 0xFFFF;

            } else if (strcasecmp(CurrentPosition, "sp") == 0) {
                *Value = X86Registers->Esp & 0xFFFF;

            } else if (strcasecmp(CurrentPosition, "bp") == 0) {
                *Value = X86Registers->Ebp & 0xFFFF;

            } else if (strcasecmp(CurrentPosition, "ip") == 0) {
                *Value = X86Registers->Eip & 0xFFFF;

            } else if (strcasecmp(CurrentPosition, "flags") == 0) {
                *Value = X86Registers->Eflags & 0xFFFF;

            } else if (strcasecmp(CurrentPosition, "cs") == 0) {
                *Value = X86Registers->Cs;

            } else if (strcasecmp(CurrentPosition, "ds") == 0) {
                *Value = X86Registers->Ds;

            } else if (strcasecmp(CurrentPosition, "es") == 0) {
                *Value = X86Registers->Es;

            } else if (strcasecmp(CurrentPosition, "fs") == 0) {
                *Value = X86Registers->Fs;

            } else if (strcasecmp(CurrentPosition, "gs") == 0) {
                *Value = X86Registers->Gs;

            } else if (strcasecmp(CurrentPosition, "ss") == 0) {
                *Value = X86Registers->Ss;

            //
            // No valid register name could be found. This is a syntax error.
            //

            } else {
                *Operator = OperatorInvalid;
                Result = EINVAL;
                goto GetNextTokenEnd;
            }

            break;

        //
        // Get ARM regisers.
        //

        case MACHINE_TYPE_ARMV7:
        case MACHINE_TYPE_ARMV6:
            if (strcasecmp(CurrentPosition, "r0") == 0) {
                *Value = ArmRegisters->R0;

            } else if (strcasecmp(CurrentPosition, "r1") == 0) {
                *Value = ArmRegisters->R1;

            } else if (strcasecmp(CurrentPosition, "r2") == 0) {
                *Value = ArmRegisters->R2;

            } else if (strcasecmp(CurrentPosition, "r3") == 0) {
                *Value = ArmRegisters->R3;

            } else if (strcasecmp(CurrentPosition, "r4") == 0) {
                *Value = ArmRegisters->R4;

            } else if (strcasecmp(CurrentPosition, "r5") == 0) {
                *Value = ArmRegisters->R5;

            } else if (strcasecmp(CurrentPosition, "r6") == 0) {
                *Value = ArmRegisters->R6;

            } else if (strcasecmp(CurrentPosition, "r7") == 0) {
                *Value = ArmRegisters->R7;

            } else if (strcasecmp(CurrentPosition, "r8") == 0) {
                *Value = ArmRegisters->R8;

            } else if (strcasecmp(CurrentPosition, "r9") == 0) {
                *Value = ArmRegisters->R9;

            } else if ((strcasecmp(CurrentPosition, "r10") == 0) ||
                       (strcasecmp(CurrentPosition, "sl") == 0)) {

                *Value = ArmRegisters->R10;

            } else if ((strcasecmp(CurrentPosition, "r11") == 0) ||
                       (strcasecmp(CurrentPosition, "fp") == 0)) {

                *Value = ArmRegisters->R11Fp;

            } else if ((strcasecmp(CurrentPosition, "r12") == 0) ||
                       (strcasecmp(CurrentPosition, "ip") == 0)) {

                *Value = ArmRegisters->R12Ip;

            } else if ((strcasecmp(CurrentPosition, "r13") == 0) ||
                       (strcasecmp(CurrentPosition, "sp") == 0)) {

                *Value = ArmRegisters->R13Sp;

            } else if ((strcasecmp(CurrentPosition, "r14") == 0) ||
                       (strcasecmp(CurrentPosition, "lr") == 0)) {

                *Value = ArmRegisters->R14Lr;

            } else if ((strcasecmp(CurrentPosition, "r15") == 0) ||
                       (strcasecmp(CurrentPosition, "pc") == 0)) {

                *Value = ArmRegisters->R15Pc;

            } else if (strcasecmp(CurrentPosition, "cpsr") == 0) {
                *Value = ArmRegisters->Cpsr;

            } else {
                *Operator = OperatorInvalid;
                Result = EINVAL;
                goto GetNextTokenEnd;
            }

            break;

        //
        // Unknown machine type.
        //

        default:
            *Operator = OperatorInvalid;
            Result = EINVAL;
            goto GetNextTokenEnd;
        }

        //
        // Restore the original string, and update the string position.
        //

        *SymbolEnd = TerminatorValue;
        CurrentPosition = SymbolEnd;

    //
    // The first character is not a recognized character, so assume it's a
    // symbol of some sort. Attempt to look up the symbol.
    //

    } else {

        //
        // Save the character after the symbol string, terminate the symbol
        // string, and attempt to look up the symbol.
        //

        TerminatorValue = *SymbolEnd;
        *SymbolEnd = '\0';
        Result = EvalGetAddressFromSymbol(Context, State->String, Value);

        //
        // Restore the original character. If the result was valid, then the
        // output parameter was already updated.
        //

        *SymbolEnd = TerminatorValue;
        CurrentPosition = SymbolEnd;
        if (Result == 0) {
            *Operator = OperatorValue;
        }
    }

    State->String = CurrentPosition;

GetNextTokenEnd:
    return Result;
}

INT
EvalGetAddressFromSymbol (
    PDEBUGGER_CONTEXT Context,
    PSTR SymbolName,
    PULONGLONG Address
    )

/*++

Routine Description:

    This routine converts a symbol name into a virtual address.

Arguments:

    Context - Supplies a pointer to the application context.

    SymbolName - Supplies the string containing the name of the symbol.

    Address - Supplies a pointer that receives the resulting address of the
        symbol.

Return Value:

    0 if a data or function symbol was found.

    Returns an error code if no match could be made.

--*/

{

    PDEBUGGER_MODULE CurrentModule;
    PLIST_ENTRY CurrentModuleEntry;
    PSTR ModuleEnd;
    ULONG ModuleLength;
    INT Result;
    PSYMBOL_SEARCH_RESULT ResultValid;
    SYMBOL_SEARCH_RESULT SearchResult;
    PDEBUGGER_MODULE UserModule;

    Result = EINVAL;
    UserModule = NULL;
    *Address = (INTN)NULL;

    //
    // If an exclamation point exists, then the module was specified. Find that
    // module.
    //

    ModuleEnd = strchr(SymbolName, '!');
    if (ModuleEnd != NULL) {
        ModuleLength = (UINTN)ModuleEnd - (UINTN)SymbolName;
        UserModule = DbgpGetModule(Context, SymbolName, ModuleLength);
        if (UserModule == NULL) {
            DbgOut("Module %s not found.\n", SymbolName);
            Result = EINVAL;
            goto GetAddressFromSymbolEnd;
        }

        //
        // Move the search string and initialize the list entry.
        //

        SymbolName = ModuleEnd + 1;
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
        // Find the symbol. This is not a search function, so accept the first
        // result.
        //

        SearchResult.Variety = SymbolResultInvalid;
        ResultValid = DbgpFindSymbolInModule(CurrentModule->Symbols,
                                             SymbolName,
                                             &SearchResult);

        //
        // If one was found, break out of this loop.
        //

        if ((ResultValid != NULL) &&
            (SearchResult.Variety != SymbolResultInvalid)) {

            if (SearchResult.Variety == SymbolResultFunction) {
                *Address = SearchResult.U.FunctionResult->StartAddress +
                           CurrentModule->BaseDifference;

                Result = 0;
                break;

            } else if ((SearchResult.Variety == SymbolResultData) &&
                       (SearchResult.U.DataResult->LocationType ==
                        DataLocationAbsoluteAddress)) {

                *Address = SearchResult.U.DataResult->Location.Address +
                           CurrentModule->BaseDifference;

                Result = 0;
                break;
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

GetAddressFromSymbolEnd:
    return Result;
}

