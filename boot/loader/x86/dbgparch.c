/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgparch.c

Abstract:

    This module contains architecture specific debug port routines.

Author:

    Evan Green 25-Mar-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/ioport.h>
#include "firmware.h"
#include "bootlib.h"
#include "loader.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro creates the address value used to read from or write to PCI
// configuration space. All parameters should be UCHARs.
//

#define PCI_CONFIG_ADDRESS(_Bus, _Device, _Function, _Register) \
    (((ULONG)(_Bus) << 16) | ((ULONG)(_Device) << 11) |         \
     ((ULONG)(_Function) << 8) | ((_Register) & 0xFF) | 0x80000000)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of debug devices to put in the table.
//

#define GENERATED_DEBUG_DEVICE_COUNT 8

//
// Define the standard I/O ports to use to access PCI configuration space.
//

#define PCI_ROOT_CONFIG_ADDRESS 0xCF8
#define PCI_ROOT_CONFIG_DATA 0xCFC

#define BOOT_MAX_PCI_BUS 16
#define MAX_PCI_FUNCTION 7
#define MAX_PCI_DEVICE 32

//
// PCI configuration space definitions.
//

#define PCI_ID_OFFSET 0x00
#define PCI_VENDOR_ID_MASK 0x0000FFFF
#define PCI_DEVICE_ID_SHIFT 16
#define PCI_DEVICE_ID_MASK 0xFFFF0000
#define PCI_CONTROL_OFFSET 0x04
#define PCI_STATUS_OFFSET 0x04
#define PCI_STATUS_MASK 0xFFFF0000
#define PCI_STATUS_SHIFT 16
#define PCI_CLASS_CODE_OFFSET 0x08
#define PCI_CLASS_CODE_MASK 0xFFFFFF00
#define PCI_HEADER_TYPE_OFFSET 0x0C
#define PCI_HEADER_TYPE_MASK 0x00FF0000
#define PCI_HEADER_TYPE_SHIFT 16
#define PCI_BAR_OFFSET 0x10
#define PCI_BAR_COUNT 6

#define PCI_INVALID_VENDOR_ID 0xFFFF

//
// PCI Base Address Register bit definitions.
//

#define PCI_BAR_MEMORY_FLAGS_MASK 0x0000000FULL
#define PCI_BAR_IO_FLAGS_MASK 0x00000003
#define PCI_BAR_IO_SPACE 0x00000001
#define PCI_BAR_MEMORY_SIZE_MASK 0x0000006
#define PCI_BAR_MEMORY_32_BIT 0x00000000
#define PCI_BAR_MEMORY_1MB 0x00000002
#define PCI_BAR_MEMORY_64_BIT 0x00000004
#define PCI_BAR_MEMORY_PREFETCHABLE 0x00000008

//
// PCI classes
//

#define PCI_CLASS_SIMPLE_COMMUNICATION      0x07
#define PCI_CLASS_SERIAL_BUS                0x0C

//
// PCI subclasses (and interfaces).
//

#define PCI_CLASS_SIMPLE_COMMUNICATION_16550 0x0002
#define PCI_CLASS_SIMPLE_COMMUNICATION_OTHER 0x8000

#define PCI_CLASS_SERIAL_BUS_USB_UHCI 0x0300
#define PCI_CLASS_SERIAL_BUS_USB_OHCI 0x0310
#define PCI_CLASS_SERIAL_BUS_USB_EHCI 0x0320

//
// Control register definitions.
//

#define PCI_CONTROL_IO_DECODE_ENABLED             0x0001
#define PCI_CONTROL_MEMORY_DECODE_ENABLED         0x0002

//
// Header type definitions.
//

#define PCI_HEADER_TYPE_STANDARD           0x00
#define PCI_HEADER_TYPE_PCI_TO_PCI_BRIDGE  0x01
#define PCI_HEADER_TYPE_CARDBUS_BRIDGE     0x02
#define PCI_HEADER_TYPE_VALUE_MASK         0x7F

//
// Header type flags.
//

#define PCI_HEADER_TYPE_FLAG_MULTIPLE_FUNCTIONS 0x80

//
// Known vendors and devices.
//

#define PCI_VENDOR_ID_INTEL 0x8086
#define PCI_DEVICE_ID_INTEL_QUARK_UART 0x0936

//
// Intel Quark UART information.
//

#define INTEL_QUARK_UART_BASE_BAUD 2764800
#define INTEL_QUARK_UART_REGISTER_SHIFT 2

//
// Define the offset within the device's PCI Configuration Space where the
// UHCI legacy support register lives.
//

#define UHCI_LEGACY_SUPPORT_REGISTER_OFFSET 0xC0

//
// Define the value written into the legacy support register (off in PCI config
// space) to enable UHCI interrupts and stop trapping into SMIs for legacy
// keyboard support.
//

#define UHCI_LEGACY_SUPPORT_ENABLE_USB_INTERRUPTS 0x2000

//
// EHCI register definitions
//

#define EHCI_CAPABILITY_CAPABILITIES_REGISTER          0x08
#define EHCI_CAPABILITY_CAPABILITIES_EXTENDED_CAPABILITIES_MASK 0x0000FF00
#define EHCI_CAPABILITY_CAPABILITIES_EXTENDED_CAPABILITIES_SHIFT 8

#define EHCI_EECP_LEGACY_SUPPORT_REGISTER 0x00
#define EHCI_LEGACY_SUPPORT_OS_OWNED      (1 << 24)
#define EHCI_LEGACY_SUPPORT_BIOS_OWNED    (1 << 16)
#define EHCI_EECP_LEGACY_CONTROL_REGISTER 0x04

#define EHCI_LEGACY_HANDOFF_SPIN_COUNT 10000

//
// OHCI register definitions.
//

#define OHCI_REGISTER_CONTROL 0x04
#define OHCI_REGISTER_COMMAND_STATUS 0x08
#define OHCI_REGISTER_INTERRUPT_ENABLE 0x10
#define OHCI_REGISTER_INTERRUPT_DISABLE 0x14
#define OHCI_REGISTER_FRAME_INTERVAL 0x34

#define OHCI_CONTROL_FUNCTIONAL_STATE_MASK (0x3 << 6)
#define OHCI_CONTROL_INTERRUPT_ROUTING (1 << 8)
#define OHCI_CONTROL_REMOTE_WAKE_CONNECTED (1 << 9)

#define OHCI_INTERRUPT_OWNERSHIP_CHANGE (1 << 30)

#define OHCI_COMMAND_CONTROLLER_RESET (1 << 0)
#define OHCI_COMMAND_OWNERSHIP_CHANGE_REQUEST (1 << 3)

//
// Define BIOS data area offsets.
//

#define BIOS_DATA_AREA 0x400
#define BIOS_DATA_AREA_COM1 0x400
#define BIOS_DATA_AREA_SIZE 0x100

//
// Define standard PC COM port locations.
//

#define PCAT_COM1_BASE 0x3F8
#define PCAT_COM2_BASE 0x2F8
#define PCAT_COM3_BASE 0x3E8
#define PCAT_COM4_BASE 0x2E8

//
// Define the PIC ports.
//

#define PIC_8259_MASTER_COMMAND 0x20
#define PIC_8259_MASTER_DATA 0x21
#define PIC_8259_SLAVE_COMMAND 0xA0
#define PIC_8259_SLAVE_DATA 0xA1

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _LEGACY_INTERRUPT_TYPE {
    LegacyInterruptInvalid,
    LegacyInterruptUhci,
    LegacyInterruptOhci,
    LegacyInterruptEhci
} LEGACY_INTERRUPT_TYPE, *PLEGACY_INTERRUPT_TYPE;

/*++

Union Description:

    This union stores the possible OEM data in a generated debug device.

Members:

    Uart16550 - Stores the 16550 UART OEM data.

--*/

typedef union _GENERATED_DEBUG_OEM_DATA {
    DEBUG_PORT_16550_OEM_DATA Uart16550;
} GENERATED_DEBUG_OEM_DATA, *PGENERATED_DEBUG_OEM_DATA;

/*++

Structure Description:

    This structure defines the format of a generated debug device that has
    one generic address entry.

Members:

    Device - Stores the standard device header.

    OemData - Stores the OEM data, which is optional.

    Address - Stores the address of the debug device.

    Size - Stores the size of the register region.

    NamespaceString - Stores space for the null namespace string ".".

--*/

typedef struct _GENERATED_DEBUG_DEVICE {
    DEBUG_DEVICE_INFORMATION Device;
    GENERATED_DEBUG_OEM_DATA OemData;
    GENERIC_ADDRESS Address;
    ULONG Size;
    CHAR NamespaceString[2];
} GENERATED_DEBUG_DEVICE, *PGENERATED_DEBUG_DEVICE;

/*++

Structure Description:

    This structure defines the format of a generated debug port table.

Members:

    Table - Stores the fixed table header portion.

    Device - Stores the array of debug devices.

--*/

typedef struct _GENERATED_DEBUG_PORT_TABLE2 {
    DEBUG_PORT_TABLE2 Table;
    GENERATED_DEBUG_DEVICE Device[GENERATED_DEBUG_DEVICE_COUNT];
} GENERATED_DEBUG_PORT_TABLE2, *PGENERATED_DEBUG_PORT_TABLE2;

/*++

Structure Description:

    This structure stores the context for disabling a legacy interrupt.

Members:

    ListEntry - Stores pointers to the next and previous legacy interrupts.

    Type - Stores the type of legacy interrupt this is.

    Bus - Stores the bus number of the device.

    Device - Stores the device number of the device.

    Function - Stores the function number of the device.

    ControlRegister - Stores the offset of the extended capabilities
        register in PCI configuration space for EHCI or the legacy control
        register for UHCI.

    Base - Stores the operational register base, used for OHCI controllers.

--*/

typedef struct _LEGACY_INTERRUPT_CONTEXT {
    LIST_ENTRY ListEntry;
    LEGACY_INTERRUPT_TYPE Type;
    UCHAR Bus;
    UCHAR Device;
    UCHAR Function;
    UCHAR ControlRegister;
    PHYSICAL_ADDRESS Base;
} LEGACY_INTERRUPT_CONTEXT, *PLEGACY_INTERRUPT_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
BopCheckForPcSerialPort (
    PFADT Fadt,
    PGENERATED_DEBUG_PORT_TABLE2 Table
    );

KSTATUS
BopCheckPotentialUartDebugDevice (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG DeviceClassCode,
    PGENERIC_ADDRESS Address,
    PGENERATED_DEBUG_DEVICE DebugDevice
    );

VOID
BopInitialize8259 (
    VOID
    );

KSTATUS
BopExplorePciForDebugDevice (
    PUCHAR Bus,
    PUCHAR Device,
    PUCHAR Function,
    PULONG DeviceClassCode,
    PGENERIC_ADDRESS Address
    );

KSTATUS
BopCreateLegacyEhciInterrupt (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    PHYSICAL_ADDRESS Address
    );

KSTATUS
BopCreateLegacyOhciInterrupt (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    PHYSICAL_ADDRESS Address
    );

KSTATUS
BopCreateLegacyUhciInterrupt (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function
    );

ULONGLONG
BopReadPciConfig (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG Register,
    ULONG AccessSize
    );

VOID
BopWritePciConfig (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG Register,
    ULONG AccessSize,
    ULONGLONG Value
    );

VOID
BopSetAcpiTableChecksum (
    PDESCRIPTION_HEADER Header
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this to TRUE to get debug prints as the PCI bus is explored looking for
// a debug device (EHCI controller).
//

BOOL BoDebugDebugDeviceExploration = FALSE;

//
// Set this to TRUE to skip probing for a serial port.
//

BOOL BoSkipSerialPortProbe = FALSE;

//
// Define the context information saved to shut off legacy interrupts.
//

LIST_ENTRY BoLegacyInterruptList;

//
// Define the template for a generated debug port table.
//

DEBUG_PORT_TABLE2 BoGeneratedDebugPortTableTemplate = {
    {
        DBG2_SIGNATURE,
        sizeof(DEBUG_PORT_TABLE2),
        0,
        0,
        {'M', 'i', 'n', 'o', 'c', 'a'},
        0,
        0,
        0,
        0,
    },

    FIELD_OFFSET(GENERATED_DEBUG_PORT_TABLE2, Device[0]),
    0
};

GENERATED_DEBUG_DEVICE BoGeneratedDebugDeviceTemplate = {
    {
        0,
        sizeof(GENERATED_DEBUG_DEVICE),
        1,
        2,
        FIELD_OFFSET(GENERATED_DEBUG_DEVICE, NamespaceString),
        0,
        0,
        0,
        0,
        0,
        FIELD_OFFSET(GENERATED_DEBUG_DEVICE, Address),
        FIELD_OFFSET(GENERATED_DEBUG_DEVICE, Size)
    },

    {{0}},

    {
        AddressSpaceMemory,
        0,
        0,
        0,
        0
    },

    0x400,
    {'.', '\0'}
};

//
// ------------------------------------------------------------------ Functions
//

VOID
BopDisableLegacyInterrupts (
    VOID
    )

/*++

Routine Description:

    This routine shuts off any legacy interrupts routed to SMIs for boot
    services.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PVOID Base;
    ULONG CommandStatus;
    ULONG Control;
    PLIST_ENTRY CurrentEntry;
    PFADT Fadt;
    ULONG FrameInterval;
    ULONGLONG LegacyControl;
    ULONG LegacyControlRegister;
    PLEGACY_INTERRUPT_CONTEXT LegacyInterrupt;
    ULONG Try;

    //
    // Start by enabling ACPI mode, which shuts off a lot of BIOS functionality.
    //

    Fadt = BoGetAcpiTable(FADT_SIGNATURE, NULL);
    if (Fadt != NULL) {
        if (Fadt->SmiCommandPort != 0) {

            //
            // Write the ACPI enable value into the SMI_CMD register.
            //

            HlIoPortOutByte((USHORT)Fadt->SmiCommandPort,
                            Fadt->AcpiEnable);
        }
    }

    CurrentEntry = BoLegacyInterruptList.Next;
    if (CurrentEntry == NULL) {
        return;
    }

    while (CurrentEntry != &BoLegacyInterruptList) {
        LegacyInterrupt = LIST_VALUE(CurrentEntry,
                                     LEGACY_INTERRUPT_CONTEXT,
                                     ListEntry);

        CurrentEntry = CurrentEntry->Next;
        switch (LegacyInterrupt->Type) {
        case LegacyInterruptEhci:

            //
            // Check to see if the EHCI controller is owned by the OS. If it
            // still owned by the BIOS, claim ownership, and wait for the BIOS
            // to agree.
            //

            LegacyControlRegister = LegacyInterrupt->ControlRegister +
                                    EHCI_EECP_LEGACY_SUPPORT_REGISTER;

            LegacyControl = BopReadPciConfig(LegacyInterrupt->Bus,
                                             LegacyInterrupt->Device,
                                             LegacyInterrupt->Function,
                                             LegacyControlRegister,
                                             sizeof(ULONG));

            if (BoDebugDebugDeviceExploration != FALSE) {
                RtlDebugPrint("Disabling EHCI interrupt at %x/%x/%x.%x: %x\n",
                              LegacyInterrupt->Bus,
                              LegacyInterrupt->Device,
                              LegacyInterrupt->Function,
                              LegacyControlRegister,
                              LegacyControl);
            }

            if ((LegacyControl & EHCI_LEGACY_SUPPORT_BIOS_OWNED) != 0) {

                //
                // If both the OS and BIOS owned bits are set, this is an
                // indication something more serious is wrong, or these are not
                // really EHCI registers.
                //

                ASSERT((LegacyControl & EHCI_LEGACY_SUPPORT_OS_OWNED) == 0);

                //
                // Write the "OS owned" bit to request that the BIOS stop
                // trying to be helpful and get out of the way.
                //

                LegacyControl |= EHCI_LEGACY_SUPPORT_OS_OWNED;
                BopWritePciConfig(LegacyInterrupt->Bus,
                                  LegacyInterrupt->Device,
                                  LegacyInterrupt->Function,
                                  LegacyControlRegister,
                                  sizeof(ULONG),
                                  LegacyControl);

                //
                // Wait for it to clear, or at least pretend to wait.
                //

                for (Try = 0; Try < EHCI_LEGACY_HANDOFF_SPIN_COUNT; Try += 1) {
                    LegacyControl = BopReadPciConfig(LegacyInterrupt->Bus,
                                                     LegacyInterrupt->Device,
                                                     LegacyInterrupt->Function,
                                                     LegacyControlRegister,
                                                     sizeof(ULONG));

                    if ((LegacyControl & EHCI_LEGACY_SUPPORT_BIOS_OWNED) == 0) {
                        break;
                    }
                }
            }

            break;

        case LegacyInterruptUhci:

            ASSERT(LegacyInterrupt->ControlRegister ==
                   UHCI_LEGACY_SUPPORT_REGISTER_OFFSET);

            if (BoDebugDebugDeviceExploration != FALSE) {
                RtlDebugPrint("Disabling UHCI interrupt at %x/%x/%x.\n",
                              LegacyInterrupt->Bus,
                              LegacyInterrupt->Device,
                              LegacyInterrupt->Function);
            }

            BopWritePciConfig(LegacyInterrupt->Bus,
                              LegacyInterrupt->Device,
                              LegacyInterrupt->Function,
                              UHCI_LEGACY_SUPPORT_REGISTER_OFFSET,
                              sizeof(USHORT),
                              UHCI_LEGACY_SUPPORT_ENABLE_USB_INTERRUPTS);

            break;

        case LegacyInterruptOhci:
            if (LegacyInterrupt->Base != (UINTN)(LegacyInterrupt->Base)) {
                break;
            }

            Base = (PVOID)(UINTN)(LegacyInterrupt->Base);

            //
            // If the interrupt routing is pointed at SMI, then ask the BIOS to
            // hand off control.
            //

            Control = HlReadRegister32(Base + OHCI_REGISTER_CONTROL);
            if ((Control & OHCI_CONTROL_INTERRUPT_ROUTING) != 0) {
                HlWriteRegister32(Base + OHCI_REGISTER_INTERRUPT_ENABLE,
                                  OHCI_INTERRUPT_OWNERSHIP_CHANGE);

                HlWriteRegister32(Base + OHCI_REGISTER_COMMAND_STATUS,
                                  OHCI_COMMAND_OWNERSHIP_CHANGE_REQUEST);

                do {
                    Control = HlReadRegister32(Base + OHCI_REGISTER_CONTROL);

                } while ((Control & OHCI_CONTROL_INTERRUPT_ROUTING) != 0);
            }

            //
            // Disable interrupts, and reset the bus.
            //

            HlWriteRegister32(Base + OHCI_REGISTER_INTERRUPT_DISABLE,
                              0xFFFFFFFF);

            if ((Control & OHCI_CONTROL_FUNCTIONAL_STATE_MASK) != 0) {
                HlWriteRegister32(Base + OHCI_REGISTER_CONTROL,
                                  Control & OHCI_CONTROL_REMOTE_WAKE_CONNECTED);

                HlReadRegister32(Base + OHCI_REGISTER_CONTROL);
            }

            //
            // Reset the controller, preserving the frame interval.
            //

            FrameInterval =
                         HlReadRegister32(Base + OHCI_REGISTER_FRAME_INTERVAL);

            HlWriteRegister32(Base + OHCI_REGISTER_COMMAND_STATUS,
                              OHCI_COMMAND_CONTROLLER_RESET);

            do {
                CommandStatus =
                         HlReadRegister32(Base + OHCI_REGISTER_COMMAND_STATUS);

            } while ((CommandStatus & OHCI_COMMAND_CONTROLLER_RESET) != 0);

            HlWriteRegister32(Base + OHCI_REGISTER_FRAME_INTERVAL,
                              FrameInterval);

            break;

        default:

            ASSERT(FALSE);

            break;
        }
    }

    BoHlTestUsbDebugInterface();
    return;
}

KSTATUS
BopExploreForDebugDevice (
    PDEBUG_PORT_TABLE2 *CreatedTable
    )

/*++

Routine Description:

    This routine performs architecture-specific actions to go hunting for a
    debug device.

Arguments:

    CreatedTable - Supplies a pointer where a pointer to a generated debug
        port table will be returned on success.

Return Value:

    Status code.

--*/

{

    GENERIC_ADDRESS Address;
    UCHAR Bus;
    UCHAR Class;
    ULONG ClassCode;
    PGENERATED_DEBUG_DEVICE DebugDevice;
    UCHAR Device;
    UINTN DeviceIndex;
    PFADT Fadt;
    BOOL FoundSomething;
    UCHAR Function;
    USHORT PortSubType;
    USHORT PortType;
    KSTATUS Status;
    USHORT Subclass;
    PGENERATED_DEBUG_PORT_TABLE2 Table;

    Bus = 0;
    Device = 0;
    FoundSomething = FALSE;
    Function = 0;
    PortType = 0;
    PortSubType = -1;
    INITIALIZE_LIST_HEAD(&BoLegacyInterruptList);
    Fadt = BoGetAcpiTable(FADT_SIGNATURE, NULL);

    //
    // Stop the debugger from stalling, as initializing the 8259 is going to
    // mask the timer interrupt backing stalls on BIOS machines.
    //

    KdSetConnectionTimeout(MAX_ULONG);

    //
    // Initialize and mask the 8259 PICs.
    //

    BopInitialize8259();

    //
    // Allocate the structure.
    //

    Table = BoAllocateMemory(sizeof(GENERATED_DEBUG_PORT_TABLE2));
    if (Table == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ExploreForDebugDeviceEnd;
    }

    RtlCopyMemory(Table,
                  &BoGeneratedDebugPortTableTemplate,
                  sizeof(GENERATED_DEBUG_PORT_TABLE2));

    Status = BopCheckForPcSerialPort(Fadt, Table);
    if (KSUCCESS(Status)) {
        FoundSomething = TRUE;
    }

    while (TRUE) {
        ClassCode = 0;
        Status = BopExplorePciForDebugDevice(&Bus,
                                             &Device,
                                             &Function,
                                             &ClassCode,
                                             &Address);

        if ((KSUCCESS(Status)) &&
            (Table->Table.DeviceInformationCount <
             GENERATED_DEBUG_DEVICE_COUNT)) {

            DeviceIndex = Table->Table.DeviceInformationCount;
            DebugDevice = &(Table->Device[DeviceIndex]);
            Class = (ClassCode >> 24) & 0xFF;
            Subclass = (ClassCode >> 8) & 0xFFFF;
            if (Class == PCI_CLASS_SERIAL_BUS) {
                PortType = DEBUG_PORT_TYPE_USB;
                switch (Subclass) {
                case PCI_CLASS_SERIAL_BUS_USB_EHCI:
                    PortSubType = DEBUG_PORT_USB_EHCI;
                    FoundSomething = TRUE;
                    RtlCopyMemory(DebugDevice,
                                  &BoGeneratedDebugDeviceTemplate,
                                  sizeof(GENERATED_DEBUG_DEVICE));

                    DebugDevice->Device.PortType = PortType;
                    DebugDevice->Device.PortSubType = PortSubType;
                    RtlCopyMemory(&(DebugDevice->Address),
                                  &Address,
                                  sizeof(GENERIC_ADDRESS));

                    Table->Table.Header.Length +=
                                                sizeof(GENERATED_DEBUG_DEVICE);

                    Table->Table.DeviceInformationCount += 1;
                    break;

                default:

                    ASSERT(FALSE);

                    Status = STATUS_INVALID_CONFIGURATION;
                    goto ExploreForDebugDeviceEnd;
                }

            } else if (Class == PCI_CLASS_SIMPLE_COMMUNICATION) {
                Status = BopCheckPotentialUartDebugDevice(Bus,
                                                          Device,
                                                          Function,
                                                          ClassCode,
                                                          &Address,
                                                          DebugDevice);

                if (KSUCCESS(Status)) {
                    Table->Table.Header.Length +=
                                            sizeof(GENERATED_DEBUG_DEVICE);

                    Table->Table.DeviceInformationCount += 1;
                }

                Status = STATUS_SUCCESS;
            }
        }

        if (!KSUCCESS(Status)) {
            break;
        }

        //
        // Advance the function to avoid finding the same device again and
        // again.
        //

        Function += 1;
    }

    BopSetAcpiTableChecksum(&(Table->Table.Header));
    Status = STATUS_NO_ELIGIBLE_DEVICES;
    if (FoundSomething != FALSE) {
        Status = STATUS_SUCCESS;
    }

ExploreForDebugDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Table != NULL) {
            BoFreeMemory(Table);
            Table = NULL;
        }
    }

    *CreatedTable = NULL;
    if (Table != NULL) {
        *CreatedTable = &(Table->Table);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
BopCheckForPcSerialPort (
    PFADT Fadt,
    PGENERATED_DEBUG_PORT_TABLE2 Table
    )

/*++

Routine Description:

    This routine looks for a PC serial port. It first checks the FADT to see if
    the device is definitely not there. It then checks the BIOS Data Area for
    the presence of a serial port.

Arguments:

    Fadt - Supplies an optional pointer to the FADT.

    Table - Supplies a pointer to the generate debug port table.

Return Value:

    STATUS_SUCCESS if one or more devices were added.

    STATUS_NO_ELIGIBLE_DEVICES if the serial port was not found.

--*/

{

    USHORT Com1;
    PGENERATED_DEBUG_DEVICE DebugDevice;
    PMEMORY_DESCRIPTOR Descriptor;
    UINTN DeviceIndex;

    //
    // If the FADT reports hardware reduced mode, then don't bother with
    // serial ports.
    //

    if ((Fadt != NULL) &&
        ((Fadt->Flags & FADT_FLAG_HARDWARE_REDUCED_ACPI) != 0)) {

        return STATUS_NO_ELIGIBLE_DEVICES;
    }

    //
    // If this is an EFI system, then there is no BIOS data area.
    //

    if (FwIsEfi() != FALSE) {
        return STATUS_NO_ELIGIBLE_DEVICES;
    }

    //
    // Look to see if the BIOS Data area is somewhere in the memory map. Don't
    // touch it if it's not mentioned, or is marked as something unexpected.
    //

    Descriptor = MmMdLookupDescriptor(&BoMemoryMap,
                                      BIOS_DATA_AREA,
                                      BIOS_DATA_AREA_SIZE);

    if ((Descriptor == NULL) ||
        ((Descriptor->Type != MemoryTypeFirmwareTemporary) &&
         (Descriptor->Type != MemoryTypeFirmwarePermanent) &&
         (Descriptor->Type != MemoryTypeReserved))) {

         return STATUS_NO_ELIGIBLE_DEVICES;
    }

    //
    // Okay, take a look at the value in the BIOS Data Area to see if there's
    // a COM port.
    //

    Com1 = *((PUSHORT)BIOS_DATA_AREA_COM1);

    //
    // Compare against expected values.
    //

    if ((Com1 != PCAT_COM1_BASE) && (Com1 != PCAT_COM2_BASE) &&
        (Com1 != PCAT_COM3_BASE) && (Com1 != PCAT_COM4_BASE)) {

        return STATUS_NO_ELIGIBLE_DEVICES;
    }

    //
    // The BIOS appears to be reporting a serial port. Return it.
    //

    DeviceIndex = Table->Table.DeviceInformationCount;
    if (DeviceIndex >= GENERATED_DEBUG_DEVICE_COUNT) {
        return STATUS_RESOURCE_IN_USE;
    }

    DebugDevice = &(Table->Device[DeviceIndex]);
    RtlCopyMemory(DebugDevice,
                  &BoGeneratedDebugDeviceTemplate,
                  sizeof(GENERATED_DEBUG_DEVICE));

    DebugDevice->Device.PortType = DEBUG_PORT_TYPE_SERIAL;
    DebugDevice->Device.PortSubType = DEBUG_PORT_SERIAL_16550;
    DebugDevice->Address.AddressSpaceId = AddressSpaceIo;
    DebugDevice->Address.RegisterBitWidth = 8;
    DebugDevice->Address.AccessSize = 1;
    DebugDevice->Address.Address = Com1;
    Table->Table.Header.Length += sizeof(GENERATED_DEBUG_DEVICE);
    Table->Table.DeviceInformationCount += 1;
    return STATUS_SUCCESS;
}

KSTATUS
BopCheckPotentialUartDebugDevice (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG DeviceClassCode,
    PGENERIC_ADDRESS Address,
    PGENERATED_DEBUG_DEVICE DebugDevice
    )

/*++

Routine Description:

    This routine checks the given device to determine if it a recognized
    UART controller suitable as a debug device.

Arguments:

    Bus - Supplies the bus number of the device.

    Device - Supplies the device number.

    Function - Supplies the function number.

    DeviceClassCode - Supplies the class code of the device.

    Address - Supplies a pointer to the configured base address of the device.

    DebugDevice - Supplies a pointer where the filled out generated debug
        device structure will be returned on success.

Return Value:

    Status code.

--*/

{

    USHORT DeviceId;
    GENERATED_DEBUG_OEM_DATA GeneratedOemData;
    PDEBUG_PORT_16550_OEM_DATA OemData;
    USHORT OemDataLength;
    KSTATUS Status;
    USHORT Subclass;
    ULONG VendorId;

    OemData = NULL;
    OemDataLength = 0;
    RtlZeroMemory(&GeneratedOemData, sizeof(GENERATED_DEBUG_OEM_DATA));
    VendorId = (ULONG)BopReadPciConfig(Bus,
                                       Device,
                                       Function,
                                       PCI_ID_OFFSET,
                                       sizeof(ULONG));

    DeviceId = (VendorId & PCI_DEVICE_ID_MASK) >> PCI_DEVICE_ID_SHIFT;
    VendorId &= PCI_VENDOR_ID_MASK;

    //
    // Handle the Intel Quark x1000, which needs special OEM data.
    //

    if ((VendorId == PCI_VENDOR_ID_INTEL) &&
        (DeviceId == PCI_DEVICE_ID_INTEL_QUARK_UART)) {

        if (BoDebugDebugDeviceExploration != FALSE) {
            RtlDebugPrint("Found Quark UART at 0x%I64x\n", Address->Address);
        }

        OemData = &(GeneratedOemData.Uart16550);
        OemDataLength = sizeof(DEBUG_PORT_16550_OEM_DATA);
        OemData->Signature = DEBUG_PORT_16550_OEM_DATA_SIGNATURE;
        OemData->BaseBaud = INTEL_QUARK_UART_BASE_BAUD;
        OemData->RegisterShift = INTEL_QUARK_UART_REGISTER_SHIFT;
        OemData->RegisterOffset = 0;
        OemData->Flags = DEBUG_PORT_16550_OEM_FLAG_64_BYTE_FIFO;
        Status = STATUS_SUCCESS;
        goto CheckPotentialUartDebugDeviceEnd;
    }

    //
    // If it claims to be a generic device, go with it.
    //

    Subclass = (DeviceClassCode >> 8) & 0xFFFF;
    if (Subclass == PCI_CLASS_SIMPLE_COMMUNICATION_16550) {
        if (BoDebugDebugDeviceExploration != FALSE) {
            RtlDebugPrint("Found Generic 16550 %04X:%04X at 0x%I64x\n",
                          VendorId,
                          DeviceId,
                          Address->Address);
        }

        Status = STATUS_SUCCESS;
        goto CheckPotentialUartDebugDeviceEnd;
    }

    //
    // The device is unknown.
    //

    RtlDebugPrint("Skipping Simple Communications device %04X:%04X, "
                  "class %08x\n",
                  VendorId,
                  DeviceId,
                  DeviceClassCode);

    Status = STATUS_NO_ELIGIBLE_DEVICES;

CheckPotentialUartDebugDeviceEnd:
    if (KSUCCESS(Status)) {
        RtlCopyMemory(DebugDevice,
                      &BoGeneratedDebugDeviceTemplate,
                      sizeof(GENERATED_DEBUG_DEVICE));

        if (OemData != NULL) {
            RtlCopyMemory(&(DebugDevice->OemData),
                          &GeneratedOemData,
                          sizeof(GENERATED_DEBUG_OEM_DATA));

            DebugDevice->Device.OemDataLength = OemDataLength;
            DebugDevice->Device.OemDataOffset =
                                 FIELD_OFFSET(GENERATED_DEBUG_DEVICE, OemData);
        }

        DebugDevice->Device.PortType = DEBUG_PORT_TYPE_SERIAL;
        DebugDevice->Device.PortSubType = DEBUG_PORT_SERIAL_16550_COMPATIBLE;
        RtlCopyMemory(&(DebugDevice->Address),
                      Address,
                      sizeof(GENERIC_ADDRESS));
    }

    return Status;
}

VOID
BopInitialize8259 (
    VOID
    )

/*++

Routine Description:

    This routine initializes the 8259 PIC, masking all interrupts.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PMADT Madt;

    //
    // If the MADT says there is no 8259, don't touch it.
    //

    Madt = BoGetAcpiTable(MADT_SIGNATURE, NULL);
    if ((Madt != NULL) &&
        ((Madt->Flags & MADT_FLAG_DUAL_8259) == 0)) {

        return;
    }

    //
    // Begin by remapping the 2 legacy 8259 interrupt controllers to vectors
    // 32-48. Upon initialization they're mapped to IRQ 0-15, however those are
    // also the vectors software exceptions come in on. Move them to avoid
    // interrupts that mean different things coming in on the same line.
    // Each 8259 is connected to 2 I/O ports, named A and B. To program the
    // controller, write 4 control words (ICW1-ICW4). The first is written to
    // port A, the rest are written to port B.
    //
    // ICW1:
    //     Bits 7-4: Reserved (set to 0001, the 1 identifies Init command).
    //     Bit 3: Trigger. 0 = Edge triggered, 1 = Level triggered.
    //     Bit 2: 0 = 8-byte interrupt vectors. 1 = 4 byte interrupt vectors.
    //     Bit 1: M/S. 0 = Master/Slave configuration. 1 = Master only.
    //     Bit 0: ICW4. 0 = No ICW4, 1 = ICW4 will be sent.
    //     Sane value: 0x11.
    //
    // ICW2:
    //     Bits 7-3: Offset into the IDT for interrupt service routines.
    //     Bits 2-0: Must be zero. Note this means that the IDT offset must be
    //         aligned to 8.
    //     Sane value: 0x20 for master, 0x28 for slave.
    //
    // ICW3 (Master):
    //     Bits 7-0: 1 if the interrupt line is connected to a slave 8259A.
    //         0 if connected to a peripheral device.
    //     Sane value for master: 0x04 (Slave connected to IRQ2).
    //
    // ICW3 (Slave):
    //     Bits 7-3: Reserved (set to 0).
    //     Bits 2-0: Specify the IRQ on the master this slave is connected to.
    //     Sane value for slave: 0x02 (Slave connected to IRQ2 on master).
    //
    // ICW4 (Optional):
    //     Bits 7-5: Reserved (set to 0).
    //     Bit 4: 1 if Speciall Fully Nested mode, 0 if not.
    //     Bit 3: 1 = Buffered mode. 0 = Nonbuffered mode.
    //     Bit 2: 1 = Master PIC, 0 = Slave PIC.
    //     Bit 1: 1 = Automatic EOI. 0 = Manual EOI.
    //     Bit 0: 1 = 8086/88 mode, 0 = MCS-80/85 Mode.
    //     Sane value for master: 0x01
    //     Sane value for slave: 0x01
    //
    // Program the first interrupt controller. Edge triggered, Master/Slave
    // configuration, ICW4 coming.
    //

    HlIoPortOutByte(PIC_8259_MASTER_COMMAND, 0x11);

    //
    // Program the interrupts to come in above IRQ0.
    //

    HlIoPortOutByte(PIC_8259_MASTER_DATA, VECTOR_SPURIOUS_INTERRUPT);

    //
    // Slave 8259 connected only on IRQ2.
    //

    HlIoPortOutByte(PIC_8259_MASTER_DATA, 0x04);

    //
    // Program ICW4 for not fully nested, nonbuffered mode, master PIC,
    // manual EOI, and 8086 mode.
    //

    HlIoPortOutByte(PIC_8259_MASTER_DATA, 0x01);

    //
    // Disable all interrupts from this controller.
    //

    HlIoPortOutByte(PIC_8259_MASTER_DATA, 0xFF);

    //
    // Program the second (slave) interrupt controller. Edge triggered,
    // master/slave configuration, ICW4 coming.
    //

    HlIoPortOutByte(PIC_8259_SLAVE_COMMAND, 0x11);

    //
    // Program ICW2: interrupts should come in right where the previous
    // controller left off.
    //

    HlIoPortOutByte(PIC_8259_SLAVE_DATA, VECTOR_SPURIOUS_INTERRUPT);

    //
    // Program ICW3: This controller is connected to IRQ2 on the master.
    //

    HlIoPortOutByte(PIC_8259_SLAVE_DATA, 0x02);

    //
    // Program ICW4: Not fully nested, non-buffered mode, slave PIC, manual
    // EOI, 8086 mode.
    //

    HlIoPortOutByte(PIC_8259_SLAVE_DATA, 0x01);

    //
    // Mask all interrupts on this controller by simply writing the mask to
    // port B.
    //

    HlIoPortOutByte(PIC_8259_SLAVE_DATA, 0xFF);
    return;
}

KSTATUS
BopExplorePciForDebugDevice (
    PUCHAR Bus,
    PUCHAR Device,
    PUCHAR Function,
    PULONG DeviceClassCode,
    PGENERIC_ADDRESS Address
    )

/*++

Routine Description:

    This routine scans the PCI bus looking for an eligible debug device. This
    routine does not configure bridges or busses, it assumes the firmware
    configured any eligible devices. For those thinking that perhaps this
    function could be expanded in scope, remember that the proper way to
    support a debug device is to report it in the debug port table. This
    routine only serves to bridge a gap in systems lacking that table.

Arguments:

    Bus - Supplies a pointer that on input specifies the bus to start the
        search from. On output, this value will be updated.

    Device - Supplies a pointer that on input specifies the device number to
        start the search from. On output, this value will be updated.

    Function - Supplies a pointer that on input specifies the function number
        to start the search from. On output, this value will be updated.

    DeviceClassCode - Supplies a pointer where the compatible device class code
        will be returned on success.

    Address - Supplies a pointer where the device base address will be
        returned.

Return Value:

    Status code.

--*/

{

    ULONG Bar;
    ULONG BarIndex;
    UCHAR Class;
    ULONG ClassCode;
    USHORT Control;
    USHORT DeviceId;
    BOOL FoundSomething;
    ULONG Id;
    KSTATUS Status;
    USHORT Subclass;
    USHORT VendorId;

    //
    // Scan the PCI bus. Only scan the first few buses to keep this from
    // taking forever.
    //

    if (BoDebugDebugDeviceExploration != FALSE) {
        RtlDebugPrint("Scanning PCI for debug devices.\n");
    }

    FoundSomething = FALSE;
    while (*Bus < BOOT_MAX_PCI_BUS) {
        while (*Device < MAX_PCI_DEVICE) {
            while (*Function <= MAX_PCI_FUNCTION) {
                Id = (ULONG)BopReadPciConfig(*Bus,
                                             *Device,
                                             *Function,
                                             PCI_ID_OFFSET,
                                             sizeof(ULONG));

                DeviceId = (Id & PCI_DEVICE_ID_MASK) >> PCI_DEVICE_ID_SHIFT;
                VendorId = Id & PCI_VENDOR_ID_MASK;
                ClassCode = (ULONG)BopReadPciConfig(*Bus,
                                                    *Device,
                                                    *Function,
                                                    PCI_CLASS_CODE_OFFSET,
                                                    sizeof(ULONG));

                ClassCode &= PCI_CLASS_CODE_MASK;
                Class = (ClassCode >> 24) & 0xFF;
                Subclass = (ClassCode >> 8) & 0xFFFF;
                Control = (USHORT)BopReadPciConfig(*Bus,
                                                   *Device,
                                                   *Function,
                                                   PCI_CONTROL_OFFSET,
                                                   sizeof(USHORT));

                if ((Id != 0xFFFFFFFF) && (Id != 0) &&
                    (BoDebugDebugDeviceExploration != FALSE)) {

                    RtlDebugPrint("BDF %X %X %X, Ven/Dev %04X/%04X, "
                                  "Class %X, Control %X\n",
                                  *Bus,
                                  *Device,
                                  *Function,
                                  VendorId,
                                  DeviceId,
                                  ClassCode,
                                  Control);
                }

                if ((VendorId != 0) && (VendorId != PCI_INVALID_VENDOR_ID) &&
                    ((Control &
                      (PCI_CONTROL_MEMORY_DECODE_ENABLED |
                       PCI_CONTROL_IO_DECODE_ENABLED)) != 0)) {

                    //
                    // If this is a supported device class, try to return it.
                    //

                    if ((Class == PCI_CLASS_SERIAL_BUS) &&
                        (Subclass == PCI_CLASS_SERIAL_BUS_USB_EHCI)) {

                        //
                        // Scan the BARs to look for the first enabled one.
                        //

                        for (BarIndex = 0;
                             BarIndex < PCI_BAR_COUNT;
                             BarIndex += 1) {

                            Bar = (ULONG)BopReadPciConfig(
                                   *Bus,
                                   *Device,
                                   *Function,
                                   PCI_BAR_OFFSET + (BarIndex * sizeof(ULONG)),
                                   sizeof(ULONG));

                            //
                            // If it's a 32-bit memory BAR, return it!
                            //

                            if ((Bar != 0) &&
                                ((Bar &
                                  (PCI_BAR_IO_SPACE | PCI_BAR_MEMORY_64_BIT)) ==
                                  0)) {

                                *DeviceClassCode = ClassCode;
                                Bar &= ~PCI_BAR_MEMORY_FLAGS_MASK;
                                if (FoundSomething == FALSE) {
                                    Address->AddressSpaceId =
                                                            AddressSpaceMemory;

                                    Address->RegisterBitWidth = 0;
                                    Address->RegisterBitOffset = 0;
                                    Address->AccessSize = 0;
                                    Address->Address = Bar;
                                }

                                if (BoDebugDebugDeviceExploration !=
                                    FALSE) {

                                    RtlDebugPrint("Found EHCI BAR %X: "
                                                  "Memory at %X\n",
                                                  BarIndex,
                                                  Bar);
                                }

                                FoundSomething = TRUE;
                                Status = BopCreateLegacyEhciInterrupt(
                                                             *Bus,
                                                             *Device,
                                                             *Function,
                                                             Bar);

                                if (!KSUCCESS(Status)) {
                                    return Status;
                                }
                            }
                        }

                    } else if ((Class == PCI_CLASS_SERIAL_BUS) &&
                               (Subclass == PCI_CLASS_SERIAL_BUS_USB_UHCI)) {

                        if (BoDebugDebugDeviceExploration != FALSE) {
                            RtlDebugPrint("Saw UHCI controller at B/D/F "
                                          "0x%X/0x%X/0x%X.\n",
                                          *Bus,
                                          *Device,
                                          *Function);
                        }

                        Status = BopCreateLegacyUhciInterrupt(*Bus,
                                                              *Device,
                                                              *Function);

                        if (!KSUCCESS(Status)) {
                            return Status;
                        }

                    } else if ((Class == PCI_CLASS_SERIAL_BUS) &&
                               (Subclass == PCI_CLASS_SERIAL_BUS_USB_OHCI)) {

                        if (BoDebugDebugDeviceExploration != FALSE) {
                            RtlDebugPrint("Saw OHCI controller at B/D/F "
                                          "0x%X/0x%X/0x%X.\n",
                                          *Bus,
                                          *Device,
                                          *Function);
                        }

                        Bar = (ULONG)BopReadPciConfig(*Bus,
                                                      *Device,
                                                      *Function,
                                                      PCI_BAR_OFFSET,
                                                      sizeof(ULONG));

                        if ((Bar != 0) &&
                            ((Bar &
                              (PCI_BAR_IO_SPACE | PCI_BAR_MEMORY_64_BIT)) ==
                              0)) {

                            Bar &= ~PCI_BAR_MEMORY_FLAGS_MASK;
                            Status = BopCreateLegacyOhciInterrupt(*Bus,
                                                                  *Device,
                                                                  *Function,
                                                                  Bar);

                            if (!KSUCCESS(Status)) {
                                return Status;
                            }
                        }

                    //
                    // Look for a PCI serial port.
                    //

                    } else if ((Class == PCI_CLASS_SIMPLE_COMMUNICATION) &&
                               ((Subclass ==
                                 PCI_CLASS_SIMPLE_COMMUNICATION_16550) ||
                                (Subclass ==
                                 PCI_CLASS_SIMPLE_COMMUNICATION_OTHER))) {

                        //
                        // Read the first BAR. If it's active, return it.
                        //

                        Bar = (ULONG)BopReadPciConfig(*Bus,
                                                      *Device,
                                                      *Function,
                                                      PCI_BAR_OFFSET,
                                                      sizeof(ULONG));

                        //
                        // If it's an active BAR, return it.
                        //

                        if ((Bar & ~PCI_BAR_MEMORY_FLAGS_MASK) != 0) {
                            *DeviceClassCode = ClassCode;
                            if (FoundSomething == FALSE) {
                                if ((Bar & PCI_BAR_IO_SPACE) != 0) {
                                    Address->AddressSpaceId = AddressSpaceIo;
                                    Address->Address =
                                                  Bar & ~PCI_BAR_IO_FLAGS_MASK;

                                } else {
                                    Address->AddressSpaceId =
                                                            AddressSpaceMemory;

                                    Address->Address =
                                              Bar & ~PCI_BAR_MEMORY_FLAGS_MASK;
                                }

                                Address->RegisterBitWidth = 0;
                                Address->RegisterBitOffset = 0;
                                Address->AccessSize = 0;
                            }

                            if (BoDebugDebugDeviceExploration !=
                                FALSE) {

                                RtlDebugPrint("Found Potential UART BAR %X\n",
                                              Bar);
                            }

                            FoundSomething = TRUE;
                        }
                    }
                }

                if (FoundSomething != FALSE) {
                    goto ExplorePciForDebugDeviceEnd;
                }

                *Function += 1;
            }

            if (*Device < MAX_PCI_DEVICE - 1) {
                *Function = 0;
            }

            *Device += 1;
        }

        if (*Bus < BOOT_MAX_PCI_BUS - 1) {
            *Device = 0;
            *Function = 0;
        }

        *Bus += 1;
    }

ExplorePciForDebugDeviceEnd:
    if (FoundSomething != FALSE) {
        return STATUS_SUCCESS;
    }

    if (BoDebugDebugDeviceExploration != FALSE) {
        RtlDebugPrint("Found no PCI debug device.\n");
    }

    return STATUS_NO_ELIGIBLE_DEVICES;
}

KSTATUS
BopCreateLegacyEhciInterrupt (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    PHYSICAL_ADDRESS Address
    )

/*++

Routine Description:

    This routine creates a legacy EHCI interrupt context structure.

Arguments:

    Bus - Supplies the bus number of the EHCI controller.

    Device - Supplies the device number of the EHCI controller.

    Function - Supplies the PCI function number of the EHCI controller.

    Address - Supplies the base address of the EHCI controller.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    ULONG Capabilities;
    PVOID CapabilitiesRegister;
    ULONG ExtendedCapabilitiesOffset;
    PLEGACY_INTERRUPT_CONTEXT Interrupt;

    CapabilitiesRegister = (PVOID)(UINTN)Address +
                           EHCI_CAPABILITY_CAPABILITIES_REGISTER;

    //
    // Read the capabilities register to get the offset of the extended
    // capabilities register.
    //

    Capabilities = HlReadRegister32(CapabilitiesRegister);
    ExtendedCapabilitiesOffset =
                  (Capabilities &
                   EHCI_CAPABILITY_CAPABILITIES_EXTENDED_CAPABILITIES_MASK) >>
                  EHCI_CAPABILITY_CAPABILITIES_EXTENDED_CAPABILITIES_SHIFT;

    //
    // If there is an extended capabilities register, save it so legacy
    // interrupts can be disabled after fully exiting boot services.
    //

    if (ExtendedCapabilitiesOffset != 0) {
        Interrupt = BoAllocateMemory(sizeof(LEGACY_INTERRUPT_CONTEXT));
        if (Interrupt == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(Interrupt, sizeof(LEGACY_INTERRUPT_CONTEXT));
        Interrupt->Type = LegacyInterruptEhci;
        Interrupt->Bus = Bus;
        Interrupt->Device = Device;
        Interrupt->Function = Function;
        Interrupt->ControlRegister = ExtendedCapabilitiesOffset;
        INSERT_BEFORE(&(Interrupt->ListEntry), &BoLegacyInterruptList);
    }

    return STATUS_SUCCESS;
}

KSTATUS
BopCreateLegacyOhciInterrupt (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    PHYSICAL_ADDRESS Address
    )

/*++

Routine Description:

    This routine creates a legacy OHCI interrupt context structure.

Arguments:

    Bus - Supplies the bus number of the OHCI controller.

    Device - Supplies the device number of the OHCI controller.

    Function - Supplies the PCI function number of the OHCI controller.

    Address - Supplies the base address of the OHCI controller.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    PLEGACY_INTERRUPT_CONTEXT Interrupt;

    Interrupt = BoAllocateMemory(sizeof(LEGACY_INTERRUPT_CONTEXT));
    if (Interrupt == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Interrupt, sizeof(LEGACY_INTERRUPT_CONTEXT));
    Interrupt->Type = LegacyInterruptOhci;
    Interrupt->Bus = Bus;
    Interrupt->Device = Device;
    Interrupt->Function = Function;
    Interrupt->Base = Address;
    INSERT_BEFORE(&(Interrupt->ListEntry), &BoLegacyInterruptList);
    return STATUS_SUCCESS;
}

KSTATUS
BopCreateLegacyUhciInterrupt (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function
    )

/*++

Routine Description:

    This routine creates a legacy UHCI interrupt context structure.

Arguments:

    Bus - Supplies the bus number of the UHCI controller.

    Device - Supplies the device number of the UHCI controller.

    Function - Supplies the PCI function number of the UHCI controller.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    PLEGACY_INTERRUPT_CONTEXT Interrupt;

    Interrupt = BoAllocateMemory(sizeof(LEGACY_INTERRUPT_CONTEXT));
    if (Interrupt == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Interrupt, sizeof(LEGACY_INTERRUPT_CONTEXT));
    Interrupt->Type = LegacyInterruptUhci;
    Interrupt->Bus = Bus;
    Interrupt->Device = Device;
    Interrupt->Function = Function;
    Interrupt->ControlRegister = UHCI_LEGACY_SUPPORT_REGISTER_OFFSET;
    INSERT_BEFORE(&(Interrupt->ListEntry), &BoLegacyInterruptList);
    return STATUS_SUCCESS;
}

ULONGLONG
BopReadPciConfig (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG Register,
    ULONG AccessSize
    )

/*++

Routine Description:

    This routine reads from PCI Configuration Space on the root PCI bus.

Arguments:

    Bus - Supplies the bus number to read from.

    Device - Supplies the device number to read from. Valid values are 0 to 31.

    Function - Supplies the PCI function to read from. Valid values are 0 to 7.

    Register - Supplies the configuration register to read from.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

Return Value:

    Returns the value read from the bus, or 0xFFFFFFFF on error.

--*/

{

    ULONG Address;
    ULONGLONG Value;

    //
    // Create the configuration address and write it into the address port.
    //

    Address = PCI_CONFIG_ADDRESS(Bus, Device, Function, Register);
    HlIoPortOutLong(PCI_ROOT_CONFIG_ADDRESS, Address);

    //
    // Read the data at that address.
    //

    switch (AccessSize) {
    case sizeof(UCHAR):
        Value = HlIoPortInByte(PCI_ROOT_CONFIG_DATA);
        break;

    case sizeof(USHORT):
        Value = HlIoPortInShort(PCI_ROOT_CONFIG_DATA);
        break;

    case sizeof(ULONG):
        Value = HlIoPortInLong(PCI_ROOT_CONFIG_DATA);
        break;

    case sizeof(ULONGLONG):
        Value = HlIoPortInLong(PCI_ROOT_CONFIG_DATA);
        HlIoPortOutLong(PCI_ROOT_CONFIG_ADDRESS, Address + 4);
        Value |= ((ULONGLONG)HlIoPortInLong(PCI_ROOT_CONFIG_DATA)) << 32;
        break;

    default:

        ASSERT(FALSE);

        Value = -1;
        break;
    }

    return Value;
}

VOID
BopWritePciConfig (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG Register,
    ULONG AccessSize,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine writes to PCI Configuration Space on the PCI root bus.

Arguments:

    Bus - Supplies the bus number to write to.

    Device - Supplies the device number to write to. Valid values are 0 to 31.

    Function - Supplies the PCI function to write to. Valid values are 0 to 7.

    Register - Supplies the configuration register to write to.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

    Value - Supplies the value to write to the register.

Return Value:

    None.

--*/

{

    ULONG Address;

    //
    // Create the configuration address and write it into the address port.
    //

    Address = PCI_CONFIG_ADDRESS(Bus, Device, Function, Register);
    HlIoPortOutLong(PCI_ROOT_CONFIG_ADDRESS, Address);

    //
    // Write the data at that address.
    //

    switch (AccessSize) {
    case sizeof(UCHAR):
        HlIoPortOutByte(PCI_ROOT_CONFIG_DATA, (UCHAR)Value);
        break;

    case sizeof(USHORT):
        HlIoPortOutShort(PCI_ROOT_CONFIG_DATA, (USHORT)Value);
        break;

    case sizeof(ULONG):
        HlIoPortOutLong(PCI_ROOT_CONFIG_DATA, (ULONG)Value);
        break;

    case sizeof(ULONGLONG):
        HlIoPortOutLong(PCI_ROOT_CONFIG_DATA, (ULONG)Value);
        HlIoPortOutLong(PCI_ROOT_CONFIG_ADDRESS, Address + 4);
        HlIoPortOutLong(PCI_ROOT_CONFIG_DATA, (ULONG)(Value >> 32));
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
BopSetAcpiTableChecksum (
    PDESCRIPTION_HEADER Header
    )

/*++

Routine Description:

    This routine sets the correct checksum on an ACPI table.

Arguments:

    Header - Supplies a pointer to the common ACPI table header.

Return Value:

    None.

--*/

{

    PUCHAR Buffer;
    ULONG ByteIndex;
    UCHAR Sum;

    Header->Checksum = 0;
    Sum = 0;
    Buffer = (PUCHAR)Header;
    for (ByteIndex = 0; ByteIndex < Header->Length; ByteIndex += 1) {
        Sum += Buffer[ByteIndex];
    }

    Header->Checksum = (UCHAR)(0x100 - Sum);
    return;
}

