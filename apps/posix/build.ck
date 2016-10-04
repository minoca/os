/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    POSIX

Abstract:

    This module builds the posix skeleton in the bin directory.

Author:

    Evan Green 19-Mar-2015

Environment:

    POSIX

--*/

function build() {
    skel = binroot + "/skel";
    skelbin = skel + "/bin";
    skellib = skel + "/lib";
    skeletc = skel + "/etc";

    binfiles = [
        ["//apps/efiboot:efiboot", "efiboot"],
        ["//apps/mount:mount", "mount"],
        ["//apps/netcon:netcon", "netcon"],
        ["//apps/profile:profile", "profile"],
        ["//apps/setup:msetup", "msetup"],
        ["//apps/swiss:swiss", "swiss"],
        ["//apps/unmount:umount", "umount"],
        ["//apps/debug/client:debug", "debug"]
    ];

    libfiles = [
        ["//apps/libc/crypt:libcrypt", "libcrypt.so.1"],
        ["//apps/libc/dynamic:libc", "libc.so.1"],
        ["//apps/netlink:libnetlink", "libnetlink.so.1"],
        ["//apps/osbase:libminocaos", "libminocaos.so.1"],
    ];

    posixfiles = [
        [skel + "/usr/sbin", "update-rc.d", "0755"],
        [skeletc, "passwd", "0644"],
        [skeletc, "group", "0644"],
        [skeletc, "shadow", "0640"],
        [skeletc, "inittab", "0644"],
        [skeletc, "issue", "0644"],
        [skeletc, "init.d/rc", "0755"],
        [skeletc, "init.d/init-functions", "0755"]
    ];

    all = [];
    entries = [];
    for (file in binfiles) {
        entry = {
            "inputs": [file[0]],
            "output": skelbin + "/" + file[1],
            "label": file[1]
        };

        entries += strip(entry);
        all += [":" + file[1]];
    }

    for (file in libfiles) {
        entry = {
            "inputs": [file[0]],
            "output": skellib + "/" + file[1],
            "label": file[1]
        };

        entries += strip(entry);
        all += [":" + file[1]];
    }

    //
    // Copy the POSIX files with specific permissions.
    //

    for (file in posixfiles) {
        entries += copy(file[1],
                        file[0] + "/" + file[1],
                        file[1],
                        null,
                        file[2]);

        all += [file[1]];
    }

    //
    // Create a symlink to swiss at /bin/sh.
    //

    entry = {
        "label": "sh",
        "output": skelbin + "/sh",
        "inputs": [":swiss"],
        "tool": "symlink",
        "config": {"SYMLINK_IN": "swiss"}
    };

    entries += [entry];
    all += [":sh"];
    entries += group("skel", all);
    return entries;
}

return build();

