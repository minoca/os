/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

from menv import application, createVersionHeader, mconfig;

function build() {
    var app;
    var baseSources;
    var buildApp;
    var buildConfig;
    var buildLibs;
    var buildIncludes;
    var buildOs = mconfig.build_os;
    var buildSources;
    var buildSourcesConfig;
    var entries;
    var loginSources;
    var minocaSources;
    var sourcesConfig;
    var targetIncludes;
    var targetLibs;
    var targetSources;
    var uosOnlyCommands;
    var uosSources;
    var win32Sources;

    baseSources = [
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
        "soko.c",
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
        "whoami.c",
        "xargs.c",
    ];

    uosOnlyCommands = [
        "chown.c",
        "hostname.c",
        "mkfifo.c",
        "readlink.c",
        "sh/shuos.c",
        "stty.c",
        "swlib/chownutl.c",
        "swlib/uos.c",
        "telnet.c",
    ];

    loginSources = [
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
        "ssdaemon.c",
        "telnetd.c",
    ];

    minocaSources = [
        "cmds.c",
        "dw.c",
        "swlib/minocaos.c"
    ];

    uosSources = [
        "dw.c",
        "swlib/linux.c",
        "uos/uoscmds.c",
    ];

    win32Sources = [
        "dw.c",
        "sh/shntos.c",
        "swlib/ntos.c",
        "win32/swiss.rc",
        "win32/w32cmds.c"
    ];

    targetLibs = [
        "lib/termlib:termlib",
        "apps/osbase:libminocaos"
    ];

    buildLibs = [
        "lib/termlib:build_termlib",
        "lib/rtl/base:build_basertl",
        "lib/rtl/urtl:build_rtlc"
    ];

    buildIncludes = [];
    targetIncludes = buildIncludes + [
        "$S/apps/libc/include"
    ];

    targetSources = baseSources + uosOnlyCommands + loginSources +
                    minocaSources;

    buildConfig = {
        "LDFLAGS": [],
        "DYNLIBS": []
    };

    sourcesConfig = {
        "CFLAGS": ["-ftls-model=initial-exec"],
    };

    buildSourcesConfig = sourcesConfig.copy();
    if (buildOs == "Minoca") {
        buildSources = targetSources;
        buildConfig["DYNLIBS"] += ["-lminocaos"];

    } else if (buildOs == "Windows") {
        buildSources = baseSources + win32Sources;
        buildLibs = ["apps/libc/dynamic:wincsup"] + buildLibs;
        buildIncludes += ["$S/apps/libc/dynamic/wincsup/include"];
        buildConfig["DYNLIBS"] += ["-lpsapi", "-lws2_32"];

    } else {
        buildSources = baseSources + uosOnlyCommands + uosSources;
        if (buildOs == "Linux") {
            buildConfig["DYNLIBS"] += ["-ldl", "-lutil"];
        }

        if (buildOs != "Darwin") {
            buildSources += loginSources;
        }
    }

    app = {
        "label": "swiss",
        "inputs": targetSources + targetLibs,
        "sources_config": sourcesConfig,
        "includes": targetIncludes
    };

    buildApp = {
        "label": "build_swiss",
        "output": "swiss",
        "inputs": buildSources + buildLibs,
        "sources_config": buildSourcesConfig,
        "includes": buildIncludes,
        "config": buildConfig,
        "build": true,
        "prefix": "build",
        "binplace": "tools/bin"
    };

    entries = application(app);
    entries += application(buildApp);

    //
    // Create the version header.
    //

    entries += createVersionHeader("0", "0", "0");

    //
    // Add the include and dependency for version.c.
    //

    for (entry in entries) {
        if (entry["inputs"][0] == "swlib/userio.c") {
            entry["config"] = entry["config"].copy();
            entry["config"]["CPPFLAGS"] = entry["config"]["CPPFLAGS"] +
                                          ["-I$O/apps/swiss"];

            entry["implicit"] = [":version.h"];
        }
    }

    return entries;
}

