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

from menv import staticLibrary, mconfig;

function build() {
    var arch = mconfig.arch;
    var archSources;
    var armSources;
    var armv6Sources;
    var armv7Sources;
    var baseSources;
    var bootLib;
    var bootSources;
    var buildArchSources;
    var buildLib;
    var entries;
    var lib;
    var x86Sources;

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

    bootSources = [
        ":mdl.o"
    ];

    armSources = [
        "armv7/archcomc.c",
        "armv7/flush.c",
        "armv7/mapping.c",
        "armv7/usermem.S"
    ];

    armv7Sources = armSources + [
        "armv7/archsupc.c"
    ];

    armv6Sources = armSources + [
        "armv6/archsupc.c"
    ];

    x86Sources = [
        "x86/archsupc.c",
        "x86/flush.c",
        "x86/mapping.c",
        "x86/usermem.S"
    ];

    if (arch == "armv7") {
        archSources = armv7Sources;

    } else if (arch == "armv6") {
        archSources = armv6Sources;

    } else if (arch == "x86") {
        archSources = x86Sources;
    }

    if (mconfig.build_arch == "armv7") {
        buildArchSources = armv7Sources;

    } else if (mconfig.build_arch == "armv6") {
        buildArchSources = armv6Sources;

    } else if (mconfig.build_arch == "x86") {
        buildArchSources = x86Sources;
    }

    lib = {
        "label": "mm",
        "inputs": baseSources + archSources,
    };

    bootLib = {
        "label": "mmboot",
        "inputs": bootSources
    };

    buildLib = {
        "label": "build_mm",
        "output": "mm",
        "inputs": baseSources + buildArchSources,
        "build": true,
        "prefix": "build"
    };

    entries = staticLibrary(lib);
    entries += staticLibrary(bootLib);
    entries += staticLibrary(buildLib);
    return entries;
}

