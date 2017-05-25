/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    file.ck

Abstract:

    This module contains file utilities for the Santa application.

Author:

    Evan Green 24-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import io;

from santa.config import config;

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

function
path (
    filepath
    )

/*++

Routine Description:

    This routine returns the adjusted path for the given file. The root will
    be added on, and if the given path starts with a ~ it will be converted to
    the current home directory.

Arguments:

    filepath - Supplies the file path to convert.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    var root = config.getKey("core.root");

    if (filepath[0..1] == "~/") {
        filepath = config.home + filepath[0..-1];
    }

    if (!root || (root.length() == 0)) {
        return filepath;
    }

    if (root[-1] != "/") {
        root = root + "/";
    }

    return root + filepath;
}

function
open (
    filepath,
    mode
    )

/*++

Routine Description:

    This routine is the same as the open function in the standard io module
    except that it munges the path to prepend the root and trade tildes for
    the home directory.

Arguments:

    path - Supplies the path to open.

    mode - Supplies the mode to open with.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    if (filepath is String) {
        filepath = path(filepath);
    }

    return (io.open)(filepath, mode);
}

//
// --------------------------------------------------------- Internal Functions
//

