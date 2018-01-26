/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    edma3.h

Abstract:

    This header contains definitions for using the TI EDMA3 controller.

Author:

    Evan Green 2-Feb-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// These macros convert a channel to its queue number register and offset.
//

#define EDMA_CHANNEL_QUEUE_REGISTER(_Channel) \
    (EdmaDmaQueue0 + (((_Channel) / 8) * 4))

#define EDMA_CHANNEL_QUEUE_SHIFT(_Channel) (((_Channel) % 8) * 4)

//
// This macro returns the register for the DMA channel map of a given channel.
//

#define EDMA_DMA_CHANNEL_MAP(_Channel) (EdmaDmaChannelMap0 + ((_Channel) * 4))

#define EDMA_GET_PARAM(_Controller, _Param) \
    (EdmaParam + ((_Param) * sizeof(EDMA_PARAM)))

//
// These macros return region access registers for the given region.
//

#define EDMA_DMA_REGION_ACCESS(_Region) \
    (EdmaDmaRegionAccessEnable0 + ((_Region) * 8))

#define EDMA_QDMA_REGION_ACCESS(_Region) \
    (EdmaQDmaRegionAccessEnable0 + ((_Region) * 8))

//
// ---------------------------------------------------------------- Definitions
//

#define UUID_EDMA_CONTROLLER {{0x010378B8, 0xADC044E1, 0x81D6A857, 0x1CB79BD5}}
#define EDMA_CHANNEL_COUNT 64

#define EDMA_PARAM_COUNT 256

#define EDMA_LINK_TERMINATE 0xFFFF

//
// Define the maximum transfer size for one PaRAM entry, rounded down to the
// nearest page boundary to prevent awkward remainders.
//

#define EDMA_MAX_TRANSFER_SIZE 0xF000

//
// Define EDMA transfer options.
//

#define EDMA_TRANSFER_SUPERVISOR (1 << 31)
#define EDMA_TRANSFER_PRIVILEGE_ID_SHIFT 24
#define EDMA_TRANSFER_PRIVILEGE_ID_MASK (0xF << 24)
#define EDMA_TRANSFER_INTERMEDIATE_COMPLETION_CHAIN (1 << 23)
#define EDMA_TRANSFER_COMPLETION_CHAIN (1 << 22)
#define EDMA_TRANSFER_INTERMEDIATE_COMPLETION_INTERRUPT (1 << 21)
#define EDMA_TRANSFER_COMPLETION_INTERRUPT (1 << 20)
#define EDMA_TRANSFER_COMPLETION_CODE_SHIFT 12
#define EDMA_TRANSFER_COMPLETION_CODE_MASK (0x3F << 12)
#define EDMA_TRANSFER_EARLY_COMPLETION (1 << 11)
#define EDMA_TRANSFER_FIFO_WIDTH_8 (0 << 8)
#define EDMA_TRANSFER_FIFO_WIDTH_16 (1 << 8)
#define EDMA_TRANSFER_FIFO_WIDTH_32 (2 << 8)
#define EDMA_TRANSFER_FIFO_WIDTH_64 (3 << 8)
#define EDMA_TRANSFER_FIFO_WIDTH_128 (4 << 8)
#define EDMA_TRANSFER_FIFO_WIDTH_256 (5 << 8)
#define EDMA_TRANSFER_STATIC (1 << 3)
#define EDMA_TRANSFER_A_SYNCHRONIZED (0 << 2)
#define EDMA_TRANSFER_AB_SYNCHRONIZED (1 << 2)
#define EDMA_TRANSFER_DESTINATION_FIFO (1 << 1)
#define EDMA_TRANSFER_SOURCE_FIFO (1 << 0)

#define EDMA_QUEUE_NUMBER_MASK 0x0000000F

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _EDMA3_TRIGGER_MODE {
    EdmaTriggerModeInvalid,
    EdmaTriggerModeManual,
    EdmaTriggerModeEvent
} EDMA3_TRIGGER_MODE, *PEDMA3_TRIGGER_MODE;

typedef enum _EDMA3_REGISTER {
    EdmaPid = 0x0000,
    EdmaCcConfig = 0x0004,
    EdmaSysConfig = 0x0010,
    EdmaDmaChannelMap0 = 0x0100,
    EdmaQDmaChannelMap0 = 0x0200,
    EdmaDmaQueue0 = 0x0240,
    EdmaQDmaQueue = 0x0260,
    EdmaQueuePriority = 0x0284,
    EdmaEventMissedLow = 0x0300,
    EdmaEventMissedHigh = 0x0304,
    EdmaEventMissedClearLow = 0x0308,
    EdmaEventMissedClearHigh = 0x030C,
    EdmaQDmaEventMissed = 0x0310,
    EdmaQDmaEventMissedClear = 0x0314,
    EdmaCcError = 0x0318,
    EdmaCcErrorClear = 0x031C,
    EdmaErrorEvaluate = 0x0320,
    EdmaDmaRegionAccessEnable0 = 0x0340,
    EdmaDmaRegionAccessEnableHigh0 = 0x0344,
    EdmaQDmaRegionAccessEnable0 = 0x0380,
    EdmaEventQueue0 = 0x0400,
    EdmaEventQueue1 = 0x0440,
    EdmaEventQueue2 = 0x0480,
    EdmaQueueStatus0 = 0x0600,
    EdmaQueueStatus1 = 0x0604,
    EdmaQueueStatus2 = 0x0608,
    EdmaQueueWatermarkThresholdA = 0x0620,
    EdmaCcStatus = 0x0640,
    EdmaMemoryProtectionFaultAddress = 0x0800,
    EdmaMemoryProtectionFaultStatus = 0x0804,
    EdmaMemoryProtectionFaultCommand = 0x0808,
    EdmaMemoryProtectionPageAttribute = 0x080C,
    EdmaMemoryProtectionPageAttribute0 = 0x0810,
    EdmaEventLow = 0x1000,
    EdmaEventHigh = 0x1004,
    EdmaEventClearLow = 0x1008,
    EdmaEventClearHigh = 0x100C,
    EdmaEventSetLow = 0x1010,
    EdmaEventSetHigh = 0x1014,
    EdmaChainedEventLow = 0x1018,
    EdmaChainedEventHigh = 0x101C,
    EdmaEventEnableLow = 0x1020,
    EdmaEventEnableHigh = 0x1024,
    EdmaEventEnableClearLow = 0x1028,
    EdmaEventEnableClearHigh = 0x102C,
    EdmaEventEnableSetLow = 0x1030,
    EdmaEventEnableSetHigh = 0x1034,
    EdmaSecondaryEventLow = 0x1038,
    EdmaSecondaryEventHigh = 0x103C,
    EdmaSecondaryEventClearLow = 0x1040,
    EdmaSecondaryEventClearHigh = 0x1044,
    EdmaInterruptEnableLow = 0x1050,
    EdmaInterruptEnableHigh = 0x1054,
    EdmaInterruptEnableClearLow = 0x1058,
    EdmaInterruptEnableClearHigh = 0x105C,
    EdmaInterruptEnableSetLow = 0x1060,
    EdmaInterruptEnableSetHigh = 0x1064,
    EdmaInterruptPendingLow = 0x1068,
    EdmaInterruptPendingHigh = 0x106C,
    EdmaInterruptClearLow = 0x1070,
    EdmaInterruptClearHigh = 0x1074,
    EdmaInterruptEvaluate = 0x1078,
    EdmaQDmaEvent = 0x1080,
    EdmaQDmaEventEnable = 0x1084,
    EdmaQDmaEventEnableClear = 0x1088,
    EdmaQDmaEventEnableSet = 0x108C,
    EdmaQDmaSecondaryEvent = 0x1090,
    EdmaQDmaSecondaryEventClear = 0x1094,
    EdmaParam = 0x4000,
} EDMA3_REGISTER, *PEDMA3_REGISTER;

/*++

Structure Description:

    This structure defines the format of an EDMA3 PaRAM parameter set, as
    mandated by the hardware.

Members:

    Options - Stores the configuration options.

    Source - Stores the byte-aligned physical address from which data is
        transferred.

    ACount - Stores the number of contiguous bytes for each transfer in the
        first (most inner) dimension.

    BCount - Stores the number of elements in the A array (second most inner
        dimension).

    Destination - Stores the byte aligned physical address to which data is
        transferred.

    SourceBIndex - Stores the byte offset between A arrays in the source. This
        probably shouldn't be less than the A count unless it's zero.

    DestinationBIndex - Stores the byte offset between A arrays in the
        destination. This probably shouldn't be less than the A count unless
        it's zero.

    Link - Stores the PaRAM set to be copied from when this one completes.
        Supply 0xFFFF to end the transfer.

    BCountReload - Stores the count value used to reload BCount when BCount
        decrements to zero. This is only relevant in A-synchronized transfers.

    SourceCIndex - Stores the byte address offset between frames (B arrays).
        For A-synchronized transfers, this is the byte address offset from the
        beginning of the last source array in a frame to the beginning of the
        first source array in the next frame. For AB-synchronized transfers,
        this is the byte address offset from the beginning of the first source
        array in a frame to the beginning of the first source array in the
        next frame.

    DestinationCIndex - Stores the byte address offset between frames (B arrays)
        in the destination. This is analagous to the source C index.

    CCount - Stores the number of frames in a block (the outermost loop).

    Reserved - Stores a reserved value. Set this to zero.

--*/

#pragma pack(push, 1)

typedef struct _EDMA_PARAM {
    ULONG Options;
    ULONG Source;
    USHORT ACount;
    USHORT BCount;
    ULONG Destination;
    SHORT SourceBIndex;
    SHORT DestinationBIndex;
    USHORT Link;
    USHORT BCountReload;
    SHORT SourceCIndex;
    SHORT DestinationCIndex;
    USHORT CCount;
    USHORT Reserved;
} PACKED EDMA_PARAM, *PEDMA_PARAM;

#pragma pack(pop)

/*++

Structure Description:

    This structure defines the format of an EDMA3 transfer configuration.

Members:

    Param - Stores the PaRAM values for the transfer.

    Mode - Stores the trigger mode.

    Queue - Stores the event queue to associate the channel with. There are 3
        independent queues.

--*/

typedef struct _EDMA_CONFIGURATION {
    EDMA_PARAM Param;
    EDMA3_TRIGGER_MODE Mode;
    ULONG Queue;
} EDMA_CONFIGURATION, *PEDMA_CONFIGURATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
