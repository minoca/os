/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    iobuf.c

Abstract:

    This module implements I/O buffer management.

Author:

    Evan Green 6-Jan-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "mmp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Store the number of I/O vectors to place on the stack before needed to
// allocate the array.
//

#define LOCAL_IO_VECTOR_COUNT 8

//
// Store the array size of virtual addresses for mapping IO buffer fragments.
// This should be at least big enough to cover normal read-aheads.
//

#define MM_MAP_IO_BUFFER_LOCAL_VIRTUAL_PAGES 0x20

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
MmpReleaseIoBufferResources (
    PIO_BUFFER IoBuffer
    );

KSTATUS
MmpMapIoBufferFragments (
    PIO_BUFFER IoBuffer,
    UINTN FragmentStart,
    UINTN FragmentCount,
    ULONG MapFlags,
    BOOL VirtuallyContiguous
    );

VOID
MmpUnmapIoBuffer (
    PIO_BUFFER IoBuffer
    );

BOOL
MmpIsIoBufferMapped (
    PIO_BUFFER IoBuffer,
    BOOL VirtuallyContiguous
    );

KSTATUS
MmpExtendIoBuffer (
    PIO_BUFFER IoBuffer,
    PHYSICAL_ADDRESS MinimumPhysicalAddress,
    PHYSICAL_ADDRESS MaximumPhysicalAddress,
    UINTN Alignment,
    UINTN Size,
    BOOL PhysicallyContiguous
    );

KSTATUS
MmpLockIoBuffer (
    PIO_BUFFER *IoBuffer
    );

VOID
MmpSplitIoBufferFragment (
    PIO_BUFFER IoBuffer,
    UINTN FragmentIndex,
    UINTN NewSize
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Remember the size of the I/O buffer alignment.
//

ULONG MmIoBufferAlignment;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
PIO_BUFFER
MmAllocateNonPagedIoBuffer (
    PHYSICAL_ADDRESS MinimumPhysicalAddress,
    PHYSICAL_ADDRESS MaximumPhysicalAddress,
    UINTN Alignment,
    UINTN Size,
    ULONG Flags
    )

/*++

Routine Description:

    This routine allocates memory for use as an I/O buffer. This memory will
    remain mapped in memory until the buffer is destroyed.

Arguments:

    MinimumPhysicalAddress - Supplies the minimum physical address of the
        allocation.

    MaximumPhysicalAddress - Supplies the maximum physical address of the
        allocation.

    Alignment - Supplies the required physical alignment of the buffer, in
        bytes.

    Size - Supplies the minimum size of the buffer, in bytes.

    Flags - Supplies a bitmask of flags used to allocate the I/O buffer. See
        IO_BUFFER_FLAG_* for definitions.

Return Value:

    Returns a pointer to the I/O buffer on success, or NULL on failure.

--*/

{

    UINTN AlignedSize;
    UINTN AllocationSize;
    PVOID CurrentAddress;
    UINTN FragmentCount;
    UINTN FragmentIndex;
    UINTN FragmentSize;
    PIO_BUFFER IoBuffer;
    BOOL NonCached;
    UINTN PageCount;
    UINTN PageIndex;
    ULONG PageShift;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    UINTN PhysicalRunAlignment;
    UINTN PhysicalRunSize;
    KSTATUS Status;
    ULONG UnmapFlags;
    VM_ALLOCATION_PARAMETERS VaRequest;
    BOOL WriteThrough;

    PageShift = MmPageShift();
    PageSize = MmPageSize();
    VaRequest.Address = NULL;

    //
    // Align both the alignment and the size up to a page. Alignment up to a
    // page does not work if the value is 0.
    //

    if (Alignment == 0) {
        Alignment = PageSize;

    } else {
        Alignment = ALIGN_RANGE_UP(Alignment, PageSize);
    }

    AlignedSize = ALIGN_RANGE_UP(Size, Alignment);
    PageCount = AlignedSize >> PageShift;

    //
    // TODO: Implement support for honoring the minimum and maximum physical
    // addresses in I/O buffers.
    //

    ASSERT((MinimumPhysicalAddress == 0) &&
           ((MaximumPhysicalAddress == MAX_ULONG) ||
            (MaximumPhysicalAddress == MAX_ULONGLONG)));

    //
    // If the buffer will be physically contiguous then only one fragment is
    // needed.
    //

    AllocationSize = sizeof(IO_BUFFER);
    if ((Flags & IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS) != 0) {
        FragmentCount = 1;

    } else {
        FragmentCount = PageCount;
    }

    FragmentSize = 0;
    if (FragmentCount > 1) {
        FragmentSize = (FragmentCount * sizeof(IO_BUFFER_FRAGMENT));
        AllocationSize += FragmentSize;
    }

    //
    // Always assume that the I/O buffer might end up cached.
    //

    if (PageCount > 1) {
        AllocationSize += (PageCount * sizeof(PPAGE_CACHE_ENTRY));
    }

    //
    // Allocate an I/O buffer.
    //

    IoBuffer = MmAllocateNonPagedPool(AllocationSize, MM_IO_ALLOCATION_TAG);
    if (IoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AllocateIoBufferEnd;
    }

    RtlZeroMemory(IoBuffer, AllocationSize);
    IoBuffer->Internal.MaxFragmentCount = FragmentCount;
    IoBuffer->Internal.PageCacheEntryCount = PageCount;
    IoBuffer->Internal.TotalSize = AlignedSize;
    if (FragmentCount == 1) {
        IoBuffer->Fragment = &(IoBuffer->Internal.Fragment);

    } else {
        IoBuffer->Fragment = (PVOID)(IoBuffer + 1);
    }

    if (PageCount == 1) {
        IoBuffer->Internal.PageCacheEntries =
                                          &(IoBuffer->Internal.PageCacheEntry);

    } else {
        IoBuffer->Internal.PageCacheEntries = (PVOID)(IoBuffer + 1) +
                                              FragmentSize;
    }

    //
    // Allocate a region of kernel address space.
    //

    VaRequest.Size = AlignedSize;
    VaRequest.Alignment = PageSize;
    VaRequest.Min = 0;
    VaRequest.Max = MAX_ADDRESS;
    VaRequest.MemoryType = MemoryTypeIoBuffer;
    VaRequest.Strategy = AllocationStrategyAnyAddress;
    Status = MmpAllocateAddressRange(&MmKernelVirtualSpace, &VaRequest, FALSE);
    if (!KSUCCESS(Status)) {
        goto AllocateIoBufferEnd;
    }

    //
    // Physically back and map the region based on the alignment and contiguity.
    //

    PhysicalRunAlignment = Alignment;
    if ((Flags & IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS) != 0) {
        PhysicalRunSize = AlignedSize;

    } else {
        PhysicalRunSize = PhysicalRunAlignment;
    }

    NonCached = FALSE;
    if ((Flags & IO_BUFFER_FLAG_MAP_NON_CACHED) != 0) {
        NonCached = TRUE;
    }

    WriteThrough = FALSE;
    if ((Flags & IO_BUFFER_FLAG_MAP_WRITE_THROUGH) != 0) {
       WriteThrough = TRUE;
    }

    Status = MmpMapRange(VaRequest.Address,
                         AlignedSize,
                         PhysicalRunAlignment,
                         PhysicalRunSize,
                         WriteThrough,
                         NonCached);

    if (!KSUCCESS(Status)) {
        goto AllocateIoBufferEnd;
    }

    //
    // Now fill in I/O buffer fragments for this allocation.
    //

    if ((Flags & IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS) != 0) {
        IoBuffer->FragmentCount = 1;
        IoBuffer->Fragment[0].VirtualAddress = VaRequest.Address;
        IoBuffer->Fragment[0].Size = AlignedSize;
        PhysicalAddress = MmpVirtualToPhysical(VaRequest.Address, NULL);

        ASSERT(PhysicalAddress != INVALID_PHYSICAL_ADDRESS);

        IoBuffer->Fragment[0].PhysicalAddress = PhysicalAddress;

    } else {

        ASSERT(IoBuffer->FragmentCount == 0);

        //
        // Iterate over the pages, coalescing physically contiguous regions
        // into the same fragment.
        //

        CurrentAddress = VaRequest.Address;
        FragmentIndex = 0;
        for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
            PhysicalAddress = MmpVirtualToPhysical(CurrentAddress, NULL);

            ASSERT(PhysicalAddress != INVALID_PHYSICAL_ADDRESS);

            //
            // If this buffer is contiguous with the last one, then just up the
            // size of this fragment. Otherwise, add a new fragment.
            //

            if ((IoBuffer->FragmentCount != 0) &&
                ((IoBuffer->Fragment[FragmentIndex - 1].PhysicalAddress +
                  IoBuffer->Fragment[FragmentIndex - 1].Size) ==
                  PhysicalAddress)) {

                IoBuffer->Fragment[FragmentIndex - 1].Size += PageSize;

            } else {
                IoBuffer->Fragment[FragmentIndex].VirtualAddress =
                                                                CurrentAddress;

                IoBuffer->Fragment[FragmentIndex].PhysicalAddress =
                                                               PhysicalAddress;

                IoBuffer->Fragment[FragmentIndex].Size = PageSize;
                IoBuffer->FragmentCount += 1;
                FragmentIndex += 1;
            }

            CurrentAddress += PageSize;
        }

        ASSERT(IoBuffer->FragmentCount <= PageCount);
    }

    IoBuffer->Internal.Flags = IO_BUFFER_INTERNAL_FLAG_NON_PAGED |
                               IO_BUFFER_INTERNAL_FLAG_VA_OWNED |
                               IO_BUFFER_INTERNAL_FLAG_PA_OWNED |
                               IO_BUFFER_INTERNAL_FLAG_MEMORY_LOCKED |
                               IO_BUFFER_INTERNAL_FLAG_MAPPED |
                               IO_BUFFER_INTERNAL_FLAG_VA_CONTIGUOUS;

    ASSERT(KSUCCESS(Status));

AllocateIoBufferEnd:
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("MmAllocateNonPagedIoBuffer(0x%x): %d\n", Size, Status);
        if (VaRequest.Address != NULL) {
            UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                         UNMAP_FLAG_SEND_INVALIDATE_IPI;

            MmpFreeAccountingRange(NULL,
                                   VaRequest.Address,
                                   AlignedSize,
                                   FALSE,
                                   UnmapFlags);
        }

        if (IoBuffer != NULL) {
            MmFreeNonPagedPool(IoBuffer);
            IoBuffer = NULL;
        }
    }

    return IoBuffer;
}

KERNEL_API
PIO_BUFFER
MmAllocatePagedIoBuffer (
    UINTN Size,
    ULONG Flags
    )

/*++

Routine Description:

    This routine allocates memory for use as a pageable I/O buffer.

Arguments:

    Size - Supplies the minimum size of the buffer, in bytes.

    Flags - Supplies a bitmask of flags used to allocate the I/O buffer. See
        IO_BUFFER_FLAG_* for definitions.

Return Value:

    Returns a pointer to the I/O buffer on success, or NULL on failure.

--*/

{

    UINTN AllocationSize;
    PIO_BUFFER IoBuffer;

    AllocationSize = sizeof(IO_BUFFER) + Size;
    IoBuffer = MmAllocatePagedPool(AllocationSize, MM_IO_ALLOCATION_TAG);
    if (IoBuffer == NULL) {
        return NULL;
    }

    RtlZeroMemory(IoBuffer, AllocationSize);
    IoBuffer->Fragment = &(IoBuffer->Internal.Fragment);
    IoBuffer->FragmentCount = 1;
    IoBuffer->Internal.TotalSize = Size;
    IoBuffer->Internal.MaxFragmentCount = 1;
    IoBuffer->Fragment[0].VirtualAddress = (PVOID)(IoBuffer + 1);
    IoBuffer->Fragment[0].Size = Size;
    IoBuffer->Fragment[0].PhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_VA_CONTIGUOUS |
                                IO_BUFFER_INTERNAL_FLAG_MAPPED;

    return IoBuffer;
}

KERNEL_API
PIO_BUFFER
MmAllocateUninitializedIoBuffer (
    UINTN Size,
    ULONG Flags
    )

/*++

Routine Description:

    This routine allocates an uninitialized I/O buffer that the caller will
    fill in with pages. It simply allocates the structures for the given
    size, assuming a buffer fragment may be required for each page.

Arguments:

    Size - Supplies the minimum size of the buffer, in bytes. This size is
        rounded up (always) to a page, but does assume page alignment.

    Flags - Supplies a bitmask of flags used to allocate the I/O buffer. See
        IO_BUFFER_FLAG_* for definitions.

Return Value:

    Returns a pointer to the I/O buffer on success, or NULL on failure.

--*/

{

    ULONG AllocationSize;
    ULONG FragmentSize;
    PIO_BUFFER IoBuffer;
    UINTN PageCount;

    Size = ALIGN_RANGE_UP(Size, MmPageSize());
    PageCount = Size >> MmPageShift();
    FragmentSize = 0;
    AllocationSize = sizeof(IO_BUFFER);
    if (PageCount > 1) {
        FragmentSize = PageCount * sizeof(IO_BUFFER_FRAGMENT);
        AllocationSize += FragmentSize;
        AllocationSize += PageCount * sizeof(PPAGE_CACHE_ENTRY);
    }

    IoBuffer = MmAllocateNonPagedPool(AllocationSize, MM_IO_ALLOCATION_TAG);
    if (IoBuffer == NULL) {
        return NULL;
    }

    RtlZeroMemory(IoBuffer, AllocationSize);
    IoBuffer->Internal.MaxFragmentCount = PageCount;
    IoBuffer->Internal.PageCacheEntryCount = PageCount;
    if (PageCount == 1) {
        IoBuffer->Fragment = &(IoBuffer->Internal.Fragment);
        IoBuffer->Internal.PageCacheEntries =
                                          &(IoBuffer->Internal.PageCacheEntry);

    } else {
        IoBuffer->Fragment = (PVOID)(IoBuffer + 1);
        IoBuffer->Internal.PageCacheEntries = (PVOID)(IoBuffer + 1) +
                                              FragmentSize;
    }

    IoBuffer->Internal.Flags = IO_BUFFER_INTERNAL_FLAG_NON_PAGED |
                               IO_BUFFER_INTERNAL_FLAG_EXTENDABLE;

    if ((Flags & IO_BUFFER_FLAG_MEMORY_LOCKED) != 0) {
        IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_MEMORY_LOCKED;
    }

    return IoBuffer;
}

KERNEL_API
KSTATUS
MmCreateIoBuffer (
    PVOID Buffer,
    UINTN SizeInBytes,
    ULONG Flags,
    PIO_BUFFER *NewIoBuffer
    )

/*++

Routine Description:

    This routine creates an I/O buffer from an existing memory buffer. This
    routine must be called at low level.

Arguments:

    Buffer - Supplies a pointer to the memory buffer on which to base the I/O
        buffer.

    SizeInBytes - Supplies the size of the buffer, in bytes.

    Flags - Supplies a bitmask of flags used to allocate the I/O buffer. See
        IO_BUFFER_FLAG_* for definitions.

    NewIoBuffer - Supplies a pointer where a pointer to the new I/O buffer
        will be returned on success.

Return Value:

    Status code.

--*/

{

    PIO_BUFFER IoBuffer;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    *NewIoBuffer = NULL;
    if ((Flags & IO_BUFFER_FLAG_KERNEL_MODE_DATA) != 0) {

        ASSERT((Buffer >= KERNEL_VA_START) && (Buffer + SizeInBytes >= Buffer));

    } else {

        ASSERT(PsGetCurrentProcess() != PsGetKernelProcess());

        if ((Buffer + SizeInBytes > KERNEL_VA_START) ||
            (Buffer + SizeInBytes < Buffer)) {

            return STATUS_ACCESS_VIOLATION;
        }
    }

    //
    // Build the I/O buffer with one fragment and only fill in the virtual
    // address. If it needs to be pinned later a new I/O buffer structure will
    // need to be created as this one is in paged pool and may not account for
    // all the different physical pages.
    //

    IoBuffer = MmAllocatePagedPool(sizeof(IO_BUFFER), MM_IO_ALLOCATION_TAG);
    if (IoBuffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(IoBuffer, sizeof(IO_BUFFER));
    IoBuffer->Fragment = &(IoBuffer->Internal.Fragment);
    IoBuffer->Internal.TotalSize = SizeInBytes;
    if ((Flags & IO_BUFFER_FLAG_KERNEL_MODE_DATA) == 0) {
        IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_USER_MODE;
    }

    IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_MAPPED |
                                IO_BUFFER_INTERNAL_FLAG_VA_CONTIGUOUS;

    IoBuffer->Internal.MaxFragmentCount = 1;
    IoBuffer->FragmentCount = 1;
    IoBuffer->Fragment[0].VirtualAddress = Buffer;
    IoBuffer->Fragment[0].Size = SizeInBytes;
    *NewIoBuffer = IoBuffer;
    return STATUS_SUCCESS;
}

KSTATUS
MmCreateIoBufferFromVector (
    PIO_VECTOR Vector,
    BOOL VectorInKernelMode,
    UINTN VectorCount,
    PIO_BUFFER *NewIoBuffer
    )

/*++

Routine Description:

    This routine creates a paged usermode I/O buffer based on an I/O vector
    array. This is generally used to support vectored I/O functions in the C
    library.

Arguments:

    Vector - Supplies a pointer to the I/O vector array.

    VectorInKernelMode - Supplies a boolean indicating if the given I/O vector
        array comes directly from kernel mode.

    VectorCount - Supplies the number of elements in the vector array.

    NewIoBuffer - Supplies a pointer where a pointer to the newly created I/O
        buffer will be returned on success. The caller is responsible for
        releasing this buffer.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the vector count is invalid.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_ACCESS_VIOLATION if the given vector array was from user-mode and
    was not valid.

--*/

{

    PVOID Address;
    PIO_VECTOR AllocatedVector;
    UINTN AllocationSize;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    PIO_BUFFER IoBuffer;
    PIO_VECTOR IoVector;
    IO_VECTOR LocalVector[LOCAL_IO_VECTOR_COUNT];
    PIO_BUFFER_FRAGMENT PreviousFragment;
    UINTN Size;
    KSTATUS Status;
    UINTN TotalSize;
    UINTN VectorIndex;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    AllocatedVector = NULL;
    IoBuffer = NULL;
    if ((VectorCount > MAX_IO_VECTOR_COUNT) || (VectorCount == 0)) {
        Status = STATUS_INVALID_PARAMETER;
        goto CreateIoBufferFromVectorEnd;
    }

    IoVector = Vector;
    if (VectorInKernelMode == FALSE) {
        if (VectorCount < LOCAL_IO_VECTOR_COUNT) {
            IoVector = LocalVector;

        } else {
            AllocatedVector = MmAllocatePagedPool(
                                               sizeof(IO_VECTOR) * VectorCount,
                                               MM_IO_ALLOCATION_TAG);

            if (AllocatedVector == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto CreateIoBufferFromVectorEnd;
            }

            IoVector = AllocatedVector;
        }

        Status = MmCopyFromUserMode(IoVector,
                                    Vector,
                                    sizeof(IO_VECTOR) * VectorCount);

        if (!KSUCCESS(Status)) {
            goto CreateIoBufferFromVectorEnd;
        }
    }

    //
    // Create an I/O buffer structure, set up for a paged user-mode buffer with
    // a fragment for each vector.
    //

    AllocationSize = sizeof(IO_BUFFER);
    if (VectorCount > 1) {
        AllocationSize += VectorCount * sizeof(IO_BUFFER_FRAGMENT);
    }

    IoBuffer = MmAllocatePagedPool(AllocationSize, MM_IO_ALLOCATION_TAG);
    if (IoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateIoBufferFromVectorEnd;
    }

    RtlZeroMemory(IoBuffer, AllocationSize);
    IoBuffer->Internal.Flags = IO_BUFFER_INTERNAL_FLAG_USER_MODE |
                               IO_BUFFER_INTERNAL_FLAG_MAPPED;

    IoBuffer->Internal.MaxFragmentCount = VectorCount;
    if (VectorCount == 1) {
        IoBuffer->Fragment = &(IoBuffer->Internal.Fragment);

    } else {
        IoBuffer->Fragment = (PVOID)(IoBuffer + 1);
    }

    //
    // Fill in the fragments.
    //

    TotalSize = 0;
    FragmentIndex = 0;
    PreviousFragment = NULL;
    Fragment = IoBuffer->Fragment;
    for (VectorIndex = 0; VectorIndex < VectorCount; VectorIndex += 1) {
        Address = IoVector[VectorIndex].Data;
        Size = IoVector[VectorIndex].Length;

        //
        // Validate the vector address.
        //

        if ((Address >= KERNEL_VA_START) ||
            (Address + Size > KERNEL_VA_START) ||
            (Address + Size < Address)) {

            Status = STATUS_ACCESS_VIOLATION;
            goto CreateIoBufferFromVectorEnd;
        }

        //
        // Skip empty vectors.
        //

        if (Size == 0) {
            continue;

        //
        // Coalesce adjacent vectors.
        //

        } else if ((PreviousFragment != NULL) &&
                   (PreviousFragment->VirtualAddress + PreviousFragment->Size ==
                    Address)) {

            PreviousFragment->Size += IoVector[VectorIndex].Length;

        //
        // Add this as a new fragment.
        //

        } else {
            Fragment->VirtualAddress = IoVector[VectorIndex].Data;
            Fragment->Size = IoVector[VectorIndex].Length;
            FragmentIndex += 1;
            PreviousFragment = Fragment;
            Fragment += 1;
        }

        TotalSize += IoVector[VectorIndex].Length;
    }

    IoBuffer->Internal.TotalSize = TotalSize;
    IoBuffer->FragmentCount = FragmentIndex;
    IoBuffer->Internal.MaxFragmentCount = FragmentIndex;
    Status = STATUS_SUCCESS;

CreateIoBufferFromVectorEnd:
    if (!KSUCCESS(Status)) {
        if (IoBuffer != NULL) {
            MmFreeIoBuffer(IoBuffer);
            IoBuffer = NULL;
        }
    }

    if (AllocatedVector != NULL) {
        MmFreePagedPool(AllocatedVector);
    }

    *NewIoBuffer = IoBuffer;
    return Status;
}

KSTATUS
MmInitializeIoBuffer (
    PIO_BUFFER IoBuffer,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN SizeInBytes,
    ULONG Flags
    )

/*++

Routine Description:

    This routine initializes an I/O buffer based on the given virtual and
    physical address and the size. If a physical address is supplied, it is
    assumed that the range of bytes is both virtually and physically contiguous
    so that it can be contained in one fragment.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer to initialize.

    VirtualAddress - Supplies the starting virtual address of the I/O buffer.

    PhysicalAddress - Supplies the starting physical address of the I/O buffer.

    SizeInBytes - Supplies the size of the I/O buffer, in bytes.

    Flags - Supplies a bitmask of flags used to initialize the I/O buffer. See
        IO_BUFFER_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    //
    // Assert that this buffer only spans one physical page. Multiple virtual
    // pages are OK.
    //

    ASSERT((PhysicalAddress == INVALID_PHYSICAL_ADDRESS) ||
           (ALIGN_RANGE_UP(PhysicalAddress + SizeInBytes, MmPageSize()) -
            ALIGN_RANGE_DOWN(PhysicalAddress, MmPageSize()) <= MmPageSize()));

    //
    // Initialize the I/O buffer structure to use the internal fragment and
    // page cache entry.
    //

    RtlZeroMemory(IoBuffer, sizeof(IO_BUFFER));
    IoBuffer->Internal.Flags = IO_BUFFER_INTERNAL_FLAG_STRUCTURE_NOT_OWNED |
                               IO_BUFFER_INTERNAL_FLAG_EXTENDABLE;

    IoBuffer->Fragment = &(IoBuffer->Internal.Fragment);
    IoBuffer->Internal.MaxFragmentCount = 1;
    IoBuffer->Internal.PageCacheEntries = &(IoBuffer->Internal.PageCacheEntry);
    IoBuffer->Internal.PageCacheEntryCount = 1;

    //
    // If the caller claims that the memory is locked, there better be a
    // physical address.
    //

    if ((Flags & IO_BUFFER_FLAG_MEMORY_LOCKED) != 0) {

        ASSERT(PhysicalAddress != INVALID_PHYSICAL_ADDRESS);

        IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_MEMORY_LOCKED;
    }

    //
    // Validate the virtual address if it was supplied.
    //

    if (VirtualAddress != NULL) {

        //
        // Validate that the buffer does not cross the user mode boundary.
        //

        if ((Flags & IO_BUFFER_FLAG_KERNEL_MODE_DATA) != 0) {

            ASSERT((VirtualAddress >= KERNEL_VA_START) &&
                   (VirtualAddress + SizeInBytes >= VirtualAddress));

        } else {

            ASSERT(PsGetCurrentProcess() != PsGetKernelProcess());

            if ((VirtualAddress + SizeInBytes > KERNEL_VA_START) ||
                (VirtualAddress + SizeInBytes < VirtualAddress)) {

                return STATUS_ACCESS_VIOLATION;
            }

            IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_USER_MODE;
        }

        IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_MAPPED |
                                    IO_BUFFER_INTERNAL_FLAG_VA_CONTIGUOUS;
    }

    //
    // Fill out the fragment if a virtual or physical address was supplied. A
    // non-zero size is a good indicator of that.
    //

    if (SizeInBytes != 0) {

        ASSERT((PhysicalAddress != INVALID_PHYSICAL_ADDRESS) ||
               (VirtualAddress != NULL));

        IoBuffer->Internal.TotalSize = SizeInBytes;
        IoBuffer->Fragment[0].VirtualAddress = VirtualAddress;
        IoBuffer->Fragment[0].Size = SizeInBytes;
        IoBuffer->Fragment[0].PhysicalAddress = PhysicalAddress;
        IoBuffer->FragmentCount = 1;
    }

    return STATUS_SUCCESS;
}

KERNEL_API
KSTATUS
MmAppendIoBufferData (
    PIO_BUFFER IoBuffer,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN SizeInBytes
    )

/*++

Routine Description:

    This routine appends a fragment to and I/O buffer.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer on which to append.

    VirtualAddress - Supplies the starting virtual address of the data to
        append.

    PhysicalAddress - Supplies the starting physical address of the data to
        append.

    SizeInBytes - Supplies the size of the data to append, in bytes.

Return Value:

    Status code.

--*/

{

    PIO_BUFFER_FRAGMENT Fragment;

    if ((IoBuffer->Internal.Flags & IO_BUFFER_INTERNAL_FLAG_EXTENDABLE) == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // First see if the fragment can be appended onto the end of the previous
    // one.
    //

    if (IoBuffer->FragmentCount != 0) {
        Fragment = &(IoBuffer->Fragment[IoBuffer->FragmentCount - 1]);
        if (Fragment->PhysicalAddress + Fragment->Size == PhysicalAddress) {
            if (((VirtualAddress == NULL) &&
                 (Fragment->VirtualAddress == NULL)) ||
                ((VirtualAddress != NULL) &&
                 (Fragment->VirtualAddress + Fragment->Size ==
                  VirtualAddress))) {

                if (Fragment->Size + SizeInBytes >= Fragment->Size) {
                    Fragment->Size += SizeInBytes;
                    IoBuffer->Internal.TotalSize += SizeInBytes;
                    return STATUS_SUCCESS;
                }
            }
        }
    }

    if (IoBuffer->FragmentCount >= IoBuffer->Internal.MaxFragmentCount) {

        ASSERT(FALSE);

        return STATUS_BUFFER_TOO_SMALL;
    }

    Fragment = &(IoBuffer->Fragment[IoBuffer->FragmentCount]);
    Fragment->VirtualAddress = VirtualAddress;
    Fragment->PhysicalAddress = PhysicalAddress;
    Fragment->Size = SizeInBytes;
    IoBuffer->FragmentCount += 1;
    IoBuffer->Internal.TotalSize += SizeInBytes;
    return STATUS_SUCCESS;
}

KERNEL_API
KSTATUS
MmAppendIoBuffer (
    PIO_BUFFER IoBuffer,
    PIO_BUFFER AppendBuffer,
    UINTN AppendOffset,
    UINTN SizeInBytes
    )

/*++

Routine Description:

    This routine appends one I/O buffer on another.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer on which to append.

    AppendBuffer - Supplies a pointer to the I/O buffer that owns the data to
        append.

    AppendOffset - Supplies the offset into the append buffer where the data to
        append starts.

    SizeInBytes - Supplies the size of the data to append, in bytes.

Return Value:

    Status code.

--*/

{

    PIO_BUFFER_FRAGMENT AppendFragment;
    UINTN AppendFragmentIndex;
    UINTN AppendFragmentOffset;
    UINTN AppendFragmentSize;
    UINTN AppendSize;
    UINTN AvailableFragments;
    UINTN BytesRemaining;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN Index;
    UINTN RequiredFragments;

    AppendOffset += AppendBuffer->Internal.CurrentOffset;
    if ((IoBuffer->Internal.Flags & IO_BUFFER_INTERNAL_FLAG_EXTENDABLE) == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if ((AppendBuffer->FragmentCount == 0) ||
        ((AppendOffset + SizeInBytes) > AppendBuffer->Internal.TotalSize)) {

        return STATUS_INVALID_PARAMETER;
    }

    //
    // Find the first fragment in the append buffer.
    //

    AppendFragment = NULL;
    AppendFragmentOffset = 0;
    AppendFragmentIndex = 0;
    for (Index = 0; Index < AppendBuffer->FragmentCount; Index += 1) {
        AppendFragment = &(AppendBuffer->Fragment[Index]);
        if ((AppendFragmentOffset + AppendFragment->Size) > AppendOffset) {
            AppendFragmentOffset = AppendOffset - AppendFragmentOffset;
            AppendFragmentIndex = Index;
            break;
        }

        AppendFragmentOffset += AppendFragment->Size;
    }

    ASSERT(AppendFragment != NULL);
    ASSERT(Index != AppendBuffer->FragmentCount);

    //
    // Make sure the buffer can fit all of the append data. Assume the worst
    // case that each append fragment will end up in its own fragment in the
    // I/O buffer.
    //

    RequiredFragments = 1;
    AppendFragmentSize = AppendFragment->Size - AppendFragmentOffset;
    Index = AppendFragmentIndex;
    BytesRemaining = SizeInBytes;
    while (BytesRemaining > AppendFragmentSize) {
        BytesRemaining -= AppendFragmentSize;
        Index += 1;
        RequiredFragments += 1;
        AppendFragmentSize = AppendBuffer->Fragment[Index].Size;
    }

    AvailableFragments = IoBuffer->Internal.MaxFragmentCount -
                         IoBuffer->FragmentCount;

    if (RequiredFragments > AvailableFragments) {

        ASSERT(FALSE);

        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Append as much to the I/O buffer's current fragment as possible. Assume
    // the append buffer is already coalesced, so only try to append its first
    // fragment to the I/O buffer's tail fragment.
    //

    BytesRemaining = SizeInBytes;
    if (IoBuffer->FragmentCount != 0) {
        Fragment = &(IoBuffer->Fragment[IoBuffer->FragmentCount - 1]);
        if ((Fragment->PhysicalAddress + Fragment->Size) ==
            (AppendFragment->PhysicalAddress + AppendFragmentOffset)) {

            if (((AppendFragment->VirtualAddress == NULL) &&
                 (Fragment->VirtualAddress == NULL)) ||
                ((AppendFragment->VirtualAddress != NULL) &&
                 ((Fragment->VirtualAddress + Fragment->Size) ==
                  (AppendFragment->VirtualAddress + AppendFragmentOffset)))) {

                AppendSize = AppendFragment->Size - AppendFragmentOffset;
                if (AppendSize > BytesRemaining) {
                    AppendSize = BytesRemaining;
                }

                if ((Fragment->Size + AppendSize) >= Fragment->Size) {
                    Fragment->Size += AppendSize;
                    BytesRemaining -= AppendSize;
                    AppendFragmentIndex += 1;
                    AppendFragmentOffset = 0;
                }
            }
        }
    }

    //
    // Add new fragments until the requested append size runs out.
    //

    while (BytesRemaining != 0) {

        ASSERT(IoBuffer->FragmentCount < IoBuffer->Internal.MaxFragmentCount);

        Fragment = &(IoBuffer->Fragment[IoBuffer->FragmentCount]);
        AppendFragment = &(AppendBuffer->Fragment[AppendFragmentIndex]);
        Fragment->VirtualAddress = AppendFragment->VirtualAddress +
                                   AppendFragmentOffset;

        Fragment->PhysicalAddress = AppendFragment->PhysicalAddress +
                                    AppendFragmentOffset;

        AppendSize = AppendFragment->Size - AppendFragmentOffset;
        if (AppendSize > BytesRemaining) {
            AppendSize = BytesRemaining;
        }

        Fragment->Size = AppendSize;
        BytesRemaining -= AppendSize;
        AppendFragmentOffset = 0;
        AppendFragmentIndex += 1;
        IoBuffer->FragmentCount += 1;
    }

    IoBuffer->Internal.TotalSize += SizeInBytes;
    return STATUS_SUCCESS;
}

KERNEL_API
VOID
MmFreeIoBuffer (
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine destroys an I/O buffer. If the memory was allocated when the
    I/O buffer was created, then the memory will be released at this time as
    well.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer to release.

Return Value:

    None.

--*/

{

    ULONG Flags;

    Flags = IoBuffer->Internal.Flags;
    MmpReleaseIoBufferResources(IoBuffer);
    if ((Flags & IO_BUFFER_INTERNAL_FLAG_STRUCTURE_NOT_OWNED) == 0) {
        if ((Flags & IO_BUFFER_INTERNAL_FLAG_NON_PAGED) != 0) {
            MmFreeNonPagedPool(IoBuffer);

        } else {
            MmFreePagedPool(IoBuffer);
        }
    }

    return;
}

VOID
MmResetIoBuffer (
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine resets an I/O buffer for re-use, unmapping any memory and
    releasing any associated page cache entries.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

Return Value:

    Status code.

--*/

{

    //
    // Support user mode I/O buffers if this fires and it seems useful to add.
    //

    ASSERT((IoBuffer->Internal.Flags & IO_BUFFER_INTERNAL_FLAG_USER_MODE) == 0);

    //
    // Release all the resources associated with the I/O buffer, but do not
    // free the buffer structure itself.
    //

    MmpReleaseIoBufferResources(IoBuffer);

    //
    // Now zero and reset the I/O buffer.
    //

    ASSERT(IoBuffer->Fragment != NULL);

    RtlZeroMemory(IoBuffer->Fragment,
                  IoBuffer->FragmentCount * sizeof(IO_BUFFER_FRAGMENT));

    IoBuffer->FragmentCount = 0;
    IoBuffer->Internal.TotalSize = 0;
    IoBuffer->Internal.CurrentOffset = 0;
    IoBuffer->Internal.Flags &= ~(IO_BUFFER_INTERNAL_FLAG_VA_OWNED |
                                  IO_BUFFER_INTERNAL_FLAG_MAPPED |
                                  IO_BUFFER_INTERNAL_FLAG_VA_CONTIGUOUS);

    IoBuffer->Internal.MapFlags = 0;
    if (IoBuffer->Internal.PageCacheEntries != NULL) {
        RtlZeroMemory(IoBuffer->Internal.PageCacheEntries,
                      IoBuffer->Internal.PageCacheEntryCount * sizeof(PVOID));
    }

    return;
}

KERNEL_API
KSTATUS
MmMapIoBuffer (
    PIO_BUFFER IoBuffer,
    BOOL WriteThrough,
    BOOL NonCached,
    BOOL VirtuallyContiguous
    )

/*++

Routine Description:

    This routine maps the given I/O buffer into memory. If the caller requests
    that the I/O buffer be mapped virtually contiguous, then all fragments will
    be updated with the virtually contiguous mappings. If the I/O buffer does
    not need to be virtually contiguous, then this routine just ensure that
    each fragment is mapped.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    WriteThrough - Supplies a boolean indicating if the virtual addresses
        should be mapped write through (TRUE) or the default write back (FALSE).

    NonCached - Supplies a boolean indicating if the virtual addresses should
        be mapped non-cached (TRUE) or the default, which is to map is as
        normal cached memory (FALSE).

    VirtuallyContiguous - Supplies a boolean indicating whether or not the
        caller needs the I/O buffer to be mapped virtually contiguous (TRUE) or
        not (FALSE). In the latter case, each I/O buffer fragment will at least
        be virtually contiguous.

Return Value:

    Status code.

--*/

{

    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentCount;
    UINTN FragmentIndex;
    ULONG IoBufferFlags;
    ULONG MapFlags;
    UINTN MapFragmentStart;
    BOOL MapRequired;
    KSTATUS Status;

    if (IoBuffer->FragmentCount == 0) {
        return STATUS_SUCCESS;
    }

    //
    // Check to see if the I/O buffer is already virtually contiguous. Note
    // that the flag might not be set if the I/O buffer is backed by the page
    // cache and a virtually contiguous mapping request has not yet been made.
    //

    IoBufferFlags = IoBuffer->Internal.Flags;
    if (VirtuallyContiguous != FALSE) {
        if ((IoBufferFlags & IO_BUFFER_INTERNAL_FLAG_VA_CONTIGUOUS) != 0) {

            ASSERT(MmpIsIoBufferMapped(IoBuffer, TRUE) != FALSE);

            return STATUS_SUCCESS;
        }

        if (MmpIsIoBufferMapped(IoBuffer, TRUE) != FALSE) {
            IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_VA_CONTIGUOUS;
            return STATUS_SUCCESS;
        }

    //
    // Otherwise, if the I/O buffer is mapped, then it is good enough.
    //

    } else {
        if ((IoBufferFlags & IO_BUFFER_INTERNAL_FLAG_MAPPED) != 0) {

            ASSERT(MmpIsIoBufferMapped(IoBuffer, FALSE) != FALSE);

            return STATUS_SUCCESS;
        }

        if (MmpIsIoBufferMapped(IoBuffer, FALSE) != FALSE) {
            IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_MAPPED;
            return STATUS_SUCCESS;
        }
    }

    //
    // User mode buffers should always be mapped virtually contiguous.
    //

    ASSERT((IoBuffer->Internal.Flags & IO_BUFFER_INTERNAL_FLAG_USER_MODE) == 0);

    //
    // Collect the map flags. This routine should never allocate user mode
    // virtual addresses.
    //

    MapFlags = MAP_FLAG_PRESENT | MAP_FLAG_GLOBAL;
    if (WriteThrough != FALSE) {
        MapFlags |= MAP_FLAG_WRITE_THROUGH;
    }

    if (NonCached != FALSE) {
        MapFlags |= MAP_FLAG_CACHE_DISABLE;
    }

    //
    // If a virtually contiguous mapping was requested, unmap any existing
    // ranges and then allocate an address range to cover the whole buffer.
    //

    if (VirtuallyContiguous != FALSE) {
        if ((IoBuffer->Internal.Flags & IO_BUFFER_INTERNAL_FLAG_MAPPED) != 0) {
            MmpUnmapIoBuffer(IoBuffer);
        }

        Status = MmpMapIoBufferFragments(IoBuffer,
                                         0,
                                         IoBuffer->FragmentCount,
                                         MapFlags,
                                         TRUE);

        if (!KSUCCESS(Status)) {
            goto MapIoBufferEnd;
        }

        IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_VA_CONTIGUOUS;

    //
    // Otherwise run through the fragments and map any portions of the I/O
    // buffer that are not currently mapped.
    //

    } else {
        MapRequired = FALSE;
        MapFragmentStart = 0;
        Status = STATUS_SUCCESS;
        for (FragmentIndex = 0;
             FragmentIndex < IoBuffer->FragmentCount;
             FragmentIndex += 1) {

            Fragment = &(IoBuffer->Fragment[FragmentIndex]);

            //
            // If this fragment is already mapped, then map the unmapped set of
            // fragments before it, if necessary.
            //

            if (Fragment->VirtualAddress != NULL) {
                if (MapRequired == FALSE) {
                    continue;
                }

                FragmentCount = FragmentIndex - MapFragmentStart;
                Status = MmpMapIoBufferFragments(IoBuffer,
                                                 MapFragmentStart,
                                                 FragmentCount,
                                                 MapFlags,
                                                 FALSE);

                if (!KSUCCESS(Status)) {
                    MapRequired = FALSE;
                    break;
                }

                //
                // Reset to search for the next run of unmapped fragments.
                //

                MapRequired = FALSE;
                continue;
            }

            //
            // If this is the first unmapped fragment found, then store its
            // index.
            //

            if (MapRequired == FALSE) {
                MapFragmentStart = FragmentIndex;
                MapRequired = TRUE;
            }
        }

        //
        // If the last set of fragments was unmapped, map it here.
        //

        if (MapRequired != FALSE) {
            FragmentCount = FragmentIndex - MapFragmentStart;
            Status = MmpMapIoBufferFragments(IoBuffer,
                                             MapFragmentStart,
                                             FragmentCount,
                                             MapFlags,
                                             FALSE);
        }
    }

    IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_VA_OWNED |
                                IO_BUFFER_INTERNAL_FLAG_MAPPED;

    Status = STATUS_SUCCESS;

MapIoBufferEnd:
    return Status;
}

KERNEL_API
KSTATUS
MmCopyIoBuffer (
    PIO_BUFFER Destination,
    UINTN DestinationOffset,
    PIO_BUFFER Source,
    UINTN SourceOffset,
    UINTN ByteCount
    )

/*++

Routine Description:

    This routine copies the contents of the source I/O buffer starting at the
    source offset to the destination I/O buffer starting at the destination
    offset. It assumes that the arguments are correct such that the copy can
    succeed.

Arguments:

    Destination - Supplies a pointer to the destination I/O buffer that is to
        be copied into.

    DestinationOffset - Supplies the offset into the destination I/O buffer
        where the copy should begin.

    Source - Supplies a pointer to the source I/O buffer whose contents will be
        copied to the destination.

    SourceOffset - Supplies the offset into the source I/O buffer where the
        copy should begin.

    ByteCount - Supplies the size of the requested copy in bytes.

Return Value:

    Status code.

--*/

{

    UINTN BytesThisRound;
    ULONG DestinationFlags;
    PIO_BUFFER_FRAGMENT DestinationFragment;
    UINTN DestinationFragmentOffset;
    PVOID DestinationVirtualAddress;
    UINTN ExtensionSize;
    UINTN FragmentIndex;
    UINTN MaxDestinationSize;
    UINTN MaxSourceSize;
    ULONG SourceFlags;
    PIO_BUFFER_FRAGMENT SourceFragment;
    UINTN SourceFragmentOffset;
    PVOID SourceVirtualAddress;
    KSTATUS Status;

    //
    // If the byte count is zero, there is no work to do.
    //

    if (ByteCount == 0) {
        return STATUS_SUCCESS;
    }

    DestinationOffset += Destination->Internal.CurrentOffset;
    DestinationFlags = Destination->Internal.Flags;
    SourceOffset += Source->Internal.CurrentOffset;
    SourceFlags = Source->Internal.Flags;

    //
    // The source should always have enough data for the copy.
    //

    ASSERT((SourceOffset + ByteCount) <= Source->Internal.TotalSize);

    //
    // If memory can be appended to the destination and it needs to be, then
    // extend the I/O buffer.
    //

    ASSERT(((DestinationFlags & IO_BUFFER_INTERNAL_FLAG_EXTENDABLE) != 0) ||
           ((DestinationOffset + ByteCount) <=
            Destination->Internal.TotalSize));

    if (((DestinationFlags & IO_BUFFER_INTERNAL_FLAG_EXTENDABLE) != 0) &&
        ((DestinationOffset + ByteCount) > Destination->Internal.TotalSize)) {

        ExtensionSize = (DestinationOffset + ByteCount) -
                        Destination->Internal.TotalSize;

        Status = MmpExtendIoBuffer(Destination,
                                   0,
                                   MAX_ULONGLONG,
                                   0,
                                   ExtensionSize,
                                   FALSE);

        if (!KSUCCESS(Status)) {
            goto CopyIoBufferEnd;
        }
    }

    //
    // Both I/O buffers had better not be user mode buffers.
    //

    ASSERT(((DestinationFlags & IO_BUFFER_INTERNAL_FLAG_USER_MODE) == 0) ||
           ((SourceFlags & IO_BUFFER_INTERNAL_FLAG_USER_MODE) == 0));

    //
    // Make sure both buffers are mapped.
    //

    Status = MmMapIoBuffer(Destination, FALSE, FALSE, FALSE);
    if (!KSUCCESS(Status)) {
        goto CopyIoBufferEnd;
    }

    Status = MmMapIoBuffer(Source, FALSE, FALSE, FALSE);
    if (!KSUCCESS(Status)) {
        goto CopyIoBufferEnd;
    }

    //
    // Do not assume that the fragments are virtually contiguous. Get the
    // starting fragment for both buffers.
    //

    DestinationFragment = NULL;
    DestinationFragmentOffset = 0;
    for (FragmentIndex = 0;
         FragmentIndex < Destination->FragmentCount;
         FragmentIndex += 1) {

        DestinationFragment = &(Destination->Fragment[FragmentIndex]);
        if ((DestinationFragmentOffset + DestinationFragment->Size) >
            DestinationOffset) {

            DestinationFragmentOffset = DestinationOffset -
                                        DestinationFragmentOffset;

            break;
        }

        DestinationFragmentOffset += DestinationFragment->Size;
    }

    ASSERT(DestinationFragment != NULL);
    ASSERT(FragmentIndex != Destination->FragmentCount);

    SourceFragment = NULL;
    SourceFragmentOffset = 0;
    for (FragmentIndex = 0;
         FragmentIndex < Source->FragmentCount;
         FragmentIndex += 1) {

        SourceFragment = &(Source->Fragment[FragmentIndex]);
        if ((SourceFragmentOffset + SourceFragment->Size) > SourceOffset) {
            SourceFragmentOffset = SourceOffset - SourceFragmentOffset;
            break;
        }

        SourceFragmentOffset += SourceFragment->Size;
    }

    ASSERT(SourceFragment != NULL);
    ASSERT(FragmentIndex != Source->FragmentCount);

    //
    // Now execute the copy fragment by fragment.
    //

    MaxDestinationSize = DestinationFragment->Size - DestinationFragmentOffset;
    MaxSourceSize = SourceFragment->Size - SourceFragmentOffset;
    while (ByteCount != 0) {
        if (MaxDestinationSize < MaxSourceSize) {
            BytesThisRound = MaxDestinationSize;

        } else {
            BytesThisRound = MaxSourceSize;
        }

        if (BytesThisRound > ByteCount) {
            BytesThisRound = ByteCount;
        }

        ASSERT(DestinationFragment->VirtualAddress != NULL);
        ASSERT(SourceFragment->VirtualAddress != NULL);

        DestinationVirtualAddress = DestinationFragment->VirtualAddress +
                                    DestinationFragmentOffset;

        SourceVirtualAddress = SourceFragment->VirtualAddress +
                               SourceFragmentOffset;

        if ((DestinationFlags & IO_BUFFER_INTERNAL_FLAG_USER_MODE) != 0) {
            Status = MmCopyToUserMode(DestinationVirtualAddress,
                                      SourceVirtualAddress,
                                      BytesThisRound);

        } else if ((SourceFlags & IO_BUFFER_INTERNAL_FLAG_USER_MODE) != 0) {
            Status = MmCopyFromUserMode(DestinationVirtualAddress,
                                        SourceVirtualAddress,
                                        BytesThisRound);

        } else {
            RtlCopyMemory(DestinationVirtualAddress,
                          SourceVirtualAddress,
                          BytesThisRound);

            Status = STATUS_SUCCESS;
        }

        if (!KSUCCESS(Status)) {
            goto CopyIoBufferEnd;
        }

        DestinationFragmentOffset += BytesThisRound;
        MaxDestinationSize -= BytesThisRound;
        if (MaxDestinationSize == 0) {

            ASSERT(DestinationFragmentOffset == DestinationFragment->Size);

            DestinationFragment += 1;
            DestinationFragmentOffset = 0;
            MaxDestinationSize = DestinationFragment->Size;
        }

        SourceFragmentOffset += BytesThisRound;
        MaxSourceSize -= BytesThisRound;
        if (MaxSourceSize == 0) {

            ASSERT(SourceFragmentOffset == SourceFragment->Size);

            SourceFragment += 1;
            SourceFragmentOffset = 0;
            MaxSourceSize = SourceFragment->Size;
        }

        ByteCount -= BytesThisRound;
    }

CopyIoBufferEnd:
    return Status;
}

KERNEL_API
KSTATUS
MmZeroIoBuffer (
    PIO_BUFFER IoBuffer,
    UINTN Offset,
    UINTN ByteCount
    )

/*++

Routine Description:

    This routine zeroes the contents of the I/O buffer starting at the offset
    for the given number of bytes.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer that is to be zeroed.

    Offset - Supplies the offset into the I/O buffer where the zeroing
        should begin.

    ByteCount - Supplies the number of bytes to zero.

Return Value:

    Status code.

--*/

{

    UINTN CurrentOffset;
    UINTN ExtensionSize;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    ULONG InternalFlags;
    KSTATUS Status;
    UINTN ZeroSize;

    Offset += IoBuffer->Internal.CurrentOffset;
    InternalFlags = IoBuffer->Internal.Flags;

    //
    // If memory can be appended to the buffer and it needs to be, then extend
    // the I/O buffer.
    //

    ASSERT(((InternalFlags & IO_BUFFER_INTERNAL_FLAG_EXTENDABLE) != 0) ||
           ((Offset + ByteCount) <= IoBuffer->Internal.TotalSize));

    if (((InternalFlags & IO_BUFFER_INTERNAL_FLAG_EXTENDABLE) != 0) &&
        ((Offset + ByteCount) > IoBuffer->Internal.TotalSize)) {

        ExtensionSize = (Offset + ByteCount) - IoBuffer->Internal.TotalSize;
        Status = MmpExtendIoBuffer(IoBuffer,
                                   0,
                                   MAX_ULONGLONG,
                                   0,
                                   ExtensionSize,
                                   FALSE);

        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    //
    // Make sure the buffer is mapped.
    //

    Status = MmMapIoBuffer(IoBuffer, FALSE, FALSE, FALSE);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    FragmentIndex = 0;
    CurrentOffset = 0;
    while (ByteCount != 0) {
        if (FragmentIndex >= IoBuffer->FragmentCount) {
            return STATUS_INCORRECT_BUFFER_SIZE;
        }

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);
        FragmentIndex += 1;
        if ((CurrentOffset + Fragment->Size) <= Offset) {
            CurrentOffset += Fragment->Size;
            continue;
        }

        ZeroSize = Fragment->Size;
        FragmentOffset = 0;
        if (Offset > CurrentOffset) {
            FragmentOffset = Offset - CurrentOffset;
            ZeroSize -= FragmentOffset;
        }

        if (ZeroSize > ByteCount) {
            ZeroSize = ByteCount;
        }

        if ((InternalFlags & IO_BUFFER_INTERNAL_FLAG_USER_MODE) != 0) {
            Status = MmpZeroUserModeMemory(
                                     Fragment->VirtualAddress + FragmentOffset,
                                     ZeroSize);

            if (Status == FALSE) {
                return STATUS_ACCESS_VIOLATION;
            }

        } else {
            RtlZeroMemory(Fragment->VirtualAddress + FragmentOffset, ZeroSize);
        }

        ByteCount -= ZeroSize;
        CurrentOffset += Fragment->Size;
    }

    return STATUS_SUCCESS;
}

KERNEL_API
KSTATUS
MmCopyIoBufferData (
    PIO_BUFFER IoBuffer,
    PVOID Buffer,
    UINTN Offset,
    UINTN Size,
    BOOL ToIoBuffer
    )

/*++

Routine Description:

    This routine copies from a buffer into the given I/O buffer or out of the
    given I/O buffer.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer to copy in or out of.

    Buffer - Supplies a pointer to the regular linear buffer to copy to or from.
        This must be a kernel mode address.

    Offset - Supplies an offset in bytes from the beginning of the I/O buffer
        to copy to or from.

    Size - Supplies the number of bytes to copy.

    ToIoBuffer - Supplies a boolean indicating whether data is copied into the
        I/O buffer (TRUE) or out of the I/O buffer (FALSE).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INCORRECT_BUFFER_SIZE if the copy goes outside the I/O buffer.

    Other error codes if the I/O buffer could not be mapped.

--*/

{

    UINTN CopyOffset;
    UINTN CopySize;
    UINTN CurrentOffset;
    UINTN ExtensionSize;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    ULONG InternalFlags;
    KSTATUS Status;

    ASSERT(Buffer >= KERNEL_VA_START);

    Offset += IoBuffer->Internal.CurrentOffset;
    InternalFlags = IoBuffer->Internal.Flags;

    //
    // If memory can be appended to the buffer and it needs to be, then extend
    // the I/O buffer.
    //

    ASSERT((ToIoBuffer != FALSE) ||
           ((Offset + Size) <= IoBuffer->Internal.TotalSize));

    ASSERT((ToIoBuffer == FALSE) ||
           ((InternalFlags & IO_BUFFER_INTERNAL_FLAG_EXTENDABLE) != 0) ||
           ((Offset + Size) <= IoBuffer->Internal.TotalSize));

    if ((ToIoBuffer != FALSE) &&
        ((InternalFlags & IO_BUFFER_INTERNAL_FLAG_EXTENDABLE) != 0) &&
        ((Offset + Size) > IoBuffer->Internal.TotalSize)) {

        ExtensionSize = (Offset + Size) - IoBuffer->Internal.TotalSize;
        Status = MmpExtendIoBuffer(IoBuffer,
                                   0,
                                   MAX_ULONGLONG,
                                   0,
                                   ExtensionSize,
                                   FALSE);

        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    Status = MmMapIoBuffer(IoBuffer, FALSE, FALSE, FALSE);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    FragmentIndex = 0;
    CurrentOffset = 0;
    while (Size != 0) {
        if (FragmentIndex >= IoBuffer->FragmentCount) {
            return STATUS_INCORRECT_BUFFER_SIZE;
        }

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);
        FragmentIndex += 1;
        if ((CurrentOffset + Fragment->Size) <= Offset) {
            CurrentOffset += Fragment->Size;
            continue;
        }

        CopySize = Fragment->Size;
        CopyOffset = 0;
        if (Offset > CurrentOffset) {
            CopyOffset = Offset - CurrentOffset;
            CopySize -= CopyOffset;
        }

        if (CopySize > Size) {
            CopySize = Size;
        }

        //
        // Copy into the I/O buffer fragment, potentially to user mode.
        //

        if (ToIoBuffer != FALSE) {
            if ((InternalFlags & IO_BUFFER_INTERNAL_FLAG_USER_MODE) != 0) {
                Status = MmCopyToUserMode(Fragment->VirtualAddress + CopyOffset,
                                          Buffer,
                                          CopySize);

            } else {
                RtlCopyMemory(Fragment->VirtualAddress + CopyOffset,
                              Buffer,
                              CopySize);
            }

        //
        // Copy out of the I/O buffer fragment, potentially from user mode.
        //

        } else {
            if ((InternalFlags & IO_BUFFER_INTERNAL_FLAG_USER_MODE) != 0) {
                Status = MmCopyFromUserMode(
                                         Buffer,
                                         Fragment->VirtualAddress + CopyOffset,
                                         CopySize);

            } else {
                RtlCopyMemory(Buffer,
                              Fragment->VirtualAddress + CopyOffset,
                              CopySize);
            }
        }

        if (!KSUCCESS(Status)) {
            return Status;
        }

        Size -= CopySize;
        Buffer += CopySize;
        CurrentOffset += Fragment->Size;
    }

    return STATUS_SUCCESS;
}

KERNEL_API
ULONG
MmGetIoBufferAlignment (
    VOID
    )

/*++

Routine Description:

    This routine returns the required alignment for all flush operations.

Arguments:

    None.

Return Value:

    Returns the size of a data cache line, in bytes.

--*/

{

    ULONG IoBufferAlignment;
    ULONG L1DataCacheLineSize;

    IoBufferAlignment = MmIoBufferAlignment;
    if (IoBufferAlignment == 0) {

        //
        // Take the maximum between the L1 cache and any registered cache
        // controllers.
        //

        L1DataCacheLineSize = MmDataCacheLineSize;
        IoBufferAlignment = HlGetDataCacheLineSize();
        if (L1DataCacheLineSize > IoBufferAlignment) {
            IoBufferAlignment = L1DataCacheLineSize;
        }

        MmIoBufferAlignment = IoBufferAlignment;
    }

    return IoBufferAlignment;
}

KSTATUS
MmValidateIoBuffer (
    PHYSICAL_ADDRESS MinimumPhysicalAddress,
    PHYSICAL_ADDRESS MaximumPhysicalAddress,
    UINTN Alignment,
    UINTN SizeInBytes,
    BOOL PhysicallyContiguous,
    PIO_BUFFER *IoBuffer,
    PBOOL LockedCopy
    )

/*++

Routine Description:

    This routine validates an I/O buffer for use by a device. If the I/O buffer
    does not meet the given requirements, then a new I/O buffer that meets the
    requirements will be returned. This new I/O buffer will not contain the
    same data as the originally supplied I/O buffer. It is up to the caller to
    decide which further actions need to be taken if a different buffer is
    returned. The exception is if the locked parameter is returned as true. In
    that case a new I/O buffer was created, but is backed by the same physical
    pages, now locked in memory.

Arguments:

    MinimumPhysicalAddress - Supplies the minimum allowed physical address for
        the I/O buffer.

    MaximumPhysicalAddress - Supplies the maximum allowed physical address for
        the I/O buffer.

    Alignment - Supplies the required physical alignment of the I/O buffer.

    SizeInBytes - Supplies the minimum required size of the buffer, in bytes.

    PhysicallyContiguous - Supplies a boolean indicating whether or not the
        I/O buffer should be physically contiguous.

    IoBuffer - Supplies a pointer to a pointer to an I/O buffer. On entry, this
        contains a pointer to the I/O buffer to be validated. On exit, it may
        point to a newly allocated I/O buffer that the caller must free.

    LockedCopy - Supplies a pointer to a boolean that receives whether or not
        the validated I/O buffer is a locked copy of the original.

Return Value:

    Status code.

--*/

{

    BOOL AllocateIoBuffer;
    UINTN BufferOffset;
    UINTN CurrentOffset;
    UINTN EndOffset;
    UINTN ExtensionSize;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    UINTN FragmentSize;
    ULONG IoBufferFlags;
    PIO_BUFFER LockedBuffer;
    ULONG LockedFlags;
    PIO_BUFFER NewBuffer;
    PIO_BUFFER OriginalBuffer;
    ULONG OriginalFlags;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddressEnd;
    PHYSICAL_ADDRESS PhysicalAddressStart;
    KSTATUS Status;
    PVOID VirtualAddress;

    *LockedCopy = FALSE;
    OriginalBuffer = *IoBuffer;
    if (OriginalBuffer == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    AllocateIoBuffer = FALSE;
    LockedBuffer = OriginalBuffer;
    NewBuffer = NULL;
    Status = STATUS_SUCCESS;
    if (Alignment == 0) {
        Alignment = 1;
    }

    //
    // If the I/O buffer won't be able to fit the data and it is not
    // extendable, then do not re-allocate a different buffer, just fail.
    //

    OriginalFlags = OriginalBuffer->Internal.Flags;
    if (((OriginalFlags & IO_BUFFER_INTERNAL_FLAG_EXTENDABLE) == 0) &&
        ((OriginalBuffer->Internal.CurrentOffset + SizeInBytes) >
          OriginalBuffer->Internal.TotalSize)) {

        Status = STATUS_BUFFER_TOO_SMALL;
        goto ValidateIoBufferEnd;
    }

    //
    // Do a quick virtual alignment check to avoid locking down a bunch of
    // physical pages to only find out that they are not aligned. If the
    // physical alignment is more than a page, the virtual addresses don't help
    // as they might not be aligned even though the physical pages are. But if
    // the alignment is less than a page and the virtual address is not
    // properly aligned, then the physical address will not be properly aligned
    // either.
    //

    PageSize = MmPageSize();
    BufferOffset = OriginalBuffer->Internal.CurrentOffset;
    if (((OriginalFlags & IO_BUFFER_INTERNAL_FLAG_MAPPED) != 0) &&
        (Alignment != 1) &&
        (Alignment < PageSize) &&
        (BufferOffset != OriginalBuffer->Internal.TotalSize)) {

        FragmentIndex = 0;
        CurrentOffset = 0;
        EndOffset = BufferOffset + SizeInBytes;
        if (EndOffset > OriginalBuffer->Internal.TotalSize) {
            EndOffset = OriginalBuffer->Internal.TotalSize;
        }

        PhysicalAddressEnd = INVALID_PHYSICAL_ADDRESS;
        while (BufferOffset < EndOffset) {
            Fragment = &(OriginalBuffer->Fragment[FragmentIndex]);
            if (BufferOffset >= (CurrentOffset + Fragment->Size)) {
                CurrentOffset += Fragment->Size;
                FragmentIndex += 1;
                continue;
            }

            FragmentOffset = BufferOffset - CurrentOffset;
            VirtualAddress = Fragment->VirtualAddress + FragmentOffset;
            FragmentSize = Fragment->Size - FragmentOffset;

            //
            // The size and virtual address better be aligned.
            //

            if ((IS_POINTER_ALIGNED(VirtualAddress, Alignment) == FALSE) ||
                (IS_ALIGNED(FragmentSize, Alignment) == FALSE)) {

                AllocateIoBuffer = TRUE;
                goto ValidateIoBufferEnd;
            }

            BufferOffset += FragmentSize;
            CurrentOffset += Fragment->Size;

            ASSERT(BufferOffset == CurrentOffset);

            FragmentIndex += 1;
        }
    }

    //
    // Make sure the I/O buffer is locked in place as the physical addresses
    // need to be validated.
    //

    ASSERT(LockedBuffer == OriginalBuffer);

    Status = MmpLockIoBuffer(&LockedBuffer);
    if (!KSUCCESS(Status)) {
        goto ValidateIoBufferEnd;
    }

    //
    // Validate that the physical pages starting at the I/O buffer's offset are
    // in the specified range, aligned and that they are physically contiguous,
    // if necessary.
    //

    BufferOffset = LockedBuffer->Internal.CurrentOffset;
    if (BufferOffset != LockedBuffer->Internal.TotalSize) {
        FragmentIndex = 0;
        CurrentOffset = 0;
        EndOffset = BufferOffset + SizeInBytes;
        if (EndOffset > LockedBuffer->Internal.TotalSize) {
            EndOffset = LockedBuffer->Internal.TotalSize;
        }

        PhysicalAddressEnd = INVALID_PHYSICAL_ADDRESS;
        while (BufferOffset < EndOffset) {
            Fragment = &(LockedBuffer->Fragment[FragmentIndex]);
            if (BufferOffset >= (CurrentOffset + Fragment->Size)) {
                CurrentOffset += Fragment->Size;
                FragmentIndex += 1;
                continue;
            }

            FragmentOffset = BufferOffset - CurrentOffset;
            PhysicalAddressStart = Fragment->PhysicalAddress + FragmentOffset;
            if ((PhysicallyContiguous != FALSE) &&
                (PhysicalAddressEnd != INVALID_PHYSICAL_ADDRESS)  &&
                (PhysicalAddressStart != PhysicalAddressEnd)) {

                AllocateIoBuffer = TRUE;
                goto ValidateIoBufferEnd;
            }

            FragmentSize = Fragment->Size - FragmentOffset;

            //
            // The size and physical address better be aligned.
            //

            if ((IS_ALIGNED(PhysicalAddressStart, Alignment) == FALSE) ||
                (IS_ALIGNED(FragmentSize, Alignment) == FALSE)) {

                AllocateIoBuffer = TRUE;
                goto ValidateIoBufferEnd;
            }

            PhysicalAddressEnd = PhysicalAddressStart + FragmentSize;

            ASSERT(PhysicalAddressEnd > PhysicalAddressStart);

            if ((PhysicalAddressStart < MinimumPhysicalAddress) ||
                (PhysicalAddressEnd > MaximumPhysicalAddress)) {

                AllocateIoBuffer = TRUE;
                goto ValidateIoBufferEnd;
            }

            BufferOffset += FragmentSize;
            CurrentOffset += Fragment->Size;

            ASSERT(BufferOffset == CurrentOffset);

            FragmentIndex += 1;
        }
    }

    //
    // With the existing physical pages in the right range, extend the buffer
    // if necessary and possible.
    //

    LockedFlags = LockedBuffer->Internal.Flags;
    if (((LockedFlags & IO_BUFFER_INTERNAL_FLAG_EXTENDABLE) != 0) &&
        ((LockedBuffer->Internal.CurrentOffset + SizeInBytes) >
         LockedBuffer->Internal.TotalSize)) {

        //
        // An extensible buffer should always be initialized with locked pages
        // from the beginning.
        //

        ASSERT(LockedBuffer == OriginalBuffer);

        //
        // If the buffer must be physically contiguous, there is no guarantee
        // the extension can satisfy that unless the current offset is at the
        // end of the existing buffer.
        //

        if ((PhysicallyContiguous != FALSE) &&
            (LockedBuffer->Internal.CurrentOffset !=
             LockedBuffer->Internal.TotalSize)) {

            AllocateIoBuffer = TRUE;
            goto ValidateIoBufferEnd;
        }

        ExtensionSize = (LockedBuffer->Internal.CurrentOffset + SizeInBytes) -
                        LockedBuffer->Internal.TotalSize;

        Status = MmpExtendIoBuffer(LockedBuffer,
                                   MinimumPhysicalAddress,
                                   MaximumPhysicalAddress,
                                   Alignment,
                                   ExtensionSize,
                                   PhysicallyContiguous);

        goto ValidateIoBufferEnd;
    }

ValidateIoBufferEnd:
    if (AllocateIoBuffer != FALSE) {

        //
        // If the buffer was locked down and then found to be useless, release
        // it now.
        //

        if (OriginalBuffer != LockedBuffer) {
            MmFreeIoBuffer(LockedBuffer);
        }

        IoBufferFlags = 0;
        if (PhysicallyContiguous != FALSE) {
            IoBufferFlags |= IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
        }

        NewBuffer = MmAllocateNonPagedIoBuffer(MinimumPhysicalAddress,
                                               MaximumPhysicalAddress,
                                               Alignment,
                                               SizeInBytes,
                                               IoBufferFlags);

        if (NewBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
        }

    } else if (OriginalBuffer != LockedBuffer) {
        NewBuffer = LockedBuffer;
        *LockedCopy = TRUE;
    }

    if (NewBuffer != NULL) {
        *IoBuffer = NewBuffer;
    }

    return Status;
}

KSTATUS
MmValidateIoBufferForCachedIo (
    PIO_BUFFER *IoBuffer,
    UINTN SizeInBytes,
    UINTN Alignment
    )

/*++

Routine Description:

    This routine validates an I/O buffer for an I/O operation, potentially
    returning a new I/O buffer.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer pointer. On entry, it stores
        the pointer to the I/O buffer to evaluate. On exit, it stores a pointer
        to a valid I/O buffer, that may actually be a new I/O buffer.

    SizeInBytes - Supplies the required size of the I/O buffer.

    Alignment - Supplies the required alignment of the I/O buffer.

Return Value:

    Status code.

--*/

{

    BOOL AllocateIoBuffer;
    UINTN AvailableFragments;
    PIO_BUFFER Buffer;
    ULONG InternalFlags;
    UINTN Offset;
    UINTN PageCount;
    ULONG PageShift;
    ULONG PageSize;
    KSTATUS Status;

    AllocateIoBuffer = FALSE;
    Buffer = *IoBuffer;
    PageShift = MmPageShift();
    PageSize = MmPageSize();
    Status = STATUS_SUCCESS;

    //
    // If no I/O buffer was supplied, it is not cached backed or the buffer
    // cannot be expanded, then a buffer needs to be allocated.
    //

    if ((Buffer == NULL) || (Buffer->Internal.PageCacheEntries == NULL)) {
        AllocateIoBuffer = TRUE;
        goto ValidateIoBufferForCachedIoEnd;
    }

    InternalFlags = Buffer->Internal.Flags;
    if ((InternalFlags & IO_BUFFER_INTERNAL_FLAG_EXTENDABLE) == 0) {
        AllocateIoBuffer = TRUE;
        goto ValidateIoBufferForCachedIoEnd;
    }

    //
    // If the I/O buffer's current offset is not aligned or not at the end of
    // the buffer, then the buffer cannot be extended to directly handle the
    // I/O.
    //

    if ((IS_ALIGNED(Buffer->Internal.CurrentOffset, Alignment) == FALSE) ||
        (Buffer->Internal.CurrentOffset != Buffer->Internal.TotalSize)) {

        AllocateIoBuffer = TRUE;
        goto ValidateIoBufferForCachedIoEnd;
    }

    //
    // Determine if the I/O buffer has enough fragments to extend into.
    //

    AvailableFragments = Buffer->Internal.MaxFragmentCount -
                         Buffer->FragmentCount;

    PageCount = ALIGN_RANGE_UP(SizeInBytes, PageSize) >> PageShift;
    if (PageCount > AvailableFragments) {
        AllocateIoBuffer = TRUE;
        goto ValidateIoBufferForCachedIoEnd;
    }

    //
    // Determine if it has enough page cache entries to handle any extension.
    //

    Offset = ALIGN_RANGE_UP(Buffer->Internal.CurrentOffset, PageSize);
    PageCount += Offset >> PageShift;
    if (PageCount > Buffer->Internal.PageCacheEntryCount) {
        AllocateIoBuffer = TRUE;
        goto ValidateIoBufferForCachedIoEnd;
    }

ValidateIoBufferForCachedIoEnd:
    if (AllocateIoBuffer != FALSE) {
        SizeInBytes = ALIGN_RANGE_UP(SizeInBytes, Alignment);
        Buffer = MmAllocateUninitializedIoBuffer(SizeInBytes, 0);
        if (Buffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;

        } else {
            *IoBuffer = Buffer;
        }
    }

    return Status;
}

VOID
MmIoBufferAppendPage (
    PIO_BUFFER IoBuffer,
    PVOID PageCacheEntry,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine appends a page, as described by its VA/PA or page cache entry,
    to the end of the given I/O buffer. The caller should either supply a page
    cache entry or a physical address (with an optional virtual address), but
    not both.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    PageCacheEntry - Supplies an optional pointer to the page cache entry whose
        data will be appended to the I/O buffer.

    VirtualAddress - Supplies an optional virtual address for the range.

    PhysicalAddress - Supplies the optional physical address of the data that
        is to be set in the I/O buffer at the given offset. Use
        INVALID_PHYSICAL_ADDRESS when supplying a page cache entry.

Return Value:

    None.

--*/

{

    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    ULONG MapFlags;
    UINTN PageIndex;
    ULONG PageSize;

    PageSize = MmPageSize();

    ASSERT((IoBuffer->Internal.Flags &
            IO_BUFFER_INTERNAL_FLAG_EXTENDABLE) != 0);

    ASSERT((PageCacheEntry == NULL) ||
           (PhysicalAddress == INVALID_PHYSICAL_ADDRESS));

    ASSERT((PageCacheEntry == NULL) ||
           (IoBuffer->Internal.PageCacheEntries != NULL));

    //
    // If a page cache entry was supplied, this better be the first page of the
    // I/O buffer or it better be already marked locked.
    //

    ASSERT((PageCacheEntry == NULL) ||
           (IoBuffer->FragmentCount == 0) ||
           ((IoBuffer->Internal.Flags &
             IO_BUFFER_INTERNAL_FLAG_MEMORY_LOCKED) != 0));

    //
    // There better bet at least one free fragment in case this is not
    // contiguous with the previous fragment.
    //

    ASSERT(IoBuffer->FragmentCount < IoBuffer->Internal.MaxFragmentCount);

    //
    // The current total size of the buffer better be page aligned.
    //

    ASSERT(IS_ALIGNED(IoBuffer->Internal.TotalSize, PageSize) != FALSE);

    //
    // Get the last fragment in the I/O buffer.
    //

    FragmentIndex = 0;
    if (IoBuffer->FragmentCount != 0) {
        FragmentIndex = IoBuffer->FragmentCount - 1;
    }

    //
    // If a page cache entry was supplied, use its physical and virtual
    // addresses.
    //

    if (PageCacheEntry != NULL) {
        PhysicalAddress = IoGetPageCacheEntryPhysicalAddress(PageCacheEntry,
                                                             &MapFlags);

        IoBuffer->Internal.MapFlags |= MapFlags;
        VirtualAddress = IoGetPageCacheEntryVirtualAddress(PageCacheEntry);
    }

    //
    // If the address is physically and virtually contiguous with the last
    // fragment, then append it there.
    //

    Fragment = &(IoBuffer->Fragment[FragmentIndex]);
    if ((IoBuffer->FragmentCount != 0) &&
        ((Fragment->PhysicalAddress + Fragment->Size) == PhysicalAddress) &&
        (((VirtualAddress == NULL) && (Fragment->VirtualAddress == NULL)) ||
         (((VirtualAddress != NULL) && (Fragment->VirtualAddress != NULL)) &&
          ((Fragment->VirtualAddress + Fragment->Size) == VirtualAddress)))) {

        ASSERT((Fragment->Size + PageSize) > Fragment->Size);

        Fragment->Size += PageSize;

    //
    // Otherwise stick it in the next fragment.
    //

    } else {
        if (IoBuffer->FragmentCount != 0) {
            Fragment += 1;
        }

        ASSERT(Fragment->PhysicalAddress == INVALID_PHYSICAL_ADDRESS);
        ASSERT(Fragment->VirtualAddress == NULL);
        ASSERT(Fragment->Size == 0);

        Fragment->PhysicalAddress = PhysicalAddress;
        Fragment->VirtualAddress = VirtualAddress;
        Fragment->Size = PageSize;
        IoBuffer->FragmentCount += 1;
    }

    //
    // If there is a page cache entry, then stick it into the array of page
    // cache entries at the appropriate offset.
    //

    if (PageCacheEntry != NULL) {

        //
        // The fragment count should always be less than or equal to the page
        // count.
        //

        ASSERT(IoBuffer->FragmentCount <=
               IoBuffer->Internal.PageCacheEntryCount);

        PageIndex = IoBuffer->Internal.TotalSize >> MmPageShift();

        ASSERT(PageIndex < IoBuffer->Internal.PageCacheEntryCount);
        ASSERT(IoBuffer->Internal.PageCacheEntries != NULL);
        ASSERT(IoBuffer->Internal.PageCacheEntries[PageIndex] == NULL);

        IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_CACHE_BACKED |
                                    IO_BUFFER_INTERNAL_FLAG_MEMORY_LOCKED;

        IoPageCacheEntryAddReference(PageCacheEntry);
        IoBuffer->Internal.PageCacheEntries[PageIndex] = PageCacheEntry;
    }

    IoBuffer->Internal.TotalSize += PageSize;
    return;
}

VOID
MmSetIoBufferPageCacheEntry (
    PIO_BUFFER IoBuffer,
    UINTN IoBufferOffset,
    PVOID PageCacheEntry
    )

/*++

Routine Description:

    This routine sets the given page cache entry in the I/O buffer at the given
    offset. The physical address of the page cache entry should match that of
    the I/O buffer at the given offset.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    IoBufferOffset - Supplies an offset into the given I/O buffer.

    PageCacheEntry - Supplies a pointer to the page cache entry to set.

Return Value:

    None.

--*/

{

    ULONG MapFlags;
    UINTN PageIndex;
    PHYSICAL_ADDRESS PhysicalAddress;

    IoBufferOffset += IoBuffer->Internal.CurrentOffset;

    //
    // The I/O buffer offset better be page aligned.
    //

    ASSERT(IS_ALIGNED(IoBufferOffset, MmPageSize()));
    ASSERT((IoBuffer->Internal.Flags & IO_BUFFER_INTERNAL_FLAG_USER_MODE) == 0);

    PageIndex = IoBufferOffset >> MmPageShift();

    //
    // The offset's page index better be valid, un-set and the physical address
    // at the given offset better match what's in the page cache entry.
    //

    ASSERT(PageIndex < IoBuffer->Internal.PageCacheEntryCount);
    ASSERT(IoBuffer->Internal.PageCacheEntries != NULL);
    ASSERT(IoBuffer->Internal.PageCacheEntries[PageIndex] == NULL);

    PhysicalAddress = IoGetPageCacheEntryPhysicalAddress(PageCacheEntry,
                                                         &MapFlags);

    ASSERT(MmGetIoBufferPhysicalAddress(IoBuffer, IoBufferOffset) ==
           PhysicalAddress);

    IoBuffer->Internal.MapFlags |= MapFlags;
    IoPageCacheEntryAddReference(PageCacheEntry);
    IoBuffer->Internal.PageCacheEntries[PageIndex] = PageCacheEntry;

    //
    // This I/O buffer is at least backed by one page cache entry. It should
    // already be marked as locked.
    //

    ASSERT((IoBuffer->Internal.Flags &
            IO_BUFFER_INTERNAL_FLAG_MEMORY_LOCKED) != 0);

    IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_CACHE_BACKED;
    return;
}

PVOID
MmGetIoBufferPageCacheEntry (
    PIO_BUFFER IoBuffer,
    UINTN IoBufferOffset
    )

/*++

Routine Description:

    This routine returns the page cache entry associated with the given I/O
    buffer at the given offset into the buffer.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    IoBufferOffset - Supplies an offset into the I/O buffer, in bytes.

Return Value:

    Returns a pointer to a page cache entry if the physical page at the given
    offset has been cached, or NULL otherwise.

--*/

{

    ULONG InternalFlags;
    UINTN PageIndex;

    InternalFlags = IoBuffer->Internal.Flags;
    if ((InternalFlags & IO_BUFFER_INTERNAL_FLAG_CACHE_BACKED) == 0) {
        return NULL;
    }

    IoBufferOffset += IoBuffer->Internal.CurrentOffset;

    //
    // The I/O buffer offset better be page aligned.
    //

    ASSERT(IS_ALIGNED(IoBufferOffset, MmPageSize()));
    ASSERT((InternalFlags & IO_BUFFER_INTERNAL_FLAG_USER_MODE) == 0);

    PageIndex = IoBufferOffset >> MmPageShift();

    ASSERT(PageIndex < IoBuffer->Internal.PageCacheEntryCount);

    return IoBuffer->Internal.PageCacheEntries[PageIndex];
}

KERNEL_API
UINTN
MmGetIoBufferSize (
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine returns the size of the I/O buffer, in bytes.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

Return Value:

    Returns the size of the I/O buffer, in bytes.

--*/

{

    return IoBuffer->Internal.TotalSize - IoBuffer->Internal.CurrentOffset;
}

KERNEL_API
UINTN
MmGetIoBufferCurrentOffset (
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine returns the given I/O buffer's current offset. The offset is
    the point at which all I/O should begin.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

Return Value:

    Returns the I/O buffers current offset.

--*/

{

    return IoBuffer->Internal.CurrentOffset;
}

KERNEL_API
VOID
MmSetIoBufferCurrentOffset (
    PIO_BUFFER IoBuffer,
    UINTN Offset
    )

/*++

Routine Description:

    This routine sets the given I/O buffer's current offset. The offset is
    the point at which all I/O should begin.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    Offset - Supplies the new offset for the I/O buffer.

Return Value:

    None.

--*/

{

    IoBuffer->Internal.CurrentOffset = Offset;
    return;
}

KERNEL_API
VOID
MmIoBufferIncrementOffset (
    PIO_BUFFER IoBuffer,
    UINTN OffsetIncrement
    )

/*++

Routine Description:

    This routine increments the I/O buffer's current offset by the given amount.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    OffsetIncrement - Supplies the number of bytes by which the offset will be
        incremented.

Return Value:

    None.

--*/

{

    IoBuffer->Internal.CurrentOffset += OffsetIncrement;

    ASSERT(IoBuffer->Internal.CurrentOffset <= IoBuffer->Internal.TotalSize);

    return;
}

KERNEL_API
VOID
MmIoBufferDecrementOffset (
    PIO_BUFFER IoBuffer,
    UINTN OffsetDecrement
    )

/*++

Routine Description:

    This routine decrements the I/O buffer's current offset by the given amount.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    OffsetDecrement - Supplies the number of bytes by which the offset will be
        incremented.

Return Value:

    None.

--*/

{

    IoBuffer->Internal.CurrentOffset -= OffsetDecrement;

    ASSERT(IoBuffer->Internal.CurrentOffset <= IoBuffer->Internal.TotalSize);

    return;
}

PHYSICAL_ADDRESS
MmGetIoBufferPhysicalAddress (
    PIO_BUFFER IoBuffer,
    UINTN IoBufferOffset
    )

/*++

Routine Description:

    This routine returns the physical address at a given offset within an I/O
    buffer.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    IoBufferOffset - Supplies a byte offset into the I/O buffer.

Return Value:

    Returns the physical address of the memory at the given offset within the
    I/O buffer.

--*/

{

    UINTN FragmentEnd;
    UINTN FragmentIndex;
    UINTN FragmentStart;
    PHYSICAL_ADDRESS PhysicalAddress;

    IoBufferOffset += IoBuffer->Internal.CurrentOffset;
    PhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    FragmentStart = 0;
    for (FragmentIndex = 0;
         FragmentIndex < IoBuffer->FragmentCount;
         FragmentIndex += 1) {

        FragmentEnd = FragmentStart + IoBuffer->Fragment[FragmentIndex].Size;
        if ((IoBufferOffset >= FragmentStart) &&
            (IoBufferOffset < FragmentEnd)) {

            PhysicalAddress = IoBuffer->Fragment[FragmentIndex].PhysicalAddress;
            PhysicalAddress += (IoBufferOffset - FragmentStart);
            break;
        }

        FragmentStart = FragmentEnd;
    }

    return PhysicalAddress;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
MmpReleaseIoBufferResources (
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine release all the memory resources for an I/O buffer. It does
    not release the memory allocated for the I/O buffer structure itself.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer to release.

Return Value:

    None.

--*/

{

    ULONGLONG EndAddress;
    ULONG Flags;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    PVOID *PageCacheEntries;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    UINTN PageCount;
    UINTN PageIndex;
    ULONG PageShift;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONGLONG StartAddress;

    Flags = IoBuffer->Internal.Flags;
    PageShift = MmPageShift();
    PageSize = MmPageSize();
    IoBuffer->Internal.CurrentOffset = 0;

    //
    // First unmap the I/O buffer, if necessary..
    //

    if ((Flags & IO_BUFFER_INTERNAL_FLAG_VA_OWNED) != 0) {
        MmpUnmapIoBuffer(IoBuffer);
    }

    //
    // Unless the physical memory is owned, locked, or backed by the page
    // cache there is no more clean-up to perform.
    //

    if (((Flags & IO_BUFFER_INTERNAL_FLAG_PA_OWNED) == 0) &&
        ((Flags & IO_BUFFER_INTERNAL_FLAG_LOCK_OWNED) == 0) &&
        ((Flags & IO_BUFFER_INTERNAL_FLAG_CACHE_BACKED) == 0)) {

        return;
    }

    //
    // Now loop to free or unlock the physical pages. If the memory itself is
    // owned by the I/O buffer structure or the I/O buffer was filled in with
    // page cache entries, iterate over the I/O buffer, releasing each fragment.
    // If the I/O buffer is locked, then just unlock each page.
    //

    PageCacheEntry = NULL;
    PageCacheEntries = IoBuffer->Internal.PageCacheEntries;
    for (FragmentIndex = 0;
         FragmentIndex < IoBuffer->FragmentCount;
         FragmentIndex += 1) {

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);
        StartAddress = Fragment->PhysicalAddress;
        EndAddress = StartAddress + Fragment->Size;
        PhysicalAddress = ALIGN_RANGE_DOWN(StartAddress, PageSize);
        PageCount = (ALIGN_RANGE_UP(EndAddress, PageSize) - PhysicalAddress) >>
                    PageShift;

        for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
            if (PageCacheEntries != NULL) {
                PageCacheEntry = *PageCacheEntries;
                PageCacheEntries += 1;
            }

            //
            // If there is a page cache entry, do not free the page. It may
            // or may not get released when the page cache entry reference
            // is dropped.
            //

            if (PageCacheEntry != NULL) {

                ASSERT((Flags & IO_BUFFER_INTERNAL_FLAG_CACHE_BACKED) != 0);
                ASSERT((Fragment->PhysicalAddress + (PageIndex * PageSize)) ==
                       IoGetPageCacheEntryPhysicalAddress(PageCacheEntry,
                                                          NULL));

                IoPageCacheEntryReleaseReference(PageCacheEntry);

            //
            // Otherwise the page needs to be unlocked and/or freed.
            //

            } else {
                if ((Flags & IO_BUFFER_INTERNAL_FLAG_LOCK_OWNED) != 0) {
                    MmpUnlockPhysicalPages(PhysicalAddress, 1);
                }

                if ((Flags & IO_BUFFER_INTERNAL_FLAG_PA_OWNED) != 0) {
                    MmFreePhysicalPage(PhysicalAddress);
                }
            }

            PhysicalAddress += PageSize;
        }
    }

    return;
}

KSTATUS
MmpMapIoBufferFragments (
    PIO_BUFFER IoBuffer,
    UINTN FragmentStart,
    UINTN FragmentCount,
    ULONG MapFlags,
    BOOL VirtuallyContiguous
    )

/*++

Routine Description:

    This routine maps the given set of fragments within the provided I/O buffer.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    FragmentStart - Supplies the index of the first fragment to be mapped.

    FragmentCount - Supplies the number of fragments to be mapped.

    MapFlags - Supplies the map flags to use when mapping the I/O buffer. See
        MAP_FLAG_* for definitions.

    VirtuallyContiguous - Supplies a boolean indicating whether or not the
        VA needs to be virtually contiguous.

Return Value:

    Status code.

--*/

{

    UINTN AddressCount;
    UINTN AddressIndex;
    UINTN ByteOffset;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentEnd;
    UINTN FragmentIndex;
    UINTN FragmentPages;
    UINTN FragmentSize;
    UINTN NewSize;
    PVOID *PageCacheEntries;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    UINTN PageIndex;
    UINTN PageOffset;
    ULONG PageShift;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    UINTN SearchIndex;
    UINTN Size;
    KSTATUS Status;
    VM_ALLOCATION_PARAMETERS VaRequest;
    PVOID VirtualAddress;
    PVOID Virtuals[MM_MAP_IO_BUFFER_LOCAL_VIRTUAL_PAGES];

    FragmentEnd = FragmentStart + FragmentCount;
    PageShift = MmPageShift();
    PageSize = MmPageSize();
    MapFlags |= IoBuffer->Internal.MapFlags;

    //
    // Get the current page offset if this is page cache backed.
    //

    PageIndex = 0;
    PageCacheEntries = NULL;
    if ((IoBuffer->Internal.Flags &
         IO_BUFFER_INTERNAL_FLAG_CACHE_BACKED) != 0) {

        ASSERT(IoBuffer->Internal.PageCacheEntries != NULL);

        PageCacheEntries = IoBuffer->Internal.PageCacheEntries;
        PageOffset = 0;
        for (FragmentIndex = 0;
             FragmentIndex < FragmentStart;
             FragmentIndex += 1) {

            Fragment = &(IoBuffer->Fragment[FragmentIndex]);
            PageOffset += Fragment->Size;
        }

        ASSERT(IS_ALIGNED(PageOffset, PageSize) != FALSE);

        PageIndex = PageOffset >> PageShift;
    }

    Virtuals[0] = NULL;

    //
    // Loop until all fragments are mapped.
    //

    FragmentIndex = FragmentStart;
    while (FragmentIndex < FragmentEnd) {

        //
        // Determine the size of the fragments to be mapped. Align all
        // fragments up to a page size so that the first and last fragments,
        // which might not be full pages, get their own VA space.
        //

        Size = 0;
        for (SearchIndex = FragmentIndex;
             SearchIndex < FragmentEnd;
             SearchIndex += 1) {

            Fragment = &(IoBuffer->Fragment[SearchIndex]);
            ByteOffset = REMAINDER(Fragment->PhysicalAddress, PageSize);
            FragmentSize = Fragment->Size + ByteOffset;
            Size += ALIGN_RANGE_UP(FragmentSize, PageSize);
        }

        ASSERT(Size != 0);
        ASSERT(IS_ALIGNED(Size, PageSize) != FALSE);

        if (VirtuallyContiguous != FALSE) {
            AddressCount = 1;
            if (Virtuals[0] == NULL) {
                VaRequest.Address = NULL;
                VaRequest.Size = Size;
                VaRequest.Alignment = PageSize;
                VaRequest.Min = 0;
                VaRequest.Max = MAX_ADDRESS;
                VaRequest.MemoryType = MemoryTypeIoBuffer;
                VaRequest.Strategy = AllocationStrategyAnyAddress;
                Status = MmpAllocateAddressRange(&MmKernelVirtualSpace,
                                                 &VaRequest,
                                                 FALSE);

                if (!KSUCCESS(Status)) {
                    goto MapIoBufferFragmentsEnd;
                }

                Virtuals[0] = VaRequest.Address;
            }

            ASSERT(Virtuals[0] >= KERNEL_VA_START);

        } else {
            AddressCount = Size >> PageShift;
            if (AddressCount > (sizeof(Virtuals) / sizeof(Virtuals[0]))) {
                AddressCount = sizeof(Virtuals) / sizeof(Virtuals[0]);
            }

            Status = MmpAllocateAddressRanges(&MmKernelVirtualSpace,
                                              PageSize,
                                              AddressCount,
                                              MemoryTypeIoBuffer,
                                              Virtuals);

            if (!KSUCCESS(Status)) {
                goto MapIoBufferFragmentsEnd;
            }
        }

        //
        // Loop assigning virtual addresses into fragments.
        //

        AddressIndex = 0;
        while ((FragmentIndex < FragmentEnd) && (AddressIndex < AddressCount)) {
            Fragment = &(IoBuffer->Fragment[FragmentIndex]);

            //
            // If the physical address is not page aligned, then the stored
            // virtual address should account for the page byte offset. This
            // should only happen on the first fragment.
            //

            PhysicalAddress = Fragment->PhysicalAddress;
            ByteOffset = REMAINDER(PhysicalAddress, PageSize);

            ASSERT((ByteOffset == 0) || (FragmentIndex == 0));

            FragmentSize = Fragment->Size + ByteOffset;
            PhysicalAddress -= ByteOffset;

            //
            // If the size is not aligned, align it up. This can only happen on
            // the first and last fragments.
            //

            ASSERT((IS_ALIGNED(FragmentSize, PageSize) != FALSE) ||
                   (FragmentIndex == 0) ||
                   (FragmentIndex == (IoBuffer->FragmentCount - 1)));

            FragmentSize = ALIGN_RANGE_UP(FragmentSize, PageSize);
            FragmentPages = FragmentSize >> PageShift;

            //
            // See if the fragment needs to be split due to discontiguous VAs.
            //

            if (VirtuallyContiguous == FALSE) {
                SearchIndex = AddressIndex + 1;

                //
                // Find out how many contiguous pages were returned.
                //

                while ((SearchIndex < FragmentPages) &&
                       (SearchIndex < AddressCount)) {

                    if (Virtuals[SearchIndex - 1] + PageSize !=
                        Virtuals[SearchIndex]) {

                        break;
                    }

                    SearchIndex += 1;
                }

                if (SearchIndex - AddressIndex < FragmentPages) {
                    FragmentSize = (SearchIndex - AddressIndex) << PageShift;
                    NewSize = FragmentSize - ByteOffset;

                    ASSERT(IS_ALIGNED(Fragment->PhysicalAddress + NewSize,
                                      PageSize));

                    MmpSplitIoBufferFragment(IoBuffer, FragmentIndex, NewSize);
                    FragmentEnd += 1;
                }
            }

            //
            // Map the whole fragment now that there's a virtually contiguous
            // range for it.
            //

            VirtualAddress = Virtuals[AddressIndex];
            AddressIndex += FragmentSize >> PageShift;
            FragmentIndex += 1;
            Fragment->VirtualAddress = VirtualAddress + ByteOffset;
            while (FragmentSize != 0) {
                MmpMapPage(PhysicalAddress, VirtualAddress, MapFlags);

                //
                // Let the page cache entry keep this mapping when the I/O
                // buffer is done with it.
                //

                if (PageCacheEntries != NULL) {
                    PageCacheEntry = PageCacheEntries[PageIndex];
                    if (PageCacheEntry != NULL) {
                        IoSetPageCacheEntryVirtualAddress(PageCacheEntry,
                                                          VirtualAddress);
                    }

                    PageIndex += 1;
                }

                PhysicalAddress += PageSize;
                VirtualAddress += PageSize;
                FragmentSize -= PageSize;
            }

            if (VirtuallyContiguous != FALSE) {
                Virtuals[0] = VirtualAddress;
            }
        }

        //
        // Ensure all virtual addresses were used up and none are leaked.
        //

        ASSERT((AddressIndex == AddressCount) ||
               (VirtuallyContiguous != FALSE));
    }

    Status = STATUS_SUCCESS;

MapIoBufferFragmentsEnd:
    return Status;
}

VOID
MmpUnmapIoBuffer (
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine unmaps the given I/O buffer.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer to be unmapped.

Return Value:

    None.

--*/

{

    ULONG ByteOffset;
    BOOL CacheMatch;
    PVOID CurrentAddress;
    PVOID EndAddress;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    UINTN FragmentSize;
    ULONG InternalFlags;
    PVOID PageCacheAddress;
    PVOID *PageCacheEntries;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    UINTN PageCacheIndex;
    UINTN PageCount;
    UINTN PageIndex;
    ULONG PageShift;
    ULONG PageSize;
    PVOID StartAddress;
    KSTATUS Status;
    UINTN UnmapSize;
    PVOID UnmapStartAddress;

    ASSERT((IoBuffer->Internal.Flags & IO_BUFFER_INTERNAL_FLAG_VA_OWNED) != 0);

    PageShift = MmPageShift();
    PageSize = MmPageSize();
    PageCacheEntries = NULL;
    InternalFlags = IoBuffer->Internal.Flags;
    if ((InternalFlags & IO_BUFFER_INTERNAL_FLAG_CACHE_BACKED) != 0) {

        ASSERT(IoBuffer->Internal.PageCacheEntries != NULL);

        PageCacheEntries = IoBuffer->Internal.PageCacheEntries;
    }

    StartAddress = NULL;
    EndAddress = NULL;
    UnmapSize = 0;
    FragmentOffset = 0;
    FragmentIndex = 0;
    PageCacheIndex = 0;
    while (FragmentIndex < IoBuffer->FragmentCount) {
        Fragment = &(IoBuffer->Fragment[FragmentIndex]);

        //
        // If this fragment has no virtual address, skip it. Maybe the next
        // fragment is virtually contiguous with the last.
        //

        if (Fragment->VirtualAddress == NULL) {
            FragmentIndex += 1;
            continue;
        }

        //
        // Start by assuming there will be nothing to unmap this time around,
        // hoping that multiple fragments can be unmapped together.
        //

        UnmapStartAddress = NULL;

        //
        // If there are page cache entries to worry about, then go through the
        // current fragment page by page starting from the fragment offset.
        // This may be finishing the same fragment started the last time around.
        //

        if (PageCacheEntries != NULL) {
            FragmentSize = Fragment->Size - FragmentOffset;
            CurrentAddress = Fragment->VirtualAddress + FragmentOffset;

            ASSERT(IS_ALIGNED((UINTN)CurrentAddress, PageSize) != FALSE);
            ASSERT(IS_ALIGNED(FragmentSize, PageSize) != FALSE);

            PageCount = FragmentSize >> PageShift;
            for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {

                ASSERT(PageCacheIndex < IoBuffer->Internal.PageCacheEntryCount);

                PageCacheEntry = PageCacheEntries[PageCacheIndex];
                FragmentOffset += PageSize;
                PageCacheIndex += 1;

                //
                // Check to see if the current virtual address matches the page
                // cache entry's virtual address.
                //

                CacheMatch = FALSE;
                if (PageCacheEntry != NULL) {
                    PageCacheAddress = IoGetPageCacheEntryVirtualAddress(
                                                               PageCacheEntry);

                    if (PageCacheAddress == CurrentAddress) {
                        CacheMatch = TRUE;
                    }
                }

                //
                // If the current virtual address needs to be unmapped, check
                // to see if it is contiguous with an existing run. If not, go
                // to unmap the existing run and set the current address as the
                // start of the next. If there is no current run, set this as
                // the beginning of the next run.
                //

                if (CacheMatch == FALSE) {
                    if (StartAddress != NULL) {
                        if (CurrentAddress != EndAddress) {
                            UnmapStartAddress = StartAddress;
                            UnmapSize = EndAddress - StartAddress;
                            StartAddress = CurrentAddress;
                            EndAddress = CurrentAddress + PageSize;
                            break;
                        }

                    } else {
                        StartAddress = CurrentAddress;
                        EndAddress = CurrentAddress;
                    }

                    EndAddress += PageSize;
                    CurrentAddress += PageSize;
                    continue;
                }

                //
                // The current virtual address is owned by the page cache. It
                // should not be unmapped. So if there is an existing run of
                // memory to unmap, go to unmap it. And don't start a new run.
                // Otherwise just move to the next virtual address.
                //

                ASSERT(CacheMatch != FALSE);

                if (StartAddress != NULL) {
                    UnmapStartAddress = StartAddress;
                    UnmapSize = EndAddress - StartAddress;
                    StartAddress = NULL;
                    break;
                }

                CurrentAddress += PageSize;
            }

            //
            // If the whole fragment was processed, move to the next fragment.
            //

            if (FragmentOffset >= Fragment->Size) {
                FragmentOffset = 0;
                FragmentIndex += 1;
            }

        //
        // If the buffer is not backed by page cache entries, treat the
        // fragment as a whole to be unmapped. If it's contiguous with the
        // current run of VA's, add it. Otherwise set it to start a new run and
        // mark the current run to be unmapped.
        //

        } else {
            if ((StartAddress != NULL) &&
                (Fragment->VirtualAddress != EndAddress)) {

                UnmapStartAddress = StartAddress;
                UnmapSize = EndAddress - StartAddress;
                StartAddress = NULL;
            }

            FragmentSize = Fragment->Size;
            if (StartAddress == NULL) {
                StartAddress = Fragment->VirtualAddress;

                //
                // The virtual address of the first fragment may not be
                // page-aligned. Align it down so that whole pages are unmapped.
                //

                ByteOffset = REMAINDER((UINTN)StartAddress, PageSize);

                ASSERT((ByteOffset == 0) || (FragmentIndex == 0));

                FragmentSize += ByteOffset;
                StartAddress -= ByteOffset;
                EndAddress = StartAddress;
            }

            //
            // The fragment size may not be page aligned for the first and last
            // segments. Align it up to a page so that whole pages are unmapped,
            // to match the whole pages that were reserved.
            //

            EndAddress += ALIGN_RANGE_UP(FragmentSize, PageSize);
            FragmentIndex += 1;
        }

        //
        // If there is something to unmap this time around, do the unmapping.
        //

        if (UnmapStartAddress != NULL) {

            ASSERT(UnmapSize != 0);

            //
            // This routine can fail if the system can no longer allocate
            // memory descriptors. Leak the VA. Not much callers can really
            // do.
            //

            Status = MmpFreeAccountingRange(NULL,
                                            UnmapStartAddress,
                                            UnmapSize,
                                            FALSE,
                                            UNMAP_FLAG_SEND_INVALIDATE_IPI);

            ASSERT(KSUCCESS(Status));
        }
    }

    //
    // There may be one last remaining sequence to be unmapped. Do it now.
    //

    if (StartAddress != NULL) {
        UnmapSize = EndAddress - StartAddress;

        //
        // This routine can fail if the system can no longer allocate
        // memory descriptors. Leak the VA. Not much callers can really
        // do.
        //

        Status = MmpFreeAccountingRange(NULL,
                                        StartAddress,
                                        UnmapSize,
                                        FALSE,
                                        UNMAP_FLAG_SEND_INVALIDATE_IPI);

        ASSERT(KSUCCESS(Status));
    }

    IoBuffer->Internal.Flags &= ~(IO_BUFFER_INTERNAL_FLAG_MAPPED |
                                  IO_BUFFER_INTERNAL_FLAG_VA_OWNED |
                                  IO_BUFFER_INTERNAL_FLAG_VA_CONTIGUOUS);

    return;
}

BOOL
MmpIsIoBufferMapped (
    PIO_BUFFER IoBuffer,
    BOOL VirtuallyContiguous
    )

/*++

Routine Description:

    This routine determines if each fragment of the I/O buffer is mapped.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer to evaluate.

    VirtuallyContiguous - Supplies a boolean indicating whether or not the I/O
        buffer needs to be virtually contiguous.

Return Value:

    Returns TRUE if the I/O buffer is mapped appropriately or FALSE otherwise.

--*/

{

    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    PVOID VirtualAddress;

    ASSERT(IoBuffer->FragmentCount >= 1);

    VirtualAddress = IoBuffer->Fragment[0].VirtualAddress;
    for (FragmentIndex = 0;
         FragmentIndex < IoBuffer->FragmentCount;
         FragmentIndex += 1) {

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);
        if ((Fragment->VirtualAddress == NULL) ||
            ((VirtuallyContiguous != FALSE) &&
             (VirtualAddress != Fragment->VirtualAddress))) {

            return FALSE;
        }

        VirtualAddress += Fragment->Size;
    }

    return TRUE;
}

KSTATUS
MmpExtendIoBuffer (
    PIO_BUFFER IoBuffer,
    PHYSICAL_ADDRESS MinimumPhysicalAddress,
    PHYSICAL_ADDRESS MaximumPhysicalAddress,
    UINTN Alignment,
    UINTN Size,
    BOOL PhysicallyContiguous
    )

/*++

Routine Description:

    This routine extends the given I/O buffer by allocating physical pages and
    appending them to the last active fragment or the inactive fragments.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer to extend.

    MinimumPhysicalAddress - Supplies the minimum physical address of the
        extension.

    MaximumPhysicalAddress - Supplies the maximum physical address of the
        extension.

    Alignment - Supplies the required physical alignment of the I/O buffer, in
        bytes.

    Size - Supplies the number of bytes by which the I/O buffer needs to be
        extended.

    PhysicallyContiguous - Supplies a boolean indicating whether or not the
        pages allocated for the extension should be physically contiguous.

Return Value:

    Status code.

--*/

{

    UINTN AvailableFragments;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN PageCount;
    UINTN PageIndex;
    ULONG PageShift;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;

    ASSERT((IoBuffer->Internal.Flags &
            IO_BUFFER_INTERNAL_FLAG_EXTENDABLE) != 0);

    //
    // This better be the first extension or the buffer better already contain
    // locked and owned pages. Mixing and matching is not allowed and this
    // routine sets the ownership and locked flags below. Page cache pages,
    // however, are acceptable.
    //

    ASSERT((IoBuffer->FragmentCount == 0) ||
           (((IoBuffer->Internal.Flags &
              IO_BUFFER_INTERNAL_FLAG_MEMORY_LOCKED) != 0) &&
            (((IoBuffer->Internal.Flags &
               IO_BUFFER_INTERNAL_FLAG_PA_OWNED) != 0) ||
             ((IoBuffer->Internal.Flags &
               IO_BUFFER_INTERNAL_FLAG_CACHE_BACKED) != 0))));

    PageShift = MmPageShift();
    PageSize = MmPageSize();

    //
    // Convert the byte alignment to pages.
    //

    Alignment = Alignment >> PageShift;

    //
    // TODO: Implement support for honoring the minimum and maximum physical
    // addresses in I/O buffers.
    //

    ASSERT((MinimumPhysicalAddress == 0) &&
           ((MaximumPhysicalAddress == MAX_ULONG) ||
            (MaximumPhysicalAddress == MAX_ULONGLONG)));

    //
    // Protect against an extension that the I/O buffer cannot accomodate.
    // Assume the worst case in that each new page needs its own fragment.
    //

    AvailableFragments = IoBuffer->Internal.MaxFragmentCount -
                         IoBuffer->FragmentCount;

    PageCount = ALIGN_RANGE_UP(Size, PageSize) >> PageShift;
    if (PageCount > AvailableFragments) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // The new pages always get attached to the last fragment or set in the
    // next fragment.
    //

    FragmentIndex = IoBuffer->FragmentCount;
    if (FragmentIndex != 0) {
        FragmentIndex -=1 ;
    }

    Fragment = &(IoBuffer->Fragment[FragmentIndex]);

    //
    // If the extension needs to be physically contiguous, allocate the pages
    // and then either append them to the current fragment or add them to the
    // next fragment.
    //

    if (PhysicallyContiguous != FALSE) {
        PhysicalAddress = MmpAllocatePhysicalPages(PageCount, Alignment);
        if (PhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
            Status = STATUS_NO_MEMORY;
            goto ExtendIoBufferEnd;
        }

        if ((Fragment->VirtualAddress == NULL) &&
            ((Fragment->PhysicalAddress + Fragment->Size) ==
             PhysicalAddress)) {

            ASSERT(Fragment->Size != 0);

            Fragment->Size += PageCount << PageShift;

        } else {
            if (IoBuffer->FragmentCount != 0) {
                FragmentIndex += 1;
                Fragment += 1;
            }

            ASSERT(FragmentIndex < IoBuffer->Internal.MaxFragmentCount);
            ASSERT(Fragment->VirtualAddress == NULL);
            ASSERT(Fragment->PhysicalAddress == INVALID_PHYSICAL_ADDRESS);
            ASSERT(Fragment->Size == 0);

            Fragment->PhysicalAddress = PhysicalAddress;
            Fragment->Size = PageCount << PageShift;
            IoBuffer->FragmentCount += 1;
        }

        IoBuffer->Internal.TotalSize += PageCount << PageShift;

    //
    // Otherwise extend the I/O buffer by allocating enough pages to cover the
    // requested size and appending them to the end of the fragment array.
    //

    } else {
        for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
            PhysicalAddress = MmpAllocatePhysicalPages(1, Alignment);
            if (PhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
                Status = STATUS_NO_MEMORY;
                goto ExtendIoBufferEnd;
            }

            //
            // Check to see if the physical page can be attached to the current
            // fragment.
            //

            if ((Fragment->VirtualAddress == NULL) &&
                ((Fragment->PhysicalAddress + Fragment->Size) ==
                 PhysicalAddress)) {

                ASSERT(Fragment->Size != 0);

                Fragment->Size += PageSize;

            } else {
                if (IoBuffer->FragmentCount != 0) {
                    FragmentIndex += 1;
                    Fragment += 1;
                }

                ASSERT(FragmentIndex < IoBuffer->Internal.MaxFragmentCount);
                ASSERT(Fragment->VirtualAddress == NULL);
                ASSERT(Fragment->PhysicalAddress == INVALID_PHYSICAL_ADDRESS);
                ASSERT(Fragment->Size == 0);

                Fragment->PhysicalAddress = PhysicalAddress;
                Fragment->Size = PageSize;
                IoBuffer->FragmentCount += 1;
            }

            IoBuffer->Internal.TotalSize += PageSize;
        }
    }

    //
    // This extension is not mapped, which means the whole buffer is no longer
    // mapped. Unset the flag.
    //

    IoBuffer->Internal.Flags &= ~IO_BUFFER_INTERNAL_FLAG_MAPPED;

    //
    // Also, the I/O buffer now contains non-pageable physical pages that need
    // to be freed on release. So, note that the pages are owned and the memory
    // is locked.
    //

    IoBuffer->Internal.Flags |= IO_BUFFER_INTERNAL_FLAG_PA_OWNED |
                                IO_BUFFER_INTERNAL_FLAG_MEMORY_LOCKED;

    Status = STATUS_SUCCESS;

ExtendIoBufferEnd:
    return Status;
}

KSTATUS
MmpLockIoBuffer (
    PIO_BUFFER *IoBuffer
    )

/*++

Routine Description:

    This routine locks the memory described by the given I/O buffer,
    potentially allocating and handing back a new I/O buffer structure that is
    also locked in memory.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer to be locked. On return, it
        may receive a pointer to a newly allocated I/O buffer.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    UINTN BytesLocked;
    PVOID CurrentAddress;
    PVOID EndAddress;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentSize;
    PIMAGE_SECTION ImageSection;
    PIO_BUFFER LockedIoBuffer;
    PVOID NextAddress;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    UINTN PageCount;
    IO_BUFFER PagedInBuffer;
    UINTN PageIndex;
    UINTN PageOffset;
    ULONG PageShift;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PKPROCESS Process;
    PVOID SectionEnd;
    PVOID StartAddress;
    KSTATUS Status;
    ULONG UnlockedFlags;
    PIO_BUFFER UnlockedIoBuffer;

    UnlockedIoBuffer = *IoBuffer;
    UnlockedFlags = UnlockedIoBuffer->Internal.Flags;
    if ((UnlockedFlags & IO_BUFFER_INTERNAL_FLAG_MEMORY_LOCKED) != 0) {
        return STATUS_SUCCESS;
    }

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // If the unlocked I/O buffer is empty, then there is nothing to lock. It
    // better be a non-paged buffer.
    //

    if (UnlockedIoBuffer->FragmentCount == 0) {

        ASSERT((UnlockedFlags & IO_BUFFER_INTERNAL_FLAG_NON_PAGED) != 0);

        return STATUS_SUCCESS;
    }

    //
    // The I/O buffer better be mapped (and contiguously at that) or else there
    // is no way to know which pages to lock. Besides, if the buffer is not
    // mapped but filled with physical pages, they are pinned due to the fact
    // that they are not in paged pool! Paged pool is always mapped.
    //

    ASSERT((UnlockedFlags & IO_BUFFER_INTERNAL_FLAG_MAPPED) != 0);
    ASSERT((UnlockedFlags & IO_BUFFER_INTERNAL_FLAG_VA_CONTIGUOUS) != 0);

    //
    // There should only be one fragment on an unlocked I/O buffer.
    //

    ASSERT(UnlockedIoBuffer->FragmentCount == 1);

    BytesLocked = 0;
    ImageSection = NULL;
    PageShift = MmPageShift();
    PageSize = MmPageSize();

    //
    // Determine the total number of physical pages that could need to be
    // locked. The I/O buffer may not be big enough.
    //

    StartAddress = UnlockedIoBuffer->Fragment[0].VirtualAddress;
    EndAddress = StartAddress + UnlockedIoBuffer->Fragment[0].Size;
    PageCount = (ALIGN_POINTER_UP(EndAddress, PageSize) -
                 ALIGN_POINTER_DOWN(StartAddress, PageSize)) >> PageShift;

    //
    // Allocate a new I/O buffer that can handle all the potential fragments in
    // the worst case where none of the physical pages are contiguous.
    //

    AllocationSize = sizeof(IO_BUFFER) +
                     (PageCount * sizeof(IO_BUFFER_FRAGMENT)) +
                     (PageCount * sizeof(PPAGE_CACHE_ENTRY));

    LockedIoBuffer = MmAllocateNonPagedPool(AllocationSize,
                                            MM_IO_ALLOCATION_TAG);

    if (LockedIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LockIoBufferEnd;
    }

    RtlZeroMemory(LockedIoBuffer, AllocationSize);
    LockedIoBuffer->Fragment = (PVOID)LockedIoBuffer + sizeof(IO_BUFFER);
    LockedIoBuffer->Internal.MaxFragmentCount = PageCount;
    LockedIoBuffer->Internal.PageCacheEntryCount = PageCount;
    LockedIoBuffer->Internal.PageCacheEntries = (PVOID)LockedIoBuffer +
                                             sizeof(IO_BUFFER) +
                                             (PageCount *
                                              sizeof(IO_BUFFER_FRAGMENT));

    LockedIoBuffer->Internal.Flags = IO_BUFFER_INTERNAL_FLAG_NON_PAGED;

    //
    // The mappings are not saved if a user mode buffer is being locked. Also
    // get the appropriate process for section lookup.
    //

    if ((UnlockedFlags & IO_BUFFER_INTERNAL_FLAG_USER_MODE) == 0) {
        LockedIoBuffer->Internal.Flags |=
                                         IO_BUFFER_INTERNAL_FLAG_MAPPED |
                                         IO_BUFFER_INTERNAL_FLAG_VA_CONTIGUOUS;

        Process = PsGetKernelProcess();

    } else {
        Process = PsGetCurrentProcess();
    }

    LockedIoBuffer->Internal.CurrentOffset =
                                      UnlockedIoBuffer->Internal.CurrentOffset;

    LockedIoBuffer->Internal.TotalSize = UnlockedIoBuffer->Internal.TotalSize;

    //
    // Make sure the entire buffer is in memory, and lock it down there.
    //

    CurrentAddress = StartAddress;
    Fragment = NULL;
    FragmentIndex = 0;
    PageIndex = 0;
    SectionEnd = NULL;
    while (CurrentAddress < EndAddress) {

        //
        // Attempt to grab the next section if a section boundary was just
        // crossed or there has been no section up to this point. If there
        // is no section, assume the memory is non-paged.
        //

        if (SectionEnd <= CurrentAddress) {
            if (ImageSection != NULL) {
                MmpImageSectionReleaseReference(ImageSection);
                ImageSection = NULL;
            }

            Status = MmpLookupSection(CurrentAddress,
                                      Process->AddressSpace,
                                      &ImageSection,
                                      &PageOffset);

            if (KSUCCESS(Status)) {
                SectionEnd = ImageSection->VirtualAddress + ImageSection->Size;
            }
        }

        //
        // If there is an image section, then page the data in and lock it down
        // at the same time.
        //

        if (ImageSection != NULL) {
            Status = MmpPageIn(ImageSection, PageOffset, &PagedInBuffer);
            if (Status == STATUS_TRY_AGAIN) {
                continue;
            }

            if (!KSUCCESS(Status)) {
                goto LockIoBufferEnd;
            }

            //
            // Get the locked physical address and page cache entry from the
            // returned I/O buffer. Transfer the reference taken on the page
            // cache entry to the new I/O buffer.
            //

            PhysicalAddress = MmGetIoBufferPhysicalAddress(&PagedInBuffer, 0);
            PhysicalAddress += REMAINDER((UINTN)CurrentAddress, PageSize);
            PageCacheEntry = MmGetIoBufferPageCacheEntry(&PagedInBuffer, 0);
            if (PageCacheEntry != NULL) {
                LockedIoBuffer->Internal.Flags |=
                                          IO_BUFFER_INTERNAL_FLAG_CACHE_BACKED;

                LockedIoBuffer->Internal.PageCacheEntries[PageIndex] =
                                                                PageCacheEntry;
            }

        //
        // If there is no image section, then the page better be non-paged and
        // the owner should not release it until this I/O buffer is done using
        // it. There is no way to prevent the owner from called free on the
        // non-paged pool region, for instance, so there is some level of trust
        // here.
        //

        } else {
            PhysicalAddress = MmpVirtualToPhysical(CurrentAddress, NULL);
            if (PhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
                Status = STATUS_INVALID_PARAMETER;
                goto LockIoBufferEnd;
            }
        }

        //
        // Determine the size of this fragment. If this is the beginning of
        // the buffer, then go up to the next page boundary. Clip if that goes
        // beyond the end. This makes sure all fragments are page aligned
        // except for the beginning and end.
        //

        NextAddress = ALIGN_POINTER_UP(CurrentAddress + 1, PageSize);
        if (NextAddress > EndAddress) {
            NextAddress = EndAddress;
        }

        FragmentSize = (UINTN)NextAddress - (UINTN)CurrentAddress;

        ASSERT(FragmentSize != 0);

        //
        // If this buffer is contiguous with the last one, then just up the
        // size of this fragment.
        //

        if ((Fragment != NULL) &&
            ((Fragment->PhysicalAddress + Fragment->Size) == PhysicalAddress)) {

            Fragment->Size += FragmentSize;

        //
        // Otherwise, add a new fragment, but do not fill in the virtual
        // address if the original, unlocked buffer was from user mode.
        //

        } else {
            Fragment = &(LockedIoBuffer->Fragment[FragmentIndex]);
            if ((UnlockedFlags & IO_BUFFER_INTERNAL_FLAG_USER_MODE) == 0) {
                Fragment->VirtualAddress = CurrentAddress;
            }

            Fragment->PhysicalAddress = PhysicalAddress;
            Fragment->Size = FragmentSize;
            LockedIoBuffer->FragmentCount += 1;
            FragmentIndex += 1;
        }

        BytesLocked += FragmentSize;
        CurrentAddress += FragmentSize;
        PageOffset += 1;
        PageIndex += 1;
    }

    Status = STATUS_SUCCESS;

LockIoBufferEnd:
    if (ImageSection != NULL) {
        MmpImageSectionReleaseReference(ImageSection);
    }

    if (BytesLocked != 0) {
        LockedIoBuffer->Internal.Flags |=
                                        IO_BUFFER_INTERNAL_FLAG_MEMORY_LOCKED |
                                        IO_BUFFER_INTERNAL_FLAG_LOCK_OWNED;
    }

    if (!KSUCCESS(Status)) {
        if (LockedIoBuffer != NULL) {
            MmFreeIoBuffer(LockedIoBuffer);
            LockedIoBuffer = NULL;
        }

    } else {
        *IoBuffer = LockedIoBuffer;
    }

    return Status;
}

VOID
MmpSplitIoBufferFragment (
    PIO_BUFFER IoBuffer,
    UINTN FragmentIndex,
    UINTN NewSize
    )

/*++

Routine Description:

    This routine splits a fragment of the given I/O buffer.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    FragmentIndex - Supplies the fragment to split.

    NewSize - Supplies the new size of the given fragment index.

Return Value:

    Status code.

--*/

{

    PIO_BUFFER_FRAGMENT Fragment;
    UINTN Index;

    ASSERT(IoBuffer->Internal.MaxFragmentCount >= IoBuffer->FragmentCount + 1);

    for (Index = IoBuffer->FragmentCount;
         Index > FragmentIndex;
         Index -= 1) {

        RtlCopyMemory(&(IoBuffer->Fragment[Index]),
                      &(IoBuffer->Fragment[Index - 1]),
                      sizeof(IO_BUFFER_FRAGMENT));
    }

    Fragment = &(IoBuffer->Fragment[Index + 1]);
    Fragment->PhysicalAddress += NewSize;
    if (Fragment->VirtualAddress != NULL) {
        Fragment->VirtualAddress += NewSize;
    }

    ASSERT(Fragment->Size > NewSize);

    Fragment->Size -= NewSize;
    Fragment = &(IoBuffer->Fragment[Index]);
    Fragment->Size = NewSize;
    IoBuffer->FragmentCount += 1;
    return;
}

