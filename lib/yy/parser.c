/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    parser.c

Abstract:

    This module implements the actual parsing code for the LALR(1) parser. The
    yygen library should have been previously used to compile the grammar data.

Author:

    Evan Green 15-May-2016

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "yyp.h"
#include <stdio.h>
#include <string.h>

//
// --------------------------------------------------------------------- Macros
//

#define YY_SYMBOL_NAME_INDEX(_Parser, _Value) \
    (((_Value) > (_Parser)->Grammar->MaxToken) ? \
     (_Parser)->Grammar->UndefinedToken : \
     (_Value))

#define YY_SYMBOL_NAME(_Parser, _Value) \
    ((_Parser)->Grammar->Names[YY_SYMBOL_NAME_INDEX((_Parser), (_Value))])

#define YypReallocate(_Parser, _Memory, _NewSize) \
    (_Parser)->Reallocate((_Parser)->Context, (_Memory), (_NewSize))

//
// ---------------------------------------------------------------- Definitions
//

#define YY_INITIAL_STACK_SIZE 256
#define YY_MAX_STACK_SIZE 10000

#define YY_EMPTY (-1)
#define YY_EOF 0
#define YY_ERROR_TOKEN 1

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the state for the current parse stack. The size of
    each element on the value stack is specified in the parser structure.

Members:

    ValueBase - Stores a pointer to the base of the parse stack of values.

    ValueTop - Stores a pointer to the current top of the value stack, the
        most recently pushed value. The stack grows up.

    ValueLast - Stores the last valid allocation on the stack before the
        stack will need to be reallocated.

    StateBase - Stores a pointer to the stack of states, which runs parallel
        to the stack of values.

    StateTop - Stores a pointer to the top of the stack of states, the most
        recent state that was pushed.

    Count - Stores the capacity of the stack, in elements.

--*/

typedef struct _YY_PARSE_STACK {
    PVOID ValueBase;
    PVOID ValueTop;
    PVOID ValueLast;
    PYY_VALUE StateBase;
    PYY_VALUE StateTop;
    UINTN Count;
} YY_PARSE_STACK, *PYY_PARSE_STACK;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
YypGrowStack (
    PYY_PARSER Parser,
    PYY_PARSE_STACK Stack
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

YY_API
YY_STATUS
YyParseGrammar (
    PYY_PARSER Parser
    )

/*++

Routine Description:

    This routine parses input according to an LALR(1) compiled grammar.

Arguments:

    Parser - Supplies a pointer to the initialized parser context.

Return Value:

    0 on success.

    Returns a non-zero value if the parsing failed, the lexer failed, or the
    callback failed.

--*/

{

    PSTR DebugPrefix;
    INT ErrorFlag;
    PVOID FirstElement;
    PYY_GRAMMAR Grammar;
    YY_VALUE LeftSide;
    YY_VALUE Length;
    PVOID LexValue;
    const char *Name;
    PVOID NewValue;
    YY_VALUE Next;
    YY_PARSE_STACK Stack;
    YY_VALUE State;
    INT Status;
    YY_VALUE Symbol;
    YY_VALUE TableIndex;
    INT ValueSize;

    NewValue = NULL;
    Stack.ValueBase = NULL;
    Stack.ValueTop = NULL;
    Stack.ValueLast = NULL;
    Stack.StateBase = NULL;
    Stack.StateTop = NULL;
    Stack.Count = 0;
    if ((Parser == NULL) ||
        (Parser->Grammar == NULL) ||
        (Parser->Reallocate == NULL) ||
        (Parser->GetToken == NULL) ||
        (Parser->ValueSize < sizeof(YY_VALUE))) {

        return YyStatusInvalidParameter;
    }

    DebugPrefix = Parser->DebugPrefix;
    Grammar = Parser->Grammar;
    Parser->ErrorCount = 0;
    ErrorFlag = 0;
    Symbol = YY_EMPTY;
    State = 0;
    Status = YypGrowStack(Parser, &Stack);
    if (Status != 0) {
        goto ParseGrammarEnd;
    }

    ValueSize = Parser->ValueSize;
    Stack.ValueTop = Stack.ValueBase;
    Stack.StateTop = Stack.StateBase;
    *((PYY_VALUE)Stack.StateTop) = 0;
    NewValue = YypReallocate(Parser, NULL, (ValueSize * 2));
    if (NewValue == NULL) {
        Status = YyStatusNoMemory;
        goto ParseGrammarEnd;
    }

    LexValue = NewValue + ValueSize;

    //
    // The main parsing loop.
    //

    while (TRUE) {

        //
        // Loop shifting values onto the stack.
        //

        while (TRUE) {

            //
            // Go reduce if this state simply reduces no matter what.
            //

            Next = Grammar->DefaultReductions[State];
            if (Next != 0) {
                break;
            }

            //
            // Grab the next token, stick it in the next unused stack slot in
            // anticipation of a shift.
            //

            if (Symbol < 0) {
                Status = Parser->GetToken(Parser->Lexer, LexValue);
                if (Status != 0) {
                    Status = YyStatusLexError;
                    goto ParseGrammarEnd;
                }

                Symbol = *((PYY_VALUE)LexValue);
                if (DebugPrefix != NULL) {
                    Name = YY_SYMBOL_NAME(Parser, Symbol);
                    printf("%s: state %d, reading %d (%s)\n",
                           DebugPrefix,
                           State,
                           Symbol,
                           Name);
                }
            }

            //
            // The shift index indicates the offset into the table to start
            // looking given a state and a potential shift index. If it matches
            // with the check value, then shift the symbol.
            //

            TableIndex = Grammar->ShiftIndex[State];
            if (TableIndex != 0) {
                TableIndex += Symbol;
                if ((TableIndex >= 0) &&
                    (TableIndex <= Grammar->TableSize) &&
                    (Grammar->Check[TableIndex] == Symbol)) {

                    if (DebugPrefix != NULL) {
                        printf("%s: state %d, shifting to state %d\n",
                               DebugPrefix,
                               State,
                               Grammar->Table[TableIndex]);
                    }

                    if (Stack.ValueTop >= Stack.ValueLast) {
                        Status = YypGrowStack(Parser, &Stack);
                        if (Status != 0) {
                            goto ParseGrammarEnd;
                        }
                    }

                    State = Grammar->Table[TableIndex];
                    Stack.StateTop += 1;
                    *(Stack.StateTop) = State;
                    Stack.ValueTop += ValueSize;
                    memcpy(Stack.ValueTop, LexValue, ValueSize);
                    Symbol = YY_EMPTY;
                    if (ErrorFlag > 0) {
                        ErrorFlag -= 1;
                    }

                    continue;
                }
            }

            //
            // The reduce index table works similarly to the shift index table.
            // Check for a possible reduction with this lookahead.
            //

            TableIndex = Grammar->ReduceIndex[State];
            if (TableIndex != 0) {
                TableIndex += Symbol;
                if ((TableIndex >= 0) &&
                    (TableIndex <= Grammar->TableSize) &&
                    (Grammar->Check[TableIndex] == Symbol)) {

                    Next = Grammar->Table[TableIndex];
                    break;
                }
            }

            //
            // There is neither a shift nor reduce given this token. That's an
            // unspecified syntax error (unless already trying to recover from
            // an error).
            //

            if (ErrorFlag == 0) {
                Parser->ErrorCount += 1;
                Status = YyStatusParseError;
                if (Parser->Error != NULL) {
                    Status = Parser->Error(Parser->Context, Status);
                }

                if (Status != 0) {
                    goto ParseGrammarEnd;
                }
            }

            //
            // Error recovery. Try to observe at least 3 correct actions
            // before declaring the parser resynchronized with the input stream.
            //

            if (ErrorFlag < 3) {
                ErrorFlag = 3;
                while (TRUE) {
                    TableIndex = *(Stack.StateTop) + YY_ERROR_TOKEN;

                    //
                    // Shift the value if there's an error specification.
                    //

                    if ((*(Stack.StateTop) != 0) &&
                        (TableIndex >= 0) &&
                        (TableIndex <= Grammar->TableSize) &&
                        (Grammar->Check[TableIndex] == YY_ERROR_TOKEN)) {

                        if (DebugPrefix != NULL) {
                            printf("%s: state %d, error recovery shifting to "
                                   "state %d\n",
                                   DebugPrefix,
                                   *(Stack.StateTop),
                                   Grammar->Table[TableIndex]);
                        }

                        if (Stack.ValueTop >= Stack.ValueLast) {
                            Status = YypGrowStack(Parser, &Stack);
                            if (Status != 0) {
                                goto ParseGrammarEnd;
                            }
                        }

                        State = Grammar->Table[TableIndex];
                        Stack.StateTop += 1;
                        *(Stack.StateTop) = State;
                        Stack.ValueTop += ValueSize;
                        memcpy(Stack.ValueTop, LexValue, ValueSize);
                        break;

                    //
                    // Discard the state from the stack.
                    //

                    } else {
                        if (DebugPrefix != NULL) {
                            printf("%s: error recovery discarding state %d\n",
                                   DebugPrefix,
                                   *(Stack.StateTop));
                        }

                        if (Stack.ValueTop <= Stack.ValueBase) {
                            Status = YyStatusParseError;
                            goto ParseGrammarEnd;
                        }

                        Stack.StateTop -= 1;
                        Stack.ValueTop -= Parser->ValueSize;
                    }
                }

            //
            // Discard the token.
            //

            } else {
                if (Symbol == YY_EOF) {
                    Status = YyStatusParseError;
                    goto ParseGrammarEnd;
                }

                if (DebugPrefix != NULL) {
                    Name = YY_SYMBOL_NAME(Parser, Symbol);
                    printf("%s: state %d, error recovery discarding token "
                           "%d (%s)\n",
                           DebugPrefix,
                           State,
                           Symbol,
                           Name);
                }

                Symbol = YY_EMPTY;
            }
        }

        //
        // Perform a reduction. The default action is to use the first value.
        //

        Length = Grammar->RuleLength[Next];
        if (Length != 0) {
            FirstElement = Stack.ValueTop - ((Length - 1) * Parser->ValueSize);
            memcpy(NewValue, FirstElement, ValueSize);

        //
        // Just zero out the value for an empty rule.
        //

        } else {
            FirstElement = NULL;
            memset(NewValue, 0, ValueSize);
        }

        LeftSide = Grammar->LeftSide[Next];
        if (DebugPrefix != NULL) {
            printf("%s: state %d, reducing by rule %d (%s)\n",
                   DebugPrefix,
                   State,
                   Next,
                   Grammar->Rules[Next]);
        }

        Status = Parser->Callback(Parser->Context,
                                  LeftSide + Grammar->MaxToken + 2,
                                  FirstElement,
                                  Length,
                                  NewValue);

        if (Status != 0) {
            goto ParseGrammarEnd;
        }

        Stack.StateTop -= Length;
        State = *(Stack.StateTop);
        Stack.ValueTop -= Length * Parser->ValueSize;

        //
        // Handle the accept condition.
        //

        if ((State == 0) && (LeftSide == Grammar->FinalSymbol)) {
            if (DebugPrefix != NULL) {
                printf("%s: after reduction, go from state 0 to state "
                       "%d (final)\n",
                       DebugPrefix,
                       Grammar->FinalState);
            }

            State = Grammar->FinalState;
            Stack.StateTop += 1;
            *(Stack.StateTop) = State;
            Stack.ValueTop += Parser->ValueSize;
            memcpy(Stack.ValueTop, NewValue, ValueSize);
            if (Symbol < 0) {
                Status = Parser->GetToken(Parser->Lexer, LexValue);
                if (Status != 0) {
                    goto ParseGrammarEnd;
                }

                Symbol = *((PYY_VALUE)LexValue);
                if (DebugPrefix != NULL) {
                    Name = YY_SYMBOL_NAME(Parser, Symbol);
                    printf("%s: state %d, reading %d (%s)\n",
                           DebugPrefix,
                           State,
                           Symbol,
                           Name);
                }
            }

            //
            // The happy ending.
            //

            if (Symbol == YY_EOF) {
                break;
            }

            continue;
        }

        //
        // After reducing, find the next state to go to.
        //

        TableIndex = Grammar->GotoIndex[LeftSide] + State;
        if ((Grammar->GotoIndex[LeftSide] != 0) &&
            (TableIndex >= 0) &&
            (TableIndex <= Grammar->TableSize) &&
            (Grammar->Check[TableIndex] == State)) {

            State = Grammar->Table[TableIndex];

        } else {
            State = Grammar->DefaultGotos[LeftSide];
        }

        if (DebugPrefix != NULL) {
            printf("%s: after reduction, go from state %d to state %d\n",
                   DebugPrefix,
                   *(Stack.StateTop),
                   State);
        }

        //
        // Replace the stack value with what the callback returned.
        //

        if (Stack.ValueTop >= Stack.ValueLast) {
            Status = YypGrowStack(Parser, &Stack);
            if (Status != 0) {
                goto ParseGrammarEnd;
            }
        }

        Stack.StateTop += 1;
        *(Stack.StateTop) = State;
        Stack.ValueTop += Parser->ValueSize;
        memcpy(Stack.ValueTop, NewValue, ValueSize);
    }

    Status = 0;

ParseGrammarEnd:
    if (Parser->ErrorCount != 0) {
        if (Status == 0) {
            Status = YyStatusParseError;
        }

    } else {

        //
        // If there was an error but the parser error count hasn't been
        // incremented, report the error now.
        //

        if (Status != 0) {
            if (Parser->Error != NULL) {
                Parser->Error(Parser->Context, Status);
            }
        }
    }

    if (NewValue != NULL) {
        YypReallocate(Parser, NewValue, 0);
    }

    //
    // Free the stack if necessary.
    //

    if (Stack.ValueBase != NULL) {
        YypReallocate(Parser, Stack.ValueBase, 0);
    }

    if (Stack.StateBase != NULL) {
        YypReallocate(Parser, Stack.StateBase, 0);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
YypGrowStack (
    PYY_PARSER Parser,
    PYY_PARSE_STACK Stack
    )

/*++

Routine Description:

    This routine increases the size of the parser stack.

Arguments:

    Parser - Supplies a pointer to the initialized parser context.

    Stack - Supplies a pointer to the stack information.

Return Value:

    0 on success.

    Returns a non-zero value if the parsing failed, the lexer failed, or the
    callback failed.

--*/

{

    UINTN AllocationSize;
    PVOID NewBuffer;
    UINTN NewCount;
    UINTN Offset;
    UINTN ValueSize;

    NewCount = Stack->Count;
    ValueSize = Parser->ValueSize;
    if (NewCount == 0) {
        NewCount = YY_INITIAL_STACK_SIZE;

    } else if (NewCount >= YY_MAX_STACK_SIZE) {
        return YyStatusTooManyItems;
    }

    NewCount *= 2;
    if (NewCount > YY_MAX_STACK_SIZE) {
        NewCount = YY_MAX_STACK_SIZE;
    }

    Offset = Stack->ValueTop - Stack->ValueBase;
    AllocationSize = NewCount * ValueSize;
    NewBuffer = YypReallocate(Parser, Stack->ValueBase, AllocationSize);
    if (NewBuffer == NULL) {
        return YyStatusNoMemory;
    }

    Stack->ValueTop = NewBuffer + Offset;
    Stack->ValueBase = NewBuffer;
    Stack->ValueLast = NewBuffer + AllocationSize - ValueSize;

    //
    // Also reallocate the state stack.
    //

    Offset = (PVOID)(Stack->StateTop) - (PVOID)(Stack->StateBase);
    AllocationSize = NewCount * sizeof(YY_VALUE);
    NewBuffer = YypReallocate(Parser, Stack->StateBase, AllocationSize);
    if (NewBuffer == NULL) {
        return YyStatusNoMemory;
    }

    Stack->StateTop = NewBuffer + Offset;
    Stack->StateBase = NewBuffer;
    Stack->Count = NewCount;
    return YyStatusSuccess;
}

