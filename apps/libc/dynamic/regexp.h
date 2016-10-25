/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    regexp.h

Abstract:

    This header contains private definitions for implementing support for
    Regular Expressions.

Author:

    Evan Green 8-Jul-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro matches the "name" character class, which is uppercase letters,
// lowercase letters, digits, and underscore.
//

#define REGULAR_EXPRESSION_IS_NAME(_Character)          \
    ((isupper(_Character)) || (islower(_Character)) ||  \
     (isdigit(_Character)) || ((_Character) == '_'))

//
// ---------------------------------------------------------------- Definitions
//

//
// Regular expression internal flags.
//

#define REGULAR_EXPRESSION_ANCHORED_LEFT 0x00000001
#define REGULAR_EXPRESSION_ANCHORED_RIGHT 0x00000002
#define REGULAR_EXPRESSION_NEGATED 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _REGULAR_EXPRESSION_STATUS {
    RegexStatusSuccess,
    RegexStatusNoMatch = REG_NOMATCH,
    RegexStatusBadPattern = REG_BADPAT,
    RegexStatusBadCollatingElement = REG_ECOLLATE,
    RegexStatusBadCharacterClass = REG_ECTYPE,
    RegexStatusTrailingEscape = REG_EESCAPE,
    RegexStatusInvalidSubexpression = REG_ESUBREG,
    RegexStatusBracketImbalance = REG_EBRACK,
    RegexStatusParenthesesImbalance = REG_EPAREN,
    RegexStatusInvalidBraces = REG_BADBR,
    RegexStatusBadRange = REG_ERANGE,
    RegexStatusNoMemory = REG_ESPACE,
    RegexStatusInvalidRepeat = REG_BADRPT,
} REGULAR_EXPRESSION_STATUS, *PREGULAR_EXPRESSION_STATUS;

typedef enum _REGEX_ENTRY_TYPE {
    RegexEntryInvalid,
    RegexEntryOrdinaryCharacters,
    RegexEntryAnyCharacter,
    RegexEntryBackReference,
    RegexEntrySubexpression,
    RegexEntryBracketExpression,
    RegexEntryStringBegin,
    RegexEntryStringEnd,
    RegexEntryBranch,
    RegexEntryBranchOption,
    RegexEntryStartOfWord,
    RegexEntryEndOfWord,
} REGEX_ENTRY_TYPE, *PREGEX_ENTRY_TYPE;

typedef enum _BRACKET_EXPRESSION_TYPE {
    BracketExpressionInvalid,
    BracketExpressionSingleCharacters,
    BracketExpressionRange,
    BracketExpressionCharacterClassAlphanumeric,
    BracketExpressionCharacterClassAlphabetic,
    BracketExpressionCharacterClassBlank,
    BracketExpressionCharacterClassControl,
    BracketExpressionCharacterClassDigit,
    BracketExpressionCharacterClassGraph,
    BracketExpressionCharacterClassLowercase,
    BracketExpressionCharacterClassPrintable,
    BracketExpressionCharacterClassPunctuation,
    BracketExpressionCharacterClassSpace,
    BracketExpressionCharacterClassUppercase,
    BracketExpressionCharacterClassHexDigit,
    BracketExpressionCharacterClassName
} BRACKET_EXPRESSION_TYPE, *PBRACKET_EXPRESSION_TYPE;

/*++

Structure Description:

    This structure defines a string in a regular expression used for storing
    characters (ordinary or set).

Members:

    Data - Supplies a pointer to the buffer containing the string characters.

    Size - Supplies the number of valid bytes in the buffer.

    Capacity - Supplies the size of the buffer allocation.

--*/

typedef struct _REGULAR_EXPRESSION_STRING {
    PSTR Data;
    ULONG Size;
    ULONG Capacity;
} REGULAR_EXPRESSION_STRING, *PREGULAR_EXPRESSION_STRING;

/*++

Structure Description:

    This structure defines a bracket expression embedded within a regular
    expression.

Members:

    Minimum - Stores the minimum character, inclusive.

    Maximum - Stores the maximum character, inclusive.

--*/

typedef struct _REGULAR_BRACKET_EXPRESSION_RANGE {
    INT Minimum;
    INT Maximum;
} REGULAR_BRACKET_EXPRESSION_RANGE, *PREGULAR_BRACKET_EXPRESSION_RANGE;

/*++

Structure Description:

    This structure defines a bracket expression embedded within a regular
    expression.

Members:

    ListEntry - Stores pointers to the next and previous bracket entries in the
        expression.

    Type - Stores the type of bracket expression this entry represents.

    Range - Stores the range for range expressions.

--*/

typedef struct _REGULAR_BRACKET_ENTRY {
    LIST_ENTRY ListEntry;
    BRACKET_EXPRESSION_TYPE Type;
    union {
        REGULAR_BRACKET_EXPRESSION_RANGE Range;
    } U;

} REGULAR_BRACKET_ENTRY, *PREGULAR_BRACKET_ENTRY;

/*++

Structure Description:

    This structure defines a bracket expression embedded within a regular
    expression.

Members:

    RegularCharacters - Stores the string containing the regular characters in
        the bracket expression.

    EntryList - Stores the list of bracket entries, which contains things like
        ranges and character classes.

--*/

typedef struct _REGULAR_BRACKET_EXPRESSION {
    REGULAR_EXPRESSION_STRING RegularCharacters;
    LIST_ENTRY EntryList;
} REGULAR_BRACKET_EXPRESSION, *PREGULAR_BRACKET_EXPRESSION;

typedef struct _REGULAR_EXPRESSION_ENTRY
    REGULAR_EXPRESSION_ENTRY, *PREGULAR_EXPRESSION_ENTRY;

/*++

Structure Description:

    This structure defines an entry within a regular expression.

Members:

    ListEntry - Stores pointers to the next and previous entries in the
        regular expression.

    Type - Stores the type of regular expression entry.

    Flags - Stores flags describing the behavior of the entry. See
        REGULAR_EXPRESSION_* definitions.

    DuplicateMin - Stores the minimum number of occurrences of the entry.

    DuplicateMax - Stores the maximum number of occurrences of the entry.
        Supply -1 for infinite recurrences.

    ChildList - Stores the list of child expression entries in this node.

    Parent - Stores the optional parent node.

    String - Stores the string for ordinary characters.

    BackReferenceNumber - Stores the subexpression index being referred to in
        a back reference.

    SubexpressionNumber - Stores the index of this subexpression, starting from
        1.

    BracketExpression - Stores the bracker expression information for bracket
        expressions.

--*/

struct _REGULAR_EXPRESSION_ENTRY {
    LIST_ENTRY ListEntry;
    REGEX_ENTRY_TYPE Type;
    ULONG Flags;
    ULONG DuplicateMin;
    ULONG DuplicateMax;
    LIST_ENTRY ChildList;
    PREGULAR_EXPRESSION_ENTRY Parent;
    union {
        REGULAR_EXPRESSION_STRING String;
        ULONG BackReferenceNumber;
        ULONG SubexpressionNumber;
        REGULAR_BRACKET_EXPRESSION BracketExpression;
    } U;

};

/*++

Structure Description:

    This structure defines the internal regular expression representation.

Members:

    SubexpressionCount - Stores the number of sub expressions.

    Flags - Stores the flags of the regular expression.

    BaseEntry - Stores the initial subexpression entry, a slightly modified
        subexpression.

--*/

typedef struct _REGULAR_EXPRESSION {
    ULONG SubexpressionCount;
    ULONG Flags;
    REGULAR_EXPRESSION_ENTRY BaseEntry;
} REGULAR_EXPRESSION, *PREGULAR_EXPRESSION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
