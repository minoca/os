/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    handles.c

Abstract:

    This module implements support for handles and handle tables.

Author:

    Evan Green 16-Feb-2013

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

#define HANDLE_TABLE_ALLOCATION_TAG 0x646E6148 // 'dnaH'

//
// Define the initial size of the handle table, in entries.
//

#define HANDLE_TABLE_INITIAL_SIZE 16

//
// Define handle flags.
//

//
// This flag is set when a handle table entry is allocated.
//

#define HANDLE_FLAG_ALLOCATED 0x80000000

//
// --------------------------------------------------------------------- Macros
//

//
// These macros acquire and release the handle table locks if they exist.
//

#define OB_ACQUIRE_HANDLE_TABLE_LOCK(_Table)                                   \
    if ((_Table)->Lock != NULL) {                                              \
        KeAcquireQueuedLock((_Table)->Lock);                                   \
    }

#define OB_RELEASE_HANDLE_TABLE_LOCK(_Table)                                   \
    if ((_Table)->Lock != NULL) {                                              \
        KeReleaseQueuedLock((_Table)->Lock);                                   \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a handle table entry.

Members:

    Flags - Stores a bitfield of flags associated with this handle. Most of
        these flags are available for the user. A couple of the high ones are
        reserved.

    HandleValue - Stores the actual value of the handle.

--*/

typedef struct _HANDLE_TABLE_ENTRY {
    ULONG Flags;
    PVOID HandleValue;
} HANDLE_TABLE_ENTRY, *PHANDLE_TABLE_ENTRY;

/*++

Structure Description:

    This structure defines a handle table.

Members:

    Process - Stores a pointer to the process that owns the handle table.

    NextDescriptor - Stores the first free descriptor number.

    MaxDescriptor - Stores the maximum valid descriptor number.

    Entries - Stores the actual array of handles.

    ArraySize - Stores the number of elements in the array.

    Lock - Stores a pointer to a lock protecting access to the handle table.

    LookupCallback - Stores an optional pointer to a routine that is called
        whenever a handle is looked up.

--*/

struct _HANDLE_TABLE {
    PKPROCESS Process;
    ULONG NextDescriptor;
    ULONG MaxDescriptor;
    PHANDLE_TABLE_ENTRY Entries;
    ULONG ArraySize;
    PQUEUED_LOCK Lock;
    PHANDLE_TABLE_LOOKUP_CALLBACK LookupCallback;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
ObpExpandHandleTable (
    PHANDLE_TABLE Table,
    ULONG Descriptor
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PHANDLE_TABLE
ObCreateHandleTable (
    PVOID Process,
    PHANDLE_TABLE_LOOKUP_CALLBACK LookupCallbackRoutine
    )

/*++

Routine Description:

    This routine creates a new handle table. This routine must be called at low
    level.

Arguments:

    Process - Supplies an optional pointer to the process that owns the handle
        table. When in doubt, supply NULL.

    LookupCallbackRoutine - Supplies an optional pointer that if supplied
        points to a function that will get called whenever a handle value is
        looked up (but not on iterates).

Return Value:

    Returns a pointer to the new handle table on success.

    NULL on insufficient resource conditions.

--*/

{

    UINTN AllocationSize;
    PHANDLE_TABLE HandleTable;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    HandleTable = MmAllocatePagedPool(sizeof(HANDLE_TABLE),
                                      HANDLE_TABLE_ALLOCATION_TAG);

    if (HandleTable == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateHandleTableEnd;
    }

    if (Process != NULL) {
        ObAddReference(Process);
    }

    HandleTable->Process = Process;
    HandleTable->Lock = NULL;
    HandleTable->NextDescriptor = 0;
    HandleTable->MaxDescriptor = 0;
    HandleTable->LookupCallback = LookupCallbackRoutine;
    AllocationSize = HANDLE_TABLE_INITIAL_SIZE * sizeof(HANDLE_TABLE_ENTRY);
    HandleTable->Entries = MmAllocatePagedPool(AllocationSize,
                                               HANDLE_TABLE_ALLOCATION_TAG);

    if (HandleTable->Entries == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateHandleTableEnd;
    }

    RtlZeroMemory(HandleTable->Entries, AllocationSize);
    HandleTable->ArraySize = HANDLE_TABLE_INITIAL_SIZE;
    Status = STATUS_SUCCESS;

CreateHandleTableEnd:
    if (!KSUCCESS(Status)) {
        if (HandleTable != NULL) {
            if (HandleTable->Entries != NULL) {
                MmFreePagedPool(HandleTable->Entries);
            }

            MmFreePagedPool(HandleTable);
            HandleTable = NULL;
        }
    }

    return HandleTable;
}

VOID
ObDestroyHandleTable (
    PHANDLE_TABLE HandleTable
    )

/*++

Routine Description:

    This routine destroys a handle table. This routine must be called at low
    level.

Arguments:

    HandleTable - Supplies a pointer to the handle table to destroy.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (HandleTable->Lock != NULL) {
        KeDestroyQueuedLock(HandleTable->Lock);
    }

    if (HandleTable->Entries != NULL) {
        MmFreePagedPool(HandleTable->Entries);
    }

    if (HandleTable->Process != NULL) {
        ObReleaseReference(HandleTable->Process);
    }

    MmFreePagedPool(HandleTable);
    return;
}

KSTATUS
ObEnableHandleTableLocking (
    PHANDLE_TABLE HandleTable
    )

/*++

Routine Description:

    This routine enables locking on the given handle table.

Arguments:

    HandleTable - Supplies a pointer to the handle table to enable locking for.

Return Value:

    Status code.

--*/

{

    if (HandleTable->Lock == NULL) {
        HandleTable->Lock = KeCreateQueuedLock();
        if (HandleTable->Lock == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
ObCreateHandle (
    PHANDLE_TABLE Table,
    PVOID HandleValue,
    ULONG Flags,
    PHANDLE NewHandle
    )

/*++

Routine Description:

    This routine creates a new handle table entry. This routine must be called
    at low level.

Arguments:

    Table - Supplies a pointer to the handle table.

    HandleValue - Supplies the value to be associated with the handle.

    Flags - Supplies a bitfield of flags to set with the handle. This value
        will be ANDed with HANDLE_FLAG_MASK, so bits set outside of that range
        will not stick.

    NewHandle - Supplies a pointer where the handle will be returned. On input,
        contains the minimum required value for the handle. Supply
        INVALID_HANDLE as the initial contents to let the system decide (which
        should be almost always).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
    handle table entry.

    STATUS_TOO_MANY_HANDLES if the given minimum handle value was too high.

--*/

{

    ULONG Descriptor;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((Table->Process == NULL) ||
           (Table->Process->ThreadCount == 0) ||
           (Table->Process == PsGetCurrentProcess()));

    OB_ACQUIRE_HANDLE_TABLE_LOCK(Table);

    //
    // Either use the next free slot, or try to use a descriptor at least as
    // high as the given handle.
    //

    if (*NewHandle == INVALID_HANDLE) {
        Descriptor = Table->NextDescriptor;

    } else {
        Descriptor = (UINTN)(*NewHandle);
    }

    //
    // Loop until a free slot is found.
    //

    while ((Descriptor < Table->ArraySize) &&
           ((Table->Entries[Descriptor].Flags & HANDLE_FLAG_ALLOCATED) != 0)) {

        Descriptor += 1;
    }

    if (*NewHandle == INVALID_HANDLE) {
        Table->NextDescriptor = Descriptor + 1;
    }

    //
    // Expand the table if needed.
    //

    if (Descriptor >= Table->ArraySize) {
        Status = ObpExpandHandleTable(Table, Descriptor);
        if (!KSUCCESS(Status)) {
            goto CreateHandleEnd;
        }
    }

    ASSERT(HandleValue != NULL);

    Table->Entries[Descriptor].Flags = HANDLE_FLAG_ALLOCATED |
                                       (Flags & HANDLE_FLAG_MASK);

    Table->Entries[Descriptor].HandleValue = HandleValue;
    *NewHandle = (HANDLE)(UINTN)Descriptor;
    if (Descriptor > Table->MaxDescriptor) {
        Table->MaxDescriptor = Descriptor;
    }

    Status = STATUS_SUCCESS;

CreateHandleEnd:
    OB_RELEASE_HANDLE_TABLE_LOCK(Table);
    return Status;
}

VOID
ObDestroyHandle (
    PHANDLE_TABLE Table,
    HANDLE Handle
    )

/*++

Routine Description:

    This routine destroys a handle.

Arguments:

    Table - Supplies a pointer to the handle table.

    Handle - Supplies the handle returned when the handle was created.

Return Value:

    None.

--*/

{

    ULONG Descriptor;

    ASSERT((Table->Process == NULL) ||
           (Table->Process->ThreadCount == 0) ||
           (Table->Process == PsGetCurrentProcess()));

    Descriptor = (UINTN)Handle;
    OB_ACQUIRE_HANDLE_TABLE_LOCK(Table);
    if (Descriptor >= Table->ArraySize) {
        goto DestroyHandleEnd;
    }

    if ((Table->Entries[Descriptor].Flags & HANDLE_FLAG_ALLOCATED) == 0) {
        goto DestroyHandleEnd;
    }

    Table->Entries[Descriptor].HandleValue = NULL;
    Table->Entries[Descriptor].Flags = 0;
    if (Table->NextDescriptor > Descriptor) {
        Table->NextDescriptor = Descriptor;
    }

DestroyHandleEnd:
    OB_RELEASE_HANDLE_TABLE_LOCK(Table);
    return;
}

KSTATUS
ObReplaceHandleValue (
    PHANDLE_TABLE Table,
    HANDLE Handle,
    PVOID NewHandleValue,
    ULONG NewFlags,
    PVOID *OldHandleValue,
    PULONG OldFlags
    )

/*++

Routine Description:

    This routine replaces a handle table entry, or creates a handle if none was
    there before. This routine must be called at low level.

Arguments:

    Table - Supplies a pointer to the handle table.

    Handle - Supplies the handle to replace or create.

    NewHandleValue - Supplies the value to be associated with the handle.

    NewFlags - Supplies the new handle flags to set.

    OldHandleValue - Supplies an optional pointer where the original handle
        value will be returned.

    OldFlags - Supplies an optional pointer where the original handle flags
        will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
    handle table entry.

    STATUS_TOO_MANY_HANDLES if the given minimum handle value was too high.

--*/

{

    ULONG Descriptor;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((Table->Process == NULL) ||
           (Table->Process->ThreadCount == 0) ||
           (Table->Process == PsGetCurrentProcess()));

    OB_ACQUIRE_HANDLE_TABLE_LOCK(Table);

    ASSERT(Handle != INVALID_HANDLE);

    Descriptor = (UINTN)Handle;
    if (Descriptor >= Table->ArraySize) {
        Status = ObpExpandHandleTable(Table, Descriptor);
        if (!KSUCCESS(Status)) {
            goto ReplaceHandleValueEnd;
        }
    }

    ASSERT(NewHandleValue != NULL);

    if (OldFlags != NULL) {
        *OldFlags = Table->Entries[Descriptor].Flags & HANDLE_FLAG_MASK;
    }

    if (OldHandleValue != NULL) {
        *OldHandleValue = Table->Entries[Descriptor].HandleValue;
    }

    Table->Entries[Descriptor].Flags = HANDLE_FLAG_ALLOCATED |
                                       (NewFlags & HANDLE_FLAG_MASK);

    Table->Entries[Descriptor].HandleValue = NewHandleValue;
    if (Descriptor > Table->MaxDescriptor) {
        Table->MaxDescriptor = Descriptor;
    }

    Status = STATUS_SUCCESS;

ReplaceHandleValueEnd:
    OB_RELEASE_HANDLE_TABLE_LOCK(Table);
    return Status;
}

PVOID
ObGetHandleValue (
    PHANDLE_TABLE Table,
    HANDLE Handle,
    PULONG Flags
    )

/*++

Routine Description:

    This routine looks up the given handle and returns the value associated
    with that handle.

Arguments:

    Table - Supplies a pointer to the handle table.

    Handle - Supplies the handle returned when the handle was created.

    Flags - Supplies an optional pointer that receives value of the handle's
        flags.

Return Value:

    Returns the value associated with that handle upon success.

    NULL if the given handle is invalid.

--*/

{

    ULONG Descriptor;
    ULONG LocalFlags;
    PVOID Value;

    ASSERT((Table->Process == NULL) ||
           (Table->Process->ThreadCount == 0) ||
           (Table->Process == PsGetCurrentProcess()));

    Descriptor = (UINTN)Handle;
    LocalFlags = 0;
    Value = NULL;
    OB_ACQUIRE_HANDLE_TABLE_LOCK(Table);
    if (Descriptor >= Table->ArraySize) {
        goto GetHandleValueEnd;
    }

    LocalFlags = Table->Entries[Descriptor].Flags;
    if ((LocalFlags & HANDLE_FLAG_ALLOCATED) == 0) {
        goto GetHandleValueEnd;
    }

    Value = Table->Entries[Descriptor].HandleValue;
    if (Table->LookupCallback != NULL) {
        Table->LookupCallback(Table, (HANDLE)(UINTN)Descriptor, Value);
    }

GetHandleValueEnd:
    OB_RELEASE_HANDLE_TABLE_LOCK(Table);
    if ((Flags != NULL) && (Value != NULL)) {
        *Flags = LocalFlags & HANDLE_FLAG_MASK;
    }

    return Value;
}

KSTATUS
ObGetSetHandleFlags (
    PHANDLE_TABLE Table,
    HANDLE Handle,
    BOOL Set,
    PULONG Flags
    )

/*++

Routine Description:

    This routine sets and/or returns the flags associated with a handle. The
    lookup callback routine initialized with the handle table is not called
    during this operation.

Arguments:

    Table - Supplies a pointer to the handle table.

    Handle - Supplies the handle whose flags should be retrieved.

    Set - Supplies a boolean indicating if the value in the flags parameter
        should be set as the new value.

    Flags - Supplies a pointer that on input contains the value of the flags
        to set if the set parameter is TRUE. This value will be ANDed with
        HANDLE_FLAG_MASK, so bits set outside of that mask will not stick.
        On output, contains the original value of the flags before the set was
        performed.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_HANDLE if no such handle could be found.

--*/

{

    ULONG Descriptor;
    ULONG NewValue;
    ULONG OriginalValue;
    KSTATUS Status;

    ASSERT((Table->Process == NULL) ||
           (Table->Process->ThreadCount == 0) ||
           (Table->Process == PsGetCurrentProcess()));

    Status = STATUS_INVALID_HANDLE;
    Descriptor = (UINTN)Handle;
    OB_ACQUIRE_HANDLE_TABLE_LOCK(Table);
    if (Descriptor >= Table->ArraySize) {
        goto GetSetHandleFlagsEnd;
    }

    if ((Table->Entries[Descriptor].Flags & HANDLE_FLAG_ALLOCATED) == 0) {
        goto GetSetHandleFlagsEnd;
    }

    Status = STATUS_SUCCESS;
    NewValue = *Flags;
    OriginalValue = Table->Entries[Descriptor].Flags;
    *Flags = OriginalValue & HANDLE_FLAG_MASK;
    if (Set != FALSE) {
        Table->Entries[Descriptor].Flags = (NewValue & HANDLE_FLAG_MASK) |
                                           (OriginalValue & ~HANDLE_FLAG_MASK);
    }

GetSetHandleFlagsEnd:
    OB_RELEASE_HANDLE_TABLE_LOCK(Table);
    return Status;
}

HANDLE
ObGetHighestHandle (
    PHANDLE_TABLE Table
    )

/*++

Routine Description:

    This routine returns the highest allocated handle.

Arguments:

    Table - Supplies a pointer to the handle table.

Return Value:

    Returns the highest handle number (not the handle value).

    INVALID_HANDLE if the table is empty.

--*/

{

    ULONG Descriptor;
    HANDLE Handle;

    ASSERT((Table->Process == NULL) ||
           (Table->Process->ThreadCount == 0) ||
           (Table->Process == PsGetCurrentProcess()));

    Handle = INVALID_HANDLE;
    OB_ACQUIRE_HANDLE_TABLE_LOCK(Table);
    Descriptor = Table->MaxDescriptor;

    ASSERT(Descriptor < Table->ArraySize);

    while ((Table->Entries[Descriptor].Flags & HANDLE_FLAG_ALLOCATED) == 0) {
        if (Descriptor == 0) {
            break;
        }

        Descriptor -= 1;
    }

    if ((Table->Entries[Descriptor].Flags & HANDLE_FLAG_ALLOCATED) != 0) {
        Handle = (HANDLE)(UINTN)Descriptor;
    }

    Table->MaxDescriptor = Descriptor;
    OB_RELEASE_HANDLE_TABLE_LOCK(Table);
    return Handle;
}

VOID
ObHandleTableIterate (
    PHANDLE_TABLE Table,
    PHANDLE_TABLE_ITERATE_ROUTINE IterateRoutine,
    PVOID IterateRoutineContext
    )

/*++

Routine Description:

    This routine iterates through all handles in the given handle table, and
    calls the given handle table for each one. The table will be locked when the
    iterate routine is called, so the iterate routine must not make any calls
    that would require use of the handle table.

Arguments:

    Table - Supplies a pointer to the handle table to iterate through.

    IterateRoutine - Supplies a pointer to the routine to be called for each
        handle in the table.

    IterateRoutineContext - Supplies an opaque context pointer that will get
        passed to the iterate routine each time it is called.

Return Value:

    None.

--*/

{

    ULONG Descriptor;

    ASSERT((Table->Process == NULL) ||
           (Table->Process->ThreadCount == 0) ||
           (Table->Process == PsGetCurrentProcess()));

    OB_ACQUIRE_HANDLE_TABLE_LOCK(Table);
    for (Descriptor = 0; Descriptor <= Table->MaxDescriptor; Descriptor += 1) {
        if ((Table->Entries[Descriptor].Flags & HANDLE_FLAG_ALLOCATED) == 0) {
            continue;
        }

        IterateRoutine(Table,
                       (HANDLE)(UINTN)Descriptor,
                       Table->Entries[Descriptor].Flags & HANDLE_FLAG_MASK,
                       Table->Entries[Descriptor].HandleValue,
                       IterateRoutineContext);
    }

    OB_RELEASE_HANDLE_TABLE_LOCK(Table);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
ObpExpandHandleTable (
    PHANDLE_TABLE Table,
    ULONG Descriptor
    )

/*++

Routine Description:

    This routine expands the given handle table to support a given number of
    descriptors.

Arguments:

    Table - Supplies a pointer to the handle table to expand.

    Descriptor - Supplies the descriptor that will need to be inserted in the
        handle table.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PVOID NewBuffer;
    UINTN NewCapacity;
    KSTATUS Status;

    if (Descriptor >= OB_MAX_HANDLES) {
        Status = STATUS_INVALID_HANDLE;
        goto ExpandHandleTableEnd;
    }

    //
    // Expand the table if needed.
    //

    if (Descriptor >= Table->ArraySize) {
        NewCapacity = Table->ArraySize * 2;
        while ((NewCapacity <= Descriptor) &&
               (NewCapacity >= Table->ArraySize)) {

            NewCapacity *= 2;
        }

        AllocationSize = NewCapacity * sizeof(HANDLE_TABLE_ENTRY);
        if ((NewCapacity <= Descriptor) ||
            (NewCapacity < Table->ArraySize) ||
            ((AllocationSize / sizeof(HANDLE_TABLE_ENTRY)) != NewCapacity)) {

            Status = STATUS_TOO_MANY_HANDLES;
            goto ExpandHandleTableEnd;
        }

        ASSERT((NewCapacity > Table->ArraySize) &&
               (NewCapacity > Table->NextDescriptor));

        NewBuffer = MmAllocatePagedPool(AllocationSize,
                                        HANDLE_TABLE_ALLOCATION_TAG);

        if (NewBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ExpandHandleTableEnd;
        }

        RtlCopyMemory(NewBuffer,
                      Table->Entries,
                      Table->ArraySize * sizeof(HANDLE_TABLE_ENTRY));

        RtlZeroMemory(
                NewBuffer + (Table->ArraySize * sizeof(HANDLE_TABLE_ENTRY)),
                (NewCapacity - Table->ArraySize) * sizeof(HANDLE_TABLE_ENTRY));

        MmFreePagedPool(Table->Entries);
        Table->Entries = NewBuffer;
        Table->ArraySize = NewCapacity;
    }

    Status = STATUS_SUCCESS;

ExpandHandleTableEnd:
    return Status;
}

