/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    yygen.h

Abstract:

    This header contains definitions for the Minoca grammar generator.

Author:

    Evan Green 9-Apr-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Set this flag for debugging in the parser generator.
//

#define YYGEN_FLAG_DEBUG 0x00000001

//
// Set this flag for the start symbol.
//

#define YY_ELEMENT_START 0x00000001

//
// Set these flags in an element's flags to indicate a symbol's associativity.
//

#define YY_ELEMENT_LEFT_ASSOCIATIVE 0x00000002
#define YY_ELEMENT_RIGHT_ASSOCIATIVE 0x00000004
#define YY_ELEMENT_NON_ASSOCIATIVE 0x00000008

//
// Define the maximum number of allowed states.
//

#define YY_VALUE_MAX MAX_SHORT
#define YY_MAX_STATES YY_VALUE_MAX
#define YY_MAX_GOTOS YY_VALUE_MAX
#define YY_MAX_TABLE 0x7FF0

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _YYGEN_CONTEXT YYGEN_CONTEXT, *PYYGEN_CONTEXT;

typedef enum _YY_ASSOCIATIVITY {
    YyNoAssociativity,
    YyLeftAssociative,
    YyRightAssociative,
    YyNonAssociative
} YY_ASSOCIATIVITY, YY_ASSOCIATIVITY;

/*++

Structure Description:

    This structure describes a grammar symbol definition, which is either a
    token (terminal) or a non-terminal, which consists of one or more rules.

Members:

    Name - Stores an optional pointer to the name of this grammar element. This
        is not used during parsing, but can be useful during debugging.

    Flags - Stores a bitfield of flags about this node. See YY_ELEMENT_*
        definitions.

    Precedence - Stores the precedence value for a given symbol. Non-zero
        values will be reduced by one to line up with the precedence values
        given at the end of rules. This make the first active precedence value
        2. 0 or 1 indicates no precedence specification.

    Components - Stores a sequence of rule elements. Each element is either a
        token value or a rule value, determined by the token count. Each form
        of a grammar expression is terminated by a negative value. The next
        alternate form starts after the terminator. Terminate the sequence
        with a zero to end the sequence of components. The rule's precence
        will be (-Value - 1). So terminating a rule with -1 specifies a
        precedence of 0 (unspecified).

--*/

typedef struct _YY_ELEMENT {
    PSTR Name;
    ULONG Flags;
    ULONG Precedence;
    PYY_VALUE Components;
} YY_ELEMENT, *PYY_ELEMENT;

/*++

Structure Description:

    This structure describes a grammar.

Members:

    Elements - Supplies a pointer to the array of elements, which describes
        both the tokens (terminal) and the non-terminals. The first element
        must always be reserved for EOF. The second element (index one) must
        always be reserved for an error token. The element at index TokenCount
        must always be reserved for the start symbol.

    TokenCount - Stores the count of token elements, including EOF. This is
        also the start symbol index.

    SymbolCount - Stores the number of elements in the array (all terminals
        and non-terminals including the start symbol).

    ExpectedShiftReduceConflicts - Stores the expected number of shift-reduce
        conflicts.

    ExpectedReduceReduceConflicts - Stores the expected number of
        reduce-reduce conflicts.

    VariablePrefix - Stores the variable prefix to prepend to all the
        output source variables.

    OutputFileName - Stores the name of the output file, which is printed in
        the output source.

--*/

typedef struct _YY_GRAMMAR_DESCRIPTION {
    PYY_ELEMENT Elements;
    YY_VALUE TokenCount;
    YY_VALUE SymbolCount;
    YY_VALUE ExpectedShiftReduceConflicts;
    YY_VALUE ExpectedReduceReduceConflicts;
    PSTR VariablePrefix;
    PSTR OutputFileName;
} YY_GRAMMAR_DESCRIPTION, *PYY_GRAMMAR_DESCRIPTION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

YY_STATUS
YyGenerateGrammar (
    PYY_GRAMMAR_DESCRIPTION Description,
    ULONG Flags,
    PYYGEN_CONTEXT *NewContext
    );

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

VOID
YyPrintGraph (
    PYYGEN_CONTEXT Context,
    FILE *File
    );

/*++

Routine Description:

    This routine prints the state graph for the given parsed grammar.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

Return Value:

    None.

--*/

VOID
YyPrintParserState (
    PYYGEN_CONTEXT Context,
    FILE *File
    );

/*++

Routine Description:

    This routine prints a human readable description of the parser states.

Arguments:

    Context - Supplies a pointer to the initialized grammar context.

    File - Supplies the file handle to output to.

Return Value:

    None.

--*/

YY_STATUS
YyOutputParserSource (
    PYYGEN_CONTEXT Context,
    FILE *File
    );

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

VOID
YyDestroyGeneratorContext (
    PYYGEN_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys a grammar generator context structure.

Arguments:

    Context - Supplies a pointer to the generator context.

Return Value:

    None.

--*/

VOID
YyGetConflictCounts (
    PYYGEN_CONTEXT Context,
    PYY_VALUE ShiftReduceConflicts,
    PYY_VALUE ReduceReduceConflicts
    );

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

