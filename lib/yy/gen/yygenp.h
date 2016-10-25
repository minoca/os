/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    yygenp.h

Abstract:

    This header contains internal definitions for the grammar generator
    library.

Author:

    Evan Green 9-Apr-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/yy.h>
#include <minoca/lib/yygen.h>

//
// ---------------------------------------------------------------- Definitions
//

#define YYGEN_BITS_PER_WORD (sizeof(ULONG) * BITS_PER_BYTE)

//
// This macro computes the number of words needed to accomodate a bitmap that
// holds at least the given number of bits.
//

#define YYGEN_BITMAP_WORD_COUNT(_Bits) \
    (((_Bits) + (YYGEN_BITS_PER_WORD - 1)) / YYGEN_BITS_PER_WORD)

//
// This macro sets a bit in the bitmap.
//

#define YYGEN_BITMAP_SET(_Row, _Bit) \
    (((PULONG)(_Row))[(_Bit) / YYGEN_BITS_PER_WORD] |= \
        1 << ((_Bit) & (YYGEN_BITS_PER_WORD - 1)))

//
// This macro evaluates to non-zero if the given bit is set in the bitmap.
//

#define YYGEN_BITMAP_IS_SET(_Row, _Bit) \
    ((((PULONG)(_Row))[(_Bit) / YYGEN_BITS_PER_WORD] & \
      (1 << ((_Bit) & (YYGEN_BITS_PER_WORD - 1)))) != 0)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _YY_ACTION_CODE {
    YyActionInvalid,
    YyActionShift,
    YyActionReduce
} YY_ACTION_CODE, *PYY_ACTION_CODE;

typedef enum _YYGEN_SUPPRESSION {
    YyNotSuppressed,
    YySuppressedNoisily,
    YySuppressedQuietly
} YYGEN_SUPPRESSION, *PYYGEN_SUPPRESSION;

typedef YY_VALUE YY_RULE_INDEX, *PYY_RULE_INDEX;
typedef YY_VALUE YY_ITEM_INDEX, *PYY_ITEM_INDEX;
typedef YY_VALUE YY_STATE_INDEX, *PYY_STATE_INDEX;
typedef YY_VALUE YY_GOTO_INDEX, *PYY_GOTO_INDEX;
typedef YY_VALUE YY_ACTION_INDEX, *PYY_ACTION_INDEX;

typedef struct _YYGEN_STATE YYGEN_STATE, *PYYGEN_STATE;
typedef struct _YYGEN_SHIFTS YYGEN_SHIFTS, *PYYGEN_SHIFTS;
typedef struct _YYGEN_REDUCTIONS YYGEN_REDUCTIONS, *PYYGEN_REDUCTIONS;
typedef struct _YYGEN_ACTION YYGEN_ACTION, *PYYGEN_ACTION;

/*++

Structure Description:

    This structure defines an individual grammar rule.

Members:

    LeftSide - Stores the left side of the rule.

    RightSide - Stores an index into the items array where the right side of
        this rule resides.

    Precedence - Stores the precedence for the rule.

    Associativity - Stores an associativity for the rule.

    Used - Stores a boolean indicating whether or not the rules were used.

--*/

typedef struct _YYGEN_RULE {
    YY_VALUE LeftSide;
    YY_ITEM_INDEX RightSide;
    ULONG Precedence;
    YY_ASSOCIATIVITY Associativity;
    BOOL Used;
} YYGEN_RULE, *PYYGEN_RULE;

/*++

Structure Description:

    This structure contains the core state structure of the LR(0) state machine.

Members:

    Next - Stores a pointer to the next state globally in the state machine.

    Link - Stores a pointer to next element in this bucket in the hash table of
        states.

    Number - Stores the state number.

    AccessingSymbol - Stores the shift symbol that causes entrance into this
        state.

    ItemCount - Stores the number of elements in the items array.

    Items - Stores an array of indices into the item array representing the
        right hand sides of all the rules in this state.

--*/

struct _YYGEN_STATE {
    PYYGEN_STATE Next;
    PYYGEN_STATE Link;
    YY_STATE_INDEX Number;
    YY_VALUE AccessingSymbol;
    YY_VALUE ItemsCount;
    PYY_ITEM_INDEX Items;
};

/*++

Structure Description:

    This structure contains the set of reductions for a state in the LR(0)
    state machine.

Members:

    Next - Stores a pointer to the next set of reductions globally in the state
        machine.

    Number - Stores the state number these reductions correspond to.

    Count - Stores the number of elements in the rules array.

    Rules - Stores the set of rules that reduce in this state.

--*/

struct _YYGEN_REDUCTIONS {
    PYYGEN_REDUCTIONS Next;
    YY_STATE_INDEX Number;
    YY_VALUE Count;
    PYY_RULE_INDEX Rules;
};

/*++

Structure Description:

    This structure descibes the set of shifts out of a given state.

Members:

    Next - Stores a pointer to the next set of shifts in the state machine.

    Number - Stores the state number of the shifts.

    Count - Stores the number of shift states in the array.

    States - Stores an array of state numbers to possible next states, sorted.

--*/

struct _YYGEN_SHIFTS {
    PYYGEN_SHIFTS Next;
    YY_STATE_INDEX Number;
    YY_VALUE Count;
    PYY_STATE_INDEX States;
};

/*++

Structure Description:

    This structure descibes a parser action.

Members:

    Next - Stores a pointer to the next action.

    Symbol - Stores the action symbol.

    Number - Stores the action index.

    Precedence - Stores the action precedence.

    Associativity - Stores the associativity of the action.

    Code - Stores the action type: shift or reduce.

    Suppression - Stores the suppression state of this action.

--*/

struct _YYGEN_ACTION {
    PYYGEN_ACTION Next;
    YY_VALUE Symbol;
    YY_ACTION_INDEX Number;
    YY_VALUE Precedence;
    YY_ASSOCIATIVITY Associativity;
    YY_ACTION_CODE Code;
    YYGEN_SUPPRESSION Suppression;
};

/*++

Structure Description:

    This structure contains the working state for the grammar generator.

Members:

    Flags - Stores the global flags. See YYGEN_FLAG_* definitions.

    Elements - Stores the array of elements.

    VariablePrefix - Stores the prefix to prepend to all the variable names.

    OutputFileName - Stores the name of the output file, which is printed in
        the output source.

    TokenCount - Stores the first invalid token number. Any value below this is
        assumed to be a token.

    SymbolCount - Stores the number of tokens plus non-terminals.

    NonTerminalCount - Stores the number of non-terminals, including the
        start symbol.

    StartSymbol - Stores the starting symbol of the grammar.

    ItemCount - Stores the total count of all the elements in all the rules.

    RuleCount - Stores the number of rules.

    Nullable - Stores an array of booleans indexed by symbol that indicates
        if that production is empty.

    Items - Stores an array of all right sides of all rules. This establishes
        a total order of item sets by rule. The arrays are also terminated by
        a negative number, the rule index.

    Rules - Stores an array of rules.

    Derives - Stores an array of pointers to the set of rules for each left
        hand side. That is, these are indices into the rules array for each
        production. This is index by symbol.

    ItemSet - Stores the item set for the state currently being built.

    ItemSetEnd - Stores the end of the current item set.

    RuleSet - Stores a bitmap of the rule set.

    FirstDerives - Stores an array of bitmaps. Each column represents a rule,
        and the rows are the non-terminals. The bitmap describes the rules
        in the FIRST set for each production. The FIRST set is the set of
        non-terminals that can appear first for any production.

    FirstState - Stores a pointer to the first state in the LR(0) state machine.

    StateCount - Stores the number of states in the LR(0) state machine.

    FirstReduction - Stores the head of the singly linked list of reductions.

    LastReduction - Stores the tail of the singly linked list of reductions.

    FirstShift - Stores the head of the singly linked list of shifts.

    LastShift - Stores the tail of the singly linked list of shifts.

    StateTable - Stores a pointer to the final states.

    AccessingSymbol - Stores an array of shift symbols that cause entrance to
        the state at each index.

    ShiftTable - Stores an array of shifts to other states, indexed by starting
        state.

    ReductionTable - Stores an array of reductions at each state.

    Lookaheads - Stores the array of indices into the lookahead sets, indexed
        by state. That is, for a given state, the element in this array shows
        where in the lookahead sets array to begin.

    LookaheadSets - Stores an array of token (terminal) bitmaps showing the
        lookaheads for every reduction in every state.

    LookaheadRule - Stores an array that runs parallel to the lookahead sets
        pointing back to a rule for the array index.

    GotoMap - Stores an array of indices into the FromState/ToState arrays
        where the gotos using a particular non-terminal symbol start. That is,
        given a symbol (minus the number of tokens), this array shows the index
        to start looking in the FromState/ToState arrays for gotos using this
        symbol.

    FromState - Stores the array of starting states for all the gotos.

    ToState - Stores the array of destination goto states, running parallel to
        the FromState array.

    Parser - Stores the combined action table.

    FinalState - Stores the state index of the "accept" state.

    UnusedRules - Stores the count of unused rules.

    ShiftReduceConflicts - Stores an array of counts of shift-reduce conflicts
        for each state.

    ReduceReduceConflicts - Stores an array of counts of reduce-reduce
        conflicts for each state.

    ShiftReduceConflictCount - Stores the total number of shift-reduce
        conflicts.

    ReduceReduceConflictCount - Stores the total number of reduce-reduce
        conflicts.

    ExpectedShiftReduceConflicts - Stores the expected number of shift-reduce
        conflicts.

    ExpectedReduceReduceConflicts - Stores the expected number of
        reduce-reduce conflicts.

    DefaultReductions - Stores the table of rules to reduce by, indexed by
        state.

--*/

struct _YYGEN_CONTEXT {
    ULONG Flags;
    PYY_ELEMENT Elements;
    PSTR VariablePrefix;
    PSTR OutputFileName;
    YY_VALUE TokenCount;
    YY_VALUE SymbolCount;
    YY_VALUE NonTerminalCount;
    YY_VALUE StartSymbol;
    ULONG ItemCount;
    ULONG RuleCount;
    PBOOL Nullable;
    PYY_VALUE Items;
    PYYGEN_RULE Rules;
    PYY_RULE_INDEX Derives;
    PYY_ITEM_INDEX ItemSet;
    PYY_ITEM_INDEX ItemSetEnd;
    PULONG RuleSet;
    PULONG FirstDerives;
    PYYGEN_STATE FirstState;
    YY_STATE_INDEX StateCount;
    PYYGEN_REDUCTIONS FirstReduction;
    PYYGEN_REDUCTIONS LastReduction;
    PYYGEN_SHIFTS FirstShift;
    PYYGEN_SHIFTS LastShift;
    PYYGEN_STATE *StateTable;
    PYY_VALUE AccessingSymbol;
    PYYGEN_SHIFTS *ShiftTable;
    PYYGEN_REDUCTIONS *ReductionTable;
    PYY_VALUE Lookaheads;
    PULONG LookaheadSets;
    PYY_RULE_INDEX LookaheadRule;
    PYY_GOTO_INDEX GotoMap;
    PYY_STATE_INDEX FromState;
    PYY_STATE_INDEX ToState;
    PYYGEN_ACTION *Parser;
    YY_STATE_INDEX FinalState;
    ULONG UnusedRules;
    PYY_VALUE ShiftReduceConflicts;
    PYY_VALUE ReduceReduceConflicts;
    YY_VALUE ShiftReduceConflictCount;
    YY_VALUE ReduceReduceConflictCount;
    YY_VALUE ExpectedShiftReduceConflicts;
    YY_VALUE ExpectedReduceReduceConflicts;
    PYY_RULE_INDEX DefaultReductions;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

YY_STATUS
YypGenerateLr0Grammar (
    PYYGEN_CONTEXT Context
    );

/*++

Routine Description:

    This routine generates an LR(0) grammar based on the description.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    YY status.

--*/

VOID
YypEstablishClosure (
    PYYGEN_CONTEXT Context,
    PYYGEN_STATE State
    );

/*++

Routine Description:

    This routine creates a closure on the itemset of the current state.

Arguments:

    Context - Supplies a pointer to the generator context.

    State - Supplies a pointer to the state to create the closure of.

Return Value:

    None.

--*/

YY_STATUS
YypGenerateLalr (
    PYYGEN_CONTEXT Context
    );

/*++

Routine Description:

    This routine generates an LALR(1) state machine based on an LR(0) state
    machine.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    YY status.

--*/

YY_STATUS
YypBuildParser (
    PYYGEN_CONTEXT Context
    );

/*++

Routine Description:

    This routine generates the parser data structures based on the LALR(1)
    construction.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    YY status.

--*/

PVOID
YypAllocate (
    UINTN Size
    );

/*++

Routine Description:

    This routine allocates and clears a region of memory.

Arguments:

    Size - Supplies the size in bytes of the allocation.

Return Value:

    Returns a pointer to the new memory on success.

    NULL on failure.

--*/

PVOID
YypReallocate (
    PVOID Allocation,
    UINTN NewSize
    );

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

VOID
YypFree (
    PVOID Allocation
    );

/*++

Routine Description:

    This routine frees a previously allocated region of memory.

Arguments:

    Allocation - Supplies a pointer to the allocation to free.

Return Value:

    None.

--*/

