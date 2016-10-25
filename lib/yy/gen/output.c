/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    output.c

Abstract:

    This module supports outputting the final data from the grammar generator.

Author:

    Evan Green 25-Apr-2016

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "yygenp.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

//
// ---------------------------------------------------------------- Definitions
//

#define YY_DEFAULT_SOURCE_FILE_HEADER \
    "/*++\n\n" \
    "Copyright (c) %Y Minoca Corp. All Rights Reserved\n\n" \
    "Module Name:\n\n" \
    "    %f\n\n" \
    "Abstract:\n\n" \
    "    This module implements grammar data. This file is machine " \
    "generated.\n\n" \
    "Author:\n\n" \
    "    Minoca yygen %d-%b-%Y\n\n" \
    "Environment\n\n" \
    "    YY\n\n" \
    "--*/\n\n" \
    "//\n" \
    "// -------------------------------------------------------------------" \
    "Includes\n" \
    "//\n\n" \
    "#include <minoca/lib/types.h>\n" \
    "#include <minoca/lib/status.h>\n" \
    "#include <minoca/lib/yy.h>\n\n" \
    "//\n" \
    "// --------------------------------------------------------------------" \
    "Globals\n" \
    "//\n\n" \

#define YY_VALUES_PER_LINES 10

#define YYGEN_VECTORS_PER_STATE 2

//
// Define how far to rebase token values (except EOF and Error, which are
// always 0 and 1. Set this to 255 for compatibility mode, or 0 normally.
//

#define YYGEN_TOKEN_OUTPUT_BASE 0

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure contains the working state for action output code.

Members:

    VectorCount - Stores the length of the from, to, tally, and width arrays.

    From - Stores the array of source state arrays. This is really two
        consecutive arrays: the first containing the array of shift symbols for
        each state, and the second containing the source reduction states for
        each state.

    To - Stores the array of destination states, running parallel to the from
        arrays.

    Tally - Stores the tally array. This is really two consecutive arrays: the
        first containing the number of shifts for a state and the second
        containing the number of reductions.

    Width - Stores the width array. This runs parallel to the from
        arrays, and indicates the symbol range/size defined by the action.

    Order - Stores an array of the sorted order of the from/to/tally/width
        arrays, sorted on width.

    EntryCount - Stores the number of elements in the order array. This is the
        final number of elements, as it culls elements with a zero tally.

    Base - Stores an array of indices into the table, indexed parallel to the
        from arrays.

    Position - Stores an array of indices into the table, indexed by final
        order.

    Table - Stores the packed table of complete destination states.

    Check - Stores the packed table of source states or shift symbols, indexed
        parallel to the table.

    TableCapacity - Stores the capacity of the table and check arrays before
        they must be reallocated.

    Low - Stores the lowest table index in use.

    High - Stores the highest table index in use.

--*/

typedef struct _YYGEN_ACTION_CONTEXT {
    YY_VALUE VectorCount;
    PYY_VALUE *From;
    PYY_VALUE *To;
    PYY_VALUE Tally;
    PYY_VALUE Width;
    PYY_VALUE Order;
    YY_VALUE EntryCount;
    PYY_VALUE Base;
    PYY_VALUE Position;
    PYY_VALUE Table;
    PYY_VALUE Check;
    YY_VALUE TableCapacity;
    YY_VALUE Low;
    YY_VALUE High;
} YYGEN_ACTION_CONTEXT, *PYYGEN_ACTION_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
YypOutputFileHeader (
    PYYGEN_CONTEXT Context,
    FILE *File
    );

VOID
YypOutputRuleData (
    PYYGEN_CONTEXT Context,
    FILE *File
    );

VOID
YypOutputDefaultReductions (
    PYYGEN_CONTEXT Context,
    FILE *File
    );

YY_STATUS
YypOutputActions (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PYY_VALUE TableSize
    );

YY_STATUS
YypCreateTokenActions (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext
    );

YY_STATUS
YypOutputDefaultGotos (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext,
    FILE *File
    );

YY_STATE_INDEX
YypFindDefaultGoto (
    PYYGEN_CONTEXT Context,
    PYY_STATE_INDEX StateCounts,
    YY_VALUE Symbol
    );

YY_STATUS
YypSaveColumn (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext,
    YY_VALUE Symbol,
    YY_STATE_INDEX DefaultGoto
    );

YY_STATUS
YypSortActions (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext
    );

YY_STATUS
YypPackOutputTable (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext
    );

YY_STATUS
YypPackVector (
    PYYGEN_ACTION_CONTEXT ActionContext,
    YY_VALUE EntryIndex,
    PYY_VALUE TableIndex
    );

YY_STATE_INDEX
YypFindMatchingVector (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext,
    YY_VALUE EntryIndex
    );

YY_STATUS
YypOutputDebug (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PYY_VALUE UndefinedToken
    );

VOID
YypOutputArray (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PSTR Name,
    PYY_VALUE Array,
    YY_VALUE Size
    );

VOID
YypOutputArrayBeginning (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PSTR Name
    );

VOID
YypOutputArrayEnd (
    PYYGEN_CONTEXT Context,
    FILE *File
    );

VOID
YypOutputValue (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_VALUE Value
    );

VOID
YypOutputString (
    FILE *File,
    PSTR String
    );

VOID
YypOutputGrammarStructure (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_VALUE TableSize,
    YY_VALUE UndefinedToken
    );

VOID
YypPrintOutputStates (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR YyAbbreviatedMonths[12] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

PSTR YyFullMonths[12] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

PSTR YyAbbreviatedWeekdays[7] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat"
};

PSTR YyFullWeekdays[7] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday"
};

PSTR YyAmPm[2] = {
    "AM",
    "PM"
};

//
// ------------------------------------------------------------------ Functions
//

YY_STATUS
YyOutputParserSource (
    PYYGEN_CONTEXT Context,
    FILE *File
    )

/*++

Routine Description:

    This routine prints a C source file containing the parser data to the given
    file.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

Return Value:

    YY Status.

--*/

{

    YY_VALUE TableSize;
    YY_VALUE UndefinedToken;
    YY_STATUS YyStatus;

    YypOutputFileHeader(Context, File);
    YypOutputRuleData(Context, File);
    YypOutputDefaultReductions(Context, File);
    YyStatus = YypOutputActions(Context, File, &TableSize);
    if (YyStatus != YyStatusSuccess) {
        goto OutputParserSouceEnd;
    }

    YyStatus = YypOutputDebug(Context, File, &UndefinedToken);
    if (YyStatus != YyStatusSuccess) {
        goto OutputParserSouceEnd;
    }

    YypOutputGrammarStructure(Context, File, TableSize, UndefinedToken);

OutputParserSouceEnd:
    return YyStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
YypOutputFileHeader (
    PYYGEN_CONTEXT Context,
    FILE *File
    )

/*++

Routine Description:

    This routine prints the C file header.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

Return Value:

    None.

--*/

{

    PSTR Current;
    ULONG Hour12;
    time_t Time;
    struct tm *TimeFields;

    Current = YY_DEFAULT_SOURCE_FILE_HEADER;
    Time = time(NULL);
    TimeFields = localtime(&Time);
    if (TimeFields == NULL) {
        return;
    }

    Hour12 = TimeFields->tm_hour;
    if (Hour12 == 0) {
        Hour12 = 12;

    } else if (Hour12 > 12) {
        Hour12 -= 12;
    }

    while (*Current != '\0') {
        if (*Current != '%') {
            fputc(*Current, File);
            Current += 1;
            continue;
        }

        Current += 1;
        switch (*Current) {
        case 'a':
            fprintf(File, "%s", YyAbbreviatedWeekdays[TimeFields->tm_wday]);
            break;

        case 'A':
            fprintf(File, "%s", YyFullWeekdays[TimeFields->tm_wday]);
            break;

        case 'b':
        case 'h':
            fprintf(File, "%s", YyAbbreviatedMonths[TimeFields->tm_mon]);
            break;

        case 'B':
            fprintf(File, "%s", YyFullMonths[TimeFields->tm_mon]);
            break;

        case 'd':
            fprintf(File, "%02d", TimeFields->tm_mday);
            break;

        case 'D':
            fprintf(File,
                    "%02d/%02d/%02d",
                    TimeFields->tm_mon,
                    TimeFields->tm_mday,
                    (TimeFields->tm_year + 1900) % 100);

            break;

        case 'e':
            fprintf(File, "%2d", TimeFields->tm_mday);
            break;

        case 'f':
            fprintf(File, "%s", Context->OutputFileName);
            break;

        case 'F':
            fprintf(File,
                    "%04d-%02d-%02d",
                    TimeFields->tm_year + 1900,
                    TimeFields->tm_mon,
                    TimeFields->tm_mday);

            break;

        case 'H':
            fprintf(File, "%02d", TimeFields->tm_hour);
            break;

        case 'I':
            fprintf(File, "%02d", Hour12);
            break;

        case 'm':
            fprintf(File, "%02d", TimeFields->tm_mon);
            break;

        case 'M':
            fprintf(File, "%02d", TimeFields->tm_min);
            break;

        case 'n':
            fputc('\n', File);
            break;

        case 'p':
        case 'P':
            fprintf(File, "%s", YyAmPm[TimeFields->tm_hour / 12]);
            break;

        case 'S':
            fprintf(File, "%02d", TimeFields->tm_sec);
            break;

        case 's':
            fprintf(File, "%lld", (long long)Time);
            break;

        case 't':
            fputc('\t', File);
            break;

        case 'T':
            fprintf(File,
                    "%02d:%02d:%02d",
                    TimeFields->tm_hour,
                    TimeFields->tm_min,
                    TimeFields->tm_sec);

            break;

        case 'u':
            fprintf(File,
                    "%d",
                    (TimeFields->tm_wday == 0) ? 7 : TimeFields->tm_wday);

            break;

        case 'w':
            fprintf(File, "%d", TimeFields->tm_wday);
            break;

        case 'y':
            fprintf(File, "%02d", (TimeFields->tm_year + 1900) % 100);
            break;

        case 'Y':
            fprintf(File, "%04d", TimeFields->tm_year + 1900);
            break;

        case '%':
            fputc('%', File);
            break;

        //
        // Unknown specifier, just swallow it.
        //

        default:
            fprintf(stderr, "Unknown source header specifier: %c.\n", *Current);
            break;
        }

        Current += 1;
    }

    return;
}

VOID
YypOutputRuleData (
    PYYGEN_CONTEXT Context,
    FILE *File
    )

/*++

Routine Description:

    This routine prints the rule left hand side symbols and rule lengths to
    the output source.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

Return Value:

    None.

--*/

{

    YY_RULE_INDEX Column;
    YY_RULE_INDEX RuleIndex;
    YY_VALUE Value;

    //
    // Spit out the rule left hand side symbols.
    //

    Column = 0;
    YypOutputArrayBeginning(Context, File, "LeftSide");
    for (RuleIndex = 2; RuleIndex < Context->RuleCount; RuleIndex += 1) {
        if (Column >= YY_VALUES_PER_LINES) {
            fprintf(File, "\n   ");
            Column = 0;
        }

        Column += 1;

        //
        // Print out the left hand sides so that the start rule is -1, and the
        // real rules start at 0.
        //

        Value = Context->Rules[RuleIndex].LeftSide - (Context->TokenCount + 1);
        YypOutputValue(Context, File, Value);
    }

    YypOutputArrayEnd(Context, File);

    //
    // Spit out the rule lengths.
    //

    Column = 0;
    YypOutputArrayBeginning(Context, File, "RuleLength");
    for (RuleIndex = 2; RuleIndex < Context->RuleCount; RuleIndex += 1) {
        if (Column >= YY_VALUES_PER_LINES) {
            fprintf(File, "\n   ");
            Column = 0;
        }

        Column += 1;
        Value = (Context->Rules[RuleIndex + 1].RightSide -
                 Context->Rules[RuleIndex].RightSide) - 1;

        YypOutputValue(Context, File, Value);
    }

    YypOutputArrayEnd(Context, File);
    return;
}

VOID
YypOutputDefaultReductions (
    PYYGEN_CONTEXT Context,
    FILE *File
    )

/*++

Routine Description:

    This routine prints the default reductions to the output source.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

Return Value:

    None.

--*/

{

    ULONG Column;
    YY_VALUE Index;
    YY_VALUE Value;

    YypOutputArrayBeginning(Context, File, "DefaultReductions");
    Column = 0;
    for (Index = 0; Index < Context->StateCount; Index += 1) {
        if (Column >= YY_VALUES_PER_LINES) {
            fprintf(File, "\n   ");
            Column = 0;
        }

        Column += 1;
        Value = Context->DefaultReductions[Index];
        if (Value != 0) {
            Value -= 2;
        }

        YypOutputValue(Context, File, Value);
    }

    YypOutputArrayEnd(Context, File);
    return;
}

YY_STATUS
YypOutputActions (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PYY_VALUE TableSize
    )

/*++

Routine Description:

    This routine prints the parser actions to the output source.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    TableSize - Supplies a pointer where the table size will be returned on
        success.

Return Value:

    YY Status.

--*/

{

    YYGEN_ACTION_CONTEXT ActionContext;
    YY_VALUE Index;
    YY_VALUE Size;
    YY_VALUE VectorCount;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;
    memset(&ActionContext, 0, sizeof(YYGEN_ACTION_CONTEXT));
    VectorCount = (Context->StateCount * YYGEN_VECTORS_PER_STATE) +
                  Context->NonTerminalCount;

    ActionContext.VectorCount = VectorCount;
    ActionContext.From = YypAllocate(VectorCount * sizeof(PYY_VALUE));
    if (ActionContext.From == NULL) {
        goto OutputActionsEnd;
    }

    ActionContext.To = YypAllocate(VectorCount * sizeof(PYY_VALUE));
    if (ActionContext.To == NULL) {
        goto OutputActionsEnd;
    }

    ActionContext.Tally = YypAllocate(VectorCount * sizeof(YY_VALUE));
    if (ActionContext.Tally == NULL) {
        goto OutputActionsEnd;
    }

    ActionContext.Width = YypAllocate(VectorCount * sizeof(YY_VALUE));
    if (ActionContext.Width == NULL) {
        goto OutputActionsEnd;
    }

    //
    // Create the actions based on tokens.
    //

    YyStatus = YypCreateTokenActions(Context, &ActionContext);
    if (YyStatus != YyStatusSuccess) {
        goto OutputActionsEnd;
    }

    //
    // Create the remaining gotos, and print the default (most used) ones.
    //

    YyStatus = YypOutputDefaultGotos(Context, &ActionContext, File);
    if (YyStatus != YyStatusSuccess) {
        goto OutputActionsEnd;
    }

    //
    // Sort the actions by width (and then by tally) so that finding duplicate
    // shift/reduce actions is quicker.
    //

    YyStatus = YypSortActions(Context, &ActionContext);
    if (YyStatus != YyStatusSuccess) {
        goto OutputActionsEnd;
    }

    //
    // Create one giant table of shift/reduce actions.
    //

    YyStatus = YypPackOutputTable(Context, &ActionContext);
    if (YyStatus != YyStatusSuccess) {
        goto OutputActionsEnd;
    }

    //
    // Output the shift index table, which indicates the index into the giant
    // table to start from for shifting from a given state.
    //

    YypOutputArray(Context,
                   File,
                   "ShiftIndex",
                   ActionContext.Base,
                   Context->StateCount);

    //
    // Output the reduce index table, which indicates the index into the giant
    // table to start from for reducing from a given state.
    //

    Index = Context->StateCount;
    Size = (Context->StateCount * 2) - Index;
    YypOutputArray(Context,
                   File,
                   "ReduceIndex",
                   ActionContext.Base + Index,
                   Size);

    //
    // Output the goto index table.
    //

    Index = Context->StateCount * YYGEN_VECTORS_PER_STATE;
    Size = (ActionContext.VectorCount - 1) - Index;
    YypOutputArray(Context,
                   File,
                   "GotoIndex",
                   ActionContext.Base + Index,
                   Size);

    //
    // It's time, output the giant table.
    //

    Size = ActionContext.High + 1;
    YypOutputArray(Context,
                   File,
                   "Table",
                   ActionContext.Table,
                   Size);

    //
    // Also output the check table, which indicates the "from" state.
    //

    YypOutputArray(Context,
                   File,
                   "Check",
                   ActionContext.Check,
                   Size);

OutputActionsEnd:
    *TableSize = ActionContext.High;
    if (ActionContext.From != NULL) {
        for (Index = 0; Index < ActionContext.VectorCount; Index += 1) {
            if (ActionContext.From[Index] != NULL) {
                YypFree(ActionContext.From[Index]);
            }
        }

        YypFree(ActionContext.From);
    }

    if (ActionContext.To != NULL) {
        for (Index = 0; Index < ActionContext.VectorCount; Index += 1) {
            if (ActionContext.To[Index] != NULL) {
                YypFree(ActionContext.To[Index]);
            }
        }

        YypFree(ActionContext.To);
    }

    if (ActionContext.Tally != NULL) {
        YypFree(ActionContext.Tally);
    }

    if (ActionContext.Width != NULL) {
        YypFree(ActionContext.Width);
    }

    if (ActionContext.Order != NULL) {
        YypFree(ActionContext.Order);
    }

    if (ActionContext.Base != NULL) {
        YypFree(ActionContext.Base);
    }

    if (ActionContext.Position != NULL) {
        YypFree(ActionContext.Position);
    }

    if (ActionContext.Table != NULL) {
        YypFree(ActionContext.Table);
    }

    if (ActionContext.Check != NULL) {
        YypFree(ActionContext.Check);
    }

    return YyStatus;
}

YY_STATUS
YypCreateTokenActions (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext
    )

/*++

Routine Description:

    This routine creates token based actions.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    ActionContext - Supplies a pointer to the working action state.

Return Value:

    YY Status.

--*/

{

    PYYGEN_ACTION Action;
    PYY_VALUE ActionRow;
    UINT AllocationSize;
    YY_VALUE ReduceCount;
    YY_VALUE ShiftCount;
    YY_STATE_INDEX StateCount;
    YY_STATE_INDEX StateIndex;
    PYY_STATE_INDEX States;
    YY_VALUE Symbol;
    YY_VALUE SymbolMax;
    YY_VALUE SymbolMin;
    PYY_VALUE Symbols;
    YY_VALUE TokenCount;
    YY_VALUE VectorIndex;
    YY_STATUS YyStatus;

    TokenCount = Context->TokenCount + YYGEN_TOKEN_OUTPUT_BASE;
    StateCount = Context->StateCount;
    YyStatus = YyStatusNoMemory;

    //
    // The action row contains two consecutive arrays of tokens, the first for
    // states to shift to by symbol and the second of rules to reduce by for
    // the symbol.
    //

    AllocationSize = TokenCount * YYGEN_VECTORS_PER_STATE * sizeof(YY_VALUE);
    ActionRow = YypAllocate(AllocationSize);
    if (ActionRow == NULL) {
        goto CreateTokenActionsEnd;
    }

    for (StateIndex = 0; StateIndex < StateCount; StateIndex += 1) {
        if (Context->Parser[StateIndex] == NULL) {
            continue;
        }

        //
        // Zero out the action table for this state.
        //

        for (VectorIndex = 0;
             VectorIndex < (YYGEN_VECTORS_PER_STATE * TokenCount);
             VectorIndex += 1) {

            ActionRow[VectorIndex] = 0;
        }

        ShiftCount = 0;
        ReduceCount = 0;
        Action = Context->Parser[StateIndex];
        while (Action != NULL) {
            if (Action->Suppression == YyNotSuppressed) {
                Symbol = Action->Symbol;

                //
                // Potentially rebase every token but EOF and Error, for
                // compatibility.
                //

                if (Symbol > 1) {
                    Symbol += YYGEN_TOKEN_OUTPUT_BASE;
                }

                //
                // For shifts, save the state number for that symbol.
                //

                if (Action->Code == YyActionShift) {
                    ShiftCount += 1;
                    ActionRow[Symbol] = Action->Number;

                //
                // For reductions (that aren't the sole reduction), save the
                // rule index by which it reduces.
                //

                } else if ((Action->Code == YyActionReduce) &&
                           (Action->Number !=
                            Context->DefaultReductions[StateIndex])) {

                    ReduceCount += 1;
                    ActionRow[Symbol + TokenCount] = Action->Number;
                }
            }

            Action = Action->Next;
        }

        //
        // Save the number of shifts and reductions in the tally.
        //

        ActionContext->Tally[StateIndex] = ShiftCount;
        ActionContext->Tally[StateIndex + StateCount] = ReduceCount;
        ActionContext->Width[StateIndex] = 0;
        ActionContext->Width[StateIndex + StateCount] = 0;

        //
        // Create the array of shifts in the from/to arrays for this state.
        //

        if (ShiftCount > 0) {
            SymbolMin = YY_VALUE_MAX;
            SymbolMax = 0;
            Symbols = YypAllocate(ShiftCount * sizeof(YY_VALUE));
            if (Symbols == NULL) {
                goto CreateTokenActionsEnd;
            }

            ActionContext->From[StateIndex] = Symbols;
            States = YypAllocate(ShiftCount * sizeof(YY_STATE_INDEX));
            if (States == NULL) {
                goto CreateTokenActionsEnd;
            }

            ActionContext->To[StateIndex] = States;
            for (Symbol = 0; Symbol < TokenCount; Symbol += 1) {
                if (ActionRow[Symbol] != 0) {
                    if (SymbolMin > Symbol) {
                        SymbolMin = Symbol;
                    }

                    if (SymbolMax < Symbol) {
                        SymbolMax = Symbol;
                    }

                    *Symbols = Symbol;
                    *States = ActionRow[Symbol];
                    Symbols += 1;
                    States += 1;
                }
            }

            //
            // Save the size of the range of symbols used by these shifts.
            //

            ActionContext->Width[StateIndex] = SymbolMax - SymbolMin + 1;
        }

        //
        // Create the array of reductions in the from/to arrays for this state.
        //

        if (ReduceCount > 0) {
            SymbolMin = YY_VALUE_MAX;
            SymbolMax = 0;
            Symbols = YypAllocate(ReduceCount * sizeof(YY_VALUE));
            if (Symbols == NULL) {
                goto CreateTokenActionsEnd;
            }

            ActionContext->From[StateCount + StateIndex] = Symbols;
            States = YypAllocate(ReduceCount * sizeof(YY_STATE_INDEX));
            if (States == NULL) {
                goto CreateTokenActionsEnd;
            }

            ActionContext->To[StateCount + StateIndex] = States;
            for (Symbol = 0; Symbol < TokenCount; Symbol += 1) {
                if (ActionRow[TokenCount + Symbol] != 0) {
                    if (SymbolMin > Symbol) {
                        SymbolMin = Symbol;
                    }

                    if (SymbolMax < Symbol) {
                        SymbolMax = Symbol;
                    }

                    *Symbols = Symbol;
                    *States = ActionRow[TokenCount + Symbol] - 2;
                    Symbols += 1;
                    States += 1;
                }
            }

            //
            // Save the range of symbols used by these reductions.
            //

            ActionContext->Width[StateCount + StateIndex] =
                                                     SymbolMax - SymbolMin + 1;
        }
    }

    if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
        printf("\nToken Actions:");
        YypPrintOutputStates(Context, ActionContext);
    }

    YyStatus = YyStatusSuccess;

CreateTokenActionsEnd:
    if (ActionRow != NULL) {
        YypFree(ActionRow);
    }

    return YyStatus;
}

YY_STATUS
YypOutputDefaultGotos (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext,
    FILE *File
    )

/*++

Routine Description:

    This routine prints the default goto actions to the source output, and
    computes the remaining gotos.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    ActionContext - Supplies a pointer to the action context.

    File - Supplies the file handle to output to.

Return Value:

    YY Status.

--*/

{

    ULONG Column;
    YY_STATE_INDEX State;
    PYY_STATE_INDEX StateCounts;
    YY_VALUE Symbol;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;
    StateCounts = YypAllocate(sizeof(YY_STATE_INDEX) * Context->StateCount);
    if (StateCounts == NULL) {
        goto OutputDefaultGotosEnd;
    }

    YypOutputArrayBeginning(Context, File, "DefaultGoto");
    Column = 0;
    for (Symbol = Context->StartSymbol + 1;
         Symbol < Context->SymbolCount;
         Symbol += 1) {

        if (Column >= YY_VALUES_PER_LINES) {
            fprintf(File, "\n   ");
            Column = 0;
        }

        Column += 1;
        State = YypFindDefaultGoto(Context, StateCounts, Symbol);
        YypOutputValue(Context, File, State);
        YyStatus = YypSaveColumn(Context, ActionContext, Symbol, State);
        if (YyStatus != YyStatusSuccess) {
            goto OutputDefaultGotosEnd;
        }
    }

    YypOutputArrayEnd(Context, File);
    YyStatus = YyStatusSuccess;

OutputDefaultGotosEnd:
    if (StateCounts != NULL) {
        YypFree(StateCounts);
    }

    return YyStatus;
}

YY_STATE_INDEX
YypFindDefaultGoto (
    PYYGEN_CONTEXT Context,
    PYY_STATE_INDEX StateCounts,
    YY_VALUE Symbol
    )

/*++

Routine Description:

    This routine returns the default goto state for the given symbol. This is
    the goto state that is referenced most often.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    StateCounts - Supplies a pointer to an array of counts, indexed by state.
        This is mainly handed down to avoid this routine having to allocate
        and free it each time.

    Symbol - Supplies the symbol to find the default goto for.

Return Value:

    Returns the default goto state.

--*/

{

    YY_STATE_INDEX DefaultState;
    YY_GOTO_INDEX End;
    YY_GOTO_INDEX GotoIndex;
    YY_STATE_INDEX MaxCount;
    YY_GOTO_INDEX Start;
    YY_STATE_INDEX StateIndex;

    assert(Symbol >= Context->TokenCount);

    Start = Context->GotoMap[Symbol - Context->TokenCount];
    End = Context->GotoMap[Symbol + 1 - Context->TokenCount];
    if (Start == End) {
        return 0;
    }

    for (StateIndex = 0; StateIndex < Context->StateCount; StateIndex += 1) {
        StateCounts[StateIndex] = 0;
    }

    //
    // Count the number of times a state is referenced in the goto map.
    //

    for (GotoIndex = Start; GotoIndex < End; GotoIndex += 1) {
        StateCounts[Context->ToState[GotoIndex]] += 1;
    }

    //
    // Figure out which one was referenced the most.
    //

    MaxCount = 0;
    DefaultState = 0;
    for (StateIndex = 0; StateIndex < Context->StateCount; StateIndex += 1) {
        if (StateCounts[StateIndex] > MaxCount) {
            MaxCount = StateCounts[StateIndex];
            DefaultState = StateIndex;
        }
    }

    return DefaultState;
}

YY_STATUS
YypSaveColumn (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext,
    YY_VALUE Symbol,
    YY_STATE_INDEX DefaultGoto
    )

/*++

Routine Description:

    This routine sets the from and to arrays for non-terminals.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    ActionContext - Supplies a pointer to the action context.

    Symbol - Supplies the non-terminal.

    DefaultGoto - Supplies the default state to go to. This one doesn't need
        to be in the array.

Return Value:

    YY Status.

--*/

{

    YY_STATE_INDEX Count;
    YY_GOTO_INDEX End;
    PYY_VALUE From;
    PYY_VALUE FromStart;
    YY_GOTO_INDEX GotoIndex;
    YY_GOTO_INDEX Start;
    PYY_VALUE To;
    YY_VALUE VectorIndex;

    assert(Symbol >= Context->TokenCount);

    //
    // Count the number of gotos excluding the most-used one.
    //

    Start = Context->GotoMap[Symbol - Context->TokenCount];
    End = Context->GotoMap[Symbol + 1 - Context->TokenCount];
    Count = 0;
    for (GotoIndex = Start; GotoIndex < End; GotoIndex += 1) {
        if (Context->ToState[GotoIndex] != DefaultGoto) {
            Count += 1;
        }
    }

    if (Count == 0) {
        return YyStatusSuccess;
    }

    VectorIndex = (Symbol - (Context->StartSymbol + 1)) +
                  (Context->StateCount * YYGEN_VECTORS_PER_STATE);

    assert((ActionContext->From[VectorIndex] == NULL) &&
           (ActionContext->To[VectorIndex] == NULL));

    From = YypAllocate(Count * sizeof(YY_VALUE));
    if (From == NULL) {
        return YyStatusNoMemory;
    }

    ActionContext->From[VectorIndex] = From;
    FromStart = From;
    To = YypAllocate(Count * sizeof(YY_VALUE));
    if (To == NULL) {
        return YyStatusNoMemory;
    }

    //
    // Save the non-default gotos in the from/to arrays.
    //

    ActionContext->To[VectorIndex] = To;
    for (GotoIndex = Start; GotoIndex < End; GotoIndex += 1) {
        if (Context->ToState[GotoIndex] != DefaultGoto) {
            *From = Context->FromState[GotoIndex];
            *To = Context->ToState[GotoIndex];
            From += 1;
            To += 1;
        }
    }

    ActionContext->Tally[VectorIndex] = Count;
    From -= 1;

    assert(From >= FromStart);

    ActionContext->Width[VectorIndex] = *From - *FromStart + 1;
    return YyStatusSuccess;
}

YY_STATUS
YypSortActions (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext
    )

/*++

Routine Description:

    This routine reorganizes the action context arrays, sorting by tally count.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    ActionContext - Supplies a pointer to the action context.

Return Value:

    YY Status.

--*/

{

    YY_VALUE EntryCount;
    YY_VALUE MoveIndex;
    PYY_VALUE Order;
    YY_VALUE SearchIndex;
    YY_VALUE Tally;
    YY_VALUE VectorIndex;
    YY_VALUE Width;
    YY_STATUS YyStatus;

    EntryCount = 0;
    YyStatus = YyStatusNoMemory;
    Order = YypAllocate(ActionContext->VectorCount * sizeof(YY_VALUE));
    if (Order == NULL) {
        goto SortActionsEnd;
    }

    ActionContext->Order = Order;
    for (VectorIndex = 0;
         VectorIndex < ActionContext->VectorCount;
         VectorIndex += 1) {

        if (ActionContext->Tally[VectorIndex] == 0) {
            continue;
        }

        //
        // Find the right spot to insert this entry, sorting primarily by the
        // width, and then by tally for tiebreakers.
        //

        Tally = ActionContext->Tally[VectorIndex];
        Width = ActionContext->Width[VectorIndex];
        SearchIndex = EntryCount - 1;
        while ((SearchIndex >= 0) &&
               (ActionContext->Width[Order[SearchIndex]] < Width)) {

            SearchIndex -= 1;
        }

        while ((SearchIndex >= 0) &&
               (ActionContext->Width[Order[SearchIndex]] == Width) &&
               (ActionContext->Tally[Order[SearchIndex]] < Tally)) {

            SearchIndex -= 1;
        }

        //
        // Slide the other entries over.
        //

        for (MoveIndex = EntryCount - 1;
             MoveIndex > SearchIndex;
             MoveIndex -= 1) {

            Order[MoveIndex + 1] = Order[MoveIndex];
        }

        Order[SearchIndex + 1] = VectorIndex;
        EntryCount += 1;
    }

    if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
        printf("\nOrder: (%d vectors, %d entries)\n",
               ActionContext->VectorCount,
               EntryCount);

        for (VectorIndex = 0; VectorIndex < EntryCount; VectorIndex += 1) {
            printf("    %d: %d\n", VectorIndex, Order[VectorIndex]);
        }
    }

    YyStatus = YyStatusSuccess;

SortActionsEnd:
    ActionContext->EntryCount = EntryCount;
    return YyStatus;
}

YY_STATUS
YypPackOutputTable (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext
    )

/*++

Routine Description:

    This routine creates the final output table, in a minimal representation.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    ActionContext - Supplies a pointer to the action context.

Return Value:

    YY Status.

--*/

{

    YY_VALUE EntryIndex;
    YY_STATE_INDEX State;
    YY_VALUE TableIndex;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;
    ActionContext->Base =
                    YypAllocate(ActionContext->VectorCount * sizeof(YY_VALUE));

    if (ActionContext->Base == NULL) {
        goto PackOutputTableEnd;
    }

    ActionContext->Position =
                     YypAllocate(ActionContext->EntryCount * sizeof(YY_VALUE));

    if (ActionContext->Position == NULL) {
        goto PackOutputTableEnd;
    }

    for (EntryIndex = 0;
         EntryIndex < ActionContext->EntryCount;
         EntryIndex += 1) {

        State = YypFindMatchingVector(Context, ActionContext, EntryIndex);
        if (State < 0) {
            YyStatus = YypPackVector(ActionContext, EntryIndex, &TableIndex);
            if (YyStatus != YyStatusSuccess) {
                goto PackOutputTableEnd;
            }

        } else {
            TableIndex = ActionContext->Base[State];
        }

        ActionContext->Position[EntryIndex] = TableIndex;
        ActionContext->Base[ActionContext->Order[EntryIndex]] = TableIndex;
    }

    YyStatus = YyStatusSuccess;

PackOutputTableEnd:
    return YyStatus;
}

YY_STATUS
YypPackVector (
    PYYGEN_ACTION_CONTEXT ActionContext,
    YY_VALUE EntryIndex,
    PYY_VALUE TableIndex
    )

/*++

Routine Description:

    This routine adds entries in the final output table (and check table)
    corresponding to the given state.

Arguments:

    ActionContext - Supplies a pointer to the action context.

    EntryIndex - Supplies the state to add table entries for.

    TableIndex - Supplies a pointer where the corresponding index in the table
        will be returned.

Return Value:

    YY Status.

--*/

{

    YY_VALUE ActionIndex;
    YY_VALUE BaseIndex;
    YY_VALUE CopyIndex;
    PYY_VALUE From;
    YY_VALUE MaxIndex;
    PYY_VALUE NewBuffer;
    YY_VALUE NewCapacity;
    YY_VALUE OrderIndex;
    BOOL RangeFree;
    YY_VALUE Tally;
    PYY_VALUE To;

    OrderIndex = ActionContext->Order[EntryIndex];
    Tally = ActionContext->Tally[OrderIndex];

    assert(Tally != 0);

    From = ActionContext->From[OrderIndex];
    To = ActionContext->To[OrderIndex];

    //
    // Figure out the highest index, minus the lowest free index in the table.
    //

    BaseIndex = ActionContext->Low - From[0];
    for (ActionIndex = 1; ActionIndex < Tally; ActionIndex += 1) {
        if (BaseIndex < ActionContext->Low - From[ActionIndex]) {
            BaseIndex = ActionContext->Low - From[ActionIndex];
        }
    }

    //
    // Find an appropriate index in the table (with a free range that's big
    // enough), and make sure the table is sized big enough for this range.
    //

    while (TRUE) {

        //
        // Reserve index zero.
        //

        if (BaseIndex == 0) {
            BaseIndex += 1;
            continue;
        }

        RangeFree = TRUE;
        for (ActionIndex = 0; ActionIndex < Tally; ActionIndex += 1) {
            MaxIndex = BaseIndex + From[ActionIndex];

            //
            // Reallocate the table if needed.
            //

            if (MaxIndex + 1 >= ActionContext->TableCapacity) {
                if (MaxIndex + 1 >= YY_MAX_TABLE) {
                    return YyStatusTooManyItems;
                }

                NewCapacity = ActionContext->TableCapacity;
                while (MaxIndex + 1 >= NewCapacity) {
                    NewCapacity += 256;
                }

                NewBuffer = YypReallocate(ActionContext->Table,
                                          NewCapacity * sizeof(YY_VALUE));

                if (NewBuffer == NULL) {
                    return YyStatusNoMemory;
                }

                ActionContext->Table = NewBuffer;
                NewBuffer = YypReallocate(ActionContext->Check,
                                          NewCapacity * sizeof(YY_VALUE));

                if (NewBuffer == NULL) {
                    return YyStatusNoMemory;
                }

                ActionContext->Check = NewBuffer;
                for (CopyIndex = ActionContext->TableCapacity;
                     CopyIndex < NewCapacity;
                     CopyIndex += 1) {

                    ActionContext->Table[CopyIndex] = 0;
                    ActionContext->Check[CopyIndex] = -1;
                }

                ActionContext->TableCapacity = NewCapacity;
            }

            if (ActionContext->Check[MaxIndex] != -1) {
                RangeFree = FALSE;
                break;
            }
        }

        //
        // Also check the position array to see if this base is in use
        // already.
        //

        if (RangeFree != FALSE) {
            for (ActionIndex = 0; ActionIndex < EntryIndex; ActionIndex += 1) {
                if (ActionContext->Position[ActionIndex] == BaseIndex) {
                    RangeFree = FALSE;
                    break;
                }
            }
        }

        if (RangeFree != FALSE) {
            break;
        }

        BaseIndex += 1;
    }

    //
    // A free range was found. Copy the tos and froms into the table and check.
    //

    for (ActionIndex = 0; ActionIndex < Tally; ActionIndex += 1) {
        CopyIndex = BaseIndex + From[ActionIndex];
        ActionContext->Table[CopyIndex] = To[ActionIndex];
        ActionContext->Check[CopyIndex] = From[ActionIndex];
        if (ActionContext->High < CopyIndex) {
            ActionContext->High = CopyIndex;
        }
    }

    //
    // Also update the lowest free index. The table is always one bigger than
    // the max index used, so don't worry about slipping off the end.
    //

    while (ActionContext->Check[ActionContext->Low] != -1) {
        ActionContext->Low += 1;
    }

    *TableIndex = BaseIndex;
    return YyStatusSuccess;
}

YY_STATE_INDEX
YypFindMatchingVector (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext,
    YY_VALUE EntryIndex
    )

/*++

Routine Description:

    This routine attempts to match the current set of froms and tos to a
    previous set.

Arguments:

    Context - Supplies a pointer to the application context.

    ActionContext - Supplies a pointer to the action context.

    EntryIndex - Supplies the entry index to match on.

Return Value:

    Returns the index of a state (in the from/to arrays) where the given set
    of shift or reduce actions occurred previously.

    -1 if this is a new set of actions.

--*/

{

    YY_VALUE CheckIndex;
    BOOL Match;
    YY_VALUE OrderIndex;
    YY_VALUE SearchOrder;
    YY_VALUE Tally;
    YY_VALUE Width;

    OrderIndex = ActionContext->Order[EntryIndex];
    if (OrderIndex > (Context->StateCount * 2)) {
        return -1;
    }

    Tally = ActionContext->Tally[OrderIndex];
    Width = ActionContext->Width[OrderIndex];
    EntryIndex -= 1;
    while (EntryIndex >= 0) {
        SearchOrder = ActionContext->Order[EntryIndex];

        //
        // Quick exit check if the widths or tallies don't match. Since the
        // order had them sorted by width and then tally, as soon as they don't
        // match, none will.
        //

        if ((ActionContext->Width[SearchOrder] != Width) ||
            (ActionContext->Tally[SearchOrder] != Tally)) {

            return -1;
        }

        Match = TRUE;
        for (CheckIndex = 0; CheckIndex < Tally; CheckIndex += 1) {
            if ((ActionContext->To[SearchOrder][CheckIndex] !=
                 ActionContext->To[OrderIndex][CheckIndex]) ||
                (ActionContext->From[SearchOrder][CheckIndex] !=
                 ActionContext->From[OrderIndex][CheckIndex])) {

                Match = FALSE;
                break;
            }
        }

        if (Match != FALSE) {
            return SearchOrder;
        }

        EntryIndex -= 1;
    }

    return -1;
}

YY_STATUS
YypOutputDebug (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PYY_VALUE UndefinedToken
    )

/*++

Routine Description:

    This routine outputs debugging information to the parser source file.

Arguments:

    Context - Supplies a pointer to the application context.

    File - Supplies a pointer to the file to output to.

    UndefinedToken - Supplies a pointer where the undefined token index into
        the names will be returned.

Return Value:

    YY Status.

--*/

{

    YY_VALUE Index;
    PYY_VALUE Items;
    PSTR Name;
    PYYGEN_RULE Rule;
    PSTR *SymbolNames;
    YY_VALUE TokenCount;

    TokenCount = Context->TokenCount + YYGEN_TOKEN_OUTPUT_BASE;
    *UndefinedToken = TokenCount;
    SymbolNames = YypAllocate((TokenCount + 1) * sizeof(PSTR));
    if (SymbolNames == NULL) {
        return YyStatusNoMemory;
    }

    //
    // EOF doesn't get rebased, but everything else does.
    //

    SymbolNames[0] = Context->Elements[0].Name;
    for (Index = 1; Index < Context->TokenCount; Index += 1) {
        SymbolNames[Index + YYGEN_TOKEN_OUTPUT_BASE] =
            Context->Elements[Index].Name;
    }

    SymbolNames[Index + YYGEN_TOKEN_OUTPUT_BASE] = "illegal-symbol";
    fprintf(File, "const char *%s%s[] = {", Context->VariablePrefix, "Names");
    for (Index = 0; Index < TokenCount + 1; Index += 1) {
        Name = SymbolNames[Index];
        if (Name != NULL) {
            fprintf(File, "\n    \"");
            YypOutputString(File, Name);
            fprintf(File, "\",");

        } else {
            fprintf(File, "\n    0,");
        }
    }

    YypOutputArrayEnd(Context, File);
    fprintf(File, "const char *%s%s[] = {", Context->VariablePrefix, "Rules");
    for (Index = 2; Index < Context->RuleCount; Index += 1) {
        Rule = &(Context->Rules[Index]);
        fprintf(File, "\n    \"");
        YypOutputString(File, Context->Elements[Rule->LeftSide].Name);
        fprintf(File, " :");
        Items = Context->Items + Rule->RightSide;
        while (*Items > 0) {
            fprintf(File, " ");
            YypOutputString(File, Context->Elements[*Items].Name);
            Items += 1;
        }

        fprintf(File, "\",");
    }

    YypOutputArrayEnd(Context, File);
    YypFree(SymbolNames);
    return YyStatusSuccess;
}

VOID
YypOutputArray (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PSTR Name,
    PYY_VALUE Array,
    YY_VALUE Size
    )

/*++

Routine Description:

    This routine prints an array to the output source.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    Name - Supplies the name of the array.

    Array - Supplies the array values.

    Size - Supplies the number of elements in the array.

Return Value:

    None.

--*/

{

    ULONG Column;
    YY_VALUE Index;

    YypOutputArrayBeginning(Context, File, Name);
    Column = 0;
    for (Index = 0; Index < Size; Index += 1) {
        if (Column >= YY_VALUES_PER_LINES) {
            fprintf(File, "\n   ");
            Column = 0;
        }

        Column += 1;
        YypOutputValue(Context, File, Array[Index]);
    }

    YypOutputArrayEnd(Context, File);
    return;
}

VOID
YypOutputArrayBeginning (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PSTR Name
    )

/*++

Routine Description:

    This routine prints an array beginning source line.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    Name - Supplies the name of the array.

Return Value:

    None.

--*/

{

    fprintf(File,
            "const YY_VALUE %s%s[] = {\n   ",
            Context->VariablePrefix,
            Name);

    return;
}

VOID
YypOutputArrayEnd (
    PYYGEN_CONTEXT Context,
    FILE *File
    )

/*++

Routine Description:

    This routine prints an array termination source.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    Name - Supplies the name of the array.

Return Value:

    None.

--*/

{

    fprintf(File, "\n};\n\n");
    return;
}

VOID
YypOutputValue (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_VALUE Value
    )

/*++

Routine Description:

    This routine prints a single integer to the output source file.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    Value - Supplies the value to print.

Return Value:

    None.

--*/

{

    fprintf(File, " %d,", Value);
    return;
}

VOID
YypOutputString (
    FILE *File,
    PSTR String
    )

/*++

Routine Description:

    This routine prints a C source string to the given output. It does not
    print the surrounding quotation marks.

Arguments:

    File - Supplies the file handle to output to.

    String - Supplies the string to print in C source form.

Return Value:

    None.

--*/

{

    while (*String != '\0') {
        switch (*String) {
        case '\n':
            fprintf(File, "\\n");
            break;

        case '\v':
            fprintf(File, "\\v");
            break;

        case '\t':
            fprintf(File, "\\t");
            break;

        case '\r':
            fprintf(File, "\\r");
            break;

        case '\f':
            fprintf(File, "\\f");
            break;

        case '\a':
            fprintf(File, "\\a");
            break;

        case '\b':
            fprintf(File, "\\b");
            break;

        case '\\':
            fprintf(File, "\\\\");
            break;

        default:
            if (isprint(*String)) {
                fputc(*String, File);
            }

            break;
        }

        String += 1;
    }

    return;
}

VOID
YypOutputGrammarStructure (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_VALUE TableSize,
    YY_VALUE UndefinedToken
    )

/*++

Routine Description:

    This routine prints the final structure that ties the grammar together.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    TableSize - Supplies the number of elements in the table.

    UndefinedToken - Supplies the index into the names array where the
        description for an undefined token lies.

Return Value:

    None.

--*/

{

    YY_VALUE Start;

    fprintf(File, "YY_GRAMMAR %sGrammar = {\n", Context->VariablePrefix);
    fprintf(File, "    %sLeftSide,\n", Context->VariablePrefix);
    fprintf(File, "    %sRuleLength,\n", Context->VariablePrefix);
    fprintf(File, "    %sDefaultReductions,\n", Context->VariablePrefix);
    fprintf(File, "    %sShiftIndex,\n", Context->VariablePrefix);
    fprintf(File, "    %sReduceIndex,\n", Context->VariablePrefix);
    fprintf(File, "    %sGotoIndex,\n", Context->VariablePrefix);
    fprintf(File, "    %sTable,\n", Context->VariablePrefix);
    fprintf(File, "    %sCheck,\n", Context->VariablePrefix);
    fprintf(File, "    %sDefaultGoto,\n", Context->VariablePrefix);
    fprintf(File, "    %d,\n", TableSize);
    fprintf(File, "    %sNames,\n", Context->VariablePrefix);
    fprintf(File, "    %sRules,\n", Context->VariablePrefix);
    fprintf(File, "    %d,\n", Context->FinalState);
    Start = Context->Items[Context->Rules[2].RightSide] -
            (Context->StartSymbol + 1);

    fprintf(File, "    %d,\n", Start);
    fprintf(File,
            "    %d,\n",
            Context->TokenCount + YYGEN_TOKEN_OUTPUT_BASE - 1);

    fprintf(File, "    %d,\n", UndefinedToken);
    fprintf(File, "};\n\n");
    return;
}

VOID
YypPrintOutputStates (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION_CONTEXT ActionContext
    )

/*++

Routine Description:

    This routine prints the actions as defined by the output generator.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    ActionContext - Supplies a pointer to the initialized action context.

Return Value:

    None.

--*/

{

    YY_VALUE ActionIndex;
    YY_STATE_INDEX StateIndex;

    printf("\nShift Output Actions:\n");
    for (StateIndex = 0; StateIndex < Context->StateCount; StateIndex += 1) {
        printf("    %d: %d (width %d)\n",
               StateIndex,
               ActionContext->Tally[StateIndex],
               ActionContext->Width[StateIndex]);

        for (ActionIndex = 0;
             ActionIndex < ActionContext->Tally[StateIndex];
             ActionIndex += 1) {

            printf("        %d -> %d\n",
                   ActionContext->From[StateIndex][ActionIndex],
                   ActionContext->To[StateIndex][ActionIndex]);
        }
    }

    printf("\nReduce Output Actions:\n");
    for (StateIndex = Context->StateCount;
         StateIndex < (Context->StateCount * 2);
         StateIndex += 1) {

        printf("    %d: %d (width %d)\n",
               StateIndex - Context->StateCount,
               ActionContext->Tally[StateIndex],
               ActionContext->Width[StateIndex]);

        for (ActionIndex = 0;
             ActionIndex < ActionContext->Tally[StateIndex];
             ActionIndex += 1) {

            printf("        %d -> %d\n",
                   ActionContext->From[StateIndex][ActionIndex],
                   ActionContext->To[StateIndex][ActionIndex]);
        }
    }

    return;
}

