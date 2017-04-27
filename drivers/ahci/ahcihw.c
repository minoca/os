/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ahcihw.c

Abstract:

    This module implements hardware support for the SATA AHCI controller.

Author:

    Evan Green 15-Nov-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/storage/ata.h>
#include "ahci.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
AhcipProcessInterrupt (
    PAHCI_PORT Port
    );

KSTATUS
AhcipPerformBiosHandoff (
    PAHCI_CONTROLLER Controller
    );

KSTATUS
AhcipStopPort (
    PAHCI_PORT Port
    );

VOID
AhcipBeginNextIrp (
    PAHCI_PORT Port,
    LONG HeaderIndex
    );

VOID
AhcipPerformDmaIo (
    PAHCI_PORT Port,
    PIRP Irp,
    LONG HeaderIndex
    );

VOID
AhcipExecuteCacheFlush (
    PAHCI_PORT Port,
    LONG Index
    );

LONG
AhcipAllocateCommand (
    PAHCI_PORT Port
    );

VOID
AhcipFreeCommand (
    PAHCI_PORT Port,
    LONG Index
    );

VOID
AhcipSubmitCommand (
    PAHCI_PORT Port,
    ULONG Mask
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INTERRUPT_STATUS
AhciInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the AHCI interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the AHCI
        controller.

Return Value:

    Interrupt status.

--*/

{

    ULONG Bit;
    PAHCI_CONTROLLER Controller;
    PAHCI_PORT Port;
    ULONG PortStatus;
    ULONG RemainingStatus;
    ULONG Status;

    Controller = (PAHCI_CONTROLLER)Context;
    Status = AHCI_READ_GLOBAL(Controller, AhciInterruptStatus);
    if (Status == 0) {
        return InterruptStatusNotClaimed;
    }

    RtlAtomicOr32(&(Controller->PendingInterrupts), Status);

    //
    // Go read and clear the port status bits for each interrupting port,
    // otherwise they'll just come back.
    //

    RemainingStatus = Status;
    for (Bit = RtlCountTrailingZeros32(RemainingStatus);
         Bit < AHCI_PORT_COUNT;
         Bit += 1) {

        if ((RemainingStatus & (1 << Bit)) == 0) {
            continue;
        }

        Port = &(Controller->Ports[Bit]);
        PortStatus = AHCI_READ(Port, AhciPortInterruptStatus);

        ASSERT(PortStatus != 0);

        RtlAtomicOr32(&(Port->PendingInterrupts), PortStatus);

        //
        // Acknowledge the interrupts to get the port to pipe down.
        //

        AHCI_WRITE(Port, AhciPortInterruptStatus, PortStatus);
        RemainingStatus &= ~(1 << Bit);
        if (RemainingStatus == 0) {
            break;
        }
    }

    //
    // Clear the port interrupts in the global interrupt register now that the
    // ports have settled down.
    //

    AHCI_WRITE_GLOBAL(Controller, AhciInterruptStatus, Status);
    return InterruptStatusClaimed;
}

INTERRUPT_STATUS
AhciInterruptServiceDpc (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the AHCI dispatch level interrupt service.

Arguments:

    Parameter - Supplies the context, in this case the AHCI controller
        structure.

Return Value:

    None.

--*/

{

    PAHCI_CONTROLLER Controller;
    ULONG Port;
    ULONG StatusBits;

    Controller = Parameter;
    StatusBits = RtlAtomicExchange32(&(Controller->PendingInterrupts), 0);
    if (StatusBits == 0) {
        return InterruptStatusNotClaimed;
    }

    for (Port = 0; Port < AHCI_PORT_COUNT; Port += 1) {
        if ((StatusBits & (1 << Port)) == 0) {
            continue;
        }

        AhcipProcessInterrupt(&(Controller->Ports[Port]));
        StatusBits &= ~(1 << Port);
        if (StatusBits == 0) {
            break;
        }
    }

    return InterruptStatusClaimed;
}

KSTATUS
AhcipResetController (
    PAHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine resets an AHCI controller device.

Arguments:

    Controller - Supplies a pointer to the AHCI controller.

Return Value:

    Status code.

--*/

{

    PVOID Address;
    ULONG AllocationSize;
    ULONG Capabilities;
    ULONG CommandCount;
    ULONG Control;
    ULONG HeaderSize;
    ULONG ImplementedPorts;
    PHYSICAL_ADDRESS PhysicalAddress;
    PAHCI_PORT Port;
    ULONG PortIndex;
    KSTATUS Status;
    ULONG Value;

    //
    // Enable the host controller.
    //

    Control = AHCI_READ_GLOBAL(Controller, AhciHostControl);
    Control |= AHCI_HOST_CONTROL_ENABLE;
    AHCI_WRITE_GLOBAL(Controller, AhciHostControl, Control);
    ImplementedPorts = AHCI_READ_GLOBAL(Controller, AhciPortsImplemented);
    if (ImplementedPorts == 0) {
        RtlDebugPrint("AHCI: No implemented ports. Assuming all.\n");
        ImplementedPorts = 0xFFFFFFFF;
    }

    //
    // Take over from the BIOS.
    //

    Status = AhcipPerformBiosHandoff(Controller);
    if (!KSUCCESS(Status)) {
        goto ResetControllerEnd;
    }

    //
    // Figure out some of the device capabilities. Mask off unsupported ports
    // in case the BIOS set a crazy value.
    //

    Capabilities = AHCI_READ_GLOBAL(Controller, AhciHostCapabilities);
    Controller->PortCount = (Capabilities &
                             AHCI_HOST_CAPABILITY_PORT_COUNT_MASK) + 1;

    if (Controller->PortCount < AHCI_PORT_COUNT) {
        ImplementedPorts &= (1 << Controller->PortCount) - 1;
    }

    Controller->ImplementedPorts = ImplementedPorts;

    //
    // Figure out the number of commands that can be simultaneously queued to
    // each port. If native queuing is not supported, then there's not much
    // point.
    //

    CommandCount = (Capabilities & AHCI_HOST_CAPABILITY_COMMAND_SLOTS_MASK) >>
                   AHCI_HOST_CAPABILITY_COMMAND_SLOTS_SHIFT;

    if (((Capabilities & AHCI_HOST_CAPABILITY_NATIVE_QUEUING) == 0) ||
        ((Capabilities & AHCI_HOST_CAPABILITY_SNOTIFICATION) == 0)) {

        CommandCount = 0;
    }

    CommandCount += 1;
    Controller->CommandCount = CommandCount;
    Controller->MaxPhysical = MAX_ULONG;
    if ((Capabilities & AHCI_HOST_CAPABILITY_64BIT) != 0) {
        Controller->MaxPhysical = MAX_ULONGLONG;
    }

    //
    // Initialize each port.
    //

    for (PortIndex = 0; PortIndex < AHCI_PORT_COUNT; PortIndex += 1) {
        Port = &(Controller->Ports[PortIndex]);
        Port->PortBase = Controller->ControllerBase +
                         (PortIndex * AHCI_PORT_REGISTER_OFFSET);

        //
        // Skip unimplemented ports.
        //

        if ((ImplementedPorts & (1 << PortIndex)) == 0) {
            continue;
        }

        Status = AhcipStopPort(Port);
        if (!KSUCCESS(Status)) {
            continue;
        }

        Port->PendingCommands = 0;
        if (CommandCount >= 32) {
            Port->CommandMask = ~0;

        } else {
            Port->CommandMask = (1 << CommandCount) - 1;
        }

        //
        // Allocate the command list and receive FIS area if not already done.
        // Without port multipliers the receive FIS size is only 256 bytes, so
        // it could technically all fit in one page. But with port multipliers
        // receive needs a whole page (256 * 16), so just do it anyway.
        //

        if (Port->CommandIoBuffer == NULL) {
            AllocationSize = (sizeof(AHCI_COMMAND_HEADER) * CommandCount);
            AllocationSize = ALIGN_RANGE_UP(AllocationSize,
                                            AHCI_COMMAND_TABLE_ALIGNMENT);

            HeaderSize = AllocationSize;
            AllocationSize += sizeof(AHCI_COMMAND_TABLE) * CommandCount;
            Port->CommandIoBuffer = MmAllocateNonPagedIoBuffer(
                                         0,
                                         Controller->MaxPhysical,
                                         AHCI_COMMAND_TABLE_ALIGNMENT,
                                         AllocationSize,
                                         IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS);

            if (Port->CommandIoBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto ResetControllerEnd;
            }

            ASSERT(Port->CommandIoBuffer->FragmentCount == 1);

            Address = Port->CommandIoBuffer->Fragment[0].VirtualAddress;
            Port->Commands = Address;
            RtlZeroMemory(Port->Commands, AllocationSize);
            Port->Tables = Address + HeaderSize;
            Port->TablesPhysical =
                Port->CommandIoBuffer->Fragment[0].PhysicalAddress + HeaderSize;

            ASSERT(IS_ALIGNED(Port->TablesPhysical,
                              AHCI_COMMAND_TABLE_ALIGNMENT));
        }

        if (Port->ReceiveIoBuffer == NULL) {
            Port->ReceiveIoBuffer = MmAllocateNonPagedIoBuffer(
                                         0,
                                         Controller->MaxPhysical,
                                         AHCI_RECEIVE_FIS_MAX_SIZE,
                                         AHCI_RECEIVE_FIS_MAX_SIZE,
                                         IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS);

            if (Port->ReceiveIoBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto ResetControllerEnd;
            }

            ASSERT(Port->ReceiveIoBuffer->FragmentCount == 1);

            Port->ReceivedFis =
                             Port->ReceiveIoBuffer->Fragment[0].VirtualAddress;

            RtlZeroMemory(Port->ReceivedFis, AHCI_RECEIVE_FIS_MAX_SIZE);
        }

        //
        // Set up the port bases, but don't enable start or receive. The spec
        // says that the start bit should not be set until software has
        // examined the task file bits.
        //

        PhysicalAddress = Port->CommandIoBuffer->Fragment[0].PhysicalAddress;

        ASSERT(PhysicalAddress <= Controller->MaxPhysical);

        AHCI_WRITE(Port, AhciPortCommandListBase, (ULONG)PhysicalAddress);
        AHCI_WRITE(Port,
                   AhciPortCommandListBaseHigh,
                   (ULONG)(PhysicalAddress >> 32));

        PhysicalAddress = Port->ReceiveIoBuffer->Fragment[0].PhysicalAddress;

        ASSERT(PhysicalAddress <= Controller->MaxPhysical);

        AHCI_WRITE(Port, AhciPortFisBase, (ULONG)PhysicalAddress);
        AHCI_WRITE(Port, AhciPortFisBaseHigh, (ULONG)(PhysicalAddress >> 32));
        Value = AHCI_READ(Port, AhciPortSataControl);
        Value &= ~AHCI_PORT_SATA_CONTROL_DETECTION_MASK;
        AHCI_WRITE(Port, AhciPortSataControl, Value);
        Value = AHCI_READ(Port, AhciPortCommand);
        Value |= AHCI_PORT_COMMAND_FIS_RX_ENABLE;
        AHCI_WRITE(Port, AhciPortCommand, Value);
        AHCI_WRITE(Port, AhciPortSataError, 0xFFFFFFFF);
        AHCI_WRITE(Port, AhciPortInterruptStatus, 0xFFFFFFFF);
        Value = AHCI_INTERRUPT_DEFAULT_ENABLE;
        AHCI_WRITE(Port, AhciPortInterruptEnable, Value);
    }

    //
    // Enable interrupts globally.
    //

    AHCI_WRITE_GLOBAL(Controller, AhciInterruptStatus, 0xFFFFFFFF);
    Control = AHCI_READ_GLOBAL(Controller, AhciHostControl);
    Control |= AHCI_HOST_CONTROL_INTERRUPT_ENABLE;
    AHCI_WRITE_GLOBAL(Controller, AhciHostControl, Control);
    Status = STATUS_SUCCESS;

ResetControllerEnd:
    return Status;
}

KSTATUS
AhcipProbePort (
    PAHCI_CONTROLLER Controller,
    ULONG PortIndex
    )

/*++

Routine Description:

    This routine probes an AHCI port to determine whether or not there is a
    drive there.

Arguments:

    Controller - Supplies a pointer to the AHCI controller.

    PortIndex - Supplies the port number to probe.

Return Value:

    STATUS_SUCCESS if there is a device ready behind the given port.

    STATUS_NO_MEDIA if there is nothing plugged into the port, or the port is
    unimplemented by the hardware.

    Other error codes on failure.

--*/

{

    ULONG Command;
    PAHCI_PORT Port;
    ULONG SataStatus;
    ULONG TaskFile;
    ULONGLONG Time;
    ULONGLONG Timeout;

    //
    // Skip unimplemented ports.
    //

    if ((Controller->ImplementedPorts & (1 << PortIndex)) == 0) {
        return STATUS_NO_MEDIA;
    }

    Port = &(Controller->Ports[PortIndex]);
    SataStatus = AHCI_READ(Port, AhciPortSataStatus);

    //
    // If the drive is already up and running, return now.
    //

    if ((SataStatus & AHCI_PORT_SATA_STATUS_DETECTION_MASK) !=
        AHCI_PORT_SATA_STATUS_DETECTION_PHY) {

        //
        // Set the spin-up bit. For controllers that don't support staggered
        // spin-up this will already be 1, so it does no harm to set.
        //

        Command = AHCI_READ(Port, AhciPortCommand);
        Command |= AHCI_PORT_COMMAND_SPIN_UP_DEVICE;
        AHCI_WRITE(Port, AhciPortCommand, Command);

        //
        // Wait up to 50 milliseconds for the PHY to come up.
        //

        Time = 0;
        Timeout = 0;
        SataStatus = AHCI_READ(Port, AhciPortSataStatus);
        while (((SataStatus & AHCI_PORT_SATA_STATUS_DETECTION_MASK) !=
                AHCI_PORT_SATA_STATUS_DETECTION_PHY) &&
               (Time <= Timeout)) {

            Time = HlQueryTimeCounter();
            if (Timeout == 0) {
                Timeout = Time + ((AHCI_PHY_DETECT_TIMEOUT_MS *
                                   HlQueryTimeCounterFrequency()) /
                                  MILLISECONDS_PER_SECOND);
            }

            SataStatus = AHCI_READ(Port, AhciPortSataStatus);
        }
    }

    if ((SataStatus & AHCI_PORT_SATA_STATUS_DETECTION_MASK) !=
        AHCI_PORT_SATA_STATUS_DETECTION_PHY) {

        return STATUS_NO_MEDIA;
    }

    TaskFile = AHCI_READ(Port, AhciPortTaskFile);
    if ((TaskFile & AHCI_PORT_TASK_ERROR_MASK) != 0) {
        RtlDebugPrint("AHCI: PHY detected on port %d, but drive status is "
                      "0x%x\n",
                      PortIndex,
                      TaskFile);

        return STATUS_NO_MEDIA;
    }

    //
    // Now that everything's verified, start up the port.
    //

    Command = AHCI_READ(Port, AhciPortCommand);
    Command |= AHCI_PORT_COMMAND_START | AHCI_PORT_COMMAND_FIS_RX_ENABLE;
    AHCI_WRITE(Port, AhciPortCommand, Command);
    return STATUS_SUCCESS;
}

KSTATUS
AhcipEnumeratePort (
    PAHCI_PORT Port
    )

/*++

Routine Description:

    This routine enumerates the drive behind the AHCI port.

Arguments:

    Port - Supplies a pointer to the AHCI port to start.

Return Value:

    Status code.

--*/

{

    PAHCI_COMMAND_TABLE Command;
    PSATA_FIS_REGISTER_H2D Fis;
    PAHCI_COMMAND_HEADER Header;
    LONG HeaderIndex;
    PATA_IDENTIFY_PACKET Identify;
    PIO_BUFFER IoBuffer;
    RUNLEVEL OldRunLevel;
    PAHCI_PRDT Prdt;
    KSTATUS Status;
    ULONG TaskFile;

    IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                          Port->Controller->MaxPhysical,
                                          ATA_SECTOR_SIZE,
                                          ATA_SECTOR_SIZE,
                                          IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS);

    if (IoBuffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ASSERT(IoBuffer->FragmentCount == 1);

    Identify = IoBuffer->Fragment[0].VirtualAddress;
    RtlZeroMemory(Identify, sizeof(ATA_IDENTIFY_PACKET));
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Port->DpcLock));
    HeaderIndex = AhcipAllocateCommand(Port);
    if (HeaderIndex < 0) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EnumeratePortEnd;
    }

    Header = &(Port->Commands[HeaderIndex]);
    Command = &(Port->Tables[HeaderIndex]);
    RtlZeroMemory(&(Command->CommandFis), sizeof(Command->CommandFis));
    Fis = (PSATA_FIS_REGISTER_H2D)&(Command->CommandFis);
    Fis->Type = SataFisRegisterH2d;
    Fis->Flags = SATA_FIS_REGISTER_H2D_FLAG_COMMAND;
    Fis->Command = AtaCommandIdentify;
    Fis->Device = ATA_DRIVE_SELECT_LBA;
    SATA_SET_FIS_COUNT(Fis, 1);
    Header->Control = AHCI_COMMAND_FIS_SIZE(sizeof(SATA_FIS_REGISTER_H2D));
    Header->PrdtLength = 1;
    Prdt = &(Command->Prdt[0]);
    Prdt->AddressLow = (ULONG)(IoBuffer->Fragment[0].PhysicalAddress);
    Prdt->AddressHigh = (ULONG)(IoBuffer->Fragment[0].PhysicalAddress >> 32);
    Prdt->Reserved = 0;
    Prdt->Count = ATA_SECTOR_SIZE - 1;

    //
    // Submit the command for execution.
    //

    AhcipSubmitCommand(Port, 1 << HeaderIndex);

    //
    // Wait for the command to complete.
    //

    KeReleaseSpinLock(&(Port->DpcLock));
    KeLowerRunLevel(OldRunLevel);
    while ((Port->PendingCommands & (1 << HeaderIndex)) != 0) {
        KeYield();
    }

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Port->DpcLock));
    TaskFile = AHCI_READ(Port, AhciPortTaskFile);
    if ((TaskFile & AHCI_PORT_TASK_ERROR_MASK) != 0) {
        Status = STATUS_DEVICE_IO_ERROR;
        goto EnumeratePortEnd;
    }

    ASSERT(Header->Size == ATA_SECTOR_SIZE);

    //
    // Get the total capacity of the disk.
    //

    if ((Identify->CommandSetSupported & ATA_SUPPORTED_COMMAND_LBA48) != 0) {
        Port->TotalSectors = Identify->TotalSectorsLba48;
        Port->Flags |= AHCI_PORT_LBA48;

    } else {
        Port->TotalSectors = Identify->TotalSectors;
    }

    Status = STATUS_SUCCESS;

EnumeratePortEnd:
    if (HeaderIndex >= 0) {
        AhcipFreeCommand(Port, HeaderIndex);
    }

    KeReleaseSpinLock(&(Port->DpcLock));
    KeLowerRunLevel(OldRunLevel);
    MmFreeIoBuffer(IoBuffer);
    return Status;
}

KSTATUS
AhcipEnqueueIrp (
    PAHCI_PORT Port,
    PIRP Irp
    )

/*++

Routine Description:

    This routine begins I/O on a fresh IRP.

Arguments:

    Port - Supplies a pointer to the port.

    Irp - Supplies a pointer to the read/write IRP.

Return Value:

    STATUS_SUCCESS if the IRP was successfully started or even queued.

    Error code on failure.

--*/

{

    LONG HeaderIndex;
    RUNLEVEL OldRunLevel;
    KSTATUS Status;

    IoPendIrp(AhciDriver, Irp);

    //
    // Attempt to grab resources. If failed, add this IRP to the queue
    // atomically so it's always clear who is taking care of the queued IRP.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Port->DpcLock));

    //
    // If the device disappeared, fail the I/O now.
    //

    if (Port->OsDevice == NULL) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto EnqueueIrpEnd;
    }

    HeaderIndex = AhcipAllocateCommand(Port);

    //
    // Transfers may already be in progress that are taking up all the command
    // slots. Queue the command if so. If a real allocation failure occured,
    // bail out.
    //

    if (HeaderIndex < 0) {
        INSERT_BEFORE(&(Irp->ListEntry), &(Port->IrpQueue));
        Status = STATUS_SUCCESS;
        goto EnqueueIrpEnd;
    }

    ASSERT(Port->CommandState[HeaderIndex].Irp == NULL);

    Port->CommandState[HeaderIndex].Irp = Irp;
    if (Irp->MajorCode == IrpMajorIo) {
        AhcipPerformDmaIo(Port, Irp, HeaderIndex);

    } else if (Irp->MajorCode == IrpMajorSystemControl) {

        ASSERT(Irp->MinorCode == IrpMinorSystemControlSynchronize);

        AhcipExecuteCacheFlush(Port, HeaderIndex);

    } else {

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        AhcipFreeCommand(Port, HeaderIndex);
        goto EnqueueIrpEnd;
    }

    Status = STATUS_SUCCESS;

EnqueueIrpEnd:
    KeReleaseSpinLock(&(Port->DpcLock));
    KeLowerRunLevel(OldRunLevel);
    return Status;
}

VOID
AhcipProcessPortRemoval (
    PAHCI_PORT Port,
    BOOL CanTouchPort
    )

/*++

Routine Description:

    This routine kills all remaining pending and queued transfers in the port,
    completing them with no such device. There still might be IRPs that have
    been claimed but not quite processed by the interrupt code.

Arguments:

    Port - Supplies a pointer to the port.

    CanTouchPort - Supplies a boolean indicating if the port registers can be
        accessed or not. During a removal of a disk, they can be. But if the
        entire AHCI controller is removed, they cannot be.

Return Value:

    None.

--*/

{

    ULONG Bit;
    PIRP Irp;
    RUNLEVEL OldRunLevel;
    ULONG Pending;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Port->DpcLock));
    if (CanTouchPort != FALSE) {
        AhcipStopPort(Port);
    }

    //
    // Clear out all pending commands.
    //

    Pending = Port->PendingCommands;
    Port->PendingCommands = 0;
    for (Bit = 0; Bit < AHCI_COMMAND_COUNT; Bit += 1) {
        if ((Pending & (1 << Bit)) == 0) {
            continue;
        }

        Irp = Port->CommandState[Bit].Irp;
        Port->CommandState[Bit].Irp = NULL;
        IoCompleteIrp(AhciDriver, Irp, STATUS_NO_SUCH_DEVICE);
        Pending &= ~(1 << Bit);
        if (Pending == 0) {
            break;
        }
    }

    //
    // Also clear out all pending IRPs on the queue.
    //

    while (!LIST_EMPTY(&(Port->IrpQueue))) {
        Irp = LIST_VALUE(Port->IrpQueue.Next, IRP, ListEntry);
        LIST_REMOVE(&(Irp->ListEntry));
        IoCompleteIrp(AhciDriver, Irp, STATUS_NO_SUCH_DEVICE);
    }

    Port->OsDevice = NULL;
    Port->TotalSectors = 0;
    Port->Flags = 0;
    KeReleaseSpinLock(&(Port->DpcLock));
    KeLowerRunLevel(OldRunLevel);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
AhcipProcessInterrupt (
    PAHCI_PORT Port
    )

/*++

Routine Description:

    This routine processes any interrupts within the given port.

Arguments:

    Port - Supplies a pointer to the port.

Return Value:

    None.

--*/

{

    LONG Bit;
    BOOL CommandInUse;
    BOOL CompleteIrp;
    ULONG Finished;
    ULONG Interrupt;
    UINTN IoSize;
    PIRP Irp;
    ULONG NewPending;
    KSTATUS Status;
    ULONG TaskFile;

    Interrupt = RtlAtomicExchange(&(Port->PendingInterrupts), 0);
    if (Interrupt == 0) {
        return;
    }

    //
    // Update the pending command mask atomically.
    //

    KeAcquireSpinLock(&(Port->DpcLock));

    //
    // If something changed, re-enumerate the drives on the controller.
    //

    if ((Interrupt & AHCI_INTERRUPT_CONNECTION_MASK) != 0) {
        RtlDebugPrint("AHCI: Port Connection Change %x\n",
                      Interrupt & AHCI_INTERRUPT_CONNECTION_MASK);

        IoNotifyDeviceTopologyChange(Port->Controller->OsDevice);
        Interrupt &= ~AHCI_INTERRUPT_CONNECTION_MASK;
    }

    if ((Interrupt & AHCI_INTERRUPT_ERROR_MASK) != 0) {
        RtlDebugPrint("AHCI: Error %x\n", Interrupt);
        Interrupt &= ~AHCI_INTERRUPT_ERROR_MASK;
    }

    ASSERT((Interrupt &
            (AHCI_INTERRUPT_D2H_REGISTER_FIS |
             AHCI_INTERRUPT_PIO_SETUP_FIS)) != 0);

    Interrupt &= ~(AHCI_INTERRUPT_D2H_REGISTER_FIS |
                   AHCI_INTERRUPT_PIO_SETUP_FIS);

    if (Interrupt != 0) {
        RtlDebugPrint("AHCI: Got unknown interrupt 0x%x\n", Interrupt);
    }

    //
    // See which commands are no longer outstanding.
    //

    NewPending = AHCI_READ(Port, AhciPortCommandIssue);
    Finished = (NewPending ^ Port->PendingCommands) & Port->PendingCommands;

    //
    // Commands better not be magically starting.
    //

    ASSERT(((NewPending ^ Port->PendingCommands) &
            ~Port->PendingCommands) == 0);

    TaskFile = AHCI_READ(Port, AhciPortTaskFile);
    Status = STATUS_SUCCESS;
    if ((TaskFile & AHCI_PORT_TASK_ERROR_MASK) != 0) {
        RtlDebugPrint("AHCI: I/O Error status: %x\n", TaskFile);
        Status = STATUS_DEVICE_IO_ERROR;
    }

    Port->PendingCommands = NewPending;

    //
    // Loop over all the commands that have finished.
    //

    for (Bit = 0; Bit < AHCI_COMMAND_COUNT; Bit += 1) {
        if ((Finished & (1 << Bit)) == 0) {
            continue;
        }

        Irp = Port->CommandState[Bit].Irp;
        IoSize = Port->CommandState[Bit].IoSize;
        Port->CommandState[Bit].IoSize = 0;
        CommandInUse = FALSE;
        CompleteIrp = FALSE;

        //
        // If there was no IRP, assume things are being handled manually. This
        // happens during the IDENTIFY command.
        //

        if (Irp == NULL) {
            CommandInUse = TRUE;

        } else if (KSUCCESS(Status)) {

            ASSERT(Port->Commands[Bit].Size == IoSize);

            if (Irp->MajorCode == IrpMajorIo) {
                Irp->U.ReadWrite.IoBytesCompleted += IoSize;
                Irp->U.ReadWrite.NewIoOffset += IoSize;
            }

            //
            // If this isn't an I/O request, just complete it.
            //

            if (Irp->MajorCode == IrpMajorIo) {

                //
                // If this is a synchronized write, then send a cache flush
                // command along with it. Use the IoSize as a hint as to
                // whether or not the cache flush part has already gone around.
                //

                if ((Irp->MinorCode == IrpMinorIoWrite) &&
                    ((Irp->U.ReadWrite.IoFlags &
                      IO_FLAG_DATA_SYNCHRONIZED) != 0) &&
                    (Irp->U.ReadWrite.IoBytesCompleted >=
                     Irp->U.ReadWrite.IoSizeInBytes) &&
                    (IoSize != 0)) {

                    AhcipExecuteCacheFlush(Port, Bit);
                    CommandInUse = TRUE;

                //
                // If the IRP is not finished, queue up the next part. The
                // command table will be in use then.
                //

                } else if (Irp->U.ReadWrite.IoBytesCompleted <
                           Irp->U.ReadWrite.IoSizeInBytes) {

                    AhcipPerformDmaIo(Port, Irp, Bit);
                    CommandInUse = TRUE;

                //
                // The IRP completed all its I/O.
                //

                } else {
                    CompleteIrp = TRUE;
                }

            //
            // Non I/O IRPs like flush just complete.
            //

            } else {
                CompleteIrp = TRUE;
            }
        }

        if (CompleteIrp != FALSE) {
            Port->CommandState[Bit].Irp = NULL;
            IoCompleteIrp(AhciDriver, Irp, Status);
        }

        //
        // Begin the next IRP reusing this command table if there's more
        // to do.
        //

        if (CommandInUse == FALSE) {
            AhcipBeginNextIrp(Port, Bit);
        }

        Finished &= ~(1 << Bit);
        if (Finished == 0) {
            break;
        }
    }

    KeReleaseSpinLock(&(Port->DpcLock));
    return;
}

KSTATUS
AhcipPerformBiosHandoff (
    PAHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine performs the BIOS handoff procedure to allow the OS to take
    over control of the AHCI controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TIMEOUT if the port could not be stopped.

--*/

{

    ULONGLONG Frequency;
    ULONG Handoff;
    ULONGLONG Time;
    ULONGLONG Timeout;

    //
    // If the controller doesn't have the BIOS handoff capability, then do
    // nothing.
    //

    if ((AHCI_READ_GLOBAL(Controller, AhciHostCapabilities2) &
         AHCI_HOST_CAPABILITY2_BIOS_HANDOFF) == 0) {

        return STATUS_SUCCESS;
    }

    //
    // Set the OS owned bit.
    //

    Handoff = AHCI_READ_GLOBAL(Controller, AhciBiosHandoff);
    Handoff |= AHCI_BIOS_HANDOFF_OS_OWNED;
    AHCI_WRITE_GLOBAL(Controller, AhciBiosHandoff, Handoff);

    //
    // The original timeout is 25 milliseconds (double it for safety). If the
    // BIOS gets the busy bit up by then, increase the timeout to 2 seconds.
    //

    Frequency = HlQueryTimeCounterFrequency();
    Time = HlQueryTimeCounter();
    Timeout = Time + ((50 * Frequency) / MILLISECONDS_PER_SECOND);
    Handoff = AHCI_READ_GLOBAL(Controller, AhciBiosHandoff);
    while ((Time <= Timeout) &&
           ((Handoff &
             (AHCI_BIOS_HANDOFF_BIOS_OWNED |
              AHCI_BIOS_HANDOFF_BIOS_BUSY)) != 0)) {

        if ((Handoff & AHCI_BIOS_HANDOFF_BIOS_BUSY) != 0) {
            Timeout = Time + (2 * Frequency);
        }

        Time = HlQueryTimeCounter();
        Handoff = AHCI_READ_GLOBAL(Controller, AhciBiosHandoff);
    }

    if ((Handoff &
         (AHCI_BIOS_HANDOFF_BIOS_OWNED | AHCI_BIOS_HANDOFF_BIOS_BUSY)) != 0) {

        RtlDebugPrint("AHCI: Failed BIOS handoff: %x\n", Handoff);
        return STATUS_TIMEOUT;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AhcipStopPort (
    PAHCI_PORT Port
    )

/*++

Routine Description:

    This routine stops an AHCI port if it is running.

Arguments:

    Port - Supplies a pointer to the port.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TIMEOUT if the port could not be stopped.

--*/

{

    ULONG Command;
    ULONG Mask;
    ULONGLONG Time;
    ULONGLONG Timeout;

    Command = AHCI_READ(Port, AhciPortCommand);
    Mask = AHCI_PORT_COMMAND_START | AHCI_PORT_COMMAND_LIST_RUNNING |
           AHCI_PORT_COMMAND_FIS_RX_ENABLE | AHCI_PORT_COMMAND_FIS_RX_RUNNING;

    if ((Command & Mask) == 0) {
        return STATUS_SUCCESS;
    }

    //
    // Clear the start and RX enable bits, and wait for them to go to zero.
    // The spec says to wait 500ms. Double it for safety.
    //

    Command &= ~(AHCI_PORT_COMMAND_START | AHCI_PORT_COMMAND_FIS_RX_ENABLE);
    AHCI_WRITE(Port, AhciPortCommand, Command);
    Time = HlQueryTimeCounter();
    Timeout = Time + HlQueryTimeCounterFrequency();
    Command = AHCI_READ(Port, AhciPortCommand);
    while (((Command & Mask) != 0) && (Time <= Timeout)) {
        Command = AHCI_READ(Port, AhciPortCommand);
        Time = HlQueryTimeCounter();
    }

    if ((Command & Mask) != 0) {
        RtlDebugPrint("AHCI: Failed to stop: %x\n", Command);
        return STATUS_TIMEOUT;
    }

    return STATUS_SUCCESS;
}

VOID
AhcipBeginNextIrp (
    PAHCI_PORT Port,
    LONG HeaderIndex
    )

/*++

Routine Description:

    This routine begins processing for the next queued I/O IRP given a
    command index and table already (reused from the previous command). If
    there is no work left to do, the command is freed. The port lock must be
    held.

Arguments:

    Port - Supplies a pointer to the port.

    HeaderIndex - Supplies the header index to use. The header had better be
        pointing at the command table already.

Return Value:

    None.

--*/

{

    PIRP Irp;

    ASSERT(KeIsSpinLockHeld(&(Port->DpcLock)) != FALSE);

    if (!LIST_EMPTY(&(Port->IrpQueue))) {
        Irp = LIST_VALUE(Port->IrpQueue.Next, IRP, ListEntry);
        LIST_REMOVE(&(Irp->ListEntry));
        Port->CommandState[HeaderIndex].Irp = Irp;
        if (Irp->MajorCode == IrpMajorIo) {
            AhcipPerformDmaIo(Port, Irp, HeaderIndex);

        } else if (Irp->MajorCode == IrpMajorSystemControl) {

            ASSERT(Irp->MinorCode == IrpMinorSystemControlSynchronize);

            AhcipExecuteCacheFlush(Port, HeaderIndex);
        }

    } else {
        Port->CommandState[HeaderIndex].Irp = NULL;
        AhcipFreeCommand(Port, HeaderIndex);
    }

    return;
}

VOID
AhcipPerformDmaIo (
    PAHCI_PORT Port,
    PIRP Irp,
    LONG HeaderIndex
    )

/*++

Routine Description:

    This routine fills out and executes a DMA I/O command.

Arguments:

    Port - Supplies a pointer to the port.

    Irp - Supplies a pointer to the read/write IRP.

    HeaderIndex - Supplies the header index to use. The header had better be
        pointing at the command table already.

Return Value:

    Status code.

--*/

{

    ULONGLONG BlockAddress;
    UINTN BytesPreviouslyCompleted;
    UINTN BytesToComplete;
    ATA_COMMAND Command;
    PAHCI_COMMAND_TABLE CommandTable;
    UCHAR DeviceSelect;
    UINTN EntrySize;
    PSATA_FIS_REGISTER_H2D Fis;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    PAHCI_COMMAND_HEADER Header;
    PIO_BUFFER IoBuffer;
    UINTN IoBufferOffset;
    ULONGLONG IoOffset;
    UINTN MaxTransferSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PAHCI_PRDT Prdt;
    ULONG PrdtIndex;
    ULONG SectorCount;
    UINTN TransferSize;
    UINTN TransferSizeRemaining;
    BOOL Write;

    IoBuffer = Irp->U.ReadWrite.IoBuffer;
    BytesPreviouslyCompleted = Irp->U.ReadWrite.IoBytesCompleted;
    BytesToComplete = Irp->U.ReadWrite.IoSizeInBytes;
    IoOffset = Irp->U.ReadWrite.NewIoOffset;

    ASSERT(BytesPreviouslyCompleted < BytesToComplete);
    ASSERT(IoOffset == (Irp->U.ReadWrite.IoOffset + BytesPreviouslyCompleted));
    ASSERT(IS_ALIGNED(IoOffset, ATA_SECTOR_SIZE) != FALSE);
    ASSERT(IS_ALIGNED(BytesToComplete, ATA_SECTOR_SIZE) != FALSE);

    //
    // Determine the bytes to complete this round.
    //

    MaxTransferSize = ATA_MAX_LBA48_SECTOR_COUNT * ATA_SECTOR_SIZE;
    if ((Port->Flags & AHCI_PORT_LBA48) == 0) {
        MaxTransferSize = ATA_MAX_LBA28_SECTOR_COUNT * ATA_SECTOR_SIZE;
    }

    TransferSize = BytesToComplete - BytesPreviouslyCompleted;
    if (TransferSize > MaxTransferSize) {
        TransferSize = MaxTransferSize;
    }

    if (TransferSize == 0) {
        AhcipFreeCommand(Port, HeaderIndex);
        IoCompleteIrp(AhciDriver, Irp, STATUS_SUCCESS);
        return;
    }

    Write = FALSE;
    if (Irp->MinorCode == IrpMinorIoWrite) {
        Write = TRUE;
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

    CommandTable = &(Port->Tables[HeaderIndex]);
    Prdt = &(CommandTable->Prdt[0]);
    PrdtIndex = 0;
    TransferSizeRemaining = TransferSize;
    while ((TransferSizeRemaining != 0) && (PrdtIndex < AHCI_PRDT_COUNT)) {

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

        if (EntrySize > AHCI_PRDT_MAX_SIZE) {
            EntrySize = AHCI_PRDT_MAX_SIZE;
        }

        ASSERT(IS_ALIGNED(EntrySize, 2));

        PhysicalAddress = Fragment->PhysicalAddress + FragmentOffset;
        TransferSizeRemaining -= EntrySize;

        ASSERT(PhysicalAddress + EntrySize <= Port->Controller->MaxPhysical);

        Prdt->AddressLow = (ULONG)PhysicalAddress;
        Prdt->AddressHigh = (ULONG)(PhysicalAddress >> 32);
        Prdt->Reserved = 0;
        Prdt->Count = EntrySize - 1;
        Prdt += 1;
        PrdtIndex += 1;
        FragmentOffset += EntrySize;
        if (FragmentOffset >= Fragment->Size) {
            FragmentIndex += 1;
            FragmentOffset = 0;
        }
    }

    ASSERT(PrdtIndex != 0);

    TransferSize -= TransferSizeRemaining;
    BlockAddress = IoOffset / ATA_SECTOR_SIZE;
    SectorCount = TransferSize / ATA_SECTOR_SIZE;
    Port->CommandState[HeaderIndex].IoSize = TransferSize;

    //
    // Use LBA48 if the block address is too high or the sector size is too
    // large.
    //

    DeviceSelect = ATA_DRIVE_SELECT_LBA;
    if ((BlockAddress > ATA_MAX_LBA28) ||
        (SectorCount > ATA_MAX_LBA28_SECTOR_COUNT)) {

        if (Write != FALSE) {
            Command = AtaCommandWriteDma48;

        } else {
            Command = AtaCommandReadDma48;
        }

    } else {
        if (Write != FALSE) {
            Command = AtaCommandWriteDma28;

        } else {
            Command = AtaCommandReadDma28;
        }

        //
        // The upper 4 bits of the LBA go in the device select register for
        // LBA28.
        //

        DeviceSelect |= (BlockAddress >> 24) & 0xF;
        BlockAddress &= 0x00FFFFFF;

        //
        // A value of 0 indicates 0x100 sectors in LBA28.
        //

        if (SectorCount == ATA_MAX_LBA28_SECTOR_COUNT) {
            SectorCount = 0;
        }
    }

    //
    // Fill out the command FIS.
    //

    Fis = (PSATA_FIS_REGISTER_H2D)CommandTable->CommandFis;
    RtlZeroMemory(Fis, sizeof(CommandTable->CommandFis));
    Fis->Type = SataFisRegisterH2d;
    Fis->Flags = SATA_FIS_REGISTER_H2D_FLAG_COMMAND;
    Fis->Command = Command;
    SATA_SET_FIS_LBA(Fis, BlockAddress);
    Fis->Device = DeviceSelect;
    SATA_SET_FIS_COUNT(Fis, SectorCount);
    Header = &(Port->Commands[HeaderIndex]);
    Header->Control = AHCI_COMMAND_FIS_SIZE(sizeof(SATA_FIS_REGISTER_H2D));
    if (Write != FALSE) {
        Header->Control |= AHCI_COMMAND_HEADER_WRITE;
    }

    Header->PrdtLength = PrdtIndex;
    Header->Size = 0;
    AhcipSubmitCommand(Port, 1 << HeaderIndex);
    return;
}

VOID
AhcipExecuteCacheFlush (
    PAHCI_PORT Port,
    LONG Index
    )

/*++

Routine Description:

    This routine executes a cache flush command on the given port using the
    given header index.

Arguments:

    Port - Supplies a pointer to the port.

    Index - Supplies the command header index returned during allocate.

Return Value:

    None.

--*/

{

    PAHCI_COMMAND_TABLE Command;
    PSATA_FIS_REGISTER_H2D Fis;
    PAHCI_COMMAND_HEADER Header;

    ASSERT(KeIsSpinLockHeld(&(Port->DpcLock)) != FALSE);
    ASSERT((Index >= 0) &&
           ((Port->AllocatedCommands & (1 << Index)) != 0) &&
           ((Port->PendingCommands & (1 << Index)) == 0));

    Header = &(Port->Commands[Index]);
    Header->Size = 0;
    Command = &(Port->Tables[Index]);
    RtlZeroMemory(&(Command->CommandFis), sizeof(Command->CommandFis));
    Fis = (PSATA_FIS_REGISTER_H2D)&(Command->CommandFis);
    Fis->Type = SataFisRegisterH2d;
    Fis->Flags = SATA_FIS_REGISTER_H2D_FLAG_COMMAND;
    Fis->Command = AtaCommandCacheFlush28;
    Fis->Device = ATA_DRIVE_SELECT_LBA;
    Header->Control = AHCI_COMMAND_FIS_SIZE(sizeof(SATA_FIS_REGISTER_H2D));
    Header->PrdtLength = 0;

    //
    // Submit the command for execution.
    //

    AhcipSubmitCommand(Port, 1 << Index);
    return;
}

LONG
AhcipAllocateCommand (
    PAHCI_PORT Port
    )

/*++

Routine Description:

    This routine allocates an AHCI command header and corresponding command
    table entry.

Arguments:

    Port - Supplies a pointer to the port.

Return Value:

    Returns a command header index on success.

    -1 on allocation failure.

--*/

{

    ULONG AllocatedMask;
    ULONG Bit;
    PAHCI_COMMAND_HEADER CommandHeader;
    ULONG Mask;
    PHYSICAL_ADDRESS PhysicalAddress;

    //
    // If there's only one command, then just allocate it. Or don't.
    //

    if ((Port->Flags & AHCI_PORT_NATIVE_COMMAND_QUEUING) == 0) {
        if (Port->AllocatedCommands != 0) {
            return -1;
        }

        Port->AllocatedCommands = 1;
        Bit = 0;

    //
    // Get the first free entry.
    //

    } else {
        Mask = Port->CommandMask;
        AllocatedMask = Port->AllocatedCommands & Mask;

        //
        // If everything is allocated, fail.
        //

        if (AllocatedMask == Mask) {
            return -1;
        }

        if (AllocatedMask == 0) {
            Bit = 0;

        } else {
            Bit = RtlCountTrailingZeros32(~AllocatedMask);
        }

        ASSERT((1 << Bit) <= Mask);

        Port->AllocatedCommands |= 1 << Bit;
    }

    PhysicalAddress = Port->TablesPhysical + (sizeof(AHCI_COMMAND_TABLE) * Bit);

    //
    // Fill out the command header with the physical address of the command
    // table.
    //

    ASSERT((IS_ALIGNED(PhysicalAddress, AHCI_COMMAND_TABLE_ALIGNMENT)) &&
           (PhysicalAddress <= Port->Controller->MaxPhysical));

    CommandHeader = &(Port->Commands[Bit]);
    RtlZeroMemory(CommandHeader, sizeof(AHCI_COMMAND_HEADER));
    CommandHeader->CommandTableLow = (ULONG)PhysicalAddress;
    CommandHeader->CommandTableHigh = (ULONG)(PhysicalAddress >> 32);
    return Bit;
}

VOID
AhcipFreeCommand (
    PAHCI_PORT Port,
    LONG Index
    )

/*++

Routine Description:

    This routine frees a previously allocated command header and command
    table entry.

Arguments:

    Port - Supplies a pointer to the port.

    Index - Supplies the command header index returned during allocate.

Return Value:

    None.

--*/

{

    ASSERT((Index >= 0) &&
           ((Port->AllocatedCommands & (1 << Index)) != 0) &&
           ((Port->PendingCommands & (1 << Index)) == 0));

    RtlAtomicAnd32(&(Port->AllocatedCommands), ~(1 << Index));
    return;
}

VOID
AhcipSubmitCommand (
    PAHCI_PORT Port,
    ULONG Mask
    )

/*++

Routine Description:

    This routine submits a command for execution. This routine must be executed
    at dispatch level with the DPC lock held for the port.

Arguments:

    Port - Supplies a pointer to the port.

    Mask - Supplies the mask to submit.

Return Value:

    None.

--*/

{

    ASSERT(KeIsSpinLockHeld(&(Port->DpcLock)) != FALSE);

    RtlMemoryBarrier();

    //
    // There is no safe order to do these in, which is why holding the lock
    // is necessary.
    //

    AHCI_WRITE(Port, AhciPortCommandIssue, Mask);
    Port->PendingCommands |= Mask;
    return;
}

