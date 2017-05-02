/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    iohandle.c

Abstract:

    This module implements support for managing I/O handles.

Author:

    Evan Green 25-Apr-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "iop.h"

//
// ---------------------------------------------------------------- Definitions
//

#define IO_HANDLE_ALLOCATION_TAG 0x61486F49 // 'aHoI'
#define IO_HANDLE_MAX_REFERENCE_COUNT 0x10000000

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
ULONG
IoGetIoHandleAccessPermissions (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine returns the access permissions for the given I/O handle. For
    directories, no access is always returned.

Arguments:

    IoHandle - Supplies a pointer to an I/O handle.

Return Value:

    Returns the access permissions for the given I/O handle.

--*/

{

    //
    // Return no access for a directory.
    //

    if (IoHandle->FileObject->Properties.Type == IoObjectRegularDirectory) {
        return 0;
    }

    return IoHandle->Access;
}

KERNEL_API
ULONG
IoGetIoHandleOpenFlags (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine returns the current open flags for a given I/O handle. Some
    of these flags can change.

Arguments:

    IoHandle - Supplies a pointer to an I/O handle.

Return Value:

    Returns the current open flags for the I/O handle.

--*/

{

    return IoHandle->OpenFlags;
}

VOID
IoIoHandleAddReference (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine increments the reference count on an I/O handle.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle.

Return Value:

    None.

--*/

{

    ULONG OldValue;

    OldValue = RtlAtomicAdd32(&(IoHandle->ReferenceCount), 1);

    ASSERT((OldValue != 0) && (OldValue < IO_HANDLE_MAX_REFERENCE_COUNT));

    return;
}

KSTATUS
IoIoHandleReleaseReference (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine decrements the reference count on an I/O handle. If the
    reference count becomes zero, the I/O handle will be destroyed.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle.

Return Value:

    None.

--*/

{

    ULONG OldValue;
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    OldValue = RtlAtomicAdd32(&(IoHandle->ReferenceCount), -1);

    ASSERT((OldValue != 0) && (OldValue < IO_HANDLE_MAX_REFERENCE_COUNT));

    if (OldValue == 1) {
        Status = IopClose(IoHandle);
        if (!KSUCCESS(Status)) {

            //
            // Restore the reference to the I/O handle.
            //

            RtlAtomicAdd32(&(IoHandle->ReferenceCount), 1);
            return Status;
        }

        MmFreePagedPool(IoHandle);
    }

    return Status;
}

PIMAGE_SECTION_LIST
IoGetImageSectionListFromIoHandle (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine gets the image section list for the given I/O handle.

Arguments:

    IoHandle - Supplies a pointer to an I/O handle.

Return Value:

    Returns a pointer to the I/O handles image section list or NULL on failure.

--*/

{

    PFILE_OBJECT FileObject;

    FileObject = IoHandle->FileObject;
    return IopGetImageSectionListFromFileObject(FileObject);
}

BOOL
IoIoHandleIsCacheable (
    PIO_HANDLE IoHandle,
    PULONG MapFlags
    )

/*++

Routine Description:

    This routine determines whether or not data for the I/O object specified by
    the given handle is cached in the page cache.

Arguments:

    IoHandle - Supplies a pointer to an I/O handle.

    MapFlags - Supplies an optional pointer where any additional map flags
        needed when mapping sections from this handle will be returned.
        See MAP_FLAG_* definitions.

Return Value:

    Returns TRUE if the I/O handle's object uses the page cache, FALSE
    otherwise.

--*/

{

    PFILE_OBJECT FileObject;

    FileObject = IoHandle->FileObject;
    if (MapFlags != NULL) {
        *MapFlags = FileObject->MapFlags;
    }

    //
    // The I/O handle is deemed cacheable if the file object is cacheable.
    //

    if (IO_IS_FILE_OBJECT_CACHEABLE(FileObject) != FALSE) {
        return TRUE;
    }

    return FALSE;
}

KSTATUS
IopCreateIoHandle (
    PIO_HANDLE *Handle
    )

/*++

Routine Description:

    This routine creates a new I/O handle with a reference count of one.

Arguments:

    Handle - Supplies a pointer where a pointer to the new I/O handle will be
        returned on success.

Return Value:

    Status code.

--*/

{

    PIO_HANDLE NewHandle;
    KSTATUS Status;

    //
    // Create the I/O handle structure.
    //

    NewHandle = MmAllocatePagedPool(sizeof(IO_HANDLE),
                                    IO_HANDLE_ALLOCATION_TAG);

    if (NewHandle == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateIoHandleEnd;
    }

    RtlZeroMemory(NewHandle, sizeof(IO_HANDLE));
    NewHandle->HandleType = IoHandleTypeDefault;
    NewHandle->ReferenceCount = 1;

    ASSERT(NewHandle->DeviceContext == NULL);

    Status = STATUS_SUCCESS;

CreateIoHandleEnd:
    if (!KSUCCESS(Status)) {
        if (NewHandle != NULL) {
            MmFreePagedPool(NewHandle);
            NewHandle = NULL;
        }
    }

    *Handle = NewHandle;
    return Status;
}

VOID
IopOverwriteIoHandle (
    PIO_HANDLE Destination,
    PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine overwrites the file object of the given handle. I/O actions
    performed on the destination handle go to the given file object. This
    routine is not thread safe.

Arguments:

    Destination - Supplies a pointer to the I/O handle that should magically
        redirect elsewhere.

    FileObject - Supplies a pointer to the file object to place in the handle.

Return Value:

    None.

--*/

{

    PFILE_OBJECT OldFileObject;

    //
    // The destination I/O handle really shouldn't be handed to anyone yet,
    // since I/O might get wonky during the switch.
    //

    ASSERT(Destination->ReferenceCount == 1);

    OldFileObject = Destination->FileObject;
    Destination->FileObject = FileObject;
    if (OldFileObject != Destination->PathPoint.PathEntry->FileObject) {
        IopFileObjectReleaseReference(OldFileObject);
    }

    IopFileObjectAddReference(FileObject);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

