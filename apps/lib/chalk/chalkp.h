/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    chalkp.h

Abstract:

    This header contains internal definitions for the Chalk interpreter. This
    file should not be included by users of the interpreter, only by the
    interpreter core itself.

Author:

    Evan Green 19-Nov-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// The YY library is expected to be linked in to this one.
//

#define YY_API

#include <minoca/types.h>
#include <minoca/status.h>
#include <minoca/lib/yy.h>
#include "chalk.h"

//
// --------------------------------------------------------------------- Macros
//

//
// Define the set of types that are to be passed by reference. Functions are
// passed by reference for efficiency but are immutable.
//

#define CHALK_PASS_BY_REFERENCE(_ObjectType)    \
    (((_ObjectType) == ChalkObjectList) ||      \
     ((_ObjectType) == ChalkObjectDict) ||      \
     ((_ObjectType) == ChalkObjectFunction))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

extern PARSER_GRAMMAR_ELEMENT ChalkGrammar[];
extern CHALK_FUNCTION_PROTOTYPE ChalkBuiltinFunctions[];

//
// -------------------------------------------------------- Function Prototypes
//

INT
ChalkPushNode (
    PCHALK_INTERPRETER Interpreter,
    PVOID ParseTree,
    PCHALK_SCRIPT Script,
    BOOL Function
    );

/*++

Routine Description:

    This routine pushes a new node onto the current interpreter execution.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    ParseTree - Supplies a pointer to the parse tree of the new node.

    Script - Supplies a pointer to the script this tree came from.

    Function - Supplies a boolean indicating if this is a function scope or
        not.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

VOID
ChalkPopNode (
    PCHALK_INTERPRETER Interpreter
    );

/*++

Routine Description:

    This routine pops the current node off the execution stack.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

Return Value:

    None.

--*/

INT
ChalkInvokeFunction (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Function,
    PCHALK_OBJECT ArgumentList,
    PCHALK_OBJECT *Result
    );

/*++

Routine Description:

    This routine pushes a new function invocation on the interpreter stack.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Function - Supplies a pointer to the function object to execute.

    ArgumentList - Supplies a pointer to the argument values.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
ChalkParseScript (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_SCRIPT Script,
    PVOID *TranslationUnit
    );

/*++

Routine Description:

    This routine lexes and parses the given script data.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Script - Supplies a pointer to the script to parse.

    TranslationUnit - Supplies a pointer where the translation unit will be
        returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PSTR
ChalkGetNodeGrammarName (
    PCHALK_NODE Node
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

KSTATUS
ChalkLexGetToken (
    PVOID Context,
    PLEXER_TOKEN Token
    );

/*++

Routine Description:

    This routine gets the next token for the parser.

Arguments:

    Context - Supplies a context pointer initialized in the parser.

    Token - Supplies a pointer where the next token will be filled out and
        returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_END_OF_FILE if the file end was reached.

    STATUS_MALFORMED_DATA_STREAM if the given input matched no rule in the
    lexer and the lexer was not configured to ignore such things.

--*/

