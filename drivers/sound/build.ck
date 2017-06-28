/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Sound

Abstract:

    This directory contains drivers related to sound input and output.

Author:

    Chris Stevens 29-Mar-2017

Environment:

    Kernel

--*/

from menv import group, mconfig;

function build() {
    var arch = mconfig.arch;
    var entries;
    var soundDrivers;

    if ((arch == "armv7") || (arch == "armv6")) {
        soundDrivers = [
            "drivers/sound/broadcom/bc27pwma:bc27pwma"
        ];

    } else if ((arch == "x86") || (arch == "x64")) {
        soundDrivers = [
            "drivers/sound/intel/hda:intelhda"
        ];
    }

    entries = group("sound_drivers", soundDrivers);
    return entries;
}

