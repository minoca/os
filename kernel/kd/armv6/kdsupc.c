/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    kdsupc.c

Abstract:

    This module implements ARMv6 specific support routines for the Kernel
    Debugging subsystem.

Author:

    Chris Stevens 6-Jan-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/dbgproto.h>
#include <minoca/kdebug.h>
#include <minoca/arm.h>
#include "../kdp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the machine architecture.
//

ULONG KdMachineType = MACHINE_TYPE_ARMV6;

//
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
//

