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

--*/

typedef struct _SETUP_SCRIPT {
    LIST_ENTRY ListEntry;
    PSTR Path;
    PSTR Data;
    ULONG Size;
    PVOID ParseTree;
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

--*/

struct _SETUP_NODE {
    PSETUP_NODE Parent;
    PSETUP_SCOPE BaseScope;
    PVOID ParseNode;
    ULONG ChildIndex;
    PSETUP_SCRIPT Script;
    PSETUP_OBJECT *Results;
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
    ULONG Size
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

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
SetupLoadScriptFile (
    PSETUP_INTERPRETER Interpreter,
    PSTR Path
    );

/*++

Routine Description:

    This routine executes the given interpreted script from a file.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Path - Supplies a pointer to the path of the file to load.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PSETUP_OBJECT
SetupGetVariable (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_OBJECT Name
    );

/*++

Routine Description:

    This routine attempts to find a variable by the given name.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Name - Supplies a pointer to the name string to find.

Return Value:

    Returns a pointer to the variable value with an increased reference count
    on success.

    NULL if no such variable exists.

--*/

INT
SetupSetVariable (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_OBJECT Name,
    PSETUP_OBJECT Value
    );

/*++

Routine Description:

    This routine sets or creates a new variable in the current scope.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Name - Supplies a pointer to the name string to create.

    Value - Supplies a pointer to the variable value.

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

