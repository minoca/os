/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    config.ck

Abstract:

    This module contains the global configuration data for the Santa package
    manager. Users can access this directly via the "config" variable within
    this package.

Author:

    Evan Green 24-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from santa.lib.santaconfig import SantaConfig;

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
// The master configuration.
//

var config = SantaConfig();

//
// Only initialize the configuration once.
//

var _configInitialized = false;

//
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
//

function
loadConfig (
    configpath,
    override
    )

/*++

Routine Description:

    This routine loads the application configuration.

Arguments:

    configpath - Supplies an optional path to use for the configuration file,
        rather than the defaults.

    override - Supplies the override dictionary of configuration options.

Return Value:

    None.

--*/

{

    if (_configInitialized) {
        return;
    }

    config.__init(configpath, override);
    _configInitialized = true;
    return;
}

