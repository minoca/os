/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    obj.h

Abstract:

    This header contains definitions for objects within the setup interpreter.

Author:

    Evan Green 15-Oct-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define SETUP_TOKEN_BASE 512
#define SETUP_NODE_BASE 1024

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SETUP_TOKEN_TYPE {
    SetupTokenMultilineComment = SETUP_TOKEN_BASE,
    SetupTokenComment,
    SetupTokenIdentifier,
    SetupTokenHexInteger,
    SetupTokenOctalInteger,
    SetupTokenDecimalInteger,
    SetupTokenString,
    SetupTokenRightAssign,
    SetupTokenLeftAssign,
    SetupTokenAddAssign,
    SetupTokenSubtractAssign,
    SetupTokenMultiplyAssign,
    SetupTokenDivideAssign,
    SetupTokenModuloAssign,
    SetupTokenAndAssign,
    SetupTokenXorAssign,
    SetupTokenOrAssign,
    SetupTokenRightShift,
    SetupTokenLeftShift,
    SetupTokenIncrement,
    SetupTokenDecrement,
    SetupTokenLogicalAnd,
    SetupTokenLogicalOr,
    SetupTokenLessOrEqual,
    SetupTokenGreaterOrEqual,
    SetupTokenIsEqual,
    SetupTokenIsNotEqual,
    SetupTokenSemicolon,
    SetupTokenOpenBrace,
    SetupTokenCloseBrace,
    SetupTokenComma,
    SetupTokenColon,
    SetupTokenAssign,
    SetupTokenOpenParentheses,
    SetupTokenCloseParentheses,
    SetupTokenOpenBracket,
    SetupTokenCloseBracket,
    SetupTokenBitAnd,
    SetupTokenLogicalNot,
    SetupTokenBitNot,
    SetupTokenMinus,
    SetupTokenPlus,
    SetupTokenAsterisk,
    SetupTokenDivide,
    SetupTokenModulo,
    SetupTokenLessThan,
    SetupTokenGreaterThan,
    SetupTokenXor,
    SetupTokenBitOr,
    SetupTokenQuestion,
} SETUP_TOKEN_TYPE, *PSETUP_TOKEN_TYPE;

typedef enum _SETUP_NODE_TYPE {
    SetupNodeBegin = SETUP_NODE_BASE,
    SetupNodeListElementList = SetupNodeBegin,
    SetupNodeList,
    SetupNodeDictElement,
    SetupNodeDictElementList,
    SetupNodeDict,
    SetupNodePrimaryExpression,
    SetupNodePostfixExpression,
    SetupNodeUnaryExpression,
    SetupNodeUnaryOperator,
    SetupNodeMultiplicativeExpression,
    SetupNodeAdditiveExpression,
    SetupNodeShiftExpression,
    SetupNodeRelationalExpression,
    SetupNodeEqualityExpression,
    SetupNodeAndExpression,
    SetupNodeExclusiveOrExpression,
    SetupNodeInclusiveOrExpression,
    SetupNodeLogicalAndExpression,
    SetupNodeLogicalOrExpression,
    SetupNodeConditionalExpression,
    SetupNodeAssignmentExpression,
    SetupNodeAssignmentOperator,
    SetupNodeExpression,
    SetupNodeStatementList,
    SetupNodeExpressionStatement,
    SetupNodeTranslationUnit,
    SetupNodeEnd
} SETUP_NODE_TYPE, *PSETUP_NODE_TYPE;

typedef enum _SETUP_OBJECT_TYPE {
    SetupObjectInvalid,
    SetupObjectInteger,
    SetupObjectString,
    SetupObjectDict,
    SetupObjectList,
    SetupObjectReference,
    SetupObjectCount
} SETUP_OBJECT_TYPE, *PSETUP_OBJECT_TYPE;

typedef union _SETUP_OBJECT SETUP_OBJECT, *PSETUP_OBJECT;

/*++

Structure Description:

    This structure stores the common header for interpreter objects.

Members:

    ReferenceCount - Stores the reference count on the object.

--*/

typedef struct _SETUP_OBJECT_HEADER {
    SETUP_OBJECT_TYPE Type;
    ULONG ReferenceCount;
} SETUP_OBJECT_HEADER, *PSETUP_OBJECT_HEADER;

/*++

Structure Description:

    This structure stores the data for a setup integer object.

Members:

    Header - Stores the common object header.

    Value - Stores the integer value, as a 64-bit signed integer.

--*/

typedef struct _SETUP_INT {
    SETUP_OBJECT_HEADER Header;
    LONGLONG Value;
} SETUP_INT, *PSETUP_INT;

/*++

Structure Description:

    This structure stores the data for a setup string object.

Members:

    Header - Stores the common object header.

    String - Stores the string value.

    Size - Stores the size of the string, not including the null terminator.

--*/

typedef struct _SETUP_STRING {
    SETUP_OBJECT_HEADER Header;
    PSTR String;
    ULONG Size;
} SETUP_STRING, *PSETUP_STRING;

/*++

Structure Description:

    This structure stores the data for a setup list object.

Members:

    Header - Stores the common object header.

    Array - Stores the list of elements.

    Count - Stores the number of elements in the list.

--*/

typedef struct _SETUP_LIST {
    SETUP_OBJECT_HEADER Header;
    PSETUP_OBJECT *Array;
    UINTN Count;
} SETUP_LIST, *PSETUP_LIST;

/*++

Structure Description:

    This structure stores an entry in a dictionary.

Members:

    ListEntry - Stores pointers to the next and previous dictionary entries
        in the dictionary.

    Key - Stores a pointer to the key for the entry.

    Value - Stores a pointer to the value for the entry.

--*/

typedef struct _SETUP_DICT_ENTRY {
    LIST_ENTRY ListEntry;
    PSETUP_OBJECT Key;
    PSETUP_OBJECT Value;
} SETUP_DICT_ENTRY, *PSETUP_DICT_ENTRY;

/*++

Structure Description:

    This structure stores the data for a setup list object.

Members:

    Header - Stores the common object header.

    EntryList - Stores the head of the list of SETUP_DICT_ENTRY entries.

--*/

typedef struct _SETUP_DICT {
    SETUP_OBJECT_HEADER Header;
    LIST_ENTRY EntryList;
} SETUP_DICT, *PSETUP_DICT;

/*++

Structure Description:

    This structure stores the data for a setup reference (to another object).

Members:

    Header - Stores the common object header.

    Value - Stores the pointer to the object referred to.

--*/

typedef struct _SETUP_REFERENCE {
    SETUP_OBJECT_HEADER Header;
    PSETUP_OBJECT Value;
} SETUP_REFERENCE, *PSETUP_REFERENCE;

/*++

Structure Description:

    This union describes the entire storage size needed for any setup object
    type.

Members:

    Header - Stores the common header (also available in each of the
        specialized types).

    Integer - Stores the integer representation.

    String - Stores the string representation.

    List - Stores the list representation.

    Dict - Stores the dict representation.

    Reference - Stores the reference representation.
--*/

union _SETUP_OBJECT {
    SETUP_OBJECT_HEADER Header;
    SETUP_INT Integer;
    SETUP_STRING String;
    SETUP_LIST List;
    SETUP_DICT Dict;
    SETUP_REFERENCE Reference;
};

//
// -------------------------------------------------------------------- Globals
//

extern PSTR SetupObjectTypeNames[SetupObjectCount];

//
// -------------------------------------------------------- Function Prototypes
//

PSETUP_OBJECT
SetupCreateInteger (
    LONGLONG Value
    );

/*++

Routine Description:

    This routine creates a new integer object.

Arguments:

    Value - Supplies the initial value.

Return Value:

    Returns a pointer to the new integer on success.

    NULL on allocation failure.

--*/

PSETUP_OBJECT
SetupCreateString (
    PSTR InitialValue,
    ULONG Size
    );

/*++

Routine Description:

    This routine creates a new string object.

Arguments:

    InitialValue - Supplies an optional pointer to the initial value.

    Size - Supplies the size of the initial value.

Return Value:

    Returns a pointer to the new string on success.

    NULL on allocation failure.

--*/

INT
SetupStringAdd (
    PSETUP_OBJECT Left,
    PSETUP_OBJECT Right,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine adds two strings together, concatenating them.

Arguments:

    Left - Supplies a pointer to the left side of the operation.

    Right - Supplies a pointer to the right side of the operation.

    Result - Supplies a pointer where the result will be returned on success.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

PSETUP_OBJECT
SetupCreateList (
    PSETUP_OBJECT *InitialValues,
    ULONG Size
    );

/*++

Routine Description:

    This routine creates a new empty list object.

Arguments:

    InitialValues - Supplies an optional pointer to the initial values to set
        on the list.

    Size - Supplies the number of entries in the initial values array.

Return Value:

    Returns a pointer to the new string on success.

    NULL on allocation failure.

--*/

PSETUP_OBJECT
SetupListLookup (
    PSETUP_OBJECT List,
    ULONG Index
    );

/*++

Routine Description:

    This routine looks up the value at a particular list index.

Arguments:

    List - Supplies a pointer to the list.

    Index - Supplies the index to lookup.

Return Value:

    Returns a pointer to the list element with an increased reference count on
    success.

    NULL if the object at that index does not exist.

--*/

INT
SetupListSetElement (
    PSETUP_OBJECT ListObject,
    ULONG Index,
    PSETUP_OBJECT Object
    );

/*++

Routine Description:

    This routine sets the given list index to the given object.

Arguments:

    ListObject - Supplies a pointer to the list.

    Index - Supplies the index to set.

    Object - Supplies a pointer to the object to set at that list index. The
        reference count on the object will be increased on success.

Return Value:

    0 on success.

    Returns an error number on allocation failure.

--*/

INT
SetupListAdd (
    PSETUP_OBJECT Destination,
    PSETUP_OBJECT Addition
    );

/*++

Routine Description:

    This routine adds two lists together, storing the result in the first.

Arguments:

    Destination - Supplies a pointer to the destination. The list elements will
        be added to this list.

    Addition - Supplies the list containing the elements to add.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

PSETUP_OBJECT
SetupCreateDict (
    PSETUP_OBJECT Source
    );

/*++

Routine Description:

    This routine creates a new empty dictionary object.

Arguments:

    Source - Supplies an optional pointer to a dictionary to copy.

Return Value:

    Returns a pointer to the new string on success.

    NULL on allocation failure.

--*/

INT
SetupDictSetElement (
    PSETUP_OBJECT DictObject,
    PSETUP_OBJECT Key,
    PSETUP_OBJECT Value,
    PSETUP_OBJECT **LValue
    );

/*++

Routine Description:

    This routine adds or assigns a given value for a specific key.

Arguments:

    DictObject - Supplies a pointer to the dictionary.

    Key - Supplies a pointer to the key. This cannot be NULL. A reference will
        be added to the key if it is saved in the dictionary.

    Value - Supplies a pointer to the value. A reference will be added.

    LValue - Supplies an optional pointer where an LValue pointer will be
        returned on success. The caller can use the return of this pointer to
        assign into the dictionary element later.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PSETUP_DICT_ENTRY
SetupDictLookup (
    PSETUP_OBJECT DictObject,
    PSETUP_OBJECT Key
    );

/*++

Routine Description:

    This routine attempts to find an entry in the given dictionary for a
    specific key.

Arguments:

    DictObject - Supplies a pointer to the dictionary to query.

    Key - Supplies a pointer to the key object to search.

Return Value:

    Returns a pointer to the dictionary entry on success.

    NULL if the key was not found.

--*/

INT
SetupDictAdd (
    PSETUP_OBJECT Destination,
    PSETUP_OBJECT Addition
    );

/*++

Routine Description:

    This routine adds two dictionaries together, returning the result in the
    left one.

Arguments:

    Destination - Supplies a pointer to the dictionary to add to.

    Addition - Supplies the entries to add.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

PSETUP_OBJECT
SetupObjectCopy (
    PSETUP_OBJECT Source
    );

/*++

Routine Description:

    This routine creates a deep copy of the given object.

Arguments:

    Source - Supplies the source to copy from.

Return Value:

    Returns a pointer to the new object on success.

    NULL on failure.

--*/

BOOL
SetupObjectGetBooleanValue (
    PSETUP_OBJECT Object
    );

/*++

Routine Description:

    This routine converts an object to a boolean value.

Arguments:

    Object - Supplies a pointer to the object to booleanize.

Return Value:

    TRUE if the object is non-zero or non-empty.

    FALSE if the object is zero or empty.

--*/

VOID
SetupObjectAddReference (
    PSETUP_OBJECT Object
    );

/*++

Routine Description:

    This routine adds a reference to the given setup object.

Arguments:

    Object - Supplies a pointer to the object to add a reference to.

Return Value:

    None.

--*/

VOID
SetupObjectReleaseReference (
    PSETUP_OBJECT Object
    );

/*++

Routine Description:

    This routine releases a reference from the given setup object. If the
    reference count its zero, the object is destroyed.

Arguments:

    Object - Supplies a pointer to the object to release.

Return Value:

    None.

--*/

VOID
SetupPrintObject (
    PSETUP_OBJECT Object,
    ULONG RecursionDepth
    );

/*++

Routine Description:

    This routine prints an object.

Arguments:

    Object - Supplies a pointer to the object to print.

    RecursionDepth - Supplies the recursion depth.

Return Value:

    None.

--*/

