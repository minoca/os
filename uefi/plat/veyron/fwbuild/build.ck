/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Veyron Firmware Image Builder

Abstract:

    This build utility adds the necessary keyblock and preamble for booting
    a firmware image on a Rk3288 Veyron SoC.

Author:

    Chris Stevens 7-Jul-2015

Environment:

    Build

--*/

from menv import application;

function build() {
    var app;
    var entries;
    var includes;
    var libs;
    var sources;
    var tool;

    sources = [
        "fwbuild.c",
    ];

    libs = [
        "lib/crypto/ssl:build_ssl",
        "lib/crypto:build_crypto",
        "lib/rtl/base:build_basertl",
        "lib/rtl/urtl:build_rtlc",
    ];

    includes = [
        "$S/uefi/include"
    ];

    app = {
        "label": "veyrnfwb",
        "inputs": sources + libs,
        "includes": includes,
        "build": true
    };

    entries = application(app);
    tool = {
        "type": "tool",
        "name": "veyrnfwb",
        "command": "$O/uefi/plat/veyron/fwbuild/veyrnfwb $text_address "
                   "$IN $OUT",

        "description": "Creating Verified Boot Image - $OUT"
    };

    entries += [tool];
    return entries;
}

