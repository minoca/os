/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/kernel/driver.h>
#include <minoca/debug/dbgext.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    PTYPE_SYMBOL TreeType,
    ULONGLONG TreeAddress,
    PULONGLONG Node
    );

INT
ExtMdlGetNextTreeNode (
    PDEBUGGER_CONTEXT Context,
    PTYPE_SYMBOL TreeType,
    PTYPE_SYMBOL TreeNodeType,
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

    ULONGLONG BaseAddress;
    ULONGLONG DescriptorAddress;
    ULONGLONG DescriptorCount;
    PVOID DescriptorData;
    ULONG DescriptorDataSize;
    ULONGLONG DescriptorEntryAddress;
    PTYPE_SYMBOL DescriptorType;
    ULONGLONG Free;
    ULONGLONG LastEndAddress;
    ULONGLONG MdlAddress;
    PVOID MdlData;
    ULONG MdlDataSize;
    ULONGLONG MdlDescriptorCount;
    ULONGLONG MdlFreeSpace;
    ULONGLONG MdlTotalSpace;
    PTYPE_SYMBOL MdlType;
    ULONGLONG MemoryType;
    ULONGLONG Size;
    INT Status;
    ULONGLONG Total;
    ULONGLONG TreeAddress;
    ULONG TreeNodeOffset;
    PTYPE_SYMBOL TreeNodeType;
    ULONG TreeOffset;
    PTYPE_SYMBOL TreeType;

    DescriptorData = NULL;
    MdlData = NULL;
    MdlType = NULL;
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
        goto ExtMdlEnd;
    }

    DbgOut("Dumping MDL at 0x%08I64x\n", MdlAddress);
    Status = DbgReadTypeByName(Context,
                               MdlAddress,
                               "MEMORY_DESCRIPTOR_LIST",
                               &MdlType,
                               &MdlData,
                               &MdlDataSize);

    if (Status != 0) {
        DbgOut("Error: Could not read mdl at 0x%I64x: %s\n",
               MdlAddress,
               strerror(Status));

        goto ExtMdlEnd;
    }

    //
    // Bail if no descriptors are there.
    //

    Status = DbgReadIntegerMember(Context,
                                  MdlType,
                                  "DescriptorCount",
                                  MdlAddress,
                                  MdlData,
                                  MdlDataSize,
                                  &DescriptorCount);

    if (Status != 0) {
        goto ExtMdlEnd;
    }

    if (DescriptorCount == 0) {
        DbgOut("No Descriptors.\n");
        Status = 0;
        goto ExtMdlEnd;
    }

    //
    // Get the red black tree types.
    //

    Status = DbgGetTypeByName(Context, "RED_BLACK_TREE", &TreeType);
    if (Status != 0) {
        goto ExtMdlEnd;
    }

    Status = DbgGetTypeByName(Context, "RED_BLACK_TREE_NODE", &TreeNodeType);
    if (Status != 0) {
        goto ExtMdlEnd;
    }

    Status = DbgGetTypeByName(Context, "MEMORY_DESCRIPTOR", &DescriptorType);
    if (Status != 0) {
        goto ExtMdlEnd;
    }

    Status = DbgGetMemberOffset(DescriptorType,
                                "TreeNode",
                                &TreeNodeOffset,
                                NULL);

    if (Status != 0) {
        goto ExtMdlEnd;
    }

    TreeNodeOffset /= BITS_PER_BYTE;
    DbgOut("\n       Start Address    End Address  Size   Type\n");
    DbgOut("-----------------------------------------------------------\n");

    //
    // Loop while the descriptor pointer doesn't equal the head of the list.
    //

    DescriptorCount = 0;
    Free = 0;
    Total = 0;
    LastEndAddress = 0;
    Status = DbgGetMemberOffset(MdlType, "Tree", &TreeOffset, NULL);
    if (Status != 0) {
        goto ExtMdlEnd;
    }

    TreeAddress = MdlAddress + (TreeOffset / BITS_PER_BYTE);
    Status = ExtMdlGetFirstTreeNode(Context,
                                    TreeType,
                                    TreeAddress,
                                    &DescriptorEntryAddress);

    if (Status != 0) {
        goto ExtMdlEnd;
    }

    while (DescriptorEntryAddress != 0) {

        //
        // Read in the descriptor.
        //

        DescriptorAddress = DescriptorEntryAddress - TreeNodeOffset;

        assert(DescriptorData == NULL);

        Status = DbgReadType(Context,
                             DescriptorAddress,
                             DescriptorType,
                             &DescriptorData,
                             &DescriptorDataSize);

        if (Status != 0) {
            DbgOut("Error: Could not read descriptor at 0x%08I64x.\n",
                   DescriptorAddress);

            goto ExtMdlEnd;
        }

        Status = DbgReadIntegerMember(Context,
                                      DescriptorType,
                                      "BaseAddress",
                                      DescriptorAddress,
                                      DescriptorData,
                                      DescriptorDataSize,
                                      &BaseAddress);

        if (Status != 0) {
            goto ExtMdlEnd;
        }

        Status = DbgReadIntegerMember(Context,
                                      DescriptorType,
                                      "Size",
                                      DescriptorAddress,
                                      DescriptorData,
                                      DescriptorDataSize,
                                      &Size);

        if (Status != 0) {
            goto ExtMdlEnd;
        }

        Status = DbgReadIntegerMember(Context,
                                      DescriptorType,
                                      "Type",
                                      DescriptorAddress,
                                      DescriptorData,
                                      DescriptorDataSize,
                                      &MemoryType);

        if (Status != 0) {
            goto ExtMdlEnd;
        }

        DbgOut("    %13I64x  %13I64x  %8I64x  ",
               BaseAddress,
               BaseAddress + Size,
               Size);

        Status = DbgPrintTypeMember(Context,
                                    DescriptorAddress,
                                    DescriptorData,
                                    DescriptorDataSize,
                                    DescriptorType,
                                    "Type",
                                    0,
                                    0);

        if (Status != 0) {
            DbgOut("Error: Could not print memory type.\n");
        }

        DbgOut("\n");
        DescriptorCount += 1;
        Total += Size;
        if (MemoryType == MemoryTypeFree) {
            Free += Size;
        }

        if (BaseAddress + Size < LastEndAddress) {
            DbgOut("Error: Overlapping or out of order descriptors. Last "
                   "ending address was 0x%08I64x, current is 0x%08I64x.\n",
                   LastEndAddress,
                   BaseAddress + Size);
        }

        LastEndAddress = BaseAddress + Size;
        Status = ExtMdlGetNextTreeNode(Context,
                                       TreeType,
                                       TreeNodeType,
                                       TreeAddress,
                                       &DescriptorEntryAddress);

        if (Status != 0) {
            return Status;
        }

        free(DescriptorData);
        DescriptorData = NULL;
    }

    DbgOut("-----------------------------------------------------------\n");
    Status = DbgReadIntegerMember(Context,
                                  MdlType,
                                  "DescriptorCount",
                                  MdlAddress,
                                  MdlData,
                                  MdlDataSize,
                                  &MdlDescriptorCount);

    if (Status != 0) {
        goto ExtMdlEnd;
    }

    Status = DbgReadIntegerMember(Context,
                                  MdlType,
                                  "TotalSpace",
                                  MdlAddress,
                                  MdlData,
                                  MdlDataSize,
                                  &MdlTotalSpace);

    if (Status != 0) {
        goto ExtMdlEnd;
    }

    Status = DbgReadIntegerMember(Context,
                                  MdlType,
                                  "FreeSpace",
                                  MdlAddress,
                                  MdlData,
                                  MdlDataSize,
                                  &MdlFreeSpace);

    if (Status != 0) {
        goto ExtMdlEnd;
    }

    if (DescriptorCount != MdlDescriptorCount) {
        DbgOut("WARNING: The MDL claims there are %I64d descriptors, but %I64d "
               "were described here!\n",
               MdlDescriptorCount,
               DescriptorCount);
    }

    DbgOut("Descriptor Count: %I64d  Free: 0x%I64x  Used: 0x%I64x  "
           "Total: 0x%I64x\n\n",
           MdlDescriptorCount,
           Free,
           Total - Free,
           Total);

    if (Total != MdlTotalSpace) {
        DbgOut("Warning: MDL reported 0x%I64x total, but 0x%I64x was "
               "calculated.\n",
               MdlTotalSpace,
               Total);
    }

    if (Free != MdlFreeSpace) {
        DbgOut("Warning: MDL reported 0x%I64x free, but 0x%I64x was "
               "calculated.\n",
               MdlFreeSpace,
               Free);
    }

ExtMdlEnd:
    if (MdlData != NULL) {
        free(MdlData);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ExtMdlGetFirstTreeNode (
    PDEBUGGER_CONTEXT Context,
    PTYPE_SYMBOL TreeType,
    ULONGLONG TreeAddress,
    PULONGLONG Node
    )

/*++

Routine Description:

    This routine initializes an iterator through a red black tree.

Arguments:

    Context - Supplies a pointer to the application context.

    MdlType - Supplies a pointer to the MEMORY_DESCRIPTOR_LIST type.

    TreeType - Supplies a pointer to the RED_BLACK_TREE type.

    TreeAddress - Supplies the target address of the tree.

    Node - Supplies a pointer where the initialized first node will be
        returned. This is the first node to iterate over.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PVOID Data;
    ULONG DataSize;
    ULONGLONG LeftChild;
    ULONGLONG NullNode;
    ULONG NullNodeOffset;
    INT Result;
    PTYPE_SYMBOL TreeNodeType;

    Data = NULL;
    Result = DbgGetMemberOffset(TreeType, "NullNode", &NullNodeOffset, NULL);
    if (Result != 0) {
        return Result;
    }

    NullNodeOffset /= BITS_PER_BYTE;
    NullNode = TreeAddress + NullNodeOffset;

    //
    // Get the root's left child.
    //

    Result = DbgReadTypeByName(Context,
                               TreeAddress,
                               "RED_BLACK_TREE.Root",
                               &TreeNodeType,
                               &Data,
                               &DataSize);

    if (Result != 0) {
        return Result;
    }

    *Node = 0;
    Result = DbgReadIntegerMember(Context,
                                  TreeNodeType,
                                  "LeftChild",
                                  0,
                                  Data,
                                  DataSize,
                                  &LeftChild);

    if (Result != 0) {
        return Result;
    }

    //
    // If the root's left child is NULL, then the tree is empty.
    //

    if (LeftChild == NullNode) {
        *Node = 0;
        goto GetFirstTreeNodeEnd;
    }

    //
    // Go left as far as possible.
    //

    while (LeftChild != NullNode) {
        *Node = LeftChild;
        free(Data);
        Data = NULL;
        Result = DbgReadType(Context,
                             LeftChild,
                             TreeNodeType,
                             &Data,
                             &DataSize);

        if (Result != 0) {
            goto GetFirstTreeNodeEnd;
        }

        Result = DbgReadIntegerMember(Context,
                                      TreeNodeType,
                                      "LeftChild",
                                      0,
                                      Data,
                                      DataSize,
                                      &LeftChild);

        if (Result != 0) {
            goto GetFirstTreeNodeEnd;
        }
    }

GetFirstTreeNodeEnd:
    if (Data != NULL) {
        free(Data);
    }

    return Result;
}

INT
ExtMdlGetNextTreeNode (
    PDEBUGGER_CONTEXT Context,
    PTYPE_SYMBOL TreeType,
    PTYPE_SYMBOL TreeNodeType,
    ULONGLONG TreeAddress,
    PULONGLONG Node
    )

/*++

Routine Description:

    This routine returns the first or next node in a red black tree.

Arguments:

    Context - Supplies a pointer to the debugger context.

    TreeType - Supplies a pointer to the RED_BLACK_TREE type.

    TreeNodeType - Supplies a pointer to the RED_BLACK_TREE_NODE type.

    TreeAddress - Supplies the target address of the tree.

    Node - Supplies a pointer to the node variable, which will be updated by
        this routine. If updated to NULL, the iteration is complete.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PVOID Data;
    ULONG DataSize;
    ULONGLONG LeftChild;
    ULONGLONG NextHighest;
    ULONGLONG NullNode;
    ULONG NullNodeOffset;
    INT Result;
    ULONGLONG RightChild;
    ULONGLONG Root;
    ULONG RootOffset;

    Data = NULL;

    //
    // Get the addresses of the null node and the root node.
    //

    Result = DbgGetMemberOffset(TreeType, "NullNode", &NullNodeOffset, NULL);
    if (Result != 0) {
        return Result;
    }

    NullNodeOffset /= BITS_PER_BYTE;
    NullNode = TreeAddress + NullNodeOffset;
    Result = DbgGetMemberOffset(TreeType, "Root", &RootOffset, NULL);
    if (Result != 0) {
        return Result;
    }

    RootOffset /= BITS_PER_BYTE;
    Root = TreeAddress + RootOffset;
    if (*Node == 0) {
        return EINVAL;
    }

    //
    // Read the node.
    //

    Result = DbgReadType(Context, *Node, TreeNodeType, &Data, &DataSize);
    if (Result != 0) {
        goto GetNextTreeNodeEnd;
    }

    //
    // If possible, go one right and then all the way left to find the node
    // with the smallest value that is still greater than the current node.
    //

    Result = DbgReadIntegerMember(Context,
                                  TreeNodeType,
                                  "RightChild",
                                  0,
                                  Data,
                                  DataSize,
                                  &NextHighest);

    if (Result != 0) {
        goto GetNextTreeNodeEnd;
    }

    if (NextHighest != NullNode) {
        while (TRUE) {
            free(Data);
            Data = NULL;
            Result = DbgReadType(Context,
                                 NextHighest,
                                 TreeNodeType,
                                 &Data,
                                 &DataSize);

            if (Result != 0) {
                goto GetNextTreeNodeEnd;
            }

            Result = DbgReadIntegerMember(Context,
                                          TreeNodeType,
                                          "LeftChild",
                                          0,
                                          Data,
                                          DataSize,
                                          &LeftChild);

            if (Result != 0) {
                goto GetNextTreeNodeEnd;
            }

            if (LeftChild == NullNode) {
                break;
            }

            NextHighest = LeftChild;
        }

    //
    // There was no right child, so go up as long as this is the right child.
    //

    } else {
        Result = DbgReadIntegerMember(Context,
                                      TreeNodeType,
                                      "Parent",
                                      0,
                                      Data,
                                      DataSize,
                                      &NextHighest);

        if (Result != 0) {
            goto GetNextTreeNodeEnd;
        }

        //
        // This won't loop forever because the child of the sentinal root is
        // always the left child.
        //

        while (TRUE) {
            Result = DbgReadType(Context,
                                 NextHighest,
                                 TreeNodeType,
                                 &Data,
                                 &DataSize);

            if (Result != 0) {
                goto GetNextTreeNodeEnd;
            }

            Result = DbgReadIntegerMember(Context,
                                          TreeNodeType,
                                          "RightChild",
                                          0,
                                          Data,
                                          DataSize,
                                          &RightChild);

            if (Result != 0) {
                goto GetNextTreeNodeEnd;
            }

            if (RightChild != *Node) {
                break;
            }

            *Node = NextHighest;
            Result = DbgReadIntegerMember(Context,
                                          TreeNodeType,
                                          "Parent",
                                          0,
                                          Data,
                                          DataSize,
                                          &NextHighest);

            if (Result != 0) {
                goto GetNextTreeNodeEnd;
            }
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

GetNextTreeNodeEnd:
    if (Data != NULL) {
        free(Data);
    }

    return Result;
}

