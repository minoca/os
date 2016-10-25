/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Test Applications

Abstract:

    This module contains applications used to test portions of
    functionality during development of the operating system.

Author:

    Evan Green 6-May-2013

Environment:

    User

--*/

function build() {
    app_names = [
        "dbgtest",
        "filetest",
        "ktest",
        "mmaptest",
        "mnttest",
        "pathtest",
        "perftest",
        "sigtest",
        "socktest",
        "utmrtest"
    ];

    apps = [];
    for (app in app_names) {
        apps += ["//apps/testapps/" + app + ":" + app];
    }

    testapps_group = group("testapps", apps);
    return testapps_group;
}

return build();
