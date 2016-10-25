/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    yygen.c

Abstract:

    This module implements the Minoca grammar generator.

Author:

    Evan Green 9-Apr-2016

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "yygenp.h"
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

YY_STATUS
YypInitializeGeneratorContext (
    PYYGEN_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

YY_STATUS
YyGenerateGrammar (
    PYY_GRAMMAR_DESCRIPTION Description,
    ULONG Flags,
    PYYGEN_CONTEXT *NewContext
    )

/*++

Routine Description:

    This routine converts a given grammar description into an LALR(1) grammar.

Arguments:

    Description - Supplies a pointer to the grammar description.

    Flags - Supplies a bitfield of flags. See YYGEN_FLAG_* definitions.

    NewContext - Supplies a pointer where a pointer to the grammar context will
        be returned on success.

Return Value:

    YY status.

--*/

{

    PYYGEN_CONTEXT Context;
    YY_STATUS YyStatus;

    Context = YypAllocate(sizeof(YYGEN_CONTEXT));
    if (Context == NULL) {
        YyStatus = YyStatusNoMemory;
        goto GenerateGrammarEnd;
    }

    memset(Context, 0, sizeof(YYGEN_CONTEXT));
    Context->Flags = Flags;
    Context->Elements = Description->Elements;
    Context->TokenCount = Description->TokenCount;
    Context->SymbolCount = Description->SymbolCount;
    Context->ExpectedShiftReduceConflicts =
                                     Description->ExpectedShiftReduceConflicts;

    Context->ExpectedReduceReduceConflicts =
                                    Description->ExpectedReduceReduceConflicts;

    Context->VariablePrefix = Description->VariablePrefix;
    Context->OutputFileName = Description->OutputFileName;
    YyStatus = YypInitializeGeneratorContext(Context);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateGrammarEnd;
    }

    //
    // Start by creating the LR(0) parser.
    //

    YyStatus = YypGenerateLr0Grammar(Context);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateGrammarEnd;
    }

    //
    // Augment with lookaheads to produce an LALR(1) parser.
    //

    YyStatus = YypGenerateLalr(Context);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateGrammarEnd;
    }

    //
    // Allocate and initialize the parser constructs.
    //

    YyStatus = YypBuildParser(Context);
    if (YyStatus != YyStatusSuccess) {
        goto GenerateGrammarEnd;
    }

GenerateGrammarEnd:
    if (YyStatus != YyStatusSuccess) {
        if (Context != NULL) {
            YyDestroyGeneratorContext(Context);
            Context = NULL;
        }
    }

    *NewContext = Context;
    return YyStatus;
}

VOID
YyDestroyGeneratorContext (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys a grammar generator context structure.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    None.

--*/

{

    PYYGEN_ACTION Action;
    YY_VALUE Index;
    PVOID Next;
    PYYGEN_REDUCTIONS Reduction;
    PYYGEN_SHIFTS Shift;
    PYYGEN_STATE State;

    if (Context->Nullable != NULL) {
        YypFree(Context->Nullable);
    }

    if (Context->Items != NULL) {
        YypFree(Context->Items);
    }

    if (Context->Rules != NULL) {
        YypFree(Context->Rules);
    }

    if (Context->Derives != NULL) {
        YypFree(Context->Derives);
    }

    if (Context->ItemSet != NULL) {
        YypFree(Context->ItemSet);
    }

    if (Context->RuleSet != NULL) {
        YypFree(Context->RuleSet);
    }

    if (Context->FirstDerives != NULL) {
        YypFree(Context->FirstDerives);
    }

    if (Context->StateTable != NULL) {
        YypFree(Context->StateTable);
    }

    State = Context->FirstState;
    while (State != NULL) {
        Next = State->Next;
        YypFree(State);
        State = Next;
    }

    if (Context->ShiftTable != NULL) {
        YypFree(Context->ShiftTable);
    }

    Shift = Context->FirstShift;
    while (Shift != NULL) {
        Next = Shift->Next;
        YypFree(Shift);
        Shift = Next;
    }

    if (Context->ReductionTable != NULL) {
        YypFree(Context->ReductionTable);
    }

    Reduction = Context->FirstReduction;
    while (Reduction != NULL) {
        Next = Reduction->Next;
        YypFree(Reduction);
        Reduction = Next;
    }

    if (Context->Lookaheads != NULL) {
        YypFree(Context->Lookaheads);
    }

    if (Context->LookaheadSets != NULL) {
        YypFree(Context->LookaheadSets);
    }

    if (Context->LookaheadRule != NULL) {
        YypFree(Context->LookaheadRule);
    }

    if (Context->GotoMap != NULL) {
        YypFree(Context->GotoMap);
    }

    if (Context->FromState != NULL) {
        YypFree(Context->FromState);
    }

    if (Context->ToState != NULL) {
        YypFree(Context->ToState);
    }

    if (Context->Parser != NULL) {
        for (Index = 0; Index < Context->StateCount; Index += 1) {
            Action = Context->Parser[Index];
            while (Action != NULL) {
                Next = Action->Next;
                YypFree(Action);
                Action = Next;
            }
        }

        YypFree(Context->Parser);
    }

    if (Context->ShiftReduceConflicts != NULL) {
        YypFree(Context->ShiftReduceConflicts);
    }

    if (Context->ReduceReduceConflicts != NULL) {
        YypFree(Context->ReduceReduceConflicts);
    }

    if (Context->DefaultReductions != NULL) {
        YypFree(Context->DefaultReductions);
    }

    YypFree(Context);
    return;
}

VOID
YyGetConflictCounts (
    PYYGEN_CONTEXT Context,
    PYY_VALUE ShiftReduceConflicts,
    PYY_VALUE ReduceReduceConflicts
    )

/*++

Routine Description:

    This routine returns the number of conflicts in the grammar, minus the
    number of expected conflicts.

Arguments:

    Context - Supplies a pointer to the generator context.

    ShiftReduceConflicts - Supplies an optional pointer where the number of
        shift-reduce conflicts will be returned.

    ReduceReduceConflicts - Supplies an optional pointer where the number of
        reduce-reduce conflicts will be returned.

Return Value:

    None.

--*/

{

    if (ShiftReduceConflicts != NULL) {
        *ShiftReduceConflicts = Context->ShiftReduceConflictCount -
                                Context->ExpectedShiftReduceConflicts;
    }

    if (ReduceReduceConflicts != NULL) {
        *ReduceReduceConflicts = Context->ReduceReduceConflictCount -
                                 Context->ExpectedReduceReduceConflicts;
    }

    return;
}

PVOID
YypAllocate (
    UINTN Size
    )

/*++

Routine Description:

    This routine allocates and clears a region of memory.

Arguments:

    Size - Supplies the size in bytes of the allocation.

Return Value:

    Returns a pointer to the new memory on success.

    NULL on failure.

--*/

{

    PVOID Allocation;

    Allocation = calloc(Size, 1);
    return Allocation;
}

PVOID
YypReallocate (
    PVOID Allocation,
    UINTN NewSize
    )

/*++

Routine Description:

    This routine reallocates previously allocated memory.

Arguments:

    Allocation - Supplies an optional pointer to the memory to reallocate.

    NewSize - Supplies the desired size of the allocation.

Return Value:

    Returns a pointer to the newly sized buffer on success. This might be the
    same buffer passed in or a new buffer. The old buffer will be invalid
    after this.

    NULL on reallocation failure. The old buffer will remain valid and still
    needs to be freed.

--*/

{

    PVOID NewAllocation;

    NewAllocation = realloc(Allocation, NewSize);
    return NewAllocation;
}

VOID
YypFree (
    PVOID Allocation
    )

/*++

Routine Description:

    This routine frees a previously allocated region of memory.

Arguments:

    Allocation - Supplies a pointer to the allocation to free.

Return Value:

    None.

--*/

{

    free(Allocation);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

YY_STATUS
YypInitializeGeneratorContext (
    PYYGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes a grammar context structure.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    YY status.

--*/

{

    PYY_VALUE Components;
    YY_VALUE ElementIndex;
    YY_VALUE ItemCount;
    YY_VALUE RuleCount;

    //
    // Verify that the tokens do not have productions.
    //

    for (ElementIndex = 0;
         ElementIndex < Context->TokenCount;
         ElementIndex += 1) {

        if (Context->Elements[ElementIndex].Components != NULL) {
            return YyStatusInvalidSpecification;
        }
    }

    if (Context->SymbolCount <= Context->TokenCount) {
        return YyStatusInvalidSpecification;
    }

    if (Context->Elements[ElementIndex].Components != NULL) {
        return YyStatusInvalidSpecification;
    }

    //
    // Count the productions and items. There are 3 extra rules:
    // Rule 0 is invalid (since it can't be negated).
    // Rule 1 is empty.
    // Rule 2 is the start rule.
    // Token zero is always assumed to be the end-of-file marker.
    //

    ItemCount = 4;
    RuleCount = 3;
    for (ElementIndex = Context->TokenCount + 1;
         ElementIndex < Context->SymbolCount;
         ElementIndex += 1) {

        if ((Context->Elements[ElementIndex].Flags & YY_ELEMENT_START) != 0) {
            if (Context->StartSymbol != 0) {
                return YyStatusInvalidSpecification;
            }

            Context->StartSymbol = ElementIndex;
        }

        Components = Context->Elements[ElementIndex].Components;
        if (Components == NULL) {
            return YyStatusInvalidSpecification;
        }

        while (*Components != 0) {
            if (*Components < 0) {
                RuleCount += 1;
            }

            ItemCount += 1;
            Components += 1;
        }
    }

    Context->ItemCount = ItemCount;
    Context->RuleCount = RuleCount;

    //
    // The first symbol is the start symbol.
    //

    Context->NonTerminalCount = Context->SymbolCount - Context->TokenCount;
    return YyStatusSuccess;
}

