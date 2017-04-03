/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    load.c

Abstract:

    This module implements support for loading executable images.

Author:

    Evan Green 7-Oct-2012

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

KSTATUS
MmMapFileSection (
    HANDLE FileHandle,
    IO_OFFSET FileOffset,
    PVM_ALLOCATION_PARAMETERS VaRequest,
    ULONG Flags,
    BOOL KernelSpace,
    PMEMORY_RESERVATION Reservation
    )

/*++

Routine Description:

    This routine maps a file or a portion of a file into virtual memory space
    of the current process. This routine must be called below dispatch level.

Arguments:

    FileHandle - Supplies the open file handle.

    FileOffset - Supplies the offset, in bytes, from the start of the file
        where the mapping should begin.

    VaRequest - Supplies a pointer to the virtual address allocation
        parameters. If the supplied size is zero, then this routine will
        attempt to map until the end of the file. The alignment will be set
        to a page size, and the memory type will be set to reserved.

    Flags - Supplies flags governing the mapping of the section. See
        IMAGE_SECTION_* definitions.

    KernelSpace - Supplies a boolean indicating whether to map the section in
        kernel space or user space.

    Reservation - Supplies an optional pointer to a memory reservation for the
        desired mapping. A reservation is required only if several mappings
        need to be allocated in the same range together for any one mapping to
        be useful.

Return Value:

    Status code.

--*/

{

    BOOL AccountingLockHeld;
    ULONG Adjustment;
    ULONGLONG FileSize;
    ULONG HandleAccess;
    PKPROCESS ImageProcess;
    PKPROCESS KernelProcess;
    ULONG PageSize;
    PKPROCESS Process;
    BOOL RangeAllocated;
    PMEMORY_ACCOUNTING Realtor;
    KSTATUS Status;
    ULONG UnmapFlags;

    AccountingLockHeld = FALSE;
    KernelProcess = PsGetKernelProcess();
    PageSize = MmPageSize();

    ASSERT(POWER_OF_2(PageSize) != FALSE);

    //
    // The file mapping must be page aligned.
    //

    ASSERT(IS_POINTER_ALIGNED(VaRequest->Address, PageSize) != FALSE);

    Process = PsGetCurrentProcess();
    Realtor = NULL;
    RangeAllocated = FALSE;

    ASSERT(FileHandle != NULL);

    //
    // This code must be run at low level. It's also illegal to try to map
    // kernel mode stuff in a user process.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((KernelSpace != FALSE) || (Process != KernelProcess));

    if (KernelSpace != FALSE) {
        ImageProcess = KernelProcess;

    } else {
        ImageProcess = Process;
    }

    //
    // Don't be rude in kernel space, it's almost certain to be a disaster.
    //

    if ((Process == KernelProcess) &&
        (VaRequest->Strategy == AllocationStrategyFixedAddressClobber)) {

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto MapFileSectionEnd;
    }

    //
    // Check the handle permissions.
    //

    if (FileHandle != INVALID_HANDLE) {
        HandleAccess = IoGetIoHandleAccessPermissions(FileHandle);
        if ((HandleAccess & IO_ACCESS_READ) == 0) {
            Status = STATUS_ACCESS_DENIED;
            goto MapFileSectionEnd;
        }

        if (((Flags & IMAGE_SECTION_SHARED) != 0) &&
            ((Flags & IMAGE_SECTION_WRITABLE) != 0) &&
            ((HandleAccess & IO_ACCESS_WRITE) == 0)) {

            Status = STATUS_ACCESS_DENIED;
            goto MapFileSectionEnd;
        }
    }

    //
    // If the size was zero, find out how big the file is and use that.
    //

    if (VaRequest->Size == 0) {
        Status = IoGetFileSize(FileHandle, &FileSize);
        if (!KSUCCESS(Status)) {
            goto MapFileSectionEnd;
        }

        if ((FileSize - FileOffset) > MAX_UINTN) {
            Status = STATUS_NOT_SUPPORTED;
            goto MapFileSectionEnd;
        }

        VaRequest->Size = FileSize - FileOffset;
    }

    if (KernelSpace != FALSE) {
        Realtor = &MmKernelVirtualSpace;

    } else {
        Realtor = Process->AddressSpace->Accountant;
    }

    //
    // If there's a valid reservation that covers the requested range, then
    // use the requested address.
    //

    Adjustment = 0;
    if (Reservation != NULL) {
        if ((VaRequest->Strategy == AllocationStrategyFixedAddress) ||
            (VaRequest->Strategy == AllocationStrategyFixedAddressClobber)) {

            //
            // Use the requested address.
            //

            Adjustment = REMAINDER((UINTN)VaRequest->Address, PageSize);
            VaRequest->Address = ALIGN_POINTER_DOWN(VaRequest->Address,
                                                    PageSize);

            //
            // Fail if the requested VA is outside the reservation. Truncate
            // the size if it goes beyond the reservation.
            //

            if ((VaRequest->Address < Reservation->VirtualBase) ||
                (VaRequest->Address >
                 (Reservation->VirtualBase + Reservation->Size))) {

                Status = STATUS_INVALID_PARAMETER;
                goto MapFileSectionEnd;
            }

            if (VaRequest->Address + VaRequest->Size >
                Reservation->VirtualBase + Reservation->Size) {

                VaRequest->Size = (UINTN)Reservation->VirtualBase +
                                  Reservation->Size - (UINTN)VaRequest->Address;

                if (VaRequest->Size == 0) {
                    Status = STATUS_INVALID_PARAMETER;
                    goto MapFileSectionEnd;
                }
            }

            //
            // Fail if the file offset is too small to be successfully adjusted
            // down to a page boundary given the (VA, FileOffset) tuple.
            //

            if (FileOffset < Adjustment) {
                Status = STATUS_INVALID_PARAMETER;
                goto MapFileSectionEnd;
            }
        }
    }

    //
    // If the allocation has not yet been done, then allocate now.
    //

    if (KernelSpace == FALSE) {
        MmpLockAccountant(Realtor, TRUE);
        AccountingLockHeld = TRUE;
    }

    VaRequest->Size = ALIGN_RANGE_UP(VaRequest->Size + Adjustment, PageSize);
    VaRequest->Alignment = PageSize;
    VaRequest->MemoryType = MemoryTypeReserved;
    if ((VaRequest->Address == NULL) || (Reservation == NULL)) {
        Adjustment = REMAINDER(FileOffset, PageSize);
        Status = MmpAllocateAddressRange(Realtor,
                                         VaRequest,
                                         AccountingLockHeld);

        if (!KSUCCESS(Status)) {
            goto MapFileSectionEnd;
        }

        RangeAllocated = TRUE;
    }

    //
    // Create the mapping between the currently unmapped pages and the
    // file, which acts as its backing store.
    //

    Status = MmpAddImageSection(ImageProcess->AddressSpace,
                                VaRequest->Address,
                                VaRequest->Size,
                                Flags,
                                FileHandle,
                                FileOffset - Adjustment);

    if (!KSUCCESS(Status)) {
        goto MapFileSectionEnd;
    }

    VaRequest->Address += Adjustment;
    Status = STATUS_SUCCESS;

MapFileSectionEnd:
    if (!KSUCCESS(Status)) {
        if (RangeAllocated != FALSE) {

            ASSERT(AccountingLockHeld != FALSE);

            UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                         UNMAP_FLAG_SEND_INVALIDATE_IPI;

            MmpFreeAccountingRange(ImageProcess->AddressSpace,
                                   VaRequest->Address,
                                   VaRequest->Size,
                                   TRUE,
                                   UnmapFlags);
        }
    }

    if (AccountingLockHeld != FALSE) {
        MmpUnlockAccountant(Realtor, TRUE);
    }

    return Status;
}

KSTATUS
MmUnmapFileSection (
    PVOID Process,
    PVOID FileMapping,
    UINTN Size,
    PMEMORY_RESERVATION Reservation
    )

/*++

Routine Description:

    This routine unmaps a file section. This routine must be called at low
    level. For kernel mode, this must specify a single whole image section.

Arguments:

    Process - Supplies a pointer to the process containing the section to
        unmap. Supply NULL to unmap from the current process.

    FileMapping - Supplies a pointer to the file mapping.

    Size - Supplies the size in bytes of the region to unmap.

    Reservation - Supplies an optional pointer to the reservation that the
        section was mapped under. If the mapping was not done under a
        memory reservation, supply NULL. If the mapping was done under a
        memory reservation, that reservation MUST be supplied here.

Return Value:

    Status code.

--*/

{

    PMEMORY_ACCOUNTING Accountant;
    BOOL AccountantLockHeld;
    PKPROCESS OwningProcess;
    UINTN PageSize;
    PIMAGE_SECTION Section;
    UINTN SectionOffset;
    KSTATUS Status;
    ULONG UnmapFlags;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    AccountantLockHeld = FALSE;
    PageSize = MmPageSize();

    //
    // The address must be page aligned.
    //

    ASSERT(IS_ALIGNED(((UINTN)FileMapping), PageSize));

    Size = ALIGN_RANGE_UP(Size, PageSize);

    ASSERT(FileMapping + Size >= FileMapping);

    OwningProcess = Process;
    if (OwningProcess == NULL) {
        OwningProcess = PsGetCurrentProcess();
    }

    //
    // If no size was supplied, look up the image section to get it, and unmap
    // to the end of the region.
    //

    if (Size == 0) {
        Status = MmpLookupSection(FileMapping,
                                  OwningProcess->AddressSpace,
                                  &Section,
                                  &SectionOffset);

        if (!KSUCCESS(Status)) {
            goto UnmapFileSectionEnd;
        }

        Size = Section->VirtualAddress + Section->Size - FileMapping;
        MmpImageSectionReleaseReference(Section);
        if (Size == 0) {
            goto UnmapFileSectionEnd;
        }
    }

    Accountant = OwningProcess->AddressSpace->Accountant;
    if (FileMapping > KERNEL_VA_START) {
        OwningProcess = PsGetKernelProcess();
        Accountant = &MmKernelVirtualSpace;

    } else {
        MmpLockAccountant(Accountant, TRUE);
        AccountantLockHeld = TRUE;
    }

    Status = MmpUnmapImageRegion(OwningProcess->AddressSpace,
                                 FileMapping,
                                 Size);

    if (!KSUCCESS(Status)) {
        goto UnmapFileSectionEnd;
    }

    //
    // If this wasn't created under a reservation, free up the space in the
    // accountant now.
    //

    if (Reservation == NULL) {
        UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                     UNMAP_FLAG_SEND_INVALIDATE_IPI;

        //
        // Do not report failures to release the accounting range. This could
        // result if the system cannot allocate more memory descriptors, but
        // by this point the region has been unmapped. Failing now might
        // indicate to the caller that section of file is still usable.
        // Releasing the accounting range before the unmap would not make sure
        // it is actually associated with a file section.
        //

        Status = MmpFreeAccountingRange(OwningProcess->AddressSpace,
                                        FileMapping,
                                        Size,
                                        AccountantLockHeld,
                                        UnmapFlags);

        ASSERT(KSUCCESS(Status));

    } else {

        ASSERT((Reservation->Process == OwningProcess) &&
               (Reservation->VirtualBase <= FileMapping) &&
               (Reservation->VirtualBase + Reservation->Size >=
                FileMapping + Size));
    }

    Status = STATUS_SUCCESS;

UnmapFileSectionEnd:
    if (AccountantLockHeld != FALSE) {
        MmpUnlockAccountant(Accountant, TRUE);
    }

    return Status;
}

INTN
MmSysMapOrUnmapMemory (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine responds to system calls from user mode requesting to map a
    file object or unmap a region of the current process' address space.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    ULONG AccessPermissions;
    PKPROCESS CurrentProcess;
    IO_OFFSET FileOffset;
    FILE_PROPERTIES FileProperties;
    PIO_HANDLE IoHandle;
    ULONG MapFlags;
    ULONG OpenFlags;
    ULONG PageSize;
    PSYSTEM_CALL_MAP_UNMAP_MEMORY Parameters;
    SET_FILE_INFORMATION Request;
    ULONG SectionFlags;
    KSTATUS Status;
    VM_ALLOCATION_PARAMETERS VaRequest;

    CurrentProcess = PsGetCurrentProcess();
    IoHandle = INVALID_HANDLE;
    PageSize = MmPageSize();
    Parameters = (PSYSTEM_CALL_MAP_UNMAP_MEMORY)SystemCallParameter;

    ASSERT(CurrentProcess != PsGetKernelProcess());
    ASSERT(IoGetCacheEntryDataSize() == PageSize);

    //
    // Align the size up to a page.
    //

    Parameters->Size = ALIGN_RANGE_UP(Parameters->Size, PageSize);

    //
    // Validate parameters. The range must be page aligned, must not go into
    // kernel space, and must not overflow.
    //

    if ((IS_ALIGNED((UINTN)Parameters->Address, PageSize) == FALSE) ||
        ((Parameters->Address + Parameters->Size) >= KERNEL_VA_START) ||
        ((Parameters->Address + Parameters->Size) < Parameters->Address)) {

        Status = STATUS_INVALID_PARAMETER;
        goto SysMapOrUnmapMemoryEnd;
    }

    //
    // If this is a map operation, then validate the parameters and map the
    // specified section of the file.
    //

    if (Parameters->Map != FALSE) {
        FileOffset = 0;
        MapFlags = Parameters->Flags;
        SectionFlags = IMAGE_SECTION_MAP_SYSTEM_CALL;
        VaRequest.Strategy = AllocationStrategyHighestAddress;

        //
        // The offset must be page-aligned.
        //

        if (IS_ALIGNED(Parameters->Offset, PageSize) == FALSE) {
            Status = STATUS_INVALID_PARAMETER;
            goto SysMapOrUnmapMemoryEnd;
        }

        //
        // The offset and size must not overflow.
        //

        if (Parameters->Offset + Parameters->Size < Parameters->Offset) {
            Status = STATUS_INVALID_PARAMETER;
            goto SysMapOrUnmapMemoryEnd;
        }

        //
        // Non-anonymous mapping requests must provide an image handle.
        // Validate it.
        //

        if ((MapFlags & SYS_MAP_FLAG_ANONYMOUS) == 0) {

            //
            // Fail if an invalid handle was supplied.
            //

            IoHandle = ObGetHandleValue(CurrentProcess->HandleTable,
                                        Parameters->Handle,
                                        NULL);

            if (IoHandle == NULL) {
                Status = STATUS_INVALID_HANDLE;
                goto SysMapOrUnmapMemoryEnd;
            }

            FileOffset = Parameters->Offset;

            //
            // If no size was supplied, try to map the whole thing.
            //

            if ((Parameters->Size == 0) &&
                ((MapFlags & SYS_MAP_FLAG_ANONYMOUS) == 0)) {

                Status = IoGetFileInformation(IoHandle, &FileProperties);
                if (!KSUCCESS(Status)) {
                    goto SysMapOrUnmapMemoryEnd;
                }

                Parameters->Size = FileProperties.Size;
                if (Parameters->Offset + Parameters->Size <=
                    Parameters->Offset) {

                    Status = STATUS_INVALID_PARAMETER;
                    goto SysMapOrUnmapMemoryEnd;
                }
            }

        //
        // Shared anonymous sections are backed by an un-named shared memory
        // object. Create one.
        //

        } else if ((MapFlags & SYS_MAP_FLAG_SHARED) != 0) {
            if (Parameters->Size == 0) {
                Status = STATUS_INVALID_PARAMETER;
                goto SysMapOrUnmapMemoryEnd;
            }

            AccessPermissions = 0;
            if ((MapFlags & SYS_MAP_FLAG_READ) != 0) {
                AccessPermissions |= IO_ACCESS_READ;
            }

            if ((MapFlags & SYS_MAP_FLAG_WRITE) != 0) {
                AccessPermissions |= IO_ACCESS_READ;
                AccessPermissions |= IO_ACCESS_WRITE;
            }

            if ((MapFlags & SYS_MAP_FLAG_EXECUTE) != 0) {
                AccessPermissions |= IO_ACCESS_READ;
                AccessPermissions |= IO_ACCESS_EXECUTE;
            }

            OpenFlags = OPEN_FLAG_CREATE |
                        OPEN_FLAG_FAIL_IF_EXISTS |
                        OPEN_FLAG_SHARED_MEMORY;

            Status = IoOpen(FALSE,
                            NULL,
                            NULL,
                            0,
                            AccessPermissions,
                            OpenFlags,
                            FILE_PERMISSION_NONE,
                            &IoHandle);

            if (!KSUCCESS(Status)) {
                goto SysMapOrUnmapMemoryEnd;
            }

            //
            // Now make the shared memory object the desired size.
            //

            Request.FieldsToSet = FILE_PROPERTY_FIELD_FILE_SIZE;
            Request.FileProperties = &FileProperties;
            FileProperties.Size = Parameters->Size;
            Status = IoSetFileInformation(FALSE, IoHandle, &Request);
            if (!KSUCCESS(Status)) {
                goto SysMapOrUnmapMemoryEnd;
            }

            ASSERT(FileOffset == 0);
        }

        //
        // Check and set the access permissions.
        //

        if ((MapFlags & SYS_MAP_FLAG_READ) != 0) {
            SectionFlags |= IMAGE_SECTION_READABLE;
        }

        if ((MapFlags & SYS_MAP_FLAG_EXECUTE) != 0) {
            SectionFlags |= IMAGE_SECTION_EXECUTABLE;
            SectionFlags |= IMAGE_SECTION_READABLE;
        }

        //
        // Write permission is not allowed if the I/O handle does not have
        // write permission and a shared mapping was requested.
        //

        if ((MapFlags & SYS_MAP_FLAG_WRITE) != 0) {
            SectionFlags |= IMAGE_SECTION_WRITABLE;
            SectionFlags |= IMAGE_SECTION_READABLE;
        }

        //
        // Check the mapping attributes.
        //

        if ((MapFlags & SYS_MAP_FLAG_SHARED) != 0) {
            SectionFlags |= IMAGE_SECTION_SHARED;
        }

        //
        // If the fixed flag was supplied, then the requested address must be
        // page-aligned and in user mode, but not NULL.
        //

        if ((MapFlags & SYS_MAP_FLAG_FIXED) != 0) {
            VaRequest.Strategy = AllocationStrategyFixedAddressClobber;
            if ((IS_ALIGNED((UINTN)Parameters->Address, PageSize) == FALSE) ||
                ((Parameters->Address + Parameters->Size) >= KERNEL_VA_START) ||
                (Parameters->Address == NULL)) {

                Status = STATUS_INVALID_PARAMETER;
                goto SysMapOrUnmapMemoryEnd;
            }
        }

        Parameters->Size = ALIGN_RANGE_UP(Parameters->Size, PageSize);
        VaRequest.Address = Parameters->Address;
        VaRequest.Size = Parameters->Size;
        VaRequest.Alignment = 0;
        VaRequest.Min = 0;
        VaRequest.Max = CurrentProcess->AddressSpace->MaxMemoryMap;
        VaRequest.MemoryType = MemoryTypeReserved;
        Status = MmMapFileSection(IoHandle,
                                  FileOffset,
                                  &VaRequest,
                                  SectionFlags,
                                  FALSE,
                                  NULL);

        Parameters->Address = VaRequest.Address;
        Parameters->Size = VaRequest.Size;

    //
    // Otherwise search through the current process' list of image sections and
    // destroy any sections that overlap with the specified address region.
    //

    } else {

        //
        // The address must be valid.
        //

        if (Parameters->Address == NULL) {
            Status = STATUS_INVALID_PARAMETER;
            goto SysMapOrUnmapMemoryEnd;
        }

        Status = MmUnmapFileSection(CurrentProcess,
                                    Parameters->Address,
                                    Parameters->Size,
                                    NULL);

        if (!KSUCCESS(Status)) {
            goto SysMapOrUnmapMemoryEnd;
        }
    }

SysMapOrUnmapMemoryEnd:
    if ((IoHandle != NULL) && (IoHandle != INVALID_HANDLE)) {
        IoIoHandleReleaseReference(IoHandle);
    }

    return Status;
}

INTN
MmSysSetMemoryProtection (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine responds to system calls from user mode requesting to change
    memory region attributes.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    UINTN PageSize;
    PSYSTEM_CALL_SET_MEMORY_PROTECTION Parameters;
    ULONG SectionFlags;
    KSTATUS Status;

    Parameters = SystemCallParameter;
    PageSize = MmPageSize();

    //
    // Align the size up to a page.
    //

    Parameters->Size = ALIGN_RANGE_UP(Parameters->Size, PageSize);

    //
    // Validate parameters. The range must be page aligned, must not go into
    // kernel space, and must not overflow.
    //

    if ((IS_ALIGNED((UINTN)Parameters->Address, PageSize) == FALSE) ||
        (Parameters->Address == NULL) ||
        ((Parameters->Address + Parameters->Size) >= KERNEL_VA_START) ||
        ((Parameters->Address + Parameters->Size) <= Parameters->Address)) {

        Status = STATUS_INVALID_PARAMETER;
        goto SysSetMemoryProtectionEnd;
    }

    SectionFlags = 0;
    if ((Parameters->NewAttributes & SYS_MAP_FLAG_READ) != 0) {
        SectionFlags |= IMAGE_SECTION_READABLE;
    }

    if ((Parameters->NewAttributes & SYS_MAP_FLAG_WRITE) != 0) {
        SectionFlags |= IMAGE_SECTION_WRITABLE;
    }

    if ((Parameters->NewAttributes & SYS_MAP_FLAG_EXECUTE) != 0) {
        SectionFlags |= IMAGE_SECTION_EXECUTABLE;
    }

    Status = MmChangeImageSectionRegionAccess(Parameters->Address,
                                              Parameters->Size,
                                              SectionFlags);

SysSetMemoryProtectionEnd:
    return Status;
}

INTN
MmSysFlushMemory (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine responds to system calls from user mode requesting to flush a
    region of memory in the current process' to permanent storage.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PADDRESS_SPACE AddressSpace;
    ULONGLONG AlignedSize;
    PLIST_ENTRY CurrentEntry;
    PIMAGE_SECTION CurrentSection;
    ULONG Flags;
    BOOL LockHeld;
    ULONG OverlapPageCount;
    ULONG OverlapPageOffset;
    UINTN OverlapSize;
    PVOID OverlapStart;
    ULONG PageShift;
    ULONG PageSize;
    PSYSTEM_CALL_FLUSH_MEMORY Parameters;
    PKPROCESS Process;
    PIMAGE_SECTION ReleaseSection;
    PVOID SectionEnd;
    PVOID SectionStart;
    KSTATUS Status;
    PVOID SyncRegionEnd;
    PVOID SyncRegionStart;
    ULONGLONG TotalSyncSize;

    PageShift = MmPageShift();
    PageSize = MmPageSize();
    Parameters = (PSYSTEM_CALL_FLUSH_MEMORY)SystemCallParameter;
    ReleaseSection = NULL;

    //
    // The address must be non-zero and a page-aligned.
    //

    if ((Parameters->Address == NULL) ||
        (IS_ALIGNED((UINTN)Parameters->Address, PageSize) == FALSE)) {

        Status = STATUS_INVALID_PARAMETER;
        goto SysSyncMemoryEnd;
    }

    //
    // A valid size must be supplied.
    //

    if (Parameters->Size == 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto SysSyncMemoryEnd;
    }

    //
    // If the specified range is not all within user mode, then fail.
    //

    if ((Parameters->Address + Parameters->Size) > KERNEL_VA_START) {
        Status = STATUS_INVALID_ADDRESS_RANGE;
        goto SysSyncMemoryEnd;
    }

    //
    // Convert the flags.
    //

    Flags = 0;
    if ((Parameters->Flags & SYS_MAP_FLUSH_FLAG_ASYNC) != 0) {
        Flags |= IMAGE_SECTION_FLUSH_FLAG_ASYNC;
    }

    //
    // Loop over the current process' image sections, synchronizing any that
    // overlap and were created via the map system call.
    //

    AlignedSize = ALIGN_RANGE_UP(Parameters->Size, PageSize);
    Status = STATUS_SUCCESS;
    TotalSyncSize = 0;
    Process = PsGetCurrentProcess();
    AddressSpace = Process->AddressSpace;
    SyncRegionStart = Parameters->Address;
    SyncRegionEnd = SyncRegionStart + AlignedSize;
    MmAcquireAddressSpaceLock(AddressSpace);
    LockHeld = TRUE;
    CurrentEntry = AddressSpace->SectionListHead.Next;
    while (CurrentEntry != &(AddressSpace->SectionListHead)) {
        CurrentSection = LIST_VALUE(CurrentEntry,
                                    IMAGE_SECTION,
                                    AddressListEntry);

        //
        // If the image section was not created as a result of the map
        // system call, then skip it.
        //

        if ((CurrentSection->Flags & IMAGE_SECTION_MAP_SYSTEM_CALL) == 0) {
            CurrentEntry = CurrentEntry->Next;
            continue;
        }

        //
        // If this section does not overlap with the specified region, it
        // can be skipped.
        //

        SectionStart = CurrentSection->VirtualAddress;
        SectionEnd = SectionStart + CurrentSection->Size;
        if ((SectionStart >= SyncRegionEnd) ||
            (SectionEnd <= SyncRegionStart)) {

            CurrentEntry = CurrentEntry->Next;
            continue;
        }

        //
        // Determine how much of this image section overlaps with the specified
        // region to synchronize.
        //

        OverlapStart = SectionStart;
        if (SectionStart < SyncRegionStart) {
            OverlapStart = SyncRegionStart;
        }

        if (SectionEnd < SyncRegionEnd) {
            OverlapSize = SectionEnd - OverlapStart;

        } else {
            OverlapSize = SyncRegionEnd - OverlapStart;
        }

        ASSERT(OverlapSize != 0);

        TotalSyncSize += OverlapSize;

        //
        // If the image section is not backed, shared, and writable, then
        // there is nothing to synchronize.
        //

        if (((CurrentSection->Flags & IMAGE_SECTION_SHARED) == 0) ||
            ((CurrentSection->Flags & IMAGE_SECTION_BACKED) == 0) ||
            ((CurrentSection->Flags & IMAGE_SECTION_WAS_WRITABLE) == 0)) {

            CurrentEntry = CurrentEntry->Next;
            continue;
        }

        //
        // Release the lock and process the current section.
        //

        MmpImageSectionAddReference(CurrentSection);
        MmReleaseAddressSpaceLock(AddressSpace);
        LockHeld = FALSE;

        //
        // Release the reference on the last section that was processed.
        //

        if (ReleaseSection != NULL) {
            MmpImageSectionReleaseReference(ReleaseSection);
            ReleaseSection = NULL;
        }

        ReleaseSection = CurrentSection;

        //
        // Flush the overlapping region of the image section to its backing
        // image.
        //

        OverlapPageCount = OverlapSize >> PageShift;
        OverlapPageOffset = (OverlapStart - SectionStart) >> PageShift;
        Status = MmpFlushImageSectionRegion(CurrentSection,
                                            OverlapPageOffset,
                                            OverlapPageCount,
                                            Flags);

        if (!KSUCCESS(Status)) {
            goto SysSyncMemoryEnd;
        }

        //
        // If the image section matched exactly, there should be nothing
        // else to process, just exit.
        //

        if ((SectionStart == SyncRegionStart) &&
            (SectionEnd == SyncRegionEnd)) {

            ASSERT(TotalSyncSize == AlignedSize);

            break;
        }

        //
        // Reacquire the lock and try to continue forward in the image section
        // list. If the current image section was removed, restart from the
        // beginning.
        //

        MmAcquireAddressSpaceLock(AddressSpace);
        LockHeld = TRUE;
        if (CurrentSection->AddressListEntry.Next == NULL) {
            CurrentEntry = AddressSpace->SectionListHead.Next;

        } else {
            CurrentEntry = CurrentEntry->Next;
        }
    }

    if (LockHeld != FALSE) {
        MmReleaseAddressSpaceLock(AddressSpace);
        LockHeld = FALSE;
    }

    //
    // If the total number of bytes synchronized does not match the requested
    // size, then some portion of the requested range was invalid.
    //

    if (TotalSyncSize != AlignedSize) {
        Status = STATUS_INVALID_ADDRESS_RANGE;
        goto SysSyncMemoryEnd;
    }

SysSyncMemoryEnd:
    if (ReleaseSection != NULL) {
        MmpImageSectionReleaseReference(ReleaseSection);
    }

    return Status;
}

INTN
MmSysSetBreak (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call for getting or modifying the
    program break.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PADDRESS_SPACE AddressSpace;
    PVOID NewBreak;
    PVOID NewBreakAligned;
    PVOID OldBreak;
    ULONG PageSize;
    PSYSTEM_CALL_SET_BREAK Parameters;
    PKPROCESS Process;
    ULONG SectionFlags;
    KSTATUS Status;
    VM_ALLOCATION_PARAMETERS VaRequest;

    Parameters = SystemCallParameter;
    NewBreak = Parameters->Break;
    PageSize = MmPageSize();
    Process = PsGetCurrentProcess();
    AddressSpace = Process->AddressSpace;

    ASSERT((AddressSpace->BreakStart != NULL) &&
           (AddressSpace->BreakEnd >= AddressSpace->BreakStart) &&
           (IS_POINTER_ALIGNED(AddressSpace->BreakStart, PageSize)));

    NewBreakAligned = ALIGN_POINTER_UP(NewBreak, PageSize);
    if (NewBreakAligned < AddressSpace->BreakStart) {
        goto SysSetBreakEnd;
    }

    OldBreak = ALIGN_POINTER_UP(AddressSpace->BreakEnd, PageSize);
    if (NewBreakAligned < OldBreak) {
        Status = MmUnmapFileSection(Process,
                                    NewBreakAligned,
                                    OldBreak - NewBreakAligned,
                                    NULL);

    } else {
        VaRequest.Address = OldBreak;
        VaRequest.Size = NewBreakAligned - VaRequest.Address;
        VaRequest.Alignment = MmPageSize();
        VaRequest.Min = 0;
        VaRequest.Max = AddressSpace->MaxMemoryMap;
        VaRequest.MemoryType = MemoryTypeReserved;
        VaRequest.Strategy = AllocationStrategyFixedAddress;
        SectionFlags = IMAGE_SECTION_READABLE | IMAGE_SECTION_WRITABLE |
                       IMAGE_SECTION_MAP_SYSTEM_CALL;

        Status = MmMapFileSection(INVALID_HANDLE,
                                  0,
                                  &VaRequest,
                                  SectionFlags,
                                  FALSE,
                                  NULL);
    }

    if (KSUCCESS(Status)) {

        //
        // This is racy, as multiple calls to this routine could be occurring
        // in parallel. User mode is the only one that would care if this
        // value were wrong, so whatever.
        //

        AddressSpace->BreakEnd = NewBreak;
    }

SysSetBreakEnd:
    Parameters->Break = AddressSpace->BreakEnd;
    return STATUS_SUCCESS;
}

VOID
MmCleanUpProcessMemory (
    PVOID ExitedProcess
    )

/*++

Routine Description:

    This routine cleans up any leftover allocations made under the given
    process.

Arguments:

    ExitedProcess - Supplies a pointer to the process to clean up.

Return Value:

    None.

--*/

{

    PKPROCESS Process;
    KSTATUS Status;

    Process = ExitedProcess;

    ASSERT((Process != NULL) && (Process != PsGetKernelProcess()));
    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Images should have been cleaned up by the last thread to terminate.
    //

    ASSERT(LIST_EMPTY(&(Process->ImageListHead)) != FALSE);

    Status = MmpUnmapImageRegion(Process->AddressSpace,
                                 (PVOID)0,
                                 (UINTN)KERNEL_VA_START);

    ASSERT(KSUCCESS(Status));

    ASSERT(LIST_EMPTY(&(Process->AddressSpace->SectionListHead)) != FALSE);

    return;
}

