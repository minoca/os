/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    interp.h

Abstract:

    This header contains definitions for the interpreter built into the setup
    application.

Author:

    Evan Green 14-Oct-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "obj.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the data for a setup script (text).

Members:

    ListEntry - Stores pointers to the next and previous entries on the list of
        scripts loaded in the interpreter.

    Path - Stores a pointer to the file path for printing errors.

    Data - Stores a pointer to the script data.

    Size - Stores the size of the script data in bytes.

    ParseTree - Stores a pointer to the parse tree for this script.

    Order - Stores the order identifier of the script.

--*/

typedef struct _SETUP_SCRIPT {
    LIST_ENTRY ListEntry;
    PSTR Path;
    PSTR Data;
    ULONG Size;
    PVOID ParseTree;
    ULONG Order;
} SETUP_SCRIPT, *PSETUP_SCRIPT;

typedef struct _SETUP_SCOPE SETUP_SCOPE, *PSETUP_SCOPE;

/*++

Structure Description:

    This structure stores a scope for the interpreter.

Members:

    Parent - Stores a pointer to the parent scope.

    Dict - Stores a pointer to the dictionary of variables visible in this
        scope.

    Function - Stores a boolean indicating whether this is a function scope
        or a brace-based scope (ie whether or not to continue looking upstream
        for variables).

--*/

struct _SETUP_SCOPE {
    PSETUP_SCOPE Parent;
    PSETUP_OBJECT Dict;
    BOOL Function;
};

typedef struct _SETUP_NODE SETUP_NODE, *PSETUP_NODE;

/*++

Structure Description:

    This structure stores an interpreter execution context node.

Members:

    Parent - Stores a pointer to the parent node.

    BaseScope - Stores a pointer to the base scope for this execution unit.

    ParseNode - Stores a pointer to the parser element being executed.

    ChildIndex - Stores the index of the child node to execute next.

    Script - Stores a pointer to the script input this node came from.

    Results - Stores the evaluation of intermediate items found while
        processing this node.

    LValue - Stores the pointer where the pointer to the first result is
        stored. This is used in assignments (so that if mylist[0] is assigned
        to, this pointer points at the list array element to set).

--*/

struct _SETUP_NODE {
    PSETUP_NODE Parent;
    PSETUP_SCOPE BaseScope;
    PVOID ParseNode;
    ULONG ChildIndex;
    PSETUP_SCRIPT Script;
    PSETUP_OBJECT *Results;
    PSETUP_OBJECT *LValue;
};

/*++

Structure Description:

    This structure stores the interpreter context in the setup application.

Members:

    Global - Stores the global scope.

    Scope - Stores a pointer to the current scope.

    Node - Stores a pointer to the current execution context.

    NodeDepth - Stores the depth of nodes being executed.

    ScriptList - Stores the head of the list of scripts loaded.

--*/

typedef struct _SETUP_INTERPRETER {
    SETUP_SCOPE Global;
    PSETUP_SCOPE Scope;
    PSETUP_NODE Node;
    ULONG NodeDepth;
    LIST_ENTRY ScriptList;
} SETUP_INTERPRETER, *PSETUP_INTERPRETER;

//
// Data types for interfacing the interpreter with the C language.
//

typedef enum _SETUP_C_TYPE {
    SetupCTypeInvalid,
    SetupCInt8,
    SetupCUint8,
    SetupCInt16,
    SetupCUint16,
    SetupCInt32,
    SetupCUint32,
    SetupCInt64,
    SetupCUint64,
    SetupCString,
    SetupCByteArray,
    SetupCFlag32,
    SetupCSubStructure,
    SetupCStructurePointer,
} SETUP_C_TYPE, *PSETUP_C_TYPE;

typedef struct _SETUP_C_STRUCTURE_MEMBER
    SETUP_C_STRUCTURE_MEMBER, *PSETUP_C_STRUCTURE_MEMBER;

/*++

Structure Description:

    This structure stores the conversion information between a dictionary
    element and a C structure member.

Members:

    Type - Stores the C data type at the structure member offset.

    Key - Stores a pointer to the key in the dictionary this member matches.

    Offset - Stores the offset from the base of the structure where this member
        resides.

    Required - Stores a boolean indicating whether or not this structure member
        is required to be in the dictionary (when converting from a dictionary
        to a structure).

    Mask - Stores the mask to use if this is a flags value. The value will be
        shifted over by the number of trailing zeros in the mask, and then
        masked with the value itself.

    Size - Stores the maximum number of bytes if this is a byte array.

    SubStructure - Stores a pointer to the substructure member arrays if this
        is a structure pointer.

--*/

struct _SETUP_C_STRUCTURE_MEMBER {
    SETUP_C_TYPE Type;
    PSTR Key;
    ULONG Offset;
    BOOL Required;
    union {
        UINTN Mask;
        UINTN Size;
        PSETUP_C_STRUCTURE_MEMBER SubStructure;
    } U;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

INT
SetupInitializeInterpreter (
    PSETUP_INTERPRETER Interpreter
    );

/*++

Routine Description:

    This routine initializes a setup interpreter.

Arguments:

    Interpreter - Supplies a pointer to the interpreter to initialize.

Return Value:

    None.

--*/

VOID
SetupDestroyInterpreter (
    PSETUP_INTERPRETER Interpreter
    );

/*++

Routine Description:

    This routine destroys a setup interpreter.

Arguments:

    Interpreter - Supplies a pointer to the interpreter to destroy.

Return Value:

    None.

--*/

INT
SetupLoadScriptBuffer (
    PSETUP_INTERPRETER Interpreter,
    PSTR Path,
    PSTR Buffer,
    ULONG Size,
    ULONG Order
    );

/*++

Routine Description:

    This routine executes the given interpreted script.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Path - Supplies a pointer to a string describing where this script came
        from. This pointer is used directly and should stick around while the
        script is loaded.

    Buffer - Supplies a pointer to the buffer containing the script to
        execute. A copy of this buffer is created.

    Size - Supplies the size of the buffer in bytes. A null terminator will be
        added if one is not present.

    Order - Supplies the order identifier for ordering which scripts should run
        when. Supply 0 to run the script now.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
SetupLoadScriptFile (
    PSETUP_INTERPRETER Interpreter,
    PSTR Path,
    ULONG Order
    );

/*++

Routine Description:

    This routine executes the given interpreted script from a file.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Path - Supplies a pointer to the path of the file to load.

    Order - Supplies the order identifier for ordering which scripts should run
        when. Supply 0 to run the script now.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
SetupExecuteDeferredScripts (
    PSETUP_INTERPRETER Interpreter,
    ULONG Order
    );

/*++

Routine Description:

    This routine executes scripts that have been loaded but not yet run.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Order - Supplies the order identifier. Any scripts with this order
        identifier that have not yet been run will be run now.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PSETUP_OBJECT
SetupGetVariable (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_OBJECT Name,
    PSETUP_OBJECT **LValue
    );

/*++

Routine Description:

    This routine attempts to find a variable by the given name.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Name - Supplies a pointer to the name string to find.

    LValue - Supplies an optional pointer where an LValue pointer will be
        returned on success. The caller can use the return of this pointer to
        assign into the dictionary element later.

Return Value:

    Returns a pointer to the variable value with an increased reference count
    on success.

    NULL if no such variable exists.

--*/

INT
SetupSetVariable (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_OBJECT Name,
    PSETUP_OBJECT Value,
    PSETUP_OBJECT **LValue
    );

/*++

Routine Description:

    This routine sets or creates a new variable in the current scope.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Name - Supplies a pointer to the name string to create.

    Value - Supplies a pointer to the variable value.

    LValue - Supplies an optional pointer where an LValue pointer will be
        returned on success. The caller can use the return of this pointer to
        assign into the dictionary element later.

Return Value:

    Returns a pointer to the variable value with an increased reference count
    on success. The caller should release this reference when finished.

    NULL if no such variable exists.

--*/

INT
SetupParseScript (
    PSETUP_SCRIPT Script,
    PVOID *TranslationUnit
    );

/*++

Routine Description:

    This routine lexes and parses the given script data.

Arguments:

    Script - Supplies a pointer to the script to parse.

    TranslationUnit - Supplies a pointer where the translation unit will be
        returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

VOID
SetupDestroyParseTree (
    PVOID TranslationUnit
    );

/*++

Routine Description:

    This routine destroys the translation unit returned when a script was
    parsed.

Arguments:

    TranslationUnit - Supplies a pointer to the translation unit to destroy.

Return Value:

    None.

--*/

PSTR
SetupGetNodeGrammarName (
    PSETUP_NODE Node
    );

/*++

Routine Description:

    This routine returns the grammatical element name for the given node.

Arguments:

    Node - Supplies a pointer to the node.

Return Value:

    Returns a pointer to a constant string of the name of the grammar element
    represented by this execution node.

--*/

//
// C interface support functions
//

INT
SetupConvertDictToStructure (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_OBJECT Dict,
    PSETUP_C_STRUCTURE_MEMBER Members,
    PVOID Structure
    );

/*++

Routine Description:

    This routine converts the contents of a dictionary into a C structure in
    a mechanical way.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Dict - Supplies a pointer to the dictionary.

    Members - Supplies a pointer to an array of structure member definitions.
        This array must be terminated by a zeroed out member (type invalid,
        key null).

    Structure - Supplies a pointer to the structure to set up according to the
        dictionary. Any strings created by this function, even in failure,
        must be freed by the caller.

Return Value:

    0 on success.

    ENOENT if a required member was not found in the dictionary.

    Returns an error number on failure.

--*/

INT
SetupConvertStructureToDict (
    PSETUP_INTERPRETER Interpreter,
    PVOID Structure,
    PSETUP_C_STRUCTURE_MEMBER Members,
    PSETUP_OBJECT Dict
    );

/*++

Routine Description:

    This routine converts the contents of a C structure into a dictionary in a
    mechanical way.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Structure - Supplies a pointer to the structure to get values from. Copies
        of any strings within the structure are made, and do not need to be
        preserved after this function returns.

    Members - Supplies a pointer to an array of structure member definitions.
        This array must be terminated by a zeroed out member (type invalid,
        key null).

    Dict - Supplies a pointer to the dictionary to set members for.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
SetupReadStringsList (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_OBJECT List,
    PSTR **StringsArray
    );

/*++

Routine Description:

    This routine converts a list of strings into an array of null-terminated
    C strings. Items that are not strings are ignored.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    List - Supplies a pointer to the list.

    StringsArray - Supplies a pointer where the array of strings will be
        returned. This will be one giant allocation, the caller simply needs to
        free the array itself to free all the internal strings.

Return Value:

    0 on success.

    EINVAL if the given object was not a list or contained something other than
    strings.

    ENOMEM on allocation failure.

--*/

INT
SetupWriteStringsList (
    PSETUP_INTERPRETER Interpreter,
    PSTR *StringsArray,
    PSETUP_OBJECT List
    );

/*++

Routine Description:

    This routine converts an array of C strings into a list of string objects.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    StringsArray - Supplies a pointer to a NULL-terminated array of C strings.

    List - Supplies a pointer to the list to add the strings to.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

PSETUP_OBJECT
SetupDictLookupCStringKey (
    PSETUP_OBJECT Dict,
    PSTR Key
    );

/*++

Routine Description:

    This routine looks up a dictionary object with the given C string key.

Arguments:

    Dict - Supplies a pointer to the dictionary to look in.

    Key - Supplies a pointer to the key string to look up.

Return Value:

    Returns a pointer to the value object for the given key on success. Note
    that the reference count on this object is not increased.

    NULL if no value for the given key exists.

--*/

