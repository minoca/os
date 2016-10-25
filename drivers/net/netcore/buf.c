/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/kernel/driver.h>
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

    Link - Supplies a pointer to the link the buffer will be sent through. If
        a link is provided, then the buffer will be backed by physically
        contiguous pages for the link's hardware. If no link is provided, then
        the buffer will not be backed by physically contiguous pages.

    Flags - Supplies a bitmask of allocation flags. See
        NET_ALLOCATE_BUFFER_FLAG_* for definitions.

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
    ULONG MinPacketSize;
    ULONG PacketSizeFlags;
    ULONG Padding;
    NET_PACKET_SIZE_INFORMATION SizeInformation;
    KSTATUS Status;
    ULONG TotalSize;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (Link != NULL) {

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
            PacketSizeFlags = 0;
            if ((Flags & NET_ALLOCATE_BUFFER_FLAG_UNENCRYPTED) != 0) {
                PacketSizeFlags |= NET_PACKET_SIZE_FLAG_UNENCRYPTED;
            }

            DataLinkEntry = Link->DataLinkEntry;
            DataLinkEntry->Interface.GetPacketSizeInformation(
                                                         Link->DataLinkContext,
                                                         &SizeInformation,
                                                         PacketSizeFlags);

            if ((Flags & NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS) != 0) {
                HeaderSize += SizeInformation.HeaderSize;
            }

            if ((Flags & NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS) != 0) {
                FooterSize += SizeInformation.FooterSize;
            }
        }

        Alignment = Link->Properties.TransmitAlignment;
        if (Alignment == 0) {
            Alignment = 1;
        }

        ASSERT(POWER_OF_2(Alignment));

        MaximumPhysicalAddress = Link->Properties.MaxPhysicalAddress;
        MinPacketSize = Link->Properties.PacketSizeInformation.MinPacketSize;

    } else {
        Alignment = 1;
        MaximumPhysicalAddress = MAX_UINTN;
        MinPacketSize = 0;
    }

    DataSize = HeaderSize + Size + FooterSize;

    //
    // If the total packet size is less than the link's allowed minimum, record
    // the size of the padding so that it can be zero'd later.
    //

    Padding = 0;
    if (DataSize < MinPacketSize) {
        Padding = MinPacketSize - DataSize;
    }

    TotalSize = DataSize + Padding;
    TotalSize = ALIGN_RANGE_UP(TotalSize, Alignment);

    //
    // Loop through the list looking for the first buffer that fits.
    //

    KeAcquireQueuedLock(NetBufferListLock);
    LockHeld = TRUE;
    CurrentEntry = NetFreeBufferList.Next;
    while (CurrentEntry != &NetFreeBufferList) {
        Buffer = LIST_VALUE(CurrentEntry, NET_PACKET_BUFFER, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        BufferSize = Buffer->IoBuffer->Fragment[0].Size;
        if (BufferSize < TotalSize) {
            continue;
        }

        BufferPhysical = Buffer->IoBuffer->Fragment[0].PhysicalAddress;
        if (Link == NULL) {
            if (BufferPhysical != INVALID_PHYSICAL_ADDRESS) {
                continue;
            }

        } else {
            if ((BufferPhysical == INVALID_PHYSICAL_ADDRESS) ||
                ((BufferPhysical + BufferSize) > MaximumPhysicalAddress) ||
                (ALIGN_RANGE_DOWN(BufferPhysical, Alignment) !=
                 BufferPhysical)) {

                continue;
            }
        }

        LIST_REMOVE(&(Buffer->ListEntry));
        Status = STATUS_SUCCESS;
        goto AllocateBufferEnd;
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

    if (Link != NULL) {
        IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
        Buffer->IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                      MaximumPhysicalAddress,
                                                      Alignment,
                                                      TotalSize,
                                                      IoBufferFlags);

    } else {
        Buffer->IoBuffer = MmAllocatePagedIoBuffer(TotalSize, 0);
    }

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
        if ((Flags & NET_ALLOCATE_BUFFER_FLAG_UNENCRYPTED) != 0) {
            Buffer->Flags |= NET_PACKET_FLAG_UNENCRYPTED;
        }

        Buffer->BufferSize = TotalSize;
        Buffer->DataSize = DataSize;
        Buffer->DataOffset = HeaderSize;
        Buffer->FooterOffset = Buffer->DataOffset + Size;

        //
        // If padding was added to the packet, then zero it.
        //

        if (Padding != 0) {
            RtlZeroMemory(Buffer->Buffer + DataSize, Padding);
        }
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

NET_API
VOID
NetDestroyBufferList (
    PNET_PACKET_LIST BufferList
    )

/*++

Routine Description:

    This routine destroys a list of network packet buffers, releasing all of
    its associated resources, not including the buffer list structure.

Arguments:

    BufferList - Supplies a pointer to the buffer list to be destroyed.

Return Value:

    None.

--*/

{

    PNET_PACKET_BUFFER Buffer;

    while (NET_PACKET_LIST_EMPTY(BufferList) == FALSE) {
        Buffer = LIST_VALUE(BufferList->Head.Next,
                            NET_PACKET_BUFFER,
                            ListEntry);

        NET_REMOVE_PACKET_FROM_LIST(Buffer, BufferList);
        NetFreeBuffer(Buffer);
    }

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

