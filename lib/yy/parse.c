/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    parse.c

Abstract:

    This module implements a very simple backtracking recursive descent parser.

Author:

    Evan Green 12-Oct-2015

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "yyp.h"

//
// --------------------------------------------------------------------- Macros
//

#define YY_PARSE_ADVANCE(_Parser)               \
    {                                           \
        (_Parser)->NextTokenIndex += 1;         \
        (_Parser)->NextToken = NULL;            \
    }

#define YY_PARSE_BACKTRACK(_Parser, _Index)     \
    {                                           \
        (_Parser)->NextTokenIndex = (_Index);   \
        (_Parser)->NextToken = NULL;            \
    }

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of elements in the first array.
//

#define YY_PARSE_INITIAL_TOKENS 64
#define YY_PARSE_INITIAL_TOKENS_SHIFT 6

//
// Define the number of child nodes a grammar node should start with.
//

#define YY_PARSE_INITIAL_CHILDREN 4

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
YypParseNode (
    PPARSER Parser,
    ULONG GrammarNode,
    PPARSER_NODE *ParsedNode
    );

KSTATUS
YypMatchRule (
    PPARSER Parser,
    ULONG GrammarNode,
    PULONG Rules,
    PPARSER_NODE Node
    );

KSTATUS
YypGetNextToken (
    PPARSER Parser,
    PLEXER_TOKEN *Token
    );

KSTATUS
YypAllocateMoreTokens (
    PPARSER Parser
    );

PLEXER_TOKEN
YypGetToken (
    PPARSER Parser,
    ULONG Index
    );

VOID
YypDestroyTokens (
    PPARSER Parser
    );

PPARSER_NODE
YypCreateNode (
    PPARSER Parser,
    ULONG GrammarElement
    );

KSTATUS
YypNodeMerge (
    PPARSER Parser,
    PPARSER_NODE Node,
    PPARSER_NODE Child
    );

KSTATUS
YypNodeAddToken (
    PPARSER Parser,
    PPARSER_NODE Node,
    PLEXER_TOKEN Token
    );

KSTATUS
YypNodeAddNode (
    PPARSER Parser,
    PPARSER_NODE Node,
    PPARSER_NODE Child
    );

VOID
YypNodeReset (
    PPARSER Parser,
    PPARSER_NODE Node,
    ULONG TokenCount,
    ULONG NodeCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

YY_API
KSTATUS
YyParserInitialize (
    PPARSER Parser
    )

/*++

Routine Description:

    This routine initializes a parser. This routine assumes the parser was
    initially zeroed out, and is currently being initialized or reset after use.

Arguments:

    Parser - Supplies a pointer to the parser to initialize.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the required fields of the lexer are not filled
    in.

--*/

{

    Parser->TokenCount = 0;
    Parser->NextTokenIndex = 0;
    Parser->NextToken = NULL;
    return STATUS_SUCCESS;
}

YY_API
VOID
YyParserReset (
    PPARSER Parser
    )

/*++

Routine Description:

    This routine resets a parser, causing it to return to its initial state but
    not forget the tokens it has seen.

Arguments:

    Parser - Supplies a pointer to the parser to reset.

Return Value:

    None.

--*/

{

    YY_PARSE_BACKTRACK(Parser, 0);
    return;
}

YY_API
VOID
YyParserDestroy (
    PPARSER Parser
    )

/*++

Routine Description:

    This routine frees all the resources associated with a given parser.

Arguments:

    Parser - Supplies a pointer to the parser.

Return Value:

    None.

--*/

{

    PPARSER_NODE NextNode;
    PPARSER_NODE Node;

    //
    // Destroy the free list of nodes.
    //

    Node = Parser->FreeNodes;
    Parser->FreeNodes = NULL;
    while (Node != NULL) {
        NextNode = Node->Nodes[0];
        if (Node->Tokens != NULL) {
            Parser->Free(Node->Tokens);
        }

        if (Node->Nodes != NULL) {
            Parser->Free(Node->Nodes);
        }

        Parser->Free(Node);
        Node = NextNode;
    }

    YypDestroyTokens(Parser);
    return;
}

YY_API
KSTATUS
YyParse (
    PPARSER Parser,
    PPARSER_NODE *Tree
    )

/*++

Routine Description:

    This routine attempts to parse input grammatically based on a set of
    grammar rules and lexer input tokens. It will build an abstract syntax
    tree, which will be returned as a node.

Arguments:

    Parser - Supplies a pointer to the parser. The caller must have filled in
        the grammar rules and support function pointers.

    Tree - Supplies a pointer where the abstract syntax tree will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_INVALID_SEQUENCE upon parse failure.

    STATUS_BUFFER_OVERRUN if the maximum recursion depth was reached.

    Other errors on other failures (such as lexer errors).

--*/

{

    KSTATUS Status;
    PLEXER_TOKEN Token;
    KSTATUS TokenStatus;

    Status = YypParseNode(Parser, Parser->GrammarStart, Tree);
    TokenStatus = YypGetNextToken(Parser, &Token);
    if (KSUCCESS(Status)) {
        if (TokenStatus != STATUS_END_OF_FILE) {
            Status = STATUS_INVALID_SEQUENCE;
        }
    }

    return Status;
}

YY_API
VOID
YyDestroyNode (
    PPARSER Parser,
    PPARSER_NODE Node
    )

/*++

Routine Description:

    This routine destroys a parser node.

Arguments:

    Parser - Supplies a pointer to the parser.

    Node - Supplies a pointer to the node to destroy.

Return Value:

    None.

--*/

{

    ULONG NodeCount;
    ULONG NodeIndex;

    NodeCount = Node->NodeCount;

    assert(Node->GrammarElement != (ULONG)-1);

    if ((Node->GrammarIndex != (ULONG)-1) && (Parser->NodeCallback != NULL)) {
        Parser->NodeCallback(Parser->Context, Node, FALSE);
    }

    for (NodeIndex = 0; NodeIndex < NodeCount; NodeIndex += 1) {
        YyDestroyNode(Parser, Node->Nodes[NodeIndex]);
    }

    Node->GrammarElement = (ULONG)-1;

    //
    // Stick it on the free list.
    //

    Node->Nodes[0] = Parser->FreeNodes;
    Parser->FreeNodes = Node;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
YypParseNode (
    PPARSER Parser,
    ULONG GrammarNode,
    PPARSER_NODE *ParsedNode
    )

/*++

Routine Description:

    This routine attempts to parse the given grammar element via a basic
    recursive descent parsing.

Arguments:

    Parser - Supplies a pointer to the parser.

    GrammarNode - Supplies the grammar node number of the grammar element to
        parse for.

    ParsedNode - Supplies a pointer where a pointer to the created and parsed
        node will be returned on success. The caller must call destroy node on
        this.

Return Value:

    Status code.

--*/

{

    PPARSER_NODE Child;
    PPARSER_GRAMMAR_ELEMENT GrammarElement;
    BOOL LeftRecursive;
    PPARSER_NODE Node;
    PPARSER_NODE OuterNode;
    ULONG RuleIndex;
    PULONG Rules;
    ULONG Start;
    KSTATUS Status;

    Node = NULL;
    Start = Parser->NextTokenIndex;
    GrammarElement = &(Parser->Grammar[GrammarNode - Parser->GrammarBase]);
    Parser->RecursionDepth += 1;
    if ((Parser->MaxRecursion != 0) &&
        (Parser->RecursionDepth > Parser->MaxRecursion)) {

        Status = STATUS_BUFFER_OVERRUN;
        goto ParseNodeEnd;
    }

    Node = YypCreateNode(Parser, GrammarNode);
    if (Node == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ParseNodeEnd;
    }

    LeftRecursive = FALSE;
    if ((Parser->Flags & YY_PARSE_FLAG_DEBUG) != 0) {
        printf("%*s %s %p\n",
               Parser->RecursionDepth,
               "",
               GrammarElement->Name,
               Node);
    }

    //
    // Go through once trying to find a simple match, and determine if the
    // rule is left recursive at the same time.
    //

    RuleIndex = 0;
    Rules = GrammarElement->Components;
    Status = STATUS_INVALID_SEQUENCE;
    while (Rules[RuleIndex] != 0) {

        //
        // If this form is left-recursive, remember that and don't try to match
        // it.
        //

        if (Rules[RuleIndex] == GrammarNode) {
            LeftRecursive = TRUE;

        //
        // Try to match the current form if there hasn't already been a match.
        //

        } else if (!KSUCCESS(Status)) {
            Status = YypMatchRule(Parser, GrammarNode, Rules + RuleIndex, Node);
            if (KSUCCESS(Status)) {
                Node->GrammarIndex = RuleIndex;
            }
        }

        if ((KSUCCESS(Status)) && (LeftRecursive != FALSE)) {
            break;
        }

        //
        // Skip to the next form, either to try and look for a match or to see
        // if the node is left recursive.
        //

        while (Rules[RuleIndex] != 0) {
            RuleIndex += 1;
        }

        RuleIndex += 1;
    }

    //
    // If something matched and the rule is left recursive, loop adding
    // as many as possible of the remainder of the left recursive rules.
    //

    if ((LeftRecursive != FALSE) && (KSUCCESS(Status))) {
        while (TRUE) {

            //
            // If nesting left recursive rules, create a new outer node,
            // add the inner one as the first child, and then try to
            // match.
            //

            if ((GrammarElement->Flags &
                 YY_GRAMMAR_NEST_LEFT_RECURSION) != 0) {

                OuterNode = YypCreateNode(Parser, GrammarNode);
                if (OuterNode == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto ParseNodeEnd;
                }

                Status = YypNodeAddNode(Parser, OuterNode, Node);
                if (!KSUCCESS(Status)) {
                    goto ParseNodeEnd;
                }

            //
            // Otherwise (in the usual case) left recursion takes the form of
            // a list, so just keep adding on tokens and elements to the same
            // node.
            //

            } else {
                OuterNode = Node;
            }

            Status = STATUS_INVALID_SEQUENCE;
            RuleIndex = 0;
            while (Rules[RuleIndex] != 0) {

                //
                // If it's a left recursive rule, try to match the remainder.
                //

                if (Rules[RuleIndex] == GrammarNode) {
                    RuleIndex += 1;
                    Status = YypMatchRule(Parser,
                                          GrammarNode,
                                          Rules + RuleIndex,
                                          OuterNode);

                    if (KSUCCESS(Status)) {
                        break;
                    }
                }

                //
                // Skip to the next form.
                //

                while (Rules[RuleIndex] != 0) {
                    RuleIndex += 1;
                }

                RuleIndex += 1;
            }

            if (!KSUCCESS(Status)) {

                //
                // Turn that frown upside down, as the non-recursive element
                // in the first loop already matched.
                //

                if ((Status == STATUS_INVALID_SEQUENCE) ||
                    (Status == STATUS_END_OF_FILE)) {

                    Status = STATUS_SUCCESS;
                }

                //
                // If there's an outer node that never matched, destroy it.
                //

                if (OuterNode != Node) {

                    assert((OuterNode->NodeCount == 1) &&
                           (OuterNode->TokenCount == 0));

                    OuterNode->NodeCount = 0;
                    YyDestroyNode(Parser, OuterNode);
                }

                break;
            }

            //
            // Perform collapsing on the inner node since it may not get the
            // treatment at the end of this function. Also remember that the
            // outer node's first child is this node.
            //

            if ((Node != OuterNode) &&
                ((GrammarElement->Flags & YY_GRAMMAR_COLLAPSE_ONE) != 0) &&
                (Node->NodeCount == 1) && (Node->TokenCount == 0)) {

                Child = Node->Nodes[0];

                assert(OuterNode->Nodes[0] == Node);

                OuterNode->Nodes[0] = Child;
                Node->NodeCount = 0;
                YyDestroyNode(Parser, Node);
                Node = Child;
            }

            //
            // Make the node to return the new outer node (no-op if not nesting
            // left-recursive nodes).
            //

            Node = OuterNode;
        }
    }

    //
    // Collapse the node if there's only one child and the grammar doesn't want
    // the intermediate node.
    //

    if ((KSUCCESS(Status)) &&
        ((GrammarElement->Flags & YY_GRAMMAR_COLLAPSE_ONE) != 0) &&
        (Node->NodeCount == 1) && (Node->TokenCount == 0)) {

        Child = Node->Nodes[0];
        Node->NodeCount = 0;
        YyDestroyNode(Parser, Node);
        Node = Child;
    }

ParseNodeEnd:
    if ((Parser->Flags & YY_PARSE_FLAG_DEBUG) != 0) {
        printf("%*s %s %p Done: %d\n",
               Parser->RecursionDepth,
               "",
               GrammarElement->Name,
               Node,
               Status);
    }

    Parser->RecursionDepth -= 1;
    if (KSUCCESS(Status)) {
        if (Parser->NodeCallback != NULL) {
            Parser->NodeCallback(Parser->Context, Node, TRUE);
        }

    } else {
        YY_PARSE_BACKTRACK(Parser, Start);
        if (Node != NULL) {
            YyDestroyNode(Parser, Node);
            Node = NULL;
        }
    }

    *ParsedNode = Node;
    return Status;
}

KSTATUS
YypMatchRule (
    PPARSER Parser,
    ULONG GrammarNode,
    PULONG Rules,
    PPARSER_NODE Node
    )

/*++

Routine Description:

    This routine attempts to match the input against a given set of rules.

Arguments:

    Parser - Supplies a pointer to the parser.

    GrammarNode - Supplies the grammar node this rule belongs to.

    Rules - Supplies a pointer to the array of elements to check.

    Node - Supplies a pointer to the node to add the elements to.

Return Value:

    Status code.

--*/

{

    PPARSER_NODE Child;
    ULONG ElementIndex;
    PSTR *ExpressionNames;
    ULONG NodeCount;
    ULONG Rule;
    ULONG Start;
    KSTATUS Status;
    PLEXER_TOKEN Token;
    ULONG TokenBase;
    ULONG TokenCount;

    ElementIndex = 0;

    assert(Node->GrammarElement != (ULONG)-1);

    //
    // Save the current state.
    //

    Start = Parser->NextTokenIndex;
    NodeCount = Node->NodeCount;
    TokenCount = Node->TokenCount;

    //
    // Try to match each element in the rule.
    //

    while (Rules[ElementIndex] != 0) {

        //
        // If the component is another rule, go try to parse that rule.
        //

        Rule = Rules[ElementIndex];
        if ((Rule >= Parser->GrammarBase) && (Rule < Parser->GrammarEnd)) {
            Status = YypParseNode(Parser, Rule, &Child);
            if (!KSUCCESS(Status)) {
                break;
            }

            Status = YypNodeAddNode(Parser, Node, Child);
            if (!KSUCCESS(Status)) {
                YyDestroyNode(Parser, Child);
                break;
            }

        //
        // Match against a token.
        //

        } else {
            Status = YypGetNextToken(Parser, &Token);
            if (Status == STATUS_END_OF_FILE) {
                goto ParseNodeEnd;

            } else if (!KSUCCESS(Status)) {
                goto ParseNodeEnd;
            }

            if (Token->Value != Rule) {
                if (((Parser->Flags &
                      YY_PARSE_FLAG_DEBUG_NON_MATCHES) != 0) &&
                    (Parser->Lexer != NULL)) {

                    ExpressionNames = Parser->Lexer->ExpressionNames;
                    if (ExpressionNames == NULL) {
                        ExpressionNames = Parser->Lexer->Expressions;
                    }

                    TokenBase = Parser->Lexer->TokenBase;
                    printf("No Match: Wanted %s got %s\n",
                           ExpressionNames[Rule - TokenBase],
                           ExpressionNames[Token->Value - TokenBase]);
                }

                break;
            }

            if (((Parser->Flags & YY_PARSE_FLAG_DEBUG_MATCHES) != 0) &&
                (Parser->Lexer != NULL)) {

                ExpressionNames = Parser->Lexer->ExpressionNames;
                if (ExpressionNames == NULL) {
                    ExpressionNames = Parser->Lexer->Expressions;
                }

                TokenBase = Parser->Lexer->TokenBase;
                printf("Match: %s (%d:%d)\n",
                       ExpressionNames[Token->Value - TokenBase],
                       Token->Line,
                       Token->Column);
            }

            Status = YypNodeAddToken(Parser, Node, Token);
            if (!KSUCCESS(Status)) {
                goto ParseNodeEnd;
            }

            YY_PARSE_ADVANCE(Parser);
        }

        ElementIndex += 1;
    }

    if (Rules[ElementIndex] == 0) {
        Status = STATUS_SUCCESS;

    } else {
        Status = STATUS_INVALID_SEQUENCE;
    }

ParseNodeEnd:

    //
    // On failure, reset the current token the node back to what it was when
    // this function began.
    //

    if (!KSUCCESS(Status)) {
        YY_PARSE_BACKTRACK(Parser, Start);
        YypNodeReset(Parser, Node, TokenCount, NodeCount);
    }

    return Status;
}

KSTATUS
YypGetNextToken (
    PPARSER Parser,
    PLEXER_TOKEN *Token
    )

/*++

Routine Description:

    This routine returns the next token in the input stream.

Arguments:

    Parser - Supplies a pointer to the parser.

    Token - Supplies a pointer where a pointer to the next token will be
        returned on success. The token will not yet be consumed.

Return Value:

    Returns a pointer to the given token.

    NULL on error or EOF.

--*/

{

    PLEXER_TOKEN NextToken;
    KSTATUS Status;

    if (Parser->NextToken != NULL) {
        *Token = Parser->NextToken;
        return STATUS_SUCCESS;
    }

    //
    // If the token's already been retrieved, then return it.
    //

    if (Parser->NextTokenIndex < Parser->TokenCount) {
        Parser->NextToken = YypGetToken(Parser, Parser->NextTokenIndex);
        *Token = Parser->NextToken;
        return STATUS_SUCCESS;
    }

    *Token = NULL;

    //
    // If the array needs to be expanded, do that now.
    //

    assert(Parser->NextTokenIndex == Parser->TokenCount);

    if (Parser->TokenCount >= Parser->TokenCapacity) {

        assert(Parser->TokenCount == Parser->TokenCapacity);

        Status = YypAllocateMoreTokens(Parser);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    NextToken = YypGetToken(Parser, Parser->NextTokenIndex);
    Status = Parser->GetToken(Parser->Context, NextToken);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Parser->NextToken = NextToken;
    Parser->TokenCount += 1;
    *Token = NextToken;
    return STATUS_SUCCESS;
}

KSTATUS
YypAllocateMoreTokens (
    PPARSER Parser
    )

/*++

Routine Description:

    This routine allocates more space in the parser for tokens, doubling each
    time.

Arguments:

    Parser - Supplies a pointer to the parser.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    ULONG ArrayCount;
    ULONG ArraySize;
    ULONG Count;
    ULONG NewArrayCount;
    PLEXER_TOKEN *NewArrays;
    ULONG NewCapacity;
    PLEXER_TOKEN NewChunk;

    //
    // The first array element holds N tokens, the second holds 2N
    // elements, the third 4N, 8N, 16N, etc.
    //

    Count = Parser->TokenCapacity;
    ArraySize = YY_PARSE_INITIAL_TOKENS;
    ArrayCount = 0;
    while (Count >= ArraySize) {
        Count -= ArraySize;
        ArraySize <<= 1;
        ArrayCount += 1;
    }

    NewArrayCount = ArrayCount + 1;
    NewCapacity = ArraySize;
    NewArrays = Parser->Allocate(NewArrayCount * sizeof(PVOID));
    if (NewArrays == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (ArrayCount != 0) {
        memcpy(NewArrays, Parser->TokenArrays, ArrayCount * sizeof(PVOID));
    }

    NewChunk = Parser->Allocate(NewCapacity * sizeof(LEXER_TOKEN));
    if (NewChunk == NULL) {
        Parser->Free(NewArrays);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (Parser->TokenArrays != NULL) {
        Parser->Free(Parser->TokenArrays);
    }

    Parser->TokenArrays = NewArrays;
    Parser->TokenCapacity += NewCapacity;
    Parser->TokenArrays[ArrayCount] = NewChunk;
    return STATUS_SUCCESS;
}

PLEXER_TOKEN
YypGetToken (
    PPARSER Parser,
    ULONG Index
    )

/*++

Routine Description:

    This routine returns a pointer to the given stored token.

Arguments:

    Parser - Supplies a pointer to the parser.

    Index - Supplies the token index to get.

Return Value:

    Returns a pointer to the given token.

--*/

{

    ULONG ArrayIndex;
    ULONG ArraySize;

    //
    // The first array element holds N tokens, the second holds 2N
    // elements, the third 4N, 8N, 16N, etc.
    //

    ArraySize = YY_PARSE_INITIAL_TOKENS;
    ArrayIndex = 0;
    while (Index >= ArraySize) {
        Index -= ArraySize;
        ArraySize <<= 1;
        ArrayIndex += 1;
    }

    return &(Parser->TokenArrays[ArrayIndex][Index]);
}

VOID
YypDestroyTokens (
    PPARSER Parser
    )

/*++

Routine Description:

    This routine frees all the tokens allocated and stored in the given parser.

Arguments:

    Parser - Supplies a pointer to the parser.

Return Value:

    None.

--*/

{

    ULONG ArrayCount;
    ULONG ArrayIndex;
    ULONG ArraySize;
    ULONG Capacity;

    Capacity = Parser->TokenCapacity;
    if (Capacity == 0) {
        return;
    }

    ArraySize = YY_PARSE_INITIAL_TOKENS;
    ArrayCount = 0;
    while (Capacity >= ArraySize) {
        Capacity -= ArraySize;
        ArraySize <<= 1;
        ArrayCount += 1;
    }

    for (ArrayIndex = 0; ArrayIndex < ArrayCount; ArrayIndex += 1) {

        assert(Parser->TokenArrays[ArrayIndex] != NULL);

        Parser->Free(Parser->TokenArrays[ArrayIndex]);
    }

    Parser->Free(Parser->TokenArrays);
    Parser->TokenArrays = NULL;
    Parser->TokenCapacity = 0;
    Parser->TokenCount = 0;
    return;
}

PPARSER_NODE
YypCreateNode (
    PPARSER Parser,
    ULONG GrammarElement
    )

/*++

Routine Description:

    This routine allocates a new parser node.

Arguments:

    Parser - Supplies a pointer to the parser.

    GrammarElement - Supplies the grammar element type for this node.

Return Value:

    Returns a pointer to a new node on success.

    NULL on allocation failure.

--*/

{

    PPARSER_NODE Node;

    //
    // Grab one off the free list if possible.
    //

    if (Parser->FreeNodes != NULL) {
        Node = Parser->FreeNodes;

        assert((Node->NodeCapacity != 0) &&
               (Node->GrammarElement == (ULONG)-1));

        Parser->FreeNodes = Node->Nodes[0];

    } else {
        Node = Parser->Allocate(sizeof(PARSER_NODE));
        if (Node == NULL) {
            return NULL;
        }

        memset(Node, 0, sizeof(PARSER_NODE));
        Node->Nodes = Parser->Allocate(
                                    YY_PARSE_INITIAL_CHILDREN * sizeof(PVOID));

        if (Node->Nodes == NULL) {
            Parser->Free(Node);
            return NULL;
        }

        Node->NodeCapacity = YY_PARSE_INITIAL_CHILDREN;
    }

    assert(GrammarElement != (ULONG)-1);

    Node->GrammarElement = GrammarElement;
    Node->GrammarIndex = (ULONG)-1;
    YypGetNextToken(Parser, &(Node->StartToken));
    Node->NodeCount = 0;
    Node->TokenCount = 0;
    return Node;
}

KSTATUS
YypNodeMerge (
    PPARSER Parser,
    PPARSER_NODE Node,
    PPARSER_NODE Child
    )

/*++

Routine Description:

    This routine merges the given child's elements onto the given node.

Arguments:

    Parser - Supplies a pointer to the parser.

    Node - Supplies a pointer to the node to add the child's elements to.

    Child - Supplies a pointer to the element to merge.

Return Value:

    Status code.

--*/

{

    ULONG Count;
    ULONG Index;
    ULONG OriginalNodes;
    ULONG OriginalTokens;
    KSTATUS Status;

    OriginalNodes = Node->NodeCount;
    OriginalTokens = Node->TokenCount;
    Count = Child->TokenCount;
    for (Index = 0; Index < Count; Index += 1) {
        Status = YypNodeAddToken(Parser, Node, Child->Tokens[Index]);
        if (!KSUCCESS(Status)) {
            goto NodeMergeEnd;
        }
    }

    Count = Child->NodeCount;
    for (Index = 0; Index < Count; Index += 1) {
        Status = YypNodeAddNode(Parser, Node, Child->Nodes[Index]);
        if (!KSUCCESS(Status)) {
            goto NodeMergeEnd;
        }
    }

    Status = STATUS_SUCCESS;

NodeMergeEnd:
    if (!KSUCCESS(Status)) {
        Node->NodeCount = OriginalNodes;
        Node->TokenCount = OriginalTokens;

    //
    // The node took over the child's children. Clear out the child's
    // children manually so as not to double free them.
    //

    } else {
        Child->NodeCount = 0;
        Child->TokenCount = 0;
    }

    return Status;
}

KSTATUS
YypNodeAddToken (
    PPARSER Parser,
    PPARSER_NODE Node,
    PLEXER_TOKEN Token
    )

/*++

Routine Description:

    This routine adds a lexer token to the given node.

Arguments:

    Parser - Supplies a pointer to the parser.

    Node - Supplies a pointer to the node to add to.

    Token - Supplies the token pointer to add.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    PVOID NewBuffer;
    ULONG NewCapacity;

    if (Node->TokenCount >= Node->TokenCapacity) {

        assert(Node->TokenCount == Node->TokenCapacity);

        if (Node->TokenCapacity == 0) {
            NewCapacity = YY_PARSE_INITIAL_CHILDREN;

        } else {
            NewCapacity = Node->TokenCapacity * 2;
        }

        NewBuffer = Parser->Allocate(NewCapacity * sizeof(PVOID));
        if (NewBuffer == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        if (Node->TokenCapacity != 0) {
            memcpy(NewBuffer, Node->Tokens, Node->TokenCount * sizeof(PVOID));
            Parser->Free(Node->Tokens);
        }

        Node->TokenCapacity = NewCapacity;
        Node->Tokens = NewBuffer;
    }

    Node->Tokens[Node->TokenCount] = Token;
    Node->TokenCount += 1;
    return STATUS_SUCCESS;
}

KSTATUS
YypNodeAddNode (
    PPARSER Parser,
    PPARSER_NODE Node,
    PPARSER_NODE Child
    )

/*++

Routine Description:

    This routine adds a child node to the given node.

Arguments:

    Parser - Supplies a pointer to the parser.

    Node - Supplies a pointer to the node to add to.

    Child - Supplies a pointer to the child node to add.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    PVOID NewBuffer;
    ULONG NewCapacity;

    assert(Child->GrammarElement != (ULONG)-1);

    if (Node->NodeCount >= Node->NodeCapacity) {

        assert(Node->NodeCount == Node->NodeCapacity);
        assert(Node->NodeCapacity != 0);

        NewCapacity = Node->NodeCapacity * 2;
        NewBuffer = Parser->Allocate(NewCapacity * sizeof(PVOID));
        if (NewBuffer == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        memcpy(NewBuffer, Node->Nodes, Node->NodeCount * sizeof(PVOID));
        Parser->Free(Node->Nodes);
        Node->NodeCapacity = NewCapacity;
        Node->Nodes = NewBuffer;
    }

    Node->Nodes[Node->NodeCount] = Child;
    Node->NodeCount += 1;
    return STATUS_SUCCESS;
}

VOID
YypNodeReset (
    PPARSER Parser,
    PPARSER_NODE Node,
    ULONG TokenCount,
    ULONG NodeCount
    )

/*++

Routine Description:

    This routine resets the given node, releasing all its children.

Arguments:

    Parser - Supplies a pointer to the parser.

    Node - Supplies a pointer to the node to reset.

    TokenCount - Supplies the number of tokens to reset the node to.

    NodeCount - Supplies the number of child nodes to reset the node to.

Return Value:

    None.

--*/

{

    ULONG Count;
    ULONG Index;

    assert((Node->TokenCount >= TokenCount) && (Node->NodeCount >= NodeCount));

    Node->TokenCount = TokenCount;
    Count = Node->NodeCount;
    for (Index = NodeCount; Index < Count; Index += 1) {
        YyDestroyNode(Parser, Node->Nodes[Index]);
    }

    Node->NodeCount = NodeCount;
    return;
}

