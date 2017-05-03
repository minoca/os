/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Chalk

Abstract:

    This directory contains the Chalk language.

Author:

    Evan Green 14-Feb-2017

Environment:

    Any

--*/

from menv import group;

function build() {
    var buildChalk;
    var chalk;
    var entries;

    buildChalk = [
        "apps/ck/lib:build_libchalk_static",
        "apps/ck/lib:build_libchalk_dynamic",
        "apps/ck/app:build_chalk",
        "apps/ck/modules:build_modules",
    ];

    chalk = [
        "apps/ck/lib:libchalk_static",
        "apps/ck/lib:libchalk_dynamic",
        "apps/ck/app:chalk",
        "apps/ck/modules:modules",
    ];

    entries = group("chalk", chalk);
    entries += group("build_chalk", buildChalk);
    return entries;
}

