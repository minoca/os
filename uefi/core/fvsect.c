/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fvsect.c

Abstract:

    This module implements section extraction support for UEFI firmware
    volumes.

Author:

    Evan Green 11-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include "fwvolp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_SECTION_STREAM_NODE_MAGIC 0x6D727453 // 'mrtS'
#define EFI_SECTION_STREAM_CHILD_MAGIC 0x72745343 // 'rtSC'

#define NULL_STREAM_HANDLE 0

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines EFI firmware volume section stream data.

Members:

    Magic - Stores the magic value EFI_SECTION_STREAM_CHILD_MAGIC.

    ListEntry - Stores pointers to the next and previous child nodes in the
        stream.

    Type - Stores the type of child section.

    Size - Stores the size of the child section.

    OffsetInStream - Stores the offset from the beginning of the stream base to
        the section header in the stream.

    EncapsulatedStreamHandle - Stores 0 if the section is not an
        encapsulating section. Otherwise, it contains the stream handle of the
        encapsulated stream. This handle is always produced any time an
        encapsulating child is encountered, irrespective of whether or not the
        encapsulated stream is processed further.

    EncapsulationGuid - Stores the GUID of the encapsulation protocol.

    Event - Stores the event used to register for notification of the
        GUIDed extraction protocol arrival.

--*/

typedef struct _EFI_SECTION_CHILD_NODE {
    ULONG Magic;
    LIST_ENTRY ListEntry;
    UINT32 Type;
    UINT32 Size;
    UINT32 OffsetInStream;
    UINTN EncapsulatedStreamHandle;
    EFI_GUID *EncapsulationGuid;
    EFI_EVENT Event;
} EFI_SECTION_CHILD_NODE, *PEFI_SECTION_CHILD_NODE;

/*++

Structure Description:

    This structure defines EFI firmware volume section stream data.

Members:

    Magic - Stores the magic value EFI_SECTION_STREAM_NODE_MAGIC.

    ListEntry - Stores pointers to the next and previous stream nodes in the
        global list.

    StreamHandle - Stores the stream handle value.

    StreamBuffer - Stores a pointer to the stream data.

    StreamLength - Stores the size of the stream data in bytes.

    ChildList - Stores the list of child sections.

    AuthenticationStatus - Stores the authentication status for GUIDed
        extractions.

--*/

typedef struct _EFI_SECTION_STREAM_NODE {
    ULONG Magic;
    LIST_ENTRY ListEntry;
    UINTN StreamHandle;
    UINT8 *StreamBuffer;
    UINTN StreamLength;
    LIST_ENTRY ChildList;
    UINT32 AuthenticationStatus;
} EFI_SECTION_STREAM_NODE, *PEFI_SECTION_STREAM_NODE;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfipFvOpenSectionStream (
    UINTN SectionStreamLength,
    VOID *SectionStream,
    BOOLEAN AllocateBuffer,
    UINT32 AuthenticationStatus,
    UINTN *SectionStreamHandle
    );

EFI_STATUS
EfipFvCreateChildNode (
    PEFI_SECTION_STREAM_NODE Stream,
    UINT32 ChildOffset,
    PEFI_SECTION_CHILD_NODE *ChildNode
    );

EFI_STATUS
EfipFvFindChildNode (
    PEFI_SECTION_STREAM_NODE SourceStream,
    EFI_SECTION_TYPE SearchType,
    UINTN *SectionInstance,
    EFI_GUID *SectionDefinitionGuid,
    PEFI_SECTION_CHILD_NODE *FoundChild,
    PEFI_SECTION_STREAM_NODE *FoundStream,
    UINT32 *AuthenticationStatus
    );

BOOLEAN
EfipFvIsValidSectionStream (
    VOID *SectionStream,
    UINTN SectionStreamLength
    );

EFI_STATUS
EfipFvFindStreamNode (
    UINTN SearchHandle,
    PEFI_SECTION_STREAM_NODE *FoundStream
    );

BOOLEAN
EfipFvChildIsType (
    PEFI_SECTION_STREAM_NODE Stream,
    PEFI_SECTION_CHILD_NODE Child,
    EFI_SECTION_TYPE SearchType,
    EFI_GUID *SectionDefinitionGuid
    );

VOID
EfipFvFreeChildNode (
    PEFI_SECTION_CHILD_NODE ChildNode
    );

//
// -------------------------------------------------------------------- Globals
//

LIST_ENTRY EfiStreamRoot;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiFvInitializeSectionExtraction (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    )

/*++

Routine Description:

    This routine initializes the section extraction support for firmware
    volumes.

Arguments:

    ImageHandle - Supplies a pointer to the image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

{

    INITIALIZE_LIST_HEAD(&EfiStreamRoot);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiFvOpenSectionStream (
    UINTN SectionStreamLength,
    VOID *SectionStream,
    UINTN *SectionStreamHandle
    )

/*++

Routine Description:

    This routine creates and returns a new section stream handle to represent
    a new section stream.

Arguments:

    SectionStreamLength - Supplies the size in bytes of the section stream.

    SectionStream - Supplies the section stream.

    SectionStreamHandle - Supplies a pointer where a handle to the stream will
        be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if memory allocation failed.

    EFI_INVALID_PARAMETER if the section stream does not end noincidentally to
    the end of the previous section.

--*/

{

    EFI_STATUS Status;

    if (EfipFvIsValidSectionStream(SectionStream, SectionStreamLength) ==
        FALSE) {

        return EFI_INVALID_PARAMETER;
    }

    Status = EfipFvOpenSectionStream(SectionStreamLength,
                                     SectionStream,
                                     TRUE,
                                     0,
                                     SectionStreamHandle);

    return Status;
}

EFIAPI
EFI_STATUS
EfiFvCloseSectionStream (
    UINTN StreamHandle
    )

/*++

Routine Description:

    This routine closes an open section stream handle.

Arguments:

    StreamHandle - Supplies the stream handle previously returned.

Return Value:

    EFI status code.

--*/

{

    PEFI_SECTION_CHILD_NODE ChildNode;
    EFI_TPL OldTpl;
    EFI_STATUS Status;
    PEFI_SECTION_STREAM_NODE StreamNode;

    OldTpl = EfiCoreRaiseTpl(TPL_NOTIFY);
    Status = EfipFvFindStreamNode(StreamHandle, &StreamNode);
    if (!EFI_ERROR(Status)) {
        LIST_REMOVE(&(StreamNode->ListEntry));
        while (LIST_EMPTY(&(StreamNode->ChildList)) == FALSE) {
            ChildNode = LIST_VALUE(StreamNode->ChildList.Next,
                                   EFI_SECTION_CHILD_NODE,
                                   ListEntry);

            EfipFvFreeChildNode(ChildNode);
        }

        EfiCoreFreePool(StreamNode->StreamBuffer);
        EfiCoreFreePool(StreamNode);
        Status = EFI_SUCCESS;

    } else {
        Status = EFI_INVALID_PARAMETER;
    }

    EfiCoreRestoreTpl(OldTpl);
    return Status;
}

EFIAPI
EFI_STATUS
EfiFvGetSection (
    UINTN SectionStreamHandle,
    EFI_SECTION_TYPE *SectionType,
    EFI_GUID *SectionDefinitionGuid,
    UINTN SectionInstance,
    VOID **Buffer,
    UINTN *BufferSize,
    UINT32 *AuthenticationStatus,
    BOOLEAN IsFfs3Fv
    )

/*++

Routine Description:

    This routine reads a section from a given section stream.

Arguments:

    SectionStreamHandle - Supplies the stream handle of the stream to get the
        section from.

    SectionType - Supplies a pointer that on input contains the type of section
        to search for. On output, this will return the type of the section
        found.

    SectionDefinitionGuid - Supplies a pointer to the GUID of the section to
        search for if the section type indicates EFI_SECTION_GUID_DEFINED.

    SectionInstance - Supplies the instance of the requested section to
        return.

    Buffer - Supplies a pointer to a buffer value. If the value of the buffer
        is NULL, then the buffer is callee-allocated. If it is not NULL, then
        the supplied buffer is used.

    BufferSize - Supplies a pointer that on input contains the size of the
        buffer, if supplied. On output, the size of the section will be
        returned.

    AuthenticationStatus - Supplies a pointer where the authentication status
        will be returned.

    IsFfs3Fv - Supplies a boolean indicating if the firmware file system is
        version 3 (TRUE) or version 2 (FALSE).

Return Value:

    EFI_SUCCESS on success.

    EFI_PROTOCOL_ERROR if a GUIDed section was encountered but no extraction
    protocol was found.

    EFI_NOT_FOUND if an error occurred while parsing the section stream, or the
    requested section does not exist.

    EFI_OUT_OF_RESOURCES on allocation failure.

    EFI_INVALID_PARAMETER if the given section stream handle does not exist.

    EFI_WARN_TOO_SMALL if a buffer value was supplied but it was not big enough
    to hold the requested section.

--*/

{

    PEFI_SECTION_CHILD_NODE ChildNode;
    PEFI_SECTION_STREAM_NODE ChildStreamNode;
    UINT8 *CopyBuffer;
    UINTN CopySize;
    UINT32 ExtractedAuthenticationStatus;
    UINTN Instance;
    EFI_TPL OldTpl;
    EFI_COMMON_SECTION_HEADER *Section;
    UINTN SectionSize;
    EFI_STATUS Status;
    PEFI_SECTION_STREAM_NODE StreamNode;

    OldTpl = EfiCoreRaiseTpl(TPL_NOTIFY);
    Instance = SectionInstance + 1;
    Status = EfipFvFindStreamNode(SectionStreamHandle, &StreamNode);
    if (EFI_ERROR(Status)) {
        Status = EFI_INVALID_PARAMETER;
        goto FvGetSectionEnd;
    }

    //
    // Locate and return the appropriate section. If the section type is NULL,
    // return the whole stream.
    //

    if (SectionType == NULL) {
        CopySize = StreamNode->StreamLength;
        CopyBuffer = StreamNode->StreamBuffer;
        *AuthenticationStatus = StreamNode->AuthenticationStatus;

    } else {
        Status = EfipFvFindChildNode(StreamNode,
                                     *SectionType,
                                     &Instance,
                                     SectionDefinitionGuid,
                                     &ChildNode,
                                     &ChildStreamNode,
                                     &ExtractedAuthenticationStatus);

        if (EFI_ERROR(Status)) {
            goto FvGetSectionEnd;
        }

        Section = (EFI_COMMON_SECTION_HEADER *)(ChildStreamNode->StreamBuffer +
                                                ChildNode->OffsetInStream);

        if (EFI_IS_SECTION2(Section)) {

            ASSERT(EFI_SECTION2_SIZE(Section) > 0x00FFFFFF);

            if (IsFfs3Fv == FALSE) {
                RtlDebugPrint("Error: FFS3 section in FFS2 volume.\n");
                Status = EFI_NOT_FOUND;
                goto FvGetSectionEnd;
            }

            CopySize = EFI_SECTION2_SIZE(Section) -
                       sizeof(EFI_COMMON_SECTION_HEADER2);

            CopyBuffer = (UINT8 *)Section + sizeof(EFI_COMMON_SECTION_HEADER2);

        } else {
            CopySize = EFI_SECTION_SIZE(Section) -
                       sizeof(EFI_COMMON_SECTION_HEADER);

            CopyBuffer = (UINT8 *)Section + sizeof(EFI_COMMON_SECTION_HEADER);
        }

        *AuthenticationStatus = ExtractedAuthenticationStatus;
    }

    SectionSize = CopySize;
    if (*Buffer != NULL) {
        if (*BufferSize < CopySize) {
            Status = EFI_WARN_BUFFER_TOO_SMALL;
            CopySize = *BufferSize;
        }

    } else {
        *Buffer = EfiCoreAllocateBootPool(CopySize);
        if (*Buffer == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
            goto FvGetSectionEnd;
        }
    }

    EfiCoreCopyMemory(*Buffer, CopyBuffer, CopySize);
    *BufferSize = SectionSize;

FvGetSectionEnd:
    EfiCoreRestoreTpl(OldTpl);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfipFvOpenSectionStream (
    UINTN SectionStreamLength,
    VOID *SectionStream,
    BOOLEAN AllocateBuffer,
    UINT32 AuthenticationStatus,
    UINTN *SectionStreamHandle
    )

/*++

Routine Description:

    This routine creates and returns a new section stream handle to represent
    a new section stream.

Arguments:

    SectionStreamLength - Supplies the size in bytes of the section stream.

    SectionStream - Supplies the section stream.

    AllocateBuffer - Supplies a boolean indicating whether to copy the stream
        buffer (TRUE) or use the buffer in-place (FALSE).

    AuthenticationStatus - Supplies the authentication status.

    SectionStreamHandle - Supplies a pointer where a handle to the stream will
        be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if memory allocation failed.

    EFI_INVALID_PARAMETER if the section stream does not end noincidentally to
    the end of the previous section.

--*/

{

    PEFI_SECTION_STREAM_NODE NewStream;
    EFI_TPL OldTpl;

    NewStream = EfiCoreAllocateBootPool(sizeof(EFI_SECTION_STREAM_NODE));
    if (NewStream == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    EfiCoreSetMemory(NewStream, sizeof(EFI_SECTION_STREAM_NODE), 0);
    if (AllocateBuffer != FALSE) {
        if (SectionStreamLength > 0) {
            NewStream->StreamBuffer =
                                  EfiCoreAllocateBootPool(SectionStreamLength);

            if (NewStream->StreamBuffer == NULL) {
                EfiCoreFreePool(NewStream);
                return EFI_OUT_OF_RESOURCES;
            }

            EfiCoreCopyMemory(NewStream->StreamBuffer,
                              SectionStream,
                              SectionStreamLength);

        }

    //
    // The caller supplied the buffer, use it directly.
    //

    } else {
        NewStream->StreamBuffer = SectionStream;
    }

    NewStream->Magic = EFI_SECTION_STREAM_NODE_MAGIC;
    NewStream->StreamHandle = (UINTN)NewStream;
    NewStream->StreamLength = SectionStreamLength;
    INITIALIZE_LIST_HEAD(&(NewStream->ChildList));
    NewStream->AuthenticationStatus = AuthenticationStatus;

    //
    // Add this shiny new stream to the list.
    //

    OldTpl = EfiCoreRaiseTpl(TPL_NOTIFY);
    INSERT_BEFORE(&(NewStream->ListEntry), &EfiStreamRoot);
    EfiCoreRestoreTpl(OldTpl);
    *SectionStreamHandle = NewStream->StreamHandle;
    return EFI_SUCCESS;
}

EFI_STATUS
EfipFvCreateChildNode (
    PEFI_SECTION_STREAM_NODE Stream,
    UINT32 ChildOffset,
    PEFI_SECTION_CHILD_NODE *ChildNode
    )

/*++

Routine Description:

    This routine parses and creates a new child node.

Arguments:

    Stream - Supplies a pointer to the stream to parse.

    ChildOffset - Supplies the offset within the stream to parse from.

    ChildNode - Supplies a pointer where a pointer to the newly created child
        will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_PROTOCOL_ERROR if the GUIDed extraction protocol needed does not
    exist.

--*/

{

    PEFI_SECTION_CHILD_NODE Node;
    EFI_COMMON_SECTION_HEADER *SectionHeader;
    EFI_STATUS Status;

    SectionHeader = (EFI_COMMON_SECTION_HEADER *)(Stream->StreamBuffer +
                                                  ChildOffset);

    *ChildNode = EfiCoreAllocateBootPool(sizeof(EFI_SECTION_CHILD_NODE));
    if (*ChildNode == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    Node = *ChildNode;
    EfiCoreSetMemory(Node, sizeof(EFI_SECTION_CHILD_NODE), 0);
    Node->Magic = EFI_SECTION_STREAM_CHILD_MAGIC;
    Node->Type = SectionHeader->Elements.Type;
    if (EFI_IS_SECTION2(SectionHeader)) {
        Node->Size = EFI_SECTION2_SIZE(SectionHeader);

    } else {
        Node->Size = EFI_SECTION_SIZE(SectionHeader);
    }

    Node->OffsetInStream = ChildOffset;
    Node->EncapsulatedStreamHandle = NULL_STREAM_HANDLE;
    Node->EncapsulationGuid = NULL;
    switch (Node->Type) {

    //
    // Handle compressed encapsulation sections.
    //

    case EFI_SECTION_COMPRESSION:
        Status = EFI_SUCCESS;
        break;

    //
    // Handle GUIDed encapsulation sections.
    //

    case EFI_SECTION_GUID_DEFINED:
        Status = EFI_SUCCESS;
        break;

    //
    // No processing is needed on leaf nodes.
    //

    default:
        Status = EFI_SUCCESS;
        break;
    }

    INSERT_BEFORE(&(Node->ListEntry), &(Stream->ChildList));
    return Status;
}

EFI_STATUS
EfipFvFindChildNode (
    PEFI_SECTION_STREAM_NODE SourceStream,
    EFI_SECTION_TYPE SearchType,
    UINTN *SectionInstance,
    EFI_GUID *SectionDefinitionGuid,
    PEFI_SECTION_CHILD_NODE *FoundChild,
    PEFI_SECTION_STREAM_NODE *FoundStream,
    UINT32 *AuthenticationStatus
    )

/*++

Routine Description:

    This routine recursively searches for and builds the section stream
    database looking for the requested section.

Arguments:

    SourceStream - Supplies a pointer to the stream to search.

    SearchType - Supplies the type of section to search for.

    SectionInstance - Supplies a pointer to the section instance to find. This
        is an in/out parameter to handle recursion.

    SectionDefinitionGuid - Supplies a pointer where the GUID of the section
        will be returned.

    FoundChild - Supplies a pointer where the child node will be returned on
        success.

    FoundStream - Supplies a pointer where the stream corresponding with the
        child will be returned on success.

    AuthenticationStatus - Supplies a pointer to the authentication status.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the requested child node does not exist.

    EFI_PROTOCOL_ERROR if a required GUIDed section extraction protocol does
    not exist.

--*/

{

    PEFI_SECTION_CHILD_NODE CurrentChildNode;
    VOID *EncapsulatedStream;
    EFI_STATUS ErrorStatus;
    BOOLEAN Match;
    UINT32 NextChildOffset;
    PEFI_SECTION_CHILD_NODE RecursedChildNode;
    PEFI_SECTION_STREAM_NODE RecursedFoundStream;
    EFI_STATUS Status;

    CurrentChildNode = NULL;
    ErrorStatus = EFI_NOT_FOUND;
    if (SourceStream->StreamLength == 0) {
        return EFI_NOT_FOUND;
    }

    //
    // If the stream exists but not child nodes have been parsed out yet, then
    // extract the first child.
    //

    if ((LIST_EMPTY(&(SourceStream->ChildList)) != FALSE) &&
        (SourceStream->StreamLength >= sizeof(EFI_COMMON_SECTION_HEADER))) {

        Status = EfipFvCreateChildNode(SourceStream, 0, &CurrentChildNode);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    //
    // At least one child has been parsed out of the section stream.  So walk
    // through the sections that have already been parsed out looking for the
    // requested section. If necessary, continue parsing section stream and
    // adding children until either the requested section is found, or the
    // stream ends.
    //

    CurrentChildNode = LIST_VALUE(SourceStream->ChildList.Next,
                                  EFI_SECTION_CHILD_NODE,
                                  ListEntry);

    while (TRUE) {

        ASSERT((CurrentChildNode != NULL) &&
               (CurrentChildNode->Magic == EFI_SECTION_STREAM_CHILD_MAGIC));

        Match = EfipFvChildIsType(SourceStream,
                                  CurrentChildNode,
                                  SearchType,
                                  SectionDefinitionGuid);

        if (Match != FALSE) {
            *SectionInstance -= 1;
            if (*SectionInstance == 0) {
                *FoundChild = CurrentChildNode;
                *FoundStream = SourceStream;
                *AuthenticationStatus = SourceStream->AuthenticationStatus;
                return EFI_SUCCESS;
            }
        }

        //
        // If the current node is an encapsulating node, recurse into it.
        //

        if (CurrentChildNode->EncapsulatedStreamHandle != NULL_STREAM_HANDLE) {
            EncapsulatedStream =
                          (VOID *)(CurrentChildNode->EncapsulatedStreamHandle);

            Status = EfipFvFindChildNode(
                                  (PEFI_SECTION_STREAM_NODE)EncapsulatedStream,
                                  SearchType,
                                  SectionInstance,
                                  SectionDefinitionGuid,
                                  &RecursedChildNode,
                                  &RecursedFoundStream,
                                  AuthenticationStatus);

            //
            // If the recursion was not successful, save the error code and
            // continue to find the requested child node in the rest of the
            // stream.
            //

            if (*SectionInstance == 0) {

                ASSERT(!EFI_ERROR(Status));

                *FoundChild = RecursedChildNode;
                *FoundStream = RecursedFoundStream;
                return Status;

            } else {
                ErrorStatus = Status;
            }

        //
        // If the node type is GUIDed, but the node has no encapsulating data,
        // node data should not be parsed because a required GUIDed section
        // extraction protocol does not exist.
        //

        } else if ((CurrentChildNode->Type == EFI_SECTION_GUID_DEFINED) &&
                   (SearchType != EFI_SECTION_GUID_DEFINED)) {

            ErrorStatus = EFI_PROTOCOL_ERROR;
        }

        //
        // If there are more parsed nodes, go look through them.
        //

        if (CurrentChildNode->ListEntry.Next != &(SourceStream->ChildList)) {
            CurrentChildNode = LIST_VALUE(CurrentChildNode->ListEntry.Next,
                                          EFI_SECTION_CHILD_NODE,
                                          ListEntry);

        //
        // This is the end of the list of parsed nodes. See if there's any more
        // data and continue parsing out more children if there is.
        //

        } else {
            NextChildOffset = CurrentChildNode->OffsetInStream +
                              CurrentChildNode->Size;

            NextChildOffset = ALIGN_VALUE(NextChildOffset, 4);
            if (NextChildOffset <=
                (SourceStream->StreamLength -
                 sizeof(EFI_COMMON_SECTION_HEADER))) {

                Status = EfipFvCreateChildNode(SourceStream,
                                               NextChildOffset,
                                               &CurrentChildNode);

                if (EFI_ERROR(Status)) {
                    return Status;
                }

            } else {

                ASSERT(EFI_ERROR(ErrorStatus));

                return ErrorStatus;
            }
        }
    }

    //
    // Execution should never actually get here.
    //

    ASSERT(FALSE);

    return EFI_NOT_FOUND;
}

BOOLEAN
EfipFvIsValidSectionStream (
    VOID *SectionStream,
    UINTN SectionStreamLength
    )

/*++

Routine Description:

    This routine determines whether or not a stream is valid.

Arguments:

    SectionStream - Supplies the section stream to check.

    SectionStreamLength - Supplies the size in bytes of the section stream.

Return Value:

    TRUE if the section stream is valid.

    FALSE if the section stream is invalid.

--*/

{

    EFI_COMMON_SECTION_HEADER *NextSectionHeader;
    EFI_COMMON_SECTION_HEADER *SectionHeader;
    UINTN SectionLength;
    UINTN TotalLength;

    TotalLength = 0;
    SectionHeader = (EFI_COMMON_SECTION_HEADER *)SectionStream;
    while (TotalLength < SectionStreamLength) {
        if (EFI_IS_SECTION2(SectionHeader)) {
            SectionLength = EFI_SECTION2_SIZE(SectionHeader);

        } else {
            SectionLength = EFI_SECTION_SIZE(SectionHeader);
        }

        TotalLength += SectionLength;
        if (TotalLength == SectionStreamLength) {
            return TRUE;
        }

        //
        // Move to the next byte following the section, and figure out where
        // the next section begins.
        //

        SectionHeader = (EFI_COMMON_SECTION_HEADER *)((UINT8 *)SectionHeader +
                                                      SectionLength);

        NextSectionHeader = ALIGN_POINTER(SectionHeader, 4);
        TotalLength += (UINTN)NextSectionHeader - (UINTN)SectionHeader;
        SectionHeader = NextSectionHeader;
    }

    ASSERT(FALSE);

    return FALSE;
}

EFI_STATUS
EfipFvFindStreamNode (
    UINTN SearchHandle,
    PEFI_SECTION_STREAM_NODE *FoundStream
    )

/*++

Routine Description:

    This routine finds the stream matching the given handle. This routine
    assumes the TPL has already been raised.

Arguments:

    SearchHandle - Supplies the handle to search for.

    FoundStream - Supplies a pointer where a pointer to the stream will be
        returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if a stream matching the given handle was not found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_SECTION_STREAM_NODE Node;

    CurrentEntry = EfiStreamRoot.Next;
    while (CurrentEntry != &EfiStreamRoot) {
        Node = LIST_VALUE(CurrentEntry, EFI_SECTION_STREAM_NODE, ListEntry);

        ASSERT(Node->Magic == EFI_SECTION_STREAM_NODE_MAGIC);

        if (Node->StreamHandle == SearchHandle) {
            *FoundStream = Node;
            return EFI_SUCCESS;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    *FoundStream = NULL;
    return EFI_NOT_FOUND;
}

BOOLEAN
EfipFvChildIsType (
    PEFI_SECTION_STREAM_NODE Stream,
    PEFI_SECTION_CHILD_NODE Child,
    EFI_SECTION_TYPE SearchType,
    EFI_GUID *SectionDefinitionGuid
    )

/*++

Routine Description:

    This routine determines if the given input stream and child matches the
    input type.

Arguments:

    Stream - Supplies a pointer to the section stream associated with the child.

    Child - Supplies a pointer to the child to check.

    SearchType - Supplies the type of section to check for.

    SectionDefinitionGuid - Supplies the GUID to check against if the search
        type is EFI_SECTION_GUID_DEFINED.

Return Value:

    TRUE if the section matches the parameters.

    FALSE if the section does not match.

--*/

{

    EFI_GUID_DEFINED_SECTION *GuidedSection;
    EFI_GUID_DEFINED_SECTION2 *GuidedSection2;
    EFI_GUID *SectionGuid;

    if (SearchType == EFI_SECTION_ALL) {
        return TRUE;
    }

    if (SearchType != Child->Type) {
        return FALSE;
    }

    if ((SearchType != EFI_SECTION_GUID_DEFINED) ||
        (SectionDefinitionGuid == NULL)) {

        return TRUE;
    }

    GuidedSection = (EFI_GUID_DEFINED_SECTION *)(Stream->StreamBuffer +
                                                 Child->OffsetInStream);

    if (EFI_IS_SECTION2(GuidedSection)) {
        GuidedSection2 = (EFI_GUID_DEFINED_SECTION2 *)GuidedSection;
        SectionGuid = &(GuidedSection2->SectionDefinitionGuid);

    } else {
        SectionGuid = &(GuidedSection->SectionDefinitionGuid);
    }

    if (EfiCoreCompareGuids(SectionGuid, SectionDefinitionGuid) != FALSE) {
        return TRUE;
    }

    return FALSE;
}

VOID
EfipFvFreeChildNode (
    PEFI_SECTION_CHILD_NODE ChildNode
    )

/*++

Routine Description:

    This routine destroys a firmware volume section child node.

Arguments:

    ChildNode - Supplies the child node to destroy.

Return Value:

    None.

--*/

{

    ASSERT(ChildNode->Magic == EFI_SECTION_STREAM_CHILD_MAGIC);

    LIST_REMOVE(&(ChildNode->ListEntry));
    if (ChildNode->EncapsulatedStreamHandle != NULL_STREAM_HANDLE) {
        EfiFvCloseSectionStream(ChildNode->EncapsulatedStreamHandle);
    }

    if (ChildNode->Event != NULL) {
        EfiCloseEvent(ChildNode->Event);
    }

    ChildNode->Magic = 0;
    EfiCoreFreePool(ChildNode);
    return;
}

