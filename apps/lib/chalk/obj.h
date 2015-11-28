/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    obj.h

Abstract:

    This header contains definitions for objects within the Chalk interpreter.

Author:

    Evan Green 15-Oct-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define CHALK_TOKEN_BASE 512
#define CHALK_NODE_BASE 1024

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _CHALK_TOKEN_TYPE {
    ChalkTokenMultilineComment = CHALK_TOKEN_BASE,
    ChalkTokenComment,
    ChalkTokenBreak,
    ChalkTokenContinue,
    ChalkTokenDo,
    ChalkTokenElse,
    ChalkTokenFor,
    ChalkTokenIf,
    ChalkTokenReturn,
    ChalkTokenWhile,
    ChalkTokenFunction,
    ChalkTokenIdentifier,
    ChalkTokenHexInteger,
    ChalkTokenOctalInteger,
    ChalkTokenDecimalInteger,
    ChalkTokenString,
    ChalkTokenRightAssign,
    ChalkTokenLeftAssign,
    ChalkTokenAddAssign,
    ChalkTokenSubtractAssign,
    ChalkTokenMultiplyAssign,
    ChalkTokenDivideAssign,
    ChalkTokenModuloAssign,
    ChalkTokenAndAssign,
    ChalkTokenXorAssign,
    ChalkTokenOrAssign,
    ChalkTokenRightShift,
    ChalkTokenLeftShift,
    ChalkTokenIncrement,
    ChalkTokenDecrement,
    ChalkTokenLogicalAnd,
    ChalkTokenLogicalOr,
    ChalkTokenLessOrEqual,
    ChalkTokenGreaterOrEqual,
    ChalkTokenIsEqual,
    ChalkTokenIsNotEqual,
    ChalkTokenSemicolon,
    ChalkTokenOpenBrace,
    ChalkTokenCloseBrace,
    ChalkTokenComma,
    ChalkTokenColon,
    ChalkTokenAssign,
    ChalkTokenOpenParentheses,
    ChalkTokenCloseParentheses,
    ChalkTokenOpenBracket,
    ChalkTokenCloseBracket,
    ChalkTokenBitAnd,
    ChalkTokenLogicalNot,
    ChalkTokenBitNot,
    ChalkTokenMinus,
    ChalkTokenPlus,
    ChalkTokenAsterisk,
    ChalkTokenDivide,
    ChalkTokenModulo,
    ChalkTokenLessThan,
    ChalkTokenGreaterThan,
    ChalkTokenXor,
    ChalkTokenBitOr,
    ChalkTokenQuestion,
} CHALK_TOKEN_TYPE, *PCHALK_TOKEN_TYPE;

typedef enum _CHALK_NODE_TYPE {
    ChalkNodeBegin = CHALK_NODE_BASE,
    ChalkNodeListElementList = ChalkNodeBegin,
    ChalkNodeList,
    ChalkNodeDictElement,
    ChalkNodeDictElementList,
    ChalkNodeDict,
    ChalkNodePrimaryExpression,
    ChalkNodePostfixExpression,
    ChalkNodeArgumentExpressionList,
    ChalkNodeUnaryExpression,
    ChalkNodeUnaryOperator,
    ChalkNodeMultiplicativeExpression,
    ChalkNodeAdditiveExpression,
    ChalkNodeShiftExpression,
    ChalkNodeRelationalExpression,
    ChalkNodeEqualityExpression,
    ChalkNodeAndExpression,
    ChalkNodeExclusiveOrExpression,
    ChalkNodeInclusiveOrExpression,
    ChalkNodeLogicalAndExpression,
    ChalkNodeLogicalOrExpression,
    ChalkNodeConditionalExpression,
    ChalkNodeAssignmentExpression,
    ChalkNodeAssignmentOperator,
    ChalkNodeExpression,
    ChalkNodeStatement,
    ChalkNodeCompoundStatement,
    ChalkNodeStatementList,
    ChalkNodeExpressionStatement,
    ChalkNodeSelectionStatement,
    ChalkNodeIterationStatement,
    ChalkNodeJumpStatement,
    ChalkNodeTranslationUnit,
    ChalkNodeExternalDeclaration,
    ChalkNodeIdentifierList,
    ChalkNodeFunctionDefinition,
    ChalkNodeEnd
} CHALK_NODE_TYPE, *PCHALK_NODE_TYPE;

typedef enum _CHALK_OBJECT_TYPE {
    ChalkObjectInvalid,
    ChalkObjectInteger,
    ChalkObjectString,
    ChalkObjectDict,
    ChalkObjectList,
    ChalkObjectFunction,
    ChalkObjectCount
} CHALK_OBJECT_TYPE, *PCHALK_OBJECT_TYPE;

typedef union _CHALK_OBJECT CHALK_OBJECT, *PCHALK_OBJECT;

/*++

Structure Description:

    This structure stores the data for a Chalk script (text).

Members:

    ListEntry - Stores pointers to the next and previous entries on the list of
        scripts loaded in the interpreter.

    Path - Stores a pointer to the file path for printing errors.

    Data - Stores a pointer to the script data.

    Size - Stores the size of the script data in bytes.

    ParseTree - Stores a pointer to the parse tree for this script.

    Order - Stores the order identifier of the script.

--*/

typedef struct _CHALK_SCRIPT {
    LIST_ENTRY ListEntry;
    PSTR Path;
    PSTR Data;
    ULONG Size;
    PVOID ParseTree;
    ULONG Order;
} CHALK_SCRIPT, *PCHALK_SCRIPT;

/*++

Structure Description:

    This structure stores the common header for interpreter objects.

Members:

    ReferenceCount - Stores the reference count on the object.

--*/

typedef struct _CHALK_OBJECT_HEADER {
    CHALK_OBJECT_TYPE Type;
    ULONG ReferenceCount;
} CHALK_OBJECT_HEADER, *PCHALK_OBJECT_HEADER;

/*++

Structure Description:

    This structure stores the data for a Chalk integer object.

Members:

    Header - Stores the common object header.

    Value - Stores the integer value, as a 64-bit signed integer.

--*/

typedef struct _CHALK_INT {
    CHALK_OBJECT_HEADER Header;
    LONGLONG Value;
} CHALK_INT, *PCHALK_INT;

/*++

Structure Description:

    This structure stores the data for a Chalk string object.

Members:

    Header - Stores the common object header.

    String - Stores the string value.

    Size - Stores the size of the string, not including the null terminator.

--*/

typedef struct _CHALK_STRING {
    CHALK_OBJECT_HEADER Header;
    PSTR String;
    ULONG Size;
} CHALK_STRING, *PCHALK_STRING;

/*++

Structure Description:

    This structure stores the data for a Chalk list object.

Members:

    Header - Stores the common object header.

    Array - Stores the list of elements.

    Count - Stores the number of elements in the list.

--*/

typedef struct _CHALK_LIST {
    CHALK_OBJECT_HEADER Header;
    PCHALK_OBJECT *Array;
    UINTN Count;
} CHALK_LIST, *PCHALK_LIST;

/*++

Structure Description:

    This structure stores an entry in a dictionary.

Members:

    ListEntry - Stores pointers to the next and previous dictionary entries
        in the dictionary.

    Key - Stores a pointer to the key for the entry.

    Value - Stores a pointer to the value for the entry.

--*/

typedef struct _CHALK_DICT_ENTRY {
    LIST_ENTRY ListEntry;
    PCHALK_OBJECT Key;
    PCHALK_OBJECT Value;
} CHALK_DICT_ENTRY, *PCHALK_DICT_ENTRY;

/*++

Structure Description:

    This structure stores the data for a Chalk list object.

Members:

    Header - Stores the common object header.

    EntryList - Stores the head of the list of CHALK_DICT_ENTRY entries.

--*/

typedef struct _CHALK_DICT {
    CHALK_OBJECT_HEADER Header;
    LIST_ENTRY EntryList;
} CHALK_DICT, *PCHALK_DICT;

/*++

Structure Description:

    This structure stores the data for a Chalk reference (to another object).

Members:

    Header - Stores the common object header.

    Value - Stores the pointer to the object referred to.

--*/

typedef struct _CHALK_REFERENCE {
    CHALK_OBJECT_HEADER Header;
    PCHALK_OBJECT Value;
} CHALK_REFERENCE, *PCHALK_REFERENCE;

/*++

Structure Description:

    This structure stores the data for a Chalk function object.

Members:

    Header - Stores the common object header.

    Arguments - Stores a pointer to a list of argument names for this function.

    Body - Stores a pointer to the Abstract Syntax Tree for the body of this
        function. This is opaque, but is currently of type PPARSER_NODE.

    Script - Stores a pointer to the script the function resides in.

--*/

typedef struct _CHALK_FUNCTION {
    CHALK_OBJECT_HEADER Header;
    PCHALK_OBJECT Arguments;
    PVOID Body;
    PCHALK_SCRIPT Script;
} CHALK_FUNCTION, *PCHALK_FUNCTION;

/*++

Structure Description:

    This union describes the entire storage size needed for any Chalk object
    type.

Members:

    Header - Stores the common header (also available in each of the
        specialized types).

    Integer - Stores the integer representation.

    String - Stores the string representation.

    List - Stores the list representation.

    Dict - Stores the dict representation.

    Reference - Stores the reference representation.

    Function - Stores the function representation.

--*/

union _CHALK_OBJECT {
    CHALK_OBJECT_HEADER Header;
    CHALK_INT Integer;
    CHALK_STRING String;
    CHALK_LIST List;
    CHALK_DICT Dict;
    CHALK_REFERENCE Reference;
    CHALK_FUNCTION Function;
};

//
// -------------------------------------------------------------------- Globals
//

extern PSTR ChalkObjectTypeNames[ChalkObjectCount];

//
// -------------------------------------------------------- Function Prototypes
//

PCHALK_OBJECT
ChalkCreateInteger (
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

PCHALK_OBJECT
ChalkCreateString (
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
ChalkStringAdd (
    PCHALK_OBJECT Left,
    PCHALK_OBJECT Right,
    PCHALK_OBJECT *Result
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

PCHALK_OBJECT
ChalkCreateList (
    PCHALK_OBJECT *InitialValues,
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

PCHALK_OBJECT
ChalkListLookup (
    PCHALK_OBJECT List,
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
ChalkListSetElement (
    PCHALK_OBJECT ListObject,
    ULONG Index,
    PCHALK_OBJECT Object
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
ChalkListAdd (
    PCHALK_OBJECT Destination,
    PCHALK_OBJECT Addition
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

PCHALK_OBJECT
ChalkCreateDict (
    PCHALK_OBJECT Source
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
ChalkDictSetElement (
    PCHALK_OBJECT DictObject,
    PCHALK_OBJECT Key,
    PCHALK_OBJECT Value,
    PCHALK_OBJECT **LValue
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

PCHALK_DICT_ENTRY
ChalkDictLookup (
    PCHALK_OBJECT DictObject,
    PCHALK_OBJECT Key
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
ChalkDictAdd (
    PCHALK_OBJECT Destination,
    PCHALK_OBJECT Addition
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

PCHALK_OBJECT
ChalkCreateFunction (
    PCHALK_OBJECT Arguments,
    PVOID Body,
    PCHALK_SCRIPT Script
    );

/*++

Routine Description:

    This routine creates a new function object.

Arguments:

    Arguments - Supplies a pointer to a list containing the arguments for the
        function. A reference is added, and this list is used directly.

    Body - Supplies a pointer to the Abstract Syntax Tree node representing the
        body of the function (what to execute when the function is called).
        This is opaque, but is currently of type PPARSER_NODE.

    Script - Supplies a pointer to the script the function is defined in.

Return Value:

    Returns a pointer to the new object on success.

    NULL on failure.

--*/

PCHALK_OBJECT
ChalkObjectCopy (
    PCHALK_OBJECT Source
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
ChalkObjectGetBooleanValue (
    PCHALK_OBJECT Object
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
ChalkObjectAddReference (
    PCHALK_OBJECT Object
    );

/*++

Routine Description:

    This routine adds a reference to the given Chalk object.

Arguments:

    Object - Supplies a pointer to the object to add a reference to.

Return Value:

    None.

--*/

VOID
ChalkObjectReleaseReference (
    PCHALK_OBJECT Object
    );

/*++

Routine Description:

    This routine releases a reference from the given Chalk object. If the
    reference count its zero, the object is destroyed.

Arguments:

    Object - Supplies a pointer to the object to release.

Return Value:

    None.

--*/

VOID
ChalkPrintObject (
    PCHALK_OBJECT Object,
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

