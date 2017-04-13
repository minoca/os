/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tty.h

Abstract:

    This header contains the definition of a global variable that converts
    baud rates into speed_t values.

Author:

    Evan Green 12-Apr-2017

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <termios.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores a terminal baud rate speed conversion.

Members:

    Name - Stores the name of the speed.

    Value - Stores the value to set in the baud rate member.

    Rate - Stores the actual baud rate as a decimal value.

--*/

typedef struct _TTY_BAUD_RATE {
    PSTR Name;
    speed_t Value;
    ULONG Rate;
} TTY_BAUD_RATE, *PTTY_BAUD_RATE;

//
// -------------------------------------------------------------------- Globals
//

TTY_BAUD_RATE TtyBaudRates[] = {
    {"0", B0, 0},
    {"50", B50, 50},
    {"75", B75, 75},
    {"110", B110, 110},
    {"134", B134, 134},
    {"150", B150, 150},
    {"200", B200, 200},
    {"300", B300, 300},
    {"600", B600, 600},
    {"1200", B1200, 1200},
    {"1800", B1800, 1800},
    {"2400", B2400, 2400},
    {"4800", B4800, 4800},
    {"9600", B9600, 9600},
    {"19200", B19200, 19200},
    {"38400", B38400, 38400},
    {"57600", B57600, 57600},
    {"115200", B115200, 115200},
    {"230400", B230400, 230400},

#ifdef B460800
    {"460800", B460800, 460800},
#endif

#ifdef B500000
    {"500000", B500000, 500000},
#endif

#ifdef B576000
    {"576000", B576000, 576000},
#endif

#ifdef B921600
    {"921600", B921600, 921600},
#endif

#ifdef B1000000
    {"1000000", B1000000, 1000000},
#endif

#ifdef B152000
    {"1152000", B1152000, 1152000},
#endif

#ifdef B1500000
    {"1500000", B1500000, 1500000},
#endif

#ifdef B2000000
    {"2000000", B2000000, 2000000},
#endif

#ifdef B2500000
    {"2500000", B2500000, 2500000},
#endif

#ifdef B3000000
    {"3000000", B3000000, 3000000},
#endif

#ifdef B3500000
    {"3500000", B3500000, 3500000},
#endif

#ifdef B4000000
    {"4000000", B4000000, 4000000},
#endif

    {NULL, 0, 0}
};

//
// -------------------------------------------------------- Function Prototypes
//
