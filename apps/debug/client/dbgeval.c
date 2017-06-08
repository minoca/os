/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
#include <minoca/debug/spproto.h>
#include <minoca/lib/im.h>
#include <minoca/debug/dbgext.h>
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
// --------------------------------------------------------------------- Macros
//

#define MALLOC(_x) malloc(_x)
#define FREE(_x) free(_x)

#define ARM_GENERAL_OFFSET(_Name) FIELD_OFFSET(ARM_GENERAL_REGISTERS, _Name)
#define X86_GENERAL_OFFSET(_Name) FIELD_OFFSET(X86_GENERAL_REGISTERS, _Name)
#define X64_GENERAL_OFFSET(_Name) FIELD_OFFSET(X64_GENERAL_REGISTERS, _Name)

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

typedef struct _REGISTER_NAME {
    PCSTR Name;
    ULONG Offset;
    ULONG Size;
} REGISTER_NAME, *PREGISTER_NAME;

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

REGISTER_NAME DbgArmRegisterLocations[] = {
    {"r0", ARM_GENERAL_OFFSET(R0), 4},
    {"r1", ARM_GENERAL_OFFSET(R1), 4},
    {"r2", ARM_GENERAL_OFFSET(R2), 4},
    {"r3", ARM_GENERAL_OFFSET(R3), 4},
    {"r4", ARM_GENERAL_OFFSET(R4), 4},
    {"r5", ARM_GENERAL_OFFSET(R5), 4},
    {"r6", ARM_GENERAL_OFFSET(R6), 4},
    {"r7", ARM_GENERAL_OFFSET(R7), 4},
    {"r8", ARM_GENERAL_OFFSET(R8), 4},
    {"r9", ARM_GENERAL_OFFSET(R9), 4},
    {"r10", ARM_GENERAL_OFFSET(R10), 4},
    {"sl", ARM_GENERAL_OFFSET(R10), 4},
    {"r11", ARM_GENERAL_OFFSET(R11Fp), 4},
    {"fp", ARM_GENERAL_OFFSET(R11Fp), 4},
    {"r12", ARM_GENERAL_OFFSET(R12Ip), 4},
    {"ip", ARM_GENERAL_OFFSET(R12Ip), 4},
    {"r13", ARM_GENERAL_OFFSET(R13Sp), 4},
    {"sp", ARM_GENERAL_OFFSET(R13Sp), 4},
    {"r14", ARM_GENERAL_OFFSET(R14Lr), 4},
    {"lr", ARM_GENERAL_OFFSET(R14Lr), 4},
    {"r15", ARM_GENERAL_OFFSET(R15Pc), 4},
    {"pc", ARM_GENERAL_OFFSET(R15Pc), 4},
    {"cpsr", ARM_GENERAL_OFFSET(Cpsr), 4},
    {0}
};

REGISTER_NAME DbgX86RegisterLocations[] = {
    {"eax", X86_GENERAL_OFFSET(Eax), 4},
    {"ebx", X86_GENERAL_OFFSET(Ebx), 4},
    {"ecx", X86_GENERAL_OFFSET(Ecx), 4},
    {"edx", X86_GENERAL_OFFSET(Edx), 4},
    {"ebp", X86_GENERAL_OFFSET(Ebp), 4},
    {"esp", X86_GENERAL_OFFSET(Esp), 4},
    {"esi", X86_GENERAL_OFFSET(Esi), 4},
    {"edi", X86_GENERAL_OFFSET(Edi), 4},
    {"eip", X86_GENERAL_OFFSET(Eip), 4},
    {"eflags", X86_GENERAL_OFFSET(Eflags), 4},
    {"ax", X86_GENERAL_OFFSET(Eax), 2},
    {"bx", X86_GENERAL_OFFSET(Ebx), 2},
    {"cx", X86_GENERAL_OFFSET(Ecx), 2},
    {"dx", X86_GENERAL_OFFSET(Edx), 2},
    {"bp", X86_GENERAL_OFFSET(Ebp), 2},
    {"sp", X86_GENERAL_OFFSET(Esp), 2},
    {"si", X86_GENERAL_OFFSET(Esi), 2},
    {"di", X86_GENERAL_OFFSET(Edi), 2},
    {"ip", X86_GENERAL_OFFSET(Eip), 2},
    {"flags", X86_GENERAL_OFFSET(Eflags), 2},
    {"al", X86_GENERAL_OFFSET(Eax), 1},
    {"bl", X86_GENERAL_OFFSET(Ebx), 1},
    {"cl", X86_GENERAL_OFFSET(Ecx), 1},
    {"dl", X86_GENERAL_OFFSET(Edx), 1},
    {"ah", X86_GENERAL_OFFSET(Eax) + 1, 1},
    {"bh", X86_GENERAL_OFFSET(Ebx) + 1, 1},
    {"ch", X86_GENERAL_OFFSET(Ecx) + 1, 1},
    {"dh", X86_GENERAL_OFFSET(Edx) + 1, 1},
    {"cs", X86_GENERAL_OFFSET(Cs), 2},
    {"ds", X86_GENERAL_OFFSET(Ds), 2},
    {"es", X86_GENERAL_OFFSET(Es), 2},
    {"fs", X86_GENERAL_OFFSET(Fs), 2},
    {"gs", X86_GENERAL_OFFSET(Gs), 2},
    {"ss", X86_GENERAL_OFFSET(Ss), 2},
    {0}
};

REGISTER_NAME DbgX64RegisterLocations[] = {
    {"rax", X64_GENERAL_OFFSET(Rax), 8},
    {"rbx", X64_GENERAL_OFFSET(Rbx), 8},
    {"rcx", X64_GENERAL_OFFSET(Rcx), 8},
    {"rdx", X64_GENERAL_OFFSET(Rdx), 8},
    {"rbp", X64_GENERAL_OFFSET(Rbp), 8},
    {"rsp", X64_GENERAL_OFFSET(Rsp), 8},
    {"rsi", X64_GENERAL_OFFSET(Rsi), 8},
    {"rdi", X64_GENERAL_OFFSET(Rdi), 8},
    {"r8", X64_GENERAL_OFFSET(R8), 8},
    {"r9", X64_GENERAL_OFFSET(R9), 8},
    {"r10", X64_GENERAL_OFFSET(R10), 8},
    {"r11", X64_GENERAL_OFFSET(R11), 8},
    {"r12", X64_GENERAL_OFFSET(R12), 8},
    {"r13", X64_GENERAL_OFFSET(R13), 8},
    {"r14", X64_GENERAL_OFFSET(R14), 8},
    {"r15", X64_GENERAL_OFFSET(R15), 8},
    {"rip", X64_GENERAL_OFFSET(Rip), 8},
    {"rflags", X64_GENERAL_OFFSET(Rflags), 8},
    {"eax", X64_GENERAL_OFFSET(Rax), 4},
    {"ebx", X64_GENERAL_OFFSET(Rbx), 4},
    {"ecx", X64_GENERAL_OFFSET(Rcx), 4},
    {"edx", X64_GENERAL_OFFSET(Rdx), 4},
    {"ebp", X64_GENERAL_OFFSET(Rbp), 4},
    {"esp", X64_GENERAL_OFFSET(Rsp), 4},
    {"esi", X64_GENERAL_OFFSET(Rsi), 4},
    {"edi", X64_GENERAL_OFFSET(Rdi), 4},
    {"r8d", X64_GENERAL_OFFSET(R8), 4},
    {"r9d", X64_GENERAL_OFFSET(R9), 4},
    {"r10d", X64_GENERAL_OFFSET(R10), 4},
    {"r11d", X64_GENERAL_OFFSET(R11), 4},
    {"r12d", X64_GENERAL_OFFSET(R12), 4},
    {"r13d", X64_GENERAL_OFFSET(R13), 4},
    {"r14d", X64_GENERAL_OFFSET(R14), 4},
    {"r15d", X64_GENERAL_OFFSET(R15), 4},
    {"eip", X64_GENERAL_OFFSET(Rip), 4},
    {"eflags", X64_GENERAL_OFFSET(Rflags), 4},
    {"ax", X64_GENERAL_OFFSET(Rax), 2},
    {"bx", X64_GENERAL_OFFSET(Rbx), 2},
    {"cx", X64_GENERAL_OFFSET(Rcx), 2},
    {"dx", X64_GENERAL_OFFSET(Rdx), 2},
    {"bp", X64_GENERAL_OFFSET(Rbp), 2},
    {"sp", X64_GENERAL_OFFSET(Rsp), 2},
    {"si", X64_GENERAL_OFFSET(Rsi), 2},
    {"di", X64_GENERAL_OFFSET(Rdi), 2},
    {"r8w", X64_GENERAL_OFFSET(R8), 2},
    {"r9w", X64_GENERAL_OFFSET(R9), 2},
    {"r10w", X64_GENERAL_OFFSET(R10), 2},
    {"r11w", X64_GENERAL_OFFSET(R11), 2},
    {"r12w", X64_GENERAL_OFFSET(R12), 2},
    {"r13w", X64_GENERAL_OFFSET(R13), 2},
    {"r14w", X64_GENERAL_OFFSET(R14), 2},
    {"r15w", X64_GENERAL_OFFSET(R15), 2},
    {"ip", X64_GENERAL_OFFSET(Rip), 2},
    {"flags", X64_GENERAL_OFFSET(Rflags), 2},
    {"al", X64_GENERAL_OFFSET(Rax), 1},
    {"bl", X64_GENERAL_OFFSET(Rbx), 1},
    {"cl", X64_GENERAL_OFFSET(Rcx), 1},
    {"dl", X64_GENERAL_OFFSET(Rdx), 1},
    {"bpl", X64_GENERAL_OFFSET(Rbp), 1},
    {"spl", X64_GENERAL_OFFSET(Rsp), 1},
    {"sil", X64_GENERAL_OFFSET(Rsi), 1},
    {"dil", X64_GENERAL_OFFSET(Rdi), 1},
    {"r8b", X64_GENERAL_OFFSET(R8), 1},
    {"r9b", X64_GENERAL_OFFSET(R9), 1},
    {"r10b", X64_GENERAL_OFFSET(R10), 1},
    {"r11b", X64_GENERAL_OFFSET(R11), 1},
    {"r12b", X64_GENERAL_OFFSET(R12), 1},
    {"r13b", X64_GENERAL_OFFSET(R13), 1},
    {"r14b", X64_GENERAL_OFFSET(R14), 1},
    {"r15b", X64_GENERAL_OFFSET(R15), 1},
    {"ah", X64_GENERAL_OFFSET(Rax) + 1, 1},
    {"bh", X64_GENERAL_OFFSET(Rbx) + 1, 1},
    {"ch", X64_GENERAL_OFFSET(Rcx) + 1, 1},
    {"dh", X64_GENERAL_OFFSET(Rdx) + 1, 1},
    {"cs", X64_GENERAL_OFFSET(Cs), 2},
    {"ds", X64_GENERAL_OFFSET(Ds), 2},
    {"es", X64_GENERAL_OFFSET(Es), 2},
    {"fs", X64_GENERAL_OFFSET(Fs), 2},
    {"gs", X64_GENERAL_OFFSET(Gs), 2},
    {"ss", X64_GENERAL_OFFSET(Ss), 2},
    {0}
};

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

BOOL
EvalGetRegister (
    PDEBUGGER_CONTEXT Context,
    PCSTR Register,
    PULONGLONG Value
    )

/*++

Routine Description:

    This routine gets the value of a register by name.

Arguments:

    Context - Supplies a pointer to the application context.

    Register - Supplies the name of the register to get.

    Value - Supplies a pointer where the value of the register will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PREGISTER_NAME Element;
    PVOID Source;

    *Value = 0;
    Element = NULL;
    Source = &(Context->FrameRegisters);
    switch (Context->MachineType) {
    case MACHINE_TYPE_X86:
        Element = DbgX86RegisterLocations;
        break;

    case MACHINE_TYPE_ARM:
        Element = DbgArmRegisterLocations;
        break;

    case MACHINE_TYPE_X64:
        Element = DbgX64RegisterLocations;
        break;

    default:
        return FALSE;
    }

    while (Element->Name != NULL) {
        if (strcasecmp(Register, Element->Name) == 0) {
            memcpy(Value, Source + Element->Offset, Element->Size);
            return TRUE;
        }

        Element += 1;
    }

    return TRUE;
}

BOOL
EvalSetRegister (
    PDEBUGGER_CONTEXT Context,
    PCSTR Register,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine sets the value of a register by name.

Arguments:

    Context - Supplies a pointer to the application context.

    Register - Supplies the name of the register to get.

    Value - Supplies the value to set in the register.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PVOID Destination;
    PREGISTER_NAME Element;

    Element = NULL;

    assert(Context->CurrentEvent.Type == DebuggerEventBreak);

    Destination = &(Context->CurrentEvent.BreakNotification.Registers);
    switch (Context->MachineType) {
    case MACHINE_TYPE_X86:
        Element = DbgX86RegisterLocations;
        break;

    case MACHINE_TYPE_ARM:
        Element = DbgArmRegisterLocations;
        break;

    case MACHINE_TYPE_X64:
        Element = DbgX64RegisterLocations;
        break;

    default:
        return FALSE;
    }

    while (Element->Name != NULL) {
        if (strcasecmp(Register, Element->Name) == 0) {
            memcpy(Destination + Element->Offset, &Value, Element->Size);
            return TRUE;
        }

        Element += 1;
    }

    return TRUE;
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

    ULONG Base;
    PSTR CurrentPosition;
    UCHAR FirstCharacter;
    INT Result;
    UCHAR SecondCharacter;
    PSTR SymbolEnd;
    UCHAR TerminatorValue;

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

        *Value = strtoull(State->String, (PCHAR *)&CurrentPosition, Base);
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

        //
        // Assume success and set the operator. It will be set back if the
        // lookup fails. terminate the symbol string and attempt to look up the
        // register.
        //

        *Operator = OperatorValue;
        TerminatorValue = *SymbolEnd;
        *SymbolEnd = '\0';
        if (EvalGetRegister(Context, CurrentPosition, Value) == FALSE) {
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
    ULONGLONG Pc;
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

                //
                // Add in the thumb bit here so things like "g myfunc" work
                // correctly on Thumb.
                //

                if ((Context->MachineType == MACHINE_TYPE_ARM) &&
                    ((Context->FrameRegisters.Arm.Cpsr &
                      PSR_FLAG_THUMB) != 0)) {

                    *Address |= ARM_THUMB_BIT;
                }

                Result = 0;
                break;

            } else if (SearchResult.Variety == SymbolResultData) {
                Pc = DbgGetPc(Context, &(Context->FrameRegisters)) -
                     CurrentModule->BaseDifference;

                Result = DbgGetDataSymbolAddress(Context,
                                                 CurrentModule->Symbols,
                                                 SearchResult.U.DataResult,
                                                 Pc,
                                                 Address);

                if (Result == 0) {
                    *Address += CurrentModule->BaseDifference;
                    break;
                }
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

