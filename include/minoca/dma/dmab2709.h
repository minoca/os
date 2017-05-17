/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dmab2709.h

Abstract:

    This header contains definitions for using the Broadcom 2709 DMA controller.

Author:

    Chris Stevens 12-Feb-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// These macros convert a channel and register into the correct offset.
//

#define DMA_BCM2709_CHANNEL_REGISTER(_Channel, _Register) \
    (DmaBcm2709Channel0 + ((_Channel) * 0x100) + (_Register))

//
// ---------------------------------------------------------------- Definitions
//

#define UUID_DMA_BCM2709_CONTROLLER \
    {{0x383496c4, 0x4EEFDFD2, 0xBC4CAF1E, 0x00B83312}}

//
// Define the total number of DMA channels supported.
//

#define DMA_BCM2709_CHANNEL_COUNT 13

//
// Define the cut off for lite DMA channels.
//

#define DMA_BCM2709_LITE_CHANNEL_START 7

//
// Define the maximum transfer size for one control block, rounded down to the
// nearest page boundary to prevent awkward remainders.
//

#define DMA_BCM2709_MAX_TRANSFER_SIZE 0x3FFFF000

//
// Define the maximum transfer size for one lite control block, rounded down to
// the nearest page boundary to prevent awkward remainders.
//

#define DMA_BCM2709_MAX_LITE_TRANSFER_SIZE 0xF000

//
// Define the required byte alignment for control blocks.
//

#define DMA_BCM2709_CONTROL_BLOCK_ALIGNMENT 32

//
// Define the bits for the channel control and status register.
//

#define DMA_BCM2709_CHANNEL_STATUS_RESET                (1 << 31)
#define DMA_BCM2709_CHANNEL_STATUS_ABORT                (1 << 30)
#define DMA_BCM2709_CHANNEL_STATUS_DISABLE_DEBUG        (1 << 29)
#define DMA_BCM2709_CHANNEL_STATUS_WAIT_FOR_WRITES      (1 << 28)
#define DMA_BCM2709_CHANNEL_STATUS_PANIC_PRIORITY_MASK  (0xF << 20)
#define DMA_BCM2709_CHANNEL_STATUS_PANIC_PRIORITY_SHIFT 20
#define DMA_BCM2709_CHANNEL_STATUS_PRIORITY_MASK        (0xF << 16)
#define DMA_BCM2709_CHANNEL_STATUS_PRIORITY_SHIFT       16
#define DMA_BCM2709_CHANNEL_STATUS_ERROR                (1 << 8)
#define DMA_BCM2709_CHANNEL_STATUS_WAITING_FOR_WRITES   (1 << 6)
#define DMA_BCM2709_CHANNEL_STATUS_DATA_REQUEST_PAUSED  (1 << 5)
#define DMA_BCM2709_CHANNEL_STATUS_PAUSED               (1 << 4)
#define DMA_BCM2709_CHANNEL_STATUS_DATA_REQUEST         (1 << 3)
#define DMA_BCM2709_CHANNEL_STATUS_INTERRUPT            (1 << 2)
#define DMA_BCM2709_CHANNEL_STATUS_END                  (1 << 1)
#define DMA_BCM2709_CHANNEL_STATUS_ACTIVE               (1 << 0)

//
// Define the bits for the control block transfer information register.
//

#define DMA_BCM2709_TRANSFER_INFORMATION_NO_WIDE_BURSTS           (1 << 26)
#define DMA_BCM2709_TRANSFER_INFORMATION_WAITS_MASK               (0x1F << 21)
#define DMA_BCM2709_TRANSFER_INFORMATION_WAITS_SHIFT              21
#define DMA_BCM2709_TRANSFER_INFORMATION_PERIPHERAL_MAP_MASK      (0x1F << 16)
#define DMA_BCM2709_TRANSFER_INFORMATION_PERIPHERAL_MAP_SHIFT     16
#define DMA_BCM2709_TRANSFER_INFORMATION_BURST_LENGTH_MASK        (0xF << 12)
#define DMA_BCM2709_TRANSFER_INFORMATION_BURST_LENGTH_SHIFT       12
#define DMA_BCM2709_TRANSFER_INFORMATION_SOURCE_IGNORE            (1 << 11)
#define DMA_BCM2709_TRANSFER_INFORMATION_SOURCE_DATA_REQUEST      (1 << 10)
#define DMA_BCM2709_TRANSFER_INFORMATION_SOURCE_WIDTH_128         (1 << 9)
#define DMA_BCM2709_TRANSFER_INFORMATION_SOURCE_WIDTH_32          (0 << 9)
#define DMA_BCM2709_TRANSFER_INFORMATION_SOURCE_INCREMENT         (1 << 8)
#define DMA_BCM2709_TRANSFER_INFORMATION_DESTINATION_IGNORE       (1 << 7)
#define DMA_BCM2709_TRANSFER_INFORMATION_DESTINATION_DATA_REQUEST (1 << 6)
#define DMA_BCM2709_TRANSFER_INFORMATION_DESTINATION_WIDTH_128    (1 << 5)
#define DMA_BCM2709_TRANSFER_INFORMATION_DESTINATION_WIDTH_32     (0 << 5)
#define DMA_BCM2709_TRANSFER_INFORMATION_DESTINATION_INCREMENT    (1 << 4)
#define DMA_BCM2709_TRANSFER_INFORMATION_WAIT_FOR_RESPONSE        (1 << 3)
#define DMA_BCM2709_TRANSFER_INFORMATION_2D_MODE                  (1 << 1)
#define DMA_BCM2709_TRANSFER_INFORMATION_INTERRUPT_ENABLE         (1 << 0)

//
// Define the bits for the control block transfer length register.
//

#define DMA_BCM2709_TRANSFER_LENGTH_2D_YLENGTH_MASK  (0x3FFF << 16)
#define DMA_BCM2709_TRANSFER_LENGTH_2D_YLENGTH_SHIFT 16
#define DMA_BCM2709_TRANSFER_LENGTH_2D_XLENGTH_MASK  (0xFFFF << 0)
#define DMA_BCM2709_TRANSFER_LENGTH_2D_XLENGTH_SHIFT 0
#define DMA_BCM2709_TRANSFER_LENGTH_XLENGTH_MASK     (0x3FFFFFFF << 0)
#define DMA_BCM2709_TRANSFER_LENGTH_XLENGTH_SHIFT    0

//
// Define the bits for the control block stride register.
//

#define DMA_BCM2709_STRIDE_DESTINATION_MASK  (0xFFFF << 16)
#define DMA_BCM2709_STRIDE_DESTINATION_SHIFT 16
#define DMA_BCM2709_STRIDE_SOURCE_MASK       (0xFFFF << 0)
#define DMA_BCM2709_STRIDE_SOURCE_SHIFT      0

//
// Define the bits for the control block debug register.
//

#define DMA_BCM2709_DEBUG_LITE                     (1 << 28)
#define DMA_BCM2709_DEBUG_VERSION_MASK             (0x7 << 25)
#define DMA_BCM2709_DEBUG_VERSION_SHIFT            25
#define DMA_BCM2709_DEBUG_STATE_MASK               (0x1FF << 16)
#define DMA_BCM2709_DEBUG_STATE_SHIFT              16
#define DMA_BCM2709_DEBUG_ID_MASK                  (0xFF << 8)
#define DMA_BCM2709_DEBUG_ID_SHIFT                 8
#define DMA_BCM2709_DEBUG_OUTSTANDING_WRITES_MASK  (0xF << 4)
#define DMA_BCM2709_DEBUG_OUTSTANDING_WRITES_SHIFT 4
#define DMA_BCM2709_DEBUG_READ_ERROR               (1 << 2)
#define DMA_BCM2709_DEBUG_FIFO_ERROR               (1 << 1)
#define DMA_BCM2709_DEBUG_READ_LAST_NOT_SET_ERROR  (1 << 0)

#define DMA_BCM2709_DEBUG_ERROR_MASK            \
    (DMA_BCM2709_DEBUG_READ_ERROR |             \
     DMA_BCM2709_DEBUG_FIFO_ERROR |             \
     DMA_BCM2709_DEBUG_READ_LAST_NOT_SET_ERROR)

//
// Define the bits for the interrupt status regiter.
//

#define DMA_BCM2709_INTERRUPT_CHANNEL_15 (1 << 15)
#define DMA_BCM2709_INTERRUPT_CHANNEL_14 (1 << 14)
#define DMA_BCM2709_INTERRUPT_CHANNEL_13 (1 << 13)
#define DMA_BCM2709_INTERRUPT_CHANNEL_12 (1 << 12)
#define DMA_BCM2709_INTERRUPT_CHANNEL_11 (1 << 11)
#define DMA_BCM2709_INTERRUPT_CHANNEL_10 (1 << 10)
#define DMA_BCM2709_INTERRUPT_CHANNEL_9  (1 << 9)
#define DMA_BCM2709_INTERRUPT_CHANNEL_8  (1 << 8)
#define DMA_BCM2709_INTERRUPT_CHANNEL_7  (1 << 7)
#define DMA_BCM2709_INTERRUPT_CHANNEL_6  (1 << 6)
#define DMA_BCM2709_INTERRUPT_CHANNEL_5  (1 << 5)
#define DMA_BCM2709_INTERRUPT_CHANNEL_4  (1 << 4)
#define DMA_BCM2709_INTERRUPT_CHANNEL_3  (1 << 3)
#define DMA_BCM2709_INTERRUPT_CHANNEL_2  (1 << 2)
#define DMA_BCM2709_INTERRUPT_CHANNEL_1  (1 << 1)
#define DMA_BCM2709_INTERRUPT_CHANNEL_0  (1 << 0)

//
// Define the bits for the enable register.
//

#define DMA_BCM2709_ENABLE_CHANNEL_15 (1 << 15)
#define DMA_BCM2709_ENABLE_CHANNEL_14 (1 << 14)
#define DMA_BCM2709_ENABLE_CHANNEL_13 (1 << 13)
#define DMA_BCM2709_ENABLE_CHANNEL_12 (1 << 12)
#define DMA_BCM2709_ENABLE_CHANNEL_11 (1 << 11)
#define DMA_BCM2709_ENABLE_CHANNEL_10 (1 << 10)
#define DMA_BCM2709_ENABLE_CHANNEL_9  (1 << 9)
#define DMA_BCM2709_ENABLE_CHANNEL_8  (1 << 8)
#define DMA_BCM2709_ENABLE_CHANNEL_7  (1 << 7)
#define DMA_BCM2709_ENABLE_CHANNEL_6  (1 << 6)
#define DMA_BCM2709_ENABLE_CHANNEL_5  (1 << 5)
#define DMA_BCM2709_ENABLE_CHANNEL_4  (1 << 4)
#define DMA_BCM2709_ENABLE_CHANNEL_3  (1 << 3)
#define DMA_BCM2709_ENABLE_CHANNEL_2  (1 << 2)
#define DMA_BCM2709_ENABLE_CHANNEL_1  (1 << 1)
#define DMA_BCM2709_ENABLE_CHANNEL_0  (1 << 0)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _DMA_BCM27093_REGISTER {
    DmaBcm2709Channel0 = 0x000,
    DmaBcm2709Channel1 = 0x100,
    DmaBcm2709Channel2 = 0x200,
    DmaBcm2709Channel3 = 0x300,
    DmaBcm2709Channel4 = 0x400,
    DmaBcm2709Channel5 = 0x500,
    DmaBcm2709Channel6 = 0x600,
    DmaBcm2709Channel7 = 0x700,
    DmaBcm2709Channel8 = 0x800,
    DmaBcm2709Channel9 = 0x900,
    DmaBcm2709Channel10 = 0xA00,
    DmaBcm2709Channel11 = 0xB00,
    DmaBcm2709Channel12 = 0xC00,
    DmaBcm2709Channel13 = 0xD00,
    DmaBcm2709Channel14 = 0xE00,
    DmaBcm2709InterruptStatus = 0xFE0,
    DmaBcm2709Enable = 0xFF0
} DMA_BCM27093_REGISTER, *PDMA_BCM27093_REGISTER;

typedef enum _DMA_BCM2709_CHANNEL_REGISTER {
    DmaBcm2709ChannelStatus = 0x0,
    DmaBcm2709ChannelControlBlockAddress = 0x4,
    DmaBcm2709ChannelTransferInformation = 0x8,
    DmaBcm2709ChannelSourceAddress = 0xC,
    DmaBcm2709ChannelDestinationAddress = 0x10,
    DmaBcm2709ChannelTransferLength = 0x14,
    DmaBcm2709ChannelStride = 0x18,
    DmaBcm2709ChannelNextControlBlockAddress = 0x1C,
    DmaBcm2709ChannelDebug = 0x20
} DMA_BCM2709_CHANNEL_REGISTER, *PDMA_BCM2709_CHANNEL_REGISTER;

/*++

Structure Description:

    This structure defines a DMA control block for the BCM2709 DMA controller.

Members:

    TransferInformation - Stores a bitmask of transfer information.
        See DMA_BCM2709_TRANSFER_INFORMATION_* for definitions.

    SourceAddress - Stores the 32-bit source address for the DMA operation.

    DestinationAddress - Stores the 32-bit destination address for the DMA
        operation.

    TransferLength - Stores the number of bytes to transfer. In 2D mode this
        stores the how many transfers (Y) of a particular size (X) to complete.

    Stride - Stores the destination and source strides for 2D mode. The DMA
        engine will increment the source/destination by the stride after each
        of the Y transfers.

    NextAddress - Stores the address of the next control block to execute.

    Reserved - Stores 8 reserved bytes.

--*/

typedef struct _DMA_BCM2709_CONTROL_BLOCK {
    ULONG TransferInformation;
    ULONG SourceAddress;
    ULONG DestinationAddress;
    ULONG TransferLength;
    ULONG Stride;
    ULONG NextAddress;
    ULONG Reserved[2];
} PACKED DMA_BCM2709_CONTROL_BLOCK, *PDMA_BCM2709_CONTROL_BLOCK;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

