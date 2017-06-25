/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Memory Manager

Abstract:

    This library contains the Memory Management library used by the kernel.
    It handles everything related to memory in the kernel, and meshes
    closely with the I/O and process libraries to provide fast and
    efficient management of memory resources.

Author:

    Evan Green 27-Jul-2012

Environment:

    Kernel

--*/

from menv import kernelLibrary, mconfig;

function build() {
    var arch = mconfig.arch;
    var archSources;
    var armSources;
    var baseSources;
    var bootLib;
    var bootLib32;
    var buildArchSources;
    var buildLib;
    var entries;
    var lib;

    baseSources = [
        "block.c",
        "imgsec.c",
        "info.c",
        "init.c",
        "invipi.c",
        "iobuf.c",
        "load.c",
        "mdl.c",
        "paging.c",
        "physical.c",
        "kpools.c",
        "virtual.c",
        "fault.c"
    ];

    armSources = [
        "armv7/archcomc.c",
        "armv7/flush.c",
        "armv7/mapping.c",
        "armv7/usermem.S"
    ];

    if (arch == "armv7") {
        archSources = armSources + [
            "armv7/archsupc.c"
        ];

    } else if (arch == "armv6") {
        archSources = armSources + [
            "armv6/archsupc.c"
        ];

    } else if (arch == "x86") {
        archSources = [
            "x86/archsupc.c",
            "x86/flush.c",
            "x86/mapping.c",
            "x86/usermem.S"
        ];

    } else if (arch == "x64") {
        archSources = [
            "x64/archsupc.c",
            "x64/mapping.c",
            "x64/usermem.S",
            "x86/flush.c",
        ];
    }

    if (mconfig.build_arch == "armv7") {
        buildArchSources = ["armv7/mapping.c"];

    } else if (mconfig.build_arch == "armv6") {
        buildArchSources = ["armv6/mapping.c"];

    } else if (mconfig.build_arch == "x86") {
        buildArchSources = ["x86/mapping.c"];

    } else if (mconfig.build_arch == "x64") {
        buildArchSources = ["x86/mapping.c"];
    }

    lib = {
        "label": "mm",
        "inputs": baseSources + archSources,
    };

    bootLib = {
        "label": "mmboot",
        "inputs": [":mdl.o"]
    };

    buildLib = {
        "label": "build_mm",
        "output": "mm",
        "inputs": baseSources + buildArchSources,
        "build": true,
        "prefix": "build"
    };

    entries = kernelLibrary(lib);
    entries += kernelLibrary(bootLib);
    entries += kernelLibrary(buildLib);
    if (arch == "x64") {
        bootLib32 = {
            "label": "mmboot32",
            "inputs": ["mdl.c"],
            "prefix": "x6432",
            "sources_config": {"CPPFLAGS": ["-m32"]}
        };

        entries += kernelLibrary(bootLib32);
    }

    return entries;
}

