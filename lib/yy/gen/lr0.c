/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lr0.c

Abstract:

    This module implements support for generating an LR(0) grammar from a
    description of productions.

Author:

    Evan Green 9-Apr-2016

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "yygenp.h"
#include <assert.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure contains the working state for the LR(0) state generator.

Members:

    StateSet - Stores the hash table of states, based on the item index of
        their first right hand side.

    ShiftSet - Stores the set of states to shift to for each shift symbol.

    ShiftSymbol - Stores the array of possible shift symbols out of the
        current state.

    ReduceSet - Stores the set of reductions being built for the current state.

    KernelBase - Stores an array of arrays of item indices, indexed by shift
        symbol. For the currently operated on state, if symbol I were shifted,
        what new item sets would be in the kernel. The kernel is the part of
        a state added by the grammar (or previous state), rather than the
        closure.

    KernelEnd - Stores an array indicating the end of the kernel base arrays,
        again indexed by shift symbol.

    KernelItems - Stores a big flattened array of all the kernel item sets.
        This is the storage area for kernel base.

    LastState - Stores a pointer to the last LR(0) state processed.

    CurrentState - Stores a pointer to the current LR(0) state being operated
        on.

    ShiftCount - Stores the number of shifts.

--*/

typedef struct _YYGEN_STATE_CONTEXT {
    PYYGEN_STATE *StateSet;
    PYY_STATE_INDEX ShiftSet;
    PYY_VALUE ShiftSymbol;
    PYY_RULE_INDEX ReduceSet;
    PYY_ITEM_INDEX *KernelBase;
    PYY_ITEM_INDEX *KernelEnd;
    PYY_ITEM_INDEX KernelItems;
    PYYGEN_STATE LastState;
    PYYGEN_STATE CurrentState;
    YY_VALUE ShiftCount;
} YYGEN_STATE_CONTEXT, *PYYGEN_STATE_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

YY_STATUS
YypGenerateDerives (
    PYYGEN_CONTEXT Context
    );

YY_STATUS
YypGenerateNullable (
    PYYGEN_CONTEXT Context
    );

YY_STATUS
YypGenerateStates (
    PYYGEN_CONTEXT Context
    );

YY_STATUS
YypInitializeStateContext (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    );

VOID
YypDestroyStateContext (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    );

YY_STATUS
YypGenerateFirstDerives (
    PYYGEN_CONTEXT Context
    );

PULONG
YypGenerateEpsilonFreeFirstSet (
    PYYGEN_CONTEXT Context
    );

VOID
YypGenerateReflexiveTransitiveClosure (
    PULONG Bitmap,
    ULONG BitCount
    );

VOID
YypGenerateTransitiveClosure (
    PULONG Bitmap,
    ULONG BitCount
    );

YY_STATUS
YypInitializeStates (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    );

YY_STATUS
YypSaveReductions (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    );

VOID
YypAdvanceItemSets (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    );

VOID
YypAddNewStates (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    );

YY_STATE_INDEX
YypGetState (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext,
    YY_VALUE Symbol
    );

PYYGEN_STATE
YypCreateState (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext,
    YY_VALUE Symbol
    );

YY_STATUS
YypSaveShifts (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    );

VOID
YypPrintItems (
    PYYGEN_CONTEXT Context
    );

VOID
YypPrintDerives (
    PYYGEN_CONTEXT Context
    );

VOID
YypPrintEpsilonFreeFirsts (
    PYYGEN_CONTEXT Context,
    PULONG EpsilonFreeFirsts
    );

VOID
YypPrintFirstDerives (
    PYYGEN_CONTEXT Context
    );

VOID
YypPrintClosure (
    PYYGEN_CONTEXT Context,
    YY_VALUE ItemsCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

YY_STATUS
YypGenerateLr0Grammar (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine generates an LR(0) grammar based on the description.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    YY status.

--*/

{

    YY_STATUS YyStatus;

    YyStatus = YypGenerateDerives(Context);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateLr0GrammarEnd;
    }

    YyStatus = YypGenerateNullable(Context);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateLr0GrammarEnd;
    }

    YyStatus = YypGenerateStates(Context);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateLr0GrammarEnd;
    }

GenerateLr0GrammarEnd:
    return YyStatus;
}

VOID
YypEstablishClosure (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE State
    )

/*++

Routine Description:

    This routine creates a closure on the itemset of the current state.

Arguments:

    Context - Supplies a pointer to the generator context.

    State - Supplies a pointer to the state to create the closure of.

Return Value:

    None.

--*/

{

    ULONG BitIndex;
    PULONG CurrentRuleSet;
    ULONG CurrentWord;
    PULONG FirstDerives;
    YY_ITEM_INDEX ItemIndex;
    PYY_ITEM_INDEX ItemsPointer;
    PYY_ITEM_INDEX Nucleus;
    ULONG NucleusCount;
    PYY_ITEM_INDEX NucleusEnd;
    YY_RULE_INDEX RuleNumber;
    PULONG RuleSetEnd;
    ULONG RuleSetSize;
    YY_VALUE StartSymbol;
    YY_VALUE Symbol;

    Nucleus = State->Items;
    NucleusCount = State->ItemsCount;
    NucleusEnd = Nucleus + NucleusCount;
    RuleSetSize = YYGEN_BITMAP_WORD_COUNT(Context->RuleCount);
    RuleSetEnd = Context->RuleSet + RuleSetSize;
    StartSymbol = Context->StartSymbol;
    CurrentRuleSet = Context->RuleSet;
    while (CurrentRuleSet < RuleSetEnd) {
        *CurrentRuleSet = 0;
        CurrentRuleSet += 1;
    }

    //
    // Loop through all the right hand sides. OR into the ruleset all of the
    // first derives from the first element if it's a non-terminal.
    //

    ItemsPointer = Nucleus;
    while (ItemsPointer < NucleusEnd) {
        Symbol = Context->Items[*ItemsPointer];
        if (Symbol >= Context->TokenCount) {
            FirstDerives = Context->FirstDerives +
                           ((Symbol - StartSymbol) * RuleSetSize);

            CurrentRuleSet = Context->RuleSet;
            while (CurrentRuleSet < RuleSetEnd) {
                *CurrentRuleSet |= *FirstDerives;
                CurrentRuleSet += 1;
                FirstDerives += 1;
            }
        }

        ItemsPointer += 1;
    }

    //
    // Merge the itemsets from the rules indicated by the ruleset into the
    // nucleus, keeping them in global item array order and avoiding duplicates.
    //

    RuleNumber = 0;
    Context->ItemSetEnd = Context->ItemSet;
    ItemsPointer = Nucleus;
    CurrentRuleSet = Context->RuleSet;
    while (CurrentRuleSet < RuleSetEnd) {
        CurrentWord = *CurrentRuleSet;
        if (CurrentWord != 0) {
            for (BitIndex = 0; BitIndex < YYGEN_BITS_PER_WORD; BitIndex += 1) {
                if ((CurrentWord & (1 << BitIndex)) != 0) {
                    ItemIndex = Context->Rules[RuleNumber + BitIndex].RightSide;
                    while ((ItemsPointer < NucleusEnd) &&
                           (*ItemsPointer < ItemIndex)) {

                        *(Context->ItemSetEnd) = *ItemsPointer;
                        Context->ItemSetEnd += 1;
                        ItemsPointer += 1;
                    }

                    *(Context->ItemSetEnd) = ItemIndex;
                    Context->ItemSetEnd += 1;
                    while ((ItemsPointer < NucleusEnd) &&
                           (*ItemsPointer == ItemIndex)) {

                        ItemsPointer += 1;
                    }
                }
            }
        }

        RuleNumber += YYGEN_BITS_PER_WORD;
        CurrentRuleSet += 1;
    }

    while (ItemsPointer < NucleusEnd) {
        *(Context->ItemSetEnd) = *ItemsPointer;
        Context->ItemSetEnd += 1;
        ItemsPointer += 1;
    }

    if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
        YypPrintClosure(Context, State->ItemsCount);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

YY_STATUS
YypGenerateDerives (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine generates the derives array, which is an array of unique rules
    and points to runs of rules.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    YY status.

--*/

{

    PYY_VALUE Components;
    PYYGEN_RULE CurrentRule;
    PYY_RULE_INDEX Derives;
    ULONG Flags;
    YY_ITEM_INDEX ItemIndex;
    PYY_VALUE Items;
    YY_VALUE LastTerminal;
    YY_VALUE LeftSide;
    YY_RULE_INDEX RuleIndex;
    PYYGEN_RULE Rules;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;
    Derives = YypAllocate(Context->SymbolCount * sizeof(YY_RULE_INDEX));
    if (Derives == NULL) {
        goto GenerateDerivesEnd;
    }

    Context->Derives = Derives;

    //
    // There's an extra rule on the end to terminate the last run of
    // rules while iterating.
    //

    Rules = YypAllocate(sizeof(YYGEN_RULE) * (Context->RuleCount + 1));
    if (Rules == NULL) {
        YyStatus = YyStatusNoMemory;
        goto GenerateDerivesEnd;
    }

    Context->Rules = Rules;
    Items = YypAllocate(sizeof(YY_VALUE) * Context->ItemCount);
    if (Items == NULL) {
        YyStatus = YyStatusNoMemory;
        goto GenerateDerivesEnd;
    }

    Context->Items = Items;

    //
    // The first item corresponds to rule 1 and it's empty.
    //

    Items[0] = -1;

    //
    // The next three items correspond to the right hand side of the start
    // rule, which is to produce the production marked start and then EOF.
    //

    Items[1] = Context->StartSymbol;
    Context->StartSymbol = Context->TokenCount;
    Items[2] = 0;
    Items[3] = -2;
    ItemIndex = 4;

    //
    // The first rule is invalid since it cannot be negated.
    // The second rule is empty.
    // The third rule is the start rule.
    //

    CurrentRule = Rules + 2;
    CurrentRule->LeftSide = Context->StartSymbol;
    CurrentRule->RightSide = 1;
    Derives[Context->StartSymbol] = 2;
    CurrentRule += 1;
    RuleIndex = 3;

    //
    // Loop over converting productions to derives + rules.
    //

    for (LeftSide = Context->StartSymbol + 1;
         LeftSide < Context->SymbolCount;
         LeftSide += 1) {

        Components = Context->Elements[LeftSide].Components;
        Derives[LeftSide] = RuleIndex;

        assert((Components != NULL) && (*Components >= 0));

        while (*Components != 0) {
            CurrentRule->LeftSide = LeftSide;
            CurrentRule->RightSide = ItemIndex;
            LastTerminal = -1;
            while (*Components > 0) {
                Items[ItemIndex] = *Components;

                //
                // Keep track of the last terminal specified in the rule.
                //

                if (*Components < Context->TokenCount) {
                    LastTerminal = *Components;
                }

                Components += 1;
                ItemIndex += 1;
            }

            Items[ItemIndex] = -RuleIndex;
            ItemIndex += 1;

            //
            // The precedence for the rule is specified in the terminator. -1
            // corresponds to precedence 0 (none). If no precedence or
            // associativity is describe for the rule, then it is taken from
            // the last terminal in the rule.
            //

            assert(*Components != 0);

            CurrentRule->Precedence = -(*Components) - 1;
            if ((CurrentRule->Precedence == 0) && (LastTerminal > 0)) {
                CurrentRule->Precedence =
                                    Context->Elements[LastTerminal].Precedence;
            }

            Flags = Context->Elements[LeftSide].Flags;
            if (Flags == 0) {
                Flags = Context->Elements[LastTerminal].Flags;
            }

            if (Flags != 0) {
                if ((Flags & YY_ELEMENT_LEFT_ASSOCIATIVE) != 0) {
                    CurrentRule->Associativity = YyLeftAssociative;

                } else if ((Flags & YY_ELEMENT_RIGHT_ASSOCIATIVE) != 0) {
                    CurrentRule->Associativity = YyRightAssociative;

                } else if ((Flags & YY_ELEMENT_NON_ASSOCIATIVE) != 0) {
                    CurrentRule->Associativity = YyNonAssociative;
                }
            }

            Components += 1;
            CurrentRule += 1;
            RuleIndex += 1;
        }
    }

    assert(Rules + Context->RuleCount == CurrentRule);

    CurrentRule->LeftSide = 0;
    CurrentRule->RightSide = ItemIndex;
    if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
        YypPrintItems(Context);
        YypPrintDerives(Context);
    }

    YyStatus = YyStatusSuccess;

GenerateDerivesEnd:
    return YyStatus;
}

YY_STATUS
YypGenerateNullable (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine generates the array of booleans indicating which rules are
    empty or are made up of other rules that are empty.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    YY status.

--*/

{

    BOOL Empty;
    BOOL FoundOne;
    YY_ITEM_INDEX ItemIndex;
    YY_VALUE LeftSide;
    PBOOL Nullable;
    YY_RULE_INDEX RuleIndex;
    YY_ITEM_INDEX Search;

    Nullable = YypAllocate(Context->SymbolCount * sizeof(BOOL));
    if (Nullable == NULL) {
        return YyStatusNoMemory;
    }

    //
    // The idea is to find which productions are empty, then go back and mark
    // any productions that are just made up of those productions as empty also,
    // and so on, until no new empty ones are found.
    //

    do {
        FoundOne = FALSE;
        for (ItemIndex = 1; ItemIndex < Context->ItemCount; ItemIndex += 1) {
            Empty = TRUE;

            //
            // Loop over each element in the rule. If it consists of something
            // that's not nullable (including a token), then it's also not
            // nullable.
            //

            Search = Context->Items[ItemIndex];
            while (Search >= 0) {
                if (Nullable[Search] == FALSE) {
                    Empty = FALSE;
                }

                ItemIndex += 1;
                Search = Context->Items[ItemIndex];
            }

            //
            // If it's empty or is only made up of other things that are
            // empty, mark it as nullable. This means everything will have
            // to be checked again.
            //

            if (Empty != FALSE) {
                RuleIndex = -Search;
                LeftSide = Context->Rules[RuleIndex].LeftSide;
                if (Nullable[LeftSide] == FALSE) {
                    Nullable[LeftSide] = TRUE;
                    FoundOne = TRUE;
                }
            }
        }

    } while (FoundOne != FALSE);

    if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
        for (ItemIndex = 0; ItemIndex < Context->SymbolCount; ItemIndex += 1) {
            if (Nullable[ItemIndex] != FALSE) {
                printf("%s is nullable\n", Context->Elements[ItemIndex].Name);

            } else {
                printf("%s is not nullable\n",
                       Context->Elements[ItemIndex].Name);
            }
        }
    }

    Context->Nullable = Nullable;
    return YyStatusSuccess;
}

YY_STATUS
YypGenerateStates (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine generates the LR(0) grammar states.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    YY status.

--*/

{

    UINTN AllocationSize;
    YYGEN_STATE_CONTEXT StateContext;
    YY_STATUS YyStatus;

    memset(&StateContext, 0, sizeof(YYGEN_STATE_CONTEXT));
    YyStatus = YypInitializeStateContext(Context, &StateContext);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateStatesEnd;
    }

    Context->ItemSet = YypAllocate(Context->ItemCount * sizeof(YY_VALUE));
    if (Context->ItemSet == NULL) {
        YyStatus = YyStatusNoMemory;
        goto GenerateStatesEnd;
    }

    AllocationSize = YYGEN_BITMAP_WORD_COUNT(Context->RuleCount) *
                     sizeof(ULONG);

    Context->RuleSet = YypAllocate(AllocationSize);
    if (Context->RuleSet == NULL) {
        YyStatus = YyStatusNoMemory;
        goto GenerateStatesEnd;
    }

    YyStatus = YypGenerateFirstDerives(Context);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateStatesEnd;
    }

    YyStatus = YypInitializeStates(Context, &StateContext);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateStatesEnd;
    }

    while (StateContext.CurrentState != NULL) {
        if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
            printf("State %d:\n", StateContext.CurrentState->Number);
        }

        YypEstablishClosure(Context, StateContext.CurrentState);
        YypSaveReductions(Context, &StateContext);
        YypAdvanceItemSets(Context, &StateContext);
        YypAddNewStates(Context, &StateContext);
        if (StateContext.ShiftCount != 0) {
            YypSaveShifts(Context, &StateContext);
        }

        StateContext.CurrentState = StateContext.CurrentState->Next;
    }

    YyStatus = YyStatusSuccess;

GenerateStatesEnd:
    YypDestroyStateContext(Context, &StateContext);
    return YyStatus;
}

YY_STATUS
YypInitializeStateContext (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    )

/*++

Routine Description:

    This routine allocates arrays needed for LR(0) state generation.

Arguments:

    Context - Supplies a pointer to the generator context.

    StateContext - Supplies a pointer to the zeroed out state context.

Return Value:

    YY status.

--*/

{

    YY_VALUE Count;
    PYY_VALUE End;
    PYY_VALUE Item;
    YY_VALUE Symbol;
    PYY_VALUE SymbolCounts;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;
    SymbolCounts = YypAllocate(Context->SymbolCount * sizeof(YY_VALUE));
    if (SymbolCounts == NULL) {
        goto InitializeStateContextEnd;
    }

    //
    // Count the number of times a symbol is referenced, and the total number
    // of symbols in all the rules.
    //

    End = Context->Items + Context->ItemCount;
    Count = 0;
    Item = Context->Items;
    while (Item < End) {
        Symbol = *Item;
        if (Symbol >= 0) {
            Count += 1;
            SymbolCounts[Symbol] += 1;
        }

        Item += 1;
    }

    //
    // Allocate the kernel base and kernel item arrays.
    //

    StateContext->KernelBase =
                    YypAllocate(Context->SymbolCount * sizeof(PYY_ITEM_INDEX));

    if (StateContext->KernelBase == NULL) {
        goto InitializeStateContextEnd;
    }

    StateContext->KernelEnd =
                    YypAllocate(Context->SymbolCount * sizeof(PYY_ITEM_INDEX));

    if (StateContext->KernelEnd == NULL) {
        goto InitializeStateContextEnd;
    }

    StateContext->KernelItems = YypAllocate(Count * sizeof(YY_ITEM_INDEX));
    if (StateContext->KernelItems == NULL) {
        goto InitializeStateContextEnd;
    }

    //
    // Initialize the indices for ther kernel base array, knowing how large
    // each kernel base array will be but not initializing it yet.
    //

    Count = 0;
    for (Symbol = 0; Symbol < Context->SymbolCount; Symbol += 1) {
        StateContext->KernelBase[Symbol] = StateContext->KernelItems + Count;
        Count += SymbolCounts[Symbol];
    }

    StateContext->ShiftSymbol = SymbolCounts;
    SymbolCounts = NULL;
    StateContext->ShiftSet =
                    YypAllocate(Context->SymbolCount * sizeof(YY_STATE_INDEX));

    if (StateContext->ShiftSet == NULL) {
        goto InitializeStateContextEnd;
    }

    StateContext->ReduceSet =
               YypAllocate((Context->SymbolCount + 1) * sizeof(YY_RULE_INDEX));

    if (StateContext->ReduceSet == NULL) {
        goto InitializeStateContextEnd;
    }

    StateContext->StateSet =
                        YypAllocate(Context->ItemCount * sizeof(PYYGEN_STATE));

    if (StateContext->StateSet == NULL) {
        goto InitializeStateContextEnd;
    }

    YyStatus = YyStatusSuccess;

InitializeStateContextEnd:
    if (SymbolCounts != NULL) {
        YypFree(SymbolCounts);
    }

    return YyStatus;
}

VOID
YypDestroyStateContext (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    )

/*++

Routine Description:

    This routine frees memory allocated in the given state context.

Arguments:

    Context - Supplies a pointer to the application context.

    StateContext - Supplies a pointer to the state context.

Return Value:

    None.

--*/

{

    if (StateContext->StateSet != NULL) {
        YypFree(StateContext->StateSet);
    }

    if (StateContext->ShiftSet != NULL) {
        YypFree(StateContext->ShiftSet);
    }

    if (StateContext->ReduceSet != NULL) {
        YypFree(StateContext->ReduceSet);
    }

    if (StateContext->ShiftSymbol != NULL) {
        YypFree(StateContext->ShiftSymbol);
    }

    if (StateContext->KernelBase != NULL) {
        YypFree(StateContext->KernelBase);
    }

    if (StateContext->KernelEnd != NULL) {
        YypFree(StateContext->KernelEnd);
    }

    if (StateContext->KernelItems != NULL) {
        YypFree(StateContext->KernelItems);
    }

    return;
}

YY_STATUS
YypGenerateFirstDerives (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine creates the array of FIRST bitmaps, indicating which rules are
    involved in the FIRST set of each non-terminal. The FIRST set is the set
    of terminals that can appear first in a given itemset.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    YY status.

--*/

{

    ULONG BitIndex;
    ULONG CurrentWord;
    PULONG EffRow;
    PULONG EpsilonFreeFirsts;
    PULONG FirstRow;
    YY_VALUE LeftSide;
    ULONG NonTerminalSetSize;
    YY_VALUE RowIndex;
    PYYGEN_RULE Rule;
    YY_RULE_INDEX RuleIndex;
    ULONG RuleSetSize;
    YY_VALUE StartSymbol;
    YY_VALUE SymbolIndex;
    YY_STATUS YyStatus;

    EpsilonFreeFirsts = NULL;
    RuleSetSize = YYGEN_BITMAP_WORD_COUNT(Context->RuleCount);
    NonTerminalSetSize = YYGEN_BITMAP_WORD_COUNT(Context->NonTerminalCount);
    StartSymbol = Context->StartSymbol;
    YyStatus = YyStatusNoMemory;
    Context->FirstDerives =
        YypAllocate(Context->NonTerminalCount * (RuleSetSize * sizeof(ULONG)));

    if (Context->FirstDerives == NULL) {
        goto GenerateFirstSetEnd;
    }

    //
    // Get the closure of first non-terminals for each non-terminal.
    //

    EpsilonFreeFirsts = YypGenerateEpsilonFreeFirstSet(Context);
    if (EpsilonFreeFirsts == NULL) {
        goto GenerateFirstSetEnd;
    }

    //
    // Loop through each row (non-terminal) of the first set.
    //

    FirstRow = Context->FirstDerives;
    for (RowIndex = StartSymbol;
         RowIndex < Context->SymbolCount;
         RowIndex += 1) {

        //
        // Get the equivalent row in the EFF bitmap.
        //

        EffRow = EpsilonFreeFirsts +
                 ((RowIndex - StartSymbol) * NonTerminalSetSize);

        BitIndex = 0;
        CurrentWord = *EffRow;

        //
        // Loop over every bit in the bitmap.
        //

        for (SymbolIndex = StartSymbol;
             SymbolIndex < Context->SymbolCount;
             SymbolIndex += 1) {

            //
            // If the bit is set in the EFF bitmap, then set the bits for all
            // of that non-terminal's rules.
            //

            if ((CurrentWord & (1 << BitIndex)) != 0) {
                RuleIndex = Context->Derives[SymbolIndex];

                assert(RuleIndex >= 0);

                Rule = &(Context->Rules[RuleIndex]);
                LeftSide = Rule->LeftSide;
                do {
                    YYGEN_BITMAP_SET(FirstRow, RuleIndex);
                    Rule += 1;
                    RuleIndex += 1;

                } while (Rule->LeftSide == LeftSide);
            }

            BitIndex += 1;
            if (BitIndex == YYGEN_BITS_PER_WORD) {
                BitIndex = 0;
                EffRow += 1;
                CurrentWord = *EffRow;
            }
        }

        FirstRow += RuleSetSize;
    }

    if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
        YypPrintFirstDerives(Context);
    }

    YyStatus = YyStatusSuccess;

GenerateFirstSetEnd:
    if (EpsilonFreeFirsts != NULL) {
        YypFree(EpsilonFreeFirsts);
    }

    return YyStatus;
}

PULONG
YypGenerateEpsilonFreeFirstSet (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine creates the grid of bits that is the Epsilon Free First set.
    This is for every non-terminal (row), the set of terminals that appear
    first in that item set. The epsilon free part means that the non-terminals
    that consist of the empty set are not included.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    Returns a pointer to the 2 dimensional bitmap containing the Epsilon Free
    First sets.

    NULL on allocation failure.

--*/

{

    UINTN AllocationSize;
    PULONG EpsilonFreeFirsts;
    YY_VALUE LeftSide;
    PULONG Row;
    ULONG RowSize;
    PYYGEN_RULE Rule;
    YY_RULE_INDEX RuleIndex;
    YY_VALUE Symbol;
    YY_VALUE SymbolIndex;

    RowSize = YYGEN_BITMAP_WORD_COUNT(Context->NonTerminalCount);
    AllocationSize = Context->NonTerminalCount * RowSize * sizeof(ULONG);
    EpsilonFreeFirsts = YypAllocate(AllocationSize);
    if (EpsilonFreeFirsts == NULL) {
        return NULL;
    }

    Row = EpsilonFreeFirsts;

    //
    // Loop through all the productions.
    //

    for (SymbolIndex = Context->StartSymbol;
         SymbolIndex < Context->SymbolCount;
         SymbolIndex += 1) {

        //
        // Loop through each rule in this production.
        //

        RuleIndex = Context->Derives[SymbolIndex];
        Rule = Context->Rules + RuleIndex;
        LeftSide = Rule->LeftSide;
        do {

            //
            // If the first symbol in the right hand side is a non-terminal,
            // add it to the bitmap for this row.
            //

            Symbol = Context->Items[Rule->RightSide];
            if (Symbol >= Context->TokenCount) {
                Symbol -= Context->StartSymbol;
                YYGEN_BITMAP_SET(Row, Symbol);
            }

            Rule += 1;

        } while (Rule->LeftSide == LeftSide);

        Row += RowSize;
    }

    YypGenerateReflexiveTransitiveClosure(EpsilonFreeFirsts,
                                          Context->NonTerminalCount);

    if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
        YypPrintEpsilonFreeFirsts(Context, EpsilonFreeFirsts);
    }

    return EpsilonFreeFirsts;
}

VOID
YypGenerateReflexiveTransitiveClosure (
    PULONG Bitmap,
    ULONG BitCount
    )

/*++

Routine Description:

    This routine generates the reflexive transitive closure of the given bitmap.
    This is just the transitive closure of the bitmap, but with each row's bit
    for itself also set.

Arguments:

    Bitmap - Supplies a pointer to the bitmap. The reflexive transitive closure
        will be returned in this bitmap.

    BitCount - Supplies the number of bits per row (and column).

Return Value:

    None.

--*/

{

    ULONG BitIndex;
    PULONG End;
    PULONG Row;
    ULONG RowSize;

    YypGenerateTransitiveClosure(Bitmap, BitCount);
    RowSize = YYGEN_BITMAP_WORD_COUNT(BitCount);
    End = Bitmap + (BitCount * RowSize);
    Row = Bitmap;
    BitIndex = 0;

    //
    // Mark the diagonal down the grid of bits to make the closure reflexive.
    //

    while (Row < End) {
        *Row |= 1 << BitIndex;
        BitIndex += 1;
        if (BitIndex == YYGEN_BITS_PER_WORD) {
            BitIndex = 0;
            Row += 1;
        }

        Row += RowSize;
    }

    return;
}

VOID
YypGenerateTransitiveClosure (
    PULONG Bitmap,
    ULONG BitCount
    )

/*++

Routine Description:

    This routine generates the transitive closure of the given bitmap using
    Warshall's algorithm.

Arguments:

    Bitmap - Supplies a pointer to the bitmap. The transitive closure will be
        returned in this bitmap.

    BitCount - Supplies the number of bits per row (and column).

Return Value:

    None.

--*/

{

    ULONG BitIndex;
    PULONG Copy;
    PULONG CurrentColumn;
    PULONG CurrentWord;
    PULONG End;
    PULONG RowEnd;
    PULONG RowI;
    PULONG RowJ;
    ULONG RowSize;

    //
    // Warshall's algorithm for the transitive closure is basically this for
    // a grid of R[row, column]:
    // for i in 0..n:
    //     for j in 0..n:
    //         for k in 0..n:
    //           R[j, k] |= R[j, i] && R[i, k];
    //

    RowSize = YYGEN_BITMAP_WORD_COUNT(BitCount);
    End = Bitmap + (BitCount * RowSize);
    CurrentWord = Bitmap;
    BitIndex = 0;
    RowI = Bitmap;

    //
    // Loop through one dimension of the bitmap in i. The current word and bit
    // index increment through the bits in that row.
    //

    while (RowI < End) {
        CurrentColumn = CurrentWord;
        RowJ = Bitmap;

        //
        // Loop j over every row in the bitmap.
        //

        while (RowJ < End) {

            //
            // Check to see if R[j, i] is set, and OR row I into row J if so.
            //

            if ((*CurrentColumn & (1 << BitIndex)) != 0) {
                Copy = RowI;
                RowEnd = RowJ + RowSize;
                while (RowJ < RowEnd) {
                    *RowJ |= *Copy;
                    RowJ += 1;
                    Copy += 1;
                }

            } else {
                RowJ += RowSize;
            }

            CurrentColumn += RowSize;
        }

        BitIndex += 1;
        if (BitIndex == YYGEN_BITS_PER_WORD) {
            BitIndex = 0;
            CurrentWord += 1;
        }

        RowI += RowSize;
    }

    return;
}

YY_STATUS
YypInitializeStates (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    )

/*++

Routine Description:

    This routine sets up the initial states of the LR(0) state machine
    generator.

Arguments:

    Context - Supplies a pointer to the generator context.

    StateContext - Supplies a pointer to the zeroed out state context.

Return Value:

    YY status.

--*/

{

    YY_VALUE LeftSide;
    PYYGEN_RULE Rule;
    YY_RULE_INDEX RuleCount;
    YY_RULE_INDEX StartDerives;
    PYYGEN_STATE State;

    StartDerives = Context->Derives[Context->StartSymbol];

    assert(StartDerives >= 0);

    Rule = &(Context->Rules[StartDerives]);
    LeftSide = Rule->LeftSide;
    RuleCount = 0;
    do {
        Rule += 1;
        RuleCount += 1;

    } while (Rule->LeftSide == LeftSide);

    State = YypAllocate(sizeof(YYGEN_STATE) + (RuleCount * sizeof(YY_VALUE)));
    if (State == NULL) {
        return YyStatusNoMemory;
    }

    State->ItemsCount = RuleCount;
    State->Items = (PYY_VALUE)(State + 1);
    Rule = &(Context->Rules[StartDerives]);
    RuleCount = 0;
    do {
        State->Items[RuleCount] = Rule->RightSide;
        Rule += 1;
        RuleCount += 1;

    } while (Rule->LeftSide == LeftSide);

    Context->FirstState = State;
    Context->StateCount = 1;
    StateContext->LastState = State;
    StateContext->CurrentState = State;
    return YyStatusSuccess;
}

YY_STATUS
YypSaveReductions (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    )

/*++

Routine Description:

    This routine examines the current itemsets and converts any empty ones
    into reductions for the current state.

Arguments:

    Context - Supplies a pointer to the generator context.

    StateContext - Supplies a pointer to the zeroed out state context.

Return Value:

    YY status.

--*/

{

    UINTN AllocationSize;
    PYY_RULE_INDEX Destination;
    PYY_RULE_INDEX End;
    YY_VALUE Item;
    PYY_ITEM_INDEX ItemSet;
    YY_VALUE ReductionCount;
    PYYGEN_REDUCTIONS Reductions;
    PYY_RULE_INDEX Source;

    //
    // Loop through all the itemsets for this state. If any are currently at
    // the end, that's a reduction.
    //

    ReductionCount = 0;
    ItemSet = Context->ItemSet;
    while (ItemSet < Context->ItemSetEnd) {
        Item = Context->Items[*ItemSet];
        if (Item < 0) {
            StateContext->ReduceSet[ReductionCount] = (YY_RULE_INDEX)-Item;
            ReductionCount += 1;
        }

        ItemSet += 1;
    }

    if (ReductionCount == 0) {
        return YyStatusSuccess;
    }

    AllocationSize = sizeof(YYGEN_REDUCTIONS) +
                     (ReductionCount * sizeof(YY_RULE_INDEX));

    Reductions = YypAllocate(AllocationSize);
    if (Reductions == NULL) {
        return YyStatusNoMemory;
    }

    Reductions->Number = StateContext->CurrentState->Number;
    Reductions->Count = ReductionCount;
    Reductions->Rules = (PYY_RULE_INDEX)(Reductions + 1);
    Source = StateContext->ReduceSet;
    End = Source + ReductionCount;
    Destination = Reductions->Rules;
    while (Source < End) {
        *Destination = *Source;
        Destination += 1;
        Source += 1;
    }

    //
    // Add it to the end of the list.
    //

    if (Context->LastReduction != NULL) {
        Context->LastReduction->Next = Reductions;
        Context->LastReduction = Reductions;

    } else {
        Context->FirstReduction = Reductions;
        Context->LastReduction = Reductions;
    }

    return YyStatusSuccess;
}

VOID
YypAdvanceItemSets (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    )

/*++

Routine Description:

    This routine creates the set of possible shift symbols and for each symbol
    determines the new item sets (kernel) in that next state.

Arguments:

    Context - Supplies a pointer to the generator context.

    StateContext - Supplies a pointer to the zeroed out state context.

Return Value:

    YY status.

--*/

{

    PYY_ITEM_INDEX CurrentKernel;
    YY_ITEM_INDEX ItemIndex;
    PYY_ITEM_INDEX ItemSet;
    YY_VALUE ShiftCount;
    YY_VALUE Symbol;

    for (Symbol = 0; Symbol < Context->SymbolCount; Symbol += 1) {
        StateContext->KernelEnd[Symbol] = NULL;
    }

    //
    // Loop across all the right hand sides for this state.
    //

    ShiftCount = 0;
    ItemSet = Context->ItemSet;
    while (ItemSet < Context->ItemSetEnd) {
        ItemIndex = *ItemSet;
        ItemSet += 1;

        //
        // If the first symbol in this right hand side is not the end and is
        // not EOF, then add it as a next item set for this symbol.
        //

        Symbol = Context->Items[ItemIndex];
        if (Symbol > 0) {
            CurrentKernel = StateContext->KernelEnd[Symbol];

            //
            // If this symbol has never been added before, then it's a new
            // shift possibility out of the current state.
            //

            if (CurrentKernel == NULL) {
                StateContext->ShiftSymbol[ShiftCount] = Symbol;
                ShiftCount += 1;
                CurrentKernel = StateContext->KernelBase[Symbol];
            }

            *CurrentKernel = ItemIndex + 1;
            CurrentKernel += 1;
            StateContext->KernelEnd[Symbol] = CurrentKernel;
        }
    }

    StateContext->ShiftCount = ShiftCount;
    return;
}

VOID
YypAddNewStates (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    )

/*++

Routine Description:

    This routine adds the new states spun out from advancing the item sets on
    the current state.

Arguments:

    Context - Supplies a pointer to the generator context.

    StateContext - Supplies a pointer to the zeroed out state context.

Return Value:

    YY status.

--*/

{

    YY_VALUE Search;
    YY_VALUE ShiftIndex;
    PYY_VALUE ShiftSymbol;
    YY_VALUE Symbol;

    ShiftSymbol = StateContext->ShiftSymbol;

    //
    // Sort the shift symbols.
    //

    for (ShiftIndex = 1;
         ShiftIndex < StateContext->ShiftCount;
         ShiftIndex += 1) {

        Symbol = ShiftSymbol[ShiftIndex];
        Search = ShiftIndex;
        while ((Search > 0) && (ShiftSymbol[Search - 1] > Symbol)) {
            ShiftSymbol[Search] = ShiftSymbol[Search - 1];
            Search -= 1;
        }

        ShiftSymbol[Search] = Symbol;
    }

    //
    // Find or add states for all new shift possibilities.
    //

    for (ShiftIndex = 0;
         ShiftIndex < StateContext->ShiftCount;
         ShiftIndex += 1) {

        Symbol = ShiftSymbol[ShiftIndex];
        StateContext->ShiftSet[ShiftIndex] =
                                    YypGetState(Context, StateContext, Symbol);

        if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
            printf("State %d -> %d via %s\n",
                   StateContext->CurrentState->Number,
                   StateContext->ShiftSet[ShiftIndex],
                   Context->Elements[Symbol].Name);
        }
    }

    return;
}

YY_STATE_INDEX
YypGetState (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext,
    YY_VALUE Symbol
    )

/*++

Routine Description:

    This routine finds or creates a state based on the incoming shift symbol
    and the item sets in that state.

Arguments:

    Context - Supplies a pointer to the generator context.

    StateContext - Supplies a pointer to the zeroed out state context.

    Symbol - Supplies a pointer to the shift symbol to get or create the
        state for.

Return Value:

    returns the state number for the new state.

--*/

{

    PYY_ITEM_INDEX CurrentKernel;
    BOOL Found;
    ULONG ItemSetCount;
    PYY_ITEM_INDEX KernelEnd;
    YY_ITEM_INDEX Key;
    PYYGEN_STATE State;
    PYY_ITEM_INDEX StateItemSet;

    CurrentKernel = StateContext->KernelBase[Symbol];
    KernelEnd = StateContext->KernelEnd[Symbol];

    assert(KernelEnd != NULL);

    ItemSetCount = KernelEnd - CurrentKernel;

    assert(ItemSetCount != 0);

    //
    // The hash table of states is keyed off of the item index of the first
    // item set in the state.
    //

    Key = *CurrentKernel;
    State = StateContext->StateSet[Key];
    if (State != NULL) {
        Found = FALSE;
        do {

            //
            // Perform a quick check against the item count. If it matches, do
            // a deeper check to see if the states contain the same item sets.
            //

            if (State->ItemsCount == ItemSetCount) {
                Found = TRUE;
                CurrentKernel = StateContext->KernelBase[Symbol];
                StateItemSet = State->Items;
                while (CurrentKernel < KernelEnd) {
                    if (*CurrentKernel != *StateItemSet) {
                        Found = FALSE;
                        break;
                    }

                    CurrentKernel += 1;
                    StateItemSet += 1;
                }
            }

            //
            // If this state wasn't it, get the next state in the hash bucket
            // by following the link. If there are no more links, then add
            // this as a new state.
            //

            if (Found == FALSE) {
                if (State->Link != NULL) {
                    State = State->Link;

                } else {
                    State = YypCreateState(Context, StateContext, Symbol);
                    if (State == NULL) {
                        return -1;
                    }

                    State->Link = State;
                    Found = TRUE;
                }
            }

        } while (Found == FALSE);

    } else {
        State = YypCreateState(Context, StateContext, Symbol);
        if (State == NULL) {
            return -1;
        }

        StateContext->StateSet[Key] = State;
    }

    return State->Number;
}

PYYGEN_STATE
YypCreateState (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext,
    YY_VALUE Symbol
    )

/*++

Routine Description:

    This routine creates a state for the current item set.

Arguments:

    Context - Supplies a pointer to the generator context.

    StateContext - Supplies a pointer to the zeroed out state context.

    Symbol - Supplies a pointer to the shift symbol to create the state for.

Return Value:

    returns a pointer to the newly created state on success.

    NULL on allocation failure.

--*/

{

    ULONG Count;
    PYY_ITEM_INDEX Destination;
    PYY_ITEM_INDEX End;
    PYY_ITEM_INDEX ItemSet;
    PYYGEN_STATE State;

    if (Context->StateCount >= YY_MAX_STATES) {
        return NULL;
    }

    ItemSet = StateContext->KernelBase[Symbol];
    End = StateContext->KernelEnd[Symbol];
    Count = End - ItemSet;
    State = YypAllocate(sizeof(YYGEN_STATE) + (Count * sizeof(YY_ITEM_INDEX)));
    if (State == NULL) {
        return NULL;
    }

    State->Items = (PYY_ITEM_INDEX)(State + 1);
    State->AccessingSymbol = Symbol;
    State->Number = Context->StateCount;
    Context->StateCount += 1;
    State->ItemsCount = Count;
    Destination = State->Items;
    while (ItemSet < End) {
        *Destination = *ItemSet;
        ItemSet += 1;
        Destination += 1;
    }

    StateContext->LastState->Next = State;
    StateContext->LastState = State;
    return State;
}

YY_STATUS
YypSaveShifts (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE_CONTEXT StateContext
    )

/*++

Routine Description:

    This routine saves the shift symbols for the current state.

Arguments:

    Context - Supplies a pointer to the generator context.

    StateContext - Supplies a pointer to the zeroed out state context.

Return Value:

    YY Status code.

--*/

{

    ULONG AllocationSize;
    PYY_STATE_INDEX Destination;
    PYY_STATE_INDEX End;
    PYYGEN_SHIFTS Shifts;
    PYY_STATE_INDEX Source;

    AllocationSize = sizeof(YYGEN_SHIFTS) +
                     (StateContext->ShiftCount * sizeof(YY_VALUE));

    Shifts = YypAllocate(AllocationSize);
    if (Shifts == NULL) {
        return YyStatusNoMemory;
    }

    Shifts->States = (PYY_STATE_INDEX)(Shifts + 1);
    Shifts->Number = StateContext->CurrentState->Number;
    Shifts->Count = StateContext->ShiftCount;
    Source = StateContext->ShiftSet;
    End = Source + StateContext->ShiftCount;
    Destination = Shifts->States;
    while (Source < End) {
        *Destination = *Source;
        Destination += 1;
        Source += 1;
    }

    if (Context->LastShift != NULL) {
        Context->LastShift->Next = Shifts;

    } else {
        Context->FirstShift = Shifts;
    }

    Context->LastShift = Shifts;
    return YyStatusSuccess;
}

VOID
YypPrintItems (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine prints the items array.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    None.

--*/

{

    YY_ITEM_INDEX Index;
    YY_VALUE Value;

    printf("\nItems:\n");
    for (Index = 0; Index < Context->ItemCount; Index += 1) {
        Value = Context->Items[Index];
        if (Value >= 0) {
            printf("    %d: %s\n", Index, Context->Elements[Value].Name);

        } else {
            printf("    %d: Rule %d (%s)\n",
                   Index,
                   -Value,
                   Context->Elements[Context->Rules[-Value].LeftSide].Name);
        }
    }

    return;
}

VOID
YypPrintDerives (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine prints the derives to standard out for debugging purposes.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    None.

--*/

{

    YY_VALUE Index;
    YY_VALUE LeftSide;
    PYYGEN_RULE Rule;
    YY_RULE_INDEX RuleIndex;

    printf("\nDerives:\n");
    for (Index = Context->StartSymbol;
         Index < Context->SymbolCount;
         Index += 1) {

        printf("%s derives ", Context->Elements[Index].Name);
        RuleIndex = Context->Derives[Index];
        Rule = &(Context->Rules[RuleIndex]);
        LeftSide = Rule->LeftSide;
        while (Rule->LeftSide == LeftSide) {
            printf("  %d", RuleIndex);
            RuleIndex += 1;
            Rule += 1;
        }

        printf("\n");
    }

    printf("\n");
    return;
}

VOID
YypPrintEpsilonFreeFirsts (
    PYYGEN_CONTEXT Context,
    PULONG EpsilonFreeFirsts
    )

/*++

Routine Description:

    This routine prints the set of epsilon-free FIRSTs.

Arguments:

    Context - Supplies a pointer to the generator context.

    EpsilonFreeFirsts - Supplies a pointer to the array of bitmaps that are the
        Epsilon-Free firsts.

Return Value:

    None.

--*/

{

    ULONG Bit;
    YY_VALUE BitIndex;
    YY_VALUE Index;
    PULONG Row;
    ULONG RowSize;
    ULONG Word;

    RowSize = YYGEN_BITMAP_WORD_COUNT(Context->NonTerminalCount);
    printf("\nEpsilon Free Firsts:\n");
    for (Index = Context->StartSymbol;
         Index < Context->SymbolCount;
         Index += 1) {

        printf("\n%s", Context->Elements[Index].Name);
        Row = EpsilonFreeFirsts + ((Index - Context->StartSymbol) * RowSize);
        Word = *Row;
        Bit = 0;
        for (BitIndex = 0;
             BitIndex < Context->NonTerminalCount;
             BitIndex += 1) {

            if ((Word & (1 << Bit)) != 0) {
                printf("  %s",
                       Context->Elements[Context->StartSymbol + BitIndex].Name);
            }

            Bit += 1;
            if (Bit == YYGEN_BITS_PER_WORD) {
                Bit = 0;
                Row += 1;
                Word = *Row;
            }
        }
    }

    return;
}

VOID
YypPrintFirstDerives (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine prints the set of first derives.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    None.

--*/

{

    ULONG Bit;
    YY_VALUE Index;
    PULONG Row;
    ULONG RowSize;
    YY_RULE_INDEX RuleIndex;
    YY_VALUE StartSymbol;
    ULONG Word;

    RowSize = YYGEN_BITMAP_WORD_COUNT(Context->RuleCount);
    StartSymbol = Context->StartSymbol;
    printf("\n\nFirst Derives:\n");
    for (Index = StartSymbol; Index < Context->SymbolCount; Index += 1) {
        printf("\n %s derives\n", Context->Elements[Index].Name);
        Row = Context->FirstDerives + ((Index - StartSymbol) * RowSize);
        Bit = 0;
        Word = *Row;
        for (RuleIndex = 0; RuleIndex <= Context->RuleCount; RuleIndex += 1) {
            if ((Word & (1 << Bit)) != 0) {
                printf("   %d\n", RuleIndex);
            }

            Bit += 1;
            if (Bit == YYGEN_BITS_PER_WORD) {
                Bit = 0;
                Row += 1;
                Word = *Row;
            }
        }
    }

    return;
}

VOID
YypPrintClosure (
    PYYGEN_CONTEXT Context,
    YY_VALUE ItemsCount
    )

/*++

Routine Description:

    This routine prints the set of first derives.

Arguments:

    Context - Supplies a pointer to the generator context.

    ItemsCount - Supplies the number of item sets in the state.

Return Value:

    None.

--*/

{

    PYY_ITEM_INDEX ItemSet;

    printf("\nn = %d\n", ItemsCount);
    ItemSet = Context->ItemSet;
    while (ItemSet < Context->ItemSetEnd) {
        printf("    %d\n", *ItemSet);
        ItemSet += 1;
    }

    return;
}

