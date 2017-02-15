/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Boot

Abstract:

    This module implements support for the boot environment, which contains
    the applications and support code needed to load and launch the
    operating system kernel. It consists of some assembly bootstrap code,
    a boot manager, and an OS loader.

Author:

    Evan Green 26-Jul-2012

Environment:

    Boot

--*/

from menv import group, mconfig;

function build() {
    var arch = mconfig.arch;
    var bootApps;
    var entries;

    bootApps = [
        "boot/bootman:bootmefi.efi",
        "boot/loader:loadefi"
    ];

    if (arch == "x86") {
        bootApps += [
            "boot/bootman:bootman.bin",
            "boot/fatboot:fatboot.bin",
            "boot/loader:loader",
            "boot/mbr:mbr.bin"
        ];
    }

    entries = group("boot_apps", bootApps);
    return entries;
}

