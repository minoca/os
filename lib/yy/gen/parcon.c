/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    parcon.c

Abstract:

    This module implements functions related to parser construction and
    finalization.

Author:

    Evan Green 23-Apr-2016

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "yygenp.h"
#include <assert.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PYYGEN_ACTION
YypCreateParseActions (
    PYYGEN_CONTEXT Context,
    YY_STATE_INDEX StateIndex
    );

PYYGEN_ACTION
YypCreateShiftActions (
    PYYGEN_CONTEXT Context,
    YY_STATE_INDEX StateIndex
    );

PYYGEN_ACTION
YypCreateReductionActions (
    PYYGEN_CONTEXT Context,
    YY_STATE_INDEX StateIndex,
    PYYGEN_ACTION Actions
    );

PYYGEN_ACTION
YypCreateReductionAction (
    PYYGEN_CONTEXT Context,
    YY_RULE_INDEX RuleIndex,
    YY_VALUE Symbol,
    PYYGEN_ACTION Actions
    );

VOID
YypFindFinalState (
    PYYGEN_CONTEXT Context
    );

YY_STATUS
YypRemoveConflicts (
    PYYGEN_CONTEXT Context
    );

VOID
YypNoticeUnusedRules (
    PYYGEN_CONTEXT Context
    );

YY_STATUS
YypCreateDefaultReductions (
    PYYGEN_CONTEXT Context
    );

YY_RULE_INDEX
YypFindSoleReduction (
    PYYGEN_CONTEXT Context,
    YY_STATE_INDEX StateIndex
    );

VOID
YypPrintAction (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION Action,
    YY_STATE_INDEX StateIndex
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

YY_STATUS
YypBuildParser (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine generates the parser data structures based on the LALR(1)
    construction.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    YY status.

--*/

{

    YY_STATE_INDEX Index;
    YY_STATUS YyStatus;

    Context->Parser = YypAllocate(Context->StateCount * sizeof(PYYGEN_ACTION));
    if (Context->Parser == NULL) {
        YyStatus = YyStatusNoMemory;
        goto BuildParserEnd;
    }

    for (Index = 0; Index < Context->StateCount; Index += 1) {
        Context->Parser[Index] = YypCreateParseActions(Context, Index);
        if (Context->Parser[Index] == NULL) {
            YyStatus = YyStatusNoMemory;
            goto BuildParserEnd;
        }
    }

    YypFindFinalState(Context);
    YyStatus = YypRemoveConflicts(Context);
    if (YyStatus != YyStatusSuccess) {
        goto BuildParserEnd;
    }

    YypNoticeUnusedRules(Context);
    YyStatus = YypCreateDefaultReductions(Context);

BuildParserEnd:
    return YyStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

PYYGEN_ACTION
YypCreateParseActions (
    PYYGEN_CONTEXT Context,
    YY_STATE_INDEX StateIndex
    )

/*++

Routine Description:

    This routine creates the parser actions for a given state.

Arguments:

    Context - Supplies a pointer to the generator context.

    StateIndex - Supplies the state index.

Return Value:

    Returns a pointer to the new action list on success.

    NULL on allocation failure.

--*/

{

    PYYGEN_ACTION Actions;

    Actions = YypCreateShiftActions(Context, StateIndex);
    Actions = YypCreateReductionActions(Context, StateIndex, Actions);
    if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
        YypPrintAction(Context, Actions, StateIndex);
    }

    return Actions;
}

PYYGEN_ACTION
YypCreateShiftActions (
    PYYGEN_CONTEXT Context,
    YY_STATE_INDEX StateIndex
    )

/*++

Routine Description:

    This routine creates the parser shift actions for a given state.

Arguments:

    Context - Supplies a pointer to the generator context.

    StateIndex - Supplies the state index.

Return Value:

    Returns a pointer to the new shift action list on success.

    NULL on allocation failure.

--*/

{

    PYYGEN_ACTION Actions;
    YY_STATE_INDEX DestinationState;
    ULONG Flags;
    PYYGEN_ACTION NewAction;
    YY_VALUE ShiftIndex;
    PYYGEN_SHIFTS Shifts;
    PYY_STATE_INDEX States;
    YY_VALUE Symbol;
    YY_STATUS YyStatus;

    Actions = NULL;
    Shifts = Context->ShiftTable[StateIndex];
    if (Shifts != NULL) {
        States = Shifts->States;

        //
        // Look through all the shifts for this state. Add actions for all
        // shifts based on terminals.
        //

        for (ShiftIndex = Shifts->Count - 1; ShiftIndex >= 0; ShiftIndex -= 1) {
            DestinationState = States[ShiftIndex];
            Symbol = Context->AccessingSymbol[DestinationState];
            if (Symbol < Context->TokenCount) {
                NewAction = YypAllocate(sizeof(YYGEN_ACTION));
                if (NewAction == NULL) {
                    YyStatus = YyStatusNoMemory;
                    goto CreateShiftActionsEnd;
                }

                NewAction->Next = Actions;
                NewAction->Symbol = Symbol;
                NewAction->Number = DestinationState;
                NewAction->Precedence = Context->Elements[Symbol].Precedence;
                Flags = Context->Elements[Symbol].Flags;
                if (Flags != 0) {
                    if ((Flags & YY_ELEMENT_LEFT_ASSOCIATIVE) != 0) {
                        NewAction->Associativity = YyLeftAssociative;

                    } else if ((Flags & YY_ELEMENT_RIGHT_ASSOCIATIVE) != 0) {
                        NewAction->Associativity = YyRightAssociative;

                    } else if ((Flags & YY_ELEMENT_NON_ASSOCIATIVE) != 0) {
                        NewAction->Associativity = YyNonAssociative;
                    }
                }

                NewAction->Code = YyActionShift;
                Actions = NewAction;
            }
        }
    }

    YyStatus = YyStatusSuccess;

CreateShiftActionsEnd:
    if (YyStatus != YyStatusSuccess) {
        while (Actions != NULL) {
            NewAction = Actions->Next;
            YypFree(Actions);
            Actions = NewAction;
        }
    }

    return Actions;
}

PYYGEN_ACTION
YypCreateReductionActions (
    PYYGEN_CONTEXT Context,
    YY_STATE_INDEX StateIndex,
    PYYGEN_ACTION Actions
    )

/*++

Routine Description:

    This routine creates the parser reduction actions for a given state.

Arguments:

    Context - Supplies a pointer to the generator context.

    StateIndex - Supplies the state index.

    Actions - Supplies the head of the list of actions already created by the
        shifts.

Return Value:

    Returns a pointer to the new complete action list on success.

    NULL on allocation failure.

--*/

{

    YY_VALUE End;
    YY_VALUE Lookahead;
    PULONG Row;
    YY_RULE_INDEX RuleIndex;
    YY_VALUE Token;
    ULONG TokenSetSize;

    TokenSetSize = YYGEN_BITMAP_WORD_COUNT(Context->TokenCount);
    Lookahead = Context->Lookaheads[StateIndex];
    End = Context->Lookaheads[StateIndex + 1];
    while (Lookahead < End) {
        RuleIndex = Context->LookaheadRule[Lookahead];
        Row = Context->LookaheadSets + (Lookahead * TokenSetSize);
        for (Token = Context->TokenCount - 1; Token >= 0; Token -= 1) {
            if (YYGEN_BITMAP_IS_SET(Row, Token)) {
                Actions = YypCreateReductionAction(Context,
                                                   RuleIndex,
                                                   Token,
                                                   Actions);

                if (Actions == NULL) {
                    return NULL;
                }
            }
        }

        Lookahead += 1;
    }

    return Actions;
}

PYYGEN_ACTION
YypCreateReductionAction (
    PYYGEN_CONTEXT Context,
    YY_RULE_INDEX RuleIndex,
    YY_VALUE Symbol,
    PYYGEN_ACTION Actions
    )

/*++

Routine Description:

    This routine creates a parser reduction action for a given rule and token.

Arguments:

    Context - Supplies a pointer to the generator context.

    RuleIndex - Supplies the rule index.

    Symbol - Supplies the lookahead symbol.

    Actions - Supplies the head of the list of actions already created by the
        shifts.

Return Value:

    Returns a pointer to the new complete action list on success.

    NULL on allocation failure.

--*/

{

    PYYGEN_ACTION NewAction;
    PYYGEN_ACTION Next;
    PYYGEN_ACTION Previous;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;

    //
    // Keep everything sorted by finding the right spot to insert this here.
    //

    Previous = NULL;
    Next = Actions;
    while ((Next != NULL) && (Next->Symbol < Symbol)) {
        Previous = Next;
        Next = Next->Next;
    }

    //
    // Let shifts with the same symbol be first.
    //

    while ((Next != NULL) && (Next->Symbol == Symbol) &&
           (Next->Code == YyActionShift)) {

        Previous = Next;
        Next = Next->Next;
    }

    //
    // Let reductions with the same symbol and an earlier rule number be first.
    //

    while ((Next != NULL) && (Next->Symbol == Symbol) &&
           (Next->Code == YyActionReduce) && (Next->Number < RuleIndex)) {

        Previous = Next;
        Next = Next->Next;
    }

    NewAction = YypAllocate(sizeof(YYGEN_ACTION));
    if (NewAction == NULL) {
        goto CreateReductionActionEnd;
    }

    NewAction->Next = Next;
    NewAction->Symbol = Symbol;
    NewAction->Number = RuleIndex;
    NewAction->Precedence = Context->Rules[RuleIndex].Precedence;
    NewAction->Associativity = Context->Rules[RuleIndex].Associativity;
    NewAction->Code = YyActionReduce;
    if (Previous != NULL) {
        Previous->Next = NewAction;

    } else {
        Actions = NewAction;
    }

    YyStatus = YyStatusSuccess;

CreateReductionActionEnd:
    if (YyStatus != YyStatusSuccess) {
        while (Actions != NULL) {
            NewAction = Actions->Next;
            YypFree(Actions);
            Actions = NewAction;
        }
    }

    return Actions;
}

VOID
YypFindFinalState (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine locates the acceptance state.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    None.

--*/

{

    YY_STATE_INDEX FinalState;
    YY_VALUE Goal;
    YY_VALUE ShiftIndex;
    PYYGEN_SHIFTS Shifts;
    PYY_STATE_INDEX ToState;

    Shifts = Context->ShiftTable[0];
    ToState = Shifts->States;
    Goal = Context->Items[1];
    for (ShiftIndex = Shifts->Count - 1; ShiftIndex >= 0; ShiftIndex -= 1) {
        FinalState = ToState[ShiftIndex];
        if (Context->AccessingSymbol[FinalState] == Goal) {
            Context->FinalState = FinalState;
            break;
        }
    }

    assert(Context->FinalState != 0);

    return;
}

YY_STATUS
YypRemoveConflicts (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine picks a solution for and notes grammar conflicts.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    YY Status.

--*/

{

    PYYGEN_ACTION Action;
    PYYGEN_ACTION Preferred;
    YY_VALUE ReduceCount;
    YY_VALUE ShiftCount;
    YY_STATE_INDEX StateIndex;
    YY_VALUE Symbol;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;
    Context->ShiftReduceConflictCount = 0;
    Context->ReduceReduceConflictCount = 0;
    Context->ShiftReduceConflicts =
                           YypAllocate(Context->StateCount * sizeof(YY_VALUE));

    if (Context->ShiftReduceConflicts == NULL) {
        goto RemoveConflictsEnd;
    }

    Context->ReduceReduceConflicts =
                           YypAllocate(Context->StateCount * sizeof(YY_VALUE));

    if (Context->ReduceReduceConflicts == NULL) {
        goto RemoveConflictsEnd;
    }

    for (StateIndex = 0; StateIndex < Context->StateCount; StateIndex += 1) {
        ShiftCount = 0;
        ReduceCount = 0;
        Symbol = -1;
        Action = Context->Parser[StateIndex];
        Preferred = NULL;
        while (Action != NULL) {

            //
            // The first action is the preferred action.
            //

            if (Action->Symbol != Symbol) {
                Preferred = Action;
                Symbol = Action->Symbol;

            } else if ((StateIndex == Context->FinalState) && (Symbol == 0)) {
                ShiftCount += 1;
                Action->Suppression = YySuppressedNoisily;

            } else if ((Preferred != NULL) &&
                       (Preferred->Code == YyActionShift)) {

                //
                // Resolve the conflict with precedences.
                //

                if ((Preferred->Precedence > 0) && (Action->Precedence > 0)) {
                    if (Preferred->Precedence < Action->Precedence) {
                        Preferred->Suppression = YySuppressedQuietly;
                        Preferred = Action;

                    } else if (Preferred->Precedence > Action->Precedence) {
                        Action->Suppression = YySuppressedQuietly;

                    } else if (Preferred->Associativity == YyLeftAssociative) {
                        Preferred->Suppression = YySuppressedQuietly;
                        Preferred = Action;

                    } else if (Preferred->Associativity == YyRightAssociative) {
                        Action->Suppression =  YySuppressedQuietly;

                    } else {
                        Preferred->Suppression = YySuppressedQuietly;
                        Action->Suppression = YySuppressedQuietly;
                    }

                //
                // Add a shift reduce conflict.
                //

                } else {
                    ShiftCount += 1;
                    Action->Suppression = YySuppressedNoisily;
                }

            } else {

                assert((Preferred != NULL) &&
                       (Preferred->Code == YyActionReduce));

                ReduceCount += 1;
                Action->Suppression = YySuppressedNoisily;
            }

            Action = Action->Next;
        }

        Context->ShiftReduceConflictCount += ShiftCount;
        Context->ReduceReduceConflictCount += ReduceCount;
        Context->ShiftReduceConflicts[StateIndex] = ShiftCount;
        Context->ReduceReduceConflicts[StateIndex] = ReduceCount;
    }

    YyStatus = YyStatusSuccess;

RemoveConflictsEnd:
    return YyStatus;
}

VOID
YypNoticeUnusedRules (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine sets the context variable of how many rules never reduced.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    None.

--*/

{

    PYYGEN_ACTION Action;
    YY_RULE_INDEX RuleIndex;
    YY_STATE_INDEX StateIndex;

    Context->UnusedRules = 0;
    for (StateIndex = 0; StateIndex < Context->StateCount; StateIndex += 1) {
        Action = Context->Parser[StateIndex];
        while (Action != NULL) {
            if ((Action->Code == YyActionReduce) &&
                (Action->Suppression == YyNotSuppressed)) {

                Context->Rules[Action->Number].Used = TRUE;
            }

            Action = Action->Next;
        }
    }

    Context->UnusedRules = 0;
    for (RuleIndex = 3; RuleIndex < Context->RuleCount; RuleIndex += 1) {
        if (Context->Rules[RuleIndex].Used == FALSE) {
            Context->UnusedRules += 1;
        }
    }

    return;
}

YY_STATUS
YypCreateDefaultReductions (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine creates the default reductions table for states with only
    one move: to reduce.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    YY Status.

--*/

{

    PYY_RULE_INDEX DefaultReductions;
    YY_STATE_INDEX StateIndex;

    DefaultReductions =
                      YypAllocate(Context->StateCount * sizeof(YY_RULE_INDEX));

    if (DefaultReductions == NULL) {
        return YyStatusNoMemory;
    }

    for (StateIndex = 0; StateIndex < Context->StateCount; StateIndex += 1) {
        DefaultReductions[StateIndex] = YypFindSoleReduction(Context,
                                                             StateIndex);
    }

    Context->DefaultReductions = DefaultReductions;
    return YyStatusSuccess;
}

YY_RULE_INDEX
YypFindSoleReduction (
    PYYGEN_CONTEXT Context,
    YY_STATE_INDEX StateIndex
    )

/*++

Routine Description:

    This routine determines the rule by which to reduce if the given state's
    only action is to reduce.

Arguments:

    Context - Supplies a pointer to the generator context.

    StateIndex - Supplies the state to examine.

Return Value:

    0 if the state does not simply reduce as its only action.

    Returns the index of the rule to reduce by if the state simply reduces.

--*/

{

    PYYGEN_ACTION Action;
    ULONG Count;
    YY_RULE_INDEX Rule;

    Count = 0;
    Rule = 0;
    Action = Context->Parser[StateIndex];
    while (Action != NULL) {
        if (Action->Suppression == YyNotSuppressed) {
            if (Action->Code == YyActionShift) {
                return 0;

            } else if (Action->Code == YyActionReduce) {

                //
                // Bail if there are multiple possible reductions.
                //

                if ((Rule > 0) && (Action->Number != Rule)) {
                    return 0;
                }

                if (Action->Symbol != -1) {
                    Count += 1;
                }

                Rule = Action->Number;
            }
        }

        Action = Action->Next;
    }

    if (Count == 0) {
        return 0;
    }

    return Rule;
}

VOID
YypPrintAction (
    PYYGEN_CONTEXT Context,
    PYYGEN_ACTION Action,
    YY_STATE_INDEX StateIndex
    )

/*++

Routine Description:

    This routine prints the given list of actions.

Arguments:

    Context - Supplies a pointer to the generator context.

    Action - Supplies the head of the action list.

    StateIndex - Supplies the state index.

Return Value:

    None.

--*/

{

    PSTR Verb;

    printf("\nActions for state %d:\n", StateIndex);
    while (Action) {
        Verb = "Shift";
        if (Action->Code == YyActionReduce) {
            Verb = "Reduce";
        }

        printf("  %s on %s to %d\n",
               Verb,
               Context->Elements[Action->Symbol].Name,
               Action->Number);

        Action = Action->Next;
    }

    return;
}

