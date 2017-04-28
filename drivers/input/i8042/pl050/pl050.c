/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pl050.c

Abstract:

    This module implements the driver for the ARM PrimeCell PL050 keyboard and
    mouse controller.

Author:

    Evan Green 22-Sep-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "../i8042.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write byte registers in the PL050. The first register
// is a pointer to the controller structure, the second is the register number.
//

#define PL050_READ(_Controller, _Register) \
    HlReadRegister8(((PUCHAR)(_Controller)->RegisterBase) + (_Register))

#define PL050_WRITE(_Controller, _Register, _Value)                         \
    HlWriteRegister8(((PUCHAR)(_Controller)->RegisterBase) + (_Register),   \
                     (_Value))

//
// This macro spins waiting for the last keyboard command to finish.
//

#define WAIT_FOR_INPUT_BUFFER(_Device)                  \
    while ((PL050_READ(_Device, Pl050RegisterStatus) &  \
            PL050_STATUS_TRANSMIT_EMPTY) == 0) {        \
                                                        \
        NOTHING;                                        \
    }

//
// This macro determines if data is available to be received from the device.
//

#define IS_DATA_AVAILABLE(_Device)                  \
    ((PL050_READ(_Device, Pl050RegisterStatus) &    \
      PL050_STATUS_RECEIVE_FULL) != 0)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the size of the buffer of bytes stored directly by the ISR.
//

#define PL050_BUFFER_SIZE 256

#define PL050_ALLOCATION_TAG 0x30506C50 // '05lP'

//
// Define the number of microseconds to wait for a command to complete.
//

#define PL050_COMMAND_TIMEOUT (50 * MICROSECONDS_PER_MILLISECOND)

//
// Define control register bits.
//

#define PL050_CONTROL_ENABLE 0x04
#define PL050_CONTROL_TRANSMIT_INTERRUPT_ENABLE 0x08
#define PL050_CONTROL_RECEIVE_INTERRUPT_ENABLE 0x10

//
// Define status register bits.
//

#define PL050_STATUS_RECEIVE_BUSY 0x08
#define PL050_STATUS_RECEIVE_FULL 0x10
#define PL050_STATUS_TRANSMIT_BUSY 0x20
#define PL050_STATUS_TRANSMIT_EMPTY 0x40

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _PL050_REGISTER {
    Pl050RegisterControl = 0x00,
    Pl050RegisterStatus = 0x04,
    Pl050RegisterData = 0x08,
    Pl050RegisterClockDivisor = 0x0C,
    Pl050RegisterInterruptStatus = 0x10
} PL050_REGISTER, *PPL050_REGISTER;

/*++

Structure Description:

    This structure stores context about a device driven by the Pl050 driver.

Members:

    IsMouse - Stores a boolean indicating whether the device is a mouse (TRUE)
        or a keyboard (FALSE).

    PhysicalAddress - Stores the physical address of the registers.

    RegisterBase - Stores the virtual address of the registers.

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

typedef struct _PL050_DEVICE {
    BOOL IsMouse;
    ULONGLONG PhysicalAddress;
    PVOID RegisterBase;
    ULONGLONG InterruptVector;
    ULONGLONG InterruptLine;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    HANDLE UserInputDeviceHandle;
    KSPIN_LOCK InterruptLock;
    PQUEUED_LOCK ReadLock;
    volatile ULONG ReadIndex;
    volatile ULONG WriteIndex;
    volatile UCHAR DataBuffer[PL050_BUFFER_SIZE];
} PL050_DEVICE, *PPL050_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Pl050AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Pl050DispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Pl050DispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Pl050DispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Pl050DispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Pl050DispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
Pl050InterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
Pl050InterruptServiceWorker (
    PVOID Parameter
    );

KSTATUS
Pl050pProcessResourceRequirements (
    PIRP Irp,
    PPL050_DEVICE Device
    );

KSTATUS
Pl050pStartDevice (
    PIRP Irp,
    PPL050_DEVICE Device
    );

KSTATUS
Pl050pEnableDevice (
    PVOID OsDevice,
    PPL050_DEVICE Device
    );

KSTATUS
Pl050pDisableDevice (
    PPL050_DEVICE Device
    );

KSTATUS
Pl050pSetLedState (
    PVOID Device,
    PVOID DeviceContext,
    ULONG LedState
    );

KSTATUS
Pl050pSendKeyboardCommand (
    PPL050_DEVICE Device,
    UCHAR Command,
    UCHAR Parameter
    );

KSTATUS
Pl050pSetScanSet (
    PPL050_DEVICE Device,
    UCHAR ScanSet
    );

KSTATUS
Pl050pIdentifyDevice (
    PPL050_DEVICE Device,
    PBOOL IsMouse
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Pl050Driver = NULL;

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

    This routine is the entry point for the Pl050 driver. It registers its other
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

    Pl050Driver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Pl050AddDevice;
    FunctionTable.DispatchStateChange = Pl050DispatchStateChange;
    FunctionTable.DispatchOpen = Pl050DispatchOpen;
    FunctionTable.DispatchClose = Pl050DispatchClose;
    FunctionTable.DispatchIo = Pl050DispatchIo;
    FunctionTable.DispatchSystemControl = Pl050DispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Pl050AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the Pl050 driver
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

    PPL050_DEVICE NewDevice;
    KSTATUS Status;

    //
    // there is a match, create the device context and attach to the device.
    //

    NewDevice = MmAllocateNonPagedPool(sizeof(PL050_DEVICE),
                                       PL050_ALLOCATION_TAG);

    if (NewDevice == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewDevice, sizeof(PL050_DEVICE));
    KeInitializeSpinLock(&(NewDevice->InterruptLock));
    NewDevice->InterruptHandle = INVALID_HANDLE;
    NewDevice->UserInputDeviceHandle = INVALID_HANDLE;
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
            if (NewDevice->UserInputDeviceHandle != INVALID_HANDLE) {
                InDestroyInputDevice(NewDevice->UserInputDeviceHandle);
            }

            if (NewDevice->ReadLock != NULL) {
                KeDestroyQueuedLock(NewDevice->ReadLock);
            }

            MmFreeNonPagedPool(NewDevice);
        }
    }

    return Status;
}

VOID
Pl050DispatchStateChange (
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

    PPL050_DEVICE Device;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Device = (PPL050_DEVICE)DeviceContext;
    switch (Irp->MinorCode) {
    case IrpMinorQueryResources:

        //
        // On the way up, filter the resource requirements to add interrupt
        // vectors to any lines.
        //

        if (Irp->Direction == IrpUp) {
            Status = Pl050pProcessResourceRequirements(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Pl050Driver, Irp, Status);
            }
        }

        break;

    case IrpMinorStartDevice:

        //
        // Attempt to fire the thing up if the bus has already started it.
        //

        if (Irp->Direction == IrpUp) {
            Status = Pl050pStartDevice(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Pl050Driver, Irp, Status);
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
Pl050DispatchOpen (
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
Pl050DispatchClose (
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
Pl050DispatchIo (
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
Pl050DispatchSystemControl (
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
Pl050InterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the PL-050 keyboard controller interrupt service
    routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the device
        context.

Return Value:

    Interrupt status.

--*/

{

    UCHAR Byte;
    PPL050_DEVICE Device;
    INTERRUPT_STATUS InterruptStatus;
    ULONG WriteIndex;

    Device = (PPL050_DEVICE)Context;
    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Check to see if there is data waiting.
    //

    if ((PL050_READ(Device, Pl050RegisterStatus) &
         PL050_STATUS_RECEIVE_FULL) != 0) {

        //
        // There was data here, so most likely it was this device interrupting.
        //

        InterruptStatus = InterruptStatusClaimed;

        //
        // Read the bytes out of the controller.
        //

        KeAcquireSpinLock(&(Device->InterruptLock));
        WriteIndex = Device->WriteIndex;
        while ((PL050_READ(Device, Pl050RegisterStatus) &
                PL050_STATUS_RECEIVE_FULL) != 0) {

            Byte = PL050_READ(Device, Pl050RegisterData);
            if (((WriteIndex + 1) % PL050_BUFFER_SIZE) != Device->ReadIndex) {
                Device->DataBuffer[WriteIndex] = Byte;

                //
                // Advance the write index.
                //

                if ((WriteIndex + 1) == PL050_BUFFER_SIZE) {
                    WriteIndex = 0;

                } else {
                    WriteIndex += 1;
                }

            } else {
                RtlDebugPrint("Pl050: Device 0x%08x, buffer overflow, losing "
                              "byte %02X\n",
                              Device,
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
Pl050InterruptServiceWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes interrupts for the Pl050 controller at low level.

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
    PPL050_DEVICE Device;
    USER_INPUT_EVENT Event;
    BOOL KeyUp;
    ULONG ReadIndex;

    Code1 = 0;
    Code2 = 0;
    Code3 = 0;
    Device = (PPL050_DEVICE)Parameter;

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
        if (ReadIndex == PL050_BUFFER_SIZE) {
            ReadIndex = 0;
        }

        if (Device->IsMouse != FALSE) {

            //
            // Eventually process the mouse events.
            //

        } else {

            //
            // If the first byte read was the extended 2 code, then another two
            // bytes should be coming in. Get those bytes.
            //

            if (Code1 == SCAN_CODE_1_EXTENDED_2_CODE) {
                if (Code2 == 0) {
                    Code2 = Byte;
                    continue;
                }

                Code3 = Byte;

            //
            // If the first byte read was the extended (1) code, then another
            // byte should be coming in. Get that byte.
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

            Event.U.Key = I8042ConvertScanCodeToKey(Code1,
                                                    Code2,
                                                    Code3,
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

                InReportInputEvent(Device->UserInputDeviceHandle, &Event);
            }

            //
            // A full key combination was read, move the read index forward.
            //

            Device->ReadIndex = ReadIndex;
            Code1 = 0;
            Code2 = 0;
        }
    }

    KeReleaseQueuedLock(Device->ReadLock);
    return InterruptStatusClaimed;
}

KSTATUS
Pl050pProcessResourceRequirements (
    PIRP Irp,
    PPL050_DEVICE Device
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus. It adds an interrupt vector requirement for any interrupt line
    requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this controller device.

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
Pl050pStartDevice (
    PIRP Irp,
    PPL050_DEVICE Device
    )

/*++

Routine Description:

    This routine starts up the PL-050 controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this controller device.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PRESOURCE_ALLOCATION LineAllocation;
    BOOL RegistersFound;
    KSTATUS Status;

    RegistersFound = FALSE;

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
        if (Allocation->Type == ResourceTypePhysicalAddressSpace) {
            if (Device->PhysicalAddress != Allocation->Allocation) {
                if (Device->RegisterBase != NULL) {
                    MmUnmapAddress(Device->RegisterBase, Allocation->Length);
                    Device->RegisterBase = NULL;
                }

                Device->PhysicalAddress = Allocation->Allocation;
            }

            RegistersFound = TRUE;
            if (Device->RegisterBase == NULL) {
                Device->RegisterBase = MmMapPhysicalAddress(
                                                       Device->PhysicalAddress,
                                                       Allocation->Length,
                                                       TRUE,
                                                       FALSE,
                                                       TRUE);

                if (Device->RegisterBase == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto StartDeviceEnd;
                }
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
    // Fail if the controller base wasn't found.
    //

    if (RegistersFound == FALSE) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto StartDeviceEnd;
    }

    //
    // Make sure the device and its interrupts are disabled before connecting
    // the interrupt. There may be leftover state from the last reboot.
    //

    Status = Pl050pDisableDevice(Device);
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
    Connect.InterruptServiceRoutine = Pl050InterruptService;
    Connect.LowLevelServiceRoutine = Pl050InterruptServiceWorker;
    Connect.Context = Device;
    Connect.Interrupt = &(Device->InterruptHandle);
    Status = IoConnectInterrupt(&Connect);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Fire up the device.
    //

    Status = Pl050pEnableDevice(Irp->Device, Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
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
Pl050pEnableDevice (
    PVOID OsDevice,
    PPL050_DEVICE Device
    )

/*++

Routine Description:

    This routine enables the given PL-050 device.

Arguments:

    OsDevice - Supplies a pointer to the OS device token.

    Device - Supplies a pointer to this controller device.

Return Value:

    Status code.

--*/

{

    UCHAR ControlByte;
    USER_INPUT_DEVICE_DESCRIPTION Description;
    KSTATUS Status;

    ControlByte = PL050_CONTROL_ENABLE;
    PL050_WRITE(Device, Pl050RegisterControl, ControlByte);

    //
    // Figure out if this is a keyboard or a mouse.
    //

    Status = Pl050pIdentifyDevice(Device, &(Device->IsMouse));
    if (!KSUCCESS(Status)) {
        goto EnableDeviceEnd;
    }

    if (Device->IsMouse != FALSE) {

        //
        // Mice are not currently supported.
        //

        return STATUS_NOT_IMPLEMENTED;

    } else {

        //
        // Set the scan set for the keyboard.
        //

        Status = Pl050pSetScanSet(Device, 1);
        if (!KSUCCESS(Status)) {
            goto EnableDeviceEnd;
        }

        //
        // Set the typematic rate/delay on the keyboard. Start by sending the
        // command. This command overlaps with the mouse sample rate.
        //

        Status = Pl050pSendKeyboardCommand(Device,
                                           KEYBOARD_COMMAND_SET_TYPEMATIC,
                                           DEFAULT_TYPEMATIC_VALUE);

        if (!KSUCCESS(Status)) {
            goto EnableDeviceEnd;
        }

        //
        // Enable the keyboard. This overlaps with the mouse enable command.
        //

        Status = Pl050pSendKeyboardCommand(Device,
                                           KEYBOARD_COMMAND_ENABLE,
                                           KEYBOARD_COMMAND_NO_PARAMETER);

        if (!KSUCCESS(Status)) {
            goto EnableDeviceEnd;
        }

        //
        // Create the user input handle if not already done.
        //

        if (Device->UserInputDeviceHandle == INVALID_HANDLE) {
            RtlZeroMemory(&Description, sizeof(USER_INPUT_DEVICE_DESCRIPTION));
            Description.Device = OsDevice;
            Description.DeviceContext = Device;
            Description.Type = UserInputDeviceKeyboard;
            Description.InterfaceVersion =
                                  USER_INPUT_KEYBOARD_DEVICE_INTERFACE_VERSION;

            Description.U.KeyboardInterface.SetLedState = Pl050pSetLedState;
            Device->UserInputDeviceHandle = InRegisterInputDevice(&Description);
            if (Device->UserInputDeviceHandle == INVALID_HANDLE) {
                Status = STATUS_UNSUCCESSFUL;
                goto EnableDeviceEnd;
            }
        }

        //
        // Enable the keyboard interrupt.
        //

        ControlByte |= PL050_CONTROL_RECEIVE_INTERRUPT_ENABLE;
        PL050_WRITE(Device, Pl050RegisterControl, ControlByte);
    }

EnableDeviceEnd:
    return Status;
}

KSTATUS
Pl050pDisableDevice (
    PPL050_DEVICE Device
    )

/*++

Routine Description:

    This routine disables a PL050 mouse or keyboard.

Arguments:

    Device - Supplies a pointer to this controller device.

Return Value:

    Status code.

--*/

{

    UCHAR KeyboardResult;
    UCHAR ReadStatus;
    KSTATUS ReturnStatus;

    //
    // Send the disable command and wait for one of the expected status codes.
    // The keyboard command overlaps with the mouse disable command.
    //

    WAIT_FOR_INPUT_BUFFER(Device);
    PL050_WRITE(Device, Pl050RegisterData, KEYBOARD_COMMAND_RESET_AND_DISABLE);
    while (TRUE) {

        //
        // Loop waiting for the command to be received.
        //

        while (TRUE) {
            ReadStatus = PL050_READ(Device, Pl050RegisterStatus);
            if ((ReadStatus & PL050_STATUS_RECEIVE_FULL) != 0) {
                break;
            }
        }

        //
        // Read the result. If it is not a keyboard status, just eat it and try
        // again. It's likely that there is something in the keyboard buffer.
        //

        KeyboardResult = PL050_READ(Device, Pl050RegisterData);
        if (KeyboardResult == KEYBOARD_STATUS_ACKNOWLEDGE) {
            ReturnStatus = STATUS_SUCCESS;
            break;
        }

        if (KeyboardResult == KEYBOARD_STATUS_RESEND) {
            ReturnStatus = STATUS_NOT_READY;
            break;
        }

        if (KeyboardResult == KEYBOARD_STATUS_OVERRUN) {
            ReturnStatus = STATUS_BUFFER_OVERRUN;
            break;
        }

        if (KeyboardResult == KEYBOARD_STATUS_INVALID) {
            ReturnStatus = STATUS_DEVICE_IO_ERROR;
            break;
        }
    }

    //
    // The control register is supposed to be cleared to zero on reset, but
    // just make sure in case of faulty hardware. This will disable interrupts.
    //

    PL050_WRITE(Device, Pl050RegisterControl, 0);
    return ReturnStatus;
}

KSTATUS
Pl050pSetLedState (
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

    UCHAR KeyboardLedState;
    PPL050_DEVICE Pl050Device;
    KSTATUS Status;

    Pl050Device = (PPL050_DEVICE)DeviceContext;

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

    Status = Pl050pSendKeyboardCommand(Pl050Device,
                                       KEYBOARD_COMMAND_SET_LEDS,
                                       KeyboardLedState);

    return Status;
}

KSTATUS
Pl050pSendKeyboardCommand (
    PPL050_DEVICE Device,
    UCHAR Command,
    UCHAR Parameter
    )

/*++

Routine Description:

    This routine sends a command byte to the keyboard itself (not the
    keyboard controller) and checks the return status byte.

Arguments:

    Device - Supplies a pointer to this controller device.

    Command - Supplies the command to write to the keyboard.

    Parameter - Supplies an additional byte to send. Set this to 0xFF to skip
        sending this byte.

Return Value:

    Status code indicating whether the keyboard succeeded or failed the
    command.

--*/

{

    ULONGLONG EndTime;
    UCHAR KeyboardResult;
    UCHAR Status;

    WAIT_FOR_INPUT_BUFFER(Device);
    PL050_WRITE(Device, Pl050RegisterData, Command);
    if (Parameter != KEYBOARD_COMMAND_NO_PARAMETER) {
        WAIT_FOR_INPUT_BUFFER(Device);
        PL050_WRITE(Device, Pl050RegisterData, Parameter);
    }

    //
    // Wait for the command to complete.
    //

    EndTime = HlQueryTimeCounter() +
              KeConvertMicrosecondsToTimeTicks(PL050_COMMAND_TIMEOUT);

    while (TRUE) {
        Status = PL050_READ(Device, Pl050RegisterStatus);
        if ((Status & PL050_STATUS_RECEIVE_FULL) != 0) {
            break;
        }

        if (HlQueryTimeCounter() >= EndTime) {
            return STATUS_TIMEOUT;
        }
    }

    //
    // Read the result.
    //

    KeyboardResult = PL050_READ(Device, Pl050RegisterData);
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
Pl050pSetScanSet (
    PPL050_DEVICE Device,
    UCHAR ScanSet
    )

/*++

Routine Description:

    This routine sets the scan set for the keyboard.

Arguments:

    Device - Supplies a pointer to this controller device.

    ScanSet - Supplies the scan set to get. Valid values are 1, 2, and 3.

Return Value:

    Status code indicating whether the keyboard succeeded or failed the
    command.

--*/

{

    KSTATUS Status;

    Status = Pl050pSendKeyboardCommand(Device,
                                       KEYBOARD_COMMAND_GET_SET_SCAN_SET,
                                       ScanSet);

    return Status;
}

KSTATUS
Pl050pIdentifyDevice (
    PPL050_DEVICE Device,
    PBOOL IsMouse
    )

/*++

Routine Description:

    This routine determines if the given device is a mouse or a keyboard.

Arguments:

    Device - Supplies a pointer to this controller device.

    IsMouse - Supplies a boolean indicating if the device is a mouse.

Return Value:

    Status code indicating whether the determination was successful or not.

--*/

{

    ULONGLONG EndTime;
    UCHAR Result[3];
    ULONG ResultCount;
    ULONG ResultIndex;
    KSTATUS Status;

    //
    // Disable the device to prevent keystrokes from getting in the way during
    // the determination.
    //

    Status = Pl050pDisableDevice(Device);
    if (!KSUCCESS(Status)) {
        goto IdentifyDeviceEnd;
    }

    //
    // Get the keyboard identity. This overlaps with the mouse read ID command.
    //

    WAIT_FOR_INPUT_BUFFER(Device);
    PL050_WRITE(Device, Pl050RegisterData, KEYBOARD_COMMAND_IDENTIFY);
    EndTime = HlQueryTimeCounter() +
              KeConvertMicrosecondsToTimeTicks(PL050_COMMAND_TIMEOUT);

    ResultCount = 0;
    while (ResultCount < sizeof(Result)) {
        if (IS_DATA_AVAILABLE(Device)) {
            Result[ResultCount] = PL050_READ(Device, Pl050RegisterData);
            ResultCount += 1;
            continue;
        }

        if (HlQueryTimeCounter() >= EndTime) {
            Status = STATUS_TIMEOUT;
            break;
        }
    }

    if (ResultCount == 0) {
        goto IdentifyDeviceEnd;
    }

    ResultIndex = 0;
    if (Result[ResultIndex] == KEYBOARD_STATUS_ACKNOWLEDGE) {
        ResultIndex += 1;
    }

    *IsMouse = FALSE;
    if ((ResultCount > ResultIndex) &&
        ((Result[ResultIndex] == PS2_STANDARD_MOUSE) ||
         (Result[ResultIndex] == PS2_MOUSE_WITH_SCROLL_WHEEL) ||
         (Result[ResultIndex] == PS2_FIVE_BUTTON_MOUSE))) {

        *IsMouse = TRUE;
    }

    //
    // Re-enable scanning. This overlaps with the mouse enable command.
    //

    Status = Pl050pSendKeyboardCommand(Device,
                                       KEYBOARD_COMMAND_ENABLE,
                                       KEYBOARD_COMMAND_NO_PARAMETER);

IdentifyDeviceEnd:
    return Status;
}

