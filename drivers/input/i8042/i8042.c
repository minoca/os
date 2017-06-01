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
// This macro spins waiting for data to show up on the data register.
//

#define WAIT_FOR_OUTPUT_BUFFER(_Device)              \
    while ((READ_STATUS_REGISTER(_Device) &          \
            I8042_STATUS_OUTPUT_BUFFER_FULL) == 0) { \
                                                     \
        NOTHING;                                     \
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

#define I8042_BUFFER_SIZE 128
#define I8042_BUFFER_MASK 0x7F

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

    ReadIndex - Stores the index of the next byte to read out of the data
        buffer.

    WriteIndex - Stores the index of the next byte to write to the data buffer.

    DataBuffer - Stores the buffer of keys coming out of the controller.

--*/

typedef struct _I8042_BUFFER {
    volatile ULONG ReadIndex;
    volatile ULONG WriteIndex;
    volatile UCHAR DataBuffer[I8042_BUFFER_SIZE];
} I8042_BUFFER, *PI8042_BUFFER;

/*++

Structure Description:

    This structure stores context about a device driven by the i8042 driver.

Members:

    KeyboardDevice - Stores a pointer to the keyboard OS device.

    MouseDevice - Stores a pointer to the mouse OS device.

    IsInitialized - Stores a boolean indicating whether or not the device has
        been initialized.

    IsMouse - Stores a boolean indicating whether the device is a mouse (TRUE)
        or a keyboard (FALSE).

    ControlPort - Stores the I/O port number of the 8042 control port.

    DataPort - Stores the I/O port number of the 8042 data port.

    MouseReportSize - Stores the number of bytes in the mouse report. Valid
        values are 3 and 4.

    KeyboardInterruptVector - Stores the interrupt vector that the keyboard
        interrupt comes in on.

    MouseInterruptVector - Stores the interrupt vector that the mouse
        interrupt comes in on.

    KeyboardInterruptLine - Stores the interrupt line that the keyboard
        interrupt comes in on.

    MouseInterruptLine - Stores the interrupt line that the mouse interrupt
        comes in on.

    KeyboardInterruptFound - Stores a boolean indicating whether or not the
        keyboard interrupt vector and interrupt line fields are valid.

    MouseInterruptFound - Stores a boolean indicating whether or not the
        mouse interrupt vector and interrupt line fields are valid.

    InterruptHandles - Stores the connected interrupt handles, one for the
        keyboard and one for the mouse.

    InterruptRunLevel - Stores the maximum runlevel between the two interrupt
        handles, used to synchronize them.

    KeyboardInputHandle - Stores the handle returned by the User Input library
        for the keyboard.

    MouseInputHandle - Stores the handle returned by the User Input library for
        the mouse.

    InterruptLock - Stores a spinlock synchronizing access to the device with
        the interrupt service routine.

    ReadLock - Stores a pointer to a queued lock that serializes read access
        to the data buffer.

    KeyboardData - Stores the keyboard data buffer.

    MouseData - Stores the mouse data buffer.

    LastMouseEvent - Stores the timestamp of the last incoming mouse data.
        This can be used to resynchronize an out-of-sync stream.

--*/

typedef struct _I8042_DEVICE {
    PDEVICE KeyboardDevice;
    PDEVICE MouseDevice;
    USHORT ControlPort;
    USHORT DataPort;
    USHORT MouseReportSize;
    ULONGLONG KeyboardInterruptVector;
    ULONGLONG MouseInterruptVector;
    ULONGLONG KeyboardInterruptLine;
    ULONGLONG MouseInterruptLine;
    BOOL KeyboardInterruptFound;
    BOOL MouseInterruptFound;
    HANDLE InterruptHandles[2];
    RUNLEVEL InterruptRunLevel;
    HANDLE KeyboardInputHandle;
    HANDLE MouseInputHandle;
    KSPIN_LOCK InterruptLock;
    PQUEUED_LOCK ReadLock;
    I8042_BUFFER KeyboardData;
    I8042_BUFFER MouseData;
    ULONGLONG LastMouseEvent;
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
    PI8042_DEVICE Device
    );

KSTATUS
I8042pEnableMouse (
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
I8042pSendMouseCommand (
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

KSTATUS
I8042pReceiveMouseResponse (
    PI8042_DEVICE Device,
    PUCHAR Data
    );

VOID
I8042pProcessMouseReport (
    PI8042_DEVICE Device,
    UCHAR Report[4]
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER I8042Driver = NULL;

//
// Define a global so that the PS2 keyboard and mouse can share a device. This
// does put the restriction that there cannot be several distinct PS/2 ports
// floating around (only one pair).
//

I8042_DEVICE I8042Device;

PCSTR I8042KeyboardDeviceIds[] = {
    "PNP0303",
    NULL
};

PCSTR I8042MouseDeviceIds[] = {
    "PNP0F03",
    "PNP0F13",
    "VMW0003",
    NULL
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

    PCSTR *CurrentId;
    BOOL DeviceIdsAreEqual;
    BOOL DeviceIsMouse;
    BOOL MatchesCompatibleId;
    BOOL MatchFound;
    PI8042_DEVICE NewDevice;
    KSTATUS Status;

    DeviceIsMouse = FALSE;
    MatchFound = FALSE;
    CurrentId = &(I8042KeyboardDeviceIds[0]);
    while (*CurrentId != NULL) {
        DeviceIdsAreEqual = IoAreDeviceIdsEqual(DeviceId, *CurrentId);
        MatchesCompatibleId =
                       IoIsDeviceIdInCompatibleIdList(*CurrentId, DeviceToken);

        if ((DeviceIdsAreEqual != FALSE) || (MatchesCompatibleId != FALSE)) {
            MatchFound = TRUE;
            break;
        }

        CurrentId += 1;
    }

    if (MatchFound == FALSE) {
        CurrentId = &(I8042MouseDeviceIds[0]);
        while (*CurrentId != NULL) {
            DeviceIdsAreEqual = IoAreDeviceIdsEqual(DeviceId, *CurrentId);
            MatchesCompatibleId =
                       IoIsDeviceIdInCompatibleIdList(*CurrentId, DeviceToken);

            if ((DeviceIdsAreEqual != FALSE) ||
                (MatchesCompatibleId != FALSE)) {

                MatchFound = TRUE;
                DeviceIsMouse = TRUE;
                break;
            }

            CurrentId += 1;
        }
    }

    //
    // If there is no match, return now.
    //

    if (MatchFound == FALSE) {
        return STATUS_SUCCESS;
    }

    //
    // There is a match, initialize the device context.
    //

    NewDevice = &I8042Device;
    if ((NewDevice->KeyboardDevice == NULL) &&
        (NewDevice->MouseDevice == NULL)) {

        KeInitializeSpinLock(&(NewDevice->InterruptLock));
        NewDevice->InterruptHandles[0] = INVALID_HANDLE;
        NewDevice->InterruptHandles[1] = INVALID_HANDLE;
        NewDevice->KeyboardInputHandle = INVALID_HANDLE;
        NewDevice->MouseInputHandle = INVALID_HANDLE;
        NewDevice->InterruptRunLevel = RunLevelHigh;
        NewDevice->ReadLock = KeCreateQueuedLock();
        if (NewDevice->ReadLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AddDeviceEnd;
        }

        NewDevice->MouseReportSize = 3;
    }

    if (DeviceIsMouse != FALSE) {
        if (NewDevice->MouseDevice != NULL) {
            RtlDebugPrint("i8042: Second PS/2 mouse unsupported.\n");
            Status = STATUS_NOT_SUPPORTED;
            goto AddDeviceEnd;
        }

        NewDevice->MouseDevice = DeviceToken;

    } else {
        if (NewDevice->KeyboardDevice != NULL) {
            RtlDebugPrint("i8042: Second PS/2 keyboard unsupported.\n");
            Status = STATUS_NOT_SUPPORTED;
            goto AddDeviceEnd;
        }

        NewDevice->KeyboardDevice = DeviceToken;
    }

    Status = IoAttachDriverToDevice(Driver, DeviceToken, NewDevice);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

AddDeviceEnd:
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

    PI8042_BUFFER Buffer;
    UCHAR Byte;
    PI8042_DEVICE Device;
    INTERRUPT_STATUS InterruptStatus;
    RUNLEVEL OldRunLevel;
    UCHAR Status;
    ULONG WriteIndex;

    Device = (PI8042_DEVICE)Context;
    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Check to see if there is data waiting.
    //

    Status = READ_STATUS_REGISTER(Device);
    if ((Status & I8042_STATUS_OUTPUT_BUFFER_FULL) != 0) {

        //
        // There was data here, so most likely it was this device interrupting.
        //

        InterruptStatus = InterruptStatusClaimed;

        //
        // Raise to the runlevel that is the maximum between the keyboard and
        // the mouse interrupts.
        //

        OldRunLevel = KeRaiseRunLevel(Device->InterruptRunLevel);

        //
        // Read the bytes out of the controller.
        //

        KeAcquireSpinLock(&(Device->InterruptLock));
        while (TRUE) {
            Status = READ_STATUS_REGISTER(Device);
            if ((Status & I8042_STATUS_OUTPUT_BUFFER_FULL) == 0) {
                break;
            }

            Byte = READ_DATA_REGISTER(Device);
            Buffer = &(Device->KeyboardData);
            if ((Status & I8042_STATUS_DATA_FROM_MOUSE) != 0) {
                Buffer = &(Device->MouseData);
            }

            WriteIndex = Buffer->WriteIndex;
            if (((WriteIndex + 1) % I8042_BUFFER_SIZE) != Buffer->ReadIndex) {
                Buffer->DataBuffer[WriteIndex] = Byte;
                WriteIndex = (WriteIndex + 1) & I8042_BUFFER_MASK;

            } else {
                RtlDebugPrint("I8042: Buffer overflow, losing byte %02X\n",
                              Byte);
            }

            Buffer->WriteIndex = WriteIndex;
        }

        //
        // Save the new write index now that everything's out.
        //

        KeReleaseSpinLock(&(Device->InterruptLock));
        KeLowerRunLevel(OldRunLevel);
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

    PI8042_BUFFER Buffer;
    UCHAR Byte;
    PI8042_DEVICE Device;
    USER_INPUT_EVENT Event;
    BOOL KeyUp;
    ULONG ReadIndex;
    UCHAR Report[4];
    ULONG Size;
    ULONGLONG Timeout;

    Report[0] = 0;
    Report[1] = 0;
    Report[2] = 0;
    Report[3] = 0;
    Device = (PI8042_DEVICE)Parameter;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    RtlZeroMemory(&Event, sizeof(USER_INPUT_EVENT));

    //
    // Pull as much data out of the buffer as there is.
    //

    KeAcquireQueuedLock(Device->ReadLock);
    Buffer = &(Device->KeyboardData);
    ReadIndex = Buffer->ReadIndex;
    while (ReadIndex != Buffer->WriteIndex) {
        Byte = Buffer->DataBuffer[ReadIndex];
        ReadIndex = (ReadIndex + 1) & I8042_BUFFER_MASK;

        //
        // If the first byte read was the extended 2 code, then another 2 bytes
        // should be coming in. Get those bytes.
        //

        if (Report[0] == SCAN_CODE_1_EXTENDED_2_CODE) {
            if (Report[1] == 0) {
                Report[1] = Byte;
                continue;
            }

            Report[2] = Byte;

        //
        // If the first byte read was the extended (1) code, then another byte
        // should be coming in. Get that byte.
        //

        } else if (Report[0] == SCAN_CODE_1_EXTENDED_CODE) {
            Report[1] = Byte;

        } else {
            Report[0] = Byte;
            if ((Report[0] == SCAN_CODE_1_EXTENDED_CODE) ||
                (Report[0] == SCAN_CODE_1_EXTENDED_2_CODE)) {

                continue;
            }
        }

        //
        // Get the specifics of the event.
        //

        Event.U.Key = I8042ConvertScanCodeToKey(Report[0],
                                                Report[1],
                                                Report[2],
                                                &KeyUp);

        if (Event.U.Key != KeyboardKeyInvalid) {
            if (KeyUp != FALSE) {
                Event.EventType = UserInputEventKeyUp;

            } else {
                Event.EventType = UserInputEventKeyDown;
            }

            //
            // Log the event.
            //

            InReportInputEvent(Device->KeyboardInputHandle, &Event);
        }

        //
        // A full key combination was read, move the read index forward.
        //

        Buffer->ReadIndex = ReadIndex;
        Report[0] = 0;
        Report[1] = 0;
    }

    //
    // Process the mouse reports as well.
    //

    Buffer = &(Device->MouseData);
    ReadIndex = Buffer->ReadIndex;
    Size = 0;
    while (ReadIndex != Buffer->WriteIndex) {

        //
        // Grab a whole report, or as much of one as possible.
        //

        Size = 0;
        for (Size = 0;
             (Size < Device->MouseReportSize) &&
             (ReadIndex != Buffer->WriteIndex);
             Size += 1) {

            Report[Size] = Buffer->DataBuffer[ReadIndex];
            ReadIndex = (ReadIndex + 1) & I8042_BUFFER_MASK;
        }

        if (Size == Device->MouseReportSize) {
            Buffer->ReadIndex = ReadIndex;

        //
        // If the whole report did not come in, look to see when it was.
        //

        } else {

            //
            // If this is the first time a strange size has come in, timestamp
            // it.
            //

            if (Device->LastMouseEvent == 0) {
                Device->LastMouseEvent = HlQueryTimeCounter();
                break;
            }

            //
            // See if the time since the last data came in is too long,
            // indicating the mouse is out of sync.
            //

            Timeout = (HlQueryTimeCounterFrequency() * 1000ULL) /
                      MILLISECONDS_PER_SECOND;

            if (HlQueryTimeCounter() > Device->LastMouseEvent + Timeout) {

                //
                // Throw all the data away in an attempt to get back in sync.
                //

                RtlDebugPrint("PS/2 Mouse resync: %d: %x %x %x %x, WI %x\n",
                              Size,
                              Report[0],
                              Report[1],
                              Report[2],
                              Report[3],
                              Buffer->WriteIndex);

                Device->LastMouseEvent = 0;
                Buffer->ReadIndex = Buffer->WriteIndex;
            }

            break;
        }

        I8042pProcessMouseReport(Device, Report);
    }

    //
    // If it ended well, then reset the timeout.
    //

    if (Size == Device->MouseReportSize) {
        Device->LastMouseEvent = 0;
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
    RUNLEVEL OldRunLevel;
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

                ASSERT((Device->DataPort == 0) ||
                       (Device->DataPort == (USHORT)Allocation->Allocation));

                Device->DataPort = (USHORT)Allocation->Allocation;
                DataFound = TRUE;

            //
            // The second resource must be the control port.
            //

            } else if (ControlFound == FALSE) {

                ASSERT((Device->ControlPort == 0) ||
                       (Device->ControlPort == (USHORT)Allocation->Allocation));

                Device->ControlPort = (USHORT)Allocation->Allocation;
                ControlFound = TRUE;
            }

        //
        // If the resource is an interrupt vector, then it should have an
        // owning interrupt line allocation.
        //

        } else if (Allocation->Type == ResourceTypeInterruptVector) {

            ASSERT(Allocation->OwningAllocation != NULL);

            LineAllocation = Allocation->OwningAllocation;
            if (Irp->Device == Device->KeyboardDevice) {

                ASSERT(Device->KeyboardInterruptFound == FALSE);

                Device->KeyboardInterruptLine = LineAllocation->Allocation;
                Device->KeyboardInterruptVector = Allocation->Allocation;
                Device->KeyboardInterruptFound = TRUE;

            } else {

                ASSERT(Irp->Device == Device->MouseDevice);

                Device->MouseInterruptLine = LineAllocation->Allocation;
                Device->MouseInterruptVector = Allocation->Allocation;
                Device->MouseInterruptFound = TRUE;
            }
        }

        //
        // Get the next allocation in the list.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    //
    // If this is the keyboard, fire everything up.
    //

    if (Irp->Device == Device->KeyboardDevice) {

        //
        // Fail if both ports were not found.
        //

        if ((Device->ControlPort == 0) || (Device->DataPort == 0)) {
            Status = STATUS_INVALID_CONFIGURATION;
            goto StartDeviceEnd;
        }

        //
        // Fire up the device.
        //

        Status = I8042pEnableDevice(Device);
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        //
        // Attempt to connect the interrupt.
        //

        ASSERT(Device->InterruptHandles[0] == INVALID_HANDLE);

        RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
        Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
        Connect.Device = Irp->Device;
        Connect.LineNumber = Device->KeyboardInterruptLine;
        Connect.Vector = Device->KeyboardInterruptVector;
        Connect.InterruptServiceRoutine = I8042InterruptService;
        Connect.LowLevelServiceRoutine = I8042InterruptServiceWorker;
        Connect.Context = Device;
        Connect.Interrupt = &(Device->InterruptHandles[0]);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Device->InterruptRunLevel =
                           IoGetInterruptRunLevel(Device->InterruptHandles, 2);

        //
        // Clear out any queued up bytes, as they might prevent future
        // interrupts from firing.
        //

        while ((READ_STATUS_REGISTER(Device) &
                I8042_STATUS_OUTPUT_BUFFER_FULL) != 0) {

            READ_DATA_REGISTER(Device);
        }
    }

    //
    // If this is the mouse, or this is the keyboard and the mouse initialized
    // first, then connect the mouse interrupt.
    //

    if ((Device->InterruptHandles[0] != INVALID_HANDLE) &&
        (Device->InterruptHandles[1] == INVALID_HANDLE) &&
        (Device->MouseInterruptFound != FALSE)) {

        RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
        Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
        Connect.Device = Device->MouseDevice;
        Connect.LineNumber = Device->MouseInterruptLine;
        Connect.Vector = Device->MouseInterruptVector;
        Connect.InterruptServiceRoutine = I8042InterruptService;
        Connect.LowLevelServiceRoutine = I8042InterruptServiceWorker;
        Connect.Context = Device;
        Connect.Interrupt = &(Device->InterruptHandles[1]);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        //
        // Both interrupts are online, though the mouse interrupt should not be
        // firing yet. Figure out the maximum runlevel between the two.
        //

        OldRunLevel = KeRaiseRunLevel(Device->InterruptRunLevel);
        KeAcquireSpinLock(&(Device->InterruptLock));
        Device->InterruptRunLevel =
                           IoGetInterruptRunLevel(Device->InterruptHandles, 2);

        KeReleaseSpinLock(&(Device->InterruptLock));
        KeLowerRunLevel(OldRunLevel);

        //
        // Fire up the mouse.
        //

        Status = I8042pEnableMouse(Device);
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }
    }

    Status = STATUS_SUCCESS;

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Irp->Device == Device->KeyboardDevice) {
            Device->KeyboardInterruptFound = FALSE;
            if (Device->InterruptHandles[0] != INVALID_HANDLE) {
                IoDisconnectInterrupt(Device->InterruptHandles[0]);
                Device->InterruptHandles[0] = INVALID_HANDLE;
            }

            if (Device->KeyboardInputHandle != INVALID_HANDLE) {
                InDestroyInputDevice(Device->KeyboardInputHandle);
                Device->KeyboardInputHandle = INVALID_HANDLE;
            }
        }

        //
        // If either the keyboard or the mouse fails, disconnect the mouse
        // interrupt.
        //

        Device->MouseInterruptFound = FALSE;
        if (Device->InterruptHandles[1] != INVALID_HANDLE) {
            IoDisconnectInterrupt(Device->InterruptHandles[1]);
            Device->InterruptHandles[1] = INVALID_HANDLE;
        }

        if (Device->MouseInputHandle != INVALID_HANDLE) {
            InDestroyInputDevice(Device->MouseInputHandle);
            Device->MouseInputHandle = INVALID_HANDLE;
        }
    }

    return Status;
}

KSTATUS
I8042pEnableDevice (
    PI8042_DEVICE Device
    )

/*++

Routine Description:

    This routine enables the given 8042 device.

Arguments:

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

    CommandByte |= I8042_COMMAND_BYTE_TRANSLATION_ENABLED |
                   I8042_COMMAND_BYTE_MOUSE_DISABLED;

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

    if (Device->KeyboardInputHandle == INVALID_HANDLE) {
        RtlZeroMemory(&Description, sizeof(USER_INPUT_DEVICE_DESCRIPTION));
        Description.Device = Device->KeyboardDevice;
        Description.DeviceContext = Device;
        Description.Type = UserInputDeviceKeyboard;
        Description.InterfaceVersion =
                              USER_INPUT_KEYBOARD_DEVICE_INTERFACE_VERSION;

        Description.U.KeyboardInterface.SetLedState = I8042pSetLedState;
        Device->KeyboardInputHandle = InRegisterInputDevice(&Description);
        if (Device->KeyboardInputHandle == INVALID_HANDLE) {
            Status = STATUS_UNSUCCESSFUL;
            goto EnableDeviceEnd;
        }
    }

    //
    // Enable the keyboard interrupt.
    //

    CommandByte |= I8042_COMMAND_BYTE_KEYBOARD_INTERRUPT_ENABLED;
    I8042pWriteCommandByte(Device, CommandByte);

EnableDeviceEnd:
    return Status;
}

KSTATUS
I8042pEnableMouse (
    PI8042_DEVICE Device
    )

/*++

Routine Description:

    This routine enables the given 8042 device.

Arguments:

    Device - Supplies a pointer to this 8042 controller device.

Return Value:

    Status code.

--*/

{

    UCHAR CommandByte;
    USER_INPUT_DEVICE_DESCRIPTION Description;
    UCHAR MouseId;
    RUNLEVEL OldRunLevel;
    UCHAR Reset;
    KSTATUS Status;
    ULONG Try;

    //
    // Create the user input handle if not already done.
    //

    if (Device->MouseInputHandle == INVALID_HANDLE) {
        RtlZeroMemory(&Description, sizeof(USER_INPUT_DEVICE_DESCRIPTION));
        Description.Device = Device->MouseDevice;
        Description.DeviceContext = Device;
        Description.Type = UserInputDeviceMouse;
        Device->MouseInputHandle = InRegisterInputDevice(&Description);
        if (Device->MouseInputHandle == INVALID_HANDLE) {
            return STATUS_UNSUCCESSFUL;
        }
    }

    OldRunLevel = KeRaiseRunLevel(Device->InterruptRunLevel);
    KeAcquireSpinLock(&(Device->InterruptLock));

    //
    // Enable the mouse but disable the interrupt during initialization.
    //

    CommandByte = I8042pReadCommandByte(Device);
    CommandByte &= ~(I8042_COMMAND_BYTE_MOUSE_DISABLED |
                     I8042_COMMAND_BYTE_MOUSE_INTERRUPT_ENABLED |
                     I8042_COMMAND_BYTE_KEYBOARD_INTERRUPT_ENABLED);

    CommandByte |= I8042_COMMAND_BYTE_KEYBOARD_DISABLED;
    I8042pWriteCommandByte(Device, CommandByte);

    //
    // Reset the mouse.
    //

    Status = I8042pSendMouseCommand(Device,
                                    MOUSE_COMMAND_RESET,
                                    MOUSE_COMMAND_NO_PARAMETER);

    if (!KSUCCESS(Status)) {
        goto EnableMouseEnd;
    }

    for (Try = 0; Try < 5; Try += 1) {
        Status = I8042pReceiveMouseResponse(Device, &Reset);
        if (Status == STATUS_TIMEOUT) {
            continue;
        }

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("i8042: Mouse failed reset response: %d %x\n",
                          Status,
                          Reset);

            goto EnableMouseEnd;
        }

        if (Reset == 0xAA) {

            //
            // Also get the mouse ID. Failure here is not fatal.
            //

            I8042pReceiveMouseResponse(Device, &MouseId);
            break;
        }
    }

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("i8042: Failed to get mouse reset response: %d\n",
                      Status);

        goto EnableMouseEnd;
    }

    //
    // Restore the defaults.
    //

    Status = I8042pSendMouseCommand(Device,
                                    MOUSE_COMMAND_SET_DEFAULTS,
                                    MOUSE_COMMAND_NO_PARAMETER);

    if (!KSUCCESS(Status)) {
        goto EnableMouseEnd;
    }

    Status = I8042pSendMouseCommand(Device,
                                    MOUSE_COMMAND_GET_MOUSE_ID,
                                    MOUSE_COMMAND_NO_PARAMETER);

    if (!KSUCCESS(Status)) {
        goto EnableMouseEnd;
    }

    Status = I8042pReceiveMouseResponse(Device, &MouseId);
    if (!KSUCCESS(Status)) {
        goto EnableMouseEnd;
    }

    //
    // If the mouse ID is 3 or 4, the 4-byte reports with the scroll wheel are
    // already enabled. Otherwise, send the magic knock sequence to enable
    // 4-byte reports with the scroll wheel.
    //

    if ((MouseId != PS2_MOUSE_WITH_SCROLL_WHEEL) &&
        (MouseId != PS2_FIVE_BUTTON_MOUSE)) {

        Status = I8042pSendMouseCommand(Device,
                                        MOUSE_COMMAND_SET_SAMPLE_RATE,
                                        200);

        if (!KSUCCESS(Status)) {
            goto EnableMouseEnd;
        }

        Status = I8042pSendMouseCommand(Device,
                                        MOUSE_COMMAND_SET_SAMPLE_RATE,
                                        100);

        if (!KSUCCESS(Status)) {
            goto EnableMouseEnd;
        }
    }

    //
    // The magic knock sequence ends with 80, but do it unconditionally since
    // that's also a decent sampling rate to end up at.
    //

    Status = I8042pSendMouseCommand(Device,
                                    MOUSE_COMMAND_SET_SAMPLE_RATE,
                                    80);

    if (!KSUCCESS(Status)) {
        goto EnableMouseEnd;
    }

    //
    // Now get the mouse ID again. If it's 3 or 4, then the reports are 4 bytes
    // long.
    //

    Status = I8042pSendMouseCommand(Device,
                                    MOUSE_COMMAND_GET_MOUSE_ID,
                                    MOUSE_COMMAND_NO_PARAMETER);

    if (!KSUCCESS(Status)) {
        goto EnableMouseEnd;
    }

    Status = I8042pReceiveMouseResponse(Device, &MouseId);
    if (!KSUCCESS(Status)) {
        goto EnableMouseEnd;
    }

    if ((MouseId == PS2_MOUSE_WITH_SCROLL_WHEEL) ||
        (MouseId == PS2_FIVE_BUTTON_MOUSE)) {

        Device->MouseReportSize = 4;
    }

    //
    // Okay, everything is ready to go. Enable streaming mouse input.
    //

    Status = I8042pSendMouseCommand(Device,
                                    MOUSE_COMMAND_ENABLE,
                                    MOUSE_COMMAND_NO_PARAMETER);

    if (!KSUCCESS(Status)) {
        goto EnableMouseEnd;
    }

    //
    // Enable the mouse interrupt.
    //

EnableMouseEnd:
    if (KSUCCESS(Status)) {
        CommandByte |= I8042_COMMAND_BYTE_MOUSE_INTERRUPT_ENABLED;
    }

    CommandByte |= I8042_COMMAND_BYTE_KEYBOARD_INTERRUPT_ENABLED;
    CommandByte &= ~I8042_COMMAND_BYTE_KEYBOARD_DISABLED;
    I8042pWriteCommandByte(Device, CommandByte);
    KeReleaseSpinLock(&(Device->InterruptLock));
    KeLowerRunLevel(OldRunLevel);
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
    RUNLEVEL OldRunLevel;
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

    OldRunLevel = KeRaiseRunLevel(I8042Device->InterruptRunLevel);
    KeAcquireSpinLock(&(I8042Device->InterruptLock));
    Status = I8042pSendKeyboardCommand(I8042Device,
                                       KEYBOARD_COMMAND_SET_LEDS,
                                       KeyboardLedState);

    KeReleaseSpinLock(&(I8042Device->InterruptLock));
    KeLowerRunLevel(OldRunLevel);
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
    WAIT_FOR_OUTPUT_BUFFER(Device);
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

    while (TRUE) {
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
    }

    return STATUS_DEVICE_IO_ERROR;
}

KSTATUS
I8042pSendMouseCommand (
    PI8042_DEVICE Device,
    UCHAR Command,
    UCHAR Parameter
    )

/*++

Routine Description:

    This routine sends a command byte to the mouse and checks the return status
    byte.

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

    UCHAR MouseResult;
    KSTATUS Status;

    Status = I8042pSendCommand(Device, I8042_COMMAND_WRITE_TO_MOUSE);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    WRITE_DATA_REGISTER(Device, Command);
    Status = I8042pReceiveMouseResponse(Device, &MouseResult);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (MouseResult != MOUSE_STATUS_ACKNOWLEDGE) {
        return STATUS_DEVICE_IO_ERROR;
    }

    if (Parameter != MOUSE_COMMAND_NO_PARAMETER) {
        Status = I8042pSendCommand(Device, I8042_COMMAND_WRITE_TO_MOUSE);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        WRITE_DATA_REGISTER(Device, Parameter);
        Status = I8042pReceiveMouseResponse(Device, &MouseResult);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        if (MouseResult != MOUSE_STATUS_ACKNOWLEDGE) {
            return STATUS_DEVICE_IO_ERROR;
        }
    }

    return STATUS_SUCCESS;
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

    Timeout = 0;
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

        if (Timeout == 0) {
            Timeout = HlQueryTimeCounter() +
                      ((HlQueryTimeCounterFrequency() * I8042_COMMAND_TIMEOUT) /
                       MILLISECONDS_PER_SECOND);

        }

    } while (HlQueryTimeCounter() <= Timeout);

    return StatusCode;
}

KSTATUS
I8042pReceiveMouseResponse (
    PI8042_DEVICE Device,
    PUCHAR Data
    )

/*++

Routine Description:

    This routine receives a byte from the mouse data port, with a timeout.

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

    Timeout = 0;
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

            //
            // If it's from the mouse, hooray. If it's from the keyboard, throw
            // it away.
            //

            if ((StatusRegister & I8042_STATUS_DATA_FROM_MOUSE) != 0) {
                StatusCode = STATUS_SUCCESS;
                break;
            }
        }

        if (Timeout == 0) {
            Timeout = HlQueryTimeCounter() +
                      ((HlQueryTimeCounterFrequency() * I8042_COMMAND_TIMEOUT) /
                       MILLISECONDS_PER_SECOND);

        }

    } while (HlQueryTimeCounter() <= Timeout);

    return StatusCode;
}

VOID
I8042pProcessMouseReport (
    PI8042_DEVICE Device,
    UCHAR Report[4]
    )

/*++

Routine Description:

    This routine processes a mouse report.

Arguments:

    Device - Supplies a pointer to this 8042 controller device.

    Report - Supplies the mouse report, which is always 4 bytes even if the
        mouse doesn't support scroll wheel operations.

Return Value:

    None.

--*/

{

    USER_INPUT_EVENT Event;

    if ((Report[0] & PS2_MOUSE_REPORT_OVERFLOW) != 0) {
        RtlDebugPrint("PS2 Mouse overflow %x %x %x\n",
                      Report[0],
                      Report[1],
                      Report[2]);

        return;
    }

    RtlZeroMemory(&Event, sizeof(USER_INPUT_EVENT));
    Event.EventType = UserInputEventMouse;
    Event.U.Mouse.MovementX = Report[1];
    Event.U.Mouse.MovementY = Report[2];
    if ((Report[0] & PS2_MOUSE_REPORT_X_NEGATIVE) != 0) {
        Event.U.Mouse.MovementX |= 0xFFFFFF00;
    }

    if ((Report[0] & PS2_MOUSE_REPORT_Y_NEGATIVE) != 0) {
        Event.U.Mouse.MovementY |= 0xFFFFFF00;
    }

    Event.U.Mouse.MovementY = -Event.U.Mouse.MovementY;

    ASSERT((PS2_MOUSE_REPORT_LEFT_BUTTON == MOUSE_BUTTON_LEFT) &&
           (PS2_MOUSE_REPORT_RIGHT_BUTTON == MOUSE_BUTTON_RIGHT) &&
           (PS2_MOUSE_REPORT_MIDDLE_BUTTON == MOUSE_BUTTON_MIDDLE));

    Event.U.Mouse.Buttons = Report[0] & PS2_MOUSE_REPORT_BUTTONS;
    switch (Report[3] & 0x0F) {
    case 0x0:
        break;

    case 0x1:
        Event.U.Mouse.ScrollY = 1;
        break;

    case 0x2:
        Event.U.Mouse.ScrollX = 1;
        break;

    case 0xE:
        Event.U.Mouse.ScrollX = -1;
        break;

    case 0xF:
        Event.U.Mouse.ScrollY = -1;
        break;

    default:
        RtlDebugPrint("PS/2 Mouse: Unknown scroll movement 0x%x\n", Report[3]);
        break;
    }

    InReportInputEvent(Device->MouseInputHandle, &Event);
    return;
}

