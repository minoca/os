/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ahci.h

Abstract:

    This header contains definitions for the Advance Host Controller Interface
    (AHCI), a SATA storage host controller.

Author:

    Evan Green 15-Nov-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read from and write to device global registers.
//

#define AHCI_READ_GLOBAL(_Controller, _Register) \
    HlReadRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register))

#define AHCI_WRITE_GLOBAL(_Controller, _Register, _Value)                  \
    HlWriteRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register), \
                      (_Value))

//
// These macros read from and write to port specific registers.
//

#define AHCI_READ(_Port, _Register) \
    HlReadRegister32((PUCHAR)(_Port)->PortBase + (_Register))

#define AHCI_WRITE(_Port, _Register, _Value)                               \
    HlWriteRegister32((PUCHAR)(_Port)->PortBase + (_Register),             \
                      (_Value))

//
// These macros help manipulate FIS fields that are spread out.
//

#define SATA_SET_FIS_LBA(_Fis, _Lba)        \
    (_Fis)->Lba0 = (UCHAR)(_Lba),           \
    (_Fis)->Lba1 = (UCHAR)((_Lba) >> 8),    \
    (_Fis)->Lba2 = (UCHAR)((_Lba) >> 16),   \
    (_Fis)->Lba3 = (UCHAR)((_Lba) >> 24),   \
    (_Fis)->Lba4 = (UCHAR)((_Lba) >> 32),   \
    (_Fis)->Lba5 = (UCHAR)((_Lba) >> 40)

#define SATA_GET_FIS_LBA(_Fis)              \
    ((ULONGLONG)((_Fis)->Lba0) |            \
     ((ULONGLONG)((_Fis)->Lba1) << 8) |     \
     ((ULONGLONG)((_Fis)->Lba2) << 16) |    \
     ((ULONGLONG)((_Fis)->Lba3) << 24) |    \
     ((ULONGLONG)((_Fis)->Lba4) << 32) |    \
     ((ULONGLONG)((_Fis)->Lba5) << 40))

#define SATA_SET_FIS_COUNT(_Fis, _Count)    \
    (_Fis)->Count0 = (UCHAR)(_Count),       \
    (_Fis)->Count1 = (UCHAR)((_Count) >> 8)

#define SATA_GET_FIS_COUNT(_Fis)            \
    ((_Fis)->Count0 | ((_Fis)->Count1 << 8))

//
// This macro determines the value to put in the CFL (command FIS length) of
// the command header control member given a size in bytes.
//

#define AHCI_COMMAND_FIS_SIZE(_Size) ((_Size) / sizeof(ULONG))

//
// This macro gets the error register out of the task file register.
//

#define AHCI_PORT_TASK_GET_ERROR(_TaskFile) (((_TaskFile) >> 8) & 0xFF)

//
// ---------------------------------------------------------------- Definitions
//

#define AHCI_ALLOCATION_TAG 0x69636841

//
// Define the maximum number of AHCI ports in a controller, as defined by the
// hardware spec.
//

#define AHCI_PORT_COUNT 32

//
// Define the maximum number of command headers, as defined by the spec.
//

#define AHCI_COMMAND_COUNT 32

//
// Define the amount of time to wait for the PHY to come up on a device, in
// milliseconds. The spec is 10 milliseconds.
//

#define AHCI_PHY_DETECT_TIMEOUT_MS 25

#define AHCI_COMMAND_TABLE_ALIGNMENT 128
#define AHCI_RECEIVE_FIS_MAX_SIZE 0x1000

#define AHCI_PORT_REGISTER_OFFSET 0x80

//
// Define the maximum number of PRDT entries in a command table. This works out
// such that the whole command table structure takes half a page.
//

#define AHCI_PRDT_COUNT 120

//
// Define the maximum size of a PRDT entry.
//

#define AHCI_PRDT_MAX_SIZE 0x400000

//
// Define software AHCI port flags.
//

//
// This bit is set if the device supports LBA48 style block addressing.
//

#define AHCI_PORT_LBA48 0x00000001

//
// This bit is set if native command queuing is enabled.
//

#define AHCI_PORT_NATIVE_COMMAND_QUEUING 0x00000002

//
// Host capabilities register bits.
//

#define AHCI_HOST_CAPABILITY_PORT_COUNT_MASK 0x0000000F
#define AHCI_HOST_CAPABILITY_EXTERNAL_SATA 0x00000020
#define AHCI_HOST_CAPABILITY_ENCLOSURE_MANAGEMENT 0x00000040
#define AHCI_HOST_CAPABILITY_COALESCING 0x00000080
#define AHCI_HOST_CAPABILITY_COMMAND_SLOTS_SHIFT 8
#define AHCI_HOST_CAPABILITY_COMMAND_SLOTS_MASK (0x3F << 8)
#define AHCI_HOST_CAPABILITY_PARTIAL 0x00002000
#define AHCI_HOST_CAPABILITY_SLUMBER 0x00004000
#define AHCI_HOST_CAPABILITY_PIO_MULTIPLE 0x00008000
#define AHCI_HOST_CAPABILITY_FIS_BASED_SWITCHING 0x00010000
#define AHCI_HOST_CAPABILITY_PORT_MULTIPLIER 0x00020000
#define AHCI_HOST_CAPABILITY_AHCI_ONLY 0x00040000
#define AHCI_HOST_CAPABILITY_SPEED_MASK 0x00F00000
#define AHCI_HOST_CAPABILITY_COMMAND_LIST_OVERRIDE 0x01000000
#define AHCI_HOST_CAPABILITY_ACTIVITY_LED 0x02000000
#define AHCI_HOST_CAPABILITY_ALPM 0x04000000
#define AHCI_HOST_CAPABILITY_STAGGERED_SPINUP 0x08000000
#define AHCI_HOST_CAPABILITY_MECHANICAL_PRESENCE 0x10000000
#define AHCI_HOST_CAPABILITY_SNOTIFICATION 0x20000000
#define AHCI_HOST_CAPABILITY_NATIVE_QUEUING 0x40000000
#define AHCI_HOST_CAPABILITY_64BIT 0x80000000

//
// Global host control register bits
//

#define AHCI_HOST_CONTROL_RESET 0x00000001
#define AHCI_HOST_CONTROL_INTERRUPT_ENABLE 0x00000002
#define AHCI_HOST_CONTROL_MSI_SINGLE_MESSAGE 0x00000004
#define AHCI_HOST_CONTROL_ENABLE 0x80000000

//
// Host capabilities 2 register bits.
//

#define AHCI_HOST_CAPABILITY2_BIOS_HANDOFF 0x00000001
#define AHCI_HOST_CAPABILITY2_NVM_HCI_PRESENT 0x00000002
#define AHCI_HOST_CAPABILITY2_AUTO_PARTIAL_TO_SLUMBER 0x00000004
#define AHCI_HOST_CAPABILITY2_SLEEP_FROM_SLUMBER_ONLY 0x00000020
#define AHCI_HOST_CAPABILITY2_DEVICE_SLEEP 0x00000008
#define AHCI_HOST_CAPABILITY2_AGGRESSIVE_SLEEP_MANAGEMENT 0x00000010

//
// BIOS/OS handoff register bits.
//

#define AHCI_BIOS_HANDOFF_BIOS_OWNED 0x00000001
#define AHCI_BIOS_HANDOFF_OS_OWNED 0x00000002
#define AHCI_BIOS_HANDOFF_SMI_ON_CHANGE 0x00000004
#define AHCI_BIOS_HANDOFF_OS_OWNERSHIP_CHANGE 0x00000008
#define AHCI_BIOS_HANDOFF_BIOS_BUSY 0x00000010

//
// Port interrupt status/enable register bits.
//

#define AHCI_INTERRUPT_D2H_REGISTER_FIS 0x00000001
#define AHCI_INTERRUPT_PIO_SETUP_FIS 0x00000002
#define AHCI_INTERRUPT_DMA_SETUP_FIS 0x00000004
#define AHCI_INTERRUPT_SET_DEVICE_BITS 0x00000008
#define AHCI_INTERRUPT_UNKNOWN_FIS 0x00000010
#define AHCI_INTERRUPT_DESCRIPTOR_PROCESSED 0x00000020
#define AHCI_INTERRUPT_PORT_CONNECT_CHANGE 0x00000040
#define AHCI_INTERRUPT_MECHANICAL_PRESENCE_CHANGE 0x00000080
#define AHCI_INTERRUPT_PHY_READY_CHANGE 0x00400000
#define AHCI_INTERRUPT_INCORRECT_PORT_MULTIPLIER 0x00800000
#define AHCI_INTERRUPT_OVERFLOW 0x01000000
#define AHCI_INTERRUPT_NON_FATAL_ERROR 0x04000000
#define AHCI_INTERRUPT_FATAL_ERROR 0x08000000
#define AHCI_INTERRUPT_HOST_BUS_DATA_ERROR 0x10000000
#define AHCI_INTERRUPT_HOST_BUS_FATAL_ERROR 0x20000000
#define AHCI_INTERRUPT_TASK_FILE_ERROR 0x40000000
#define AHCI_INTERRUPT_COLD_PORT_DETECT 0x80000000

#define AHCI_INTERRUPT_DEFAULT_ENABLE \
    (AHCI_INTERRUPT_D2H_REGISTER_FIS | \
     AHCI_INTERRUPT_PIO_SETUP_FIS | \
     AHCI_INTERRUPT_DMA_SETUP_FIS | \
     AHCI_INTERRUPT_SET_DEVICE_BITS | \
     AHCI_INTERRUPT_UNKNOWN_FIS | \
     AHCI_INTERRUPT_DESCRIPTOR_PROCESSED | \
     AHCI_INTERRUPT_PORT_CONNECT_CHANGE | \
     AHCI_INTERRUPT_MECHANICAL_PRESENCE_CHANGE | \
     AHCI_INTERRUPT_PHY_READY_CHANGE | \
     AHCI_INTERRUPT_INCORRECT_PORT_MULTIPLIER | \
     AHCI_INTERRUPT_OVERFLOW | \
     AHCI_INTERRUPT_NON_FATAL_ERROR | \
     AHCI_INTERRUPT_FATAL_ERROR | \
     AHCI_INTERRUPT_HOST_BUS_DATA_ERROR | \
     AHCI_INTERRUPT_HOST_BUS_FATAL_ERROR | \
     AHCI_INTERRUPT_TASK_FILE_ERROR | \
     AHCI_INTERRUPT_TASK_FILE_ERROR)

#define AHCI_INTERRUPT_CONNECTION_MASK \
    (AHCI_INTERRUPT_PORT_CONNECT_CHANGE | \
     AHCI_INTERRUPT_MECHANICAL_PRESENCE_CHANGE | \
     AHCI_INTERRUPT_PHY_READY_CHANGE | \
     AHCI_INTERRUPT_COLD_PORT_DETECT)

#define AHCI_INTERRUPT_ERROR_MASK \
    (AHCI_INTERRUPT_INCORRECT_PORT_MULTIPLIER | \
     AHCI_INTERRUPT_OVERFLOW | \
     AHCI_INTERRUPT_NON_FATAL_ERROR | \
     AHCI_INTERRUPT_FATAL_ERROR | \
     AHCI_INTERRUPT_HOST_BUS_DATA_ERROR | \
     AHCI_INTERRUPT_HOST_BUS_FATAL_ERROR | \
     AHCI_INTERRUPT_TASK_FILE_ERROR)

//
// Port command/status register bits.
//

#define AHCI_PORT_COMMAND_START 0x00000001
#define AHCI_PORT_COMMAND_SPIN_UP_DEVICE 0x00000002
#define AHCI_PORT_COMMAND_POWER_ON_DEVICE 0x00000004
#define AHCI_PORT_COMMAND_COMMAND_LIST_OVERRIDE 0x00000008
#define AHCI_PORT_COMMAND_FIS_RX_ENABLE 0x00000010
#define AHCI_PORT_COMMAND_CURRENT_SLOT_SHIFT 8
#define AHCI_PORT_COMMAND_CURRENT_SLOT_MASK (0x1F << 8)
#define AHCI_PORT_COMMAND_MECHANICAL_SWITCH_STATE 0x00002000
#define AHCI_PORT_COMMAND_FIS_RX_RUNNING 0x00004000
#define AHCI_PORT_COMMAND_LIST_RUNNING 0x00008000
#define AHCI_PORT_COMMAND_COLD_PRESENCE_STATE 0x00010000
#define AHCI_PORT_COMMAND_PORT_MULTIPLIER 0x00020000
#define AHCI_PORT_COMMAND_HOT_PLUG_CAPABLE 0x00040000
#define AHCI_PORT_COMMAND_MECHANICAL_SWITCH_ATTACHED 0x00080000
#define AHCI_PORT_COMMAND_COLD_PRESENCE_DETECTION 0x00100000
#define AHCI_PORT_COMMAND_EXTERNAL_SATA 0x00200000
#define AHCI_PORT_COMMAND_FIS_SWITCHING_CAPABLE 0x00400000
#define AHCI_PORT_COMMAND_AUTO_PARTIAL_TO_SLUMBER 0x00800000
#define AHCI_PORT_COMMAND_ATAPI 0x01000000
#define AHCI_PORT_COMMAND_ATAPI_DRIVE_LED 0x02000000
#define AHCI_PORT_COMMAND_AGGRESSIVE_LINK_POWER_MANAGEMENT 0x04000000
#define AHCI_PORT_COMMAND_AGGRESSIVE_SLUMBER_PARTIAL 0x08000000
#define AHCI_PORT_COMMAND_NOP (0x0 << 28)
#define AHCI_PORT_COMMAND_ACTIVE (0x1 << 28)
#define AHCI_PORT_COMMAND_PARTIAL (0x2 << 28)
#define AHCI_PORT_COMMAND_SLUMBER (0x6 << 28)
#define AHCI_PORT_COMMAND_SLEEP (0x8 << 28)

//
// Port task file data register bits.
//

#define AHCI_PORT_TASK_ERROR 0x00000001
#define AHCI_PORT_TASK_DATA_REQUEST 0x00000008
#define AHCI_PORT_TASK_BUSY 0x00000080

#define AHCI_PORT_TASK_ERROR_MASK \
    (AHCI_PORT_TASK_ERROR | AHCI_PORT_TASK_DATA_REQUEST | AHCI_PORT_TASK_BUSY)

//
// Port SATA status register bits.
//

#define AHCI_PORT_SATA_STATUS_POWER_SHIFT 8
#define AHCI_PORT_SATA_STATUS_POWER_MASK (0xF << 8)
#define AHCI_PORT_SATA_STATUS_POWER_NONE (0x0 << 8)
#define AHCI_PORT_SATA_STATUS_POWER_ACTIVE (0x1 << 8)
#define AHCI_PORT_SATA_STATUS_POWER_PARTIAL (0x2 << 8)
#define AHCI_PORT_SATA_STATUS_POWER_SLUMBER (0x6 << 8)
#define AHCI_PORT_SATA_STATUS_POWER_SLEEP (0x8 << 8)
#define AHCI_PORT_SATA_STATUS_SPEED_SHIFT 4
#define AHCI_PORT_SATA_STATUS_SPEED_MASK (0xF << 4)
#define AHCI_PORT_SATA_STATUS_SPEED_NONE (0x0 << 4)
#define AHCI_PORT_SATA_STATUS_SPEED_GENERATION_1 (0x1 << 4)
#define AHCI_PORT_SATA_STATUS_SPEED_GENERATION_2 (0x2 << 4)
#define AHCI_PORT_SATA_STATUS_SPEED_GENERATION_3 (0x3 << 4)
#define AHCI_PORT_SATA_STATUS_DETECTION_MASK 0x0000000F
#define AHCI_PORT_SATA_STATUS_DETECTION_NONE 0x00000000
#define AHCI_PORT_SATA_STATUS_DETECTION_NO_PHY 0x00000001
#define AHCI_PORT_SATA_STATUS_DETECTION_PHY 0x00000003
#define AHCI_PORT_SATA_STATUS_DETECTION_OFFLINE 0x00000004

//
// Port SATA control register bits.
//

#define AHCI_PORT_SATA_CONTROL_DETECTION_MASK 0x0000000F
#define AHCI_PORT_SATA_CONTROL_DETECTION_NOP 0x00000000
#define AHCI_PORT_SATA_CONTROL_DETECTION_COMRESET 0x00000001
#define AHCI_PORT_SATA_CONTROL_DETECTION_OFFLINE 0x00000004
#define AHCI_PORT_SATA_CONTROL_DETECTION_SPEED_MASK (0xF << 4)
#define AHCI_PORT_SATA_CONTROL_DETECTION_POWER_MASK (0xF << 8)

//
// Port SATA error register bits.
//

#define AHCI_PORT_SATA_ERROR_RECOVERED_DATA_INTEGRITY 0x00000001
#define AHCI_PORT_SATA_ERROR_RECOVERED_COMMUNICATIONS 0x00000002
#define AHCI_PORT_SATA_ERROR_TRANSIENT_DATA_INTEGRITY 0x00000100
#define AHCI_PORT_SATA_ERROR_PERSISTENT 0x00000200
#define AHCI_PORT_SATA_ERROR_PROTOCOL 0x00000400
#define AHCI_PORT_SATA_ERROR_INTERNAL 0x00000800
#define AHCI_PORT_SATA_ERROR_PHY_READY_CHAGNE 0x00010000
#define AHCI_PORT_SATA_ERROR_PHY_INTERNAL_ERROR 0x00020000
#define AHCI_PORT_SATA_ERROR_COMM_WAKE 0x00040000
#define AHCI_PORT_SATA_ERROR_10B_8B_DECODE 0x00080000
#define AHCI_PORT_SATA_ERROR_DISPARITY 0x00100000
#define AHCI_PORT_SATA_ERROR_CRC 0x00200000
#define AHCI_PORT_SATA_ERROR_HANDSHAKE 0x00400000
#define AHCI_PORT_SATA_ERROR_LINK_SEQUENCE 0x00800000
#define AHCI_PORT_SATA_ERROR_TRANSPORT_STATE_TRANSITIO 0x01000000
#define AHCI_PORT_SATA_ERROR_UNKNOWN_FIS 0x02000000
#define AHCI_PORT_SATA_ERROR_EXCHANGED 0x04000000

//
// Port FIS-based switching control register bits.
//

#define AHCI_PORT_FIS_SWITCH_ENABLE 0x00000001
#define AHCI_PORT_FIS_SWITCH_DEVICE_ERROR_CLEAR 0x00000002
#define AHCI_PORT_FIS_SWITCH_SINGLE_DEVICE_ERROR 0x00000004
#define AHCI_PORT_FIS_SWITCH_DEVICE_SHIFT 8
#define AHCI_PORT_FIS_SWITCH_ACTIVE_DEVICE_OPTIMIZATION_SHIFT 12
#define AHCI_PORT_FIS_SWITCH_DEVICE_WITH_ERROR_SHIFT 16

//
// Port sleep register bits.
//

#define AHCI_PORT_SLEEP_AGGRESSIVE_SLEEP_ENABLE 0x00000001
#define AHCI_PORT_SLEEP_DEVICE_SLEEP_PRESENT 0x00000002
#define AHCI_PORT_SLEEP_EXIT_TIMEOUT_SHIFT 2
#define AHCI_PORT_SLEEP_MINIMUM_TIME_SHIFT 10
#define AHCI_PORT_SLEEP_IDLE_TIMEOUT_SHIFT 15
#define AHCI_PORT_SLEEP_IDLE_MULTIPLIER_SHIFT 25

//
// AHCI command header control flags.
//

#define AHCI_COMMAND_HEADER_ATAPI 0x0020
#define AHCI_COMMAND_HEADER_WRITE 0x0040
#define AHCI_COMMAND_HEADER_PREFETCHABLE 0x0080
#define AHCI_COMMAND_HEADER_RESET 0x0100
#define AHCI_COMMAND_HEADER_BIST 0x0200
#define AHCI_COMMAND_HEADER_CLEAR_BUSY_ON_OK 0x0400
#define AHCI_COMMAND_HEADER_PORT_MULTIPLIER_SHIFT 12

//
// Set this bit in the count member to interrupt on completion.
//

#define AHCI_PRDT_INTERRUPT 0x80000000

//
// SATA FIS flags.
//

#define SATA_FIS_REGISTER_H2D_FLAG_COMMAND 0x80

#define SATA_FIS_REGISTER_D2H_FLAG_INTERRUPT 0x40

#define SATA_FIS_SET_DEVICE_BITS_FLAG_INTERRUPT 0x40
#define SATA_FIS_SET_DEVICE_BITS_FLAG_NOTIFICATION 0x80

#define SATA_FIS_DMA_SETUP_FLAG_DIRECTION 0x20
#define SATA_FIS_DMA_SETUP_FLAG_INTERRUPT 0x40
#define SATA_FIS_DMA_SETUP_FLAG_AUTO_ACTIVATE 0x80

#define SATA_FIS_PIO_SETUP_FLAG_INTERRUPT 0x40

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _AHCI_CONTEXT_TYPE {
    AhciContextInvalid,
    AhciContextController,
    AhciContextPort
} AHCI_CONTEXT_TYPE, *PAHCI_CONTEXT_TYPE;

typedef enum _SATA_FIS_TYPE {
    SataFisRegisterH2d = 0x27,
    SataFisRegisterD2h = 0x34,
    SataFisDmaActivate = 0x39,
    SataFisDmaSetup = 0x41,
    SataFisData = 0x46,
    SataFisBistActivate = 0x58,
    SataFisPioSetup = 0x5F,
    SataFisSetDeviceBits = 0xA1
} SATA_FIS_TYPE, *PSATA_FIS_TYPE;

typedef enum _AHCI_CONTROLLER_REGISTER {
    AhciHostCapabilities = 0x00,
    AhciHostControl = 0x04,
    AhciInterruptStatus = 0x08,
    AhciPortsImplemented = 0x0C,
    AhciVersion = 0x10,
    AhciCoalescingControl = 0x14,
    AhciCoalescingPorts = 0x18,
    AhciEnclosureLocation = 0x1C,
    AhciEnclosureControl = 0x20,
    AhciHostCapabilities2 = 0x24,
    AhciBiosHandoff = 0x28,
    AhciPortCommandListBase = 0x100,
    AhciPortCommandListBaseHigh = 0x104,
    AhciPortFisBase = 0x108,
    AhciPortFisBaseHigh = 0x10C,
    AhciPortInterruptStatus = 0x110,
    AhciPortInterruptEnable = 0x114,
    AhciPortCommand = 0x118,
    AhciPortTaskFile = 0x120,
    AhciPortSignature = 0x124,
    AhciPortSataStatus = 0x128,
    AhciPortSataControl = 0x12C,
    AhciPortSataError = 0x130,
    AhciPortSataActive = 0x134,
    AhciPortCommandIssue = 0x138,
    AhciPortSataNotification = 0x13C,
    AhciPortFisSwitchingControl = 0x140,
    AhciPortDeviceSleep = 0x144,
} AHCI_CONTROLLER_REGISTER, *PAHCI_CONTROLLER_REGISTER;

typedef struct _AHCI_CONTROLLER AHCI_CONTROLLER, *PAHCI_CONTROLLER;

/*++

Structure Description:

    This structure defines the host to device register FIS structure, defined
    by the SATA spec.

Members:

    Type - Stores the constant SataFisRegisterH2d.

    Flags - Stores the port multiplier port and some other flags. See
        SATA_FIS_REGISTER_H2D_FLAG_* definitions.

    Command - Stores the contents of the command register of the shadow
        register block.

    FeaturesLow - Stores the low 8 bits of the features register of the shadow
        register block.

    Lba0 - Stores bits 7-0 of the LBA.

    Lba1 - Stores bits 15-8 of the LBA.

    Lba2 - Stores bits 23-16 of the LBA.

    Device - Stores the contents of the device register in the shadow register
        block.

    Lba3 - Stores bits 31-24 of the LBA.

    Lba4 - Stores bits 39-32 of the LBA.

    Lba5 - Stores bits 47-40 of the LBA.

    FeaturesHigh - Stores the high 8 bits of the features register of the
        shadow register block.

    Count0 - Stores the low 8 bits of the sector count.

    Count1 - Stores the high 8 bits of the sector count.

    Icc - Stores the Isochronous Command Complete value, which is a value set
        by the host to inform the device of a time limit.

    Control - Stores the contents of the device control register of the shadow
        register block.

    Reserved - Stores an extra few bytes for padding. This should be set to
        zero.

--*/

#pragma pack(push, 1)

typedef struct _SATA_FIS_REGISTER_H2D {
    UCHAR Type;
    UCHAR Flags;
    UCHAR Command;
    UCHAR FeaturesLow;
    UCHAR Lba0;
    UCHAR Lba1;
    UCHAR Lba2;
    UCHAR Device;
    UCHAR Lba3;
    UCHAR Lba4;
    UCHAR Lba5;
    UCHAR FeaturesHigh;
    UCHAR Count0;
    UCHAR Count1;
    UCHAR Icc;
    UCHAR Control;
    ULONG Reserved;
} PACKED SATA_FIS_REGISTER_H2D, *PSATA_FIS_REGISTER_H2D;

/*++

Structure Description:

    This structure defines the device to host register FIS structure, defined
    by the SATA spec.

Members:

    Type - Stores the constant SataFisRegisterD2h.

    Flags - Stores the port multiplier port and some other flags. See
        SATA_FIS_REGISTER_D2H_FLAG_* definitions.

    Status - Stores the new value of the status (and alternate status) register
        of the shadow register block.

    Error - Stores the new value of the Error register of the shadow register
        block.

    Lba0 - Stores bits 7-0 of the LBA.

    Lba1 - Stores bits 15-8 of the LBA.

    Lba2 - Stores bits 23-16 of the LBA.

    Device - Stores the contents of the device register in the shadow register
        block.

    Lba3 - Stores bits 31-24 of the LBA.

    Lba4 - Stores bits 39-32 of the LBA.

    Lba5 - Stores bits 47-40 of the LBA.

    Reserved0 - Stores a reserved value. Should be set to zero.

    Count0 - Stores the low 8 bits of the sector count.

    Count1 - Stores the high 8 bits of the sector count.

    Reserved1 - Stores a reserved value. Should be set to zero.

    Reserved2 - Stores a reserved value. Should be set to zero.

--*/

typedef struct _SATA_FIS_REGISTER_D2H {
    UCHAR Type;
    UCHAR Flags;
    UCHAR Status;
    UCHAR Error;
    UCHAR Lba0;
    UCHAR Lba1;
    UCHAR Lba2;
    UCHAR Device;
    UCHAR Lba3;
    UCHAR Lba4;
    UCHAR Lba5;
    UCHAR Reserved0;
    UCHAR Count0;
    UCHAR Count1;
    UCHAR Reserved1[2];
    ULONG Reserved2;
} PACKED SATA_FIS_REGISTER_D2H, *PSATA_FIS_REGISTER_D2H;

/*++

Structure Description:

    This structure defines the device to host set device bits FIS, defined by
    the SATA spec.

Members:

    Type - Stores the constant SataFisSetDeviceBits.

    Flags - Stores the port multiplier port and some other flags. See
        SATA_FIS_SET_DEVICE_BITS_FLAG_* definitions.

    Status - Stores the new value of the status register of the shadow register
        block.

    Error - Stores the new value of the Error register of the shadow register
        block.

    ProtocolSpecific - Stores a protocol-specific value.

--*/

typedef struct _SATA_FIS_SET_DEVICE_BITS {
    UCHAR Type;
    UCHAR Flags;
    UCHAR Status;
    UCHAR Error;
    ULONG ProtocolSpecific;
} PACKED SATA_FIS_SET_DEVICE_BITS, *PSATA_FIS_SET_DEVICE_BITS;

/*++

Structure Description:

    This structure defines the device to host DMA activate FIS structure, as
    defined by the SATA spec.

Members:

    Type - Stores the constant SataFisDmaActivate.

    Flags - Stores the port multiplier port.

    Reserved - Stores a reserved value. Should be set to zero.

--*/

typedef struct _SATA_FIS_DMA_ACTIVATE {
    UCHAR Type;
    UCHAR Flags;
    USHORT Reserved;
} PACKED SATA_FIS_DMA_ACTIVATE, *PSATA_FIS_DMA_ACTIVATE;

/*++

Structure Description:

    This structure defines the bidirectional DMA setup FIS structure, as
    defined by the SATA spec.

Members:

    Type - Stores the constant SataFisDmaSetup.

    Flags - Stores the port multiplier port and some other flags.

    Reserved0 - Stores a reserved value. Should be set to zero.

    DmaBufferIdLow - Stores the low 32 bits of the DMA buffer identifier, which
        is probably a physical address.

    DmaBufferIdHigh - Stores the upper 32 bits of the DMA buffer identifier.

    Reserved1 - Stores a reserved value. Should be set to zero.

    DmaBufferOffset - Stores the offset into the buffer to start from. This
        must be a multiple of 4.

    DmaTransferCount - Stores the number of bytes to transfer. This must be an
        even number.

    Reserved2 - Stores a reserved value. Should be set to zero.

--*/

typedef struct _SATA_FIS_DMA_SETUP {
    UCHAR Type;
    UCHAR Flags;
    USHORT Reserved0;
    ULONG DmaBufferIdLow;
    ULONG DmaBufferIdHigh;
    ULONG Reserved1;
    ULONG DmaBufferOffset;
    ULONG DmaTransferCount;
    ULONG Reserved2;
} PACKED SATA_FIS_DMA_SETUP, *PSATA_FIS_DMA_SETUP;

/*++

Structure Description:

    This structure defines the bidirectional BIST activate FIS structure, as
    defined by the SATA spec.

Members:

    Type - Stores the constant SataFisBistActivate.

    Flags - Stores the port multiplier port.

    PatternDefinition - Stores the pattern definition bits.

    Reserved0 - Stores a reserved value. Should be set to zero.

    Data - Stores the test data.

--*/

typedef struct _SATA_FIS_BIST_ACTIVATE {
    UCHAR Type;
    UCHAR Flags;
    UCHAR PatternDefinition;
    UCHAR Reserved;
    UCHAR Data[8];
} PACKED SATA_FIS_BIST_ACTIVATE, *PSATA_FIS_BIST_ACTIVATE;

/*++

Structure Description:

    This structure defines the device to host PIO setup FIS structure, as
    defined by the SATA spec.

Members:

    Type - Stores the constant SataFisPioSetup.

    Flags - Stores the port multiplier port and some other flags.

    Status - Stores the new value of the status (and alternate status) register
        of the shadow register block.

    Error - Stores the new value of the Error register of the shadow register
        block.

    Lba0 - Stores bits 7-0 of the LBA.

    Lba1 - Stores bits 15-8 of the LBA.

    Lba2 - Stores bits 23-16 of the LBA.

    Device - Stores the contents of the device register in the shadow register
        block.

    Lba3 - Stores bits 31-24 of the LBA.

    Lba4 - Stores bits 39-32 of the LBA.

    Lba5 - Stores bits 47-40 of the LBA.

    Reserved0 - Stores a reserved value. Should be set to zero.

    Count0 - Stores the low 8 bits of the sector count.

    Count1 - Stores the high 8 bits of the sector count.

    Reserved1 - Stores a reserved value. Should be set to zero.

    EndStatus - Stores the new value of the status register of the command
        block at the conclusion of the subsequent data FIS.

    TransferCount - Stores the number of bytes to be transferred in the
        subsequent data FIS. This must be an even number.

    Reserved2 - Stores a reserved value. Should be set to zero.

--*/

typedef struct _SATA_FIS_PIO_SETUP {
    UCHAR Type;
    UCHAR Flags;
    UCHAR Status;
    UCHAR Error;
    UCHAR Lba0;
    UCHAR Lba1;
    UCHAR Lba2;
    UCHAR Device;
    UCHAR Lba3;
    UCHAR Lba4;
    UCHAR Lba5;
    UCHAR Reserved0;
    UCHAR Count0;
    UCHAR Count1;
    UCHAR Reserved1;
    UCHAR EndStatus;
    USHORT TransferCount;
    USHORT Reserved2;
} PACKED SATA_FIS_PIO_SETUP, *PSATA_FIS_PIO_SETUP;

/*++

Structure Description:

    This structure defines the header of the bidrectional data FIS structure,
    as defined by the SATA spec.

Members:

    Type - Stores the constant SataFisData.

    Flags - Stores the port multiplier port.

    Reserved - Stores a reserved value. Should be set to zero.

--*/

typedef struct _SATA_FIS_DATA {
    UCHAR Type;
    UCHAR Flags;
    USHORT Reserved;
} PACKED SATA_FIS_DATA, *PSATA_FIS_DATA;

/*++

Structure Description:

    This structure defines the format of an AHCI physical region descriptor
    table, which defines a physical memory data location.

Members:

    AddressLow - Stores the lower 32 bits of the data buffer address. This must
        be 4 byte aligned.

    AddressHigh - Stores the upper 32 bits of the data buffer address.

    Reserved - Stores a reserved value. Set this to zero.

    Count - Stores the number of bytes that are valid in this descriptor.

--*/

typedef struct _AHCI_PRDT {
    ULONG AddressLow;
    ULONG AddressHigh;
    ULONG Reserved;
    ULONG Count;
} PACKED AHCI_PRDT, *PAHCI_PRDT;

/*++

Structure Description:

    This structure defines the command header, which is the top level command
    structure in each port. It points to a command table. This structure is
    hardware defined.

Members:

    Control - Stores the multiport device destination, control flags, and
        command FIS length.

    PrdtLength - Stores the number of PRDT entries in the command table.

    Size - Stores the transfer size in bytes.

    CommandTableLow - Stores the lower 32 bits of the physical address of the
        command table.

    CommandTableHigh - Stores the upper 32 bits of the physical address of the
        command table.

    Reserved - Stores reserved padding bytes. Set this to zero.

--*/

typedef struct _AHCI_COMMAND_HEADER {
    USHORT Control;
    USHORT PrdtLength;
    ULONG Size;
    ULONG CommandTableLow;
    ULONG CommandTableHigh;
    ULONG Reserved[4];
} PACKED AHCI_COMMAND_HEADER, *PAHCI_COMMAND_HEADER;

/*++

Structure Description:

    This structure defines the format of an AHCI command table, which contains
    the parameters for a particular command.

Members:

    CommandFis - Stores the command FIS structure.

    AtapiCommand - Stores the ATAPI command structure, which is either 12 or
        16 bytes.

    Prdt - Stores the physical region descriptor table entries. The spec allows
        for up to 65,535, but this implementation limits them for convenience.

--*/

typedef struct _AHCI_COMMAND_TABLE {
    UCHAR CommandFis[0x40];
    UCHAR AtapiCommand[0x40];
    AHCI_PRDT Prdt[AHCI_PRDT_COUNT];
} PACKED AHCI_COMMAND_TABLE, *PAHCI_COMMAND_TABLE;

/*++

Structure Description:

    This structure defines the format of the received FIS region.

Members:

    DmaSetupFis - Stores the DMA setup FIS.

    PioSetupFis - Stores the PIO setup FIS.

    RegisterD2hFis - Stores the device to host register FIS.

    SetDeviceBitsFis - Stores the set-device-bits FIS.

    UnknownFis - Stores an unknown FIS.

    Reserved - Stores padding bytes.

--*/

typedef struct _AHCI_RECEIVED_FIS {
    UCHAR DmaSetupFis[0x20];
    UCHAR PioSetupFis[0x20];
    UCHAR RegisterD2hFis[0x18];
    UCHAR SetDeviceBitsFis[0x08];
    UCHAR UnknownFis[0x40];
    UCHAR Reserved[0x60];
} PACKED AHCI_RECEIVED_FIS, *PAHCI_RECEIVED_FIS;

#pragma pack(pop)

/*++

Structure Description:

    This structure defines state associated with an executing AHCI command.

Members:

    IoSize - Supplies the current I/O size in flight.

    Irp - Supplies a pointer to the IRP.

--*/

typedef struct _AHCI_COMMAND_STATE {
    UINTN IoSize;
    PIRP Irp;
} AHCI_COMMAND_STATE, *PAHCI_COMMAND_STATE;

/*++

Structure Description:

    This structure defines state associated with an AHCI port.

Members:

    Type - Stores a marker identifying the structure as a port.

    PortBase - Stores the mapping to the port registers.

    Controller - Stores a pointer to the parent controller.

    PendingInterrupts - Stores the mask of pending interrupts on the port.

    CommandIoBuffer - Stores a pointer to the I/O buffer that stores the
        command table. The receive FIS area might also be in here.

    ReceiveIoBuffer - Stores a pointer to the I/O buffer that stores the
        receive FIS area, if FIS-based context switching is enabled.

    ReceivedFis - Stores a pointer to the received FIS structure. If a port
        multiplier is on, then this is an array of 16.

    Commands - Stores a pointer to the command headers.

    Tables - Stores a pointer to the array of command tables, one for each
        command header.

    TablesPhysical - Stores the physical address of the first command table
        entry.

    CommandState - Stores an array of contexts for each in-flight command. This
        array runs parallel to the commands array.

    Irps - Stores the array of IRPs in progress, running in parallel to the
        commands.

    Tables - Stores the array of command tables in progress, running in
        parallel to the commands.

    CommandMask - Stores the mask of supported commands in the port.

    AllocatedCommands - Stores the mask of allocated command slots.

    PendingCommands - Stores the mask of commands that are in use.

    OsDevice - Stores a pointer to the OS device for this port, if present.

    Flags - Stores a bitfield of flags about the port. See AHCI_PORT_*
        definitions.

    DevicePresent - Stores a boolean indicating if there is a device there.

    DpcLock - Stores a spinlock used to serialize DPC execution.

    TotalSectors - Stores the total number of sectors on the device.

    Table0 - Stores a pointer to the command table that goes in slot zero.

    Table0Physical - Stores the physical address of the slot zero command table.

    IrpQueue - Stores the queue of IRPs that have not yet been started.

--*/

typedef struct _AHCI_PORT {
    AHCI_CONTEXT_TYPE Type;
    PVOID PortBase;
    PAHCI_CONTROLLER Controller;
    volatile ULONG PendingInterrupts;
    PIO_BUFFER CommandIoBuffer;
    PIO_BUFFER ReceiveIoBuffer;
    PAHCI_RECEIVED_FIS ReceivedFis;
    PAHCI_COMMAND_HEADER Commands;
    PAHCI_COMMAND_TABLE Tables;
    PHYSICAL_ADDRESS TablesPhysical;
    AHCI_COMMAND_STATE CommandState[AHCI_COMMAND_COUNT];
    ULONG CommandMask;
    volatile ULONG AllocatedCommands;
    ULONG PendingCommands;
    PDEVICE OsDevice;
    ULONG Flags;
    KSPIN_LOCK DpcLock;
    ULONGLONG TotalSectors;
    LIST_ENTRY IrpQueue;
} AHCI_PORT, *PAHCI_PORT;

/*++

Structure Description:

    This structure defines state associated with an ATA controller.

Members:

    Type - Stores a value identifying this structure as an AHCI controller (as
        opposed to a port).

    ControllerBase - Stores the mapping to the controller registers.

    PendingInterrupts - Stores the mask of ports with a pending interrupt.

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptFound - Stores a boolean indicating whether or not the interrupt
        line and interrupt vector fields are valid.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    Ports - Stores the array of port structures.

    PortCount - Stores the number of ports supported in the silicon.

    ImplementedPorts - Stores the value from the BIOS indicating which ports
        are actually populated.

    CommandCount - Stores the maximum number of commands that can be queued at
        once.

    MaxPhysical - Stores the maximum supported physical address of the hardware.

    OsDevice - Stores a pointer to the controller's device structure.

--*/

struct _AHCI_CONTROLLER {
    AHCI_CONTEXT_TYPE Type;
    PVOID ControllerBase;
    volatile ULONG PendingInterrupts;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    HANDLE InterruptHandle;
    AHCI_PORT Ports[AHCI_PORT_COUNT];
    ULONG PortCount;
    ULONG ImplementedPorts;
    ULONG CommandCount;
    ULONGLONG MaxPhysical;
    PDEVICE OsDevice;
};

//
// -------------------------------------------------------------------- Globals
//

extern PDRIVER AhciDriver;

//
// -------------------------------------------------------- Function Prototypes
//

INTERRUPT_STATUS
AhciInterruptService (
    PVOID Context
    );

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

INTERRUPT_STATUS
AhciInterruptServiceDpc (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine implements the AHCI dispatch level interrupt service.

Arguments:

    Parameter - Supplies the context, in this case the AHCI controller
        structure.

Return Value:

    None.

--*/

KSTATUS
AhcipResetController (
    PAHCI_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine resets an AHCI controller device.

Arguments:

    Controller - Supplies a pointer to the AHCI controller.

Return Value:

    Status code.

--*/

KSTATUS
AhcipProbePort (
    PAHCI_CONTROLLER Controller,
    ULONG PortIndex
    );

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

KSTATUS
AhcipEnumeratePort (
    PAHCI_PORT Port
    );

/*++

Routine Description:

    This routine enumerates the drive behind the AHCI port.

Arguments:

    Port - Supplies a pointer to the AHCI port to start.

Return Value:

    Status code.

--*/

KSTATUS
AhcipEnqueueIrp (
    PAHCI_PORT Port,
    PIRP Irp
    );

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

VOID
AhcipProcessPortRemoval (
    PAHCI_PORT Port,
    BOOL CanTouchPort
    );

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

