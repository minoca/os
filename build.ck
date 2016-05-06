/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    Minoca OS

Abstract:

    This module contains the top level build target for Minoca OS.

Author:

    Evan Green 14-Apr-2016

Environment:

    Build

--*/

function build() {
    cflags_line = "$BASE_CPPFLAGS $CPPFLAGS $BASE_CFLAGS $CFLAGS " +
                  "-MMD -MF $OUT.d ";

    cc = {
        "type": "tool",
        "name": "cc",
        "command": "$CC " + cflags_line + "-c -o $OUT $IN",
        "description": "Compiling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    };

    cxx = {
        "type": "tool",
        "name": "cxx",
        "command": "$CXX " + cflags_line + "-c -o $OUT $IN",
        "description": "Compiling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    };

    ldflags_line = "-Wl,-Map=$OUT.map $BASE_LDFLAGS $LDFLAGS ";
    ld = {
        "type": "tool",
        "name": "ld",
        "command": "$CC " + ldflags_line + "-o $OUT $IN -Bdynamic $DYNLIBS",
        "description": "Linking - $OUT",
    };

    ar = {
        "type": "tool",
        "name": "ar",
        "command": "$AR rcs $OUT $IN",
        "description": "Building Library - $OUT",
    };

    asflags_line = cflags_line + "$BASE_ASFLAGS $ASFLAGS ";
    as = {
        "type": "tool",
        "name": "as",
        "command": "$CC " + asflags_line + "-c -o $OUT $IN",
        "description": "Assembling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    };

    objcopy = {
        "type": "tool",
        "name": "objcopy",
        "command": "$SHELL -c \"cd `dirname $IN` && $OBJCOPY $OBJCOPY_FLAGS `basename $IN` $OUT\"",
        "description": "Objectifying - $IN"
    };

    strip_tool = {
        "type": "tool",
        "name": "strip",
        "command": "$STRIP $STRIP_FLAGS -o $OUT $IN",
        "description": "Stripping - $OUT",
    };

    build_cflags_line = "$BUILD_BASE_CPPFLAGS $CPPFLAGS $BUILD_BASE_CFLAGS " +
                        "$CFLAGS -MMD -MF $OUT.d ";

    build_cc = {
        "type": "tool",
        "name": "build_cc",
        "command": "$BUILD_CC " + build_cflags_line + "-c -o $OUT $IN",
        "description": "Compiling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    };

    build_cxx = {
        "type": "tool",
        "name": "build_cxx",
        "command": "$BUILD_CXX " + build_cflags_line + "-c -o $OUT $IN",
        "description": "Compiling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    };

    build_ldflags_line = "-Wl,-Map=$OUT.map $BUILD_BASE_LDFLAGS $LDFLAGS ";
    build_ld = {
        "type": "tool",
        "name": "build_ld",
        "command": "$BUILD_CC " + build_ldflags_line + "-o $OUT $IN -Bdynamic $DYNLIBS",
        "description": "Linking - $OUT",
    };

    build_ar = {
        "type": "tool",
        "name": "build_ar",
        "command": "$BUILD_AR rcs $OUT $IN",
        "description": "Building Library - $OUT",
    };

    build_asflags_line = build_cflags_line + "$BUILD_BASE_ASFLAGS $ASFLAGS ";
    build_as = {
        "type": "tool",
        "name": "build_as",
        "command": "$BUILD_CC " + build_asflags_line + "-c -o $OUT $IN",
        "description": "Assembling - $IN",
        "depsformat": "gcc",
        "depfile": "$OUT.d"
    };

    build_objcopy = {
        "type": "tool",
        "name": "build_objcopy",
        "command": "$SHELL -c \"cd `dirname $IN` && $BUILD_OBJCOPY $BUILD_OBJCOPY_FLAGS `basename $IN` $OUT\"",
        "description": "Objectifying - $IN"
    };

    build_strip = {
        "type": "tool",
        "name": "build_strip",
        "command": "$BUILD_STRIP $STRIP_FLAGS -o $OUT $IN",
        "description": "Stripping - $OUT",
    };

    build_rcc = {
        "type": "tool",
        "name": "build_rcc",
        "command": "$RCC -o $OUT $IN",
        "description": "Compiling Resource - $IN",
    };

    iasl = {
        "type": "tool",
        "name": "iasl",
        "command": "$SHELL -c \"$IASL $IASL_FLAGS -p $OUT $IN > $OUT.stdout\"",
        "description": "Compiling ASL - $IN"
    };

    cp = {
        "type": "tool",
        "name": "copy",
        "command": "$SHELL -c \"cp $CPFLAGS $IN $OUT && [ -z $CHMOD_FLAGS ] || chmod $CHMOD_FLAGS $OUT\"",
        "description": "Copying - $IN -> $OUT"
    };

    if (build_os == "Windows") {
        symlink_command = "cp $IN $OUT";

    } else {
        symlink_command = "ln -sf $SYMLINK_IN $OUT";
    }

    symlink = {
        "type": "tool",
        "name": "symlink",
        "command": symlink_command,
        "description": "Symlinking - $OUT"
    };

    stamp = {
        "type": "tool",
        "name": "stamp",
        "command": "$SHELL -c \"date > $OUT\"",
        "description": "Stamp - $OUT"
    };

    touch = {
        "type": "tool",
        "name": "touch",
        "command": "touch $OUT",
        "description": "Touch - $OUT"
    };

    gen_version = {
        "type": "tool",
        "name": "gen_version",
        "command": "$SHELL $//tasks/build/print_version.sh $OUT $FORM $MAJOR $MINOR $REVISION $RELEASE $SERIAL $BUILD_STRING",
        "description": "Versioning - $OUT"
    };

    config_entry = {
        "type": "global_config",
        "config": global_config
    };

    entries = [cc, cxx, ld, ar, as, objcopy, strip_tool,
               build_cc, build_cxx, build_ld, build_ar, build_as, build_rcc,
               build_objcopy, build_strip, iasl, cp, symlink, stamp, touch,
               gen_version, config_entry];

    all = [
        "//lib:test_apps",
        "//images:"
    ];

    all_group = group("all", all);
    all_group[0]["default"] = TRUE;
    entries += all_group;
    return entries;
}

return build();
