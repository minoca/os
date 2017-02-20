/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    flush.c

Abstract:

    This module implements cache flushing routines for the memory manager.

Author:

    Evan Green 20-Aug-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "../mmp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
MmpCleanInvalidateCacheRegion (
    PVOID Address,
    UINTN Size
    );

BOOL
MmpInvalidateCacheRegion (
    PVOID Address,
    UINTN Size
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
MmFlushBufferForDataIn (
    PVOID Buffer,
    UINTN SizeInBytes
    )

/*++

Routine Description:

    This routine flushes a buffer in preparation for incoming I/O from a device.

Arguments:

    Buffer - Supplies the virtual address of the buffer to flush. This buffer
        must be cache-line aligned.

    SizeInBytes - Supplies the size of the buffer to flush, in bytes. This
        size must also be cache-line aligned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the region was user mode and an address in the
    region was not valid. Kernel mode addresses are always expected to be
    valid.

--*/

{

    PHYSICAL_ADDRESS PhysicalAddress;
    BOOL Result;

    //
    // Invalidate the data in any second level cache followed by the first
    // level cache.
    //

    PhysicalAddress = MmpVirtualToPhysical(Buffer, NULL);
    ArSerializeExecution();
    Result = MmpInvalidateCacheRegion(Buffer, SizeInBytes);
    if (Result == FALSE) {
        return STATUS_ACCESS_VIOLATION;
    }

    HlFlushCacheRegion(PhysicalAddress, SizeInBytes, HL_CACHE_FLAG_INVALIDATE);
    Result = MmpInvalidateCacheRegion(Buffer, SizeInBytes);
    if (Result == FALSE) {
        return STATUS_ACCESS_VIOLATION;
    }

    return STATUS_SUCCESS;
}

KERNEL_API
KSTATUS
MmFlushBufferForDataOut (
    PVOID Buffer,
    UINTN SizeInBytes
    )

/*++

Routine Description:

    This routine flushes a buffer in preparation for outgoing I/O to a device.

Arguments:

    Buffer - Supplies the virtual address of the buffer to flush. This buffer
        must be cache-line aligned.

    SizeInBytes - Supplies the size of the buffer to flush, in bytes. This
        size must also be cache-line aligned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the region was user mode and an address in the
    region was not valid. Kernel mode addresses are always expected to be
    valid.

--*/

{

    PHYSICAL_ADDRESS PhysicalAddress;
    BOOL Result;

    //
    // Clean the data in the first level cache followed by any second level
    // cache. Since the device is not modifying this data, there's no need to
    // invalidate.
    //

    PhysicalAddress = MmpVirtualToPhysical(Buffer, NULL);
    ArSerializeExecution();
    Result = MmpCleanCacheRegion(Buffer, SizeInBytes);
    if (Result == FALSE) {
        return STATUS_ACCESS_VIOLATION;
    }

    HlFlushCacheRegion(PhysicalAddress, SizeInBytes, HL_CACHE_FLAG_CLEAN);
    return STATUS_SUCCESS;
}

KERNEL_API
KSTATUS
MmFlushBufferForDataIo (
    PVOID Buffer,
    UINTN SizeInBytes
    )

/*++

Routine Description:

    This routine flushes a buffer in preparation for data that is both
    incoming and outgoing (ie the buffer is read from and written to by an
    external device).

Arguments:

    Buffer - Supplies the virtual address of the buffer to flush. This buffer
        must be cache-line aligned.

    SizeInBytes - Supplies the size of the buffer to flush, in bytes. This
        size must also be cache-line aligned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the region was user mode and an address in the
    region was not valid. Kernel mode addresses are always expected to be
    valid.

--*/

{

    ULONG Flags;
    PHYSICAL_ADDRESS PhysicalAddress;
    BOOL Result;

    //
    // Data is both going out to the device and coming in from the device, so
    // clean and then invalidate the cache region. Start with a first level
    // clean, then a clean and invalidate at any second level cache, and
    // complete with a clean and invalidate of the first level cache.
    //

    Flags = HL_CACHE_FLAG_CLEAN | HL_CACHE_FLAG_INVALIDATE;
    PhysicalAddress = MmpVirtualToPhysical(Buffer, NULL);
    ArSerializeExecution();
    Result = MmpCleanCacheRegion(Buffer, SizeInBytes);
    if (Result == FALSE) {
        return STATUS_ACCESS_VIOLATION;
    }

    HlFlushCacheRegion(PhysicalAddress, SizeInBytes, Flags);
    Result = MmpInvalidateCacheRegion(Buffer, SizeInBytes);
    if (Result == FALSE) {
        return STATUS_ACCESS_VIOLATION;
    }

    return STATUS_SUCCESS;
}

KERNEL_API
KSTATUS
MmSyncCacheRegion (
    PVOID Address,
    UINTN Size
    )

/*++

Routine Description:

    This routine unifies the instruction and data caches for the given region,
    probably after a region of executable code was modified. This does not
    necessarily flush data to the point where it's observable to device DMA
    (called the point of coherency).

Arguments:

    Address - Supplies the address to flush.

    Size - Supplies the number of bytes in the region to flush.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if one of the addresses in the given range was not
    valid.

--*/

{

    PVOID AlignedAddress;
    ULONG DataLineSize;
    BOOL Result;
    KSTATUS Status;

    //
    // Clean the data cache, then clean the instruction cache. Ensure each
    // page is mapped before touching it.
    //

    DataLineSize = MmDataCacheLineSize;
    AlignedAddress = ALIGN_POINTER_DOWN(Address, DataLineSize);
    Size += REMAINDER((UINTN)Address, DataLineSize);
    Size = ALIGN_RANGE_UP(Size, DataLineSize);
    Status = STATUS_SUCCESS;
    ArSerializeExecution();
    Result = MmpCleanCacheRegion(AlignedAddress, Size);
    Result &= MmpInvalidateInstructionCacheRegion(AlignedAddress, Size);
    if (Result == FALSE) {
        Status = STATUS_ACCESS_VIOLATION;
    }

    ArSerializeExecution();
    return Status;
}

INTN
MmSysFlushCache (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine responds to system calls from user mode requesting to
    invalidate the instruction cache after changing a code region.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PVOID Address;
    PSYSTEM_CALL_FLUSH_CACHE Parameters;
    UINTN Size;

    Parameters = SystemCallParameter;
    Address = Parameters->Address;
    Size = Parameters->Size;
    if (Address >= KERNEL_VA_START) {
        Address = KERNEL_VA_START - 1;
    }

    if ((Address + Size > KERNEL_VA_START) || (Address + Size < Address)) {
        Size = KERNEL_VA_START - Address;
    }

    return MmSyncCacheRegion(Address, Size);
}

VOID
MmpSyncSwapPage (
    PVOID SwapPage,
    ULONG PageSize
    )

/*++

Routine Description:

    This routine cleans the data cache but does not invalidate the instruction
    cache for the given kernel region. It is used by the paging code for a
    temporary mapping that is going to get marked executable, but this mapping
    itself does not need an instruction cache flush.

Arguments:

    SwapPage - Supplies a pointer to the swap page.

    PageSize - Supplies the size of a page.

Return Value:

    None.

--*/

{

    BOOL Result;

    //
    // Make sure all the previous writes have finished.
    //

    ArSerializeExecution();
    Result = MmpCleanCacheRegion(SwapPage, PageSize);

    ASSERT(Result != FALSE);

    ArSerializeExecution();
    return;
}

BOOL
MmpInvalidateInstructionCacheRegion (
    PVOID Address,
    ULONG Size
    )

/*++

Routine Description:

    This routine invalidates the given region of virtual address space in the
    instruction cache.

Arguments:

    Address - Supplies the virtual address of the region to invalidate.

    Size - Supplies the number of bytes to invalidate.

Return Value:

    TRUE on success.

    FALSE if one of the addresses in the region caused a bad page fault.

--*/

{

    ULONG CacheLineSize;
    PVOID CurrentAddress;
    BOOL Result;

    Result = TRUE;
    CacheLineSize = MmInstructionCacheLineSize;
    CurrentAddress = ALIGN_POINTER_DOWN(Address, CacheLineSize);
    Size += REMAINDER((UINTN)Address, CacheLineSize);
    Size = ALIGN_RANGE_UP(Size, CacheLineSize);
    while (Size != 0) {
        Result &= MmpInvalidateInstructionCacheLine(CurrentAddress);

        ASSERT((Result != FALSE) ||
               ((Address < KERNEL_VA_START) &&
                (Address + Size <= KERNEL_VA_START)));

        CurrentAddress += CacheLineSize;
        Size -= CacheLineSize;
    }

    return Result;
}

BOOL
MmpCleanCacheRegion (
    PVOID Address,
    UINTN Size
    )

/*++

Routine Description:

    This routine cleans the given region of virtual address space in the first
    level data cache.

Arguments:

    Address - Supplies the virtual address of the region to clean.

    Size - Supplies the number of bytes to clean.

Return Value:

    TRUE on success.

    FALSE if one of the addresses in the region caused a bad page fault.

--*/

{

    ULONG CacheLineSize;
    BOOL Result;

    Result = TRUE;
    CacheLineSize = MmDataCacheLineSize;
    if (CacheLineSize == 0) {
        return Result;
    }

    //
    // It is not possible to flush half a cache line. Being asked to do so is
    // definitely trouble (as it could be the boundary of two distinct I/O
    // buffers.
    //

    ASSERT(ALIGN_RANGE_DOWN(Size, CacheLineSize) == Size);
    ASSERT(ALIGN_RANGE_DOWN((UINTN)Address, CacheLineSize) == (UINTN)Address);

    while (Size != 0) {
        Result &= MmpCleanCacheLine(Address);

        ASSERT((Result != FALSE) ||
               ((Address < KERNEL_VA_START) &&
                (Address + Size <= KERNEL_VA_START)));

        Address += CacheLineSize;
        Size -= CacheLineSize;
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
MmpCleanInvalidateCacheRegion (
    PVOID Address,
    UINTN Size
    )

/*++

Routine Description:

    This routine cleans and invalidates the given region of virtual address
    space in the first level data cache.

Arguments:

    Address - Supplies the virtual address of the region to clean.

    Size - Supplies the number of bytes to clean.

Return Value:

    TRUE on success.

    FALSE if one of the addresses in the region caused a bad page fault.

--*/

{

    ULONG CacheLineSize;
    BOOL Result;

    Result = TRUE;
    CacheLineSize = MmDataCacheLineSize;
    if (CacheLineSize == 0) {
        return Result;
    }

    //
    // It is not possible to flush half a cache line. Being asked to do so is
    // definitely trouble (as it could be the boundary of two distinct I/O
    // buffers.
    //

    ASSERT(ALIGN_RANGE_DOWN(Size, CacheLineSize) == Size);
    ASSERT(ALIGN_RANGE_DOWN((UINTN)Address, CacheLineSize) == (UINTN)Address);

    while (Size != 0) {
        Result &= MmpCleanInvalidateCacheLine(Address);

        ASSERT((Result != FALSE) ||
               ((Address < KERNEL_VA_START) &&
                (Address + Size <= KERNEL_VA_START)));

        Address += CacheLineSize;
        Size -= CacheLineSize;
    }

    return Result;
}

BOOL
MmpInvalidateCacheRegion (
    PVOID Address,
    UINTN Size
    )

/*++

Routine Description:

    This routine invalidates the region of virtual address space in the first
    level data cache. This routine is very dangerous, as any dirty data in the
    cache will be lost and gone.

Arguments:

    Address - Supplies the virtual address of the region to clean.

    Size - Supplies the number of bytes to clean.

Return Value:

    TRUE on success.

    FALSE if one of the addresses in the region caused a bad page fault.

--*/

{

    ULONG CacheLineSize;
    BOOL Result;

    Result = TRUE;
    CacheLineSize = MmDataCacheLineSize;
    if (CacheLineSize == 0) {
        return Result;
    }

    //
    // It is not possible to flush half a cache line. Being asked to do so is
    // definitely trouble (as it could be the boundary of two distinct I/O
    // buffers.
    //

    ASSERT(ALIGN_RANGE_DOWN(Size, CacheLineSize) == Size);
    ASSERT(ALIGN_RANGE_DOWN((UINTN)Address, CacheLineSize) == (UINTN)Address);

    while (Size != 0) {
        Result &= MmpInvalidateCacheLine(Address);

        ASSERT((Result != FALSE) ||
               ((Address < KERNEL_VA_START) &&
                (Address + Size <= KERNEL_VA_START)));

        Address += CacheLineSize;
        Size -= CacheLineSize;
    }

    return Result;
}

