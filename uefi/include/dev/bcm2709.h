/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bcm2709.h

Abstract:

    This header contains definitions for the BCM2709 UEFI device library.

Author:

    Chris Stevens 18-Mar-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/soc/bcm2709.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts a BCM2709 device offset into the base address.
//

#define BCM2709_GET_BASE(_Offset) (EfiBcm2709Base + (_Offset))

//
// These macros define the device base addresses based on the fixed offsets.
//

#define BCM2709_SYSTEM_TIMER_BASE BCM2709_GET_BASE(BCM2709_SYSTEM_TIMER_OFFSET)
#define BCM2709_INTERRUPT_BASE BCM2709_GET_BASE(BCM2709_INTERRUPT_OFFSET)
#define BCM2709_ARM_TIMER_BASE BCM2709_GET_BASE(BCM2709_ARM_TIMER_OFFSET)
#define BCM2709_MAILBOX_BASE BCM2709_GET_BASE(BCM2709_MAILBOX_OFFSET)
#define BCM2709_PRM_BASE BCM2709_GET_BASE(BCM2709_PRM_OFFSET)
#define BCM2709_CLOCK_BASE BCM2709_GET_BASE(BCM2709_CLOCK_OFFSET)
#define BCM2709_GPIO_BASE BCM2709_GET_BASE(BCM2709_GPIO_OFFSET)
#define BCM2709_UART_BASE BCM2709_GET_BASE(BCM2709_UART_OFFSET)
#define BCM2709_EMMC_BASE BCM2709_GET_BASE(BCM2709_EMMC_OFFSET)

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

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

/*++

Structure Description:

    This structure defines a BCM2709 timer.

Members:

    ClockTimer - Stores a boolean indicating whether or not the timer is the
        clock timer (TRUE) or the time counter (FALSE).

    Predivider - Stores the predivider to use when initializing the clock timer.

--*/

typedef struct _BCM2709_TIMER {
    BOOLEAN ClockTimer;
    UINT32 Predivider;
} BCM2709_TIMER, *PBCM2709_TIMER;

//
// -------------------------------------------------------------------- Globals
//

//
// Store the base address of the BCM2709 device registers.
//

extern VOID *EfiBcm2709Base;

//
// Store whether or not the BCM2709 device library has been initialized.
//

extern BOOLEAN EfiBcm2709Initialized;

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfipBcm2709Initialize (
    VOID *PlatformBase
    );

/*++

Routine Description:

    This routine initializes the BCM2709 UEFI device library.

Arguments:

    PlatformBase - Supplies the base address for the BCM2709 device registers.

Return Value:

    Status code.

--*/

EFI_STATUS
EfipBcm2709InterruptInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes a BCM2709 Interrupt Controller.

Arguments:

    PlatformBase - Supplies the platform's BCM2709 register base address.

Return Value:

    EFI Status code.

--*/

VOID
EfipBcm2709InterruptBeginInterrupt (
    UINT32 *InterruptNumber,
    VOID **InterruptContext
    );

/*++

Routine Description:

    This routine is called when an interrupts comes in. This routine is
    responsible for reporting the interrupt number.

Arguments:

    InterruptNumber - Supplies a pointer where interrupt line number will be
        returned.

    InterruptContext - Supplies a pointer where the platform can store a
        pointer's worth of context that will be passed back when ending the
        interrupt.

Return Value:

    None.

--*/

VOID
EfipBcm2709InterruptEndInterrupt (
    UINT32 InterruptNumber,
    VOID *InterruptContext
    );

/*++

Routine Description:

    This routine is called to finish handling of a platform interrupt. This is
    where the End-Of-Interrupt would get sent to the interrupt controller.

Arguments:

    InterruptNumber - Supplies the interrupt number that occurred.

    InterruptContext - Supplies the context returned by the interrupt
        controller when the interrupt began.

Return Value:

    None.

--*/

EFI_STATUS
EfipBcm2709InterruptSetInterruptLineState (
    UINT32 LineNumber,
    BOOLEAN Enabled,
    BOOLEAN EdgeTriggered
    );

/*++

Routine Description:

    This routine enables or disables an interrupt line.

Arguments:

    LineNumber - Supplies the line number to enable or disable.

    Enabled - Supplies a boolean indicating if the line should be enabled or
        disabled.

    EdgeTriggered - Supplies a boolean indicating if the interrupt is edge
        triggered (TRUE) or level triggered (FALSE).

Return Value:

    EFI Status code.

--*/

VOID
EfipBcm2709MailboxSend (
    UINT32 Channel,
    VOID *Data
    );

/*++

Routine Description:

    This routine sends the given data to the specified mailbox channel.

Arguments:

    Channel - Supplies the mailbox channel to which the data should be sent.

    Data - Supplies the data buffer to send to the mailbox.

Return Value:

    None.

--*/

EFI_STATUS
EfipBcm2709MailboxReceive (
    UINT32 Channel,
    VOID **Data
    );

/*++

Routine Description:

    This routine receives data from the given mailbox channel.

Arguments:

    Channel - Supplies the mailbox channel from which data is expected.

    Data - Supplies a pointer that receives the data buffer returned by the
        mailbox.

Return Value:

    Status code.

--*/

EFI_STATUS
EfipBcm2709MailboxSendCommand (
    UINT32 Channel,
    VOID *Command,
    UINT32 CommandSize,
    BOOLEAN Set
    );

/*++

Routine Description:

    This routine sends the given command to the given channel of the BCM2709's
    mailbox. If it is a GET request, then the data will be returned in the
    supplied command buffer.

Arguments:

    Channel - Supplies the mailbox channel that is to receive the command.

    Command - Supplies the command to send.

    CommandSize - Supplies the size of the command to send.

    Set - Supplies a boolean indicating whether or not the command is a SET
        (TRUE) or GET (FALSE) request.

Return Value:

    Status code.

--*/

EFI_STATUS
EfipBcm2709TimerInitialize (
    PBCM2709_TIMER Timer
    );

/*++

Routine Description:

    This routine initializes a BCM2709 timer.

Arguments:

    Timer - Supplies the pointer to the timer data.

Return Value:

    Status code.

--*/

UINT64
EfipBcm2709TimerRead (
    PBCM2709_TIMER Timer
    );

/*++

Routine Description:

    This routine returns the hardware counter's raw value.

Arguments:

    Timer - Supplies the pointer to the timer data.

Return Value:

    Returns the timer's current count.

--*/

VOID
EfipBcm2709TimerArm (
    PBCM2709_TIMER Timer,
    UINT64 TickCount
    );

/*++

Routine Description:

    This routine arms the timer to fire an interrupt after the specified number
    of ticks.

Arguments:

    Timer - Supplies the pointer to the timer data.

    Periodic - Supplies a boolean indicating if the timer should be armed
        periodically or one-shot.

    TickCount - Supplies the interval, in ticks, from now for the timer to fire
        in.

Return Value:

    None.

--*/

VOID
EfipBcm2709TimerDisarm (
    PBCM2709_TIMER Timer
    );

/*++

Routine Description:

    This routine disarms the timer, stopping interrupts from firing.

Arguments:

    Timer - Supplies the pointer to the timer data.

Return Value:

    None.

--*/

VOID
EfipBcm2709TimerAcknowledgeInterrupt (
    PBCM2709_TIMER Timer
    );

/*++

Routine Description:

    This routine performs any actions necessary upon reciept of a timer's
    interrupt. This may involve writing to an acknowledge register to re-enable
    the timer to fire again, or other hardware specific actions.

Arguments:

    Timer - Supplies the pointer to the timer data.

Return Value:

    None.

--*/

EFI_STATUS
EfipBcm2709GetInitialMemoryMap (
    EFI_MEMORY_DESCRIPTOR **Map,
    UINTN *MapSize
    );

/*++

Routine Description:

    This routine returns the initial platform memory map to the EFI core. The
    core maintains this memory map. The memory map returned does not need to
    take into account the firmware image itself or stack, the EFI core will
    reserve those regions automatically.

Arguments:

    Map - Supplies a pointer where the array of memory descriptors constituting
        the initial memory map is returned on success. The EFI core will make
        a copy of these descriptors, so they can be in read-only or
        temporary memory.

    MapSize - Supplies a pointer where the number of elements in the initial
        memory map will be returned on success.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipBcm2709GpioFunctionSelect (
    UINT32 Pin,
    UINT32 Mode
    );

/*++

Routine Description:

    This routine sets the given mode for the pin's function select.

Arguments:

    Pin - Supplies the GPIO pin whose function select to modify.

    Mode - Supplies the function select mode to set.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipBcm2709UsbInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initialize the USB device on Broadcom 2709 SoCs.

Arguments:

    None.

Return Value:

    None.

--*/

EFI_STATUS
EfipBcm2709PwmInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes the PWM controller making sure that it is exposed
    on GPIO pins 40 and 45. This allows audio to be generated using PWM and it
    will go out the headphone jack.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipBcm2709EnumerateSd (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the SD card on the BCM2709.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipBcm2709EnumerateVideo (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the display on BCM2709 SoCs.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipBcm2709EnumerateSerial (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the serial port on BCM2709 SoCs.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

