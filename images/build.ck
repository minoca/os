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

function build_image(name, msetup_flags) {
    install_deps = [
        "//apps:all_apps",
        "//apps/posix:skel",
        "//apps/tzcomp:tz_files",
        "//kernel:kernel",
        "//kernel:devmap.set",
        "//kernel:dev2drv.set",
        "//kernel:init.set",
        "//kernel:init.sh",
        "//boot:boot_apps",
        "//drivers:drivers",
        "//uefi:platfw",
        "//apps/setup:build_msetup"
    ];

    entry = {
        "type": "target",
        "tool": "msetup_image",
        "label": name,
        "output": binroot + "/" + name,
        "implicit": install_deps,
        "config": {"MSETUP_FLAGS": msetup_flags}
    };

    if (name != "install.img") {
        entry["inputs"] = [":install.img"];
    }

    return [entry];
}

function build() {
    image_size = "-G512M";
    tiny_image_size = "-G30M";
    common_image_flags = [
        "-q"
    ];

    if (debug == "dbg") {
        common_image_flags += ["-D"];
    }

    install_flags = common_image_flags + [
        "-linstall-" + arch,
        "-i$" + outroot,
        image_size,
    ];

    common_image_flags += [
        "-i$" + binroot + "/install.img"
    ];

    entries = build_image("install.img", install_flags);
    if (arch == "x86") {
        if (variant == "q") {
            flags = common_image_flags + [
                "-lgalileo",
                image_size
            ];

            entries += build_image("galileo.img", flags);

        } else {
            flags = common_image_flags + [
                "-lpc",
                image_size
            ];

            entries += build_image("pc.img", flags);
            flags = common_image_flags + [
                "-lpcefi",
                image_size
            ];

            entries += build_image("pcefi.img", flags);
            flags = common_image_flags + [
                "-lpc-tiny",
                tiny_image_size
            ];

            entries += build_image("pctiny.img", flags);
        }

    } else if (arch == "armv7") {
        flags = common_image_flags + [image_size];
        entries += build_image("bbone.img", flags + ["-lbeagleboneblack"]);
        entries += build_image("panda.img", flags + ["-lpanda"]);
        entries += build_image("rpi2.img", flags + ["-lraspberrypi2"]);
        entries += build_image("veyron.img", flags + ["-lveyron"]);
        ramdisk_images = [
            "//uefi/plat/integcp:integ.img",
            "//uefi/plat/panda:pandausb.img"
        ];

        entries += group("ramdisk_images", ramdisk_images);

    } else if (arch == "armv6") {
        flags = common_image_flags + [
            image_size,
            "-lraspberrypi"
        ];

        entries += build_image("rpi.img", flags + ["-lraspberrypi"]);
    }

    return entries;
}

return build();
