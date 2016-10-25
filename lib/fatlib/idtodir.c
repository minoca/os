/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    idtodir.c

Abstract:

    This module stores the mappings between a starting cluster number, used
    as a file ID, and the location of its directory entry.

Author:

    Evan Green 19-Jun-2013

Environment:

    Kernel, Boot, Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/fat/fatlib.h>
#include <minoca/lib/fat/fat.h>
#include "fatlibp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the relationship between a FAT file (represented by
    a starting cluster number) and its directory entry.

Members:

    TreeNode - Stores the red-black tree information.

    Cluster - Stores the cluster of the file.

    DirectoryCluster - Stores the cluster of the directory that holds the file.

    DirectoryOffset - Stores the offset into the directory where the entry for
        this file resides.

--*/

typedef struct _FAT_FILE_MAPPING {
    RED_BLACK_TREE_NODE TreeNode;
    ULONG Cluster;
    ULONG DirectoryCluster;
    ULONGLONG DirectoryOffset;
} FAT_FILE_MAPPING, *PFAT_FILE_MAPPING;

//
// ----------------------------------------------- Internal Function Prototypes
//

COMPARISON_RESULT
FatpCompareFileMappingNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
FatpInitializeFileMappingTree (
    PFAT_VOLUME Volume
    )

/*++

Routine Description:

    This routine initializes the file mapping tree for the given volume.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

Return Value:

    None.

--*/

{

    RtlRedBlackTreeInitialize(&(Volume->FileMappingTree),
                              0,
                              FatpCompareFileMappingNodes);

    return;
}

VOID
FatpDestroyFileMappingTree (
    PFAT_VOLUME Volume
    )

/*++

Routine Description:

    This routine drains and frees all entries on the file mapping tree.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

Return Value:

    None.

--*/

{

    PFAT_FILE_MAPPING Mapping;
    PRED_BLACK_TREE_NODE Node;

    //
    // The lock isn't acquired because the volume is being destroyed, so no one
    // should be doing any accesses.
    //

    while (TRUE) {
        Node = RtlRedBlackTreeGetLowestNode(&(Volume->FileMappingTree));
        if (Node == NULL) {
            break;
        }

        Mapping = RED_BLACK_TREE_VALUE(Node, FAT_FILE_MAPPING, TreeNode);
        RtlRedBlackTreeRemove(&(Volume->FileMappingTree), Node);
        FatFreePagedMemory(Volume->Device.DeviceToken, Mapping);
    }

    return;
}

KSTATUS
FatpSetFileMapping (
    PFAT_VOLUME Volume,
    ULONG Cluster,
    ULONG DirectoryCluster,
    ULONGLONG DirectoryOffset
    )

/*++

Routine Description:

    This routine attempts to store the file mapping relationship between a
    file and its directory entry.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    Cluster - Supplies the starting cluster number of the file.

    DirectoryCluster - Supplies the cluster number of the directory.

    DirectoryOffset - Supplies the offset into the directory where the file
        resides.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    PFAT_FILE_MAPPING ExistingMapping;
    PRED_BLACK_TREE_NODE FoundNode;
    PFAT_FILE_MAPPING NewMapping;
    FAT_FILE_MAPPING Search;

    Search.Cluster = Cluster;

    //
    // Check once to see if the mapping is already there.
    //

    FatAcquireLock(Volume->Lock);
    FoundNode = RtlRedBlackTreeSearch(&(Volume->FileMappingTree),
                                      &(Search.TreeNode));

    if (FoundNode != NULL) {
        ExistingMapping = RED_BLACK_TREE_VALUE(FoundNode,
                                               FAT_FILE_MAPPING,
                                               TreeNode);

        if ((ExistingMapping->DirectoryCluster != DirectoryCluster) ||
            (ExistingMapping->DirectoryOffset != DirectoryOffset)) {

            RtlDebugPrint("FAT: Error: Cluster at directory/offset "
                          "0x%x/0x%I64x also at 0x%x/0x%I64x.\n",
                          DirectoryCluster,
                          DirectoryOffset,
                          ExistingMapping->DirectoryCluster,
                          ExistingMapping->DirectoryOffset);
        }
    }

    FatReleaseLock(Volume->Lock);
    if (FoundNode != NULL) {
        return STATUS_SUCCESS;
    }

    NewMapping = FatAllocatePagedMemory(Volume->Device.DeviceToken,
                                        sizeof(FAT_FILE_MAPPING));

    if (NewMapping == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NewMapping->Cluster = Cluster;
    NewMapping->DirectoryCluster = DirectoryCluster;
    NewMapping->DirectoryOffset = DirectoryOffset;

    //
    // Now check once again since someone may have added it since the lock was
    // released, and add it if not.
    //

    FatAcquireLock(Volume->Lock);
    FoundNode = RtlRedBlackTreeSearch(&(Volume->FileMappingTree),
                                      &(Search.TreeNode));

    if (FoundNode != NULL) {
        ExistingMapping = RED_BLACK_TREE_VALUE(FoundNode,
                                               FAT_FILE_MAPPING,
                                               TreeNode);

        ASSERT((ExistingMapping->DirectoryCluster == DirectoryCluster) &&
               (ExistingMapping->DirectoryOffset == DirectoryOffset));

    } else {
        RtlRedBlackTreeInsert(&(Volume->FileMappingTree),
                              &(NewMapping->TreeNode));
    }

    FatReleaseLock(Volume->Lock);

    //
    // If the mapping was already there, free the new one.
    //

    if (FoundNode != NULL) {
        FatFreePagedMemory(Volume->Device.DeviceToken, NewMapping);
    }

    return STATUS_SUCCESS;
}

VOID
FatpUnsetFileMapping (
    PFAT_VOLUME Volume,
    ULONG Cluster
    )

/*++

Routine Description:

    This routine unsets the mapping for the given cluster number.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    Cluster - Supplies the starting cluster number of the file.

Return Value:

    None.

--*/

{

    PFAT_FILE_MAPPING ExistingMapping;
    PRED_BLACK_TREE_NODE FoundNode;
    FAT_FILE_MAPPING Search;

    ExistingMapping = NULL;
    Search.Cluster = Cluster;

    //
    // Check once to see if the mapping is already there.
    //

    FatAcquireLock(Volume->Lock);
    FoundNode = RtlRedBlackTreeSearch(&(Volume->FileMappingTree),
                                      &(Search.TreeNode));

    if (FoundNode != NULL) {
        ExistingMapping = RED_BLACK_TREE_VALUE(FoundNode,
                                               FAT_FILE_MAPPING,
                                               TreeNode);

        RtlRedBlackTreeRemove(&(Volume->FileMappingTree), FoundNode);

    } else {

        //
        // The mapping really is expected to be there, there's no good reason
        // why it shouldn't be.
        //

        ASSERT(FALSE);

    }

    FatReleaseLock(Volume->Lock);
    if (FoundNode == NULL) {
        return;
    }

    FatFreePagedMemory(Volume->Device.DeviceToken, ExistingMapping);
    return;
}

KSTATUS
FatpGetFileMapping (
    PFAT_VOLUME Volume,
    ULONG Cluster,
    PULONG DirectoryCluster,
    PULONGLONG DirectoryOffset
    )

/*++

Routine Description:

    This routine gets the global offset of the directory entry for the file
    starting at this cluster.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    Cluster - Supplies the starting cluster number of the file.

    DirectoryCluster - Supplies a pointer where the directory cluster will be
        returned on success.

    DirectoryOffset - Supplies a pointer where the offset within the directory
        where the file entry begins will be returned on success.

Return Value:

    STATUS_SUCCESS if the entry was successfully looked up and returned.

    STATUS_NOT_FOUND if no entry could be found for the mapping.

--*/

{

    PFAT_FILE_MAPPING ExistingMapping;
    PRED_BLACK_TREE_NODE FoundNode;
    FAT_FILE_MAPPING Search;
    KSTATUS Status;

    Search.Cluster = Cluster;
    FatAcquireLock(Volume->Lock);
    FoundNode = RtlRedBlackTreeSearch(&(Volume->FileMappingTree),
                                      &(Search.TreeNode));

    if (FoundNode != NULL) {
        ExistingMapping = RED_BLACK_TREE_VALUE(FoundNode,
                                               FAT_FILE_MAPPING,
                                               TreeNode);

        *DirectoryCluster = ExistingMapping->DirectoryCluster;
        *DirectoryOffset = ExistingMapping->DirectoryOffset;
        Status = STATUS_SUCCESS;

    } else {

        //
        // The operation is not expected to fail, most likely this represents a
        // bug in the FAT file system code.
        //

        ASSERT(FALSE);

        *DirectoryCluster = 0;
        *DirectoryOffset = 0;
        Status = STATUS_NOT_FOUND;
    }

    FatReleaseLock(Volume->Lock);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

COMPARISON_RESULT
FatpCompareFileMappingNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares FAT file mapping nodes by their cluster numbers.

Arguments:

    Tree - Supplies a pointer to the Red-Black tree that owns both nodes.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

{

    PFAT_FILE_MAPPING First;
    PFAT_FILE_MAPPING Second;

    First = RED_BLACK_TREE_VALUE(FirstNode, FAT_FILE_MAPPING, TreeNode);
    Second = RED_BLACK_TREE_VALUE(SecondNode, FAT_FILE_MAPPING, TreeNode);
    if (First->Cluster > Second->Cluster) {
        return ComparisonResultDescending;
    }

    if (First->Cluster < Second->Cluster) {
        return ComparisonResultAscending;
    }

    return ComparisonResultSame;
}

