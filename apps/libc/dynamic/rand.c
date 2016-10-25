/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rand.c

Abstract:

    This module implements support for the random functions of the C library.

Author:

    Evan Green 11-Jul-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <stdlib.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define constants used in the linear congruential generator.
//

#define RANDOM_MULTIPLIER 1103515245
#define RANDOM_INCREMENT 12345

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

unsigned int ClRandomSeed = 1;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
rand (
    void
    )

/*++

Routine Description:

    This routine returns a pseudo-random number.

Arguments:

    None.

Return Value:

    Returns a pseudo-random integer between 0 and RAND_MAX, inclusive.

--*/

{

    return rand_r(&ClRandomSeed);
}

LIBC_API
int
rand_r (
    unsigned *Seed
    )

/*++

Routine Description:

    This routine implements the re-entrant and thread-safe version of the
    pseudo-random number generator.

Arguments:

    Seed - Supplies a pointer to the seed to use. This seed will be updated
        to contain the next seed.

Return Value:

    Returns a pseudo-random integer between 0 and RAND_MAX, inclusive.

--*/

{

    *Seed = (*Seed * RANDOM_MULTIPLIER) + RANDOM_INCREMENT;
    return *Seed % ((unsigned int)RAND_MAX + 1);
}

LIBC_API
void
srand (
    unsigned Seed
    )

/*++

Routine Description:

    This routine sets the seed for the rand function.

Arguments:

    Seed - Supplies the seed to use.

Return Value:

    None.

--*/

{

    ClRandomSeed = Seed;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

