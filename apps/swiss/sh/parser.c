/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    parser.c

Abstract:

    This module implements the shell grammar parser.

Author:

    Evan Green 5-Jun-2013

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "sh.h"
#include "shparse.h"
#include "../swlib.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
ShParseCompleteCommand (
    PSHELL Shell,
    PSHELL_NODE *CompleteCommandNode
    );

PSHELL_NODE
ShParseList (
    PSHELL Shell
    );

PSHELL_NODE
ShParseAndOr (
    PSHELL Shell
    );

PSHELL_NODE
ShParsePipeline (
    PSHELL Shell
    );

PSHELL_NODE
ShParseCommand (
    PSHELL Shell
    );

PSHELL_NODE
ShParseCompoundCommand (
    PSHELL Shell
    );

PSHELL_NODE
ShParseBraceGroup (
    PSHELL Shell
    );

PSHELL_NODE
ShParseSubshell (
    PSHELL Shell
    );

PSHELL_NODE
ShParseIf (
    PSHELL Shell
    );

PSHELL_NODE
ShParseWhileOrUntil (
    PSHELL Shell
    );

PSHELL_NODE
ShParseFor (
    PSHELL Shell
    );

PSHELL_NODE
ShParseCase (
    PSHELL Shell
    );

BOOL
ShParsePattern (
    PSHELL Shell,
    PSHELL_NODE Case,
    PSHELL_CASE_PATTERN_SET *NewPatternSet
    );

PSHELL_NODE
ShParseDoGroup (
    PSHELL Shell
    );

PSHELL_NODE
ShParseCompoundList (
    PSHELL Shell
    );

PSHELL_NODE
ShParseTerm (
    PSHELL Shell
    );

PSHELL_NODE
ShParseSimpleCommandOrFunction (
    PSHELL Shell
    );

PSHELL_NODE
ShParseSimpleCommand (
    PSHELL Shell,
    PSTR FirstWord,
    UINTN FirstWordSize
    );

PSHELL_NODE
ShParseFunctionDefinition (
    PSHELL Shell,
    PSTR FunctionName,
    UINTN FunctionNameSize
    );

BOOL
ShParseOptionalRedirectList (
    PSHELL Shell,
    PSHELL_NODE Node
    );

BOOL
ShParseRedirection (
    PSHELL Shell,
    PSHELL_NODE Node
    );

BOOL
ShParseAssignment (
    PSHELL Shell,
    PSHELL_NODE Node
    );

BOOL
ShParseLineBreak (
    PSHELL Shell,
    BOOL Required,
    BOOL FirstCommandWord
    );

BOOL
ShParseSeparator (
    PSHELL Shell,
    PCHAR Separator
    );

BOOL
ShParseSeparatorOp (
    PSHELL Shell,
    PCHAR Separator
    );

PSHELL_NODE
ShCreateNode (
    PSHELL Shell,
    SHELL_NODE_TYPE Type
    );

VOID
ShDeleteNode (
    PSHELL_NODE Node
    );

VOID
ShPrintNode (
    PSHELL Shell,
    PSHELL_NODE Node,
    ULONG Depth
    );

BOOL
ShCreateRedirection (
    PSHELL Shell,
    PSHELL_NODE Node,
    SHELL_IO_REDIRECTION_TYPE Type,
    INT FileNumber,
    PSTR FileName,
    UINTN FileNameSize
    );

VOID
ShDestroyRedirection (
    PSHELL_IO_REDIRECT Redirect
    );

BOOL
ShCreateAssignment (
    PSHELL_NODE Node,
    PSTR Name,
    UINTN NameSize,
    PSTR Value,
    UINTN ValueSize
    );

VOID
ShDestroyAssignment (
    PSHELL_ASSIGNMENT Assignment
    );

BOOL
ShAddPatternToSet (
    PSHELL_CASE_PATTERN_SET Set,
    PSTR Pattern,
    UINTN PatternSize
    );

VOID
ShDestroyCasePatternList (
    PSHELL_NODE CaseNode
    );

BOOL
ShAddComponentToCommand (
    PSHELL_NODE Command,
    PSTR Component,
    UINTN ComponentSize
    );

BOOL
ShIsStringQuoted (
    PSTR String
    );

VOID
ShParseError (
    PSHELL Shell,
    PSHELL_NODE Node,
    PSTR Format,
    ...
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL ShDebugPrintParseTree = FALSE;

//
// ------------------------------------------------------------------ Functions
//

BOOL
ShParse (
    PSHELL Shell,
    PSHELL_NODE *Node
    )

/*++

Routine Description:

    This routine attempts to parse a complete command out of the shell input.

Arguments:

    Shell - Supplies a pointer to the shell object.

    Node - Supplies a pointer where the parsed node representing the command
        will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_NODE Command;
    BOOL Result;

    Command = NULL;

    //
    // Prime the lexer.
    //

    if (Shell->Lexer.LexerPrimed == FALSE) {
        Result = ShGetToken(Shell, TRUE);
        if (Result == FALSE) {
            goto ParseEnd;
        }

        Shell->Lexer.LexerPrimed = TRUE;
    }

    Result = ShParseCompleteCommand(Shell, &Command);
    if ((Result == FALSE) &&
        ((Shell->Options & SHELL_OPTION_INTERACTIVE) == 0)) {

        PRINT_ERROR("sh: Failed to parse command.\n");
    }

    if ((Command != NULL) && (ShDebugPrintParseTree != FALSE)) {
        ShPrintNode(Shell, Command, 0);
    }

ParseEnd:
    *Node = Command;
    return Result;
}

VOID
ShDestroyHereDocument (
    PSHELL_HERE_DOCUMENT HereDocument
    )

/*++

Routine Description:

    This routine destroys a here document. It assumes the here document is
    already removed from any list it was on.

Arguments:

    HereDocument - Supplies a pointer to the here document to destroy.

Return Value:

    None.

--*/

{

    if (HereDocument->EndWord != NULL) {
        free(HereDocument->EndWord);
    }

    if (HereDocument->Document != NULL) {
        free(HereDocument->Document);
    }

    free(HereDocument);
    return;
}

VOID
ShRetainNode (
    PSHELL_NODE Node
    )

/*++

Routine Description:

    This routine increases the reference count on a node.

Arguments:

    Node - Supplies a pointer to the node to retain.

Return Value:

    None.

--*/

{

    assert((Node->ReferenceCount > 0) && (Node->ReferenceCount < 0x1000));

    Node->ReferenceCount += 1;
    return;
}

VOID
ShReleaseNode (
    PSHELL_NODE Node
    )

/*++

Routine Description:

    This routine releases a reference on a shell node. If this causes the
    reference count to drop to zero then the node is destroyed.

Arguments:

    Node - Supplies a pointer to the node to release.

Return Value:

    None.

--*/

{

    assert((Node->ReferenceCount > 0) && (Node->ReferenceCount < 0x1000));

    Node->ReferenceCount -= 1;
    if (Node->ReferenceCount == 0) {
        ShDeleteNode(Node);
    }

    return;
}

BOOL
ShIsName (
    PSTR String,
    UINTN StringSize
    )

/*++

Routine Description:

    This routine determines if the given string is a valid NAME according to
    the shell grammar.

Arguments:

    String - Supplies a pointer to the candidate string.

    StringSize - Supplies the number of characters to check.

Return Value:

    TRUE if the string is a valid name.

    FALSE if the string is not a valid name.

--*/

{

    UCHAR Character;
    ULONG Index;

    if (StringSize == 0) {
        return FALSE;
    }

    //
    // The first character can be A through Z or an underscore.
    //

    Character = *String;
    if (!SHELL_NAME_FIRST_CHARACTER(Character)) {
        return FALSE;
    }

    for (Index = 1; Index < StringSize; Index += 1) {
        Character = String[Index];
        if (Character == '\0') {
            break;
        }

        if (!SHELL_NAME_CHARACTER(Character)) {
            return FALSE;
        }
    }

    return TRUE;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ShParseCompleteCommand (
    PSHELL Shell,
    PSHELL_NODE *CompleteCommandNode
    )

/*++

Routine Description:

    This routine attempts to parse a complete command out of the shell input.

Arguments:

    Shell - Supplies a pointer to the shell object.

    CompleteCommandNode - Supplies a pointer where the complete command will
        be returned on success. The caller is responsible for destroying this
        node.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_NODE List;
    BOOL Result;
    CHAR SeparatorOp;

    Result = FALSE;
    List = NULL;

    //
    // Get rid of any extraneous newlines.
    //

    while (Shell->Lexer.TokenType == '\n') {
        ShCheckForSignals(Shell);
        ShPrintPrompt(Shell, 1);
        Result = ShGetToken(Shell, TRUE);
        if (Result == FALSE) {
            goto ParseCompleteCommandEnd;
        }
    }

    if (Shell->Lexer.TokenType == TOKEN_END_OF_FILE) {
        Result = TRUE;
        goto ParseCompleteCommandEnd;
    }

    List = ShParseList(Shell);
    if (List == NULL) {
        Result = FALSE;
        goto ParseCompleteCommandEnd;
    }

    Result = ShParseSeparatorOp(Shell, &SeparatorOp);
    if ((Result != FALSE) && (SeparatorOp == '&')) {
        List->RunInBackground = TRUE;
    }

    Result = TRUE;

ParseCompleteCommandEnd:
    if (Result == FALSE) {
        if (List != NULL) {
            ShReleaseNode(List);
            List = NULL;
        }
    }

    *CompleteCommandNode = List;
    return Result;
}

PSHELL_NODE
ShParseList (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse a list out of the shell input.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE AndOr;
    PSHELL_NODE List;
    BOOL Result;
    CHAR SeparatorOp;

    Result = FALSE;
    List = NULL;

    //
    // Parse and-or statements separated by separator operators.
    //

    while (TRUE) {
        AndOr = ShParseAndOr(Shell);
        if (AndOr == NULL) {
            if (List == NULL) {
                Result = FALSE;
                ShParseError(Shell, NULL, "Unexpected token.");
                goto ParseListEnd;
            }

            break;
        }

        Result = ShParseSeparatorOp(Shell, &SeparatorOp);

        //
        // If this is the first time around and there's only one item, then
        // just return that item.
        //

        if ((Result == FALSE) && (List == NULL)) {
            List = AndOr;
            break;
        }

        //
        // There's more than one and-or. If the listhas yet to be made, make it
        // now.
        //

        if (List == NULL) {
            List = ShCreateNode(Shell, ShellNodeList);
            if (List == NULL) {
                goto ParseListEnd;
            }
        }

        INSERT_BEFORE(&(AndOr->SiblingListEntry), &(List->Children));

        //
        // Regardless of where the command was, if there's no separator, this
        // loop is done.
        //

        if (Result == FALSE) {
            break;
        }

        if (SeparatorOp == '&') {
            AndOr->RunInBackground = TRUE;
        }
    }

    AndOr = NULL;
    Result = TRUE;

ParseListEnd:
    if (AndOr != NULL) {
        ShReleaseNode(AndOr);
    }

    if (Result == FALSE) {
        if (List != NULL) {
            ShReleaseNode(List);
            List = NULL;
        }
    }

    return List;
}

PSHELL_NODE
ShParseAndOr (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse an and-or statement out of the shell input.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE AndOr;
    PSHELL_NODE Pipeline;
    BOOL Result;
    ULONG SeparatorToken;

    Result = FALSE;
    AndOr = NULL;

    //
    // Parse pipeline statements separated by separator operators.
    //

    while (TRUE) {
        Pipeline = ShParsePipeline(Shell);
        if (Pipeline == NULL) {
            goto ParseAndOrEnd;
        }

        SeparatorToken = Shell->Lexer.TokenType;

        //
        // If this is the first time around and there's only one item, then
        // just return that item.
        //

        if ((SeparatorToken != TOKEN_DOUBLE_AND) &&
            (SeparatorToken != TOKEN_DOUBLE_OR) &&
            (AndOr == NULL)) {

            AndOr = Pipeline;
            break;
        }

        //
        // There's more than one pipeline. If the and-or has yet to be
        // made, make it now.
        //

        if (AndOr == NULL) {
            AndOr = ShCreateNode(Shell, ShellNodeAndOr);
            if (AndOr == NULL) {
                goto ParseAndOrEnd;
            }
        }

        INSERT_BEFORE(&(Pipeline->SiblingListEntry), &(AndOr->Children));

        //
        // Regardless of where the pipeline was, if there's no valid AND or OR,
        // stop.
        //

        if ((SeparatorToken != TOKEN_DOUBLE_AND) &&
            (SeparatorToken != TOKEN_DOUBLE_OR)) {

            break;
        }

        Pipeline->AndOr = SeparatorToken;
        Pipeline = NULL;
        Result = ShGetToken(Shell, TRUE);
        if (Result == FALSE) {
            goto ParseAndOrEnd;
        }

        Result = ShParseLineBreak(Shell, FALSE, TRUE);
        if (Result == FALSE) {
            goto ParseAndOrEnd;
        }
    }

    Pipeline = NULL;
    Result = TRUE;

ParseAndOrEnd:
    if (Pipeline != NULL) {
        ShReleaseNode(Pipeline);
    }

    if (Result == FALSE) {
        if (AndOr != NULL) {
            ShReleaseNode(AndOr);
            AndOr = NULL;
        }
    }

    return AndOr;
}

PSHELL_NODE
ShParsePipeline (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse a pipeline out of the shell input.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE Command;
    PSHELL_NODE Pipeline;
    BOOL Result;

    Command = NULL;
    Pipeline = NULL;
    Result = FALSE;

    //
    // Parse an optional ! at the beginning.
    //

    if (Shell->Lexer.TokenType == '!') {
        Pipeline = ShCreateNode(Shell, ShellNodePipeline);
        if (Pipeline == NULL) {
            goto ParsePipelineEnd;
        }

        Pipeline->U.Pipeline.Bang = TRUE;
        Result = ShGetToken(Shell, TRUE);
        if (Result == FALSE) {
            goto ParsePipelineEnd;
        }
    }

    //
    // Parse pipeline statements separated by separator operators.
    //

    while (TRUE) {
        Command = ShParseCommand(Shell);
        if (Command == NULL) {
            Result = FALSE;
            goto ParsePipelineEnd;
        }

        //
        // If this is the first time around and there's only one item, then
        // just return that item.
        //

        if ((Shell->Lexer.TokenType != '|') && (Pipeline == NULL)) {
            Pipeline = Command;
            break;
        }

        //
        // There's more than one command. If the pipeline has yet to be made,
        // make it now.
        //

        if (Pipeline == NULL) {
            Pipeline = ShCreateNode(Shell, ShellNodePipeline);
            if (Pipeline == NULL) {
                goto ParsePipelineEnd;
            }
        }

        INSERT_BEFORE(&(Command->SiblingListEntry), &(Pipeline->Children));

        //
        // Regardless of where the command was, if there's no valid pipe
        // character then stop. Otherwise, move over the pipe character.
        //

        if (Shell->Lexer.TokenType != '|') {
            break;
        }

        Result = ShGetToken(Shell, TRUE);
        if (Result == FALSE) {
            Command = NULL;
            goto ParsePipelineEnd;
        }

        //
        // It's known now that most recent command wasn't the last, so set it
        // to run in the background so the shell doesn't wait on it.
        //

        Command->RunInBackground = TRUE;
        ShParseLineBreak(Shell, FALSE, TRUE);
    }

    Command = NULL;
    Result = TRUE;

ParsePipelineEnd:
    if (Command != NULL) {
        ShReleaseNode(Command);
    }

    if (Result == FALSE) {
        if (Pipeline != NULL) {
            ShReleaseNode(Pipeline);
            Pipeline = NULL;
        }
    }

    return Pipeline;
}

PSHELL_NODE
ShParseCommand (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse a command.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE Command;

    //
    // A command is either a simple command, compound command, or function
    // definition.
    //

    switch (Shell->Lexer.TokenType) {
    case '{':
    case '(':
    case TOKEN_FOR:
    case TOKEN_CASE:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_UNTIL:
        Command = ShParseCompoundCommand(Shell);
        break;

    case TOKEN_ASSIGNMENT_WORD:
    case TOKEN_IO_NUMBER:
    case TOKEN_LESS_THAN_AND:
    case TOKEN_GREATER_THAN_AND:
    case TOKEN_DOUBLE_GREATER_THAN:
    case TOKEN_DOUBLE_LESS_THAN:
    case TOKEN_DOUBLE_LESS_THAN_DASH:
    case '>':
    case '<':
        Command = ShParseSimpleCommand(Shell, NULL, 0);
        break;

    case TOKEN_WORD:
        Command = ShParseSimpleCommandOrFunction(Shell);
        break;

    default:
        Command = NULL;
        break;
    }

    return Command;
}

PSHELL_NODE
ShParseCompoundCommand (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse a command.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE Command;
    BOOL Result;

    Command = NULL;
    Result = TRUE;

    //
    // A command is either a simple command, compound command, or function
    // definition.
    //

    switch (Shell->Lexer.TokenType) {
    case '{':
        Command = ShParseBraceGroup(Shell);
        break;

    case '(':
        Command = ShParseSubshell(Shell);
        break;

    case TOKEN_FOR:
        Command = ShParseFor(Shell);
        break;

    case TOKEN_CASE:
        Command = ShParseCase(Shell);
        break;

    case TOKEN_IF:
        Command = ShParseIf(Shell);
        break;

    case TOKEN_WHILE:
    case TOKEN_UNTIL:
        Command = ShParseWhileOrUntil(Shell);
        break;

    default:
        ShParseError(Shell, NULL, "Unexpected token for compound command.");
        break;
    }

    if (Command == NULL) {
        return NULL;
    }

    //
    // Compound commands can optionally have a redirect list on them too.
    //

    Result = ShParseOptionalRedirectList(Shell, Command);
    if (Result == FALSE) {
        goto ParseCompoundCommandEnd;
    }

ParseCompoundCommandEnd:
    if (Result == FALSE) {
        if (Command != NULL) {
            ShReleaseNode(Command);
            Command = NULL;
        }
    }

    return Command;
}

PSHELL_NODE
ShParseBraceGroup (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse a set of statements surrounded by a brace
    group.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE BraceGroup;
    PSHELL_NODE CompoundList;
    BOOL Result;

    //
    // Scan over the open brace.
    //

    assert(Shell->Lexer.TokenType == '{');

    BraceGroup = ShCreateNode(Shell, ShellNodeBraceGroup);
    if (BraceGroup == NULL) {
        Result = FALSE;
        goto ParseBraceGroupEnd;
    }

    Result = ShGetToken(Shell, TRUE);
    if (Result == FALSE) {
        goto ParseBraceGroupEnd;
    }

    //
    // Get the tender compound list inside.
    //

    CompoundList = ShParseCompoundList(Shell);
    if (CompoundList == NULL) {
        Result = FALSE;
        goto ParseBraceGroupEnd;
    }

    INSERT_BEFORE(&(CompoundList->SiblingListEntry), &(BraceGroup->Children));

    //
    // Scan over the closing brace.
    //

    if (Shell->Lexer.TokenType != '}') {
        ShParseError(Shell,
                     NULL,
                     "Expected '}' for open brace at line %d.",
                     BraceGroup->LineNumber);

        Result = FALSE;
        goto ParseBraceGroupEnd;
    }

    Result = ShGetToken(Shell, TRUE);
    if (Result == FALSE) {
        goto ParseBraceGroupEnd;
    }

ParseBraceGroupEnd:
    if (Result == FALSE) {
        if (BraceGroup != NULL) {
            ShReleaseNode(BraceGroup);
            BraceGroup = NULL;
        }
    }

    return BraceGroup;
}

PSHELL_NODE
ShParseSubshell (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse a subshell sequence, which is basically of
    the form ( compound list ).

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE CompoundList;
    PSHELL_NODE Node;
    BOOL Result;

    Node = ShCreateNode(Shell, ShellNodeSubshell);
    if (Node == NULL) {
        Result = FALSE;
        goto ParseSubshellEnd;
    }

    assert(Shell->Lexer.TokenType == '(');

    Result = ShGetToken(Shell, TRUE);
    if (Result == FALSE) {
        goto ParseSubshellEnd;
    }

    CompoundList = ShParseCompoundList(Shell);
    if (CompoundList == NULL) {
        Result = FALSE;
        goto ParseSubshellEnd;
    }

    INSERT_BEFORE(&(CompoundList->SiblingListEntry), &(Node->Children));
    if (Shell->Lexer.TokenType != ')') {
        ShParseError(Shell,
                     NULL,
                     "Expected ')' for subshell at line %d.",
                     Node->LineNumber);

        Result = FALSE;
        goto ParseSubshellEnd;
    }

    Result = ShGetToken(Shell, TRUE);
    if (Result == FALSE) {
        goto ParseSubshellEnd;
    }

    Result = TRUE;

ParseSubshellEnd:
    if (Result == FALSE) {
        if (Node != NULL) {
            ShReleaseNode(Node);
            Node = NULL;
        }
    }

    return Node;
}

PSHELL_NODE
ShParseIf (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse an if statement.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE Condition;
    PSHELL_NODE ElseBody;
    PSHELL_NODE Node;
    BOOL Result;
    BOOL SwallowFi;
    PSHELL_NODE ThenCompoundList;

    assert((Shell->Lexer.TokenType == TOKEN_IF) ||
           (Shell->Lexer.TokenType == TOKEN_ELIF));

    //
    // Don't swallow the "fi" if this is an elif.
    //

    SwallowFi = TRUE;
    if (Shell->Lexer.TokenType == TOKEN_ELIF) {
        SwallowFi = FALSE;
    }

    Node = NULL;
    Result = ShGetToken(Shell, TRUE);
    if (Result == FALSE) {
        goto ParseIfEnd;
    }

    Node = ShCreateNode(Shell, ShellNodeIf);
    if (Node == NULL) {
        Result = FALSE;
        goto ParseIfEnd;
    }

    //
    // Parse out the condition part of the if statement.
    //

    Condition = ShParseCompoundList(Shell);
    if (Condition == NULL) {
        Result = FALSE;
        goto ParseIfEnd;
    }

    INSERT_BEFORE(&(Condition->SiblingListEntry), &(Node->Children));

    //
    // The next thing had better be a "then".
    //

    if (Shell->Lexer.TokenType != TOKEN_THEN) {
        ShParseError(Shell,
                     NULL,
                     "Expected 'then' for if at line %d.",
                     Node->LineNumber);

        Result = FALSE;
        goto ParseIfEnd;
    }

    Result = ShGetToken(Shell, TRUE);
    if (Result == FALSE) {
        goto ParseIfEnd;
    }

    //
    // Parse the compound list of stuff to do if the condition succeeds.
    //

    ThenCompoundList = ShParseCompoundList(Shell);
    if (ThenCompoundList == NULL) {
        Result = FALSE;
        goto ParseIfEnd;
    }

    INSERT_BEFORE(&(ThenCompoundList->SiblingListEntry), &(Node->Children));

    //
    // Ok, so there's an else or elif. If it's an else, parse out the
    // compound list that follows. For elifs, pretend it's a brand new if
    // statement inside the else.
    //

    if ((Shell->Lexer.TokenType == TOKEN_ELSE) ||
        (Shell->Lexer.TokenType == TOKEN_ELIF)) {

        if (Shell->Lexer.TokenType == TOKEN_ELSE) {
            Result = ShGetToken(Shell, TRUE);
            if (Result == FALSE) {
                goto ParseIfEnd;
            }

            ElseBody = ShParseCompoundList(Shell);

        } else {
            ElseBody = ShParseIf(Shell);
        }

        if (ElseBody == NULL) {
            Result = FALSE;
            goto ParseIfEnd;
        }

        INSERT_BEFORE(&(ElseBody->SiblingListEntry), &(Node->Children));
    }

    //
    // There had better be a fi at the end.
    //

    if (Shell->Lexer.TokenType != TOKEN_FI) {
        ShParseError(Shell,
                     NULL,
                     "Expected 'fi' for if at line %d.",
                     Node->LineNumber);

        Result = FALSE;
        goto ParseIfEnd;
    }

    if (SwallowFi != FALSE) {
        Result = ShGetToken(Shell, TRUE);
        if (Result == FALSE) {
            goto ParseIfEnd;
        }
    }

ParseIfEnd:
    if (Result == FALSE) {
        if (Node != NULL) {
            ShReleaseNode(Node);
        }

        Node = NULL;
    }

    return Node;
}

PSHELL_NODE
ShParseWhileOrUntil (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse either a while statement or an until
    statement.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE Condition;
    PSHELL_NODE DoGroup;
    PSHELL_NODE Node;
    SHELL_NODE_TYPE NodeType;
    BOOL Result;

    assert((Shell->Lexer.TokenType == TOKEN_WHILE) ||
           (Shell->Lexer.TokenType == TOKEN_UNTIL));

    if (Shell->Lexer.TokenType == TOKEN_WHILE) {
        NodeType = ShellNodeWhile;

    } else {
        NodeType = ShellNodeUntil;
    }

    Node = ShCreateNode(Shell, NodeType);
    if (Node == NULL) {
        Result = FALSE;
        goto ParseWhileOrUntilEnd;
    }

    //
    // Skip over the while or until.
    //

    Result = ShGetToken(Shell, TRUE);
    if (Result == FALSE) {
        goto ParseWhileOrUntilEnd;
    }

    //
    // Parse the compound list that is the condition.
    //

    Condition = ShParseCompoundList(Shell);
    if (Condition == NULL) {
        Result = FALSE;
        goto ParseWhileOrUntilEnd;
    }

    INSERT_BEFORE(&(Condition->SiblingListEntry), &(Node->Children));

    //
    // Parse the do group of stuff to do inside the loop.
    //

    DoGroup = ShParseDoGroup(Shell);
    if (DoGroup == NULL) {
        Result = FALSE;
        goto ParseWhileOrUntilEnd;
    }

    INSERT_BEFORE(&(DoGroup->SiblingListEntry), &(Node->Children));
    Result = TRUE;

ParseWhileOrUntilEnd:
    if (Result == FALSE) {
        if (Node != NULL) {
            ShReleaseNode(Node);
            Node = NULL;
        }
    }

    return Node;
}

PSHELL_NODE
ShParseFor (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse a for statement.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE DoGroup;
    PSHELL_NODE ForNode;
    BOOL LineBreakRequired;
    BOOL Result;

    assert(Shell->Lexer.TokenType == TOKEN_FOR);

    ForNode = NULL;
    Result = ShGetToken(Shell, FALSE);
    if (Result == FALSE) {
        goto ParseForEnd;
    }

    //
    // The next token is the name of the iterator variable.
    //

    if (Shell->Lexer.TokenType != TOKEN_WORD) {
        ShParseError(Shell, NULL, "Expected 'for' variable iterator name.");
        Result = FALSE;
        goto ParseForEnd;
    }

    Result = ShIsName(Shell->Lexer.TokenBuffer, Shell->Lexer.TokenBufferSize);
    if (Result == FALSE) {
        ShParseError(Shell, NULL, "Bad for loop variable name.");
        goto ParseForEnd;
    }

    ForNode = ShCreateNode(Shell, ShellNodeFor);
    if (ForNode == NULL) {
        Result = FALSE;
        goto ParseForEnd;
    }

    ForNode->U.For.Name = SwStringDuplicate(Shell->Lexer.TokenBuffer,
                                            Shell->Lexer.TokenBufferSize);

    if (ForNode->U.For.Name == NULL) {
        Result = FALSE;
        goto ParseForEnd;
    }

    ForNode->U.For.NameSize = Shell->Lexer.TokenBufferSize;
    Result = ShGetToken(Shell, FALSE);
    if (Result == FALSE) {
        goto ParseForEnd;
    }

    ShParseLineBreak(Shell, FALSE, FALSE);

    //
    // There can be an "in" next, but it's optional (the command line is used
    // if nothing's supplied).
    //

    LineBreakRequired = FALSE;
    if (Shell->Lexer.TokenType == TOKEN_IN) {
        LineBreakRequired = TRUE;
        Result = ShGetToken(Shell, FALSE);
        if (Result == FALSE) {
            goto ParseForEnd;
        }

        //
        // Now there's an optional wordlist, which means as long as words are
        // coming in add them to the wordlist.
        //

        while (SHELL_TOKEN_WORD_LIKE(Shell->Lexer.TokenType)) {
            Result = ShStringAppend(&(ForNode->U.For.WordListBuffer),
                                    &(ForNode->U.For.WordListBufferSize),
                                    &(ForNode->U.For.WordListBufferCapacity),
                                    Shell->Lexer.TokenBuffer,
                                    Shell->Lexer.TokenBufferSize);

            if (Result == FALSE) {
                goto ParseForEnd;
            }

            Result = ShGetToken(Shell, FALSE);
            if (Result == FALSE) {
                goto ParseForEnd;
            }
        }
    }

    //
    // Now parse a sequential separator, which is either a semicolon and
    // a linebreak or one or more newlines.
    //

    if (Shell->Lexer.TokenType == ';') {
        Result = ShGetToken(Shell, TRUE);
        if (Result == FALSE) {
            goto ParseForEnd;
        }

        Result = ShParseLineBreak(Shell, FALSE, FALSE);

    } else {
        Result = ShParseLineBreak(Shell, LineBreakRequired, FALSE);
    }

    if (Result == FALSE) {
        goto ParseForEnd;
    }

    //
    // Parse out a "do group" and call it a day.
    //

    DoGroup = ShParseDoGroup(Shell);
    if (DoGroup == NULL) {
        Result = FALSE;
        goto ParseForEnd;
    }

    INSERT_BEFORE(&(DoGroup->SiblingListEntry), &(ForNode->Children));
    Result = TRUE;

ParseForEnd:
    if (Result == FALSE) {
        if (ForNode != NULL) {
            ShReleaseNode(ForNode);
            ForNode = NULL;
        }
    }

    return ForNode;
}

PSHELL_NODE
ShParseCase (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse a case statement.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE CaseNode;
    PSHELL_CASE_PATTERN_SET PatternSet;
    BOOL Result;

    CaseNode = NULL;

    assert(Shell->Lexer.TokenType == TOKEN_CASE);

    Result = ShGetToken(Shell, FALSE);
    if (Result == FALSE) {
        goto ParseCaseEnd;
    }

    //
    // After the word case comes the name of the thing to match against.
    //

    if (!SHELL_TOKEN_WORD_LIKE(Shell->Lexer.TokenType)) {
        ShParseError(Shell, NULL, "Expected case input word.");
        Result = FALSE;
        goto ParseCaseEnd;
    }

    CaseNode = ShCreateNode(Shell, ShellNodeCase);
    if (CaseNode == NULL) {
        Result = FALSE;
        goto ParseCaseEnd;
    }

    CaseNode->U.Case.Name = SwStringDuplicate(Shell->Lexer.TokenBuffer,
                                              Shell->Lexer.TokenBufferSize);

    if (CaseNode->U.Case.Name == NULL) {
        Result = FALSE;
        goto ParseCaseEnd;
    }

    CaseNode->U.Case.NameSize = Shell->Lexer.TokenBufferSize;
    Result = ShGetToken(Shell, FALSE);
    if (Result == FALSE) {
        goto ParseCaseEnd;
    }

    //
    // Then parse a linebreak.
    //

    Result = ShParseLineBreak(Shell, FALSE, FALSE);
    if (Result == FALSE) {
        goto ParseCaseEnd;
    }

    //
    // Then parse an "in" and another linebreak.
    //

    if (Shell->Lexer.TokenType != TOKEN_IN) {
        ShParseError(Shell, CaseNode, "Expected 'in'.");
        Result = FALSE;
        goto ParseCaseEnd;
    }

    Result = ShGetToken(Shell, FALSE);
    if (Result == FALSE) {
        goto ParseCaseEnd;
    }

    Result = ShParseLineBreak(Shell, FALSE, FALSE);
    if (Result == FALSE) {
        goto ParseCaseEnd;
    }

    //
    // Here comes the case list, which is actually optional altogether.
    //

    while (TRUE) {

        //
        // Parse past an optional opening parentheses.
        //

        if (Shell->Lexer.TokenType == '(') {
            Result = ShGetToken(Shell, FALSE);
            if (Result == FALSE) {
                goto ParseCaseEnd;
            }
        }

        //
        // Scan the pattern(s).
        //

        Result = ShParsePattern(Shell, CaseNode, &PatternSet);
        if (Result == FALSE) {
            goto ParseCaseEnd;
        }

        //
        // If the pattern set is null, this is an esac, stop parsing patterns.
        //

        if (PatternSet == NULL) {

            assert(Shell->Lexer.TokenType == TOKEN_ESAC);

            break;
        }

        //
        // Scan the closing parentheses.
        //

        if (Shell->Lexer.TokenType != ')') {
            ShParseError(Shell, NULL, "Expected ')' to close case pattern.");
            Result = FALSE;
            goto ParseCaseEnd;
        }

        Result = ShGetToken(Shell, TRUE);
        if (Result == FALSE) {
            goto ParseCaseEnd;
        }

        //
        // Scan an optional linebreak.
        //

        Result = ShParseLineBreak(Shell, FALSE, TRUE);
        if (Result == FALSE) {
            goto ParseCaseEnd;
        }

        //
        // Now there's either a compound list there, a double semicolon, or an
        // esac.
        //

        if (Shell->Lexer.TokenType == TOKEN_DOUBLE_SEMICOLON) {
            Result = ShGetToken(Shell, FALSE);
            if (Result == FALSE) {
                goto ParseCaseEnd;
            }

            Result = ShParseLineBreak(Shell, FALSE, TRUE);
            if (Result == FALSE) {
                goto ParseCaseEnd;
            }

        } else if (Shell->Lexer.TokenType == TOKEN_ESAC) {
            break;

        //
        // It must be a compound list.
        //

        } else {
            PatternSet->Action = ShParseCompoundList(Shell);
            if (PatternSet->Action == NULL) {
                goto ParseCaseEnd;
            }

            //
            // Parse an optional linebreak.
            //

            Result = ShParseLineBreak(Shell, FALSE, FALSE);
            if (Result == FALSE) {
                goto ParseCaseEnd;
            }

            //
            // Now there had better be an esac or a double semicolon there.
            //

            if (Shell->Lexer.TokenType == TOKEN_ESAC) {
                break;

            } else if (Shell->Lexer.TokenType != TOKEN_DOUBLE_SEMICOLON) {
                ShParseError(Shell,
                             NULL,
                             "Expected ';;' for case at line %d.",
                             CaseNode->LineNumber);

                Result = FALSE;
                goto ParseCaseEnd;
            }

            //
            // Scan over the double semicolon and an optional linebreak.
            //

            Result = ShGetToken(Shell, FALSE);
            if (Result == FALSE) {
                goto ParseCaseEnd;
            }

            Result = ShParseLineBreak(Shell, FALSE, FALSE);
            if (Result == FALSE) {
                goto ParseCaseEnd;
            }
        }
    }

    //
    // Scan over the esac.
    //

    if (Shell->Lexer.TokenType != TOKEN_ESAC) {
        ShParseError(Shell,
                     NULL,
                     "Expected 'esac' for case at line %d.",
                     CaseNode->LineNumber);

        Result = FALSE;
        goto ParseCaseEnd;
    }

    Result = ShGetToken(Shell, TRUE);
    if (Result == FALSE) {
        goto ParseCaseEnd;
    }

ParseCaseEnd:
    if (Result == FALSE) {
        if (CaseNode != NULL) {
            ShReleaseNode(CaseNode);
            CaseNode = NULL;
        }
    }

    return CaseNode;
}

BOOL
ShParsePattern (
    PSHELL Shell,
    PSHELL_NODE Case,
    PSHELL_CASE_PATTERN_SET *NewPatternSet
    )

/*++

Routine Description:

    This routine attempts to scan a pattern, which is a sequence of 1 or more
    words separated by bars (|).

Arguments:

    Shell - Supplies a pointer to the shell object.

    Case - Supplies a pointer to the case node where the pattern will be added.

    NewPatternSet - Supplies a pointer where the new pattern set will be
        returned on success.

Return Value:

    Returns a pointer to the created case pattern on success.

    NULL on failure.

--*/

{

    BOOL GotSomething;
    BOOL Result;
    PSHELL_CASE_PATTERN_SET Set;

    assert(Case->Type == ShellNodeCase);

    //
    // Create a pattern set and optimistically add it to the case statement.
    //

    Set = malloc(sizeof(SHELL_CASE_PATTERN_SET));
    if (Set == NULL) {
        return FALSE;
    }

    memset(Set, 0, sizeof(SHELL_CASE_PATTERN_SET));
    INITIALIZE_LIST_HEAD(&(Set->PatternEntryList));
    INSERT_BEFORE(&(Set->ListEntry), &(Case->U.Case.PatternList));

    //
    // Loop through and try to get at least one pattern word out.
    //

    GotSomething = FALSE;
    while (SHELL_TOKEN_WORD_LIKE(Shell->Lexer.TokenType)) {

        //
        // If it's an esac on the first pattern, then treat it with respect.
        //

        if ((Shell->Lexer.TokenType == TOKEN_ESAC) && (GotSomething == FALSE)) {
            break;
        }

        //
        // Add the new pattern word to this set.
        //

        Result = ShAddPatternToSet(Set,
                                   Shell->Lexer.TokenBuffer,
                                   Shell->Lexer.TokenBufferSize);

        if (Result == FALSE) {
            goto ParsePatternEnd;
        }

        GotSomething = TRUE;
        Result = ShGetToken(Shell, FALSE);
        if (Result == FALSE) {
            goto ParsePatternEnd;
        }

        //
        // If the next thing isn't a pipe, then this pattern list is over.
        //

        if (Shell->Lexer.TokenType != '|') {
            break;
        }

        Result = ShGetToken(Shell, FALSE);
        if (Result == FALSE) {
            goto ParsePatternEnd;
        }
    }

    //
    // If it was an esac and nothing else, take the set back off the list, this
    // case statement is over.
    //

    if ((GotSomething == FALSE) && (Shell->Lexer.TokenType == TOKEN_ESAC)) {

        assert((Set->Action == NULL) && (LIST_EMPTY(&(Set->PatternEntryList))));

        LIST_REMOVE(&(Set->ListEntry));
        free(Set);
        Set = NULL;
        Result = TRUE;

    } else {
        Result = GotSomething;
        if (Result == FALSE) {
            ShParseError(Shell, Case, "Expected pattern word.");
        }
    }

ParsePatternEnd:
    *NewPatternSet = Set;
    return Result;
}

PSHELL_NODE
ShParseDoGroup (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse a "do group", which consists of "do", a
    compound list, and "done". This is used in most loop constructs.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE CompoundList;
    ULONG DoLineNumber;
    BOOL Result;

    CompoundList = NULL;
    ShParseLineBreak(Shell, FALSE, FALSE);

    //
    // The first thing in a do group should really be a do.
    //

    if (Shell->Lexer.TokenType != TOKEN_DO) {
        ShParseError(Shell, NULL, "Expected 'do'.");
        Result = FALSE;
        goto ParseDoGroupEnd;
    }

    DoLineNumber = Shell->Lexer.LineNumber;
    Result = ShGetToken(Shell, TRUE);
    if (Result == FALSE) {
        goto ParseDoGroupEnd;
    }

    //
    // Now get the compound list.
    //

    CompoundList = ShParseCompoundList(Shell);
    if (CompoundList == NULL) {
        goto ParseDoGroupEnd;
    }

    //
    // Now there should be a done.
    //

    if (Shell->Lexer.TokenType != TOKEN_DONE) {
        ShParseError(Shell,
                     NULL,
                     "Expected 'done' for 'do' at line %d.",
                     DoLineNumber);

        Result = FALSE;
        goto ParseDoGroupEnd;
    }

    Result = ShGetToken(Shell, TRUE);
    if (Result == FALSE) {
        goto ParseDoGroupEnd;
    }

ParseDoGroupEnd:
    if (Result == FALSE) {
        if (CompoundList != NULL) {
            ShReleaseNode(CompoundList);
            CompoundList = NULL;
        }
    }

    return CompoundList;
}

PSHELL_NODE
ShParseCompoundList (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse a compound list.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE CompoundList;
    BOOL Result;
    CHAR Separator;

    CompoundList = NULL;

    //
    // Parse a line break (really just an optional newline list).
    //

    Result = ShParseLineBreak(Shell, FALSE, TRUE);
    if (Result == FALSE) {
        goto ParseCompoundListEnd;
    }

    //
    // Parse a term, call it a compound list.
    //

    CompoundList = ShParseTerm(Shell);
    if (CompoundList == NULL) {
        Result = FALSE;
        goto ParseCompoundListEnd;
    }

    //
    // Parse an optional separator.
    //

    Result = ShParseSeparator(Shell, &Separator);
    if (Result != FALSE) {
        if (Separator == '&') {
            CompoundList->RunInBackground = TRUE;
        }
    }

    Result = TRUE;

ParseCompoundListEnd:
    if (Result == FALSE) {
        if (CompoundList != NULL) {
            ShReleaseNode(CompoundList);
            CompoundList = NULL;
        }
    }

    return CompoundList;
}

PSHELL_NODE
ShParseTerm (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse a term out of the shell input.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE AndOr;
    BOOL Result;
    CHAR SeparatorOp;
    PSHELL_NODE Term;

    Result = FALSE;
    Term = NULL;

    //
    // Parse and-or statements separated by separators (this is different than
    // a list where the separators were separator ops only).
    //

    while (TRUE) {
        AndOr = ShParseAndOr(Shell);
        if (AndOr == NULL) {
            if (Term == NULL) {
                Result = FALSE;
                ShParseError(Shell, NULL, "Unexpected token.");
                goto ParseTermEnd;
            }

            break;
        }

        Result = ShParseSeparator(Shell, &SeparatorOp);

        //
        // If this is the first time around and there's only one item, then
        // just return that item.
        //

        if ((Result == FALSE) && (Term == NULL)) {
            Term = AndOr;
            break;
        }

        //
        // There's more than one and-or. If the list has yet to be made, make it
        // now.
        //

        if (Term == NULL) {
            Term = ShCreateNode(Shell, ShellNodeTerm);
            if (Term == NULL) {
                goto ParseTermEnd;
            }
        }

        INSERT_BEFORE(&(AndOr->SiblingListEntry), &(Term->Children));

        //
        // Regardless of where the command was, if there's no separator, this
        // loop is done.
        //

        if (Result == FALSE) {
            break;
        }

        if (SeparatorOp == '&') {
            AndOr->RunInBackground = TRUE;
        }
    }

    AndOr = NULL;
    Result = TRUE;

ParseTermEnd:
    if (AndOr != NULL) {
        ShReleaseNode(AndOr);
    }

    if (Result == FALSE) {
        if (Term != NULL) {
            ShReleaseNode(Term);
            Term = NULL;
        }
    }

    return Term;
}

PSHELL_NODE
ShParseSimpleCommandOrFunction (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine attempts to parse a simple command or function definition.

Arguments:

    Shell - Supplies a pointer to the shell object.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE Command;
    PSTR FirstWord;
    UINTN FirstWordSize;
    BOOL Result;

    Command = NULL;

    //
    // Get the first word and look beyond it.
    //

    FirstWord = NULL;
    FirstWordSize = 0;
    if (Shell->Lexer.TokenType == TOKEN_WORD) {
        FirstWordSize = Shell->Lexer.TokenBufferSize;
        FirstWord = SwStringDuplicate(Shell->Lexer.TokenBuffer, FirstWordSize);
        if (FirstWord == NULL) {
            Result = FALSE;
            goto ParseSimpleCommandOrFunctionEnd;
        }

        Result = ShGetToken(Shell, FALSE);
        if (Result == FALSE) {
            goto ParseSimpleCommandOrFunctionEnd;
        }
    }

    //
    // If the next thing is an open parentheses, then it's a function
    // definition. Otherwise it's a simple command.
    //

    if ((ShIsName(FirstWord, FirstWordSize) != FALSE) &&
        (Shell->Lexer.TokenType == '(')) {

        Command = ShParseFunctionDefinition(Shell, FirstWord, FirstWordSize);

    } else {
        Command = ShParseSimpleCommand(Shell, FirstWord, FirstWordSize);
    }

ParseSimpleCommandOrFunctionEnd:
    if (FirstWord != NULL) {
        free(FirstWord);
    }

    return Command;
}

PSHELL_NODE
ShParseSimpleCommand (
    PSHELL Shell,
    PSTR FirstWord,
    UINTN FirstWordSize
    )

/*++

Routine Description:

    This routine attempts to parse a simple command.

Arguments:

    Shell - Supplies a pointer to the shell object.

    FirstWord - Supplies an optional pointer to the command name that was
        parsed out earlier.

    FirstWordSize - Supplies the size of the first word buffer in bytes
        including the null terminator.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    BOOL AllowAssignmentWords;
    PSHELL_NODE Command;
    BOOL NonEmpty;
    BOOL Result;
    BOOL SwallowToken;

    NonEmpty = FALSE;
    Result = TRUE;
    Command = ShCreateNode(Shell, ShellNodeSimpleCommand);
    if (Command == NULL) {
        return NULL;
    }

    AllowAssignmentWords = TRUE;
    if (FirstWord != NULL) {
        AllowAssignmentWords = FALSE;
        Result = ShAddComponentToCommand(Command, FirstWord, FirstWordSize);
        if (Result == FALSE) {
            goto ParseSimpleCommandEnd;
        }

        NonEmpty = TRUE;
    }

    while (TRUE) {
        SwallowToken = TRUE;
        switch (Shell->Lexer.TokenType) {
        case TOKEN_IO_NUMBER:
        case TOKEN_LESS_THAN_AND:
        case TOKEN_GREATER_THAN_AND:
        case TOKEN_DOUBLE_GREATER_THAN:
        case TOKEN_DOUBLE_LESS_THAN:
        case TOKEN_DOUBLE_LESS_THAN_DASH:
        case TOKEN_LESS_THAN_GREATER_THAN:
        case TOKEN_CLOBBER:
        case '>':
        case '<':
            Result = ShParseRedirection(Shell, Command);
            if (Result == FALSE) {
                goto ParseSimpleCommandEnd;
            }

            SwallowToken = FALSE;
            break;

        case TOKEN_ASSIGNMENT_WORD:

            //
            // If still at that phase (before the initial command word) parse
            // any assignment words that come out. If the assignment word was
            // miscategorized, fall through to the regular word processing.
            //

            if (AllowAssignmentWords != FALSE) {
                Result = ShParseAssignment(Shell, Command);
                if (Result != FALSE) {
                    break;
                }

                Result = TRUE;
            }

            //
            // If assignment words are not allowed or the assignment word
            // failed, fall through.
            //

        //
        // Inside of a simple command, they keywords are just regular arguments.
        //

        case TOKEN_IF:
        case TOKEN_THEN:
        case TOKEN_ELSE:
        case TOKEN_ELIF:
        case TOKEN_FI:
        case TOKEN_DO:
        case TOKEN_DONE:
        case TOKEN_CASE:
        case TOKEN_ESAC:
        case TOKEN_WHILE:
        case TOKEN_UNTIL:
        case TOKEN_FOR:
        case TOKEN_IN:
        case TOKEN_WORD:
        case '!':
        case '{':
        case '}':
            Result = ShAddComponentToCommand(Command,
                                             Shell->Lexer.TokenBuffer,
                                             Shell->Lexer.TokenBufferSize);

            if (Result == FALSE) {
                goto ParseSimpleCommandEnd;
            }

            AllowAssignmentWords = FALSE;
            break;

        default:
            Result = NonEmpty;
            if (Result == FALSE) {
                ShParseError(Shell, Command, "Expected simple command word.");
            }

            goto ParseSimpleCommandEnd;
        }

        NonEmpty = TRUE;
        if (SwallowToken != FALSE) {
            Result = ShGetToken(Shell, FALSE);
            if (Result == FALSE) {
                goto ParseSimpleCommandEnd;
            }
        }
    }

ParseSimpleCommandEnd:
    if (Result == FALSE) {
        if (Command != NULL) {
            ShReleaseNode(Command);
            Command = NULL;
        }
    }

    return Command;
}

PSHELL_NODE
ShParseFunctionDefinition (
    PSHELL Shell,
    PSTR FunctionName,
    UINTN FunctionNameSize
    )

/*++

Routine Description:

    This routine attempts to parse a function definition.

Arguments:

    Shell - Supplies a pointer to the shell object.

    FunctionName - Supplies a pointer to the function name.

    FunctionNameSize - Supplies the size of the function name buffer in bytes
        including the null terminator.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE Body;
    PSHELL_NODE Function;
    BOOL Result;

    Result = TRUE;
    Function = ShCreateNode(Shell, ShellNodeFunction);
    if (Function == NULL) {
        return NULL;
    }

    Function->U.Function.Name = SwStringDuplicate(FunctionName,
                                                  FunctionNameSize);

    if (Function->U.Function.Name == NULL) {
        Result = FALSE;
        goto ParseFunctionDefinitionEnd;
    }

    Function->U.Function.NameSize = FunctionNameSize;

    //
    // The current token should be an open parentheses, and then there should
    // be a close parenthese and a newline.
    //

    assert(Shell->Lexer.TokenType == '(');

    Result = ShGetToken(Shell, FALSE);
    if (Result == FALSE) {
        goto ParseFunctionDefinitionEnd;
    }

    if (Shell->Lexer.TokenType != ')') {
        ShParseError(Shell,
                     Function,
                     "Expected ')' for function definition.");

        Result = FALSE;
        goto ParseFunctionDefinitionEnd;
    }

    Result = ShGetToken(Shell, TRUE);
    if (Result == FALSE) {
        goto ParseFunctionDefinitionEnd;
    }

    Result = ShParseLineBreak(Shell, FALSE, TRUE);
    if (Result == FALSE) {
        goto ParseFunctionDefinitionEnd;
    }

    Body = ShParseCompoundCommand(Shell);
    if (Body == NULL) {
        goto ParseFunctionDefinitionEnd;
    }

    INSERT_BEFORE(&(Body->SiblingListEntry), &(Function->Children));

    //
    // Parse an optional redirect list on the function body.
    //

    Result = ShParseOptionalRedirectList(Shell, Function);
    if (Result == FALSE) {
        goto ParseFunctionDefinitionEnd;
    }

    Result = TRUE;

ParseFunctionDefinitionEnd:
    if (Result == FALSE) {
        if (Function != NULL) {
            ShReleaseNode(Function);
            Function = NULL;
        }
    }

    return Function;
}

BOOL
ShParseOptionalRedirectList (
    PSHELL Shell,
    PSHELL_NODE Node
    )

/*++

Routine Description:

    This routine attempts to parse a redirect list that may or may not be there.

Arguments:

    Shell - Supplies a pointer to the shell object.

    Node - Supplies a pointer to the node the I/O redirection belongs to.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL BreakOut;
    BOOL Result;

    Result = TRUE;
    BreakOut = FALSE;
    while (BreakOut == FALSE) {
        switch (Shell->Lexer.TokenType) {
        case TOKEN_IO_NUMBER:
        case TOKEN_LESS_THAN_AND:
        case TOKEN_GREATER_THAN_AND:
        case TOKEN_DOUBLE_GREATER_THAN:
        case TOKEN_DOUBLE_LESS_THAN:
        case TOKEN_DOUBLE_LESS_THAN_DASH:
        case TOKEN_LESS_THAN_GREATER_THAN:
        case TOKEN_CLOBBER:
        case '>':
        case '<':
            Result = ShParseRedirection(Shell, Node);
            if (Result == FALSE) {
                goto ParseOptionalRedirectListEnd;
            }

            break;

        default:
            BreakOut = TRUE;
            break;
        }
    }

ParseOptionalRedirectListEnd:
    return Result;
}

BOOL
ShParseRedirection (
    PSHELL Shell,
    PSHELL_NODE Node
    )

/*++

Routine Description:

    This routine attempts to parse an I/O redirection.

Arguments:

    Shell - Supplies a pointer to the shell object.

    Node - Supplies a pointer to the node the I/O redirection belongs to.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG DefaultFileNumber;
    PSTR FileName;
    UINTN FileNameSize;
    LONG FileNumber;
    BOOL Result;
    SHELL_IO_REDIRECTION_TYPE Type;

    DefaultFileNumber = -1;
    FileNumber = -1;
    Type = ShellRedirectInvalid;

    //
    // Start by attempting to get an I/O number.
    //

    if (Shell->Lexer.TokenType == TOKEN_IO_NUMBER) {
        FileNumber = strtol(Shell->Lexer.TokenBuffer, NULL, 10);
        if (FileNumber < 0) {

            //
            // The lexer should have stripped off any piece that couldn't be
            // interpreted as a positive number, so it's weird that this
            // number wouldn't convert.
            //

            assert(FALSE);

            Result = FALSE;
            goto ParseRedirectionEnd;
        }

        Result = ShGetToken(Shell, FALSE);
        if (Result == FALSE) {
            goto ParseRedirectionEnd;
        }
    }

    //
    // Now get the operator.
    //

    switch (Shell->Lexer.TokenType) {
    case TOKEN_LESS_THAN_AND:
        Type = ShellRedirectReadFromDescriptor;
        DefaultFileNumber = STDIN_FILENO;
        break;

    case TOKEN_GREATER_THAN_AND:
        Type = ShellRedirectWriteToDescriptor;
        DefaultFileNumber = STDOUT_FILENO;
        break;

    case TOKEN_DOUBLE_GREATER_THAN:
        Type = ShellRedirectAppend;
        DefaultFileNumber = STDOUT_FILENO;
        break;

    case TOKEN_DOUBLE_LESS_THAN:
        Type = ShellRedirectHereDocument;
        DefaultFileNumber = STDIN_FILENO;
        break;

    case TOKEN_DOUBLE_LESS_THAN_DASH:
        Type = ShellRedirectStrippedHereDocument;
        DefaultFileNumber = STDIN_FILENO;
        break;

    case TOKEN_LESS_THAN_GREATER_THAN:
        Type = ShellRedirectReadWrite;
        DefaultFileNumber = STDIN_FILENO;
        break;

    case TOKEN_CLOBBER:
        Type = ShellRedirectClobber;
        DefaultFileNumber = STDOUT_FILENO;
        break;

    case '>':
        Type = ShellRedirectWrite;
        DefaultFileNumber = STDOUT_FILENO;
        break;

    case '<':
        Type = ShellRedirectRead;
        DefaultFileNumber = STDIN_FILENO;
        break;

    default:
        Result = FALSE;
        goto ParseRedirectionEnd;
    }

    Result = ShGetToken(Shell, FALSE);
    if (Result == FALSE) {
        goto ParseRedirectionEnd;
    }

    //
    // Now get the word of where to redirect to. Convert to a file descriptor
    // number if needed.
    //

    if (!SHELL_TOKEN_WORD_LIKE(Shell->Lexer.TokenType)) {
        ShParseError(Shell, Node, "Expected redirection file name.");
        Result = FALSE;
        goto ParseRedirectionEnd;
    }

    FileName = Shell->Lexer.TokenBuffer;
    FileNameSize = Shell->Lexer.TokenBufferSize;
    if (FileNumber == -1) {
        FileNumber = DefaultFileNumber;
    }

    Result = ShCreateRedirection(Shell,
                                 Node,
                                 Type,
                                 FileNumber,
                                 FileName,
                                 FileNameSize);

    if (Result == FALSE) {
        goto ParseRedirectionEnd;
    }

    Result = ShGetToken(Shell, TRUE);
    if (Result == FALSE) {
        goto ParseRedirectionEnd;
    }

ParseRedirectionEnd:
    return Result;
}

BOOL
ShParseAssignment (
    PSHELL Shell,
    PSHELL_NODE Node
    )

/*++

Routine Description:

    This routine attempts to parse an assignment word.

Arguments:

    Shell - Supplies a pointer to the shell object.

    Node - Supplies a pointer to the node the assignment belongs to.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR Name;
    UINTN NameSize;
    BOOL Result;
    PSTR Token;
    UINTN TokenSize;
    PSTR Value;
    UINTN ValueSize;

    Token = Shell->Lexer.TokenBuffer;
    TokenSize = Shell->Lexer.TokenBufferSize;
    Name = Token;
    Value = strchr(Token, '=');
    if (Value == NULL) {

        //
        // How would something get here unless this was already categorized as
        // an assignment word?
        //

        assert(FALSE);

        return FALSE;
    }

    //
    // There can't be a zero length name.
    //

    if (Value == Token) {
        return FALSE;
    }

    NameSize = ((UINTN)Value - (UINTN)Token);
    if (ShIsName(Name, NameSize) == FALSE) {
        return FALSE;
    }

    NameSize += 1;
    Value += 1;
    ValueSize = TokenSize - ((UINTN)Value - (UINTN)Token);
    Result = ShCreateAssignment(Node, Name, NameSize, Value, ValueSize);
    return Result;
}

BOOL
ShParseLineBreak (
    PSHELL Shell,
    BOOL Required,
    BOOL FirstCommandWord
    )

/*++

Routine Description:

    This routine parses a line break term, which is just zero or more newline.

Arguments:

    Shell - Supplies a pointer to the shell whose input is being parsed.

    Required - Supplies a boolean indicating if at least one line break is
        required.

    FirstCommandWord - Supplies the boolean that will be passed to the get
        token routine indicating whether or not this next token could be the
        command word of a simple command.

Return Value:

    TRUE on success.

    FALSE if the lexer failed to get a token.

--*/

{

    BOOL Result;

    if ((Required != FALSE) && (Shell->Lexer.TokenType != '\n')) {
        return FALSE;
    }

    while (Shell->Lexer.TokenType == '\n') {
        ShPrintPrompt(Shell, 2);
        Result = ShGetToken(Shell, FirstCommandWord);
        if (Result == FALSE) {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL
ShParseSeparator (
    PSHELL Shell,
    PCHAR Separator
    )

/*++

Routine Description:

    This routine attempts to parse a separator, which is either a separator op
    followed by a linebreak, or 1+ newlines.

Arguments:

    Shell - Supplies a pointer to the shell whose input is being parsed.

    Separator - Supplies a pointer where the separator character will be
        returned on success, or 0 if there was no separator.

Return Value:

    TRUE if a separator operator was parsed.

    FALSE if there was no separator or the lexer failed to move on.

--*/

{

    BOOL Result;

    //
    // First try to get a separator op and linebreak. A linebreak swallows a
    // newline list.
    //

    Result = ShParseSeparatorOp(Shell, Separator);
    if (Result != FALSE) {
        Result = ShParseLineBreak(Shell, FALSE, TRUE);

    //
    // There's no separator op, try to get at least one newline.
    //

    } else {
        Result = ShParseLineBreak(Shell, TRUE, TRUE);
    }

    return Result;
}

BOOL
ShParseSeparatorOp (
    PSHELL Shell,
    PCHAR Separator
    )

/*++

Routine Description:

    This routine attempts to parse a separator operator, which is either a
    semicolon or an ampersand.

Arguments:

    Shell - Supplies a pointer to the shell whose input is being parsed.

    Separator - Supplies a pointer where the separator character will be
        returned on success, or 0 if there was no separator.

Return Value:

    TRUE if a separator operator was parsed.

    FALSE if there was no separator or the lexer failed to move on.

--*/

{

    BOOL Result;

    if ((Shell->Lexer.TokenType == ';') || (Shell->Lexer.TokenType == '&')) {
        *Separator = Shell->Lexer.TokenType;
        Result = ShGetToken(Shell, TRUE);
        return Result;
    }

    *Separator = 0;
    return FALSE;
}

PSHELL_NODE
ShCreateNode (
    PSHELL Shell,
    SHELL_NODE_TYPE Type
    )

/*++

Routine Description:

    This routine allocates and initializes a shell node.

Arguments:

    Shell - Supplies a pointer to the shell.

    Type - Supplies the type of node to create.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PSHELL_NODE NewNode;

    NewNode = malloc(sizeof(SHELL_NODE));
    if (NewNode == NULL) {
        return NULL;
    }

    NewNode->Type = Type;
    NewNode->ReferenceCount = 1;
    NewNode->LineNumber = Shell->Lexer.LineNumber;
    if (Shell->Lexer.TokenType == '\n') {
        NewNode->LineNumber -= 1;
    }

    INITIALIZE_LIST_HEAD(&(NewNode->Children));
    INITIALIZE_LIST_HEAD(&(NewNode->RedirectList));
    NewNode->RunInBackground = FALSE;
    NewNode->AndOr = 0;
    switch (Type) {
    case ShellNodePipeline:
        NewNode->U.Pipeline.Bang = FALSE;
        break;

    case ShellNodeSimpleCommand:
        memset(&(NewNode->U.SimpleCommand),
               0,
               sizeof(SHELL_NODE_SIMPLE_COMMAND));

        INITIALIZE_LIST_HEAD(&(NewNode->U.SimpleCommand.AssignmentList));
        break;

    case ShellNodeFunction:
        memset(&(NewNode->U.Function), 0, sizeof(SHELL_NODE_FUNCTION));
        break;

    case ShellNodeFor:
        memset(&(NewNode->U.For), 0, sizeof(SHELL_NODE_FOR));
        break;

    case ShellNodeCase:
        memset(&(NewNode->U.Case), 0, sizeof(SHELL_NODE_CASE));
        INITIALIZE_LIST_HEAD(&(NewNode->U.Case.PatternList));
        break;

    default:
        break;
    }

    return NewNode;
}

VOID
ShDeleteNode (
    PSHELL_NODE Node
    )

/*++

Routine Description:

    This routine destroys a shell node.

Arguments:

    Node - Supplies a pointer to the node to destroy.

Return Value:

    None.

--*/

{

    PSHELL_ASSIGNMENT Assignment;
    PSHELL_NODE Child;
    PSHELL_IO_REDIRECT Redirect;

    assert(Node->ReferenceCount == 0);

    switch (Node->Type) {
    case ShellNodeSimpleCommand:
        while (LIST_EMPTY(&(Node->U.SimpleCommand.AssignmentList)) == FALSE) {
            Assignment = LIST_VALUE(Node->U.SimpleCommand.AssignmentList.Next,
                                    SHELL_ASSIGNMENT,
                                    ListEntry);

            ShDestroyAssignment(Assignment);
        }

        if (Node->U.SimpleCommand.Arguments != NULL) {
            free(Node->U.SimpleCommand.Arguments);
        }

        break;

    case ShellNodeFunction:
        if (Node->U.Function.Name != NULL) {
            free(Node->U.Function.Name);
        }

        break;

    case ShellNodeFor:
        if (Node->U.For.Name != NULL) {
            free(Node->U.For.Name);
        }

        if (Node->U.For.WordListBuffer != NULL) {
            free(Node->U.For.WordListBuffer);
        }

        break;

    case ShellNodeCase:
        ShDestroyCasePatternList(Node);
        if (Node->U.Case.Name != NULL) {
            free(Node->U.Case.Name);
        }

        break;

    default:
        break;
    }

    while (LIST_EMPTY(&(Node->RedirectList)) == FALSE) {
        Redirect = LIST_VALUE(Node->RedirectList.Next,
                              SHELL_IO_REDIRECT,
                              ListEntry);

        ShDestroyRedirection(Redirect);
    }

    while (LIST_EMPTY(&(Node->Children)) == FALSE) {
        Child = LIST_VALUE(Node->Children.Next, SHELL_NODE, SiblingListEntry);
        LIST_REMOVE(&(Child->SiblingListEntry));
        ShReleaseNode(Child);
    }

    free(Node);
    return;
}

VOID
ShPrintNode (
    PSHELL Shell,
    PSHELL_NODE Node,
    ULONG Depth
    )

/*++

Routine Description:

    This routine prints out a parsed shell node.

Arguments:

    Shell - Supplies a pointer to the shell.

    Node - Supplies a pointer to the node to print.

    Depth - Supplies the indentation depth to print it at.

Return Value:

    None.

--*/

{

    PSHELL_ASSIGNMENT Assignment;
    PSHELL_NODE Child;
    PLIST_ENTRY CurrentEntry;
    PLIST_ENTRY CurrentEntryEntry;
    ULONG DepthIndex;
    PSHELL_CASE_PATTERN_ENTRY Entry;
    PSHELL_IO_REDIRECT Redirect;
    PSHELL_CASE_PATTERN_SET Set;

    for (DepthIndex = 0; DepthIndex < Depth; DepthIndex += 1) {
        ShPrintTrace(Shell, " ");
    }

    ShPrintTrace(Shell, "Line %d ", Node->LineNumber);
    switch (Node->Type) {
    case ShellNodeInvalid:
        ShPrintTrace(Shell, "Invalid Node");
        break;

    case ShellNodeList:
        ShPrintTrace(Shell, "List");
        break;

    case ShellNodeAndOr:
        ShPrintTrace(Shell, "AndOr");
        break;

    case ShellNodePipeline:
        if (Node->U.Pipeline.Bang != FALSE) {
            ShPrintTrace(Shell, "! ");
        }

        ShPrintTrace(Shell, "Pipeline");
        break;

    case ShellNodeSimpleCommand:
        ShPrintTrace(Shell, "SimpleCommand:");
        CurrentEntry = Node->U.SimpleCommand.AssignmentList.Next;
        while (CurrentEntry != &(Node->U.SimpleCommand.AssignmentList)) {
            Assignment = LIST_VALUE(CurrentEntry, SHELL_ASSIGNMENT, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            ShPrintTrace(Shell,
                         " [%s]=[%s]",
                         Assignment->Name,
                         Assignment->Value);
        }

        ShPrintTrace(Shell, " [%s] ", Node->U.SimpleCommand.Arguments);
        break;

    case ShellNodeFunction:
        ShPrintTrace(Shell, "Function %s", Node->U.Function.Name);
        break;

    case ShellNodeIf:
        ShPrintTrace(Shell, "If");
        break;

    case ShellNodeTerm:
        ShPrintTrace(Shell, "Term");
        break;

    case ShellNodeFor:
        ShPrintTrace(Shell,
                     "For [%s] in [%s]",
                     Node->U.For.Name,
                     Node->U.For.WordListBuffer);

        ShPrintTrace(Shell, " do");
        break;

    case ShellNodeBraceGroup:
        ShPrintTrace(Shell, "BraceGroup");
        break;

    case ShellNodeCase:
        ShPrintTrace(Shell, "Case [%s]", Node->U.Case.Name);
        break;

    case ShellNodeWhile:
        ShPrintTrace(Shell, "While");
        break;

    case ShellNodeUntil:
        ShPrintTrace(Shell, "Until");
        break;

    case ShellNodeSubshell:
        ShPrintTrace(Shell, "Subshell");
        break;

    default:

        assert(FALSE);

        break;
    }

    CurrentEntry = Node->RedirectList.Next;
    while (CurrentEntry != &(Node->RedirectList)) {
        Redirect = LIST_VALUE(CurrentEntry, SHELL_IO_REDIRECT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        switch (Redirect->Type) {
        case ShellRedirectInvalid:
            ShPrintTrace(Shell, " INVALID_REDIRECT");
            break;

        case ShellRedirectRead:
            ShPrintTrace(Shell,
                         " [%d<%s]",
                         Redirect->FileNumber,
                         Redirect->FileName);

            break;

        case ShellRedirectReadFromDescriptor:
            ShPrintTrace(Shell,
                         " [%d<&%s]",
                         Redirect->FileNumber,
                         Redirect->FileName);

            break;

        case ShellRedirectWrite:
            ShPrintTrace(Shell,
                         " [%d>%s]",
                         Redirect->FileNumber,
                         Redirect->FileName);

            break;

        case ShellRedirectWriteToDescriptor:
            ShPrintTrace(Shell,
                         " [%d>&%s]",
                         Redirect->FileNumber,
                         Redirect->FileName);

            break;

        case ShellRedirectClobber:
            ShPrintTrace(Shell,
                         " [%d>|%s]",
                         Redirect->FileNumber,
                         Redirect->FileName);

            break;

        case ShellRedirectAppend:
            ShPrintTrace(Shell,
                         " [%d>>%s]",
                         Redirect->FileNumber,
                         Redirect->FileName);

            break;

        case ShellRedirectReadWrite:
            ShPrintTrace(Shell,
                         " [%d<>%s]",
                         Redirect->FileNumber,
                         Redirect->FileName);

            break;

        case ShellRedirectHereDocument:
            ShPrintTrace(Shell,
                         " [%d<<]>>>>\n",
                         Redirect->FileNumber);

            ShPrintTrace(Shell,
                         "%s\n<<<<",
                         Redirect->HereDocument->Document);

            break;

        case ShellRedirectStrippedHereDocument:
            ShPrintTrace(Shell, " [%d<<-]>>>>\n", Redirect->FileNumber);
            ShPrintTrace(Shell, "%s\n<<<<", Redirect->HereDocument->Document);
            break;

        default:

            assert(FALSE);

            break;
        }
    }

    if (Node->RunInBackground != FALSE) {
        ShPrintTrace(Shell, " &");
    }

    if (Node->AndOr == TOKEN_DOUBLE_AND) {
        ShPrintTrace(Shell, " &&");

    } else if (Node->AndOr == TOKEN_DOUBLE_OR) {
        ShPrintTrace(Shell, " ||");
    }

    ShPrintTrace(Shell, "\n");
    if (Node->Type == ShellNodeCase) {

        //
        // Loop through every set in the case.
        //

        CurrentEntry = Node->U.Case.PatternList.Next;
        while (CurrentEntry != &(Node->U.Case.PatternList)) {
            Set = LIST_VALUE(CurrentEntry, SHELL_CASE_PATTERN_SET, ListEntry);
            CurrentEntry = CurrentEntry->Next;

            //
            // Loop through every entry in the set.
            //

            CurrentEntryEntry = Set->PatternEntryList.Next;
            while (CurrentEntryEntry != &(Set->PatternEntryList)) {
                Entry = LIST_VALUE(CurrentEntryEntry,
                                   SHELL_CASE_PATTERN_ENTRY,
                                   ListEntry);

                CurrentEntryEntry = CurrentEntryEntry->Next;
                for (DepthIndex = 0; DepthIndex < Depth + 1; DepthIndex += 1) {
                    ShPrintTrace(Shell, " ");
                }

                ShPrintTrace(Shell, "Pattern: %s\n", Entry->Pattern);
            }

            if (Set->Action != NULL) {
                ShPrintNode(Shell, Set->Action, Depth + 2);

            } else {
                for (DepthIndex = 0; DepthIndex < Depth + 2; DepthIndex += 1) {
                    ShPrintTrace(Shell, " ");
                }

                ShPrintTrace(Shell, "No Action");
            }
        }
    }

    CurrentEntry = Node->Children.Next;
    while (CurrentEntry != &(Node->Children)) {
        Child = LIST_VALUE(CurrentEntry, SHELL_NODE, SiblingListEntry);
        CurrentEntry = CurrentEntry->Next;
        ShPrintNode(Shell, Child, Depth + 1);
    }

    return;
}

BOOL
ShCreateRedirection (
    PSHELL Shell,
    PSHELL_NODE Node,
    SHELL_IO_REDIRECTION_TYPE Type,
    INT FileNumber,
    PSTR FileName,
    UINTN FileNameSize
    )

/*++

Routine Description:

    This routine creates a new I/O redirection entry and puts it on the list
    for the given node.

Arguments:

    Shell - Supplies a pointer to the shell the redirection is executing on.

    Node - Supplies a pointer to the shell node (probably a command).

    Type - Supplies the type of redirection.

    FileNumber - Supplies the file number being affected by the redirection.

    FileName - Supplies the optional name of the file being redirected to or
        from. If this parameter is not NULL, a copy of the given string will
        be made.

    FileNameSize - Supplies the size of the file name buffer in bytes.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_HERE_DOCUMENT HereDocument;
    PSHELL_IO_REDIRECT Redirect;
    BOOL Result;

    HereDocument = NULL;
    Redirect = malloc(sizeof(SHELL_IO_REDIRECT));
    if (Redirect == NULL) {
        return FALSE;
    }

    memset(Redirect, 0, sizeof(SHELL_IO_REDIRECT));
    Redirect->Type = Type;
    Redirect->FileNumber = FileNumber;

    //
    // If it's a here document, create a new structure and add it to the
    // lexer's list of pending here documents.
    //

    if ((Redirect->Type == ShellRedirectHereDocument) ||
        (Redirect->Type == ShellRedirectStrippedHereDocument)) {

        HereDocument = malloc(sizeof(SHELL_HERE_DOCUMENT));
        if (HereDocument == NULL) {
            Result = FALSE;
            goto CreateRedirectionEnd;
        }

        memset(HereDocument, 0, sizeof(SHELL_HERE_DOCUMENT));
        if (Redirect->Type == ShellRedirectStrippedHereDocument) {
            HereDocument->StripLeadingTabs = TRUE;
        }

        if (ShIsStringQuoted(FileName) != FALSE) {
            HereDocument->EndWordWasQuoted = TRUE;
        }

        HereDocument->EndWord = SwStringDuplicate(FileName, FileNameSize);
        if (HereDocument->EndWord == NULL) {
            Result = FALSE;
            goto CreateRedirectionEnd;
        }

        HereDocument->EndWordSize = FileNameSize;
        ShStringDequote(HereDocument->EndWord,
                        HereDocument->EndWordSize,
                        0,
                        &(HereDocument->EndWordSize));

        INSERT_BEFORE(&(HereDocument->ListEntry),
                      &(Shell->Lexer.HereDocumentList));

        Redirect->HereDocument = HereDocument;

    } else if (FileName != NULL) {
        Redirect->FileNameSize = FileNameSize;
        Redirect->FileName = SwStringDuplicate(FileName, FileNameSize);
        if (Redirect->FileName == NULL) {
            Result = FALSE;
            goto CreateRedirectionEnd;
        }
    }

    INSERT_BEFORE(&(Redirect->ListEntry), &(Node->RedirectList));
    Result = TRUE;

CreateRedirectionEnd:
    if (Result == FALSE) {
        if (HereDocument != NULL) {
            ShDestroyHereDocument(HereDocument);
        }

        if (Redirect != NULL) {
            ShDestroyRedirection(Redirect);
        }
    }

    return Result;
}

VOID
ShDestroyRedirection (
    PSHELL_IO_REDIRECT Redirect
    )

/*++

Routine Description:

    This routine destroys an I/O redirection entry.

Arguments:

    Redirect - Supplies a pointer to the redirect entry.

Return Value:

    None.

--*/

{

    LIST_REMOVE(&(Redirect->ListEntry));
    if (Redirect->FileName != NULL) {
        free(Redirect->FileName);
    }

    if (Redirect->HereDocument != NULL) {
        if (Redirect->HereDocument->ListEntry.Next != NULL) {
            LIST_REMOVE(&(Redirect->HereDocument->ListEntry));
        }

        ShDestroyHereDocument(Redirect->HereDocument);
    }

    free(Redirect);
    return;
}

BOOL
ShCreateAssignment (
    PSHELL_NODE Node,
    PSTR Name,
    UINTN NameSize,
    PSTR Value,
    UINTN ValueSize
    )

/*++

Routine Description:

    This routine creates an assignment structure.

Arguments:

    Node - Supplies a pointer to the node to put the assignment on.

    Name - Supplies a pointer to the name string. A copy of this string will be
        made.

    NameSize - Supplies the length of the name string including a null
        terminator.

    Value - Supplies a pointer to the value string. A copy of this string will
        be made.

    ValueSize - Supplies the length of the value string including a null
        terminator.

Return Value:

    TRUE on success.

    FALSE on allocation failure.

--*/

{

    PSHELL_ASSIGNMENT Assignment;
    BOOL Result;

    Result = FALSE;
    Assignment = malloc(sizeof(SHELL_ASSIGNMENT));
    if (Assignment == NULL) {
        return FALSE;
    }

    memset(Assignment, 0, sizeof(SHELL_ASSIGNMENT));
    Assignment->Name = SwStringDuplicate(Name, NameSize);
    if (Assignment->Name == NULL) {
        goto CreateAssignmentEnd;
    }

    Assignment->NameSize = NameSize;
    Assignment->Value = SwStringDuplicate(Value, ValueSize);
    if (Assignment->Value == NULL) {
        goto CreateAssignmentEnd;
    }

    Assignment->ValueSize = ValueSize;

    assert(Node->Type == ShellNodeSimpleCommand);

    INSERT_BEFORE(&(Assignment->ListEntry),
                  &(Node->U.SimpleCommand.AssignmentList));

    Result = TRUE;

CreateAssignmentEnd:
    if (Result == FALSE) {
        if (Assignment != NULL) {
            if (Assignment->Name != NULL) {
                free(Assignment->Name);
            }

            if (Assignment->Value != NULL) {
                free(Assignment->Value);
            }

            free(Assignment);
        }
    }

    return Result;
}

VOID
ShDestroyAssignment (
    PSHELL_ASSIGNMENT Assignment
    )

/*++

Routine Description:

    This routine destroys an assignment entry.

Arguments:

    Assignment - Supplies a pointer to the assignment.

Return Value:

    None.

--*/

{

    LIST_REMOVE(&(Assignment->ListEntry));
    if (Assignment->Name != NULL) {
        free(Assignment->Name);
    }

    if (Assignment->Value != NULL) {
        free(Assignment->Value);
    }

    free(Assignment);
    return;
}

BOOL
ShAddPatternToSet (
    PSHELL_CASE_PATTERN_SET Set,
    PSTR Pattern,
    UINTN PatternSize
    )

/*++

Routine Description:

    This routine adds a case pattern entry to the given case statement.

Arguments:

    Set - Supplies a pointer to the set of patterns.

    Pattern - Supplies a pointer to the pattern string to add. A copy of this
        string will be made.

    PatternSize - Supplies the length of the pattern string in bytes including
        the null terminator.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_CASE_PATTERN_ENTRY Entry;

    Entry = malloc(sizeof(SHELL_CASE_PATTERN_ENTRY));
    if (Entry == NULL) {
        return FALSE;
    }

    Entry->Pattern = SwStringDuplicate(Pattern, PatternSize);
    if (Entry->Pattern == NULL) {
        free(Entry);
        return FALSE;
    }

    Entry->PatternSize = PatternSize;
    INSERT_BEFORE(&(Entry->ListEntry), &(Set->PatternEntryList));
    return TRUE;
}

VOID
ShDestroyCasePatternList (
    PSHELL_NODE CaseNode
    )

/*++

Routine Description:

    This routine destroys the pattern sets in a case statement.

Arguments:

    CaseNode - Supplies a pointer to the case statement.

Return Value:

    None.

--*/

{

    PSHELL_CASE_PATTERN_ENTRY Entry;
    PSHELL_CASE_PATTERN_SET Set;

    assert(CaseNode->Type == ShellNodeCase);

    //
    // Loop through every set in the case.
    //

    while (LIST_EMPTY(&(CaseNode->U.Case.PatternList)) == FALSE) {
        Set = LIST_VALUE(CaseNode->U.Case.PatternList.Next,
                         SHELL_CASE_PATTERN_SET,
                         ListEntry);

        LIST_REMOVE(&(Set->ListEntry));
        if (Set->Action != NULL) {
            ShReleaseNode(Set->Action);
        }

        //
        // Loop through every entry in the set.
        //

        while (LIST_EMPTY(&(Set->PatternEntryList)) == FALSE) {
            Entry = LIST_VALUE(Set->PatternEntryList.Next,
                               SHELL_CASE_PATTERN_ENTRY,
                               ListEntry);

            LIST_REMOVE(&(Entry->ListEntry));
            free(Entry->Pattern);
            free(Entry);
        }

        free(Set);
    }

    return;
}

BOOL
ShAddComponentToCommand (
    PSHELL_NODE Command,
    PSTR Component,
    UINTN ComponentSize
    )

/*++

Routine Description:

    This routine adds a component to a simple command string.

Arguments:

    Command - Supplies a pointer to the simple command node.

    Component - Supplies a pointer to the component string, either the command
        or an argument.

    ComponentSize - Supplies the size of the component string in bytes
        including the null terminator.

Return Value:

    TRUE on success.

    FALSE on allocation failure.

--*/

{

    BOOL Result;
    PSHELL_NODE_SIMPLE_COMMAND SimpleCommand;

    assert(Command->Type == ShellNodeSimpleCommand);
    assert(ComponentSize != 0);

    SimpleCommand = &(Command->U.SimpleCommand);
    Result = ShStringAppend(&(SimpleCommand->Arguments),
                            &(SimpleCommand->ArgumentsSize),
                            &(SimpleCommand->ArgumentsBufferCapacity),
                            Component,
                            ComponentSize);

    return Result;
}

BOOL
ShIsStringQuoted (
    PSTR String
    )

/*++

Routine Description:

    This routine determines if any part of the given string is quoted, meaning
    it has a backslash, single quote, or double quote character in it.

Arguments:

    String - Supplies a pointer to the string to check.

Return Value:

    TRUE if the string has a quoting character in it.

    FALSE if the string is clean.

--*/

{

    while (*String != '\0') {
        if ((*String == SHELL_CONTROL_QUOTE) ||
            (*String == SHELL_CONTROL_ESCAPE)) {

            return TRUE;
        }

        String += 1;
    }

    return FALSE;
}

VOID
ShParseError (
    PSHELL Shell,
    PSHELL_NODE Node,
    PSTR Format,
    ...
    )

/*++

Routine Description:

    This routine prints a shell parse error to standard error.

Arguments:

    Shell - Supplies a pointer to the shell.

    Node - Supplies an optional pointer to the shell node being parsed.

    Format - Supplies the printf style format string.

    ... - Supplies the remaining arguments to the printf string.

Return Value:

    TRUE if the string has a quoting character in it.

    FALSE if the string is clean.

--*/

{

    va_list ArgumentList;
    ULONG LineNumber;

    if (Node != NULL) {
        LineNumber = Node->LineNumber;

    } else {
        LineNumber = Shell->Lexer.LineNumber;
    }

    fprintf(stderr, "sh: %d: ", LineNumber);
    va_start(ArgumentList, Format);
    vfprintf(stderr, Format, ArgumentList);
    va_end(ArgumentList);
    if (Shell->Lexer.TokenBufferSize != 0) {

        //
        // Make sure the buffer is null terminated, ideally not clobbering the
        // valid part of the string.
        //

        if (Shell->Lexer.TokenBufferSize < Shell->Lexer.TokenBufferCapacity) {
            Shell->Lexer.TokenBuffer[Shell->Lexer.TokenBufferSize] = '\0';

        } else {
            Shell->Lexer.TokenBuffer[Shell->Lexer.TokenBufferSize - 1] = '\0';
        }

        fprintf(stderr, " Token: %s.\n", Shell->Lexer.TokenBuffer);

    } else {
        fprintf(stderr, "\n");
    }

    return;
}

