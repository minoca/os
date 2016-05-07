/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    Debug Client

Abstract:

    This module builds the debug client, which debugs and profiles firmware,
    the kernel, and user mode applications.

Author:

    Evan Green 26-Jul-2012

Environment:

    User

--*/

function build() {
    common_sources = [
        "armdis.c",
        "cmdtab.c",
        "coff.c",
        "consio.c",
        "dbgapi.c",
        "dbgdwarf.c",
        "dbgeval.c",
        "dbgrcomm.c",
        "dbgrprof.c",
        "dbgsym.c",
        "debug.c",
        "disasm.c",
        "dwarf.c",
        "dwexpr.c",
        "dwframe.c",
        "dwline.c",
        "dwread.c",
        "elf.c",
        "exts.c",
        "profthrd.c",
        "remsrv.c",
        "stabs.c",
        "symbols.c",
        "thmdis.c",
        "thm32dis.c",
        "x86dis.c"
    ];

    x86_sources = [
        "x86/dbgarch.c"
    ];

    arm_sources = [
        "armv7/dbgarch.c"
    ];

    minoca_sources = [
        "minoca/cmdln.c",
        "minoca/extsup.c",
        "minoca/sock.c"
    ];

    win32_sources = [
        "win32/ntcomm.c",
        "win32/ntextsup.c",
        "win32/ntsock.c",
        "win32/ntusrdbg.c",
        "win32/ntusrsup.c"
    ];

    win32_cmd_sources = [
        "win32/cmdln/ntcmdln.c",
    ];

    win32_gui_sources = [
        "win32/ui/debugres.rc",
        "win32/ui/ntdbgui.c"
    ];

    target_libs = [
        "//lib/im:im"
    ];

    build_libs = [
        "//lib/im:build_im",
        "//lib/rtl/base:build_basertl",
        "//lib/rtl/rtlc:build_rtlc"
    ];

    target_dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    includes = [
        "$//lib/im",
        "$//debug/client",
        "$//apps/include"
    ];

    if (arch == "x86") {
        arch_sources = x86_sources;

    } else if ((arch == "armv7") || (arch == "armv6")) {
        arch_sources = arm_sources;

    } else {

        assert(0, "Unknown architecture");
    }

    target_sources = common_sources + minoca_sources + target_libs +
                     target_dynlibs + arch_sources;

    if (build_arch == "x86") {
        build_arch_sources = x86_sources;

    } else if ((build_arch == "armv7") || (build_arch == "armv6")) {
        build_arch_sources = arm_sources;

    } else {

        assert(0, "Unknown architecture");
    }

    build_config = {};
    build_gui_config = {};
    if (build_os == "Windows") {
        win32_common_sources = common_sources + build_arch_sources +
                               win32_sources;

        build_sources = win32_common_sources + win32_cmd_sources;
        build_gui_sources = win32_common_sources + win32_gui_sources;
        build_config["DYNLIBS"] = [
            "-lpsapi",
            "-lws2_32",
            "-lmswsock",
            "-ladvapi32"
        ];

        build_gui_config["DYNLIBS"] = build_config["DYNLIBS"] + ["-lshlwapi"];
        build_gui_config["LDFLAGS"] = ["-mwindows"];

    } else if (build_os == "Minoca") {
        build_sources = common_sources + minoca_sources + target_libs;
        build_config["DYNLIBS"] = ["-lminocaos"];

    } else {
        build_sources = null;
    }

    target_app = {
        "label": "debug",
        "inputs": target_sources,
        "includes": includes
    };

    entries = application(target_app);
    if (build_sources) {
        build_app = {
            "label": "build_debug",
            "output": "debug",
            "inputs": build_sources + build_libs,
            "includes": includes,
            "config": build_config,
            "build": TRUE,
            "prefix": "build"
        };

        entries += application(build_app);
    }

    //
    // Build the Windows GUI application if applicable.
    //

    if (build_os == "Windows") {
        build_gui_app = {
            "label": "build_debugui",
            "output": "debugui",
            "inputs": build_gui_sources + build_libs,
            "includes": includes,
            "config": build_gui_config,
            "build": TRUE,
            "prefix": "buildui"
        };

        entries += application(build_gui_app);
    }

    return entries;
}

return build();
