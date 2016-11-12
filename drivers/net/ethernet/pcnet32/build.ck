/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    AMD PCnet32 LANCE

Abstract:

    This module implements the AMD 79C9xx PCnet32 LANCE driver.

Author:

    Chris Stevens 8-Nov-2016

Environment:

    Kernel

--*/

function build() {
    name = "pcnet32";
    sources = [
        "pcnet.c",
        "pcnethw.c"
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
