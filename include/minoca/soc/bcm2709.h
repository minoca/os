/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bcm2709.h

Abstract:

    This header contains definitions for Broadcom 2709 System on Chip family.

Author:

    Chris Stevens 7-Jan-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

#define BCM2709_MAILBOX_CHECK_TAG_LENGTH(_TagLength, _ExpectedLength) \
    ((((_TagLength) & BCM2709_MAILBOX_TAG_LENGTH_RESPONSE) != 0) &&   \
     (((_TagLength) & ~BCM2709_MAILBOX_TAG_LENGTH_RESPONSE) ==        \
      (_ExpectedLength)))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the offsets from the platform base for various BCM2709 components
// and their associated sizes, if necessary. The platform base is define
// specifically for each chip in the BCM2709 family.
//

#define BCM2709_SYSTEM_TIMER_OFFSET 0x00003000
#define BCM2709_INTERRUPT_OFFSET    0x0000B200
#define BCM2709_ARM_TIMER_OFFSET    0x0000B400
#define BCM2709_MAILBOX_OFFSET      0x0000B880
#define BCM2709_PRM_OFFSET          0x00100000
#define BCM2709_PRM_SIZE            0x1000
#define BCM2709_CLOCK_OFFSET        0x00101000
#define BCM2709_GPIO_OFFSET         0x00200000
#define BCM2709_UART_OFFSET         0x00201000
#define BCM2709_EMMC_OFFSET         0x00300000

//
// Define the flags for the basic interrupts.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_TIMER            0x00000001
#define BCM2709_INTERRUPT_IRQ_BASIC_MAILBOX          0x00000002
#define BCM2709_INTERRUPT_IRQ_BASIC_DOORBELL0        0x00000004
#define BCM2709_INTERRUPT_IRQ_BASIC_DOORBELL1        0x00000008
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU0_HALTED      0x00000010
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU1_HALTED      0x00000020
#define BCM2709_INTERRUPT_IRQ_BASIC_ILLEGAL_ACCESS_1 0x00000040
#define BCM2709_INTERRUPT_IRQ_BASIC_ILLEGAL_ACCESS_0 0x00000080

#define BCM2709_INTERRUPT_IRQ_BASIC_MASK             0x000000FF

//
// Define the flags for the GPU interrupts included in the basic pending status
// register.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_7            0x00000400
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_9            0x00000800
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_10           0x00001000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_18           0x00002000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_19           0x00004000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_53           0x00008000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_54           0x00010000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_55           0x00020000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_56           0x00040000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_57           0x00080000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_62           0x00100000

#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_MASK         0x001FFC00

//
// Define the number of bits to shift in order to get to the GPU bits in the
// basic pending register.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_SHIFT 10

//
// Define the number of GPU registers whose pending status is expressed in the
// basic pending status register.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_COUNT 11

//
// Define the flags that signify that one of the normal pending status
// registers has a pending interrupt.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_PENDING_1        0x00000100
#define BCM2709_INTERRUPT_IRQ_BASIC_PENDING_2        0x00000200

#define BCM2709_INTERRUPT_IRQ_BASIC_PENDING_MASK     0x00000300

//
// Define the number of GPU interrupt lines on the BCM2709.
//

#define BCM2709_INTERRUPT_GPU_LINE_COUNT 64

//
// Timer Control register bits.
//
// The BCM2709's version of the SP804 does not support one-shot mode and is
// always periodic based on the load value, making those bits defunct. It also
// introduces extra control bits for controlling its extra free-running counter.
//

#define BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_DIVIDE_MASK  0x00FF0000
#define BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_DIVIDE_SHIFT 16
#define BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_ENABLED      0x00000200
#define BCM2709_ARM_TIMER_CONTROL_HALT_ON_DEBUG             0x00000100
#define BCM2709_ARM_TIMER_CONTROL_ENABLED                   0x00000080
#define BCM2709_ARM_TIMER_CONTROL_INTERRUPT_ENABLE          0x00000020
#define BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_1               0x00000000
#define BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_16              0x00000004
#define BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_256             0x00000008
#define BCM2709_ARM_TIMER_CONTROL_32_BIT                    0x00000002
#define BCM2709_ARM_TIMER_CONTROL_16_BIT                    0x00000000

//
// Define the target default frequency to use for the BCM2709 timer, if
// possible.
//

#define BCM2709_ARM_TIMER_TARGET_FREQUENCY 1000000

//
// Define the frequency of the BCM2709 System Timer.
//

#define BCM2709_SYSTEM_TIMER_FREQUENCY 1000000

//
// Run at at a period of 15.625ms.
//

#define BCM2709_CLOCK_TICK_COUNT 15625

//
// Define the maximum predivider.
//

#define BCM2709_TIMER_PREDIVIDER_MAX 0x1FF

//
// Define the GSI for the clock timer.
//

#define BCM2709_CLOCK_TIMER_INTERRUPT 64

//
// Define the channel used to get and set information by property.
//

#define BCM2709_MAILBOX_PROPERTIES_CHANNEL 8

//
// Define status codes for the BCM2709 mailbox.
//

#define BCM2709_MAILBOX_STATUS_SUCCESS     0x80000000
#define BCM2709_MAILBOX_STATUS_PARSE_ERROR 0x80000001

//
// Define tag response acknowledgement flags.
//

#define BCM2709_MAILBOX_TAG_LENGTH_RESPONSE 0x80000000

//
// Define the tag values for getting basic board information.
//

#define BCM2709_MAILBOX_TAG_GET_BOARD_MODEL    0x00010001
#define BCM2709_MAILBOX_TAG_GET_BOARD_REVISION 0x00010002
#define BCM2709_MAILBOX_TAG_GET_BOARD_SERIAL   0x00010004

//
// Define the tag values for getting the memory region.
//

#define BCM2709_MAILBOX_TAG_GET_ARM_CORE_MEMORY   0x00010005
#define BCM2709_MAILBOX_TAG_GET_VIDEO_CORE_MEMORY 0x00010006

//
// Define the tag value for setting device power states.
//

#define BCM2709_MAILBOX_TAG_SET_POWER_STATE 0x00028001

//
// Define the tag value for getting the clock rate.
//

#define BCM2709_MAILBOX_TAG_GET_CLOCK_STATE    0x00030001
#define BCM2709_MAILBOX_TAG_SET_CLOCK_STATE    0x00038001
#define BCM2709_MAILBOX_TAG_GET_CLOCK_RATE     0x00030002
#define BCM2709_MAILBOX_TAG_SET_CLOCK_RATE     0x00038002
#define BCM2709_MAILBOX_TAG_GET_CLOCK_MAX_RATE 0x00030004
#define BCM2709_MAILBOX_TAG_GET_CLOCK_MIN_RATE 0x00030007

//
// Define The bits for the clock state.
//

#define BCM2709_MAILBOX_CLOCK_STATE_ON          0x00000001
#define BCM2709_MAILBOX_CLOCK_STATE_NOT_PRESENT 0x00000002

//
// Define the tag values for various video information.
//

#define BCM2709_MAILBOX_TAG_GET_FRAME_BUFFER        0x00040001
#define BCM2709_MAILBOX_TAG_GET_PHYSICAL_RESOLUTION 0x00040003
#define BCM2709_MAILBOX_TAG_SET_PHYSICAL_RESOLUTION 0x00048003
#define BCM2709_MAILBOX_TAG_GET_VIRTUAL_RESOLUTION  0x00040004
#define BCM2709_MAILBOX_TAG_SET_VIRTUAL_RESOLUTION  0x00048004
#define BCM2709_MAILBOX_TAG_GET_BITS_PER_PIXEL      0x00040005
#define BCM2709_MAILBOX_TAG_SET_BITS_PER_PIXEL      0x00048005
#define BCM2709_MAILBOX_TAG_GET_PIXEL_ORDER         0x00040006
#define BCM2709_MAILBOX_TAG_SET_PIXEL_ORDER         0x00048006
#define BCM2709_MAILBOX_TAG_GET_ALPHA_MODE          0x00040007
#define BCM2709_MAILBOX_TAG_SET_ALPHA_MODE          0x00048007
#define BCM2709_MAILBOX_TAG_GET_PITCH               0x00040008
#define BCM2709_MAILBOX_TAG_GET_VIRTUAL_OFFSET      0x00040009
#define BCM2709_MAILBOX_TAG_SET_VIRTUAL_OFFSET      0x00048009
#define BCM2709_MAILBOX_TAG_GET_OVERSCAN            0x0004000A
#define BCM2709_MAILBOX_TAG_SET_OVERSCAN            0x0004800A

//
// Define the values for BCM2709 video pixel order.
//

#define BCM2709_MAILBOX_PIXEL_ORDER_BGR 0
#define BCM2709_MAILBOX_PIXEL_ORDER_RGB 1

//
// Define the values for the BCM2709 video alpha mode.
//

#define BCM2709_MAILBOX_ALPHA_MODE_OPAQUE 0
#define BCM2709_MAILBOX_ALPHA_MODE_TRANSPARENT 1
#define BCM2709_MAILBOX_ALPHA_MODE_IGNORED 2

//
// Define the values for the BCM2709 devices.
//

#define BCM2709_MAILBOX_DEVICE_SDHCI 0
#define BCM2709_MAILBOX_DEVICE_USB 3

//
// Define the values for the BCM2709 power states.
//

#define BCM2709_MAILBOX_POWER_STATE_ON 3

//
// Define the ID values for the BCM2709 clocks.
//

#define BCM2709_MAILBOX_CLOCK_ID_EMMC   1
#define BCM2709_MAILBOX_CLOCK_ID_UART   2
#define BCM2709_MAILBOX_CLOCK_ID_ARM    3
#define BCM2709_MAILBOX_CLOCK_ID_VIDEO  4
#define BCM2709_MAILBOX_CLOCK_ID_V3D    5
#define BCM2709_MAILBOX_CLOCK_ID_H264   6
#define BCM2709_MAILBOX_CLOCK_ID_ISP    7
#define BCM2709_MAILBOX_CLOCK_ID_SDRAM  8
#define BCM2709_MAILBOX_CLOCK_ID_PIXEL  9
#define BCM2709_MAILBOX_CLOCK_ID_PWM   10

//
// Define values for the mailbox read and write registers.
//

#define BCM2709_MAILBOX_READ_WRITE_CHANNEL_MASK 0x0000000F
#define BCM2709_MAILBOX_READ_WRITE_DATA_SHIFT 4

//
// Define the alignment for all data sent to the mailbox.
//

#define BCM2709_MAILBOX_DATA_ALIGNMENT 0x00000010

//
// Define values for the mailbox status register.
//

#define BCM2709_MAILBOX_STATUS_READ_EMPTY 0x40000000
#define BCM2709_MAILBOX_STATUS_WRITE_FULL 0x80000000

//
// Define the masks for the various pixel order options. The Raspberry Pi seems
// to only support BGR mode.
//

#define BCM2709_BGR_RED_MASK      0x000000FF
#define BCM2709_BGR_GREEN_MASK    0x0000FF00
#define BCM2709_BGR_BLUE_MASK     0x00FF0000
#define BCM2709_BGR_RESERVED_MASK 0xFF000000
#define BCM2709_RGB_RED_MASK      0x00FF0000
#define BCM2709_RGB_GREEN_MASK    0x0000FF00
#define BCM2709_RGB_BLUE_MASK     0x000000FF
#define BCM2709_RGB_RESERVED_MASK 0xFF000000

//
// Define the default bits per pixel.
//

#define BCM2709_DEFAULT_BITS_PER_PIXEL 32

//
// Define the power management password.
//

#define BCM2709_PRM_PASSWORD 0x5A000000

//
// Define the bits for the power management reset control register.
//

#define BCM2709_PRM_RESET_CONTROL_TYPE_MASK 0x00000030
#define BCM2709_PRM_RESET_CONTROL_TYPE_FULL 0x00000020
#define BCM2709_PRM_RESET_CONTROL_RESET     0x00000102

//
// Define the number of ticks to configure the watchdog timer for on reset.
//

#define BCM2709_PRM_WATCHDOG_RESET_TICKS 10

//
// Define the bits for the I2C control register.
//

#define BCM2709_I2C_CONTROL_ENABLE             0x00008000
#define BCM2709_I2C_CONTROL_INTERRUPT_RECEIVE  0x00000400
#define BCM2709_I2C_CONTROL_INTERRUPT_TRANSMIT 0x00000200
#define BCM2709_I2C_CONTROL_INTERRUPT_DONE     0x00000100
#define BCM2709_I2C_CONTROL_START_TRANSFER     0x00000080
#define BCM2709_I2C_CONTROL_CLEAR_FIFO         0x00000030
#define BCM2709_I2C_CONTROL_READ_TRANSFER      0x00000001

#define BCM2709_I2C_BUFFER_SIZE 16

//
// Define the bits for the I2C status register.
//

#define BCM2709_I2C_STATUS_CLOCK_STRETCH_TIMEOUT 0x00000200
#define BCM2709_I2C_STATUS_ACK_ERROR             0x00000100
#define BCM2709_I2C_STATUS_RECEIVE_FIFO_FULL     0x00000080
#define BCM2709_I2C_STATUS_TRANSMIT_FIFO_EMPTY   0x00000040
#define BCM2709_I2C_STATUS_RECEIVE_FIFO_DATA     0x00000020
#define BCM2709_I2C_STATUS_TRANSMIT_FIFO_DATA    0x00000010
#define BCM2709_I2C_STATUS_RECEIVE_FIFO_READING  0x00000008
#define BCM2709_I2C_STATUS_TRANSMIT_FIFO_WRITING 0x00000004
#define BCM2709_I2C_STATUS_TRANSFER_DONE         0x00000002
#define BCM2709_I2C_STATUS_TRANSFER_ACTIVE       0x00000001

//
// Define the bits for the I2C data length register.
//

#define BCM2709_I2C_DATA_LENGTH_MASK  0x0000FFFF
#define BCM2709_I2C_DATA_LENGTH_SHIFT 0

#define BCM2709_I2C_DATA_LENGTH_MAX 0xFFFF

//
// Define the bits for the I2C slave address register.
//

#define BCM2709_I2C_SLAVE_ADDRESS_MASK  0x0000007F
#define BCM2709_I2C_SLAVE_ADDRESS_SHIFT 0

//
// Define the bits for a 10-bit slave address.
//

#define BCM2709_I2C_10_BIT_ADDRESS_HIGH_MASK  0x00000300
#define BCM2709_I2C_10_BIT_ADDRESS_HIGH_SHIFT 8
#define BCM2709_I2C_10_BIT_ADDRESS_LOW_MASK   0x000000FF
#define BCM2709_I2C_10_BIT_ADDRESS_LOW_SHIFT  0

#define BCM2709_I2C_SLAVE_ADDRESS_10_BIT_HIGH_MASK  0x00000003
#define BCM2709_I2C_SLAVE_ADDRESS_10_BIT_HIGH_SHIFT 0
#define BCM2709_I2C_SLAVE_ADDRESS_10_BIT_HEADER     0x00000078

//
// Define the bits for the I2C FIFO register.
//

#define BCM2709_I2C_FIFO_REGISTER_DATA_MASK  0x0000000F
#define BCM2709_I2C_FIFO_REGISTER_DATA_SHIFT 0

//
// Define the bits for the I2C clock divider register.
//

#define BCM2709_I2C_CLOCK_DIVIDER_MASK  0x0000FFFF
#define BCM2709_I2C_CLOCK_DIVIDER_SHIFT 0

//
// Define the bits for the I2C data delay register.
//

#define BCM2709_I2C_DATA_DELAY_FALLING_EDGE_MASK  0xFFFF0000
#define BCM2709_I2C_DATA_DELAY_FALLING_EDGE_SHIFT 16
#define BCM2709_I2C_DATA_DELAY_RISING_EDGE_MASK   0x0000FFFF
#define BCM2709_I2C_DATA_DELAY_RISING_EDGE_SHIFT  0

//
// Define the bits for the I2C clock stretch timeout register.
//

#define BCM2709_I2C_CLOCK_STRETCH_TIMEOUT_VALUE_MASK  0x0000FFFF
#define BCM2709_I2C_CLOCK_STRETCH_TIMEOUT_VALUE_SHIFT 0

//
// Define the GPIO function select values.
//

#define BCM2709_GPIO_FUNCTION_SELECT_INPUT  0x0
#define BCM2709_GPIO_FUNCTION_SELECT_OUTPUT 0x1
#define BCM2709_GPIO_FUNCTION_SELECT_ALT_0  0x4
#define BCM2709_GPIO_FUNCTION_SELECT_ALT_1  0x5
#define BCM2709_GPIO_FUNCTION_SELECT_ALT_2  0x6
#define BCM2709_GPIO_FUNCTION_SELECT_ALT_3  0x7
#define BCM2709_GPIO_FUNCTION_SELECT_ALT_4  0x3
#define BCM2709_GPIO_FUNCTION_SELECT_ALT_5  0x2

#define BCM2709_GPIO_FUNCTION_SELECT_MASK   0x7

//
// Define the number of pins accounted for by each function select register,
// the bit width for each pin, and the byte width of each select register.
//

#define BCM2709_GPIO_FUNCTION_SELECT_PIN_COUNT 10
#define BCM2709_GPIO_FUNCTION_SELECT_PIN_BIT_WIDTH 3
#define BCM2709_GPIO_FUNCTION_SELECT_REGISTER_BYTE_WIDTH 0x4

//
// Define the default headphone jack left and right channel pins.
//

#define BCM2709_GPIO_HEADPHONE_JACK_LEFT 40
#define BCM2709_GPIO_HEADPHONE_JACK_RIGHT 45

//
// Define the default UART transmit and receive pins.
//

#define BCM2709_GPIO_TRANSMIT_PIN 14
#define BCM2709_GPIO_RECEIVE_PIN 15

//
// Define the pull up/down register values.
//

#define BCM2709_GPIO_PULL_NONE 0x0
#define BCM2709_GPIO_PULL_DOWN 0x1
#define BCM2709_GPIO_PULL_UP   0x2

//
// Define the maximum GPIO pin.
//

#define BCM2709_GPIO_PIN_MAX 53

//
// Define the PCM control and status register bits.
//

#define BCM2709_PCM_CONTROL_RAM_STANDBY              0x02000000
#define BCM2709_PCM_CONTROL_CLOCK_SYNC               0x01000000
#define BCM2709_PCM_CONTROL_RECEIVE_SIGN_EXTEND      0x00800000
#define BCM2709_PCM_COTNROL_RECEIVE_FIFO_FULL        0x00400000
#define BCM2709_PCM_CONTROL_TRANSMIT_FIFO_EMPTY      0x00200000
#define BMC2709_PCM_CONTROL_RECEIVE_FIFO_READY       0x00100000
#define BCM2709_PCM_CONTROL_TRANSMIT_FIFO_READY      0x00080000
#define BCM2709_PCM_CONTROL_RECEIVE_FIFO_READ        0x00040000
#define BCM2709_PCM_CONTROL_TRANSMIT_FIFO_WRITE      0x00020000
#define BCM2709_PCM_CONTROL_RECEIVE_FIFO_ERROR       0x00010000
#define BCM2709_PCM_CONTROL_TRANSMIT_FIFO_ERROR      0x00008000
#define BCM2709_PCM_CONTROL_RECEIVE_FIFO_SYNC        0x00004000
#define BCM2709_PCM_CONTROL_TRANSMIT_FIFO_SYNC       0x00002000
#define BCM2709_PCM_CONTROL_DMA_ENABLE               0x00000200
#define BCM2709_PCM_CONTROL_RECEIVE_THRESHOLD_MASK   0x00000180
#define BCM2709_PCM_CONTROL_RECEIVE_THRESHOLD_SHIFT  7
#define BCM2709_PCM_CONTROL_TRANSMIT_THRESHOLD_MASK  0x00000060
#define BCM2709_PCM_CONTROL_TRANSMIT_THRESHOLD_SHIFT 5
#define BCM2709_PCM_CONTROL_RECEIVE_FIFO_CLEAR       0x00000010
#define BCM2709_PCM_CONTROL_TRANSMIT_FIFO_CLEAR      0x00000008
#define BCM2709_PCM_CONTROL_TRANSMIT_ENABLE          0x00000004
#define BCM2709_PCM_CONTROL_RECEIVE_ENABLE           0x00000002
#define BCM2709_PCM_CONTROL_ENABLE                   0x00000001

#define BCM2709_PCM_CONTROL_THRESHOLD_EMPTY  0x0
#define BCM2709_PCM_CONTROL_THRESHOLD_HALF_1 0x1
#define BCM2709_PCM_CONTROL_THRESHOLD_HALF_2 0x2
#define BCM2709_PCM_CONTROL_THRESHOLD_FULL   0x3

//
// Define the PCM mode register bits.
//

#define BCM2709_PCM_MODE_CLOCK_DISABLE           0x10000000
#define BCM2709_PCM_MODE_PDM_DECIMATION_FACTOR   0x08000000
#define BCM2709_PCM_MODE_PDM_INPUT_ENABLE        0x04000000
#define BCM2709_PCM_MODE_RECEIVE_FRAME_PACKED    0x02000000
#define BCM2709_PCM_MODE_TRANSMIT_FRAME_PACKED   0x01000000
#define BCM2709_PCM_MODE_CLOCK_SLAVE             0x00800000
#define BCM2709_PCM_MODE_CLOCK_INVERT            0x00400000
#define BCM2709_PCM_MODE_FRAME_SYNC_SLAVE        0x00200000
#define BCM2709_PCM_MODE_FRAME_SYNC_INVERT       0x00100000
#define BCM2709_PCM_MODE_FRAME_LENGTH_MASK       0x000FFC00
#define BCM2709_PCM_MODE_FRAME_LENGTH_SHIFT      10
#define BCM2709_PCM_MODE_FRAME_SYNC_LENGTH_MASK  0x000003FF
#define BCM2709_PCM_MODE_FRAME_SYNC_LENGTH_SHIFT 0

//
// Define the PCM transmit and receive configuration register bits.
//

#define BCM2709_PCM_CHANNEL_WIDTH_EXTENSION 0x8000
#define BCM2709_PCM_CHANNEL_ENABLE          0x4000
#define BCM2709_PCM_CHANNEL_POSITION_MASK   0x3FF0
#define BCM2709_PCM_CHANNEL_POSITION_SHIFT  4
#define BCM2709_PCM_CHANNEL_WIDTH_MASK      0x000F
#define BCM2709_PCM_CHANNEL_WIDTH_SHIFT     0

#define BCM2709_PCM_CONFIG_CHANNEL_1_MASK  0xFFFF0000
#define BCM2709_PCM_CONFIG_CHANNEL_1_SHIFT 16
#define BCM2709_PCM_CONFIG_CHANNEL_2_MASK  0x0000FFFF
#define BCM2709_PCM_CONFIG_CHANNEL_2_SHIFT 0

//
// Define the PCM DMA request register bits.
//

#define BCM2709_PCM_DMA_TRANSMIT_PANIC_LEVEL_MASK    0x7F000000
#define BCM2709_PCM_DMA_TRANSMIT_PANIC_LEVEL_SHIFT   24
#define BCM2709_PCM_DMA_RECEIVE_PANIC_LEVEL_MASK     0x007F0000
#define BCM2709_PCM_DMA_RECEIVE_PANIC_LEVEL_SHIFT    16
#define BCM2709_PCM_DMA_TRANSMIT_REQUEST_LEVEL_MASK  0x00007F00
#define BCM2709_PCM_DMA_TRANSMIT_REQUEST_LEVEL_SHIFT 8
#define BCM2709_PCM_DMA_RECEIVE_REQUEST_LEVEL_MASK   0x0000007F
#define BCM2709_PCM_DMA_RECEIVE_REQUEST_LEVEL_SHIFT  0

//
// Define the PCM interrupt enable register bits.
//

#define BCM2709_PCM_INTERRUPT_ENABLE_RECEIVE_ERROR  0x00000008
#define BCM2709_PCM_INTERRUPT_ENABLE_TRANSMIT_ERROR 0x00000004
#define BCM2709_PCM_INTERRUPT_ENABLE_RECEIVE_READY  0x00000002
#define BCM2709_PCM_INTERRUPT_ENABLE_TRANSMIT_READY 0x00000001

//
// Define the PCM interrupt status register bits.
//

#define BCM2709_PCM_INTERRUPT_STATUS_RECEIVE_ERROR  0x00000008
#define BCM2709_PCM_INTERRUPT_STATUS_TRANSMIT_ERROR 0x00000004
#define BCM2709_PCM_INTERRUPT_STATUS_RECEIVE_READY  0x00000002
#define BCM2709_PCM_INTERRUPT_STATUS_TRANSMIT_READY 0x00000001

//
// Define the PCM GRAY mode register bits.
//

#define BCM2709_PCM_GRAY_RECEIVE_FIFO_LEVEL_MASK    0x003F0000
#define BCM2709_PCM_GRAY_RECEIVE_FIFO_LEVEL_SHIFT   16
#define BCM2709_PCM_GRAY_FLUSHED_MASK               0x0000FC00
#define BCM2709_PCM_GRAY_FLUSHED_SHIFT              10
#define BCM2709_PCM_GRAY_RECEIVE_BUFFER_LEVEL_MASK  0x000003F0
#define BCM2709_PCM_GRAY_RECEIVE_BUFFER_LEVEL_SHIFT 4
#define BCM2709_PCM_GRAY_FLUSH                      0x00000004
#define BCM2709_PCM_GRAY_CLEAR                      0x00000002
#define BCM2709_PCM_GRAY_ENABLE                     0x00000001

//
// Define the bits for the PWM control register.
//

#define BCM2709_PWM_CONTROL_CHANNEL_2_MASK  0x0000BF00
#define BCM2709_PWM_CONTROL_CHANNEL_2_SHIFT 8
#define BCM2709_PWM_CONTROL_CLEAR_FIFO      0x00000040
#define BCM2709_PWM_CONTROL_CHANNEL_1_MASK  0x000000BF
#define BCM2709_PWM_CONTROL_CHANNEL_1_SHIFT 0

#define BCM2709_PWM_CONTROL_CHANNEL_MS_ENABLE        0x80
#define BCM2709_PWM_CONTROL_CHANNEL_USE_FIFO         0x20
#define BCM2709_PWM_CONTROL_CHANNEL_POLARITY         0x10
#define BCM2709_PWM_CONTROL_CHANNEL_SILENCE          0x08
#define BCM2709_PWM_CONTROL_CHANNEL_REPEAT_LAST_DATA 0x04
#define BCM2709_PWM_CONTROL_CHANNEL_SERIALIZER_MODE  0x02
#define BCM2709_PWM_CONTROL_CHANNEL_ENABLE           0x01

//
// Define the bits for the PWM status register.
//

#define BCM2709_PWM_STATUS_CHANNEL_4_STATE  0x00001000
#define BCM2709_PWM_STATUS_CHANNEL_3_STATE  0x00000800
#define BCM2709_PWM_STATUS_CHANNEL_2_STATE  0x00000400
#define BCM2709_PWM_STATUS_CHANNEL_1_STATE  0x00000200
#define BCM2709_PWM_STATUS_BUS_ERROR        0x00000100
#define BCM2709_PWM_STATUS_CHANNEL_4_GAP    0x00000080
#define BCM2709_PWM_STATUS_CHANNEL_3_GAP    0x00000040
#define BCM2709_PWM_STATUS_CHANNEL_2_GAP    0x00000020
#define BCM2709_PWM_STATUS_CHANNEL_1_GAP    0x00000010
#define BCM2709_PWM_STATUS_FIFO_READ_ERROR  0x00000008
#define BCM2709_PWM_STATUS_FIFO_WRITE_ERROR 0x00000004
#define BCM2709_PWM_STATUS_FIFO_EMPTY       0x00000002
#define BCM2709_PWM_STATUS_FIFO_FULL        0x00000001

//
// Define the bits for the PWM DMA configuration register.
//

#define BCM2709_PWM_DMA_CONFIG_ENABLE             0x80000000
#define BCM2709_PWM_DMA_CONFIG_PANIC_MASK         0x0000FF00
#define BCM2709_PWM_DMA_CONFIG_PANIC_SHIFT        8
#define BCM2709_PWM_DMA_CONFIG_DATA_REQUEST_MASK  0x000000FF
#define BCM2709_PWM_DMA_CONFIG_DATA_REQUEST_SHIFT 0

//
// Define the clock manager password.
//

#define BCM2709_CLOCK_PASSWORD 0x5A000000

//
// Define the bits for the clock manager clock control registers.
//

#define BCM2709_CLOCK_CONTROL_MASH_MASK         0x00000600
#define BCM2709_CLOCK_CONTROL_MASH_SHIFT        9
#define BCM2709_CLOCK_CONTROL_FLIP              0x00000100
#define BCM2709_CLOCK_CONTROL_BUSY              0x00000080
#define BCM2709_CLOCK_CONTROL_KILL              0x00000020
#define BCM2709_CLOCK_CONTROL_ENABLE            0x00000010
#define BCM2709_CLOCK_CONTROL_SOURCE_MASK       0x0000000F
#define BCM2709_CLOCK_CONTROL_SOURCE_SHIFT      0
#define BCM2709_CLOCK_CONTROL_SOURCE_GROUND     0x0
#define BCM2709_CLOCK_CONTROL_SOURCE_OSCILLATOR 0x1
#define BCM2709_CLOCK_CONTROL_SOURCE_DEBUG_0    0x2
#define BCM2709_CLOCK_CONTROL_SOURCE_DEBUG_1    0x3
#define BCM2709_CLOCK_CONTROL_SOURCE_PLLA       0x4
#define BCM2709_CLOCK_CONTROL_SOURCE_PLLC       0x5
#define BCM2709_CLOCK_CONTROL_SOURCE_PLLD       0x6
#define BCM2709_CLOCK_CONTROL_SOURCE_HDMI       0x7

//
// Define the bits for the clock manager divisor registers.
//

#define BCM2709_CLOCK_DIVISOR_INTEGER_MASK   0x00FFF000
#define BCM2709_CLOCK_DIVISOR_INTEGER_SHIFT  12
#define BCM2709_CLOCK_DIVISOR_FRACTION_MASK  0x00000FFF
#define BCM2709_CLOCK_DIVISOR_FRACTION_SHIFT 0

//
// Define the denominator of the fractional divisor.
//

#define BCM2709_CLOCK_DIVISOR_FRACTION_DENOMINATOR 1024

//
// Define the mask used to convert VC bus addresses to ARM core addresses.
//

#define BCM2709_ARM_PHYSICAL_ADDRESS_MASK ~(0xC0000000)

//
// BCM2835 specific definitions
//

#define BCM2835_BASE 0x20000000

//
// BCM2836 specific definitions
//

//
// Define the base address for global system configuration space.
//

#define BCM2836_BASE 0x3F000000

//
// Define the base address for processor local configuration space.
//

#define BCM2836_LOCAL_BASE 0x40000000

//
// Define the core timer register addresses.
//

#define BCM2836_CORE_TIMER_CONTROL    (BCM2836_LOCAL_BASE + 0x00)
#define BCM2836_CORE_TIMER_PRE_SCALER (BCM2836_LOCAL_BASE + 0x08)

//
// Define the bits for the core timer control register.
//

#define BCM2836_CORE_TIMER_CONTROL_INCREMENT_BY_2 0x00000200
#define BCM2836_CORE_TIMER_CONTROL_INCREMENT_BY_1 0x00000000
#define BCM2836_CORE_TIMER_CONTROL_APB_CLOCK      0x00000100
#define BCM2836_CORE_TIMER_CONTROL_CRYSTAL_CLOCK  0x00000000

//
// Define the timer interrupt control registers for each CPU.
//

#define BCM2836_CPU_0_TIMER_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x40)
#define BCM2836_CPU_1_TIMER_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x44)
#define BCM2836_CPU_2_TIMER_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x48)
#define BCM2836_CPU_3_TIMER_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x4C)

//
// Define the mailbox interrupt control registers for each CPU.
//

#define BCM2836_CPU_0_MAILBOX_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x50)
#define BCM2836_CPU_1_MAILBOX_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x54)
#define BCM2836_CPU_2_MAILBOX_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x58)
#define BCM2836_CPU_3_MAILBOX_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x5C)

//
// Define the IRQ pending registers for the four CPUs.
//

#define BCM2836_CPU_0_IRQ_PENDING (BCM2836_LOCAL_BASE + 0x60)
#define BCM2836_CPU_1_IRQ_PENDING (BCM2836_LOCAL_BASE + 0x64)
#define BCM2836_CPU_2_IRQ_PENDING (BCM2836_LOCAL_BASE + 0x68)
#define BCM2836_CPU_3_IRQ_PENDING (BCM2836_LOCAL_BASE + 0x6C)

//
// Define the FIQ pending registers for the four CPUs.
//

#define BCM2836_CPU_0_FIQ_PENDING (BCM2836_LOCAL_BASE + 0x70)
#define BCM2836_CPU_1_FIQ_PENDING (BCM2836_LOCAL_BASE + 0x74)
#define BCM2836_CPU_2_FIQ_PENDING (BCM2836_LOCAL_BASE + 0x78)
#define BCM2836_CPU_3_FIQ_PENDING (BCM2836_LOCAL_BASE + 0x7C)

//
// Define the mailbox set registers for the four CPUs.
//

#define BCM2836_CPU_0_MAILBOX_0_SET (BCM2836_LOCAL_BASE + 0x80)
#define BCM2836_CPU_0_MAILBOX_1_SET (BCM2836_LOCAL_BASE + 0x84)
#define BCM2836_CPU_0_MAILBOX_2_SET (BCM2836_LOCAL_BASE + 0x88)
#define BCM2836_CPU_0_MAILBOX_3_SET (BCM2836_LOCAL_BASE + 0x8C)
#define BCM2836_CPU_1_MAILBOX_0_SET (BCM2836_LOCAL_BASE + 0x90)
#define BCM2836_CPU_1_MAILBOX_1_SET (BCM2836_LOCAL_BASE + 0x94)
#define BCM2836_CPU_1_MAILBOX_2_SET (BCM2836_LOCAL_BASE + 0x98)
#define BCM2836_CPU_1_MAILBOX_3_SET (BCM2836_LOCAL_BASE + 0x9C)
#define BCM2836_CPU_2_MAILBOX_0_SET (BCM2836_LOCAL_BASE + 0xA0)
#define BCM2836_CPU_2_MAILBOX_1_SET (BCM2836_LOCAL_BASE + 0xA4)
#define BCM2836_CPU_2_MAILBOX_2_SET (BCM2836_LOCAL_BASE + 0xA8)
#define BCM2836_CPU_2_MAILBOX_3_SET (BCM2836_LOCAL_BASE + 0xAC)
#define BCM2836_CPU_3_MAILBOX_0_SET (BCM2836_LOCAL_BASE + 0xB0)
#define BCM2836_CPU_3_MAILBOX_1_SET (BCM2836_LOCAL_BASE + 0xB4)
#define BCM2836_CPU_3_MAILBOX_2_SET (BCM2836_LOCAL_BASE + 0xB8)
#define BCM2836_CPU_3_MAILBOX_3_SET (BCM2836_LOCAL_BASE + 0xBC)

//
// Define the mailbox clear registers for the four CPUs.
//

#define BCM2836_CPU_0_MAILBOX_0_CLEAR (BCM2836_LOCAL_BASE + 0xC0)
#define BCM2836_CPU_0_MAILBOX_1_CLEAR (BCM2836_LOCAL_BASE + 0xC4)
#define BCM2836_CPU_0_MAILBOX_2_CLEAR (BCM2836_LOCAL_BASE + 0xC8)
#define BCM2836_CPU_0_MAILBOX_3_CLEAR (BCM2836_LOCAL_BASE + 0xCC)
#define BCM2836_CPU_1_MAILBOX_0_CLEAR (BCM2836_LOCAL_BASE + 0xD0)
#define BCM2836_CPU_1_MAILBOX_1_CLEAR (BCM2836_LOCAL_BASE + 0xD4)
#define BCM2836_CPU_1_MAILBOX_2_CLEAR (BCM2836_LOCAL_BASE + 0xD8)
#define BCM2836_CPU_1_MAILBOX_3_CLEAR (BCM2836_LOCAL_BASE + 0xDC)
#define BCM2836_CPU_2_MAILBOX_0_CLEAR (BCM2836_LOCAL_BASE + 0xE0)
#define BCM2836_CPU_2_MAILBOX_1_CLEAR (BCM2836_LOCAL_BASE + 0xE4)
#define BCM2836_CPU_2_MAILBOX_2_CLEAR (BCM2836_LOCAL_BASE + 0xE8)
#define BCM2836_CPU_2_MAILBOX_3_CLEAR (BCM2836_LOCAL_BASE + 0xEC)
#define BCM2836_CPU_3_MAILBOX_0_CLEAR (BCM2836_LOCAL_BASE + 0xF0)
#define BCM2836_CPU_3_MAILBOX_1_CLEAR (BCM2836_LOCAL_BASE + 0xF4)
#define BCM2836_CPU_3_MAILBOX_2_CLEAR (BCM2836_LOCAL_BASE + 0xF8)
#define BCM2836_CPU_3_MAILBOX_3_CLEAR (BCM2836_LOCAL_BASE + 0xFC)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the offsets to interrupt controller registers, in bytes.
//

typedef enum _BCM2709_INTERRUPT_REGISTER {
    Bcm2709InterruptIrqPendingBasic = 0x00,
    Bcm2709InterruptIrqPending1     = 0x04,
    Bcm2709InterruptIrqPending2     = 0x08,
    Bcm2709InterruptFiqControl      = 0x0C,
    Bcm2709InterruptIrqEnable1      = 0x10,
    Bcm2709InterruptIrqEnable2      = 0x14,
    Bcm2709InterruptIrqEnableBasic  = 0x18,
    Bcm2709InterruptIrqDisable1     = 0x1C,
    Bcm2709InterruptIrqDisable2     = 0x20,
    Bcm2709InterruptIrqDisableBasic = 0x24,
    Bcm2709InterruptSize            = 0x28,
} BCM2709_INTERRUPT_REGISTER, *PBCM2709_INTERRUPT_REGISTER;

//
// Define the interrupt lines for the non GPU interrupts.
//

typedef enum _BCM2709_CPU_INTERRUPT_LINE {
    Bcm2709InterruptArmTimer       = 64,
    Bcm2709InterruptArmMailbox     = 65,
    Bcm2709InterruptArmDoorbell0   = 66,
    Bcm2709InterruptArmDoorbell1   = 67,
    Bcm2709InterruptGpu0Halted     = 68,
    Bcm2709InterruptGpu1Halted     = 69,
    Bcm2709InterruptIllegalAccess1 = 70,
    Bcm2709InterruptIllegalAccess0 = 71,
    Bcm2709InterruptLineCount      = 72
} BCM2709_CPU_INTERRUPT_LINE, *PBCM2709_CPU_INTERRUPT_LINE;

//
// Define the registers for the timer, in bytes.
//

typedef enum _BCM2709_ARM_TIMER_REGISTER {
    Bcm2709ArmTimerLoadValue           = 0x00,
    Bcm2709ArmTimerCurrentValue        = 0x04,
    Bcm2709ArmTimerControl             = 0x08,
    Bcm2709ArmTimerInterruptClear      = 0x0C,
    Bcm2709ArmTimerInterruptRawStatus  = 0x10,
    Bcm2709ArmTimerInterruptStatus     = 0x14,
    Bcm2709ArmTimerBackgroundLoadValue = 0x18,
    Bcm2709ArmTimerPredivider          = 0x1C,
    Bcm2709ArmTimerFreeRunningCounter  = 0x20,
    Bcm2709ArmTimerRegisterSize        = 0x24
} BCM2709_TIMER_REGISTER, *PBCM2709_TIMER_REGISTER;

//
// Define the registers for the System timer, in bytes.
//

typedef enum _BCM2709_SYSTEM_TIMER_REGISTER {
    Bcm2709SystemTimerControl      = 0x00,
    Bcm2709SystemTimerCounterLow   = 0x04,
    Bcm2709SystemTimerCounterHigh  = 0x08,
    Bcm2709SystemTimerCompare0     = 0x0C,
    Bcm2709SystemTimerCompare1     = 0x10,
    Bcm2709SystemTimerCompare2     = 0x14,
    Bcm2709SystemTimerCompare3     = 0x18,
    Bcm2709SystemTimerRegisterSize = 0x1C
} BCM2709_SYSTEM_TIMER_REGISTER, *PBCM2709_SYSTEM_TIMER_REGISTER;

//
// Register set definition for the BCM2709 mailbox. These are offsets in bytes,
//

typedef enum _BCM2709_MAILBOX_REGISTER {
    Bcm2709MailboxRead   = 0x00,
    Bcm2709MailboxPeak   = 0x10,
    Bcm2709MailboxSender = 0x14,
    Bcm2709MailboxStatus = 0x18,
    Bcm2709MailboxConfig = 0x1C,
    Bcm2709MailboxWrite  = 0x20
} BCM2709_MAILBOX_REGISTER, *PBCM2709_MAILBOX_REGISTER;

//
// Define the offsets to power management registers, in bytes.
//

typedef enum _BCM2709_PRM_REGISTER {
    Bcm2709PrmResetControl = 0x1C,
    Bcm2709PrmResetStatus  = 0x20,
    Bcm2709PrmWatchdog     = 0x24,
} BCM2709_PRM_REGISTER, *PBCM2709_PRM_REGISTER;

//
// Define the offsets to I2C registers, in bytes.
//

typedef enum _BCM2709_I2C_REGISTER {
    Bcm2709I2cControl             = 0x00,
    Bcm2709I2cStatus              = 0x04,
    Bcm2709I2cDataLength          = 0x08,
    Bcm2709I2cSlaveAddress        = 0x0C,
    Bcm2709I2cDataFifo            = 0x10,
    Bcm2709I2cClockDivider        = 0x14,
    Bcm2709I2cDataDelay           = 0x18,
    Bcm2709I2cClockStretchTimeout = 0x1c
} BCM2709_I2C_REGISTER, *PBCM2709_I2C_REGISTER;

//
// Define the offsets to the GPIO registers, in bytes.
//

typedef enum _BCM2709_GPIO_REGISTER {
    Bcm2709GpioSelect0                    = 0x00,
    Bcm2709GpioSelect1                    = 0x04,
    Bcm2709GpioSelect2                    = 0x08,
    Bcm2709GpioSelect3                    = 0x0C,
    Bcm2709GpioSelect4                    = 0x10,
    Bcm2709GpioSelect5                    = 0x14,
    Bcm2709GpioPinOutputSet0              = 0x1C,
    Bcm2709GpioPinOutputSet1              = 0x20,
    Bcm2709GpioPinOutputClear0            = 0x28,
    Bcm2709GpioPinOutputClear1            = 0x2C,
    Bcm2709GpioPinLevel0                  = 0x34,
    Bcm2709GpioPinLevel1                  = 0x38,
    Bcm2709GpioPinEventDetectStatus0      = 0x40,
    Bcm2709GpioPinEventDetectStatus1      = 0x44,
    Bcm2709GpioPinRisingEdgeDetect0       = 0x4C,
    Bcm2709GpioPinRisingEdgeDetect1       = 0x50,
    Bcm2709GpioPinFallingEdgeDetect0      = 0x58,
    Bcm2709GpioPinFallingEdgeDetect1      = 0x5C,
    Bcm2709GpioPinHighDetect0             = 0x64,
    Bcm2709GpioPinHighDetect1             = 0x68,
    Bcm2709GpioPinLowDetect0              = 0x70,
    Bcm2709GpioPinLowDetect1              = 0x74,
    Bcm2709GpioPinAsyncRisingEdgeDetect0  = 0x7C,
    Bcm2709GpioPinAsyncRisingEdgeDetect1  = 0x80,
    Bcm2709GpioPinAsyncFallingEdgeDetect0 = 0x88,
    Bcm2709GpioPinAsyncFallingEdgeDetect1 = 0x8C,
    Bcm2709GpioPinPullUpDownEnable        = 0x94,
    Bcm2709GpioPinPullUpDownClock0        = 0x98,
    Bcm2709GpioPinPullUpDownClock1        = 0x9C,
} BCM2709_GPIO_REGISTER, *PBCM2709_GPIO_REGISTER;

//
// Define the offsets of the PCM registers, in bytes.
//

typedef enum _BCM2709_PCM_REGISTER {
    Bcm2709PcmControl         = 0x00,
    Bcm2709PcmFifoData        = 0x04,
    Bcm2709PcmMode            = 0x08,
    Bcm2709PcmReceiveConfig   = 0x0C,
    Bcm2709PcmTransmitConfig  = 0x10,
    Bcm2709PcmDmaRequestLevel = 0x14,
    Bcm2709PcmInterruptEnable = 0x18,
    Bcm2709PcmInterruptStatus = 0x1C,
    Bcm2709PcmGrayModeControl = 0x20
} BCM2709_PCM_REGISTER, *PBCM2709_PCM_REGISTER;

//
// Define the offsets of the PWM registers, in bytes.
//

typedef enum _BCM2709_PWM_REGISTER {
    Bcm2709PwmControl       = 0x00,
    Bcm2709PwmStatus        = 0x04,
    Bcm2709PwmDmaConfig     = 0x08,
    Bcm2709PwmChannel1Range = 0x10,
    Bcm2709PwmChannel1Data  = 0x14,
    Bcm2709PwmFifo          = 0x18,
    Bcm2709PwmChannel2Range = 0x20,
    Bcm2709PwmChannel2Data  = 0x24,
} BCM2709_PWM_REGISTER, *PBCM2709_PWM_REGISTER;

//
// Define the clock manager register offsets, in bytes.
//

typedef enum _BCM2709_CLOCK_REGISTER {
    Bcm2709ClockPwmControl = 0xA0,
    Bcm2709ClockPwmDivisor = 0xA4
} BCM2709_CLOCK_REGISTER, *PBCM2709_CLOCK_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
