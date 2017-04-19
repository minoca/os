/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    irp.c

Abstract:

    This module implements support for handling I/O Request Packets (IRPs).

Author:

    Evan Green 17-Sep-2012

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

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _IRP_CRASH_REASON {
    IrpCrashReasonInvalid,
    IrpCrashCorruption,
    IrpCrashConstantStateModified,
    IrpCrashImproperlyAllocated
} IRP_CRASH_REASON, *PIRP_CRASH_REASON;

/*++

Structure Description:

    This structure defines an entry in an IRP stack.

Members:

    DriverStackEntry - Stores a pointer to the driver stack entry this IRP
        stack entry corresponds to.

    IrpContext - Stores the driver's context associated with this particular
        IRP.

--*/

typedef struct _IRP_STACK_ENTRY {
    PDRIVER_STACK_ENTRY DriverStackEntry;
    PVOID IrpContext;
} IRP_STACK_ENTRY, *PIRP_STACK_ENTRY;

/*++

Structure Description:

    This structure defines the internal structure of an IRP, which includes
    extra fields not exposed to drivers.

Members:

    Public - Stores the public portion of the IRP.

    Magic - Stores a magic value used to validate that drivers are not
        attempting to create IRPs outside the system routines for doing so.

    MajorCode - Stores a copy of the major code the IRP was created with, used
        to ensure that drivers aren't changing the major code after an IRP
        is allocated.

    Device - Stores a copy of the device that the IRP was created for, used to
        ensure that drivers aren't changing the device after an IRP is created.

    Stack - Stores the IRP stack for this IRP.

    StackIndex - Stores the current index into the IRP stack.

    StackSize - Stores the number of elements in the IRP stack.

    Flags - Stores a set of informational flags about the IRP. See IRP_*
        definitions.

--*/

typedef struct _IRP_INTERNAL {
    IRP Public;
    USHORT Magic;
    IRP_MAJOR_CODE MajorCode;
    PDEVICE Device;
    PIRP_STACK_ENTRY Stack;
    ULONG StackIndex;
    ULONG StackSize;
    ULONG Flags;
} IRP_INTERNAL, *PIRP_INTERNAL;

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
IopPumpIrpThroughStack (
    PIRP_INTERNAL Irp
    );

VOID
IopCallDriver (
    PIRP_INTERNAL Irp
    );

BOOL
IopAdvanceIrpStackLocation (
    PIRP_INTERNAL Irp
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the parent object of all IRPs.
//

POBJECT_HEADER IoIrpDirectory = NULL;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
IoGetIrpStatus (
    PIRP Irp
    )

/*++

Routine Description:

    This routine returns the IRP's completion status.

Arguments:

    Irp - Supplies a pointer to the IRP to query.

Return Value:

    Returns the IRP completion status. If no driver has completed the IRP,
    STATUS_NOT_HANDLED will be returned (the initialization value put into the
    IRP).

--*/

{

    return Irp->Status;
}

KERNEL_API
VOID
IoUpdateIrpStatus (
    PIRP Irp,
    KSTATUS StatusCode
    )

/*++

Routine Description:

    This routine updates the IRP's completion status if the current completion
    status indicates success.

Arguments:

    Irp - Supplies a pointer to the IRP to query.

    StatusCode - Supplies a status code to associate with the completed IRP.
        This will be returned back to the caller requesting the IRP.

Return Value:

    None.

--*/

{

    if (KSUCCESS(Irp->Status)) {
        Irp->Status = StatusCode;
    }

    return;
}

KERNEL_API
VOID
IoCompleteIrp (
    PDRIVER Driver,
    PIRP Irp,
    KSTATUS StatusCode
    )

/*++

Routine Description:

    This routine is called by a driver to mark an IRP as completed. This
    function can only be called from a driver's dispatch routine when the
    driver owns the IRP. When the dispatch routine returns, the system will not
    continue to move down the driver stack, but will switch directions and
    move up the stack. Only one driver in the stack should complete the IRP.
    This routine must be called at or below dispatch level.

Arguments:

    Driver - Supplies a pointer to the driver completing the IRP.

    Irp - Supplies a pointer to the IRP owned by the driver to mark as
        completed.

    StatusCode - Supplies a status code to associated with the completed IRP.
        This will be returned back to the caller requesting the IRP.

Return Value:

    None.

--*/

{

    PDRIVER_STACK_ENTRY DriverStackEntry;
    PIRP_INTERNAL InternalIrp;

    InternalIrp = (PIRP_INTERNAL)Irp;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);
    ASSERT(InternalIrp->StackIndex < InternalIrp->StackSize);
    ASSERT((InternalIrp->Flags & IRP_ACTIVE) != 0);

    DriverStackEntry =
                  InternalIrp->Stack[InternalIrp->StackIndex].DriverStackEntry;

    ASSERT(DriverStackEntry->Driver == Driver);

    if (DriverStackEntry->Driver == Driver) {
        InternalIrp->Flags |= IRP_COMPLETE;
        Irp->Direction = IrpUp;
        Irp->Status = StatusCode;

        //
        // If the IRP is pending, nothing else is driving it. Signal the IRP to
        // wake the sending thread to continue driving the IRP. Do not clear
        // the pending flag. Otherwise the sending thread may never see it set,
        // resulting in the pending driver only getting called in the down
        // direction.
        //

        if ((InternalIrp->Flags & IRP_PENDING) != 0) {
            ObSignalObject(Irp, SignalOptionSignalAll);
        }
    }

    return;
}

KERNEL_API
VOID
IoPendIrp (
    PDRIVER Driver,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is called by a driver to mark an IRP as pending. This function
    can only be called from a driver's dispatch routine when the driver owns
    the IRP. When the dispatch routine returns, the system will not move to the
    next stack location: the driver will continue to own the IRP until it
    marks it completed or continues the IRP. This routine must be called at or
    below dispatch level.

Arguments:

    Driver - Supplies a pointer to the driver pending the IRP.

    Irp - Supplies a pointer to the IRP owned by the driver to mark as pending.

Return Value:

    None.

--*/

{

    PDRIVER_STACK_ENTRY DriverStackEntry;
    PIRP_INTERNAL InternalIrp;

    InternalIrp = (PIRP_INTERNAL)Irp;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);
    ASSERT(InternalIrp->StackIndex < InternalIrp->StackSize);
    ASSERT((InternalIrp->Flags & IRP_ACTIVE) != 0);

    DriverStackEntry =
                  InternalIrp->Stack[InternalIrp->StackIndex].DriverStackEntry;

    ASSERT(DriverStackEntry->Driver == Driver);

    if (DriverStackEntry->Driver == Driver) {
        InternalIrp->Flags |= IRP_PENDING;
    }

    return;
}

KERNEL_API
VOID
IoContinueIrp (
    PDRIVER Driver,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is called by a driver to continue processing an IRP that was
    previously marked pending. The driver that pended the IRP will get called
    a second time in the same directon for this IRP. This routine must be
    called at or below dispatch level.

Arguments:

    Driver - Supplies a pointer to the driver unpending the IRP.

    Irp - Supplies a pointer to the IRP owned by the driver to continue
        processing.

Return Value:

    None.

--*/

{

    PDRIVER_STACK_ENTRY DriverStackEntry;
    PIRP_INTERNAL InternalIrp;

    InternalIrp = (PIRP_INTERNAL)Irp;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);
    ASSERT(InternalIrp->StackIndex < InternalIrp->StackSize);
    ASSERT((InternalIrp->Flags & IRP_ACTIVE) != 0);
    ASSERT((InternalIrp->Flags & IRP_PENDING) != 0);

    DriverStackEntry =
                  InternalIrp->Stack[InternalIrp->StackIndex].DriverStackEntry;

    ASSERT(DriverStackEntry->Driver == Driver);

    if (DriverStackEntry->Driver == Driver) {

        //
        // If the IRP is pending, nothing else is driving it. Signal the IRP to
        // wake the sending thread to continue driving the IRP. Do not clear
        // the pending flag. The driving thread will do that.
        //

        if ((InternalIrp->Flags & IRP_PENDING) != 0) {
            ObSignalObject(Irp, SignalOptionSignalAll);
        }
    }

    return;
}

KERNEL_API
PIRP
IoCreateIrp (
    PDEVICE Device,
    IRP_MAJOR_CODE MajorCode,
    ULONG Flags
    )

/*++

Routine Description:

    This routine creates and initializes an IRP. This routine must be called
    at or below dispatch level.

Arguments:

    Device - Supplies a pointer to the device the IRP will be sent to.

    MajorCode - Supplies the major code of the IRP, which cannot be changed
        once an IRP is allocated (or else disaster ensues).

    Flags - Supplies a bitmask of IRP creation flags. See IRP_FLAG_* for
        definitions.

Return Value:

    Returns a pointer to the newly allocated IRP on success, or NULL on
    failure.

--*/

{

    ULONG AllocationSize;
    PDRIVER_CREATE_IRP CreateIrp;
    PLIST_ENTRY CurrentEntry;
    PDRIVER_STACK_ENTRY CurrentStackEntry;
    PDEVICE CurrentTarget;
    PDRIVER_DISPATCH DestroyIrp;
    ULONG EntryIndex;
    PIRP_INTERNAL Irp;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);
    ASSERT((Device->Header.Type == ObjectDevice) ||
           (Device->Header.Type == ObjectVolume));

    Irp = NULL;

    //
    // Ensure that a valid device was specified and the device is somewhat
    // initialized.
    //

    if (Device != NULL) {
        if ((Device->DriverStackSize == 0) ||
            (LIST_EMPTY(&(Device->DriverStackHead)) != FALSE)) {

            Status = STATUS_INVALID_CONFIGURATION;
            goto CreateIrpEnd;
        }
    }

    //
    // Attempt to allocate and initialize the IRP.
    //

    Irp = ObCreateObject(ObjectIrp,
                         IoIrpDirectory,
                         NULL,
                         0,
                         sizeof(IRP_INTERNAL),
                         NULL,
                         0,
                         IRP_ALLOCATION_TAG);

    if (Irp == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateIrpEnd;
    }

    Irp->Magic = IRP_MAGIC_VALUE;
    Irp->MajorCode = MajorCode;
    Irp->Device = Device;
    Irp->Public.Device = Device;
    Irp->Public.MajorCode = MajorCode;
    Irp->Flags = 0;
    Irp->Stack = NULL;
    Irp->StackIndex = 0;

    //
    // Figure out the size of the IRP stack, which is a chain of all the
    // target devices. Don't follow the target device through volumes.
    //

    Irp->StackSize = 0;
    CurrentTarget = Device;
    while (CurrentTarget != NULL) {
        Irp->StackSize += CurrentTarget->DriverStackSize;
        if (CurrentTarget->Header.Type != ObjectDevice) {
            break;
        }

        CurrentTarget = CurrentTarget->TargetDevice;
    }

    //
    // Allocate the IRP stack.
    //

    AllocationSize = sizeof(IRP_STACK_ENTRY) * Irp->StackSize;
    Irp->Stack = MmAllocateNonPagedPool(AllocationSize, IRP_ALLOCATION_TAG);
    if (Irp->Stack == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Irp->Stack, AllocationSize);
    IoInitializeIrp(&(Irp->Public));

    //
    // Loop through every device in the IRP stack.
    //

    EntryIndex = 0;
    CurrentTarget = Device;
    while (CurrentTarget != NULL) {

        //
        // Loop through every driver on this device stack and allow it to
        // create state with this IRP.
        //

        CurrentEntry = CurrentTarget->DriverStackHead.Next;
        while (CurrentEntry != &(CurrentTarget->DriverStackHead)) {
            CurrentStackEntry = LIST_VALUE(CurrentEntry,
                                           DRIVER_STACK_ENTRY,
                                           ListEntry);

            CurrentEntry = CurrentEntry->Next;

            ASSERT(EntryIndex < Irp->StackSize);

            Irp->Stack[EntryIndex].DriverStackEntry = CurrentStackEntry;
            CreateIrp = CurrentStackEntry->Driver->FunctionTable.CreateIrp;
            if (CreateIrp != NULL) {
                Status = CreateIrp((PIRP)Irp,
                                   CurrentStackEntry->DriverContext,
                                   &(Irp->Stack[EntryIndex].IrpContext),
                                   Flags);

                if (!KSUCCESS(Status)) {
                    goto CreateIrpEnd;
                }
            }

            EntryIndex += 1;
        }

        //
        // Move to the next device in the chain, but don't follow the target
        // device through a volume.
        //

        if (CurrentTarget->Header.Type != ObjectDevice) {
            break;
        }

        CurrentTarget = CurrentTarget->TargetDevice;
    }

    Status = STATUS_SUCCESS;

CreateIrpEnd:
    if (!KSUCCESS(Status)) {
        if (Irp != NULL) {
            if (Irp->Stack != NULL) {

                //
                // If a driver failed the allocation, then clean up everything
                // up until then.
                //

                for (EntryIndex = 0;
                     EntryIndex < Irp->StackSize;
                     EntryIndex += 1) {

                    if (Irp->Stack[EntryIndex].DriverStackEntry == NULL) {
                        break;
                    }

                    DestroyIrp =
                           CurrentStackEntry->Driver->FunctionTable.DestroyIrp;

                    ASSERT((Irp->Stack[EntryIndex].IrpContext == NULL) ||
                           (DestroyIrp != NULL));

                    if (DestroyIrp != NULL) {
                        DestroyIrp((PIRP)Irp,
                                   CurrentStackEntry->DriverContext,
                                   Irp->Stack[EntryIndex].IrpContext);
                    }
                }

                MmFreeNonPagedPool(Irp->Stack);
            }

            ASSERT(Irp->Public.Header.ReferenceCount == 1);

            ObReleaseReference(Irp);
            Irp = NULL;
        }
    }

    return (PIRP)Irp;
}

KERNEL_API
VOID
IoDestroyIrp (
    PIRP Irp
    )

/*++

Routine Description:

    This routine destroys an IRP, freeing all memory associated with it. This
    routine must be called at or below dispatch level.

Arguments:

    Irp - Supplies a pointer to the IRP to free.

Return Value:

    None.

--*/

{

    PDRIVER_DISPATCH DestroyIrp;
    PDRIVER_STACK_ENTRY DriverStackEntry;
    ULONG EntryIndex;
    PIRP_INTERNAL InternalIrp;

    InternalIrp = (PIRP_INTERNAL)Irp;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);
    ASSERT(Irp != NULL);
    ASSERT((InternalIrp->Flags & IRP_ACTIVE) == 0);

    //
    // Crash if the IRP was improperly allocated or modified.
    //

    if (InternalIrp->Magic != IRP_MAGIC_VALUE) {
        KeCrashSystem(CRASH_INVALID_IRP,
                      IrpCrashImproperlyAllocated,
                      (UINTN)Irp,
                      (UINTN)Irp->Device,
                      0);
    }

    if ((InternalIrp->Device != Irp->Device) ||
        (InternalIrp->MajorCode != Irp->MajorCode)) {

        KeCrashSystem(CRASH_INVALID_IRP,
                      IrpCrashConstantStateModified,
                      (UINTN)Irp,
                      (UINTN)Irp->Device,
                      0);
    }

    //
    // Loop through and call every driver that has the destroy IRP routine
    // filled in.
    //

    for (EntryIndex = 0; EntryIndex < InternalIrp->StackSize; EntryIndex += 1) {
        DriverStackEntry = InternalIrp->Stack[EntryIndex].DriverStackEntry;
        DestroyIrp = DriverStackEntry->Driver->FunctionTable.DestroyIrp;

        ASSERT((InternalIrp->Stack[EntryIndex].IrpContext == NULL) ||
               (DestroyIrp != NULL));

        if (DestroyIrp != NULL) {
            DestroyIrp(Irp,
                       DriverStackEntry->DriverContext,
                       InternalIrp->Stack[EntryIndex].IrpContext);

        }
    }

    MmFreeNonPagedPool(InternalIrp->Stack);
    ObReleaseReference(Irp);
    return;
}

KERNEL_API
VOID
IoInitializeIrp (
    PIRP Irp
    )

/*++

Routine Description:

    This routine initializes an IRP and prepares it to be sent to a device.
    This routine does not mean that IRPs can be allocated randomly from pool
    and initialized here; IRPs must still be allocated from IoCreateIrp. This
    routine just resets an IRP back to its initialized state.

Arguments:

    Irp - Supplies a pointer to the initialized IRP to initialize. This IRP
        must already have a valid object header.

Return Value:

    None.

--*/

{

    PIRP_INTERNAL InternalIrp;

    InternalIrp = (PIRP_INTERNAL)Irp;
    InternalIrp->Public.Direction = IrpDown;

    ASSERT(InternalIrp->Device == InternalIrp->Public.Device);
    ASSERT(InternalIrp->MajorCode == InternalIrp->Public.MajorCode);
    ASSERT(InternalIrp->Device != NULL);

    InternalIrp->Public.Status = STATUS_NOT_HANDLED;
    InternalIrp->Flags &= ~(IRP_COMPLETE | IRP_PENDING);
    InternalIrp->Public.CompletionRoutine = NULL;
    InternalIrp->StackIndex = 0;
    return;
}

KERNEL_API
KSTATUS
IoSendSynchronousIrp (
    PIRP Irp
    )

/*++

Routine Description:

    This routine sends an initialized IRP down the device stack and does not
    return until the IRP completed. This routine must be called at or below
    dispatch level.

Arguments:

    Irp - Supplies a pointer to the initialized IRP to send. All parameters
        should already be filled out and ready to go.

Return Value:

    STATUS_SUCCESS if the IRP was actually sent properly. This says nothing of
    the completion status of the IRP, which may have failed spectacularly.

    STATUS_INVALID_PARAMETER if the IRP was not properly initialized.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated.

--*/

{

    PIRP_INTERNAL InternalIrp;
    BOOL IrpDone;
    KSTATUS Status;

    InternalIrp = (PIRP_INTERNAL)Irp;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    //
    // Crash if the IRP was improperly allocated or modified.
    //

    if (InternalIrp->Magic != IRP_MAGIC_VALUE) {
        KeCrashSystem(CRASH_INVALID_IRP,
                      IrpCrashImproperlyAllocated,
                      (UINTN)Irp,
                      (UINTN)Irp->Device,
                      0);
    }

    if ((InternalIrp->Device != Irp->Device) ||
        (InternalIrp->MajorCode != Irp->MajorCode)) {

        KeCrashSystem(CRASH_INVALID_IRP,
                      IrpCrashConstantStateModified,
                      (UINTN)Irp,
                      (UINTN)Irp->Device,
                      0);
    }

    //
    // Fail if the IRP is not properly initialized.
    //

    if ((Irp == NULL) ||
        (Irp->MinorCode == IrpMinorInvalid) ||
        (Irp->Direction != IrpDown) ||
        (Irp->CompletionRoutine != NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto SendSynchronousIrpEnd;
    }

    ASSERT((InternalIrp->Flags &
            (IRP_COMPLETE | IRP_ACTIVE | IRP_PENDING)) == 0);

    //
    // Pump the IRP through its driver stack. If it returns and is not done,
    // then it was pended. Wait for the IRP to be signaled and then continue
    // pumping it through the stack until it is done.
    //

    Status = STATUS_SUCCESS;
    InternalIrp->Flags |= IRP_ACTIVE;
    while (TRUE) {
        ObSignalObject(Irp, SignalOptionUnsignal);
        IrpDone = IopPumpIrpThroughStack(InternalIrp);
        if (IrpDone != FALSE) {
            break;
        }

        ASSERT((InternalIrp->Flags & IRP_PENDING) != 0);

        Status = ObWaitOnObject(Irp, 0, WAIT_TIME_INDEFINITE);
        if (!KSUCCESS(Status)) {

            ASSERT(FALSE);

            break;
        }

        ASSERT((InternalIrp->Flags & IRP_COMPLETE) != 0);

        InternalIrp->Flags &= ~IRP_PENDING;
    }

    InternalIrp->Flags &= ~IRP_ACTIVE;

SendSynchronousIrpEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoPrepareReadWriteIrp (
    PIRP_READ_WRITE IrpReadWrite,
    UINTN Alignment,
    PHYSICAL_ADDRESS MinimumPhysicalAddress,
    PHYSICAL_ADDRESS MaximumPhysicalAddress,
    ULONG Flags
    )

/*++

Routine Description:

    This routine prepares the given read/write IRP context for I/O based on the
    given physical address, physical alignment, and flag requirements. It will
    ensure that the IRP's I/O buffer is sufficient and preform any necessary
    flushing that is needed to prepare for the I/O.

Arguments:

    IrpReadWrite - Supplies a pointer to the IRP read/write context that needs
        to be prepared for data transfer.

    Alignment - Supplies the required physical alignment of the I/O buffer, in
        bytes.

    MinimumPhysicalAddress - Supplies the minimum allowed physical address for
        the I/O buffer.

    MaximumPhysicalAddress - Supplies the maximum allowed physical address for
        the I/O buffer.

    Flags - Supplies a bitmask of flags for the preparation. See
        IRP_READ_WRITE_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    BOOL LockedCopy;
    PIO_BUFFER OriginalIoBuffer;
    BOOL PhysicallyContiguous;
    KSTATUS Status;
    PIO_BUFFER ValidIoBuffer;

    //
    // Clobber the current buffer state. It should not be in use, but not every
    // initialization of IRP read/write state bothers to zero-initialize the
    // structure.
    //

    IrpReadWrite->IoBufferState.IoBuffer = NULL;
    IrpReadWrite->IoBufferState.Flags = 0;

    //
    // If this is not a polled I/O transfer, then make sure the buffer is
    // aligned up to a cache line.
    //

    if ((Flags & IRP_READ_WRITE_FLAG_POLLED) == 0) {
        if (Alignment == 0) {
            Alignment = 1;
        }

        Alignment = ALIGN_RANGE_UP(Alignment, MmGetIoBufferAlignment());
    }

    //
    // Validate the I/O buffer to make sure it is suitable for the supplied
    // constraints.
    //

    PhysicallyContiguous = FALSE;
    if ((Flags & IRP_READ_WRITE_FLAG_PHYSICALLY_CONTIGUOUS) != 0) {
        PhysicallyContiguous = TRUE;
    }

    OriginalIoBuffer = IrpReadWrite->IoBuffer;
    ValidIoBuffer = OriginalIoBuffer;
    Status = MmValidateIoBuffer(MinimumPhysicalAddress,
                                MaximumPhysicalAddress,
                                Alignment,
                                IrpReadWrite->IoSizeInBytes,
                                PhysicallyContiguous,
                                &ValidIoBuffer,
                                &LockedCopy);

    if (!KSUCCESS(Status)) {
        goto PrepareReadWriteIrpEnd;
    }

    //
    // If the original buffer was deemed invalid and not just because it needed
    // to be locked, copy the contents from the original buffer to the valid
    // buffer.
    //

    if ((ValidIoBuffer != OriginalIoBuffer) &&
        (LockedCopy == FALSE) &&
        ((Flags & IRP_READ_WRITE_FLAG_WRITE) != 0)) {

        Status = MmCopyIoBuffer(ValidIoBuffer,
                                0,
                                OriginalIoBuffer,
                                0,
                                IrpReadWrite->IoSizeInBytes);

        if (!KSUCCESS(Status)) {
            goto PrepareReadWriteIrpEnd;
        }
    }

    //
    // If the valid buffer will be used for DMA, then it needs to be cleaned or
    // invalidated depending on whether it is a read or a write.
    //

    if ((Flags & IRP_READ_WRITE_FLAG_DMA) != 0) {

        //
        // TODO: Remove this map request when cache cleanliness is fixed.
        //

        Status = MmMapIoBuffer(ValidIoBuffer, FALSE, FALSE, FALSE);
        if (!KSUCCESS(Status)) {
            goto PrepareReadWriteIrpEnd;
        }

        for (FragmentIndex = 0;
             FragmentIndex < ValidIoBuffer->FragmentCount;
             FragmentIndex += 1) {

            Fragment = &(ValidIoBuffer->Fragment[FragmentIndex]);
            if ((Flags & IRP_READ_WRITE_FLAG_WRITE) != 0) {
                Status = MmFlushBufferForDataOut(Fragment->VirtualAddress,
                                                 Fragment->Size);

            } else {
                Status = MmFlushBufferForDataIn(Fragment->VirtualAddress,
                                                Fragment->Size);
            }

            if (!KSUCCESS(Status)) {
                goto PrepareReadWriteIrpEnd;
            }
        }
    }

PrepareReadWriteIrpEnd:
    if (ValidIoBuffer != OriginalIoBuffer) {
        if (!KSUCCESS(Status)) {
            MmFreeIoBuffer(ValidIoBuffer);

        } else {
            IrpReadWrite->IoBufferState.IoBuffer = OriginalIoBuffer;
            IrpReadWrite->IoBuffer = ValidIoBuffer;
            if (LockedCopy != FALSE) {
                IrpReadWrite->IoBufferState.Flags |=
                                          IRP_IO_BUFFER_STATE_FLAG_LOCKED_COPY;
            }
        }
    }

    return Status;
}

KERNEL_API
KSTATUS
IoCompleteReadWriteIrp (
    PIRP_READ_WRITE IrpReadWrite,
    ULONG Flags
    )

/*++

Routine Description:

    This routine handles read/write IRP completion. It will perform any
    necessary flushes based on the type of I/O (as indicated by the flags) and
    destroy any temporary I/O buffers created for the operation during the
    prepare step.

Arguments:

    IrpReadWrite - Supplies a pointer to the read/write context for the
        completed IRP.

    Flags - Supplies a bitmask of flags for the completion. See
        IRP_READ_WRITE_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    PIO_BUFFER Buffer;
    PIRP_IO_BUFFER_STATE BufferState;
    UINTN BytesToFlush;
    BOOL FlushOriginal;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    PIO_BUFFER OriginalBuffer;
    ULONG StateFlags;
    KSTATUS Status;
    KSTATUS TotalStatus;

    Buffer = IrpReadWrite->IoBuffer;
    BufferState = &(IrpReadWrite->IoBufferState);
    FlushOriginal = FALSE;
    OriginalBuffer = BufferState->IoBuffer;
    StateFlags = BufferState->Flags;
    TotalStatus = STATUS_SUCCESS;
    if (OriginalBuffer == NULL) {
        OriginalBuffer = Buffer;
    }

    //
    // An I/O buffer used for DMA read needs to be invalidated. It was
    // invalidated before the DMA so that dirty data did not clobber the DMA,
    // but clean, prefetched data could be sitting in the cache preventing
    // access to the read data.
    //

    if (((Flags & IRP_READ_WRITE_FLAG_DMA) != 0) &&
        ((Flags & IRP_READ_WRITE_FLAG_WRITE) == 0) &&
        (IrpReadWrite->IoBytesCompleted != 0)) {

        BytesToFlush = IrpReadWrite->IoBytesCompleted;
        for (FragmentIndex = 0;
             FragmentIndex < Buffer->FragmentCount;
             FragmentIndex += 1) {

            Fragment = &(Buffer->Fragment[FragmentIndex]);
            Status = MmFlushBufferForDataIn(Fragment->VirtualAddress,
                                            Fragment->Size);

            if (!KSUCCESS(Status)) {
                TotalStatus = Status;
            }

            if (BytesToFlush < Fragment->Size) {
                break;
            }

            BytesToFlush -= Fragment->Size;
        }
    }

    //
    // Free the buffer used for I/O if it differs from the original.
    //

    if (OriginalBuffer != Buffer) {

        //
        // On a read operation, potentially copy the data back into the
        // original I/O buffer.
        //

        if (((Flags & IRP_READ_WRITE_FLAG_WRITE) == 0) &&
            ((StateFlags & IRP_IO_BUFFER_STATE_FLAG_LOCKED_COPY) == 0) &&
            (IrpReadWrite->IoBytesCompleted != 0)) {

            Status = MmCopyIoBuffer(OriginalBuffer,
                                    0,
                                    Buffer,
                                    0,
                                    IrpReadWrite->IoBytesCompleted);

            if (!KSUCCESS(Status)) {
                IrpReadWrite->IoBytesCompleted = 0;
                TotalStatus = Status;

            } else {
                FlushOriginal = TRUE;
            }
        }

        MmFreeIoBuffer(Buffer);
        IrpReadWrite->IoBuffer = OriginalBuffer;
        BufferState->IoBuffer = NULL;
        BufferState->Flags = 0;
    }

    //
    // The original I/O buffer always needs to be flushed for polled reads.
    // This is true even if a locked copy was created for the bounce buffer.
    //

    if (((Flags & IRP_READ_WRITE_FLAG_POLLED) != 0) &&
        ((Flags & IRP_READ_WRITE_FLAG_WRITE) == 0) &&
        (IrpReadWrite->IoBytesCompleted != 0)) {

        FlushOriginal = TRUE;
    }

    //
    // Flush the original I/O buffer to the point of unification. This is
    // necessary for polled reads and for all reads done to a bounce buffer in
    // case the original buffer is destined for execution.
    //

    if (FlushOriginal != FALSE) {
        for (FragmentIndex = 0;
             FragmentIndex < OriginalBuffer->FragmentCount;
             FragmentIndex += 1) {

            Fragment = &(OriginalBuffer->Fragment[FragmentIndex]);
            Status = MmSyncCacheRegion(Fragment->VirtualAddress,
                                       Fragment->Size);

            if (!KSUCCESS(Status)) {
                TotalStatus = Status;
            }
        }
    }

    return TotalStatus;
}

KSTATUS
IopSendStateChangeIrp (
    PDEVICE Device,
    IRP_MINOR_CODE MinorCode,
    PVOID IrpBody,
    UINTN IrpBodySize
    )

/*++

Routine Description:

    This routine sends a state change IRP.

Arguments:

    Device - Supplies a pointer to the device to send the IRP to.

    MinorCode - Supplies the IRP minor code.

    IrpBody - Supplies a pointer to a buffer that will be copied into the IRP
        data union on input. On output, this buffer will receive the altered
        data.

    IrpBodySize - Supplies the size of the IRP body in bytes.

Return Value:

    Status code.

--*/

{

    PIRP Irp;
    KSTATUS Status;

    Irp = IoCreateIrp(Device, IrpMajorStateChange, 0);
    if (Irp == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Irp->MinorCode = MinorCode;
    if (IrpBodySize != 0) {
        RtlCopyMemory(&(Irp->U), IrpBody, IrpBodySize);
    }

    Status = IoSendSynchronousIrp(Irp);
    if (KSUCCESS(Status)) {
        if (IrpBodySize != 0) {
            RtlCopyMemory(IrpBody, &(Irp->U), IrpBodySize);
        }

        Status = IoGetIrpStatus(Irp);
    }

    IoDestroyIrp(Irp);
    return Status;
}

KSTATUS
IopSendOpenIrp (
    PDEVICE Device,
    PIRP_OPEN OpenRequest
    )

/*++

Routine Description:

    This routine sends an open IRP.

Arguments:

    Device - Supplies a pointer to the device to send the IRP to.

    OpenRequest - Supplies a pointer that on input contains the open request
        parameters. The contents of the IRP will also be copied here upon
        returning.

Return Value:

    Status code.

--*/

{

    PIRP OpenIrp;
    KSTATUS Status;

    ASSERT((Device != NULL) && (Device != IoRootDevice));

    OpenIrp = NULL;
    KeAcquireSharedExclusiveLockShared(Device->Lock);
    if (Device->State == DeviceRemoved) {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto SendOpenIrpEnd;
    }

    OpenIrp = IoCreateIrp(Device, IrpMajorOpen, 0);
    if (OpenIrp == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendOpenIrpEnd;
    }

    //
    // Copy the supplied contents in and send the IRP.
    //

    OpenIrp->MinorCode = IrpMinorOpen;
    RtlCopyMemory(&(OpenIrp->U.Open), OpenRequest, sizeof(IRP_OPEN));
    Status = IoSendSynchronousIrp(OpenIrp);
    if (!KSUCCESS(Status)) {
        goto SendOpenIrpEnd;
    }

    //
    // Copy the result of the IRP back to the request structure.
    //

    RtlCopyMemory(OpenRequest, &(OpenIrp->U.Open), sizeof(IRP_OPEN));
    Status = IoGetIrpStatus(OpenIrp);

SendOpenIrpEnd:
    KeReleaseSharedExclusiveLockShared(Device->Lock);
    if (OpenIrp != NULL) {
        IoDestroyIrp(OpenIrp);
    }

    return Status;
}

KSTATUS
IopSendCloseIrp (
    PDEVICE Device,
    PIRP_CLOSE CloseRequest
    )

/*++

Routine Description:

    This routine sends a close IRP to the given device.

Arguments:

    Device - Supplies a pointer to the device to send the close IRP to.

    CloseRequest - Supplies a pointer to the IRP close context.

Return Value:

    Status code.

--*/

{

    PIRP CloseIrp;
    KSTATUS Status;

    CloseIrp = IoCreateIrp(Device, IrpMajorClose, 0);
    if (CloseIrp == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendCloseIrpEnd;
    }

    CloseIrp->MinorCode = IrpMinorClose;
    RtlCopyMemory(&(CloseIrp->U.Close), CloseRequest, sizeof(IRP_CLOSE));
    Status = IoSendSynchronousIrp(CloseIrp);
    if (!KSUCCESS(Status)) {
        goto SendCloseIrpEnd;
    }

    Status = IoGetIrpStatus(CloseIrp);

SendCloseIrpEnd:
    if (CloseIrp != NULL) {
        IoDestroyIrp(CloseIrp);
    }

    return Status;
}

KSTATUS
IopSendIoIrp (
    PDEVICE Device,
    IRP_MINOR_CODE MinorCodeNumber,
    PIRP_READ_WRITE Request
    )

/*++

Routine Description:

    This routine sends an I/O IRP.

Arguments:

    Device - Supplies a pointer to the device to send the IRP to.

    MinorCodeNumber - Supplies the minor code number to send to the IRP.

    Request - Supplies a pointer that on input contains the I/O request
        parameters.

Return Value:

    Status code.

--*/

{

    PIRP IoIrp;
    KSTATUS Status;
    PKTHREAD Thread;

    ASSERT((Device != NULL) && (Device != IoRootDevice));
    ASSERT(KeGetRunLevel() < RunLevelDispatch);

    IoIrp = IoCreateIrp(Device, IrpMajorIo, 0);
    if (IoIrp == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendIoIrpEnd;
    }

    Thread = KeGetCurrentThread();

    //
    // If this request came from servicing a page fault, then increment the
    // number of hard page faults.
    //

    if ((Request->IoFlags & IO_FLAG_SERVICING_FAULT) != 0) {
        Thread->ResourceUsage.HardPageFaults += 1;
        Request->IoFlags &= ~IO_FLAG_SERVICING_FAULT;
    }

    //
    // Copy the supplied contents in and send the IRP.
    //

    IoIrp->MinorCode = MinorCodeNumber;
    RtlCopyMemory(&(IoIrp->U.ReadWrite), Request, sizeof(IRP_READ_WRITE));
    IoIrp->U.ReadWrite.IoBufferState.IoBuffer = NULL;
    Status = IoSendSynchronousIrp(IoIrp);
    if (!KSUCCESS(Status)) {
        goto SendIoIrpEnd;
    }

    ASSERT(IoIrp->U.ReadWrite.IoBufferState.IoBuffer == NULL);

    RtlCopyMemory(Request, &(IoIrp->U.ReadWrite), sizeof(IRP_READ_WRITE));
    if (Device->Header.Type == ObjectDevice) {
        if (MinorCodeNumber == IrpMinorIoWrite) {
            RtlAtomicAdd64(&(IoGlobalStatistics.BytesWritten),
                           IoIrp->U.ReadWrite.IoBytesCompleted);

            Thread->ResourceUsage.BytesWritten +=
                                           IoIrp->U.ReadWrite.IoBytesCompleted;

            Thread->ResourceUsage.DeviceWrites += 1;

        } else {
            RtlAtomicAdd64(&(IoGlobalStatistics.BytesRead),
                           IoIrp->U.ReadWrite.IoBytesCompleted);

            Thread->ResourceUsage.BytesRead +=
                                           IoIrp->U.ReadWrite.IoBytesCompleted;

            Thread->ResourceUsage.DeviceReads += 1;
        }
    }

    Status = IoGetIrpStatus(IoIrp);

SendIoIrpEnd:
    if (IoIrp != NULL) {
        IoDestroyIrp(IoIrp);
    }

    return Status;
}

KSTATUS
IopSendIoReadIrp (
    PDEVICE Device,
    PIRP_READ_WRITE Request
    )

/*++

Routine Description:

    This routine sends an I/O read IRP to the given device. It makes sure that
    the bytes completed that are returned do not extend beyond the file size.
    Here the file size is that which is currently on the device and not in the
    system's cached view of the world.

Arguments:

    Device - Supplies a pointer to the device to send the IRP to.

    Request - Supplies a pointer that on input contains the I/O request
        parameters.

Return Value:

    Status code.

--*/

{

    PFILE_PROPERTIES FileProperties;
    ULONGLONG FileSize;
    KSTATUS Status;

    Status = IopSendIoIrp(Device, IrpMinorIoRead, Request);
    FileProperties = Request->FileProperties;
    FileSize = FileProperties->Size;
    if ((Request->IoOffset + Request->IoBytesCompleted) > FileSize) {
        if (Request->IoOffset > FileSize) {
            Request->IoBytesCompleted = 0;
            Request->NewIoOffset = Request->IoOffset;

        } else {

            ASSERT((FileSize - Request->IoOffset) <= MAX_UINTN);

            Request->IoBytesCompleted = FileSize - Request->IoOffset;
            Request->NewIoOffset = Request->IoOffset +
                                   Request->IoBytesCompleted;
        }
    }

    return Status;
}

KSTATUS
IopSendSystemControlIrp (
    PDEVICE Device,
    IRP_MINOR_CODE ControlNumber,
    PVOID SystemContext
    )

/*++

Routine Description:

    This routine sends a system control request to the given device. This
    routine must be called at low level.

Arguments:

    Device - Supplies a pointer to the device to send the system control
        request to.

    ControlNumber - Supplies the system control number to send.

    SystemContext - Supplies a pointer to the request details, which are
        dependent on the control number.

Return Value:

    Status code.

--*/

{

    PIRP Irp;
    BOOL LockHeld;
    KSTATUS Status;
    PVOLUME Volume;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Irp = NULL;
    LockHeld = FALSE;
    if (ControlNumber <= IrpMinorSystemControlInvalid) {
        Status = STATUS_INVALID_PARAMETER;
        goto SendSystemControlIrpEnd;
    }

    //
    // Synchronize this system control IRP with device removal.
    //

    KeAcquireSharedExclusiveLockShared(Device->Lock);
    LockHeld = TRUE;
    if (Device->State == DeviceRemoved) {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto SendSystemControlIrpEnd;
    }

    //
    // If the device is a volume, do not allow new root look-ups if it is about
    // to be removed. In fact, only allow the file properties to be flushed and
    // any lingering file objects to be deleted.
    //

    if (Device->Header.Type == ObjectVolume) {
        Volume = (PVOLUME)Device;
        if (((Volume->Flags & VOLUME_FLAG_UNMOUNTING) != 0) &&
            (ControlNumber != IrpMinorSystemControlWriteFileProperties) &&
            (ControlNumber != IrpMinorSystemControlDelete)) {

            Status = STATUS_DEVICE_NOT_CONNECTED;
            goto SendSystemControlIrpEnd;
        }
    }

    Irp = IoCreateIrp(Device, IrpMajorSystemControl, 0);
    if (Irp == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendSystemControlIrpEnd;
    }

    Irp->MinorCode = ControlNumber;
    Irp->U.SystemControl.SystemContext = SystemContext;
    Status = IoSendSynchronousIrp(Irp);
    if (!KSUCCESS(Status)) {
        goto SendSystemControlIrpEnd;
    }

    Status = IoGetIrpStatus(Irp);

SendSystemControlIrpEnd:
    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockShared(Device->Lock);
    }

    if (Irp != NULL) {
        IoDestroyIrp(Irp);
    }

    return Status;
}

KSTATUS
IopSendUserControlIrp (
    PIO_HANDLE Handle,
    ULONG MinorCode,
    BOOL FromKernelMode,
    PVOID UserContext,
    UINTN UserContextSize
    )

/*++

Routine Description:

    This routine sends a user control request to the device associated with
    the given handle. This routine must be called at low level.

Arguments:

    Handle - Supplies the open file handle.

    MinorCode - Supplies the minor code of the request.

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    UserContext - Supplies a pointer to the context buffer allocated by the
        caller for the request.

    UserContextSize - Supplies the size of the supplied context buffer.

Return Value:

    Status code.

--*/

{

    PDEVICE Device;
    PIRP Irp;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Device = Handle->FileObject->Device;

    ASSERT(Device->Header.Type == ObjectDevice);

    Irp = NULL;
    KeAcquireSharedExclusiveLockShared(Device->Lock);
    if (Device->State == DeviceRemoved) {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto SendUserControlIrpEnd;
    }

    Irp = IoCreateIrp(Device, IrpMajorUserControl, 0);
    if (Irp == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendUserControlIrpEnd;
    }

    Irp->MinorCode = MinorCode;
    Irp->U.UserControl.FromKernelMode = FromKernelMode;
    Irp->U.UserControl.DeviceContext = Handle->DeviceContext;
    Irp->U.UserControl.UserBuffer = UserContext;
    Irp->U.UserControl.UserBufferSize = UserContextSize;
    Status = IoSendSynchronousIrp(Irp);
    if (!KSUCCESS(Status)) {
        goto SendUserControlIrpEnd;
    }

    Status = IoGetIrpStatus(Irp);

SendUserControlIrpEnd:
    KeReleaseSharedExclusiveLockShared(Device->Lock);
    if (Irp != NULL) {
        IoDestroyIrp(Irp);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
IopPumpIrpThroughStack (
    PIRP_INTERNAL Irp
    )

/*++

Routine Description:

    This routine pumps an IRP through the device stack as far as it can take it
    towards completion. If a device pends the IRP, the function returns and can
    be called again when the IRP is completed.

Arguments:

    Irp - Supplies a pointer to the initialized IRP to pump through the drivers.

Return Value:

    TRUE if the IRP has completed its round trip.

    FALSE if the IRP still has more stack entries to go through.

--*/

{

    BOOL IrpDone;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    IrpDone = FALSE;
    while (IrpDone == FALSE) {

        //
        // Call the driver at the current stack location.
        //

        IopCallDriver(Irp);

        //
        // If this driver pended the IRP, stop processing.
        //

        if ((Irp->Flags & IRP_PENDING) != 0) {
            break;
        }

        //
        // Advance to the next driver.
        //

        IrpDone = IopAdvanceIrpStackLocation(Irp);
    }

    //
    // If the IRP is complete, call the completion routine.
    //

    if (IrpDone != FALSE) {

        ASSERT(((Irp->Flags & IRP_COMPLETE) != 0) ||
               (Irp->Public.Status == STATUS_NOT_HANDLED));

        ASSERT((Irp->Flags & IRP_PENDING) == 0);

        if (Irp->Public.CompletionRoutine != NULL) {
            Irp->Public.CompletionRoutine((PIRP)Irp,
                                          Irp->Public.CompletionContext);
        }
    }

    return IrpDone;
}

VOID
IopCallDriver (
    PIRP_INTERNAL Irp
    )

/*++

Routine Description:

    This routine calls the driver's dispatch routine for the given IRP.

Arguments:

    Irp - Supplies a pointer to the IRP to call the driver with.

Return Value:

    None.

--*/

{

    PVOID Context;
    PDRIVER_DISPATCH DispatchRoutine;
    PVOID DriverIrpContext;
    PDRIVER_STACK_ENTRY DriverStackEntry;
    PDRIVER_FUNCTION_TABLE FunctionTable;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);
    ASSERT(Irp->StackIndex < Irp->StackSize);

    DriverStackEntry = Irp->Stack[Irp->StackIndex].DriverStackEntry;
    Context = DriverStackEntry->DriverContext;
    DispatchRoutine = NULL;
    FunctionTable = &(DriverStackEntry->Driver->FunctionTable);

    //
    // Determine which dispatch routine to call based on the major code of the
    // IRP.
    //

    switch (Irp->MajorCode) {
    case IrpMajorStateChange:
        DispatchRoutine = FunctionTable->DispatchStateChange;
        break;

    case IrpMajorOpen:
        DispatchRoutine = FunctionTable->DispatchOpen;
        break;

    case IrpMajorClose:
        DispatchRoutine = FunctionTable->DispatchClose;
        break;

    case IrpMajorIo:
        DispatchRoutine = FunctionTable->DispatchIo;
        break;

    case IrpMajorSystemControl:
        DispatchRoutine = FunctionTable->DispatchSystemControl;
        break;

    case IrpMajorUserControl:
        DispatchRoutine = FunctionTable->DispatchUserControl;
        break;

    //
    // In the default case, there is nothing to call, since the IRP seems to be
    // invalid.
    //

    default:
        KeCrashSystem(CRASH_INVALID_IRP,
                      IrpCrashCorruption,
                      Irp->MajorCode,
                      (UINTN)Irp,
                      0);

        break;
    }

    //
    // Call the driver.
    //

    if (DispatchRoutine != NULL) {
        DriverIrpContext = Irp->Stack[Irp->StackIndex].IrpContext;
        DispatchRoutine((PIRP)Irp, Context, DriverIrpContext);
    }

    return;
}

BOOL
IopAdvanceIrpStackLocation (
    PIRP_INTERNAL Irp
    )

/*++

Routine Description:

    This routine determines what the next driver stack entry would be for the
    given IRP, and advances the IRP's state.

Arguments:

    Irp - Supplies a pointer to the IRP whose next stack location should be
        returned.

Return Value:

    TRUE if the IRP has completed its round trip.

    FALSE if the IRP still has more stack entries to go through.

--*/

{

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    //
    // If the IRP is going down, send it down more. If it hits the end of the
    // list, reverse the direction and send to the same driver as last time.
    //

    if (Irp->Public.Direction == IrpDown) {
        if (Irp->StackIndex + 1 < Irp->StackSize) {
            Irp->StackIndex += 1;

        } else {
            Irp->Public.Direction = IrpUp;
        }

        return FALSE;
    }

    //
    // The IRP must be going back up. If it's not at zero yet, move it along.
    // If it is at zero, it's done.
    //

    ASSERT(Irp->Public.Direction == IrpUp);

    if (Irp->StackIndex == 0) {
        return TRUE;
    }

    Irp->StackIndex -= 1;
    return FALSE;
}

