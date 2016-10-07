/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    const.c

Abstract:

    This module implements support for variable initializers.

Author:

    Evan Green 14-Oct-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "chalkp.h"
#include "visit.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
ChalkVisitListElementList (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a list element list.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    LONG Index;
    PCHALK_OBJECT List;
    PPARSER_NODE ParseNode;
    INT Status;

    ParseNode = Node->ParseNode;
    if (Node->ChildIndex != 0) {
        Node->Results[Node->ChildIndex - 1] = *Result;
        *Result = NULL;
    }

    Interpreter->LValue = NULL;

    //
    // If not all the list elements have been parsed yet, go get them.
    //

    if (Node->ChildIndex < ParseNode->NodeCount) {
        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[Node->ChildIndex],
                               Node->Script,
                               FALSE);

        Node->ChildIndex += 1;
        return Status;
    }

    //
    // Create a list with these elements in it.
    //

    List = ChalkCreateList(NULL, ParseNode->NodeCount);
    if (List == NULL) {
        return ENOMEM;
    }

    //
    // Add each entry in the element list to the dictionary.
    //

    for (Index = 0; Index < ParseNode->NodeCount; Index += 1) {
        Status = ChalkListSetElement(List, Index, Node->Results[Index]);
        if (Status != 0) {
            ChalkObjectReleaseReference(List);
            return Status;
        }
    }

    *Result = List;
    ChalkPopNode(Interpreter);
    return 0;
}

INT
ChalkVisitList (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a list constant.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    PPARSER_NODE ParseNode;
    INT Status;

    ParseNode = Node->ParseNode;
    Interpreter->LValue = NULL;

    assert(ParseNode->NodeCount <= 1);

    //
    // If it's an empty list, create it now. Otherwise by the time this
    // node is evaluated the list element list has already fully formed the
    // dictionary.
    //

    if (ParseNode->NodeCount == 0) {

        assert(*Result == NULL);

        *Result = ChalkCreateList(NULL, 0);
        if (*Result == NULL) {
            return ENOMEM;
        }

    } else {

        //
        // If this is the first time through, go get the list element list.
        //

        if (Node->ChildIndex < ParseNode->NodeCount) {
            Status = ChalkPushNode(Interpreter,
                                   ParseNode->Nodes[Node->ChildIndex],
                                   Node->Script,
                                   FALSE);

            Node->ChildIndex += 1;
            return Status;
        }

        assert(*Result != NULL);
    }

    ChalkPopNode(Interpreter);
    return 0;
}

INT
ChalkVisitDictElement (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a dictionary element.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    PPARSER_NODE ParseNode;
    INT Status;

    ParseNode = Node->ParseNode;

    assert(ParseNode->NodeCount == 2);

    if (Node->ChildIndex != 0) {
        Node->Results[Node->ChildIndex - 1] = *Result;
        *Result = NULL;
    }

    //
    // If not all the dict element pieces have been parsed yet, go get them.
    //

    if (Node->ChildIndex < ParseNode->NodeCount) {
        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[Node->ChildIndex],
                               Node->Script,
                               FALSE);

        Node->ChildIndex += 1;
        return Status;
    }

    Interpreter->LValue = NULL;
    *Result = ChalkCreateList(Node->Results, 2);
    if (*Result == NULL) {
        return ENOMEM;
    }

    ChalkPopNode(Interpreter);
    return 0;
}

INT
ChalkVisitDictElementList (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a dictionary element list.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    PCHALK_OBJECT Dict;
    ULONG Index;
    PCHALK_LIST List;
    PPARSER_NODE ParseNode;
    INT Status;

    ParseNode = Node->ParseNode;
    if (Node->ChildIndex != 0) {
        Node->Results[Node->ChildIndex - 1] = *Result;
        *Result = NULL;
    }

    //
    // If not all the dict elements have been parsed yet, go get them.
    //

    if (Node->ChildIndex < ParseNode->NodeCount) {
        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[Node->ChildIndex],
                               Node->Script,
                               FALSE);

        Node->ChildIndex += 1;
        return Status;
    }

    Interpreter->LValue = NULL;
    Dict = ChalkCreateDict(NULL);
    if (Dict == NULL) {
        return ENOMEM;
    }

    //
    // Add each entry in the element list to the dictionary. Each child node
    // is a dictionary element, which contains a list of the key and value.
    //

    for (Index = 0; Index < ParseNode->NodeCount; Index += 1) {
        List = (PCHALK_LIST)(Node->Results[Index]);

        assert(List->Header.Type == ChalkObjectList);

        Status = ChalkDictSetElement(Dict,
                                     List->Array[0],
                                     List->Array[1],
                                     NULL);

        if (Status != 0) {
            ChalkObjectReleaseReference(Dict);
            return Status;
        }
    }

    *Result = Dict;
    ChalkPopNode(Interpreter);
    return 0;
}

INT
ChalkVisitDict (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a dictionary constant.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    PPARSER_NODE ParseNode;
    INT Status;

    ParseNode = Node->ParseNode;
    Interpreter->LValue = NULL;

    //
    // If it's an empty dictionary, create it now.
    //

    if (ParseNode->NodeCount == 0) {

        assert(*Result == 0);

        *Result = ChalkCreateDict(NULL);
        if (*Result == NULL) {
            return ENOMEM;
        }

    } else {

        //
        // If the dict element list hasn't been evaluated yet, go get it.
        //

        assert(ParseNode->NodeCount == 1);

        if (Node->ChildIndex < ParseNode->NodeCount) {
            Status = ChalkPushNode(Interpreter,
                                   ParseNode->Nodes[Node->ChildIndex],
                                   Node->Script,
                                   FALSE);

            Node->ChildIndex += 1;
            return Status;
        }

        assert(*Result != NULL);

    }

    Interpreter->LValue = NULL;
    ChalkPopNode(Interpreter);
    return 0;
}

INT
ChalkVisitPrimaryExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a primary expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    PSTR After;
    CHAR Character;
    PSTR Destination;
    ULONG Index;
    LONGLONG Integer;
    PCHALK_OBJECT Name;
    PPARSER_NODE ParseNode;
    PSTR Source;
    INT Status;
    PLEXER_TOKEN Token;
    PSTR TokenString;
    PCHALK_OBJECT Value;

    Name = NULL;
    ParseNode = Node->ParseNode;
    Value = NULL;

    //
    // It's a dictionary or a list, just return it. Allow the LValue to pass
    // up, as (x) = 4 is allowed.
    //

    if (ParseNode->NodeCount != 0) {

        assert(ParseNode->NodeCount == 1);

        //
        // Get the item if it's not yet been parsed.
        //

        if (Node->ChildIndex < ParseNode->NodeCount) {
            Status = ChalkPushNode(Interpreter,
                                   ParseNode->Nodes[Node->ChildIndex],
                                   Node->Script,
                                   FALSE);

            Node->ChildIndex += 1;
            return Status;
        }

        assert(*Result != NULL);

    //
    // It's an identifier, constant, or string literal.
    //

    } else {

        assert(ParseNode->TokenCount == 1);
        assert(Interpreter->LValue == NULL);
        assert(*Result == NULL);

        Token = ParseNode->Tokens[0];
        TokenString = Node->Script->Data + Token->Position;
        switch (Token->Value) {

        //
        // Look up the variable value.
        //

        case ChalkTokenIdentifier:
            Name = ChalkCreateString(TokenString, Token->Size);
            if (Name == NULL) {
                Status = ENOMEM;
                goto VisitPrimaryExpressionEnd;
            }

            Value = ChalkGetVariable(Interpreter, Name, &(Interpreter->LValue));

            //
            // If the variable does not exist, create it now.
            //

            if (Value == NULL) {

                //
                // Fail if this is a variable being used before being created.
                //

                if (ChalkIsNodeAssignmentLValue(Interpreter, Node) == FALSE) {
                    fprintf(stderr,
                            "Error: '%s' used before assignment.\n",
                            Name->String.String);

                    Status = EINVAL;
                    goto VisitPrimaryExpressionEnd;
                }

                Value = ChalkCreateNull();
                if (Value == NULL) {
                    Status = ENOMEM;
                    goto VisitPrimaryExpressionEnd;
                }

                Status = ChalkSetVariable(Interpreter,
                                          Name,
                                          Value,
                                          &(Interpreter->LValue));

                if (Status != 0) {
                    goto VisitPrimaryExpressionEnd;
                }
            }

            break;

        case ChalkTokenHexInteger:
            Integer = strtoull(TokenString, &After, 16);
            Value = ChalkCreateInteger(Integer);
            break;

        case ChalkTokenOctalInteger:
            Integer = strtoull(TokenString, &After, 8);
            Value = ChalkCreateInteger(Integer);
            break;

        case ChalkTokenDecimalInteger:
            Integer = strtoull(TokenString, &After, 10);
            Value = ChalkCreateInteger(Integer);
            break;

        case ChalkTokenString:

            assert((*TokenString == '"') && (Token->Size >= 2));

            Value = ChalkCreateString(TokenString + 1, Token->Size - 1);
            if (Value == NULL) {
                break;
            }

            //
            // Convert the escaped C string into a binary string.
            //

            Source = Value->String.String;
            Destination = Source;
            while (*Source != '"') {
                if (*Source == '\\') {
                    Source += 1;
                    Character = *Source;
                    Source += 1;
                    switch (Character) {
                    case 'r':
                        *Destination = '\r';
                        break;

                    case 'n':
                        *Destination = '\n';
                        break;

                    case 'f':
                        *Destination = '\f';
                        break;

                    case 'v':
                        *Destination = '\v';
                        break;

                    case 't':
                        *Destination = '\t';
                        break;

                    case 'a':
                        *Destination = '\a';
                        break;

                    case 'b':
                        *Destination = '\b';
                        break;

                    case 'x':
                        *Destination = 0;
                        for (Index = 0; Index < 2; Index += 1) {
                            if (!isxdigit(*Source)) {
                                break;
                            }

                            *Destination <<= 4;
                            if (isdigit(*Source)) {
                                *Destination += *Source - '0';

                            } else {
                                *Destination += tolower(*Source) - 'a' + 0xA;
                            }

                            Source += 1;
                        }

                        break;

                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                        *Destination = 0;
                        for (Index = 0; Index < 3; Index += 1) {
                            if (!((*Source >= '0') && (*Source <= '7'))) {
                                break;
                            }

                            *Destination <<= 3;
                            *Destination += *Source - '0';
                        }

                        break;

                    default:
                        *Destination = Character;
                        break;
                    }

                    Destination += 1;

                } else {
                    *Destination = *Source;
                    Destination += 1;
                    Source += 1;
                }
            }

            assert(Destination <= Source);

            *Destination = '\0';
            Value->String.Size = Destination - Value->String.String;
            break;

        case ChalkTokenNull:
            Value = ChalkCreateNull();
            break;

        default:

            assert(FALSE);

            break;
        }

        if (Value == NULL) {
            Status = ENOMEM;
            goto VisitPrimaryExpressionEnd;
        }

        *Result = Value;
    }

    ChalkPopNode(Interpreter);
    Status = 0;

VisitPrimaryExpressionEnd:
    if (Status != 0) {
        if (Value != NULL) {
            ChalkObjectReleaseReference(Value);
            Value = NULL;
        }
    }

    if (Name != NULL) {
        ChalkObjectReleaseReference(Name);
    }

    return Status;
}

INT
ChalkVisitStatement (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a statement.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    //
    // Statements should never get evaluated because they collapse for single
    // element child lists and the grammar rule is composed of only that.
    //

    assert(FALSE);

    return EINVAL;
}

INT
ChalkVisitCompoundStatement (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a statement list.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    //
    // In C, a compound statement creates a new scope, and can go anywhere
    // in a function. In order for this language to support the addition of
    // dicts {}, compound statements had to be done away with except at the
    // beginning of functions and conditionals. So they no longer introduce
    // a new scope themselves.
    //

    return ChalkVisitStatementList(Interpreter, Node, Result);
}

INT
ChalkVisitStatementList (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a statement list.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    PPARSER_NODE ParseNode;
    INT Status;

    if (*Result != NULL) {
        ChalkObjectReleaseReference(*Result);
        *Result = NULL;
    }

    assert(Interpreter->LValue == NULL);

    ParseNode = Node->ParseNode;
    if (Node->ChildIndex < ParseNode->NodeCount) {
        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[Node->ChildIndex],
                               Node->Script,
                               FALSE);

        Node->ChildIndex += 1;
        return Status;
    }

    ChalkPopNode(Interpreter);
    return 0;
}

INT
ChalkVisitTranslationUnit (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a translation unit.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    //
    // Just like a statement list, a translation unit is nothing but its
    // side effects.
    //

    return ChalkVisitStatementList(Interpreter, Node, Result);
}

INT
ChalkVisitExternalDeclaration (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates an external declaration.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    //
    // Just like a statement list, an external declaration is nothing but its
    // side effects.
    //

    return ChalkVisitStatementList(Interpreter, Node, Result);
}

INT
ChalkVisitIdentifierList (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine is called to visit an identifier list.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer that on input contains a pointer to the
        previous evaluation. On output, returns a pointer to the evaluation.
        It is the caller's responsibility to release this reference on output.
        If the output value is not the same as the input value, the callee
        becomes the owner of the object, and must take responsibility for
        releasing it.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    assert(FALSE);

    return EINVAL;
}

INT
ChalkVisitFunctionDefinition (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine is called to visit a function definition node.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer that on input contains a pointer to the
        previous evaluation. On output, returns a pointer to the evaluation.
        It is the caller's responsibility to release this reference on output.
        If the output value is not the same as the input value, the callee
        becomes the owner of the object, and must take responsibility for
        releasing it.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    PCHALK_OBJECT Argument;
    ULONG ArgumentCount;
    PCHALK_OBJECT Arguments;
    PPARSER_NODE Body;
    PCHALK_OBJECT Function;
    PPARSER_NODE IdentifierList;
    ULONG Index;
    PCHALK_OBJECT Name;
    PPARSER_NODE ParseNode;
    INT Status;
    PLEXER_TOKEN Token;
    PSTR TokenString;

    Arguments = NULL;
    Function = NULL;
    Name = NULL;
    ParseNode = Node->ParseNode;

    assert((ParseNode->TokenCount == 4) &&
           ((ParseNode->NodeCount == 2) || (ParseNode->NodeCount == 1)));

    //
    // Get the name.
    //

    Token = ParseNode->Tokens[1];
    TokenString = Node->Script->Data + Token->Position;
    Name = ChalkCreateString(TokenString, Token->Size);
    if (Name == NULL) {
        Status = ENOMEM;
        goto VisitFunctionDefinitionEnd;
    }

    //
    // Get the argument name list if there is one.
    //

    if (ParseNode->NodeCount == 2) {
        IdentifierList = ParseNode->Nodes[0];
        Body = ParseNode->Nodes[1];

        assert((IdentifierList->GrammarElement == ChalkNodeIdentifierList) &&
               (IdentifierList->TokenCount != 0) &&
               (IdentifierList->NodeCount == 0));

        //
        // Create a list to hold all the argument names. The argument list goes
        // ID , ID , etc, so only every other token is an argument.
        //

        if (IdentifierList->TokenCount >= MAX_LONG) {
            fprintf(stderr, "Too many function arguments\n");
            Status = EINVAL;
            goto VisitFunctionDefinitionEnd;
        }

        ArgumentCount = (IdentifierList->TokenCount + 1) / 2;
        Arguments = ChalkCreateList(NULL, ArgumentCount);
        if (Arguments == NULL) {
            Status = ENOMEM;
            goto VisitFunctionDefinitionEnd;
        }

        //
        // Create strings for all the argument names, and stick them in the
        // list.
        //

        for (Index = 0; Index < ArgumentCount; Index += 1) {
            Token = IdentifierList->Tokens[Index * 2];
            TokenString = Node->Script->Data + Token->Position;
            Argument = ChalkCreateString(TokenString, Token->Size);
            if (Argument == NULL) {
                Status = ENOMEM;
                goto VisitFunctionDefinitionEnd;
            }

            Status = ChalkListSetElement(Arguments, Index, Argument);

            assert(Status == 0);

            ChalkObjectReleaseReference(Argument);
        }

    //
    // There is no argument name list, it was just function myfunc().
    //

    } else {
        Body = ParseNode->Nodes[0];
    }

    //
    // Create the function object itself, and set it in the current scope
    // (which should just be the global scope).
    //

    Function = ChalkCreateFunction(Arguments, Body, Node->Script);
    if (Function == NULL) {
        Status = ENOMEM;
        goto VisitFunctionDefinitionEnd;
    }

    Status = ChalkSetVariable(Interpreter,
                              Name,
                              Function,
                              &(Interpreter->LValue));

    Interpreter->LValue = NULL;
    if (Status != 0) {
        goto VisitFunctionDefinitionEnd;
    }

    ChalkPopNode(Interpreter);
    Status = 0;

VisitFunctionDefinitionEnd:
    if (Arguments != NULL) {
        ChalkObjectReleaseReference(Arguments);
    }

    if (Name != NULL) {
        ChalkObjectReleaseReference(Name);
    }

    if (Function != NULL) {
        ChalkObjectReleaseReference(Function);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

