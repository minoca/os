/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Base Runtime Library

Abstract:

    This library contains the base Runtime Library that is shared between
    kernel and user modes.

Author:

    Evan Green 26-Jul-2012

Environment:

    Any

--*/

from menv import mconfig, kernelLibrary, staticLibrary;

function build() {
    var arch = mconfig.arch;
    var armv7BootSources;
    var armv7Intrinsics;
    var armv7Sources;
    var baseRtl32Lib;
    var baseRtl32Sources;
    var baseRtlLib;
    var bootLib;
    var bootSources;
    var buildBaseRtlLib;
    var buildSources;
    var entries;
    var includes;
    var intrinsics;
    var intrinsicsLib;
    var intrinsicsSourcesConfig;
    var sources;
    var targetSources;
    var wideLib;
    var wideSources;
    var x64Sources;
    var x86Intrinsics;
    var x86Sources;

    sources = [
        "crc32.c",
        "heap.c",
        "heapprof.c",
        "math.c",
        "print.c",
        "rbtree.c",
        "scan.c",
        "string.c",
        "time.c",
        "timezone.c",
        "version.c",
        "wchar.c"
    ];

    wideSources = [
        "wprint.c",
        "wscan.c",
        "wstring.c",
        "wtime.c"
    ];

    x86Intrinsics = [
        "x86/intrinsc.c"
    ];

    x86Sources = x86Intrinsics + [
        "x86/rtlarch.S",
        "x86/rtlmem.S"
    ];

    armv7Intrinsics = [
        "armv7/intrinsa.S",
        "armv7/intrinsc.c"
    ];

    armv7Sources = armv7Intrinsics + [
        "armv7/rtlarch.S",
        "armv7/rtlmem.S",
        "fp2int.c"
    ];

    //
    // Add eabisfp.c and softfp.c even though they're not needed in this
    // library just so they get exercise.
    //

    armv7BootSources = armv7Intrinsics + [
        "armv7/aeabisfp.c",
        "armv7/rtlmem.S",
        "boot/armv7/rtlarch.S",
        "fp2int.c",
        "softfp.c"
    ];

    x64Sources = [
        "x64/rtlarch.S",
        "x64/rtlmem.S"
    ];

    //
    // Put together the target sources by architecture.
    //

    if (arch == "x86") {
        targetSources = sources + x86Sources;
        intrinsics = x86Intrinsics;

    } else if ((arch == "armv7") || (arch == "armv6")) {
        targetSources = sources + armv7Sources;
        intrinsics = armv7Intrinsics;
        bootSources = sources + armv7BootSources;

    } else if (arch == "x64") {
        targetSources = sources + x64Sources;
        intrinsics = null;
        bootSources = sources + x64Sources;
        baseRtl32Sources = sources + x86Sources;
    }

    //
    // Put together the build sources by architecture.
    //

    buildSources = sources + wideSources;
    if (mconfig.build_arch == "x86") {
        buildSources += x86Sources + [
            "fp2int.c",
            "softfp.c"
        ];

    } else if ((mconfig.build_arch == "armv7") ||
               (mconfig.build_arch == "armv6")) {

        buildSources += armv7Sources;

    } else if (mconfig.build_arch == "x64") {
        buildSources += x64Sources + [
            "fp2int.c",
            "softfp.c"
        ];
    }

    entries = [];
    includes = [
        "$S/lib/rtl"
    ];

    //
    // Compile the intrinsics library, which contains just the compiler math
    // support routines.
    //

    if (intrinsics) {

        //
        // The library is being compiled such that the rest of Rtl is in
        // another binary.
        //

        intrinsicsSourcesConfig = {
            "CPPFLAGS": ["-DRTL_API=__DLLIMPORT"]
        };

        intrinsicsLib = {
            "label": "intrins",
            "inputs": intrinsics,
            "sources_config": intrinsicsSourcesConfig,
            "includes": includes,
            "prefix": "intrins"
        };

        entries += kernelLibrary(intrinsicsLib);
    }

    //
    // Compile the boot version of Rtl, which is the same as the regular
    // version except on ARM it contains no ldrex/strex.
    //

    if (bootSources) {
        bootLib = {
            "label": "basertlb",
            "inputs": bootSources,
            "prefix": "boot",
            "includes": includes,
        };

        entries += kernelLibrary(bootLib);
    }

    //
    // Compile the wide library support.
    //

    wideLib = {
        "label": "basertlw",
        "inputs": wideSources,
        "includes": includes,
    };

    entries += kernelLibrary(wideLib);

    //
    // Compile the main Rtl base library.
    //

    baseRtlLib = {
        "label": "basertl",
        "inputs": targetSources,
        "includes": includes,
    };

    entries += kernelLibrary(baseRtlLib);

    //
    // Compile the build version of the Rtl base library.
    //

    buildBaseRtlLib = {
        "label": "build_basertl",
        "output": "basertl",
        "inputs": buildSources,
        "includes": includes,
        "prefix": "build",
        "build": true
    };

    entries += staticLibrary(buildBaseRtlLib);
    if (arch == "x64") {
        baseRtl32Lib = {
            "label": "basertl32",
            "inputs": baseRtl32Sources,
            "includes": includes,
            "prefix": "x6432",
            "sources_config": {"CPPFLAGS": ["-m32"]}
        };

        entries += kernelLibrary(baseRtl32Lib);
    }

    return entries;
}

