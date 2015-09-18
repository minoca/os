/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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

#include <minoca/kernel.h>
#include <minoca/bootload.h>
#include "mmp.h"

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
    PKPROCESS Process,
    PVOID VirtualAddress,
    UINTN Size,
    ULONG Flags,
    HANDLE ImageHandle,
    ULONGLONG ImageOffset,
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
    BOOL ProcessLockHeld
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
    ULONG Flags,
    PBOOL PageWasDirty
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
// ------------------------------------------------------------------ Functions
//

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
    ULONGLONG Offset,
    ULONGLONG Size,
    ULONG Flags,
    PBOOL PageWasDirty
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
        be page aligned.

    Flags - Supplies a bitmask of flags for the unmap. See
        IMAGE_SECTION_UNMAP_FLAG_* for definitions.

    PageWasDirty - Supplies a pointer where a boolean will be returned
        indicating if any page that was unmapped was dirty. This parameter is
        optional.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PIMAGE_SECTION CurrentSection;
    BOOL Dirty;
    ULONGLONG EndOffset;
    UINTN PageCount;
    UINTN PageOffset;
    ULONG PageShift;
    PIMAGE_SECTION ReleaseSection;
    ULONGLONG StartOffset;
    KSTATUS Status;
    ULONGLONG UnmapEndOffset;
    ULONGLONG UnmapStartOffset;

    ASSERT(IS_ALIGNED(Offset, MmPageSize()) != FALSE);
    ASSERT(IS_ALIGNED(Size, MmPageSize()) != FALSE);
    ASSERT((Offset + Size) > Offset);

    PageShift = MmPageShift();
    ReleaseSection = NULL;
    if (PageWasDirty != NULL) {
        *PageWasDirty = FALSE;
    }

    //
    // Iterate over the sections in the list. Sections are added to the list
    // such that children are processed after their parent. Without this order
    // a section copy could be created after the loop starts, get added to the
    // beginning and then not get unmapped.
    //

    UnmapStartOffset = Offset;
    UnmapEndOffset = UnmapStartOffset + Size;
    KeAcquireQueuedLock(ImageSectionList->Lock);
    CurrentEntry = ImageSectionList->ListHead.Next;
    while (CurrentEntry != &(ImageSectionList->ListHead)) {
        CurrentSection = LIST_VALUE(CurrentEntry,
                                    IMAGE_SECTION,
                                    ImageListEntry);

        ASSERT((CurrentSection->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0);
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
                                      Flags,
                                      &Dirty);

        KeReleaseQueuedLock(CurrentSection->Lock);
        if (!KSUCCESS(Status)) {
            MmpImageSectionReleaseReference(CurrentSection);
            goto UnmapImageSectionListEnd;
        }

        if ((Dirty != FALSE) && (PageWasDirty != NULL)) {

            //
            // Truncate should not be interested in dirty pages.
            //

            ASSERT((Flags & IMAGE_SECTION_UNMAP_FLAG_TRUNCATE) == 0);

            *PageWasDirty = TRUE;
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

    PLIST_ENTRY CurrentEntry;
    PVOID End;
    PKPROCESS KernelProcess;
    UINTN PageSize;
    PKPROCESS Process;
    PIMAGE_SECTION Section;
    PVOID SectionEnd;
    KSTATUS Status;

    PageSize = MmPageSize();

    ASSERT((NewAccess & ~(IMAGE_SECTION_ACCESS_MASK)) == 0);
    ASSERT(IS_ALIGNED((UINTN)Address | Size, PageSize));

    Process = PsGetCurrentProcess();
    KernelProcess = PsGetKernelProcess();
    KeAcquireQueuedLock(Process->QueuedLock);
    Status = STATUS_SUCCESS;
    End = Address + Size;
    CurrentEntry = Process->SectionListHead.Next;
    while (CurrentEntry != &(Process->SectionListHead)) {
        Section = LIST_VALUE(CurrentEntry, IMAGE_SECTION, ProcessListEntry);
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
                if (Section->Process == KernelProcess) {

                    ASSERT(FALSE);

                    Status = STATUS_NOT_SUPPORTED;
                    break;
                }

                //
                // Split the portion of the section that doesn't apply to this
                // region.
                //

                if (Section->VirtualAddress < Address) {
                    Status = MmpClipImageSection(&(Process->SectionListHead),
                                                 Address,
                                                 0,
                                                 Section);

                    if (!KSUCCESS(Status)) {
                        break;
                    }

                    ASSERT(Section->VirtualAddress + Section->Size == Address);

                    CurrentEntry = Section->ProcessListEntry.Next;
                    continue;
                }

                //
                // Clip a region of the section with size zero to break up the
                // section.
                //

                Status = MmpClipImageSection(&(Process->SectionListHead),
                                             End,
                                             0,
                                             Section);

                if (!KSUCCESS(Status)) {
                    break;
                }

                ASSERT(Section->VirtualAddress + Section->Size == End);

                CurrentEntry = Section->ProcessListEntry.Next;
            }

            ASSERT((Section->VirtualAddress >= Address) &&
                   ((Section->VirtualAddress + Section->Size) <= End));

            Status = MmpChangeImageSectionAccess(Section, NewAccess);
            if (!KSUCCESS(Status)) {
                break;
            }
        }
    }

    KeReleaseQueuedLock(Process->QueuedLock);
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
                              PsGetCurrentProcess(),
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
                                  Process,
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
    PKPROCESS Process,
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

    Process - Supplies the process to look in.

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

    KeAcquireQueuedLock(Process->QueuedLock);
    CurrentSectionEntry = Process->SectionListHead.Next;
    while (CurrentSectionEntry != &(Process->SectionListHead)) {
        CurrentSection = LIST_VALUE(CurrentSectionEntry,
                                    IMAGE_SECTION,
                                    ProcessListEntry);

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
    KeReleaseQueuedLock(Process->QueuedLock);
    return Status;
}

KSTATUS
MmpAddImageSection (
    PKPROCESS Process,
    PVOID VirtualAddress,
    UINTN Size,
    ULONG Flags,
    HANDLE ImageHandle,
    ULONGLONG ImageOffset
    )

/*++

Routine Description:

    This routine creates an image section for the given process so that page
    faults can be recognized and handled appropriately. This routine must be
    called at low level.

Arguments:

    Process - Supplies a pointer to the process to create the section
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
    // The caller should not be supplying the cache-backed or anonymous flags.
    //

    ASSERT((Flags & IMAGE_SECTION_INTERNAL_MASK) == 0);

    Status = MmpAllocateImageSection(Process,
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
    // Lock the process and destroy any image sections that were there.
    //

    KeAcquireQueuedLock(Process->QueuedLock);
    Status = MmpClipImageSections(&(Process->SectionListHead),
                                  VirtualAddress,
                                  Size,
                                  &EntryBefore);

    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

        goto AddImageSectionEnd;
    }

    INSERT_AFTER(&(NewSection->ProcessListEntry), EntryBefore);
    KeReleaseQueuedLock(Process->QueuedLock);

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
            if (NewSection->ProcessListEntry.Next != NULL) {
                KeAcquireQueuedLock(Process->QueuedLock);
                LIST_REMOVE(&(NewSection->ProcessListEntry));
                KeReleaseQueuedLock(Process->QueuedLock);
                NewSection->ProcessListEntry.Next = NULL;
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
    PKPROCESS DestinationProcess
    )

/*++

Routine Description:

    This routine copies an image section to another process.

Arguments:

    SectionToCopy - Supplies a pointer to the section to copy.

    DestinationProcess - Supplies a pointer to the process to copy the section
        to.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated to create
        the image or there is no more room in the page file.

--*/

{

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
    PKPROCESS Process;
    BOOL ProcessLockHeld;
    KSTATUS Status;

    ImageSectionList = NULL;
    NewSection = NULL;
    PageSize = MmPageSize();
    PageShift = MmPageShift();
    ParentDestroyed = FALSE;

    ASSERT(POWER_OF_2(PageSize) != FALSE);

    PageMask = PageSize - 1;
    Process = PsGetCurrentProcess();
    ProcessLockHeld = FALSE;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((((UINTN)SectionToCopy->VirtualAddress & PageMask) == 0) &&
           ((SectionToCopy->Size & PageMask) == 0));

    //
    // Currently the copied section must be in the current process, as the
    // standard "virtual to physical" function is used to determine which pages
    // are mapped in.
    //

    ASSERT(SectionToCopy->Process == Process);

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

        ASSERT((SectionToCopy->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0);
        ASSERT(SectionToCopy->ImageBacking.DeviceHandle != INVALID_HANDLE);

        Flags = SectionToCopy->Flags & IMAGE_SECTION_COPY_MASK;
        Status = MmpAddImageSection(DestinationProcess,
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
    NewSection = MmAllocateNonPagedPool(AllocationSize, MM_ALLOCATION_TAG);
    if (NewSection == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CopyImageSectionEnd;
    }

    //
    // Zero the dirty bitmap and initialize the inherit bitmap to all 1's.
    // As this is a hot path, do not zero the image section itself. The code
    // below initializes all the appropriate fields.
    //

    NewSection->DirtyPageBitmap = (PULONG)(NewSection + 1);
    NewSection->InheritPageBitmap = NewSection->DirtyPageBitmap +
                                    (BitmapSize / sizeof(ULONG));

    RtlZeroMemory(NewSection->DirtyPageBitmap, BitmapSize);
    RtlSetMemory(NewSection->InheritPageBitmap, MAX_UCHAR, BitmapSize);
    NewSection->ReferenceCount = 1;
    NewSection->Flags = SectionToCopy->Flags;
    if ((NewSection->Flags & IMAGE_SECTION_WRITABLE) != 0) {
        NewSection->Flags |= IMAGE_SECTION_WAS_WRITABLE;

    } else {
        NewSection->Flags &= ~IMAGE_SECTION_WAS_WRITABLE;
    }

    INITIALIZE_LIST_HEAD(&(NewSection->ChildList));
    NewSection->Process = DestinationProcess;
    ObAddReference(NewSection->Process);
    NewSection->VirtualAddress = SectionToCopy->VirtualAddress;
    NewSection->Size = SectionToCopy->Size;
    NewSection->TruncateCount = 0;
    NewSection->SwapSpace = NULL;
    NewSection->PagingInIrp = NULL;
    NewSection->ProcessListEntry.Next = NULL;
    NewSection->ImageListEntry.Next = NULL;

    //
    // If the image section is page cache backed, then it will add itself to
    // the backing image's list of image sections. Take a reference on the
    // backing image while this section is around so it is not removed while
    // the image section's entry is in the list.
    //

    if ((NewSection->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0) {
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
    // If the image section is backed by the page cache, then insert it into
    // the owning file object's list of image sections. This allows file size
    // modifications to unmap any portions of the image section beyond the new
    // file size.
    //

    if ((NewSection->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0) {

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

    //
    // Lock the source section and try to add this one as a child.
    //

    KeAcquireQueuedLock(SectionToCopy->Lock);

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

    Status = MmpCopyAndChangeSectionMappings(DestinationProcess,
                                             Process->PageDirectory,
                                             SectionToCopy->VirtualAddress,
                                             SectionToCopy->Size);

    if (!KSUCCESS(Status)) {
        goto CopyImageSectionEnd;
    }

    KeReleaseQueuedLock(SectionToCopy->Lock);

    //
    // Lock the process and determine where the new section should be inserted.
    //

    KeAcquireQueuedLock(DestinationProcess->QueuedLock);
    ProcessLockHeld = TRUE;
    CurrentEntry = DestinationProcess->SectionListHead.Next;
    while (CurrentEntry != &(DestinationProcess->SectionListHead)) {
        CurrentSection = LIST_VALUE(CurrentEntry,
                                    IMAGE_SECTION,
                                    ProcessListEntry);

        if (CurrentSection->VirtualAddress > NewSection->VirtualAddress) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Insert the section onto the destination process list.
    //

    INSERT_BEFORE(&(NewSection->ProcessListEntry), CurrentEntry);
    Status = STATUS_SUCCESS;

CopyImageSectionEnd:
    if (ProcessLockHeld != FALSE) {
        KeReleaseQueuedLock(DestinationProcess->QueuedLock);
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
    PKPROCESS Process,
    PVOID SectionAddress,
    UINTN Size
    )

/*++

Routine Description:

    This routine unmaps and destroys any image sections at the given address.
    This routine must be called at low level. For kernel mode, this must
    specify a single whole image section.

Arguments:

    Process - Supplies a pointer to the process containing the image
        section.

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

    if (Process == PsGetKernelProcess()) {
        Status = STATUS_SUCCESS;
        while (Size != 0) {
            Status = MmpLookupSection(SectionAddress,
                                      Process,
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
        KeAcquireQueuedLock(Process->QueuedLock);
        Status = MmpClipImageSections(&(Process->SectionListHead),
                                      SectionAddress,
                                      Size,
                                      NULL);

        KeReleaseQueuedLock(Process->QueuedLock);
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
    ULONG DirtyPageCount;
    ULONGLONG DirtySize;
    ULONG FirstDirtyPage;
    ULONG LastDirtyPage;
    BOOL LockHeld;
    BOOL MarkedDirty;
    ULONGLONG Offset;
    ULONG PageAttributes;
    ULONG PageIndex;
    ULONG PageShift;
    PHYSICAL_ADDRESS PhysicalAddress;
    BOOL ReferenceAdded;
    ULONG RegionEndOffset;
    KSTATUS Status;

    ASSERT(Section->Process == PsGetCurrentProcess());

    LockHeld = FALSE;
    ReferenceAdded = FALSE;
    PageShift = MmPageShift();
    KeAcquireQueuedLock(Section->Lock);
    LockHeld = TRUE;

    //
    // There is nothing to flush if the image section is not writable or not
    // shared.
    //

    if (((Section->Flags & IMAGE_SECTION_WAS_WRITABLE) == 0) ||
        ((Section->Flags & IMAGE_SECTION_SHARED) == 0)) {

        Status = STATUS_SUCCESS;
        goto FlushImageSectionRegionEnd;
    }

    ASSERT((Section->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0);
    ASSERT((Section->Flags & IMAGE_SECTION_SHARED) != 0);

    //
    // Acquire the image section lock and search for dirty pages in the given
    // region. If they are dirty, mark the backing page cache entry as dirty.
    //

    RegionEndOffset = PageOffset + PageCount;
    FirstDirtyPage = RegionEndOffset;
    LastDirtyPage = PageOffset;
    if ((Section->Flags & IMAGE_SECTION_DESTROYED) != 0) {
        Status = STATUS_SUCCESS;
        goto FlushImageSectionRegionEnd;
    }

    ASSERT(Section->ImageBacking.DeviceHandle != INVALID_HANDLE);

    MmpImageSectionAddImageBackingReference(Section);
    ReferenceAdded = TRUE;
    DirtyPageCount = 0;
    for (PageIndex = PageOffset; PageIndex < RegionEndOffset; PageIndex += 1) {

        //
        // Skip any pages that are not mapped, not dirty, or not writable.
        //

        CurrentAddress = Section->VirtualAddress + (PageIndex << PageShift);
        PhysicalAddress = MmpVirtualToPhysical(CurrentAddress, &PageAttributes);
        if ((PhysicalAddress == INVALID_PHYSICAL_ADDRESS) ||
            ((PageAttributes & MAP_FLAG_DIRTY) == 0) ||
            ((PageAttributes & MAP_FLAG_READ_ONLY) != 0)) {

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

        //
        // The page cache entries are in paged pool, but since this is a
        // shared section, all mapped pages are from the page cache and not
        // eligible for page-out. So this cannot cause a dead-lock.
        //

        CacheEntry = MmpGetPageCacheEntryForPhysicalAddress(PhysicalAddress);

        //
        // The page cache entry must be present. It was mapped and the only
        // way for it to be removed is for the page cache to have unmapped
        // it, which requires obtaining the image section lock.
        //

        ASSERT(CacheEntry != NULL);

        //
        // Mark it dirty.
        //

        MarkedDirty = IoMarkPageCacheEntryDirty(CacheEntry, 0, 0, TRUE);
        if (MarkedDirty != FALSE) {
            DirtyPageCount += 1;
        }
    }

    //
    // Release the lock and attempt to flush to the backing image, if necessary.
    //

    KeReleaseQueuedLock(Section->Lock);
    LockHeld = FALSE;
    if (DirtyPageCount == 0) {
        Status = STATUS_SUCCESS;
        goto FlushImageSectionRegionEnd;
    }

    //
    // If performing an asynchronous flush, then just schedule the whole page
    // cache to be flushed.
    //

    if ((Flags & IMAGE_SECTION_FLUSH_FLAG_ASYNC) != 0) {
        Status = IoFlush(INVALID_HANDLE, 0, 0, FLUSH_FLAG_ALL);

    //
    // Otherwise flush the necessary region of the file.
    //

    } else {

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
    }

    if (!KSUCCESS(Status)) {
        goto FlushImageSectionRegionEnd;
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
    // inherits from a parent or the page cache. Shared sections are the
    // exception. Write faults for those just need the mapping access bits
    // changed. All of these pages need to be allocated with the section lock
    // released. The routine needs to set the private page for itself after
    // handling the children, but it cannot release the section lock at that
    // point because a new child may appear. As a result, allocate a paging
    // entry and physical page before acquiring the lock if they might be
    // needed later.
    //

    if (((Section->Parent != NULL) ||
         ((Section->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0)) &&
        ((Section->Flags & IMAGE_SECTION_SHARED) == 0) &&
        ((Section->InheritPageBitmap[BitmapIndex] & BitmapMask) != 0)) {

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
    // Loop through and allocate writable copies for all children.
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

    if ((Section->Flags & IMAGE_SECTION_DESTROYED) != 0) {
        Status = STATUS_SUCCESS;
        goto IsolateImageSectionEnd;
    }

    //
    // Compute the appropriate flags for the page.
    //

    MapFlags = MAP_FLAG_PAGABLE;
    if ((Section->Flags & IMAGE_SECTION_READABLE) != 0) {
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
    // from a parent or the page cache and the section is writable, then simply
    // change the attributes on the page. Shared sections always get the page
    // attributes set.
    //

    if (((Section->Parent == NULL) &&
         ((Section->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) == 0)) ||
        ((Section->Flags & IMAGE_SECTION_SHARED) != 0) ||
        ((Section->InheritPageBitmap[BitmapIndex] & BitmapMask) == 0)) {

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

        ASSERT((Section->Flags & IMAGE_SECTION_SHARED) == 0);

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
        }

        ASSERT(Section->InheritPageBitmap != NULL);

        Section->InheritPageBitmap[BitmapIndex] &= ~BitmapMask;
        PagingEntry = NULL;
        PhysicalAddress = INVALID_PHYSICAL_ADDRESS;
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
    assumes the process lock is already held. This routine does not change
    any accountant mappings.

Arguments:

    SectionListHead - Supplies a pointer to the head of the list of image
        sections for the process.

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
        Section = LIST_VALUE(CurrentEntry, IMAGE_SECTION, ProcessListEntry);
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
    PKPROCESS Process,
    PVOID VirtualAddress,
    UINTN Size,
    ULONG Flags,
    HANDLE ImageHandle,
    ULONGLONG ImageOffset,
    PIMAGE_SECTION *AllocatedSection
    )

/*++

Routine Description:

    This routine allocates and initializes a new image section.

Arguments:

    Process - Supplies a pointer to the process to create the section
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
    PIMAGE_SECTION NewSection;
    UINTN PageCount;
    ULONG PageMask;
    ULONG PageShift;
    ULONG PageSize;
    UINTN SizeWhenAlignedToPageBoundaries;
    KSTATUS Status;

    ImageSectionList = NULL;
    NewSection = NULL;
    PageSize = MmPageSize();
    PageShift = MmPageShift();

    ASSERT(POWER_OF_2(PageSize) != FALSE);

    PageMask = PageSize - 1;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((VirtualAddress < KERNEL_VA_START) ||
           (Process == PsGetKernelProcess()));

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
    // If an image section can use pages directly collected from the page cache
    // then mark it as page cache backed. This is only the case if there is an
    // image handle and the offset is cache-aligned.
    //

    if ((ImageHandle != INVALID_HANDLE) &&
        (IS_ALIGNED(ImageOffset, IoGetCacheEntryDataSize()) != FALSE) &&
        (IoIoHandleIsCacheable(ImageHandle) != FALSE)) {

        Flags |= IMAGE_SECTION_PAGE_CACHE_BACKED;

        //
        // Private, page cache backed sections get an inherit bitmap. This is
        // used to track whether or not a mapped page is from the page cache.
        // Shared sections always inherit from the page cache and do not need
        // an inherit bitmap.
        //

        if ((Flags & IMAGE_SECTION_SHARED) == 0) {
            BitmapCount += 1;
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

        Flags &= ~IMAGE_SECTION_PAGE_CACHE_BACKED;
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
    NewSection = MmAllocateNonPagedPool(AllocationSize, MM_ALLOCATION_TAG);
    if (NewSection == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AllocateImageSectionEnd;
    }

    NewSection->ReferenceCount = 1;
    NewSection->Flags = Flags;
    NewSection->ProcessListEntry.Next = NULL;
    NewSection->ImageListEntry.Next = NULL;
    NewSection->CopyListEntry.Next = NULL;
    NewSection->CopyListEntry.Previous = NULL;
    NewSection->Parent = NULL;
    INITIALIZE_LIST_HEAD(&(NewSection->ChildList));
    NewSection->Process = Process;
    ObAddReference(NewSection->Process);
    NewSection->VirtualAddress = VirtualAddress;
    NewSection->PagingInIrp = NULL;
    NewSection->SwapSpace = NULL;
    NewSection->Size = Size;
    NewSection->TruncateCount = 0;
    NewSection->PageFileBacking.DeviceHandle = INVALID_HANDLE;
    NewSection->ImageBacking.DeviceHandle = ImageHandle;
    NewSection->ImageBackingReferenceCount = 1;
    if (ImageHandle != INVALID_HANDLE) {
        IoIoHandleAddReference(ImageHandle);
        NewSection->ImageBacking.Offset = ImageOffset;
    }

    //
    // Set up the bitmaps based on the flags and number of bitmaps allocated.
    // Non-paged sections can only have 1 bitmap and it is the inherit bitmap
    // used by page cache backed sections.
    //

    ASSERT((BitmapCount != 0) ||
           ((Flags & IMAGE_SECTION_NON_PAGED) != 0) ||
           ((Flags & IMAGE_SECTION_SHARED) != 0));

    NewSection->InheritPageBitmap = NULL;
    NewSection->DirtyPageBitmap = NULL;
    if ((Flags & IMAGE_SECTION_SHARED) == 0) {
        if ((Flags & IMAGE_SECTION_NON_PAGED) != 0) {
            if (BitmapCount == 1) {

                ASSERT((Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0);

                NewSection->InheritPageBitmap = (PULONG)(NewSection + 1);
            }

        //
        // Private pageable sections always have the dirty page bitmap and may
        // have the inherit bitmap if they are page cache backed.
        //

        } else {

            ASSERT(BitmapCount >= 1);

            NewSection->DirtyPageBitmap = (PULONG)(NewSection + 1);
            RtlZeroMemory(NewSection->DirtyPageBitmap, BitmapSize);
            if (BitmapCount == 2) {
                NewSection->InheritPageBitmap = NewSection->DirtyPageBitmap +
                                                (BitmapSize / sizeof(ULONG));
            }
        }
    }

    //
    // Initialize the bitmap to inherit everything from the parent.
    //

    if (NewSection->InheritPageBitmap != NULL) {
        RtlSetMemory(NewSection->InheritPageBitmap, MAX_UCHAR, BitmapSize);
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
    // If the image section is backed by the page cache, then insert it into
    // the owning file object's list of image sections. This allows the page
    // cache to unmap from this section when it wants to evict a page.
    //

    if ((NewSection->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0) {

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
    into two. This routine assumes the process lock is already held (protecting
    VA changes).

Arguments:

    SectionListHead - Supplies a pointer to the head of the list of image
        sections for the process.

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
    ULONGLONG RemainderOffset;
    UINTN RemainderPages;
    PIMAGE_SECTION RemainderSection;
    UINTN RemainderSize;
    PVOID SectionEnd;
    ULONG SourceBlock;
    UINTN SourceIndex;
    KSTATUS Status;

    PageShift = MmPageShift();
    RegionEnd = Address + Size;
    RemainderSection = NULL;
    SectionEnd = Section->VirtualAddress + Section->Size;

    //
    // This doesn't work for the kernel process because it touches paged data
    // with the process lock held.
    //

    ASSERT(Section->Process != PsGetKernelProcess());
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
    // the process lock is held throughout.
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
        Status = MmpAllocateImageSection(Section->Process,
                                         RegionEnd,
                                         RemainderSize,
                                         Section->Flags,
                                         Section->ImageBacking.DeviceHandle,
                                         RemainderOffset,
                                         &RemainderSection);

        if (!KSUCCESS(Status)) {
            goto ClipImageSectionEnd;
        }
    }

    RemainderPages = RemainderSize >> PageShift;

    //
    // Acquire the section lock to prevent further changes to the bitmap.
    //

    KeAcquireQueuedLock(Section->Lock);
    if (RemainderSection != NULL) {

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
        if (Section->InheritPageBitmap != NULL) {
            for (BitmapIndex = 0; BitmapIndex < BitmapCount; BitmapIndex += 1) {
                SourceIndex = BitmapIndex + BitmapOffset;
                SourceBlock = Section->InheritPageBitmap[SourceIndex];
                RemainderSection->InheritPageBitmap[BitmapIndex] =
                                                    SourceBlock >> BitmapShift;

                if (BitmapIndex != BitmapCount - 1) {
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

                if (BitmapIndex != BitmapCount - 1) {
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
        MmpUnmapImageSection(Section, HolePageOffset, HolePageCount, 0, NULL);
        MmpFreePartialPageFileSpace(Section, HolePageOffset, HolePageCount);
    }

    //
    // Shrink the image section. Everything after the beginning of the hole is
    // either unmapped and destroyed or handed off to the remainder section.
    //

    Section->Size = (UINTN)HoleBegin - (UINTN)(Section->VirtualAddress);

    //
    // Put the remainder section online.
    //

    if (RemainderSection != NULL) {
        INSERT_AFTER(&(RemainderSection->ProcessListEntry),
                     &(Section->ProcessListEntry));
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
    BOOL ProcessLockHeld
    )

/*++

Routine Description:

    This routine removes an decommissions an image section This routine must be
    called at low level.

Arguments:

    Section - Supplies a pointer to the section to kill.

    ProcessLockHeld - Supplies a boolean indicating whether or not the process
        lock is already held.

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
    PKPROCESS Process;
    BOOL SharedWithChild;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    PageShift = MmPageShift();
    PageCount = Section->Size >> PageShift;
    Process = Section->Process;

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
               (Section->Process == PsGetCurrentProcess()));

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

    if (ProcessLockHeld == FALSE) {
        KeAcquireQueuedLock(Process->QueuedLock);
    }

    LIST_REMOVE(&(Section->ProcessListEntry));
    Section->ProcessListEntry.Next = NULL;
    if (ProcessLockHeld == FALSE) {
        KeReleaseQueuedLock(Process->QueuedLock);
    }

    //
    // Now get in the back of the line to acquire the section lock. Once the
    // lock is acquired, no new pages should get mapped into the image section.
    // Anything trying to map such pages needs to check the
    // IMAGE_SECTION_DESTROYED flags before proceeding.
    //

    KeAcquireQueuedLock(Section->Lock);

    //
    // If the section inherits from another, remove it as a child.
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
    ASSERT(ImageSection->ProcessListEntry.Next == NULL);
    ASSERT(ImageSection->ImageListEntry.Next == NULL);

    MmpFreePageFileSpace(ImageSection);
    if (ImageSection->Lock != NULL) {
        KeDestroyQueuedLock(ImageSection->Lock);
    }

    if (ImageSection->SwapSpace != NULL) {
        MmFreeMemoryReservation(ImageSection->SwapSpace);
    }

    ASSERT((ImageSection->ImageBackingReferenceCount == 0) &&
           (ImageSection->ImageBacking.DeviceHandle == INVALID_HANDLE));

    ObReleaseReference(ImageSection->Process);
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
    // Keep score of whether or not this section was ever writable.
    //

    BecomingReadOnly = FALSE;
    if ((Section->Flags & IMAGE_SECTION_WRITABLE) != 0) {
        Section->Flags |= IMAGE_SECTION_WAS_WRITABLE;
        if ((NewAccess & IMAGE_SECTION_WRITABLE) == 0) {
            BecomingReadOnly = TRUE;
        }
    }

    //
    // Modify the mappings in the region.
    //

    Section->Flags = (Section->Flags & ~IMAGE_SECTION_ACCESS_MASK) |
                     (NewAccess & IMAGE_SECTION_ACCESS_MASK);

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
    ULONG Flags,
    PBOOL PageWasDirty
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

    PageWasDirty - Supplies an optional pointer to a boolean that receives
        whether or not any of the unmapped pages were dirty.

Return Value:

    Status code.

--*/

{

    UINTN BitmapIndex;
    ULONG BitmapMask;
    BOOL Dirty;
    ULONG DirtyPageCount;
    BOOL FreePhysicalPage;
    PULONG InheritPageBitmap;
    PIMAGE_SECTION OwningSection;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    UINTN PageIndex;
    BOOL PageMapped;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;

    ASSERT(((Flags & IMAGE_SECTION_UNMAP_FLAG_PAGE_CACHE_ONLY) == 0) ||
           ((Section->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0));

    if (PageWasDirty != NULL) {
        *PageWasDirty = FALSE;
    }

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
    // Iterate over the region of the image section that needs to be unmapped
    // and unmap each page.
    //

    ASSERT(IS_ALIGNED((UINTN)Section->VirtualAddress, MmPageSize()) != FALSE);

    DirtyPageCount = 0;
    for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
        BitmapIndex = IMAGE_SECTION_BITMAP_INDEX(PageOffset + PageIndex);
        BitmapMask = IMAGE_SECTION_BITMAP_MASK(PageOffset + PageIndex);

        //
        // If the section that owns this page is not inheriting, it's a private
        // page. If only unmapping page cache pages, skip this one, as it's not
        // page cache backed.
        //

        if ((Flags & IMAGE_SECTION_UNMAP_FLAG_PAGE_CACHE_ONLY) != 0) {
            OwningSection = MmpGetOwningSection(Section,
                                                PageOffset + PageIndex);

            InheritPageBitmap = OwningSection->InheritPageBitmap;
            if ((InheritPageBitmap != NULL) &&
                ((InheritPageBitmap[BitmapIndex] & BitmapMask) == 0)) {

                ASSERT((Section->Flags & IMAGE_SECTION_SHARED) == 0);
                ASSERT((Section->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0);

                MmpImageSectionReleaseReference(OwningSection);
                OwningSection = NULL;
                continue;
            }

            MmpImageSectionReleaseReference(OwningSection);
            OwningSection = NULL;
        }

        //
        // When unmapping due to truncation, reset the dirty bit, even if there
        // is no page to unmap. Page-in needs to start fresh for any pages
        // touched by this routine in this case.
        //

        if (((Flags & IMAGE_SECTION_UNMAP_FLAG_TRUNCATE) != 0) &&
            (Section->DirtyPageBitmap != NULL)) {

            Section->DirtyPageBitmap[BitmapIndex] &= ~BitmapMask;
        }

        //
        // If the section is not mapped at this page offset, then neither are
        // any of its inheriting children. Skip it.
        //

        PageMapped = MmpIsImageSectionMapped(Section,
                                             PageOffset + PageIndex,
                                             &PhysicalAddress);

        if (PageMapped == FALSE) {
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
        // If the section is mapped shared or the section is backed by the page
        // cache, has no parent and is inherited from the page cache, then do
        // not free the physical page on unmap.
        //

        FreePhysicalPage = TRUE;
        if (((Section->Flags & IMAGE_SECTION_SHARED) != 0) ||
            (((Section->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0) &&
             (Section->Parent == NULL) &&
             ((Section->InheritPageBitmap[BitmapIndex] & BitmapMask) != 0))) {

            FreePhysicalPage = FALSE;
        }

        //
        // Unmap the page from the section and any inheriting children.
        //

        MmpModifySectionMapping(Section,
                                PageOffset + PageIndex,
                                INVALID_PHYSICAL_ADDRESS,
                                FALSE,
                                &Dirty,
                                TRUE);

        //
        // Only record the dirty status if the section was writable. Some
        // architectures do not have a dirty bit in their page table entries,
        // forcing unmap to assume every page is dirty. Checking whether the
        // individual mapping was writable is not accurate as a dirty page may
        // have been mapped read-only when creating a child section or during a
        // memory protection call.
        //

        if ((Dirty != FALSE) &&
            ((Section->Flags & IMAGE_SECTION_WAS_WRITABLE) != 0)) {

            //
            // If the caller does not care about dirty pages and this was a
            // shared section, mark the page cache entry dirty.
            //

            if ((PageWasDirty == NULL) &&
                ((Section->Flags & IMAGE_SECTION_SHARED) != 0)) {

                ASSERT((Flags & IMAGE_SECTION_UNMAP_FLAG_TRUNCATE) == 0);
                ASSERT(PhysicalAddress != INVALID_PHYSICAL_ADDRESS);

                PageCacheEntry = MmpGetPageCacheEntryForPhysicalAddress(
                                                              PhysicalAddress);

                ASSERT(PageCacheEntry != NULL);

                IoMarkPageCacheEntryDirty(PageCacheEntry, 0, 0, TRUE);
                DirtyPageCount += 1;

            //
            // If the caller does want information about pages being dirty,
            // then report it as dirty.
            //

            } else if (PageWasDirty != NULL) {
                *PageWasDirty = TRUE;
            }
        }

        //
        // If it was determined above that the phyiscal page could be released,
        // free it now.
        //

        if (FreePhysicalPage != FALSE) {

            ASSERT((Flags & IMAGE_SECTION_UNMAP_FLAG_PAGE_CACHE_ONLY) == 0);

            MmFreePhysicalPage(PhysicalAddress);
        }
    }

    Status = STATUS_SUCCESS;

UnmapImageSectionEnd:
    if (DirtyPageCount != 0) {
        IoFlush(INVALID_HANDLE, 0, 0, FLUSH_FLAG_ALL);
    }

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
    PKPROCESS CurrentProcess;
    PKPROCESS KernelProcess;
    BOOL Mapped;
    PHYSICAL_ADDRESS MappedPhysicalAddress;
    PKPROCESS Process;

    CurrentProcess = PsGetCurrentProcess();
    KernelProcess = PsGetKernelProcess();
    Process = Section->Process;

    //
    // Determine if the image section is mapped at the given page offset based
    // on the process to which it belongs.
    //

    Address = Section->VirtualAddress + (PageOffset << MmPageShift());
    MappedPhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    if ((Process == CurrentProcess) || (Process == KernelProcess)) {
        MappedPhysicalAddress = MmpVirtualToPhysical(Address, NULL);

    } else if (Process->PageDirectory != NULL) {
        MappedPhysicalAddress = MmpVirtualToPhysicalInOtherProcess(
                                                        Process->PageDirectory,
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

    ULONG Attributes;
    UINTN BitmapIndex;
    ULONG BitmapMask;
    PVOID CurrentAddress;
    PKPROCESS CurrentProcess;
    UINTN DirtyPageCount;
    PKPROCESS KernelProcess;
    BOOL MarkedDirty;
    BOOL MultipleIpisRequired;
    BOOL OtherProcess;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    UINTN PageCount;
    UINTN PageIndex;
    ULONG PageShift;
    ULONG PageSize;
    BOOL PageWasDirty;
    PHYSICAL_ADDRESS PhysicalAddress;
    PKPROCESS Process;
    PHYSICAL_ADDRESS RunPhysicalAddress;
    UINTN RunSize;
    ULONG UnmapFlags;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeIsQueuedLockHeld(Section->Lock) != FALSE);

    //
    // If this section has been reduced to nothing, then just exit.
    //

    PageShift = MmPageShift();
    PageCount = Section->Size >> PageShift;
    if (PageCount == 0) {
        return;
    }

    CurrentProcess = PsGetCurrentProcess();
    KernelProcess = PsGetKernelProcess();
    DirtyPageCount = 0;
    PageSize = MmPageSize();
    Process = Section->Process;

    //
    // Record the first virtual address of the section.
    //

    CurrentAddress = ALIGN_POINTER_DOWN(Section->VirtualAddress, PageSize);

    //
    // Depending on the image section, there are different, more efficient
    // paths to unmap and free the region. Sections belonging to the current
    // process or the kernel process are most efficient.
    //

    MultipleIpisRequired = TRUE;
    if ((Process == CurrentProcess) || (Process == KernelProcess)) {

        //
        // If the section has no parent and is not backed by the page cache
        // then it should have no children and own all its pages. Freely unmap
        // and release all pages. The work here is then done.
        //

        if ((Section->Parent == NULL) &&
            ((Section->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) == 0)) {

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
            ((Process->ThreadCount <= 1) &&
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
    // but is a rare case. Watch out for test sections, as those run with a
    // dummy process that does not have a page directory.
    //

    } else {
        if (Process->PageDirectory == NULL) {
            goto DestroyImageSectionMappingsEnd;
        }

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
        BitmapIndex = IMAGE_SECTION_BITMAP_INDEX(PageIndex);
        BitmapMask = IMAGE_SECTION_BITMAP_MASK(PageIndex);
        UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                     UNMAP_FLAG_SEND_INVALIDATE_IPI;

        PageWasDirty = FALSE;

        //
        // If the page is shared with the section's parent, the section is
        // mapped shared, or the section is backed by the page cache, has no
        // parent and is inherited from the page cache, then do not free the
        // physical page on unmap.
        //

        if (((Section->Parent != NULL) &&
             ((Section->InheritPageBitmap[BitmapIndex] & BitmapMask) != 0)) ||
            ((Section->Flags & IMAGE_SECTION_SHARED) != 0) ||
            (((Section->Flags & IMAGE_SECTION_PAGE_CACHE_BACKED) != 0) &&
             (Section->Parent == NULL) &&
             ((Section->InheritPageBitmap[BitmapIndex] & BitmapMask) != 0))) {

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
                                                        Process->PageDirectory,
                                                        CurrentAddress);

            if (PhysicalAddress != INVALID_PHYSICAL_ADDRESS) {
                MmpUnmapPageInOtherProcess(Process,
                                           CurrentAddress,
                                           UnmapFlags,
                                           &PageWasDirty);
            }
        }

        //
        // If this is a shared section that was writable and the page was dirty,
        // then the page cache needs to be notified about this dirty page.
        //

        if ((PageWasDirty != FALSE) &&
            ((Section->Flags & IMAGE_SECTION_SHARED) != 0) &&
            ((Section->Flags & IMAGE_SECTION_WAS_WRITABLE) != 0)) {

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
            // The page cache entry must be present. It was mapped and the only
            // way for it to be removed is for the page cache to have unmapped
            // it.
            //

            ASSERT(PageCacheEntry != NULL);

            //
            // Mark it dirty.
            //

            MarkedDirty = IoMarkPageCacheEntryDirty(PageCacheEntry, 0, 0, TRUE);
            if (MarkedDirty != FALSE) {
                DirtyPageCount += 1;
            }
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
        CurrentAddress = ALIGN_POINTER_DOWN(Section->VirtualAddress, PageSize);
        MmpUnmapPages(CurrentAddress, PageCount, 0, NULL);
    }

DestroyImageSectionMappingsEnd:
    if (DirtyPageCount != 0) {
        IoFlush(INVALID_HANDLE, 0, 0, FLUSH_FLAG_ALL);
    }

    return;
}

