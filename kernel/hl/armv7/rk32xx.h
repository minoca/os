/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    rk32xx.h

Abstract:

    This header contains definitions for the hardware modules supporting the
    Rockchip RK32xx SoC.

Author:

    Evan Green 9-Jul-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define RK32_ALLOCATION_TAG 0x32336B52 // '23kR'

//
// Define the signature of the RK32xx ACPI table: Rk32
//

#define RK32XX_SIGNATURE 0x32336B52

//
// Define the number of timers in the SoC.
//

#define RK32_TIMER_COUNT 8

//
// Define attributes of the timers.
//

#define RK32_TIMER_BIT_WIDTH 64
#define RK32_TIMER_FREQUENCY 24000000
#define RK32_TIMER_BLOCK_SIZE 0x1000

//
// Define RK32 timer register bits.
//

//
// Control bits
//

#define RK32_TIMER_CONTROL_ENABLE           0x00000001
#define RK32_TIMER_CONTROL_ONE_SHOT         0x00000002
#define RK32_TIMER_CONTROL_INTERRUPT_ENABLE 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the Rockchip RK32xx ACPI table.

Members:

    Header - Stores the standard ACPI table header. The signature here is
        'Rk32'.

    TimerBase - Stores the array of physical addresses of all the timers.

    TimerGsi - Stores the array of Global System Interrupt numbers for each of
        the timers.

    TimerCountDownMask - Stores a mask of bits, one for each timer, where if a
        bit is set that timer counts down. If the bit for a timer is clear, the
        timer counts up.

    TimerEnabledMask - Stores a bitfield of which timers are available for use
        by the kernel.

--*/

typedef struct _RK32XX_TABLE {
    DESCRIPTION_HEADER Header;
    ULONGLONG TimerBase[RK32_TIMER_COUNT];
    ULONG TimerGsi[RK32_TIMER_COUNT];
    ULONG TimerCountDownMask;
    ULONG TimerEnabledMask;
} PACKED RK32XX_TABLE, *PRK32XX_TABLE;

//
// Define the RK32xx timer register offsets, in bytes.
//

typedef enum _RK32_TIMER_REGISTER {
    Rk32TimerLoadCountLow     = 0x00,
    Rk32TimerLoadCountHigh    = 0x04,
    Rk32TimerCurrentValueLow  = 0x08,
    Rk32TimerCurrentValueHigh = 0x0C,
    Rk32TimerControl          = 0x10,
    Rk32TimerInterruptStatus  = 0x18
} RK32_TIMER_REGISTER, *PRK32_TIMER_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the provided hardware layer services.
//

extern PHARDWARE_MODULE_KERNEL_SERVICES HlRk32KernelServices;

//
// Store a pointer to the Rk32xx ACPI table.
//

extern PRK32XX_TABLE HlRk32Table;

//
// -------------------------------------------------------- Function Prototypes
//

