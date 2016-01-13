/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    memory.c

Abstract:

    This module implements Memory Management related debugger extensions.

Author:

    Evan Green 10-Sep-2012

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/driver.h>
#include "dbgext.h"

#include <errno.h>
#include <stdio.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ExtMdlGetFirstTreeNode (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG TreeAddress,
    PULONGLONG Node
    );

INT
ExtMdlGetNextTreeNode (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG TreeAddress,
    PULONGLONG Node
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
ExtMdl (
    PDEBUGGER_CONTEXT Context,
    PSTR Command,
    ULONG ArgumentCount,
    PSTR *ArgumentValues
    )

/*++

Routine Description:

    This routine prints out the contents of a Memory Descriptor List. Arguments
    to the extension are:

        Address - Supplies the address of the MDL.

Arguments:

    Context - Supplies a pointer to the debugger applicaton context, which is
        an argument to most of the API functions.

    Command - Supplies the subcommand entered. This parameter is unused.

    ArgumentCount - Supplies the number of arguments in the ArgumentValues
        array.

    ArgumentValues - Supplies the values of each argument. This memory will be
        reused when the function returns, so extensions must not touch this
        memory after returning from this call.

Return Value:

    0 if the debugger extension command was successful.

    Returns an error code if a failure occurred along the way.

--*/

{

    ULONG BytesRead;
    MEMORY_DESCRIPTOR Descriptor;
    ULONGLONG DescriptorAddress;
    ULONG DescriptorCount;
    ULONGLONG DescriptorEntryAddress;
    ULONGLONG Free;
    ULONGLONG LastEndAddress;
    MEMORY_DESCRIPTOR_LIST Mdl;
    ULONGLONG MdlAddress;
    INT Status;
    ULONGLONG Total;
    ULONGLONG TreeAddress;

    if ((Command != NULL) || (ArgumentCount != 2)) {
        DbgOut("Usage: !mdl <MdlAddress>.\n"
               "       The MDL extension prints out the contents of a Memory "
               "Descriptor List.\n"
               "       MdlAddress - Supplies the address of the MDL to dump."
               "\n");

        return EINVAL;
    }

    //
    // Get the address of the MDL and read in the structure.
    //

    Status = DbgEvaluate(Context, ArgumentValues[1], &MdlAddress);
    if (Status != 0) {
        DbgOut("Error: Unable to evaluate Address parameter.\n");
        return Status;
    }

    DbgOut("Dumping MDL at 0x%08I64x\n", MdlAddress);
    Status = DbgReadMemory(Context,
                           TRUE,
                           MdlAddress,
                           sizeof(MEMORY_DESCRIPTOR_LIST),
                           &Mdl,
                           &BytesRead);

    if ((Status != 0) || (BytesRead != sizeof(MEMORY_DESCRIPTOR_LIST))) {
        DbgOut("Error: Could not read MDL.\n");
        if (Status == 0) {
            Status = EINVAL;
        }

        return Status;
    }

    //
    // Bail if no descriptors are there.
    //

    if (Mdl.DescriptorCount == 0) {
        DbgOut("No Descriptors.\n");
        return 0;
    }

    DbgOut("\n       Start Address    End Address  Size   Type\n");
    DbgOut("-----------------------------------------------------------\n");

    //
    // Loop while the descriptor pointer doesn't equal the head of the list.
    //

    DescriptorCount = 0;
    Free = 0;
    Total = 0;
    LastEndAddress = 0;

    //
    // TODO: 64-bit capable.
    //

    TreeAddress = MdlAddress + FIELD_OFFSET(MEMORY_DESCRIPTOR_LIST, Tree);
    Status = ExtMdlGetFirstTreeNode(Context,
                                    TreeAddress,
                                    &DescriptorEntryAddress);

    if (Status != 0) {
        return Status;
    }

    while (DescriptorEntryAddress != 0) {

        //
        // Read in the descriptor.
        //

        DescriptorAddress = DescriptorEntryAddress -
                            FIELD_OFFSET(MEMORY_DESCRIPTOR, TreeNode);

        Status = DbgReadMemory(Context,
                               TRUE,
                               DescriptorAddress,
                               sizeof(MEMORY_DESCRIPTOR),
                               &Descriptor,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != sizeof(MEMORY_DESCRIPTOR))) {
            DbgOut("Error: Could not read descriptor at 0x%08I64x.\n",
                   DescriptorAddress);

            if (Status == 0) {
                Status = EINVAL;
            }

            return Status;
        }

        DbgOut("    %13I64x  %13I64x  %8I64x  ",
               Descriptor.BaseAddress,
               Descriptor.BaseAddress + Descriptor.Size,
               Descriptor.Size);

        Status = DbgPrintType(Context,
                              "MEMORY_TYPE",
                              &(Descriptor.Type),
                              sizeof(MEMORY_TYPE));

        if (Status != 0) {
            DbgOut("Error: Could not print memory type.\n");
        }

        DbgOut("\n");
        DescriptorCount += 1;
        Total += Descriptor.Size;
        if (Descriptor.Type == MemoryTypeFree) {
            Free += Descriptor.Size;
        }

        if (Descriptor.BaseAddress + Descriptor.Size < LastEndAddress) {
            DbgOut("Error: Overlapping or out of order descriptors. Last "
                   "ending address was 0x%08I64x, current is 0x%08I64x.\n",
                   LastEndAddress,
                   Descriptor.BaseAddress + Descriptor.Size);
        }

        LastEndAddress = Descriptor.BaseAddress + Descriptor.Size;

        //
        // TODO: 64-bit capable.
        //

        Status = ExtMdlGetNextTreeNode(Context,
                                       TreeAddress,
                                       &DescriptorEntryAddress);

        if (Status != 0) {
            return Status;
        }
    }

    DbgOut("-----------------------------------------------------------\n");
    if (DescriptorCount != Mdl.DescriptorCount) {
        DbgOut("WARNING: The MDL claims there are %d descriptors, but %d "
               "were described here!\n",
               Mdl.DescriptorCount,
               DescriptorCount);
    }

    DbgOut("Descriptor Count: %d  Free: 0x%I64x  Used: 0x%I64x  "
           "Total: 0x%I64x\n\n",
           Mdl.DescriptorCount,
           Free,
           Total - Free,
           Total);

    if (Total != Mdl.TotalSpace) {
        DbgOut("Warning: MDL reported 0x%I64x total, but 0x%I64x was "
               "calculated.\n",
               Mdl.TotalSpace,
               Total);
    }

    if (Free != Mdl.FreeSpace) {
        DbgOut("Warning: MDL reported 0x%I64x free, but 0x%I64x was "
               "calculated.\n",
               Mdl.FreeSpace,
               Free);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ExtMdlGetFirstTreeNode (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG TreeAddress,
    PULONGLONG Node
    )

/*++

Routine Description:

    This routine initializes an iterator through a red black tree.

Arguments:

    Context - Supplies a pointer to the application context.

    TreeAddress - Supplies the target address of the tree.

    Node - Supplies a pointer where the initialized first node will be
        returned. This is the first node to iterate over.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONG BytesRead;
    RED_BLACK_TREE_NODE NodeValue;
    ULONGLONG NullNode;
    INT Result;
    ULONGLONG Root;

    NullNode = TreeAddress + FIELD_OFFSET(RED_BLACK_TREE, NullNode);
    Root = TreeAddress + FIELD_OFFSET(RED_BLACK_TREE, Root);

    //
    // Read the root.
    //

    Result = DbgReadMemory(Context,
                           TRUE,
                           Root,
                           sizeof(RED_BLACK_TREE_NODE),
                           &NodeValue,
                           &BytesRead);

    if ((Result != 0) || (BytesRead != sizeof(RED_BLACK_TREE_NODE))) {
        if (Result == 0) {
            Result = EINVAL;
        }

        return Result;
    }

    //
    // If the root's left child is NULL, then the tree is empty.
    //

    *Node = (UINTN)(NodeValue.LeftChild);
    if (*Node == NullNode) {
        *Node = 0;
    }

    //
    // Go left as far as possible.
    //

    while ((UINTN)(NodeValue.LeftChild) != NullNode) {
        *Node = (UINTN)(NodeValue.LeftChild);
        Result = DbgReadMemory(Context,
                               TRUE,
                               *Node,
                               sizeof(RED_BLACK_TREE_NODE),
                               &NodeValue,
                               &BytesRead);

        if ((Result != 0) || (BytesRead != sizeof(RED_BLACK_TREE_NODE))) {
            if (Result == 0) {
                Result = EINVAL;
            }

            return Result;
        }
    }

    return 0;
}

INT
ExtMdlGetNextTreeNode (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG TreeAddress,
    PULONGLONG Node
    )

/*++

Routine Description:

    This routine returns the first or next node in a red black tree.

Arguments:

    Context - Supplies a pointer to the debugger context.

    TreeAddress - Supplies the target address of the tree.

    Node - Supplies a pointer to the node variable, which will be updated by
        this routine. If updated to NULL, the iteration is complete.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONG BytesRead;
    ULONGLONG NextHighest;
    RED_BLACK_TREE_NODE NodeValue;
    ULONGLONG NullNode;
    INT Result;
    ULONGLONG Root;

    NullNode = TreeAddress + FIELD_OFFSET(RED_BLACK_TREE, NullNode);
    Root = TreeAddress + FIELD_OFFSET(RED_BLACK_TREE, Root);
    if (*Node == 0) {
        return EINVAL;
    }

    //
    // Read the node.
    //

    Result = DbgReadMemory(Context,
                           TRUE,
                           *Node,
                           sizeof(RED_BLACK_TREE_NODE),
                           &NodeValue,
                           &BytesRead);

    if ((Result != 0) || (BytesRead != sizeof(RED_BLACK_TREE_NODE))) {
        if (Result == 0) {
            Result = EINVAL;
        }

        return Result;
    }

    //
    // If possible, go one right and then all the way left to find the node
    // with the smallest value that is still greater than the current node.
    //

    NextHighest = (UINTN)(NodeValue.RightChild);
    if (NextHighest != NullNode) {
        while (TRUE) {
            Result = DbgReadMemory(Context,
                                   TRUE,
                                   NextHighest,
                                   sizeof(RED_BLACK_TREE_NODE),
                                   &NodeValue,
                                   &BytesRead);

            if ((Result != 0) || (BytesRead != sizeof(RED_BLACK_TREE_NODE))) {
                if (Result == 0) {
                    Result = EINVAL;
                }

                return Result;
            }

            if ((UINTN)(NodeValue.LeftChild) == NullNode) {
                break;
            }

            NextHighest = (UINTN)(NodeValue.LeftChild);
        }

    //
    // There was no right child, so go up as long as this is the right child.
    //

    } else {
        NextHighest = (UINTN)(NodeValue.Parent);

        //
        // This won't loop forever because the child of the sentinal root is
        // always the left child.
        //

        while (TRUE) {
            Result = DbgReadMemory(Context,
                                   TRUE,
                                   NextHighest,
                                   sizeof(RED_BLACK_TREE_NODE),
                                   &NodeValue,
                                   &BytesRead);

            if ((Result != 0) || (BytesRead != sizeof(RED_BLACK_TREE_NODE))) {
                if (Result == 0) {
                    Result = EINVAL;
                }

                return Result;
            }

            if ((UINTN)(NodeValue.RightChild) != *Node) {
                break;
            }

            *Node = NextHighest;
            NextHighest = (UINTN)(NodeValue.Parent);
        }

        if (NextHighest == Root) {
            NextHighest = NullNode;
        }
    }

    if (NextHighest == NullNode) {
        *Node = 0;

    } else {
        *Node = NextHighest;
    }

    return 0;
}

