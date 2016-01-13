/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

#include <minoca/kernel.h>
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

    FileObject = IoHandle->PathPoint.PathEntry->FileObject;
    return IopGetImageSectionListFromFileObject(FileObject);
}

KERNEL_API
ULONG
IoGetIoHandleAccessPermissions (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine returns the access permissions for the given I/O handle.

Arguments:

    IoHandle - Supplies a pointer to an I/O handle.

Return Value:

    Returns the access permissions for the given I/O handle.

--*/

{

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

BOOL
IoIoHandleIsCacheable (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine determines whether or not data for the I/O object specified by
    the given handle is cached in the page cache.

Arguments:

    IoHandle - Supplies a pointer to an I/O handle.

Return Value:

    Returns TRUE if the I/O handle's object is cached or FALSE otherwise.

--*/

{

    PFILE_OBJECT FileObject;

    //
    // The I/O handle is deemed cacheable if the file object is cacheable.
    //

    FileObject = IoHandle->PathPoint.PathEntry->FileObject;
    if (IO_IS_FILE_OBJECT_CACHEABLE(FileObject) != FALSE) {
        return TRUE;
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

