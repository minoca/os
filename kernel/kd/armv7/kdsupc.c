/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdsupc.c

Abstract:

    This module implements ARMv7 specific support routines for the Kernel
    Debugging subsystem.

Author:

    Chris Stevens 6-Jan-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/debug/dbgproto.h>
#include <minoca/kernel/kdebug.h>
#include <minoca/kernel/arm.h>
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

ULONG KdMachineType = MACHINE_TYPE_ARM;

//
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
//

