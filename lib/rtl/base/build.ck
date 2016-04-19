/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

function build() {
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

    wide_sources = [
        "wprint.c",
        "wscan.c",
        "wstring.c",
        "wtime.c"
    ];

    x86_intrinsics = [
        "x86/intrinsc.c"
    ];

    x86_sources = x86_intrinsics + [
        "x86/rtlarch.S",
        "x86/rtlmem.S"
    ];

    armv7_intrinsics = [
        "armv7/intrinsa.S",
        "armv7/intrinsc.c"
    ];

    armv7_sources = armv7_intrinsics + [
        "armv7/rtlarch.S",
        "armv7/rtlmem.S",
        "fp2int.c"
    ];

    //
    // Add eabisfp.c and softfp.c even though they're not needed in this
    // library just so they get exercise.
    //

    armv7_boot_sources = armv7_intrinsics + [
        "armv7/aeabisfp.c",
        "armv7/rtlmem.S",
        "boot/armv7/rtlarch.S",
        "fp2int.c",
        "softfp.c"
    ];

    x64_sources = [
        "x64/rtlarch.S",
        "x64/rtlmem.S"
    ];

    //
    // Put together the target sources by architecture.
    //

    boot_sources = sources;
    if (arch == "x86") {
        target_sources = sources + x86_sources;
        intrinsics = x86_intrinsics;
        boot_sources = sources + x86_sources;

    } else if ((arch == "armv7") || (arch == "armv6")) {
        target_sources = sources + armv7_sources;
        intrinsics = armv7_intrinsics;
        boot_sources = sources + armv7_boot_sources;

    } else if (arch == "x64") {
        target_sources = sources + x64_sources;
        intrinsics = null;
    }

    //
    // Put together the build sources by architecture.
    //

    build_sources = sources + wide_sources;
    if (build_arch == "x86") {
        build_sources += x86_sources + [
            "fp2int.c",
            "softfp.c"
        ];

    } else if ((build_arch == "armv7") || (build_arch == "armv6")) {
        build_sources += armv7_sources;

    } else if (build_arch == "x64") {
        build_sources += + x64_sources + [
            "fp2int.c",
            "softfp.c"
        ];
    }

    entries = [];
    includes = [
        "$//lib/rtl"
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

        intrinsics_sources_config = {
            "CPPFLAGS": ["-DRTL_API=DLLIMPORT"]
        };

        intrinsics_lib = {
            "label": "intrins",
            "inputs": intrinsics,
            "sources_config": intrinsics_sources_config,
            "includes": includes,
            "prefix": "intrins"
        };

        entries += static_library(intrinsics_lib);
    }

    //
    // Compile the boot version of Rtl, which is the same as the regular
    // version except on ARM it contains no ldrex/strex.
    //

    boot_lib = {
        "label": "basertlb",
        "inputs": boot_sources,
        "prefix": "boot",
        "includes": includes,
    };

    entries += static_library(boot_lib);

    //
    // Compile the wide library support.
    //

    wide_lib = {
        "label": "basertlw",
        "inputs": wide_sources,
        "includes": includes,
    };

    entries += static_library(wide_lib);

    //
    // Compile the main Rtl base library.
    //

    basertl_lib = {
        "label": "basertl",
        "inputs": target_sources,
        "includes": includes,
    };

    entries += static_library(basertl_lib);

    //
    // Compile the build version of the Rtl base library.
    //

    build_basertl_lib = {
        "label": "build_basertl",
        "output": "basertl",
        "inputs": build_sources,
        "includes": includes,
        "prefix": "build",
        "build": TRUE
    };

    entries += static_library(build_basertl_lib);
    return entries;
}

return build();
