/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Images

Abstract:

    This module builds the final OS images.

Author:

    Evan Green 26-Oct-2015

Environment:

    Build

--*/

from menv import group, mconfig;

function buildImage(name, msetupFlags) {
    var entry;
    var installDeps;

    if (mconfig.arch == "x64") {
        installDeps = [
            //"apps/posix:skel",
            "apps/tzcomp:tz_files",
            "kernel:kernel",
            "kernel:devmap.set",
            "kernel:dev2drv.set",
            "kernel:init.set",
            "kernel:init.sh",
            "boot/loader:loader",
            "boot/bootman:bootman.bin",
            "boot/fatboot:fatboot.bin",
            "boot/mbr:mbr.bin",
            "apps/libc/dynamic:libc",
            "apps/osbase:libminocaos",
            "drivers:drivers",
            "apps/setup:build_msetup"
        ];

    } else {
        installDeps = [
            "apps:all_apps",
            "apps/posix:skel",
            "apps/tzcomp:tz_files",
            "kernel:kernel",
            "kernel:devmap.set",
            "kernel:dev2drv.set",
            "kernel:init.set",
            "kernel:init.sh",
            "boot:boot_apps",
            "drivers:drivers",
            "uefi:platfw",
            "apps/setup:build_msetup"
        ];
    }

    entry = {
        "type": "target",
        "tool": "msetup_image",
        "label": name,
        "output": mconfig.binroot + "/" + name,
        "implicit": installDeps,
        "config": {"MSETUP_FLAGS": msetupFlags}
    };

    if (name != "install.img") {
        entry["inputs"] = [":install.img"];
    }

    return [entry];
}

function build() {
    var arch = mconfig.arch;
    var commonImageFlags;
    var entries;
    var flags;
    var imageSize = "-G512M";
    var installFlags;
    var ramdiskImages;
    var tinyImageSize = "-G30M";
    var variant = mconfig.variant;

    commonImageFlags = [
        "-q",
        "-a0"
    ];

    if (mconfig.debug == "dbg") {
        commonImageFlags += ["-D"];
    }

    installFlags = commonImageFlags + [
        "-linstall-" + arch,
        "-i$" + mconfig.outroot,
        imageSize,
    ];

    commonImageFlags += [
        "-i$" + mconfig.binroot + "/install.img"
    ];

    entries = buildImage("install.img", installFlags);
    if (arch == "x86") {
        if (variant == "q") {
            flags = commonImageFlags + [
                "-lgalileo",
                imageSize
            ];

            entries += buildImage("galileo.img", flags);

        } else {
            flags = commonImageFlags + [
                "-lpc",
                imageSize
            ];

            entries += buildImage("pc.img", flags);
            flags = commonImageFlags + [
                "-lpcefi",
                imageSize
            ];

            entries += buildImage("pcefi.img", flags);
            flags = commonImageFlags + [
                "-lpc-tiny",
                tinyImageSize
            ];

            entries += buildImage("pctiny.img", flags);
        }

    } else if (arch == "x64") {
        flags = commonImageFlags + [
            "-lpc64",
            imageSize
        ];

        entries += buildImage("pc.img", flags);

    } else if (arch == "armv7") {
        flags = commonImageFlags + [imageSize];
        entries += buildImage("bbone.img", flags + ["-lbeagleboneblack"]);
        entries += buildImage("panda.img", flags + ["-lpanda"]);
        entries += buildImage("rpi2.img", flags + ["-lraspberrypi2"]);
        entries += buildImage("veyron.img", flags + ["-lveyron"]);
        ramdiskImages = [
            "uefi/plat/integcp:integ.img",
            "uefi/plat/panda:pandausb.img"
        ];

        entries += group("ramdisk_images", ramdiskImages);

    } else if (arch == "armv6") {
        flags = commonImageFlags + [
            imageSize,
            "-lraspberrypi"
        ];

        entries += buildImage("rpi.img", flags + ["-lraspberrypi"]);
    }

    return entries;
}

