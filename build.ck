/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Minoca OS

Abstract:

    This module contains the top level build target for Minoca OS.

Author:

    Evan Green 14-Apr-2016

Environment:

    Build

--*/

import menv;
from menv import setupEnv, group;

function build() {
    var allGroup;
    var buildCflagsLine = "$BUILD_BASE_CPPFLAGS $CPPFLAGS " +
                          "$BUILD_BASE_CFLAGS $CFLAGS -MMD -MF $OUT.d ";

    var buildAsflagsLine = buildCflagsLine +
                           "$BUILD_BASE_ASFLAGS $ASFLAGS ";

    var buildLdflagsLine = "-Wl,-Map=$OUT.map $BUILD_BASE_LDFLAGS $LDFLAGS ";
    var cflagsLine = "$BASE_CPPFLAGS $CPPFLAGS $BASE_CFLAGS $CFLAGS "
                     "-MMD -MF $OUT.d ";

    var asflagsLine = cflagsLine + "$BASE_ASFLAGS $ASFLAGS ";
    var entries;
    var ldflagsLine = "-Wl,-Map=$OUT.map $BASE_LDFLAGS $LDFLAGS ";
    var mconfig;
    var symlinkCommand = "ln -sf $SYMLINK_IN $OUT";
    var buildLdLine = "$BUILD_CC " + buildLdflagsLine +
                      "-o $OUT $IN -Bdynamic $DYNLIBS";

    var tools;

    setupEnv();
    mconfig = menv.mconfig;
    if (mconfig.build_os == "Windows") {
        symlinkCommand = "cp $IN $OUT";
    }

    //
    // On Mac OS there shouldn't be a -Bdynamic flag to indicate the start of
    // the dynamic libraries section.
    //

    if (mconfig.build_os == "Darwin") {
        buildLdLine = "$BUILD_CC " + buildLdflagsLine +
                      "-o $OUT $IN $DYNLIBS";
    }

    //
    // Define the tools used.
    //

    tools = [

    //
    // C compiler for target binaries.
    //

    {
        "type": "tool",
        "name": "cc",
        "command": "$CC " + cflagsLine + "-c -o $OUT $IN",
        "description": "Compiling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    },

    //
    // C++ compiler for target binaries.
    //

    {
        "type": "tool",
        "name": "cxx",
        "command": "$CXX " + cflagsLine + "-c -o $OUT $IN",
        "description": "Compiling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    },

    //
    // Linker for target binaries.
    //

    {
        "type": "tool",
        "name": "ld",
        "command": "$CC " + ldflagsLine + "-o $OUT $IN -Bdynamic $DYNLIBS",
        "description": "Linking - $OUT",
    },

    //
    // Static archiver for target binaries.
    //

    {
        "type": "tool",
        "name": "ar",
        "command": "$AR rcs $OUT $IN",
        "description": "Building Library - $OUT",
    },

    //
    // Assembler for target binaries.
    //

    {
        "type": "tool",
        "name": "as",
        "command": "$CC " + asflagsLine + "-c -o $OUT $IN",
        "description": "Assembling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    },

    //
    // Objcopy for target binaries.
    //

    {
        "type": "tool",
        "name": "objcopy",
        "command": "$SHELL -c \"cd `dirname $IN` && $OBJCOPY $OBJCOPY_FLAGS `basename $IN` $OUT\"",
        "description": "Objectifying - $IN"
    },

    //
    // Strip for target binaries.
    //

    {
        "type": "tool",
        "name": "strip",
        "command": "$STRIP $STRIP_FLAGS -o $OUT $IN",
        "description": "Stripping - $OUT",
    },

    //
    // C compiler for the build machine.
    //

    {
        "type": "tool",
        "name": "build_cc",
        "command": "$BUILD_CC " + buildCflagsLine + "-c -o $OUT $IN",
        "description": "Compiling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    },

    //
    // C++ compiler for the build machine.
    //

    {
        "type": "tool",
        "name": "build_cxx",
        "command": "$BUILD_CXX " + buildCflagsLine + "-c -o $OUT $IN",
        "description": "Compiling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    },

    //
    // Linker for the build machine.
    //

    {
        "type": "tool",
        "name": "build_ld",
        "command": "$BUILD_CC " + buildLdflagsLine + "-o $OUT $IN -Bdynamic $DYNLIBS",
        "description": "Linking - $OUT",
    },

    //
    // Static archiver for the build machine.
    //

    {
        "type": "tool",
        "name": "build_ar",
        "command": "$BUILD_AR rcs $OUT $IN",
        "description": "Building Library - $OUT",
    },

    //
    // Assembler for the build machine.
    //

    {
        "type": "tool",
        "name": "build_as",
        "command": "$BUILD_CC " + buildAsflagsLine + "-c -o $OUT $IN",
        "description": "Assembling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    },

    //
    // Strip for the build machine.
    //

    {
        "type": "tool",
        "name": "build_strip",
        "command": "$BUILD_STRIP $STRIP_FLAGS -o $OUT $IN",
        "description": "Stripping - $OUT",
    },

    //
    // Windows resource compiler for the build machine.
    //

    {
        "type": "tool",
        "name": "build_rcc",
        "command": "$RCC -o $OUT $IN",
        "description": "Compiling Resource - $IN",
    },

    //
    // ACPI assembler used to build firmware images.
    //

    {
        "type": "tool",
        "name": "iasl",
        "command": "$SHELL -c \"$IASL $IASL_FLAGS -p $OUT $IN > $OUT.stdout\"",
        "description": "Compiling ASL - $IN"
    },

    //
    // Copy files from one location to another.
    //

    {
        "type": "tool",
        "name": "copy",
        "command": "$SHELL -c \"cp $CPFLAGS $IN $OUT && [ -z $CHMOD_FLAGS ] || chmod $CHMOD_FLAGS $OUT\"",
        "description": "Copying - $IN -> $OUT"
    },

    //
    // Create symbolic links (or just copy on Windows).
    //

    {
        "type": "tool",
        "name": "symlink",
        "command": symlinkCommand,
        "description": "Symlinking - $OUT"
    },

    //
    // Touch a file with the date.
    //

    {
        "type": "tool",
        "name": "stamp",
        "command": "$SHELL -c \"date > $OUT\"",
        "description": "Stamp - $OUT"
    },

    //
    // Touch to create a timestamped empty file.
    //

    {
        "type": "tool",
        "name": "touch",
        "command": "touch $OUT",
        "description": "Touch - $OUT"
    },

    //
    // Generate a version.h.
    //

    {
        "type": "tool",
        "name": "gen_version",
        "command": "$SHELL $S/tasks/build/print_version.sh $OUT $FORM $MAJOR $MINOR $REVISION $RELEASE $SERIAL $BUILD_STRING",
        "description": "Versioning - $OUT"
    }];

    entries = [
        "lib/basevid:basevid",
        "lib/basevid:build_basevid",
        "lib/bconflib:bconf",
        "lib/bconflib:build_bconf",
        "lib/crypto:crypto",
        "lib/crypto:build_crypto",
        "lib/crypto/ssl:ssl",
        "lib/crypto/testcryp:build_testcryp",
        "lib/rtl/testrtl:build_testrtl",
        "lib/fatlib:fat",
        "lib/fatlib/fattest:build_fattest",
        "lib/im:im",
        "lib/im:build_im",
        "lib/partlib:partlib",
        "lib/partlib:build_partlib",
        "lib/termlib:termlib",
        "lib/termlib:build_termlib",
        "lib/yy:yy",
        "lib/yy:build_yy",
        "lib/yy/yytest:build_yytest",
        "lib/yy/gen:yygen",
        "lib/yy/gen:build_yygen",
        "lib:test_apps",
        "kernel:kernel"
    ];

    allGroup = group("all", entries);
    allGroup[0].default = true;
    entries = allGroup + tools;
    return entries;
}

