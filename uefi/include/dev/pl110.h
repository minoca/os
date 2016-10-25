/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pl110.h

Abstract:

    This header contains definitions for the ARM PL110 LCD Controller library.

Author:

    Evan Green 7-Apr-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfipPl110Initialize (
    EFI_PHYSICAL_ADDRESS Controller,
    EFI_PHYSICAL_ADDRESS FrameBufferBase,
    UINT32 FrameBufferWidth,
    UINT32 FrameBufferHeight
    );

/*++

Routine Description:

    This routine initialize the PrimeCell PL110 display controller found in
    the Integrator/CP.

Arguments:

    Controller - Supplies the physical address of the PL110 registers.

    FrameBufferBase - Supplies the base of the frame buffer memory to set.

    FrameBufferWidth - Supplies the desired width.

    FrameBufferHeight - Supplies the desired height.

Return Value:

    EFI status code.

--*/

