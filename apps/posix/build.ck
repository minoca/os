/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    POSIX

Abstract:

    This module builds the posix skeleton in the bin directory.

Author:

    Evan Green 19-Mar-2015

Environment:

    POSIX

--*/

from menv import copy, group, makedir, mconfig, strip, touch;

function build() {
    var all;
    var binfiles;
    var emptydirs;
    var emptyfiles;
    var entries;
    var entry;
    var libfiles;
    var posixfiles;
    var skel = mconfig.binroot + "/skel";
    var skelbin = skel + "/bin";
    var skeletc = skel + "/etc";
    var skellib = skel + "/lib";
    var updateRcCommand;
    var updateRcTool;

    binfiles = [
        ["apps/debug/client:debug", "debug"],
        ["apps/efiboot:efiboot", "efiboot"],
        ["apps/mount:mount", "mount"],
        ["apps/setup:msetup", "msetup"],
        ["apps/netcon:netcon", "netcon"],
        ["apps/profile:profile", "profile"],
        ["apps/swiss:swiss", "sh"],
        ["apps/tzset:tzset", "tzset"],
        ["apps/unmount:umount", "umount"],
    ];

    libfiles = [
        ["apps/libc/crypt:libcrypt", "libcrypt.so.1"],
        ["apps/libc/dynamic:libc", "libc.so.1"],
        ["apps/netlink:libnetlink", "libnetlink.so.1"],
        ["apps/osbase:libminocaos", "libminocaos.so.1"],
    ];

    posixfiles = [
        [skel + "/usr/sbin", "update-rc.d", "0755"],
        [skeletc, "passwd", "0644"],
        [skeletc, "group", "0644"],
        [skeletc, "shadow", "0640"],
        [skeletc, "inittab", "0644"],
        [skeletc, "issue", "0644"],
        [skeletc, "init.d/rc", "0755"],
        [skeletc, "init.d/init-functions", "0755"],
        [skeletc, "init.d/hostname.sh", "0755"]
    ];

    emptyfiles = [
        [skel + "/var/run", "utmp", "0664"],
        [skel + "/var/log", "wtmp", "0664"]
    ];

    emptydirs = [
        [skel + "/root", "root"],
        [skel + "/home", "home"],
        [skel + "/tmp", "tmp"]
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

        all += [":" + file[1]];
    }

    //
    // Create empty files and directories.
    //

    for (file in emptyfiles) {
        entries += touch(file[0] + "/" + file[1], file[1], file[2]);
        all += [":" + file[1]];
    }

    for (dir in emptydirs) {
        entries += makedir(dir[0], dir[1]);
        all += [":" + dir[1]];
    }

    //
    // Define a tool for using update-rc.d.
    //

    updateRcCommand = "$SHELL -c \"SYSROOT=" + skel +
                      " sh $S/apps/posix/update-rc.d $UPDATERC_ARGS\"";

    updateRcTool = {
        "type": "tool",
        "name": "updaterc",
        "command": updateRcCommand,
        "description": "mkdir $OUT"
    };

    entries.append(updateRcTool);
    entry = {
        "type": "target",
        "inputs": [":init.d/hostname.sh"],
        "output": skeletc + "/rc2.d/S10hostname.sh",
        "label": "init_hostname",
        "tool": "updaterc",
        "config": {"UPDATERC_ARGS": "-f hostname.sh defaults 10"}
    };

    entries.append(entry);
    all += [":init_hostname"];
    entries += group("skel", all);
    return entries;
}

