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

    //
    // The x86 is cache coherent with all observers of memory.
    //

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

    //
    // The x86 is cache coherent with all observers of memory.
    //

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

    //
    // The x86 is cache coherent with all observers of memory.
    //

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

    //
    // The x86 is cache coherent with all observers of memory.
    //

    return STATUS_SUCCESS;
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

    //
    // The x86 is cache coherent with all observers of memory.
    //

    return STATUS_SUCCESS;
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

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

