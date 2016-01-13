/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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

#include <cpu/bcm2709.h>

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

