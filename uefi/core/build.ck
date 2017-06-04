/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    UEFI Core

Abstract:

    This file is responsible for building the core UEFI support. This is a
    library containing the bulk of the UEFI interfaces, which platform
    specific images include into their boot image.

Author:

    Evan Green 26-Feb-2014

Environment:

    Firmware

--*/

from menv import addConfig, createVersionHeader, mconfig, kernelLibrary;

function build() {
    var arch = mconfig.arch;
    var armSources;
    var baseSources;
    var emptyrdLib;
    var entries;
    var fwCoreVersionMajor;
    var fwCoreVersionMinor;
    var fwCoreVersionRevision;
    var includes;
    var lib;
    var sources;
    var sourcesConfig;
    var x86_sources;

    baseSources = [
        "acpi.c",
        "acpitabs.c",
        "basepe.c",
        "bdsboot.c",
        "bdscon.c",
        "bdsentry.c",
        "bdsutil.c",
        "cfgtable.c",
        "crc32.c",
        "dbgser.c",
        "devpathu.c",
        "diskio.c",
        "dispatch.c",
        "div.c",
        "drvsup.c",
        "event.c",
        "fatdev.c",
        "fatfs.c",
        "fsvars.c",
        "fvblock.c",
        "fvsect.c",
        "fwvol.c",
        "fwvolio.c",
        "handle.c",
        "image.c",
        "init.c",
        "intr.c",
        "locate.c",
        "lock.c",
        "memory.c",
        "part.c",
        "partelto.c",
        "partgpt.c",
        "partmbr.c",
        "pool.c",
        "ramdisk.c",
        "smbios.c",
        "stubs.c",
        "tpl.c",
        "timer.c",
        "util.c",
        "version.c",
        "vidcon.c",
    ];

    x86_sources = [
        "x86/archsup.S",
        "x86/prochw.c"
    ];

    armSources = [
        "armv7/commsup.S",
        "armv7/inttable.S",
        "armv7/prochw.c"
    ];

    if ((arch == "armv6") || (arch == "armv7")) {
        sources = baseSources + armSources;

    } else if (arch == "x86") {
        sources = baseSources + x86_sources;

    } else {
        Core.raise(ValueError("Unknown architecture"));
    }

    includes = [
        "$S/uefi/include",
        "$S/uefi/core"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"],
    };

    //
    // Create the version.h header.
    //

    fwCoreVersionMajor = "1";
    fwCoreVersionMinor = "0";
    fwCoreVersionRevision = "0";
    entries = createVersionHeader(fwCoreVersionMajor,
                                  fwCoreVersionMinor,
                                  fwCoreVersionRevision);

    lib = {
        "label": "ueficore",
        "inputs": sources,
        "sources_config": sourcesConfig,
        "includes": includes
    };

    emptyrdLib = {
        "label": "emptyrd",
        "inputs": ["emptyrd/emptyrd.S"]
    };

    entries += kernelLibrary(lib);

    //
    // Add the include and dependency for version.c.
    //

    for (entry in entries) {
        if (entry["output"] == "version.o") {
            addConfig(entry, "CPPFLAGS", "-I$O/uefi/core");
            entry["implicit"] = [":version.h"];
            break;
        }
    }

    entries += kernelLibrary(emptyrdLib);
    return entries;
}

