/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    env.ck

Abstract:

    This build module contains the environment and functions used throughout
    the Minoca OS build.

Author:

    Evan Green 1-Feb-2017

Environment:

    Build

--*/

//
// -------------------------------------------------------------------- Imports
//

from mingen import config;
from os import getenv, basename;

//
// -------------------------------------------------------------------- Globals
//

var mconfig;

//
// ------------------------------------------------------------------ Functions
//

function
initListFromEnvironment (
    name,
    default
    )

/*++

Routine Description:

    This routine gets an environment variable. It either returns that
    environment variable wrapped in a list, or the given default.

Arguments:

    name - Supplies the name of the environment variable.

    default - Supplies the default value to return if not set.

Return Value:

    Returns eithe the provided default or has the contents of the environment
    variable wrapped in a list.

--*/

{

    var value = getenv(name);

    if (value != null) {
        return [value];
    }

    return default;
}

function
getTools (
    )

/*++

Routine Description:

    This routine is called as part of setupEnv, at the end. It returns the
    basic set of tools used by the environment.

Arguments:

    None.

Return Value:

    Returns a list of the basic tools used in the environment.

--*/

{

    var buildCflagsLine = "$BUILD_BASE_CPPFLAGS $CPPFLAGS " +
                          "$BUILD_BASE_CFLAGS $CFLAGS -MMD -MF $OUT.d ";

    var buildAsflagsLine = buildCflagsLine +
                           "$BUILD_BASE_ASFLAGS $ASFLAGS ";

    var buildLdflagsLine = "$BUILD_BASE_LDFLAGS $LDFLAGS ";
    var cflagsLine = "$BASE_CPPFLAGS $CPPFLAGS $BASE_CFLAGS $CFLAGS "
                     "-MMD -MF $OUT.d ";

    var asflagsLine = cflagsLine + "$BASE_ASFLAGS $ASFLAGS ";
    var entries;
    var ldflagsLine = "-Wl,-Map=$OUT.map $BASE_LDFLAGS $LDFLAGS ";
    var symlinkCommand = "ln -sf $SYMLINK_IN $OUT";
    var buildLdLine = "$BUILD_CC " + buildLdflagsLine +
                      "-o $OUT $IN -Bdynamic $DYNLIBS";

    var tools;

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

    //
    // Create a .map file (except on Mac, which doesn't support it).
    //

    } else {
        buildLdflagsLine = "-Wl,-Map=$OUT.map " + buildLdflagsLine;
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
        "command": "$SHELL -c \"cd `dirname $IN` && "
                   "$OBJCOPY $OBJCOPY_FLAGS `basename $IN` $OUT\"",

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
        "command": "$BUILD_CC " + buildLdflagsLine +
                   "-o $OUT $IN -Bdynamic $DYNLIBS",

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
        "command": "$SHELL -c \"cp $CPFLAGS $IN $OUT && [ -z $CHMOD_FLAGS ] || "
                   "chmod $CHMOD_FLAGS $OUT\"",

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
    // Create a directory.
    //

    {
        "type": "tool",
        "name": "mkdir",
        "command": "mkdir -p $OUT",
        "description": "mkdir $OUT"
    },

    //
    // Generate a version.h.
    //

    {
        "type": "tool",
        "name": "gen_version",
        "command": "$SHELL $S/tasks/build/print_version.sh $OUT $FORM "
                   "$MAJOR $MINOR $REVISION $RELEASE $SERIAL $BUILD_STRING",

        "description": "Versioning - $OUT"
    }];

    return tools;
}

function
setupEnv (
    )

/*++

Routine Description:

    This routine is called once to set up the build environment.

Arguments:

    None.

Return Value:

    Returns the basic set of tools used by the environment.

--*/

{

    var archVariant;

    //
    // Prefer Ninja files.
    //

    config.format ?= "ninja";

    //
    // Set up the Minoca config dictionary.
    //

    mconfig = {};
    mconfig.build_os = config.build_os;
    mconfig.build_machine = config.build_machine;
    mconfig.build_variant = "";
    if (mconfig.build_machine == "i686") {
        mconfig.build_arch = "x86";

    } else if (mconfig.build_machine == "i586") {
        mconfig.build_arch = "x86";
        mconfig.build_variant = "q";

    } else if ((mconfig.build_machine == "armv7") ||
               (mconfig.build_machine == "armv6")) {

        mconfig.build_arch = mconfig.build_machine;

    } else if (mconfig.build_machine == "x86_64") {
        mconfig.build_arch = "x64";
    }

    mconfig.arch = getenv("ARCH");
    mconfig.arch ?= mconfig.build_arch;
    mconfig.arch ?= "x86";
    mconfig.debug = getenv("DEBUG");
    mconfig.debug ?= "dbg";
    mconfig.variant = getenv("VARIANT");
    mconfig.variant ?= mconfig.build_variant;
    mconfig.release_level = "SystemReleaseDevelopment";
    mconfig.cflags = initListFromEnvironment("CFLAGS",
                                             ["-O2", "-Wall", "-Werror"]);

    mconfig.kernelCflags = [];
    mconfig.cppflags = initListFromEnvironment("CPPFLAGS", []);
    mconfig.ldflags = initListFromEnvironment("LDFLAGS", []);
    mconfig.asflags = initListFromEnvironment("ASFLAGS", []);
    mconfig.stripflags = initListFromEnvironment("STRIP_FLAGS", []);
    mconfig.build_cc = getenv("BUILD_CC");
    mconfig.build_cc ?= "gcc";
    mconfig.build_ar = getenv("BUILD_AR");
    mconfig.build_ar ?= "ar";
    mconfig.build_strip = getenv("BUILD_STRIP");
    mconfig.build_strip ?= "strip";
    config.output ?= "$S/../" + mconfig.arch + mconfig.variant + mconfig.debug +
                     "/obj/os";

    mconfig.outroot = "$O/../..";
    mconfig.binroot = mconfig.outroot + "/bin";
    mconfig.stripped = mconfig.binroot + "/stripped";
    mconfig.cc = getenv("CC");
    mconfig.ar = getenv("AR");
    mconfig.objcopy = getenv("OBJCOPY");
    mconfig.strip = getenv("STRIP");
    mconfig.rcc = getenv("RCC");
    mconfig.rcc ?= "windres";
    mconfig.iasl = getenv("IASL");
    mconfig.iasl ?= "iasl";
    mconfig.shell = getenv("SHELL");
    mconfig.shell ?= "sh";
    mconfig.target = null;

    //
    // Add in the command line variables, then define the derived variables.
    //

    for (key in config.cmdvars) {
        mconfig[key] = config.cmdvars[key];
    }

    archVariant = mconfig.arch + mconfig.variant;
    if (!mconfig.target) {
        if (archVariant == "x86") {
            mconfig.target = "i686-pc-minoca";

        } else if (archVariant == "x86q") {
            mconfig.target = "i586-pc-minoca";

        } else if ((mconfig.arch == "armv7") || (mconfig.arch == "armv6")) {
            mconfig.target = "arm-none-minoca";

        } else if (mconfig.arch == "x64") {
            mconfig.target = "x86_64-pc-minoca";

        } else {
            Core.raise(ValueError("Unknown architecture" + mconfig.arch));
        }
    }

    mconfig.native = false;
    if ((mconfig.build_os == "Minoca") &&
        (mconfig.arch == mconfig.build_arch)) {

        mconfig.native = true;
    }

    if (mconfig.native) {
        mconfig.cc ?= mconfig.build_cc;
        mconfig.ar ?= mconfig.build_ar;
        mconfig.strip ?= mconfig.build_strip;
        mconfig.objcopy ?= "objcopy";

    } else {
        mconfig.cc ?= mconfig.target + "-gcc";
        mconfig.ar ?= mconfig.target + "-ar";
        mconfig.strip ?= mconfig.target + "-strip";
        mconfig.objcopy ?= mconfig.target + "-objcopy";
    }

    if (mconfig.debug == "dbg") {
        mconfig.cflags += ["-DDEBUG=1"];

    } else {
        mconfig.cflags += ["-Wno-unused-but-set-variable", "-DNDEBUG"];
    }

    mconfig.cflags += ["-fno-builtin",
                       "-g",
                       "-save-temps=obj",
                       "-ffunction-sections",
                       "-fdata-sections",
                       "-fvisibility=hidden"];

    mconfig.cppflags += ["-I$S/include"];
    mconfig.build_cflags = [] + mconfig.cflags;
    mconfig.build_cppflags = [] + mconfig.cppflags;
    mconfig.cflags += ["-fpic"];

    //
    // Windows cannot handle -fpic, but everyone else can.
    //

    if (mconfig.build_os == "Windows") {
        mconfig.build_cflags += ["-mno-ms-bitfields"];

    } else {
        mconfig.build_cflags += ["-fpic"];
    }

    if (mconfig.build_os == "Darwin") {
        mconfig.build_cflags += ["-Wno-tautological-compare",
                                 "-Wno-parentheses-equality"];
    }

    //
    // Add some architecture variant flags.
    //

    if (archVariant == "x86q") {
        mconfig.cppflags += ["-Wa,-momit-lock-prefix=yes", "-march=i586"];

    } else if (archVariant == "x64") {
        mconfig.kernelCflags = ["-mno-sse", "-mno-red-zone"];

    } else if (archVariant == "armv6") {
        mconfig.cflags += ["-march=armv6zk", "-marm", "-mfpu=vfp"];
    }

    mconfig.build_asflags = [];
    mconfig.asflags += ["-Wa,-g"];
    mconfig.build_ldflags = initListFromEnvironment("BUILD_LDFLAGS",
                                                    [] + mconfig.ldflags);

    mconfig.ldflags += ["-Wl,--gc-sections"];

    //
    // Mac OS cannot handle --gc-sections or strip -p.
    //

    if (mconfig.build_os != "Darwin") {
        mconfig.build_ldflags += ["-Wl,--gc-sections"];
        mconfig.stripflags += ["-p"];
    }

    //
    // Define the set of variables that get passed all the way through to the
    // final Make/ninja file. Passing these on as variables rather than
    // substituting during the mingen build process allows for a smaller
    // build file, and easier manual tweaking by the user.
    //

    config.vars = {
        "BUILD_CC": mconfig.build_cc,
        "BUILD_AR": mconfig.build_ar,
        "BUILD_STRIP": mconfig.build_strip,
        "CC": mconfig.cc,
        "AR": mconfig.ar,
        "STRIP": mconfig.strip,
        "OBJCOPY": mconfig.objcopy,
        "RCC": mconfig.rcc,
        "IASL": mconfig.iasl,
        "SHELL": mconfig.shell,
        "BASE_CFLAGS": mconfig.cflags,
        "KERNEL_CFLAGS": mconfig.kernelCflags,
        "BASE_CPPFLAGS": mconfig.cppflags,
        "BASE_LDFLAGS": mconfig.ldflags,
        "BASE_ASFLAGS": mconfig.asflags,
        "BUILD_BASE_CFLAGS": mconfig.build_cflags,
        "BUILD_BASE_CPPFLAGS": mconfig.build_cppflags,
        "BUILD_BASE_LDFLAGS": mconfig.build_ldflags,
        "BUILD_BASE_ASFLAGS": mconfig.build_asflags,
        "STRIP_FLAGS": mconfig.stripflags,
        "IASL_FLAGS": ["-we"]
    };

    if (config.verbose) {
        Core.print("Minoca Build Configuration:");
        for (key in mconfig) {
            Core.print("\t%s: %s" % [key, mconfig[key].__str()]);
        }
    }

    return getTools();
}

function
addConfig (
    entry,
    name,
    value
    )

/*++

Routine Description:

    This routine adds a configure option to a list, ensuring that both the
    config dictionary and option already exist.

Arguments:

    entry - Supplies the entry to add the configure option to.

    name - Supplies the name of the option to add.

    value - Supplies the new value to add to the list of options.

Return Value:

    None.

--*/

{

    if (!entry.get("config")) {
        entry.config = {};
    }

    if (!entry.config.get(name)) {
        entry.config[name] = [];
    }

    entry.config[name] += [value];
    return;
}

function
group (
    name,
    entries
    )

/*++

Routine Description:

    This routine creates a phony target that groups a bunch of different
    targets together under a common name.

Arguments:

    name - Supplies the name of the new group target.

    entries - Supplies the list of entries.

Return Value:

    Returns a list containing the entry for the group target.

--*/

{
    var entry = {
        "label": name,
        "type": "target",
        "tool": "phony",
        "inputs": entries,
        "config": {}
    };

    return [entry];
}

function
touch (
    destination,
    destinationLabel,
    mode
    )

/*++

Routine Description:

    This routine creates an empty file target.

Arguments:

    destination - Supplies the copy destination.

    destinationLabel - Supplies a label naming the target.

    mode - Supplies the chmod mode of the destination.

Return Value:

    Returns a list containing the copy entry.

--*/

{

    var config = {};

    if (mode) {
        config["CHMOD_FLAGS"] = mode;
    }

    destinationLabel ?= destination;
    var entry = {
        "type": "target",
        "tool": "touch",
        "label": destinationLabel,
        "output": destination,
        "config": config
    };

    return [entry];
}

function
makedir (
    destination,
    destinationLabel
    )

/*++

Routine Description:

    This routine creates an empty directory target.

Arguments:

    destination - Supplies the copy destination.

    destinationLabel - Supplies a label naming the target.

Return Value:

    Returns a list containing the copy entry.

--*/

{

    destinationLabel ?= destination;
    var entry = {
        "type": "target",
        "tool": "mkdir",
        "label": destinationLabel,
        "output": destination,
    };

    return [entry];
}

function
copy (
    source,
    destination,
    destinationLabel,
    flags,
    mode
    )

/*++

Routine Description:

    This routine creates a target that copies a file from one place to another.

Arguments:

    source - Supplies the source to copy.

    destination - Supplies the copy destination.

    destinationLabel - Supplies a label naming the copy target.

    flags - Supplies the flags to include in the copy.

    mode - Supplies the chmod mode of the destination.

Return Value:

    Returns a list containing the copy entry.

--*/

{

    var config = {};

    if (flags) {
        config["CPFLAGS"] = flags;
    }

    if (mode) {
        config["CHMOD_FLAGS"] = mode;
    }

    var entry = {
        "type": "target",
        "tool": "copy",
        "label": destinationLabel,
        "inputs": [source],
        "output": destination,
        "config": config
    };

    if (!destinationLabel) {
        entry["label"] = destination;
    }

    return [entry];
}

function
strip (
    params
    )

/*++

Routine Description:

    This routine converts an entry to a strip entry, where the target will be
    stripped.

Arguments:

    params - Supplies the existing entry.

Return Value:

    Returns a list containing the strip entry.

--*/

{

    params.type = "target";
    params.tool = "strip";
    if (params.get("build")) {
        params.tool = "build_strip";
    }

    return [params];
}

function
binplace (
    params
    )

/*++

Routine Description:

    This routine replaces the current target with a copied version in the
    final bin directory. This will also create a stripped version in the
    stripped directory unless told not to.

Arguments:

    params - Supplies the existing entry.

Return Value:

    Returns a list containing the strip entry.

--*/

{

    var build = params.get("build");
    var copiedEntry;
    var cpflags = params.get("cpflags");;
    var destination;
    var destinationLabel;
    var destinationPath;
    var element;
    var entries = [];
    var extraCopy;
    var fileName;
    var label = params.get("label");
    var mode = params.get("mode");
    var newOriginalLabel;
    var originalTarget;
    var source = params.get("output");
    var strippedEntry;
    var strippedOutput;

    label ?= source;
    source ?= label;
    if ((!label) || (!source)) {
        Core.raise(ValueError("Label or output must be defined"));
    }

    //
    // Set the output since the label is going to be renamed and create the
    // copy target.
    //

    params.output = source;
    params.type = "target";
    fileName = basename(source);
    destination = params.get("binplace");
    newOriginalLabel = label + "_orig";
    originalTarget = ":" + newOriginalLabel;

    //
    // Create the first or only one with the genuine label.
    //

    element = destination;
    if (element is List) {
        element = element[0];
    }

    if (!element) {
        element = "bin";
    }

    destinationPath = mconfig.outroot + "/" + element + "/" + fileName;
    copiedEntry = copy(originalTarget,
                       destinationPath,
                       label,
                       cpflags,
                       mode)[0];

    entries.append(copiedEntry);

    //
    // Handle binplacing into several areas.
    //

    if (destination is List) {
        for (index in 1..destination.length()) {
            element = destination[index];
            if (!element) {
                element = "bin";
            }

            destinationPath = mconfig.outroot + "/" + element + "/" + fileName;
            destinationLabel = "%s_%d" % [label, index];
            extraCopy = copy(originalTarget,
                             destinationPath,
                             destinationLabel,
                             cpflags,
                             mode)[0];

            copiedEntry.implicit = [":" + destinationLabel];
            entries.append(extraCopy);
        }
    }

    //
    // The original label was given to the copied destination, so tack an _orig
    // on the source label.
    //

    params.label = newOriginalLabel;

    //
    // Unless asked not to, create a stripped entry as well.
    //

    if (!params.get("nostrip")) {
        if (build) {
            strippedOutput = mconfig.stripped + "/build/" + fileName;

        } else {
            strippedOutput = mconfig.stripped + "/" + fileName;
        }

        strippedEntry = {
            "label": label + "_stripped",
            "inputs": [originalTarget],
            "output": strippedOutput,
            "build": build,
        };

        //
        // Make the binplaced copy depend on the stripped version.
        //

        copiedEntry.implicit = [":" + strippedEntry["label"]];
        entries += strip(strippedEntry);
    }

    entries.append(params);
    return entries;
}

function
compiledSources (
    params
    )

/*++

Routine Description:

    This routine compiles a group of object file targets from source files.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns the list containing a list of object names, and a list of object
    targets.

--*/

{

    var build = params.get("build");
    var entries = [];
    var ext;
    var includes = params.get("includes");
    var inputParts;
    var inputs = params.inputs;
    var objName;
    var obj;
    var objs = [];
    var sourcesConfig = params.get("sources_config");
    var prefix = params.get("prefix");
    var suffix;
    var tool;

    if (includes) {
        sourcesConfig ?= {};
        if (!sourcesConfig.get("CPPFLAGS")) {
            sourcesConfig.CPPFLAGS = [];
        }

        for (include in includes) {
            sourcesConfig["CPPFLAGS"] += ["-I" + include];
        }
    }

    if (inputs.length() == 0) {
        Core.raise(ValueError("Compilation must have inputs"));
    }

    for (input in inputs) {
        inputParts = input.rsplit(".", 1);
        try {
            ext = inputParts[1];

        } except IndexError {
            ext = "";
        }

        suffix = ".o";
        if (ext == "c") {
            tool = "cc";

        } else if (ext == "cc") {
            tool = "cxx";

        } else if (ext == "S") {
            tool = "as";

        } else if (ext == "rc") {
            tool = "rcc";
            suffix = ".rsc";

        } else {
            objs += [input];
            continue;
        }

        if (build) {
            tool = "build_" + tool;
        }

        objName = inputParts[0] + suffix;
        if (objName[0] == ":") {
            objName = objName[1...-1];
        }

        if (prefix) {
            objName = prefix + "/" + objName;
        }

        obj = {
            "type": "target",
            "label": objName,
            "output": objName,
            "inputs": [input],
            "tool": tool,
            "config": sourcesConfig,
        };

        entries += [obj];
        objs += [":" + objName];
    }

    if (prefix) {
        if (!params.get("output")) {
            params.output = params.label;
        }

        params.output = prefix + "/" + params.output;
    }

    return [objs, entries];
}

function
executable (
    params
    )

/*++

Routine Description:

    This routine links a set of sources into some sort of executable or shared
    object.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns the list of the linked entry.

--*/

{

    var build = params.get("build");
    var entry = params.get("entry");
    var compilation = compiledSources(params);
    var linkerScript = params.get("linker_script");
    var objs = compilation[0];
    var entries = compilation[1];
    var textAddress = params.get("text_address");;

    params.type = "target";
    params.inputs = objs;
    params.tool = "ld";
    if (build) {
        params.tool = "build_ld";
    }

    //
    // Convert options for text_address, linker_script, and entry to actual
    // LDFLAGS.
    //

    if (textAddress) {
        addConfig(params, "LDFLAGS", "-Wl,-Ttext-segment=" + textAddress);
        addConfig(params,
                  "LDFLAGS",
                  "-Wl,--section-start,.init=" + textAddress);
    }

    if (linkerScript) {
        addConfig(params, "LDFLAGS", "-Wl,-T" + linkerScript);
    }

    if (entry != null) {
        addConfig(params, "LDFLAGS", "-Wl,-e" + entry);
        addConfig(params, "LDFLAGS", "-Wl,-u" + entry);
    }

    if (params.get("binplace")) {
        entries += binplace(params);

    } else {
        entries += [params];
    }

    return entries;
}

function
application (
    params
    )

/*++

Routine Description:

    This routine creates a position independent application.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns the list of the application entry.

--*/

{

    var build = params.get("build");
    var exename = params.get("output");

    exename ?= params.get("label");
    if (!exename) {
        Core.raise(ValueError("Missing output or label"));
    }

    if (build && (mconfig.build_os == "Windows")) {
        params.output = exename + ".exe";
    }

    if (build && (mconfig.build_os == "Darwin")) {
        addConfig(params, "LDFLAGS", "-Wl,-pie");

    } else {
        addConfig(params, "LDFLAGS", "-pie");
    }

    if ((!build) && (params.get("binplace") == null)) {
        params.binplace = "bin";
    }

    return executable(params);
}

function
staticApplication (
    params
    )

/*++

Routine Description:

    This routine creates a static application suitable for kernel or firmware
    environments.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns the list of the application entry.

--*/

{

    if (!(params.get("sources_config") is Dict)) {
        params.sources_config = {};
    }

    if (!(params.sources_config.get("CFLAGS") is List)) {
        params.sources_config.CFLAGS = [];
    }

    params.sources_config.CFLAGS += ["$KERNEL_CFLAGS"];
    addConfig(params, "LDFLAGS", "-static");
    addConfig(params, "LDFLAGS", "-nostdlib");
    return executable(params);
}

function
sharedLibrary (
    params
    )

/*++

Routine Description:

    This routine creates a shared library or DLL.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns the list of the application entry.

--*/

{

    var build = params.get("build");
    var majorVersion = params.get("major_version");
    var minorVersion = params.get("minor_version");
    var soname = params.get("soname");

    soname ?= params.get("output");
    soname ?= params.get("label");
    if (!soname) {
        Core.raise(ValueError(
                          "One of output, label, or soname must be defined."));
    }

    //
    // Darwin shared libraries build with a whole different ballgame of options.
    //

    if (build && (mconfig.build_os == "Darwin")) {
        majorVersion ?= "0";
        minorVersion ?= "0";
        soname += ".%s.dylib" % majorVersion;
        addConfig(params,
                  "LDFLAGS",
                  "-undefined dynamic_lookup -dynamiclib");

        addConfig(params,
                  "LDFLAGS",
                  "-current_version %s.%s" % [majorVersion, minorVersion]);

        addConfig(params,
                  "LDFLAGS",
                  "-compatibility_version %s.%d" % [majorVersion, 0]);


    } else {
        addConfig(params, "LDFLAGS", "-shared");
        if ((!build) || (mconfig.build_os != "Windows")) {
            soname += ".so";
            if (majorVersion != null) {
                soname += "." + majorVersion;
            }

            addConfig(params, "LDFLAGS", "-Wl,-soname=" + soname);

        } else {
            soname += ".dll";
        }
    }

    params.output = soname;
    if ((!build) && (params.get("binplace") == null)) {
        params.binplace = "bin";
    }

    return executable(params);
}

function
staticLibrary (
    params
    )

/*++

Routine Description:

    This routine creates a static library.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns the list of the application entry.

--*/

{

    var build = params.get("build");
    var compilation = compiledSources(params);
    var objs = compilation[0];
    var output;
    var entries = compilation[1];

    params.type = "target";
    output = params.get("output");
    output ?= params.get("label");
    if (!output) {
        Core.raise(ValueError("output or label must be defined"));
    }

    params.output = output + ".a";
    params.inputs  = objs;
    params.tool = "ar";
    if (build) {
        params.tool = "build_ar";
    }

    if (params.get("binplace")) {
        entries += binplace(params);

    } else {
        entries += [params];
    }

    return entries;
}

function
kernelLibrary (
    params
    )

/*++

Routine Description:

    This routine creates a static library compiled suitably for running within
    the kernel.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns the list of the application entry.

--*/

{

    if (!(params.get("sources_config") is Dict)) {
        params.sources_config = {};
    }

    if (!(params.sources_config.get("CFLAGS") is List)) {
        params.sources_config.CFLAGS = [];
    }

    params.sources_config.CFLAGS += ["$KERNEL_CFLAGS"];
    return staticLibrary(params);
}

function
compiledAsl (
    inputs
    )

/*++

Routine Description:

    This routine creates a list of compiled .aml files from a list of .asl
    files.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns a list where the first element is a list of all the resulting
    target names, and the second element is a list of the target entries.

--*/

{

    var entries = [];
    var ext;
    var inputParts;
    var objName;
    var obj;
    var objs = [];

    if (inputs.length() == 0) {
        Core.raise(ValueError("Compilation must have inputs"));
    }

    for (input in inputs) {
        inputParts = input.rsplit(".", 1);
        ext = inputParts[1];
        objName = inputParts[0] + ".aml";
        obj = {
            "type": "target",
            "label": objName,
            "output": objName,
            "inputs": [input],
            "tool": "iasl"
        };

        entries += [obj];
        objs += [":" + objName];
    }

    return [objs, entries];
}

function
objectifiedBinaries (
    params
    )

/*++

Routine Description:

    This routine creates a group of object file targets from binary files.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns a list of object names in the first element and the object file
    target entries in the second element.

--*/

{

    var build = params.get("build");
    var entries = [];
    var inputs = params.inputs;
    var obj;
    var objcopyConfig = params.get("config");
    var objName;
    var objs = [];
    var prefix = params.get("prefix");
    var tool;

    if (inputs.length() == 0) {
        Core.raise(ValueError("Compilation must have inputs"));
    }

    for (input in inputs) {
        tool = "objcopy";
        if (build) {
            tool = "build_objcopy";;
        }

        objName = input + ".o";
        if (prefix) {
            objName = prefix + "/" + objName;
        }

        obj = {
            "type": "target",
            "label": objName,
            "output": objName,
            "inputs": [input],
            "tool": tool,
            "config": objcopyConfig,
        };

        entries += [obj];
        objs += [":" + objName];
    }

    if (prefix) {
        params.output ?= params.label;
        params.output = prefix + "/" + params.output;
    }

    return [objs, entries];
}

function
objectifiedBinary (
    params
    )

/*++

Routine Description:

    This routine creates a single object file from a binary file.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns a list of the object file.

--*/

{
    return objectifiedBinaries(params)[1];
}

function
objectifiedLibrary (
    params
    )

/*++

Routine Description:

    This routine creates a library from a set of objectified files.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns a list of the object file.

--*/

{

    var build = params.get("build");
    var compilation = objectifiedBinaries(params);
    var objs = compilation[0];
    var entries = compilation[1];
    var output = params.get("output");

    params.type = "target";
    output ?= params.label;
    params.output = output + ".a";
    params.inputs = objs;
    params.tool = "ar";
    if (build) {
        params.tool = "build_ar";
    }

    entries += [params];
    return entries;
}

//
// Create a flat binary from an executable image.
//

function
flattenedBinary (
    params
    )

/*++

Routine Description:

    This routine creates a flat binary from an executable image.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns a list of the flat binary.

--*/

{

    var entries;

    params.type = "target";
    params.tool = "objcopy";
    addConfig(params, "OBJCOPY_FLAGS", "-O binary");
    if (params.get("binplace")) {
        params.nostrip = true;
        entries = binplace(params);

    } else {
        entries = [params];
    }

    return entries;
}

function
driver (
    params
    )

/*++

Routine Description:

    This routine creates a Minoca kernel driver.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns a list of the driver entry.

--*/

{

    var soname = params.get("output");

    if (!params.get("entry")) {
        params.entry = "DriverEntry";
    }

    if (!(params.get("sources_config") is Dict)) {
        params.sources_config = {};
    }

    if (!(params.sources_config.get("CFLAGS") is List)) {
        params.sources_config.CFLAGS = [];
    }

    params.sources_config.CFLAGS += ["$KERNEL_CFLAGS"];
    params.binplace = "bin";
    soname ?= params.get("label");
    if (soname != "kernel") {
        soname += ".drv";
        params.output = soname;
        params.inputs += ["kernel:kernel"];
    }

    addConfig(params, "LDFLAGS", "-shared");
    addConfig(params, "LDFLAGS", "-Wl,-soname=" + soname);
    addConfig(params, "LDFLAGS", "-nostdlib");
    return executable(params);
}

function
uefiRuntimeFfs (
    name
    )

/*++

Routine Description:

    This routine creates a runtime driver .FFS file from an ELF file.

Arguments:

    params - Supplies the entry with inputs filled out.

Return Value:

    Returns a list of the .FFS entry.

--*/

{

    var elfconvConfig;
    var ffs;
    var pe;

    elfconvConfig = {
        "ELFCONV_FLAGS": "-t efiruntimedriver"
    };

    pe = {
        "type": "target",
        "label": name,
        "inputs": [":" + name + ".elf"],
        "implicit": ["uefi/tools/elfconv:elfconv"],
        "tool": "elfconv",
        "config": elfconvConfig
    };

    ffs = {
        "type": "target",
        "label": name + ".ffs",
        "inputs": [":" + name],
        "implicit": ["uefi/tools/genffs:genffs"],
        "tool": "genffs_runtime"
    };

    return [pe, ffs];
}

function
uefiFwvol (
    path,
    name,
    ffs
    )

/*++

Routine Description:

    This routine creates a UEFI firmware volume object file based on a platform
    name and a list of FFS inputs.

Arguments:

    path - Supplies the path to the source. The object directory version of
        this path will be added to the include path.

    name - Supplies the name of the firmware volume.

    ffs - Supplies the FFS inputs.

Return Value:

    Returns a list of the firmware volume entry.

--*/

{

    var fwv;
    var fwvO;
    var fwvName = name + "fwv";
    var fwvS;

    fwv = {
        "type": "target",
        "label": fwvName,
        "inputs": ffs,
        "implicit": ["uefi/tools/genfv:genfv"],
        "tool": "genfv"
    };

    fwvS = {
        "inputs": [fwvName + ".S"],
        "includes": ["$O/" + path]
    };

    fwvO = compiledSources(fwvS);
    fwvO = fwvO[1][0];
    fwvO["implicit"] = [":" + fwvName];
    return [fwv, fwvO];
}

//
// Define a function that creates a version.h file target.
//

function
createVersionHeader (
    major,
    minor,
    revision
    )

/*++

Routine Description:

    This routine creates a version.h header that includes aspects of the
    build environment.

Arguments:

    major - Supplies the major number.

    minor - Supplies the minor number.

    revision - Supplies the revision.

Return Value:

    Returns a list of the firmware volume entry.

--*/

{

    var versionConfig;
    var versionH;

    versionConfig = {
        "FORM": "header",
        "MAJOR": major,
        "MINOR": minor,
        "REVISION": revision,
        "RELEASE": mconfig.release_level
    };

    versionH = {
        "type": "target",
        "output": "version.h",
        "inputs": ["$S/.git/HEAD"],
        "tool": "gen_version",
        "config": versionConfig
    };

    return [versionH];
}

