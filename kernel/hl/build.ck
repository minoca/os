/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Hardware Layer

Abstract:

    This library contains the Hardware Layer, which provides abstractions
    for core system resources such as timers, interrupt controllers,
    cache controllers, and serial ports. These are devices that are
    too central to the functioning of the system to be abstracted to
    device drivers, as they are required for normal operation of the
    kernel.

Author:

    Evan Green 5-Aug-2012

Environment:

    Kernel

--*/

from menv import kernelLibrary, mconfig;

function build() {
    var arch = mconfig.arch;
    var archBootSources;
    var archSources;
    var baseSources;
    var boot32Sources;
    var boot32Lib;
    var bootLib;
    var bootSources;
    var entries;
    var lib;

    baseSources = [
        "cache.c",
        "calendar.c",
        "clock.c",
        "dbgdev.c",
        "efi.c",
        "hmodapi.c",
        "info.c",
        "init.c",
        "intlevel.c",
        "intrupt.c",
        "ipi.c",
        "ns16550.c",
        "profiler.c",
        "reset.c",
        "suspend.c",
        "timer.c"
    ];

    bootSources = [
        "boot/hmodapi.c",
        ":dbgdev.o",
        ":ns16550.o"
    ];

    if (arch == "armv7") {
        archSources = [
            "armv7/am335int.c",
            "armv7/am335pwr.c",
            "armv7/am335tmr.c",
            "armv7/apinit.c",
            "armv7/apstart.S",
            "armv7/archcach.c",
            "armv7/archdbg.c",
            "armv7/archintr.c",
            "armv7/archrst.c",
            "armv7/archtimr.c",
            "armv7/cpintr.c",
            "armv7/cptimer.c",
            "armv7/cyccnt.c",
            "armv7/cycsupc.c",
            "armv7/gic.c",
            "armv7/gt.c",
            "armv7/gta.S",
            "armv7/omapintr.c",
            "armv7/omap3pwr.c",
            "armv7/omap3tmr.c",
            "armv7/omap4chc.c",
            "armv7/omap4pwr.c",
            "armv7/omap4smc.S",
            "armv7/omap4tmr.c",
            "armv7/regacces.c",
            "armv7/rk32tmr.c",
            "armv7/sp804tmr.c",
            "armv7/uartpl11.c",
            "armv7/uartomap.c",
            "armv7/b2709int.c",
            "armv7/b2709tmr.c"
        ];

        archBootSources = [
            ":armv7/archdbg.o",
            ":armv7/regacces.o",
            ":armv7/uartpl11.o",
            ":armv7/uartomap.o"
        ];

    } else if (arch == "armv6") {
        archSources = [
            "armv6/archcach.c",
            "armv6/archdbg.c",
            "armv6/archintr.c",
            "armv6/archtimr.c",
            "armv6/cycsupc.c",
            "armv7/apinit.c",
            "armv7/apstart.S",
            "armv7/archrst.c",
            "armv7/b2709int.c",
            "armv7/b2709tmr.c",
            "armv7/cyccnt.c",
            "armv7/regacces.c",
            "armv7/uartpl11.c",
        ];

        archBootSources = [
            ":armv6/archdbg.o",
            ":armv7/regacces.o",
            ":armv7/uartpl11.o",
        ];

    } else if ((arch == "x86") || (arch == "x64")) {
        archSources = [
            "x86/apic.c",
            "x86/apictimr.c",
            "x86/archcach.c",
            "x86/archdbg.c",
            "x86/archintr.c",
            "x86/archrst.c",
            "x86/archsup.S",
            "x86/archtimr.c",
            "x86/ioport.c",
            "x86/pmtimer.c",
            "x86/regacces.c",
            "x86/rtc.c",
            "x86/tsc.c"
        ];

        archBootSources = [
            ":x86/archdbg.o",
            ":x86/ioport.o",
            ":x86/regacces.o"
        ];

        if (arch == "x64") {
            boot32Sources = [
                "boot/hmodapi.c",
                "dbgdev.c",
                "ns16550.c",
                "x86/archdbg.c",
                "x86/ioport.c",
                "x86/regacces.c"
            ];

            archSources += [
                "x64/apinit.c",
                "x64/apstart.S",
            ];

        } else {
            archSources += [
                "x86/apinit.c",
                "x86/apstart.S",
            ];
        }
    }

    lib = {
        "label": "hl",
        "inputs": baseSources + archSources,
    };

    bootLib = {
        "label": "hlboot",
        "inputs": bootSources + archBootSources
    };

    entries = kernelLibrary(lib);
    entries += kernelLibrary(bootLib);
    if (arch == "x64") {
        boot32Lib = {
            "label": "hlboot32",
            "inputs": boot32Sources,
            "prefix": "x6432",
            "sources_config": {"CPPFLAGS": ["-m32"]}
        };

        entries += kernelLibrary(boot32Lib);
    }

    return entries;
}

