/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    verbose.c

Abstract:

    This module implements verbose and debug output for the YY parser generator.

Author:

    Evan Green 24-Apr-2016

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "yygenp.h"
#include <stdio.h>
#include <string.h>

//
// --------------------------------------------------------------------- Macros
//

#define YYGEN_PLURALIZE(_Value) (((_Value) == 1) ? "" : "s")

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
YypGraphState (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_STATE_INDEX StateIndex
    );

VOID
YypGraphLookaheads (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_RULE_INDEX RuleIndex,
    PYY_VALUE LookaheadIndex
    );

VOID
YypPrintGrammar (
    PYYGEN_CONTEXT Context,
    FILE *File
    );

VOID
YypPrintState (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PYY_RULE_INDEX NullRules,
    YY_STATE_INDEX StateIndex
    );

VOID
YypPrintUnusedRules (
    PYYGEN_CONTEXT Context,
    FILE *File
    );

VOID
YypPrintConflicts (
    PYYGEN_CONTEXT Context,
    FILE *File
    );

VOID
YypPrintConflictsForState (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_STATE_INDEX StateIndex
    );

VOID
YypPrintStateItemSets (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_STATE_INDEX StateIndex
    );

VOID
YypPrintStateNulls (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PYY_RULE_INDEX NullRules,
    YY_STATE_INDEX StateIndex
    );

VOID
YypPrintActions (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_STATE_INDEX StateIndex
    );

VOID
YypPrintShifts (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PYYGEN_ACTION Action
    );

VOID
YypPrintReductions (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PYYGEN_ACTION Action,
    YY_RULE_INDEX DefaultReduction
    );

VOID
YypPrintGotos (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_STATE_INDEX StateIndex
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
YyPrintGraph (
    PYYGEN_CONTEXT Context,
    FILE *File
    )

/*++

Routine Description:

    This routine prints the state graph for the given parsed grammar.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

Return Value:

    None.

--*/

{

    YY_VALUE ShiftIndex;
    PYYGEN_SHIFTS Shifts;
    YY_STATE_INDEX ShiftState;
    YY_STATE_INDEX StateIndex;
    YY_VALUE Symbol;

    for (StateIndex = 0; StateIndex < Context->StateCount; StateIndex += 1) {
        YypEstablishClosure(Context, Context->StateTable[StateIndex]);
        YypGraphState(Context, File, StateIndex);
    }

    fprintf(File, "\n\n");
    for (StateIndex = 0; StateIndex < Context->StateCount; StateIndex += 1) {
        Shifts = Context->ShiftTable[StateIndex];
        if (Shifts != NULL) {
            for (ShiftIndex = 0; ShiftIndex < Shifts->Count; ShiftIndex += 1) {
                ShiftState = Shifts->States[ShiftIndex];
                Symbol = Context->AccessingSymbol[ShiftState];
                fprintf(File,
                        "\tq%d -> q%d [label=\"%s\"];\n",
                        StateIndex,
                        ShiftState,
                        Context->Elements[Symbol].Name);
            }
        }
    }

    fprintf(File, "}\n");
    return;
}

VOID
YyPrintParserState (
    PYYGEN_CONTEXT Context,
    FILE *File
    )

/*++

Routine Description:

    This routine prints a human readable description of the parser states.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

Return Value:

    None.

--*/

{

    PYY_RULE_INDEX NullRules;
    YY_STATE_INDEX StateIndex;

    NullRules = YypAllocate(Context->RuleCount * sizeof(YY_RULE_INDEX));
    if (NullRules == NULL) {
        goto DumpParserStateEnd;
    }

    YypPrintGrammar(Context, File);
    fprintf(File, "\n\n");
    for (StateIndex = 0; StateIndex < Context->StateCount; StateIndex += 1) {
        YypPrintState(Context, File, NullRules, StateIndex);
    }

    if (Context->UnusedRules != 0) {
        YypPrintUnusedRules(Context, File);
    }

    if ((Context->ShiftReduceConflictCount != 0) ||
        (Context->ReduceReduceConflictCount != 0)) {

        YypPrintConflicts(Context, File);
    }

    fprintf(File,
            "\n\n%d terminals, %d nonterminals\n",
            Context->TokenCount,
            Context->NonTerminalCount);

    fprintf(File,
            "%d grammar rules, %d states\n",
            Context->RuleCount - 2,
            Context->StateCount);

DumpParserStateEnd:
    if (NullRules != NULL) {
        YypFree(NullRules);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
YypGraphState (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_STATE_INDEX StateIndex
    )

/*++

Routine Description:

    This routine prints the graph for a particular state.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    StateIndex - Supplies the state to graph. The context's item set array
        should be properly set up for this state.

Return Value:

    None.

--*/

{

    PYY_VALUE Items;
    PYY_ITEM_INDEX ItemSet;
    PYY_VALUE ItemsStart;
    YY_VALUE LookaheadIndex;
    YY_RULE_INDEX RuleIndex;
    YY_VALUE Symbol;

    LookaheadIndex = Context->Lookaheads[StateIndex];
    fprintf(File, "\n\tq%d [label=\"%d:\\l", StateIndex, StateIndex);
    ItemSet = Context->ItemSet;
    while (ItemSet < Context->ItemSetEnd) {
        Items = Context->Items + *ItemSet;
        ItemsStart = Items;
        while (*Items >= 0) {
            Items += 1;
        }

        RuleIndex = -(*Items);
        Symbol = Context->Rules[RuleIndex].LeftSide;
        fprintf(File, "  %s -> ", Context->Elements[Symbol].Name);
        Items = Context->Items + Context->Rules[RuleIndex].RightSide;
        while (Items < ItemsStart) {
            fprintf(File, "%s ", Context->Elements[*Items].Name);
            Items += 1;
        }

        fputc('.', File);
        while (*Items >= 0) {
            fprintf(File, " %s", Context->Elements[*Items].Name);
            Items += 1;
        }

        if (*ItemsStart < 0) {
            YypGraphLookaheads(Context, File, RuleIndex, &LookaheadIndex);
        }

        fprintf(File, "\\l");
        ItemSet += 1;
    }

    fprintf(File, "\"]'");
    return;
}

VOID
YypGraphLookaheads (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_RULE_INDEX RuleIndex,
    PYY_VALUE LookaheadIndex
    )

/*++

Routine Description:

    This routine prints the lookahead tokens for a particular rule.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    RuleIndex - Supplies the rule whose lookaheads should be graphed.

    LookaheadIndex - Supplies a pointer to the index into the lookahead array.
        This may be updated.

Return Value:

    None.

--*/

{

    PULONG Row;
    YY_VALUE Token;
    ULONG TokenSetSize;

    TokenSetSize = YYGEN_BITMAP_WORD_COUNT(Context->TokenCount);
    if (RuleIndex == Context->LookaheadRule[*LookaheadIndex]) {
        Row = Context->LookaheadSets + (*LookaheadIndex * TokenSetSize);
        fprintf(File, " { ");
        for (Token = Context->TokenCount - 1; Token >= 0; Token -= 1) {
            if (YYGEN_BITMAP_IS_SET(Row, Token)) {
                fprintf(File, "%s ", Context->Elements[Token].Name);
            }
        }

        fprintf(File, " } ");
        *LookaheadIndex += 1;
    }

    return;
}

VOID
YypPrintGrammar (
    PYYGEN_CONTEXT Context,
    FILE *File
    )

/*++

Routine Description:

    This routine prints the set of numbered rules.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

Return Value:

    None.

--*/

{

    PYY_VALUE Item;
    YY_VALUE LeftSide;
    PSTR Name;
    PYYGEN_RULE Rule;
    YY_RULE_INDEX RuleIndex;
    INT Spacing;

    Spacing = 0;
    LeftSide = -1;
    for (RuleIndex = 2; RuleIndex < Context->RuleCount; RuleIndex += 1) {
        Rule = &(Context->Rules[RuleIndex]);
        if (Rule->LeftSide != LeftSide) {
            if (RuleIndex != 2) {
                fprintf(File, "\n");
            }

            Name = Context->Elements[Rule->LeftSide].Name;
            fprintf(File, "%4d  %s :", RuleIndex - 2, Name);
            Spacing = strlen(Name) + 1;

        } else {
            fprintf(File, "%4d  %*s|", RuleIndex - 2, Spacing, "");
        }

        Item = Context->Items + Rule->RightSide;
        while (*Item >= 0) {
            fprintf(File, " %s", Context->Elements[*Item].Name);
            Item += 1;
        }

        fprintf(File, "\n");
        LeftSide = Rule->LeftSide;
    }

    return;
}

VOID
YypPrintState (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PYY_RULE_INDEX NullRules,
    YY_STATE_INDEX StateIndex
    )

/*++

Routine Description:

    This routine prints a human readable description of a particular parser
    state.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    NullRules - Supplies a pointer to a null rules array.

    StateIndex - Supplies the index of the state to print.

Return Value:

    None.

--*/

{

    if (StateIndex != 0) {
        fprintf(File, "\n\n");
    }

    if ((Context->ShiftReduceConflicts[StateIndex] != 0) ||
        (Context->ReduceReduceConflicts[StateIndex] != 0)) {

        YypPrintConflictsForState(Context, File, StateIndex);
    }

    fprintf(File, "state %d\n", StateIndex);
    YypPrintStateItemSets(Context, File, StateIndex);
    YypPrintStateNulls(Context, File, NullRules, StateIndex);
    YypPrintActions(Context, File, StateIndex);
    return;
}

VOID
YypPrintUnusedRules (
    PYYGEN_CONTEXT Context,
    FILE *File
    )

/*++

Routine Description:

    This routine prints a human readable description of the unreduced parser
    rules.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

Return Value:

    None.

--*/

{

    PYY_VALUE Items;
    PYYGEN_RULE Rule;
    YY_RULE_INDEX RuleIndex;

    fprintf(File, "\n\nRules never reduced:\n");
    for (RuleIndex = 3; RuleIndex < Context->RuleCount; RuleIndex += 1) {
        Rule = &(Context->Rules[RuleIndex]);
        if (Rule->Used == FALSE) {
            fprintf(File, "\t%s : ", Context->Elements[Rule->LeftSide].Name);
            Items = Context->Items + Rule->RightSide;
            while (*Items >= 0) {
                fprintf(File, " %s", Context->Elements[*Items].Name);
                Items += 1;
            }

            fprintf(File, "  (%d)\n", RuleIndex - 1);
        }
    }

    return;
}

VOID
YypPrintConflicts (
    PYYGEN_CONTEXT Context,
    FILE *File
    )

/*++

Routine Description:

    This routine prints a human readable description of the parser conflicts
    that could not be resolved via precedence or associativity rules.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

Return Value:

    None.

--*/

{

    ULONG ReduceConflicts;
    ULONG ShiftConflicts;
    YY_STATE_INDEX StateIndex;

    fprintf(File, "\n\n");
    for (StateIndex = 0; StateIndex < Context->StateCount; StateIndex += 1) {
        ShiftConflicts = Context->ShiftReduceConflicts[StateIndex];
        ReduceConflicts = Context->ReduceReduceConflicts[StateIndex];
        if ((ShiftConflicts != 0) || (ReduceConflicts != 0)) {
            fprintf(File, "State %d contains ", StateIndex);
            if (ShiftConflicts != 0) {
                fprintf(File,
                        "%d shift/reduce conflict%s",
                        ShiftConflicts,
                        YYGEN_PLURALIZE(ShiftConflicts));

                if (ReduceConflicts != 0) {
                    fprintf(File, ", ");
                }
            }

            if (ReduceConflicts != 0) {
                fprintf(File,
                        "%d reduce/reduce conflict%s",
                        ReduceConflicts,
                        YYGEN_PLURALIZE(ReduceConflicts));
            }

            fprintf(File, ".\n");
        }
    }

    return;
}

VOID
YypPrintConflictsForState (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_STATE_INDEX StateIndex
    )

/*++

Routine Description:

    This routine prints a human readable description of the parser conflicts
    for a particular state.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    StateIndex - Supplies the state index to print conflicts for.

Return Value:

    None.

--*/

{

    PYYGEN_ACTION Action;
    PSTR ActionName;
    YY_ACTION_CODE Code;
    YY_ACTION_INDEX Number;
    YY_VALUE Symbol;

    Code = YyActionInvalid;
    Action = Context->Parser[StateIndex];
    Number = -1;
    Symbol = -1;
    while (Action != NULL) {
        if (Action->Suppression == YySuppressedQuietly) {
            Action = Action->Next;
            continue;
        }

        if (Action->Symbol != Symbol) {
            Symbol = Action->Symbol;
            Number = Action->Number;
            Code = Action->Code;
            if (Code == YyActionReduce) {
                Number -= 2;
            }

        } else if (Action->Suppression == YySuppressedNoisily) {
            if ((StateIndex == Context->FinalState) && (Symbol == 0)) {
                fprintf(File,
                        "%d: shift/reduce conflict (accept, reduce %d) on "
                        "$end\n",
                        StateIndex,
                        Action->Number - 2);

            } else {
                ActionName = "shift";
                if (Code == YyActionReduce) {
                    ActionName = "reduce";
                }

                fprintf(File,
                        "%d: %s/reduce conflict (%s %d, reduce %d) on %s\n",
                        StateIndex,
                        ActionName,
                        ActionName,
                        Number,
                        Action->Number - 2,
                        Context->Elements[Symbol].Name);
            }
        }

        Action = Action->Next;
    }

    return;
}

VOID
YypPrintStateItemSets (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_STATE_INDEX StateIndex
    )

/*++

Routine Description:

    This routine prints the item sets in a particular state.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    StateIndex - Supplies the state index to print.

Return Value:

    None.

--*/

{

    PYY_VALUE Current;
    PYY_ELEMENT Elements;
    PYY_VALUE Items;
    YY_ITEM_INDEX ItemsCount;
    YY_ITEM_INDEX ItemSetIndex;
    PYYGEN_RULE Rule;
    YY_RULE_INDEX RuleIndex;
    PYYGEN_STATE State;

    Elements = Context->Elements;
    State = Context->StateTable[StateIndex];
    ItemsCount = State->ItemsCount;
    for (ItemSetIndex = 0; ItemSetIndex < ItemsCount; ItemSetIndex += 1) {
        Items = Context->Items + State->Items[ItemSetIndex];
        Current = Items;
        while (*Current >= 0) {
            Current += 1;
        }

        RuleIndex = -(*Current);
        Rule = &(Context->Rules[RuleIndex]);
        fprintf(File, "\t%s : ", Elements[Rule->LeftSide].Name);
        Current = Context->Items + Rule->RightSide;
        while (*Current >= 0) {
            if (Current == Items) {
                fprintf(File, ". ");
            }

            fprintf(File, "%s ", Elements[*Current].Name);
            Current += 1;
        }

        if (Current == Items) {
            fprintf(File, ". ");
        }

        fprintf(File, "(%d)\n", RuleIndex - 2);
    }

    return;
}

VOID
YypPrintStateNulls (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PYY_RULE_INDEX NullRules,
    YY_STATE_INDEX StateIndex
    )

/*++

Routine Description:

    This routine prints the item set states for empty rules.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    NullRules - Supplies a pointer to an array indexed by rule of whether or
        not a rule is null.

    StateIndex - Supplies the state index.

Return Value:

    None.

--*/

{

    PYYGEN_ACTION Action;
    YY_RULE_INDEX ActionIndex;
    YY_VALUE NullCount;
    YY_VALUE NullIndex;
    PYYGEN_RULE Rule;
    YY_RULE_INDEX RuleIndex;

    NullCount = 0;
    Action = Context->Parser[StateIndex];
    while (Action != NULL) {
        if ((Action->Code == YyActionReduce) &&
            (Action->Suppression != YySuppressedQuietly)) {

            ActionIndex = Action->Number;
            if (Context->Rules[ActionIndex].RightSide ==
                Context->Rules[ActionIndex + 1].RightSide) {

                RuleIndex = 0;
                while ((RuleIndex < NullCount) &&
                       (ActionIndex > NullRules[RuleIndex])) {

                    RuleIndex += 1;
                }

                if (RuleIndex == NullCount) {
                    NullCount += 1;
                    NullRules[RuleIndex] = ActionIndex;

                } else if (ActionIndex != NullRules[RuleIndex]) {
                    NullCount += 1;
                    for (NullIndex = NullCount - 1;
                         NullIndex > RuleIndex;
                         NullIndex -= 1) {

                        NullRules[NullIndex] = NullRules[NullIndex - 1];
                    }

                    NullRules[RuleIndex] = ActionIndex;
                }
            }
        }

        Action = Action->Next;
    }

    for (NullIndex = 0; NullIndex < NullCount; NullIndex += 1) {
        RuleIndex = NullRules[NullIndex];
        Rule = &(Context->Rules[RuleIndex]);
        fprintf(File,
                "\t%s : . (%d)\n",
                Context->Elements[Rule->LeftSide].Name,
                RuleIndex - 2);
    }

    fprintf(File, "\n");
    return;
}

VOID
YypPrintActions (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_STATE_INDEX StateIndex
    )

/*++

Routine Description:

    This routine prints actions out of a particular state.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    StateIndex - Supplies the state index.

Return Value:

    None.

--*/

{

    PYYGEN_ACTION Action;
    PYYGEN_SHIFTS Shifts;
    YY_VALUE Symbol;

    if (StateIndex == Context->FinalState) {
        fprintf(File, "\t$end  accept\n");
    }

    Action = Context->Parser[StateIndex];
    if (Action != NULL) {
        YypPrintShifts(Context, File, Action);
        YypPrintReductions(Context,
                           File,
                           Action,
                           Context->DefaultReductions[StateIndex]);
    }

    Shifts = Context->ShiftTable[StateIndex];
    if ((Shifts != NULL) && (Shifts->Count != 0)) {
        Symbol = Context->AccessingSymbol[Shifts->States[Shifts->Count - 1]];
        if (Symbol >= Context->TokenCount) {
            YypPrintGotos(Context, File, StateIndex);
        }
    }

    return;
}

VOID
YypPrintShifts (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PYYGEN_ACTION Action
    )

/*++

Routine Description:

    This routine prints the given shift actions.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    Action - Supplies a pointer to the head of the action list to print shifts
        for.

Return Value:

    None.

--*/

{

    while (Action != NULL) {
        if ((Action->Code == YyActionShift) &&
            (Action->Suppression == YyNotSuppressed)) {

            fprintf(File,
                    "\t%s  shift %d\n",
                    Context->Elements[Action->Symbol].Name,
                    Action->Number);
        }

        Action = Action->Next;
    }

    return;
}

VOID
YypPrintReductions (
    PYYGEN_CONTEXT Context,
    FILE *File,
    PYYGEN_ACTION Action,
    YY_RULE_INDEX DefaultReduction
    )

/*++

Routine Description:

    This routine prints the given reduction actions.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    Action - Supplies a pointer to the head of the action list to print
        reductions for.

    DefaultReduction - Supplies the default rule to reduce by.

Return Value:

    None.

--*/

{

    BOOL AnyReductions;
    YY_RULE_INDEX RuleIndex;

    AnyReductions = FALSE;
    while (Action != NULL) {
        if ((Action->Code == YyActionReduce) &&
            (Action->Suppression != YySuppressedQuietly)) {

            AnyReductions = TRUE;
        }

        if ((Action->Code == YyActionReduce) &&
            (Action->Number != DefaultReduction)) {

            RuleIndex = Action->Number - 2;
            if (Action->Suppression == YyNotSuppressed) {
                fprintf(File,
                        "\t%s  reduce %d\n",
                        Context->Elements[Action->Symbol].Name,
                        RuleIndex);
            }
        }

        Action = Action->Next;
    }

    if (DefaultReduction > 0) {
        fprintf(File, "\t.  reduce %d\n", DefaultReduction - 2);
    }

    if (AnyReductions == FALSE) {
        fprintf(File, "\t.  error\n");
    }

    return;
}

VOID
YypPrintGotos (
    PYYGEN_CONTEXT Context,
    FILE *File,
    YY_STATE_INDEX StateIndex
    )

/*++

Routine Description:

    This routine prints the gotos for the given action.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

    StateIndex - Supplies the state index to print gotos for.

Return Value:

    None.

--*/

{

    YY_STATE_INDEX Destination;
    YY_VALUE ShiftIndex;
    PYYGEN_SHIFTS Shifts;
    PYY_STATE_INDEX States;
    YY_VALUE Symbol;

    fputc('\n', File);
    Shifts = Context->ShiftTable[StateIndex];
    States = Shifts->States;
    for (ShiftIndex = 0; ShiftIndex < Shifts->Count; ShiftIndex += 1) {
        Destination = States[ShiftIndex];
        Symbol = Context->AccessingSymbol[Destination];
        if (Symbol >= Context->TokenCount) {
            fprintf(File,
                    "\t%s  goto %d\n",
                    Context->Elements[Symbol].Name,
                    Destination);
        }
    }

    return;
}

