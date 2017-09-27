/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    virtual.c

Abstract:

    This module implements virtual memory accounting in the kernel.

Author:

    Evan Green 1-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include "mmp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define DESCRIPTOR_REFILL_PAGE_COUNT 16

//
// Define the system virtual memory warning levels, in bytes, for systems with
// a small amount of virtual memory (i.e. <= 4GB).
//

#define MM_SMALL_VIRTUAL_MEMORY_WARNING_LEVEL_1_TRIGGER (512 * _1MB)
#define MM_SMALL_VIRTUAL_MEMORY_WARNING_LEVEL_1_RETREAT (768 * _1MB)

//
// Define the system virtual memory warning levels, in bytes, for systems with
// a large amount of virtual memory (e.g. 64-bit systems).
//

#define MM_LARGE_VIRTUAL_MEMORY_WARNING_LEVEL_1_TRIGGER (1 * (UINTN)_1GB)
#define MM_LARGE_VIRTUAL_MEMORY_WARNING_LEVEL_1_RETREAT (2 * (UINTN)_1GB)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the iteration context when initializing the kernel
    address space.

Members:

    Status - Stores the resulting status code.

--*/

typedef struct _INITIALIZE_KERNEL_VA_CONTEXT {
    KSTATUS Status;
} INITIALIZE_KERNEL_VA_CONTEXT, *PINITIALIZE_KERNEL_VA_CONTEXT;

/*++

Structure Description:

    This structure defines the iteration context when cloning the memory map
    of an address space.

Members:

    Accounting - Stores the destination of the clone operation.

    Status - Stores the resulting status code.

--*/

typedef struct _CLONE_ADDRESS_SPACE_CONTEXT {
    PMEMORY_ACCOUNTING Accounting;
    KSTATUS Status;
} CLONE_ADDRESS_SPACE_CONTEXT, *PCLONE_ADDRESS_SPACE_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
MmpPrepareToAddAccountingDescriptor (
    PMEMORY_ACCOUNTING Accountant,
    UINTN NewAllocations
    );

VOID
MmpUpdateVirtualMemoryWarningLevel (
    VOID
    );

VOID
MmpInitializeKernelVaIterator (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    );

VOID
MmpCloneAddressSpaceIterator (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Stores information about which kernel VA space is occupied and which is free.
//

MEMORY_ACCOUNTING MmKernelVirtualSpace;

//
// Stores the kernel address of the user shared data.
//

PUSER_SHARED_DATA MmUserSharedData;

//
// Stores the event used to signal a virtual memory notification when there is
// a significant change in the amount of allocated virtual memory.
//

PKEVENT MmVirtualMemoryWarningEvent;

//
// Stores the current virtual memory warning level.
//

MEMORY_WARNING_LEVEL MmVirtualMemoryWarningLevel;

//
// Stores the number of bytes for each warning level's threshold.
//

UINTN MmVirtualMemoryWarningLevel1Retreat;
UINTN MmVirtualMemoryWarningLevel1Trigger;

//
// Store the number of free virtual pages. This is defined as a global rather
// than simply using the MDL's free space indicator so as not to produce
// strange transient results while the MDL is being operated on.
//

UINTN MmFreeVirtualByteCount;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
PVOID
MmGetVirtualMemoryWarningEvent (
    VOID
    )

/*++

Routine Description:

    This routine returns the memory manager's system virtual memory warning
    event. This event is signaled whenever there is a change in system virtual
    memory's warning level.

Arguments:

    None.

Return Value:

    Returns a pointer to the virutal memory warning event.

--*/

{

    ASSERT(MmVirtualMemoryWarningEvent != NULL);

    return MmVirtualMemoryWarningEvent;
}

KERNEL_API
MEMORY_WARNING_LEVEL
MmGetVirtualMemoryWarningLevel (
    VOID
    )

/*++

Routine Description:

    This routine returns the current system virtual memory warning level.

Arguments:

    None.

Return Value:

    Returns the current virtual memory warning level.

--*/

{

    return MmVirtualMemoryWarningLevel;
}

UINTN
MmGetTotalVirtualMemory (
    VOID
    )

/*++

Routine Description:

    This routine returns the size of the kernel virtual address space, in bytes.

Arguments:

    None.

Return Value:

    Returns the total number of bytes in the kernel virtual address space.

--*/

{

    return (UINTN)MmKernelVirtualSpace.Mdl.TotalSpace;
}

UINTN
MmGetFreeVirtualMemory (
    VOID
    )

/*++

Routine Description:

    This routine returns the number of unallocated bytes in the kernel virtual
    address space.

Arguments:

    None.

Return Value:

    Returns the total amount of free kernel virtual memory, in bytes.

--*/

{

    return MmFreeVirtualByteCount;
}

KERNEL_API
PVOID
MmMapPhysicalAddress (
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN SizeInBytes,
    BOOL Writable,
    BOOL WriteThrough,
    BOOL CacheDisabled
    )

/*++

Routine Description:

    This routine maps a physical address into kernel VA space. It is meant so
    that system components can access memory mapped hardware.

Arguments:

    PhysicalAddress - Supplies a pointer to the physical address.

    SizeInBytes - Supplies the size in bytes of the mapping. This will be
        rounded up to the nearest page size.

    Writable - Supplies a boolean indicating if the memory is to be marked
        writable (TRUE) or read-only (FALSE).

    WriteThrough - Supplies a boolean indicating if the memory is to be marked
        write-through (TRUE) or write-back (FALSE).

    CacheDisabled - Supplies a boolean indicating if the memory is to be mapped
        uncached.

Return Value:

    Returns a pointer to the virtual address of the mapping on success, or
    NULL on failure.

--*/

{

    ULONG PageOffset;
    ULONG PageSize;
    PVOID VirtualAddress;

    PageSize = MmPageSize();
    PageOffset = PhysicalAddress - ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
    VirtualAddress = MmpMapPhysicalAddress(PhysicalAddress - PageOffset,
                                           SizeInBytes + PageOffset,
                                           Writable,
                                           WriteThrough,
                                           CacheDisabled,
                                           MemoryTypeHardware);

    return VirtualAddress + PageOffset;
}

KERNEL_API
VOID
MmUnmapAddress (
    PVOID VirtualAddress,
    UINTN SizeInBytes
    )

/*++

Routine Description:

    This routine unmaps memory mapped with MmMapPhysicalMemory.

Arguments:

    VirtualAddress - Supplies the virtual address to unmap.

    SizeInBytes - Supplies the number of bytes to unmap.

Return Value:

    None.

--*/

{

    ULONG PageOffset;
    ULONG PageSize;

    PageSize = MmPageSize();
    PageOffset = REMAINDER((UINTN)VirtualAddress, PageSize);
    VirtualAddress = ALIGN_POINTER_DOWN(VirtualAddress, PageSize);
    SizeInBytes = ALIGN_RANGE_UP(SizeInBytes + PageOffset, PageSize);
    MmpFreeAccountingRange(NULL,
                           VirtualAddress,
                           SizeInBytes,
                           FALSE,
                           UNMAP_FLAG_SEND_INVALIDATE_IPI);

    return;
}

KSTATUS
MmCreateCopyOfUserModeString (
    PCSTR UserModeString,
    ULONG UserModeStringBufferLength,
    ULONG AllocationTag,
    PSTR *CreatedCopy
    )

/*++

Routine Description:

    This routine is a convenience method that captures a string from user mode
    and creates a paged-pool copy in kernel mode. The caller can be sure that
    the string pointer was properly sanitized and the resulting buffer is null
    terminated. The caller is responsible for freeing the memory returned by
    this function on success.

Arguments:

    UserModeString - Supplies the user mode pointer to the string.

    UserModeStringBufferLength - Supplies the size of the buffer containing the
        user mode string.

    AllocationTag - Supplies the allocation tag that should be used when
        creating the kernel buffer.

    CreatedCopy - Supplies a pointer where the paged pool allocation will be
        returned.

Return Value:

    Status code.

--*/

{

    PSTR Copy;
    KSTATUS Status;

    Copy = NULL;
    if ((UserModeString == NULL) || (UserModeStringBufferLength == 0)) {
        Status = STATUS_INVALID_PARAMETER;
        goto CreateCopyOfUserModeStringEnd;
    }

    //
    // Allocate the new buffer.
    //

    UserModeStringBufferLength += 1;
    Copy = MmAllocatePagedPool(UserModeStringBufferLength, AllocationTag);
    if (Copy == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateCopyOfUserModeStringEnd;
    }

    //
    // Copy the string from user mode.
    //

    Status = MmCopyFromUserMode(Copy,
                                UserModeString,
                                UserModeStringBufferLength);

    if (!KSUCCESS(Status)) {
        goto CreateCopyOfUserModeStringEnd;
    }

    //
    // Explicitly null terminate the buffer.
    //

    Copy[UserModeStringBufferLength - 1] = '\0';

CreateCopyOfUserModeStringEnd:
    if (!KSUCCESS(Status)) {
        if (Copy != NULL) {
            MmFreePagedPool(Copy);
            Copy = NULL;
        }
    }

    *CreatedCopy = Copy;
    return Status;
}

KERNEL_API
KSTATUS
MmCopyFromUserMode (
    PVOID KernelModePointer,
    PCVOID UserModePointer,
    UINTN Size
    )

/*++

Routine Description:

    This routine copies memory from user mode to kernel mode.

Arguments:

    KernelModePointer - Supplies the kernel mode pointer, the destination of
        the copy.

    UserModePointer - Supplies the untrusted user mode pointer, the source of
        the copy.

    Size - Supplies the number of bytes to copy.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the user mode memory is invalid or corrupt.

--*/

{

    BOOL Result;

    if ((UserModePointer >= USER_VA_END) ||
        (UserModePointer + Size > USER_VA_END) ||
        (UserModePointer + Size <= UserModePointer)) {

        return STATUS_ACCESS_VIOLATION;
    }

    Result = MmpCopyUserModeMemory(KernelModePointer, UserModePointer, Size);
    if (Result != FALSE) {
        return STATUS_SUCCESS;
    }

    return STATUS_ACCESS_VIOLATION;
}

KERNEL_API
KSTATUS
MmCopyToUserMode (
    PVOID UserModePointer,
    PCVOID KernelModePointer,
    UINTN Size
    )

/*++

Routine Description:

    This routine copies memory to user mode from kernel mode.

Arguments:

    UserModePointer - Supplies the untrusted user mode pointer, the destination
        of the copy.

    KernelModePointer - Supplies the kernel mode pointer, the source of the
        copy.

    Size - Supplies the number of bytes to copy.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the user mode memory is invalid or corrupt.

--*/

{

    BOOL Result;

    if ((UserModePointer >= USER_VA_END) ||
        (UserModePointer + Size > USER_VA_END) ||
        (UserModePointer + Size <= UserModePointer)) {

        return STATUS_ACCESS_VIOLATION;
    }

    Result = MmpCopyUserModeMemory(UserModePointer, KernelModePointer, Size);
    if (Result != FALSE) {
        return STATUS_SUCCESS;
    }

    return STATUS_ACCESS_VIOLATION;
}

KERNEL_API
KSTATUS
MmTouchUserModeBuffer (
    PVOID Buffer,
    UINTN Size,
    BOOL Write
    )

/*++

Routine Description:

    This routine touches a user mode buffer, validating it either for reading
    or writing. Note that the caller must also have the process VA space
    locked, or else this data is immediately stale.

Arguments:

    Buffer - Supplies a pointer to the buffer to probe.

    Size - Supplies the number of bytes to copy.

    Write - Supplies a boolean indicating whether to probe the memory for
        reading or writing.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the user mode memory is invalid.

--*/

{

    BOOL Result;

    if ((Buffer >= USER_VA_END) ||
        (Buffer + Size > USER_VA_END) ||
        (Buffer + Size <= Buffer)) {

        return STATUS_ACCESS_VIOLATION;
    }

    if (Write != FALSE) {
        Result = MmpTouchUserModeMemoryForWrite(Buffer, Size);

    } else {
        Result = MmpTouchUserModeMemoryForRead(Buffer, Size);
    }

    if (Result != FALSE) {
        return STATUS_SUCCESS;
    }

    return STATUS_ACCESS_VIOLATION;
}

VOID
MmLockProcessAddressSpace (
    VOID
    )

/*++

Routine Description:

    This routine acquires a shared lock on the process address space to ensure
    that user mode cannot change the virtual address map while the kernel is
    using a region.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PKPROCESS Process;

    Process = PsGetCurrentProcess();
    MmpLockAccountant(Process->AddressSpace->Accountant, FALSE);
    return;
}

VOID
MmUnlockProcessAddressSpace (
    VOID
    )

/*++

Routine Description:

    This routine unlocks the current process address space, allowing changes
    to be made once again.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PKPROCESS Process;

    Process = PsGetCurrentProcess();
    MmpUnlockAccountant(Process->AddressSpace->Accountant, FALSE);
    return;
}

PMEMORY_RESERVATION
MmCreateMemoryReservation (
    PVOID PreferredVirtualAddress,
    UINTN Size,
    PVOID Min,
    PVOID Max,
    ALLOCATION_STRATEGY FallbackStrategy,
    BOOL KernelMode
    )

/*++

Routine Description:

    This routine creates a virtual address reservation for the current process.

Arguments:

    PreferredVirtualAddress - Supplies the preferred virtual address of the
        reservation. Supply NULL to indicate no preference.

    Size - Supplies the size of the requested reservation, in bytes.

    Min - Supplies the minimum virtual address to allocate.

    Max - Supplies the maximum virtual address to allocate.

    FallbackStrategy - Supplies the fallback memory allocation strategy in
        case the preferred address isn't available (or wasn't supplied).

    KernelMode - Supplies a boolean indicating whether the VA reservation must
        be in kernel mode (TRUE) or user mode (FALSE).

Return Value:

    Returns a pointer to the reservation structure on success.

    NULL on failure.

--*/

{

    PMEMORY_ACCOUNTING Accountant;
    UINTN AlignedSize;
    PKPROCESS KernelProcess;
    ULONG PageSize;
    PKPROCESS Process;
    PMEMORY_RESERVATION Reservation;
    KSTATUS Status;
    VM_ALLOCATION_PARAMETERS VaRequest;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KernelProcess = PsGetKernelProcess();
    PageSize = MmPageSize();
    Process = PsGetCurrentProcess();
    Reservation = NULL;
    AlignedSize = ALIGN_RANGE_UP(Size, PageSize);
    if (AlignedSize == 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto CreateMemoryReservationEnd;
    }

    if (KernelMode != FALSE) {
        Process = KernelProcess;
        Accountant = &MmKernelVirtualSpace;

        //
        // If the caller specified kernel mode and a usermode preferred address,
        // pretend like the preference didn't happen.
        //

        if ((PreferredVirtualAddress != NULL) &&
            (PreferredVirtualAddress < KERNEL_VA_START)) {

            PreferredVirtualAddress = NULL;
        }

    } else {

        //
        // It is not valid to be running in the kernel process and requesting
        // user space.
        //

        if (Process == KernelProcess) {
            Status = STATUS_INVALID_PARAMETER;
            goto CreateMemoryReservationEnd;
        }

        Accountant = Process->AddressSpace->Accountant;
    }

    //
    // Allocate space for the reservation.
    //

    if (Process == KernelProcess) {
        Reservation = MmAllocateNonPagedPool(sizeof(MEMORY_RESERVATION),
                                             MM_ALLOCATION_TAG);

    } else {
        Reservation = MmAllocatePagedPool(sizeof(MEMORY_RESERVATION),
                                          MM_ALLOCATION_TAG);
    }

    if (Reservation == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateMemoryReservationEnd;
    }

    //
    // If there was a preferred address, attempt to allocate it.
    //

    VaRequest.Address = NULL;
    VaRequest.Size = AlignedSize;
    VaRequest.Alignment = PageSize;
    VaRequest.Min = Min;
    VaRequest.Max = Max;
    VaRequest.MemoryType = MemoryTypeReserved;
    if (PreferredVirtualAddress != NULL) {
        VaRequest.Strategy = AllocationStrategyFixedAddress;
        VaRequest.Address = PreferredVirtualAddress;
        Status = MmpAllocateAddressRange(Accountant, &VaRequest, FALSE);

        //
        // If the range was successfully allocated, fill out the reservation
        // and return.
        //

        if (KSUCCESS(Status)) {

            ASSERT(VaRequest.Address == PreferredVirtualAddress);

            Reservation->Process = Process;
            Reservation->VirtualBase = PreferredVirtualAddress;
            Reservation->Size = AlignedSize;
            goto CreateMemoryReservationEnd;
        }
    }

    VaRequest.Address = NULL;
    VaRequest.Strategy = FallbackStrategy;

    //
    // Either there was no preferred address, or the attempt to allocate at
    // that preferred address failed. Allocate anywhere.
    //

    Status = MmpAllocateAddressRange(Accountant, &VaRequest, FALSE);
    if (!KSUCCESS(Status)) {
        goto CreateMemoryReservationEnd;
    }

    Reservation->Process = Process;
    Reservation->VirtualBase = VaRequest.Address;
    Reservation->Size = AlignedSize;
    Status = STATUS_SUCCESS;

CreateMemoryReservationEnd:
    if (!KSUCCESS(Status)) {
        if (Reservation != NULL) {
            if (Process == KernelProcess) {
                MmFreeNonPagedPool(Reservation);

            } else {
                MmFreePagedPool(Reservation);
            }

            Reservation = NULL;
        }
    }

    return Reservation;
}

VOID
MmFreeMemoryReservation (
    PMEMORY_RESERVATION Reservation
    )

/*++

Routine Description:

    This routine destroys a memory reservation. All memory must be unmapped
    and freed prior to this call.

Arguments:

    Reservation - Supplies a pointer to the reservation structure returned when
        the reservation was made.

Return Value:

    None.

--*/

{

    PKPROCESS Process;
    KSTATUS Status;
    ULONG UnmapFlags;

    ASSERT(Reservation != NULL);

    Process = (PKPROCESS)(Reservation->Process);
    UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                 UNMAP_FLAG_SEND_INVALIDATE_IPI;

    Status = MmpFreeAccountingRange(Process->AddressSpace,
                                    Reservation->VirtualBase,
                                    Reservation->Size,
                                    FALSE,
                                    UnmapFlags);

    ASSERT(KSUCCESS(Status));

    if (Process == PsGetKernelProcess()) {
        MmFreeNonPagedPool(Reservation);

    } else {
        MmFreePagedPool(Reservation);
    }

    return;
}

KSTATUS
MmInitializeMemoryAccounting (
    PMEMORY_ACCOUNTING Accountant,
    ULONG Flags
    )

/*++

Routine Description:

    This routine initializes a memory accounting structure.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure to
        initialize.

    Flags - Supplies flags to control the behavior of the accounting. See the
        MEMORY_ACCOUNTING_FLAG_* definitions for valid flags.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid flag was passed.

--*/

{

    MDL_ALLOCATION_SOURCE Source;
    KSTATUS Status;

    ASSERT((Flags & ~MEMORY_ACCOUNTING_FLAG_MASK) == 0);

    if ((Flags & MEMORY_ACCOUNTING_FLAG_SYSTEM) != 0) {
        Source = MdlAllocationSourceNone;

    } else {
        Source = MdlAllocationSourcePagedPool;
    }

    Accountant->Flags = Flags;

    //
    // If the system accountant is initializing, then it is too early to
    // create objects. Skip it for now, once the object manager is online the
    // queued lock will be created.
    //

    Accountant->Lock = NULL;
    if ((Flags & MEMORY_ACCOUNTING_FLAG_SYSTEM) == 0) {
        Accountant->Lock = KeCreateSharedExclusiveLock();
        if (Accountant->Lock == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    MmMdInitDescriptorList(&(Accountant->Mdl), Source);

    //
    // Create the free range of user space.
    //

    if ((Flags & MEMORY_ACCOUNTING_FLAG_SYSTEM) == 0) {
        Status = MmReinitializeUserAccounting(Accountant);
        if (!KSUCCESS(Status)) {
            goto InitializeMemoryAccountingEnd;
        }
    }

    Accountant->Flags |= MEMORY_ACCOUNTING_FLAG_INITIALIZED;
    Status = STATUS_SUCCESS;

InitializeMemoryAccountingEnd:
    return Status;
}

KSTATUS
MmReinitializeUserAccounting (
    PMEMORY_ACCOUNTING Accountant
    )

/*++

Routine Description:

    This routine resets the memory reservations on a user memory accounting
    structure to those of a clean process.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure to
        initialize.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid flag was passed.

--*/

{

    MEMORY_DESCRIPTOR FreeRange;
    ULONG PageSize;

    PageSize = MmPageSize();

    ASSERT((Accountant->Flags & MEMORY_ACCOUNTING_FLAG_SYSTEM) == 0);

    MmMdInitDescriptor(&FreeRange,
                       PageSize,
                       (UINTN)USER_VA_END,
                       MemoryTypeFree);

    return MmpAddAccountingDescriptor(Accountant, &FreeRange);
}

VOID
MmDestroyMemoryAccounting (
    PMEMORY_ACCOUNTING Accountant
    )

/*++

Routine Description:

    This routine destroys a memory accounting structure, freeing all memory
    associated with it (except the MEMORY_ACCOUNTING structure itself, which
    was provided to the initialize function separately).

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure to
        destroy.

Return Value:

    None.

--*/

{

    if ((Accountant->Flags & MEMORY_ACCOUNTING_FLAG_INITIALIZED) == 0) {
        return;
    }

    MmMdDestroyDescriptorList(&(Accountant->Mdl));
    KeDestroySharedExclusiveLock(Accountant->Lock);
    Accountant->Lock = NULL;
    Accountant->Flags = 0;
    return;
}

KSTATUS
MmCloneAddressSpace (
    PADDRESS_SPACE Source,
    PADDRESS_SPACE Destination
    )

/*++

Routine Description:

    This routine makes a clone of one process' entire address space into
    another process. The copy is not shared memory, the destination segments
    are marked copy on write. This includes copying the mapping for the user
    shared data page.

Arguments:

    Source - Supplies a pointer to the source address space to copy.

    Destination - Supplies a pointer to the newly created destination to copy
        the sections to.

Return Value:

    Status code.

--*/

{

    CLONE_ADDRESS_SPACE_CONTEXT Context;
    PLIST_ENTRY CurrentEntry;
    ULONG Flags;
    PHYSICAL_ADDRESS PhysicalAddress;
    PIMAGE_SECTION SourceSection;
    KSTATUS Status;

    //
    // This routine must be called at low level, and neither process can be
    // the kernel process, one because that would make no sense, and two
    // because then page faults couldn't be serviced while the locks acquired
    // in this function are held.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((Source != MmKernelAddressSpace) &&
           (Destination != MmKernelAddressSpace));

    //
    // Grab both the account lock and the process lock so that neither image
    // sections nor address space reservations can be changed during the copy.
    //

    MmpLockAccountant(Source->Accountant, FALSE);
    MmpLockAccountant(Destination->Accountant, TRUE);
    MmAcquireAddressSpaceLock(Source);
    Destination->MaxMemoryMap = Source->MaxMemoryMap;
    Destination->BreakStart = Source->BreakStart;
    Destination->BreakEnd = Source->BreakEnd;

    //
    // Preallocate all the page tables in the destination process so that
    // allocations don't occur while holding the image section lock.
    //

    Status = MmpPreallocatePageTables(Source, Destination);
    if (!KSUCCESS(Status)) {
        goto CloneProcessAddressSpaceEnd;
    }

    //
    // Create a copy of every image section in the process.
    //

    CurrentEntry = Source->SectionListHead.Next;
    while (CurrentEntry != &(Source->SectionListHead)) {
        SourceSection = LIST_VALUE(CurrentEntry,
                                   IMAGE_SECTION,
                                   AddressListEntry);

        CurrentEntry = CurrentEntry->Next;
        Status = MmpCopyImageSection(SourceSection, Destination);
        if (!KSUCCESS(Status)) {
            goto CloneProcessAddressSpaceEnd;
        }
    }

    //
    // Invalidate the entire TLB as all the source process's writable image
    // sections were converted to read-only image sections.
    //

    ArInvalidateEntireTlb();

    //
    // Map the user shared data page. The accounting descriptor will get copied
    // in the next step.
    //

    Flags = MAP_FLAG_PRESENT | MAP_FLAG_USER_MODE | MAP_FLAG_READ_ONLY;
    PhysicalAddress = MmpVirtualToPhysical(MmUserSharedData, NULL);
    MmpMapPageInOtherProcess(Destination,
                             PhysicalAddress,
                             USER_SHARED_DATA_USER_ADDRESS,
                             Flags,
                             FALSE);

    //
    // Copy the memory accounting descriptors.
    //

    Context.Accounting = Destination->Accountant;
    Context.Status = STATUS_SUCCESS;
    MmMdIterate(&(Source->Accountant->Mdl),
                MmpCloneAddressSpaceIterator,
                &Context);

    if (!KSUCCESS(Context.Status)) {
        Status = Context.Status;
        goto CloneProcessAddressSpaceEnd;
    }

    Status = STATUS_SUCCESS;

CloneProcessAddressSpaceEnd:
    MmpUnlockAccountant(Destination->Accountant, TRUE);
    MmpUnlockAccountant(Source->Accountant, FALSE);
    MmReleaseAddressSpaceLock(Source);
    return Status;
}

KSTATUS
MmMapUserSharedData (
    PADDRESS_SPACE AddressSpace
    )

/*++

Routine Description:

    This routine maps the user shared data at a fixed address in a new
    process' address space.

Arguments:

    AddressSpace - Supplies the address space to map the user shared data page
        into.

Return Value:

    Status code.

--*/

{

    PMEMORY_ACCOUNTING Accountant;
    PKPROCESS CurrentProcess;
    ULONG Flags;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    UINTN RangeEnd;
    UINTN RangeStart;
    KSTATUS Status;
    MEMORY_DESCRIPTOR UserSharedDataRange;

    //
    // Reserve the fixed virtual address for the user shared data page,
    // updating the memory accounting for the current process.
    //

    PageSize = MmPageSize();
    RangeStart = (UINTN)USER_SHARED_DATA_USER_ADDRESS;
    RangeEnd = RangeStart + PageSize;

    ASSERT(sizeof(USER_SHARED_DATA) <= PageSize);

    MmMdInitDescriptor(&UserSharedDataRange,
                       RangeStart,
                       RangeEnd,
                       MemoryTypeReserved);

    CurrentProcess = PsGetCurrentProcess();
    if (AddressSpace == NULL) {
        AddressSpace = CurrentProcess->AddressSpace;
    }

    ASSERT(AddressSpace != MmKernelAddressSpace);

    Accountant = AddressSpace->Accountant;
    Status = MmpAddAccountingDescriptor(Accountant, &UserSharedDataRange);
    if (!KSUCCESS(Status)) {
        goto MapUserSharedDataEnd;
    }

    //
    // Read-only map the user shared data page at the fixed user mode address.
    //

    Flags = MAP_FLAG_PRESENT | MAP_FLAG_USER_MODE | MAP_FLAG_READ_ONLY;
    PhysicalAddress = MmpVirtualToPhysical(MmUserSharedData, NULL);
    if (AddressSpace == CurrentProcess->AddressSpace) {
        if (MmpVirtualToPhysical((PVOID)USER_SHARED_DATA_USER_ADDRESS, NULL) ==
            INVALID_PHYSICAL_ADDRESS) {

            MmpMapPage(PhysicalAddress, USER_SHARED_DATA_USER_ADDRESS, Flags);
        }

    } else {
        MmpMapPageInOtherProcess(AddressSpace,
                                 PhysicalAddress,
                                 USER_SHARED_DATA_USER_ADDRESS,
                                 Flags,
                                 FALSE);
    }

MapUserSharedDataEnd:
    return Status;
}

PVOID
MmGetUserSharedData (
    VOID
    )

/*++

Routine Description:

    This routine returns the kernel virtual address of the user shared data
    area.

Arguments:

    None.

Return Value:

    The kernel mode address of the user shared data page.

--*/

{

    return MmUserSharedData;
}

KSTATUS
MmpAddAccountingDescriptor (
    PMEMORY_ACCOUNTING Accountant,
    PMEMORY_DESCRIPTOR Descriptor
    )

/*++

Routine Description:

    This routine adds the given descriptor to the accounting information. The
    caller must he holding the accounting lock.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure.

    Descriptor - Supplies a pointer to the descriptor to add. Note that the
        descriptor being passed in does not have to be permanent. A copy of the
        descriptor will be made.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Adding this descriptor will potentially require allocating new
    // descriptors. Make sure the accountant's MDL is prepared for this.
    //

    Status = MmpPrepareToAddAccountingDescriptor(Accountant, 1);
    if (!KSUCCESS(Status)) {
        goto AddAccountingDescriptorEnd;
    }

    //
    // Add the new descriptor to the list.
    //

    Status = MmMdAddDescriptorToList(&(Accountant->Mdl), Descriptor);
    if (!KSUCCESS(Status)) {
        goto AddAccountingDescriptorEnd;
    }

AddAccountingDescriptorEnd:
    return Status;
}

KSTATUS
MmpAllocateFromAccountant (
    PMEMORY_ACCOUNTING Accountant,
    PVM_ALLOCATION_PARAMETERS Request
    )

/*++

Routine Description:

    This routine allocates a piece of free memory from the given memory
    accountant's memory list and marks it as the given memory type.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure.

    Request - Supplies a pointer to the allocation request. The allocated
        address is also returned here.

Return Value:

    Status code.

--*/

{

    ULONGLONG AddressResult;
    KSTATUS Status;

    ASSERT((Accountant->Lock == NULL) ||
           (KeIsSharedExclusiveLockHeldExclusive(Accountant->Lock) != FALSE));

    //
    // Allocating from the MDL will potentially require adding new descriptors.
    // Make sure the accountant's MDL is prepared for this.
    //

    Status = MmpPrepareToAddAccountingDescriptor(Accountant, 1);
    if (!KSUCCESS(Status)) {
        goto AllocateFromAccountantEnd;
    }

    //
    // Go ahead and perform the allocation.
    //

    AddressResult = 0;
    Status = MmMdAllocateFromMdl(&(Accountant->Mdl),
                                 &AddressResult,
                                 Request->Size,
                                 Request->Alignment,
                                 (UINTN)(Request->Min),
                                 (UINTN)(Request->Max),
                                 Request->MemoryType,
                                 Request->Strategy);

    if (!KSUCCESS(Status)) {
        goto AllocateFromAccountantEnd;
    }

    ASSERT((UINTN)AddressResult == AddressResult);

    Request->Address = (PVOID)(UINTN)AddressResult;

AllocateFromAccountantEnd:
    return Status;
}

KSTATUS
MmpFreeAccountingRange (
    PADDRESS_SPACE AddressSpace,
    PVOID Allocation,
    UINTN SizeInBytes,
    BOOL LockHeld,
    ULONG UnmapFlags
    )

/*++

Routine Description:

    This routine frees the previously allocated memory range.

Arguments:

    AddressSpace - Supplies a pointer to the address space containing the
        allocated range. If NULL is supplied, the kernel address space will
        be used.

    Allocation - Supplies the allocation to free.

    SizeInBytes - Supplies the length of space, in bytes, to release.

    LockHeld - Supplies a boolean indicating whether or not the accountant's
        lock is already held exclusively.

    UnmapFlags - Supplies a bitmask of flags for the unmap operation. See
        UNMAP_FLAG_* for definitions. In the default case, this should contain
        UNMAP_FLAG_SEND_INVALIDATE_IPI. There are specific situations where
        it's known that this memory could not exist in another processor's TLB.

Return Value:

    Status code.

--*/

{

    PMEMORY_ACCOUNTING Accountant;
    PKTHREAD CurrentThread;
    ULONGLONG EndAddress;
    BOOL LockAcquired;
    MEMORY_DESCRIPTOR NewDescriptor;
    UINTN PageCount;
    UINTN PageIndex;
    ULONG PageShift;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    LockAcquired = FALSE;
    PageShift = MmPageShift();
    PageCount = ALIGN_RANGE_UP(SizeInBytes, MmPageSize()) >> PageShift;
    EndAddress = (ULONGLONG)(UINTN)Allocation + (PageCount << PageShift);
    if (EndAddress <= (UINTN)Allocation) {
        Status = STATUS_INVALID_PARAMETER;
        goto FreeAccountingRangeEnd;
    }

    if (AddressSpace == NULL) {
        AddressSpace = MmKernelAddressSpace;
    }

    Accountant = AddressSpace->Accountant;

    //
    // Initialize the new descriptor.
    //

    MmMdInitDescriptor(&NewDescriptor,
                       (UINTN)Allocation,
                       (ULONGLONG)(UINTN)Allocation + SizeInBytes,
                       MemoryTypeFree);

    //
    // Acquire the accountant lock to synchronize the check with the insertion.
    //

    if (LockHeld == FALSE) {
        MmpLockAccountant(Accountant, TRUE);
        LockAcquired = TRUE;
    }

    //
    // Assert that this is a valid range that was previously allocated.
    //

    ASSERT(MmpIsAccountingRangeAllocated(Accountant,
                                         Allocation,
                                         SizeInBytes) != FALSE);

    //
    // Add the new descriptor to the MDL.
    //

    Status = MmpAddAccountingDescriptor(Accountant, &NewDescriptor);
    if (!KSUCCESS(Status)) {
        goto FreeAccountingRangeEnd;
    }

    //
    // Unmap and free any pages associated with this range.
    //

    if ((Accountant->Flags & MEMORY_ACCOUNTING_FLAG_NO_MAP) == 0) {
        CurrentThread = KeGetCurrentThread();

        //
        // If the current thread is NULL, then this is the test. Do not unmap
        // anything.
        //

        if (CurrentThread != NULL) {
            if ((CurrentThread->OwningProcess->AddressSpace == AddressSpace) ||
                (Accountant == &MmKernelVirtualSpace)) {

                MmpUnmapPages(Allocation, PageCount, UnmapFlags, NULL);

            } else {
                for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
                    MmpUnmapPageInOtherProcess(
                                         AddressSpace,
                                         Allocation + (PageIndex << PageShift),
                                         UnmapFlags,
                                         NULL);
                }
            }
        }
    }

FreeAccountingRangeEnd:
    if (KSUCCESS(Status) &&
        ((Accountant->Flags & MEMORY_ACCOUNTING_FLAG_SYSTEM) != 0)) {

        MmpUpdateVirtualMemoryWarningLevel();
    }

    if (LockAcquired != FALSE) {
        MmpUnlockAccountant(Accountant, TRUE);
    }

    return Status;
}

KSTATUS
MmpRemoveAccountingRange (
    PMEMORY_ACCOUNTING Accountant,
    ULONGLONG StartAddress,
    ULONGLONG EndAddress
    )

/*++

Routine Description:

    This routine removes the given address range from the memory accountant.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure.

    StartAddress - Supplies the starting address of the range to remove.

    EndAddress - Supplies the first address beyond the region being removed.
        That is, the non-inclusive end address of the range.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    ASSERT(StartAddress < EndAddress);

    MmpLockAccountant(Accountant, TRUE);

    //
    // Removing the memory range will potentially require allocating new
    // descriptors if it splits an existing descriptor. Make sure the
    // accountant's MDL is prepared for this.
    //

    Status = MmpPrepareToAddAccountingDescriptor(Accountant, 1);
    if (!KSUCCESS(Status)) {
        goto RemoveAccountingRangeEnd;
    }

    Status = MmMdRemoveRangeFromList(&(Accountant->Mdl),
                                     StartAddress,
                                     EndAddress);

    if (!KSUCCESS(Status)) {
        goto RemoveAccountingRangeEnd;
    }

RemoveAccountingRangeEnd:
    MmpUnlockAccountant(Accountant, TRUE);
    return Status;
}

KSTATUS
MmpAllocateAddressRange (
    PMEMORY_ACCOUNTING Accountant,
    PVM_ALLOCATION_PARAMETERS Request,
    BOOL LockHeld
    )

/*++

Routine Description:

    This routine finds an address range of a certain size in the given memory
    space.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure.

    Request - Supplies a pointer to the allocation request. The resulting
        allocation is also returned here.

    LockHeld - Supplies a boolean indicating whether or not the accountant's
        lock is already held exclusively.

Return Value:

    Status code.

--*/

{

    BOOL LockAcquired;
    MEMORY_DESCRIPTOR NewDescriptor;
    BOOL RangeFree;
    KSTATUS Status;

    LockAcquired = FALSE;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Request->MemoryType != MemoryTypeFree);

    if (Request->Size == 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto AllocateAddressRangeEnd;
    }

    if (LockHeld == FALSE) {
        MmpLockAccountant(Accountant, TRUE);
        LockAcquired = TRUE;
    }

    //
    // If the caller requested an address, check to see if the range is in use.
    // If it is not, go ahead and allocate it.
    //

    if (Request->Address != NULL) {
        if (Request->Strategy == AllocationStrategyFixedAddressClobber) {
            RangeFree = TRUE;

        } else {
            RangeFree = MmpIsAccountingRangeFree(Accountant,
                                                 Request->Address,
                                                 Request->Size);
        }

        if (RangeFree != FALSE) {

            //
            // This virtual address is available. Allocate it.
            //

            MmMdInitDescriptor(
                            &NewDescriptor,
                            (UINTN)Request->Address,
                            (ULONGLONG)(UINTN)Request->Address + Request->Size,
                            Request->MemoryType);

            Status = MmpAddAccountingDescriptor(Accountant, &NewDescriptor);
            if (KSUCCESS(Status)) {
                goto AllocateAddressRangeEnd;
            }

        } else {
            Status = STATUS_MEMORY_CONFLICT;
        }

        if (Request->Strategy == AllocationStrategyFixedAddress) {
            goto AllocateAddressRangeEnd;
        }

        //
        // The original strategy is actually the fallback strategy when a
        // provided address does not work.
        //

        ASSERT(Request->Strategy != AllocationStrategyFixedAddressClobber);
    }

    //
    // Otherwise allocate any free address range.
    //

    Status = MmpAllocateFromAccountant(Accountant, Request);
    if (!KSUCCESS(Status)) {
        goto AllocateAddressRangeEnd;
    }

AllocateAddressRangeEnd:

    //
    // If the system accountant successfully allocate a range, update the
    // memory warning level.
    //

    if (KSUCCESS(Status) &&
        ((Accountant->Flags & MEMORY_ACCOUNTING_FLAG_SYSTEM) != 0)) {

        MmpUpdateVirtualMemoryWarningLevel();
    }

    if (LockAcquired != FALSE) {
        MmpUnlockAccountant(Accountant, TRUE);
    }

    return Status;
}

KSTATUS
MmpAllocateAddressRanges (
    PMEMORY_ACCOUNTING Accountant,
    UINTN Size,
    UINTN Count,
    MEMORY_TYPE MemoryType,
    PVOID *Allocations
    )

/*++

Routine Description:

    This routine allocates multiple potentially discontiguous address ranges
    of a given size.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure.

    Size - Supplies the size of each allocation, in bytes. This will also be
        the alignment of each allocation.

    Count - Supplies the number of allocations to make. This is the number of
        elements assumed to be in the return array.

    MemoryType - Supplies a the type of memory this allocation should be marked
        as. Do not specify MemoryTypeFree for this parameter.

    Allocations - Supplies a pointer where the addresses are returned on
        success. The caller is responsible for freeing each of these.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(MemoryType != MemoryTypeFree);

    MmpLockAccountant(Accountant, TRUE);
    Status = MmpPrepareToAddAccountingDescriptor(Accountant, Count);
    if (!KSUCCESS(Status)) {
        goto AllocateAddressRangesEnd;
    }

    Status = MmMdAllocateMultiple(&(Accountant->Mdl),
                                  Size,
                                  Count,
                                  MemoryType,
                                  (PUINTN)Allocations);

    if (!KSUCCESS(Status)) {
        goto AllocateAddressRangesEnd;
    }

AllocateAddressRangesEnd:
    if ((Accountant->Flags & MEMORY_ACCOUNTING_FLAG_SYSTEM) != 0) {
        MmpUpdateVirtualMemoryWarningLevel();
    }

    MmpUnlockAccountant(Accountant, TRUE);
    return Status;
}

KSTATUS
MmpMapRange (
    PVOID RangeAddress,
    UINTN RangeSize,
    UINTN PhysicalRunAlignment,
    UINTN PhysicalRunSize,
    BOOL WriteThrough,
    BOOL NonCached
    )

/*++

Routine Description:

    This routine maps the given memory region after allocating physical pages
    to back the region. The pages will be allocated in sets of physically
    contiguous pages according to the given physical run size. Each set of
    physical pages will be aligned to the given physical run alignment.

Arguments:

    RangeAddress - Supplies the starting virtual address of the range to map.

    RangeSize - Supplies the size of the virtual range to map, in bytes.

    PhysicalRunAlignment - Supplies the required alignment of the runs of
        physical pages, in bytes.

    PhysicalRunSize - Supplies the size of each run of physically contiguous
        pages.

    WriteThrough - Supplies a boolean indicating if the virtual addresses
        should be mapped write through (TRUE) or the default write back (FALSE).

    NonCached - Supplies a boolean indicating if the virtual addresses should
        be mapped non-cached (TRUE) or the default, which is to map is as
        normal cached memory (FALSE).

Return Value:

    Status code.

--*/

{

    ULONG MapFlags;
    UINTN MapIndex;
    UINTN PageCount;
    UINTN PageIndex;
    ULONG PageShift;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalPage;
    UINTN RunPageCount;
    KSTATUS Status;
    ULONG UnmapFlags;
    PVOID VirtualAddress;

    PageShift = MmPageShift();
    PageSize = MmPageSize();

    ASSERT(IS_ALIGNED((UINTN)RangeAddress, PageSize) != FALSE);
    ASSERT(IS_ALIGNED(RangeSize, PageSize) != FALSE);
    ASSERT(IS_ALIGNED(PhysicalRunAlignment, PageSize) != FALSE);
    ASSERT(IS_ALIGNED(PhysicalRunSize, PageSize) != FALSE);

    MapFlags = MAP_FLAG_PRESENT;
    if (RangeAddress >= KERNEL_VA_START) {
        MapFlags |= MAP_FLAG_GLOBAL;

    } else {
        MapFlags |= MAP_FLAG_USER_MODE;
    }

    if (WriteThrough != FALSE) {
        MapFlags |= MAP_FLAG_WRITE_THROUGH;
    }

    if (NonCached != FALSE) {
        MapFlags |= MAP_FLAG_CACHE_DISABLE;
    }

    PageCount = RangeSize >> PageShift;
    RunPageCount = PhysicalRunSize >> PageShift;

    ASSERT(RunPageCount != 0);

    PhysicalRunAlignment >>= PageShift;
    Status = STATUS_SUCCESS;
    VirtualAddress = RangeAddress;
    for (PageIndex = 0; PageIndex < PageCount; PageIndex += RunPageCount) {
        PhysicalPage = MmpAllocatePhysicalPages(RunPageCount,
                                                PhysicalRunAlignment);

        if (PhysicalPage == INVALID_PHYSICAL_ADDRESS) {
            Status = STATUS_NO_MEMORY;
            break;
        }

        for (MapIndex = 0; MapIndex < RunPageCount; MapIndex += 1) {
            MmpMapPage(PhysicalPage, VirtualAddress, MapFlags);
            VirtualAddress += PageSize;
            PhysicalPage += PageSize;
        }
    }

    if (!KSUCCESS(Status)) {
        UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                     UNMAP_FLAG_SEND_INVALIDATE_IPI;

        MmpUnmapPages(RangeAddress, PageIndex, UnmapFlags, NULL);
    }

    return Status;
}

VOID
MmpLockAccountant (
    PMEMORY_ACCOUNTING Accountant,
    BOOL Exclusive
    )

/*++

Routine Description:

    This routine acquires the memory accounting lock, preventing changes to the
    virtual address space of the given process.

Arguments:

    Accountant - Supplies a pointer to the memory accountant.

    Exclusive - Supplies a boolean indicating whether to acquire the lock
        shared (FALSE) if the caller just wants to make sure the VA layout
        doesn't change or exclusive (TRUE) if the caller wants to change the
        VA layout.

Return Value:

    None.

--*/

{

    if (Accountant->Lock == NULL) {
        return;
    }

    if (Exclusive != FALSE) {
        KeAcquireSharedExclusiveLockExclusive(Accountant->Lock);

    } else {
        KeAcquireSharedExclusiveLockShared(Accountant->Lock);
    }

    return;
}

VOID
MmpUnlockAccountant (
    PMEMORY_ACCOUNTING Accountant,
    BOOL Exclusive
    )

/*++

Routine Description:

    This routine releases the memory accounting lock.

Arguments:

    Accountant - Supplies a pointer to the memory accountant.

    Exclusive - Supplies a boolean indicating whether the lock was held
        shared (FALSE) or exclusive (TRUE).

Return Value:

    None.

--*/

{

    if (Accountant->Lock == NULL) {
        return;
    }

    if (Exclusive != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(Accountant->Lock);

    } else {
        KeReleaseSharedExclusiveLockShared(Accountant->Lock);
    }

    return;
}

KSTATUS
MmpInitializeKernelVa (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine initializes the kernel's virtual memory accounting structures.

Arguments:

    Parameters - Supplies a pointer to the parameters provided by the boot
        loader.

Return Value:

    Status code.

--*/

{

    INITIALIZE_KERNEL_VA_CONTEXT Context;
    MEMORY_DESCRIPTOR Descriptor;
    ULONG DescriptorSize;
    KSTATUS Status;

    Status = MmInitializeMemoryAccounting(&MmKernelVirtualSpace,
                                          MEMORY_ACCOUNTING_FLAG_SYSTEM);

    if (!KSUCCESS(Status)) {
        goto InitializeKernelVaEnd;
    }

    //
    // Add enough room for the initial memory map's worth of descriptors from
    // the MM init memory provided by the loader.
    //

    DescriptorSize = (Parameters->VirtualMap->DescriptorCount +
                      FREE_SYSTEM_DESCRIPTORS_REQUIRED_FOR_REFILL) *
                     sizeof(MEMORY_DESCRIPTOR);

    if (Parameters->MmInitMemory.Size < DescriptorSize) {

        ASSERT(FALSE);

        Status = STATUS_NO_MEMORY;
        goto InitializeKernelVaEnd;
    }

    //
    // Actually add all the rest of the init memory.
    //

    DescriptorSize = Parameters->MmInitMemory.Size;
    MmMdAddFreeDescriptorsToMdl(&(MmKernelVirtualSpace.Mdl),
                                Parameters->MmInitMemory.Buffer,
                                DescriptorSize);

    Parameters->MmInitMemory.Buffer += DescriptorSize;
    Parameters->MmInitMemory.Size -= DescriptorSize;

    //
    // Add the entire kernel address space as free.
    //

    MmMdInitDescriptor(&Descriptor,
                       (UINTN)KERNEL_VA_START,
                       KERNEL_VA_END,
                       MemoryTypeFree);

    Status = MmpAddAccountingDescriptor(&MmKernelVirtualSpace, &Descriptor);
    if (!KSUCCESS(Status)) {
        goto InitializeKernelVaEnd;
    }

    //
    // Loop through and copy all the boot descriptors.
    //

    Context.Status = STATUS_SUCCESS;
    MmMdIterate(Parameters->VirtualMap,
                MmpInitializeKernelVaIterator,
                &Context);

    if (!KSUCCESS(Context.Status)) {
        Status = Context.Status;
        goto InitializeKernelVaEnd;
    }

    //
    // Set up the virtual memory warning trigger and retreat values depending
    // on the total size of system virtual memory. There are really only two
    // buckets here: system VA less than 4GB and the expansive amount of system
    // VA available on a 64-bit system.
    //

    if (MmKernelVirtualSpace.Mdl.TotalSpace <= MAX_ULONG) {
        MmVirtualMemoryWarningLevel1Trigger =
                               MM_SMALL_VIRTUAL_MEMORY_WARNING_LEVEL_1_TRIGGER;

        MmVirtualMemoryWarningLevel1Retreat =
                               MM_SMALL_VIRTUAL_MEMORY_WARNING_LEVEL_1_RETREAT;

    } else {
        MmVirtualMemoryWarningLevel1Trigger =
                               MM_LARGE_VIRTUAL_MEMORY_WARNING_LEVEL_1_TRIGGER;

        MmVirtualMemoryWarningLevel1Retreat =
                               MM_LARGE_VIRTUAL_MEMORY_WARNING_LEVEL_1_RETREAT;
    }

    Status = STATUS_SUCCESS;

InitializeKernelVaEnd:
    return Status;
}

BOOL
MmpIsAccountingRangeFree (
    PMEMORY_ACCOUNTING Accountant,
    PVOID Address,
    ULONGLONG SizeInBytes
    )

/*++

Routine Description:

    This routine determines whether the given address range is free according
    to the accountant. This routine assumes the accounting lock is already
    held.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure.

    Address - Supplies the address to find the corresponding descriptor for.

    SizeInBytes - Supplies the size, in bytes, of the range in question.

Return Value:

    TRUE if the given range is free.

    FALSE if at least part of the given range is in use.

--*/

{

    ULONGLONG EndAddress;
    ULONGLONG StartAddress;

    StartAddress = (UINTN)Address;
    EndAddress = StartAddress + SizeInBytes;
    if (EndAddress < StartAddress) {
        return FALSE;
    }

    if (MmMdIsRangeFree(&(Accountant->Mdl), StartAddress, EndAddress) != NULL) {
        return TRUE;
    }

    return FALSE;
}

BOOL
MmpIsAccountingRangeInUse (
    PMEMORY_ACCOUNTING Accountant,
    PVOID Address,
    ULONG SizeInBytes
    )

/*++

Routine Description:

    This routine determines whether or not any portion of the supplied range
    is in use.

Arguments:

    Accountant - Supplies a pointer to a memory accounting structure.

    Address - Supplies the base address of the range.

    SizeInBytes - Supplies the size of the range, in bytes.

Return Value:

    Returns TRUE if a portion of the range is in use or FALSE otherwise.

--*/

{

    ULONGLONG EndAddress;
    PMEMORY_DESCRIPTOR ExistingAllocation;
    ULONGLONG ExistingEndAddress;
    ULONG PageCount;
    ULONG PageShift;

    PageShift = MmPageShift();
    PageCount = ALIGN_RANGE_UP(SizeInBytes, MmPageSize()) >> PageShift;
    EndAddress = (UINTN)Address + (PageCount << PageShift);

    //
    // Look up the descriptor containing this range. If no descriptor is found
    // it means that the range is not in use.
    //

    ExistingAllocation = MmMdLookupDescriptor(&(Accountant->Mdl),
                                              (UINTN)Address,
                                              (UINTN)Address + SizeInBytes);

    if (ExistingAllocation == NULL) {
        return FALSE;
    }

    //
    // If a descriptor is found and it is not free, then the region is in use.
    //

    if (ExistingAllocation->Type != MemoryTypeFree) {
        return TRUE;
    }

    //
    // As free regions are coalesced, if the found descriptor does not contain
    // the entire region, then consider it in use. The corner case is that the
    // rest of the region is actually not described by the MDL (i.e. it is
    // technically not in use), but don't consider that case in order to avoid
    // splitting off a portion of a free descriptor to merge with undescribed
    // space.
    //

    ExistingEndAddress = ExistingAllocation->BaseAddress +
                         ExistingAllocation->Size;

    if ((ExistingAllocation->BaseAddress > (ULONGLONG)(UINTN)Address) ||
        (ExistingEndAddress < EndAddress)) {

        return TRUE;
    }

    return FALSE;
}

BOOL
MmpIsAccountingRangeAllocated (
    PMEMORY_ACCOUNTING Accountant,
    PVOID Address,
    ULONG SizeInBytes
    )

/*++

Routine Description:

    This routine determines whether or not the supplied range is currently
    allocated in the given memory accountant.

Arguments:

    Accountant - Supplies a pointer to a memory accounting structure.

    Address - Supplies the base address of the range.

    SizeInBytes - Supplies the size of the range, in bytes.

Return Value:

    Returns TRUE if the range is completely allocated for a single memory type
    or FALSE otherwise.

--*/

{

    ULONGLONG EndAddress;
    PMEMORY_DESCRIPTOR ExistingAllocation;
    ULONGLONG ExistingEndAddress;
    ULONG PageCount;
    ULONG PageShift;

    PageShift = MmPageShift();
    PageCount = ALIGN_RANGE_UP(SizeInBytes, MmPageSize()) >> PageShift;
    EndAddress = (ULONGLONG)(UINTN)Address + (PageCount << PageShift);

    //
    // Look up the descriptor containing this allocation.
    //

    ExistingAllocation = MmMdLookupDescriptor(&(Accountant->Mdl),
                                              (UINTN)Address,
                                              EndAddress);

    if ((ExistingAllocation == NULL) ||
        (ExistingAllocation->Type == MemoryTypeFree)) {

        return FALSE;
    }

    //
    // Ensure that the descriptor covers the whole allocation.
    //

    ExistingEndAddress = ExistingAllocation->BaseAddress +
                         ExistingAllocation->Size;

    if ((ExistingEndAddress < EndAddress) ||
        (ExistingAllocation->BaseAddress > (ULONGLONG)(UINTN)Address)) {

        return FALSE;
    }

    return TRUE;
}

PVOID
MmpMapPhysicalAddress (
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN SizeInBytes,
    BOOL Writable,
    BOOL WriteThrough,
    BOOL CacheDisabled,
    MEMORY_TYPE MemoryType
    )

/*++

Routine Description:

    This routine maps a physical address into kernel VA space.

Arguments:

    PhysicalAddress - Supplies a pointer to the physical address. This address
        must be page aligned.

    SizeInBytes - Supplies the size in bytes of the mapping. This will be
        rounded up to the nearest page size.

    Writable - Supplies a boolean indicating if the memory is to be marked
        writable (TRUE) or read-only (FALSE).

    WriteThrough - Supplies a boolean indicating if the memory is to be marked
        write-through (TRUE) or write-back (FALSE).

    CacheDisabled - Supplies a boolean indicating if the memory is to be mapped
        uncached.

    MemoryType - Supplies the memory type to allocate this as.

Return Value:

    Returns a pointer to the virtual address of the mapping on success, or
    NULL on failure.

--*/

{

    PHYSICAL_ADDRESS CurrentPhysicalAddress;
    PVOID CurrentVirtualAddress;
    ULONGLONG Index;
    ULONG MapFlags;
    ULONGLONG PageCount;
    ULONG PageShift;
    ULONG PageSize;
    UINTN Size;
    KSTATUS Status;
    VM_ALLOCATION_PARAMETERS VaRequest;

    PageShift = MmPageShift();
    PageSize = MmPageSize();
    VaRequest.Address = NULL;

    ASSERT((PhysicalAddress & (PageSize - 1)) == 0);

    Size = ALIGN_RANGE_UP(SizeInBytes, PageSize);
    PageCount = Size >> PageShift;
    if (Size == 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto MapPhysicalAddressEnd;
    }

    //
    // Find a VA range for this mapping.
    //

    VaRequest.Size = Size;
    VaRequest.Alignment = PageSize;
    VaRequest.Min = 0;
    VaRequest.Max = MAX_ADDRESS;
    VaRequest.MemoryType = MemoryType;
    VaRequest.Strategy = AllocationStrategyAnyAddress;
    Status = MmpAllocateAddressRange(&MmKernelVirtualSpace, &VaRequest, FALSE);
    if (!KSUCCESS(Status)) {
        goto MapPhysicalAddressEnd;
    }

    //
    // Map each page with the desired attributes.
    //

    MapFlags = MAP_FLAG_PRESENT | MAP_FLAG_GLOBAL;
    if (Writable == FALSE) {
        MapFlags |= MAP_FLAG_READ_ONLY;
    }

    if (WriteThrough != FALSE) {
        MapFlags |= MAP_FLAG_WRITE_THROUGH;
    }

    if (CacheDisabled != FALSE) {
        MapFlags |= MAP_FLAG_CACHE_DISABLE;
    }

    CurrentPhysicalAddress = PhysicalAddress;
    CurrentVirtualAddress = VaRequest.Address;
    for (Index = 0; Index < PageCount; Index += 1) {
        MmpMapPage(CurrentPhysicalAddress, CurrentVirtualAddress, MapFlags);
        CurrentPhysicalAddress += PageSize;
        CurrentVirtualAddress += PageSize;
    }

    Status = STATUS_SUCCESS;

MapPhysicalAddressEnd:
    if (!KSUCCESS(Status)) {

        //
        // Free the VA range if it was claimed, but do not free the physical
        // pages as those are owned by the caller.
        //

        if (VaRequest.Address != NULL) {
            MmpFreeAccountingRange(NULL,
                                   VaRequest.Address,
                                   Size,
                                   FALSE,
                                   UNMAP_FLAG_SEND_INVALIDATE_IPI);
        }
    }

    return VaRequest.Address;
}

KSTATUS
MmpInitializeUserSharedData (
    VOID
    )

/*++

Routine Description:

    This routine allocates and maps the user shared data into kernel virtual
    address space. The address is stored globally.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONG PageSize;
    KSTATUS Status;
    ULONG UnmapFlags;
    PVOID UserSharedDataPage;
    VM_ALLOCATION_PARAMETERS VaRequest;

    PageSize = MmPageSize();

    ASSERT(sizeof(USER_SHARED_DATA) <= PageSize);

    //
    // Allocate and map a single page that is page-aligned. The virtual address
    // can be dynamic.
    //

    UserSharedDataPage = NULL;
    VaRequest.Address = NULL;
    VaRequest.Size = ALIGN_RANGE_UP(sizeof(USER_SHARED_DATA), PageSize);
    VaRequest.Alignment = PageSize;
    VaRequest.Min = 0;
    VaRequest.Max = MAX_ADDRESS;
    VaRequest.MemoryType = MemoryTypeReserved;
    VaRequest.Strategy = AllocationStrategyAnyAddress;
    Status = MmpAllocateAddressRange(&MmKernelVirtualSpace, &VaRequest, FALSE);
    if (!KSUCCESS(Status)) {
        goto InitializeUserSharedDataEnd;
    }

    UserSharedDataPage = VaRequest.Address;
    Status = MmpMapRange(UserSharedDataPage,
                         PageSize,
                         PageSize,
                         PageSize,
                         FALSE,
                         FALSE);

    if (!KSUCCESS(Status)) {
        goto InitializeUserSharedDataEnd;
    }

    RtlZeroMemory(UserSharedDataPage, PageSize);
    MmUserSharedData = UserSharedDataPage;
    Status = STATUS_SUCCESS;

InitializeUserSharedDataEnd:
    if (!KSUCCESS(Status)) {
        if (UserSharedDataPage != NULL) {
            UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                         UNMAP_FLAG_SEND_INVALIDATE_IPI;

            MmpFreeAccountingRange(NULL,
                                   UserSharedDataPage,
                                   PageSize,
                                   FALSE,
                                   UnmapFlags);

            UserSharedDataPage = NULL;
        }
    }

    return Status;
}

VOID
MmpCopyPage (
    PIMAGE_SECTION Section,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine copies the page at the given virtual address. It temporarily
    maps the physical address at the given temporary virtual address in order
    to perform the copy.

Arguments:

    Section - Supplies a pointer to the image section to which the virtual
        address belongs.

    VirtualAddress - Supplies the page-aligned virtual address to use as the
        initial contents of the page.

    PhysicalAddress - Supplies the physical address of the destination page
        where the data is to be copied.

Return Value:

    None.

--*/

{

    ULONG Attributes;
    RUNLEVEL OldRunLevel;
    ULONG PageSize;
    PPROCESSOR_BLOCK ProcessorBlock;
    PHYSICAL_ADDRESS SourcePhysical;

    ASSERT(PhysicalAddress != INVALID_PHYSICAL_ADDRESS);

    SourcePhysical = MmpVirtualToPhysical(VirtualAddress, &Attributes);

    ASSERT(SourcePhysical != INVALID_PHYSICAL_ADDRESS);

    PageSize = MmPageSize();

    //
    // Map the page to the temporary virtual address in order to perform a copy.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorBlock = KeGetCurrentProcessorBlock();
    MmpMapPage(PhysicalAddress, ProcessorBlock->SwapPage, MAP_FLAG_PRESENT);

    //
    // If the page is not accessible, make it accessible temporarily.
    //

    if ((Attributes & MAP_FLAG_PRESENT) == 0) {
        MmpChangeMemoryRegionAccess(VirtualAddress,
                                    1,
                                    MAP_FLAG_PRESENT | MAP_FLAG_READ_ONLY,
                                    MAP_FLAG_ALL_MASK);
    }

    //
    // Make a copy of the original page (which is still read-only).
    //

    RtlCopyMemory(ProcessorBlock->SwapPage, VirtualAddress, PageSize);

    //
    // Make the page inaccessible again if it was not accessible before.
    //

    if ((Attributes & MAP_FLAG_PRESENT) == 0) {
        MmpChangeMemoryRegionAccess(VirtualAddress,
                                    1,
                                    Attributes,
                                    MAP_FLAG_ALL_MASK);
    }

    if ((Section->Flags & IMAGE_SECTION_EXECUTABLE) != 0) {
        MmpSyncSwapPage(ProcessorBlock->SwapPage, PageSize);
    }

    //
    // Unmap the page from the temporary space.
    //

    MmpUnmapPages(ProcessorBlock->SwapPage, 1, 0, NULL);
    KeLowerRunLevel(OldRunLevel);
    return;
}

VOID
MmpZeroPage (
    PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine zeros the page specified by the physical address. It maps the
    page temporarily in order to zero it out.

Arguments:

    PhysicalAddress - Supplies the physical address of the page to be filled
        with zero.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;
    ULONG PageSize;
    PPROCESSOR_BLOCK ProcessorBlock;

    ASSERT(PhysicalAddress != INVALID_PHYSICAL_ADDRESS);

    PageSize = MmPageSize();

    //
    // Map the page to the temporary address in order to perform the zero.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorBlock = KeGetCurrentProcessorBlock();
    MmpMapPage(PhysicalAddress, ProcessorBlock->SwapPage, MAP_FLAG_PRESENT);

    //
    // Zero the page.
    //

    RtlZeroMemory(ProcessorBlock->SwapPage, PageSize);

    //
    // Unmap the page from the temporary space.
    //

    MmpUnmapPages(ProcessorBlock->SwapPage, 1, 0, NULL);
    KeLowerRunLevel(OldRunLevel);
    return;
}

VOID
MmpUpdateResidentSetCounter (
    PADDRESS_SPACE AddressSpace,
    INTN Addition
    )

/*++

Routine Description:

    This routine adjusts the process resident set counter. This should only
    be done for user mode addresses.

Arguments:

    AddressSpace - Supplies a pointer to the address space to update.

    Addition - Supplies the number of pages to add or subtract from the counter.

Return Value:

    None.

--*/

{

    UINTN OriginalValue;
    UINTN PreviousMaximum;
    UINTN ReadMaximum;

    OriginalValue = RtlAtomicAdd(&(AddressSpace->ResidentSet), Addition);
    if (Addition <= 0) {

        ASSERT((Addition == 0) || (OriginalValue != 0));

        return;
    }

    OriginalValue += Addition;
    PreviousMaximum = AddressSpace->MaxResidentSet;

    //
    // Loop trying to update the maximum to this new value.
    //

    while (PreviousMaximum < OriginalValue) {
        ReadMaximum = RtlAtomicCompareExchange(&(AddressSpace->MaxResidentSet),
                                               OriginalValue,
                                               PreviousMaximum);

        if (ReadMaximum == PreviousMaximum) {
            break;
        }

        PreviousMaximum = ReadMaximum;
    }

    return;
}

VOID
MmpAddPageZeroDescriptorsToMdl (
    PMEMORY_ACCOUNTING Accountant
    )

/*++

Routine Description:

    This routine maps page zero and adds it to be used as memory descriptors
    for the given memory accountant. It is assume that page zero was already
    reserved by some means.

Arguments:

    Accountant - Supplies a pointer to the memory accontant to receive the
        memory descriptors from page zero.

Return Value:

    None.

--*/

{

    ULONG PageSize;
    PVOID VirtualAddress;

    ASSERT(MmPhysicalPageZeroAvailable != FALSE);

    //
    // Map physical page zero. If this fails then physical page zero is just
    // wasted.
    //

    PageSize = MmPageSize();
    VirtualAddress = MmpMapPhysicalAddress(0,
                                           PageSize,
                                           TRUE,
                                           FALSE,
                                           FALSE,
                                           MemoryTypeMmStructures);

    if (VirtualAddress == NULL) {
        return;
    }

    //
    // Insert the now mapped page zero as descriptors for the accountant.
    //

    MmpLockAccountant(Accountant, TRUE);
    MmMdAddFreeDescriptorsToMdl(&(Accountant->Mdl), VirtualAddress, PageSize);
    MmpUnlockAccountant(Accountant, TRUE);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
MmpPrepareToAddAccountingDescriptor (
    PMEMORY_ACCOUNTING Accountant,
    UINTN NewAllocations
    )

/*++

Routine Description:

    This routine make sures the memory accountant's MDL has enough available
    free memory descriptors to allow for the addition of a new memory region,
    either from insertion or allocation.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure.

    NewAllocations - Supplies the number of new allocations that are going to
        be added to the MDL.

Return Value:

    Status code.

--*/

{

    ULONGLONG Address;
    ULONG AllocationSize;
    PVOID CurrentAddress;
    ULONG Index;
    UINTN Needed;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress[DESCRIPTOR_REFILL_PAGE_COUNT];
    KSTATUS Status;
    PVOID VirtualAddress;

    //
    // If this is not the system accountant, then it's ready to go. The memory
    // descriptor library will allocate new descriptors as necessary.
    //

    if ((Accountant->Flags & MEMORY_ACCOUNTING_FLAG_SYSTEM) == 0) {
        return STATUS_SUCCESS;
    }

    ASSERT((Accountant->Lock == NULL) ||
           (KeIsSharedExclusiveLockHeldExclusive(Accountant->Lock) != FALSE));

    //
    // If each descriptor splits an existing one, then two new descriptors are
    // needed per allocation. Add an extra for the descriptor refill.
    //

    Needed = (NewAllocations + 1) * 2;

    //
    // If there are enough free descriptors left to proceed and to still allow
    // the descriptors to be replenished in the future, then exit successfully.
    //

    if (Accountant->Mdl.UnusedDescriptorCount >= Needed) {
        return STATUS_SUCCESS;
    }

    //
    // Otherwise it is time to add more descriptors to the list. Allocate a
    // few physical pages.
    //

    PageSize = MmPageSize();
    RtlZeroMemory(PhysicalAddress, sizeof(PHYSICAL_ADDRESS) * 3);
    Status = MmpAllocateScatteredPhysicalPages(0,
                                               -1ULL,
                                               PhysicalAddress,
                                               DESCRIPTOR_REFILL_PAGE_COUNT);

    if (!KSUCCESS(Status)) {
        goto PrepareToAddAccountingDescriptorEnd;
    }

    //
    // Get a virtual address region to map the physical pages. There should be
    // enough free descriptors left for this allocation.
    //

    ASSERT(Accountant->Mdl.UnusedDescriptorCount >=
           FREE_SYSTEM_DESCRIPTORS_MIN);

    AllocationSize = DESCRIPTOR_REFILL_PAGE_COUNT * PageSize;
    Status = MmMdAllocateFromMdl(&(Accountant->Mdl),
                                 &Address,
                                 AllocationSize,
                                 PageSize,
                                 0,
                                 MAX_UINTN,
                                 MemoryTypeMmStructures,
                                 AllocationStrategyAnyAddress);

    if (!KSUCCESS(Status)) {
        goto PrepareToAddAccountingDescriptorEnd;
    }

    ASSERT((UINTN)Address == Address);

    VirtualAddress = (PVOID)(UINTN)Address;

    //
    // Map the physical pages.
    //

    CurrentAddress = VirtualAddress;
    for (Index = 0; Index < DESCRIPTOR_REFILL_PAGE_COUNT; Index += 1) {
        MmpMapPage(PhysicalAddress[Index],
                   CurrentAddress,
                   MAP_FLAG_PRESENT | MAP_FLAG_GLOBAL);

        CurrentAddress += PageSize;
    }

    //
    // Insert these new pages as descriptors.
    //

    MmMdAddFreeDescriptorsToMdl(&(Accountant->Mdl),
                                VirtualAddress,
                                AllocationSize);

    ASSERT(Accountant->Mdl.UnusedDescriptorCount >= Needed);

    Status = STATUS_SUCCESS;

PrepareToAddAccountingDescriptorEnd:
    if (!KSUCCESS(Status)) {
        for (Index = 0; Index < 3; Index += 1) {
            if (PhysicalAddress[Index] != INVALID_PHYSICAL_ADDRESS) {
                MmFreePhysicalPage(PhysicalAddress[Index]);
            }
        }
    }

    return Status;
}

VOID
MmpUpdateVirtualMemoryWarningLevel (
    VOID
    )

/*++

Routine Description:

    This routine updates the current virtual memory warning level. It is
    called after the system virtual memory map has changed.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PMEMORY_ACCOUNTING Accountant;
    PLIST_ENTRY BinList;
    MEMORY_WARNING_LEVEL CurrentLevel;
    MEMORY_WARNING_LEVEL NewLevel;
    UINTN RequiredFree;

    Accountant = &MmKernelVirtualSpace;
    MmFreeVirtualByteCount = Accountant->Mdl.FreeSpace;
    CurrentLevel = MmVirtualMemoryWarningLevel;
    NewLevel = CurrentLevel;
    BinList = &(Accountant->Mdl.FreeLists[MDL_BIN_COUNT - 1]);
    if (CurrentLevel != MemoryWarningLevelNone) {
        RequiredFree = MmVirtualMemoryWarningLevel1Retreat;
        if ((!LIST_EMPTY(BinList)) &&
            (Accountant->Mdl.FreeSpace >= RequiredFree)) {

            NewLevel = MemoryWarningLevelNone;

        //
        // Potentially upgrade from level 1 to level 2.
        //

        } else if (CurrentLevel == MemoryWarningLevel1) {
            BinList = &(Accountant->Mdl.FreeLists[MDL_BIN_COUNT - 2]);
            if (LIST_EMPTY(BinList)) {
                NewLevel = MemoryWarningLevel2;
            }
        }

    //
    // There is currently no warning, see if there should be.
    //

    } else {
        RequiredFree = MmVirtualMemoryWarningLevel1Trigger;
        if ((Accountant->Mdl.FreeSpace < RequiredFree) ||
            (LIST_EMPTY(BinList))) {

            NewLevel = MemoryWarningLevel1;
            BinList = &(Accountant->Mdl.FreeLists[MDL_BIN_COUNT - 2]);
            if (LIST_EMPTY(BinList)) {
                NewLevel = MemoryWarningLevel2;
            }
        }
    }

    if (NewLevel != CurrentLevel) {
        MmVirtualMemoryWarningLevel = NewLevel;
        KeSignalEvent(MmVirtualMemoryWarningEvent, SignalOptionPulse);
    }

    return;
}

VOID
MmpInitializeKernelVaIterator (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called once for each descriptor in the memory descriptor
    list.

Arguments:

    DescriptorList - Supplies a pointer to the descriptor list being iterated
        over.

    Descriptor - Supplies a pointer to the current descriptor.

    Context - Supplies an optional opaque pointer of context that was provided
        when the iteration was requested.

Return Value:

    None.

--*/

{

    PINITIALIZE_KERNEL_VA_CONTEXT MemoryContext;
    KSTATUS Status;

    MemoryContext = Context;
    if (IS_MEMORY_FREE_TYPE(Descriptor->Type)) {
        return;
    }

    Status = MmpAddAccountingDescriptor(&MmKernelVirtualSpace, Descriptor);
    if (!KSUCCESS(Status)) {
        MemoryContext->Status = Status;
    }

    return;
}

VOID
MmpCloneAddressSpaceIterator (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called once for each descriptor in the memory descriptor
    list.

Arguments:

    DescriptorList - Supplies a pointer to the descriptor list being iterated
        over.

    Descriptor - Supplies a pointer to the current descriptor.

    Context - Supplies an optional opaque pointer of context that was provided
        when the iteration was requested.

Return Value:

    None.

--*/

{

    PCLONE_ADDRESS_SPACE_CONTEXT CloneContext;
    KSTATUS Status;

    CloneContext = Context;
    if (!KSUCCESS(CloneContext->Status)) {
        return;
    }

    Status = MmpAddAccountingDescriptor(CloneContext->Accounting, Descriptor);
    if (!KSUCCESS(Status)) {
        CloneContext->Status = Status;
    }

    return;
}

