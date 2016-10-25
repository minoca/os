/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    endian.h

Abstract:

    This header contains definitions for the endian-ness of the machine.

Author:

    Evan Green 16-Apr-2015

--*/

#ifndef _ENDIAN_H
#define _ENDIAN_H

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// This machine is little endian.
//

#define BYTE_ORDER LITTLE_ENDIAN

#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN 4321

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#endif

