/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lalr.c

Abstract:

    This module implements the production of an LALR(1) parser from an LR(0)
    state machine.

Author:

    Evan Green 17-Apr-2016

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

typedef struct _YYGEN_LOOKBACK YYGEN_LOOKBACK, *PYYGEN_LOOKBACK;

/*++

Structure Description:

    This structure maps a lookahead symbol back to the list of gotos that use
    it.

Members:

    Next - Supplies a pointer to the next lookback in the set.

    Goto - Supplies the goto index the lookback is referring to.

--*/

struct _YYGEN_LOOKBACK {
    PYYGEN_LOOKBACK Next;
    YY_GOTO_INDEX Goto;
};

/*++

Structure Description:

    This structure contains the working state for the LALR generator.

Members:

    MaxRightLength - Stores the number of elements in the longest right hand
        side of any rule.

    TokenSetSize - Stores the number of 32-bit words needed to represent a
        bitmap of all the terminals.

    GotoCount - Stores the number of gotos.

    GotoFollows - Stores the FOLLOW set of gotos, which is an array of token
        bitmaps indexed by goto. It shows for any goto the set of terminals
        that can come after the destination of the goto is found.

    Infinity - Stores a value greater than any possible goto number for the
        purposes of graph traversal.

    GotoVertex - Stores an array mapping each goto index to its corresponding
        vertex. 0 is not valid.

    Vertices - Stores the array of graph vertices.

    Top - Stores the current count of graph vertices.

    Relations - Stores an array of arrays of goto indices, indicating the
        relations between graph vertices for each goto index.

    Includes - Stores the set of relations to include in the follow set,
        indexed by goto index.

    Lookback - Stores an array of optional pointers to lookback sets, running
        parallel to the lookahead sets and lookahead rule arrays. It shows
        which gotos use that lookahead.

    StartGoto - Stores the goto index associated with the start symbol.

--*/

typedef struct _YYGEN_LALR_CONTEXT {
    ULONG MaxRightLength;
    ULONG TokenSetSize;
    YY_GOTO_INDEX GotoCount;
    PULONG GotoFollows;
    YY_GOTO_INDEX Infinity;
    PYY_GOTO_INDEX GotoVertex;
    PYY_GOTO_INDEX Vertices;
    YY_GOTO_INDEX Top;
    PYY_GOTO_INDEX *Relations;
    PYY_GOTO_INDEX *Includes;
    PYYGEN_LOOKBACK *Lookback;
    YY_GOTO_INDEX StartGoto;
} YYGEN_LALR_CONTEXT, *PYYGEN_LALR_CONTEXT;

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

YY_STATUS
YypInitializeLalrContext (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    );

VOID
YypDestroyLalrContext (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    );

YY_STATUS
YypInitializeLookaheads (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    );

YY_STATUS
YypSetGotoMap (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    );

YY_STATUS
YypInitializeFollows (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    );

YY_STATUS
YypBuildRelations (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    );

YY_STATUS
YypAddLookbackEdge (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr,
    YY_STATE_INDEX State,
    YY_RULE_INDEX Rule,
    YY_GOTO_INDEX Goto
    );

YY_STATUS
YypComputeFollowSet (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    );

VOID
YypComputeLookaheads (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    );

YY_STATUS
YypBuildDigraph (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr,
    PYY_GOTO_INDEX *Relations
    );

VOID
YypTraverseDigraph (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr,
    YY_GOTO_INDEX GotoIndex
    );

PYY_GOTO_INDEX *
YypTranspose (
    PYY_GOTO_INDEX *Relations,
    YY_GOTO_INDEX Count
    );

YY_GOTO_INDEX
YypFindGoto (
    PYYGEN_CONTEXT Context,
    YY_STATE_INDEX State,
    YY_VALUE Symbol
    );

VOID
YypPrintGotoMap (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    );

VOID
YypPrintTokenBitmapArray (
    PYYGEN_CONTEXT Context,
    PULONG BitmapArray,
    YY_VALUE Count
    );

VOID
YypPrintIncludes (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

YY_STATUS
YypGenerateLalr (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine generates an LALR(1) state machine based on an LR(0) state
    machine.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    YY status.

--*/

{

    YYGEN_LALR_CONTEXT Lalr;
    YY_STATUS YyStatus;

    memset(&Lalr, 0, sizeof(YYGEN_LALR_CONTEXT));
    YyStatus = YypInitializeLalrContext(Context, &Lalr);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateLalrEnd;
    }

    YyStatus = YypSetGotoMap(Context, &Lalr);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateLalrEnd;
    }

    YyStatus = YypInitializeFollows(Context, &Lalr);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateLalrEnd;
    }

    YyStatus = YypBuildRelations(Context, &Lalr);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateLalrEnd;
    }

    YyStatus = YypComputeFollowSet(Context, &Lalr);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateLalrEnd;
    }

    YypComputeLookaheads(Context, &Lalr);

GenerateLalrEnd:
    YypDestroyLalrContext(Context, &Lalr);
    return YyStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

YY_STATUS
YypInitializeLalrContext (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    )

/*++

Routine Description:

    This routine allocates arrays needed for LALR generation.

Arguments:

    Context - Supplies a pointer to the generator context.

    Lalr - Supplies a pointer to the zeroed out LALR context.

Return Value:

    YY status.

--*/

{

    PYY_ITEM_INDEX Items;
    PYY_ITEM_INDEX ItemsEnd;
    PYYGEN_REDUCTIONS Reductions;
    ULONG RuleLength;
    PYYGEN_SHIFTS Shifts;
    PYYGEN_STATE State;
    YY_STATE_INDEX StateCount;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;
    Lalr->TokenSetSize = YYGEN_BITMAP_WORD_COUNT(Context->TokenCount);

    //
    // Create flattened arrays of states, accessing symbols, shifts, and
    // reductions indexed by state.
    //

    StateCount = Context->StateCount;
    Context->StateTable = YypAllocate(StateCount * sizeof(PYYGEN_STATE));
    if (Context->StateTable == NULL) {
        goto InitializeLalrContextEnd;
    }

    Context->AccessingSymbol = YypAllocate(StateCount * sizeof(YY_VALUE));
    if (Context->AccessingSymbol == NULL) {
        goto InitializeLalrContextEnd;
    }

    Context->ShiftTable = YypAllocate(StateCount * sizeof(PYYGEN_SHIFTS));
    if (Context->ShiftTable == NULL) {
        goto InitializeLalrContextEnd;
    }

    Context->ReductionTable =
                            YypAllocate(StateCount * sizeof(YYGEN_REDUCTIONS));

    if (Context->ReductionTable == NULL) {
        goto InitializeLalrContextEnd;
    }

    State = Context->FirstState;
    StateCount = 0;
    while (State != NULL) {
        Context->StateTable[State->Number] = State;
        Context->AccessingSymbol[State->Number] = State->AccessingSymbol;
        State = State->Next;
        StateCount += 1;
    }

    assert(StateCount == Context->StateCount);

    Shifts = Context->FirstShift;
    while (Shifts != NULL) {
        Context->ShiftTable[Shifts->Number] = Shifts;
        Shifts = Shifts->Next;
    }

    Reductions = Context->FirstReduction;
    while (Reductions != NULL) {
        Context->ReductionTable[Reductions->Number] = Reductions;
        Reductions = Reductions->Next;
    }

    //
    // Figure out the maximum right hand side length.
    //

    Items = Context->Items;
    ItemsEnd = Context->Items + Context->ItemCount;
    RuleLength = 0;
    while (Items < ItemsEnd) {
        if (*Items >= 0) {
            RuleLength += 1;

        } else {
            if (RuleLength > Lalr->MaxRightLength) {
                Lalr->MaxRightLength = RuleLength;
            }

            RuleLength = 0;
        }

        Items += 1;
    }

    YyStatus = YypInitializeLookaheads(Context, Lalr);
    if (YyStatus != YyStatusSuccess) {
        goto InitializeLalrContextEnd;
    }

    YyStatus = YyStatusSuccess;

InitializeLalrContextEnd:
    return YyStatus;
}

VOID
YypDestroyLalrContext (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    )

/*++

Routine Description:

    This routine frees memory allocated in the given LALR context.

Arguments:

    Context - Supplies a pointer to the application context.

    Lalr - Supplies a pointer to the LALR context.

Return Value:

    None.

--*/

{

    YY_VALUE Count;
    YY_VALUE Index;
    PYYGEN_LOOKBACK Lookback;
    PYYGEN_LOOKBACK Next;

    if (Lalr->GotoFollows != NULL) {
        YypFree(Lalr->GotoFollows);
    }

    if (Lalr->GotoVertex != NULL) {
        YypFree(Lalr->GotoVertex);
    }

    if (Lalr->Vertices != NULL) {
        YypFree(Lalr->Vertices);
    }

    assert(Lalr->Relations == NULL);

    if (Lalr->Includes != NULL) {
        for (Index = 0; Index < Lalr->GotoCount; Index += 1) {
            if (Lalr->Includes[Index] != NULL) {
                YypFree(Lalr->Includes[Index]);
            }
        }

        YypFree(Lalr->Includes);
    }

    if (Lalr->Lookback != NULL) {
        Count = Context->Lookaheads[Context->StateCount];
        for (Index = 0; Index < Count; Index += 1) {
            Lookback = Lalr->Lookback[Index];
            while (Lookback != NULL) {
                Next = Lookback->Next;
                YypFree(Lookback);
                Lookback = Next;
            }
        }

        YypFree(Lalr->Lookback);
    }

    return;
}

YY_STATUS
YypInitializeLookaheads (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    )

/*++

Routine Description:

    This routine allocates the lookahead arrays and initializes parts of them.

Arguments:

    Context - Supplies a pointer to the generator context.

    Lalr - Supplies a pointer to the LALR context.

Return Value:

    YY status.

--*/

{

    ULONG Count;
    YY_RULE_INDEX ReductionIndex;
    PYYGEN_REDUCTIONS Reductions;
    YY_STATE_INDEX State;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;
    Context->Lookaheads =
                     YypAllocate((Context->StateCount + 1) * sizeof(YY_VALUE));

    if (Context->Lookaheads == NULL) {
        goto InitializeLookaheadsEnd;
    }

    //
    // Set indices and count how many total reductions there are.
    //

    Count = 0;
    for (State = 0; State < Context->StateCount; State += 1) {
        Context->Lookaheads[State] = Count;
        Reductions = Context->ReductionTable[State];
        if (Reductions != NULL) {
            Count += Reductions->Count;
        }
    }

    Context->Lookaheads[State] = Count;
    Context->LookaheadSets =
                     YypAllocate((Count * Lalr->TokenSetSize) * sizeof(ULONG));

    if (Context->LookaheadSets == NULL) {
        goto InitializeLookaheadsEnd;
    }

    Context->LookaheadRule = YypAllocate(Count * sizeof(YY_RULE_INDEX));
    if (Context->LookaheadRule == NULL) {
        goto InitializeLookaheadsEnd;
    }

    Lalr->Lookback = YypAllocate(Count * sizeof(PYYGEN_LOOKBACK));
    if (Lalr->Lookback == NULL) {
        goto InitializeLookaheadsEnd;
    }

    //
    // Initialize the lookahead rule numbers.
    //

    Count = 0;
    for (State = 0; State < Context->StateCount; State += 1) {
        Context->Lookaheads[State] = Count;
        Reductions = Context->ReductionTable[State];
        if (Reductions != NULL) {
            for (ReductionIndex = 0;
                 ReductionIndex < Reductions->Count;
                 ReductionIndex += 1) {

                Context->LookaheadRule[Count] =
                                             Reductions->Rules[ReductionIndex];

                Count += 1;
            }
        }
    }

    YyStatus = YyStatusSuccess;

InitializeLookaheadsEnd:
    return YyStatus;
}

YY_STATUS
YypSetGotoMap (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    )

/*++

Routine Description:

    This routine expands the shifts of each state out into gotos.

Arguments:

    Context - Supplies a pointer to the generator context.

    Lalr - Supplies a pointer to the LALR context.

Return Value:

    YY status.

--*/

{

    YY_STATE_INDEX DestinationState;
    YY_VALUE Goal;
    YY_GOTO_INDEX GotoIndex;
    PYY_GOTO_INDEX GotoMap;
    YY_VALUE ShiftIndex;
    PYYGEN_SHIFTS Shifts;
    YY_VALUE Symbol;
    YY_VALUE TokenCount;
    PYY_GOTO_INDEX WorkingMap;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;
    GotoMap = NULL;
    TokenCount = Context->TokenCount;

    //
    // Allocate the goto map array and a working array used during construction.
    // Allocate an extra slot because many functions use GotoMap[myindex + 1] as
    // their terminating bound.
    //

    WorkingMap = YypAllocate(
                      (Context->NonTerminalCount + 1) * sizeof(YY_GOTO_INDEX));

    if (WorkingMap == NULL) {
        goto SetGotoMapEnd;
    }

    GotoMap = YypAllocate(
                      (Context->NonTerminalCount + 1) * sizeof(YY_GOTO_INDEX));

    if (GotoMap == NULL) {
        goto SetGotoMapEnd;
    }

    //
    // The gotos are created by going through all the shifts. Start by counting
    // them, and figuring the size for each symbol bucket.
    //

    Lalr->GotoCount = 0;
    Shifts = Context->FirstShift;
    while (Shifts != NULL) {

        //
        // Go backwards to get all the non-terminals only.
        //

        for (ShiftIndex = Shifts->Count - 1; ShiftIndex >= 0; ShiftIndex -= 1) {
            DestinationState = Shifts->States[ShiftIndex];
            Symbol = Context->AccessingSymbol[DestinationState];
            if (Symbol < TokenCount) {
                break;
            }

            if (Lalr->GotoCount >= YY_MAX_GOTOS) {
                YyStatus = YyStatusTooManyItems;
                goto SetGotoMapEnd;
            }

            Lalr->GotoCount += 1;

            //
            // Count for each shift symbol how many gotos transition on it.
            //

            GotoMap[Symbol - TokenCount] += 1;
        }

        Shifts = Shifts->Next;
    }

    //
    // Now convert those counts into indices into one big array. The working
    // map will be the current index for a symbol array.
    //

    GotoIndex = 0;
    for (Symbol = 0; Symbol < Context->NonTerminalCount; Symbol += 1) {
        WorkingMap[Symbol] = GotoIndex;
        GotoIndex += GotoMap[Symbol];
    }

    //
    // Copy it to the goto map.
    //

    for (Symbol = 0; Symbol < Context->NonTerminalCount; Symbol += 1) {
        GotoMap[Symbol] = WorkingMap[Symbol];
    }

    WorkingMap[Symbol] = Lalr->GotoCount;
    GotoMap[Symbol] = Lalr->GotoCount;

    //
    // Allocate the from and to state arrays that actually describe the gotos.
    //

    assert(GotoIndex == Lalr->GotoCount);

    Context->FromState = YypAllocate(GotoIndex * sizeof(YY_STATE_INDEX));
    if (Context->FromState == NULL) {
        goto SetGotoMapEnd;
    }

    Context->ToState = YypAllocate(GotoIndex * sizeof(YY_STATE_INDEX));
    if (Context->ToState == NULL) {
        goto SetGotoMapEnd;
    }

    //
    // Now go through again and set the from and to states corresponding to the
    // shifts.
    //

    Goal = Context->Items[1];
    Shifts = Context->FirstShift;
    while (Shifts != NULL) {
        for (ShiftIndex = Shifts->Count - 1; ShiftIndex >= 0; ShiftIndex -= 1) {
            DestinationState = Shifts->States[ShiftIndex];
            Symbol = Context->AccessingSymbol[DestinationState];
            if (Symbol < TokenCount) {
                break;
            }

            GotoIndex = WorkingMap[Symbol - TokenCount];

            //
            // Remember the goto for the final state, as it needs to get an EOF
            // set in the initial follow set.
            //

            if (Symbol == Goal) {
                Lalr->StartGoto = GotoIndex;
            }

            WorkingMap[Symbol - TokenCount] += 1;
            Context->FromState[GotoIndex] = Shifts->Number;
            Context->ToState[GotoIndex] = DestinationState;
        }

        Shifts = Shifts->Next;
    }

    YyStatus = YyStatusSuccess;
    Context->GotoMap = GotoMap;
    if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
        YypPrintGotoMap(Context, Lalr);
    }

    GotoMap = NULL;

SetGotoMapEnd:
    if (GotoMap != NULL) {
        YypFree(GotoMap);
    }

    if (WorkingMap != NULL) {
        YypFree(WorkingMap);
    }

    return YyStatus;
}

YY_STATUS
YypInitializeFollows (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    )

/*++

Routine Description:

    This routine performs some initialization of the FOLLOW set.

Arguments:

    Context - Supplies a pointer to the generator context.

    Lalr - Supplies a pointer to the LALR context.

Return Value:

    YY status.

--*/

{

    ULONG CopyIndex;
    PYY_GOTO_INDEX EdgeCopy;
    ULONG EdgeCount;
    PYY_GOTO_INDEX Edges;
    YY_GOTO_INDEX GotoIndex;
    PYY_GOTO_INDEX *Reads;
    PULONG Row;
    YY_VALUE ShiftCount;
    ULONG ShiftIndex;
    PYYGEN_SHIFTS Shifts;
    YY_STATE_INDEX State;
    YY_VALUE Symbol;
    ULONG WordCount;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;
    Edges = NULL;
    Reads = NULL;
    WordCount = Lalr->GotoCount * Lalr->TokenSetSize;
    Lalr->GotoFollows = YypAllocate(WordCount * sizeof(ULONG));
    if (Lalr->GotoFollows == NULL) {
        goto InitializeFirstsEnd;
    }

    Reads = YypAllocate(Lalr->GotoCount * sizeof(PYY_GOTO_INDEX));
    if (Reads == NULL) {
        goto InitializeFirstsEnd;
    }

    Edges = YypAllocate(Lalr->GotoCount * sizeof(YY_GOTO_INDEX));
    if (Edges == NULL) {
        goto InitializeFirstsEnd;
    }

    //
    // Loop through every goto, initializing the token bitmap for that row in
    // the lookahead graph.
    //

    EdgeCount = 0;
    Row = Lalr->GotoFollows;
    for (GotoIndex = 0; GotoIndex < Lalr->GotoCount; GotoIndex += 1) {

        //
        // Go through all the shifts out of the destination state. Add the
        // shift tokens that shift out of the destination state to the bitmap.
        //

        State = Context->ToState[GotoIndex];
        Shifts = Context->ShiftTable[State];
        if (Shifts != NULL) {
            ShiftCount = Shifts->Count;
            for (ShiftIndex = 0; ShiftIndex < ShiftCount; ShiftIndex += 1) {

                //
                // Get the symbol that shifts out of the state.
                //

                Symbol = Context->AccessingSymbol[Shifts->States[ShiftIndex]];

                //
                // Stop at the first non-terminal.
                //

                if (Symbol >= Context->TokenCount) {
                    break;
                }

                YYGEN_BITMAP_SET(Row, Symbol);
            }

            //
            // Now for the non-terminal shifts. For those non-terminal shifts
            // that are nullable (empty), add the goto edge out of that state
            // to the current list of edges, since they will have to be
            // traversed through to find the actual first symbol.
            //

            while (ShiftIndex < ShiftCount) {
                Symbol = Context->AccessingSymbol[Shifts->States[ShiftIndex]];

                assert(Symbol >= Context->TokenCount);

                if (Context->Nullable[Symbol] != FALSE) {
                    Edges[EdgeCount] = YypFindGoto(Context, State, Symbol);
                    EdgeCount += 1;
                }

                ShiftIndex += 1;
            }

            //
            // If there were any edges, add the edges to the list of edges to
            // traverse.
            //

            if (EdgeCount != 0) {
                EdgeCopy = YypAllocate((EdgeCount + 1) * sizeof(YY_GOTO_INDEX));
                if (EdgeCopy == NULL) {
                    goto InitializeFirstsEnd;
                }

                Reads[GotoIndex] = EdgeCopy;
                for (CopyIndex = 0; CopyIndex < EdgeCount; CopyIndex += 1) {
                    EdgeCopy[CopyIndex] = Edges[CopyIndex];
                }

                EdgeCopy[EdgeCount] = -1;
                EdgeCount = 0;
            }
        }

        Row += Lalr->TokenSetSize;
    }

    //
    // The goto for the starting symbol is followed by EOF.
    //

    Row = Lalr->GotoFollows + (Lalr->StartGoto * Lalr->TokenSetSize);
    YYGEN_BITMAP_SET(Row, 0);

    //
    // Traverse through the empty states to figure out the terminals that
    // follow after that.
    //

    YyStatus = YypBuildDigraph(Context, Lalr, Reads);
    if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
        printf("\nInitial Follows:");
        YypPrintTokenBitmapArray(Context, Lalr->GotoFollows, Lalr->GotoCount);
    }

InitializeFirstsEnd:
    for (GotoIndex = 0; GotoIndex < Lalr->GotoCount; GotoIndex += 1) {
        if (Reads[GotoIndex] != NULL) {
            YypFree(Reads[GotoIndex]);
        }
    }

    if (Reads != NULL) {
        YypFree(Reads);
    }

    if (Edges != NULL) {
        YypFree(Edges);
    }

    return YyStatus;
}

YY_STATUS
YypBuildRelations (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    )

/*++

Routine Description:

    This routine builds the includes graph.

Arguments:

    Context - Supplies a pointer to the generator context.

    Lalr - Supplies a pointer to the LALR context.

Return Value:

    YY status.

--*/

{

    YY_STATE_INDEX CurrentState;
    PYY_GOTO_INDEX Destination;
    BOOL Done;
    ULONG EdgeCount;
    PYY_GOTO_INDEX Edges;
    YY_STATE_INDEX FromState;
    YY_VALUE FromSymbol;
    YY_GOTO_INDEX GotoIndex;
    PYY_VALUE Items;
    YY_VALUE LeftSide;
    ULONG Length;
    YY_VALUE RightSymbol;
    YY_RULE_INDEX RuleIndex;
    ULONG ShiftIndex;
    PYYGEN_SHIFTS Shifts;
    PYY_GOTO_INDEX Source;
    PYY_STATE_INDEX States;
    PYY_GOTO_INDEX *TransposedIncludes;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;
    Edges = NULL;
    States = NULL;
    Lalr->Includes = YypAllocate(Lalr->GotoCount * sizeof(PYY_GOTO_INDEX));
    if (Lalr->Includes == NULL) {
        goto BuildRelationsEnd;
    }

    Edges = YypAllocate((Lalr->GotoCount + 1) * sizeof(YY_GOTO_INDEX));
    if (Edges == NULL) {
        goto BuildRelationsEnd;
    }

    States = YypAllocate((Lalr->MaxRightLength + 1) * sizeof(YY_STATE_INDEX));
    if (States == NULL) {
        goto BuildRelationsEnd;
    }

    //
    // Loop through all the gotos.
    //

    for (GotoIndex = 0; GotoIndex < Lalr->GotoCount; GotoIndex += 1) {
        EdgeCount = 0;
        FromState = Context->FromState[GotoIndex];
        FromSymbol = Context->AccessingSymbol[Context->ToState[GotoIndex]];

        //
        // Loop through every rule in the production for the from state.
        //

        RuleIndex = Context->Derives[FromSymbol];
        LeftSide = Context->Rules[RuleIndex].LeftSide;
        do {
            Length = 1;
            States[0] = FromState;
            CurrentState = FromState;

            //
            // Loop through all the right hand side items for this rule.
            // Generate the array of states that represents seeing each of
            // these items.
            //

            Items = Context->Items + Context->Rules[RuleIndex].RightSide;
            while (*Items >= 0) {
                RightSymbol = *Items;

                //
                // Find the next state from the current one that is entered via
                // the current right hand side symbol.
                //

                Shifts = Context->ShiftTable[CurrentState];
                for (ShiftIndex = 0;
                     ShiftIndex < Shifts->Count;
                     ShiftIndex += 1) {

                    CurrentState = Shifts->States[ShiftIndex];
                    if (Context->AccessingSymbol[CurrentState] == RightSymbol) {
                        break;
                    }
                }

                States[Length] = CurrentState;
                Length += 1;
                Items += 1;
            }

            //
            // Add a lookback edge which says that the final state reached for
            // a particular rule corresponds to this goto.
            //

            YyStatus = YypAddLookbackEdge(Context,
                                          Lalr,
                                          CurrentState,
                                          RuleIndex,
                                          GotoIndex);

            if (YyStatus != YyStatusSuccess) {
                goto BuildRelationsEnd;
            }

            //
            // Now go through that sequence of states backwards. While the
            // last state is empty, add an edge to traverse later.
            //

            Length -= 1;
            do {
                Done = TRUE;
                Items -= 1;
                RightSymbol = *Items;
                if (RightSymbol >= Context->TokenCount) {
                    Length -= 1;
                    CurrentState = States[Length];
                    Edges[EdgeCount] = YypFindGoto(Context,
                                                   CurrentState,
                                                   RightSymbol);

                    EdgeCount += 1;
                    if ((Context->Nullable[RightSymbol] != FALSE) &&
                        (Length > 0)) {

                        Done = FALSE;
                    }
                }

            } while (Done == FALSE);

            RuleIndex += 1;

        } while (LeftSide == Context->Rules[RuleIndex].LeftSide);

        //
        // Save the edges to be traversed.
        //

        if (EdgeCount != 0) {
            Destination = YypAllocate((EdgeCount + 1) * sizeof(YY_GOTO_INDEX));
            if (Destination == NULL) {
                YyStatus = YyStatusNoMemory;
                goto BuildRelationsEnd;
            }

            Lalr->Includes[GotoIndex] = Destination;
            Source = Edges;
            while (EdgeCount != 0) {
                *Destination = *Source;
                Destination += 1;
                Source += 1;
                EdgeCount -= 1;
            }

            *Destination = -1;
        }
    }

    TransposedIncludes = YypTranspose(Lalr->Includes, Lalr->GotoCount);
    if (TransposedIncludes == NULL) {
        YyStatus = YyStatusNoMemory;
        goto BuildRelationsEnd;
    }

    //
    // Destroy the old includes and replace with the transposed one.
    //

    for (GotoIndex = 0; GotoIndex < Lalr->GotoCount; GotoIndex += 1) {
        if (Lalr->Includes[GotoIndex] != NULL) {
            YypFree(Lalr->Includes[GotoIndex]);
        }
    }

    YypFree(Lalr->Includes);
    Lalr->Includes = TransposedIncludes;
    if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
        YypPrintIncludes(Context, Lalr);
    }

    YyStatus = YyStatusSuccess;

BuildRelationsEnd:
    if (Edges != NULL) {
        YypFree(Edges);
    }

    if (States != NULL) {
        YypFree(States);
    }

    return YyStatus;
}

YY_STATUS
YypAddLookbackEdge (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr,
    YY_STATE_INDEX State,
    YY_RULE_INDEX Rule,
    YY_GOTO_INDEX Goto
    )

/*++

Routine Description:

    This routine adds a lookback edge for the given state and rule number.

Arguments:

    Context - Supplies a pointer to the generator context.

    Lalr - Supplies a pointer to the LALR context.

    State - Supplies the state index to add the lookback under.

    Rule - Supplies the rule index within the state to add the lookback under.

    Goto - Supplies the goto number adding the lookback.

Return Value:

    YY status.

--*/

{

    YY_VALUE Index;
    PYYGEN_LOOKBACK Lookback;

    for (Index = Context->Lookaheads[State];
         Index < Context->Lookaheads[State + 1];
         Index += 1) {

        if (Context->LookaheadRule[Index] == Rule) {
            break;
        }
    }

    assert(Index != Context->Lookaheads[State + 1]);

    Lookback = YypAllocate(sizeof(YYGEN_LOOKBACK));
    if (Lookback == NULL) {
        return YyStatusNoMemory;
    }

    Lookback->Goto = Goto;
    Lookback->Next = Lalr->Lookback[Index];
    Lalr->Lookback[Index] = Lookback;
    return YyStatusSuccess;
}

YY_STATUS
YypComputeFollowSet (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    )

/*++

Routine Description:

    This routine computes the FOLLOW set for all non-terminal symbols.

Arguments:

    Context - Supplies a pointer to the generator context.

    Lalr - Supplies a pointer to the LALR context.

Return Value:

    YY Status.

--*/

{

    YY_STATUS YyStatus;

    YyStatus = YypBuildDigraph(Context, Lalr, Lalr->Includes);
    if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
        printf("\nFollows:");
        YypPrintTokenBitmapArray(Context, Lalr->GotoFollows, Lalr->GotoCount);
    }

    return YyStatus;
}

VOID
YypComputeLookaheads (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    )

/*++

Routine Description:

    This routine computes lookahead set based on the FOLLOWS and relations.

Arguments:

    Context - Supplies a pointer to the generator context.

    Lalr - Supplies a pointer to the LALR context.

Return Value:

    None.

--*/

{

    YY_VALUE Count;
    PULONG Destination;
    PULONG End;
    YY_VALUE Index;
    PYYGEN_LOOKBACK Lookback;
    PULONG Row;
    PULONG Source;

    Row = Context->LookaheadSets;
    Count = Context->Lookaheads[Context->StateCount];
    for (Index = 0; Index < Count; Index += 1) {
        End = Row + Lalr->TokenSetSize;

        //
        // OR in the follow sets from the gotos in the lookbacks to this follow
        // set.
        //

        Lookback = Lalr->Lookback[Index];
        while (Lookback != NULL) {
            Destination = Row;
            Source = Lalr->GotoFollows + (Lookback->Goto * Lalr->TokenSetSize);
            while (Destination < End) {
                *Destination |= *Source;
                Destination += 1;
                Source += 1;
            }

            Lookback = Lookback->Next;
        }

        //
        // Move to the next row for the next lookahead set.
        //

        Row = End;
    }

    if ((Context->Flags & YYGEN_FLAG_DEBUG) != 0) {
        printf("\nLookaheads:");
        YypPrintTokenBitmapArray(Context,
                                 Context->LookaheadSets,
                                 Context->StateCount);
    }

    return;
}

YY_STATUS
YypBuildDigraph (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr,
    PYY_GOTO_INDEX *Relations
    )

/*++

Routine Description:

    This routine builds a directed graph from an array of edges.

Arguments:

    Context - Supplies a pointer to the generator context.

    Lalr - Supplies a pointer to the LALR context.

    Relations - Supplies the array of relations. Each element is an array of
        nodes that are reachable from that given index.

Return Value:

    YY status.

--*/

{

    YY_GOTO_INDEX GotoIndex;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;
    Lalr->Relations = Relations;
    Lalr->Infinity = Lalr->GotoCount + 2;
    if (Lalr->GotoVertex == NULL) {
        Lalr->GotoVertex =
                    YypAllocate((Lalr->GotoCount + 1) * sizeof(YY_GOTO_INDEX));

        if (Lalr->GotoVertex == NULL) {
            goto BuildDigraphEnd;
        }
    }

    if (Lalr->Vertices == NULL) {
        Lalr->Vertices =
                    YypAllocate((Lalr->GotoCount + 1) * sizeof(YY_GOTO_INDEX));

        if (Lalr->Vertices == NULL) {
            goto BuildDigraphEnd;
        }
    }

    //
    // Reset the index array to indicate no vertices have been visited.
    //

    Lalr->Top = 0;
    for (GotoIndex = 0; GotoIndex < Lalr->GotoCount; GotoIndex += 1) {
        Lalr->GotoVertex[GotoIndex] = 0;
    }

    //
    // Traverse each goto array. Watch out that the traverse function didn't
    // recurse and traverse that index itself.
    //

    for (GotoIndex = 0; GotoIndex < Lalr->GotoCount; GotoIndex += 1) {
        if ((Lalr->GotoVertex[GotoIndex] == 0) &&
            (Relations[GotoIndex] != NULL)) {

            YypTraverseDigraph(Context, Lalr, GotoIndex);
        }
    }

    YyStatus = YyStatusSuccess;

BuildDigraphEnd:
    Lalr->Relations = NULL;
    return YyStatus;
}

VOID
YypTraverseDigraph (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr,
    YY_GOTO_INDEX GotoIndex
    )

/*++

Routine Description:

    This routine traverses a vertex in the digraph.

Arguments:

    Context - Supplies a pointer to the generator context.

    Lalr - Supplies a pointer to the LALR context.

    GotoIndex - Supplies the goto index to traverse edges for.

Return Value:

    None.

--*/

{

    PULONG Base;
    PULONG Destination;
    YY_GOTO_INDEX Edge;
    PYY_GOTO_INDEX Edges;
    PULONG End;
    YY_GOTO_INDEX Height;
    PULONG Source;

    //
    // Create a new vertex for this goto.
    //

    Lalr->Top += 1;
    Lalr->Vertices[Lalr->Top] = GotoIndex;
    Lalr->GotoVertex[GotoIndex] = Lalr->Top;
    Height = Lalr->Top;

    //
    // Get the FOLLOW token bitmap for this goto index.
    //

    Base = Lalr->GotoFollows + (GotoIndex * Lalr->TokenSetSize);
    End = Base + Lalr->TokenSetSize;
    Edges = Lalr->Relations[GotoIndex];
    if (Edges != NULL) {
        while (*Edges >= 0) {
            Edge = *Edges;
            Edges += 1;

            //
            // If this is a never before explored goto, go explore it.
            //

            if (Lalr->GotoVertex[Edge] == 0) {
                YypTraverseDigraph(Context, Lalr, Edge);
            }

            //
            // If the vertex for the goto being explored is farther that the
            // destination, then set the vertex for this goto to the edges
            // vertex. Meaning if this is reachable faster, use the faster
            // route.
            //

            if (Lalr->GotoVertex[GotoIndex] > Lalr->GotoVertex[Edge]) {
                Lalr->GotoVertex[GotoIndex] = Lalr->GotoVertex[Edge];
            }

            //
            // Absorb the follows of the reachable edge.
            //

            Destination = Base;
            Source = Lalr->GotoFollows + (Edge * Lalr->TokenSetSize);
            while (Destination < End) {
                *Destination |= *Source;
                Destination += 1;
                Source += 1;
            }
        }
    }

    //
    // If this vertex only expanded outwards and did not have any edges
    // pointing backwards towards previous edges, then remove those vertices,
    // since they'll never be more useful than this one. Also propagate the
    // follow set of this one out to those dead ones.
    //

    if (Lalr->GotoVertex[GotoIndex] == Height) {
        while (TRUE) {
            Edge = Lalr->Vertices[Lalr->Top];
            Lalr->Top -= 1;
            Lalr->GotoVertex[Edge] = Lalr->Infinity;
            if (Edge == GotoIndex) {
                break;
            }

            Source = Base;
            Destination = Lalr->GotoFollows + (Edge * Lalr->TokenSetSize);
            while (Source < End) {
                *Destination = *Source;
                Destination += 1;
                Source += 1;
            }
        }
    }

    return;
}

PYY_GOTO_INDEX *
YypTranspose (
    PYY_GOTO_INDEX *Relations,
    YY_GOTO_INDEX Count
    )

/*++

Routine Description:

    This routine transposes the relations array.

Arguments:

    Relations - Supplies an array of goto index arrays to transpose.

    Count - Supplies the number of gotos.

Return Value:

    Returns the transposed arrays on success.

    NULL on allocation failure.

--*/

{

    PYY_GOTO_INDEX *Current;
    ULONG EdgeCount;
    PULONG EdgeCounts;
    PYY_GOTO_INDEX Edges;
    YY_GOTO_INDEX Index;
    PYY_GOTO_INDEX *NewRelations;
    YY_STATUS YyStatus;

    YyStatus = YyStatusNoMemory;
    NewRelations = NULL;
    Current = NULL;
    EdgeCounts = YypAllocate(Count * sizeof(ULONG));
    if (EdgeCounts == NULL) {
        goto TransposeEnd;
    }

    //
    // Count how many times each goto appears.
    //

    for (Index = 0; Index < Count; Index += 1) {
        Edges = Relations[Index];
        if (Edges != NULL) {
            while (*Edges >= 0) {
                EdgeCounts[*Edges] += 1;
                Edges += 1;
            }
        }
    }

    NewRelations = YypAllocate(Count * sizeof(PYY_GOTO_INDEX));
    if (NewRelations == NULL) {
        goto TransposeEnd;
    }

    Current = YypAllocate(Count * sizeof(PYY_GOTO_INDEX));
    if (Current == NULL) {
        goto TransposeEnd;
    }

    //
    // Allocate the inner arrays now that the sizes of each are known.
    //

    for (Index = 0; Index < Count; Index += 1) {
        EdgeCount = EdgeCounts[Index];
        if (EdgeCount > 0) {
            NewRelations[Index] =
                          YypAllocate((EdgeCount + 1) * sizeof(YY_GOTO_INDEX));

            if (NewRelations[Index] == NULL) {
                goto TransposeEnd;
            }

            Current[Index] = NewRelations[Index];
            NewRelations[Index][EdgeCount] = -1;
        }
    }

    //
    // Fill in the arrays.
    //

    for (Index = 0; Index < Count; Index += 1) {
        Edges = Relations[Index];
        if (Edges != NULL) {
            while (*Edges >= 0) {
                *(Current[*Edges]) = Index;
                Current[*Edges] += 1;
                Edges += 1;
            }
        }
    }

    YyStatus = YyStatusSuccess;

TransposeEnd:
    if (YyStatus != YyStatusSuccess) {
        if (NewRelations != NULL) {
            for (Index = 0; Index < Count; Index += 1) {
                if (NewRelations[Index] != NULL) {
                    YypFree(NewRelations[Index]);
                }
            }

            YypFree(NewRelations);
            NewRelations = NULL;
        }
    }

    if (EdgeCounts != NULL) {
        YypFree(EdgeCounts);
    }

    if (Current != NULL) {
        YypFree(Current);
    }

    return NewRelations;
}

YY_GOTO_INDEX
YypFindGoto (
    PYYGEN_CONTEXT Context,
    YY_STATE_INDEX State,
    YY_VALUE Symbol
    )

/*++

Routine Description:

    This routine finds the goto corresponding to the given source (from) state
    and symbol.

Arguments:

    Context - Supplies a pointer to the generator context.

    State - Supplies the index of the source state of the goto.

    Symbol - Supplies the symbol the state uses to transition.

Return Value:

    Returns the goto index corresponding to the given state and symbol.

--*/

{

    YY_STATE_INDEX FromState;
    YY_GOTO_INDEX High;
    YY_GOTO_INDEX Low;
    YY_GOTO_INDEX Middle;

    //
    // The goto map starts at the first non-terminal.
    //

    Symbol -= Context->TokenCount;

    //
    // Throw a little binary search in there to make it jazzy.
    //

    Low = Context->GotoMap[Symbol];
    High = Context->GotoMap[Symbol + 1];
    while (TRUE) {

        assert(Low <= High);

        Middle = (Low + High) / 2;
        FromState = Context->FromState[Middle];
        if (FromState == State) {
            return Middle;

        } else if (FromState < State) {
            Low = Middle + 1;

        } else {
            High = Middle - 1;
        }
    }

    //
    // Execution never gets here.
    //

    assert(FALSE);

    return YY_MAX_GOTOS;
}

VOID
YypPrintGotoMap (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    )

/*++

Routine Description:

    This routine prints the initial goto map.

Arguments:

    Context - Supplies a pointer to the generator context.

    Lalr - Supplies a pointer to the LALR context.

Return Value:

    None.

--*/

{

    YY_STATE_INDEX Destination;
    YY_GOTO_INDEX Index;

    printf("\nGoto map:\n");
    for (Index = 0; Index < Lalr->GotoCount; Index += 1) {
        Destination = Context->ToState[Index];
        printf("    %d: %d -> %d via %s\n",
               Index,
               Context->FromState[Index],
               Destination,
               Context->Elements[Context->AccessingSymbol[Destination]].Name);
    }

    return;
}

VOID
YypPrintTokenBitmapArray (
    PYYGEN_CONTEXT Context,
    PULONG BitmapArray,
    YY_VALUE Count
    )

/*++

Routine Description:

    This routine prints an array of token bitmaps.

Arguments:

    Context - Supplies a pointer to the generator context.

    BitmapArray - Supplies a pointer to the array of token bitmaps.

    Count - Supplies the number of elements in the (outer) array.

Return Value:

    None.

--*/

{

    YY_GOTO_INDEX Index;
    PULONG Row;
    ULONG RowSize;
    YY_VALUE Symbol;

    RowSize = YYGEN_BITMAP_WORD_COUNT(Context->TokenCount);
    Row = BitmapArray;
    for (Index = 0; Index < Count; Index += 1) {
        printf("\n    %d:", Index);
        for (Symbol = 0; Symbol < Context->TokenCount; Symbol += 1) {
            if (YYGEN_BITMAP_IS_SET(Row, Symbol)) {
                printf("%s ", Context->Elements[Symbol].Name);
            }
        }

        Row += RowSize;
    }

    printf("\n");
    return;
}

VOID
YypPrintIncludes (
    PYYGEN_CONTEXT Context,
    PYYGEN_LALR_CONTEXT Lalr
    )

/*++

Routine Description:

    This routine prints the includes array.

Arguments:

    Context - Supplies a pointer to the generator context.

    Lalr - Supplies a pointer to the LALR context.

Return Value:

    None.

--*/

{

    PYY_GOTO_INDEX Array;
    YY_GOTO_INDEX Outer;

    printf("\nIncludes:");
    for (Outer = 0; Outer < Lalr->GotoCount; Outer += 1) {
        Array = Lalr->Includes[Outer];
        if (Array == NULL) {
            continue;
        }

        printf("\n    %d: ", Outer);
        while (*Array >= 0) {
            printf("%d ", *Array);
            Array += 1;
        }
    }

    printf("\n");
    return;
}

