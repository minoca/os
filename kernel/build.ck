/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Kernel

Abstract:

    This is the core of the operating system.

Author:

    Evan Green 26-Jul-2012

Environment:

    Kernel

--*/

function build() {
    base_sources = [
        "init.c"
    ];

    boot_arch_sources = [];
    if ((arch == "armv7") || (arch == "armv6")) {
        arm_sources = [
            "armv7/commsup.S",
            "armv7/inttable.S",
            "armv7/prochw.c",
            "armv7/sstep.c",
            "armv7/trap.S",
            "armv7/vfp.c"
        ];

        boot_arch_sources = [":armv7/sstep.o"];
        if (arch == "armv7") {
            arch_sources = arm_sources + [
                "armv7/archsup.S",
                "armv7/archsupc.c"
            ];

        } else {
            arch_sources = arm_sources + [
                "armv6/archsup.S",
                "armv6/archsupc.c"
            ];
        }

    } else if (arch == "x86") {
        arch_sources = [
            "x86/archsup.S",
            "x86/archsupc.c",
            "x86/prochw.c",
            "x86/trap.S"
        ];
    }

    kernel_libs = [
        "//kernel/acpi:acpi",
        "//lib/crypto:crypto",
        "//kernel/ob:ob",
        "//lib/rtl/base:basertl",
        "//lib/rtl/kmode:krtl",
        "//lib/im:im",
        "//lib/basevid:basevid",
        "//lib/termlib:termlib",
        "//kernel/kd:kd",
        "//kernel/kd/kdusb:kdusb",
        "//kernel/ps:ps",
        "//kernel/ke:ke",
        "//kernel/io:io",
        "//kernel/hl:hl",
        "//kernel/mm:mm",
        "//kernel/sp:sp"
    ];

    kernel_config = {
        "LDFLAGS": ["-Wl,--whole-archive"]
    };

    kernel = {
        "label": "kernel",
        "inputs": base_sources + arch_sources + kernel_libs,
        "implicit": [":kernel-version"],
        "entry": "KepStartSystem",
        "config": kernel_config
    };

    boot_arch_lib = {
        "label": "archboot",
        "inputs": boot_arch_sources
    };

    entries = driver(kernel);
    if (boot_arch_sources) {
        entries += static_library(boot_arch_lib);
    }

    //
    // Copy the config files.
    //

    entries += copy("config/dev2drv.set",
                    binroot + "/dev2drv.set",
                    "dev2drv.set",
                    null,
                    null);

    entries += copy("config/devmap.set",
                    binroot + "/devmap.set",
                    "devmap.set",
                    null,
                    null);

    entries += copy("config/init.set",
                    binroot + "/init.set",
                    "init.set",
                    null,
                    null);

    entries += copy("config/init.sh",
                    binroot + "/init.sh",
                    "init.sh",
                    null,
                    null);

    //
    // Create the version header.
    //

    version_major = "0";
    version_minor = "2";
    version_revision = "0";
    entries += create_version_header(version_major,
                                     version_minor,
                                     version_revision);

    //
    // Also create a version file in the binroot.
    //

    version_config = {
        "MAJOR": version_major,
        "MINOR": version_minor,
        "REVISION": version_revision,
        "FORM": "simple"
    };

    version_file = {
        "type": "target",
        "label": "kernel-version",
        "output": binroot + "/kernel-version",
        "tool": "gen_version",
        "config": version_config
    };

    entries += [version_file];
    return entries;
}

return build();

