/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    emptyrd.c

Abstract:

    This module implements stub symbols for an empty RAM disk.

Author:

    Evan Green 13-Apr-2016

Environment:

    Firmware

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
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// The RAM disk is embedded in the firmware image.
//

const char _binary_ramdisk_start;
const char _binary_ramdisk_end;

//
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
//

