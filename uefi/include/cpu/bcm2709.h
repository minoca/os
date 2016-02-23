/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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
// Define the channel used to get and set video information by property.
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

#define BCM2709_MAILBOX_TAG_GET_CLOCK_RATE     0x00030002
#define BCM2709_MAILBOX_TAG_SET_CLOCK_RATE     0x00038002
#define BCM2709_MAILBOX_TAG_GET_CLOCK_MAX_RATE 0x00030004

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
    Bcm2709MailboxRead   = 0x0,
    Bcm2709MailboxPeak   = 0x10,
    Bcm2709MailboxSender = 0x14,
    Bcm2709MailboxStatus = 0x18,
    Bcm2709MailboxConfig = 0x1C,
    Bcm2709MailboxWrite  = 0x20
} BCM2709_MAILBOX_REGISTER, *PBCM2709_MAILBOX_REGISTER;

/*++

Structure Description:

    This structure defines the header used when sending property messages to
    the BCM2709 mailbox.

Members:

    Size - Stores the size of the data being sent.

    Code - Stores the status code on return from the mailbox.

--*/

typedef struct _BCM2709_MAILBOX_HEADER {
    UINT32 Size;
    UINT32 Code;
} BCM2709_MAILBOX_HEADER, *PBCM2709_MAILBOX_HEADER;

/*++

Structure Description:

    This structure defines the header for a mailbox tag, that is, an individual
    property's message.

Members:

    Tag - Stores the tag that devices the nature of the mailbox message.

    Size - Stores the number of bytes in the message's buffer.

    Length - Stores the number of bytes sent to the mailbox in the message's
        buffer. On receive, this will contain the number of bytes read from the
        mailbox.

--*/

typedef struct _BCM2709_MAILBOX_TAG {
    UINT32 Tag;
    UINT32 Size;
    UINT32 Length;
} BCM2709_MAILBOX_TAG, *PBCM2709_MAILBOX_TAG;

/*++

Structure Description:

    This structure defines a memory region message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    BaseAddress - Stores the base physical address of the memory region.

    Size - Stores the size of the memory region, in bytes.

--*/

typedef struct _BCM2709_MAILBOX_MEMORY_REGION {
    BCM2709_MAILBOX_TAG TagHeader;
    UINT32 BaseAddress;
    UINT32 Size;
} BCM2709_MAILBOX_MEMORY_REGION, *PBCM2709_MAILBOX_MEMORY_REGION;

/*++

Structure Description:

    This structure defines a device state message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    DeviceId - Stores the identification number for the targeted device.

    State - Stores the desired state of the device.

--*/

typedef struct _BCM2709_MAILBOX_DEVICE_STATE {
    BCM2709_MAILBOX_TAG TagHeader;
    UINT32 DeviceId;
    UINT32 State;
} BCM2709_MAILBOX_DEVICE_STATE, *PBCM2709_MAILBOX_DEVICE_STATE;

/*++

Structure Description:

    This structure defines the get clock rate message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    ClockId - Stores the identification number for the clock.

    Rate - Stores the frequency of the clock in Hz.

--*/

typedef struct _BCM2709_MAILBOX_GET_CLOCK_RATE {
    BCM2709_MAILBOX_TAG TagHeader;
    UINT32 ClockId;
    UINT32 Rate;
} BCM2709_MAILBOX_GET_CLOCK_RATE, *PBCM2709_MAILBOX_GET_CLOCK_RATE;

/*++

Structure Description:

    This structure defines the set clock rate message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    ClockId - Stores the identification number for the clock.

    Rate - Stores the frequency of the clock in Hz.

    SkipSettingTurbo - Stores a boolean indicating whether or not to skip
        setting other high performance ("turbo") settings when the ARM
        frequency is set above the default.

--*/

typedef struct _BCM2709_MAILBOX_SET_CLOCK_RATE {
    BCM2709_MAILBOX_TAG TagHeader;
    UINT32 ClockId;
    UINT32 Rate;
    UINT32 SkipSettingTurbo;
} BCM2709_MAILBOX_SET_CLOCK_RATE, *PBCM2709_MAILBOX_SET_CLOCK_RATE;

/*++

Structure Description:

    This structure defines a video resolution used by the BCM2709 mailbox.

Members:

    Width - Stores the width of the resolution, in pixels.

    Height - Stores the height of the resolution, in pixels.

--*/

typedef struct _BCM2709_RESOLUTION {
    UINT32 Width;
    UINT32 Height;
} BCM2709_RESOLUTION, *PBCM2709_RESOLUTION;

/*++

Structure Description:

    This structure defines a video offset used by the BCM2709 mailbox.

Members:

    X - Stores the horizontal offset.

    Y - Stores the vertical offset.

--*/

typedef struct _BCM2709_OFFSET {
    UINT32 X;
    UINT32 Y;
} BCM2709_OFFSET, *PBCM2709_OFFSET;

/*++

Structure Description:

    This structure defines a video overscan used by the BCM2709 mailbox.

Members:

    Top - Stores the overcan value for the top edge of the screen.

    Bottom - Stores the overcan value for the bottom edge of the screen.

    Left - Stores the overcan value for the left side of the screen.

    Right - Stores the overcan value for the right side of the screen.

--*/

typedef struct _BCM2709_OVERSCAN {
    UINT32 Top;
    UINT32 Bottom;
    UINT32 Left;
    UINT32 Right;
} BCM2709_OVERSCAN, *PBCM2709_OVERSCAN;

/*++

Structure Description:

    This structure defines a frame buffer used by the BCM2709 mailbox.

Members:

    Base - Stores the base address of the frame buffer.

    Size - Stores the size of the frame buffer, in bytes.

--*/

typedef struct _BCM2709_FRAME_BUFFER {
    UINT32 Base;
    UINT32 Size;
} BCM2709_FRAME_BUFFER, *PBCM2709_FRAME_BUFFER;

/*++

Structure Description:

    This structure defines a video resolution message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    Resolution - Stores the resolution to set or receives the current
        resolution.

--*/

typedef struct _BCM2709_MAILBOX_RESOLUTION {
    BCM2709_MAILBOX_TAG TagHeader;
    BCM2709_RESOLUTION Resolution;
} BCM2709_MAILBOX_RESOLUTION, *PBCM2709_MAILBOX_RESOLUTION;

/*++

Structure Description:

    This structure defines a bits per pixel message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    BitsPerPixel - Stores the bits per pixel to set or receives the current
        bits per pixel.

--*/

typedef struct _BCM2709_MAILBOX_BITS_PER_PIXEL {
    BCM2709_MAILBOX_TAG TagHeader;
    UINT32 BitsPerPixel;
} BCM2709_MAILBOX_BITS_PER_PIXEL, *PBCM2709_MAILBOX_BITS_PER_PIXEL;

/*++

Structure Description:

    This structure defines a pixel order message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    PixelOrder - Stores the pixel order to set or receives the current pixel
        order.

--*/

typedef struct _BCM2709_MAILBOX_PIXEL_ORDER {
    BCM2709_MAILBOX_TAG TagHeader;
    UINT32 PixelOrder;
} BCM2709_MAILBOX_PIXEL_ORDER, *PBCM2709_MAILBOX_PIXEL_ORDER;

/*++

Structure Description:

    This structure defines a video alpha mode message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    AlphaMode - Stores the alpha mode to set or receives the current alpha mode.

--*/

typedef struct _BCM2709_MAILBOX_ALPHA_MODE {
    BCM2709_MAILBOX_TAG TagHeader;
    UINT32 AlphaMode;
} BCM2709_MAILBOX_ALPHA_MODE, *PBCM2709_MAILBOX_ALPHA_MODE;

/*++

Structure Description:

    This structure defines a video virtual offset message for the BCM2709
    mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    Offset - Stores the virtual offset to set or receives the current virtual
        offset.

--*/

typedef struct _BCM2709_MAILBOX_VIRTUAL_OFFSET {
    BCM2709_MAILBOX_TAG TagHeader;
    BCM2709_OFFSET Offset;
} BCM2709_MAILBOX_VIRTUAL_OFFSET, *PBCM2709_MAILBOX_VIRTUAL_OFFSET;

/*++

Structure Description:

    This structure defines a video overscan message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    Overcan - Stores the overscan values to set or receives the current
        overscan values.

--*/

typedef struct _BCM2709_MAILBOX_OVERSCAN {
    BCM2709_MAILBOX_TAG TagHeader;
    BCM2709_OVERSCAN Overscan;
} BCM2709_MAILBOX_OVERSCAN, *PBCM2709_MAILBOX_OVERSCAN;

/*++

Structure Description:

    This structure defines a video pitch message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    BytesPerLine - Stores the bytes per scan line of the frame buffer.

--*/

typedef struct _BCM2709_MAILBOX_PITCH {
    BCM2709_MAILBOX_TAG TagHeader;
    UINT32 BytesPerScanLine;
} BCM2709_MAILBOX_PITCH, *PBCM2709_MAILBOX_PITCH;

/*++

Structure Description:

    This structure defines a video frame buffer message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    FrameBuffer - Stores the frame buffer to release or receives a frame buffer
        base and size.

--*/

typedef struct _BCM2709_MAILBOX_FRAME_BUFFER {
    BCM2709_MAILBOX_TAG TagHeader;
    BCM2709_FRAME_BUFFER FrameBuffer;
} BCM2709_MAILBOX_FRAME_BUFFER, *PBCM2709_MAILBOX_FRAME_BUFFER;

/*++

Structure Description:

    This structure defines a board model message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    ModelNumber - Stores the board model number.

--*/

typedef struct _BCM2709_MAILBOX_BOARD_MODEL {
    BCM2709_MAILBOX_TAG TagHeader;
    UINT32 ModelNumber;
} BCM2709_MAILBOX_BOARD_MODEL, *PBCM2709_MAILBOX_BOARD_MODEL;

/*++

Structure Description:

    This structure defines a board revision message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    ModelNumber - Stores the board revision number.

--*/

typedef struct _BCM2709_MAILBOX_BOARD_REVISION {
    BCM2709_MAILBOX_TAG TagHeader;
    UINT32 Revision;
} BCM2709_MAILBOX_BOARD_REVISION, *PBCM2709_MAILBOX_BOARD_REVISION;

/*++

Structure Description:

    This structure defines a board serial number message for the BCM2709
    mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    SerialNumber - Stores the board serial number.

--*/

typedef struct _BCM2709_MAILBOX_BOARD_SERIAL_NUMBER {
    BCM2709_MAILBOX_TAG TagHeader;
    UINT32 SerialNumber[2];
} BCM2709_MAILBOX_BOARD_SERIAL_NUMBER, *PBCM2709_MAILBOX_BOARD_SERIAL_NUMBER;

//
// Define the offsets to power management registers, in bytes.
//

typedef enum _BCM2709_PRM_REGISTER {
    Bcm2709PrmResetControl = 0x1C,
    Bcm2709PrmResetStatus  = 0x20,
    Bcm2709PrmWatchdog     = 0x24,
} BCM2709_PRM_REGISTER, *PBCM2709_PRM_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
