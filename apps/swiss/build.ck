/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    swiss

Abstract:

    This executable implements the Swiss Utility, which contains
    many basic core utilities, including a Bourne-compatible shell.

Author:

    Evan Green 5-Jun-2013

Environment:

    User

--*/

function build() {
    base_sources = [
        "basename.c",
        "cat.c",
        "cecho.c",
        "chmod.c",
        "chroot.c",
        "cmp.c",
        "comm.c",
        "cp.c",
        "cut.c",
        "date.c",
        "dd.c",
        "diff.c",
        "dirname.c",
        "easy.c",
        "echo.c",
        "env.c",
        "expr.c",
        "find.c",
        "grep.c",
        "head.c",
        "id.c",
        "install.c",
        "kill.c",
        "ln.c",
        "ls/compare.c",
        "ls/ls.c",
        "mkdir.c",
        "mktemp.c",
        "mv.c",
        "nl.c",
        "nproc.c",
        "od.c",
        "printf.c",
        "ps.c",
        "pwd.c",
        "reboot.c",
        "rm.c",
        "rmdir.c",
        "sed/sed.c",
        "sed/sedfunc.c",
        "sed/sedparse.c",
        "sed/sedutil.c",
        "seq.c",
        "sh/alias.c",
        "sh/arith.c",
        "sh/builtin.c",
        "sh/exec.c",
        "sh/expand.c",
        "sh/lex.c",
        "sh/linein.c",
        "sh/parser.c",
        "sh/path.c",
        "sh/sh.c",
        "sh/signals.c",
        "sh/util.c",
        "sh/var.c",
        "sort.c",
        "split.c",
        "sum.c",
        "swiss.c",
        "swlib/copy.c",
        "swlib/delete.c",
        "swlib/pattern.c",
        "swlib/pwdcmd.c",
        "swlib/string.c",
        "swlib/userio.c",
        "tail.c",
        "tee.c",
        "test.c",
        "time.c",
        "touch.c",
        "tr.c",
        "uname.c",
        "uniq.c",
        "wc.c",
        "which.c",
        "xargs.c",
    ];

    uos_only_commands = [
        "chown.c",
        "init.c",
        "login/chpasswd.c",
        "login/getty.c",
        "login/groupadd.c",
        "login/groupdel.c",
        "login/login.c",
        "login/lutil.c",
        "login/passwd.c",
        "login/su.c",
        "login/sulogin.c",
        "login/useradd.c",
        "login/userdel.c",
        "login/vlock.c",
        "mkfifo.c",
        "readlink.c",
        "sh/shuos.c",
        "ssdaemon.c",
        "stty.c",
        "swlib/chownutl.c",
        "swlib/uos.c",
        "telnet.c",
        "telnetd.c",
    ];

    minoca_sources = [
        "cmds.c",
        "dw.c",
        "swlib/minocaos.c"
    ];

    uos_sources = [
        "dw.c",
        "swlib/linux.c",
        "uos/uoscmds.c",
    ];

    win32_sources = [
        "dw.c",
        "sh/shntos.c",
        "swlib/ntos.c",
        "win32/swiss.rc",
        "win32/w32cmds.c"
    ];

    target_libs = [
        "//lib/termlib:termlib",
        "//apps/osbase:libminocaos"
    ];

    build_libs = [
        "//lib/termlib:build_termlib",
        "//lib/rtl/base:build_basertl",
        "//lib/rtl/rtlc:build_rtlc"
    ];

    build_includes = [];
    target_includes = build_includes + [
        "$//apps/libc/include"
    ];

    target_sources = base_sources + uos_only_commands + minoca_sources;
    build_config = {
        "LDFLAGS": [],
        "DYNLIBS": []
    };

    sources_config = {
        "CFLAGS": ["-ftls-model=initial-exec"],
    };

    build_sources_config = sources_config + {};
    if (build_os == "Minoca") {
        build_sources = target_sources;
        build_config["DYNLIBS"] += ["-lminocaos"];

    } else if (build_os == "Windows") {
        build_sources = base_sources + win32_sources;
        build_libs = ["//apps/libc/dynamic:wincsup"] + build_libs;
        build_includes += ["$//apps/libc/dynamic/wincsup/include"];
        build_config["DYNLIBS"] += ["-lpsapi", "-lws2_32"];

    } else {
        build_sources = base_sources + uos_only_commands + uos_sources;
        if (build_os == "Linux") {
            build_config["DYNLIBS"] += ["-ldl", "-lutil"];
        }
    }

    app = {
        "label": "swiss",
        "inputs": target_sources + target_libs,
        "sources_config": sources_config,
        "includes": target_includes
    };

    build_app = {
        "label": "build_swiss",
        "output": "swiss",
        "inputs": build_sources + build_libs,
        "sources_config": build_sources_config,
        "includes": build_includes,
        "config": build_config,
        "build": TRUE,
        "prefix": "build"
    };

    entries = application(app);
    entries += application(build_app);

    //
    // Create the version header.
    //

    entries += create_version_header("0", "0", "0");

    //
    // Add the include and dependency for version.c.
    //

    for (entry in entries) {
        if (entry["inputs"][0] == "swlib/userio.c") {
            entry["config"] = entry["config"] + {};
            entry["config"]["CPPFLAGS"] = entry["config"]["CPPFLAGS"] +
                                          ["-I$^/apps/swiss"];

            entry["implicit"] = [":version.h", "//.git/HEAD"];
        }
    }

    return entries;
}

return build();

