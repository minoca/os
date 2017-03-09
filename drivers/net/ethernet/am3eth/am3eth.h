/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    am3eth.h

Abstract:

    This header contains definitions for the CPSW Ethernet controller in the
    TI AM335x SoCs.

Author:

    Evan Green 20-Mar-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write registers in the controller.
//

#define A3E_READ(_Controller, _Register) \
    HlReadRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register))

#define A3E_WRITE(_Controller, _Register, _Value)                          \
    HlWriteRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register), \
                      (_Value))

#define A3E_ALE_READ(_Controller, _Register) \
    A3E_READ((_Controller), A3E_ALE_OFFSET + (_Register))

#define A3E_ALE_WRITE(_Controller, _Register, _Value) \
    A3E_WRITE((_Controller), A3E_ALE_OFFSET + (_Register), (_Value))

#define A3E_SS_READ(_Controller, _Register) \
    A3E_READ((_Controller), A3E_SS_OFFSET + (_Register))

#define A3E_SS_WRITE(_Controller, _Register, _Value) \
    A3E_WRITE((_Controller), A3E_SS_OFFSET + (_Register), (_Value))

#define A3E_WR_READ(_Controller, _Register) \
    A3E_READ((_Controller), A3E_WR_OFFSET + (_Register))

#define A3E_WR_WRITE(_Controller, _Register, _Value) \
    A3E_WRITE((_Controller), A3E_WR_OFFSET + (_Register), (_Value))

#define A3E_SL1_READ(_Controller, _Register) \
    A3E_READ((_Controller), A3E_SL1_OFFSET + (_Register))

#define A3E_SL1_WRITE(_Controller, _Register, _Value) \
    A3E_WRITE((_Controller), A3E_SL1_OFFSET + (_Register), (_Value))

#define A3E_SL2_READ(_Controller, _Register) \
    A3E_READ((_Controller), A3E_SL2_OFFSET + (_Register))

#define A3E_SL2_WRITE(_Controller, _Register, _Value) \
    A3E_WRITE((_Controller), A3E_SL2_OFFSET + (_Register), (_Value))

#define A3E_DMA_READ(_Controller, _Register) \
    A3E_READ((_Controller), A3E_CPDMA_OFFSET + (_Register))

#define A3E_DMA_WRITE(_Controller, _Register, _Value) \
    A3E_WRITE((_Controller), A3E_CPDMA_OFFSET + (_Register), (_Value))

#define A3E_MDIO_READ(_Controller, _Register) \
    A3E_READ((_Controller), A3E_MDIO_OFFSET + (_Register))

#define A3E_MDIO_WRITE(_Controller, _Register, _Value) \
    A3E_WRITE((_Controller), A3E_MDIO_OFFSET + (_Register), (_Value))

#define A3E_PORT_READ(_Controller, _Port, _Register) \
    A3E_READ((_Controller), A3E_PORT0_OFFSET + ((_Port) * 0x100) + (_Register))

#define A3E_PORT_WRITE(_Controller, _Port, _Register, _Value)       \
    A3E_WRITE((_Controller),                                        \
              A3E_PORT0_OFFSET + ((_Port) * 0x100) + (_Register),   \
              (_Value))

//
// These macros access the interrupt control/status and rate registers for a
// given core (0 or 1).
//

#define A3E_WR_CORE(_Register, _Core) ((_Register) + ((_Core) * 16))
#define A3E_WR_CORE_RATE(_Register, _Core) ((_Register) + ((_Core) * 8))

//
// This macro gets the given register for the given channel (for registers
// where that's valid in the CPDMA submodule). Valid channel values are 0
// through 7.
//

#define A3E_CPDMA_CHANNEL(_Register, _Channel) ((_Register) + ((_Channel) * 4))

//
// This macro gets the ALE table word given an index. Valid indices are zero
// through two, inclusive.
//

#define A3E_ALE_TABLE(_Index) (A3eAleTable + ((2 - (_Index)) * 4))

//
// This macro gets the ALE port control register. Valid ports are 0 through 5,
// inclusive.
//

#define A3E_ALE_PORT_CONTROL(_Index) (A3eAlePortControl + ((_Index) * 4))

//
// This macro returns the DSCP priority map register given an index, 0 through
// 7.
//

#define A3E_PORT_RX_DSCP_PRIORITY_MAP(_Index) \
    (A3ePortRxDscpPriorityMap + ((_Index) * 4))

//
// This macro returns the physical address of the receive descriptor with the
// given index.
//

#define A3E_RX_DESCRIPTOR(_Device, _Index)      \
    ((_Device)->ReceiveDescriptorsPhysical +    \
     ((_Index) * sizeof(A3E_DESCRIPTOR)))

//
// This macro turns a channel number into a mask, used by the interrupt mask
// registers.
//

#define A3E_CPDMA_CHANNEL_MASK(_Channel) (1 << (_Channel))

//
// This macro turns a channel number into a mask.
//

#define A3E_WR_CHANNEL_MASK(_Channel) (1 << (_Channel))

#define A3E_SLAVE_PORT_MASK(_SlavePort) (1 << (_SlavePort))

//
// ---------------------------------------------------------------- Definitions
//

//
// LAN8710 PHY register bits (probably not the best place for them).
//

#define PHY_LAN8710_MODE 17
#define PHY_LAN8710_MODE_ENERGY_DETECT_POWER_DOWN (1 << 13)

//
// Define the TI AM335x Ethernet controller allocation tag: Am3E
//

#define A3E_ALLOCATION_TAG 0x45336D41

//
// Define how often to check the link for connect/disconnect, in seconds.
//

#define A3E_LINK_CHECK_INTERVAL 5

//
// Define the minimum allowed packet size. The CPSW Ethernet controller does
// not automatically pad packets up to the Ethernet minimum of 64-bytes.
//

#define A3E_TRANSMIT_MINIMUM_PACKET_SIZE 64

//
// Define the size of receive frame data. This is rounded up to be a multiple
// of 64 for more predictable cache line flushing.
//

#define A3E_RECEIVE_FRAME_DATA_SIZE 1536

//
// Define the size of the built-in RAM, which is used for descriptors.
//

#define A3E_CPPI_RAM_SIZE 0x2000
#define A3E_RECEIVE_DESCRIPTORS_SIZE (A3E_CPPI_RAM_SIZE / 2)
#define A3E_TRANSMIT_DESCRIPTORS_SIZE (A3E_CPPI_RAM_SIZE / 2)

//
// Define the number of receive buffers that will be allocated for the
// controller.
//

#define A3E_RECEIVE_FRAME_COUNT \
    (A3E_RECEIVE_DESCRIPTORS_SIZE / sizeof(A3E_DESCRIPTOR))

//
// Define the number of transmit descriptors to allocate for the controller.
//

#define A3E_TRANSMIT_DESCRIPTOR_COUNT \
    (A3E_TRANSMIT_DESCRIPTORS_SIZE / sizeof(A3E_DESCRIPTOR))

//
// Define software flags to remember whether a transmit or receive interrupt
// is in progress.
//

#define A3E_PENDING_RECEIVE_INTERRUPT 0x00000001
#define A3E_PENDING_TRANSMIT_INTERRUPT 0x00000002
#define A3E_PENDING_LINK_CHECK_TIMER 0x00000004

//
// Define descriptor flags. Some of these are common, others only apply to
// transmit or receive descriptors.
//

#define A3E_DESCRIPTOR_NEXT_NULL 0

#define A3E_DESCRIPTOR_BUFFER_LENGTH_MASK 0x0000FFFF
#define A3E_DESCRIPTOR_BUFFER_OFFSET_SHIFT 16

#define A3E_DESCRIPTOR_TX_PACKET_LENGTH_MASK 0x000007FF
#define A3E_DESCRIPTOR_TX_TO_PORT_SHIFT 16
#define A3E_DESCRIPTOR_PORT_MASK (0x3 << 16)
#define A3E_DESCRIPTOR_VLAN (1 << 19)

#define A3E_DESCRIPTOR_TX_TO_PORT_ENABLE (1 << 20)
#define A3E_DESCRIPTOR_RX_PACKET_ERROR_MASK (0x3 << 20)
#define A3E_DESCRIPTOR_RX_PACKET_ERROR_NONE (0x0 << 20)
#define A3E_DESCRIPTOR_RX_PACKET_ERROR_CRC (0x1 << 20)
#define A3E_DESCRIPTOR_RX_PACKET_ERROR_CODE (0x2 << 20)
#define A3E_DESCRIPTOR_RX_PACKET_ERROR_ALIGN (0x3 << 20)
#define A3E_DESCRIPTOR_OVERRUN (1 << 22)
#define A3E_DESCRIPTOR_MAC_CONTROL (1 << 23)
#define A3E_DESCRIPTOR_SHORT (1 << 24)
#define A3E_DESCRIPTOR_LONG (1 << 25)

#define A3E_DESCRIPTOR_PASS_CRC (1 << 26)
#define A3E_DESCRIPTOR_TEARDOWN_COMPLETE (1 << 27)
#define A3E_DESCRIPTOR_END_OF_QUEUE (1 << 28)
#define A3E_DESCRIPTOR_HARDWARE_OWNED (1 << 29)
#define A3E_DESCRIPTOR_END_OF_PACKET (1 << 30)
#define A3E_DESCRIPTOR_START_OF_PACKET (1 << 31)

//
// Define submodule register offsets.
//

#define A3E_SS_OFFSET 0x0000
#define A3E_PORT0_OFFSET 0x0100
#define A3E_PORT1_OFFSET 0x0200
#define A3E_PORT2_OFFSET 0x0300
#define A3E_CPDMA_OFFSET 0x0800
#define A3E_STATS_OFFSET 0x0900
#define A3E_STATERAM_OFFSET 0x0A00
#define A3E_CPTS_OFFSET 0x0C00
#define A3E_ALE_OFFSET 0x0D00
#define A3E_SL1_OFFSET 0x0D80
#define A3E_SL2_OFFSET 0x0DC0
#define A3E_MDIO_OFFSET 0x1000
#define A3E_WR_OFFSET 0x1200
#define A3E_CPPI_RAM_OFFSET 0x2000
#define A3E_REGISTERS_SIZE 0x4000

#define A3E_SS_SOFT_RESET_SOFT_RESET 0x00000001

#define A3E_WR_SOFT_RESET_SOFT_RESET 0x00000001

#define A3E_SL_SOFT_RESET_SOFT_RESET 0x00000001

#define A3E_CPDMA_CHANNEL_COUNT 8
#define A3E_CPDMA_DMA_SOFT_RESET_SOFT_RESET 0x00000001

#define A3E_PORT_0_MASK 0x1
#define A3E_PORT_1_MASK 0x2
#define A3E_PORT_2_MASK 0x4
#define A3E_HOST_PORT_MASK A3E_PORT_0_MASK

//
// DMA End of Interrupt register definitions
//

#define A3E_CPDMA_EOI_TX_PULSE 0x02
#define A3E_CPDMA_EOI_RX_PULSE 0x01

//
// CPDMA Transmit Control register definitions.
//

#define A3E_CPDMA_TX_CONTROL_ENABLE 0x00000001

//
// CPDMA Receive Control register definitions.
//

#define A3E_CPDMA_RX_CONTROL_ENABLE 0x00000001

//
// Statistics port enable register definitions.
//

#define A3E_SS_STATISTICS_PORT_ENABLE_PORT0_STATISTICS_ENABLE 0x00000001
#define A3E_SS_STATISTICS_PORT_ENABLE_PORT1_STATISTICS_ENABLE 0x00000002
#define A3E_SS_STATISTICS_PORT_ENABLE_PORT2_STATISTICS_ENABLE 0x00000004

//
// MDIO input and desired clock frequencies, in Hertz.
//

#define A3E_MDIO_FREQUENCY_INPUT 125000000
#define A3E_MDIO_FREQUENCY_OUTPUT 1000000

//
// MDIO control register definitions
//

#define A3E_MDIO_CONTROL_DIVISOR_MASK 0x0000FFFF
#define A3E_MDIO_CONTROL_ENABLE 0x40000000
#define A3E_MDIO_CONTROL_PREAMBLE 0x00100000
#define A3E_MDIO_CONTROL_FAULTENB 0x00040000

//
// ALE Control register definitions
//

#define A3E_ALE_CONTROL_VLAN_AWARE 0x00000004
#define A3E_ALE_CONTROL_BYPASS 0x00000010
#define A3E_ALE_CONTROL_CLEAR_TABLE 0x40000000
#define A3E_ALE_CONTROL_ENABLE_ALE 0x80000000

//
// ALE port control register definitions.
//

#define A3E_ALE_PORT_CONTROL_STATE_MASK 0x00000003
#define A3E_ALE_PORT_STATE_FORWARD 0x03
#define A3E_ALE_PORT_STATE_LEARN 0x02
#define A3E_ALE_PORT_STATE_BLOCKED 0x01
#define A3E_ALE_PORT_STATE_DISABLED 0x00

//
// Define the number of 32-bit words in an ALE entry.
//

#define A3E_ALE_ENTRY_WORDS 3

//
// Define the maximum number of ALE entries.
//

#define A3E_MAX_ALE_ENTRIES 1024

//
// Define ALE entry definitions.
//

#define A3E_ALE_ENTRY_TYPE_MASK 0x30
#define A3E_ALE_ENTRY_TYPE_VLAN 0x20
#define A3E_ALE_ENTRY_TYPE_VLANUCAST 0x30
#define A3E_ALE_ENTRY_TYPE_FREE 0x00

#define A3E_ALE_ENTRY_TYPE_INDEX 7

#define A3E_ALE_VLAN_ENTRY_MEMBER_LIST_INDEX 0
#define A3E_ALE_VLAN_ENTRY_FRC_UNTAG_EGR_INDEX 3
#define A3E_ALE_VLAN_ENTRY_ID_BIT0_BIT7_INDEX 6
#define A3E_ALE_VLAN_ENTRY_TYPE_ID_BIT8_BIT11_INDEX 7

#define A3E_ALE_VLANUCAST_ENTRY_ID_BIT0_BIT7_INDEX 6
#define A3E_ALE_VLANUCAST_ENTRY_TYPE_ID_BIT8_BIT11_INDEX 7

//
// ALE Table control register bits. The lower bits are the ALE index.
//

#define A3E_ALE_TABLE_CONTROL_WRITE 0x80000000

//
// Port TX control register definitions
//

#define A3E_PORT_TX_IN_CONTROL_TX_IN_SELECT 0x00030000
#define A3E_PORT_TX_IN_CONTROL_TX_IN_DUAL_MAC 0x00010000

//
// VLAN port configuration register definitions
//

#define A3E_PORT_VLAN_PORT_CFI_SHIFT 12
#define A3E_PORT_VLAN_PORT_PRIORITY_SHIFT 13

//
// Define how long to wait for a PHY command to complete, in seconds.
//

#define A3E_PHY_TIMEOUT 5

//
// MDIO User Access 0 register definitions.
//

#define A3E_MDIO_USERACCESS0_READ 0x00000000
#define A3E_MDIO_USERACCESS0_ACK 0x20000000
#define A3E_MDIO_USERACCESS0_WRITE 0x40000000
#define A3E_MDIO_USERACCESS0_GO 0x80000000

#define A3E_PHY_REGISTER_MASK 0x1F
#define A3E_PHY_ADDRESS_MASK 0x1F
#define A3E_PHY_DATA_MASK 0xFFFF
#define A3E_PHY_REGISTER_SHIFT 21
#define A3E_PHY_ADDRESS_SHIFT 16

//
// Sliver MAC Control register definitions
//

#define A3E_SL_MAC_CONTROL_FULL_DUPLEX 0x00000001
#define A3E_SL_MAC_CONTROL_GMII_ENABLE 0x00000020
#define A3E_SL_MAC_CONTROL_GIGABIT 0x00000080
#define A3E_SL_MAC_CONTROL_IFCTL_A 0x00008000
#define A3E_SL_MAC_CONTROL_IFCTL_B 0x00010000
#define A3E_SL_MAC_CONTROL_EXT_IN 0x00040000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _A3E_SS_REGISTER {
    A3eSsIdVersion = 0x00,
    A3eSsControl = 0x04,
    A3eSsSoftReset = 0x08,
    A3eSsStatisticsPortEnable = 0x0C,
    A3eSsTransmitPriorityType = 0x10,
    A3eSsSoftwareIdle = 0x14,
    A3eSsThroughputRate = 0x18,
    A3eSsShortGapThreshold = 0x1C,
    A3eSsTransmitStartWords = 0x20,
    A3eSsFlowControl = 0x24,
    A3eSsVlanLType = 0x28,
    A3eSsTsLType = 0x2C,
    A3eSsDlrLType = 0x30
} A3E_SS_REGISTER, *PA3E_SS_REGISTER;

typedef enum _A3E_WR_REGISTER {
    A3eWrIdVersion = 0x00,
    A3eWrSoftReset = 0x04,
    A3eWrControl = 0x08,
    A3eWrInterruptControl = 0x0C,
    A3eWrCoreRxThresholdInterruptEnable = 0x10,
    A3eWrCoreRxInterruptEnable = 0x14,
    A3eWrCoreTxInterruptEnable = 0x18,
    A3eWrCoreMiscInterruptEnable = 0x1C,
    A3eWrCoreRxThresholdInterruptStatus = 0x40,
    A3eWrCoreRxInterruptStatus = 0x44,
    A3eWrCoreTxInterruptStatus = 0x48,
    A3eWrCoreMiscInterruptStatus = 0x4C,
    A3eWrCoreRxInterruptRate = 0x70,
    A3eWrCoreTxInterruptRate = 0x74,
    A3eWrRgmiiControl = 0x88
} A3E_WR_REGISTER, *PA3E_WR_REGISTER;

typedef enum _A3E_SL_REGISTER {
    A3eSlIdVersion = 0x00,
    A3eSlMacControl = 0x04,
    A3eSlMacStatus = 0x08,
    A3eSlSoftReset = 0x0C,
    A3eSlRxMaxLength = 0x10,
    A3eSlBackoffTest = 0x14,
    A3eSlRxPause = 0x18,
    A3eSlTxPause = 0x1C,
    A3eSlEmulationControl = 0x20,
    A3eSlRxPriorityMap = 0x24,
    A3eSlTxGap = 0x28
} A3E_SL_REGISTER, *PA3E_SL_REGISTER;

typedef enum _A3E_CPDMA_REGISTER {
    A3eDmaTxIdVersion = 0x00,
    A3eDmaTxControl = 0x04,
    A3eDmaTxTeardown = 0x08,
    A3eDmaRxIdVersion = 0x10,
    A3eDmaRxControl = 0x14,
    A3eDmaRxTeardown = 0x18,
    A3eDmaSoftReset = 0x1C,
    A3eDmaControl = 0x20,
    A3eDmaStatus = 0x24,
    A3eDmaRxBufferOffset = 0x28,
    A3eDmaEmulationControl = 0x2C,
    A3eDmaTxPriorityRate = 0x30,
    A3eDmaTxInterruptStatusRaw = 0x80,
    A3eDmaTxInterruptStatusMasked = 0x84,
    A3eDmaTxInterruptMaskSet = 0x88,
    A3eDmaTxInterruptMaskClear = 0x8C,
    A3eDmaCpDmaInputVector = 0x90,
    A3eDmaCpDmaEoiVector = 0x94,
    A3eDmaRxInterruptStatusRaw = 0xA0,
    A3eDmaRxInterruptStatusMasked = 0xA4,
    A3eDmaRxInterruptMaskSet = 0xA8,
    A3eDmaRxInterruptMaskClear = 0xAC,
    A3eDmaInterruptStatusRaw = 0xB0,
    A3eDmaInterruptStatusMasked = 0xB4,
    A3eDmaInterruptMaskSet = 0xB8,
    A3eDmaInterruptMaskClear = 0xBC,
    A3eDmaRxPendingThreshold = 0xC0,
    A3eDmaRxFreeBuffer = 0xE0,
    A3eDmaTxHeadDescriptorPointer = 0x200,
    A3eDmaRxHeadDescriptorPointer = 0x220,
    A3eDmaTxCompletionPointer = 0x240,
    A3eDmaRxCompletionPointer = 0x260
} A3E_CPDMA_REGISTER, *PA3E_CPDMA_REGISTER;

typedef enum _A3E_MDIO_REGISTER {
    A3eMdioRevisionId = 0x00,
    A3eMdioControl = 0x04,
    A3eMdioAlive = 0x08,
    A3eMdioLink = 0x0C,
    A3eMdioLinkInterruptStatusRaw = 0x10,
    A3eMdioLinkInterruptStatusMasked = 0x14,
    A3eMdioUserInterruptStatusRaw = 0x20,
    A3eMdioUserInterruptStatusMasked = 0x24,
    A3eMdioUserInterruptMaskSet = 0x28,
    A3eMdioUserInterruptMaskClear = 0x2C,
    A3eMdioUserAccess0 = 0x80,
    A3eMdioPhySelect0 = 0x84,
    A3eMdioUserAccess1 = 0x88,
    A3eMdioPhySelect1 = 0x8C,
} A3E_MDIO_REGISTER, *PA3E_MDIO_REGISTER;

typedef enum _A3E_ALE_REGISTER {
    A3eAleIdVersion = 0x00,
    A3eAleControl = 0x08,
    A3eAlePrescale = 0x10,
    A3eAleUnknownVlan = 0x18,
    A3eAleTableControl = 0x20,
    A3eAleTable = 0x34,
    A3eAlePortControl = 0x40,
} A3E_ALE_REGISTER, *PA3E_ALE_REGISTER;

typedef enum _A3E_PORT_REGISTER {
    A3ePortControl = 0x00,
    A3ePortMaxBlocks = 0x08,
    A3ePortBlockCount = 0x0C,
    A3ePortTxInControl = 0x10,
    A3ePortPortVlan = 0x14,
    A3ePortTxPriorityMap = 0x18,
    A3ePortDmaTxPriorityMap0 = 0x1C,
    A3ePortTsSeqMtype = 0x1C,
    A3ePortDmaRxChMap0 = 0x20,
    A3ePortSourceAddressLow = 0x20,
    A3ePortSourceAddressHigh = 0x24,
    A3ePortSendPercent = 0x28,
    A3ePortRxDscpPriorityMap = 0x30,
} A3E_PORT_REGISTER, *PA3E_PORT_REGISTER;

/*++

Structure Description:

    This structure defines the TI AM335x Ethernet controller transmit and
    receive descriptor format, as defined by the hardware.

Members:

    NextDescriptor - Stores a pointer to the next buffer descriptor in the
        queue, or 0 if this is the last descriptor. This value must be 32-bit
        aligned.

    Buffer - Stores a pointer to the data buffer, which is byte aligned.

    BufferLengthOffset - Stores the buffer length in the lower 16 bits and the
        buffer offset in the upper 16 bits.

    PacketLengthFlags - Stores the packet length in the lower 16 bits, and
        flags in the upper 16 bits. See A3E_DESCRIPTOR_* definitions for the
        flags.

--*/

typedef struct _A3E_DESCRIPTOR {
    ULONG NextDescriptor;
    ULONG Buffer;
    ULONG BufferLengthOffset;
    ULONG PacketLengthFlags;
} PACKED ALIGNED16 A3E_DESCRIPTOR, *PA3E_DESCRIPTOR;

/*++

Structure Description:

    This structure defines an AM335x Ethernet controller device.

Members:

    OsDevice - Stores a pointer to the OS device object.

    TxInterruptLine - Stores the interrupt line that this controller's
        transmit interrupt comes in on.

    TxInterruptVector - Stores the interrupt vector that this controller's
        transmit interrupt comes in on.

    TxInterruptHandle - Stores a pointer to the handle received when the
        transmit interrupt was connected.

    RxInterruptLine - Stores the interrupt line that this controller's
        receive interrupt comes in on.

    RxInterruptVector - Stores the interrupt vector that this controller's
        receive interrupt comes in on.

    RxInterruptHandle - Stores a pointer to the handle received when the
        receive interrupt was connected.

    InterruptResourcesFound - Stores the number of interrupt resources found,
        which should total two (transmit and receive).

    ControllerBase - Stores the virtual address of the memory mapping to the
        controller's registers.

    ControllerBasePhysical - Stores the physical address of the controller
        registers.

    NetworkLink - Stores a pointer to the core networking link.

    ReceiveDataIoBuffer - Stores a pointer to the I/O buffer associated with
        the receive frames.

    ReceiveFrameDataSize - Stores the size of each receive frame's data.

    ReceiveBegin - Stores the index of the beginning of the list, which is
        the oldest received frame and the first one to dispatch.

    ReceiveLock - Stores a pointer to a queued lock that protects the
        received list.

    CommandPhysicalAddress - Stores the physical address of the base of the
        command list (called a list but is really an array).

    TransmitDescriptors - Stores the virtual address of the array of transmit
        descriptors.

    ReceiveDescriptors - Stores the virtual address of the array of receive
        descriptors.

    TransmitDescriptorsPhysical - Stores the physical address of the base of
        the array of transmit descriptors.

    ReceiveDescriptorsPhysical - Stores the physical address of the base of the
        array of receive descriptors.

    TransmitPacket - Stores a pointer to the array of net packet buffers that
        go with each command.

    TransmitPacketList - Stores a list of net packet buffers waiting to be
        queued.

    TransmitBegin - Stores the index of the least recent command, the first
        one to reap.

    TransmitEnd - Stores the index where the next command should be placed.

    TransmitLock - Stores the lock protecting software access to the transmit
        descriptors.

    CommandEntryFreeEvent - Stores the event to wait on in the event that no
        command entries are free.

    LinkActive - Stores a boolean indicating if there is an active network link.

    LinkSpeed - Stores the current link speed, if active.

    FullDuplex - Stores the duplex status of the link, TRUE for full duplex and
        FALSE for half duplex.

    LinkCheckTimer - Stores a pointer to the timer that fires periodically to
        see if the link is active.

    LinkCheckDpc - Stores a pointer to the DPC associated with the link check
        timer.

    NextLinkCheck - Stores the time counter value when the next link check
        should be performed.

    LinkCheckInterval - Stores the interval in time counter ticks that the
        link state should be polled.

    WorkItem - Stores a pointer to the work item queued from the DPC.

    InterruptLock - Stores the spin lock, synchronized at the interrupt
        run level, that synchronizes access to the pending status bits, DPC,
        and work item.

    InterruptRunLevel - Stores the runlevel that the interrupt lock should be
        acquired at.

    PendingStatusBits - Stores the bitfield of status bits that have yet to be
        dealt with by software.

    MacAddressAssigned - Stores a boolean indicating if the MAC address matter
        has been settled.

    MacAddress - Stores the default MAC address of the device.

    PhyId - Stores the address of the PHY.

    DataAlignment - Stores the required alignment of all data packets.

    GigabitCapable - Stores a boolean indicating if this device can do 1000Mbps.

    SupportedCapabilities - Stores the set of capabilities that this device
        supports. See NET_LINK_CAPABILITY_* for definitions.

    EnabledCapabilities - Stores the currently enabled capabilities on the
        devices. See NET_LINK_CAPABILITY_* for definitions.

    ConfigurationLock - Stores a queued lock that synchronizes changes to the
        enabled capabilities field and their supporting hardware registers.

--*/

typedef struct _A3E_DEVICE {
    PDEVICE OsDevice;
    ULONGLONG TxInterruptLine;
    ULONGLONG TxInterruptVector;
    HANDLE TxInterruptHandle;
    ULONGLONG RxInterruptLine;
    ULONGLONG RxInterruptVector;
    HANDLE RxInterruptHandle;
    ULONG InterruptResourcesFound;
    PVOID ControllerBase;
    ULONG ControllerBasePhysical;
    PNET_LINK NetworkLink;
    PIO_BUFFER ReceiveDataIoBuffer;
    ULONG ReceiveFrameDataSize;
    ULONG ReceiveBegin;
    PQUEUED_LOCK ReceiveLock;
    PA3E_DESCRIPTOR TransmitDescriptors;
    PA3E_DESCRIPTOR ReceiveDescriptors;
    ULONG TransmitDescriptorsPhysical;
    ULONG ReceiveDescriptorsPhysical;
    PNET_PACKET_BUFFER *TransmitPacket;
    NET_PACKET_LIST TransmitPacketList;
    ULONG TransmitBegin;
    ULONG TransmitEnd;
    PQUEUED_LOCK TransmitLock;
    BOOL LinkActive;
    ULONGLONG LinkSpeed;
    BOOL FullDuplex;
    PKTIMER LinkCheckTimer;
    PDPC LinkCheckDpc;
    ULONGLONG NextLinkCheck;
    ULONGLONG LinkCheckInterval;
    PWORK_ITEM WorkItem;
    KSPIN_LOCK InterruptLock;
    RUNLEVEL InterruptRunLevel;
    volatile ULONG PendingStatusBits;
    BOOL MacAddressAssigned;
    BYTE MacAddress[ETHERNET_ADDRESS_SIZE];
    ULONG PhyId;
    ULONG DataAlignment;
    BOOL GigabitCapable;
    ULONG SupportedCapabilities;
    ULONG EnabledCapabilities;
    PQUEUED_LOCK ConfigurationLock;
} A3E_DEVICE, *PA3E_DEVICE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
A3eSend (
    PVOID DeviceContext,
    PNET_PACKET_LIST PacketList
    );

/*++

Routine Description:

    This routine sends data through the network.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the link down which this data is to be sent.

    PacketList - Supplies a pointer to a list of network packets to send. Data
        in these packets may be modified by this routine, but must not be used
        once this routine returns.

Return Value:

    STATUS_SUCCESS if all packets were sent.

    STATUS_RESOURCE_IN_USE if some or all of the packets were dropped due to
    the hardware being backed up with too many packets to send.

    Other failure codes indicate that none of the packets were sent.

--*/

KSTATUS
A3eGetSetInformation (
    PVOID DeviceContext,
    NET_LINK_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets the network device layer's link information.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the link for which information is being set or queried.

    InformationType - Supplies the type of information being queried or set.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or a
        set operation (TRUE).

Return Value:

    Status code.

--*/

KSTATUS
A3epInitializeDeviceStructures (
    PA3E_DEVICE Device
    );

/*++

Routine Description:

    This routine creates the data structures needed for an AM335x CPSW
    Ethernet controller.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

KSTATUS
A3epResetDevice (
    PA3E_DEVICE Device
    );

/*++

Routine Description:

    This routine resets the TI CPSW Ethernet device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

INTERRUPT_STATUS
A3epTxInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the TI CPSW Ethernet transmit interrupt service
    routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e100 device
        structure.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
A3epRxInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the TI CPSW Ethernet receive interrupt service
    routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e100 device
        structure.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
A3epInterruptServiceWorker (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine processes interrupts for the TI CPSW Ethernet controller at
    low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    None.

--*/

KSTATUS
A3epAddNetworkDevice (
    PA3E_DEVICE Device
    );

/*++

Routine Description:

    This routine adds the device to core networking's available links.

Arguments:

    Device - Supplies a pointer to the device to add.

Return Value:

    Status code.

--*/

