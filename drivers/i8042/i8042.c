/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    i8042.c

Abstract:

    This module implements the Intel 8042 keyboard/mouse controller driver.

Author:

    Evan Green 20-Dec-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "i8042.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read from and write to the control, status, and data registers.
//

#define WRITE_CONTROL_REGISTER(_Device, _Value) \
    HlIoPortOutByte(_Device->ControlPort, _Value)

#define READ_STATUS_REGISTER(_Device) HlIoPortInByte(_Device->ControlPort)
#define WRITE_DATA_REGISTER(_Device, _Value) \
    HlIoPortOutByte(_Device->DataPort, _Value)

#define READ_DATA_REGISTER(_Device) HlIoPortInByte(_Device->DataPort)

//
// This macro spins waiting for the last keyboard command to finish.
//

#define WAIT_FOR_INPUT_BUFFER(_Device)              \
    while ((READ_STATUS_REGISTER(_Device) &         \
            I8042_STATUS_INPUT_BUFFER_FULL) != 0) { \
                                                    \
        NOTHING;                                    \
    }

//
// This macro determines if data is available to be received from the device.
//

#define IS_DATA_AVAILABLE(_Device)           \
    ((READ_STATUS_REGISTER(_Device) & I8042_STATUS_OUTPUT_BUFFER_FULL) != 0)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the size of the device keyboard buffer size.
//

#define I8042_BUFFER_SIZE 256

//
// Define the bits in the 8042 status register.
//

#define I8042_STATUS_OUTPUT_BUFFER_FULL 0x01
#define I8042_STATUS_INPUT_BUFFER_FULL  0x02
#define I8042_STATUS_SELF_TEST_COMPLETE 0x04
#define I8042_STATUS_LAST_WRITE_COMMAND 0x08
#define I8042_STATUS_KEYBOARD_UNLOCK    0x10
#define I8042_STATUS_DATA_FROM_MOUSE    0x20
#define I8042_STATUS_TIMEOUT            0x40
#define I8042_STATUS_PARITY_ERROR       0x80

//
// Define bits in the 8042 command byte register.
//

#define I8042_COMMAND_BYTE_KEYBOARD_INTERRUPT_ENABLED 0x01
#define I8042_COMMAND_BYTE_MOUSE_INTERRUPT_ENABLED    0x02
#define I8042_COMMAND_BYTE_SYSTEM_FLAG                0x04
#define I8042_COMMAND_BYTE_PCAT_INHIBIT               0x08
#define I8042_COMMAND_BYTE_KEYBOARD_DISABLED          0x10
#define I8042_COMMAND_BYTE_MOUSE_DISABLED             0x20
#define I8042_COMMAND_BYTE_TRANSLATION_ENABLED        0x40

//
// Define the known device identifiers that this driver responds to.
//

#define KEYBOARD_HARDWARE_IDENTIFIER "PNP0303"
#define MOUSE_HARDWARE_IDENTIFIER "PNP0F13"

//
// Define the allocation tag used by this driver.
//

#define I8042_ALLOCATION_TAG 0x32343869 // '248i'

//
// Define the amount of time to allow the keyboard to reset, in microseconds.
//

#define I8042_RESET_DELAY 10000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores context about a device driven by the i8042 driver.

Members:

    IsMouse - Stores a boolean indicating whether the device is a mouse (TRUE)
        or a keyboard (FALSE).

    ControlPort - Stores the I/O port number of the 8042 control port.

    DataPort - Stores the I/O port number of the 8042 data port.

    InterruptVector - Stores the interrupt vector that this interrupt comes in
        on.

    InterruptLine - Stores the interrupt line that the interrupt comes in on.

    InterruptResourcesFound - Stores a boolean indicating whether or not the
        interrupt vector and interrupt line fields are valid.

    InterruptHandle - Stores the handle for the connected interrupt.

    UserInputDeviceHandle - Stores the handle returned by the User Input
        library.

    InterruptLock - Stores a spinlock synchronizing access to the device with
        the interrupt service routine.

    ReadLock - Stores a pointer to a queued lock that serializes read access
        to the data buffer.

    ReadIndex - Stores the index of the next byte to read out of the data
        buffer.

    WriteIndex - Stores the index of the next byte to write to the data buffer.

    DataBuffer - Stores the buffer of keys coming out of the controller.

--*/

typedef struct _I8042_DEVICE {
    BOOL IsMouse;
    USHORT ControlPort;
    USHORT DataPort;
    ULONGLONG InterruptVector;
    ULONGLONG InterruptLine;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    HANDLE UserInputDeviceHandle;
    KSPIN_LOCK InterruptLock;
    PQUEUED_LOCK ReadLock;
    volatile ULONG ReadIndex;
    volatile ULONG WriteIndex;
    volatile UCHAR DataBuffer[I8042_BUFFER_SIZE];
} I8042_DEVICE, *PI8042_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
I8042AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
I8042DispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
I8042DispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
I8042DispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
I8042DispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
I8042DispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
I8042InterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
I8042InterruptServiceWorker (
    PVOID Parameter
    );

KSTATUS
I8042pProcessResourceRequirements (
    PIRP Irp,
    PI8042_DEVICE Device
    );

KSTATUS
I8042pStartDevice (
    PIRP Irp,
    PI8042_DEVICE Device
    );

KSTATUS
I8042pEnableDevice (
    PVOID OsDevice,
    PI8042_DEVICE Device
    );

KSTATUS
I8042pSetLedState (
    PVOID Device,
    PVOID DeviceContext,
    ULONG LedState
    );

UCHAR
I8042pReadCommandByte (
    PI8042_DEVICE Device
    );

VOID
I8042pWriteCommandByte (
    PI8042_DEVICE Device,
    UCHAR Value
    );

KSTATUS
I8042pSendKeyboardCommand (
    PI8042_DEVICE Device,
    UCHAR Command,
    UCHAR Parameter
    );

KSTATUS
I8042pSendCommand (
    PI8042_DEVICE Device,
    UCHAR Command
    );

KSTATUS
I8042pReceiveResponse (
    PI8042_DEVICE Device,
    PUCHAR Data
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER I8042Driver = NULL;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the i8042 driver. It registers its other
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

    I8042Driver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = I8042AddDevice;
    FunctionTable.DispatchStateChange = I8042DispatchStateChange;
    FunctionTable.DispatchOpen = I8042DispatchOpen;
    FunctionTable.DispatchClose = I8042DispatchClose;
    FunctionTable.DispatchIo = I8042DispatchIo;
    FunctionTable.DispatchSystemControl = I8042DispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
I8042AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the i8042 driver
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

    BOOL DeviceIdsAreEqual;
    BOOL DeviceIsMouse;
    BOOL MatchesCompatibleId;
    BOOL MatchFound;
    PI8042_DEVICE NewDevice;
    KSTATUS Status;

    DeviceIsMouse = FALSE;
    MatchFound = FALSE;
    DeviceIdsAreEqual = IoAreDeviceIdsEqual(DeviceId,
                                            KEYBOARD_HARDWARE_IDENTIFIER);

    MatchesCompatibleId =
                   IoIsDeviceIdInCompatibleIdList(KEYBOARD_HARDWARE_IDENTIFIER,
                                                  DeviceToken);

    if ((DeviceIdsAreEqual != FALSE) || (MatchesCompatibleId != FALSE)) {
        MatchFound = TRUE;

    } else {

        //
        // Attempt to match against the mouse ID.
        //

        DeviceIdsAreEqual = IoAreDeviceIdsEqual(DeviceId,
                                                MOUSE_HARDWARE_IDENTIFIER);

        MatchesCompatibleId =
                      IoIsDeviceIdInCompatibleIdList(MOUSE_HARDWARE_IDENTIFIER,
                                                     DeviceToken);

        if ((DeviceIdsAreEqual != FALSE) || (MatchesCompatibleId != FALSE)) {
            MatchFound = TRUE;
            DeviceIsMouse = TRUE;
        }
    }

    //
    // If there is no match, return now.
    //

    if (MatchFound == FALSE) {
        return STATUS_SUCCESS;
    }

    //
    // there is a match, create the device context and attach to the device.
    //

    NewDevice = MmAllocateNonPagedPool(sizeof(I8042_DEVICE),
                                       I8042_ALLOCATION_TAG);

    if (NewDevice == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewDevice, sizeof(I8042_DEVICE));
    KeInitializeSpinLock(&(NewDevice->InterruptLock));
    NewDevice->InterruptHandle = INVALID_HANDLE;
    NewDevice->UserInputDeviceHandle = INVALID_HANDLE;
    NewDevice->IsMouse = DeviceIsMouse;
    NewDevice->ReadLock = KeCreateQueuedLock();
    if (NewDevice->ReadLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    Status = IoAttachDriverToDevice(Driver, DeviceToken, NewDevice);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (NewDevice != NULL) {
            if (NewDevice->ReadLock != NULL) {
                KeDestroyQueuedLock(NewDevice->ReadLock);
            }

            MmFreeNonPagedPool(NewDevice);
        }
    }

    return Status;
}

VOID
I8042DispatchStateChange (
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

    PI8042_DEVICE Device;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Device = (PI8042_DEVICE)DeviceContext;
    switch (Irp->MinorCode) {
    case IrpMinorQueryResources:

        //
        // On the way up, filter the resource requirements to add interrupt
        // vectors to any lines.
        //

        if (Irp->Direction == IrpUp) {
            Status = I8042pProcessResourceRequirements(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(I8042Driver, Irp, Status);
            }
        }

        break;

    case IrpMinorStartDevice:

        //
        // Attempt to fire the thing up if the bus has already started it.
        //

        if (Irp->Direction == IrpUp) {
            Status = I8042pStartDevice(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(I8042Driver, Irp, Status);
            }
        }

        break;

    //
    // For all other IRPs, do nothing.
    //

    default:
        break;
    }

    return;
}

VOID
I8042DispatchOpen (
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

    return;
}

VOID
I8042DispatchClose (
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

    return;
}

VOID
I8042DispatchIo (
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

    return;
}

VOID
I8042DispatchSystemControl (
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

    ASSERT(Irp->MajorCode == IrpMajorSystemControl);

    //
    // Do no processing on any IRPs. Let them flow.
    //

    return;
}

INTERRUPT_STATUS
I8042InterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the 8042 keyboard controller interrupt service
    routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the 8042 device
        context.

Return Value:

    Interrupt status.

--*/

{

    UCHAR Byte;
    PI8042_DEVICE Device;
    INTERRUPT_STATUS InterruptStatus;
    UCHAR Status;
    ULONG WriteIndex;

    Device = (PI8042_DEVICE)Context;
    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Check to see if there is data waiting.
    //

    if ((READ_STATUS_REGISTER(Device) & I8042_STATUS_OUTPUT_BUFFER_FULL) != 0) {

        //
        // There was data here, so most likely it was this device interrupting.
        //

        InterruptStatus = InterruptStatusClaimed;

        //
        // Read the bytes out of the controller.
        //

        KeAcquireSpinLock(&(Device->InterruptLock));
        WriteIndex = Device->WriteIndex;
        while (TRUE) {
            Status = READ_STATUS_REGISTER(Device);
            if ((Status & I8042_STATUS_OUTPUT_BUFFER_FULL) == 0) {
                break;
            }

            Byte = READ_DATA_REGISTER(Device);

            //
            // Toss out all mouse data. Mice are not yet supported.
            //

            if ((Status & I8042_STATUS_DATA_FROM_MOUSE) != 0) {

                ASSERT(Device->IsMouse == FALSE);

                continue;
            }

            if (((WriteIndex + 1) % I8042_BUFFER_SIZE) != Device->ReadIndex) {
                Device->DataBuffer[WriteIndex] = Byte;

                //
                // Advance the write index.
                //

                if ((WriteIndex + 1) == I8042_BUFFER_SIZE) {
                    WriteIndex = 0;

                } else {
                    WriteIndex += 1;
                }

            } else {
                RtlDebugPrint("I8042: Buffer overflow, losing byte %02X\n",
                              Byte);
            }
        }

        //
        // Save the new write index now that everything's out.
        //

        Device->WriteIndex = WriteIndex;
        KeReleaseSpinLock(&(Device->InterruptLock));
    }

    return InterruptStatus;
}

INTERRUPT_STATUS
I8042InterruptServiceWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes interrupts for the e100 controller at low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    Interrupt status.

--*/

{

    UCHAR Byte;
    UCHAR Code1;
    UCHAR Code2;
    UCHAR Code3;
    PI8042_DEVICE Device;
    USER_INPUT_EVENT Event;
    BOOL KeyUp;
    ULONG ReadIndex;

    Code1 = 0;
    Code2 = 0;
    Code3 = 0;
    Device = (PI8042_DEVICE)Parameter;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    RtlZeroMemory(&Event, sizeof(USER_INPUT_EVENT));

    //
    // Pull as much data out of the buffer as there is.
    //

    KeAcquireQueuedLock(Device->ReadLock);
    ReadIndex = Device->ReadIndex;
    while (ReadIndex != Device->WriteIndex) {
        Byte = Device->DataBuffer[ReadIndex];
        ReadIndex += 1;
        if (ReadIndex == I8042_BUFFER_SIZE) {
            ReadIndex = 0;
        }

        //
        // If the first byte read was the extended 2 code, then another 2 bytes
        // should be coming in. Get those bytes.
        //

        if (Code1 == SCAN_CODE_1_EXTENDED_2_CODE) {
            if (Code2 == 0) {
                Code2 = Byte;
                continue;
            }

            Code3 = Byte;

        //
        // If the first byte read was the extended (1) code, then another byte
        // should be coming in. Get that byte.
        //

        } else if (Code1 == SCAN_CODE_1_EXTENDED_CODE) {
            Code2 = Byte;

        } else {
            Code1 = Byte;
            if ((Code1 == SCAN_CODE_1_EXTENDED_CODE) ||
                (Code1 == SCAN_CODE_1_EXTENDED_2_CODE)) {

                continue;
            }
        }

        //
        // Get the specifics of the event.
        //

        Event.U.Key = I8042ConvertScanCodeToKey(Code1, Code2, Code3, &KeyUp);
        if (Event.U.Key != KeyboardKeyInvalid) {
            if (KeyUp != FALSE) {
                Event.EventType = UserInputEventKeyUp;

            } else {
                Event.EventType = UserInputEventKeyDown;
            }

            //
            // Log the event.
            //

            InReportInputEvent(Device->UserInputDeviceHandle, &Event);
        }

        //
        // A full key combination was read, move the read index forward.
        //

        Device->ReadIndex = ReadIndex;
        Code1 = 0;
        Code2 = 0;
    }

    KeReleaseQueuedLock(Device->ReadLock);
    return InterruptStatusClaimed;
}

KSTATUS
I8042pProcessResourceRequirements (
    PIRP Irp,
    PI8042_DEVICE Device
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus. It adds an interrupt vector requirement for any interrupt line
    requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this 8042 controller device.

Return Value:

    Status code.

--*/

{

    PRESOURCE_CONFIGURATION_LIST Requirements;
    KSTATUS Status;
    RESOURCE_REQUIREMENT VectorRequirement;

    ASSERT((Irp->MajorCode == IrpMajorStateChange) &&
           (Irp->MinorCode == IrpMinorQueryResources));

    //
    // Initialize a nice interrupt vector requirement in preparation.
    //

    RtlZeroMemory(&VectorRequirement, sizeof(RESOURCE_REQUIREMENT));
    VectorRequirement.Type = ResourceTypeInterruptVector;
    VectorRequirement.Minimum = 0;
    VectorRequirement.Maximum = -1;
    VectorRequirement.Length = 1;

    //
    // Loop through all configuration lists, creating a vector for each line.
    //

    Requirements = Irp->U.QueryResources.ResourceRequirements;
    Status = IoCreateAndAddInterruptVectorsForLines(Requirements,
                                                    &VectorRequirement);

    if (!KSUCCESS(Status)) {
        goto ProcessResourceRequirementsEnd;
    }

ProcessResourceRequirementsEnd:
    return Status;
}

KSTATUS
I8042pStartDevice (
    PIRP Irp,
    PI8042_DEVICE Device
    )

/*++

Routine Description:

    This routine starts up the 8042 controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this 8042 controller device.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    BOOL ControlFound;
    BOOL DataFound;
    PRESOURCE_ALLOCATION LineAllocation;
    KSTATUS Status;

    ControlFound = FALSE;
    DataFound = FALSE;

    //
    // If there are no resources, then return success but don't start anything.
    //

    AllocationList = Irp->U.StartDevice.ProcessorLocalResources;
    if (AllocationList == NULL) {
        Status = STATUS_SUCCESS;
        goto StartDeviceEnd;
    }

    //
    // Loop through the allocated resources to get the control and data ports,
    // and the interrupt.
    //

    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {
        if (Allocation->Type == ResourceTypeIoPort) {

            ASSERT(Allocation->Length == 1);
            ASSERT(Allocation->Allocation <= 0xFFFF);

            //
            // Assume the first resource is the data port.
            //

            if (DataFound == FALSE) {
                Device->DataPort = (USHORT)Allocation->Allocation;
                DataFound = TRUE;

            //
            // The second resource must be the control port.
            //

            } else if (ControlFound == FALSE) {
                Device->ControlPort = (USHORT)Allocation->Allocation;
                ControlFound = TRUE;
            }

        //
        // If the resource is an interrupt vector, then it should have an
        // owning interrupt line allocation.
        //

        } else if (Allocation->Type == ResourceTypeInterruptVector) {

            //
            // Currently only one interrupt resource is expected.
            //

            ASSERT(Device->InterruptResourcesFound == FALSE);
            ASSERT(Allocation->OwningAllocation != NULL);

            //
            // Save the line and vector number.
            //

            LineAllocation = Allocation->OwningAllocation;
            Device->InterruptLine = LineAllocation->Allocation;
            Device->InterruptVector = Allocation->Allocation;
            Device->InterruptResourcesFound = TRUE;
        }

        //
        // Get the next allocation in the list.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    //
    // Fail if both ports were not found.
    //

    if ((DataFound == FALSE) || (ControlFound == FALSE)) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto StartDeviceEnd;
    }

    //
    // Fire up the device.
    //

    Status = I8042pEnableDevice(Irp->Device, Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Attempt to connect the interrupt.
    //

    ASSERT(Device->InterruptHandle == INVALID_HANDLE);

    RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
    Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
    Connect.Device = Irp->Device;
    Connect.LineNumber = Device->InterruptLine;
    Connect.Vector = Device->InterruptVector;
    Connect.InterruptServiceRoutine = I8042InterruptService;
    Connect.LowLevelServiceRoutine = I8042InterruptServiceWorker;
    Connect.Context = Device;
    Connect.Interrupt = &(Device->InterruptHandle);
    Status = IoConnectInterrupt(&Connect);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Clear out any queued up bytes, as they might prevent future interrupts
    // from firing.
    //

    while (TRUE) {
        if ((READ_STATUS_REGISTER(Device) &
             I8042_STATUS_OUTPUT_BUFFER_FULL) == 0) {

            break;
        }

        READ_DATA_REGISTER(Device);
    }

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        Device->InterruptResourcesFound = FALSE;
        if (Device->InterruptHandle != INVALID_HANDLE) {
            IoDisconnectInterrupt(Device->InterruptHandle);
            Device->InterruptHandle = INVALID_HANDLE;
        }

        if (Device->UserInputDeviceHandle != INVALID_HANDLE) {
            InDestroyInputDevice(Device->UserInputDeviceHandle);
            Device->UserInputDeviceHandle = INVALID_HANDLE;
        }
    }

    return Status;
}

KSTATUS
I8042pEnableDevice (
    PVOID OsDevice,
    PI8042_DEVICE Device
    )

/*++

Routine Description:

    This routine enables the given 8042 device.

Arguments:

    OsDevice - Supplies a pointer to the OS device token.

    Device - Supplies a pointer to this 8042 controller device.

Return Value:

    Status code.

--*/

{

    UCHAR CommandByte;
    USER_INPUT_DEVICE_DESCRIPTION Description;
    UCHAR Response;
    KSTATUS Status;
    BOOL TwoPorts;

    if (Device->IsMouse != FALSE) {

        //
        // Mice are not currently supported.
        //

        return STATUS_NOT_IMPLEMENTED;

    } else {

        //
        // Disable both ports.
        //

        I8042pSendCommand(Device, I8042_COMMAND_DISABLE_KEYBOARD);
        I8042pSendCommand(Device, I8042_COMMAND_DISABLE_MOUSE_PORT);

        //
        // Flush any leftover data out.
        //

        while ((READ_STATUS_REGISTER(Device) &
                I8042_STATUS_OUTPUT_BUFFER_FULL) != 0) {

            READ_DATA_REGISTER(Device);
        }

        //
        // Enable the keyboard in the command byte. Disable the interrupt
        // for now during setup.
        //

        CommandByte = I8042pReadCommandByte(Device);
        CommandByte &= ~(I8042_COMMAND_BYTE_KEYBOARD_DISABLED |
                         I8042_COMMAND_BYTE_PCAT_INHIBIT |
                         I8042_COMMAND_BYTE_KEYBOARD_INTERRUPT_ENABLED |
                         I8042_COMMAND_BYTE_MOUSE_INTERRUPT_ENABLED);

        I8042pWriteCommandByte(Device, CommandByte);

        //
        // Send a self test to the controller itself, and verify that it
        // passes.
        //

        I8042pSendCommand(Device, I8042_COMMAND_SELF_TEST);
        HlBusySpin(I8042_RESET_DELAY);
        Status = I8042pReceiveResponse(Device, &Response);
        if (!KSUCCESS(Status)) {
            goto EnableDeviceEnd;
        }

        if (Response != I8042_SELF_TEST_SUCCESS) {
            RtlDebugPrint("i8042: Received %x to keyboard reset instead of "
                          "expected %x.\n",
                          Response,
                          I8042_SELF_TEST_SUCCESS);

            Status = STATUS_DEVICE_IO_ERROR;
            goto EnableDeviceEnd;
        }

        //
        // Determine if there are two ports. Enable the mouse port, and the
        // "data from mouse" bit in the status should clear.
        //

        TwoPorts = FALSE;
        I8042pSendCommand(Device, I8042_COMMAND_ENABLE_MOUSE_PORT);
        if ((READ_STATUS_REGISTER(Device) &
             I8042_STATUS_DATA_FROM_MOUSE) == 0) {

            TwoPorts = TRUE;
        }

        I8042pSendCommand(Device, I8042_COMMAND_DISABLE_MOUSE_PORT);

        //
        // Test the ports.
        //

        I8042pSendCommand(Device, I8042_COMMAND_INTERFACE_TEST);
        Status = I8042pReceiveResponse(Device, &Response);
        if (!KSUCCESS(Status)) {
            goto EnableDeviceEnd;
        }

        if (Response != I8042_PORT_TEST_SUCCESS) {
            Status = STATUS_DEVICE_IO_ERROR;
            goto EnableDeviceEnd;
        }

        if (TwoPorts != FALSE) {
            I8042pSendCommand(Device, I8042_COMMAND_TEST_MOUSE_PORT);
            Status = I8042pReceiveResponse(Device, &Response);
            if (!KSUCCESS(Status)) {
                goto EnableDeviceEnd;
            }

            if (Response != I8042_PORT_TEST_SUCCESS) {
                Status = STATUS_DEVICE_IO_ERROR;
                goto EnableDeviceEnd;
            }
        }

        //
        // Enable the ports.
        //

        I8042pSendCommand(Device, I8042_COMMAND_ENABLE_KEYBOARD);
        if (TwoPorts != FALSE) {
            I8042pSendCommand(Device, I8042_COMMAND_ENABLE_MOUSE_PORT);
        }

        //
        // Reset the keyboard.
        //

        Status = I8042pSendKeyboardCommand(Device,
                                           KEYBOARD_COMMAND_RESET,
                                           KEYBOARD_COMMAND_NO_PARAMETER);

        if (!KSUCCESS(Status)) {
            goto EnableDeviceEnd;
        }

        //
        // Read the BAT (Basic Assurance Test) code that the keyboard sends
        // when it finishes resetting.
        //

        Status = I8042pReceiveResponse(Device, &Response);
        if (!KSUCCESS(Status)) {
            goto EnableDeviceEnd;
        }

        if (Response != KEYBOARD_BAT_PASS) {
            goto EnableDeviceEnd;
        }

        //
        // Set the typematic rate/delay on the keyboard. Start by sending the
        // command.
        //

        Status = I8042pSendKeyboardCommand(Device,
                                           KEYBOARD_COMMAND_SET_TYPEMATIC,
                                           DEFAULT_TYPEMATIC_VALUE);

        if (!KSUCCESS(Status)) {
            goto EnableDeviceEnd;
        }

        //
        // Enable the keyboard.
        //

        Status = I8042pSendKeyboardCommand(Device,
                                           KEYBOARD_COMMAND_ENABLE,
                                           KEYBOARD_COMMAND_NO_PARAMETER);

        if (!KSUCCESS(Status)) {
            goto EnableDeviceEnd;
        }

        //
        // Create the user input handle if not already done.
        //

        if (Device->UserInputDeviceHandle == INVALID_HANDLE) {
            Description.Device = OsDevice;
            Description.DeviceContext = Device;
            Description.Type = UserInputDeviceKeyboard;
            Description.InterfaceVersion =
                                  USER_INPUT_KEYBOARD_DEVICE_INTERFACE_VERSION;

            Description.U.KeyboardInterface.SetLedState = I8042pSetLedState;
            Device->UserInputDeviceHandle = InRegisterInputDevice(&Description);
            if (Device->UserInputDeviceHandle == INVALID_HANDLE) {
                Status = STATUS_UNSUCCESSFUL;
                goto EnableDeviceEnd;
            }
        }

        //
        // Enable the keyboard interrupt.
        //

        CommandByte |= I8042_COMMAND_BYTE_KEYBOARD_INTERRUPT_ENABLED;
        I8042pWriteCommandByte(Device, CommandByte);
    }

EnableDeviceEnd:
    return Status;
}

KSTATUS
I8042pSetLedState (
    PVOID Device,
    PVOID DeviceContext,
    ULONG LedState
    )

/*++

Routine Description:

    This routine sets a keyboard's LED state (e.g. Number lock, Caps lock and
    scroll lock). The state is absolute; the desired state for each LED must be
    supplied.

Arguments:

    Device - Supplies a pointer to the OS device representing the user input
        device.

    DeviceContext - Supplies the opaque device context supplied in the device
        description upon registration with the user input library.

    LedState - Supplies a bitmask of flags describing the desired LED state.
        See USER_INPUT_KEYBOARD_LED_* for definition.

Return Value:

    Status code.

--*/

{

    PI8042_DEVICE I8042Device;
    UCHAR KeyboardLedState;
    KSTATUS Status;

    I8042Device = (PI8042_DEVICE)DeviceContext;

    //
    // Convert the LED state to the proper format.
    //

    KeyboardLedState = 0;
    if ((LedState & USER_INPUT_KEYBOARD_LED_SCROLL_LOCK) != 0) {
        KeyboardLedState |= KEYBOARD_LED_SCROLL_LOCK;
    }

    if ((LedState & USER_INPUT_KEYBOARD_LED_NUM_LOCK) != 0) {
        KeyboardLedState |= KEYBOARD_LED_NUM_LOCK;
    }

    if ((LedState & USER_INPUT_KEYBOARD_LED_CAPS_LOCK) != 0) {
        KeyboardLedState |= KEYBOARD_LED_CAPS_LOCK;
    }

    Status = I8042pSendKeyboardCommand(I8042Device,
                                       KEYBOARD_COMMAND_SET_LEDS,
                                       KeyboardLedState);

    return Status;
}

UCHAR
I8042pReadCommandByte (
    PI8042_DEVICE Device
    )

/*++

Routine Description:

    This routine reads the contents of the command byte in the 8042 keyboard
    controller.

Arguments:

    Device - Supplies a pointer to this 8042 controller device.

Return Value:

    Status code.

--*/

{

    I8042pSendCommand(Device, I8042_COMMAND_READ_COMMAND_BYTE);
    return READ_DATA_REGISTER(Device);
}

VOID
I8042pWriteCommandByte (
    PI8042_DEVICE Device,
    UCHAR Value
    )

/*++

Routine Description:

    This routine reads the contents of the command byte in the 8042 keyboard
    controller.

Arguments:

    Device - Supplies a pointer to this 8042 controller device.

    Value - Supplies the value to write to the command register.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = I8042pSendCommand(Device, I8042_COMMAND_WRITE_COMMAND_BYTE);
    if (KSUCCESS(Status)) {
        WRITE_DATA_REGISTER(Device, Value);
    }

    return;
}

KSTATUS
I8042pSendKeyboardCommand (
    PI8042_DEVICE Device,
    UCHAR Command,
    UCHAR Parameter
    )

/*++

Routine Description:

    This routine sends a command byte to the keyboard itself (not the
    keyboard controller) and checks the return status byte.

Arguments:

    Device - Supplies a pointer to this 8042 controller device.

    Command - Supplies the command to write to the keyboard.

    Parameter - Supplies an additional byte to send. Set this to 0xFF to
        skip sending this byte.

Return Value:

    Status code indicating whether the keyboard succeeded or failed the
    command.

--*/

{

    UCHAR KeyboardResult;
    KSTATUS Status;

    WAIT_FOR_INPUT_BUFFER(Device);
    WRITE_DATA_REGISTER(Device, Command);
    if (Parameter != KEYBOARD_COMMAND_NO_PARAMETER) {
        WAIT_FOR_INPUT_BUFFER(Device);
        WRITE_DATA_REGISTER(Device, Parameter);
    }

    Status = I8042pReceiveResponse(Device, &KeyboardResult);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (KeyboardResult == KEYBOARD_STATUS_ACKNOWLEDGE) {
        return STATUS_SUCCESS;
    }

    if (KeyboardResult == KEYBOARD_STATUS_RESEND) {
        return STATUS_NOT_READY;
    }

    if (KeyboardResult == KEYBOARD_STATUS_OVERRUN) {
        return STATUS_BUFFER_OVERRUN;
    }

    return STATUS_DEVICE_IO_ERROR;
}

KSTATUS
I8042pSendCommand (
    PI8042_DEVICE Device,
    UCHAR Command
    )

/*++

Routine Description:

    This routine sends a command to the PS/2 controller (not the device
    connected to it).

Arguments:

    Device - Supplies a pointer to this 8042 controller device.

    Command - Supplies the command to write to the controller.

Return Value:

    Status code indicating whether the keyboard succeeded or failed the
    command.

--*/

{

    WAIT_FOR_INPUT_BUFFER(Device);
    WRITE_CONTROL_REGISTER(Device, Command);
    WAIT_FOR_INPUT_BUFFER(Device);
    return STATUS_SUCCESS;
}

KSTATUS
I8042pReceiveResponse (
    PI8042_DEVICE Device,
    PUCHAR Data
    )

/*++

Routine Description:

    This routine receives a byte from the data port, with a timeout.

Arguments:

    Device - Supplies a pointer to this 8042 controller device.

    Data - Supplies a pointer where the data will be returned on success.

Return Value:

    Status code.

--*/

{

    KSTATUS StatusCode;
    UCHAR StatusRegister;
    ULONGLONG Timeout;

    Timeout = KeGetRecentTimeCounter() +
              ((HlQueryTimeCounterFrequency() * I8042_COMMAND_TIMEOUT) /
               MILLISECONDS_PER_SECOND);

    StatusCode = STATUS_TIMEOUT;
    do {
        StatusRegister = READ_STATUS_REGISTER(Device);
        if ((StatusRegister & I8042_STATUS_TIMEOUT) != 0) {
            StatusCode = STATUS_TIMEOUT;
            break;

        } else if ((StatusRegister & I8042_STATUS_PARITY_ERROR) != 0) {
            StatusCode = STATUS_PARITY_ERROR;
            break;

        } else if ((StatusRegister & I8042_STATUS_OUTPUT_BUFFER_FULL) != 0) {
            *Data = READ_DATA_REGISTER(Device);
            StatusCode = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    return StatusCode;
}

