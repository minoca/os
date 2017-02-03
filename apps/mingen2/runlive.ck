/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    runlive.ck

Abstract:

    This module runs the mingen application directly without being compiled
    into a bundle.

Author:

    30-Jan-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from mingen import main;
from os import exit;

//
// --------------------------------------------------------------------- Macros
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
// ------------------------------------------------------------------ Functions
//

exit(main());

//
// --------------------------------------------------------- Internal Functions
//

