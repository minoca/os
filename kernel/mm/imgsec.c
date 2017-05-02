/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    imgsec.c

Abstract:

    This module implements image section support in the kernel.

Author:

    Chris Stevens 6-Mar-2014

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
// --------------------------------------------------------------------- Macros
//

//
// This macro asserts that the touched boundaries of the section lie within the
// section.
//

#define ASSERT_SECTION_TOUCH_BOUNDARIES(_Section) \
    ASSERT(((_Section)->MinTouched >= (_Section)->VirtualAddress) && \
           ((_Section)->MinTouched <= \
            (_Section)->VirtualAddress + (_Section)->Size) && \
           ((_Section)->MaxTouched >= (_Section)->VirtualAddress) && \
           ((_Section)->MaxTouched <= \
            (_Section)->VirtualAddress + (_Section)->Size))

//
// ---------------------------------------------------------------- Definitions
//

/*++

Structure Description:

    This defines a list of image sections that are all backed by the same file.

Members:

    ListHead - Stores pointers to the first and last image sections in the
        list.

    Lock - Stores a pointer to a lock that protects access to the list.

--*/

struct _IMAGE_SECTION_LIST {
    LIST_ENTRY ListHead;
    PQUEUED_LOCK Lock;
};

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
MmpAllocateImageSection (
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress,
    UINTN Size,
    ULONG Flags,
    HANDLE ImageHandle,
    IO_OFFSET ImageOffset,
    PIMAGE_SECTION *AllocatedSection
    );

KSTATUS
MmpClipImageSection (
    PLIST_ENTRY SectionListHead,
    PVOID Address,
    UINTN Size,
    PIMAGE_SECTION Section
    );

VOID
MmpRemoveImageSection (
    PIMAGE_SECTION Section,
    BOOL AddressSpaceLockHeld
    );

VOID
MmpDeleteImageSection (
    PIMAGE_SECTION ImageSection
    );

KSTATUS
MmpChangeImageSectionAccess (
    PIMAGE_SECTION Section,
    ULONG NewAccess
    );

KSTATUS
MmpUnmapImageSection (
    PIMAGE_SECTION Section,
    UINTN PageOffset,
    UINTN PageCount,
    ULONG Flags
    );

BOOL
MmpIsImageSectionMapped (
    PIMAGE_SECTION Section,
    ULONG PageOffset,
    PPHYSICAL_ADDRESS PhysicalAddress
    );

VOID
MmpDestroyImageSectionMappings (
    PIMAGE_SECTION Section
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the kernel's address space context.
//

PADDRESS_SPACE MmKernelAddressSpace;

//
// ------------------------------------------------------------------ Functions
//

PADDRESS_SPACE
MmCreateAddressSpace (
    VOID
    )

/*++

Routine Description:

    This routine creates a new address space context. This routine allocates
    the structure, zeros at least the common portion, and initializes any
    architecture specific members after the common potion.

Arguments:

    None.

Return Value:

    Returns a pointer to the new address space on success.

    NULL on allocation failure.

--*/

{

    ULONG AccountingFlags;
    PADDRESS_SPACE Space;
    KSTATUS Status;

    Space = MmpArchCreateAddressSpace();
    if (Space == NULL) {
        return NULL;
    }

    INITIALIZE_LIST_HEAD(&(Space->SectionListHead));
    if (MmKernelAddressSpace == NULL) {
        MmKernelAddressSpace = Space;
        Space->Accountant = &MmKernelVirtualSpace;
        Status = STATUS_SUCCESS;

    } else {
        Space->Accountant = MmAllocatePagedPool(
                                              sizeof(MEMORY_ACCOUNTING),
                                              MM_ADDRESS_SPACE_ALLOCATION_TAG);

        if (Space->Accountant == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateAddressSpaceEnd;
        }

        AccountingFlags = MEMORY_ACCOUNTING_FLAG_NO_MAP;
        Status = MmInitializeMemoryAccounting(Space->Accountant,
                                              AccountingFlags);

        if (!KSUCCESS(Status)) {
            goto CreateAddressSpaceEnd;
        }
    }

    Space->Lock = KeCreateQueuedLock();
    if (Space->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateAddressSpaceEnd;
    }

CreateAddressSpaceEnd:
    if (!KSUCCESS(Status)) {
        if (Space != NULL) {
            MmDestroyAddressSpace(Space);
            Space = NULL;
        }
    }

    return Space;
}

VOID
MmDestroyAddressSpace (
    PADDRESS_SPACE AddressSpace
    )

/*++

Routine Description:

    This routine destroys an address space, freeing this structure and all
    architecture-specific content. The common portion of the structure will
    already have been taken care of.

Arguments:

    AddressSpace - Supplies a pointer to the address space to destroy.

Return Value:

    None.

--*/

{

    if (AddressSpace == NULL) {
        return;
    }

    if (AddressSpace->Accountant != NULL) {
        MmDestroyMemoryAccounting(AddressSpace->Accountant);
        MmFreePagedPool(AddressSpace->Accountant);
    }

    if (AddressSpace->Lock != NULL) {
        KeDestroyQueuedLock(AddressSpace->Lock);
    }

    MmpArchDestroyAddressSpace(AddressSpace);
    return;
}

PIMAGE_SECTION_LIST
MmCreateImageSectionList (
    VOID
    )

/*++

Routine Description:

    This routine creates an image section list.

Arguments:

    None.

Return Value:

    Returns a pointer to the new image section list.

--*/

{

    PIMAGE_SECTION_LIST ImageSectionList;

    ImageSectionList = MmAllocatePagedPool(sizeof(IMAGE_SECTION_LIST),
                                           MM_ALLOCATION_TAG);

    if (ImageSectionList == NULL) {
        return NULL;
    }

    INITIALIZE_LIST_HEAD(&(ImageSectionList->ListHead));
    ImageSectionList->Lock = KeCreateQueuedLock();
    if (ImageSectionList->Lock == NULL) {
        MmFreePagedPool(ImageSectionList);
        return NULL;
    }

    return ImageSectionList;
}

VOID
MmDestroyImageSectionList (
    PIMAGE_SECTION_LIST ImageSectionList
    )

/*++

Routine Description:

    This routine destroys an image section list.

Arguments:

    ImageSectionList - Supplies a pointer to the image section list to destroy.

Return Value:

    None.

--*/

{

    ASSERT(LIST_EMPTY(&(ImageSectionList->ListHead)) != FALSE);

    KeDestroyQueuedLock(ImageSectionList->Lock);
    MmFreePagedPool(ImageSectionList);
    return;
}

KSTATUS
MmUnmapImageSectionList (
    PIMAGE_SECTION_LIST ImageSectionList,
    IO_OFFSET Offset,
    ULONGLONG Size,
    ULONG Flags
    )

/*++

Routine Description:

    This routine unmaps all pages in each image section in the given image
    section list starting at the given offset and for the supplied size.

Arguments:

    ImageSectionList - Supplies a pointer to an image section list.

    Offset - Supplies the start offset beyond which all mappings in each image
        section will be unmapped. The offset should be page aligned.

    Size - Supplies the size of the region to unmap, in bytes. The size should
        be page aligned. Supply -1 to unmap everything after the given offset.

    Flags - Supplies a bitmask of flags for the unmap. See
        IMAGE_SECTION_UNMAP_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PIMAGE_SECTION CurrentSection;
    IO_OFFSET EndOffset;
    UINTN PageCount;
    UINTN PageOffset;
    ULONG PageShift;
    PIMAGE_SECTION ReleaseSection;
    IO_OFFSET StartOffset;
    KSTATUS Status;
    IO_OFFSET UnmapEndOffset;
    IO_OFFSET UnmapStartOffset;

    if (LIST_EMPTY(&(ImageSectionList->ListHead))) {
        return STATUS_SUCCESS;
    }

    PageShift = MmPageShift();
    ReleaseSection = NULL;

    ASSERT(IS_ALIGNED(Offset, MmPageSize()) != FALSE);
    ASSERT((Size == -1ULL) || (IS_ALIGNED(Size, MmPageSize()) != FALSE));
    ASSERT((Size == -1ULL) || ((Offset + Size) > Offset));

    //
    // Iterate over the sections in the list. Sections are added to the list
    // such that children are processed after their parent. Without this order
    // a section copy could be created after the loop starts, get added to the
    // beginning and then not get unmapped.
    //

    UnmapStartOffset = Offset;
    UnmapEndOffset = IO_OFFSET_MAX;
    if (Size != -1ULL) {
        UnmapEndOffset = UnmapStartOffset + Size;
    }

    KeAcquireQueuedLock(ImageSectionList->Lock);
    CurrentEntry = ImageSectionList->ListHead.Next;
    while (CurrentEntry != &(ImageSectionList->ListHead)) {
        CurrentSection = LIST_VALUE(CurrentEntry,
                                    IMAGE_SECTION,
                                    ImageListEntry);

        ASSERT((CurrentSection->Flags & IMAGE_SECTION_BACKED) != 0);
        ASSERT(CurrentSection->ImageBacking.DeviceHandle != INVALID_HANDLE);

        //
        // Find the bounds of the image section. If the image section is
        // outside the bounds of the region to unmap, then skip it.
        //

        StartOffset = CurrentSection->ImageBacking.Offset;
        EndOffset = StartOffset + CurrentSection->Size;
        if ((EndOffset <= UnmapStartOffset) ||
            (StartOffset >= UnmapEndOffset)) {

            CurrentEntry = CurrentEntry->Next;
            continue;
        }

        //
        // Reference the section and then release the image section list lock.
        //

        MmpImageSectionAddReference(CurrentSection);
        KeReleaseQueuedLock(ImageSectionList->Lock);

        //
        // Release the reference on the previously checked section now that the
        // lock is released.
        //

        if (ReleaseSection != NULL) {
            MmpImageSectionReleaseReference(ReleaseSection);
            ReleaseSection = NULL;
        }

        KeAcquireQueuedLock(CurrentSection->Lock);
        EndOffset = StartOffset + CurrentSection->Size;

        //
        // Determine the number of pages to unmap.
        //

        if (StartOffset < UnmapStartOffset) {
            PageOffset = (UnmapStartOffset - StartOffset) >> PageShift;
            if (EndOffset < UnmapEndOffset) {
                PageCount = (EndOffset - UnmapStartOffset) >> PageShift;

            } else {
                PageCount = (UnmapEndOffset - UnmapStartOffset) >> PageShift;
            }

        } else {
            PageOffset = 0;
            if (EndOffset < UnmapEndOffset) {
                PageCount = CurrentSection->Size >> PageShift;

            } else {
                PageCount = (UnmapEndOffset - StartOffset) >> PageShift;
            }
        }

        //
        // Unmap the pages from the current image section.
        //

        Status = MmpUnmapImageSection(CurrentSection,
                                      PageOffset,
                                      PageCount,
                                      Flags);

        KeReleaseQueuedLock(CurrentSection->Lock);
        if (!KSUCCESS(Status)) {
            MmpImageSectionReleaseReference(CurrentSection);
            goto UnmapImageSectionListEnd;
        }

        //
        // Reacquire the section list lock and move to the next section.
        //

        KeAcquireQueuedLock(ImageSectionList->Lock);

        //
        // A reference was taken on the section so that it could not be
        // destroyed but it may have disappeared. Tread lightly. If the next
        // pointer was set to NULL by the destroy routine, then start over from
        // the beginning.
        //

        ASSERT(CurrentEntry == &(CurrentSection->ImageListEntry));

        if (CurrentSection->ImageListEntry.Next == NULL) {
            CurrentEntry = ImageSectionList->ListHead.Next;

        //
        // Otherwise move to the next entry in the list.
        //

        } else {
            CurrentEntry = CurrentEntry->Next;
        }

        //
        // Remember the current section so that the reference taken on it above
        // can be released once the list's spin lock is released.
        //

        ASSERT(ReleaseSection == NULL);

        ReleaseSection = CurrentSection;
    }

    KeReleaseQueuedLock(ImageSectionList->Lock);
    Status = STATUS_SUCCESS;
    if (ReleaseSection != NULL) {
        MmpImageSectionReleaseReference(ReleaseSection);
    }

UnmapImageSectionListEnd:
    return Status;
}

KSTATUS
MmChangeImageSectionRegionAccess (
    PVOID Address,
    UINTN Size,
    ULONG NewAccess
    )

/*++

Routine Description:

    This routine sets the memory region protection for the given address range.

Arguments:

    Address - Supplies the starting address of the region to change.

    Size - Supplies the size of the region to change.

    NewAccess - Supplies the new access permissions to set. See IMAGE_SECTION_*
        definitions. Only the read, write, and execute flags can be changed.

Return Value:

    Status code.

--*/

{

    PADDRESS_SPACE AddressSpace;
    PLIST_ENTRY CurrentEntry;
    PVOID End;
    UINTN PageSize;
    PKPROCESS Process;
    PIMAGE_SECTION Section;
    PVOID SectionEnd;
    KSTATUS Status;

    PageSize = MmPageSize();

    ASSERT((NewAccess & ~(IMAGE_SECTION_ACCESS_MASK)) == 0);
    ASSERT(IS_ALIGNED((UINTN)Address | Size, PageSize));

    Process = PsGetCurrentProcess();
    AddressSpace = Process->AddressSpace;
    MmAcquireAddressSpaceLock(AddressSpace);
    Status = STATUS_SUCCESS;
    End = Address + Size;
    CurrentEntry = AddressSpace->SectionListHead.Next;
    while (CurrentEntry != &(AddressSpace->SectionListHead)) {
        Section = LIST_VALUE(CurrentEntry, IMAGE_SECTION, AddressListEntry);
        if (Section->VirtualAddress >= End) {
            break;
        }

        //
        // Move on before changing the section as the section may get split.
        // Don't bother the section if the attributes already agree.
        //

        CurrentEntry = CurrentEntry->Next;
        SectionEnd = Section->VirtualAddress + Section->Size;
        if ((SectionEnd > Address) &&
            (((Section->Flags ^ NewAccess) & IMAGE_SECTION_ACCESS_MASK) != 0)) {

            //
            // If the region only covers part of the section, then the section
            // will need to be split. This is not supported in kernel mode,
            // kernel callers are required to specify whole regions only.
            //

            if ((Section->VirtualAddress < Address) || (SectionEnd > End)) {
                if (Section->AddressSpace == MmKernelAddressSpace) {

                    ASSERT(FALSE);

                    Status = STATUS_NOT_SUPPORTED;
                    break;
                }

                //
                // Split the portion of the section that doesn't apply to this
                // region.
                //

                if (Section->VirtualAddress < Address) {
                    Status = MmpClipImageSection(
                                              &(AddressSpace->SectionListHead),
                                              Address,
                                              0,
                                              Section);

                    if (!KSUCCESS(Status)) {
                        break;
                    }

                    ASSERT(Section->VirtualAddress + Section->Size == Address);

                    CurrentEntry = Section->AddressListEntry.Next;
                    continue;
                }

                //
                // Clip a region of the section with size zero to break up the
                // section.
                //

                Status = MmpClipImageSection(&(AddressSpace->SectionListHead),
                                             End,
                                             0,
                                             Section);

                if (!KSUCCESS(Status)) {
                    break;
                }

                ASSERT(Section->VirtualAddress + Section->Size == End);

                CurrentEntry = Section->AddressListEntry.Next;
            }

            ASSERT((Section->VirtualAddress >= Address) &&
                   ((Section->VirtualAddress + Section->Size) <= End));

            Status = MmpChangeImageSectionAccess(Section, NewAccess);
            if (!KSUCCESS(Status)) {
                break;
            }
        }
    }

    MmReleaseAddressSpaceLock(AddressSpace);
    return Status;
}

PVOID
MmGetObjectForAddress (
    PVOID Address,
    PUINTN Offset,
    PBOOL Shared
    )

/*++

Routine Description:

    This routine returns a pointer to the object backing the memory at the
    given user mode address. This is an opaque object with an increased
    reference count on it.

Arguments:

    Address - Supplies the user mode address to look up.

    Offset - Supplies a pointer where the offset in bytes from the base of the
        object's virtual region will be returned.

    Shared - Supplies a pointer indicating whether the memory is a shared file
        mapping (TRUE) or either a private file mapping or just general
        memory (FALSE).

Return Value:

    Returns a pointer to the object that owns this user mode address for the
    current process. The caller must release the reference held on this object.

    NULL if the address passed in is invalid or not mapped.

--*/

{

    PVOID FileObject;
    UINTN PageOffset;
    PIMAGE_SECTION Section;
    UINTN SectionOffset;
    KSTATUS Status;

    if (Address >= KERNEL_VA_START) {
        return NULL;
    }

    Status = MmpLookupSection(Address,
                              PsGetCurrentProcess()->AddressSpace,
                              &Section,
                              &PageOffset);

    if (!KSUCCESS(Status)) {
        return NULL;
    }

    SectionOffset = Address - Section->VirtualAddress;
    if ((Section->Flags & IMAGE_SECTION_SHARED) != 0) {

        ASSERT(Section->ImageBacking.DeviceHandle != INVALID_HANDLE);

        FileObject = IoReferenceFileObjectForHandle(
                                           Section->ImageBacking.DeviceHandle);

        //
        // This does truncate the offset on 32-bit systems. So far this is
        // only used by user mode locks, for which the truncation seems like
        // not a huge deal.
        //

        *Offset = Section->ImageBacking.Offset + SectionOffset;
        *Shared = TRUE;
        MmpImageSectionReleaseReference(Section);
        return FileObject;
    }

    *Offset = SectionOffset;
    *Shared = FALSE;
    return Section;
}

VOID
MmReleaseObjectReference (
    PVOID Object,
    BOOL Shared
    )

/*++

Routine Description:

    This routine releases the reference acquired by getting the object for a
    user mode address.

Arguments:

    Object - Supplies a pointer to the object returned when the address was
        looked up.

    Shared - Supplies the shared boolean that was returned when the address was
        looked up. This is needed to know how to release the object.

Return Value:

    None.

--*/

{

    if (Shared != FALSE) {
        IoFileObjectReleaseReference(Object);

    } else {
        MmpImageSectionReleaseReference(Object);
    }

    return;
}

KSTATUS
MmUserModeDebuggerWrite (
    PVOID KernelBuffer,
    PVOID UserDestination,
    UINTN Size
    )

/*++

Routine Description:

    This routine performs a user mode debugger write to the current
    process memory. This routine may convert a read-only image section it
    finds to a writable section.

Arguments:

    KernelBuffer - Supplies a pointer to the kernel-mode buffer containing
        the data to write.

    UserDestination - Supplies the destination buffer where the contents
        should be written.

    Size - Supplies the number of bytes to write.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the buffer is not valid.

    STATUS_ACCESS_DENIED if an attempt was made to write to a read-only
    shared section.

--*/

{

    ULONG NewAccess;
    UINTN PageOffset;
    PKPROCESS Process;
    PIMAGE_SECTION Section;
    UINTN SizeThisRound;
    KSTATUS Status;

    ASSERT((UserDestination + Size < KERNEL_VA_START) &&
           (UserDestination + Size >= UserDestination));

    //
    // First make an attempt without any fanciness.
    //

    Status = MmCopyToUserMode(UserDestination, KernelBuffer, Size);
    if (KSUCCESS(Status)) {
        Status = MmSyncCacheRegion(UserDestination, Size);
        return Status;
    }

    //
    // Loop converting sections to writable.
    //

    Process = PsGetCurrentProcess();
    while (Size != 0) {
        Status = MmpLookupSection(UserDestination,
                                  Process->AddressSpace,
                                  &Section,
                                  &PageOffset);

        if (!KSUCCESS(Status)) {
            return STATUS_ACCESS_VIOLATION;
        }

        SizeThisRound = (UINTN)(Section->VirtualAddress + Section->Size) -
                        (UINTN)UserDestination;

        if (SizeThisRound > Size) {
            SizeThisRound = Size;
        }

        NewAccess = (Section->Flags | IMAGE_SECTION_WRITABLE) &
                    IMAGE_SECTION_ACCESS_MASK;

        Status = MmpChangeImageSectionAccess(Section, NewAccess);
        MmpImageSectionReleaseReference(Section);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        Status = MmCopyToUserMode(UserDestination, KernelBuffer, SizeThisRound);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        Status = MmSyncCacheRegion(UserDestination, SizeThisRound);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        KernelBuffer += SizeThisRound;
        UserDestination += SizeThisRound;
        Size -= SizeThisRound;
    }

    return STATUS_SUCCESS;
}

KSTATUS
MmpLookupSection (
    PVOID VirtualAddress,
    PADDRESS_SPACE AddressSpace,
    PIMAGE_SECTION *Section,
    PUINTN PageOffset
    )

/*++

Routine Description:

    This routine looks up the image section corresponding to the given
    virtual address. This routine must be called at low level. If the section
    is found, a reference is added to the section.

Arguments:

    VirtualAddress - Supplies the virtual address to query for.

    AddressSpace - Supplies the address space to look up the section in.

    Section - Supplies a pointer where a pointer to the image section will be
        returned.

    PageOffset - Supplies a pointer where the offset in pages from the
        beginning of the section will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if the virtual address does not map to an image section
        for this process.

--*/

{

    PIMAGE_SECTION CurrentSection;
    PLIST_ENTRY CurrentSectionEntry;
    ULONG PageShift;
    KSTATUS Status;
    ULONGLONG VirtualAddressPage;

    PageShift = MmPageShift();
    Status = STATUS_NOT_FOUND;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    MmAcquireAddressSpaceLock(AddressSpace);
    CurrentSectionEntry = AddressSpace->SectionListHead.Next;
    while (CurrentSectionEntry != &(AddressSpace->SectionListHead)) {
        CurrentSection = LIST_VALUE(CurrentSectionEntry,
                                    IMAGE_SECTION,
                                    AddressListEntry);

        CurrentSectionEntry = CurrentSectionEntry->Next;

        //
        // If the VA is inside the current section, return it.
        //

        if ((CurrentSection->VirtualAddress <= VirtualAddress) &&
            (CurrentSection->VirtualAddress +
             CurrentSection->Size > VirtualAddress)) {

            VirtualAddressPage = (UINTN)VirtualAddress >> PageShift;
            *Section = CurrentSection;
            *PageOffset = VirtualAddressPage -
                          ((UINTN)CurrentSection->VirtualAddress >>
                           PageShift);

            MmpImageSectionAddReference(CurrentSection);
            Status = STATUS_SUCCESS;
            goto LookupSectionEnd;
        }
    }

LookupSectionEnd:
    MmReleaseAddressSpaceLock(AddressSpace);
    return Status;
}

KSTATUS
MmpAddImageSection (
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress,
    UINTN Size,
    ULONG Flags,
    HANDLE ImageHandle,
    IO_OFFSET ImageOffset
    )

/*++

Routine Description:

    This routine creates an image section for the given process so that page
    faults can be recognized and handled appropriately. This routine must be
    called at low level.

Arguments:

    AddressSpace - Supplies a pointer to the address space to add the section
        under.

    VirtualAddress - Supplies the virtual address of the section.

    Size - Supplies the size of the section, in bytes.

    Flags - Supplies a bitfield of flags governing this image section. See
        IMAGE_SECTION_* definitions.

    ImageHandle - Supplies an open handle to the backing image, if the section
        is backed by a file. Supply INVALID_HANDLE here for pure memory
        allocations.

    ImageOffset - Supplies the offset, in bytes, to the beginning of the
        backing with the image.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated to create
        the image or there is no more room in the page file.

--*/

{

    PLIST_ENTRY EntryBefore;
    PIMAGE_SECTION_LIST ImageSectionList;
    PIMAGE_SECTION NewSection;
    UINTN PageCount;
    UINTN PageIndex;
    ULONG PageShift;
    ULONG PageSize;
    KSTATUS Status;

    ImageSectionList = NULL;
    NewSection = NULL;
    PageSize = MmPageSize();
    PageShift = MmPageShift();

    ASSERT(POWER_OF_2(PageSize) != FALSE);

    PageCount = Size >> PageShift;

    //
    // The caller should not be supplying certain flags.
    //

    ASSERT((Flags & IMAGE_SECTION_INTERNAL_MASK) == 0);

    Status = MmpAllocateImageSection(AddressSpace,
                                     VirtualAddress,
                                     Size,
                                     Flags,
                                     ImageHandle,
                                     ImageOffset,
                                     &NewSection);

    if (!KSUCCESS(Status)) {
        goto AddImageSectionEnd;
    }

    //
    // Lock the address space and destroy any image sections that were there.
    //

    MmAcquireAddressSpaceLock(AddressSpace);
    Status = MmpClipImageSections(&(AddressSpace->SectionListHead),
                                  VirtualAddress,
                                  Size,
                                  &EntryBefore);

    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

        goto AddImageSectionEnd;
    }

    INSERT_AFTER(&(NewSection->AddressListEntry), EntryBefore);
    MmReleaseAddressSpaceLock(AddressSpace);
    if (ImageHandle != INVALID_HANDLE) {
        Status = IoNotifyFileMapping(ImageHandle, TRUE);
        if (!KSUCCESS(Status)) {
            goto AddImageSectionEnd;
        }
    }

    //
    // If the image section is non-paged and accessible, then page in and lock
    // down all the pages now.
    //

    Status = STATUS_SUCCESS;
    if (((Flags & IMAGE_SECTION_NON_PAGED) != 0) &&
        ((Flags & IMAGE_SECTION_ACCESS_MASK) != 0)) {

        for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
            Status = MmpPageIn(NewSection, PageIndex, NULL);
            if (!KSUCCESS(Status)) {

                ASSERT(Status != STATUS_TRY_AGAIN);

                break;
            }
        }

        //
        // If this fails then unlock and unmap all the paged-in pages. It
        // should not have any children as it either belongs to the current
        // process, a child of the current process (in the middle of fork) or
        // the kernel.
        //

        if (!KSUCCESS(Status)) {
            PageCount = PageIndex;
            KeAcquireQueuedLock(NewSection->Lock);

            ASSERT(LIST_EMPTY(&(NewSection->ChildList)) != FALSE);

            MmpDestroyImageSectionMappings(NewSection);
            KeReleaseQueuedLock(NewSection->Lock);
            goto AddImageSectionEnd;
        }
    }

AddImageSectionEnd:
    if (!KSUCCESS(Status)) {
        if (NewSection != NULL) {
            if (NewSection->AddressListEntry.Next != NULL) {
                MmAcquireAddressSpaceLock(AddressSpace);
                LIST_REMOVE(&(NewSection->AddressListEntry));
                MmReleaseAddressSpaceLock(AddressSpace);
                NewSection->AddressListEntry.Next = NULL;
            }

            if (NewSection->ImageListEntry.Next != NULL) {

                ASSERT(ImageSectionList != NULL);

                KeAcquireQueuedLock(ImageSectionList->Lock);
                LIST_REMOVE(&(NewSection->ImageListEntry));
                NewSection->ImageListEntry.Next = NULL;
                KeReleaseQueuedLock(ImageSectionList->Lock);
                MmpImageSectionReleaseReference(NewSection);
            }

            if (NewSection->ImageBackingReferenceCount != 0) {
                MmpImageSectionReleaseImageBackingReference(NewSection);
            }

            MmpImageSectionReleaseReference(NewSection);
            NewSection = NULL;
        }
    }

    return Status;
}

KSTATUS
MmpCopyImageSection (
    PIMAGE_SECTION SectionToCopy,
    PADDRESS_SPACE DestinationAddressSpace
    )

/*++

Routine Description:

    This routine copies an image section to another address space.

Arguments:

    SectionToCopy - Supplies a pointer to the section to copy.

    DestinationAddressSpace - Supplies a pointer to the address space to copy
        the image section to.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated to create
        the image or there is no more room in the page file.

--*/

{

    BOOL AddressLockHeld;
    ULONG AllocationSize;
    ULONG BitmapSize;
    PLIST_ENTRY CurrentEntry;
    PIMAGE_SECTION CurrentSection;
    ULONG Flags;
    PIMAGE_SECTION_LIST ImageSectionList;
    PIMAGE_SECTION NewSection;
    UINTN PageCount;
    ULONG PageMask;
    ULONG PageShift;
    ULONG PageSize;
    BOOL ParentDestroyed;
    KSTATUS Status;

    ImageSectionList = NULL;
    NewSection = NULL;
    PageSize = MmPageSize();
    PageShift = MmPageShift();
    ParentDestroyed = FALSE;

    ASSERT(POWER_OF_2(PageSize) != FALSE);

    PageMask = PageSize - 1;
    AddressLockHeld = FALSE;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((((UINTN)SectionToCopy->VirtualAddress & PageMask) == 0) &&
           ((SectionToCopy->Size & PageMask) == 0));

    //
    // Currently the copied section must be in the current process, as the
    // standard "virtual to physical" function is used to determine which pages
    // are mapped in.
    //

    ASSERT(SectionToCopy->AddressSpace == PsGetCurrentProcess()->AddressSpace);

    //
    // Copying a non-paged section is currently not supported.
    //

    ASSERT((SectionToCopy->Flags & IMAGE_SECTION_NON_PAGED) == 0);

    //
    // Shared image sections do not perform a typical copy. Because the new
    // image section does not need to inherit anything from the original image
    // section, an identical image section is just created from scratch.
    //

    if ((SectionToCopy->Flags & IMAGE_SECTION_SHARED) != 0) {

        ASSERT((SectionToCopy->Flags & IMAGE_SECTION_BACKED) != 0);
        ASSERT(SectionToCopy->ImageBacking.DeviceHandle != INVALID_HANDLE);

        Flags = SectionToCopy->Flags & IMAGE_SECTION_COPY_MASK;
        Status = MmpAddImageSection(DestinationAddressSpace,
                                    SectionToCopy->VirtualAddress,
                                    SectionToCopy->Size,
                                    Flags,
                                    SectionToCopy->ImageBacking.DeviceHandle,
                                    SectionToCopy->ImageBacking.Offset);

        goto CopyImageSectionEnd;
    }

    //
    // Create and fill out a new image section.
    //

    PageCount = SectionToCopy->Size >> PageShift;
    BitmapSize = ALIGN_RANGE_UP(PageCount, BITS_PER_BYTE * sizeof(ULONG)) /
                 BITS_PER_BYTE;

    AllocationSize = sizeof(IMAGE_SECTION) + (2 * BitmapSize);
    NewSection = MmAllocateNonPagedPool(AllocationSize,
                                        MM_IMAGE_SECTION_ALLOCATION_TAG);

    if (NewSection == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CopyImageSectionEnd;
    }

    //
    // Copy the dirty bitmap and initialize the inherit bitmap to all 1's.
    // As this is a hot path, do not zero the image section itself. The code
    // below initializes all the appropriate fields.
    //

    NewSection->DirtyPageBitmap = (PULONG)(NewSection + 1);
    NewSection->InheritPageBitmap = NewSection->DirtyPageBitmap +
                                    (BitmapSize / sizeof(ULONG));

    ASSERT(SectionToCopy->DirtyPageBitmap != NULL);

    RtlCopyMemory(NewSection->DirtyPageBitmap,
                  SectionToCopy->DirtyPageBitmap,
                  BitmapSize);

    RtlSetMemory(NewSection->InheritPageBitmap, MAX_UCHAR, BitmapSize);
    NewSection->ReferenceCount = 1;
    NewSection->Flags = SectionToCopy->Flags;
    INITIALIZE_LIST_HEAD(&(NewSection->ChildList));
    NewSection->AddressSpace = DestinationAddressSpace;
    NewSection->VirtualAddress = SectionToCopy->VirtualAddress;
    NewSection->Size = SectionToCopy->Size;
    NewSection->TruncateCount = 0;
    NewSection->SwapSpace = NULL;
    NewSection->PagingInIrp = NULL;
    NewSection->AddressListEntry.Next = NULL;
    NewSection->ImageListEntry.Next = NULL;
    NewSection->MapFlags = SectionToCopy->MapFlags;

    //
    // If the image section is backed, then it will add itself to the backing
    // image's list of image sections. Take a reference on the backing image
    // while this section is around so it is not removed while the image
    // section's entry is in the list.
    //

    if ((NewSection->Flags & IMAGE_SECTION_BACKED) != 0) {
        IoIoHandleAddReference(SectionToCopy->ImageBacking.DeviceHandle);
        NewSection->ImageBacking.DeviceHandle =
                                      SectionToCopy->ImageBacking.DeviceHandle;

        NewSection->ImageBacking.Offset = SectionToCopy->ImageBacking.Offset;

    //
    // Otherwise don't copy the device image backing, rely on the parent to
    // provide those base clean pages.
    //

    } else {
        NewSection->ImageBacking.DeviceHandle = INVALID_HANDLE;
        NewSection->ImageBacking.Offset = 0;
    }

    NewSection->ImageBackingReferenceCount = 1;
    NewSection->PageFileBacking.DeviceHandle = INVALID_HANDLE;

    //
    // The parent and child share the same lock.
    //

    NewSection->Lock = SectionToCopy->Lock;
    ObAddReference(NewSection->Lock);

    //
    // If the image section is backed, then insert it into the owning file
    // object's list of image sections. This allows file size modifications to
    // unmap any portions of the image section beyond the new file size.
    //

    if ((NewSection->Flags & IMAGE_SECTION_BACKED) != 0) {

        ASSERT(NewSection->ImageBacking.DeviceHandle != INVALID_HANDLE);

        ImageSectionList = IoGetImageSectionListFromIoHandle(
                                        NewSection->ImageBacking.DeviceHandle);

        if (ImageSectionList == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CopyImageSectionEnd;
        }

        MmpImageSectionAddReference(NewSection);
        KeAcquireQueuedLock(ImageSectionList->Lock);
        INSERT_BEFORE(&(NewSection->ImageListEntry),
                      &(ImageSectionList->ListHead));

        KeReleaseQueuedLock(ImageSectionList->Lock);
    }

    if (NewSection->ImageBacking.DeviceHandle != INVALID_HANDLE) {
        Status = IoNotifyFileMapping(NewSection->ImageBacking.DeviceHandle,
                                     TRUE);

        if (!KSUCCESS(Status)) {
            goto CopyImageSectionEnd;
        }
    }

    //
    // Lock the source section and try to add this one as a child.
    //

    KeAcquireQueuedLock(SectionToCopy->Lock);
    NewSection->MinTouched = SectionToCopy->MinTouched;
    NewSection->MaxTouched = SectionToCopy->MaxTouched;

    //
    // Synchronize with destruction of the parent. If the parent is on its way
    // out then, the copy cannot proceed. Act like it did, however.
    //

    if ((SectionToCopy->Flags & IMAGE_SECTION_DESTROYING) != 0) {
        ParentDestroyed = TRUE;
        Status = STATUS_SUCCESS;
        goto CopyImageSectionEnd;
    }

    NewSection->Parent = SectionToCopy;
    MmpImageSectionAddReference(SectionToCopy);
    INSERT_BEFORE(&(NewSection->CopyListEntry), &(SectionToCopy->ChildList));

    //
    // Convert the mapping to read-only and copy the mappings to the
    // destination in one skillful maneuver.
    //

    if (SectionToCopy->MinTouched < SectionToCopy->MaxTouched) {
        Status = MmpCopyAndChangeSectionMappings(
                        DestinationAddressSpace,
                        SectionToCopy->AddressSpace,
                        SectionToCopy->MinTouched,
                        SectionToCopy->MaxTouched - SectionToCopy->MinTouched);

        if (!KSUCCESS(Status)) {
            goto CopyImageSectionEnd;
        }
    }

    KeReleaseQueuedLock(SectionToCopy->Lock);

    //
    // Lock the address space and determine where the new section should be
    // inserted.
    //

    MmAcquireAddressSpaceLock(DestinationAddressSpace);
    AddressLockHeld = TRUE;
    CurrentEntry = DestinationAddressSpace->SectionListHead.Next;
    while (CurrentEntry != &(DestinationAddressSpace->SectionListHead)) {
        CurrentSection = LIST_VALUE(CurrentEntry,
                                    IMAGE_SECTION,
                                    AddressListEntry);

        if (CurrentSection->VirtualAddress > NewSection->VirtualAddress) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Insert the section onto the destination section list.
    //

    INSERT_BEFORE(&(NewSection->AddressListEntry), CurrentEntry);
    Status = STATUS_SUCCESS;

CopyImageSectionEnd:
    if (AddressLockHeld != FALSE) {
        MmReleaseAddressSpaceLock(DestinationAddressSpace);
    }

    if (!KSUCCESS(Status) || (ParentDestroyed != FALSE)) {
        if (NewSection != NULL) {
            if (NewSection->ImageListEntry.Next != NULL) {

                ASSERT(ImageSectionList != NULL);

                KeAcquireQueuedLock(ImageSectionList->Lock);
                LIST_REMOVE(&(NewSection->ImageListEntry));
                NewSection->ImageListEntry.Next = NULL;
                KeReleaseQueuedLock(ImageSectionList->Lock);
                MmpImageSectionReleaseReference(NewSection);
            }

            if (NewSection->ImageBackingReferenceCount != 0) {
                MmpImageSectionReleaseImageBackingReference(NewSection);
            }

            MmpImageSectionReleaseReference(NewSection);
            NewSection = NULL;
        }
    }

    return Status;
}

KSTATUS
MmpUnmapImageRegion (
    PADDRESS_SPACE AddressSpace,
    PVOID SectionAddress,
    UINTN Size
    )

/*++

Routine Description:

    This routine unmaps and destroys any image sections at the given address.
    This routine must be called at low level. For kernel mode, this must
    specify a single whole image section.

Arguments:

    AddressSpace - Supplies a pointer to the address space to unmap from.

    SectionAddress - Supplies the virtual address of the section.

    Size - Supplies the size of the section in bytes.

Return Value:

    Status code.

--*/

{

    UINTN PageOffset;
    PIMAGE_SECTION Section;
    KSTATUS Status;

    //
    // For kernel mode, get the whole sections and destroy them.
    //

    if (AddressSpace == MmKernelAddressSpace) {
        Status = STATUS_SUCCESS;
        while (Size != 0) {
            Status = MmpLookupSection(SectionAddress,
                                      AddressSpace,
                                      &Section,
                                      &PageOffset);

            if (!KSUCCESS(Status)) {

                ASSERT(FALSE);

                return Status;
            }

            if ((SectionAddress != Section->VirtualAddress) ||
                (Section->Size > Size)) {

                ASSERT(FALSE);

                return STATUS_INVALID_PARAMETER;
            }

            ASSERT(PageOffset == 0);

            SectionAddress += Section->Size;
            Size -= Section->Size;
            MmpRemoveImageSection(Section, FALSE);
            MmpImageSectionReleaseReference(Section);
        }

    //
    // For user mode, unmap whatever crazy region they're specifying.
    //

    } else {
        MmAcquireAddressSpaceLock(AddressSpace);
        Status = MmpClipImageSections(&(AddressSpace->SectionListHead),
                                      SectionAddress,
                                      Size,
                                      NULL);

        MmReleaseAddressSpaceLock(AddressSpace);
    }

    return Status;
}

KSTATUS
MmpFlushImageSectionRegion (
    PIMAGE_SECTION Section,
    ULONG PageOffset,
    ULONG PageCount,
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes the specified region of the given image section to
    its backing image.

Arguments:

    Section - Supplies a pointer to the image section to flush.

    PageOffset - Supplies the offset, in pages, to the start of the region that
        is to be flushed.

    PageCount - Supplies the number of pages to flush.

    Flags - Supplies a bitmask of flags. See IMAGE_SECTION_FLUSH_FLAG_* for
        definitions.

Return Value:

    Status code.

--*/

{

    PPAGE_CACHE_ENTRY CacheEntry;
    PVOID CurrentAddress;
    ULONGLONG DirtySize;
    ULONG FirstDirtyPage;
    ULONG LastDirtyPage;
    BOOL LockHeld;
    IO_OFFSET Offset;
    ULONG PageAttributes;
    ULONG PageIndex;
    ULONG PageShift;
    PHYSICAL_ADDRESS PhysicalAddress;
    BOOL ReferenceAdded;
    ULONG RegionEndOffset;
    KSTATUS Status;

    ASSERT(Section->AddressSpace == PsGetCurrentProcess()->AddressSpace);

    ReferenceAdded = FALSE;
    PageShift = MmPageShift();
    KeAcquireQueuedLock(Section->Lock);
    LockHeld = TRUE;
    Status = STATUS_SUCCESS;

    //
    // There is nothing to flush if the image section is not cache-backed,
    // shared, and writable.
    //

    if (((Section->Flags & IMAGE_SECTION_SHARED) == 0) ||
        ((Section->Flags & IMAGE_SECTION_BACKED) == 0) ||
        ((Section->Flags & IMAGE_SECTION_WAS_WRITABLE) == 0)) {

        goto FlushImageSectionRegionEnd;
    }

    //
    // Search for dirty pages in the region. If they are dirty, mark the
    // backing page cache entry as dirty.
    //

    RegionEndOffset = PageOffset + PageCount;
    FirstDirtyPage = RegionEndOffset;
    LastDirtyPage = PageOffset;
    if ((Section->Flags & IMAGE_SECTION_DESTROYED) != 0) {
        goto FlushImageSectionRegionEnd;
    }

    if (Section->MinTouched >= Section->MaxTouched) {
        goto FlushImageSectionRegionEnd;
    }

    ASSERT(Section->ImageBacking.DeviceHandle != INVALID_HANDLE);

    MmpImageSectionAddImageBackingReference(Section);
    ReferenceAdded = TRUE;
    for (PageIndex = PageOffset; PageIndex < RegionEndOffset; PageIndex += 1) {

        //
        // Skip any pages that are not mapped, not dirty, or not writable.
        //

        CurrentAddress = Section->VirtualAddress + (PageIndex << PageShift);
        if ((CurrentAddress < Section->MinTouched) ||
            (CurrentAddress > Section->MaxTouched)) {

            continue;
        }

        PhysicalAddress = MmpVirtualToPhysical(CurrentAddress, &PageAttributes);
        if ((PhysicalAddress == INVALID_PHYSICAL_ADDRESS) ||
            ((PageAttributes & MAP_FLAG_DIRTY) == 0)) {

            continue;
        }

        //
        // Record the first and last dirty pages encountered.
        //

        if (PageIndex < FirstDirtyPage) {
            FirstDirtyPage = PageIndex;
        }

        if (PageIndex > LastDirtyPage) {
            LastDirtyPage = PageIndex;
        }

        if ((Section->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0) {

            //
            // The page cache entries are in paged pool, but since this is a
            // shared section, all mapped pages are from the page cache and not
            // eligible for page-out. So this cannot cause a dead-lock.
            //

            CacheEntry =
                       MmpGetPageCacheEntryForPhysicalAddress(PhysicalAddress);

            //
            // The page cache entry must be present. It was mapped and the only
            // way for it to be removed is for the page cache to have unmapped
            // it, which requires obtaining the image section lock.
            //

            ASSERT(CacheEntry != NULL);

            //
            // Mark it dirty.
            //

            IoMarkPageCacheEntryDirty(CacheEntry);
        }
    }

    //
    // Release the lock and attempt to flush to the backing image, if necessary.
    //

    KeReleaseQueuedLock(Section->Lock);
    LockHeld = FALSE;
    if (FirstDirtyPage == RegionEndOffset) {
        goto FlushImageSectionRegionEnd;
    }

    //
    // If performing an synchronous flush, then make sure the dirty bits hit
    // permanent storage before returning. If it's an async flush, the mark
    // dirty routine already scheduled the page cache to be flushed.
    //

    if ((Flags & IMAGE_SECTION_FLUSH_FLAG_ASYNC) == 0) {

        //
        // The last dirty page records the start of the page. Increment it by
        // one to include the whole page.
        //

        LastDirtyPage += 1;

        ASSERT(FirstDirtyPage != RegionEndOffset);
        ASSERT(LastDirtyPage != PageOffset);

        Offset = Section->ImageBacking.Offset + (FirstDirtyPage << PageShift);
        DirtySize = (LastDirtyPage - FirstDirtyPage) << PageShift;
        Status = IoFlush(Section->ImageBacking.DeviceHandle,
                         Offset,
                         DirtySize,
                         0);

        if (!KSUCCESS(Status)) {
            goto FlushImageSectionRegionEnd;
        }
    }

FlushImageSectionRegionEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Section->Lock);
    }

    if (ReferenceAdded != FALSE) {
        MmpImageSectionReleaseImageBackingReference(Section);
    }

    return Status;
}

VOID
MmpImageSectionAddReference (
    PIMAGE_SECTION ImageSection
    )

/*++

Routine Description:

    This routine increases the reference count on an image section.

Arguments:

    ImageSection - Supplies a pointer to the image section to add the reference
        to.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    OldReferenceCount = RtlAtomicAdd32(&(ImageSection->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    return;
}

VOID
MmpImageSectionReleaseReference (
    PIMAGE_SECTION ImageSection
    )

/*++

Routine Description:

    This routine decreases the reference count on an image section. If this was
    the last reference on the image section, then the section is destroyed.

Arguments:

    ImageSection - Supplies a pointer to the image section to release the
        reference from.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(ImageSection->ReferenceCount),
                                       (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        MmpDeleteImageSection(ImageSection);
    }

    return;
}

VOID
MmpImageSectionAddImageBackingReference (
    PIMAGE_SECTION ImageSection
    )

/*++

Routine Description:

    This routine increases the reference count on the image backing portion of
    an image section. This routine assumes the section lock is already held.

Arguments:

    ImageSection - Supplies a pointer to the image section to add the reference
        to.

Return Value:

    None.

--*/

{

    UINTN OldReferenceCount;

    ASSERT(KeIsQueuedLockHeld(ImageSection->Lock) != FALSE);

    OldReferenceCount =
                  RtlAtomicAdd(&(ImageSection->ImageBackingReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    return;
}

VOID
MmpImageSectionReleaseImageBackingReference (
    PIMAGE_SECTION ImageSection
    )

/*++

Routine Description:

    This routine decreases the reference count on the image backing handle of
    an image section. If this was the last reference on the handle, then the
    handle is set to INVALID_HANDLE and closed. This routine must NOT be called
    with the image section lock held.

Arguments:

    ImageSection - Supplies a pointer to the image section to release the
        backing image reference from.

Return Value:

    None.

--*/

{

    HANDLE Handle;
    UINTN OldReferenceCount;

    OldReferenceCount =
                 RtlAtomicAdd(&(ImageSection->ImageBackingReferenceCount), -1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        Handle = ImageSection->ImageBacking.DeviceHandle;
        ImageSection->ImageBacking.DeviceHandle = INVALID_HANDLE;
        if (Handle != INVALID_HANDLE) {
            IoIoHandleReleaseReference(Handle);
        }
    }

    return;
}

PIMAGE_SECTION
MmpGetOwningSection (
    PIMAGE_SECTION ImageSection,
    UINTN PageOffset
    )

/*++

Routine Description:

    This routine returns the image section that owns the given page, based off
    of inheritance. It assumes the section lock is held and it takes a
    reference on the owning section.

Arguments:

    ImageSection - Supplies a pointer to the initial image section.

    PageOffset - Supplies the page's offset within the image section.

Return Value:

    Returns a pointer to the image section that currently owns the page.

--*/

{

    UINTN Index;
    ULONG Mask;
    PIMAGE_SECTION OwningSection;

    ASSERT(KeIsQueuedLockHeld(ImageSection->Lock) != FALSE);

    Index = IMAGE_SECTION_BITMAP_INDEX(PageOffset);
    Mask = IMAGE_SECTION_BITMAP_MASK(PageOffset);
    OwningSection = ImageSection;
    while ((OwningSection->Parent != NULL) &&
           ((OwningSection->InheritPageBitmap[Index] & Mask) != 0)) {

        OwningSection = OwningSection->Parent;

        ASSERT(OwningSection->Lock == ImageSection->Lock);
    }

    MmpImageSectionAddReference(OwningSection);
    return OwningSection;
}

PIMAGE_SECTION
MmpGetRootSection (
    PIMAGE_SECTION ImageSection
    )

/*++

Routine Description:

    This routine returns the root of the image section tree to which the given
    section belongs. It assumes the shared section lock is held and it takes a
    reference on the root section.

Arguments:

    ImageSection - Supplies a pointer to the initial image section.

Return Value:

    Returns a pointer to the image section tree's root image section.

--*/

{

    PIMAGE_SECTION RootSection;

    ASSERT(KeIsQueuedLockHeld(ImageSection->Lock) != FALSE);

    RootSection = ImageSection;
    while (RootSection->Parent != NULL) {
        RootSection = RootSection->Parent;

        ASSERT(RootSection->Lock == ImageSection->Lock);
    }

    MmpImageSectionAddReference(RootSection);
    return RootSection;
}

KSTATUS
MmpIsolateImageSection (
    PIMAGE_SECTION Section,
    UINTN PageOffset
    )

/*++

Routine Description:

    This routine isolates the page mapped in the given image section by
    breaking the section's inheritance from a parent or the page cache. It also
    breaks the inheritance of any children that map the same physical page as
    the given section.

Arguments:

    Section - Supplies a pointer to the image section that contains page that
        needs to be isolated.

    PageOffset - Supplies the offset, in pages, from the beginning of the
        section.

Return Value:

    Status code.

--*/

{

    ULONG Attributes;
    UINTN BitmapIndex;
    ULONG BitmapMask;
    PIMAGE_SECTION Child;
    PPAGING_ENTRY ChildPagingEntry;
    PHYSICAL_ADDRESS ChildPhysicalAddress;
    PLIST_ENTRY CurrentEntry;
    ULONG MapFlags;
    ULONG PageShift;
    PPAGING_ENTRY PagingEntry;
    PHYSICAL_ADDRESS PhysicalAddress;
    PIMAGE_SECTION ReleaseSection;
    BOOL SectionLocked;
    KSTATUS Status;
    PVOID VirtualAddress;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    ChildPagingEntry = NULL;
    ChildPhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    PageShift = MmPageShift();
    PagingEntry = NULL;
    PhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    ReleaseSection = NULL;
    SectionLocked = FALSE;
    VirtualAddress = Section->VirtualAddress + (PageOffset << PageShift);

    ASSERT(VirtualAddress < Section->VirtualAddress + Section->Size);

    //
    // Calculate the block and bitmask to use for this page.
    //

    BitmapIndex = IMAGE_SECTION_BITMAP_INDEX(PageOffset);
    BitmapMask = IMAGE_SECTION_BITMAP_MASK(PageOffset);

    //
    // This routine needs to create a private page for each of the section's
    // inheriting children and potentially a private page for itself if it
    // inherits from a parent or is backed. Shared sections are the
    // exception. Write faults for those just need the mapping access bits
    // changed. All of these pages need to be allocated with the section lock
    // released. The routine needs to set the private page for itself after
    // handling the children, but it cannot release the section lock at that
    // point because a new child may appear. As a result, allocate a paging
    // entry and physical page before acquiring the lock if they might be
    // needed later.
    //

    if (((Section->Flags & IMAGE_SECTION_DESTROYING) == 0) &&
        ((Section->Flags & IMAGE_SECTION_SHARED) == 0) &&
        (((Section->Parent != NULL) &&
          ((Section->InheritPageBitmap[BitmapIndex] & BitmapMask) != 0)) ||
         (((Section->Flags & IMAGE_SECTION_BACKED) != 0) &&
          ((Section->DirtyPageBitmap[BitmapIndex] & BitmapMask) == 0)))) {

        if ((Section->Flags & IMAGE_SECTION_NON_PAGED) == 0) {
            PagingEntry = MmpCreatePagingEntry(Section, PageOffset);
            if (PagingEntry == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto IsolateImageSectionEnd;
            }
        }

        PhysicalAddress = MmpAllocatePhysicalPages(1, 1);
        if (PhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto IsolateImageSectionEnd;
        }
    }

    //
    // Make sure the page is mapped and the section locked to prevent it from
    // being unmapped.
    //

    Status = MmpPageInAndLock(Section, PageOffset);
    if (!KSUCCESS(Status)) {
        goto IsolateImageSectionEnd;
    }

    ASSERT(KeIsQueuedLockHeld(Section->Lock) != FALSE);
    ASSERT((Section->Flags & IMAGE_SECTION_DESTROYED) == 0);

    SectionLocked = TRUE;

    //
    // Loop through and allocate copies for all children.
    //

    CurrentEntry = Section->ChildList.Next;
    while (CurrentEntry != &(Section->ChildList)) {

        ASSERT((Section->Flags & IMAGE_SECTION_SHARED) == 0);

        Child = LIST_VALUE(CurrentEntry, IMAGE_SECTION, CopyListEntry);

        //
        // If the child isn't sharing with this section (ie already has its own
        // copy), skip it.
        //

        if ((Child->InheritPageBitmap[BitmapIndex] & BitmapMask) == 0) {
            CurrentEntry = CurrentEntry->Next;
            continue;
        }

        //
        // If the section is backed and clean, then the children can continue
        // to map the backing page after clearing the inheritance.
        //

        if (((Section->Flags & IMAGE_SECTION_BACKED) != 0) &&
            ((Section->DirtyPageBitmap[BitmapIndex] & BitmapMask) == 0)) {

            ASSERT((Child->DirtyPageBitmap[BitmapIndex] & BitmapMask) == 0);

            Child->InheritPageBitmap[BitmapIndex] &= ~BitmapMask;
            CurrentEntry = CurrentEntry->Next;
            continue;
        }

        //
        // Temporarily release the lock so that the page can be copied for the
        // child. This requires an allocation and the rule of thumb is to not
        // hold a section lock while doing allocations. Take a reference on the
        // child when the lock is released in case it gets destroyed.
        //

        MmpImageSectionAddReference(Child);
        KeReleaseQueuedLock(Section->Lock);
        SectionLocked = FALSE;

        //
        // With the lock released, release any previous child section and set
        // the current section to be released.
        //

        if (ReleaseSection != NULL) {
            MmpImageSectionReleaseReference(ReleaseSection);
            ReleaseSection = NULL;
        }

        ReleaseSection = Child;

        //
        // Create the paging entry that will be used in the physical page entry
        // to mark this page as pagable. First try to reuse the existing one
        // that may have been allocated and not used last time around.
        //

        ASSERT((Child->Flags & IMAGE_SECTION_NON_PAGED) == 0);

        if (ChildPagingEntry == NULL) {
            ChildPagingEntry = MmpCreatePagingEntry(Child, PageOffset);
            if (ChildPagingEntry == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto IsolateImageSectionEnd;
            }

        } else {
            MmpReinitializePagingEntry(ChildPagingEntry, Child, PageOffset);
        }

        //
        // Allocate a new physical page for this write access. If one was
        // allocated but not used, go ahead and use it.
        //

        if (ChildPhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
            ChildPhysicalAddress = MmpAllocatePhysicalPages(1, 1);
            if (ChildPhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto IsolateImageSectionEnd;
            }
        }

        //
        // With the local allocations out of the way, make sure the page is
        // still paged into memory and locked temporarily by way of holding the
        // image section lock.
        //

        Status = MmpPageInAndLock(Section, PageOffset);
        if (!KSUCCESS(Status)) {
            goto IsolateImageSectionEnd;
        }

        ASSERT(KeIsQueuedLockHeld(Section->Lock) != FALSE);

        SectionLocked = TRUE;

        //
        // If the child was destroyed while the lock was released, then move
        // on. The loop must reset to the beginning, because the child was
        // removed from the parent's list.
        //

        if ((Child->Flags & IMAGE_SECTION_DESTROYED) != 0) {
            CurrentEntry = Section->ChildList.Next;
            continue;
        }

        //
        // If the child broke its inheritance while the lock was released, then
        // move on. The allocated paging entry and physical page will get
        // released later.
        //

        if ((Child->InheritPageBitmap[BitmapIndex] & BitmapMask) == 0) {
            CurrentEntry = CurrentEntry->Next;
            continue;
        }

        ASSERT(VirtualAddress < Child->VirtualAddress + Child->Size);

        //
        // Copy the page for this child. With the lock held it cannot get
        // unmmapped from the virtual address.
        //

        MmpCopyPage(Child, VirtualAddress, ChildPhysicalAddress);

        //
        // Map the page in the other process.
        //

        MmpModifySectionMapping(Child,
                                PageOffset,
                                ChildPhysicalAddress,
                                TRUE,
                                NULL,
                                TRUE);

        MmpEnablePagingOnPhysicalAddress(ChildPhysicalAddress,
                                         1,
                                         &ChildPagingEntry,
                                         FALSE);

        ChildPagingEntry = NULL;
        ChildPhysicalAddress = INVALID_PHYSICAL_ADDRESS;

        //
        // If the parent is dirty, mark the child dirty. There is no danger of
        // the mapping attribute dirty flag being set after the check because
        // the page is currently read-only in the current process and the lock
        // is held.
        //

        if ((Section->Flags & IMAGE_SECTION_WAS_WRITABLE) != 0) {
            if ((Section->DirtyPageBitmap[BitmapIndex] & BitmapMask) == 0) {
                MmpVirtualToPhysical(VirtualAddress, &Attributes);

                ASSERT((Attributes & MAP_FLAG_READ_ONLY) != 0);

                if ((Attributes & MAP_FLAG_DIRTY) != 0) {
                    Section->DirtyPageBitmap[BitmapIndex] |= BitmapMask;
                    Child->DirtyPageBitmap[BitmapIndex] |= BitmapMask;
                }

            } else {
                Child->DirtyPageBitmap[BitmapIndex] |= BitmapMask;
            }
        }

        //
        // Clear the inheritance bit in the child.
        //

        Child->InheritPageBitmap[BitmapIndex] &= ~BitmapMask;
        CurrentEntry = CurrentEntry->Next;
    }

    //
    // The page is no longer shared with with any children. The lock should be
    // held and it cannot be released before the supplied section is handled
    // or else a new child could appear.
    //

    ASSERT(KeIsQueuedLockHeld(Section->Lock) != FALSE);
    ASSERT(MmpVirtualToPhysical(VirtualAddress, NULL) !=
           INVALID_PHYSICAL_ADDRESS);

    //
    // Make sure that the supplied section is still alive.
    //

    if (((Section->Flags & IMAGE_SECTION_DESTROYED) != 0) ||
        ((Section->Flags & IMAGE_SECTION_DESTROYING) != 0)) {

        Status = STATUS_SUCCESS;
        goto IsolateImageSectionEnd;
    }

    //
    // Compute the appropriate flags for the page.
    //

    MapFlags = Section->MapFlags | MAP_FLAG_PAGABLE;
    if ((Section->Flags &
        (IMAGE_SECTION_READABLE | IMAGE_SECTION_WRITABLE)) != 0) {

        MapFlags |= MAP_FLAG_PRESENT;
    }

    if ((Section->Flags & IMAGE_SECTION_WRITABLE) == 0) {
        MapFlags |= MAP_FLAG_READ_ONLY;
    }

    if ((Section->Flags & IMAGE_SECTION_EXECUTABLE) != 0) {
        MapFlags |= MAP_FLAG_EXECUTE;
    }

    if (VirtualAddress < KERNEL_VA_START) {
        MapFlags |= MAP_FLAG_USER_MODE;

    } else {
        MapFlags |= MAP_FLAG_GLOBAL;
    }

    //
    // The page is no longer shared with any children. If it's not inherited
    // from a parent or backed and the section is writable, then simply
    // change the attributes on the page. Shared sections always get the page
    // attributes set.
    //

    if (((Section->Flags & IMAGE_SECTION_SHARED) != 0) ||
        (((Section->Parent == NULL) ||
          ((Section->InheritPageBitmap[BitmapIndex] & BitmapMask) == 0)) &&
         (((Section->Flags & IMAGE_SECTION_BACKED) == 0) ||
          ((Section->Flags & IMAGE_SECTION_WAS_WRITABLE) == 0) ||
          ((Section->DirtyPageBitmap[BitmapIndex] & BitmapMask) != 0)))) {

        if ((Section->Flags & IMAGE_SECTION_WRITABLE) != 0) {
            MmpChangeMemoryRegionAccess(VirtualAddress,
                                        1,
                                        MapFlags,
                                        MAP_FLAG_ALL_MASK);
        }

    //
    // Otherwise map the new page that was allocated before the process of
    // copying children began.
    //

    } else {

        //
        // A backed section better be writable if a private page, potentially
        // with a paging entry, is being set.
        //

        ASSERT(((Section->Flags & IMAGE_SECTION_BACKED) == 0) ||
               ((Section->Flags & IMAGE_SECTION_WAS_WRITABLE) != 0));

        ASSERT((Section->Flags & IMAGE_SECTION_SHARED) == 0);
        ASSERT((Section->MinTouched <= VirtualAddress) &&
               (Section->MaxTouched > VirtualAddress));

        MmpCopyPage(Section, VirtualAddress, PhysicalAddress);

        //
        // Unmap the virtual address, sending TLB invalidate IPIs.
        //

        MmpUnmapPages(VirtualAddress, 1, UNMAP_FLAG_SEND_INVALIDATE_IPI, NULL);

        //
        // Map the new page. If it is a writable section, then map it writable.
        //

        MmpMapPage(PhysicalAddress, VirtualAddress, MapFlags);

        //
        // If this is a pageable section, then mark the allocated page as
        // pageable.
        //

        if ((Section->Flags & IMAGE_SECTION_NON_PAGED) == 0) {

            ASSERT(PagingEntry != NULL);

            MmpEnablePagingOnPhysicalAddress(PhysicalAddress,
                                             1,
                                             &PagingEntry,
                                             FALSE);

            PagingEntry = NULL;
        }

        if (Section->InheritPageBitmap != NULL) {
            Section->InheritPageBitmap[BitmapIndex] &= ~BitmapMask;
        }

        PhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    }

    //
    // If a dirty page bitmap is present, mark this page as dirty now that this
    // section has stopped inheriting from the original source (i.e. the parent
    // or the page cache). The only accurate version of this page is mapped by
    // this image section. Page out must be aware of this. This must be set for
    // both the case where this section gets a new page and just modifies the
    // mapping for this page. The instance where the latter is necessary comes
    // when a writable anonymous section forks, writes, and forks again. The
    // child section on the second fork needs to inherit the dirty bit.
    //

    if (((Section->Flags & IMAGE_SECTION_WAS_WRITABLE) != 0) &&
        (Section->DirtyPageBitmap != NULL)) {

        Section->DirtyPageBitmap[BitmapIndex] |= BitmapMask;
    }

    Status = STATUS_SUCCESS;

IsolateImageSectionEnd:
    if (SectionLocked != FALSE) {
        KeReleaseQueuedLock(Section->Lock);
    }

    if (ReleaseSection != NULL) {
        MmpImageSectionReleaseReference(ReleaseSection);
    }

    if (PagingEntry != NULL) {
        MmpDestroyPagingEntry(PagingEntry);
    }

    if (PhysicalAddress != INVALID_PHYSICAL_ADDRESS) {
        MmFreePhysicalPage(PhysicalAddress);
    }

    if (ChildPagingEntry != NULL) {
        MmpDestroyPagingEntry(ChildPagingEntry);
    }

    if (ChildPhysicalAddress != INVALID_PHYSICAL_ADDRESS) {
        MmFreePhysicalPage(ChildPhysicalAddress);
    }

    return Status;
}

KSTATUS
MmpClipImageSections (
    PLIST_ENTRY SectionListHead,
    PVOID Address,
    UINTN Size,
    PLIST_ENTRY *ListEntryBefore
    )

/*++

Routine Description:

    This routine wipes out any image sections covering the given VA range. It
    assumes the address space lock is already held. This routine does not
    change any accountant mappings.

Arguments:

    SectionListHead - Supplies a pointer to the head of the list of image
        sections for the address space.

    Address - Supplies the first address (inclusive) to remove image sections
        for.

    Size - Supplies the size in bytes of the region to clear.

    ListEntryBefore - Supplies an optional pointer to the list entry
        immediately before where the given address range starts.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PVOID End;
    PIMAGE_SECTION Section;
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    End = Address + Size;
    CurrentEntry = SectionListHead->Next;
    while (CurrentEntry != SectionListHead) {
        Section = LIST_VALUE(CurrentEntry, IMAGE_SECTION, AddressListEntry);
        if (Section->VirtualAddress >= End) {
            break;
        }

        //
        // Move on before clipping the section as the section may get unlinked
        // and destroyed.
        //

        CurrentEntry = CurrentEntry->Next;
        if (Section->VirtualAddress + Section->Size > Address) {
            Status = MmpClipImageSection(SectionListHead,
                                         Address,
                                         Size,
                                         Section);

            if (!KSUCCESS(Status)) {
                break;
            }

            //
            // Go back in case the remainder is greater than the next one.
            //

            if (CurrentEntry->Previous != SectionListHead) {
                CurrentEntry = CurrentEntry->Previous;
            }
        }
    }

    if (ListEntryBefore != NULL) {
        *ListEntryBefore = CurrentEntry->Previous;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
MmpAllocateImageSection (
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress,
    UINTN Size,
    ULONG Flags,
    HANDLE ImageHandle,
    IO_OFFSET ImageOffset,
    PIMAGE_SECTION *AllocatedSection
    )

/*++

Routine Description:

    This routine allocates and initializes a new image section.

Arguments:

    AddressSpace - Supplies a pointer to the address space to create the
        section under.

    VirtualAddress - Supplies the virtual address of the section.

    Size - Supplies the size of the section, in bytes.

    Flags - Supplies a bitfield of flags governing this image section. See
        IMAGE_SECTION_* definitions.

    ImageHandle - Supplies an open handle to the backing image, if the section
        is backed by a file. Supply INVALID_HANDLE here for pure memory
        allocations.

    ImageOffset - Supplies the offset, in bytes, to the beginning of the
        backing with the image.

    AllocatedSection - Supplies a pointer where the newly allocated section
        will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated to create
        the image or there is no more room in the page file.

--*/

{

    ULONG AllocationSize;
    ULONG BitmapCount;
    ULONG BitmapSize;
    PIMAGE_SECTION_LIST ImageSectionList;
    ULONG MapFlags;
    PIMAGE_SECTION NewSection;
    UINTN PageCount;
    ULONG PageMask;
    ULONG PageShift;
    ULONG PageSize;
    UINTN SizeWhenAlignedToPageBoundaries;
    KSTATUS Status;

    ImageSectionList = NULL;
    MapFlags = 0;
    NewSection = NULL;
    PageSize = MmPageSize();
    PageShift = MmPageShift();

    ASSERT(POWER_OF_2(PageSize) != FALSE);

    PageMask = PageSize - 1;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((VirtualAddress < KERNEL_VA_START) ||
           (AddressSpace == MmKernelAddressSpace));

    ASSERT(ImageHandle != NULL);

    //
    // Currently only page aligned bases and sizes are supported.
    //

    if ((((UINTN)VirtualAddress & PageMask) != 0) ||
        ((Size & PageMask) != 0)) {

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto AllocateImageSectionEnd;
    }

    //
    // Private, pageable sections get a dirty bitmap. Non-paged and shared
    // sections do not. They always page-in from the backing image.
    //

    BitmapCount = 0;
    if (((Flags & IMAGE_SECTION_NON_PAGED) == 0) &&
        ((Flags & IMAGE_SECTION_SHARED) == 0)) {

        BitmapCount = 1;
    }

    //
    // If an image section can use pages directly collected from the backing
    // image then mark it as backed. This is only the case if there is an
    // image handle and the offset is cache-aligned.
    //

    if ((ImageHandle != INVALID_HANDLE) &&
        (IS_ALIGNED(ImageOffset, IoGetCacheEntryDataSize()) != FALSE)) {

        Flags |= IMAGE_SECTION_BACKED;
        if (IoIoHandleIsCacheable(ImageHandle, &MapFlags) != FALSE) {
            Flags |= IMAGE_SECTION_PAGE_CACHE_BACKED;
        }

        //
        // If this is not a shared section, private mappings always get mapped
        // cached, etc.
        //

        if ((Flags & IMAGE_SECTION_SHARED) == 0) {
            MapFlags = 0;
        }

        //
        // Non-paged, cache-backed sections need a dirty page bitmap in order
        // to track which pages are not mapped page cache entries so that they
        // are released when the section is destroyed.
        //

        if (((Flags & IMAGE_SECTION_NON_PAGED) != 0) &&
            ((Flags & IMAGE_SECTION_SHARED) == 0)) {

            ASSERT(BitmapCount == 0);

            BitmapCount = 1;
        }

    //
    // Otherwise fail if the shared flag is set. Without backing from the page
    // cache there is no mechanism to share an image section.
    //

    } else {
        if ((Flags & IMAGE_SECTION_SHARED) != 0) {

            ASSERT((Flags & IMAGE_SECTION_SHARED) == 0);

            Status = STATUS_INVALID_PARAMETER;
            goto AllocateImageSectionEnd;
        }

        Flags &= ~IMAGE_SECTION_BACKED;
    }

    //
    // If no image handle was provided, then this is an anonymous section.
    //

    if (ImageHandle == INVALID_HANDLE) {
        Flags |= IMAGE_SECTION_NO_IMAGE_BACKING;
    }

    if ((Flags & IMAGE_SECTION_WRITABLE) != 0) {
        Flags |= IMAGE_SECTION_WAS_WRITABLE;
    }

    //
    // Create and fill out a new image section. Do not zero it as this routine
    // has to fill out the majority of the fields and fills out the rest to
    // avoid zeroing it all.
    //

    SizeWhenAlignedToPageBoundaries =
                       ALIGN_RANGE_UP((UINTN)VirtualAddress + Size, PageSize) -
                       ALIGN_RANGE_DOWN((UINTN)VirtualAddress, PageSize);

    PageCount = ALIGN_RANGE_UP(SizeWhenAlignedToPageBoundaries, PageSize) >>
                               PageShift;

    BitmapSize = ALIGN_RANGE_UP(PageCount, (BITS_PER_BYTE * sizeof(ULONG))) /
                 BITS_PER_BYTE;

    AllocationSize = sizeof(IMAGE_SECTION) + (BitmapCount * BitmapSize);
    NewSection = MmAllocateNonPagedPool(AllocationSize,
                                        MM_IMAGE_SECTION_ALLOCATION_TAG);

    if (NewSection == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AllocateImageSectionEnd;
    }

    NewSection->ReferenceCount = 1;
    NewSection->Flags = Flags;
    NewSection->AddressListEntry.Next = NULL;
    NewSection->ImageListEntry.Next = NULL;
    NewSection->CopyListEntry.Next = NULL;
    NewSection->CopyListEntry.Previous = NULL;
    NewSection->Parent = NULL;
    INITIALIZE_LIST_HEAD(&(NewSection->ChildList));
    NewSection->AddressSpace = AddressSpace;
    NewSection->VirtualAddress = VirtualAddress;
    NewSection->PagingInIrp = NULL;
    NewSection->SwapSpace = NULL;
    NewSection->Size = Size;
    NewSection->TruncateCount = 0;
    NewSection->PageFileBacking.DeviceHandle = INVALID_HANDLE;
    NewSection->ImageBacking.DeviceHandle = ImageHandle;
    NewSection->ImageBackingReferenceCount = 1;
    NewSection->MinTouched = VirtualAddress + Size;
    NewSection->MaxTouched = VirtualAddress;
    NewSection->MapFlags = MapFlags;
    if (ImageHandle != INVALID_HANDLE) {
        IoIoHandleAddReference(ImageHandle);
        NewSection->ImageBacking.Offset = ImageOffset;
    }

    //
    // Set up the bitmaps based on the flags and number of bitmaps allocated.
    // Shared image sections should have no bitmaps as they have no parent and
    // always dirty page cache pages directly.
    //

    ASSERT((BitmapCount == 0) || ((Flags & IMAGE_SECTION_SHARED) == 0));

    NewSection->InheritPageBitmap = NULL;
    NewSection->DirtyPageBitmap = NULL;
    if (BitmapCount != 0) {

        ASSERT(BitmapCount == 1);

        //
        // If a non-paged section has a bitmap, it better be backed.
        //

        ASSERT(((Flags & IMAGE_SECTION_NON_PAGED) == 0) ||
               ((Flags & IMAGE_SECTION_BACKED) != 0));

        NewSection->DirtyPageBitmap = (PULONG)(NewSection + 1);
        RtlZeroMemory(NewSection->DirtyPageBitmap, BitmapSize);
    }

    NewSection->Lock = KeCreateQueuedLock();
    if (NewSection->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AllocateImageSectionEnd;
    }

    //
    // Ensure there are page tables there for this VA range. This is needed
    // to avoid a situation where page-in code locks the section, and then when
    // it tries to map the final page into place it needs to allocate a page
    // for a page table. If the system were low on memory at that point, the
    // paging out thread could get blocked trying to acquire that image section
    // lock.
    //
    // A proposal was discussed to try a page-granular locking scheme on image
    // sections, which involved a bitmap plus a single event per image section.
    // That scheme might allow allocations during page in, which would then in
    // turn allow this next line to be removed. Trying that proposal might be
    // worthwhile.
    //

    MmpCreatePageTables(NewSection->VirtualAddress, NewSection->Size);

    //
    // If the image section is backed, then insert it into the owning file
    // object's list of image sections. This allows the page cache to unmap
    // from this section when it wants to evict a page.
    //

    if ((NewSection->Flags & IMAGE_SECTION_BACKED) != 0) {

        ASSERT(NewSection->ImageBacking.DeviceHandle != INVALID_HANDLE);

        ImageSectionList = IoGetImageSectionListFromIoHandle(
                                        NewSection->ImageBacking.DeviceHandle);

        if (ImageSectionList == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AllocateImageSectionEnd;
        }

        MmpImageSectionAddReference(NewSection);
        KeAcquireQueuedLock(ImageSectionList->Lock);
        if ((NewSection->Flags & IMAGE_SECTION_NON_PAGED) != 0) {
            INSERT_AFTER(&(NewSection->ImageListEntry),
                          &(ImageSectionList->ListHead));

        } else {
            INSERT_BEFORE(&(NewSection->ImageListEntry),
                          &(ImageSectionList->ListHead));
        }

        KeReleaseQueuedLock(ImageSectionList->Lock);
    }

    Status = STATUS_SUCCESS;

AllocateImageSectionEnd:
    if (!KSUCCESS(Status)) {
        if (NewSection != NULL) {
            if (NewSection->ImageBackingReferenceCount != 0) {
                MmpImageSectionReleaseImageBackingReference(NewSection);
            }

            MmpImageSectionReleaseReference(NewSection);
            NewSection = NULL;
        }
    }

    *AllocatedSection = NewSection;
    return Status;
}

KSTATUS
MmpClipImageSection (
    PLIST_ENTRY SectionListHead,
    PVOID Address,
    UINTN Size,
    PIMAGE_SECTION Section
    )

/*++

Routine Description:

    This routine clips the given VA range out of the given image section, which
    may cause the image section to get destroyed entirely, shrunk, or split
    into two. This routine assumes the address space lock is already held
    (protecting VA changes).

Arguments:

    SectionListHead - Supplies a pointer to the head of the list of image
        sections for the address space.

    Address - Supplies the first address (inclusive) to remove image sections
        for.

    Size - Supplies the size in bytes of the region to clear.

    Section - Supplies a pointer to the image section to clip.

Return Value:

    Status code.

--*/

{

    UINTN BitmapCount;
    UINTN BitmapIndex;
    UINTN BitmapOffset;
    UINTN BitmapShift;
    UINTN BitmapSize;
    PVOID HoleBegin;
    PVOID HoleEnd;
    UINTN HolePageCount;
    UINTN HolePageOffset;
    UINTN PageCount;
    UINTN PageIndex;
    UINTN PageOffset;
    UINTN PageShift;
    PVOID RegionEnd;
    IO_OFFSET RemainderOffset;
    UINTN RemainderPages;
    PIMAGE_SECTION RemainderSection;
    UINTN RemainderSize;
    PVOID SectionEnd;
    UINTN SourceBitmapCount;
    ULONG SourceBlock;
    UINTN SourceIndex;
    KSTATUS Status;

    PageShift = MmPageShift();
    RegionEnd = Address + Size;
    RemainderSection = NULL;
    SectionEnd = Section->VirtualAddress + Section->Size;

    //
    // This doesn't work for the kernel process because it touches paged data
    // with the address space lock held.
    //

    ASSERT(Section->AddressSpace != MmKernelAddressSpace);
    ASSERT(Address < SectionEnd);

    //
    // As an optimization, if the region covers the whole section, just destroy
    // the section.
    //

    if ((Address <= Section->VirtualAddress) && (RegionEnd >= SectionEnd)) {
        MmpRemoveImageSection(Section, TRUE);
        return STATUS_SUCCESS;
    }

    HoleBegin = Section->VirtualAddress;
    if (Address > HoleBegin) {
        HoleBegin = Address;
    }

    HoleEnd = SectionEnd;
    if (RegionEnd < HoleEnd) {
        HoleEnd = RegionEnd;
    }

    ASSERT(HoleEnd >= HoleBegin);

    //
    // Isolate the entire section, except for the portion at the beginning that
    // stays the same. Multiple clippings can't be going on at once because
    // the address space lock is held throughout.
    //

    if ((Section->Parent != NULL) || (!LIST_EMPTY(&(Section->ChildList)))) {
        PageCount = Section->Size >> PageShift;
        PageIndex = ((UINTN)HoleBegin - (UINTN)(Section->VirtualAddress)) >>
                    PageShift;

        while (PageIndex < PageCount) {
            Status = MmpIsolateImageSection(Section, PageIndex);
            if ((!KSUCCESS(Status)) && (Status != STATUS_END_OF_FILE)) {

                ASSERT(Status != STATUS_TRY_AGAIN);

                goto ClipImageSectionEnd;
            }

            PageIndex += 1;
        }
    }

    //
    // Assume the image is going to get split into three sections: the original
    // portion of the section, a hole, and a new remainder section. Some of
    // these sections may turn out to be of size zero. Start by allocating
    // and initializing the remainder section, as that should be all done
    // before the image section lock is acquired.
    //

    RemainderSection = NULL;
    RemainderSize = 0;
    if (RegionEnd < SectionEnd) {
        RemainderOffset = Section->ImageBacking.Offset +
                          (UINTN)RegionEnd -
                          (UINTN)(Section->VirtualAddress);

        RemainderSize = (UINTN)SectionEnd - (UINTN)RegionEnd;
        Status = MmpAllocateImageSection(Section->AddressSpace,
                                         RegionEnd,
                                         RemainderSize,
                                         Section->Flags,
                                         Section->ImageBacking.DeviceHandle,
                                         RemainderOffset,
                                         &RemainderSection);

        if (!KSUCCESS(Status)) {
            goto ClipImageSectionEnd;
        }

        if (Section->ImageBacking.DeviceHandle != INVALID_HANDLE) {
            Status = IoNotifyFileMapping(Section->ImageBacking.DeviceHandle,
                                         TRUE);

            if (!KSUCCESS(Status)) {
                goto ClipImageSectionEnd;
            }
        }
    }

    RemainderPages = RemainderSize >> PageShift;

    ASSERT(RemainderPages << PageShift == RemainderSize);

    //
    // Acquire the section lock to prevent further changes to the bitmap.
    //

    KeAcquireQueuedLock(Section->Lock);
    if (RemainderSection != NULL) {
        if (Section->MaxTouched > RegionEnd) {
            RemainderSection->MaxTouched = Section->MaxTouched;
            RemainderSection->MinTouched = Section->MinTouched;
            if (RemainderSection->MinTouched < RegionEnd) {
                RemainderSection->MinTouched = RegionEnd;
            }
        }

        //
        // Copy the bitmaps to the remainder section. This gets ugly because
        // they may be shifted.
        //

        PageOffset = ((UINTN)RegionEnd - (UINTN)(Section->VirtualAddress)) >>
                     PageShift;

        BitmapOffset = PageOffset / (sizeof(ULONG) * BITS_PER_BYTE);
        BitmapShift = PageOffset % (sizeof(ULONG) * BITS_PER_BYTE);
        BitmapSize = ALIGN_RANGE_UP(RemainderPages,
                                    (sizeof(ULONG) * BITS_PER_BYTE));

        BitmapSize /= BITS_PER_BYTE;
        BitmapCount = BitmapSize / sizeof(ULONG);

        ASSERT(((Section->Size >> PageShift) << PageShift) == Section->Size);

        SourceBitmapCount = ALIGN_RANGE_UP((Section->Size >> PageShift),
                                           (sizeof(ULONG) * BITS_PER_BYTE));

        SourceBitmapCount /= sizeof(ULONG) * BITS_PER_BYTE;
        if (Section->InheritPageBitmap != NULL) {
            for (BitmapIndex = 0; BitmapIndex < BitmapCount; BitmapIndex += 1) {
                SourceIndex = BitmapIndex + BitmapOffset;
                SourceBlock = Section->InheritPageBitmap[SourceIndex];
                RemainderSection->InheritPageBitmap[BitmapIndex] =
                                                    SourceBlock >> BitmapShift;

                if (SourceIndex != SourceBitmapCount - 1) {
                    SourceIndex += 1;
                    SourceBlock = Section->InheritPageBitmap[SourceIndex];
                    RemainderSection->InheritPageBitmap[BitmapIndex] |=
                        SourceBlock <<
                        ((sizeof(ULONG) * BITS_PER_BYTE) - BitmapShift);
                }
            }
        }

        if (Section->DirtyPageBitmap != NULL) {
            for (BitmapIndex = 0; BitmapIndex < BitmapCount; BitmapIndex += 1) {
                SourceIndex = BitmapIndex + BitmapOffset;
                SourceBlock = Section->DirtyPageBitmap[SourceIndex];
                RemainderSection->DirtyPageBitmap[BitmapIndex] =
                                                    SourceBlock >> BitmapShift;

                if (SourceIndex != SourceBitmapCount - 1) {
                    SourceIndex += 1;
                    SourceBlock = Section->DirtyPageBitmap[SourceIndex];
                    RemainderSection->DirtyPageBitmap[BitmapIndex] |=
                        SourceBlock <<
                        ((sizeof(ULONG) * BITS_PER_BYTE) - BitmapShift);
                }
            }
        }

        //
        // Copy the page file space if it's allocated.
        //

        if (Section->PageFileBacking.DeviceHandle != INVALID_HANDLE) {
            RemainderSection->PageFileBacking.DeviceHandle =
                                         Section->PageFileBacking.DeviceHandle;

            RemainderSection->PageFileBacking.Offset =
                                              Section->PageFileBacking.Offset +
                                              (PageOffset << PageShift);
        }

        //
        // Migrate all the existing mappings to the new section.
        //

        MmpMigratePagingEntries(Section,
                                RemainderSection,
                                RemainderSection->VirtualAddress,
                                RemainderPages);
    }

    //
    // Unmap and free anything in the "hole".
    //

    if (HoleEnd > HoleBegin) {
        HolePageOffset = ((UINTN)HoleBegin - (UINTN)Section->VirtualAddress) >>
                         PageShift;

        HolePageCount = ((UINTN)HoleEnd - (UINTN)HoleBegin) >> PageShift;
        MmpUnmapImageSection(Section, HolePageOffset, HolePageCount, 0);

        ASSERT(HolePageOffset + HolePageCount <= (Section->Size >> PageShift));

        MmFreePartialPageFileSpace(&(Section->PageFileBacking),
                                   HolePageOffset,
                                   HolePageCount);
    }

    //
    // Shrink the image section. Everything after the beginning of the hole is
    // either unmapped and destroyed or handed off to the remainder section.
    //

    Section->Size = (UINTN)HoleBegin - (UINTN)(Section->VirtualAddress);

    //
    // If the minimum touched address is above the hole, then none of this
    // section has been touched.
    //

    if (Section->MinTouched > HoleBegin) {
        Section->MinTouched = HoleBegin;
        Section->MaxTouched = Section->VirtualAddress;

    } else if (Section->MaxTouched > HoleBegin) {
        Section->MaxTouched = HoleBegin;
    }

    //
    // Put the remainder section online.
    //

    if (RemainderSection != NULL) {
        INSERT_AFTER(&(RemainderSection->AddressListEntry),
                     &(Section->AddressListEntry));
    }

    KeReleaseQueuedLock(Section->Lock);

    //
    // If the section was completely clipped to nothingness, then destroy it.
    //

    if (Section->Size == 0) {
        MmpRemoveImageSection(Section, TRUE);
    }

    Status = STATUS_SUCCESS;

ClipImageSectionEnd:
    if (!KSUCCESS(Status)) {
        if (RemainderSection != NULL) {
            MmpImageSectionReleaseReference(RemainderSection);
        }
    }

    return Status;
}

VOID
MmpRemoveImageSection (
    PIMAGE_SECTION Section,
    BOOL AddressSpaceLockHeld
    )

/*++

Routine Description:

    This routine removes an decommissions an image section This routine must be
    called at low level.

Arguments:

    Section - Supplies a pointer to the section to kill.

    AddressSpaceLockHeld - Supplies a boolean indicating whether or not the
        address space lock is already held.

Return Value:

    None.

--*/

{

    UINTN BitmapIndex;
    ULONG BitmapMask;
    PIMAGE_SECTION Child;
    PLIST_ENTRY CurrentEntry;
    PIMAGE_SECTION_LIST ImageSectionList;
    UINTN PageCount;
    UINTN PageIndex;
    ULONG PageShift;
    BOOL SharedWithChild;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    PageShift = MmPageShift();
    PageCount = Section->Size >> PageShift;

    ASSERT(IS_ALIGNED(Section->Size, MmPageSize()));

    //
    // At this point, mark that the image section is in the destroying state so
    // that it does not get copied anymore. Perform this under the lock so that
    // it synchronizes with image section copies.
    //

    KeAcquireQueuedLock(Section->Lock);
    Section->Flags |= IMAGE_SECTION_DESTROYING;
    KeReleaseQueuedLock(Section->Lock);

    //
    // If there are children of this section, force the inheritance to break on
    // any page inherited by a child.
    //

    if (LIST_EMPTY(&(Section->ChildList)) == FALSE) {

        ASSERT((Section->Flags & IMAGE_SECTION_SHARED) == 0);

        //
        // If virtual addresses are going to get poked directly, assert that
        // this is the right address space.
        //

        ASSERT((Section->VirtualAddress >= KERNEL_VA_START) ||
               (Section->AddressSpace == PsGetCurrentProcess()->AddressSpace));

        KeAcquireQueuedLock(Section->Lock);
        for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {

            //
            // First determine if anyone is inheriting from this page, as it's
            // usually less work to check a bitmap than to potentially fault in
            // a page for no reason.
            //

            SharedWithChild = FALSE;
            CurrentEntry = Section->ChildList.Next;
            while (CurrentEntry != &(Section->ChildList)) {
                Child = LIST_VALUE(CurrentEntry, IMAGE_SECTION, CopyListEntry);
                CurrentEntry = CurrentEntry->Next;

                ASSERT(Child->Parent == Section);
                ASSERT(Section->ReferenceCount > 1);

                BitmapIndex = IMAGE_SECTION_BITMAP_INDEX(PageIndex);
                BitmapMask = IMAGE_SECTION_BITMAP_MASK(PageIndex);

                //
                // Determine if the page is shared with the section's parent.
                //

                if ((Child->InheritPageBitmap[BitmapIndex] & BitmapMask) != 0) {
                    SharedWithChild = TRUE;
                    break;
                }
            }

            if (SharedWithChild != FALSE) {
                KeReleaseQueuedLock(Section->Lock);
                MmpIsolateImageSection(Section, PageIndex);
                KeAcquireQueuedLock(Section->Lock);
            }
        }

        KeReleaseQueuedLock(Section->Lock);
    }

    //
    // The next goal is to remove the section from the section list, taking it
    // offline for everyone that's not actively working on it.
    //

    if (AddressSpaceLockHeld == FALSE) {
        MmAcquireAddressSpaceLock(Section->AddressSpace);
    }

    LIST_REMOVE(&(Section->AddressListEntry));
    Section->AddressListEntry.Next = NULL;
    if (AddressSpaceLockHeld == FALSE) {
        MmReleaseAddressSpaceLock(Section->AddressSpace);
    }

    if (Section->ImageBacking.DeviceHandle != INVALID_HANDLE) {
        IoNotifyFileMapping(Section->ImageBacking.DeviceHandle, FALSE);
    }

    //
    // Now get in the back of the line to acquire the section lock. Once the
    // lock is acquired, no new pages should get mapped into the image section.
    // Anything trying to map such pages needs to check the
    // IMAGE_SECTION_DESTROYED flags before proceeding.
    //

    KeAcquireQueuedLock(Section->Lock);

    //
    // If the section inherits from another, remove it as a child. Don't set
    // the parent to NULL because it still may need to isolate (really hand
    // pages over) from the parent.
    //

    if (Section->Parent != NULL) {

        ASSERT(Section->Lock == Section->Parent->Lock);
        ASSERT((Section->Flags & IMAGE_SECTION_SHARED) == 0);

        LIST_REMOVE(&(Section->CopyListEntry));
        MmpImageSectionReleaseReference(Section->Parent);
    }

    //
    // Loop through and remove each child from this dying section.
    //

    CurrentEntry = Section->ChildList.Next;
    while (CurrentEntry != &(Section->ChildList)) {
        Child = LIST_VALUE(CurrentEntry, IMAGE_SECTION, CopyListEntry);
        CurrentEntry = CurrentEntry->Next;

        ASSERT(Child->Parent == Section);
        ASSERT(Section->ReferenceCount > 1);
        ASSERT((Section->Flags & IMAGE_SECTION_SHARED) == 0);

        LIST_REMOVE(&(Child->CopyListEntry));
        Child->Parent = NULL;
        MmpImageSectionReleaseReference(Section);
    }

    ASSERT(LIST_EMPTY(&(Section->ChildList)) != FALSE);

    //
    // Destroy the mappings for this image section.
    //

    MmpDestroyImageSectionMappings(Section);

    //
    // Now that all the pages have been unmapped and it has been detached from
    // its parent and children, mark the section as destroyed. This allows
    // page ins and write faults to check to make sure it is OK to map a page
    // before proceeding.
    //

    Section->Flags |= IMAGE_SECTION_DESTROYED;

    //
    // Release the section lock, allowing all remaining page out operations on
    // the section to continue and any lingering page in operations to see that
    // the section has been marked as destroyed.
    //

    KeReleaseQueuedLock(Section->Lock);

    //
    // If the image section was inserted into an I/O handle's image section
    // list then remove it now.
    //

    if (Section->ImageListEntry.Next != NULL) {

        ASSERT(Section->ImageBacking.DeviceHandle != INVALID_HANDLE);

        ImageSectionList = IoGetImageSectionListFromIoHandle(
                                           Section->ImageBacking.DeviceHandle);

        ASSERT(ImageSectionList != NULL);

        KeAcquireQueuedLock(ImageSectionList->Lock);
        LIST_REMOVE(&(Section->ImageListEntry));
        Section->ImageListEntry.Next = NULL;
        KeReleaseQueuedLock(ImageSectionList->Lock);
        MmpImageSectionReleaseReference(Section);
    }

    //
    // Release the original reference on the backing image.
    //

    MmpImageSectionReleaseImageBackingReference(Section);

    //
    // Release the extra reference on the section holding it in place. The
    // section may get destroyed immediately here or get destroyed later when
    // the last paging out operation that was just unblocked completes.
    //

    MmpImageSectionReleaseReference(Section);
    return;
}

VOID
MmpDeleteImageSection (
    PIMAGE_SECTION ImageSection
    )

/*++

Routine Description:

    This routine destroys all resources consumed by an image section.

Arguments:

    ImageSection - Supplies a pointer to the image section to destroy.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(ImageSection->AddressListEntry.Next == NULL);
    ASSERT(ImageSection->ImageListEntry.Next == NULL);

    MmFreePageFileSpace(&(ImageSection->PageFileBacking), ImageSection->Size);
    if (ImageSection->PagingInIrp != NULL) {
        IoDestroyIrp(ImageSection->PagingInIrp);
    }

    if (ImageSection->Lock != NULL) {
        KeDestroyQueuedLock(ImageSection->Lock);
    }

    if (ImageSection->SwapSpace != NULL) {
        MmFreeMemoryReservation(ImageSection->SwapSpace);
    }

    ASSERT((ImageSection->ImageBackingReferenceCount == 0) &&
           (ImageSection->ImageBacking.DeviceHandle == INVALID_HANDLE));

    ImageSection->AddressSpace = NULL;
    MmFreeNonPagedPool(ImageSection);
    return;
}

KSTATUS
MmpChangeImageSectionAccess (
    PIMAGE_SECTION Section,
    ULONG NewAccess
    )

/*++

Routine Description:

    This routine changes the access attributes for the given image section.

Arguments:

    Section - Supplies a pointer to the section to change.

    NewAccess - Supplies the new access attributes.

Return Value:

    Status code.

--*/

{

    BOOL BecomingReadOnly;
    ULONG HandleAccess;
    ULONG MapFlags;
    KSTATUS Status;

    KeAcquireQueuedLock(Section->Lock);

    //
    // If the flags agree, there's nothing to do.
    //

    if (((Section->Flags ^ NewAccess) & IMAGE_SECTION_ACCESS_MASK) == 0) {
        Status = STATUS_SUCCESS;
        goto ChangeImageSectionAccessEnd;
    }

    if (Section->ImageBacking.DeviceHandle != INVALID_HANDLE) {
        HandleAccess = IoGetIoHandleAccessPermissions(
                                           Section->ImageBacking.DeviceHandle);

        //
        // Prevent the caller from getting write access to a shared section
        // if the file handle wasn't opened with write permissions.
        //

        if (((Section->Flags & IMAGE_SECTION_SHARED) != 0) &&
            ((NewAccess & IMAGE_SECTION_WRITABLE) != 0) &&
            ((HandleAccess & IO_ACCESS_WRITE) == 0)) {

            Status = STATUS_ACCESS_DENIED;
            goto ChangeImageSectionAccessEnd;
        }
    }

    //
    // If the flags are different and it used to be writable, it must be
    // becoming read only.
    //

    BecomingReadOnly = FALSE;
    if ((((Section->Flags ^ NewAccess) & Section->Flags) &
         IMAGE_SECTION_WRITABLE) != 0) {

        BecomingReadOnly = TRUE;
    }

    //
    // Update the flags. If the section is writable, make sure that it gets
    // marked as "was writable". Dirty bitmap accounting and page out depend on
    // whether or not the section was ever writable, not its current state.
    //

    Section->Flags = (Section->Flags & ~IMAGE_SECTION_ACCESS_MASK) |
                     (NewAccess & IMAGE_SECTION_ACCESS_MASK);

    if ((Section->Flags & IMAGE_SECTION_WRITABLE) != 0) {
        Section->Flags |= IMAGE_SECTION_WAS_WRITABLE;
    }

    //
    // If the section is going read-only, then change the mappings. Don't
    // change the mappings if going to writable because there might be page
    // cache pages mapped (which shouldn't get modified for a private section).
    //

    if (BecomingReadOnly != FALSE) {
        MapFlags = MAP_FLAG_PAGABLE;
        if ((Section->Flags &
             (IMAGE_SECTION_READABLE | IMAGE_SECTION_WRITABLE)) != 0) {

            MapFlags |= MAP_FLAG_PRESENT;
        }

        if ((Section->Flags & IMAGE_SECTION_WRITABLE) == 0) {
            MapFlags |= MAP_FLAG_READ_ONLY;
        }

        if ((Section->Flags & IMAGE_SECTION_EXECUTABLE) != 0) {
            MapFlags |= MAP_FLAG_EXECUTE;
        }

        if (Section->VirtualAddress < KERNEL_VA_START) {
            MapFlags |= MAP_FLAG_USER_MODE;

        } else {
            MapFlags |= MAP_FLAG_GLOBAL;
        }

        MmpChangeMemoryRegionAccess(Section->VirtualAddress,
                                    Section->Size >> MmPageShift(),
                                    MapFlags,
                                    MAP_FLAG_ALL_MASK);
    }

    Status = STATUS_SUCCESS;

ChangeImageSectionAccessEnd:
    KeReleaseQueuedLock(Section->Lock);
    return Status;
}

KSTATUS
MmpUnmapImageSection (
    PIMAGE_SECTION Section,
    UINTN PageOffset,
    UINTN PageCount,
    ULONG Flags
    )

/*++

Routine Description:

    This routine unmaps all pages in the given image section starting at the
    given page offset for the supplied page count. It also unmaps the page from
    any image section that inherits from the given section. This routine
    assumes the image section lock is already held.

Arguments:

    Section - Supplies a pointer to an image section.

    PageOffset - Supplies the offset, in pages, of the first page to be
        unmapped.

    PageCount - Supplies the number of pages to be unmapped.

    Flags - Supplies a bitmask of flags for the unmap. See
        IMAGE_SECTION_UNMAP_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    UINTN BitmapIndex;
    ULONG BitmapMask;
    UINTN Boundary;
    UINTN CurrentPageOffset;
    PULONG DirtyPageBitmap;
    BOOL FreePhysicalPage;
    PIMAGE_SECTION OwningSection;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    UINTN PageIndex;
    BOOL PageMapped;
    ULONG PageShift;
    BOOL PageWasDirty;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;

    ASSERT(((Flags & IMAGE_SECTION_UNMAP_FLAG_PAGE_CACHE_ONLY) == 0) ||
           ((Section->Flags & IMAGE_SECTION_BACKED) != 0));

    ASSERT(KeIsQueuedLockHeld(Section->Lock) != FALSE);

    //
    // If the image section is destroyed, then everything was already unmapped
    // and any children have broken their inheritance.
    //

    if ((Section->Flags & IMAGE_SECTION_DESTROYED) != 0) {
        return STATUS_SUCCESS;
    }

    //
    // Increment the image section's sequence number if this unmap is due to a
    // truncation.
    //

    if ((Flags & IMAGE_SECTION_UNMAP_FLAG_TRUNCATE) != 0) {
        Section->TruncateCount += 1;
    }

    //
    // Return immediately if the image section has never been touched.
    //

    if (Section->MinTouched >= Section->MaxTouched) {
        return STATUS_SUCCESS;
    }

    //
    // Clip the bounds by what has been actually accessed in the section.
    //

    ASSERT_SECTION_TOUCH_BOUNDARIES(Section);

    PageShift = MmPageShift();
    Boundary = (Section->MaxTouched - Section->VirtualAddress) >> PageShift;
    if (Boundary <= PageOffset) {
        return STATUS_SUCCESS;
    }

    if (Boundary < PageOffset + PageCount) {
        PageCount = Boundary - PageOffset;
    }

    Boundary = (Section->MinTouched - Section->VirtualAddress) >> PageShift;
    if (Boundary >= PageOffset + PageCount) {
        return STATUS_SUCCESS;
    }

    if (Boundary > PageOffset) {
        PageCount = PageOffset + PageCount - Boundary;
        PageOffset = Boundary;
    }

    //
    // Iterate over the region of the image section that needs to be unmapped
    // and unmap each page.
    //

    ASSERT(IS_ALIGNED((UINTN)Section->VirtualAddress, MmPageSize()) != FALSE);

    for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
        BitmapIndex = IMAGE_SECTION_BITMAP_INDEX(PageOffset + PageIndex);
        BitmapMask = IMAGE_SECTION_BITMAP_MASK(PageOffset + PageIndex);
        CurrentPageOffset = PageOffset + PageIndex;

        //
        // If only unmapping backed pages, skip the page if the owner is
        // dirty, as it could only be mapping a private page. Shared sections
        // never map private pages.
        //

        if (((Flags & IMAGE_SECTION_UNMAP_FLAG_PAGE_CACHE_ONLY) != 0) &&
            ((Section->Flags & IMAGE_SECTION_SHARED) == 0)) {

            ASSERT((Section->Flags & IMAGE_SECTION_BACKED) != 0);

            OwningSection = MmpGetOwningSection(Section, CurrentPageOffset);
            DirtyPageBitmap = OwningSection->DirtyPageBitmap;
            if ((DirtyPageBitmap[BitmapIndex] & BitmapMask) != 0) {
                MmpImageSectionReleaseReference(OwningSection);
                OwningSection = NULL;
                continue;
            }

            MmpImageSectionReleaseReference(OwningSection);
            OwningSection = NULL;
        }

        //
        // If the section is not mapped at this page offset, then neither are
        // any of its inheriting children. Skip it.
        //

        PageMapped = MmpIsImageSectionMapped(Section,
                                             CurrentPageOffset,
                                             &PhysicalAddress);

        if (PageMapped == FALSE) {

            //
            // When unmapping due to truncation, reset the dirty bit, even if
            // there is no page to unmap. Page-in needs to start fresh for any
            // pages touched by this routine in this case.
            //

            if (((Flags & IMAGE_SECTION_UNMAP_FLAG_TRUNCATE) != 0) &&
                (Section->DirtyPageBitmap != NULL)) {

                Section->DirtyPageBitmap[BitmapIndex] &= ~BitmapMask;
            }

            continue;
        }

        ASSERT(PhysicalAddress != INVALID_PHYSICAL_ADDRESS);

        //
        // If only page cache backed entries are to be unmapped and this is a
        // non-paged section with a valid mapping at this location, then fail
        // the unmap. This prevents the page cache from unmapping pinned pages.
        //

        if (((Flags & IMAGE_SECTION_UNMAP_FLAG_PAGE_CACHE_ONLY) != 0) &&
            ((Section->Flags & IMAGE_SECTION_NON_PAGED) != 0)) {

            Status = STATUS_RESOURCE_IN_USE;
            goto UnmapImageSectionEnd;
        }

        //
        // If the section is mapped shared or the section is backed and isn't
        // dirty (i.e. it still maps the backing image), then do not free the
        // physical page.
        //

        FreePhysicalPage = TRUE;
        if (((Section->Flags & IMAGE_SECTION_SHARED) != 0) ||
            (((Section->Flags & IMAGE_SECTION_BACKED) != 0) &&
             ((Section->DirtyPageBitmap[BitmapIndex] & BitmapMask) == 0))) {

            FreePhysicalPage = FALSE;
        }

        //
        // Unmap the page from the section and any inheriting children.
        //

        MmpModifySectionMapping(Section,
                                CurrentPageOffset,
                                INVALID_PHYSICAL_ADDRESS,
                                FALSE,
                                &PageWasDirty,
                                TRUE);

        //
        // If this is a shared, writable image section and the mapping was
        // dirty, then mark the associated page cache entry dirty. As this
        // routine does not handle paging out, the callers must not care about
        // the dirty state of any other image section type (e.g. clip and
        // truncate don't need to save private dirty pages).
        //

        if (((Section->Flags & IMAGE_SECTION_SHARED) != 0) &&
            ((Section->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0) &&
            ((Section->Flags & IMAGE_SECTION_WAS_WRITABLE) != 0) &&
            (PageWasDirty != FALSE)) {

            PageCacheEntry = MmpGetPageCacheEntryForPhysicalAddress(
                                                              PhysicalAddress);

            ASSERT(PageCacheEntry != NULL);

            IoMarkPageCacheEntryDirty(PageCacheEntry);
        }

        //
        // If it was determined above that the phyiscal page could be released,
        // free it now.
        //

        if (FreePhysicalPage != FALSE) {

            ASSERT((Flags & IMAGE_SECTION_UNMAP_FLAG_PAGE_CACHE_ONLY) == 0);

            MmFreePhysicalPage(PhysicalAddress);
        }

        //
        // When unmapping due to truncation, reset the dirty bit. Page-in needs
        // to start fresh for any pages touched by this routine in this case.
        //

        if (((Flags & IMAGE_SECTION_UNMAP_FLAG_TRUNCATE) != 0) &&
            (Section->DirtyPageBitmap != NULL)) {

            Section->DirtyPageBitmap[BitmapIndex] &= ~BitmapMask;
        }
    }

    Status = STATUS_SUCCESS;

UnmapImageSectionEnd:
    return Status;
}

BOOL
MmpIsImageSectionMapped (
    PIMAGE_SECTION Section,
    ULONG PageOffset,
    PPHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine determines whether or not an image section is mapped at the
    given page offset.

Arguments:

    Section - Supplies a pointer to an image section.

    PageOffset - Supplies an offset, in pages, into the image section.

    PhysicalAddress - Supplies an optional pointer that receives the physical
        address mapped at the given section offset.

Return Value:

    Returns TRUE if the image section is mapped at the current offset or FALSE
    otherwise.

--*/

{

    PVOID Address;
    PADDRESS_SPACE AddressSpace;
    PKPROCESS CurrentProcess;
    BOOL Mapped;
    PHYSICAL_ADDRESS MappedPhysicalAddress;

    AddressSpace = Section->AddressSpace;
    CurrentProcess = PsGetCurrentProcess();

    //
    // Determine if the image section is mapped at the given page offset based
    // on the process to which it belongs.
    //

    Address = Section->VirtualAddress + (PageOffset << MmPageShift());
    MappedPhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    if ((AddressSpace == CurrentProcess->AddressSpace) ||
        (AddressSpace == MmKernelAddressSpace)) {

        MappedPhysicalAddress = MmpVirtualToPhysical(Address, NULL);

    } else {
        MappedPhysicalAddress = MmpVirtualToPhysicalInOtherProcess(AddressSpace,
                                                                   Address);
    }

    //
    // If a valid physical address was found above then the section is mapped
    // at the given page offset.
    //

    Mapped = FALSE;
    if (MappedPhysicalAddress != INVALID_PHYSICAL_ADDRESS) {
        Mapped = TRUE;
    }

    //
    // Return the physical address if requested.
    //

    if (PhysicalAddress != NULL) {
        *PhysicalAddress = MappedPhysicalAddress;
    }

    return Mapped;
}

VOID
MmpDestroyImageSectionMappings (
    PIMAGE_SECTION Section
    )

/*++

Routine Description:

    This routine destroys the mappings for the given image section. It assumes
    that the image section's lock is held.

Arguments:

    Section - Supplies a pointer to the image section whose mappings are to be
        destroyed.

Return Value:

    None.

--*/

{

    PADDRESS_SPACE AddressSpace;
    ULONG Attributes;
    UINTN BitmapIndex;
    ULONG BitmapMask;
    PVOID CurrentAddress;
    PKPROCESS CurrentProcess;
    UINTN MinOffset;
    BOOL MultipleIpisRequired;
    BOOL OtherProcess;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    UINTN PageCount;
    UINTN PageIndex;
    ULONG PageShift;
    ULONG PageSize;
    BOOL PageWasDirty;
    PHYSICAL_ADDRESS PhysicalAddress;
    PHYSICAL_ADDRESS RunPhysicalAddress;
    UINTN RunSize;
    ULONG UnmapFlags;

    PageSize = MmPageSize();

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeIsQueuedLockHeld(Section->Lock) != FALSE);
    ASSERT_SECTION_TOUCH_BOUNDARIES(Section);
    ASSERT(IS_POINTER_ALIGNED(Section->VirtualAddress, PageSize));

    //
    // If this section has been reduced to nothing, then just exit.
    //

    PageShift = MmPageShift();
    if (Section->MinTouched >= Section->MaxTouched) {
        return;
    }

    CurrentProcess = PsGetCurrentProcess();
    AddressSpace = Section->AddressSpace;

    //
    // Record the first virtual address of the section.
    //

    CurrentAddress = Section->MinTouched;
    PageCount = (Section->MaxTouched - CurrentAddress) >> PageShift;
    MinOffset = (CurrentAddress - Section->VirtualAddress) >> PageShift;

    //
    // Depending on the image section, there are different, more efficient
    // paths to unmap and free the region. Sections belonging to the current
    // process or the kernel process are most efficient.
    //

    MultipleIpisRequired = TRUE;
    if ((AddressSpace == CurrentProcess->AddressSpace) ||
        (AddressSpace == MmKernelAddressSpace)) {

        //
        // If the section has no parent and is not backed then it should have
        // no children and own all its pages. Freely unmap and release all
        // pages. The work here is then done.
        //

        if ((Section->Parent == NULL) &&
            ((Section->Flags & IMAGE_SECTION_BACKED) == 0)) {

            ASSERT(LIST_EMPTY(&(Section->ChildList)) != FALSE);
            ASSERT((Section->Flags & IMAGE_SECTION_SHARED) == 0);

            UnmapFlags = UNMAP_FLAG_SEND_INVALIDATE_IPI |
                         UNMAP_FLAG_FREE_PHYSICAL_PAGES;

            MmpUnmapPages(CurrentAddress, PageCount, UnmapFlags, NULL);
            goto DestroyImageSectionMappingsEnd;
        }

        //
        // Otherwise each page needs to be treated individually and, if the
        // section is shared, any dirty mappings need to trigger a page cache
        // flush. Determine whether or not multiple IPIs will need to be sent
        // when unmapping pages. Single page sections will never need to send
        // multiple IPIs. Otherwise kernel sections always require IPIs, but
        // user mode sections for processes with one or no threads do not need
        // to IPI.
        //

        if ((PageCount == 1) ||
            ((CurrentProcess->ThreadCount <= 1) &&
             (CurrentAddress < KERNEL_VA_START))) {

            MultipleIpisRequired = FALSE;
        }

        //
        // If multiple IPIs would be required during the unmap phase, get it
        // over with now. Set the whole section to not present and send 1 IPI.
        //

        if (MultipleIpisRequired != FALSE) {
            MmpChangeMemoryRegionAccess(CurrentAddress,
                                        PageCount,
                                        0,
                                        MAP_FLAG_PRESENT);
        }

        OtherProcess = FALSE;

    //
    // Sections belonging to other processes need to use the helper routines
    // that unmap pages belonging to another process. This is not as efficient,
    // but is a rare case.
    //

    } else {

        //
        // There should be no non-paged sections in user mode.
        //

        ASSERT((Section->Flags & IMAGE_SECTION_NON_PAGED) == 0);

        OtherProcess = TRUE;
    }

    //
    // Now run through each page.
    //

    RunSize = 0;
    RunPhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    PhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
        BitmapIndex = IMAGE_SECTION_BITMAP_INDEX(PageIndex + MinOffset);
        BitmapMask = IMAGE_SECTION_BITMAP_MASK(PageIndex + MinOffset);
        UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                     UNMAP_FLAG_SEND_INVALIDATE_IPI;

        PageWasDirty = FALSE;

        //
        // If the page is shared with the section's parent, the section is
        // mapped shared, or the section is backed by something and is not
        // dirty, then do not free the physical page on unmap.
        //

        if (((Section->Parent != NULL) &&
             ((Section->InheritPageBitmap[BitmapIndex] & BitmapMask) != 0)) ||
            ((Section->Flags & IMAGE_SECTION_SHARED) != 0) ||
            (((Section->Flags & IMAGE_SECTION_BACKED) != 0) &&
             ((Section->DirtyPageBitmap[BitmapIndex] & BitmapMask) == 0))) {

            UnmapFlags &= ~UNMAP_FLAG_FREE_PHYSICAL_PAGES;
        }

        ASSERT(CurrentAddress >= Section->VirtualAddress);

        //
        // If this is a section for the current process or kernel process, get
        // the physical address that used to be mapped and the attributes
        // associated with the mapping. The attributes should not be in a state
        // of change due to the unmap above.
        //

        if (OtherProcess == FALSE) {
            PhysicalAddress = MmpVirtualToPhysical(CurrentAddress, &Attributes);
            if (PhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
                CurrentAddress += PageSize;
                continue;
            }

            if (MultipleIpisRequired != FALSE) {

                ASSERT((Attributes & MAP_FLAG_PRESENT) == 0);

                if ((UnmapFlags & UNMAP_FLAG_FREE_PHYSICAL_PAGES) != 0) {
                    if (RunSize != 0) {
                        if ((RunPhysicalAddress + RunSize) == PhysicalAddress) {
                            RunSize += PageSize;

                        } else {
                            MmFreePhysicalPages(RunPhysicalAddress,
                                                RunSize >> PageShift);

                            RunPhysicalAddress = PhysicalAddress;
                            RunSize = PageSize;
                        }

                    } else {
                        RunPhysicalAddress = PhysicalAddress;
                        RunSize = PageSize;
                    }

                } else if ((Attributes & MAP_FLAG_DIRTY) != 0) {
                    PageWasDirty = TRUE;
                }

            } else {
                MmpUnmapPages(CurrentAddress, 1, UnmapFlags, &PageWasDirty);
            }

        //
        // Otherwise just get the physical address and unmap it if necessary.
        //

        } else {
            PhysicalAddress = MmpVirtualToPhysicalInOtherProcess(
                                                               AddressSpace,
                                                               CurrentAddress);

            if (PhysicalAddress != INVALID_PHYSICAL_ADDRESS) {
                MmpUnmapPageInOtherProcess(AddressSpace,
                                           CurrentAddress,
                                           UnmapFlags,
                                           &PageWasDirty);
            }
        }

        //
        // If this is a shared section that was writable and the page was dirty,
        // then the page cache needs to be notified about this dirty page.
        //

        if (((Section->Flags & IMAGE_SECTION_SHARED) != 0) &&
            ((Section->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0) &&
            ((Section->Flags & IMAGE_SECTION_WAS_WRITABLE) != 0) &&
            (PageWasDirty != FALSE)) {

            ASSERT(PhysicalAddress != INVALID_PHYSICAL_ADDRESS);
            ASSERT((UnmapFlags & UNMAP_FLAG_FREE_PHYSICAL_PAGES) == 0);

            //
            // The page cache entries are in paged pool, but since this is a
            // shared section, all mapped pages are from the page cache and not
            // eligible for page-out. So, like non-paged pool, this cannot
            // cause a dead-lock. Recall that the section's lock is held.
            //

            PageCacheEntry = MmpGetPageCacheEntryForPhysicalAddress(
                                                              PhysicalAddress);

            //
            // Mark it dirty.
            //

            IoMarkPageCacheEntryDirty(PageCacheEntry);
        }

        CurrentAddress += PageSize;
    }

    if (RunSize != 0) {
        MmFreePhysicalPages(RunPhysicalAddress, RunSize >> PageShift);
    }

    //
    // When handling a section for the current process or kernel process that
    // would have required multiple IPIs, all pages have been set to
    // "not present" and the physical pages have been dispatched as necessary.
    // So, finish with a final "unmap" to set the page table entries to zero
    // and update the mapping metrics.
    //

    if ((OtherProcess == FALSE) && (MultipleIpisRequired != FALSE)) {
        MmpUnmapPages(Section->MinTouched, PageCount, 0, NULL);
    }

DestroyImageSectionMappingsEnd:
    return;
}

