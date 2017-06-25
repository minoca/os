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

from menv import kernelLibrary, copy, driver, createVersionHeader, mconfig;

function build() {
    var arch = mconfig.arch;
    var archSources;
    var armSources;
    var baseSources;
    var binroot = mconfig.binroot;
    var bootArchLib;
    var bootArchSources;
    var entries;
    var kernel;
    var kernelConfig;
    var kernelLibs;
    var versionConfig;
    var versionFile;
    var versionMajor;
    var versionMinor;
    var versionRevision;

    baseSources = [
        "init.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        armSources = [
            "armv7/commsup.S",
            "armv7/inttable.S",
            "armv7/prochw.c",
            "armv7/sstep.c",
            "armv7/trap.S",
            "armv7/vfp.c"
        ];

        bootArchSources = [":armv7/sstep.o"];
        if (arch == "armv7") {
            archSources = armSources + [
                "armv7/archsup.S",
                "armv7/archsupc.c"
            ];

        } else {
            archSources = armSources + [
                "armv6/archsup.S",
                "armv6/archsupc.c"
            ];
        }

    } else if (arch == "x86") {
        archSources = [
            "x86/archsup.S",
            "x86/archsupc.c",
            "x86/prochw.c",
            "x86/trap.S"
        ];

    } else if (arch == "x64") {
        archSources = [
            "x64/archsup.S",
            "x64/archsupc.c",
            "x64/prochw.c",
            "x64/trap.S"
        ];
    }

    kernelLibs = [
        "kernel/acpi:acpi",
        "lib/crypto:crypto",
        "kernel/ob:ob",
        "lib/rtl/base:basertl",
        "lib/rtl/kmode:krtl",
        "lib/im:imn",
        "lib/basevid:basevid",
        "lib/termlib:termlib",
        "kernel/kd:kd",
        "kernel/kd/kdusb:kdusb",
        "kernel/ps:ps",
        "kernel/ke:ke",
        "kernel/io:io",
        "kernel/hl:hl",
        "kernel/mm:mm",
        "kernel/sp:sp"
    ];

    kernelConfig = {
        "LDFLAGS": ["-Wl,--whole-archive"]
    };

    kernel = {
        "label": "kernel",
        "inputs": baseSources + archSources + kernelLibs,
        "implicit": [":kernel-version"],
        "entry": "KepStartSystem",
        "config": kernelConfig
    };

    bootArchLib = {
        "label": "archboot",
        "inputs": bootArchSources
    };

    entries = driver(kernel);
    if (bootArchSources) {
        entries += kernelLibrary(bootArchLib);
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

    versionMajor = "0";
    versionMinor = "4";
    versionRevision = "0";
    entries += createVersionHeader(versionMajor,
                                   versionMinor,
                                   versionRevision);

    //
    // Also create a version file in the binroot.
    //

    versionConfig = {
        "MAJOR": versionMajor,
        "MINOR": versionMinor,
        "REVISION": versionRevision,
        "FORM": "simple"
    };

    versionFile = {
        "type": "target",
        "label": "kernel-version",
        "output": binroot + "/kernel-version",
        "tool": "gen_version",
        "config": versionConfig
    };

    entries += [versionFile];
    return entries;
}

