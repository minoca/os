/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    chalk.h

Abstract:

    This header contains definitions for the Chalk interpreter.

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

typedef struct _CHALK_SCOPE CHALK_SCOPE, *PCHALK_SCOPE;

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

struct _CHALK_SCOPE {
    PCHALK_SCOPE Parent;
    PCHALK_OBJECT Dict;
    BOOL Function;
};

typedef struct _CHALK_NODE CHALK_NODE, *PCHALK_NODE;

/*++

Structure Description:

    This structure stores an interpreter execution context node.

Members:

    Parent - Stores a pointer to the parent node.

    BaseScope - Stores a pointer to the base scope for this execution unit.

    ParseNode - Stores a pointer to the parser element being executed.

    ChildIndex - Stores the index of the child node to evaluate next.

    Script - Stores a pointer to the script input this node came from.

    Results - Stores the evaluation of intermediate items found while
        processing this node.

    Data - Stores a pointer's worth of additinal data, for example the
        iteration context in an interation statement.

--*/

struct _CHALK_NODE {
    PCHALK_NODE Parent;
    PCHALK_SCOPE BaseScope;
    PVOID ParseNode;
    ULONG ChildIndex;
    PCHALK_SCRIPT Script;
    PCHALK_OBJECT *Results;
    PVOID Data;
};

/*++

Structure Description:

    This structure stores the interpreter context in the Chalk application.

Members:

    Global - Stores the global scope.

    Scope - Stores a pointer to the current scope.

    Node - Stores a pointer to the current execution context.

    NodeDepth - Stores the depth of nodes being executed.

    ScriptList - Stores the head of the list of scripts loaded.

    LValue - Stores the last LValue pointer retrieved. This is used during
        assignments to know how to set the value of a dictionary element, list,
        or variable (which is really just another dict).

    Generation - Stores the interpreter generation number. This is incremented
        whenever the interpreter context is cleared.

--*/

struct _CHALK_INTERPRETER {
    CHALK_SCOPE Global;
    PCHALK_SCOPE Scope;
    PCHALK_NODE Node;
    ULONG NodeDepth;
    LIST_ENTRY ScriptList;
    PCHALK_OBJECT *LValue;
    ULONG Generation;
};

//
// Data types for interfacing the interpreter with the C language.
//

typedef enum _CHALK_C_TYPE {
    ChalkCTypeInvalid,
    ChalkCInt8,
    ChalkCUint8,
    ChalkCInt16,
    ChalkCUint16,
    ChalkCInt32,
    ChalkCUint32,
    ChalkCInt64,
    ChalkCUint64,
    ChalkCString,
    ChalkCByteArray,
    ChalkCFlag32,
    ChalkCSubStructure,
    ChalkCStructurePointer,
    ChalkCObjectPointer,
} CHALK_C_TYPE, *PCHALK_C_TYPE;

typedef struct _CHALK_C_STRUCTURE_MEMBER
    CHALK_C_STRUCTURE_MEMBER, *PCHALK_C_STRUCTURE_MEMBER;

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

struct _CHALK_C_STRUCTURE_MEMBER {
    CHALK_C_TYPE Type;
    PSTR Key;
    ULONG Offset;
    BOOL Required;
    union {
        UINTN Mask;
        UINTN Size;
        PCHALK_C_STRUCTURE_MEMBER SubStructure;
    } U;
};

/*++

Structure Description:

    This structure stores the short list of information for a Chalk C function.

Members:

    Name - Stores a pointer to the function name.

    ArgumentNames - Stores a pointer to a null-terminated array of argument
        names.

    Function - Stores a pointer to the C function to call.

--*/

typedef struct _CHALK_FUNCTION_PROTOTYPE {
    PSTR Name;
    PSTR *ArgumentNames;
    PCHALK_C_FUNCTION Function;
} CHALK_FUNCTION_PROTOTYPE, *PCHALK_FUNCTION_PROTOTYPE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

INT
ChalkInitializeInterpreter (
    PCHALK_INTERPRETER Interpreter
    );

/*++

Routine Description:

    This routine initializes a Chalk interpreter.

Arguments:

    Interpreter - Supplies a pointer to the interpreter to initialize.

Return Value:

    None.

--*/

VOID
ChalkDestroyInterpreter (
    PCHALK_INTERPRETER Interpreter
    );

/*++

Routine Description:

    This routine destroys a Chalk interpreter.

Arguments:

    Interpreter - Supplies a pointer to the interpreter to destroy.

Return Value:

    None.

--*/

INT
ChalkClearInterpreter (
    PCHALK_INTERPRETER Interpreter
    );

/*++

Routine Description:

    This routine clears the global variable scope back to its original state
    for the given Chalk interpreter. Loaded scripts are still saved, but the
    interpreter state is as if they had never been executed.

Arguments:

    Interpreter - Supplies a pointer to the interpreter to reset.

Return Value:

    0 on success.

    Returns an error number if the context could not be fully reinitialized.

--*/

INT
ChalkLoadScriptBuffer (
    PCHALK_INTERPRETER Interpreter,
    PSTR Path,
    PSTR Buffer,
    ULONG Size,
    ULONG Order,
    PCHALK_OBJECT *ReturnValue
    );

/*++

Routine Description:

    This routine executes the given interpreted script.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Path - Supplies a pointer to a string describing where this script came
        from. A copy of this string is made.

    Buffer - Supplies a pointer to the buffer containing the script to
        execute. A copy of this buffer is created.

    Size - Supplies the size of the buffer in bytes. A null terminator will be
        added if one is not present.

    Order - Supplies the order identifier for ordering which scripts should run
        when. Supply 0 to run the script now.

    ReturnValue - Supplies an optional pointer where the return value from
        the script will be returned. It is the caller's responsibility to
        release the object. This is only filled in if the order is zero
        (so the script is executed now).

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
ChalkLoadScriptFile (
    PCHALK_INTERPRETER Interpreter,
    PSTR Path,
    ULONG Order,
    PCHALK_OBJECT *ReturnValue
    );

/*++

Routine Description:

    This routine executes the given interpreted script from a file.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Path - Supplies a pointer to the path of the file to load. A copy of this
        string is made.

    Order - Supplies the order identifier for ordering which scripts should run
        when. Supply 0 to run the script now.

    ReturnValue - Supplies an optional pointer where the return value from
        the script will be returned. It is the caller's responsibility to
        release the object. This is only filled in if the order is zero
        (so the script is executed now).

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
ChalkExecuteDeferredScripts (
    PCHALK_INTERPRETER Interpreter,
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

INT
ChalkUnloadScriptsByOrder (
    PCHALK_INTERPRETER Interpreter,
    ULONG Order
    );

/*++

Routine Description:

    This routine unloads all scripts of a given order. It also resets the
    interpreter context.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Order - Supplies the order identifier. Any scripts with this order
        identifier will be unloaded. Supply 0 to unload all scripts.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PCHALK_OBJECT
ChalkGetVariable (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Name,
    PCHALK_OBJECT **LValue
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
ChalkSetVariable (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Name,
    PCHALK_OBJECT Value,
    PCHALK_OBJECT **LValue
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

//
// C interface support functions
//

INT
ChalkConvertDictToStructure (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Dict,
    PCHALK_C_STRUCTURE_MEMBER Members,
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
ChalkConvertStructureToDict (
    PCHALK_INTERPRETER Interpreter,
    PVOID Structure,
    PCHALK_C_STRUCTURE_MEMBER Members,
    PCHALK_OBJECT Dict
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
ChalkReadStringsList (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT List,
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
ChalkWriteStringsList (
    PCHALK_INTERPRETER Interpreter,
    PSTR *StringsArray,
    PCHALK_OBJECT List
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

PCHALK_OBJECT
ChalkDictLookupCStringKey (
    PCHALK_OBJECT Dict,
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

PCHALK_OBJECT
ChalkCGetVariable (
    PCHALK_INTERPRETER Interpreter,
    PSTR Name
    );

/*++

Routine Description:

    This routine looks up a variable or function parameter corresponding to the
    given C string name.

Arguments:

    Interpreter - Supplies a pointer to the interpreter state.

    Name - Supplies a pointer to the NULL-terminated case sensitive name of the
        variable or parameter to look up.

Return Value:

    Returns a pointer to the value object for the given key on success. Note
    that the reference count on this object is not increased.

    NULL if no value for the given key exists.

--*/

INT
ChalkRegisterFunctions (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_FUNCTION_PROTOTYPE Prototypes
    );

/*++

Routine Description:

    This routine registers several new C functions with the Chalk interpreter.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Context - Supplies a pointer's worth of context to pass to the C functions
        when they are called.

    Prototypes - Supplies a pointer to an array of prototypes. Terminate the
        array with an entry whose name is NULL.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
ChalkRegisterFunction (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_FUNCTION_PROTOTYPE Prototype
    );

/*++

Routine Description:

    This routine registers a new Chalk C function in the current context.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Context - Supplies a pointer's worth of context to pass to the C function
        when it is called.

    Prototype - Supplies a pointer to the prototype information.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

//
// Utility functions
//

PVOID
ChalkAllocate (
    size_t Size
    );

/*++

Routine Description:

    This routine dynamically allocates memory for the Chalk interpreter.

Arguments:

    Size - Supplies the number of bytes to allocate.

Return Value:

    Returns a pointer to the memory on success.

    NULL on allocation failure.

--*/

PVOID
ChalkReallocate (
    PVOID Allocation,
    size_t Size
    );

/*++

Routine Description:

    This routine reallocates a previously allocated dynamic memory chunk,
    changing its size.

Arguments:

    Allocation - Supplies the previous allocation.

    Size - Supplies the number of bytes to allocate.

Return Value:

    Returns a pointer to the memory on success.

    NULL on allocation failure.

--*/

VOID
ChalkFree (
    PVOID Allocation
    );

/*++

Routine Description:

    This routine releases dynamically allocated memory back to the system.

Arguments:

    Allocation - Supplies a pointer to the allocation received from the
        allocate function.

Return Value:

    None.

--*/

VOID
ChalkPrintAllocations (
    VOID
    );

/*++

Routine Description:

    This routine prints any outstanding Chalk allocations.

Arguments:

    None.

Return Value:

    None.

--*/

