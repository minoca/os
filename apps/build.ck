/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Applications

Abstract:

    This directory contains user mode applications and libraries.

Author:

    Evan Green 25-Feb-2013

Environment:

    User

--*/

function build() {
    test_apps = [
        "//apps/libc/dynamic/testc:build_testc",
        "//apps/testapps:testapps",
    ];

    libc = [
        "//apps/libc/crypt:libcrypt",
        "//apps/libc/dynamic/pthread/static:libpthread_nonshared",
        "//apps/libc/static:libc_nonshared",
    ];

    mingen_build = [
        "//apps/mingen:mingen_build"
    ];

    apps = [
        "//apps/efiboot:efiboot",
        "//apps/mingen:mingen",
        "//apps/mount:mount",
        "//apps/netcon:netcon",
        "//apps/profile:profile",
        "//apps/setup:msetup",
        "//apps/setup:build_msetup",
        "//apps/swiss:swiss",
        "//apps/swiss:build_swiss",
        "//apps/unmount:umount",
        "//apps/vmstat:vmstat",
    ];

    all_apps = test_apps + libc + mingen_build + apps;
    entries = group("all_apps", all_apps);
    return entries;
}

return build();
