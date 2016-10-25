/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sddwc.h

Abstract:

    This header contains definitions for the DesignWare SD/MMC device library.

Author:

    Chris Stevens 16-Jul-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <dev/sd.h>

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write SD DesignWare controller registers.
//

#define SD_DWC_READ_REGISTER(_Controller, _Register) \
    EfiReadRegister32((_Controller)->ControllerBase + (_Register))

#define SD_DWC_WRITE_REGISTER(_Controller, _Register, _Value) \
    EfiWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of time to wait in microseconds for the controller to
// respond.
//

#define EFI_SD_DWC_CONTROLLER_TIMEOUT 1000000

//
// Define the block sized used by the SD library.
//

#define SD_DWC_BLOCK_SIZE 512

//
// Define the SD control register bits.
//

#define SD_DWC_CONTROL_USE_INTERNAL_DMAC       (1 << 25)
#define SD_DWC_CONTROL_ENABLE_OD_PULLUP        (1 << 24)
#define SD_DWC_CONTROL_CARD_VOLTAGE_B_MASK     (0xF << 20)
#define SD_DWC_CONTROL_CARD_VOLTAGE_B_SHIFT    20
#define SD_DWC_CONTROL_CARD_VOLTAGE_A_MASK     (0xF << 16)
#define SD_DWC_CONTROL_CARD_VOLTAGE_A_SHIFT    16
#define SD_DWC_CONTROL_CE_ATA_INTERRUPT_ENABLE (1 << 11)
#define SD_DWC_CONTROL_SEND_AUTO_STOP_CCSD     (1 << 10)
#define SD_DWC_CONTROL_SEND_CCSD               (1 << 9)
#define SD_DWC_CONTROL_ABORT_READ_DATA         (1 << 8)
#define SD_DWC_CONTROL_SEND_IRQ_RESPONSE       (1 << 7)
#define SD_DWC_CONTROL_READ_WAIT               (1 << 6)
#define SD_DWC_CONTROL_DMA_ENABLE              (1 << 5)
#define SD_DWC_CONTROL_INTERRUPT_ENABLE        (1 << 4)
#define SD_DWC_CONTROL_DMA_RESET               (1 << 2)
#define SD_DWC_CONTROL_FIFO_RESET              (1 << 1)
#define SD_DWC_CONTROL_CONTROLLER_RESET        (1 << 0)

//
// Define the SD power register bits.
//

#define SD_DWC_POWER_DISABLE (0 << 0)
#define SD_DWC_POWER_ENABLE  (1 << 0)

//
// Define the SD clock divider register bits.
//

#define SD_DWC_CLOCK_DIVIDER_3_MASK  (0xFF << 24)
#define SD_DWC_CLOCK_DIVIDER_3_SHIFT 24
#define SD_DWC_CLOCK_DIVIDER_2_MASK  (0xFF << 16)
#define SD_DWC_CLOCK_DIVIDER_2_SHIFT 16
#define SD_DWC_CLOCK_DIVIDER_1_MASK  (0xFF << 8)
#define SD_DWC_CLOCK_DIVIDER_1_SHIFT 8
#define SD_DWC_CLOCK_DIVIDER_0_MASK  (0xFF << 0)
#define SD_DWC_CLOCK_DIVIDER_0_SHIFT 0

#define SD_DWC_MAX_DIVISOR (0xFF * 2)

//
// Define the SD clock source register bits.
//

#define SD_DWC_CLOCK_SOURCE_DIVIDER_3     0x3
#define SD_DWC_CLOCK_SOURCE_DIVIDER_2     0x2
#define SD_DWC_CLOCK_SOURCE_DIVIDER_1     0x1
#define SD_DWC_CLOCK_SOURCE_DIVIDER_0     0x0
#define SD_DWC_CLOCK_SOURCE_DIVIDER_MASK  (0x3 << 0)
#define SD_DWC_CLOCK_SOURCE_DIVIDER_SHIFT 0

//
// Define the SD clock enable register bits.
//

#define SD_DWC_CLOCK_ENABLE_LOW_POWER (1 << 16)
#define SD_DWC_CLOCK_ENABLE_ON        (1 << 0)

//
// Define the SD clock timeout register bits.
//

#define SD_DWC_TIMEOUT_DATA_MASK      (0xFFFFFF << 8)
#define SD_DWC_TIMEOUT_DATA_SHIFT     8
#define SD_DWC_TIMEOUT_RESPONSE_MASK  (0xFF << 0)
#define SD_DWC_TIMEOUT_RESPONSE_SHIFT 0

#define SD_DWC_TIMEOUT_DEFAULT 0xFFFFFF40

//
// Define the SD card type register bits.
//

#define SD_DWC_CARD_TYPE_8_BIT_WIDTH (1 << 16)
#define SD_DWC_CARD_TYPE_4_BIT_WIDTH (1 << 0)
#define SD_DWC_CARD_TYPE_1_BIT_WIDTH (0 << 0)

//
// Define the SD block size register bits.
//

#define SD_DWC_BLOCK_SIZE_MASK  (0xFFFF << 0)
#define SD_DWC_BLOCK_SIZE_SHIFT 0

#define SD_DWC_BLOCK_SIZE_MAX 0xFFFF

//
// Define the SD interrupt mask register bits.
//

#define SD_DWC_INTERRUPT_MASK_SDIO                       (1 << 24)
#define SD_DWC_INTERRUPT_MASK_DATA_NO_BUSY               (1 << 16)
#define SD_DWC_INTERRUPT_MASK_ERROR_END_BIT              (1 << 15)
#define SD_DWC_INTERRUPT_MASK_AUTO_COMMAND_DONE          (1 << 14)
#define SD_DWC_INTERRUPT_MASK_ERROR_START_BIT            (1 << 13)
#define SD_DWC_INTERRUPT_MASK_ERROR_HARDWARE_LOCKED      (1 << 12)
#define SD_DWC_INTERRUPT_MASK_ERROR_FIFO_UNDERRUN        (1 << 11)
#define SD_DWC_INTERRUPT_MASK_ERROR_HOST_TIMEOUT         (1 << 10)
#define SD_DWC_INTERRUPT_MASK_ERROR_DATA_READ_TIMEOUT    (1 << 9)
#define SD_DWC_INTERRUPT_MASK_ERROR_RESPONSE_TIMEOUT     (1 << 8)
#define SD_DWC_INTERRUPT_MASK_ERROR_DATA_CRC             (1 << 7)
#define SD_DWC_INTERRUPT_MASK_ERROR_RESPONSE_CRC         (1 << 6)
#define SD_DWC_INTERRUPT_MASK_RECEIVE_FIFO_DATA_REQUEST  (1 << 5)
#define SD_DWC_INTERRUPT_MASK_TRANSMIT_FIFO_DATA_REQUEST (1 << 4)
#define SD_DWC_INTERRUPT_MASK_DATA_TRANSFER_OVER         (1 << 3)
#define SD_DWC_INTERRUPT_MASK_COMMAND_DONE               (1 << 2)
#define SD_DWC_INTERRUPT_MASK_ERROR_RESPONSE             (1 << 1)
#define SD_DWC_INTERRUPT_MASK_CARD_DETECT                (1 << 0)

#define SD_DWC_INTERRUPT_ERROR_MASK                     \
    (SD_DWC_INTERRUPT_MASK_ERROR_END_BIT |              \
     SD_DWC_INTERRUPT_MASK_ERROR_START_BIT |            \
     SD_DWC_INTERRUPT_MASK_ERROR_DATA_READ_TIMEOUT |    \
     SD_DWC_INTERRUPT_MASK_ERROR_RESPONSE_TIMEOUT |     \
     SD_DWC_INTERRUPT_MASK_ERROR_DATA_CRC |             \
     SD_DWC_INTERRUPT_MASK_ERROR_RESPONSE_CRC |         \
     SD_DWC_INTERRUPT_MASK_ERROR_RESPONSE)

#define SD_DWC_INTERRUPT_DEFAULT_MASK SD_DWC_INTERRUPT_MASK_CARD_DETECT

//
// Define the SD interrupt status register bits.
//

#define SD_DWC_INTERRUPT_STATUS_SDIO                       (1 << 24)
#define SD_DWC_INTERRUPT_STATUS_DATA_NO_BUSY_DISABLE       (1 << 16)
#define SD_DWC_INTERRUPT_STATUS_ERROR_END_BIT              (1 << 15)
#define SD_DWC_INTERRUPT_STATUS_AUTO_COMMAND_DONE          (1 << 14)
#define SD_DWC_INTERRUPT_STATUS_ERROR_START_BIT            (1 << 13)
#define SD_DWC_INTERRUPT_STATUS_ERROR_HARDWARE_LOCKED      (1 << 12)
#define SD_DWC_INTERRUPT_STATUS_ERROR_FIFO_UNDERRUN        (1 << 11)
#define SD_DWC_INTERRUPT_STATUS_ERROR_HOST_TIMEOUT         (1 << 10)
#define SD_DWC_INTERRUPT_STATUS_ERROR_DATA_READ_TIMEOUT    (1 << 9)
#define SD_DWC_INTERRUPT_STATUS_ERROR_RESPONSE_TIMEOUT     (1 << 8)
#define SD_DWC_INTERRUPT_STATUS_ERROR_DATA_CRC             (1 << 7)
#define SD_DWC_INTERRUPT_STATUS_ERROR_RESPONSE_CRC         (1 << 6)
#define SD_DWC_INTERRUPT_STATUS_RECEIVE_FIFO_DATA_REQUEST  (1 << 5)
#define SD_DWC_INTERRUPT_STATUS_TRANSMIT_FIFO_DATA_REQUEST (1 << 4)
#define SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER         (1 << 3)
#define SD_DWC_INTERRUPT_STATUS_COMMAND_DONE               (1 << 2)
#define SD_DWC_INTERRUPT_STATUS_ERROR_RESPONSE             (1 << 1)
#define SD_DWC_INTERRUPT_STATUS_CARD_DETECT                (1 << 0)
#define SD_DWC_INTERRUPT_STATUS_ALL_MASK                   0xFFFFFFFF

#define SD_DWC_INTERRUPT_STATUS_COMMAND_ERROR_MASK \
    (SD_DWC_INTERRUPT_STATUS_ERROR_RESPONSE |      \
     SD_DWC_INTERRUPT_STATUS_ERROR_RESPONSE_CRC)

#define SD_DWC_INTERRUPT_STATUS_DATA_ERROR_MASK        \
    (SD_DWC_INTERRUPT_STATUS_ERROR_DATA_CRC |          \
     SD_DWC_INTERRUPT_STATUS_ERROR_DATA_READ_TIMEOUT | \
     SD_DWC_INTERRUPT_STATUS_ERROR_HOST_TIMEOUT |      \
     SD_DWC_INTERRUPT_STATUS_ERROR_START_BIT |         \
     SD_DWC_INTERRUPT_STATUS_ERROR_END_BIT)

//
// Define the SD command register bits.
//

#define SD_DWC_COMMAND_START                       (1 << 31)
#define SD_DWC_COMMAND_USE_HOLD_REGISTER           (1 << 29)
#define SD_DWC_COMMAND_VOLT_SWITCH                 (1 << 28)
#define SD_DWC_COMMAND_BOOT_MODE                   (1 << 27)
#define SD_DWC_COMMAND_DISABLE_BOOT                (1 << 26)
#define SD_DWC_COMMAND_EXPECT_BOOT_ACK             (1 << 25)
#define SD_DWC_COMMAND_ENABLE_BOOT                 (1 << 24)
#define SD_DWC_COMMAND_CSS_EXPECTED                (1 << 23)
#define SD_DWC_COMMAND_READ_CE_ATA                 (1 << 22)
#define SD_DWC_COMMAND_UPDATE_CLOCK_REGISTERS      (1 << 21)
#define SD_DWC_COMMAND_CARD_NUMBER_MASK            (0x1F << 16)
#define SD_DWC_COMMAND_CARD_NUMBER_SHIFT           16
#define SD_DWC_COMMAND_SEND_INITIALIZATION         (1 << 15)
#define SD_DWC_COMMAND_STOP_ABORT                  (1 << 14)
#define SD_DWC_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE (1 << 13)
#define SD_DWC_COMMAND_SEND_AUTO_STOP              (1 << 12)
#define SD_DWC_COMMAND_TRANSFER_MODE_BLOCK         (0 << 11)
#define SD_DWC_COMMAND_TRANSFER_MODE_STREAM        (1 << 11)
#define SD_DWC_COMMAND_READ                        (0 << 10)
#define SD_DWC_COMMAND_WRITE                       (1 << 10)
#define SD_DWC_COMMAND_DATA_EXPECTED               (1 << 9)
#define SD_DWC_COMMAND_CHECK_RESPONSE_CRC          (1 << 8)
#define SD_DWC_COMMAND_LONG_RESPONSE               (1 << 7)
#define SD_DWC_COMMAND_RESPONSE_EXPECTED           (1 << 6)
#define SD_DWC_COMMAND_INDEX_MASK                  (0x3F << 0)
#define SD_DWC_COMMAND_INDEX_SHIFT                 0

//
// Define the SD status register bits.
//

#define SD_DWC_STATUS_DMA_REQUEST              (1 << 31)
#define SD_DWC_STATUS_DMA_ACK                  (1 << 30)
#define SD_DWC_STATUS_FIFO_COUNT_MASK          (0x1FFF << 17)
#define SD_DWC_STATUS_FIFO_COUNT_SHIFT         17
#define SD_DWC_STATUS_RESPONSE_INDEX_MASK      (0x3F << 11)
#define SD_DWC_STATUS_RESPONSE_INDEX_SHIFT     11
#define SD_DWC_STATUS_DATA_STATE_MACHINE_BUSY  (1 << 10)
#define SD_DWC_STATUS_DATA_BUSY                (1 << 9)
#define SD_DWC_STATUS_DATA_3_STATUS            (1 << 8)
#define SD_DWC_STATUS_COMMAND_FSM_STATE_MASK   (0xF << 4)
#define SD_DWC_STATUS_COMMAND_FSM_STATE_SHIFT  4
#define SD_DWC_STATUS_FIFO_FULL                (1 << 3)
#define SD_DWC_STATUS_FIFO_EMPTY               (1 << 2)
#define SD_DWC_STATUS_FIFO_TRANSMIT_WATERMARK  (1 << 1)
#define SD_DWC_STATUS_FIFO_RECEIVE_WATERMARK   (1 << 0)

//
// Define the SD FIFO threshold register bits.
//

#define SD_DWC_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_1     0
#define SD_DWC_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_4     1
#define SD_DWC_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_8     2
#define SD_DWC_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_16    3
#define SD_DWC_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_32    4
#define SD_DWC_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_64    5
#define SD_DWC_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_128   6
#define SD_DWC_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_256   7
#define SD_DWC_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_MASK  (0x7 << 28)
#define SD_DWC_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_SHIFT 28
#define SD_DWC_FIFO_THRESHOLD_RECEIVE_WATERMARK_MASK              (0xFFF << 16)
#define SD_DWC_FIFO_THRESHOLD_RECEIVE_WATERMARK_SHIFT             16
#define SD_DWC_FIFO_THRESHOLD_TRANSMIT_WATERMARK_MASK             (0xFFF << 0)
#define SD_DWC_FIFO_THRESHOLD_TRANSMIT_WATERMARK_SHIFT            0

#define SD_DWC_FIFO_THRESHOLD_DEFAULT                              \
    ((SD_DWC_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_16 <<    \
      SD_DWC_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_SHIFT) | \
     ((((SD_DWC_FIFO_DEPTH / 2) - 1) <<                            \
       SD_DWC_FIFO_THRESHOLD_RECEIVE_WATERMARK_SHIFT) &            \
      SD_DWC_FIFO_THRESHOLD_RECEIVE_WATERMARK_MASK) |              \
     (((SD_DWC_FIFO_DEPTH / 2) <<                                  \
       SD_DWC_FIFO_THRESHOLD_TRANSMIT_WATERMARK_SHIFT) &           \
      SD_DWC_FIFO_THRESHOLD_TRANSMIT_WATERMARK_MASK))

#define SD_DWC_FIFO_DEPTH 0x100

//
// Define the SD UHS register bits.
//

#define SD_DWC_UHS_DDR_MODE     (1 << 16)
#define SD_DWC_UHS_VOLTAGE_MASK (1 << 0)
#define SD_DWC_UHS_VOLTAGE_3V3  (0 << 0)
#define SD_DWC_UHS_VOLTAGE_1V8  (1 << 0)

//
// Define the SD reset register bits.
//

#define SD_DWC_RESET_ENABLE (1 << 0)

//
// Define the SD bus mode register bits.
//

#define SD_DWC_BUS_MODE_BURST_LENGTH_1               0
#define SD_DWC_BUS_MODE_BURST_LENGTH_4               1
#define SD_DWC_BUS_MODE_BURST_LENGTH_8               2
#define SD_DWC_BUS_MODE_BURST_LENGTH_16              3
#define SD_DWC_BUS_MODE_BURST_LENGTH_32              4
#define SD_DWC_BUS_MODE_BURST_LENGTH_64              5
#define SD_DWC_BUS_MODE_BURST_LENGTH_128             6
#define SD_DWC_BUS_MODE_BURST_LENGTH_256             7
#define SD_DWC_BUS_MODE_BURST_LENGTH_MASK            (0x7 << 8)
#define SD_DWC_BUS_MODE_BURST_LENGTH_SHIFT           8
#define SD_DWC_BUS_MODE_IDMAC_ENABLE                 (1 << 7)
#define SD_DWC_BUS_MODE_DESCRIPTOR_SKIP_LENGTH_MASK  (0x1F << 2)
#define SD_DWC_BUS_MODE_DESCRIPTOR_SKIP_LENGTH_SHIFT 2
#define SD_DWC_BUS_MODE_FIXED_BURST                  (1 << 1)
#define SD_DWC_BUS_MODE_INTERNAL_DMA_RESET           (1 << 0)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SD_DWC_REGISTER {
    SdDwcControl = 0x000,
    SdDwcPower = 0x004,
    SdDwcClockDivider = 0x008,
    SdDwcClockSource = 0x00C,
    SdDwcClockEnable = 0x010,
    SdDwcTimeout = 0x014,
    SdDwcCardType = 0x018,
    SdDwcBlockSize = 0x01C,
    SdDwcByteCount = 0x020,
    SdDwcInterruptMask = 0x024,
    SdDwcCommandArgument = 0x028,
    SdDwcCommand = 0x02C,
    SdDwcResponse0 = 0x030,
    SdDwcResponse1 = 0x034,
    SdDwcResponse2 = 0x038,
    SdDwcResponse3 = 0x03C,
    SdDwcMaskedInterruptStatus = 0x040,
    SdDwcInterruptStatus = 0x044,
    SdDwcStatus = 0x048,
    SdDwcFifoThreshold = 0x04C,
    SdDwcCardDetect = 0x050,
    SdDwcWriteProtect = 0x054,
    SdDwcTransferredCiuByteCount = 0x058,
    SdDwcTransferredBiuByteCount = 0x05C,
    SdDwcUhs = 0x074,
    SdDwcResetN = 0x078,
    SdDwcBusMode = 0x080,
    SdDwcDescriptorBaseAddress = 0x088,
    SdDwcFifoBase = 0x200,
} SD_DWC_REGISTER, *PSD_DWC_REGISTER;

/*++

Structure Description:

    This structure defines the initialization parameters passed upon creation
    of a new DesignWare SD controller.

Members:

    ControllerBase - Stores a pointer to the base address of the host
        controller registers.

    Voltages - Stores a bitmask of supported voltages. See SD_VOLTAGE_*
        definitions.

    FundamentalClock - Stores the fundamental clock speed in Hertz.

    HostCapabilities - Stores the host controller capability bits See SD_MODE_*
        definitions.

    OverrideFunctionTable - Stores an optional pointer to a set of functions
        for which the instantiator would like to override the default
        DesignWare behavior. If these are NULL, the default DesignWare SD
        functions are used.

    OverrideContext - Stores a pointer passed to the override functions.

--*/

typedef struct _EFI_SD_DWC_INITIALIZATION_BLOCK {
    VOID *ControllerBase;
    UINT32 Voltages;
    UINT32 FundamentalClock;
    UINT32 HostCapabilities;
    PSD_FUNCTION_TABLE OverrideFunctionTable;
    VOID *OverrideContext;
} EFI_SD_DWC_INITIALIZATION_BLOCK, *PEFI_SD_DWC_INITIALIZATION_BLOCK;

/*++

Structure Description:

    This structure defines the context for a DesignWare SD/MMC controller
    instance.

Members:

    ControllerBase - Stores a pointer to the base address of the host
        controller registers.

    SdController - Stores a pointer to the controller for the SD/MMC library
        instance.

    Voltages - Stores a bitmask of supported voltages.

    HostCapabilities - Stores the host controller capability bits.

    FundamentalClock - Stores the fundamental clock speed in Hertz.

    OverrideFunctionTable - Stores a set of functions for which the
        instantiator would like to override the default DesignWare behavior. If
        these are NULL, the default DesignWare SD functions are used.

    OverrideContext - Stores a pointer passed to the override functions.

--*/

typedef struct _EFI_SD_DWC_CONTROLLER {
    VOID *ControllerBase;
    PEFI_SD_CONTROLLER SdController;
    UINT32 Voltages;
    UINT32 HostCapabilities;
    UINT32 FundamentalClock;
    SD_FUNCTION_TABLE OverrideFunctionTable;
    VOID *OverrideContext;
} EFI_SD_DWC_CONTROLLER, *PEFI_SD_DWC_CONTROLLER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

PEFI_SD_DWC_CONTROLLER
EfiSdDwcCreateController (
    PEFI_SD_DWC_INITIALIZATION_BLOCK Parameters
    );

/*++

Routine Description:

    This routine creates a new DesignWare SD controller object.

Arguments:

    Parameters - Supplies a pointer to the parameters to use when creating the
        controller. This can be stack allocated, as the Rockchip SD device
        won't use this memory after this routine returns.

Return Value:

    Returns a pointer to the controller structure on success.

    NULL on allocation failure or if a required parameter was not filled in.

--*/

VOID
EfiSdDwcDestroyController (
    PEFI_SD_DWC_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys a DesignWare SD controller object.

Arguments:

    Controller - Supplies a pointer to the controller to destroy.

Return Value:

    None.

--*/

EFI_STATUS
EfiSdDwcInitializeController (
    PEFI_SD_DWC_CONTROLLER Controller,
    BOOLEAN SoftReset
    );

/*++

Routine Description:

    This routine resets and initializes the DesignWare SD host controller.

Arguments:

    Controller - Supplies a pointer to the controller to initialize.

    SoftReset - Supplies a boolean indicating whether or not to perform a soft
        reset on the controller.

Return Value:

    Status code.

--*/

EFI_STATUS
EfiSdDwcBlockIoPolled (
    PEFI_SD_DWC_CONTROLLER Controller,
    UINT64 BlockOffset,
    UINTN BlockCount,
    VOID *BufferVirtual,
    BOOLEAN Write
    );

/*++

Routine Description:

    This routine performs a block I/O read or write using the CPU and not
    DMA.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockOffset - Supplies the logical block address of the I/O.

    BlockCount - Supplies the number of blocks to read or write.

    BufferVirtual - Supplies the virtual address of the I/O buffer.

    Write - Supplies a boolean indicating if this is a read operation (FALSE)
        or a write operation.

Return Value:

    Status code.

--*/

EFI_STATUS
EfiSdDwcGetMediaParameters (
    PEFI_SD_DWC_CONTROLLER Controller,
    UINT64 *BlockCount,
    UINT32 *BlockSize
    );

/*++

Routine Description:

    This routine returns information about the media card.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockCount - Supplies a pointer where the number of blocks in the user
        area of the medium will be returned.

    BlockSize - Supplies a pointer where the block size of the medium will be
        returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_NO_MEDIA if there is no card in the slot.

--*/

EFI_STATUS
EfiSdDwcSetClockSpeed (
    PEFI_SD_DWC_CONTROLLER DwcController,
    UINT32 ClockSpeed
    );

/*++

Routine Description:

    This routine sets the controller's clock speed.

Arguments:

    DwcController - Supplies a pointer to this SD Rockchip controller.

    ClockSpeed - Supplies the desired clock speed in Hertz.

Return Value:

    Status code.

--*/

