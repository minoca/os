/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ata.c

Abstract:

    This module implements the AT Attachment (ATA) driver.

Author:

    Evan Green 4-Jun-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/intrface/disk.h>
#include <minoca/intrface/pci.h>
#include <minoca/storage/ata.h>
#include "ata.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns the correct time counter function depending on whether
// the operation is occurring in critical mode or not.
//

#define ATA_GET_TIME_FUNCTION(_CriticalMode) \
    ((_CriticalMode) ? HlQueryTimeCounter : KeGetRecentTimeCounter)

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
ULONGLONG
(*PATA_QUERY_TIME_COUNTER) (
    VOID
    );

/*++

Routine Description:

    This routine returns snap of the time counter.

Arguments:

    None.

Return Value:

    Returns a snap of the time counter.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
AtapServiceInterruptForChannel (
    PATA_CHANNEL Channel,
    ULONG PendingBits
    );

KSTATUS
AtaAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
AtaDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
AtaDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
AtaDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
AtaDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
AtaDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
AtaInterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
AtaInterruptServiceDpc (
    PVOID Context
    );

VOID
AtapDispatchControllerStateChange (
    PIRP Irp,
    PATA_CONTROLLER Controller
    );

VOID
AtapDispatchChildStateChange (
    PIRP Irp,
    PATA_CHILD Child
    );

VOID
AtapDispatchChildSystemControl (
    PIRP Irp,
    PATA_CHILD Device
    );

KSTATUS
AtapProcessResourceRequirements (
    PIRP Irp,
    PATA_CONTROLLER Controller
    );

KSTATUS
AtapStartController (
    PIRP Irp,
    PATA_CONTROLLER Controller
    );

KSTATUS
AtapResetController (
    PATA_CONTROLLER Controller
    );

VOID
AtapEnumerateDrives (
    PIRP Irp,
    PATA_CONTROLLER Controller
    );

KSTATUS
AtapIdentifyDevice (
    PATA_CHILD Device
    );

KSTATUS
AtapPerformDmaIo (
    PIRP Irp,
    PATA_CHILD Device,
    BOOL HaveDpcLock
    );

KSTATUS
AtapPerformPolledIo (
    PIRP_READ_WRITE Irp,
    PATA_CHILD Device,
    BOOL Write,
    BOOL CriticalMode
    );

KSTATUS
AtapSynchronizeDevice (
    PATA_CHILD Device
    );

KSTATUS
AtapBlockRead (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    );

KSTATUS
AtapBlockWrite (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    );

KSTATUS
AtapReadWriteSectorsPio (
    PATA_CHILD AtaDevice,
    ULONGLONG BlockAddress,
    UINTN SectorCount,
    PVOID Buffer,
    BOOL Write,
    BOOL CriticalMode
    );

KSTATUS
AtapPioCommand (
    PATA_CHILD Device,
    ATA_COMMAND Command,
    BOOL Lba48,
    BOOL Write,
    ULONG Features,
    ULONGLONG Lba,
    PVOID Buffer,
    ULONG SectorCount,
    ULONG MultiCount,
    BOOL CriticalMode
    );

KSTATUS
AtapExecuteCacheFlush (
    PATA_CHILD Child,
    BOOL CriticalMode
    );

KSTATUS
AtapSelectDevice (
    PATA_CHILD Device,
    BOOL CriticalMode
    );

VOID
AtapSetupCommand (
    PATA_CHILD Device,
    BOOL Lba48,
    ULONG FeaturesRegister,
    ULONG SectorCountRegister,
    ULONGLONG Lba,
    ULONG DeviceControl
    );

VOID
AtapStall (
    PATA_CHANNEL Channel
    );

UCHAR
AtapReadRegister (
    PATA_CHANNEL Channel,
    ATA_REGISTER Register
    );

VOID
AtapWriteRegister (
    PATA_CHANNEL Channel,
    ATA_REGISTER Register,
    UCHAR Value
    );

VOID
AtapProcessPciConfigInterfaceChangeNotification (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER AtaDriver = NULL;
UUID AtaPciConfigurationInterfaceUuid = UUID_PCI_CONFIG_ACCESS;
UUID AtaDiskInterfaceUuid = UUID_DISK_INTERFACE;

DISK_INTERFACE AtaDiskInterfaceTemplate = {
    DISK_INTERFACE_VERSION,
    NULL,
    ATA_SECTOR_SIZE,
    0,
    NULL,
    NULL,
    AtapBlockRead,
    AtapBlockWrite
};

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the ATA driver. It registers its other
    dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    AtaDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = AtaAddDevice;
    FunctionTable.DispatchStateChange = AtaDispatchStateChange;
    FunctionTable.DispatchOpen = AtaDispatchOpen;
    FunctionTable.DispatchClose = AtaDispatchClose;
    FunctionTable.DispatchIo = AtaDispatchIo;
    FunctionTable.DispatchSystemControl = AtaDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
AtaAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the ATA device
    acts as the function driver. The driver will attach itself to the stack.

Arguments:

    Driver - Supplies a pointer to the driver being called.

    DeviceId - Supplies a pointer to a string with the device ID.

    ClassId - Supplies a pointer to a string containing the device's class ID.

    CompatibleIds - Supplies a pointer to a string containing device IDs
        that would be compatible with this device.

    DeviceToken - Supplies an opaque token that the driver can use to identify
        the device in the system. This token should be used when attaching to
        the stack.

Return Value:

    STATUS_SUCCESS on success.

    Failure code if the driver was unsuccessful in attaching itself.

--*/

{

    PATA_CONTROLLER Controller;
    UINTN Index;
    ULONG IoBufferFlags;
    PVOID Prdt;
    PHYSICAL_ADDRESS PrdtPhysical;
    KSTATUS Status;

    Controller = MmAllocateNonPagedPool(sizeof(ATA_CONTROLLER),
                                        ATA_ALLOCATION_TAG);

    if (Controller == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    RtlZeroMemory(Controller, sizeof(ATA_CONTROLLER));
    KeInitializeSpinLock(&(Controller->DpcLock));
    Controller->Type = AtaControllerContext;
    Controller->PrimaryInterruptHandle = INVALID_HANDLE;
    Controller->SecondaryInterruptHandle = INVALID_HANDLE;

    //
    // Allocate a page for the PRDT.
    //

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS |
                    IO_BUFFER_FLAG_MAP_NON_CACHED;

    Controller->PrdtIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                          MAX_ULONG,
                                                          ATA_PRDT_TOTAL_SIZE,
                                                          ATA_PRDT_TOTAL_SIZE,
                                                          IoBufferFlags);

    if (Controller->PrdtIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    ASSERT(Controller->PrdtIoBuffer->FragmentCount == 1);

    Prdt = Controller->PrdtIoBuffer->Fragment[0].VirtualAddress;
    PrdtPhysical = Controller->PrdtIoBuffer->Fragment[0].PhysicalAddress;

    //
    // Initialize the two channels, and then the four child contexts.
    //

    for (Index = 0; Index < ATA_CABLE_COUNT; Index += 1) {
        Controller->Channel[Index].Lock = KeCreateQueuedLock();
        if (Controller->Channel[Index].Lock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AddDeviceEnd;
        }

        Controller->Channel[Index].SelectedDevice = 0xFF;
        Controller->Channel[Index].Prdt = Prdt;
        Controller->Channel[Index].PrdtPhysicalAddress = PrdtPhysical;
        Prdt += ATA_PRDT_DISK_SIZE;
        PrdtPhysical += ATA_PRDT_DISK_SIZE;
    }

    for (Index = 0; Index < ATA_CHILD_COUNT; Index += 1) {
        Controller->ChildContexts[Index].Type = AtaChildContext;
        Controller->ChildContexts[Index].Controller = Controller;
        Controller->ChildContexts[Index].Channel =
                                            &(Controller->Channel[Index >> 1]);

        if ((Index & 0x1) != 0) {
            Controller->ChildContexts[Index].Slave = ATA_DRIVE_SELECT_SLAVE;

        } else {
            Controller->ChildContexts[Index].Slave = ATA_DRIVE_SELECT_MASTER;
        }
    }

    Status = IoAttachDriverToDevice(Driver, DeviceToken, Controller);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    Status = STATUS_SUCCESS;

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Controller != NULL) {
            for (Index = 0; Index < ATA_CABLE_COUNT; Index += 1) {
                if (Controller->Channel[Index].Lock != NULL) {
                    KeDestroyQueuedLock(Controller->Channel[Index].Lock);
                }
            }

            if (Controller->PrdtIoBuffer != NULL) {
                MmFreeIoBuffer(Controller->PrdtIoBuffer);
            }

            MmFreeNonPagedPool(Controller);
        }
    }

    return Status;
}

VOID
AtaDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles State Change IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PATA_CONTROLLER Controller;

    Controller = DeviceContext;
    switch (Controller->Type) {
    case AtaControllerContext:
        AtapDispatchControllerStateChange(Irp, Controller);
        break;

    case AtaChildContext:
        AtapDispatchChildStateChange(Irp, (PATA_CHILD)Controller);
        break;

    default:

        ASSERT(FALSE);

        IoCompleteIrp(AtaDriver, Irp, STATUS_INVALID_CONFIGURATION);
        break;
    }

    return;
}

VOID
AtaDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Open IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PATA_CHILD Disk;

    //
    // Only the disk can be opened or closed.
    //

    Disk = (PATA_CHILD)DeviceContext;
    if (Disk->Type != AtaChildContext) {
        return;
    }

    Irp->U.Open.DeviceContext = Disk;
    IoCompleteIrp(AtaDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
AtaDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Close IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PATA_CHILD Disk;

    //
    // Only the disk can be opened or closed.
    //

    Disk = (PATA_CHILD)DeviceContext;
    if (Disk->Type != AtaChildContext) {
        return;
    }

    Irp->U.Open.DeviceContext = Disk;
    IoCompleteIrp(AtaDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
AtaDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles I/O IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    BOOL CompleteIrp;
    PATA_CHILD Device;
    ULONG IrpReadWriteFlags;
    BOOL PmReferenceAdded;
    KSTATUS Status;
    BOOL Write;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Device = (PATA_CHILD)Irp->U.ReadWrite.DeviceContext;
    if (Device->Type != AtaChildContext) {
        return;
    }

    CompleteIrp = TRUE;
    Write = FALSE;
    if (Irp->MinorCode == IrpMinorIoWrite) {
        Write = TRUE;
    }

    //
    // If this IRP is on the way down, always add a power management reference.
    //

    PmReferenceAdded = FALSE;
    if (Irp->Direction == IrpDown) {
        Status = PmDeviceAddReference(Device->OsDevice);
        if (!KSUCCESS(Status)) {
            goto DispatchIoEnd;
        }

        PmReferenceAdded = TRUE;
    }

    //
    // Polled I/O is shared by a few code paths and prepares the IRP for I/O
    // further down the stack. It should also only be hit in the down direction
    // path as it always completes the IRP.
    //

    if (Device->DmaSupported == FALSE) {

        ASSERT(Irp->Direction == IrpDown);

        Status = AtapPerformPolledIo(&(Irp->U.ReadWrite), Device, Write, FALSE);
        goto DispatchIoEnd;
    }

    //
    // Set the IRP read/write flags for the preparation and completion steps.
    //

    IrpReadWriteFlags = IRP_READ_WRITE_FLAG_DMA;
    if (Write != FALSE) {
        IrpReadWriteFlags |= IRP_READ_WRITE_FLAG_WRITE;
    }

    //
    // If the IRP is on the way up, then clean up after the DMA as this IRP is
    // still sitting in the channel. An IRP going up is already complete.
    //

    if (Irp->Direction == IrpUp) {
        CompleteIrp = FALSE;

        ASSERT(Irp == Device->Channel->Irp);
        ASSERT(Device == Device->Channel->OwningChild);
        ASSERT(KeIsQueuedLockHeld(Device->Channel->Lock) != FALSE);

        Device->Channel->OwningChild = NULL;
        Device->Channel->Irp = NULL;
        KeReleaseQueuedLock(Device->Channel->Lock);
        PmDeviceReleaseReference(Device->OsDevice);
        Status = IoCompleteReadWriteIrp(&(Irp->U.ReadWrite), IrpReadWriteFlags);
        if (!KSUCCESS(Status)) {
            IoUpdateIrpStatus(Irp, Status);
        }

    //
    // Start the DMA on the way down.
    //

    } else {
        Irp->U.ReadWrite.NewIoOffset = Irp->U.ReadWrite.IoOffset;

        //
        // Before acquiring the channel's lock and starting the DMA, prepare
        // the I/O context for ATA (i.e. it must use physical addresses that
        // are less than 4GB and be sector size aligned).
        //

        Status = IoPrepareReadWriteIrp(&(Irp->U.ReadWrite),
                                       ATA_SECTOR_SIZE,
                                       0,
                                       MAX_ULONG,
                                       IrpReadWriteFlags);

        if (!KSUCCESS(Status)) {
            goto DispatchIoEnd;
        }

        //
        // Fire off the DMA. If this succeeds, it will have pended the IRP.
        // Return with the lock held.
        //

        KeAcquireQueuedLock(Device->Channel->Lock);
        Device->Channel->Irp = Irp;
        Device->Channel->OwningChild = Device;
        CompleteIrp = FALSE;
        Status = AtapPerformDmaIo(Irp, Device, FALSE);
        if (!KSUCCESS(Status)) {
            Device->Channel->OwningChild = NULL;
            Device->Channel->Irp = NULL;
            KeReleaseQueuedLock(Device->Channel->Lock);
            IoCompleteReadWriteIrp(&(Irp->U.ReadWrite), IrpReadWriteFlags);
            CompleteIrp = TRUE;
        }
    }

DispatchIoEnd:
    if (CompleteIrp != FALSE) {
        if (PmReferenceAdded != FALSE) {
            PmDeviceReleaseReference(Device->OsDevice);
        }

        IoCompleteIrp(AtaDriver, Irp, Status);
    }

    return;
}

VOID
AtaDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles System Control IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PATA_CHILD Child;

    ASSERT(Irp->MajorCode == IrpMajorSystemControl);

    Child = (PATA_CHILD)DeviceContext;
    if (Child->Type == AtaChildContext) {
        AtapDispatchChildSystemControl(Irp, Child);
    }

    return;
}

INTERRUPT_STATUS
AtaInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the ATA interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the ATA
        controller.

Return Value:

    Interrupt status.

--*/

{

    ULONG BusMasterStatus;
    PATA_CONTROLLER Controller;
    INTERRUPT_STATUS InterruptStatus;

    InterruptStatus = InterruptStatusNotClaimed;
    Controller = (PATA_CONTROLLER)Context;

    //
    // Check the primary channel's bus master status.
    //

    BusMasterStatus = AtapReadRegister(&(Controller->Channel[0]),
                                       AtaRegisterBusMasterStatus);

    BusMasterStatus &= IDE_STATUS_INTERRUPT | IDE_STATUS_ERROR;
    if (BusMasterStatus != 0) {
        AtapWriteRegister(&(Controller->Channel[0]),
                          AtaRegisterBusMasterStatus,
                          BusMasterStatus);

        AtapWriteRegister(&(Controller->Channel[0]),
                          AtaRegisterBusMasterCommand,
                          0);

    //
    // Try the secondary one.
    //

    } else {
        BusMasterStatus = AtapReadRegister(&(Controller->Channel[1]),
                                           AtaRegisterBusMasterStatus);

        BusMasterStatus &= IDE_STATUS_INTERRUPT | IDE_STATUS_ERROR;
        if (BusMasterStatus != 0) {
            AtapWriteRegister(&(Controller->Channel[1]),
                              AtaRegisterBusMasterStatus,
                              BusMasterStatus);

            AtapWriteRegister(&(Controller->Channel[1]),
                              AtaRegisterBusMasterCommand,
                              0);

            BusMasterStatus <<= BITS_PER_BYTE;
        }
    }

    if (BusMasterStatus != 0) {
        RtlAtomicOr32(&(Controller->PendingStatusBits), BusMasterStatus);
        InterruptStatus = InterruptStatusClaimed;
    }

    return InterruptStatus;
}

INTERRUPT_STATUS
AtaInterruptServiceDpc (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the ATA dispatch-level interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the ATA
        controller.

Return Value:

    Interrupt status.

--*/

{

    UCHAR BusMasterMask;
    PATA_CONTROLLER Device;
    ULONG PendingBits;

    Device = (PATA_CONTROLLER)Context;

    //
    // Clear out the pending bits.
    //

    PendingBits = RtlAtomicExchange32(&(Device->PendingStatusBits), 0);
    if (PendingBits == 0) {
        return InterruptStatusNotClaimed;
    }

    KeAcquireSpinLock(&(Device->DpcLock));

    //
    // Handle the primary controller.
    //

    BusMasterMask = IDE_STATUS_ERROR | IDE_STATUS_INTERRUPT;
    if ((PendingBits & BusMasterMask) != 0) {
        AtapServiceInterruptForChannel(&(Device->Channel[0]),
                                       PendingBits & BusMasterMask);
    }

    //
    // Handle the secondary controller.
    //

    PendingBits >>= 8;
    if ((PendingBits & BusMasterMask) != 0) {
        AtapServiceInterruptForChannel(&(Device->Channel[1]),
                                       PendingBits & BusMasterMask);
    }

    KeReleaseSpinLock(&(Device->DpcLock));
    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
AtapServiceInterruptForChannel (
    PATA_CHANNEL Channel,
    ULONG PendingBits
    )

/*++

Routine Description:

    This routine services an interrupt for a given ATA channel.

Arguments:

    Channel - Supplies a pointer to the channel.

    PendingBits - Supplies the pending bitmask.

Return Value:

    None.

--*/

{

    BOOL CompleteIrp;
    UINTN IoSize;
    PIRP Irp;
    KSTATUS Status;
    UCHAR StatusRegister;

    Irp = Channel->Irp;
    if ((Irp != NULL) && (PendingBits != 0) && (Channel->IoSize != 0)) {
        IoSize = Channel->IoSize;
        Channel->IoSize = 0;
        Status = STATUS_SUCCESS;
        CompleteIrp = FALSE;
        StatusRegister = AtapReadRegister(Channel, AtaRegisterStatus);
        if (((PendingBits & IDE_STATUS_ERROR) != 0) ||
            ((StatusRegister & ATA_STATUS_ERROR_MASK) != 0)) {

            RtlDebugPrint("ATA: I/O Error: Status 0x%x, BMStatus 0x%x.\n",
                          StatusRegister,
                          PendingBits);

            Status = STATUS_DEVICE_IO_ERROR;
            CompleteIrp = FALSE;

        } else if ((PendingBits & IDE_STATUS_INTERRUPT) != 0) {
            CompleteIrp = TRUE;

            ASSERT(Irp->MajorCode == IrpMajorIo);

            Irp->U.ReadWrite.IoBytesCompleted += IoSize;
            Irp->U.ReadWrite.NewIoOffset += IoSize;

            ASSERT(Irp->U.ReadWrite.IoBytesCompleted <=
                   Irp->U.ReadWrite.IoSizeInBytes);

            if (Irp->U.ReadWrite.IoBytesCompleted !=
                Irp->U.ReadWrite.IoSizeInBytes) {

                Status = AtapPerformDmaIo(Irp, Channel->OwningChild, TRUE);
                if (KSUCCESS(Status)) {
                    CompleteIrp = FALSE;
                }
            }
        }

        if (CompleteIrp != FALSE) {

            //
            // If this is a synchronized write, then send a cache flush
            // command along with it.
            //

            if ((Status == STATUS_SUCCESS) &&
                (Irp->MinorCode == IrpMinorIoWrite) &&
                ((Irp->U.ReadWrite.IoFlags & IO_FLAG_DATA_SYNCHRONIZED) != 0)) {

                Status = AtapExecuteCacheFlush(Channel->OwningChild, FALSE);

                ASSERT(KSUCCESS(Status));
            }

            //
            // If successful, the I/O should be completed fully.
            //

            ASSERT((!KSUCCESS(Status)) ||
                   (Irp->U.ReadWrite.IoBytesCompleted ==
                    Irp->U.ReadWrite.IoSizeInBytes));

            //
            // Complete the IRP but do not release the lock as the channel is
            // cleaned up by this driver after the IRP is reversed to the up
            // direction. This allows it to perform said clean up at low level.
            //

            IoCompleteIrp(AtaDriver, Irp, Status);
        }
    }

    return;
}

VOID
AtapDispatchControllerStateChange (
    PIRP Irp,
    PATA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine handles state change IRPs for an ATA controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Controller - Supplies a pointer to the controller context.

Return Value:

    None. The routine completes the IRP if appropriate.

--*/

{

    KSTATUS Status;

    if (Irp->Direction == IrpUp) {
        if (!KSUCCESS(IoGetIrpStatus(Irp))) {
            return;
        }

        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            Status = AtapProcessResourceRequirements(Irp, Controller);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(AtaDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = AtapStartController(Irp, Controller);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(AtaDriver, Irp, Status);
            }

            break;

        case IrpMinorQueryChildren:
            AtapEnumerateDrives(Irp, Controller);
            break;

        case IrpMinorIdle:
        case IrpMinorSuspend:
        case IrpMinorResume:
        default:
            break;
        }
    }

    return;
}

VOID
AtapDispatchChildStateChange (
    PIRP Irp,
    PATA_CHILD Child
    )

/*++

Routine Description:

    This routine handles state change IRPs for an ATA child device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Child - Supplies a pointer to the child device.

Return Value:

    None. The routine completes the IRP if appropriate.

--*/

{

    KSTATUS Status;

    if (Irp->Direction == IrpDown) {
        switch (Irp->MinorCode) {
        case IrpMinorStartDevice:
            Child->OsDevice = Irp->Device;
            Status = PmInitialize(Irp->Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(AtaDriver, Irp, Status);
                break;
            }

            //
            // Publish the disk interface.
            //

            RtlCopyMemory(&(Child->DiskInterface),
                          &AtaDiskInterfaceTemplate,
                          sizeof(DISK_INTERFACE));

            Status = STATUS_SUCCESS;
            if (Child->DiskInterface.DiskToken == NULL) {
                Child->DiskInterface.DiskToken = Child;
                Child->DiskInterface.BlockCount = Child->TotalSectors;
                Status = IoCreateInterface(&AtaDiskInterfaceUuid,
                                           Irp->Device,
                                           &(Child->DiskInterface),
                                           sizeof(DISK_INTERFACE));

                if (!KSUCCESS(Status)) {
                    Child->DiskInterface.DiskToken = NULL;
                }
            }

            IoCompleteIrp(AtaDriver, Irp, Status);
            break;

        case IrpMinorQueryResources:
        case IrpMinorQueryChildren:
            IoCompleteIrp(AtaDriver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorIdle:
            IoCompleteIrp(AtaDriver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorSuspend:
            IoCompleteIrp(AtaDriver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorResume:
            IoCompleteIrp(AtaDriver, Irp, STATUS_SUCCESS);
            break;

        default:
            break;
        }
    }

    return;
}

VOID
AtapDispatchChildSystemControl (
    PIRP Irp,
    PATA_CHILD Device
    )

/*++

Routine Description:

    This routine handles System Control IRPs for an ATA child device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    PVOID Context;
    PSYSTEM_CONTROL_FILE_OPERATION FileOperation;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    PFILE_PROPERTIES Properties;
    ULONGLONG PropertiesFileSize;
    KSTATUS Status;

    Context = Irp->U.SystemControl.SystemContext;
    switch (Irp->MinorCode) {
    case IrpMinorSystemControlLookup:
        Lookup = (PSYSTEM_CONTROL_LOOKUP)Context;
        Status = STATUS_PATH_NOT_FOUND;
        if (Lookup->Root != FALSE) {

            //
            // Enable opening of the root as a single file.
            //

            Properties = Lookup->Properties;
            Properties->FileId = 0;
            Properties->Type = IoObjectBlockDevice;
            Properties->HardLinkCount = 1;
            Properties->BlockSize = ATA_SECTOR_SIZE;
            Properties->BlockCount = Device->TotalSectors;
            Properties->Size = Device->TotalSectors * ATA_SECTOR_SIZE;
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(AtaDriver, Irp, Status);
        break;

    //
    // Writes to the disk's properties are not allowed. Fail if the data
    // has changed.
    //

    case IrpMinorSystemControlWriteFileProperties:
        FileOperation = (PSYSTEM_CONTROL_FILE_OPERATION)Context;
        Properties = FileOperation->FileProperties;
        PropertiesFileSize = Properties->Size;
        if ((Properties->FileId != 0) ||
            (Properties->Type != IoObjectBlockDevice) ||
            (Properties->HardLinkCount != 1) ||
            (Properties->BlockSize != ATA_SECTOR_SIZE) ||
            (Properties->BlockCount != Device->TotalSectors) ||
            (PropertiesFileSize != (Device->TotalSectors * ATA_SECTOR_SIZE))) {

            Status = STATUS_NOT_SUPPORTED;

        } else {
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(AtaDriver, Irp, Status);
        break;

    //
    // Do not support hard disk device truncation.
    //

    case IrpMinorSystemControlTruncate:
        IoCompleteIrp(AtaDriver, Irp, STATUS_NOT_SUPPORTED);
        break;

    //
    // Gather and return device information.
    //

    case IrpMinorSystemControlDeviceInformation:
        break;

    //
    // Send a cache flush command to the device upon getting a synchronize
    // request.
    //

    case IrpMinorSystemControlSynchronize:
        Status = PmDeviceAddReference(Device->OsDevice);
        if (!KSUCCESS(Status)) {
            IoCompleteIrp(AtaDriver, Irp, Status);
            break;
        }

        Status = AtapSynchronizeDevice(Device);
        PmDeviceReleaseReference(Device->OsDevice);
        IoCompleteIrp(AtaDriver, Irp, Status);
        break;

    //
    // Ignore everything unrecognized.
    //

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

KSTATUS
AtapProcessResourceRequirements (
    PIRP Irp,
    PATA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an ATA controller. It adds an interrupt vector requirement for
    any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Controller - Supplies a pointer to the ATA controller.

Return Value:

    Status code.

--*/

{

    ULONGLONG Interface;
    RESOURCE_REQUIREMENT LegacyRequirement;
    ULONGLONG LineCharacteristics;
    PRESOURCE_REQUIREMENT NewRequirement;
    BOOL PrimaryLegacy;
    PRESOURCE_REQUIREMENT Requirement;
    PRESOURCE_REQUIREMENT_LIST RequirementList;
    PRESOURCE_CONFIGURATION_LIST Requirements;
    BOOL SecondaryLegacy;
    KSTATUS Status;
    ULONGLONG VectorCharacteristics;
    RESOURCE_REQUIREMENT VectorRequirement;

    ASSERT((Irp->MajorCode == IrpMajorStateChange) &&
           (Irp->MinorCode == IrpMinorQueryResources));

    Requirements = Irp->U.QueryResources.ResourceRequirements;
    if (Requirements == NULL) {
        Status = STATUS_NOT_CONFIGURED;
        goto ProcessResourceRequirementsEnd;
    }

    RequirementList = IoGetNextResourceConfiguration(Requirements, NULL);

    //
    // Start listening for a PCI config interface.
    //

    if (Controller->RegisteredForPciConfigInterfaces == FALSE) {
        Status = IoRegisterForInterfaceNotifications(
                              &AtaPciConfigurationInterfaceUuid,
                              AtapProcessPciConfigInterfaceChangeNotification,
                              Irp->Device,
                              Controller,
                              TRUE);

        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }

        Controller->RegisteredForPciConfigInterfaces = TRUE;
    }

    //
    // Try to read the interface from PCI.
    //

    if (Controller->PciConfigInterfaceAvailable == FALSE) {
        Status = STATUS_NOT_CONFIGURED;
        goto ProcessResourceRequirementsEnd;
    }

    Status = Controller->PciConfigInterface.ReadPciConfig(
                                    Controller->PciConfigInterface.DeviceToken,
                                    IDE_INTERFACE_OFFSET,
                                    IDE_INTERFACE_SIZE,
                                    &Interface);

    if (!KSUCCESS(Status)) {
        goto ProcessResourceRequirementsEnd;
    }

    Controller->Interface = Interface;

    //
    // Look to see if the interface is in native or legacy mode.
    //

    PrimaryLegacy = TRUE;
    SecondaryLegacy = TRUE;
    if ((Interface & IDE_INTERFACE_PRIMARY_NATIVE_SUPPORTED) != 0) {
        if ((Interface & IDE_INTERFACE_PRIMARY_NATIVE_ENABLED) != 0) {
            PrimaryLegacy = FALSE;
        }
    }

    if ((Interface & IDE_INTERFACE_SECONDARY_NATIVE_SUPPORTED) != 0) {
        if ((Interface & IDE_INTERFACE_SECONDARY_NATIVE_ENABLED) != 0) {
            SecondaryLegacy = FALSE;
        }
    }

    //
    // Add the primary legacy region if this controller is using that.
    //

    if (PrimaryLegacy != FALSE) {
        RtlZeroMemory(&LegacyRequirement, sizeof(RESOURCE_REQUIREMENT));
        LegacyRequirement.Type = ResourceTypeIoPort;
        LegacyRequirement.Flags = RESOURCE_FLAG_NOT_SHAREABLE;
        LegacyRequirement.Minimum = ATA_LEGACY_PRIMARY_IO_BASE;
        LegacyRequirement.Length = ATA_LEGACY_IO_SIZE;
        LegacyRequirement.Maximum = LegacyRequirement.Minimum +
                                    LegacyRequirement.Length;

        Status = IoCreateAndAddResourceRequirement(&LegacyRequirement,
                                                   RequirementList,
                                                   NULL);

        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }

        LegacyRequirement.Minimum = ATA_LEGACY_PRIMARY_CONTROL_BASE;
        LegacyRequirement.Length = ATA_LEGACY_CONTROL_SIZE;
        LegacyRequirement.Maximum = LegacyRequirement.Minimum +
                                    LegacyRequirement.Length;

        Status = IoCreateAndAddResourceRequirement(&LegacyRequirement,
                                                   RequirementList,
                                                   NULL);

        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }
    }

    //
    // Add the secondary legacy region if this controller is using that.
    //

    if (SecondaryLegacy != FALSE) {
        RtlZeroMemory(&LegacyRequirement, sizeof(RESOURCE_REQUIREMENT));
        LegacyRequirement.Type = ResourceTypeIoPort;
        LegacyRequirement.Flags = RESOURCE_FLAG_NOT_SHAREABLE;
        LegacyRequirement.Minimum = ATA_LEGACY_SECONDARY_IO_BASE;
        LegacyRequirement.Length = ATA_LEGACY_IO_SIZE;
        LegacyRequirement.Maximum = LegacyRequirement.Minimum +
                                    LegacyRequirement.Length;

        Status = IoCreateAndAddResourceRequirement(&LegacyRequirement,
                                                   RequirementList,
                                                   NULL);

        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }

        LegacyRequirement.Minimum = ATA_LEGACY_SECONDARY_CONTROL_BASE;
        LegacyRequirement.Length = ATA_LEGACY_CONTROL_SIZE;
        LegacyRequirement.Maximum = LegacyRequirement.Minimum +
                                    LegacyRequirement.Length;

        Status = IoCreateAndAddResourceRequirement(&LegacyRequirement,
                                                   RequirementList,
                                                   NULL);

        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }
    }

    //
    // Initialize a nice interrupt vector requirement in preparation.
    //

    RtlZeroMemory(&VectorRequirement, sizeof(RESOURCE_REQUIREMENT));
    VectorRequirement.Type = ResourceTypeInterruptVector;
    VectorRequirement.Minimum = 0;
    VectorRequirement.Maximum = -1;
    VectorRequirement.Length = 1;

    //
    // Loop through all configuration lists adding a vector for each line.
    //

    while (RequirementList != NULL) {

        //
        // Loop through every requirement in the list.
        //

        Requirement = IoGetNextResourceRequirement(RequirementList, NULL);
        while (Requirement != NULL) {

            //
            // If the requirement is an interrupt line, then add a requirement
            // for a vector as well. If legacy vectors are going to be added,
            // then just remember there's an extra interrupt line there.
            //

            if (Requirement->Type == ResourceTypeInterruptLine) {
                if ((PrimaryLegacy == FALSE) || (SecondaryLegacy == FALSE)) {
                    VectorCharacteristics = 0;
                    LineCharacteristics = Requirement->Characteristics;
                    if ((LineCharacteristics &
                         INTERRUPT_LINE_ACTIVE_LOW) != 0) {

                        VectorCharacteristics |= INTERRUPT_VECTOR_ACTIVE_LOW;
                    }

                    if ((LineCharacteristics &
                         INTERRUPT_LINE_EDGE_TRIGGERED) != 0) {

                        VectorCharacteristics |=
                                               INTERRUPT_VECTOR_EDGE_TRIGGERED;
                    }

                    VectorRequirement.Characteristics = VectorCharacteristics;
                    VectorRequirement.OwningRequirement = Requirement;
                    Status = IoCreateAndAddResourceRequirement(
                                                            &VectorRequirement,
                                                            RequirementList,
                                                            NULL);

                    if (!KSUCCESS(Status)) {
                        goto ProcessResourceRequirementsEnd;
                    }

                } else {
                    Controller->SkipFirstInterrupt = TRUE;
                }
            }

            //
            // Get the next resource requirement.
            //

            Requirement = IoGetNextResourceRequirement(RequirementList,
                                                       Requirement);
        }

        //
        // Get the next possible resource configuration.
        //

        RequirementList = IoGetNextResourceConfiguration(Requirements,
                                                         RequirementList);
    }

    //
    // If in legacy mode, add the legacy interrupts.
    //

    if ((SecondaryLegacy != FALSE) && (PrimaryLegacy != FALSE)) {
        RequirementList = IoGetNextResourceConfiguration(Requirements, NULL);

        ASSERT(RequirementList != NULL);

        LegacyRequirement.Type = ResourceTypeInterruptLine;
        LegacyRequirement.Minimum = ATA_LEGACY_PRIMARY_INTERRUPT;
        LegacyRequirement.Maximum = LegacyRequirement.Minimum + 1;
        LegacyRequirement.Length = 1;
        LegacyRequirement.Characteristics =
                                          ATA_LEGACY_INTERRUPT_CHARACTERISTICS;

        LegacyRequirement.Flags = 0;
        Status = IoCreateAndAddResourceRequirement(&LegacyRequirement,
                                                   RequirementList,
                                                   &NewRequirement);

        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }

        VectorRequirement.Flags |= RESOURCE_FLAG_NOT_SHAREABLE;
        VectorRequirement.Characteristics = ATA_LEGACY_VECTOR_CHARACTERISTICS;
        VectorRequirement.OwningRequirement = NewRequirement;
        Status = IoCreateAndAddResourceRequirement(&VectorRequirement,
                                                   RequirementList,
                                                   NULL);

        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }

        LegacyRequirement.Minimum = ATA_LEGACY_SECONDARY_INTERRUPT;
        LegacyRequirement.Maximum = LegacyRequirement.Minimum + 1;
        Status = IoCreateAndAddResourceRequirement(&LegacyRequirement,
                                                   RequirementList,
                                                   &NewRequirement);

        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }

        VectorRequirement.OwningRequirement = NewRequirement;
        Status = IoCreateAndAddResourceRequirement(&VectorRequirement,
                                                   RequirementList,
                                                   NULL);

        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }
    }

    Status = STATUS_SUCCESS;

ProcessResourceRequirementsEnd:
    return Status;
}

KSTATUS
AtapStartController (
    PIRP Irp,
    PATA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine starts an ATA controller device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Controller - Supplies a pointer to the ATA controller.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    UINTN Index;
    BOOL LineSkipped;
    BOOL PrimaryInterruptConnected;
    BOOL SecondaryInterruptConnected;
    KSTATUS Status;
    PRESOURCE_ALLOCATION VectorAllocation;

    PrimaryInterruptConnected = Controller->PrimaryInterruptFound;
    SecondaryInterruptConnected = Controller->SecondaryInterruptFound;
    for (Index = 0; Index < ATA_CABLE_COUNT; Index += 1) {
        Controller->Channel[Index].IoBase = -1;
        Controller->Channel[Index].ControlBase = -1;
        Controller->Channel[Index].BusMasterBase = -1;
    }

    Index = 0;
    LineSkipped = FALSE;
    Status = PmInitialize(Irp->Device);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = PmDeviceAddReference(Irp->Device);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Loop through the allocated resources to get the controller base and the
    // interrupt.
    //

    VectorAllocation = NULL;
    AllocationList = Irp->U.StartDevice.ProcessorLocalResources;
    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {

        //
        // If the resource is an interrupt line, search for the next interrupt
        // vector.
        //

        if (Allocation->Type == ResourceTypeInterruptLine) {
            if ((LineSkipped == FALSE) &&
                (Controller->SkipFirstInterrupt != FALSE)) {

                LineSkipped = TRUE;

            } else {
                VectorAllocation = IoGetNextResourceAllocation(
                                                             AllocationList,
                                                             VectorAllocation);

                while (VectorAllocation != NULL) {
                    if (VectorAllocation->Type == ResourceTypeInterruptVector) {

                        ASSERT(VectorAllocation->OwningAllocation ==
                               Allocation);

                        if (Controller->PrimaryInterruptFound == FALSE) {
                            Controller->PrimaryInterruptLine =
                                                        Allocation->Allocation;

                            Controller->PrimaryInterruptVector =
                                                  VectorAllocation->Allocation;

                            Controller->PrimaryInterruptFound = TRUE;

                        } else if (Controller->SecondaryInterruptFound ==
                                   FALSE) {

                            Controller->SecondaryInterruptLine =
                                                        Allocation->Allocation;

                            Controller->SecondaryInterruptVector =
                                                  VectorAllocation->Allocation;

                            Controller->SecondaryInterruptFound = TRUE;

                        } else {

                            //
                            // There shouldn't be more than two interrupts to
                            // connect.
                            //

                            ASSERT(FALSE);
                        }

                        break;
                    }

                    VectorAllocation = IoGetNextResourceAllocation(
                                                             AllocationList,
                                                             VectorAllocation);
                }
            }

        } else if (Allocation->Type == ResourceTypeIoPort) {

            ASSERT(Allocation->Allocation < MAX_USHORT);

            switch (Index) {
            case 0:
                if (Allocation->Length >= 8) {
                    Controller->Channel[0].IoBase = Allocation->Allocation;
                }

                break;

            case 1:
                if (Allocation->Length >= 4) {
                    Controller->Channel[0].ControlBase =
                                                    Allocation->Allocation + 2;
                }

                break;

            case 2:
                if (Allocation->Length >= 8) {
                    Controller->Channel[1].IoBase = Allocation->Allocation;
                }

                break;

            case 3:
                if (Allocation->Length >= 4) {
                    Controller->Channel[1].ControlBase =
                                                    Allocation->Allocation + 2;
                }

                break;

            case 4:
                if (Allocation->Length >= 16) {
                    Controller->Channel[0].BusMasterBase =
                                                    Allocation->Allocation;

                    Controller->Channel[1].BusMasterBase =
                                                Allocation->Allocation + 8;
                }

                break;
            }

            Index += 1;

        } else if (Allocation->Type == ResourceTypePhysicalAddressSpace) {
            Index += 1;
        }

        //
        // Get the next allocation in the list.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    //
    // Assign the legacy register locations if needed.
    //

    if ((Controller->Interface & IDE_INTERFACE_PRIMARY_NATIVE_ENABLED) == 0) {
        Controller->Channel[0].IoBase = ATA_LEGACY_PRIMARY_IO_BASE;
        Controller->Channel[0].ControlBase = ATA_LEGACY_PRIMARY_CONTROL_BASE;
    }

    if ((Controller->Interface & IDE_INTERFACE_SECONDARY_NATIVE_ENABLED) == 0) {
        Controller->Channel[1].IoBase = ATA_LEGACY_SECONDARY_IO_BASE;
        Controller->Channel[1].ControlBase = ATA_LEGACY_SECONDARY_CONTROL_BASE;
    }

    //
    // Put the controller into a known state.
    //

    Status = AtapResetController(Controller);
    if (!KSUCCESS(Status)) {
        goto StartControllerEnd;
    }

    RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
    Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
    Connect.Device = Irp->Device;
    Connect.InterruptServiceRoutine = AtaInterruptService;
    Connect.DispatchServiceRoutine = AtaInterruptServiceDpc;
    Connect.Context = Controller;
    if ((PrimaryInterruptConnected == FALSE) &&
        (Controller->PrimaryInterruptFound != FALSE) &&
        (Controller->Channel[0].BusMasterBase != (USHORT)-1)) {

        Connect.LineNumber = Controller->PrimaryInterruptLine;
        Connect.Vector = Controller->PrimaryInterruptVector;
        Connect.Interrupt = &(Controller->PrimaryInterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            goto StartControllerEnd;
        }
    }

    if ((SecondaryInterruptConnected == FALSE) &&
        (Controller->SecondaryInterruptFound != FALSE) &&
        (Controller->Channel[1].BusMasterBase != (USHORT)-1)) {

        Connect.LineNumber = Controller->SecondaryInterruptLine;
        Connect.Vector = Controller->SecondaryInterruptVector;
        Connect.Interrupt = &(Controller->SecondaryInterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            goto StartControllerEnd;
        }
    }

    Status = STATUS_SUCCESS;

StartControllerEnd:
    PmDeviceReleaseReference(Irp->Device);
    return Status;
}

KSTATUS
AtapResetController (
    PATA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine resets an ATA controller device.

Arguments:

    Controller - Supplies a pointer to the ATA controller.

Return Value:

    Status code.

--*/

{

    //
    // Disable interrupts.
    //

    Controller->Channel[0].InterruptDisable = ATA_CONTROL_INTERRUPT_DISABLE;
    Controller->Channel[1].InterruptDisable = ATA_CONTROL_INTERRUPT_DISABLE;
    HlBusySpin(2 * MICROSECONDS_PER_MILLISECOND);
    AtapWriteRegister(&(Controller->Channel[0]),
                      AtaRegisterControl,
                      Controller->Channel[0].InterruptDisable);

    AtapReadRegister(&(Controller->Channel[0]), AtaRegisterStatus);
    if (Controller->Channel[0].BusMasterBase != (USHORT)-1) {
        AtapWriteRegister(&(Controller->Channel[0]),
                          AtaRegisterBusMasterStatus,
                          IDE_STATUS_INTERRUPT | IDE_STATUS_ERROR);

        AtapWriteRegister(&(Controller->Channel[0]),
                          AtaRegisterBusMasterCommand,
                          0);
    }

    if (Controller->Channel[1].IoBase != (USHORT)-1) {
        HlBusySpin(2 * MICROSECONDS_PER_MILLISECOND);
        AtapWriteRegister(&(Controller->Channel[1]),
                          AtaRegisterControl,
                          Controller->Channel[1].InterruptDisable);

        AtapReadRegister(&(Controller->Channel[1]), AtaRegisterStatus);
        if (Controller->Channel[1].BusMasterBase != (USHORT)-1) {
            AtapWriteRegister(&(Controller->Channel[1]),
                              AtaRegisterBusMasterStatus,
                              IDE_STATUS_INTERRUPT | IDE_STATUS_ERROR);

            AtapWriteRegister(&(Controller->Channel[1]),
                              AtaRegisterBusMasterCommand,
                              0);
        }
    }

    return STATUS_SUCCESS;
}

VOID
AtapEnumerateDrives (
    PIRP Irp,
    PATA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine enumerates all drives on an ATA controller.

Arguments:

    Irp - Supplies a pointer to the query children IRP.

    Controller - Supplies a pointer to the ATA controller.

Return Value:

    None.

--*/

{

    PATA_CHILD Child;
    ULONG ChildCount;
    UINTN ChildIndex;
    PDEVICE Children[4];
    KSTATUS Status;

    Status = PmDeviceAddReference(Irp->Device);
    if (!KSUCCESS(Status)) {
        IoCompleteIrp(AtaDriver, Irp, Status);
        return;
    }

    ChildCount = 0;
    for (ChildIndex = 0; ChildIndex < ATA_CHILD_COUNT; ChildIndex += 1) {
        Child = &(Controller->ChildContexts[ChildIndex]);
        Status = AtapIdentifyDevice(Child);
        if (!KSUCCESS(Status)) {
            Controller->ChildDevices[ChildIndex] = NULL;

        } else {
            if (Controller->ChildDevices[ChildIndex] == NULL) {
                Status = IoCreateDevice(
                                      AtaDriver,
                                      Child,
                                      Irp->Device,
                                      "Disk",
                                      DISK_CLASS_ID,
                                      NULL,
                                      &(Controller->ChildDevices[ChildIndex]));

                if (!KSUCCESS(Status)) {
                    Controller->ChildDevices[ChildIndex] = NULL;
                }
            }
        }

        if (Controller->ChildDevices[ChildIndex] != NULL) {
            Children[ChildCount] = Controller->ChildDevices[ChildIndex];
            ChildCount += 1;
        }
    }

    if (ChildCount != 0) {
        Status = IoMergeChildArrays(Irp,
                                    Children,
                                    ChildCount,
                                    ATA_ALLOCATION_TAG);

        if (!KSUCCESS(Status)) {
            goto EnumerateDrivesEnd;
        }
    }

    Status = STATUS_SUCCESS;

EnumerateDrivesEnd:
    PmDeviceReleaseReference(Irp->Device);
    IoCompleteIrp(AtaDriver, Irp, Status);
    return;
}

KSTATUS
AtapIdentifyDevice (
    PATA_CHILD Device
    )

/*++

Routine Description:

    This routine attempts to send the IDENTIFY packet command and process the
    results.

Arguments:

    Device - Supplies a pointer to the child device to query.

Return Value:

    Status code.

--*/

{

    ATA_IDENTIFY_PACKET Identify;
    UCHAR Lba1;
    UCHAR Lba2;
    KSTATUS Status;

    if (Device->Channel->IoBase == (USHORT)-1) {
        return STATUS_NO_SUCH_DEVICE;
    }

    Device->DmaSupported = FALSE;
    Status = AtapPioCommand(Device,
                            AtaCommandIdentify,
                            TRUE,
                            FALSE,
                            0,
                            0,
                            &Identify,
                            0,
                            0,
                            FALSE);

    if (!KSUCCESS(Status)) {

        //
        // If the identify command failed, check out LBA1 and LBA2 to see if
        // they're responding like an ATAPI device.
        //

        Lba1 = AtapReadRegister(Device->Channel, AtaRegisterLba1);
        Lba2 = AtapReadRegister(Device->Channel, AtaRegisterLba2);
        if (((Lba1 == ATA_PATAPI_LBA1) && (Lba2 == ATA_PATAPI_LBA2)) ||
            ((Lba1 == ATA_PATAPI_LBA1) && (Lba2 == ATA_PATAPI_LBA2))) {

            //
            // TODO: ATAPI devices.
            //

        } else if ((Lba1 == ATA_SATA_LBA1) && (Lba2 == ATA_SATA_LBA2)) {
            RtlDebugPrint("TODO: SATA\n");
        }

        goto IdentifyDeviceEnd;
    }

    //
    // Get the total capacity of the disk.
    //

    if ((Identify.CommandSetSupported & ATA_SUPPORTED_COMMAND_LBA48) != 0) {
        Device->TotalSectors = Identify.TotalSectorsLba48;

    } else {
        Device->TotalSectors = Identify.TotalSectors;
    }

    //
    // Determine whether or not to do DMA to this device.
    //

    if (Device->Channel->BusMasterBase != (USHORT)-1) {
        Device->DmaSupported = TRUE;
    }

IdentifyDeviceEnd:
    return Status;
}

KSTATUS
AtapPerformDmaIo (
    PIRP Irp,
    PATA_CHILD Device,
    BOOL HaveDpcLock
    )

/*++

Routine Description:

    This routine starts a DMA-based I/O transfer. This routine assumes the
    channel lock is already held.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to the ATA child device.

    HaveDpcLock - Supplies a boolean indicating if the caller already has the
        DPC lock acquired.

Return Value:

    Status code.

--*/

{

    ULONGLONG BlockAddress;
    UINTN BytesPreviouslyCompleted;
    UINTN BytesToComplete;
    ATA_COMMAND Command;
    UCHAR DmaCommand;
    PHYSICAL_ADDRESS EndBoundary;
    ULONG EntrySize;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    PIO_BUFFER IoBuffer;
    UINTN IoBufferOffset;
    IO_OFFSET IoOffset;
    BOOL Lba48;
    UINTN MaxTransferSize;
    RUNLEVEL OldRunLevel;
    PHYSICAL_ADDRESS PhysicalAddress;
    PATA_PRDT Prdt;
    USHORT PrdtAddressRegister;
    UINTN PrdtIndex;
    UINTN SectorCount;
    KSTATUS Status;
    UINTN TransferSize;
    UINTN TransferSizeRemaining;
    BOOL Write;

    ASSERT(Device->Channel->Irp == Irp);
    ASSERT(Device->Channel->OwningChild == Device);
    ASSERT(Irp->U.ReadWrite.IoBuffer != NULL);

    IoBuffer = Irp->U.ReadWrite.IoBuffer;
    BytesPreviouslyCompleted = Irp->U.ReadWrite.IoBytesCompleted;
    BytesToComplete = Irp->U.ReadWrite.IoSizeInBytes;
    IoOffset = Irp->U.ReadWrite.NewIoOffset;

    ASSERT(BytesPreviouslyCompleted < BytesToComplete);
    ASSERT(IoOffset == (Irp->U.ReadWrite.IoOffset + BytesPreviouslyCompleted));
    ASSERT(Device->Channel->BusMasterBase != (USHORT)-1);
    ASSERT(IS_ALIGNED(IoOffset, ATA_SECTOR_SIZE) != FALSE);
    ASSERT(IS_ALIGNED(BytesToComplete, ATA_SECTOR_SIZE) != FALSE);

    Write = FALSE;
    if (Irp->MinorCode == IrpMinorIoWrite) {
        Write = TRUE;
    }

    //
    // Determine the bytes to complete this round.
    //

    MaxTransferSize = ATA_MAX_LBA48_SECTOR_COUNT * ATA_SECTOR_SIZE;
    if (Device->Lba48Supported == FALSE) {
        MaxTransferSize = ATA_MAX_LBA28_SECTOR_COUNT * ATA_SECTOR_SIZE;
    }

    TransferSize = BytesToComplete - BytesPreviouslyCompleted;
    if (TransferSize > MaxTransferSize) {
        TransferSize = MaxTransferSize;
    }

    //
    // Get to the currect spot in the I/O buffer.
    //

    IoBufferOffset = MmGetIoBufferCurrentOffset(IoBuffer);
    IoBufferOffset += BytesPreviouslyCompleted;
    FragmentIndex = 0;
    FragmentOffset = 0;
    while (IoBufferOffset != 0) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);
        if (IoBufferOffset < Fragment->Size) {
            FragmentOffset = IoBufferOffset;
            break;
        }

        IoBufferOffset -= Fragment->Size;
        FragmentIndex += 1;
    }

    //
    // Loop over every fragment in the I/O buffer setting up PRDT entries.
    //

    Prdt = Device->Channel->Prdt;
    PrdtIndex = 0;
    TransferSizeRemaining = TransferSize;
    while ((TransferSizeRemaining != 0) &&
           (PrdtIndex < (ATA_PRDT_DISK_SIZE / sizeof(ATA_PRDT)))) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);

        ASSERT(IS_ALIGNED(Fragment->Size, ATA_SECTOR_SIZE) != FALSE);
        ASSERT(IS_ALIGNED(FragmentOffset, ATA_SECTOR_SIZE) != FALSE);

        //
        // Determine the size of the PRDT entry.
        //

        EntrySize = TransferSizeRemaining;
        if (EntrySize > (Fragment->Size - FragmentOffset)) {
            EntrySize = Fragment->Size - FragmentOffset;
        }

        PhysicalAddress = Fragment->PhysicalAddress + FragmentOffset;
        EndBoundary = ALIGN_RANGE_DOWN(PhysicalAddress + EntrySize - 1,
                                       ATA_DMA_BOUNDARY);

        if (ALIGN_RANGE_DOWN(PhysicalAddress, ATA_DMA_BOUNDARY) !=
            EndBoundary) {

            EntrySize =
                    ALIGN_RANGE_UP(PhysicalAddress + 1, ATA_DMA_BOUNDARY) -
                    PhysicalAddress;
        }

        TransferSizeRemaining -= EntrySize;

        //
        // ATA can only DMA to lower 4GB addresses.
        //

        ASSERT(IS_ALIGNED(PhysicalAddress, ATA_SECTOR_SIZE) != FALSE);
        ASSERT(PhysicalAddress == (ULONG)PhysicalAddress);
        ASSERT((PhysicalAddress + EntrySize) ==
               (ULONG)(PhysicalAddress + EntrySize));

        Prdt->PhysicalAddress = PhysicalAddress;
        if (EntrySize == ATA_DMA_BOUNDARY) {
            Prdt->Size = 0;

        } else {
            Prdt->Size = EntrySize;
        }

        Prdt->Flags = 0;
        Prdt += 1;
        PrdtIndex += 1;
        FragmentOffset += EntrySize;
        if (FragmentOffset >= Fragment->Size) {
            FragmentIndex += 1;
            FragmentOffset = 0;
        }
    }

    ASSERT(PrdtIndex != 0);

    Prdt -= 1;
    Prdt->Flags |= ATA_DMA_LAST_DESCRIPTOR;
    TransferSize -= TransferSizeRemaining;
    BlockAddress = IoOffset / ATA_SECTOR_SIZE;
    SectorCount = TransferSize / ATA_SECTOR_SIZE;

    ASSERT(SectorCount == (ULONG)SectorCount);

    //
    // Use LBA48 if the block address is too high or the sector size is too
    // large.
    //

    if ((BlockAddress > ATA_MAX_LBA28) ||
        (SectorCount > ATA_MAX_LBA28_SECTOR_COUNT)) {

        Lba48 = TRUE;
        if (Write != FALSE) {
            Command = AtaCommandWriteDma48;

        } else {
            Command = AtaCommandReadDma48;
        }

    } else {
        Lba48 = FALSE;
        if (Write != FALSE) {
            Command = AtaCommandWriteDma28;

        } else {
            Command = AtaCommandReadDma28;
        }

        if (SectorCount == ATA_MAX_LBA28_SECTOR_COUNT) {
            SectorCount = 0;
        }
    }

    OldRunLevel = RunLevelCount;
    if (HaveDpcLock == FALSE) {
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        KeAcquireSpinLock(&(Device->Controller->DpcLock));
    }

    Status = AtapSelectDevice(Device, FALSE);
    if (!KSUCCESS(Status)) {
        goto PerformDmaIoEnd;
    }

    //
    // Set up the usual registers for a command.
    //

    AtapSetupCommand(Device, Lba48, 0, (ULONG)SectorCount, BlockAddress, 0);

    //
    // Enable interrupts and start the command.
    //

    Device->Channel->IoSize = TransferSize;
    Device->Channel->InterruptDisable = 0;
    AtapWriteRegister(Device->Channel, AtaRegisterControl, 0);
    AtapWriteRegister(Device->Channel, AtaRegisterCommand, Command);

    //
    // Write the PRDT base address.
    //

    PrdtAddressRegister = Device->Channel->BusMasterBase +
                          ATA_BUS_MASTER_TABLE_REGISTER;

    HlIoPortOutLong(PrdtAddressRegister,
                    Device->Channel->PrdtPhysicalAddress);

    //
    // Start the DMA.
    //

    DmaCommand = ATA_BUS_MASTER_COMMAND_DMA_ENABLE;
    if (Write == FALSE) {
        DmaCommand |= ATA_BUS_MASTER_COMMAND_DMA_READ;
    }

    //
    // If this is the first set of DMA for the IRP, pend it.
    //

    if (BytesPreviouslyCompleted == 0) {
        IoPendIrp(AtaDriver, Irp);
    }

    AtapWriteRegister(Device->Channel,
                      AtaRegisterBusMasterStatus,
                      IDE_STATUS_INTERRUPT | IDE_STATUS_ERROR);

    AtapWriteRegister(Device->Channel, AtaRegisterBusMasterCommand, DmaCommand);
    Status = STATUS_SUCCESS;

PerformDmaIoEnd:
    if (HaveDpcLock == FALSE) {
        KeReleaseSpinLock(&(Device->Controller->DpcLock));
        KeLowerRunLevel(OldRunLevel);
    }

    return Status;
}

KSTATUS
AtapPerformPolledIo (
    PIRP_READ_WRITE IrpReadWrite,
    PATA_CHILD Device,
    BOOL Write,
    BOOL CriticalMode
    )

/*++

Routine Description:

    This routine performs polled I/O data transfers.

Arguments:

    IrpReadWrite - Supplies a pointer to the I/O request read/write packet.

    Device - Supplies a pointer to the ATA child device.

    Write - Supplies a boolean indicating if this is a read operation (TRUE) or
        a write operation (FALSE).

    CriticalMode - Supplies a boolean indicating if this I/O operation is in
        a critical code path (TRUE), such as a crash dump I/O request, or in
        the default code path.

Return Value:

    Status code.

--*/

{

    UINTN BlockCount;
    ULONGLONG BlockOffset;
    UINTN BytesRemaining;
    UINTN BytesThisRound;
    KSTATUS CompletionStatus;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    PIO_BUFFER IoBuffer;
    UINTN IoBufferOffset;
    ULONG IrpReadWriteFlags;
    BOOL ReadWriteIrpPrepared;
    KSTATUS Status;
    PVOID VirtualAddress;

    IrpReadWrite->IoBytesCompleted = 0;
    ReadWriteIrpPrepared = FALSE;

    //
    // All requests should be block aligned.
    //

    ASSERT(IrpReadWrite->IoBuffer != NULL);
    ASSERT(IS_ALIGNED(IrpReadWrite->IoSizeInBytes, ATA_SECTOR_SIZE) != FALSE);
    ASSERT(IS_ALIGNED(IrpReadWrite->IoOffset, ATA_SECTOR_SIZE) != FALSE);

    //
    // Prepare the I/O buffer for the polled I/O operation.
    //

    IrpReadWriteFlags = IRP_READ_WRITE_FLAG_POLLED;
    if (Write != FALSE) {
        IrpReadWriteFlags |= IRP_READ_WRITE_FLAG_WRITE;
    }

    Status = IoPrepareReadWriteIrp(IrpReadWrite,
                                   ATA_SECTOR_SIZE,
                                   0,
                                   MAX_ULONGLONG,
                                   IrpReadWriteFlags);

    if (!KSUCCESS(Status)) {
        goto PerformPolledIoEnd;
    }

    ReadWriteIrpPrepared = TRUE;

    //
    // Make sure the I/O buffer is mapped before use. ATA currently depends on
    // the buffer being mapped.
    //

    IoBuffer = IrpReadWrite->IoBuffer;
    Status = MmMapIoBuffer(IoBuffer, FALSE, FALSE, FALSE);
    if (!KSUCCESS(Status)) {
        goto PerformPolledIoEnd;
    }

    //
    // Find the starting fragment based on the current offset.
    //

    IoBufferOffset = MmGetIoBufferCurrentOffset(IoBuffer);
    FragmentIndex = 0;
    FragmentOffset = 0;
    while (IoBufferOffset != 0) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);
        if (IoBufferOffset < Fragment->Size) {
            FragmentOffset = IoBufferOffset;
            break;
        }

        IoBufferOffset -= Fragment->Size;
        FragmentIndex += 1;
    }

    //
    // Loop reading in or writing out each fragment in the I/O buffer.
    //

    BlockOffset = IrpReadWrite->IoOffset / ATA_SECTOR_SIZE;
    BytesRemaining = IrpReadWrite->IoSizeInBytes;
    while (BytesRemaining != 0) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = (PIO_BUFFER_FRAGMENT)&(IoBuffer->Fragment[FragmentIndex]);
        VirtualAddress = Fragment->VirtualAddress + FragmentOffset;
        BytesThisRound = Fragment->Size - FragmentOffset;
        if (BytesRemaining < BytesThisRound) {
            BytesThisRound = BytesRemaining;
        }

        ASSERT(IS_ALIGNED(BytesThisRound, ATA_SECTOR_SIZE) != FALSE);

        BlockCount = BytesThisRound / ATA_SECTOR_SIZE;

        //
        // Make sure the system isn't trying to do I/O off the end of the disk.
        //

        ASSERT(BlockOffset < Device->TotalSectors);
        ASSERT(BlockCount >= 1);

        Status = AtapReadWriteSectorsPio(Device,
                                         BlockOffset,
                                         BlockCount,
                                         VirtualAddress,
                                         Write,
                                         CriticalMode);

        if (!KSUCCESS(Status)) {
            goto PerformPolledIoEnd;
        }

        BlockOffset += BlockCount;
        BytesRemaining -= BytesThisRound;
        FragmentOffset += BytesThisRound;
        IrpReadWrite->IoBytesCompleted += BytesThisRound;
        if (FragmentOffset >= Fragment->Size) {
            FragmentIndex += 1;
            FragmentOffset = 0;
        }
    }

    Status = STATUS_SUCCESS;

PerformPolledIoEnd:
    if (ReadWriteIrpPrepared != FALSE) {
        CompletionStatus = IoCompleteReadWriteIrp(IrpReadWrite,
                                                  IrpReadWriteFlags);

        if (!KSUCCESS(CompletionStatus) && KSUCCESS(Status)) {
            Status = CompletionStatus;
        }
    }

    IrpReadWrite->NewIoOffset = IrpReadWrite->IoOffset +
                                IrpReadWrite->IoBytesCompleted;

    return Status;
}

KSTATUS
AtapSynchronizeDevice (
    PATA_CHILD Device
    )

/*++

Routine Description:

    This routine synchronizes the device by sending a cache flush command.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    RUNLEVEL OldRunLevel;
    KSTATUS Status;

    KeAcquireQueuedLock(Device->Channel->Lock);
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Device->Controller->DpcLock));
    Status = AtapSelectDevice(Device, FALSE);
    if (KSUCCESS(Status)) {
        Status = AtapExecuteCacheFlush(Device, FALSE);
    }

    KeReleaseSpinLock(&(Device->Controller->DpcLock));
    KeLowerRunLevel(OldRunLevel);
    KeReleaseQueuedLock(Device->Channel->Lock);
    return Status;
}

KSTATUS
AtapBlockRead (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    )

/*++

Routine Description:

    This routine reads the block contents from the disk into the given I/O
    buffer using polled I/O. It does so without acquiring any locks or
    allocating any resources, as this routine is used for crash dump support
    when the system is in a very fragile state. This routine must be called at
    high level.

Arguments:

    DiskToken - Supplies an opaque token for the disk. The appropriate token is
        retrieved by querying the disk device information.

    IoBuffer - Supplies a pointer to the I/O buffer where the data will be read.

    BlockAddress - Supplies the block index to read (for physical disk, this is
        the LBA).

    BlockCount - Supplies the number of blocks to read.

    BlocksCompleted - Supplies a pointer that receives the total number of
        blocks read.

Return Value:

    Status code.

--*/

{

    IRP_READ_WRITE IrpReadWrite;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    //
    // As this read routine is meant for critical code paths (crash dump),
    // indicate that the channel should not be locked when performing the I/O.
    // It may be that some other thread holds the lock, which would cause a
    // dead lock as all other processors and threads are likely frozen.
    //

    IrpReadWrite.IoBuffer = IoBuffer;
    IrpReadWrite.IoOffset = BlockAddress * ATA_SECTOR_SIZE;
    IrpReadWrite.IoSizeInBytes = BlockCount * ATA_SECTOR_SIZE;
    Status = AtapPerformPolledIo(&IrpReadWrite, DiskToken, FALSE, TRUE);
    *BlocksCompleted = IrpReadWrite.IoBytesCompleted / ATA_SECTOR_SIZE;
    return Status;
}

KSTATUS
AtapBlockWrite (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    )

/*++

Routine Description:

    This routine writes the contents of the given I/O buffer to the disk using
    polled I/O. It does so without acquiring any locks or allocating any
    resources, as this routine is used for crash dump support when the system
    is in a very fragile state. This routine must be called at high level.

Arguments:

    DiskToken - Supplies an opaque token for the disk. The appropriate token is
        retrieved by querying the disk device information.

    IoBuffer - Supplies a pointer to the I/O buffer containing the data to
        write.

    BlockAddress - Supplies the block index to write to (for physical disk,
        this is the LBA).

    BlockCount - Supplies the number of blocks to write.

    BlocksCompleted - Supplies a pointer that receives the total number of
        blocks written.

Return Value:

    Status code.

--*/

{

    IRP_READ_WRITE IrpReadWrite;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    //
    // As this write routine is meant for critical code paths (crash dump),
    // indicate that the channel should not be locked when performing the I/O.
    // It may be that some other thread holds the lock, which would cause a
    // dead lock as all other processors and threads are likely frozen.
    //

    IrpReadWrite.IoBuffer = IoBuffer;
    IrpReadWrite.IoOffset = BlockAddress * ATA_SECTOR_SIZE;
    IrpReadWrite.IoSizeInBytes = BlockCount * ATA_SECTOR_SIZE;
    Status = AtapPerformPolledIo(&IrpReadWrite, DiskToken, TRUE, TRUE);
    *BlocksCompleted = IrpReadWrite.IoBytesCompleted / ATA_SECTOR_SIZE;
    return Status;
}

KSTATUS
AtapReadWriteSectorsPio (
    PATA_CHILD AtaDevice,
    ULONGLONG BlockAddress,
    UINTN SectorCount,
    PVOID Buffer,
    BOOL Write,
    BOOL CriticalMode
    )

/*++

Routine Description:

    This routine reads or writes a given number of sectors from the ATA disk
    using polled I/O.

Arguments:

    AtaDevice - Supplies a pointer to the ATA disk's context.

    BlockAddress - Supplies the block number to read from or write to (LBA).

    SectorCount - Supplies the number of blocks (sectors) to read from or write
        to the device.

    Buffer - Supplies the data buffer where the data will be read or written.

    Write - Supplies a boolean indicating if this is a read operation (FALSE)
        or a write operation (TRUE).

    CriticalMode - Supplies a boolean indicating if this I/O operation is in
        a critical code path (TRUE), such as a crash dump I/O request, or in
        the default code path.

Return Value:

    Status code.

--*/

{

    ATA_COMMAND Command;
    BOOL Lba48;
    UINTN SectorCountThisRound;
    KSTATUS Status;

    if (BlockAddress > ATA_MAX_LBA28) {
        Lba48 = TRUE;
        if (Write != FALSE) {
            Command = AtaCommandWritePio48;

        } else {
            Command = AtaCommandReadPio48;
        }

    } else {
        Lba48 = FALSE;
        if (Write != FALSE) {
            Command = AtaCommandWritePio28;

        } else {
            Command = AtaCommandReadPio28;
        }
    }

    Status = STATUS_SUCCESS;
    while (SectorCount != 0) {
        SectorCountThisRound = SectorCount;
        if (SectorCountThisRound > ATA_MAX_LBA28_SECTOR_COUNT) {
            SectorCountThisRound = ATA_MAX_LBA28_SECTOR_COUNT;
        }

        ASSERT(SectorCountThisRound == (ULONG)SectorCountThisRound);

        Status = AtapPioCommand(AtaDevice,
                                Command,
                                Lba48,
                                Write,
                                0,
                                BlockAddress,
                                Buffer,
                                (ULONG)SectorCountThisRound,
                                0,
                                CriticalMode);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("ATA: Failed IO: %x\n", Status);
            goto ReadWriteSectorsPioEnd;
        }

        BlockAddress += SectorCountThisRound;
        Buffer += SectorCountThisRound * ATA_SECTOR_SIZE;
        SectorCount -= SectorCountThisRound;
    }

ReadWriteSectorsPioEnd:
    return Status;
}

KSTATUS
AtapPioCommand (
    PATA_CHILD Device,
    ATA_COMMAND Command,
    BOOL Lba48,
    BOOL Write,
    ULONG Features,
    ULONGLONG Lba,
    PVOID Buffer,
    ULONG SectorCount,
    ULONG MultiCount,
    BOOL CriticalMode
    )

/*++

Routine Description:

    This routine executes a data transfer using polled I/O.

Arguments:

    Device - Supplies a pointer to the device to read from.

    Command - Supplies the ATA command to execute.

    Lba48 - Supplies a boolean indicating if LBA48 mode is in use.

    Write - Supplies a boolean indicating if this is a read PIO command (FALSE)
        or a write data PIO command (TRUE).

    Features - Supplies the contents of the feature register.

    Lba - Supplies the logical block address to read from.

    Buffer - Supplies a pointer to the buffer where the data will be returned.

    SectorCount - Supplies the sector count to program in to the command.
        The IDENTIFY and PACKET IDENTIFY commands have a known sector count of
        one, zero should be passed in for those.

    MultiCount - Supplies the actual number of sectors.

    CriticalMode - Supplies a boolean indicating if this I/O operation is in
        a critical code path (TRUE), such as a crash dump I/O request, or in
        the default code path.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_DEVICE_IO_ERROR if the device is unresponsive or has a failure.

--*/

{

    ULONG BusMasterStatus;
    ULONG ByteCount;
    ULONG BytesTransferred;
    PATA_CHANNEL Channel;
    PUSHORT CurrentBuffer;
    UCHAR DeviceStatus;
    RUNLEVEL OldRunLevel;
    PATA_QUERY_TIME_COUNTER QueryTimeCounter;
    KSTATUS Status;
    ULONGLONG Timeout;

    ASSERT(SectorCount <= ATA_MAX_LBA28_SECTOR_COUNT);

    CurrentBuffer = (PUSHORT)Buffer;
    Channel = Device->Channel;

    //
    // Lock the other device out.
    //

    QueryTimeCounter = ATA_GET_TIME_FUNCTION(CriticalMode);
    OldRunLevel = RunLevelCount;
    if (CriticalMode == FALSE) {
        KeAcquireQueuedLock(Channel->Lock);
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        KeAcquireSpinLock(&(Device->Controller->DpcLock));
    }

    //
    // Clear the error bit of the bus master status.
    //

    if (Channel->BusMasterBase != -1) {
        AtapWriteRegister(Channel,
                          AtaRegisterBusMasterStatus,
                          IDE_STATUS_ERROR);
    }

    //
    // Select the device.
    //

    Status = AtapSelectDevice(Device, CriticalMode);
    if (!KSUCCESS(Status)) {
        goto PioCommandEnd;
    }

    //
    // Set up all registers of the command except the command register itself.
    //

    AtapSetupCommand(Device,
                     Lba48,
                     Features,
                     SectorCount,
                     Lba,
                     0);

    //
    // Disable interrupts.
    //

    AtapWriteRegister(Channel,
                      AtaRegisterControl,
                      ATA_CONTROL_INTERRUPT_DISABLE);

    if ((Command == AtaCommandIdentify) ||
        (Command == AtaCommandIdentifyPacket)) {

        SectorCount = 1;
    }

    //
    // Execute the command.
    //

    AtapWriteRegister(Channel, AtaRegisterCommand, (UCHAR)Command);
    AtapStall(Channel);

    //
    // This is the main read loop. The primary status register must not be read
    // more than once for each sector transferred, as reading the status
    // register clears the IRQ status. The alternate status register can be
    // read any number of times.
    //

    Timeout = QueryTimeCounter() +
              (HlQueryTimeCounterFrequency() * ATA_TIMEOUT);

    while (SectorCount != 0) {

        //
        // Read the status register once.
        //

        DeviceStatus = AtapReadRegister(Channel, AtaRegisterStatus);
        if ((Command == AtaCommandIdentify) && (DeviceStatus == 0)) {
            Status = STATUS_NO_SUCH_DEVICE;
            goto PioCommandEnd;
        }

        //
        // Fail if an error occurred.
        //

        if ((DeviceStatus & ATA_STATUS_ERROR_MASK) != 0) {
            Status = STATUS_DEVICE_IO_ERROR;
            goto PioCommandEnd;
        }

        if (((DeviceStatus & ATA_STATUS_BUSY) != 0) ||
            ((DeviceStatus & ATA_STATUS_DATA_REQUEST) == 0)) {

            if (QueryTimeCounter() > Timeout) {
                Status = STATUS_TIMEOUT;
                goto PioCommandEnd;
            }

            continue;
        }

        //
        // If the device is ready, read or write the data.
        //

        if ((DeviceStatus & ATA_STATUS_BUSY_MASK) == ATA_STATUS_DATA_REQUEST) {
            ByteCount = ATA_SECTOR_SIZE;
            if (MultiCount != 0) {
                ByteCount = MultiCount * ATA_SECTOR_SIZE;
            }

            if (Write != FALSE) {
                for (BytesTransferred = 0;
                     BytesTransferred < ByteCount;
                     BytesTransferred += sizeof(USHORT)) {

                    HlIoPortOutShort(Channel->IoBase + AtaRegisterData,
                                     *CurrentBuffer);

                    CurrentBuffer += 1;
                }

            } else {
                for (BytesTransferred = 0;
                     BytesTransferred < ByteCount;
                     BytesTransferred += sizeof(USHORT)) {

                    *CurrentBuffer =
                            HlIoPortInShort(Channel->IoBase + AtaRegisterData);

                    CurrentBuffer += 1;
                }
            }

            //
            // Stall to give the device a chance to settle.
            //

            AtapStall(Channel);
            if (MultiCount != 0) {

                ASSERT(SectorCount >= MultiCount);

                SectorCount -= MultiCount;

            } else {
                SectorCount -= 1;
            }
        }

        //
        // If this was the last sector, read the status register one more time.
        // If the error bits or data request is set, fail.
        //

        if (SectorCount == 0) {
            DeviceStatus = AtapReadRegister(Channel, AtaRegisterStatus);
            DeviceStatus &= ATA_STATUS_ERROR_MASK | ATA_STATUS_DATA_REQUEST;
            if (DeviceStatus != 0) {
                Status = STATUS_DEVICE_IO_ERROR;
                goto PioCommandEnd;
            }
        }
    }

    //
    // Check the bus master status register.
    //

    if (Channel->BusMasterBase != -1) {
        BusMasterStatus = AtapReadRegister(Channel, AtaRegisterBusMasterStatus);
        if ((BusMasterStatus & IDE_STATUS_ERROR) != 0) {
            Status = STATUS_DEVICE_IO_ERROR;
            goto PioCommandEnd;
        }
    }

    //
    // Send a clean cache command if this was a polled I/O write.
    //

    Status = STATUS_SUCCESS;
    if (Write != FALSE) {
        Status = AtapExecuteCacheFlush(Device, CriticalMode);
    }

PioCommandEnd:
    if (CriticalMode == FALSE) {
        KeReleaseSpinLock(&(Device->Controller->DpcLock));
        KeLowerRunLevel(OldRunLevel);
        KeReleaseQueuedLock(Channel->Lock);
    }

    return Status;
}

KSTATUS
AtapExecuteCacheFlush (
    PATA_CHILD Child,
    BOOL CriticalMode
    )

/*++

Routine Description:

    This routine sends a cache flush command to the device. This routine
    assumes the lock is held and the device is selected.

Arguments:

    Child - Supplies a pointer to the ATA child device.

    CriticalMode - Supplies a boolean indicating that the operation is
        operating in hostile conditions.

Return Value:

    Status code.

--*/

{

    PATA_CHANNEL Channel;
    ATA_COMMAND Command;
    PATA_QUERY_TIME_COUNTER QueryTimeCounter;
    KSTATUS Status;
    UCHAR StatusRegister;
    ULONGLONG Timeout;

    Channel = Child->Channel;
    Command = AtaCommandCacheFlush28;
    QueryTimeCounter = ATA_GET_TIME_FUNCTION(CriticalMode);
    Timeout = QueryTimeCounter() +
              (HlQueryTimeCounterFrequency() * ATA_TIMEOUT);

    Status = STATUS_SUCCESS;
    AtapWriteRegister(Channel, AtaRegisterCommand, Command);
    AtapStall(Channel);
    while (TRUE) {
        StatusRegister = AtapReadRegister(Channel, AtaRegisterStatus);
        if ((StatusRegister & ATA_STATUS_ERROR_MASK) != 0) {
            Status = STATUS_DEVICE_IO_ERROR;
            break;
        }

        if ((StatusRegister & ATA_STATUS_BUSY_MASK) == 0) {
            break;
        }

        if (QueryTimeCounter() > Timeout) {
            Status = STATUS_TIMEOUT;
            break;
        }
    }

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("ATA_CHILD 0x%x failed cache flush: %d\n", Child, Status);
    }

    return Status;
}

KSTATUS
AtapSelectDevice (
    PATA_CHILD Device,
    BOOL CriticalMode
    )

/*++

Routine Description:

    This routine selects the given ATA device in the hardware.

Arguments:

    Device - Supplies a pointer to the device to select. The interface needs to
        be valid.

    CriticalMode - Supplies a boolean indicating if this I/O operation is in
        a critical code path (TRUE), such as a crash dump I/O request, or in
        the default code path.

Return Value:

    Status code.

--*/

{

    PATA_CHANNEL Channel;
    UCHAR DeviceStatus;
    PATA_QUERY_TIME_COUNTER QueryTimeCounter;
    ULONGLONG Timeout;
    ULONGLONG TimeoutDuration;

    Channel = Device->Channel;
    if (Channel->SelectedDevice == Device->Slave) {
        return STATUS_SUCCESS;
    }

    //
    // Clear the selected device in case this selection fails.
    //

    Channel->SelectedDevice = 0xFF;

    //
    // Get the appropriate time counter routine. The recent time counter
    // requests do not work in critical mode, as interrupts are likely disabled.
    //

    if (CriticalMode != FALSE) {
        QueryTimeCounter = HlQueryTimeCounter;

    } else {
        QueryTimeCounter = KeGetRecentTimeCounter;
    }

    TimeoutDuration = KeConvertMicrosecondsToTimeTicks(ATA_SELECT_TIMEOUT);
    Timeout = QueryTimeCounter() + TimeoutDuration;

    //
    // Wait until whichever drive is currently selected to become not busy.
    //

    do {
        DeviceStatus = AtapReadRegister(Channel, AtaRegisterStatus);
        if ((DeviceStatus & ATA_STATUS_BUSY) == 0) {
            break;
        }

    } while (QueryTimeCounter() <= Timeout);

    if ((DeviceStatus & ATA_STATUS_BUSY) != 0) {
        return STATUS_TIMEOUT;
    }

    //
    // Select the device.
    //

    AtapWriteRegister(Channel, AtaRegisterDeviceSelect, Device->Slave);

    //
    // Wait for the device to become ready.
    //

    do {
        DeviceStatus = AtapReadRegister(Channel, AtaRegisterStatus);
        if (((DeviceStatus & ATA_STATUS_BUSY_MASK) == 0) &&
            ((DeviceStatus & ATA_STATUS_DRIVE_READY) != 0)) {

            break;
        }

        if ((DeviceStatus & ATA_STATUS_ERROR_MASK) != 0) {
            return STATUS_DEVICE_IO_ERROR;
        }

    } while (QueryTimeCounter() <= Timeout);

    if (((DeviceStatus & ATA_STATUS_BUSY_MASK) != 0) ||
        ((DeviceStatus & ATA_STATUS_DRIVE_READY) == 0)) {

        return STATUS_TIMEOUT;
    }

    Channel->SelectedDevice = Device->Slave;
    return STATUS_SUCCESS;
}

VOID
AtapSetupCommand (
    PATA_CHILD Device,
    BOOL Lba48,
    ULONG FeaturesRegister,
    ULONG SectorCountRegister,
    ULONGLONG Lba,
    ULONG DeviceControl
    )

/*++

Routine Description:

    This routine writes all registers to the ATA interface, preparing it to
    execute a command. It does not write the command register, so the command
    is not executed.

Arguments:

    Device - Supplies a pointer to the device.

    Lba48 - Supplies a boolean indicating if LBA48 mode is to be used.

    FeaturesRegister - Supplies the features register to write in.

    SectorCountRegister - Supplies the sector count register value to write.

    Lba - Supplies the logical block address value to write in the registers.

    DeviceControl - Supplies the device control value to write.

Return Value:

    None.

--*/

{

    PATA_CHANNEL Channel;
    UCHAR DeviceSelect;

    Channel = Device->Channel;
    DeviceSelect = Device->Slave | ATA_DRIVE_SELECT_LBA;

    //
    // Device control is written the same way in all cases. All other registers
    // are written slightly differently depending on the LBA mode.
    //

    AtapWriteRegister(Channel, AtaRegisterControl, DeviceControl);

    //
    // Write LBA48 mode.
    //

    if (Lba48 != FALSE) {

        //
        // Gain access to the high order bytes. The register access functions
        // will also do this when writing to registers like LBA3, etc, but
        // doing this directly allows these registers to be written in a batch.
        //

        AtapWriteRegister(Channel,
                          AtaRegisterControl,
                          ATA_CONTROL_HIGH_ORDER | Channel->InterruptDisable);

        AtapWriteRegister(Channel,
                          AtaRegisterSectorCountLow,
                          (UCHAR)(SectorCountRegister >> 8));

        AtapWriteRegister(Channel,
                          AtaRegisterLba0,
                          (UCHAR)(Lba >> 24));

        AtapWriteRegister(Channel,
                          AtaRegisterLba1,
                          (UCHAR)(Lba >> 32));

        AtapWriteRegister(Channel,
                          AtaRegisterLba2,
                          (UCHAR)(Lba >> 40));

        //
        // Back to the low registers.
        //

        AtapWriteRegister(Channel,
                          AtaRegisterControl,
                          Channel->InterruptDisable);

    //
    // Use LBA28 mode.
    //

    } else {
        DeviceSelect |= (UCHAR)((Lba >> 24) & 0x0F);
    }

    AtapWriteRegister(Channel, AtaRegisterFeatures, FeaturesRegister);
    AtapWriteRegister(Channel,
                      AtaRegisterSectorCountLow,
                      (UCHAR)SectorCountRegister);

    AtapWriteRegister(Channel,
                      AtaRegisterLba0,
                      (UCHAR)(Lba & 0xFF));

    AtapWriteRegister(Channel,
                      AtaRegisterLba1,
                      (UCHAR)(Lba >> 8));

    AtapWriteRegister(Channel,
                      AtaRegisterLba2,
                      (UCHAR)(Lba >> 16));

    AtapWriteRegister(Channel,
                      AtaRegisterDeviceSelect,
                      DeviceSelect);

    return;
}

VOID
AtapStall (
    PATA_CHANNEL Channel
    )

/*++

Routine Description:

    This routine stalls to give the ATA device time to settle.

Arguments:

    Channel - Supplies a pointer to the channel.

Return Value:

    None.

--*/

{

    AtapReadRegister(Channel, AtaRegisterAlternateStatus);
    AtapReadRegister(Channel, AtaRegisterAlternateStatus);
    AtapReadRegister(Channel, AtaRegisterAlternateStatus);
    AtapReadRegister(Channel, AtaRegisterAlternateStatus);
    return;
}

UCHAR
AtapReadRegister (
    PATA_CHANNEL Channel,
    ATA_REGISTER Register
    )

/*++

Routine Description:

    This routine reads an ATA register.

Arguments:

    Channel - Supplies a pointer to the channel information.

    Register - Supplies the register to read.

Return Value:

    Returns the value at the given register.

--*/

{

    UCHAR Result;

    //
    // If writing the high order bytes, flip into that mode.
    //

    if ((Register > AtaRegisterCommand) && (Register < AtaRegisterControl)) {
        AtapWriteRegister(Channel,
                          AtaRegisterControl,
                          ATA_CONTROL_HIGH_ORDER | Channel->InterruptDisable);
    }

    if (Register < AtaRegisterSectorCountHigh) {
        Result = HlIoPortInByte(Channel->IoBase + Register);

    } else if (Register < AtaRegisterControl) {
        Register -= ATA_HIGH_ADDRESSING_OFFSET;
        Result = HlIoPortInByte(Channel->IoBase + Register);

    } else if (Register < AtaRegisterBusMasterCommand) {
        Register -= ATA_CONTROL_REGISTER_OFFSET;
        Result = HlIoPortInByte(Channel->ControlBase + Register);

    } else {
        Register -= ATA_BUS_MASTER_REGISTER_OFFSET;
        Result = HlIoPortInByte(Channel->BusMasterBase + Register);
    }

    if ((Register > AtaRegisterCommand) && (Register < AtaRegisterControl)) {
        AtapWriteRegister(Channel,
                          AtaRegisterControl,
                          Channel->InterruptDisable);
    }

    return Result;
}

VOID
AtapWriteRegister (
    PATA_CHANNEL Channel,
    ATA_REGISTER Register,
    UCHAR Value
    )

/*++

Routine Description:

    This routine writes an ATA register.

Arguments:

    Channel - Supplies a pointer to the channel information.

    Register - Supplies the register to write.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    //
    // If writing the high order bytes, flip into that mode.
    //

    if ((Register > AtaRegisterCommand) && (Register < AtaRegisterControl)) {
        AtapWriteRegister(Channel,
                          AtaRegisterControl,
                          ATA_CONTROL_HIGH_ORDER | Channel->InterruptDisable);
    }

    if (Register < AtaRegisterSectorCountHigh) {
        HlIoPortOutByte(Channel->IoBase + Register, Value);

    } else if (Register < AtaRegisterControl) {
        Register -= ATA_HIGH_ADDRESSING_OFFSET;
        HlIoPortOutByte(Channel->IoBase + Register, Value);

    } else if (Register < AtaRegisterBusMasterCommand) {
        Register -= ATA_CONTROL_REGISTER_OFFSET;
        HlIoPortOutByte(Channel->ControlBase + Register, Value);

    } else {
        Register -= ATA_BUS_MASTER_REGISTER_OFFSET;
        HlIoPortOutByte(Channel->BusMasterBase + Register, Value);
    }

    if ((Register > AtaRegisterCommand) && (Register < AtaRegisterControl)) {
        AtapWriteRegister(Channel,
                          AtaRegisterControl,
                          Channel->InterruptDisable);
    }

    return;
}

VOID
AtapProcessPciConfigInterfaceChangeNotification (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    )

/*++

Routine Description:

    This routine is called when a PCI configuration space access interface
    changes in availability.

Arguments:

    Context - Supplies the caller's context pointer, supplied when the caller
        requested interface notifications.

    Device - Supplies a pointer to the device exposing or deleting the
        interface.

    InterfaceBuffer - Supplies a pointer to the interface buffer of the
        interface.

    InterfaceBufferSize - Supplies the buffer size.

    Arrival - Supplies TRUE if a new interface is arriving, or FALSE if an
        interface is departing.

Return Value:

    None.

--*/

{

    PATA_CONTROLLER ControllerContext;

    ControllerContext = (PATA_CONTROLLER)Context;
    if (Arrival != FALSE) {
        if (InterfaceBufferSize >= sizeof(INTERFACE_PCI_CONFIG_ACCESS)) {

            ASSERT(ControllerContext->PciConfigInterfaceAvailable == FALSE);

            RtlCopyMemory(&(ControllerContext->PciConfigInterface),
                          InterfaceBuffer,
                          sizeof(INTERFACE_PCI_CONFIG_ACCESS));

            ControllerContext->PciConfigInterfaceAvailable = TRUE;
        }

    } else {
        ControllerContext->PciConfigInterfaceAvailable = FALSE;
    }

    return;
}

