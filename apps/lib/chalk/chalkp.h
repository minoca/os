/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    chalkp.h

Abstract:

    This header contains internal definitions for the Chalk interpreter. This
    file should not be included by users of the interpreter, only by the
    interpreter core itself.

Author:

    Evan Green 19-Nov-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// The YY library is expected to be linked in to this one.
//

#define YY_API

#include <minoca/types.h>
#include <minoca/status.h>
#include <minoca/lib/yy.h>
#include "chalk.h"

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
