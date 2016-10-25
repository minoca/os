/*++

Copyright (c) 2016 Minoca Corp.

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

    Evan Green 14-Apr-2016

Environment:

    Build

--*/

TRUE = 1;
FALSE = 0;

arch ?= getenv("ARCH");
//debug ?= getenv("DEBUG");
variant ?= getenv("VARIANT");

arch ?= "x86";
debug ?= "dbg";
variant ?= "";

release_level ?= "SystemReleaseDevelopment";

outroot = "^/../..";
binroot = outroot + "/bin";
stripped_dir = binroot + "/stripped";

cflags ?= getenv("CFLAGS") ? [getenv("CFLAGS")] : ["-Wall", "-Werror"];
cppflags ?= getenv("CPPFLAGS") ? [getenv("CPPFLAGS")] : [];
ldflags ?= getenv("LDFLAGS") ? [getenv("LDFLAGS")] : [];
asflags ?= getenv("ASFLAGS") ? [getenv("ASLAGS")] : [];

global_config = {
    "ARCH": arch,
    "DEBUG": debug,
    "VARIANT": variant,
    "BUILD_CC": getenv("BUILD_CC"),
    "BUILD_AR": getenv("BUILD_AR"),
    "BUILD_STRIP": getenv("BUILD_STRIP"),
    "CC": getenv("CC"),
    "AR": getenv("AR"),
    "OBJCOPY": getenv("OBJCOPY"),
    "STRIP": getenv("STRIP"),
    "RCC": getenv("RCC"),
    "IASL", getenv("IASL"),
    "SHELL": getenv("SHELL"),
};

build_os ?= uname_s();
build_arch ?= uname_m();

if (build_arch == "i686") {
    build_arch = "x86";

} else if (build_arch == "x86-64") {
    build_arch = "x64";
}

assert(((arch == "x86") || (arch == "armv7") ||
        (arch == "armv6") || (arch == "x64")),
       "Invalid architecture");

assert(((build_arch == "x86") || (build_arch == "armv7") ||
        (build_arch == "armv6") || (build_arch == "x64")),
       "Unknown build architecture");

assert(((debug == "dbg") || (debug == "rel")), "Invalid debug setting");

//
// Set up target architecture-specific globals.
//

bfd_arch = "";
obj_format = "";
if (arch == "x86") {
    bfd_arch = "i386";
    obj_format = "elf32-i386";
    if (variant == "q") {
        tool_prefix = "i586-pc-minoca-";

    } else {
        tool_prefix = "i686-pc-minoca-";
    }

} else if ((arch == "armv7") || (arch == "armv6")) {
    bfd_arch = "arm";
    obj_format = "elf32-littlearm";
    tool_prefix = "arm-none-minoca-";

} else if (arch == "x64") {
    bfd_arch = "x86-64";
    obj_format = "elf64-x86-64";
    tool_prefix = "x86_64-pc-minoca-";
}

//
// Set a default build compiler if none was set.
//

global_config["BUILD_CC"] ?= "gcc";
global_config["BUILD_AR"] ?= "ar";
global_config["BUILD_STRIP"] ?= "strip";
global_config["RCC"] ?= "windres";
global_config["IASL"] ?= "iasl";
global_config["SHELL"] ?= "sh";

echo = "echo";

//
// Pin down CFLAGS and the like.
//

if (debug == "dbg") {
    cflags += ["-O1", "-DDEBUG=1"];

} else {
    cflags += ["-O2", "-Wno-unused-but-set-variable", "-DNDEBUG"];
}

cflags += ["-fno-builtin",
           "-fno-omit-frame-pointer",
           "-g",
           "-save-temps=obj",
           "-ffunction-sections",
           "-fdata-sections",
           "-fvisibility=hidden"];

cppflags += ["-I$//include"];
build_cflags = cflags + [];
build_cppflags = cppflags + [];

cflags += ["-fpic"];
if (build_os == "Windows") {
    build_cflags += ["-mno-ms-bitfields"];

} else {
    build_cflags +=  ["-fpic"];
}

if (arch == "armv6") {
    cppflags += ["-march=armv6zk", "-marm", "-mfpu=vfp"];
}

if (arch == "x86") {
    cflags += ["-mno-ms-bitfields"];
    if (variant == "q") {
        cppflags += ["-Wa,-momit-lock-prefix=yes", "-march=i586"];
    }
}

asflags += ["-Wa,-g"];
ldflags += ["-Wl,--gc-sections"];

objcopy_flags ?= [
    "--rename-section .data=.rodata,alloc,load,data,contents,readonly",
    "-I binary",
    "-O " + obj_format,
    "-B " + bfd_arch
];

//
// Set a default target compiler if one was not set. On Minoca building its own
// architecture, use the native compiler.
//

if ((build_os == "Minoca") && (build_arch == arch)) {
    global_config["CC"] ?= global_config["BUILD_CC"];
    global_config["AR"] ?= global_config["BUILD_AR"];
    global_config["STRIP"] ?= global_config["BUILD_STRIP"];

} else {
    global_config["CC"] ?= tool_prefix + "gcc";
    global_config["AR"] ?= tool_prefix + "ar";
    global_config["OBJCOPY"] ?= tool_prefix + "objcopy";
    global_config["STRIP"] ?= tool_prefix + "strip";
}

global_config["BASE_CFLAGS"] = cflags;
global_config["BASE_CPPFLAGS"] = cppflags;
global_config["BASE_LDFLAGS"] = ldflags;
global_config["BASE_ASFLAGS"] = asflags;
global_config["OBJCOPY_FLAGS"] = objcopy_flags;
global_config["BUILD_BASE_CFLAGS"] = build_cflags;
global_config["BUILD_BASE_CPPFLAGS"] = build_cppflags;
global_config["BUILD_BASE_LDFLAGS"] = ldflags;
global_config["BUILD_BASE_ASFLAGS"] = asflags;
global_config["IASL_FLAGS"] = ["-we"];

//
// Add a config value, creating it if it does not exist.
//

function add_config(entry, name, value) {
    entry["config"] ?= {};
    entry["config"][name] ?= [];
    entry["config"][name] += [value];
    return;
}

//
// Create a phony group target that depends on all the given input entries.
//

function group(name, entries) {
    entry = {
        "label": name,
        "type": "target",
        "tool": "phony",
        "inputs": entries,
        "config": {}
    };

    return [entry];
}

//
// Create a copy target.
//

function copy(source, destination, destination_label, flags, mode) {
    config = {};
    if (flags) {
        config["CPFLAGS"] = flags;
    }

    if (mode) {
        config["CHMOD_FLAGS"] = mode;
    }

    entry = {
        "type": "target",
        "tool": "copy",
        "label": destination_label,
        "inputs": [source],
        "output": destination,
        "config": config
    };

    if (!destination_label) {
        entry["label"] = destination;
    }

    return [entry];
}

//
// Add a stripped version of the target.
//

function strip(params) {
    tool_name = "strip";
    if (get(params, "build")) {
        tool_name = "build_strip";
    }

    params["type"] = "target";
    params["tool"] = tool_name;
    return [params];
}

//
// Replace the current target with a copied version in the bin directory. Also
// strip unless told not to.
//

function binplace(params) {
    label = get(params, "label");
    label ?= get(params, "output");
    source = get(params, "output");
    source ?= label;

    assert(label && source, "Label or output must be defined");

    build = get(params, "build");

    //
    // Set the output since the label is going to be renamed and create the
    // copy target.
    //

    params["output"] = source;
    file_name = basename(source);
    if (build) {
        destination = binroot + "/tools/bin/" + file_name;

    } else {
        destination = binroot + "/" + file_name;
    }

    cpflags = get(params, "cpflags");
    mode = get(params, "chmod");
    new_original_label = label + "_orig";
    original_target = ":" + new_original_label;
    copied_entry = copy(original_target, destination, label, cpflags, mode)[0];

    //
    // The original label was given to the copied destination, so tack a _orig
    // on the source label.
    //

    params["label"] = new_original_label;
    entries = [copied_entry, params];

    //
    // Unless asked not to, create a stripped entry as well.
    //

    if (!get(params, "nostrip")) {
        if (build) {
            stripped_output = stripped_dir + "/build/" + file_name;

        } else {
            stripped_output = stripped_dir + "/" + file_name;
        }

        stripped_entry = {
            "label": label + "_stripped",
            "inputs": [original_target],
            "output": stripped_output,
            "build": get(params, "build"),
        };

        //
        // Make the binplaced copy depend on the stripped version.
        //

        copied_entry["implicit"] = [":" + stripped_entry["label"]];
        entries += strip(stripped_entry);
    }

    return entries;
}

//
// Create a group of object file targets from source file targets. Returns a
// list of the object names in the first element and the object file target
// entries in the second element.
//

function compiled_sources(params) {
    inputs = params["inputs"];
    objs = [];
    entries = [];
    sources_config = get(params, "sources_config");
    prefix = get(params, "prefix");
    build = get(params, "build");
    includes = get(params, "includes");
    if (includes) {
        sources_config ?= {};
        sources_config["CPPFLAGS"] ?= [];
        for (include in includes) {
            sources_config["CPPFLAGS"] += ["-I" + include];
        }
    }

    assert(len(inputs) != 0, "Compilation must have inputs");

    for (input in inputs) {
        input_parts = split_extension(input);
        ext = input_parts[1];
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

        obj_name = input_parts[0] + suffix;
        if (prefix) {
            obj_name = prefix + "/" + obj_name;
        }

        obj = {
            "type": "target",
            "label": obj_name,
            "output": obj_name,
            "inputs": [input],
            "tool": tool,
            "config": sources_config,
        };

        entries += [obj];
        objs += [":" + obj_name];
    }

    if (prefix) {
        params["output"] ?= params["label"];
        params["output"] = prefix + "/" + params["output"];
    }

    return [objs, entries];
}

//
// Vanilla link of a set of sources into some sort of executable or shared
// object.
//

function executable(params) {
    build = get(params, "build");
    compilation = compiled_sources(params);
    objs = compilation[0];
    entries = compilation[1];
    params["type"] = "target";
    params["inputs"] = objs;
    params["tool"] = "ld";
    if (build) {
        params["tool"] = "build_ld";
    }

    //
    // Convert options for text_address, linker_script, and entry to actual
    // LDFLAGS.
    //

    text_address = get(params, "text_address");
    if (text_address) {
        text_option = "-Wl,-Ttext-segment=" + text_address +
                      " -Wl,-Ttext=" + text_address;

        add_config(params, "LDFLAGS", text_option);
    }

    linker_script = get(params, "linker_script");
    if (linker_script) {
        script_option = "-Wl,-T" + linker_script;
        add_config(params, "LDFLAGS", script_option);
    }

    entry = get(params, "entry");
    if (entry) {
        entry_option = "-Wl,-e" + entry + " -Wl,-u" + entry;
        add_config(params, "LDFLAGS", entry_option);
    }

    if (get(params, "binplace")) {
        entries += binplace(params);

    } else {
        entries += [params];
    }

    return entries;
}

//
// Creates a regular position independent application.
//

function application(params) {
    build = get(params, "build");
    exename = get(params, "output");
    exename ?= params["label"];
    if (build && (build_os == "Windows")) {
        params["output"] = exename + ".exe";
    }

    add_config(params, "LDFLAGS", "-pie");
    params["binplace"] ?= TRUE;
    return executable(params);
}

//
// Creates a shared library or DLL.
//

function shared_library(params) {
    build = get(params, "build");
    soname = get(params, "output");
    soname ?= params["label"];
    major_version = get(params, "major_version");
    add_config(params, "LDFLAGS", "-shared");
    if ((!build) || (build_os != "Windows")) {
        soname += ".so";
        if (major_version != null) {
            soname += "." + major_version;
        }

        add_config(params, "LDFLAGS", "-Wl,-soname=" + soname);

    } else {
        soname += ".dll";
    }

    params["output"] = soname;
    params["binplace"] ?= TRUE;
    return executable(params);
}

//
// Creates a static archive.
//

function static_library(params) {
    compilation = compiled_sources(params);
    objs = compilation[0];
    entries = compilation[1];
    params["type"] = "target";
    output = get(params, "output");
    output ?= params["label"];
    params["output"] = output + ".a";
    params["inputs"] = objs;
    params["tool"] = "ar";
    if (build) {
        params["tool"] = "build_ar";
    }

    if (get(params, "binplace")) {
        entries += binplace(params);

    } else {
        entries += [params];
    }

    return entries;
}

//
// Create a list of compiled .aml files from a list of asl files. Returns a
// list where the first element is a list of all the resulting target names,
// and the second element is a list of the target entries.
//

function compiled_asl(inputs) {
    entries = [];
    objs = [];

    assert(len(inputs) != 0, "Compilation must have inputs");

    for (input in inputs) {
        input_parts = split_extension(input);
        ext = input_parts[1];
        suffix = ".aml";
        tool = "iasl";
        obj_name = input_parts[0] + suffix;
        obj = {
            "type": "target",
            "label": obj_name,
            "output": obj_name,
            "inputs": [input],
            "tool": tool
        };

        entries += [obj];
        objs += [":" + obj_name];
    }

    return [objs, entries];
}

//
// Create a group of object file targets from binary files. Returns a
// list of the object names in the first element and the object file target
// entries in the second element.
//

function objectified_binaries(params) {
    inputs = params["inputs"];
    objs = [];
    entries = [];
    objcopy_config = get(params, "config");
    prefix = get(params, "prefix");
    build = get(params, "build");

    assert(len(inputs) != 0, "Compilation must have inputs");

    for (input in inputs) {
        input_parts = split_extension(input);
        ext = input_parts[1];
        suffix = ".o";
        tool = "objcopy";
        if (build) {
            tool = "build_" + tool;
        }

        obj_name = input_parts[0] + suffix;
        if (prefix) {
            obj_name = prefix + "/" + obj_name;
        }

        obj = {
            "type": "target",
            "label": obj_name,
            "output": obj_name,
            "inputs": [input],
            "tool": tool,
            "config": objcopy_config,
        };

        entries += [obj];
        objs += [":" + obj_name];
    }

    if (prefix) {
        params["output"] ?= params["label"];
        params["output"] = prefix + "/" + params["output"];
    }

    return [objs, entries];
}

//
// Create a single object file from a binary file.
//

function objectified_binary(params) {
    return objectified_binaries(params)[1];
}

//
// Create a library from a set of objectified files.
//

function objectified_library(params) {
    build = get(params, "build");
    compilation = objectified_binaries(params);
    objs = compilation[0];
    entries = compilation[1];
    params["type"] = "target";
    output = get(params, "output");
    output ?= params["label"];
    params["output"] = output + ".a";
    params["inputs"] = objs;
    params["tool"] = "ar";
    if (build) {
        params["tool"] = "build_ar";
    }

    entries += [params];
    return entries;
}

//
// Create a flat binary from an executable image.
//

function flattened_binary(params) {
    params["type"] = "target";
    params["tool"] = "objcopy";
    flags = "OBJCOPY_FLAGS";
    add_config(params, flags, "-O binary");
    if (get(params, "binplace")) {
        params["nostrip"] = TRUE;
        entries = binplace(params);

    } else {
        entries = [params];
    }

    return entries;
}

//
// Create a Minoca kernel driver.
//

function driver(params) {
    params["entry"] ?= "DriverEntry";
    params["binplace"] ?= TRUE;
    soname = get(params, "output");
    soname ?= params["label"];
    if (soname != "kernel") {
        soname += ".drv";
        params["output"] = soname;
        params["inputs"] += ["//kernel:kernel"];
    }

    add_config(params, "LDFLAGS", "-shared");
    add_config(params, "LDFLAGS", "-Wl,-soname=" + soname);
    add_config(params, "LDFLAGS", "-nostdlib");
    return executable(params);
}

//
// Define a function for creating a runtime driver .FFS file from an ELF.
//

function uefi_runtime_ffs(name) {
    elfconv_config = {
        "ELFCONV_FLAGS": "-t efiruntimedriver"
    };

    pe = {
        "type": "target",
        "label": name,
        "inputs": [":" + name + ".elf"],
        "implicit": ["//uefi/tools/elfconv:elfconv"],
        "tool": "elfconv",
        "config": elfconv_config
    };

    ffs = {
        "type": "target",
        "label": name + ".ffs",
        "inputs": [":" + name],
        "implicit": ["//uefi/tools/genffs:genffs"],
        "tool": "genffs_runtime"
    };

    return [pe, ffs];
}

//
// Define a function that creates a UEFI firmware volume object file based on
// a platform name and a list of FFS inputs.
//

function uefi_fwvol_o(name, ffs) {
    fwv_name = name + "fwv";
    fwv = {
        "type": "target",
        "label": fwv_name,
        "inputs": ffs,
        "implicit": ["//uefi/tools/genfv:genfv"],
        "tool": "genfv"
    };

    fwv_o = compiled_sources({"inputs": [fwv_name + ".S"]);
    return [fwv] + fwv_o;
}

//
// Define a function that creates a version.h file target.
//

function create_version_header(major, minor, revision) {
    version_config = {
        "FORM": "header",
        "MAJOR": major,
        "MINOR": minor,
        "REVISION": revision,
        "RELEASE": release_level
    };

    version_h = {
        "type": "target",
        "output": "version.h",
        "inputs": ["//.git/HEAD"],
        "tool": "gen_version",
        "config": version_config + {"FORM": "header"}
    };

    return [version_h];
}

