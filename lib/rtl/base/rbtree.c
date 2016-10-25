/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rbtree.c

Abstract:

    This module implements support for a Red-Black tree. This code is loosely
    based on Emin Martinian's Red-Black tree implementation which can be found
    at http://web.mit.edu/~emin/www/source_code/red_black_tree/index.html.

Author:

    Evan Green 1-Feb-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define RED_BLACK_TREE_VALIDATE_MASK 0x000000FF

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
RtlpRedBlackTreeRotateLeft (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE OldParent
    );

VOID
RtlpRedBlackTreeRotateRight (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE OldParent
    );

VOID
RtlpRedBlackTreePerformInsert (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE NewNode
    );

PRED_BLACK_TREE_NODE
RtlpRedBlackTreeGetNextLowest (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node
    );

PRED_BLACK_TREE_NODE
RtlpRedBlackTreeGetNextHighest (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node
    );

PRED_BLACK_TREE_NODE
RtlpRedBlackTreeGetSuccessor (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node
    );

VOID
RtlpRedBlackTreeFixAfterRemoval (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node
    );

BOOL
RtlpValidateRedBlackTree (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node,
    PULONG BlackCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

RTL_API
VOID
RtlRedBlackTreeInitialize (
    PRED_BLACK_TREE Tree,
    ULONG Flags,
    PCOMPARE_RED_BLACK_TREE_NODES CompareFunction
    )

/*++

Routine Description:

    This routine initializes a Red-Black tree structure.

Arguments:

    Tree - Supplies a pointer to a tree to initialize. Tree structures should
        not be initialized more than once.

    Flags - Supplies a bitmask of flags governing the behavior of the tree. See
        RED_BLACK_TREE_FLAG_* definitions.

    CompareFunction - Supplies a pointer to a function called to compare nodes
        to each other. This routine is used on insertion, deletion, and search.

Return Value:

    None.

--*/

{

    Tree->Flags = Flags;
    Tree->CompareFunction = CompareFunction;
    Tree->Root.Red = FALSE;
    Tree->Root.LeftChild = &(Tree->NullNode);
    Tree->Root.RightChild = &(Tree->NullNode);
    Tree->Root.Parent = NULL;
    Tree->NullNode.Red = FALSE;
    Tree->NullNode.LeftChild = &(Tree->NullNode);
    Tree->NullNode.RightChild = &(Tree->NullNode);
    Tree->NullNode.Parent = NULL;
    Tree->CallCount = 0;
    return;
}

RTL_API
VOID
RtlRedBlackTreeInsert (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE NewNode
    )

/*++

Routine Description:

    This routine inserts a node into the given Red-Black tree.

Arguments:

    Tree - Supplies a pointer to a tree to insert the node into.

    NewNode - Supplies a pointer to the new node to insert.

Return Value:

    None.

--*/

{

    PRED_BLACK_TREE_NODE CurrentNode;
    PRED_BLACK_TREE_NODE Uncle;

    //
    // Insert the node into the tree as if it were a regular binary search
    // tree.
    //

    RtlpRedBlackTreePerformInsert(Tree, NewNode);

    //
    // All insertions start out Red in the hope that no work needs to be
    // performed.
    //

    NewNode->Red = TRUE;

    //
    // The insertion may have caused a Red violation, which means that a red
    // node has a red child. Loop up the tree fixing up Red violations. The
    // sentinal root is black, so this loop won't go too far.
    //

    CurrentNode = NewNode;
    while (CurrentNode->Parent->Red != FALSE) {

        //
        // Get the uncle (the parent's sibling). The logic is the same, but
        // the direction this node is as a child determines the direction of
        // rotations.
        //

        if (CurrentNode->Parent->Parent->LeftChild == CurrentNode->Parent) {
            Uncle = CurrentNode->Parent->Parent->RightChild;
            if (Uncle->Red != FALSE) {
                CurrentNode->Parent->Red = FALSE;
                Uncle->Red = FALSE;
                CurrentNode->Parent->Parent->Red = TRUE;
                CurrentNode = CurrentNode->Parent->Parent;

            } else {
                if (CurrentNode->Parent->RightChild == CurrentNode) {
                    CurrentNode = CurrentNode->Parent;
                    RtlpRedBlackTreeRotateLeft(Tree, CurrentNode);
                }

                CurrentNode->Parent->Red = FALSE;
                CurrentNode->Parent->Parent->Red = TRUE;
                RtlpRedBlackTreeRotateRight(Tree, CurrentNode->Parent->Parent);
            }

        //
        // The parent is the right child of its grandparent.
        //

        } else {
            Uncle = CurrentNode->Parent->Parent->LeftChild;
            if (Uncle->Red != FALSE) {
                CurrentNode->Parent->Red = FALSE;
                Uncle->Red = FALSE;
                CurrentNode->Parent->Parent->Red = TRUE;
                CurrentNode = CurrentNode->Parent->Parent;

            } else {
                if (CurrentNode->Parent->LeftChild == CurrentNode) {
                    CurrentNode = CurrentNode->Parent;
                    RtlpRedBlackTreeRotateRight(Tree, CurrentNode);
                }

                CurrentNode->Parent->Red = FALSE;
                CurrentNode->Parent->Parent->Red = TRUE;
                RtlpRedBlackTreeRotateLeft(Tree, CurrentNode->Parent->Parent);
            }
        }
    }

    Tree->Root.LeftChild->Red = FALSE;

    ASSERT(Tree->NullNode.Red == FALSE);
    ASSERT(Tree->Root.Red == FALSE);

    Tree->CallCount += 1;
    if (((Tree->Flags & RED_BLACK_TREE_FLAG_PERIODIC_VALIDATION) != 0) &&
        ((Tree->CallCount & RED_BLACK_TREE_VALIDATE_MASK) == 0)) {

        if (RtlValidateRedBlackTree(Tree) == FALSE) {

            ASSERT(FALSE);
        }
    }

    return;
}

RTL_API
PRED_BLACK_TREE_NODE
RtlRedBlackTreeSearch (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Value
    )

/*++

Routine Description:

    This routine searches for a node in the tree with the given value. If there
    are multiple nodes with the same value, then the first one found will be
    returned.

Arguments:

    Tree - Supplies a pointer to a tree that owns the node to search.

    Value - Supplies a pointer to a dummy node that will be passed to the
        compare function. This node only has to be filled in to the extent that
        the compare function can be called to compare its value. Usually this
        is a stack allocated variable of the parent structure with that value
        filled in.

Return Value:

    Returns a pointer to a node in the tree matching the desired value on
    success.

    NULL if a node matching the given value could not be found.

--*/

{

    COMPARISON_RESULT CompareResult;
    PRED_BLACK_TREE_NODE CurrentNode;
    PRED_BLACK_TREE_NODE NullNode;

    NullNode = &(Tree->NullNode);
    CurrentNode = Tree->Root.LeftChild;
    while (TRUE) {
        if (CurrentNode == NullNode) {
            CurrentNode = NULL;
            break;
        }

        CompareResult = Tree->CompareFunction(Tree, CurrentNode, Value);

        //
        // Break out if the value is found.
        //

        if (CompareResult == ComparisonResultSame) {
            break;

        //
        // If the current node is less than the value, go right.
        //

        } else if (CompareResult == ComparisonResultAscending) {
            CurrentNode = CurrentNode->RightChild;

        //
        // The current node is greater than the value, so go left.
        //

        } else {

            ASSERT(CompareResult == ComparisonResultDescending);

            CurrentNode = CurrentNode->LeftChild;
        }
    }

    return CurrentNode;
}

RTL_API
PRED_BLACK_TREE_NODE
RtlRedBlackTreeSearchClosest (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Value,
    BOOL GreaterThan
    )

/*++

Routine Description:

    This routine searches for a node in the tree with the given value. If there
    are multiple nodes with the same value, then the first one found will be
    returned. If no node matches the given value, then the closest node
    greater than or less than the given value (depending on the parameter) will
    be returned instead.

Arguments:

    Tree - Supplies a pointer to a tree that owns the node to search.

    Value - Supplies a pointer to a dummy node that will be passed to the
        compare function. This node only has to be filled in to the extent that
        the compare function can be called to compare its value. Usually this
        is a stack allocated variable of the parent structure with that value
        filled in.

    GreaterThan - Supplies a boolean indicating whether the closest value
        greater than the given value should be returned (TRUE) or the closest
        value less than the given value shall be returned (FALSE).

Return Value:

    Returns a pointer to a node in the tree matching the desired value on
    success.

    Returns a pointer to the closest node greater than the given value if the
    greater than parameter is set and there is a node greater than the given
    value.

    Returns a pointer to the closest node less than the given node if the
    greater than parameter is not set, and such a node exists.

    NULL if the node cannot be found and there is no node greater than (or less
    than, depending on the parameter) the given value.

--*/

{

    PRED_BLACK_TREE_NODE Closest;
    COMPARISON_RESULT CompareResult;
    PRED_BLACK_TREE_NODE CurrentNode;
    PRED_BLACK_TREE_NODE NullNode;

    NullNode = &(Tree->NullNode);
    CurrentNode = Tree->Root.LeftChild;
    Closest = NULL;
    while (TRUE) {
        if (CurrentNode == NullNode) {
            CurrentNode = NULL;
            break;
        }

        CompareResult = Tree->CompareFunction(Tree, CurrentNode, Value);

        //
        // Break out if the value is found.
        //

        if (CompareResult == ComparisonResultSame) {
            break;

        //
        // If the current node is less than the value, go right.
        //

        } else if (CompareResult == ComparisonResultAscending) {
            if (GreaterThan == FALSE) {
                Closest = CurrentNode;
            }

            CurrentNode = CurrentNode->RightChild;

        //
        // The current node is greater than the value, so go left.
        //

        } else {

            ASSERT(CompareResult == ComparisonResultDescending);

            if (GreaterThan != FALSE) {
                Closest = CurrentNode;
            }

            CurrentNode = CurrentNode->LeftChild;
        }
    }

    if (CurrentNode != NULL) {
        return CurrentNode;
    }

    return Closest;
}

RTL_API
PRED_BLACK_TREE_NODE
RtlRedBlackTreeGetLowestNode (
    PRED_BLACK_TREE Tree
    )

/*++

Routine Description:

    This routine returns the node in the given Red-Black tree with the lowest
    value.

Arguments:

    Tree - Supplies a pointer to a tree.

Return Value:

    Returns a pointer to the node with the lowest value.

    NULL if the tree is empty.

--*/

{

    PRED_BLACK_TREE_NODE Node;
    PRED_BLACK_TREE_NODE NullNode;

    Node = Tree->Root.LeftChild;
    NullNode = &(Tree->NullNode);
    if (Node == NullNode) {
        return NULL;
    }

    while (Node->LeftChild != NullNode) {
        Node = Node->LeftChild;
    }

    return Node;
}

RTL_API
PRED_BLACK_TREE_NODE
RtlRedBlackTreeGetHighestNode (
    PRED_BLACK_TREE Tree
    )

/*++

Routine Description:

    This routine returns the node in the given Red-Black tree with the highest
    value.

Arguments:

    Tree - Supplies a pointer to a tree.

Return Value:

    Returns a pointer to the node with the lowest value.

    NULL if the tree is empty.

--*/

{

    PRED_BLACK_TREE_NODE Node;
    PRED_BLACK_TREE_NODE NullNode;

    Node = Tree->Root.LeftChild;
    NullNode = &(Tree->NullNode);
    if (Node == NullNode) {
        return NULL;
    }

    while (Node->RightChild != NullNode) {
        Node = Node->RightChild;
    }

    return Node;
}

RTL_API
VOID
RtlRedBlackTreeRemove (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node
    )

/*++

Routine Description:

    This routine removes the given node from the Red-Black tree.

Arguments:

    Tree - Supplies a pointer to a tree that the node is currently inserted
        into.

    Node - Supplies a pointer to the node to remove from the tree.

Return Value:

    None.

--*/

{

    PRED_BLACK_TREE_NODE Child;
    PRED_BLACK_TREE_NODE NodeToRemove;
    PRED_BLACK_TREE_NODE NullNode;
    PRED_BLACK_TREE_NODE Successor;

    NullNode = &(Tree->NullNode);

    //
    // If the node being removed is a leaf node, then it will just get pulled
    // out. If it's not a leaf node, then find the successor. The successor
    // will get removed from its current position and glued in the original
    // nodes position.
    //

    if ((Node->LeftChild == NullNode) || (Node->RightChild == NullNode)) {
        NodeToRemove = Node;

    } else {
        NodeToRemove = RtlpRedBlackTreeGetSuccessor(Tree, Node);
    }

    ASSERT((NodeToRemove->LeftChild == NullNode) ||
           (NodeToRemove->RightChild == NullNode));

    if (NodeToRemove->LeftChild != NullNode) {
        Child = NodeToRemove->LeftChild;

    } else {
        Child = NodeToRemove->RightChild;
    }

    //
    // Patch up the child to point at the parent's parent.
    //

    Child->Parent = NodeToRemove->Parent;
    if (NodeToRemove->Parent->LeftChild == NodeToRemove) {
        NodeToRemove->Parent->LeftChild = Child;

    } else {

        ASSERT(NodeToRemove->Parent->RightChild == NodeToRemove);

        NodeToRemove->Parent->RightChild = Child;
    }

    //
    // If there's a node replacing the node being removed, fix up that
    // now-removed node to act in its new place.
    //

    ASSERT(NodeToRemove != &(Tree->NullNode));

    if (NodeToRemove != Node) {

        //
        // If a black node was just removed, fix up the carnage.
        //

        if (NodeToRemove->Red == FALSE) {
            RtlpRedBlackTreeFixAfterRemoval(Tree, Child);
        }

        Successor = NodeToRemove;
        Successor->LeftChild = Node->LeftChild;
        Successor->RightChild = Node->RightChild;
        Successor->Parent = Node->Parent;
        Successor->Red = Node->Red;
        Node->LeftChild->Parent = Successor;
        Node->RightChild->Parent = Successor;
        if (Node->Parent->LeftChild == Node) {
            Node->Parent->LeftChild = Successor;

        } else {

            ASSERT(Node->Parent->RightChild == Node);

            Node->Parent->RightChild = Successor;
        }

    } else {

        //
        // Fix up the wreckage if a black node was removed.
        //

        if (NodeToRemove->Red == FALSE) {
            RtlpRedBlackTreeFixAfterRemoval(Tree, Child);
        }
    }

    ASSERT(Tree->NullNode.Red == FALSE);

    Tree->CallCount += 1;
    if (((Tree->Flags & RED_BLACK_TREE_FLAG_PERIODIC_VALIDATION) != 0) &&
        ((Tree->CallCount & RED_BLACK_TREE_VALIDATE_MASK) == 0)) {

        if (RtlValidateRedBlackTree(Tree) == FALSE) {

            ASSERT(FALSE);
        }
    }

    return;
}

RTL_API
VOID
RtlRedBlackTreeIterate (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_ITERATION_ROUTINE Routine,
    PVOID Context
    )

/*++

Routine Description:

    This routine iterates through all nodes in a Red-Black tree (via in in
    order traversal) and calls the given routine for each node in the tree.
    The routine passed must not modify the tree.

Arguments:

    Tree - Supplies a pointer to a tree that the node is currently inserted
        into.

    Routine - Supplies a pointer to the routine that will be called for each
        node encountered.

    Context - Supplies an optional caller-provided context that will be passed
        to the interation routine for each node.

Return Value:

    None.

--*/

{

    ULONG Level;
    PRED_BLACK_TREE_NODE NextNode;
    PRED_BLACK_TREE_NODE Node;
    PRED_BLACK_TREE_NODE NullNode;
    PRED_BLACK_TREE_NODE PreviousNode;
    PRED_BLACK_TREE_NODE Root;

    NullNode = &(Tree->NullNode);
    Root = &(Tree->Root);
    PreviousNode = Root;
    Node = Root->LeftChild;
    if (Node == NullNode) {
        return;
    }

    Level = 0;
    while (Node != Root) {

        //
        // If coming from the parent, attempt to go left.
        //

        if (PreviousNode == Node->Parent) {
            NextNode = Node->LeftChild;
            Level += 1;

        //
        // If coming from the left node, print out this node and attempt to
        // go right.
        //

        } else if (PreviousNode == Node->LeftChild) {
            Routine(Tree, Node, Level, Context);
            NextNode = Node->RightChild;

            //
            // If the right child is also null, then go up now to avoid an
            // infinite loop of also matching on the left child.
            //

            if (NextNode == Node->LeftChild) {

                ASSERT(NextNode == NullNode);

                NextNode = Node->Parent;
                Level -= 1;
            }

        //
        // Otherwise, the previous node was the right child, so go up.
        //

        } else {

            ASSERT(PreviousNode == Node->RightChild);

            NextNode = Node->Parent;

            ASSERT(Level != 0);

            Level -= 1;
        }

        //
        // Move on to the next node. If it's nil, just pretend like it went
        // there and came right back up.
        //

        PreviousNode = Node;
        if (NextNode == NullNode) {
            PreviousNode = NextNode;

        } else {
            Node = NextNode;
        }
    }

    return;
}

RTL_API
PRED_BLACK_TREE_NODE
RtlRedBlackTreeGetNextNode (
    PRED_BLACK_TREE Tree,
    BOOL Descending,
    PRED_BLACK_TREE_NODE PreviousNode
    )

/*++

Routine Description:

    This routine gets the node in the Red-Black tree with the next highest
    or lower value depending on the supplied boolean.

Arguments:

    Tree - Supplies a pointer to a Red-Black tree.

    Descending - Supplies a boolean indicating if the next node should be a
        descending value or not.

    PreviousNode - Supplies a pointer to the previous node on which to base the
        search.

Return Value:

    Returns a pointer to the node in the tree with the next highest value, or
    NULL if the given previous node is the node with the highest value.

--*/

{

    PRED_BLACK_TREE_NODE NextNode;

    //
    // Return the lowest node if a previous node was not supplied.
    //

    if (PreviousNode == NULL) {
        if (Descending != FALSE) {
            return RtlRedBlackTreeGetHighestNode(Tree);

        } else {
            return RtlRedBlackTreeGetLowestNode(Tree);
        }
    }

    if (Descending != FALSE) {
        NextNode = RtlpRedBlackTreeGetNextLowest(Tree, PreviousNode);

    } else {
        NextNode = RtlpRedBlackTreeGetNextHighest(Tree, PreviousNode);
    }

    if (NextNode == &(Tree->NullNode)) {
        return NULL;
    }

    return NextNode;
}

RTL_API
BOOL
RtlValidateRedBlackTree (
    PRED_BLACK_TREE Tree
    )

/*++

Routine Description:

    This routine determines if the given Red-Black tree is valid.

    Note: This function is recursive, and should not be used outside of debug
          builds and test environments.

Arguments:

    Tree - Supplies a pointer to the tree to validate.

Return Value:

    TRUE if the tree is valid.

    FALSE if the tree is corrupt or is breaking required rules of Red-Black
    trees.

--*/

{

    ULONG BlackCount;
    BOOL Result;

    if (Tree->Root.LeftChild == &(Tree->NullNode)) {
        return TRUE;
    }

    //
    // Verify the parent link of the first real node.
    //

    if (Tree->Root.LeftChild->Parent != &(Tree->Root)) {
        RtlDebugPrint("Error: Tree 0x%x root 0x%x (NullNode 0x%x) "
                      "LeftChild 0x%x Parent was 0x%x instead of pointing "
                      "back to root.\n",
                      Tree,
                      &(Tree->Root),
                      &(Tree->NullNode),
                      Tree->Root.LeftChild,
                      Tree->Root.LeftChild->Parent);

        return FALSE;
    }

    BlackCount = 0;
    Result = RtlpValidateRedBlackTree(Tree, Tree->Root.LeftChild, &BlackCount);
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
RtlpRedBlackTreeRotateLeft (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE OldParent
    )

/*++

Routine Description:

    This routine performs a left rotation on the given Red-Black tree node.

Arguments:

    Tree - Supplies a pointer to a tree that owns the node to rotate.

    OldParent - Supplies a pointer to the node that is currently the parent,
        but will become the left child after the rotation.

Return Value:

    None.

--*/

{

    PRED_BLACK_TREE_NODE NewParent;
    PRED_BLACK_TREE_NODE NullNode;

    NullNode = &(Tree->NullNode);

    //
    // Tree rotations looks like this:
    //
    //      Q       Right Rotation        P        *
    //    /   \    --------------->     /   \      *
    //   P     c                       a     Q     *
    //  / \        <---------------         / \    *
    // a   b        Left Rotation          b   c   *
    //
    // Here, P is OldParent and Q is NewParent.
    //

    NewParent = OldParent->RightChild;

    //
    // Move the "b" subtree in the diagram over to the old parent. Fix up the
    // parent as long as this is not the null node.
    //

    OldParent->RightChild = NewParent->LeftChild;
    if (NewParent->LeftChild != NullNode) {
        NewParent->LeftChild->Parent = OldParent;
    }

    //
    // Fix up the right child (Q) to be new the parent, and fix up the parent's
    // link to that new node.
    //

    NewParent->Parent = OldParent->Parent;

    //
    // Fix up the link pointing down at this tree being messed with. The fact
    // that there is a root sentinal means no root check is needed here.
    //

    if (OldParent->Parent->LeftChild == OldParent) {
        OldParent->Parent->LeftChild = NewParent;

    } else {

        ASSERT(OldParent->Parent->RightChild == OldParent);

        OldParent->Parent->RightChild = NewParent;
    }

    //
    // Set the new parent's left child to be old parent.
    //

    NewParent->LeftChild = OldParent;
    OldParent->Parent = NewParent;

    //
    // Leaf nodes should always be black.
    //

    ASSERT(NullNode->Red == FALSE);

    return;
}

VOID
RtlpRedBlackTreeRotateRight (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE OldParent
    )

/*++

Routine Description:

    This routine performs a right rotation on the given Red-Black tree node.

Arguments:

    Tree - Supplies a pointer to a tree that owns the node to rotate.

    OldParent - Supplies a pointer to the node that is currently the parent,
        but will become the right child after the rotation.

Return Value:

    None.

--*/

{

    PRED_BLACK_TREE_NODE NewParent;
    PRED_BLACK_TREE_NODE NullNode;

    NullNode = &(Tree->NullNode);

    //
    // Tree rotations looks like this:
    //
    //      Q       Right Rotation        P        *
    //    /   \    --------------->     /   \      *
    //   P     c                       a     Q     *
    //  / \        <---------------         / \    *
    // a   b        Left Rotation          b   c   *
    //
    // Here, Q is OldParent and P is NewParent.
    //

    NewParent = OldParent->LeftChild;

    //
    // Fix up the "b" subtree in the diagram above so that it moves to the
    // left child of the old parent (Q). Fix the parent link too as long as it
    // isn't the null node.
    //

    OldParent->LeftChild = NewParent->RightChild;
    if (NewParent->RightChild != NullNode) {
        NewParent->RightChild->Parent = OldParent;
    }

    //
    // Fix up the links to put the new parent in its place. The use of a null
    // node at the root means there's no need for root checks here.
    //

    NewParent->Parent = OldParent->Parent;
    if (OldParent->Parent->LeftChild == OldParent) {
        OldParent->Parent->LeftChild = NewParent;

    } else {

        ASSERT(OldParent->Parent->RightChild == OldParent);

        OldParent->Parent->RightChild = NewParent;
    }

    //
    // Put the old parent under the new parent.
    //

    NewParent->RightChild = OldParent;
    OldParent->Parent = NewParent;

    //
    // Leaf nodes should always be black.
    //

    ASSERT(NullNode->Red == FALSE);

    return;
}

VOID
RtlpRedBlackTreePerformInsert (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE NewNode
    )

/*++

Routine Description:

    This routine performs an insertion of a new node into the given Red-Black
    tree as if it were a regular binary tree. This function should not be
    called directly by outside code, as it requires fixups after it is called.

Arguments:

    Tree - Supplies a pointer to a tree to insert into.

    NewNode - Supplies a pointer to the new node to insert.

Return Value:

    None.

--*/

{

    COMPARISON_RESULT CompareResult;
    PRED_BLACK_TREE_NODE CurrentNode;
    PRED_BLACK_TREE_NODE NullNode;
    PRED_BLACK_TREE_NODE PreviousNode;

    NullNode = &(Tree->NullNode);
    NewNode->LeftChild = NullNode;
    NewNode->RightChild = NullNode;
    PreviousNode = &(Tree->Root);
    CurrentNode = Tree->Root.LeftChild;
    while (CurrentNode != NullNode) {
        PreviousNode = CurrentNode;
        CompareResult = Tree->CompareFunction(Tree, CurrentNode, NewNode);

        //
        // If the current node is greater than the new node, then go left.
        // Otherwise, go right.
        //

        if (CompareResult == ComparisonResultDescending) {
            CurrentNode = CurrentNode->LeftChild;

        } else {

            ASSERT((CompareResult == ComparisonResultAscending) ||
                   (CompareResult == ComparisonResultSame));

            CurrentNode = CurrentNode->RightChild;
        }
    }

    //
    // The parent of the new node was found. Determine if the node should get
    // put as the left or right child of the parent.
    //

    NewNode->Parent = PreviousNode;
    if (PreviousNode == &(Tree->Root)) {
        CompareResult = ComparisonResultDescending;

    } else {
        CompareResult = Tree->CompareFunction(Tree, PreviousNode, NewNode);
    }

    if (CompareResult == ComparisonResultDescending) {
        PreviousNode->LeftChild = NewNode;

    } else {

        ASSERT((CompareResult == ComparisonResultAscending) ||
               (CompareResult == ComparisonResultSame));

        PreviousNode->RightChild = NewNode;
    }

    //
    // Leaf nodes should always be black.
    //

    ASSERT(NullNode->Red == FALSE);

    return;
}

PRED_BLACK_TREE_NODE
RtlpRedBlackTreeGetNextLowest (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node
    )

/*++

Routine Description:

    This routine gets the node in the tree with the next lowest value.

Arguments:

    Tree - Supplies a pointer to a tree.

    Node - Supplies a pointer to a node.

Return Value:

    Returns a pointer to the node with the next lowest value.

--*/

{

    PRED_BLACK_TREE_NODE NextLowest;
    PRED_BLACK_TREE_NODE NullNode;

    NullNode = &(Tree->NullNode);

    //
    // If possible, go one left and then all the way right to find the node
    // with the largest value that is still less than the current node.
    //

    NextLowest = Node->LeftChild;
    if (NextLowest != NullNode) {
        while (NextLowest->RightChild != NullNode) {
            NextLowest = NextLowest->RightChild;
        }

    //
    // There was no left child, so go up as long as this is the left child.
    //

    } else {
        NextLowest = Node->Parent;

        //
        // Because the child of the sentinal root is the left child, each loop
        // must check to see if the root has been reached.
        //

        while ((NextLowest->LeftChild == Node) &&
               (NextLowest != &(Tree->Root))) {

            Node = NextLowest;
            NextLowest = NextLowest->Parent;
        }

        if (NextLowest == &(Tree->Root)) {
            NextLowest = NullNode;
        }
    }

    return NextLowest;
}

PRED_BLACK_TREE_NODE
RtlpRedBlackTreeGetNextHighest (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node
    )

/*++

Routine Description:

    This routine gets the node in the tree with the next highest value.

Arguments:

    Tree - Supplies a pointer to a tree.

    Node - Supplies a pointer to a node.

Return Value:

    Returns a pointer to the node with the next highest value.

--*/

{

    PRED_BLACK_TREE_NODE NextHighest;
    PRED_BLACK_TREE_NODE NullNode;

    NullNode = &(Tree->NullNode);

    //
    // If possible, go one right and then all the way left to find the node
    // with the smallest value that is still greater than the current node.
    //

    NextHighest = Node->RightChild;
    if (NextHighest != NullNode) {
        while (NextHighest->LeftChild != NullNode) {
            NextHighest = NextHighest->LeftChild;
        }

    //
    // There was no right child, so go up as long as this is the right child.
    //

    } else {
        NextHighest = Node->Parent;

        //
        // This won't loop forever because the child of the sentinal root is
        // always the left child.
        //

        while (NextHighest->RightChild == Node) {
            Node = NextHighest;
            NextHighest = NextHighest->Parent;
        }

        if (NextHighest == &(Tree->Root)) {
            NextHighest = NullNode;
        }
    }

    return NextHighest;
}

PRED_BLACK_TREE_NODE
RtlpRedBlackTreeGetSuccessor (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node
    )

/*++

Routine Description:

    This routine determines which node should take the given node's place when
    it is deleted.

Arguments:

    Tree - Supplies a pointer to a tree.

    Node - Supplies a pointer to the node that will be deleted.

Return Value:

    Returns a pointer to the node that should take this nodes place.

--*/

{

    //
    // The sucessor is simply the node in the tree with the next highest value.
    //

    return RtlpRedBlackTreeGetNextHighest(Tree, Node);
}

VOID
RtlpRedBlackTreeFixAfterRemoval (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node
    )

/*++

Routine Description:

    This routine fixes up a Red-Black tree after the removal of a node.

Arguments:

    Tree - Supplies a pointer to the tree to fix up.

    Node - Supplies a pointer to the node to start fixing from, which is
        usually the child of the node removed from the tree.

Return Value:

    None.

--*/

{

    PRED_BLACK_TREE_NODE Root;
    PRED_BLACK_TREE_NODE Sibling;

    Root = Tree->Root.LeftChild;
    while ((Node->Red == FALSE) && (Node != Root)) {

        //
        // The direction of the left and right rotations depends on whether
        // this is the left or right child.
        //

        if (Node->Parent->LeftChild == Node) {
            Sibling = Node->Parent->RightChild;
            if (Sibling->Red != FALSE) {
                Sibling->Red = FALSE;
                Node->Parent->Red = TRUE;
                RtlpRedBlackTreeRotateLeft(Tree, Node->Parent);
                Sibling = Node->Parent->RightChild;
            }

            if ((Sibling->RightChild->Red == FALSE) &&
                (Sibling->LeftChild->Red == FALSE)) {

                Sibling->Red = TRUE;
                Node = Node->Parent;

            } else {
                if (Sibling->RightChild->Red == FALSE) {
                    Sibling->LeftChild->Red = FALSE;
                    Sibling->Red = TRUE;
                    RtlpRedBlackTreeRotateRight(Tree, Sibling);
                    Sibling = Node->Parent->RightChild;
                }

                Sibling->Red = Node->Parent->Red;
                Node->Parent->Red = FALSE;
                Sibling->RightChild->Red = FALSE;
                RtlpRedBlackTreeRotateLeft(Tree, Node->Parent);
                Node = Root;
            }

        //
        // This is the right child. Do the same thing but with the left and
        // right rotates switched.
        //

        } else {

            ASSERT(Node->Parent->RightChild == Node);

            Sibling = Node->Parent->LeftChild;
            if (Sibling->Red != FALSE) {
                Sibling->Red = FALSE;
                Node->Parent->Red = TRUE;
                RtlpRedBlackTreeRotateRight(Tree, Node->Parent);
                Sibling = Node->Parent->LeftChild;
            }

            if ((Sibling->RightChild->Red == FALSE) &&
                (Sibling->LeftChild->Red == FALSE)) {

                Sibling->Red = TRUE;
                Node = Node->Parent;

            } else {
                if (Sibling->LeftChild->Red == FALSE) {
                    Sibling->RightChild->Red = FALSE;
                    Sibling->Red = TRUE;
                    RtlpRedBlackTreeRotateLeft(Tree, Sibling);
                    Sibling = Node->Parent->LeftChild;
                }

                Sibling->Red = Node->Parent->Red;
                Node->Parent->Red = FALSE;
                Sibling->LeftChild->Red = FALSE;
                RtlpRedBlackTreeRotateRight(Tree, Node->Parent);
                Node = Root;
            }
        }
    }

    Node->Red = FALSE;

    ASSERT(Tree->NullNode.Red == FALSE);

    return;
}

BOOL
RtlpValidateRedBlackTree (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node,
    PULONG BlackCount
    )

/*++

Routine Description:

    This routine performs validation on a node of the given Red-Black tree.

    Note: This function is recursive, and should only be used in debug and
        test environments.

Arguments:

    Tree - Supplies a pointer to the tree that the node belongs to.

    Node - Supplies a pointer to the root of the subtree to validate.

    BlackCount - Supplies a pointer where the number of black nodes encountered
        in this subtree will be returned. This is expected to be initialized to
        zero at the start of the recursive call.

Return Value:

    TRUE if the tree is valid.

    FALSE if the tree is invalid.

--*/

{

    COMPARISON_RESULT CompareResult;
    ULONG LeftBlackCount;
    BOOL Result;
    ULONG RightBlackCount;
    BOOL SubtreeResult;

    Result = TRUE;
    if (Node == &(Tree->NullNode)) {
        return TRUE;
    }

    //
    // If the node is red, validate that neither of its children are red.
    //

    if (Node->Red != FALSE) {
        if ((Node->LeftChild->Red != FALSE) ||
            (Node->RightChild->Red != FALSE)) {

            RtlDebugPrint("Error: Red-Black Tree 0x%x has a red node 0x%x "
                          "with a red Left child 0x%x\n",
                          Tree,
                          Node,
                          Node->LeftChild);

            Result = FALSE;
        }

    } else {
        *BlackCount += 1;
    }

    //
    // Validate that the binary search properties are valid.
    //

    if (Node->LeftChild != &(Tree->NullNode)) {
        CompareResult = Tree->CompareFunction(Tree, Node->LeftChild, Node);
        if ((CompareResult != ComparisonResultSame) &&
            (CompareResult != ComparisonResultAscending)) {

            RtlDebugPrint("Error: Red-Black Tree 0x%x has a node 0x%x whose "
                          "left child 0x%x is not less than it. Compare was "
                          "%d\n",
                          Tree,
                          Node,
                          Node->LeftChild,
                          CompareResult);

            Result = FALSE;
        }

        //
        // Also validate the child's parent link.
        //

        if (Node->LeftChild->Parent != Node) {
            RtlDebugPrint("Error: Node 0x%x LeftChild 0x%x Parent is 0x%x "
                          "insteaf of pointing back to node.\n",
                          Node,
                          Node->LeftChild,
                          Node->LeftChild->Parent);
        }
    }

    if (Node->RightChild != &(Tree->NullNode)) {
        CompareResult = Tree->CompareFunction(Tree, Node->RightChild, Node);
        if ((CompareResult != ComparisonResultSame) &&
            (CompareResult != ComparisonResultDescending)) {

            RtlDebugPrint("Error: Red-Black Tree 0x%x has a node 0x%x whose "
                          "right child 0x%x is not greater than it. Compare "
                          "was %d\n",
                          Tree,
                          Node,
                          Node->RightChild,
                          CompareResult);

            Result = FALSE;
        }

        //
        // Also validate the child's parent link.
        //

        if (Node->RightChild->Parent != Node) {
            RtlDebugPrint("Error: Node 0x%x LeftChild 0x%x Parent is 0x%x "
                          "insteaf of pointing back to node.\n",
                          Node,
                          Node->LeftChild,
                          Node->LeftChild->Parent);
        }
    }

    //
    // Validate the left and right subtrees.
    //

    LeftBlackCount = 0;
    RightBlackCount = 0;
    SubtreeResult = RtlpValidateRedBlackTree(Tree,
                                             Node->LeftChild,
                                             &LeftBlackCount);

    if (SubtreeResult == FALSE) {
        Result = FALSE;
    }

    SubtreeResult = RtlpValidateRedBlackTree(Tree,
                                             Node->RightChild,
                                             &RightBlackCount);

    if (LeftBlackCount != RightBlackCount) {
        RtlDebugPrint("Error: Red-Black Tree 0x%x has a node 0x%x with a left "
                      "black count of 0x%x and a right black count of 0x%x, "
                      "which should be equal!\n",
                      Tree,
                      Node,
                      LeftBlackCount,
                      RightBlackCount);

        Result = FALSE;
    }

    *BlackCount += LeftBlackCount;
    return Result;
}

