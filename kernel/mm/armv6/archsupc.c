/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsupc.c

Abstract:

    This module implements ARMv6 processor architecture features.

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
    ULONG LengthField;

    //
    // The Cache Type Register stores an off-by-one shift of the number of
    // words in the smallest data and instruction cache lines. On ARM, a word
    // is fixed at 32-bits so multiply (1 << (x + 1)) by the size of a ULONG.
    //

    CacheTypeRegister = ArGetCacheTypeRegister();

    ASSERT((CacheTypeRegister & ARMV6_CACHE_TYPE_SEPARATE_MASK) != 0);

    LengthField = (CacheTypeRegister &
                   ARMV6_CACHE_TYPE_DATA_CACHE_LENGTH_MASK) >>
                  ARMV6_CACHE_TYPE_DATA_CACHE_LENGTH_SHIFT;

    MmDataCacheLineSize = (1 << (LengthField + 1)) * sizeof(ULONG);
    LengthField = CacheTypeRegister &
                  ARMV6_CACHE_TYPE_INSTRUCTION_CACHE_LENGTH_MASK;

    MmInstructionCacheLineSize = (1 << (LengthField + 1)) * sizeof(ULONG);

    //
    // ARMv6 instruction caches are always assumed to be virtually indexed.
    //

    MmVirtuallyIndexedInstructionCache = TRUE;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

