/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    paging.c

Abstract:

    This module implements interactions with swap files or other memory
    backing stores.

Author:

    Evan Green 30-Sep-2012

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

#define MM_PAGE_FILE_ALLOCATION_TAG 0x66506D4D // 'fPmM'
#define MM_PAGING_ENTRY_BLOCK_ALLOCATION_TAG 0x6C426550 // 'lBeP'

//
// Define the name of the page file on the system.
//

#define PAGE_FILE_NAME "pagefile.sys"

//
// Define the maximum chunk size for paging out.
//

#define PAGE_OUT_CHUNK_SIZE (1024 * 512)

//
// Define the maximum number of clean pages to encounter in a row before
// breaking up a write chunk.
//

#define PAGE_OUT_MAX_CLEAN_STREAK 4

//
// Define the alignment and initial capacity for the paging entry block
// allocator.
//

#define MM_PAGING_ENTRY_BLOCK_ALLOCATOR_ALIGNMENT 1
#define MM_PAGING_ENTRY_BLOCK_ALLOCATOR_EXPANSION_COUNT 50

//
// Define the bitmap of page in context flags.
//

#define PAGE_IN_CONTEXT_FLAG_ALLOCATE_PAGE       0x00000001
#define PAGE_IN_CONTEXT_FLAG_ALLOCATE_IRP        0x00000002
#define PAGE_IN_CONTEXT_FLAG_ALLOCATE_SWAP_SPACE 0x00000004
#define PAGE_IN_CONTEXT_FLAG_ALLOCATE_MASK       0x00000007

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the resources necessary to page in from disk or a
    page file.

Members:

    Irp - Stores a pointer to an IRP to use for paging in from a page file.
        Ownership of this IRP may be transfered to the root or owning image
        section.

    IrpDevice - Stores a pointer to the paging device to use for the IRP.

    PhysicalAddress - Stores an allocated physical address to page into.

    SwapSpace - Stores a pointer to a memory reservation to use for temporarily
        mapping the physical address when paging in from a page file. Ownership
        of this memory reservation may be transfered to the root image section.

    PagingEntry - Stores a pointer to a paging entry to use for the new
        physical page.

    Flags - Stores a bitmask of page in context flags. See
        PAGE_IN_CONTEXT_FLAG_* for definitions.

--*/

typedef struct _PAGE_IN_CONTEXT {
    PIRP Irp;
    PDEVICE IrpDevice;
    PHYSICAL_ADDRESS PhysicalAddress;
    PMEMORY_RESERVATION SwapSpace;
    PPAGING_ENTRY PagingEntry;
    ULONG Flags;
} PAGE_IN_CONTEXT, *PPAGE_IN_CONTEXT;

/*++

Structure Description:

    This structure defines the context used for a page file I/O operation.

Members:

    Offset - Stores the offset from the beginning of the file or device where
        the I/O should be done.

    IoBuffer - Stores a pointer to an I/O buffer that either contains the data
        to write or will contain the read data.

    Irp - Stores the optional IRP to use for reads. Each page file has its own
        write IRP.

    SizeInBytes - Stores the number of bytes to read or write.

    BytesCompleted - Stores the number of bytes of I/O actually performed.

    Flags - Stores the flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Stores the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    Write - Stores a boolean value indicating if the I/O operation is a write
        (TRUE) or a read (FALSE).

--*/

typedef struct _PAGE_FILE_IO_CONTEXT {
    IO_OFFSET Offset;
    PIO_BUFFER IoBuffer;
    PIRP Irp;
    UINTN SizeInBytes;
    UINTN BytesCompleted;
    ULONG Flags;
    ULONG TimeoutInMilliseconds;
    BOOL Write;
} PAGE_FILE_IO_CONTEXT, *PPAGE_FILE_IO_CONTEXT;

/*++

Structure Description:

    This structure embodies a memory page backing store.

Members:

    ListEntry - Stores pointers to the next and previous paging store entries.

    Handle - Stores the open handle to the backing store.

    Lock - Stores a pointer to the lock that synchronizes access to this
        structure.

    Bitmap - Stores a pointer to the bitmap indicating which pages are free
        and which are in use.

    PagingOutIrp - Stores a pointer to an IRP used for paging out to this page
        file.

    PageCount - Stores the number of pages this backing store can hold.

    FreePages - Stores the number of free pages in this backing store.

    LastAllocatedPage - Stores the index into the backing store of the most
        recently allocated backing store.

    FailedAllocations - Stores the number of times this page file has failed
        to meet a request for page file space.

--*/

typedef struct _PAGE_FILE {
    LIST_ENTRY ListEntry;
    PIO_HANDLE Handle;
    PQUEUED_LOCK Lock;
    PULONG Bitmap;
    PIRP PagingOutIrp;
    UINTN PageCount;
    UINTN FreePages;
    UINTN LastAllocatedPage;
    UINTN FailedAllocations;
} PAGE_FILE, *PPAGE_FILE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
MmpCreatePageFile (
    PIO_HANDLE Handle,
    ULONGLONG Size
    );

VOID
MmpDestroyPageFile (
    PPAGE_FILE PageFile
    );

KSTATUS
MmpAllocateFromPageFile (
    PPAGE_FILE PageFile,
    ULONG PageCount,
    PULONG Allocation
    );

VOID
MmpFreeFromPageFile (
    PPAGE_FILE PageFile,
    ULONG Allocation,
    ULONG PageCount
    );

VOID
MmpPagingThread (
    PVOID Parameter
    );

KSTATUS
MmpPageInAnonymousSection (
    PIMAGE_SECTION ImageSection,
    UINTN PageOffset,
    PIO_BUFFER LockedIoBuffer
    );

KSTATUS
MmpPageInSharedSection (
    PIMAGE_SECTION ImageSection,
    UINTN PageOffset,
    PIO_BUFFER LockedIoBuffer
    );

KSTATUS
MmpPageInBackedSection (
    PIMAGE_SECTION ImageSection,
    UINTN PageOffset,
    PIO_BUFFER LockedIoBuffer
    );

KSTATUS
MmpCheckExistingMapping (
    PIMAGE_SECTION Section,
    ULONG PageOffset,
    BOOL LockPage,
    PIO_BUFFER LockedIoBuffer,
    PPHYSICAL_ADDRESS ExistingPhysicalAddress
    );

KSTATUS
MmpPageInDefaultSection (
    PIMAGE_SECTION ImageSection,
    UINTN PageOffset,
    PIO_BUFFER LockedIoBuffer
    );

KSTATUS
MmpPrepareForPageFileRead (
    PIMAGE_SECTION RootSection,
    PIMAGE_SECTION OwningSection,
    PPAGE_IN_CONTEXT Context
    );

KSTATUS
MmpReadPageFile (
    PIMAGE_SECTION RootSection,
    PIMAGE_SECTION OwningSection,
    ULONG PageOffset,
    PPAGE_IN_CONTEXT Context
    );

KSTATUS
MmpPageFilePerformIo (
    PIMAGE_BACKING ImageBacking,
    PPAGE_FILE_IO_CONTEXT IoContext
    );

KSTATUS
MmpReadBackingImage (
    PIMAGE_SECTION Section,
    UINTN PageOffset,
    PIO_BUFFER IoBuffer
    );

VOID
MmpMapPageInSection (
    PIMAGE_SECTION OwningSection,
    UINTN PageOffset,
    PHYSICAL_ADDRESS PhysicalAddress,
    PPAGING_ENTRY PagingEntry,
    BOOL LockPage
    );

KSTATUS
MmpAllocatePageInStructures (
    PIMAGE_SECTION Section,
    PPAGE_IN_CONTEXT Context
    );

VOID
MmpDestroyPageInContext (
    PPAGE_IN_CONTEXT Context
    );

BOOL
MmpCanWriteToSection (
    PIMAGE_SECTION OwningSection,
    PIMAGE_SECTION Section,
    UINTN PageOffset
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Boolean indicating whether or not paging to disk is enabled.
//

BOOL MmPagingEnabled = FALSE;

//
// Boolean indicating whether paging is forcefully disabled on this system.
//

BOOL MmPagingForceDisable = FALSE;

//
// If this boolean is set to TRUE, the system will attempt to open and use
// a page file on any volume that is connected to the system. By default, only
// the system volume is eligible to have a page file.
//

BOOL MmPagingAllVolumes = FALSE;

//
// Store a list of the available paging devices.
//

LIST_ENTRY MmPageFileListHead;
PQUEUED_LOCK MmPageFileListLock = NULL;

//
// Store the paging thread and paging related events.
//

BOOL MmPagingThreadCreated = FALSE;
PKTHREAD MmPagingThread;
PKEVENT MmPagingEvent;
PKEVENT MmPagingFreePagesEvent;
volatile UINTN MmPagingFreeTarget;

//
// Store the block allocator used for allocating paging entries.
//

PBLOCK_ALLOCATOR MmPagingEntryBlockAllocator;

//
// ------------------------------------------------------------------ Functions
//

BOOL
MmRequestPagingOut (
    UINTN FreePageTarget
    )

/*++

Routine Description:

    This routine schedules the backgroung paging thread to run, hopefully
    freeing up some memory. This must be called at low level. This routine is
    asynchronous, it will signal the paging thread and return immediately. The
    request may be ignored or coalesced with other paging out requests.

Arguments:

    FreePageTarget - Supplies the target number of free physical pages the
        caller would like to see in the system.

Return Value:

    Returns TRUE if a request was submitted or FALSE otherwise (e.g. paging is
    not enabled).

--*/

{

    UINTN PagingFreeTarget;
    UINTN PreviousPagingFreeTarget;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // There is nothing to be done if paging is not enabled.
    //

    if (MmPagingEnabled == FALSE) {
        return FALSE;
    }

    //
    // Set the supplied page count if it is larger than the current value.
    //

    PreviousPagingFreeTarget = MmPagingFreeTarget;
    while (FreePageTarget > PreviousPagingFreeTarget) {
        PagingFreeTarget = RtlAtomicCompareExchange(&MmPagingFreeTarget,
                                                    FreePageTarget,
                                                    PreviousPagingFreeTarget);

        if (PagingFreeTarget == PreviousPagingFreeTarget) {
            break;
        }

        PreviousPagingFreeTarget = PagingFreeTarget;
    }

    //
    // Unsignal the free pages event. This will allow the caller to wait until
    // the page worker has had a chance to free at least one page. Note that
    // the page worker could be in the midst of releasing some other minimal
    // set of pages. So the caller might be notified prematurely, but it can
    // just try again if it doesn't find enough pages.
    //

    KeSignalEvent(MmPagingFreePagesEvent, SignalOptionUnsignal);

    //
    // Now schedule the paging thread by signaling the paging event. The paging
    // thread is either in the middle of working, in which case this will
    // schedule it to run again. Or it is waiting on the event or just woke up
    // from the event and is yet to unsignal it. Either way, this callers
    // requested page count will be acknowledged.
    //

    KeSignalEvent(MmPagingEvent, SignalOptionSignalAll);
    return TRUE;
}

VOID
MmVolumeArrival (
    PCSTR VolumeName,
    ULONG VolumeNameLength,
    BOOL SystemVolume
    )

/*++

Routine Description:

    This routine implements the memory manager's response to a new volume in
    the system.

Arguments:

    VolumeName - Supplies the full path to the new volume.

    VolumeNameLength - Supplies the length of the volume name buffer, including
        the null terminator, in bytes.

    SystemVolume - Supplies a boolean indicating if this is the system volume
        or not.

Return Value:

    None.

--*/

{

    PSTR AppendedPath;
    ULONG AppendedPathSize;
    PIO_HANDLE FileHandle;
    ULONGLONG FileSize;
    KSTATUS Status;

    AppendedPath = NULL;
    FileHandle = INVALID_HANDLE;
    Status = STATUS_UNSUCCESSFUL;

    //
    // If paging is forcefully disabled, don't create any paging devices.
    //

    if (MmPagingForceDisable != FALSE) {
        Status = STATUS_SUCCESS;
        goto VolumeArrivalEnd;
    }

    //
    // For now, don't do paging on anything but the system volume.
    //

    if ((MmPagingAllVolumes == FALSE) && (SystemVolume == FALSE)) {
        Status = STATUS_SUCCESS;
        goto VolumeArrivalEnd;
    }

    //
    // Create an appended path with the volume and page file name.
    //

    Status = IoPathAppend(VolumeName,
                          VolumeNameLength,
                          PAGE_FILE_NAME,
                          sizeof(PAGE_FILE_NAME),
                          MM_ALLOCATION_TAG,
                          &AppendedPath,
                          &AppendedPathSize);

    if (!KSUCCESS(Status)) {
        goto VolumeArrivalEnd;
    }

    //
    // Attempt to open the page file on this device.
    //

    Status = IoOpenPageFile(AppendedPath,
                            AppendedPathSize,
                            IO_ACCESS_READ | IO_ACCESS_WRITE,
                            0,
                            &FileHandle,
                            &FileSize);

    if (!KSUCCESS(Status)) {
        FileHandle = INVALID_HANDLE;
        goto VolumeArrivalEnd;
    }

    //
    // If a page file was successfully opened, add it to the list of available
    // page files. Upon success, the file handle belongs to the newly created
    // page file.
    //

    Status = MmpCreatePageFile(FileHandle, FileSize);
    if (!KSUCCESS(Status)) {
        goto VolumeArrivalEnd;
    }

    FileHandle = INVALID_HANDLE;

VolumeArrivalEnd:
    if (AppendedPath != NULL) {
        MmFreePagedPool(AppendedPath);
    }

    if (FileHandle != INVALID_HANDLE) {
        IoClose(FileHandle);
    }

    return;
}

KSTATUS
MmVolumeRemoval (
    PVOID Device
    )

/*++

Routine Description:

    This routine implements the memory manager's response to a volume being
    removed from the system.

Arguments:

    Device - Supplies a pointer to the device (volume) being removed.

Return Value:

    Status code.

--*/

{

    PDEVICE CurrentDevice;
    PLIST_ENTRY CurrentEntry;
    PPAGE_FILE CurrentPageFile;
    PPAGE_FILE PageFile;
    BOOL PageFileListLocked;
    KSTATUS Status;

    PageFileListLocked = FALSE;
    Status = STATUS_UNSUCCESSFUL;

    //
    // If paging is forcefully disabled or simply not enabled, then a page file
    // was never opened on this volume.
    //

    if ((MmPagingForceDisable != FALSE) || (MmPagingEnabled == FALSE)) {
        Status = STATUS_SUCCESS;
        goto VolumeRemovalEnd;
    }

    //
    // Search for a page file that has the same device as the page file on the
    // supplied volume.
    //

    CurrentDevice = NULL;
    CurrentPageFile = NULL;
    PageFile = NULL;
    KeAcquireQueuedLock(MmPageFileListLock);
    PageFileListLocked = TRUE;
    CurrentEntry = MmPageFileListHead.Next;
    while (CurrentEntry != &MmPageFileListHead) {
        CurrentPageFile = LIST_VALUE(CurrentEntry, PAGE_FILE, ListEntry);
        Status = IoGetDevice(CurrentPageFile->Handle, &CurrentDevice);
        if (!KSUCCESS(Status)) {
            goto VolumeRemovalEnd;
        }

        if (CurrentDevice == Device) {
            PageFile = CurrentPageFile;
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // If a page file was found for the given volume and not all of its pages
    // are free, then crash the system. The user should not be removing an
    // active paging device. Otherwise remove it from the list of page files.
    //

    if (PageFile != NULL) {
        KeAcquireQueuedLock(PageFile->Lock);
        if (PageFile->PageCount != PageFile->FreePages) {
            KeCrashSystem(CRASH_PAGING_DEVICE_REMOVAL,
                          (UINTN)PageFile,
                          (UINTN)Device,
                          0,
                          0);
        }

        KeReleaseQueuedLock(PageFile->Lock);
        LIST_REMOVE(&(PageFile->ListEntry));
    }

    //
    // If the list is now empty, then paging is effectively disabled. Don't
    // bother to destroy the paging thread. It may still be in use.
    //

    if (LIST_EMPTY(&MmPageFileListHead) != FALSE) {
        MmPagingEnabled = FALSE;
    }

    KeReleaseQueuedLock(MmPageFileListLock);
    PageFileListLocked = FALSE;

    //
    // Destroy the page file now that the locks are released and it is no
    // longer in the list of available page files.
    //

    if (PageFile != NULL) {
        MmpDestroyPageFile(PageFile);
    }

    Status = STATUS_SUCCESS;

VolumeRemovalEnd:
    if (PageFileListLocked != FALSE) {
        KeReleaseQueuedLock(MmPageFileListLock);
    }

    return Status;
}

KSTATUS
MmpInitializePaging (
    VOID
    )

/*++

Routine Description:

    This routine initializes the paging infrastructure, preparing for the
    arrival of a page file.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PBLOCK_ALLOCATOR BlockAllocator;
    KSTATUS Status;

    //
    // Initialize the structure necessary to maintain a list of page files.
    //

    INITIALIZE_LIST_HEAD(&MmPageFileListHead);
    MmPageFileListLock = KeCreateQueuedLock();
    if (MmPageFileListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializePagingEnd;
    }

    //
    // Initialize the structures necessary to run a background thread that
    // handles paging and releasing memory pressure.
    //

    MmPagingFreePagesEvent = KeCreateEvent(NULL);
    if (MmPagingFreePagesEvent == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializePagingEnd;
    }

    MmPagingEvent = KeCreateEvent(NULL);
    if (MmPagingEvent == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializePagingEnd;
    }

    //
    // Initialize the block allocator from which paging entries will be
    // allocated.
    //

    BlockAllocator = MmCreateBlockAllocator(
                           sizeof(PAGING_ENTRY),
                           MM_PAGING_ENTRY_BLOCK_ALLOCATOR_ALIGNMENT,
                           MM_PAGING_ENTRY_BLOCK_ALLOCATOR_EXPANSION_COUNT,
                           (BLOCK_ALLOCATOR_FLAG_NON_PAGED |
                            BLOCK_ALLOCATOR_FLAG_TRIM),
                           MM_PAGING_ENTRY_BLOCK_ALLOCATION_TAG);

    if (BlockAllocator == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializePagingEnd;
    }

    MmPagingEntryBlockAllocator = BlockAllocator;
    Status = STATUS_SUCCESS;

InitializePagingEnd:
    if (!KSUCCESS(Status)) {
        if (MmPageFileListLock != NULL) {
            KeDestroyQueuedLock(MmPageFileListLock);
        }

        if (MmPagingFreePagesEvent != NULL) {
            KeDestroyEvent(MmPagingFreePagesEvent);
        }

        if (MmPagingEvent != NULL) {
            KeDestroyEvent(MmPagingEvent);
        }

        if (MmPagingEntryBlockAllocator != NULL) {
            MmDestroyBlockAllocator(MmPagingEntryBlockAllocator);
        }
    }

    return Status;
}

KSTATUS
MmAllocatePageFileSpace (
    PIMAGE_BACKING ImageBacking,
    UINTN Size
    )

/*++

Routine Description:

    This routine allocates space from a page file.

Arguments:

    ImageBacking - Supplies a pointer to an image backing structure that
        recevies the allocated page file space.

    Size - Supplies the size of the page file space to allocate, in bytes.

Return Value:

    STATUS_SUCCESS on success. In this case the image backing structure
    parameterwill be filled out.

    STATUS_INSUFFICIENT_RESOURCES if the request could not be satisified.

--*/

{

    ULONG Allocation;
    PLIST_ENTRY CurrentEntry;
    PPAGE_FILE CurrentPageFile;
    UINTN PageCount;
    ULONG PageShift;
    ULONG PageSize;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(ImageBacking->DeviceHandle == INVALID_HANDLE);

    //
    // If paging is not enabled, then there's no way page file space could be
    // allocated.
    //

    if (MmPagingEnabled == FALSE) {
        return STATUS_NO_SUCH_FILE;
    }

    //
    // Determine the number of pages in the section.
    //

    PageShift = MmPageShift();
    PageSize = MmPageSize();

    ASSERT(IS_ALIGNED(Size, PageSize));

    PageCount = Size >> PageShift;

    //
    // Look through all page files for the given space.
    //

    KeAcquireQueuedLock(MmPageFileListLock);
    if (MmPagingEnabled == FALSE) {
        Status = STATUS_NO_SUCH_FILE;

    } else {
        CurrentEntry = MmPageFileListHead.Next;
        Status = STATUS_INSUFFICIENT_RESOURCES;
        while (CurrentEntry != &MmPageFileListHead) {
            CurrentPageFile = LIST_VALUE(CurrentEntry, PAGE_FILE, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            if (CurrentPageFile->FreePages == 0) {
                continue;
            }

            //
            // Attempt to allocate the space from this page file.
            //

            Status = MmpAllocateFromPageFile(CurrentPageFile,
                                             PageCount,
                                             &Allocation);

            //
            // If it was successful, leave the loop.
            //

            if (KSUCCESS(Status)) {
                ImageBacking->DeviceHandle = CurrentPageFile;
                ImageBacking->Offset = Allocation << PageShift;
                break;
            }
        }
    }

    KeReleaseQueuedLock(MmPageFileListLock);
    if (!KSUCCESS(Status)) {
        goto AllocatePageFileSpaceEnd;
    }

AllocatePageFileSpaceEnd:
    if (!KSUCCESS(Status)) {
        if (ImageBacking->DeviceHandle != INVALID_HANDLE) {
            MmpFreeFromPageFile(ImageBacking->DeviceHandle,
                                ImageBacking->Offset >> PageShift,
                                PageCount);
        }
    }

    return Status;
}

VOID
MmFreePageFileSpace (
    PIMAGE_BACKING ImageBacking,
    UINTN Size
    )

/*++

Routine Description:

    This routine frees space from a page file.

Arguments:

    ImageBacking - Supplies a pointer to the page file image backing to release.

    Size - Supplies the size of the image backing.

Return Value:

    None.

--*/

{

    UINTN PageCount;
    ULONG PageShift;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (MmPagingEnabled == FALSE) {
        return;
    }

    if (ImageBacking->DeviceHandle == INVALID_HANDLE) {
        return;
    }

    PageShift = MmPageShift();
    PageCount = Size >> PageShift;
    MmpFreeFromPageFile(ImageBacking->DeviceHandle,
                        ImageBacking->Offset >> PageShift,
                        PageCount);

    ImageBacking->DeviceHandle = INVALID_HANDLE;
    return;
}

VOID
MmFreePartialPageFileSpace (
    PIMAGE_BACKING ImageBacking,
    UINTN PageOffset,
    UINTN PageCount
    )

/*++

Routine Description:

    This routine frees a portion of the original space allocated in the page
    file.

Arguments:

    ImageBacking - Supplies a pointer to the image backing taking up page file
        space.

    PageOffset - Supplies the offset in pages to the beginning of the region
        that should be freed.

    PageCount - Supplies the number of pages to free.

Return Value:

    None.

--*/

{

    ULONG PageFileOffset;
    ULONG PageShift;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (MmPagingEnabled == FALSE) {
        return;
    }

    if (ImageBacking->DeviceHandle == INVALID_HANDLE) {
        return;
    }

    PageShift = MmPageShift();
    PageFileOffset = (ImageBacking->Offset >> PageShift) + PageOffset;
    MmpFreeFromPageFile(ImageBacking->DeviceHandle, PageFileOffset, PageCount);
    return;
}

KSTATUS
MmPageFilePerformIo (
    PIMAGE_BACKING ImageBacking,
    PIO_BUFFER IoBuffer,
    UINTN Offset,
    UINTN SizeInBytes,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    BOOL Write,
    PUINTN BytesCompleted
    )

/*++

Routine Description:

    This routine performs I/O on the page file region described by the given
    image backing.

Arguments:

    ImageBacking - Supplies a pointer to the image backing that holds a device
        handle and offset for the page file region.

    IoBuffer - Supplies a pointer to an I/O buffer to use for the read or write.

    Offset - Supplies the offset from the beginning of the file or device where
        the I/O should be done.

    SizeInBytes - Supplies the number of bytes to read or write.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    Write - Supplies a boolean indicating whether or not the I/O is a read
        (FALSE) or a write (TRUE).

    BytesCompleted - Supplies the a pointer where the number of bytes actually
        read or written will be returned.

Return Value:

    Status code.

--*/

{

    PAGE_FILE_IO_CONTEXT IoContext;
    KSTATUS Status;

    IoContext.Offset = Offset;
    IoContext.IoBuffer = IoBuffer;
    IoContext.Irp = NULL;
    IoContext.SizeInBytes = SizeInBytes;
    IoContext.BytesCompleted = 0;
    IoContext.Flags = Flags;
    IoContext.TimeoutInMilliseconds = TimeoutInMilliseconds;
    IoContext.Write = Write;
    Status = MmpPageFilePerformIo(ImageBacking, &IoContext);
    *BytesCompleted = IoContext.BytesCompleted;
    return Status;
}

KSTATUS
MmpPageIn (
    PIMAGE_SECTION ImageSection,
    UINTN PageOffset,
    PIO_BUFFER LockedIoBuffer
    )

/*++

Routine Description:

    This routine pages a physical page in from disk or allocates a new free
    physical page. This routine must be called at low level.

Arguments:

    ImageSection - Supplies a pointer to the image section within the process
        to page in.

    PageOffset - Supplies the offset, in pages, from the beginning of the
        section.

    LockedIoBuffer - Supplies an optional pointer to an uninitialized I/O
        buffer that will be initialized with the the paged in page, effectively
        locking the page until the I/O buffer is released.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TOO_LATE if the given image section is destroyed.

    STATUS_TRY_AGAIN if the given image section is no longer large enough to
    cover the given page offset.

    Other status codes on other errors.

--*/

{

    KSTATUS Status;

    //
    // Handle image sections that do not belong to a backing image.
    //

    if ((ImageSection->Flags & IMAGE_SECTION_NO_IMAGE_BACKING) != 0) {
        Status = MmpPageInAnonymousSection(ImageSection,
                                           PageOffset,
                                           LockedIoBuffer);

    //
    // Handle shared image sections.
    //

    } else if ((ImageSection->Flags & IMAGE_SECTION_SHARED) != 0) {
        Status = MmpPageInSharedSection(ImageSection,
                                        PageOffset,
                                        LockedIoBuffer);

    //
    // Handle image sections backed by something.
    //

    } else if ((ImageSection->Flags & IMAGE_SECTION_BACKED) != 0) {
        Status = MmpPageInBackedSection(ImageSection,
                                        PageOffset,
                                        LockedIoBuffer);

    //
    // Otherwise handle default image sections. These have a backing image, but
    // are not aligned with the page cache.
    //

    } else {
        Status = MmpPageInDefaultSection(ImageSection,
                                         PageOffset,
                                         LockedIoBuffer);
    }

    return Status;
}

KSTATUS
MmpPageInAndLock (
    PIMAGE_SECTION Section,
    UINTN PageOffset
    )

/*++

Routine Description:

    This routine attempts to page in the given page and lock the image section
    as to prevent the page from being paged back out, unmapped, or destroyed by
    the owning section.

Arguments:

    Section - Supplies a pointer to the image section that contains the
        address.

    PageOffset - Supplies the offset, in pages, from the beginning of the
        section.

Return Value:

    Status code.

--*/

{

    ULONG PageShift;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;
    PVOID VirtualAddress;

    //
    // Loop trying to pin down the page while the section is locked.
    //

    PageShift = MmPageShift();
    VirtualAddress = Section->VirtualAddress + (PageOffset << PageShift);
    while (TRUE) {
        KeAcquireQueuedLock(Section->Lock);

        //
        // If the section doesn't cover the page, tell the caller to find
        // the real image section.
        //

        if ((Section->VirtualAddress + Section->Size) <= VirtualAddress) {
            Status = STATUS_TRY_AGAIN;
            KeReleaseQueuedLock(Section->Lock);
            goto PageInAndLockEnd;
        }

        //
        // If the page is already mapped, great. Exit with the lock held.
        //

        PhysicalAddress = MmpVirtualToPhysical(VirtualAddress, NULL);
        if (PhysicalAddress != INVALID_PHYSICAL_ADDRESS) {
            Status = STATUS_SUCCESS;
            goto PageInAndLockEnd;
        }

        KeReleaseQueuedLock(Section->Lock);

        //
        // The page is not present. Try to page it in. If this succeeds, then
        // loop back and try to trap the mapping.
        //

        Status = MmpPageIn(Section, PageOffset, NULL);
        if (!KSUCCESS(Status)) {
            goto PageInAndLockEnd;
        }
    }

PageInAndLockEnd:
    return Status;
}

KSTATUS
MmpPageOut (
    PPAGING_ENTRY PagingEntry,
    PIMAGE_SECTION Section,
    UINTN PageOffset,
    PHYSICAL_ADDRESS PhysicalAddress,
    PIO_BUFFER IoBuffer,
    PMEMORY_RESERVATION SwapRegion,
    PUINTN PagesPaged
    )

/*++

Routine Description:

    This routine pages a physical page out to disk. It assumes the page has
    been flagged for paging out. This routine will attempt to batch writes
    and page out several physical pages at this offset.

Arguments:

    PagingEntry - Supplies a pointer to the physical page's paging entry.

    Section - Supplies a pointer to the image section, snapped from the paging
        entry while the physical page lock was still held.

    PageOffset - Supplies the offset into the section in pages where this page
        resides, snapped form the paging entry while the physical page lock was
        still held.

    PhysicalAddress - Supplies the address of the physical page to swap out.

    IoBuffer - Supplies a pointer to an allocated but uninitialized I/O buffer
        to use during page out I/O.

    SwapRegion - Supplies a pointer to a region of VA space to use during
        paging.

    PagesPaged - Supplies a pointer where the count of pages removed will
        be returned.

Return Value:

    Status code.

--*/

{

    UINTN BitmapIndex;
    ULONG BitmapMask;
    UINTN BytesCompleted;
    UINTN CleanStreak;
    BOOL Dirty;
    UINTN IoBufferSize;
    PPAGING_ENTRY OriginalPagingEntry;
    PIMAGE_SECTION OwningSection;
    UINTN PageCount;
    PPAGE_FILE PageFile;
    ULONG PageShift;
    ULONG PageSize;
    IO_OFFSET SectionOffset;
    UINTN SectionPageCount;
    KSTATUS Status;
    UINTN SwapOffset;
    ULONG UnmapFlags;
    PVOID VirtualAddress;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((PagingEntry->U.Flags & PAGING_ENTRY_FLAG_PAGING_OUT) != 0);
    ASSERT(IoBuffer->FragmentCount == 0);

    OriginalPagingEntry = PagingEntry;
    PageShift = MmPageShift();
    PageSize = MmPageSize();
    *PagesPaged = 0;
    SectionOffset = -1LL;

    ASSERT((Section->VirtualAddress < KERNEL_VA_START) ||
           (Section->AddressSpace == MmKernelAddressSpace));

    //
    // This section better not be non-paged or shared and thus should have a
    // dirty page bitmap.
    //

    ASSERT((Section->Flags & IMAGE_SECTION_NON_PAGED) == 0);
    ASSERT((Section->Flags & IMAGE_SECTION_SHARED) == 0);
    ASSERT(Section->DirtyPageBitmap != NULL);

    //
    // Acquire the section's lock. Add a reference in case the only thing
    // keeping the section alive is the paging entry, which may get destroyed
    // here.
    //

    KeAcquireQueuedLock(Section->Lock);
    MmpImageSectionAddReference(Section);

    ASSERT(PagingEntry->Section != NULL);

    //
    // If the section has been destroyed, there is nothing to do. The page is
    // free to release.
    //

    if ((Section->Flags & IMAGE_SECTION_DESTROYED) != 0) {
        PagingEntry->U.Flags &= ~PAGING_ENTRY_FLAG_PAGING_OUT;
        PagingEntry = NULL;
        MmFreePhysicalPage(PhysicalAddress);
        Status = STATUS_SUCCESS;
        goto PageOutEnd;
    }

    //
    // If the page has been locked since it was selected for page out, skip it.
    // A pageable page's lock count can only increment if the section lock is
    // held.
    //

    if (PagingEntry->U.LockCount != 0) {
        Status = STATUS_RESOURCE_IN_USE;
        goto PageOutEnd;
    }

    //
    // If this section has a chance of being dirty, make sure the page file
    // space is allocated before it gets unmapped. There is a chance that the
    // page is not dirty and that this is unnecessary, but that's OK. Keep the
    // page file backing around.
    //

    PageFile = INVALID_HANDLE;
    if ((Section->Flags & IMAGE_SECTION_WAS_WRITABLE) != 0) {
        if (Section->PageFileBacking.DeviceHandle == INVALID_HANDLE) {

            ASSERT(IS_POINTER_ALIGNED(Section->VirtualAddress, PageSize));

            Status = MmAllocatePageFileSpace(&(Section->PageFileBacking),
                                             Section->Size);

            if (!KSUCCESS(Status)) {
                goto PageOutEnd;
            }
        }

        PageFile = (PPAGE_FILE)(Section->PageFileBacking.DeviceHandle);

        ASSERT(PageFile != INVALID_HANDLE);

        SectionOffset = (PageOffset << PageShift);
    }

    //
    // Loop trying to gather pages of this section together for a bigger write.
    //

    SectionPageCount = Section->Size >> PageShift;
    SwapOffset = 0;
    CleanStreak = 0;
    while ((SwapOffset < SwapRegion->Size) && (PageOffset < SectionPageCount)) {
        BitmapIndex = IMAGE_SECTION_BITMAP_INDEX(PageOffset);
        BitmapMask = IMAGE_SECTION_BITMAP_MASK(PageOffset);

        //
        // Get the section that actually owns the page. If the owning section
        // is not this section, then stop, as that page would need to be paged
        // out to a different page file location.
        //

        OwningSection = MmpGetOwningSection(Section, PageOffset);
        MmpImageSectionReleaseReference(OwningSection);
        if (OwningSection != Section) {
            break;
        }

        //
        // If the section is backed and this page is using the page cache, then
        // it can't be freed, as the backing image owns it.
        //

        if (((Section->Flags & IMAGE_SECTION_BACKED) != 0) &&
            ((Section->DirtyPageBitmap[BitmapIndex] & BitmapMask) == 0)) {

            ASSERT((SwapOffset != 0) || (PagingEntry == NULL));

            break;
        }

        //
        // Get the physical address (except for the first one, which was
        // handed down in a parameter and is already marked for paging out).
        // The paging out flag in the paging entry does not need to be set
        // in these subsequent pages because they can only be freed or locked
        // while the section lock is held.
        //

        if ((SwapOffset != 0) || (PagingEntry == NULL)) {
            VirtualAddress = Section->VirtualAddress +
                             (PageOffset << PageShift);

            if (Section->AddressSpace == MmKernelAddressSpace) {
                PhysicalAddress = MmpVirtualToPhysical(VirtualAddress, NULL);

            } else {
                PhysicalAddress = MmpVirtualToPhysicalInOtherProcess(
                                                        Section->AddressSpace,
                                                        VirtualAddress);
            }

            //
            // Stop if there is no page here.
            //

            if (PhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
                break;
            }
        }

        //
        // Unmap the pages and flush the TLB entry on all processors, officially
        // taking this page offline. Do not use the writable flag as pages from
        // copied sections may be mapped read-only even though they are dirty.
        //

        MmpModifySectionMapping(Section,
                                PageOffset,
                                INVALID_PHYSICAL_ADDRESS,
                                FALSE,
                                &Dirty,
                                TRUE);

        //
        // If the page is dirty, it will need to be written out to disk. Ignore
        // the dirty status if the section is not writable. Some architectures
        // do not have a dirty bit in their page table entries, forcing unmap
        // to assume every page is dirty. Also check the dirty page bitmap, as
        // a child might acquire a dirty page from a parent during isolation
        // without the page table entry ever being set dirty.
        //

        if (((Section->Flags & IMAGE_SECTION_WAS_WRITABLE) != 0) &&
            ((Dirty != FALSE) ||
             ((Section->DirtyPageBitmap[BitmapIndex] & BitmapMask) != 0))) {

            CleanStreak = 0;

            //
            // Mark it as dirty so when it is paged back in it will come from
            // the swap file. This dirty bitmap update is protected by the
            // section lock.
            //

            Section->DirtyPageBitmap[BitmapIndex] |= BitmapMask;

        //
        // This page is clean.
        //

        } else {

            //
            // If this is the first page, then just free it, there's no need
            // to page anything out for a streak of clean pages.
            //

            if (SwapOffset == 0) {
                if (PagingEntry != NULL) {
                    PagingEntry->U.Flags &= ~PAGING_ENTRY_FLAG_PAGING_OUT;
                    PagingEntry = NULL;
                }

                MmFreePhysicalPage(PhysicalAddress);
                PageOffset += 1;
                SectionOffset += PageSize;
                continue;
            }

            //
            // This is a clean page coming after at least one dirty page. In
            // an effort to do larger writes, tolerate a certain streak of
            // clean pages in order to get multiple dirty ones.
            //

            CleanStreak += 1;
            if (CleanStreak > PAGE_OUT_MAX_CLEAN_STREAK) {

                //
                // Free this page since it's already been unmapped and marked
                // as paging out.
                //

                *PagesPaged += 1;
                MmFreePhysicalPage(PhysicalAddress);
                break;
            }
        }

        //
        // Map the page to the temporary region.
        //

        VirtualAddress = SwapRegion->VirtualBase + SwapOffset;
        MmpMapPage(PhysicalAddress,
                   VirtualAddress,
                   MAP_FLAG_PRESENT | MAP_FLAG_GLOBAL | MAP_FLAG_READ_ONLY);

        //
        // Add this page to the I/O buffer.
        //

        MmIoBufferAppendPage(IoBuffer, NULL, VirtualAddress, PhysicalAddress);
        SwapOffset += PageSize;
        PageOffset += 1;
    }

    //
    // Acquire the page file's lock in order to use its paging out IRP, and
    // perform the write.
    //

    IoBufferSize = SwapOffset;
    PageCount = IoBufferSize >> PageShift;
    if (PageCount != 0) {
        Status = MmPageFilePerformIo(&(Section->PageFileBacking),
                                     IoBuffer,
                                     SectionOffset,
                                     IoBufferSize,
                                     0,
                                     WAIT_TIME_INDEFINITE,
                                     TRUE,
                                     &BytesCompleted);

        if (PagingEntry != NULL) {
            PagingEntry->U.Flags &= ~PAGING_ENTRY_FLAG_PAGING_OUT;
            PagingEntry = NULL;
        }

        UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                     UNMAP_FLAG_SEND_INVALIDATE_IPI;

        MmpUnmapPages(SwapRegion->VirtualBase, PageCount, UnmapFlags, NULL);
        if (!KSUCCESS(Status)) {
            KeCrashSystem(CRASH_PAGE_OUT_ERROR,
                          (UINTN)OriginalPagingEntry,
                          PhysicalAddress,
                          Status,
                          0);

            goto PageOutEnd;
        }

        ASSERT(BytesCompleted == IoBufferSize);
    }

    *PagesPaged += PageCount;
    Status = STATUS_SUCCESS;

PageOutEnd:

    //
    // If the paging entry wasn't dealt with, clear the paging out flag.
    //

    if (PagingEntry != NULL) {
        PagingEntry->U.Flags &= ~PAGING_ENTRY_FLAG_PAGING_OUT;
    }

    MmResetIoBuffer(IoBuffer);
    KeReleaseQueuedLock(Section->Lock);
    MmpImageSectionReleaseReference(Section);
    return Status;
}

VOID
MmpModifySectionMapping (
    PIMAGE_SECTION OwningSection,
    UINTN PageOffset,
    PHYSICAL_ADDRESS PhysicalAddress,
    BOOL CreateMapping,
    PBOOL PageWasDirty,
    BOOL SendTlbInvalidateIpi
    )

/*++

Routine Description:

    This routine maps or unmaps a freshly paged-in physical page to or from its
    location in all appropriate processes.

Arguments:

    OwningSection - Supplies a pointer to the parent section that owns this
        page. The page will be mapped in this section and all children who
        inherit this page from the owning section.

    PageOffset - Supplies the offset in pages from the beginning of the section
        where this page belongs.

    PhysicalAddress - Supplies the physical address of the freshly initialized
        page. If the mapping is not being created (ie being unmapped) then this
        parameter is ignored.

    CreateMapping - Supplies a boolean whether the page should be mapped (TRUE)
        or unmapped (FALSE).

    PageWasDirty - Supplies a pointer where a boolean will be returned
        indicating if the page that was unmapped was dirty. If the create
        mapping parameter was TRUE, this value is ignored.

    SendTlbInvalidateIpi - Supplies a boolean indicating whether a TLB
        invalidate IPI needs to be sent out for this mapping. If in doubt,
        specify TRUE.

Return Value:

    None.

--*/

{

    UINTN BitmapIndex;
    ULONG BitmapMask;
    BOOL CanWrite;
    PKPROCESS CurrentProcess;
    PIMAGE_SECTION CurrentSection;
    PKTHREAD CurrentThread;
    BOOL Dirty;
    ULONG MapFlags;
    ULONG PageShift;
    PIMAGE_SECTION PreviousSection;
    PIMAGE_SECTION PreviousSibling;
    BOOL ThisPageWasDirty;
    BOOL TraverseChildren;
    PVOID VirtualAddress;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeIsQueuedLockHeld(OwningSection->Lock) != FALSE);
    ASSERT((CreateMapping == FALSE) ||
           (PhysicalAddress != INVALID_PHYSICAL_ADDRESS));

    Dirty = FALSE;
    BitmapIndex = IMAGE_SECTION_BITMAP_INDEX(PageOffset);
    BitmapMask = IMAGE_SECTION_BITMAP_MASK(PageOffset);
    CurrentThread = KeGetCurrentThread();
    CurrentProcess = CurrentThread->OwningProcess;
    CurrentSection = OwningSection;
    PageShift = MmPageShift();
    PreviousSection = CurrentSection->Parent;
    VirtualAddress = OwningSection->VirtualAddress + (PageOffset << PageShift);

    //
    // Iterate over every section that might need to map or unmap the page.
    //

    while (CurrentSection != NULL) {

        //
        // If this is the first time the node is being visited (via parent or
        // sibling, but not child), then process it.
        //

        PreviousSibling = LIST_VALUE(CurrentSection->CopyListEntry.Previous,
                                     IMAGE_SECTION,
                                     CopyListEntry);

        if ((PreviousSection == CurrentSection->Parent) ||
            ((CurrentSection->CopyListEntry.Previous != NULL) &&
             (PreviousSection == PreviousSibling))) {

            //
            // Process the section. Skip it (and avoid traversing through
            // children if the section is not inherting the page from its
            // parent.
            //

            TraverseChildren = TRUE;
            if ((CurrentSection != OwningSection) &&
                ((CurrentSection->InheritPageBitmap[BitmapIndex] &
                                                           BitmapMask) == 0)) {

                TraverseChildren = FALSE;
            }

            MapFlags = CurrentSection->MapFlags | MAP_FLAG_PAGABLE;
            if (VirtualAddress >= KERNEL_VA_START) {
                MapFlags |= MAP_FLAG_GLOBAL;

            } else {
                MapFlags |= MAP_FLAG_USER_MODE;
            }

            if ((CurrentSection->Flags & IMAGE_SECTION_EXECUTABLE) != 0) {
                MapFlags |= MAP_FLAG_EXECUTE;
            }

            if ((CurrentSection->Flags &
                 (IMAGE_SECTION_READABLE | IMAGE_SECTION_WRITABLE)) != 0) {

                MapFlags |= MAP_FLAG_PRESENT;
            }

            if (TraverseChildren != FALSE) {

                //
                // Update the mapped boundaries.
                //

                if (CreateMapping != FALSE) {
                    if (CurrentSection->MinTouched > VirtualAddress) {
                        CurrentSection->MinTouched = VirtualAddress;
                    }

                    if (CurrentSection->MaxTouched <
                        VirtualAddress + (1 << PageShift)) {

                        CurrentSection->MaxTouched =
                                         VirtualAddress + (1 << PageShift);
                    }
                }

                if ((CurrentSection->AddressSpace ==
                     CurrentProcess->AddressSpace) ||
                    (VirtualAddress >= KERNEL_VA_START)) {

                    if (CreateMapping != FALSE) {
                        CanWrite = MmpCanWriteToSection(OwningSection,
                                                        CurrentSection,
                                                        PageOffset);

                        ASSERT(SendTlbInvalidateIpi == FALSE);

                        if (CanWrite == FALSE) {
                            MapFlags |= MAP_FLAG_READ_ONLY;
                        }

                        MmpMapPage(PhysicalAddress, VirtualAddress, MapFlags);

                    //
                    // Unmap the page in the current process.
                    //

                    } else {
                        MmpUnmapPages(VirtualAddress,
                                      1,
                                      UNMAP_FLAG_SEND_INVALIDATE_IPI,
                                      &ThisPageWasDirty);

                        if (ThisPageWasDirty != FALSE) {
                            Dirty = TRUE;
                        }
                    }

                //
                // The page belongs to another process.
                //

                } else {
                    if (CreateMapping != FALSE) {
                        CanWrite = MmpCanWriteToSection(OwningSection,
                                                        CurrentSection,
                                                        PageOffset);

                        if (CanWrite == FALSE) {
                            MapFlags |= MAP_FLAG_READ_ONLY;
                        }

                        MmpMapPageInOtherProcess(CurrentSection->AddressSpace,
                                                 PhysicalAddress,
                                                 VirtualAddress,
                                                 MapFlags,
                                                 SendTlbInvalidateIpi);

                    } else {
                        MmpUnmapPageInOtherProcess(CurrentSection->AddressSpace,
                                                   VirtualAddress,
                                                   0,
                                                   &ThisPageWasDirty);

                        if (ThisPageWasDirty != FALSE) {
                            Dirty = TRUE;
                        }
                    }
                }
            }

            //
            // The majority of the rest of this function is dedicated to tree
            // traversal. Move to the first child if eligible.
            //

            PreviousSection = CurrentSection;
            if ((TraverseChildren != FALSE) &&
                (LIST_EMPTY(&(CurrentSection->ChildList)) == FALSE)) {

                CurrentSection = LIST_VALUE(CurrentSection->ChildList.Next,
                                            IMAGE_SECTION,
                                            CopyListEntry);

            //
            // Move to the next sibling if possible.
            //

            } else if ((CurrentSection != OwningSection) &&
                       (CurrentSection->CopyListEntry.Next !=
                        &(CurrentSection->Parent->ChildList))) {

                CurrentSection = LIST_VALUE(CurrentSection->CopyListEntry.Next,
                                            IMAGE_SECTION,
                                            CopyListEntry);

            //
            // There are no children and this is the last sibling, move up to
            // the parent.
            //

            } else {

                //
                // This case only gets hit if the root is the only node in the
                // tree.
                //

                if (CurrentSection == OwningSection) {
                    CurrentSection = NULL;

                } else {
                    CurrentSection = CurrentSection->Parent;
                }
            }

        //
        // If the node is popping up from the previous, attempt to move to
        // the next sibling, or up the tree.
        //

        } else {
            PreviousSection = CurrentSection;
            if (CurrentSection == OwningSection) {
                CurrentSection = NULL;

            } else if (CurrentSection->CopyListEntry.Next !=
                       &(CurrentSection->Parent->ChildList)) {

                CurrentSection = LIST_VALUE(CurrentSection->CopyListEntry.Next,
                                            IMAGE_SECTION,
                                            CopyListEntry);

            } else {
                CurrentSection = CurrentSection->Parent;
            }
        }
    }

    if (PageWasDirty != NULL) {
        *PageWasDirty = Dirty;
    }

    return;
}

PPAGING_ENTRY
MmpCreatePagingEntry (
    PIMAGE_SECTION ImageSection,
    ULONGLONG SectionOffset
    )

/*++

Routine Description:

    This routine creates a paging entry based on the provided image section and
    page offset.

Arguments:

    ImageSection - Supplies an optional pointer to an image section.

    SectionOffset - Supplies the offset, in pages, from the beginning of the
        section.

Return Value:

    Returns a pointer to a new paging entry on success or NULL on failure.

--*/

{

    PPAGING_ENTRY PagingEntry;

    PagingEntry = MmAllocateBlock(MmPagingEntryBlockAllocator, NULL);
    if (PagingEntry == NULL) {
        goto CreatePagingEntryEnd;
    }

    RtlZeroMemory(PagingEntry, sizeof(PAGING_ENTRY));
    if (ImageSection != NULL) {
        MmpImageSectionAddReference(ImageSection);
        PagingEntry->Section = ImageSection;
        PagingEntry->U.SectionOffset = SectionOffset;
    }

CreatePagingEntryEnd:
    return PagingEntry;
}

VOID
MmpInitializePagingEntry (
    PPAGING_ENTRY PagingEntry,
    PIMAGE_SECTION ImageSection,
    ULONGLONG SectionOffset
    )

/*++

Routine Description:

    This routine initializes the given paging entry based on the provided image
    section and page offset.

Arguments:

    PagingEntry - Supplies a pointer to a paging entry to initialize.

    ImageSection - Supplies a pointer to an image section.

    SectionOffset - Supplies the offset, in pages, from the beginning of the
        section.

Return Value:

    None.

--*/

{

    ASSERT(PagingEntry->Section == NULL);

    MmpImageSectionAddReference(ImageSection);
    PagingEntry->Section = ImageSection;
    PagingEntry->U.SectionOffset = SectionOffset;
    return;
}

VOID
MmpReinitializePagingEntry (
    PPAGING_ENTRY PagingEntry,
    PIMAGE_SECTION ImageSection,
    ULONGLONG SectionOffset
    )

/*++

Routine Description:

    This routine re-initializes the given paging entry based on the provided
    image section and page offset. If there is an existing section, it will be
    dereferenced and overwritten.

Arguments:

    PagingEntry - Supplies a pointer to a paging entry to re-initialize.

    ImageSection - Supplies a pointer to an image section.

    SectionOffset - Supplies the offset, in pages, from the beginning of the
        section.

Return Value:

    None.

--*/

{

    if (PagingEntry->Section != NULL) {
        MmpImageSectionReleaseReference(PagingEntry->Section);
        PagingEntry->Section = NULL;
    }

    MmpInitializePagingEntry(PagingEntry, ImageSection, SectionOffset);
    return;
}

VOID
MmpDestroyPagingEntry (
    PPAGING_ENTRY PagingEntry
    )

/*++

Routine Description:

    This routine destroys a paging entry.

Arguments:

    PagingEntry - Supplies a pointer to a paging entry.

Return Value:

    None.

--*/

{

    if (PagingEntry->Section != NULL) {
        MmpImageSectionReleaseReference(PagingEntry->Section);
        PagingEntry->Section = NULL;
    }

    MmFreeBlock(MmPagingEntryBlockAllocator, PagingEntry);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
MmpCreatePageFile (
    PIO_HANDLE Handle,
    ULONGLONG Size
    )

/*++

Routine Description:

    This routine initializes the structures necessary for a new paging backing
    store.

Arguments:

    Handle - Supplies an open handle to the new paging file.

    Size - Supplies the size of the store in bytes.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PDEVICE Device;
    BOOL LockHeld;
    ULONGLONG PageCount;
    PPAGE_FILE PageFile;
    ULONG PageShift;
    ULONG PageSize;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    LockHeld = FALSE;
    PageFile = NULL;
    PageShift = MmPageShift();
    PageSize = MmPageSize();

    ASSERT(MmPagingForceDisable == FALSE);

    //
    // Page files are only useful in blocks of 32 pages, as the pagefile is
    // stored as a bitmap.
    //

    PageCount = ALIGN_RANGE_DOWN(Size, PageSize) >> PageShift;
    PageCount = ALIGN_RANGE_DOWN(PageCount, 32);
    if (PageCount == 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto CreatePagingDeviceEnd;
    }

    if (PageCount > MAX_ULONG) {
        PageCount = ALIGN_RANGE_DOWN(MAX_ULONG, 32);
    }

    //
    // Allocate and initialize the page file information.
    //

    AllocationSize = sizeof(PAGE_FILE) + (PageCount / 8);
    PageFile = MmAllocateNonPagedPool(AllocationSize, MM_ALLOCATION_TAG);
    if (PageFile == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreatePagingDeviceEnd;
    }

    RtlZeroMemory(PageFile, AllocationSize);
    PageFile->Handle = INVALID_HANDLE;
    PageFile->PageCount = PageCount;
    PageFile->FreePages = PageCount;
    PageFile->LastAllocatedPage = 0;
    PageFile->Bitmap = (PULONG)(PageFile + 1);
    PageFile->Lock = KeCreateQueuedLock();
    if (PageFile->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreatePagingDeviceEnd;
    }

    Status = IoGetDevice(Handle, &Device);
    if (!KSUCCESS(Status)) {
        goto CreatePagingDeviceEnd;
    }

    ASSERT(Device != NULL);

    PageFile->PagingOutIrp = IoCreateIrp(Device,
                                         IrpMajorIo,
                                         IRP_CREATE_FLAG_NO_ALLOCATE);

    if (PageFile->PagingOutIrp == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreatePagingDeviceEnd;
    }

    //
    // Notify the kernel executive about the page file so it could possibly be
    // used to collect crash information. Ignore failures here, as it's still
    // a valid page file at this point.
    //

    KeRegisterCrashDumpFile(Handle, TRUE);

    //
    // Synchronize with the arrival of other page files. The first arriving
    // page file should create all necessary events if they aren't already
    // allocated.
    //

    KeAcquireQueuedLock(MmPageFileListLock);
    LockHeld = TRUE;

    //
    // If the paging thread has not yet been created, attempt to create the
    // thread.
    //

    if (MmPagingThreadCreated == FALSE) {
        Status = PsCreateKernelThread(MmpPagingThread,
                                      NULL,
                                      "MmpPagingThread");

        if (!KSUCCESS(Status)) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreatePagingDeviceEnd;
        }

        MmPagingThreadCreated = TRUE;
    }

    //
    // With success on the horizon, transfer the handle to the page file. This
    // is a paging device handle so there is no reference count. Because it is
    // caller supplied, the caller will destroy it in all failure cases, so
    // only set it when this routine will succeed in order to avoid closing it
    // twice.
    //

    PageFile->Handle = Handle;

    //
    // Officially add it to the list of paging devices.
    //

    INSERT_BEFORE(&(PageFile->ListEntry), &MmPageFileListHead);
    MmPagingEnabled = TRUE;
    Status = STATUS_SUCCESS;

CreatePagingDeviceEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(MmPageFileListLock);
    }

    if (!KSUCCESS(Status)) {
        if (PageFile != NULL) {
            MmpDestroyPageFile(PageFile);
        }
    }

    return Status;
}

VOID
MmpDestroyPageFile (
    PPAGE_FILE PageFile
    )

/*++

Routine Description:

    This routine destroys a page file.

Arguments:

    PageFile - Supplies a pointer to a page file that is to be destroyed.

Return Value:

    None.

--*/

{

    ASSERT(PageFile->FreePages == PageFile->PageCount);

    //
    // De-register the page file from use by the crash dump system.
    //

    KeRegisterCrashDumpFile(PageFile->Handle, FALSE);
    if (PageFile->Lock != NULL) {
        KeDestroyQueuedLock(PageFile->Lock);
    }

    if (PageFile->PagingOutIrp != NULL) {
        IoDestroyIrp(PageFile->PagingOutIrp);
    }

    if (PageFile->Handle != INVALID_HANDLE) {
        IoClose(PageFile->Handle);
    }

    MmFreeNonPagedPool(PageFile);
    return;
}

KSTATUS
MmpAllocateFromPageFile (
    PPAGE_FILE PageFile,
    ULONG PageCount,
    PULONG Allocation
    )

/*++

Routine Description:

    This routine allocates space from a page file.

Arguments:

    PageFile - Supplies a pointer to the page file to allocate from.

    PageCount - Supplies the number of consecutive pages needed.

    Allocation - Supplies a pointer where the allocation will be returned
        upon success.

Return Value:

    STATUS_SUCCESS on success. In this case allocation will be filled out with
    a valid page file allocation.

    STATUS_INSUFFICIENT_RESOURCES if the request could not be satisified.

--*/

{

    ULONG BitsRemaining;
    ULONG CurrentChunk;
    ULONG CurrentChunkIndex;
    ULONG CurrentIndex;
    ULONG CurrentProposal;
    ULONG FreePagesThisRange;
    KSTATUS Status;
    ULONG TotalPages;
    ULONG TotalPagesSearched;
    ULONG Value;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    *Allocation = -1;
    Status = STATUS_INSUFFICIENT_RESOURCES;
    if (PageCount == 0) {

        ASSERT(FALSE);

        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireQueuedLock(PageFile->Lock);
    if (PageFile->FreePages == 0) {
        goto AllocateFromPageFileEnd;
    }

    TotalPages = PageFile->PageCount;
    TotalPagesSearched = 0;

    //
    // Get the hint from the page file and set up the variables.
    //

    CurrentProposal = PageFile->LastAllocatedPage;
    CurrentIndex = CurrentProposal;
    CurrentChunkIndex = CurrentProposal / 32;
    CurrentChunk = PageFile->Bitmap[CurrentChunkIndex];
    CurrentChunk = CurrentChunk >> (CurrentIndex - (CurrentChunkIndex * 32));
    BitsRemaining = 32 - (CurrentProposal - (CurrentChunkIndex * 32));

    //
    // Loop scanning alternatively for free ranges and then allocated ranges.
    //

    Value = 0;
    while (TRUE) {

        //
        // Loop while the range is free and clear.
        //

        FreePagesThisRange = 0;
        while ((CurrentChunk & 0x1) == Value) {
            TotalPagesSearched += 1;
            CurrentChunk >>= 1;
            BitsRemaining -= 1;
            CurrentIndex += 1;
            if (Value == 0) {
                FreePagesThisRange += 1;
            }

            //
            // If this satisfies the allocation, use it.
            //

            if (FreePagesThisRange == PageCount) {
                Status = STATUS_SUCCESS;
                break;
            }

            //
            // Get the next chunk if the bits need updating.
            //

            if (BitsRemaining == 0) {
                CurrentChunkIndex = CurrentIndex / 32;
                CurrentChunk = PageFile->Bitmap[CurrentChunkIndex];
                BitsRemaining = 32;

                //
                // If this is where the search started, fail.
                //

                if (TotalPagesSearched >= TotalPages) {
                    goto AllocateFromPageFileEnd;
                }

                //
                // If this is the end of the page file, break.
                //

                if (CurrentIndex >= PageFile->PageCount) {
                    CurrentIndex = 0;
                    CurrentChunkIndex = 0;
                    CurrentChunk = PageFile->Bitmap[CurrentChunkIndex];
                    BitsRemaining = 32;
                    break;
                }
            }
        }

        //
        // If the allocation was satisfied, break out here.
        //

        if (KSUCCESS(Status)) {
            break;
        }

        //
        // If free pages were being searched for and a claimed one was hit,
        // loop for claimed pages now. Otherwise, loop for free pages and
        // remember this was the index where the search started.
        //

        FreePagesThisRange = 0;
        if (Value == 0) {
            Value = 1;

        } else {
            CurrentProposal = CurrentIndex;
            Value = 0;
        }
    }

    //
    // If the search was successful, allocate those pages.
    //

    if (KSUCCESS(Status)) {
        for (CurrentIndex = CurrentProposal;
             CurrentIndex < CurrentProposal + PageCount;
             CurrentIndex += 1) {

            CurrentChunkIndex = CurrentIndex / 32;
            PageFile->Bitmap[CurrentChunkIndex] |=
                                 1 << (CurrentIndex - (CurrentChunkIndex * 32));
        }

        *Allocation = CurrentProposal;
        PageFile->LastAllocatedPage = CurrentProposal + PageCount;
        if (PageFile->LastAllocatedPage >= PageFile->PageCount) {
            PageFile->LastAllocatedPage = 0;
        }

        ASSERT(PageFile->FreePages >= PageCount);

        PageFile->FreePages -= PageCount;
    }

AllocateFromPageFileEnd:
    if (Status == STATUS_INSUFFICIENT_RESOURCES) {
        PageFile->FailedAllocations += 1;
    }

    KeReleaseQueuedLock(PageFile->Lock);
    return Status;
}

VOID
MmpFreeFromPageFile (
    PPAGE_FILE PageFile,
    ULONG Allocation,
    ULONG PageCount
    )

/*++

Routine Description:

    This routine frees space from a page file.

Arguments:

    PageFile - Supplies a pointer to the page file where the space was
        allocated from.

    Allocation - Supplies the index of the beginning of the allocation.

    PageCount - Supplies the number of pages to free.

Return Value:

    None.

--*/

{

    ULONG CurrentChunkIndex;
    ULONG CurrentIndex;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(PageFile->Lock);
    for (CurrentIndex = Allocation;
         CurrentIndex < Allocation + PageCount;
         CurrentIndex += 1) {

        CurrentChunkIndex = CurrentIndex / 32;

        //
        // Assert that the page was actually marked as claimed, and unmark it.
        //

        ASSERT((PageFile->Bitmap[CurrentChunkIndex] &
                (1 << (CurrentIndex - (CurrentChunkIndex * 32)))) != 0);

        PageFile->Bitmap[CurrentChunkIndex] &=
                             ~(1 << (CurrentIndex - (CurrentChunkIndex * 32)));
    }

    PageFile->FreePages += PageCount;
    KeReleaseQueuedLock(PageFile->Lock);
    return;
}

VOID
MmpPagingThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine attempts to release physical page pressure by paging out
    pages or removing them from the page cache if memory is tight. This runs
    on its own thread, which cannot allocate memory or touch paged pool.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread. This
        parameter is not used.

Return Value:

    None. This thread never exits.

--*/

{

    ULONG AllocationSize;
    UINTN FreePagesTarget;
    PIO_BUFFER IoBuffer;
    UINTN PageCount;
    PKEVENT PhysicalMemoryWarningEvent;
    PVOID SignalingObject;
    UINTN Size;
    KSTATUS Status;
    PMEMORY_RESERVATION SwapRegion;
    PVOID WaitObjectArray[2];

    //
    // Create the I/O buffer used to page out in chunks, and allocate a VA
    // range for it. Allocate it locally in order to avoid allocating the array
    // for the page cache entries, as this I/O buffer will never be backed by
    // page cache entries.
    //

    Size = ALIGN_RANGE_UP(PAGE_OUT_CHUNK_SIZE, MmPageSize());
    PageCount = Size >> MmPageShift();
    AllocationSize = sizeof(IO_BUFFER);
    AllocationSize += (PageCount * sizeof(IO_BUFFER_FRAGMENT));
    IoBuffer = MmAllocateNonPagedPool(AllocationSize, MM_IO_ALLOCATION_TAG);
    if (IoBuffer == NULL) {
        return;
    }

    RtlZeroMemory(IoBuffer, AllocationSize);
    IoBuffer->Internal.MaxFragmentCount = PageCount;
    IoBuffer->Fragment = (PVOID)(IoBuffer + 1);
    IoBuffer->Internal.Flags = IO_BUFFER_INTERNAL_FLAG_NON_PAGED |
                               IO_BUFFER_INTERNAL_FLAG_EXTENDABLE |
                               IO_BUFFER_INTERNAL_FLAG_MEMORY_LOCKED;

    SwapRegion = MmCreateMemoryReservation(NULL,
                                           PAGE_OUT_CHUNK_SIZE,
                                           0,
                                           MAX_ADDRESS,
                                           AllocationStrategyAnyAddress,
                                           TRUE);

    if (SwapRegion == NULL) {
        MmFreeIoBuffer(IoBuffer);
        return;
    }

    //
    // Make sure the page tables are in place for this swap region. The paging
    // thread cannot be caught waiting for physical memory to become free in
    // order to allocate a page table.
    //

    MmpCreatePageTables(SwapRegion->VirtualBase, SwapRegion->Size);
    MmPagingThread = KeGetCurrentThread();

    ASSERT(2 < BUILTIN_WAIT_BLOCK_ENTRY_COUNT);

    PhysicalMemoryWarningEvent = MmGetPhysicalMemoryWarningEvent();
    WaitObjectArray[0] = MmPagingEvent;
    WaitObjectArray[1] = PhysicalMemoryWarningEvent;
    while (TRUE) {
        Status = ObWaitOnObjects(WaitObjectArray,
                                 2,
                                 0,
                                 WAIT_TIME_INDEFINITE,
                                 NULL,
                                 &SignalingObject);

        ASSERT(KSUCCESS(Status));

        //
        // If the memory warning event signaled for something other than
        // warning level 1, ignore it.
        //

        if (SignalingObject == PhysicalMemoryWarningEvent) {
            if (MmGetPhysicalMemoryWarningLevel() != MemoryWarningLevel1) {
                continue;
            }
        }

        //
        // Always unsignal the paging event because paging is about to run.
        //

        KeSignalEvent(MmPagingEvent, SignalOptionUnsignal);

        //
        // If paging is not enabled, act like something was released and go
        // back to sleep.
        //

        if (MmPagingEnabled == FALSE) {
            KeSignalEvent(MmPagingFreePagesEvent, SignalOptionSignalAll);
            continue;
        }

        //
        // Snap and reset the target free page count, then go for it.
        //

        FreePagesTarget = RtlAtomicExchange(&MmPagingFreeTarget, 0);
        MmpPageOutPhysicalPages(FreePagesTarget, IoBuffer, SwapRegion);
    }

    MmFreeIoBuffer(IoBuffer);
    MmFreeMemoryReservation(SwapRegion);
    return;
}

KSTATUS
MmpPageInAnonymousSection (
    PIMAGE_SECTION ImageSection,
    UINTN PageOffset,
    PIO_BUFFER LockedIoBuffer
    )

/*++

Routine Description:

    This routine pages a physical page in from a page file or allocates a new
    free physical page. This routine must be called at low level.

Arguments:

    ImageSection - Supplies a pointer to the image section within the process
        to page in.

    PageOffset - Supplies the offset, in pages, from the beginning of the
        section.

    LockedIoBuffer - Supplies an optional pointer to an uninitialized I/O
        buffer that will be initialized with the the paged in page, effectively
        locking the page until the I/O buffer is released.

Return Value:

    Status code.

--*/

{

    ULONG Attributes;
    UINTN BitmapIndex;
    ULONG BitmapMask;
    PAGE_IN_CONTEXT Context;
    BOOL Dirty;
    PHYSICAL_ADDRESS ExistingPhysicalAddress;
    ULONG IoBufferFlags;
    BOOL LockHeld;
    BOOL LockPage;
    BOOL NonPaged;
    PIMAGE_SECTION OwningSection;
    ULONG PageShift;
    ULONG PageSize;
    PIMAGE_SECTION RootSection;
    KSTATUS Status;
    PVOID VirtualAddress;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((ImageSection->Flags & IMAGE_SECTION_BACKED) == 0);
    ASSERT((ImageSection->Flags & IMAGE_SECTION_SHARED) == 0);
    ASSERT(ImageSection->ImageBacking.DeviceHandle == INVALID_HANDLE);

    RtlZeroMemory(&Context, sizeof(PAGE_IN_CONTEXT));

    ASSERT(Context.PhysicalAddress == INVALID_PHYSICAL_ADDRESS);

    ExistingPhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    LockHeld = FALSE;
    OwningSection = NULL;
    PageShift = MmPageShift();
    PageSize = MmPageSize();
    RootSection = NULL;
    VirtualAddress = ImageSection->VirtualAddress + (PageOffset << PageShift);

    //
    // Loop trying to page into the section.
    //

    while (TRUE) {
        if ((Context.Flags & PAGE_IN_CONTEXT_FLAG_ALLOCATE_MASK) != 0) {
            Status = MmpAllocatePageInStructures(ImageSection, &Context);
            if (!KSUCCESS(Status)) {
                goto PageInAnonymousSectionEnd;
            }
        }

        //
        // Acquire the section lock to freeze all mappings and unmappings.
        //

        KeAcquireQueuedLock(ImageSection->Lock);
        LockHeld = TRUE;

        //
        // Check the mapping in case another processor has taken the page fault
        // and resolved it.
        //

        ExistingPhysicalAddress = MmpVirtualToPhysical(VirtualAddress,
                                                       &Attributes);

        if (ExistingPhysicalAddress != INVALID_PHYSICAL_ADDRESS) {

            ASSERT((Attributes & MAP_FLAG_PRESENT) != 0);

            Status = STATUS_SUCCESS;
            break;
        }

        //
        // Find the actual section owning this page, which may be a parent.
        // This takes a reference on the owning section.
        //

        OwningSection = MmpGetOwningSection(ImageSection, PageOffset);

        ASSERT(ImageSection->Lock == OwningSection->Lock);

        //
        // If the owning section is destroyed, there is no reason to read
        // anything from the page file or to allocate a new page. The entire
        // section has already been unmapped.
        //

        if ((OwningSection->Flags & IMAGE_SECTION_DESTROYED) != 0) {

            //
            // A section isolates itself from all inheriting children before
            // being destroyed. So a destroyed owning section must be the
            // faulting section.
            //

            ASSERT(OwningSection == ImageSection);

            Status = STATUS_TOO_LATE;
            break;
        }

        //
        // Figure out if the page is clean or dirty.
        //

        BitmapIndex = IMAGE_SECTION_BITMAP_INDEX(PageOffset);
        BitmapMask = IMAGE_SECTION_BITMAP_MASK(PageOffset);
        Dirty = FALSE;
        if ((OwningSection->DirtyPageBitmap != NULL) &&
            ((OwningSection->DirtyPageBitmap[BitmapIndex] & BitmapMask) != 0)) {

            Dirty = TRUE;
        }

        //
        // If the page is clean then simply use the fresh memory.
        //

        if (Dirty == FALSE) {

            ASSERT(OwningSection->ImageBacking.DeviceHandle == INVALID_HANDLE);

            //
            // Loop back and allocate a physical page if necessary.
            //

            if (Context.PhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
                KeReleaseQueuedLock(ImageSection->Lock);
                MmpImageSectionReleaseReference(OwningSection);
                if (RootSection != NULL) {
                    MmpImageSectionReleaseReference(RootSection);
                    RootSection = NULL;
                }

                OwningSection = NULL;
                Context.Flags |= PAGE_IN_CONTEXT_FLAG_ALLOCATE_PAGE;
                LockHeld = FALSE;
                continue;
            }

            //
            // Zero the contents if the page is getting mapped to user mode.
            //

            if (VirtualAddress < KERNEL_VA_START) {
                MmpZeroPage(Context.PhysicalAddress);
            }

            Status = STATUS_SUCCESS;
            break;
        }

        //
        // Otherwise it is dirty. Prepare to read from the page file, which
        // requires both an IRP and a memory reservation in addition to a
        // physical page. The IRP and memory reservation are shared by the
        // whole image section tree, so see if the root already has them
        // allocated.
        //

        RootSection = MmpGetRootSection(OwningSection);
        Status = MmpPrepareForPageFileRead(RootSection,
                                           OwningSection,
                                           &Context);

        if (!KSUCCESS(Status)) {
            goto PageInAnonymousSectionEnd;
        }

        //
        // Loop back if something in the context needs to be allocated.
        //

        if ((Context.Flags & PAGE_IN_CONTEXT_FLAG_ALLOCATE_MASK) != 0) {
            KeReleaseQueuedLock(ImageSection->Lock);
            MmpImageSectionReleaseReference(OwningSection);
            MmpImageSectionReleaseReference(RootSection);
            OwningSection = NULL;
            RootSection = NULL;
            LockHeld = FALSE;
            continue;
        }

        ASSERT(Context.PagingEntry != NULL);

        //
        // Read the page file to get the necessary image section contents.
        //

        Status = MmpReadPageFile(RootSection,
                                 OwningSection,
                                 PageOffset,
                                 &Context);

        if (!KSUCCESS(Status)) {
            goto PageInAnonymousSectionEnd;
        }

        //
        // If the end of the loop is reached, then break out.
        //

        break;
    }

    ASSERT(Status == STATUS_SUCCESS);

    //
    // Before proceeding with the mapping, check to see if the image section
    // has been destroyed.
    //

    if ((ImageSection->Flags & IMAGE_SECTION_DESTROYED) != 0) {
        Status = STATUS_TOO_LATE;
        goto PageInAnonymousSectionEnd;
    }

    //
    // Also check to ensure the section covers the region.
    //

    if ((ImageSection->Size >> PageShift) <= PageOffset) {
        Status = STATUS_TRY_AGAIN;
        goto PageInAnonymousSectionEnd;
    }

PageInAnonymousSectionEnd:

    //
    // Success means that either this routine just paged the memory in or it
    // was already paged in and mapped by another thread.
    //

    if (KSUCCESS(Status)) {

        ASSERT((ImageSection->Flags & IMAGE_SECTION_DESTROYED) == 0);
        ASSERT(LockHeld != FALSE);

        //
        // Handle the case where the page was already mapped.
        //

        if (ExistingPhysicalAddress != INVALID_PHYSICAL_ADDRESS) {

            //
            // Lock the page if requested. Do not do this for non-paged
            // sections. They should not need to lock a page for the second
            // time. It's already locked.
            //

            if (LockedIoBuffer != NULL) {
                NonPaged = TRUE;
                if ((ImageSection->Flags & IMAGE_SECTION_NON_PAGED) == 0) {
                    NonPaged = FALSE;
                    Status = MmpLockPhysicalPages(ExistingPhysicalAddress, 1);
                }

                if (KSUCCESS(Status)) {
                    IoBufferFlags = IO_BUFFER_FLAG_KERNEL_MODE_DATA |
                                    IO_BUFFER_FLAG_MEMORY_LOCKED;

                    Status = MmInitializeIoBuffer(LockedIoBuffer,
                                                  NULL,
                                                  ExistingPhysicalAddress,
                                                  PageSize,
                                                  IoBufferFlags);

                    //
                    // On success, pass the lock off to the I/O buffer so that
                    // when the caller releases it, the page gets unlocked.
                    //

                    if (KSUCCESS(Status) && (NonPaged == FALSE)) {
                        LockedIoBuffer->Internal.Flags |=
                                            IO_BUFFER_INTERNAL_FLAG_LOCK_OWNED;
                    }
                }
            }

        //
        // Otherwise, map the page to its rightful spot. Other processors may
        // begin touching it immediately.
        //

        } else {

            ASSERT(OwningSection != NULL);
            ASSERT(KeIsQueuedLockHeld(OwningSection->Lock) != FALSE);

            LockPage = FALSE;
            if (LockedIoBuffer != NULL) {
                LockPage = TRUE;
                IoBufferFlags = IO_BUFFER_FLAG_KERNEL_MODE_DATA |
                                IO_BUFFER_FLAG_MEMORY_LOCKED;

                Status = MmInitializeIoBuffer(LockedIoBuffer,
                                              NULL,
                                              Context.PhysicalAddress,
                                              PageSize,
                                              IoBufferFlags);

                //
                // Pass the lock off to the I/O buffer so that when the caller
                // releases it, the page gets unlocked.
                //

                if (KSUCCESS(Status) && (Context.PagingEntry != NULL)) {
                    LockedIoBuffer->Internal.Flags |=
                                            IO_BUFFER_INTERNAL_FLAG_LOCK_OWNED;
                }
            }

            if (KSUCCESS(Status)) {
                MmpMapPageInSection(OwningSection,
                                    PageOffset,
                                    Context.PhysicalAddress,
                                    Context.PagingEntry,
                                    LockPage);

                Context.PagingEntry = NULL;
                Context.PhysicalAddress = INVALID_PHYSICAL_ADDRESS;
            }
        }
    }

    //
    // Release the lock, if necessary, and release any un-used resources.
    //

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(ImageSection->Lock);
    }

    if (OwningSection != NULL) {
        MmpImageSectionReleaseReference(OwningSection);
    }

    if (RootSection != NULL) {
        MmpImageSectionReleaseReference(RootSection);
    }

    MmpDestroyPageInContext(&Context);
    return Status;
}

KSTATUS
MmpPageInSharedSection (
    PIMAGE_SECTION ImageSection,
    UINTN PageOffset,
    PIO_BUFFER LockedIoBuffer
    )

/*++

Routine Description:

    This routine pages a physical page in from the page cache. This routine
    must be called at low level.

Arguments:

    ImageSection - Supplies a pointer to the image section within the process
        to page in.

    PageOffset - Supplies the offset, in pages, from the beginning of the
        section.

    LockedIoBuffer - Supplies an optional pointer to an uninitialized I/O
        buffer that will be initialized with the the paged in page, effectively
        locking the page until the I/O buffer is released.

Return Value:

    Status code.

--*/

{

    ULONG Attributes;
    PHYSICAL_ADDRESS ExistingPhysicalAddress;
    PIO_BUFFER IoBuffer;
    IO_BUFFER IoBufferData;
    BOOL LockHeld;
    ULONG MapFlags;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    ULONG PageShift;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;
    ULONG TruncateCount;
    PVOID VirtualAddress;

    ExistingPhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    IoBuffer = NULL;
    PageCacheEntry = NULL;
    PageShift = MmPageShift();
    PhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    VirtualAddress = ImageSection->VirtualAddress + (PageOffset << PageShift);

    ASSERT((ImageSection->Flags & IMAGE_SECTION_SHARED) != 0);
    ASSERT((ImageSection->Flags & IMAGE_SECTION_BACKED) != 0);
    ASSERT(ImageSection->Parent == NULL);
    ASSERT(LIST_EMPTY(&(ImageSection->ChildList)) != FALSE);

    //
    // Acquire the image section lock to freeze all mappings and unmappings in
    // this section.
    //

    KeAcquireQueuedLock(ImageSection->Lock);
    LockHeld = TRUE;

    //
    // Take a reference on the image backing handle to ensure it doesn't go
    // away during the paging operation.
    //

    MmpImageSectionAddImageBackingReference(ImageSection);

    //
    // While the lock was released, another thread may have paged in this page
    // so check for an existing mapping. Skip this if the page needs to be
    // locked as the page cache entry needs to be retrieved via read in order
    // to lock the page.
    //

    if (LockedIoBuffer == NULL) {
        ExistingPhysicalAddress = MmpVirtualToPhysical(VirtualAddress,
                                                       &Attributes);

        if (ExistingPhysicalAddress != INVALID_PHYSICAL_ADDRESS) {

            ASSERT((Attributes & MAP_FLAG_PRESENT) != 0);

            Status = STATUS_SUCCESS;
            goto PageInSharedSectionEnd;
        }
    }

    //
    // Loop trying to read the backing image.
    //

    while (TRUE) {
        if ((ImageSection->Flags & IMAGE_SECTION_DESTROYED) != 0) {
            Status = STATUS_TOO_LATE;
            goto PageInSharedSectionEnd;
        }

        ASSERT(ImageSection->ImageBacking.DeviceHandle != INVALID_HANDLE);

        //
        // Record the current truncation count for this image section and then
        // release the lock to perform the read.
        //

        TruncateCount = ImageSection->TruncateCount;
        KeReleaseQueuedLock(ImageSection->Lock);
        LockHeld = FALSE;

        //
        // Reset the I/O buffer if it was previously used or initialize it.
        //

        if (IoBuffer != NULL) {
            MmResetIoBuffer(IoBuffer);

        } else {
            IoBuffer = &IoBufferData;
            Status = MmInitializeIoBuffer(IoBuffer,
                                          NULL,
                                          INVALID_PHYSICAL_ADDRESS,
                                          0,
                                          IO_BUFFER_FLAG_KERNEL_MODE_DATA);

            if (!KSUCCESS(Status)) {
                goto PageInSharedSectionEnd;
            }
        }

        //
        // Read from the backing image at the faulting page's offset.
        //

        Status = MmpReadBackingImage(ImageSection, PageOffset, IoBuffer);
        if (!KSUCCESS(Status)) {

            ASSERT(MmGetIoBufferPageCacheEntry(IoBuffer, 0) == NULL);

            goto PageInSharedSectionEnd;
        }

        //
        // Get the physical address from the I/O buffer. It comes directly from
        // the page cache.
        //

        ASSERT(IoBuffer->FragmentCount == 1);

        PageCacheEntry = MmGetIoBufferPageCacheEntry(IoBuffer, 0);
        PhysicalAddress = IoBuffer->Fragment[0].PhysicalAddress;

        ASSERT((PageCacheEntry == NULL) ||
               (PhysicalAddress ==
                IoGetPageCacheEntryPhysicalAddress(PageCacheEntry, NULL)));

        //
        // Acquire the image section lock.
        //

        KeAcquireQueuedLock(ImageSection->Lock);
        LockHeld = TRUE;

        //
        // While the lock was released, another thread may have paged it in.
        // Check for an existing mapping and break out of the loop if the page
        // does not need to be locked. If it needs to be locked then the page
        // cache entry must match the existing physical address. This is not
        // a guarantee.
        //

        ExistingPhysicalAddress = MmpVirtualToPhysical(VirtualAddress,
                                                       &Attributes);

        if ((LockedIoBuffer == NULL) &&
            (ExistingPhysicalAddress != INVALID_PHYSICAL_ADDRESS)) {

            ASSERT((Attributes & MAP_FLAG_PRESENT) != 0);

            Status = STATUS_SUCCESS;
            break;
        }

        //
        // If the truncate count is not the same, the page cache entry read
        // above may have been evicted. Loop back and try again.
        //

        if (ImageSection->TruncateCount != TruncateCount) {
            continue;
        }

        //
        // With no intervening truncations, either there is no existing mapping
        // or the existing mapping maps the physical address stored in the page
        // cache entry.
        //

        ASSERT((ExistingPhysicalAddress == INVALID_PHYSICAL_ADDRESS) ||
               (ExistingPhysicalAddress == PhysicalAddress));

        break;
    }

    ASSERT(KSUCCESS(Status));

    //
    // If the image section got destroyed, fail rather than mapping the page
    // in.
    //

    if ((ImageSection->Flags & IMAGE_SECTION_DESTROYED) != 0) {
        Status = STATUS_TOO_LATE;
        goto PageInSharedSectionEnd;
    }

    //
    // Also check to ensure the section covers the region.
    //

    if ((ImageSection->Size >> PageShift) <= PageOffset) {
        Status = STATUS_TRY_AGAIN;
        goto PageInSharedSectionEnd;
    }

PageInSharedSectionEnd:

    //
    // Handle the success case. The page may have been read from the page cache
    // or found to have already been mapped.
    //
    // N.B. Paged pool backed page cache entries may be touched below. Shared
    //      sections are allowed to touch paged pool with their section lock
    //      held because the paging thread cannot select their pages for
    //      page-out.
    //

    if (KSUCCESS(Status)) {

        ASSERT(LockHeld != FALSE);
        ASSERT((ImageSection->Flags & IMAGE_SECTION_DESTROYED) == 0);

        //
        // If an existing mapping was not found, then map the page that was
        // read from the page cache.
        //

        if (ExistingPhysicalAddress == INVALID_PHYSICAL_ADDRESS) {

            ASSERT(PhysicalAddress != INVALID_PHYSICAL_ADDRESS);

            //
            // Always map shared regions read-only to start. If the page write
            // faults then the mapping will be changed. This is done to prevent
            // unnecessary page cache cleaning when the section is destroyed.
            //

            MapFlags = ImageSection->MapFlags | MAP_FLAG_READ_ONLY;
            if (VirtualAddress >= KERNEL_VA_START) {
                MapFlags |= MAP_FLAG_GLOBAL;

            } else {
                MapFlags |= MAP_FLAG_USER_MODE;
            }

            if ((ImageSection->Flags &
                 (IMAGE_SECTION_READABLE | IMAGE_SECTION_WRITABLE)) != 0) {

                MapFlags |= MAP_FLAG_PRESENT;
            }

            if ((ImageSection->Flags & IMAGE_SECTION_EXECUTABLE) != 0) {
                MapFlags |= MAP_FLAG_EXECUTE;
            }

            MmpMapPage(PhysicalAddress, VirtualAddress, MapFlags);

            //
            // Update the mapped section boundaries.
            //

            if (ImageSection->MinTouched > VirtualAddress) {
                ImageSection->MinTouched = VirtualAddress;
            }

            if (ImageSection->MaxTouched < VirtualAddress + (1 << PageShift)) {
                ImageSection->MaxTouched = VirtualAddress + (1 << PageShift);
            }
        }

        //
        // If the locked I/O buffer needs to be filled in, do so with the
        // saved page cache entry. This will take a reference on the page cache
        // entry and, therefore, touch paged pool. This is OK to do under the
        // image section lock given that shared image sections are not eligible
        // for paging out.
        //

        if (LockedIoBuffer != NULL) {

            ASSERT((ExistingPhysicalAddress == INVALID_PHYSICAL_ADDRESS) ||
                   (ExistingPhysicalAddress == PhysicalAddress));

            //
            // Initialize the I/O buffer for locked kernel memory. When page
            // cache entries are appended to I/O buffers, an extra reference
            // is taken, automatically locking them. When the I/O buffer is
            // released, the reference count is decremented and the page cache
            // entry is essentially unlocked.
            //

            Status = MmInitializeIoBuffer(LockedIoBuffer,
                                          NULL,
                                          INVALID_PHYSICAL_ADDRESS,
                                          0,
                                          IO_BUFFER_FLAG_KERNEL_MODE_DATA);

            if (KSUCCESS(Status)) {
                if (PageCacheEntry != NULL) {
                    MmIoBufferAppendPage(LockedIoBuffer,
                                         PageCacheEntry,
                                         NULL,
                                         INVALID_PHYSICAL_ADDRESS);

                } else {

                    ASSERT(ExistingPhysicalAddress != INVALID_PHYSICAL_ADDRESS);

                    Status = MmAppendIoBufferData(LockedIoBuffer,
                                                  VirtualAddress,
                                                  ExistingPhysicalAddress,
                                                  1 << PageShift);
                }
            }
        }
    }

    //
    // Unlock the section if necessary and then release un-used resources.
    //

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(ImageSection->Lock);
    }

    MmpImageSectionReleaseImageBackingReference(ImageSection);
    if (IoBuffer != NULL) {
        MmFreeIoBuffer(IoBuffer);
    }

    return Status;
}

KSTATUS
MmpPageInBackedSection (
    PIMAGE_SECTION ImageSection,
    UINTN PageOffset,
    PIO_BUFFER LockedIoBuffer
    )

/*++

Routine Description:

    This routine pages a physical page in from a page file or an aligned
    backing image. This routine must be called at low level.

Arguments:

    ImageSection - Supplies a pointer to the image section within the process
        to page in.

    PageOffset - Supplies the offset, in pages, from the beginning of the
        section.

    LockedIoBuffer - Supplies an optional pointer to an uninitialized I/O
        buffer that will be initialized with the the paged in page, effectively
        locking the page until the I/O buffer is released.

Return Value:

    Status code.

--*/

{

    UINTN BitmapIndex;
    ULONG BitmapMask;
    PAGE_IN_CONTEXT Context;
    PULONG DirtyPageBitmap;
    PHYSICAL_ADDRESS ExistingPhysicalAddress;
    PIO_BUFFER IoBuffer;
    IO_BUFFER IoBufferData;
    ULONG IoBufferFlags;
    PIO_BUFFER LockedPageCacheIoBuffer;
    IO_BUFFER LockedPageCacheIoBufferData;
    BOOL LockHeld;
    BOOL LockPage;
    BOOL LockPageCacheEntry;
    BOOL NonPaged;
    PIMAGE_SECTION OriginalOwner;
    PIMAGE_SECTION OwningSection;
    PHYSICAL_ADDRESS PageCacheAddress;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    UINTN PageShift;
    UINTN PageSize;
    PPAGING_ENTRY PagingEntry;
    PIMAGE_SECTION RootSection;
    KSTATUS Status;
    ULONG TruncateCount;
    PVOID VirtualAddress;

    BitmapIndex = IMAGE_SECTION_BITMAP_INDEX(PageOffset);
    BitmapMask = IMAGE_SECTION_BITMAP_MASK(PageOffset);
    RtlZeroMemory(&Context, sizeof(PAGE_IN_CONTEXT));

    ASSERT(Context.PhysicalAddress == INVALID_PHYSICAL_ADDRESS);

    ExistingPhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    IoBuffer = NULL;
    LockHeld = FALSE;
    LockPageCacheEntry = FALSE;
    LockedPageCacheIoBuffer = NULL;
    OwningSection = NULL;
    PageCacheAddress = INVALID_PHYSICAL_ADDRESS;
    PageCacheEntry = NULL;
    PageShift = MmPageShift();
    PageSize = MmPageSize();
    RootSection = NULL;
    VirtualAddress = ImageSection->VirtualAddress + (PageOffset << PageShift);

    ASSERT((ImageSection->Flags & IMAGE_SECTION_BACKED) != 0);

    //
    // The presence of a locked I/O buffer indicates that the page should be
    // locked.
    //

    LockPage = FALSE;
    if (LockedIoBuffer != NULL) {
        LockPage = TRUE;
    }

    //
    // Loop trying to page into the section.
    //

    while (TRUE) {
        if ((Context.Flags & PAGE_IN_CONTEXT_FLAG_ALLOCATE_MASK) != 0) {
            Status = MmpAllocatePageInStructures(ImageSection, &Context);
            if (!KSUCCESS(Status)) {
                goto PageInCacheBackedSectionEnd;
            }
        }

        //
        // Lock the page cache entry if requested. The page cache entry is
        // found by reading the backing image at the given offset.
        //

        if (LockPageCacheEntry != FALSE) {
            LockPageCacheEntry = FALSE;

            ASSERT(LockPage != FALSE);

            //
            // Release any resources from the previous read or initialize
            // resources for first time use.
            //

            if (IoBuffer != NULL) {
                MmResetIoBuffer(IoBuffer);

            } else {
                IoBuffer = &IoBufferData;
                Status = MmInitializeIoBuffer(IoBuffer,
                                              NULL,
                                              INVALID_PHYSICAL_ADDRESS,
                                              0,
                                              IO_BUFFER_FLAG_KERNEL_MODE_DATA);

                if (!KSUCCESS(Status)) {
                    goto PageInCacheBackedSectionEnd;
                }
            }

            Status = MmpReadBackingImage(ImageSection, PageOffset, IoBuffer);
            MmpImageSectionReleaseImageBackingReference(ImageSection);
            if (!KSUCCESS(Status)) {

                ASSERT(MmGetIoBufferPageCacheEntry(IoBuffer, 0) == NULL);

                goto PageInCacheBackedSectionEnd;
            }

            //
            // Get the page cache entry that was just read.
            //

            ASSERT(IoBuffer != NULL);
            ASSERT(IoBuffer->FragmentCount == 1);

            PageCacheEntry = MmGetIoBufferPageCacheEntry(IoBuffer, 0);
            PageCacheAddress = IoBuffer->Fragment[0].PhysicalAddress;

            ASSERT((PageCacheEntry == NULL) ||
                   (PageCacheAddress ==
                    IoGetPageCacheEntryPhysicalAddress(PageCacheEntry, NULL)));

            //
            // Reset or initialize the locked page cache I/O buffer for use.
            //

            if (LockedPageCacheIoBuffer != NULL) {
                MmResetIoBuffer(LockedPageCacheIoBuffer);

            } else {
                LockedPageCacheIoBuffer = &LockedPageCacheIoBufferData;
                Status = MmInitializeIoBuffer(LockedPageCacheIoBuffer,
                                              NULL,
                                              INVALID_PHYSICAL_ADDRESS,
                                              0,
                                              IO_BUFFER_FLAG_KERNEL_MODE_DATA);

                if (!KSUCCESS(Status)) {
                    goto PageInCacheBackedSectionEnd;
                }
            }

            //
            // Store the page cache entry in the locked I/O buffer. This takes
            // a reference on the page cache entry.
            //

            if (PageCacheEntry != NULL) {
                MmIoBufferAppendPage(LockedPageCacheIoBuffer,
                                     PageCacheEntry,
                                     NULL,
                                     INVALID_PHYSICAL_ADDRESS);

            } else {
                Status = MmAppendIoBufferData(LockedPageCacheIoBuffer,
                                              VirtualAddress,
                                              PageCacheAddress,
                                              PageSize);

                if (!KSUCCESS(Status)) {
                    goto PageInCacheBackedSectionEnd;
                }
            }
        }

        //
        // Acquire the image section lock to check for an existing mapping and
        // whether the page is dirty or clean.
        //

        KeAcquireQueuedLock(ImageSection->Lock);
        LockHeld = TRUE;
        Status = MmpCheckExistingMapping(ImageSection,
                                         PageOffset,
                                         LockPage,
                                         LockedPageCacheIoBuffer,
                                         &ExistingPhysicalAddress);

        if (KSUCCESS(Status)) {
            break;
        }

        if (Status == STATUS_TRY_AGAIN) {
            MmpImageSectionAddImageBackingReference(ImageSection);
            KeReleaseQueuedLock(ImageSection->Lock);
            LockHeld = FALSE;
            LockPageCacheEntry = TRUE;
            continue;
        }

        //
        // If the owning section is destroyed, there is no reason to read
        // anything from the page file or backing image. The entire section has
        // already been unmapped.
        //

        OwningSection = MmpGetOwningSection(ImageSection, PageOffset);
        if ((OwningSection->Flags & IMAGE_SECTION_DESTROYED) != 0) {

            //
            // A section isolates itself from all inheriting children before
            // being destroyed. So a destroyed owning section must be the
            // faulting section.
            //

            ASSERT(OwningSection == ImageSection);

            Status = STATUS_TOO_LATE;
            break;
        }

        //
        // If the page is dirty, read from the page file.
        //

        ASSERT(OwningSection->DirtyPageBitmap != NULL);

        if ((OwningSection->DirtyPageBitmap[BitmapIndex] & BitmapMask) != 0) {
            RootSection = MmpGetRootSection(OwningSection);
            Status = MmpPrepareForPageFileRead(RootSection,
                                               OwningSection,
                                               &Context);

            if (!KSUCCESS(Status)) {
                goto PageInCacheBackedSectionEnd;
            }

            //
            // If something in the context needs to be allocated, loop back.
            //

            if ((Context.Flags & PAGE_IN_CONTEXT_FLAG_ALLOCATE_MASK) != 0) {
                KeReleaseQueuedLock(ImageSection->Lock);
                MmpImageSectionReleaseReference(OwningSection);
                MmpImageSectionReleaseReference(RootSection);
                LockHeld = FALSE;
                OwningSection = NULL;
                RootSection = NULL;
                continue;
            }

            //
            // Read from the page file and break out of the loop if succesful.
            //

            Status = MmpReadPageFile(RootSection,
                                     OwningSection,
                                     PageOffset,
                                     &Context);

            if (!KSUCCESS(Status)) {
                goto PageInCacheBackedSectionEnd;
            }

            break;
        }

        //
        // The page is not dirty, increment the reference count on the image
        // backing handle while the lock is still held.
        //

        ASSERT(ImageSection->ImageBacking.DeviceHandle != INVALID_HANDLE);

        //
        // Also check to ensure the section covers the region.
        //

        if ((ImageSection->Size >> PageShift) <= PageOffset) {
            Status = STATUS_TRY_AGAIN;
            break;
        }

        MmpImageSectionAddImageBackingReference(ImageSection);

        //
        // Record the current truncation count for this image section and
        // release the lock.
        //

        TruncateCount = ImageSection->TruncateCount;
        KeReleaseQueuedLock(ImageSection->Lock);
        LockHeld = FALSE;
        OriginalOwner = OwningSection;
        OwningSection = NULL;

        //
        // Release the I/O buffer if it was previously used or initialize for
        // first time use.
        //

        if (IoBuffer != NULL) {
            MmResetIoBuffer(IoBuffer);

        } else {
            IoBuffer = &IoBufferData;
            Status = MmInitializeIoBuffer(IoBuffer,
                                          NULL,
                                          INVALID_PHYSICAL_ADDRESS,
                                          0,
                                          IO_BUFFER_FLAG_KERNEL_MODE_DATA);

            if (!KSUCCESS(Status)) {
                goto PageInCacheBackedSectionEnd;
            }
        }

        //
        // Read from the backing image at the faulting page's offset.
        //

        Status = MmpReadBackingImage(ImageSection, PageOffset, IoBuffer);
        MmpImageSectionReleaseImageBackingReference(ImageSection);
        if (!KSUCCESS(Status)) {

            ASSERT(MmGetIoBufferPageCacheEntry(IoBuffer, 0) == NULL);

            goto PageInCacheBackedSectionEnd;
        }

        //
        // Get the page cache entry and physical address that were just read.
        //

        ASSERT(IoBuffer->FragmentCount == 1);

        PageCacheEntry = MmGetIoBufferPageCacheEntry(IoBuffer, 0);
        PageCacheAddress = IoBuffer->Fragment[0].PhysicalAddress;

        ASSERT((PageCacheEntry == NULL) ||
               (PageCacheAddress ==
                IoGetPageCacheEntryPhysicalAddress(PageCacheEntry, NULL)));

        //
        // Store the page cache entry in the locked I/O buffer.
        //

        if (LockPage != FALSE) {
            if (LockedPageCacheIoBuffer != NULL) {
                MmResetIoBuffer(LockedPageCacheIoBuffer);

            } else {
                LockedPageCacheIoBuffer = &LockedPageCacheIoBufferData;
                Status = MmInitializeIoBuffer(LockedPageCacheIoBuffer,
                                              NULL,
                                              INVALID_PHYSICAL_ADDRESS,
                                              0,
                                              IO_BUFFER_FLAG_KERNEL_MODE_DATA);

                if (!KSUCCESS(Status)) {
                    goto PageInCacheBackedSectionEnd;
                }
            }

            if (PageCacheEntry != NULL) {
                MmIoBufferAppendPage(LockedPageCacheIoBuffer,
                                     PageCacheEntry,
                                     NULL,
                                     INVALID_PHYSICAL_ADDRESS);

            } else {
                Status = MmAppendIoBufferData(LockedPageCacheIoBuffer,
                                              VirtualAddress,
                                              PageCacheAddress,
                                              PageSize);

                if (!KSUCCESS(Status)) {
                    goto PageInCacheBackedSectionEnd;
                }
            }
        }

        //
        // Acquire the image section lock.
        //

        KeAcquireQueuedLock(ImageSection->Lock);
        LockHeld = TRUE;

        //
        // While the lock was released, another thread may have paged it in.
        //

        Status = MmpCheckExistingMapping(ImageSection,
                                         PageOffset,
                                         LockPage,
                                         LockedPageCacheIoBuffer,
                                         &ExistingPhysicalAddress);

        if (KSUCCESS(Status)) {
            break;
        }

        if (Status == STATUS_TRY_AGAIN) {
            MmpImageSectionAddImageBackingReference(ImageSection);
            KeReleaseQueuedLock(ImageSection->Lock);
            MmpImageSectionReleaseReference(OriginalOwner);
            LockHeld = FALSE;
            LockPageCacheEntry = TRUE;
            OriginalOwner = NULL;
            continue;
        }

        //
        // If the truncate count changed then the page cache entry read in may
        // have been evicted.
        //

        if (ImageSection->TruncateCount != TruncateCount) {
            KeReleaseQueuedLock(ImageSection->Lock);
            MmpImageSectionReleaseReference(OriginalOwner);
            LockHeld = FALSE;
            OriginalOwner = NULL;
            continue;
        }

        //
        // While the lock was released, the faulting image section's
        // inheritance landscape may have changed. If it did, then the page is
        // likely dirty and needs to be read from the page file.
        //

        OwningSection = MmpGetOwningSection(ImageSection, PageOffset);
        if (OwningSection != OriginalOwner) {
            KeReleaseQueuedLock(ImageSection->Lock);
            MmpImageSectionReleaseReference(OwningSection);
            MmpImageSectionReleaseReference(OriginalOwner);
            LockHeld = FALSE;
            OwningSection = NULL;
            OriginalOwner = NULL;
            continue;
        }

        ASSERT((OwningSection->DirtyPageBitmap == NULL) ||
               ((OwningSection->DirtyPageBitmap[BitmapIndex] & BitmapMask) ==
                0));

        ASSERT(Context.PhysicalAddress == INVALID_PHYSICAL_ADDRESS);

        Context.PhysicalAddress = PageCacheAddress;
        Status = STATUS_SUCCESS;
        break;
    }

    ASSERT(KSUCCESS(Status));

    //
    // If the image section got destroyed, fail rather than mapping the page
    // in.
    //

    if ((ImageSection->Flags & IMAGE_SECTION_DESTROYED) != 0) {
        Status = STATUS_TOO_LATE;
        goto PageInCacheBackedSectionEnd;
    }

    //
    // Also check to ensure the section covers the region.
    //

    if ((ImageSection->Size >> PageShift) <= PageOffset) {
        Status = STATUS_TRY_AGAIN;
        goto PageInCacheBackedSectionEnd;
    }

PageInCacheBackedSectionEnd:

    //
    // If successful, map the new page or lock down the existing address.
    //

    if (KSUCCESS(Status)) {

        ASSERT(LockHeld != FALSE);

        //
        // Handle the case where an existing mapping was found and it needs to
        // be locked.
        //

        if ((ExistingPhysicalAddress != INVALID_PHYSICAL_ADDRESS) &&
            (LockPage != FALSE)) {

            if (OwningSection == NULL) {
                OwningSection = MmpGetOwningSection(ImageSection, PageOffset);
            }

            //
            // If the owning section is dirty (i.e. it does not map a cached
            // page), then initialize the locked I/O buffer via physical
            // address, locking the address for non-paged sections.
            //

            DirtyPageBitmap = OwningSection->DirtyPageBitmap;
            if ((DirtyPageBitmap[BitmapIndex] & BitmapMask) != 0) {
                NonPaged = TRUE;
                if ((OwningSection->Flags & IMAGE_SECTION_NON_PAGED) == 0) {
                    NonPaged = FALSE;
                    Status = MmpLockPhysicalPages(ExistingPhysicalAddress, 1);
                }

                //
                // Initialize the I/O buffer with the locked page and transfer
                // ownership of the lock to the I/O buffer so that it is
                // unlocked when the I/O buffer is released.
                //

                if (KSUCCESS(Status)) {
                    IoBufferFlags = IO_BUFFER_FLAG_KERNEL_MODE_DATA |
                                    IO_BUFFER_FLAG_MEMORY_LOCKED;

                    Status = MmInitializeIoBuffer(LockedIoBuffer,
                                                  NULL,
                                                  ExistingPhysicalAddress,
                                                  PageSize,
                                                  IoBufferFlags);

                    if (KSUCCESS(Status) && (NonPaged == FALSE)) {
                        LockedIoBuffer->Internal.Flags |=
                                            IO_BUFFER_INTERNAL_FLAG_LOCK_OWNED;
                    }
                }

            //
            // Otherwise a local locked I/O buffer was initialized above when
            // the image section lock was not held. Copy it to the locked I/O
            // buffer supplied by the caller.
            //

            } else {

                ASSERT(PageCacheAddress == ExistingPhysicalAddress);
                ASSERT(LockedPageCacheIoBuffer != NULL);

                RtlCopyMemory(LockedIoBuffer,
                              LockedPageCacheIoBuffer,
                              sizeof(IO_BUFFER));

                LockedPageCacheIoBuffer = NULL;
            }

        //
        // Handle new mappings.
        //

        } else if (ExistingPhysicalAddress == INVALID_PHYSICAL_ADDRESS) {

            ASSERT(OwningSection != NULL);
            ASSERT(Context.PhysicalAddress != INVALID_PHYSICAL_ADDRESS);

            if (LockPage != FALSE) {
                if (Context.PhysicalAddress == PageCacheAddress) {

                    ASSERT(LockedPageCacheIoBuffer != NULL);

                    RtlCopyMemory(LockedIoBuffer,
                                  LockedPageCacheIoBuffer,
                                  sizeof(IO_BUFFER));

                    LockedPageCacheIoBuffer = NULL;

                } else {
                    IoBufferFlags = IO_BUFFER_FLAG_KERNEL_MODE_DATA |
                                    IO_BUFFER_FLAG_MEMORY_LOCKED;

                    Status = MmInitializeIoBuffer(LockedIoBuffer,
                                                  NULL,
                                                  Context.PhysicalAddress,
                                                  PageSize,
                                                  IoBufferFlags);

                    if (KSUCCESS(Status) && (Context.PagingEntry != NULL)) {
                        LockedIoBuffer->Internal.Flags |=
                                            IO_BUFFER_INTERNAL_FLAG_LOCK_OWNED;
                    }
                }
            }

            if (KSUCCESS(Status)) {

                //
                // A paging entry should only be set if the page was read from
                // the page file. Otherwise it is a page cache page and should
                // not have a paging entry. Truncate can cause a page to go
                // from dirty to clean, so a paging entry may be present even
                // though a page cache page is being mapped.
                //

                PagingEntry = NULL;
                if (Context.PhysicalAddress != PageCacheAddress) {
                    PagingEntry = Context.PagingEntry;
                    Context.PagingEntry = NULL;
                }

                MmpMapPageInSection(OwningSection,
                                    PageOffset,
                                    Context.PhysicalAddress,
                                    PagingEntry,
                                    LockPage);

                Context.PhysicalAddress = INVALID_PHYSICAL_ADDRESS;
            }
        }
    }

    //
    // Unlock the section if necessary and then release un-used resources.
    //

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(ImageSection->Lock);
    }

    if (OwningSection != NULL) {
        MmpImageSectionReleaseReference(OwningSection);
    }

    if (OriginalOwner != NULL) {
        MmpImageSectionReleaseReference(OriginalOwner);
    }

    if (RootSection != NULL) {
        MmpImageSectionReleaseReference(RootSection);
    }

    if (LockedPageCacheIoBuffer != NULL) {
        MmFreeIoBuffer(LockedPageCacheIoBuffer);
    }

    if (IoBuffer != NULL) {
        MmFreeIoBuffer(IoBuffer);
    }

    MmpDestroyPageInContext(&Context);
    return Status;
}

KSTATUS
MmpCheckExistingMapping (
    PIMAGE_SECTION Section,
    ULONG PageOffset,
    BOOL LockPage,
    PIO_BUFFER LockedIoBuffer,
    PPHYSICAL_ADDRESS ExistingPhysicalAddress
    )

/*++

Routine Description:

    This routine checks for an existing mapping in a page cache backed section.

Arguments:

    Section - Supplies a pointer to the faulting image section.

    PageOffset - Supplies the offset, in pages, from the beginning of the
        section.

    LockPage - Supplies a boolean indicating if the page should be locked.

    LockedIoBuffer - Supplies an optional pointer to an I/O buffer that
        contains any pages that have been locked for the mapping.

    ExistingPhysicalAddress - Supplies a pointer that receives the physical
        address used in the existing mapping if it exists.

Return Value:

    Returns STATUS_SUCCESS if there is an existing mapping and all necessary
    locking steps were taken.

    Returns STATUS_NOT_FOUND if there is no existing mapping.

    Returns STATUS_TRY_AGAIN if there is an existing mapping, but a lock
    request was made with the wrong previously locked address.

--*/

{

    UINTN BitmapIndex;
    ULONG BitmapMask;
    PHYSICAL_ADDRESS LockedPhysicalAddress;
    PIMAGE_SECTION OwningSection;
    ULONG PageShift;
    KSTATUS Status;
    PVOID VirtualAddress;

    ASSERT(KeIsQueuedLockHeld(Section->Lock) != FALSE);
    ASSERT((Section->Flags & IMAGE_SECTION_BACKED) != 0);

    OwningSection = NULL;
    PageShift = MmPageShift();
    VirtualAddress = Section->VirtualAddress + (PageOffset << PageShift);

    //
    // Check the mapping in case another processor has resolved the page
    // fault.
    //

    *ExistingPhysicalAddress = MmpVirtualToPhysical(VirtualAddress, NULL);
    if (*ExistingPhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
        Status = STATUS_NOT_FOUND;
        goto CheckExistingMappingEnd;
    }

    //
    // If there is no request to lock the page, then return successfully.
    //

    if (LockPage == FALSE) {
        Status = STATUS_SUCCESS;
        goto CheckExistingMappingEnd;
    }

    //
    // Get the physical page that has been locked for this mapping. The invalid
    // physical page indicates that no page has been locked.
    //

    LockedPhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    if (LockedIoBuffer != NULL) {
        LockedPhysicalAddress = MmGetIoBufferPhysicalAddress(LockedIoBuffer, 0);
    }

    //
    // Get the owning section to determine if the page comes from the page
    // cache or not.
    //

    OwningSection = MmpGetOwningSection(Section, PageOffset);

    ASSERT(OwningSection->DirtyPageBitmap != NULL);

    //
    // If the page is not taken from the page cache (i.e. it's dirty) or it
    // maps the already locked page, then exit successfully.
    //

    BitmapIndex = IMAGE_SECTION_BITMAP_INDEX(PageOffset);
    BitmapMask = IMAGE_SECTION_BITMAP_MASK(PageOffset);
    if (((OwningSection->DirtyPageBitmap[BitmapIndex] & BitmapMask) != 0) ||
        (*ExistingPhysicalAddress == LockedPhysicalAddress)) {

        Status = STATUS_SUCCESS;
        goto CheckExistingMappingEnd;
    }

    //
    // Otherwise there was an existing mapping, but the physical address was
    // not appropriately locked. The caller has to try again.
    //

    Status = STATUS_TRY_AGAIN;

CheckExistingMappingEnd:
    if (OwningSection != NULL) {
        MmpImageSectionReleaseReference(OwningSection);
    }

    return Status;
}

KSTATUS
MmpPageInDefaultSection (
    PIMAGE_SECTION ImageSection,
    UINTN PageOffset,
    PIO_BUFFER LockedIoBuffer
    )

/*++

Routine Description:

    This routine pages a physical page in from disk or allocates a new free
    physical page. This routine must be called at low level.

Arguments:

    ImageSection - Supplies a pointer to the image section within the process
        to page in.

    PageOffset - Supplies the offset, in pages, from the beginning of the
        section.

    LockedIoBuffer - Supplies an optional pointer to an uninitialized I/O
        buffer that will be initialized with the the paged in page, effectively
        locking the page until the I/O buffer is released.

Return Value:

    Status code.

--*/

{

    ULONG Attributes;
    UINTN BitmapIndex;
    ULONG BitmapMask;
    ULONG BufferOffset;
    PAGE_IN_CONTEXT Context;
    ULONG CopySize;
    BOOL Dirty;
    PHYSICAL_ADDRESS ExistingPhysicalAddress;
    PIO_BUFFER IoBuffer;
    ULONG IoBufferFlags;
    BOOL LockHeld;
    BOOL LockPage;
    RUNLEVEL OldRunLevel;
    PIMAGE_SECTION OriginalOwner;
    PIMAGE_SECTION OwningSection;
    ULONG PageShift;
    ULONG PageSize;
    PPROCESSOR_BLOCK ProcessorBlock;
    IO_OFFSET ReadOffset;
    PIMAGE_SECTION RootSection;
    KSTATUS Status;
    PVOID SwapSpace;
    PVOID VirtualAddress;

    RtlZeroMemory(&Context, sizeof(PAGE_IN_CONTEXT));

    ASSERT(Context.PhysicalAddress == INVALID_PHYSICAL_ADDRESS);

    ExistingPhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    IoBuffer = NULL;
    LockHeld = FALSE;
    OwningSection = NULL;
    PageShift = MmPageShift();
    PageSize = MmPageSize();
    RootSection = NULL;
    VirtualAddress = ImageSection->VirtualAddress + (PageOffset << PageShift);

    ASSERT((ImageSection->Flags & IMAGE_SECTION_BACKED) == 0);

    //
    // Loop trying to page into the section.
    //

    while (TRUE) {
        if ((Context.Flags & PAGE_IN_CONTEXT_FLAG_ALLOCATE_MASK) != 0) {
            Status = MmpAllocatePageInStructures(ImageSection, &Context);
            if (!KSUCCESS(Status)) {
                goto PageInDefaultSectionEnd;
            }
        }

        //
        // Acquire the image section lock to check for an existing mapping and
        // whether the page is dirty or clean.
        //

        KeAcquireQueuedLock(ImageSection->Lock);
        LockHeld = TRUE;
        ExistingPhysicalAddress = MmpVirtualToPhysical(VirtualAddress,
                                                       &Attributes);

        if (ExistingPhysicalAddress != INVALID_PHYSICAL_ADDRESS) {

            ASSERT((Attributes & MAP_FLAG_PRESENT) != 0);

            Status = STATUS_SUCCESS;
            break;
        }

        //
        // If the owning section is destroyed, there is no reason to read
        // anything from the page file or backing image. The entire section has
        // already been unmapped.
        //

        OwningSection = MmpGetOwningSection(ImageSection, PageOffset);
        if ((OwningSection->Flags & IMAGE_SECTION_DESTROYED) != 0) {

            //
            // A section isolates itself from all inheriting children before
            // being destroyed. So a destroyed owning section must be the
            // faulting section.
            //

            ASSERT(OwningSection == ImageSection);

            Status = STATUS_TOO_LATE;
            break;
        }

        //
        // Figure out if the page is clean or dirty by looking at the owning
        // section.
        //

        Dirty = FALSE;
        BitmapIndex = IMAGE_SECTION_BITMAP_INDEX(PageOffset);
        BitmapMask = IMAGE_SECTION_BITMAP_MASK(PageOffset);
        if ((OwningSection->DirtyPageBitmap != NULL) &&
            ((OwningSection->DirtyPageBitmap[BitmapIndex] & BitmapMask) != 0)) {

            Dirty = TRUE;
        }

        //
        // If the page is dirty, read from the page file.
        //

        if (Dirty != FALSE) {
            RootSection = MmpGetRootSection(OwningSection);
            Status = MmpPrepareForPageFileRead(RootSection,
                                               OwningSection,
                                               &Context);

            if (!KSUCCESS(Status)) {
                goto PageInDefaultSectionEnd;
            }

            //
            // If something in the context needs to be allocated, loop back.
            //

            if ((Context.Flags & PAGE_IN_CONTEXT_FLAG_ALLOCATE_MASK) != 0) {
                KeReleaseQueuedLock(ImageSection->Lock);
                MmpImageSectionReleaseReference(OwningSection);
                MmpImageSectionReleaseReference(RootSection);
                LockHeld = FALSE;
                OwningSection = NULL;
                RootSection = NULL;
                continue;
            }

            //
            // Read from the page file and break out of the loop if successful.
            //

            Status = MmpReadPageFile(RootSection,
                                     OwningSection,
                                     PageOffset,
                                     &Context);

            if (!KSUCCESS(Status)) {
                goto PageInDefaultSectionEnd;
            }

            break;
        }

        //
        // The page is not dirty. Take a reference on the image handle so it
        // can't be closed while the lock is released. Also check to ensure the
        // section covers the region.
        //

        if ((ImageSection->Size >> PageShift) <= PageOffset) {
            Status = STATUS_TRY_AGAIN;
            goto PageInDefaultSectionEnd;
        }

        //
        // Only the owning section should have a handle to the original backing
        // device.
        //

        ASSERT((OwningSection->Flags & IMAGE_SECTION_DESTROYED) == 0);
        ASSERT(OwningSection->ImageBacking.DeviceHandle != INVALID_HANDLE);

        MmpImageSectionAddImageBackingReference(OwningSection);

        //
        // Release the lock to perform a read from the file.
        //

        KeReleaseQueuedLock(ImageSection->Lock);
        LockHeld = FALSE;
        OriginalOwner = OwningSection;
        OwningSection = NULL;

        //
        // A physical page will be needed, so allocate it now.
        //

        if (Context.PhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
            Context.Flags |= PAGE_IN_CONTEXT_FLAG_ALLOCATE_PAGE;
            Status = MmpAllocatePageInStructures(ImageSection, &Context);
            if (!KSUCCESS(Status)) {
                MmpImageSectionReleaseImageBackingReference(OriginalOwner);
                goto PageInDefaultSectionEnd;
            }
        }

        //
        // Release the I/O buffer if it was previously used.
        //

        if (IoBuffer != NULL) {
            MmResetIoBuffer(IoBuffer);

        //
        // Otherwise allocate an uninitialized I/O buffer than can handle up to
        // 2 pages. Default sections are not backed by the page cache as a
        // result of not being cache-aligned. So, two pages may need to be read
        // from the cache to get the apppropriate data.
        //

        } else {
            IoBuffer = MmAllocateUninitializedIoBuffer(2 * PageSize, 0);
            if (IoBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto PageInDefaultSectionEnd;
            }
        }

        //
        // Read from the backing image at the faulting page's offset. This
        // routine will round down the offset and round up the read size in
        // order to make a cache-aligned read.
        //

        Status = MmpReadBackingImage(OriginalOwner, PageOffset, IoBuffer);
        MmpImageSectionReleaseImageBackingReference(OriginalOwner);
        if (!KSUCCESS(Status)) {
            goto PageInDefaultSectionEnd;
        }

        //
        // Map the I/O buffer before the lock is reacquired.
        //

        Status = MmMapIoBuffer(IoBuffer, FALSE, FALSE, FALSE);
        if (!KSUCCESS(Status)) {
            goto PageInDefaultSectionEnd;
        }

        //
        // Acquire the image section lock.
        //

        KeAcquireQueuedLock(ImageSection->Lock);
        LockHeld = TRUE;

        //
        // While the lock was released, another thread may have paged it in.
        //

        ExistingPhysicalAddress = MmpVirtualToPhysical(VirtualAddress,
                                                       &Attributes);

        if (ExistingPhysicalAddress != INVALID_PHYSICAL_ADDRESS) {

            ASSERT((Attributes & MAP_FLAG_PRESENT) != 0);

            Status = STATUS_SUCCESS;
            break;
        }

        //
        // While the lock was released, the faulting image section's
        // inheritance landscape may have changed. If it did, then the page is
        // likely dirty and needs to be read from the page file.
        //

        OwningSection = MmpGetOwningSection(ImageSection, PageOffset);

        ASSERT(OwningSection->Lock == ImageSection->Lock);

        if (OwningSection != OriginalOwner) {
            KeReleaseQueuedLock(ImageSection->Lock);
            MmpImageSectionReleaseReference(OwningSection);
            MmpImageSectionReleaseReference(OriginalOwner);
            LockHeld = FALSE;
            OwningSection = NULL;
            OriginalOwner = NULL;
            continue;
        }

        ASSERT((OwningSection->DirtyPageBitmap == NULL) ||
               ((OwningSection->DirtyPageBitmap[BitmapIndex] & BitmapMask) ==
                0));

        //
        // Copy the page from the I/O buffer into the allocated physical page.
        //

        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        ProcessorBlock = KeGetCurrentProcessorBlock();
        SwapSpace = ProcessorBlock->SwapPage;

        ASSERT(SwapSpace != NULL);
        ASSERT(Context.PhysicalAddress != INVALID_PHYSICAL_ADDRESS);

        MmpMapPage(Context.PhysicalAddress,
                   SwapSpace,
                   MAP_FLAG_PRESENT | MAP_FLAG_GLOBAL);

        ASSERT(IoBuffer->FragmentCount <= 2);

        //
        // If the I/O buffer was broken into two fragments, then copy
        // the contents of the second fragment to the latter half of
        // the temporary mapping.
        //

        ASSERT(IoGetCacheEntryDataSize() == PageSize);

        ReadOffset = OwningSection->ImageBacking.Offset +
                     (PageOffset << PageShift);

        BufferOffset = REMAINDER(ReadOffset, PageSize);
        if (IoBuffer->FragmentCount == 2) {

            ASSERT(BufferOffset != 0);

            CopySize = PageSize - BufferOffset;
            RtlCopyMemory(SwapSpace + CopySize,
                          IoBuffer->Fragment[1].VirtualAddress,
                          BufferOffset);

        } else {

            ASSERT(IoBuffer->FragmentCount == 1);
            ASSERT(IoBuffer->Fragment[0].Size >= PageSize);

            CopySize = PageSize;
        }

        //
        // Always copy some amount of contents from the first fragment.
        //

        RtlCopyMemory(SwapSpace,
                      IoBuffer->Fragment[0].VirtualAddress + BufferOffset,
                      CopySize);

        //
        // Unmap the page from the temporary space.
        //

        if ((OwningSection->Flags & IMAGE_SECTION_EXECUTABLE) != 0) {

            ASSERT(CopySize <= PageSize);

            MmpSyncSwapPage(SwapSpace, PageSize);
        }

        MmpUnmapPages(SwapSpace, 1, 0, NULL);
        KeLowerRunLevel(OldRunLevel);
        break;
    }

    ASSERT(KSUCCESS(Status));

    //
    // If the image section got destroyed, fail rather than mapping the page
    // in.
    //

    if ((ImageSection->Flags & IMAGE_SECTION_DESTROYED) != 0) {
        Status = STATUS_TOO_LATE;
        goto PageInDefaultSectionEnd;
    }

    //
    // Also check to ensure the section covers the region.
    //

    if ((ImageSection->Size >> PageShift) <= PageOffset) {
        Status = STATUS_TRY_AGAIN;
        goto PageInDefaultSectionEnd;
    }

PageInDefaultSectionEnd:

    //
    // If successful, map the new page or potentially lock the existing page.
    //

    if (KSUCCESS(Status)) {

        ASSERT(LockHeld != FALSE);

        //
        // Handle the case where an existing mapping was found and it needs to
        // be locked.
        //

        if ((ExistingPhysicalAddress != INVALID_PHYSICAL_ADDRESS) &&
            (LockedIoBuffer != NULL)) {

            //
            // Only lock paged sections. Non-paged sections always remain
            // pinned.
            //

            if ((ImageSection->Flags & IMAGE_SECTION_NON_PAGED) == 0) {
                Status = MmpLockPhysicalPages(ExistingPhysicalAddress, 1);
            }

            //
            // Initialize the I/O buffer with the locked page and transfer
            // ownership of the lock to the I/O buffer so that it is unlocked
            // when the buffer is freed.
            //

            if (KSUCCESS(Status)) {
                IoBufferFlags = IO_BUFFER_FLAG_KERNEL_MODE_DATA |
                                IO_BUFFER_FLAG_MEMORY_LOCKED;

                Status = MmInitializeIoBuffer(LockedIoBuffer,
                                              NULL,
                                              ExistingPhysicalAddress,
                                              PageSize,
                                              IoBufferFlags);

                if (KSUCCESS(Status) &&
                    ((ImageSection->Flags & IMAGE_SECTION_NON_PAGED) == 0)) {

                    LockedIoBuffer->Internal.Flags |=
                                            IO_BUFFER_INTERNAL_FLAG_LOCK_OWNED;
                }
            }

        //
        // Handle new mappings.
        //

        } else if (ExistingPhysicalAddress == INVALID_PHYSICAL_ADDRESS) {

            ASSERT(Context.PhysicalAddress != INVALID_PHYSICAL_ADDRESS);
            ASSERT(OwningSection != NULL);
            ASSERT(KeIsQueuedLockHeld(OwningSection->Lock) != FALSE);

            LockPage = FALSE;
            if (LockedIoBuffer != NULL) {
                LockPage = TRUE;
                IoBufferFlags = IO_BUFFER_FLAG_KERNEL_MODE_DATA |
                                IO_BUFFER_FLAG_MEMORY_LOCKED;

                //
                // Initialize the I/O buffer with the soon-to-be locked page
                // and transfer ownership of the lock to the I/O buffer so that
                // it is unlocked when the buffer is freed.
                //

                Status = MmInitializeIoBuffer(LockedIoBuffer,
                                              NULL,
                                              Context.PhysicalAddress,
                                              PageSize,
                                              IoBufferFlags);

                if (KSUCCESS(Status) && (Context.PagingEntry != NULL)) {
                    LockedIoBuffer->Internal.Flags |=
                                            IO_BUFFER_INTERNAL_FLAG_LOCK_OWNED;
                }
            }

            if (KSUCCESS(Status)) {
                MmpMapPageInSection(OwningSection,
                                    PageOffset,
                                    Context.PhysicalAddress,
                                    Context.PagingEntry,
                                    LockPage);

                Context.PagingEntry = NULL;
                Context.PhysicalAddress = INVALID_PHYSICAL_ADDRESS;
            }
        }
    }

    //
    // Unlock the section if necessary and then release un-used resources.
    //

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(ImageSection->Lock);
    }

    if (OwningSection != NULL) {
        MmpImageSectionReleaseReference(OwningSection);
    }

    if (OriginalOwner != NULL) {
        MmpImageSectionReleaseReference(OriginalOwner);
    }

    if (RootSection != NULL) {
        MmpImageSectionReleaseReference(RootSection);
    }

    MmpDestroyPageInContext(&Context);
    if (IoBuffer != NULL) {
        MmFreeIoBuffer(IoBuffer);
    }

    return Status;
}

KSTATUS
MmpPrepareForPageFileRead (
    PIMAGE_SECTION RootSection,
    PIMAGE_SECTION OwningSection,
    PPAGE_IN_CONTEXT Context
    )

/*++

Routine Description:

    This routine prepares for a read from the page file. It make sure that the
    given context is suitable for the read.

Arguments:

    RootSection - Supplies a pointer to the root of the image section tree.

    OwningSection - Supplies a pointer to the section that owns the page file
        that will be read.

    Context - Supplies a pointer to the page in context that will be used for
        the read.

Return Value:

    Status code.

--*/

{

    PPAGE_FILE PageFile;
    HANDLE PageFileHandle;
    KSTATUS Status;

    //
    // A physical page will need to be allocated for the read.
    //

    Context->Flags &= ~PAGE_IN_CONTEXT_FLAG_ALLOCATE_PAGE;
    if (Context->PhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
        Context->Flags |= PAGE_IN_CONTEXT_FLAG_ALLOCATE_PAGE;
    }

    //
    // An IRP for the page file's device is required to do the read. The owning
    // section IRP's can be used if present. The root section's IRP can be used
    // if the owning section does not have an IRP and the root section's paging
    // device is the same as the owning section's paging device.
    //

    Context->Flags &= ~PAGE_IN_CONTEXT_FLAG_ALLOCATE_IRP;
    if ((OwningSection->PagingInIrp == NULL) &&
        ((RootSection->PagingInIrp == NULL) ||
         (OwningSection->PageFileBacking.DeviceHandle !=
          RootSection->PageFileBacking.DeviceHandle))) {

        PageFileHandle = OwningSection->PageFileBacking.DeviceHandle;
        PageFile = (PPAGE_FILE)PageFileHandle;

        ASSERT(Context->IrpDevice == NULL);

        Status = IoGetDevice(PageFile->Handle, &(Context->IrpDevice));
        if (!KSUCCESS(Status)) {
            goto PrepareForPageFileReadEnd;
        }

        ObAddReference(Context->IrpDevice);
        if ((Context->Irp == NULL) ||
            (Context->Irp->Device != Context->IrpDevice)) {

            Context->Flags |= PAGE_IN_CONTEXT_FLAG_ALLOCATE_IRP;
        }
    }

    //
    // A memory reservation is required for temporarily mapping the new page
    // during page file reads. This is shared by the whole image section tree.
    //

    Context->Flags &= ~PAGE_IN_CONTEXT_FLAG_ALLOCATE_SWAP_SPACE;
    if ((RootSection->SwapSpace == NULL) && (Context->SwapSpace == NULL)) {
        Context->Flags |= PAGE_IN_CONTEXT_FLAG_ALLOCATE_SWAP_SPACE;
    }

    Status = STATUS_SUCCESS;

PrepareForPageFileReadEnd:
    return Status;
}

KSTATUS
MmpReadPageFile (
    PIMAGE_SECTION RootSection,
    PIMAGE_SECTION OwningSection,
    ULONG PageOffset,
    PPAGE_IN_CONTEXT Context
    )

/*++

Routine Description:

    This routine reads in from the image section's page file at the given page
    offset. The page file's contents are read into the supplied physical
    address which will be temporarily mapped by this routine.

Arguments:

    RootSection - Supplies a pointer to the root of the image section tree.

    OwningSection - Supplies a pointer to the section whose page file will be
        read.

    PageOffset - Supplies the offset, in pages, from the beginning of the
        section.

    Context - Supplies a pointer to the page in context that will be used for
        the read.

Return Value:

    Status code.

--*/

{

    PIO_BUFFER IoBuffer;
    IO_BUFFER IoBufferData;
    ULONG IoBufferFlags;
    PAGE_FILE_IO_CONTEXT IoContext;
    PIRP Irp;
    ULONG PageShift;
    ULONG PageSize;
    KSTATUS Status;
    PVOID SwapSpace;

    ASSERT(KeIsQueuedLockHeld(OwningSection->Lock) != FALSE);
    ASSERT((OwningSection->Flags & IMAGE_SECTION_NON_PAGED) == 0);
    ASSERT((RootSection->SwapSpace != NULL) || (Context->SwapSpace != NULL));
    ASSERT(OwningSection->PageFileBacking.DeviceHandle != INVALID_HANDLE);
    ASSERT(Context->PhysicalAddress != INVALID_PHYSICAL_ADDRESS);

    PageShift = MmPageShift();
    PageSize = MmPageSize();

    //
    // Determine which IRP to use. Prefer the owning section's and then the
    // root section's if it uses the same paging device. If neither have an
    // IRP, then use the IRP in the context and transfer ownership of the
    // context's IRP to one of the image sections, preferring the root section.
    //

    Irp = OwningSection->PagingInIrp;
    if (Irp == NULL) {
        Irp = RootSection->PagingInIrp;
        if ((Irp == NULL) ||
            (OwningSection->PageFileBacking.DeviceHandle !=
             RootSection->PageFileBacking.DeviceHandle)) {

            Irp = Context->Irp;
            if (OwningSection->PageFileBacking.DeviceHandle !=
                RootSection->PageFileBacking.DeviceHandle) {

                OwningSection->PagingInIrp = Context->Irp;

            } else {
                RootSection->PagingInIrp = Context->Irp;
            }

            Context->Irp = NULL;
        }
    }

    ASSERT(Irp != NULL);

    //
    // Set the swap space in the root image section if this is the first time
    // paging into this image section tree.
    //

    if (RootSection->SwapSpace == NULL) {
        RootSection->SwapSpace = Context->SwapSpace;
        Context->SwapSpace = NULL;
    }

    //
    // Reading from the page file does not go through the page cache. A buffer
    // must be supplied. Map the allocated physical page to the temporary swap
    // space VA. The section lock must be held for the duration of the read.
    //

    ASSERT(RootSection->SwapSpace->VirtualBase != NULL);

    SwapSpace = RootSection->SwapSpace->VirtualBase;
    MmpMapPage(Context->PhysicalAddress,
               SwapSpace,
               MAP_FLAG_PRESENT | MAP_FLAG_GLOBAL);

    IoBuffer = &IoBufferData;
    IoBufferFlags = IO_BUFFER_FLAG_KERNEL_MODE_DATA |
                    IO_BUFFER_FLAG_MEMORY_LOCKED;

    Status = MmInitializeIoBuffer(IoBuffer,
                                  SwapSpace,
                                  Context->PhysicalAddress,
                                  PageSize,
                                  IoBufferFlags);

    if (!KSUCCESS(Status)) {
        goto ReadPageFileEnd;
    }

    //
    // Read the page in from the backing store of the owning section. Note that
    // the root section may page in from a different file and device.
    //

    IoContext.Offset = PageOffset << PageShift;
    IoContext.IoBuffer = IoBuffer;
    IoContext.Irp = Irp;
    IoContext.SizeInBytes = PageSize;
    IoContext.BytesCompleted = 0;
    IoContext.Flags = IO_FLAG_SERVICING_FAULT;
    IoContext.TimeoutInMilliseconds = WAIT_TIME_INDEFINITE;
    IoContext.Write = FALSE;
    Status = MmpPageFilePerformIo(&(OwningSection->PageFileBacking),
                                  &IoContext);

    //
    // A successful read should have read the full page and reads from the
    // page file should not go beyond the end of the file.
    //

    ASSERT(!KSUCCESS(Status) || (IoContext.BytesCompleted == PageSize));
    ASSERT(Status != STATUS_END_OF_FILE);

    //
    // Unmap the page from the temporary space.
    //

    if ((OwningSection->Flags & IMAGE_SECTION_EXECUTABLE) != 0) {
        MmpSyncSwapPage(SwapSpace, PageSize);
    }

ReadPageFileEnd:
    MmpUnmapPages(SwapSpace, 1, UNMAP_FLAG_SEND_INVALIDATE_IPI, NULL);
    return Status;
}

KSTATUS
MmpPageFilePerformIo (
    PIMAGE_BACKING ImageBacking,
    PPAGE_FILE_IO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine performs I/O on a page file.

Arguments:

    ImageBacking - Supplies a pointer to the image backing that contains a
        handle and offset for the page file.

    IoContext - Supplies a pointer to the page file I/O context.

Return Value:

    Status code.

--*/

{

    PDEVICE Device;
    PIRP Irp;
    PPAGE_FILE PageFile;
    KSTATUS Status;

    PageFile = (PPAGE_FILE)ImageBacking->DeviceHandle;
    IoContext->Offset = ImageBacking->Offset + IoContext->Offset;

    ASSERT(IS_ALIGNED(IoContext->SizeInBytes, MmPageSize()) != FALSE);
    ASSERT(IS_ALIGNED(IoContext->Offset, MmPageSize()) != FALSE);

    //
    // All page file writes must be serialized. If the file system's block size
    // is greater than a page, it may perform a read-modify-write operation. If
    // multiple read-modify-write operations were not synchronized, the page
    // file could be corrupted.
    //

    if (IoContext->Write != FALSE) {
        KeAcquireQueuedLock(PageFile->Lock);
        Status = IoWriteAtOffset(PageFile->Handle,
                                 IoContext->IoBuffer,
                                 IoContext->Offset,
                                 IoContext->SizeInBytes,
                                 IoContext->Flags | IO_FLAG_NO_ALLOCATE,
                                 IoContext->TimeoutInMilliseconds,
                                 &(IoContext->BytesCompleted),
                                 PageFile->PagingOutIrp);

        KeReleaseQueuedLock(PageFile->Lock);

    } else {
        Irp = IoContext->Irp;
        if (Irp == NULL) {
            Status = IoGetDevice(PageFile->Handle, &Device);
            if (!KSUCCESS(Status)) {
                goto PageFilePerformIoEnd;
            }

            Irp = IoCreateIrp(Device, IrpMajorIo, IRP_CREATE_FLAG_NO_ALLOCATE);
            if (Irp == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto PageFilePerformIoEnd;
            }
        }

        Status = IoReadAtOffset(PageFile->Handle,
                                IoContext->IoBuffer,
                                IoContext->Offset,
                                IoContext->SizeInBytes,
                                IoContext->Flags | IO_FLAG_NO_ALLOCATE,
                                IoContext->TimeoutInMilliseconds,
                                &(IoContext->BytesCompleted),
                                Irp);

        if (Irp != IoContext->Irp) {
            IoDestroyIrp(Irp);
        }
    }

PageFilePerformIoEnd:
    return Status;
}

KSTATUS
MmpReadBackingImage (
    PIMAGE_SECTION Section,
    UINTN PageOffset,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine reads from the given image section's backing image at the
    specified page offset. If the resulting file offset is not page-aligned,
    then this routine will read the two aligned pages that contain the
    requested unaligned page.

Arguments:

    Section - Supplies a pointer to an image section.

    PageOffset - Supplies the offset, in pages, of the page that is to be read.

    IoBuffer - Supplies a pointer to an I/O buffer that will receive the data
        read from the backing image.

Return Value:

    Status code.

--*/

{

    UINTN BytesRead;
    ULONG PageShift;
    ULONG PageSize;
    IO_OFFSET ReadOffset;
    UINTN ReadSize;
    KSTATUS Status;

    ASSERT(Section->ImageBacking.DeviceHandle != INVALID_HANDLE);

    PageShift = MmPageShift();
    PageSize = MmPageSize();
    ReadOffset = Section->ImageBacking.Offset + (PageOffset << PageShift);

    //
    // If the image section is not directly backed by the page cache, then
    // round down the offset and read two pages that are cache-aligned.
    //

    ASSERT(IoGetCacheEntryDataSize() == PageSize);

    if (((Section->Flags & IMAGE_SECTION_BACKED) == 0) &&
        (IS_ALIGNED(ReadOffset, PageSize) == FALSE)) {

        ReadSize = 2 << PageShift;
        ReadOffset = ALIGN_RANGE_DOWN(ReadOffset, PageSize);

    } else {
        ReadSize = PageSize;
    }

    //
    // Read from the backing image.
    //

    Status = IoReadAtOffset(Section->ImageBacking.DeviceHandle,
                            IoBuffer,
                            ReadOffset,
                            ReadSize,
                            IO_FLAG_SERVICING_FAULT,
                            WAIT_TIME_INDEFINITE,
                            &BytesRead,
                            NULL);

    return Status;
}

VOID
MmpMapPageInSection (
    PIMAGE_SECTION OwningSection,
    UINTN PageOffset,
    PHYSICAL_ADDRESS PhysicalAddress,
    PPAGING_ENTRY PagingEntry,
    BOOL LockPage
    )

/*++

Routine Description:

    This routine maps the given physical address within the specified owning
    section at the virtual address determined by the page offset. If a paging
    entry is supplied it will make the physical page pageable.

Arguments:

    OwningSection - Supplies a pointer to the parent section that owns this
        page. The page will be mapped in this section and all children who
        inherit this page from the owning section.

    PageOffset - Supplies the offset in pages from the beginning of the section
        where this page belongs.

    PhysicalAddress - Supplies the physical address of the freshly initialized
        page.

    PagingEntry - Supplies an optional pointer to a paging entry to be used if
        to make the physical page pageable.

    LockPage - Supplies a boolean indicating whether or not the page should be
        locked.

Return Value:

    None.

--*/

{

    ASSERT(PhysicalAddress != INVALID_PHYSICAL_ADDRESS);

    //
    // Map the page in the owning section and all its inheriting children.
    //

    MmpModifySectionMapping(OwningSection,
                            PageOffset,
                            PhysicalAddress,
                            TRUE,
                            NULL,
                            FALSE);

    //
    // If a paging entry was supplied, then mark the page as pageable,
    // potentially locking it at the same time. There is no need to lock
    // non-paged sections. The supplied physical address is currently non-paged.
    //

    if (PagingEntry != NULL) {

        ASSERT((OwningSection->Flags & IMAGE_SECTION_NON_PAGED) == 0);
        ASSERT((OwningSection->Flags & IMAGE_SECTION_DESTROYED) == 0);

        MmpInitializePagingEntry(PagingEntry, OwningSection, PageOffset);
        MmpEnablePagingOnPhysicalAddress(PhysicalAddress,
                                         1,
                                         &PagingEntry,
                                         LockPage);
    }

    return;
}

KSTATUS
MmpAllocatePageInStructures (
    PIMAGE_SECTION Section,
    PPAGE_IN_CONTEXT Context
    )

/*++

Routine Description:

    This routine allocates the structures necessary to page in from the page
    file.

Arguments:

    Section - Supplies a pointer to the image section being paged into.

    Context - Supplies a pointer to the page in context used to store the
        allocated structures.

Return Value:

    Status code.

--*/

{

    ULONG PageSize;
    KSTATUS Status;

    //
    // If necessary, allocate a physical page. The page will be marked as
    // non-paged. This should only happend once.
    //

    if ((Context->Flags & PAGE_IN_CONTEXT_FLAG_ALLOCATE_PAGE) != 0) {

        ASSERT(Context->PhysicalAddress == INVALID_PHYSICAL_ADDRESS);
        ASSERT(Context->PagingEntry == NULL);

        Context->PhysicalAddress = MmpAllocatePhysicalPages(1, 1);
        if (Context->PhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
            Status = STATUS_NO_MEMORY;
            goto AllocatePageInStructuresEnd;
        }

        //
        // If this page is going to become pagable, create a paging entry
        // for it. Do not supply an image section, as the owning section
        // may change by the time the page gets mapped.
        //

        if ((Section->Flags & IMAGE_SECTION_NON_PAGED) == 0) {
            Context->PagingEntry = MmpCreatePagingEntry(NULL, 0);
            if (Context->PagingEntry == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto AllocatePageInStructuresEnd;
            }
        }
    }

    //
    // Create an IRP for paging in if requested.
    //

    if ((Context->Flags & PAGE_IN_CONTEXT_FLAG_ALLOCATE_IRP) != 0) {

        //
        // If a paging IRP already exists then it was for the wrong device.
        //

        ASSERT(Context->IrpDevice != NULL);
        ASSERT((Context->Irp == NULL) ||
               (Context->Irp->Device != Context->IrpDevice));

        if (Context->Irp != NULL) {
            IoDestroyIrp(Context->Irp);
        }

        Context->Irp = IoCreateIrp(Context->IrpDevice,
                                   IrpMajorIo,
                                   IRP_CREATE_FLAG_NO_ALLOCATE);

        if (Context->Irp == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AllocatePageInStructuresEnd;
        }

        ObReleaseReference(Context->IrpDevice);
        Context->IrpDevice = NULL;
    }

    //
    // Allocate swap space for the section to do paging operations.
    //

    if ((Context->Flags & PAGE_IN_CONTEXT_FLAG_ALLOCATE_SWAP_SPACE) != 0) {
        PageSize = MmPageSize();
        Context->SwapSpace = MmCreateMemoryReservation(
                                                  NULL,
                                                  PageSize,
                                                  0,
                                                  MAX_ADDRESS,
                                                  AllocationStrategyAnyAddress,
                                                  TRUE);

        if (Context->SwapSpace == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AllocatePageInStructuresEnd;
        }

        //
        // Make sure the leaf page table is in place for this virtual address
        // space. Otherwise, during page in, a physical page may need to be
        // allocated, which could cause a deadlock with the page out thread in
        // low memory scenarios.
        //

        MmpCreatePageTables(Context->SwapSpace->VirtualBase,
                            Context->SwapSpace->Size);
    }

    Status = STATUS_SUCCESS;

AllocatePageInStructuresEnd:
    return Status;
}

VOID
MmpDestroyPageInContext (
    PPAGE_IN_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys the given page in context by releasing all of its
    resources. It does not release the context itself.

Arguments:

    Context - Supplies a pointer to the page in context whose resources are to
        be released.

Return Value:

    None.

--*/

{

    if (Context->Irp != NULL) {
        IoDestroyIrp(Context->Irp);
    }

    if (Context->IrpDevice != NULL) {
        ObReleaseReference(Context->IrpDevice);
    }

    if (Context->PhysicalAddress != INVALID_PHYSICAL_ADDRESS) {
        MmFreePhysicalPage(Context->PhysicalAddress);
    }

    if (Context->PagingEntry != NULL) {
        MmpDestroyPagingEntry(Context->PagingEntry);
    }

    if (Context->SwapSpace != NULL) {
        MmFreeMemoryReservation(Context->SwapSpace);
    }

    return;
}

BOOL
MmpCanWriteToSection (
    PIMAGE_SECTION OwningSection,
    PIMAGE_SECTION Section,
    UINTN PageOffset
    )

/*++

Routine Description:

    This routine determines whether or not the given page within the supplied
    section can be mapped writable.

Arguments:

    OwningSection - Supplies a pointer to the image section that owns the page.

    Section - Supplies a pointer to the image section that is trying to map the
        page.

    PageOffset - Supplies the offset in pages from the beginning of the section
        where this page belongs.

Return Value:

    Returns TRUE if the page can be mapped writable or FALSE otherwise.

--*/

{

    UINTN BitmapIndex;
    ULONG BitmapMask;
    BOOL CanWrite;
    PIMAGE_SECTION Child;
    PLIST_ENTRY ChildEntry;
    ULONG Flags;

    //
    // If the image section is read-only then the page should never be writable.
    //

    Flags = Section->Flags;
    if ((Flags & IMAGE_SECTION_WRITABLE) == 0) {
        CanWrite = FALSE;

    //
    // Otherwise, if the image section is to be mapped shared, then the page
    // should always be mapped writable.
    //

    } else if ((Flags & IMAGE_SECTION_SHARED) != 0) {
        CanWrite = TRUE;

    //
    // Otherwise if the given section equals the owning section, special rules
    // apply that may allow the page to be mapped writable.
    //

    } else if (Section == OwningSection) {
        BitmapIndex = IMAGE_SECTION_BITMAP_INDEX(PageOffset);
        BitmapMask = IMAGE_SECTION_BITMAP_MASK(PageOffset);
        CanWrite = TRUE;

        //
        // If this is in the owning section, but it inherits from the page
        // cache then the page cannot be marked writable.
        //

        if (((Flags & IMAGE_SECTION_BACKED) != 0) &&
            ((Section->DirtyPageBitmap[BitmapIndex] & BitmapMask) == 0)) {

            CanWrite = FALSE;

        //
        // Otherwise the page can be written to unless there are any children
        // inheriting from it. This case optimizes for the common case of one
        // parent and one child who no longer inherits from the parent.
        //

        } else {
            ChildEntry = Section->ChildList.Next;
            while (ChildEntry != &(Section->ChildList)) {
                Child = LIST_VALUE(ChildEntry, IMAGE_SECTION, CopyListEntry);
                ChildEntry = ChildEntry->Next;
                if ((Child->InheritPageBitmap[BitmapIndex] & BitmapMask) != 0) {
                    CanWrite = FALSE;
                    break;
                }
            }
        }

    //
    // Otherwise the page is shared with another section and writable.
    // Copy-on-write must be triggered. Map it read-only.
    //

    } else {
        CanWrite = FALSE;
    }

    return CanWrite;
}

