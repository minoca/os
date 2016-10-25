/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    elf64.c

Abstract:

    This module implements support for loading and processing 64-bit ELF
    binaries. It is really just a recompile of the 32-bit code with some
    macros defined differently.

Author:

    Evan Green 8-Apr-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Recompile everything in elf.c, except with the 64 bit macro enabled.
// A little off the beaten path, but way better than maintaining two copies of
// the ELF code.
//

#define WANT_ELF64 1

#include "elf.c"

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

//
// --------------------------------------------------------- Internal Functions
//

