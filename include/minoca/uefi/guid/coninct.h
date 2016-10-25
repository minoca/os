/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    coninct.h

Abstract:

    This header contains UEFI GUID definitions for the event group that is
    signaled on the first attempt to check for a keystroke for a console input
    (ConIn) device.

Author:

    Evan Green 17-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define CONNECT_CONIN_EVENT_GUID                            \
    {                                                       \
        0xDB4E8151, 0x57ED, 0x4BED,                         \
        {0x88, 0x33, 0x67, 0x51, 0xB5, 0xD1, 0xA8, 0xD7}    \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
