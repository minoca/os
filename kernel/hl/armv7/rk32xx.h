/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/soc/rk32xx.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the signature of the RK32xx ACPI table: Rk32
//

#define RK32XX_SIGNATURE 0x32336B52 // '23kR'

#define RK32_ALLOCATION_TAG 0x32336B52 // '23kR'

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

#pragma pack(push, 1)

typedef struct _RK32XX_TABLE {
    DESCRIPTION_HEADER Header;
    ULONGLONG TimerBase[RK32_TIMER_COUNT];
    ULONG TimerGsi[RK32_TIMER_COUNT];
    ULONG TimerCountDownMask;
    ULONG TimerEnabledMask;
} PACKED RK32XX_TABLE, *PRK32XX_TABLE;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the Rk32xx ACPI table.
//

extern PRK32XX_TABLE HlRk32Table;

//
// -------------------------------------------------------- Function Prototypes
//

