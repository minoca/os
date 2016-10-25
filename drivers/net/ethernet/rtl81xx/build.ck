/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    RTL 81xx

Abstract:

    This module implements support for the Realtek RTL81xx family of
    ethernet controllers.

Author:

    Chris Stevens 20-June-2014

Environment:

    Kernel

--*/

function build() {
    name = "rtl81xx";
    sources = [
        "rtl81.c",
        "rtl81hw.c"
    ];

    dynlibs = [
        "//drivers/net/netcore:netcore"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

return build();
