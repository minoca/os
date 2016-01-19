/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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

