/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Chalk Library

Abstract:

    This module contains the Chalk language interpreter library. It is built
    for both the build machine as well as Minoca, and comes in both static and
    dynamic flavors.

Author:

    Evan Green 14-Feb-2017

Environment:

    C

--*/

from menv import addConfig, application, compiledSources, mconfig,
    sharedLibrary, staticLibrary;

function build() {
    var binplaceLocation = "tools/lib";
    var buildLibConfig = {};
    var buildLibs;
    var buildObjs;
    var buildSources;
    var buildOs = mconfig.build_os;
    var entries;
    var gen;
    var genSources;
    var genTool;
    var grammar;
    var includes;
    var installName;
    var lib;
    var libs;
    var majorVersion = "1";
    var objs;
    var posixSources;
    var sources;

    sources = [
        "capi.c",
        "capilib.c",
        "cdump.c",
        "ckcore.S",
        "compexpr.c",
        "compiler.c",
        "compio.c",
        "compvar.c",
        "core.c",
        "debug.c",
        "dict.c",
        "except.c",
        "fiber.c",
        "gc.c",
        ":gram.c",
        "int.c",
        "lex.c",
        "list.c",
        "module.c",
        "string.c",
        "utils.c",
        "value.c",
        "vm.c",
        "vmsys.c"
    ];

    posixSources = [
        "dlopen.c"
    ];

    if (buildOs == "Windows") {
        buildSources = sources + ["dynwin32.c"];

    } else {
        buildSources = sources + posixSources;
    }

    sources += posixSources;
    libs = [
        "lib/yy:yy"
    ];

    buildLibs = [
        "lib/yy:build_yy"
    ];

    //
    // Create the gramgen tool, which is used to build gram.c.
    //

    genSources = [
        "gram/gramgen.c",
        "lib/yy/gen:build_yygen"
    ];

    gen = {
        "label": "gramgen",
        "inputs": genSources,
        "build": true
    };

    entries = application(gen);
    genTool = {
        "type": "tool",
        "name": "ckgramgen",
        "command": "$O/apps/ck/lib/gramgen $OUT",
        "description": "Generating Grammar - $OUT"
    };

    entries += [genTool];

    //
    // Create the entry for gram.c, which is created by running the gramgen
    // tool.
    //

    grammar = {
        "type": "target",
        "output": "gram.c",
        "inputs": [":gramgen"],
        "tool": "ckgramgen"
    };

    entries += [grammar];

    //
    // Create the static libraries, both for the build machine and targeted at
    // Minoca.
    //

    lib = {
        "label": "libchalk_static",
        "output": "libchalk",
        "inputs": sources
    };

    objs = compiledSources(lib);
    entries += staticLibrary(lib);
    lib = {
        "label": "build_libchalk_static",
        "output": "libchalk",
        "inputs": buildSources,
        "build": true,
        "prefix": "build"
    };

    buildObjs = compiledSources(lib);
    entries += staticLibrary(lib);

    //
    // Create the dynamic libraries.
    //

    if (buildOs == "Linux") {
        buildLibConfig["DYNLIBS"] = ["-ldl"];

    } else if (buildOs == "Darwin") {
        installName = "@rpath/libchalk." + majorVersion + ".dylib";
        buildLibConfig["LDFLAGS"] = ["-install_name " + installName];

    } else if (buildOs == "Windows") {
        binplaceLocation = "tools/bin";
    }

    lib = {
        "label": "libchalk_dynamic",
        "output": "libchalk",
        "inputs": objs[0] + libs,
        "major_version": majorVersion,
        "binplace": ["bin", "apps/usr/lib"]
    };

    entries += sharedLibrary(lib);
    lib = {
        "label": "build_libchalk_dynamic",
        "output": "libchalk",
        "inputs": buildObjs[0] + buildLibs,
        "major_version": majorVersion,
        "config": buildLibConfig,
        "build": true,
        "prefix": "build",
        "binplace": binplaceLocation,
        "nostrip": true
    };

    if (buildOs == "Windows") {
        lib.output = "chalk";
    }

    entries += sharedLibrary(lib);

    //
    // ckcore.o depends on ckcore.ck.
    //

    for (entry in entries) {
        if ((entry.get("output")) && (entry.output.endsWith("ckcore.o"))) {
            entry["implicit"] = ["ckcore.ck"];
            addConfig(entry, "CPPFLAGS", "-I$S/apps/ck/lib");
        }
    }

    return entries;
}

