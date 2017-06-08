/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Boot Library

Abstract:

    This library contains support routines for the boot applications.

Author:

    Evan Green 19-Feb-2014

Environment:

    Boot

--*/

from menv import addConfig, compiledSources, mconfig, kernelLibrary;

function build() {
    var arch = mconfig.arch;
    var commonArmEfiSources;
    var commonArmSources;
    var commonLib;
    var commonList;
    var commonSources;
    var commonX86Sources;
    var entries;
    var includes;
    var efiLib;
    var efiSources;
    var pcat32Lib;
    var pcatLib;
    var pcatSources;
    var pcat32Sources;
    var pcatX86Sources;
    var sourcesConfig;

    commonSources = [
        "bootfat.c",
        "bootmem.c",
        "file.c",
        "stubs.c",
        "version.c"
    ];

    pcatSources = [
        "pcat/fwapi.c",
        "pcat/int10.c",
        "pcat/int13.c",
        "pcat/memory.c",
        "pcat/realmode.c",
        "pcat/reset.c",
        "pcat/rsdp.c",
        "pcat/time.c"
    ];

    efiSources = [
        "efi/dbgser.c",
        "efi/disk.c",
        "efi/fwapi.c",
        "efi/memory.c",
        "efi/time.c",
        "efi/util.c",
        "efi/video.c"
    ];

    commonArmSources = [
        "armv7/commsup.S",
        "armv7/inttable.S",
        "armv7/prochw.c"
    ];

    commonArmEfiSources = [
        "efi/armv7/efia.S",
        "efi/armv7/efiarch.c"
    ];

    commonX86Sources = [
        "x86/archsup.S",
        "x86/prochw.c"
    ];

    pcatX86Sources = [
        "pcat/x86/realmexe.S",
    ];

    if (arch == "armv7") {
        commonSources += commonArmSources + [
            "armv7/archsup.S"
        ];

        efiSources += commonArmEfiSources;

    } else if (arch == "armv6") {
        commonSources += commonArmSources + [
            "armv6/archsup.S"
        ];

        efiSources += commonArmEfiSources;

    } else if (arch == "x86") {
        efiSources += [
            "efi/x86/efia.S",
            "efi/x86/efiarch.c"
        ];

        commonSources += commonX86Sources;
        pcatSources += pcatX86Sources;

    } else if (arch == "x64") {
        pcat32Sources = commonSources + commonX86Sources +
                        pcatSources + pcatX86Sources;

        commonSources += [
            "x64/archsup.S",
            "x64/prochw.c"
        ];

        pcatSources += [
            "pcat/x64/realmexe.S"
        ];
    }

    includes = [
        "$S/boot/lib/include",
        "$S/boot/lib"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"],
    };

    commonLib = {
        "inputs": commonSources,
        "sources_config": sourcesConfig,
        "includes": includes
    };

    commonList = compiledSources(commonLib);
    entries = commonList[1];
    efiLib = {
        "label": "bootefi",
        "inputs": commonList[0] + efiSources,
        "sources_config": sourcesConfig
    };

    entries += kernelLibrary(efiLib);

    //
    // On PC machines, build the BIOS library as well.
    //

    if ((arch == "x86") || (arch == "x64")) {
        pcatLib = {
            "label": "bootpcat",
            "inputs": commonList[0] + pcatSources,
            "sources_config": sourcesConfig,
            "includes": includes
        };

        entries += kernelLibrary(pcatLib);

        //
        // Also build the 32-bit version of the PCAT library in 64-bit mode
        // for the boot manager (which is 32-bits always).
        //

        if (arch == "x64") {
            sourcesConfig = sourcesConfig.copy();
            sourcesConfig["CPPFLAGS"] = ["-m32"];
            pcat32Lib = {
                "label": "bootpcat32",
                "inputs": pcat32Sources,
                "sources_config": sourcesConfig,
                "includes": includes,
                "prefix": "x6432"
            };

            entries += kernelLibrary(pcat32Lib);
        }
    }

    //
    // Add the include and dependency for version.c, which uses the kernel's
    // version.h
    //

    for (entry in entries) {
        if (entry["output"].endsWith("version.o")) {
            addConfig(entry, "CPPFLAGS", "-I$O/kernel");
            entry["implicit"] = ["kernel:version.h"];
        }
    }

    return entries;
}

