/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsupc.c

Abstract:

    This module implements ARMv7 processor architecture features.

Author:

    Chris Stevens 19-Mar-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>
#include "../mmp.h"

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
// ------------------------------------------------------------------ Functions
//

VOID
MmpInitializeCpuCaches (
    VOID
    )

/*++

Routine Description:

    This routine initializes the system's processor cache infrastructure.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG CacheTypeRegister;
    ULONG Log2CacheLineSize;

    //
    // The Cache Type Register stores Log base 2 of the number of words in the
    // smallest data and instruction cache lines. On ARM, a word is fixed at
    // 32-bits so multiply 2^x by the size of a ULONG.
    //

    CacheTypeRegister = ArGetCacheTypeRegister();
    Log2CacheLineSize = (CacheTypeRegister &
                         ARMV7_CACHE_TYPE_DATA_CACHE_SIZE_MASK) >>
                        ARMV7_CACHE_TYPE_DATA_CACHE_SIZE_SHIFT;

    MmDataCacheLineSize = (1 << Log2CacheLineSize) * sizeof(ULONG);
    Log2CacheLineSize = CacheTypeRegister &
                        ARMV7_CACHE_TYPE_INSTRUCTION_CACHE_SIZE_MASK;

    MmInstructionCacheLineSize = (1 << Log2CacheLineSize) * sizeof(ULONG);

    //
    // Also determine whether or not the I-cache is virtually indexed.
    //

    if ((CacheTypeRegister & ARMV7_CACHE_TYPE_INSTRUCTION_CACHE_TYPE_MASK) !=
        ARMV7_CACHE_TYPE_INSTRUCTION_CACHE_TYPE_PIPT) {

        MmVirtuallyIndexedInstructionCache = TRUE;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

