/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

function build() {
    boot_apps = [
        "//boot/bootman:bootmefi.efi",
        "//boot/loader:loadefi"
    ];

    if (arch == "x86") {
        boot_apps += [
            "//boot/bootman:bootman.bin",
            "//boot/fatboot:fatboot.bin",
            "//boot/loader:loader",
            "//boot/mbr:mbr.bin"
        ];
    }

    entries = group("boot_apps", boot_apps);
    return entries;
}

return build();
