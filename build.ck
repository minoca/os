/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Minoca OS

Abstract:

    This module contains the top level build target for Minoca OS.

Author:

    Evan Green 14-Apr-2016

Environment:

    Build

--*/

import menv;
from menv import setupEnv, group;

function build() {
    var allGroup;
    var entries;
    var tools;

    tools = setupEnv();
    entries = [
        "lib:test_apps",
        "images:"
    ];

    allGroup = group("all", entries);
    allGroup[0].default = true;
    entries = allGroup + tools;
    return entries;
}

