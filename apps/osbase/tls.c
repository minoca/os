/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tls.c

Abstract:

    This module handles thread-local storage support for the base Minoca OS
    library.

Author:

    Evan Green 16-Apr-2015

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "osbasep.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used by TLS regions: mTLS.
//

#define TLS_ALLOCATION_TAG 0x534C546D

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PTHREAD_CONTROL_BLOCK
OspGetThreadControlBlock (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a list entry of active thread control structures.
//

LIST_ENTRY OsThreadList;
OS_LOCK OsThreadListLock;

//
// ------------------------------------------------------------------ Functions
//

OS_API
PVOID
OsGetTlsAddress (
    PTLS_INDEX Entry
    )

/*++

Routine Description:

    This routine returns the address of the given thread local storage symbol.
    This routine supports a C library call, references to which are emitted
    directly by the compiler.

Arguments:

    Entry - Supplies a pointer to the TLS entry to get the symbol address for.

Return Value:

    Returns a pointer to the thread local storage symbol on success.

--*/

{

    UINTN Alignment;
    PVOID *Allocation;
    UINTN AllocationSize;
    UINTN AvailableSize;
    PLIST_ENTRY CurrentEntry;
    PVOID *DynamicThreadVector;
    UINTN Generation;
    PLOADED_IMAGE Image;
    UINTN ModuleId;
    UINTN NeededSize;
    PVOID *NewVector;
    UINTN Offset;
    PVOID *Region;
    PTHREAD_CONTROL_BLOCK ThreadControlBlock;
    UINTN ThreadGeneration;

    ModuleId = Entry->Module;
    Offset = Entry->Offset;
    ThreadControlBlock = OspGetThreadControlBlock();
    DynamicThreadVector = ThreadControlBlock->TlsVector;

    //
    // Reallocate the TLS vector if it's behind and doesn't even have this
    // module.
    //

    ThreadGeneration = (UINTN)(DynamicThreadVector[0]);
    if (ThreadGeneration < Entry->Module) {
        Generation = OsImModuleGeneration;

        ASSERT((ModuleId != 0) && (Generation >= ModuleId));

        NeededSize = (Generation + 1) * sizeof(PVOID);

        //
        // If the TLS vector is already not part of the initial allocation,
        // just reallocate it from the heap.
        //

        if (ThreadControlBlock->TlsVector !=
            (PVOID *)(ThreadControlBlock + 1)) {

            NewVector = OsHeapReallocate(DynamicThreadVector,
                                         NeededSize,
                                         TLS_ALLOCATION_TAG);

            if (NewVector == NULL) {
                return NULL;
            }

            RtlZeroMemory(&(NewVector[ThreadGeneration]),
                          NeededSize - (ThreadGeneration * sizeof(PVOID)));

        //
        // The TLS vector is part of the initial allocation. See if there's
        // still room to grow in that initial allocation. There probably is.
        //

        } else {
            AvailableSize = (ThreadControlBlock->BaseAllocation +
                             ThreadControlBlock->BaseAllocationSize) -
                            (PVOID)(ThreadControlBlock + 1);

            //
            // If there is enough space, just continue using the same vector.
            // If there's not, allocate it from the heap.
            //

            if (AvailableSize >= NeededSize) {
                NewVector = ThreadControlBlock->TlsVector;

            } else {
                NewVector = OsHeapAllocate(NeededSize, TLS_ALLOCATION_TAG);
                if (NewVector == NULL) {
                    return NULL;
                }

                RtlCopyMemory(NewVector,
                              ThreadControlBlock->TlsVector,
                              ThreadGeneration * sizeof(PVOID));

                RtlZeroMemory(&(NewVector[ThreadGeneration]),
                              NeededSize - (ThreadGeneration * sizeof(PVOID)));
            }
        }

        NewVector[0] = (PVOID)Generation;
        ThreadControlBlock->TlsVector = NewVector;
        ThreadControlBlock->ModuleCount = Generation + 1;
        DynamicThreadVector = NewVector;
    }

    //
    // Go initialize the TLS section if this is the first time the module has
    // accessed TLS data on this thread.
    //

    DynamicThreadVector += ModuleId;
    if (*DynamicThreadVector == NULL) {

        //
        // Go find the module.
        //

        Image = NULL;
        OspAcquireImageLock(FALSE);
        CurrentEntry = OsLoadedImagesHead.Next;
        while (CurrentEntry != &OsLoadedImagesHead) {
            Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
            if (Image->ModuleNumber == ModuleId) {
                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        ASSERT(CurrentEntry != &OsLoadedImagesHead);
        ASSERT(Offset < Image->TlsSize);

        //
        // Allocate enough space to align the region, and store the original
        // allocation pointer of the region.
        //

        Alignment = Image->TlsAlignment;
        AllocationSize = sizeof(PVOID) + Image->TlsSize + Alignment;
        Allocation = OsHeapAllocate(AllocationSize, TLS_ALLOCATION_TAG);
        if (Allocation != NULL) {

            //
            // Add space for the allocation pointer, then align up to the
            // required alignment. Set the actual address of the allocation
            // just before what gets set in the vector.
            //

            if (Alignment <= 1) {
                Region = Allocation + sizeof(PVOID);

            } else {
                Region = (PVOID)(UINTN)ALIGN_RANGE_UP(
                                             (UINTN)Allocation + sizeof(PVOID),
                                             Alignment);
            }

            *(Region - 1) = Allocation;
            *DynamicThreadVector = Region;
            RtlCopyMemory(Region, Image->TlsImage, Image->TlsImageSize);
            RtlZeroMemory((PVOID)Region + Image->TlsImageSize,
                          Image->TlsSize - Image->TlsImageSize);
        }

        OspReleaseImageLock();
    }

    return *DynamicThreadVector + Offset;
}

OS_API
UINTN
OsGetThreadId (
    VOID
    )

/*++

Routine Description:

    This routine returns the currently running thread's identifier.

Arguments:

    None.

Return Value:

    Returns the current thread's ID. This number will be unique to the current
    thread as long as the thread is running.

--*/

{

    //
    // For now just return the pointer to the thread control block as a unique
    // number.
    //

    return (UINTN)OspGetThreadControlBlock();
}

OS_API
KSTATUS
OsSetThreadPointer (
    PVOID Pointer
    )

/*++

Routine Description:

    This routine sets the thread control pointer, which points to the thread
    control block. This function should only be called by the C library, not by
    user applications.

Arguments:

    Pointer - Supplies a pointer to associate with the thread in an
        architecture-specific way.

Return Value:

    Status code.

--*/

{

    return OsSystemCall(SystemCallSetThreadPointer, Pointer);
}

VOID
OspInitializeThreadSupport (
    VOID
    )

/*++

Routine Description:

    This routine initializes thread and TLS support in the OS library.

Arguments:

    None.

Return Value:

    None.

--*/

{

    INITIALIZE_LIST_HEAD(&OsThreadList);
    OsInitializeLockDefault(&OsThreadListLock);
    return;
}

KSTATUS
OspTlsAllocate (
    PLIST_ENTRY ImageList,
    PVOID *ThreadData,
    BOOL CopyInitImage
    )

/*++

Routine Description:

    This routine creates the OS library data necessary to manage a new thread.
    This function is usually called by the C library.

Arguments:

    ImageList - Supplies a pointer to the head of the list of loaded images.
        Elements on this list have type LOADED_IMAGE.

    ThreadData - Supplies a pointer where a pointer to the thread data will be
        returned on success. It is the callers responsibility to destroy this
        thread data.

    CopyInitImage - Supplies a boolean indicating whether or not to copy the
        initial image over to the new TLS area or not. If this is the initial
        program load and images have not yet been relocated, then the copies
        are skipped since they need to be done after relocations are applied.

Return Value:

    Status code.

--*/

{

    PVOID Allocation;
    UINTN AllocationSize;
    BOOL AnyAssigned;
    PLIST_ENTRY CurrentEntry;
    PVOID CurrentPointer;
    UINTN CurrentSize;
    PLOADED_IMAGE Image;
    ULONG MapFlags;
    UINTN ModuleCount;
    KSTATUS Status;
    PTHREAD_CONTROL_BLOCK ThreadControlBlock;
    UINTN VectorSize;

    Allocation = NULL;
    AnyAssigned = FALSE;

    //
    // Figure out how much to allocate for the thread control block and the
    // static TLS allocations.
    //

    ModuleCount = 0;
    AllocationSize = 0;
    ThreadControlBlock = NULL;
    CurrentEntry = ImageList->Next;
    while (CurrentEntry != ImageList) {
        Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Image->ModuleNumber > ModuleCount) {
            ModuleCount = Image->ModuleNumber;
        }

        if (((Image->Flags & IMAGE_FLAG_STATIC_TLS) != 0) ||
            ((Image->LoadFlags & IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE) != 0)) {

            if (Image->TlsAlignment <= 1) {
                AllocationSize += Image->TlsSize;

            } else {
                AllocationSize = ALIGN_RANGE_UP(AllocationSize + Image->TlsSize,
                                                Image->TlsAlignment);
            }
        }
    }

    ASSERT(ModuleCount != 0);

    ModuleCount += 1;
    AllocationSize = ALIGN_RANGE_UP(AllocationSize, sizeof(ULONGLONG));
    AllocationSize += sizeof(THREAD_CONTROL_BLOCK);
    VectorSize = ModuleCount * sizeof(PVOID);
    AllocationSize += VectorSize;
    AllocationSize = ALIGN_RANGE_UP(AllocationSize, OsPageSize);

    //
    // Allocate the region. Don't use the heap since it acquires locks which
    // might be left held if fork is called.
    //

    MapFlags = SYS_MAP_FLAG_ANONYMOUS | SYS_MAP_FLAG_READ | SYS_MAP_FLAG_WRITE;
    Status = OsMemoryMap(INVALID_HANDLE,
                         0,
                         AllocationSize,
                         MapFlags,
                         &Allocation);

    if (!KSUCCESS(Status)) {
        goto TlsAllocateEnd;
    }

    //
    // The structure looks like this:       |<<< Thread Pointer.
    // | TLS  | TLS    | ... | TLS  | TLS   | TCB | Dt  |
    //      m      m-1            2       1           v
    //
    // So the thread control block is at the very end (almost).
    //

    ThreadControlBlock = Allocation + AllocationSize - VectorSize -
                         sizeof(THREAD_CONTROL_BLOCK);

    ThreadControlBlock->Self = ThreadControlBlock;
    ThreadControlBlock->ModuleCount = ModuleCount;
    ThreadControlBlock->BaseAllocation = Allocation;
    ThreadControlBlock->TlsVector = (PVOID *)(ThreadControlBlock + 1);
    ThreadControlBlock->TlsVector[0] = (PVOID)OsImModuleGeneration;
    ThreadControlBlock->BaseAllocationSize = AllocationSize;

    //
    // Loop through the modules again, assigning space and initializing the
    // image.
    //

    CurrentEntry = ImageList->Next;
    CurrentSize = 0;
    while (CurrentEntry != ImageList) {
        Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        ASSERT((Image->ModuleNumber <= ThreadControlBlock->ModuleCount) &&
               (Image->ModuleNumber != 0));

        if ((Image->TlsSize == 0) ||
            (((Image->Flags & IMAGE_FLAG_STATIC_TLS) == 0) &&
             ((Image->LoadFlags & IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE) == 0))) {

            continue;
        }

        if (Image->TlsAlignment <= 1) {
            CurrentSize += Image->TlsSize;

        } else {
            CurrentSize = ALIGN_RANGE_UP(CurrentSize + Image->TlsSize,
                                         Image->TlsAlignment);
        }

        //
        // If static TLS offsets have not been assigned, then assign one now.
        //

        if (Image->TlsOffset == (UINTN)-1) {
            Image->TlsOffset = CurrentSize;
            AnyAssigned = TRUE;

        //
        // Otherwise, use the TLS offset previously assigned. There must not be
        // a mix of assigned and unassigned, since the required sizes would
        // come out differently depending on order.
        //

        } else {

            ASSERT(AnyAssigned == FALSE);

            CurrentSize = Image->TlsOffset;
        }

        CurrentPointer = ((PVOID)ThreadControlBlock) - CurrentSize;

        //
        // It would be bad if a module number was double allocated.
        //

        ASSERT(ThreadControlBlock->TlsVector[Image->ModuleNumber] == NULL);
        ASSERT(CurrentPointer >= Allocation);

        //
        // Set the vector pointer for this module, and initialize the image.
        //

        ThreadControlBlock->TlsVector[Image->ModuleNumber] = CurrentPointer;
        if ((CopyInitImage != FALSE) && (Image->TlsImageSize != 0)) {

            ASSERT(Image->TlsImageSize <= Image->TlsSize);

            RtlCopyMemory(CurrentPointer, Image->TlsImage, Image->TlsImageSize);
        }
    }

    //
    // Stick it on the thread list.
    //

    OsAcquireLock(&OsThreadListLock);
    INSERT_BEFORE(&(ThreadControlBlock->ListEntry), &OsThreadList);
    OsReleaseLock(&OsThreadListLock);
    Status = STATUS_SUCCESS;

TlsAllocateEnd:
    if (!KSUCCESS(Status)) {
        if (Allocation != NULL) {
            OsMemoryUnmap(Allocation, AllocationSize);
            Allocation = NULL;
        }
    }

    *ThreadData = ThreadControlBlock;
    return Status;
}

VOID
OspTlsDestroy (
    PVOID ThreadData
    )

/*++

Routine Description:

    This routine destroys a previously created thread data structure. Callers
    may not use OS library assisted TLS after this routine completes. Signals
    should also probably be masked.

Arguments:

    ThreadData - Supplies a pointer to the thread data to destroy.

Return Value:

    None.

--*/

{

    PVOID *AllocationPointer;
    UINTN Index;
    PTHREAD_CONTROL_BLOCK ThreadControlBlock;
    PVOID TlsBlock;

    ThreadControlBlock = ThreadData;
    for (Index = 1; Index < ThreadControlBlock->ModuleCount; Index += 1) {
        TlsBlock = ThreadControlBlock->TlsVector[Index];

        //
        // Don't free empty slots or slots that were in the initial allocation.
        //

        if ((TlsBlock == NULL) ||
            ((TlsBlock >= ThreadControlBlock->BaseAllocation) &&
             (TlsBlock < (PVOID)ThreadControlBlock))) {

            continue;
        }

        //
        // The value in this array may have been moved up from the actual
        // allocation due to alignment requirements. So the actual address of
        // the allocation is stored right below the region.
        //

        AllocationPointer = TlsBlock;
        OsHeapFree(*(AllocationPointer - 1));
    }

    //
    // If the TLS vector is not part of the initial allocation, free it.
    //

    if (ThreadControlBlock->TlsVector != (PVOID *)(ThreadControlBlock + 1)) {
        OsHeapFree(ThreadControlBlock->TlsVector);
    }

    OsAcquireLock(&OsThreadListLock);
    LIST_REMOVE(&(ThreadControlBlock->ListEntry));
    OsReleaseLock(&OsThreadListLock);
    ThreadControlBlock->Self = NULL;
    OsMemoryUnmap(ThreadControlBlock->BaseAllocation,
                  ThreadControlBlock->BaseAllocationSize);

    return;
}

VOID
OspTlsTearDownModule (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine is called when a module is unloaded. It goes through and
    frees all the TLS images for the module.

Arguments:

    Image - Supplies a pointer to the image being unloaded.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    UINTN ModuleNumber;
    PTHREAD_CONTROL_BLOCK ThreadControlBlock;
    PVOID *TlsData;

    if (Image->TlsSize == 0) {
        return;
    }

    OsAcquireLock(&OsThreadListLock);

    //
    // Loop through all threads and destroy the TLS block for this image.
    // The list is guarded by the image list lock held by the caller.
    //

    ModuleNumber = Image->ModuleNumber;
    CurrentEntry = OsThreadList.Next;
    while (CurrentEntry != &OsThreadList) {
        ThreadControlBlock = LIST_VALUE(CurrentEntry,
                                        THREAD_CONTROL_BLOCK,
                                        ListEntry);

        CurrentEntry = CurrentEntry->Next;
        if (ThreadControlBlock->ModuleCount < ModuleNumber) {
            continue;
        }

        TlsData = ThreadControlBlock->TlsVector[ModuleNumber];
        if (TlsData != NULL) {

            //
            // The actual allocation pointer is stored just below the TLS data
            // itself, as the buffer may have been scootched up for alignment.
            // Don't try to free something from part of the initial allocation.
            //

            if (!(((PVOID)TlsData >= ThreadControlBlock->BaseAllocation) &&
                  ((PVOID)TlsData < (PVOID)ThreadControlBlock))) {

                OsHeapFree(*(TlsData - 1));
            }

            ThreadControlBlock->TlsVector[ModuleNumber] = NULL;
        }
    }

    OsReleaseLock(&OsThreadListLock);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

