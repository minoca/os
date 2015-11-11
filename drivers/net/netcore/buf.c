/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    buf.c

Abstract:

    This module handles common buffer-related support for the core networking
    library.

Author:

    Evan Green 5-Apr-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/driver.h>
#include "netcore.h"

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
// Store a pointer to the global list of network buffers.
//

LIST_ENTRY NetFreeBufferList;
PQUEUED_LOCK NetBufferListLock;

//
// ------------------------------------------------------------------ Functions
//

NET_API
KSTATUS
NetAllocateBuffer (
    ULONG HeaderSize,
    ULONG Size,
    ULONG FooterSize,
    PNET_LINK Link,
    ULONG Flags,
    PNET_PACKET_BUFFER *NewBuffer
    )

/*++

Routine Description:

    This routine allocates a network buffer.

Arguments:

    HeaderSize - Supplies the number of header bytes needed.

    Size - Supplies the number of data bytes needed.

    FooterSize - Supplies the number of footer bytes needed.

    Link - Supplies a pointer to the link the buffer will be sent through.

    Flags - Supplies a bitmask of allocation flags. See
        NET_ALLOCATOR_BUFFER_FLAG_* for definitions.

    NewBuffer - Supplies a pointer where a pointer to the new allocation will be
        returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if a zero length buffer was requested.

    STATUS_INSUFFICIENT_RESOURCES if the buffer or any auxiliary structures
        could not be allocated.

--*/

{

    ULONG Alignment;
    PNET_PACKET_BUFFER Buffer;
    PHYSICAL_ADDRESS BufferPhysical;
    ULONGLONG BufferSize;
    PLIST_ENTRY CurrentEntry;
    PNET_DATA_LINK_ENTRY DataLinkEntry;
    ULONG DataLinkMask;
    ULONG DataSize;
    ULONG IoBufferFlags;
    BOOL LockHeld;
    PHYSICAL_ADDRESS MaximumPhysicalAddress;
    NET_PACKET_SIZE_INFORMATION SizeInformation;
    KSTATUS Status;
    ULONG TotalSize;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Link != NULL);

    Alignment = Link->Properties.TransmitAlignment;
    if (Alignment == 0) {
        Alignment = 1;
    }

    ASSERT(POWER_OF_2(Alignment));

    //
    // If requested, add the additional headers and footers.
    //

    if ((Flags & NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS) != 0) {
        HeaderSize += Link->Properties.PacketSizeInformation.HeaderSize;
    }

    if ((Flags & NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS) != 0) {
        FooterSize += Link->Properties.PacketSizeInformation.FooterSize;
    }

    DataLinkMask = NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
                   NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

    if ((Flags & DataLinkMask) != 0) {
        DataLinkEntry = Link->DataLinkEntry;
        DataLinkEntry->Interface.GetPacketSizeInformation(Link,
                                                          &SizeInformation);

        if ((Flags & NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS) != 0) {
            HeaderSize += SizeInformation.HeaderSize;
        }

        if ((Flags & NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS) != 0) {
            FooterSize += SizeInformation.FooterSize;
        }
    }

    MaximumPhysicalAddress = Link->Properties.MaxPhysicalAddress;
    DataSize = HeaderSize + Size + FooterSize;
    TotalSize = ALIGN_RANGE_UP(DataSize, Alignment);
    TotalSize = ALIGN_RANGE_UP(TotalSize, 64);

    //
    // Loop through the list looking for the first buffer that fits.
    //

    KeAcquireQueuedLock(NetBufferListLock);
    LockHeld = TRUE;
    CurrentEntry = NetFreeBufferList.Next;
    while (CurrentEntry != &NetFreeBufferList) {
        Buffer = LIST_VALUE(CurrentEntry, NET_PACKET_BUFFER, ListEntry);
        BufferPhysical = Buffer->IoBuffer->Fragment[0].PhysicalAddress;
        BufferSize = Buffer->IoBuffer->Fragment[0].Size;

        //
        // This buffer works if it's big enough, doesn't go beyond the maximum
        // physical address, and meets the alignment requirement.
        //

        if ((BufferSize >= TotalSize) &&
            (BufferPhysical + BufferSize <= MaximumPhysicalAddress) &&
            (ALIGN_RANGE_DOWN(BufferPhysical, Alignment) == BufferPhysical)) {

            LIST_REMOVE(&(Buffer->ListEntry));

            ASSERT(Buffer->BufferPhysicalAddress == BufferPhysical);

            Status = STATUS_SUCCESS;
            goto AllocateBufferEnd;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseQueuedLock(NetBufferListLock);
    LockHeld = FALSE;

    //
    // Allocate a network packet buffer, but do not bother to zero it. This
    // routine takes care to initialize all the necessary fields before it is
    // used.
    //

    Buffer = MmAllocatePagedPool(sizeof(NET_PACKET_BUFFER),
                                 NET_CORE_ALLOCATION_TAG);

    if (Buffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AllocateBufferEnd;
    }

    //
    // A buffer will need to be allocated.
    //

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Buffer->IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                  MaximumPhysicalAddress,
                                                  Alignment,
                                                  TotalSize,
                                                  IoBufferFlags);

    if (Buffer->IoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AllocateBufferEnd;
    }

    ASSERT(Buffer->IoBuffer->FragmentCount == 1);

    Buffer->BufferPhysicalAddress =
                                 Buffer->IoBuffer->Fragment[0].PhysicalAddress;

    Buffer->Buffer = Buffer->IoBuffer->Fragment[0].VirtualAddress;
    Status = STATUS_SUCCESS;

AllocateBufferEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(NetBufferListLock);
    }

    if (!KSUCCESS(Status)) {
        if (Buffer != NULL) {
            if (Buffer->IoBuffer != NULL) {
                MmFreeIoBuffer(Buffer->IoBuffer);
            }

            MmFreePagedPool(Buffer);
            Buffer = NULL;
        }

    } else {
        Buffer->Flags = 0;
        Buffer->BufferSize = TotalSize;
        Buffer->DataSize = DataSize;
        Buffer->DataOffset = HeaderSize;
        Buffer->FooterOffset = Buffer->DataOffset + Size;
    }

    *NewBuffer = Buffer;
    return Status;
}

NET_API
VOID
NetFreeBuffer (
    PNET_PACKET_BUFFER Buffer
    )

/*++

Routine Description:

    This routine frees a previously allocated network buffer.

Arguments:

    Buffer - Supplies a pointer to the buffer returned by the allocation
        routine.

Return Value:

    None.

--*/

{

    KeAcquireQueuedLock(NetBufferListLock);
    INSERT_AFTER(&(Buffer->ListEntry), &NetFreeBufferList);
    KeReleaseQueuedLock(NetBufferListLock);
    return;
}

KSTATUS
NetpInitializeBuffers (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for network buffers.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    INITIALIZE_LIST_HEAD(&NetFreeBufferList);
    NetBufferListLock = KeCreateQueuedLock();
    if (NetBufferListLock == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return STATUS_SUCCESS;
}

VOID
NetpDestroyBuffers (
    VOID
    )

/*++

Routine Description:

    This routine destroys any allocations made during network buffer
    initialization.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (NetBufferListLock != NULL) {
        KeDestroyQueuedLock(NetBufferListLock);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

