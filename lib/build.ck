/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Libraries

Abstract:

    This directory builds common libraries that run in multiple
    environments.

Author:

    Evan Green 28-Mar-2014

Environment:

    Any

--*/

from menv import group;

function build() {
    var entries;
    var testApps;

    testApps = [
        "lib/crypto/testcryp:",
        "lib/fatlib/fattest:",
        "lib/rtl/testrtl:",
        "lib/yy/yytest:",
        "kernel/mm/testmm:",
    ];

    entries = group("test_apps", testApps);
    return entries;
}

