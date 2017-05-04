/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mingen.ck

Abstract:

    This module implements the Minoca build generator application, which can
    transform a build specification into a Makefile or Ninja file.

Author:

    Evan Green 30-Jan-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from app import argv;
from make import buildMakefile;
from ninja import buildNinja;
from getopt import gnuGetopt;
import os;
from os import getcwd;

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

var VERSION_MAJOR = 2;
var VERSION_MINOR = 0;

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

function
_loadProjectRoot (
    );

function
_processEntries (
    );

function
_selectTargets (
    );

function
_loadModule (
    name
    );

function
_validateEntries (
    moduleName,
    entries
    );

function
_validateTargetEntry (
    moduleName,
    entry
    );

function
_validateToolEntry (
    moduleName,
    entry
    );

function
_validatePoolEntry (
    moduleName,
    entry
    );

function
_processTarget (
    target
    );

function
_createInputsList (
    target,
    inputs
    );

function
_processInput (
    target,
    input
    );

function
_raiseModuleError (
    moduleName,
    entry,
    description
    );

function
_canonicalizeLabel (
    moduleName,
    label
    );

function
_isPathAbsolute (
    path
    );

function
_findAllTargets (
    module
    );

function
_markTargetActive (
    target
    );

function
_printAllEntries (
    );

function
_printTool (
    tool
    );

function
_printPool (
    pool
    );

function
_printTarget (
    target
    );

//
// -------------------------------------------------------------------- Globals
//

var shortOptions = "B:De:F:ghi:no:O:uvV";
var longOptions = [
    "build-os=",
    "expr=",
    "debug",
    "format=",
    "no-generator",
    "input=",
    "dry-run",
    "output=",
    "output-file=",
    "help",
    "unanchored",
    "verbose",
    "version"
];

var usage =
    "usage: mingen [options] [targets...]\n"
    "The Minoca Build Generator creates Ninja files describing the build at \n"
    "the current directory. If specific targets are specified, then a build \n"
    "file for only those targets will be built. Otherwise, the build file \n"
    "is created for the whole project. Options are:\n"
    "  -B, --build-os=os,machine -- Set the build OS and build machine.\n"
    "  -e, --expr=var=val -- Set a custom build option.\n"
    "      This can be specified multiple times.\n"
    "  -D, --debug -- Print lots of information during execution.\n"
    "  -f, --format=fmt -- Specify the output format as make or ninja. The \n"
    "      default is make.\n"
    "  -g, --no-generator -- Don't include a re-generate rule in the output.\n"
    "  -n, --dry-run -- Do all the processing, but do not actually create \n"
    "      any output files.\n"
    "  -i, --input=dir -- Sets the top level directory of the source. The \n"
    "      default is to use the current working directory.\n"
    "  -o, --output=build_dir -- Set the given directory as the build \n"
    "      output directory.\n"
    "  -O, --output-file=file -- Set the output file name.\n"
    "  -u, --unanchored -- Leave the input and output directories blank in \n"
    "      the final build file. They must be specified manually later.\n"
    "  -v, --verbose -- Print more information during processing.\n"
    "  --help -- Show this help text and exit.\n"
    "  --version -- Print the application version information and exit.\n\n";

var config = {
    "build_os": os.system,
    "build_machine": os.machine,
    "debug": false,
    "format": null,
    "generator": true,
    "input": null,
    "output": null,
    "output_file": null,
    "unanchored": false,
    "verbose": false,
    "build_module_name": "build",
    "default_target": ":",
    "targets": [],
    "input_variable": "S",
    "output_variable": "O",
    "vars": {},
    "cmdvars": {}
};

var modules = {};
var targets = {};
var targetsList = [];
var tools = {};
var pools = {};
var buildDirectories = {};
var scripts = {};

//
// ------------------------------------------------------------------ Functions
//

function
main (
    )

/*++

Routine Description:

    This routine implements the application entry point for the mingen
    application.

Arguments:

    None.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    var appOptions = gnuGetopt(argv[1...-1], shortOptions, longOptions);
    var args = appOptions[1];
    var currentDirectory = getcwd().replace("\\", "/", -1);
    var entries = {};
    var name;
    var value;

    //
    // The return from getopt is [config, remainingArgs]. Get the config now.
    //

    appOptions = appOptions[0];
    for (option in appOptions) {
        name = option[0];
        value = option[1];
        if ((name == "-B") || (name == "--build-os")) {
            value = value.split(",", 1);
            config.build_os = value[0];
            if (value.length() == 2) {
                config.build_machine = value[1];
            }

        } else if ((name == "-e") || (name == "--expr")) {
            value = value.split("=", 1);
            if (value.length() == 2) {
                config.cmdvars[value[0]] = value[1];

            } else {
                Core.raise(ValueError("Invalid expression '%s'" % value));
            }

        } else if ((name == "-D") || (name == "--debug")) {
            config.debug = true;
            config.verbose = true;

        } else if ((name == "f") || (name == "--format")) {
            if (!config.format) {
                config.format = value;
                if (!["make", "ninja", "none"].contains(value)) {
                    Core.raise(ValueError("Invalid format '%s'" % value));
                }
            }

        } else if ((name == "-g") || (name == "--no-generator")) {
            config.generator = false;

        } else if ((name == "-n") || (name == "--dry-run")) {
            config.format = "none";

        } else if ((name == "-i") || (name == "--input")) {
            if (!config.input) {
                config.input = value;
            }

        } else if ((name == "-o") || (name == "--output")) {
            if (!config.output) {
                config.output = value;
            }

        } else if ((name == "-O") || (name == "--output-file")) {
            config.output_file = value;

        } else if ((name == "-u") || (name == "--unanchored")) {
            config.unanchored = true;

        } else if ((name == "-v") || (name == "--verbose")) {
            config.verbose = true;

        } else if ((name == "-h") || (name == "--help")) {
            Core.print(usage);
            return 1;

        } else if ((name == "-V") || (name == "--version")) {
            Core.print("mingen version %d.%d" % [VERSION_MAJOR, VERSION_MINOR]);
            return 1;

        } else {
            Core.raise(ValueError("Invalid option '%s'" % name));
        }

    }

    config.targets = args;
    config.argv = argv;
    if (!config.input) {
        config.input = currentDirectory;
    }

    Core.setModulePath([config.input] + Core.modulePath());
    if (config.debug) {
        Core.print("Module search path: " + Core.modulePath().__str());
    }

    _loadProjectRoot();
    config.format ?= "make";
    _processEntries();
    _selectTargets();
    if (config.verbose) {
        _printAllEntries();
    }

    entries.targets = targets;
    entries.targetsList = targetsList;
    entries.tools = tools;
    entries.pools = pools;
    entries.buildDirectories = buildDirectories;
    entries.scripts = scripts;
    if (config.format == "make") {
        buildMakefile(config, entries);

    } else if (config.format == "ninja") {
        buildNinja(config, entries);
    }

    if (config.verbose) {
        Core.print("Done");
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

function
_loadProjectRoot (
    )

/*++

Routine Description:

    This routine loads the project root script.

Arguments:

    None.

Return Value:

    None.

--*/

{

    var build;
    var entries;
    var module = Core.importModule(config.build_module_name);

    module.run();
    modules[""] = module;
    scripts[config.build_module_name + ".ck"] = true;
    if (config.debug) {
        Core.print("Initial Config:");
        for (key in config) {
            Core.print("  %s: %s" % [key, config[key].__str()]);
        }

        Core.print("");
    }

    build = module.build;
    entries = build();
    _validateEntries("", entries);
    if (!config.output) {
        config.output = config.input;
    }

    return;
}

function
_processEntries (
    )

/*++

Routine Description:

    This routine processes all target entries, adding in dependencies as needed.

Arguments:

    None.

Return Value:

    None.

--*/

{

    var index;
    var target;

    //
    // Iterate carefully as the list of targets may be growing as it's being
    // processed. Entries only get added to the end.
    //

    for (index = 0; index < targetsList.length(); index += 1) {
        target = targetsList[index];
        _processTarget(target);
    }

    return;
}

function
_selectTargets (
    )

/*++

Routine Description:

    This routine marks the requested targets, tools, and pool active, as well
    as their dependencies.

Arguments:

    None.

Return Value:

    None.

--*/

{

    var first = true;
    var label;
    var moduleTargets;
    var target;

    for (entry in config.targets) {
        label = entry.rsplit(":", 1);
        if (label.length() == 0) {
            Core.raise(ValueError("Invalid target '%s' requested" % entry));
        }

        if ((label.length() == 2) && (label[1] == "")) {
            moduleTargets = _findAllTargets(label[0]);
            for (target in moduleTargets) {
                _markTargetActive(target);
            }

        } else {
            target = targets.get(entry);
            if (!target) {
                Core.raise(ValueError("Unknown target '%s' requested" % entry));
            }

            if (first) {
                target.default = true;
                first = false;
            }

            _markTargetActive(target);
        }
    }

    return;
}

function
_loadModule (
    name
    )

/*++

Routine Description:

    This routine loads the given build file. If it has not yet been run, it
    runs it and adds the entries to the global list.

Arguments:

    name - Supplies the name of the module to load, not including the build.ck
        portion.

Return Value:

    None.

--*/

{

    var build;
    var entries;
    var fullName;
    var module;
    var scriptFile;

    module = modules.get(name);
    if (module) {
        return module;
    }

    fullName = name.replace("/", ".", -1) + "." + config.build_module_name;
    module = Core.importModule(fullName);
    module.run();
    scriptFile = fullName.replace(".", "/", -1) + ".ck";
    scripts[scriptFile] = true;
    modules[name] = module;
    build = module.build;
    entries = build();
    _validateEntries(name, entries);
    return module;
}

function
_validateEntries (
    moduleName,
    entries
    )

/*++

Routine Description:

    This routine processes the entries returned from loading a new module.

Arguments:

    moduleName - Supplies the name of the module these entries are associated
        with.

    entries - Supplies the new build entries.

Return Value:

    None.

--*/

{

    var entryType;

    if (!(entries is List)) {
        Core.raise(TypeError("In %s: Return a list from build(), not a %s" %
                             [moduleName,
                              entries.type().name()]));
    }

    for (entry in entries) {
        if (!(entry is Dict)) {
            Core.raise(TypeError("In %s: Expected a dict, got a %s: %s" %
                                 [moduleName,
                                  entry.type().name(),
                                  entry.__str()]));
        }

        try {
            entryType = entry.type;

        } except KeyError {
            Core.print("In entry:");
            for (key in entry) {
                Core.print("  %s: %s" % [key, entry[key].__str()]);
            }

            Core.raise(ValueError("In %s: Dict must have a 'type' member" %
                                  moduleName));
        }

        if (entryType == "target") {
            _validateTargetEntry(moduleName, entry);

        } else if (entryType == "tool") {
            _validateToolEntry(moduleName, entry);

        } else if (entryType == "pool") {
            _validatePoolEntry(moduleName, entry);

        } else if (entryType != "ignore") {
            _raiseModuleError(moduleName,
                              entry,
                              "Invalid entry type '%s'" % entryType);
        }
    }

    if (config.verbose) {
        Core.print("Processed module " + moduleName);
    }

    return;
}

function
_validateTargetEntry (
    moduleName,
    entry
    )

/*++

Routine Description:

    This routine validates a new target entry.

Arguments:

    moduleName - Supplies the name of the module this entry is associated with.

    entry - Supplies the new target entry.

Return Value:

    None.

--*/

{

    var label;

    //
    // Set the label or output to each other if both are not specified. At
    // least one of those two must be specified.
    //

    if (!entry.get("label")) {
        if (!entry.get("output")) {
            _raiseModuleError(moduleName,
                              entry,
                              "Either label or output must be specified");
        }

        entry.label = entry.output;

    } else if (!entry.get("output")) {
        entry.output = entry.label;
    }

    if (!entry.get("tool")) {
        _raiseModuleError(moduleName, entry, "'tool' must be specified");
    }

    //
    // Canonicalize the label and output members so they're fully specified.
    //

    entry.module = moduleName;
    entry.label = _canonicalizeLabel(moduleName, entry.label);
    if (!_isPathAbsolute(entry.output)) {
        if (moduleName != "") {
            entry.output = moduleName + "/" + entry.output;
        }

        if (entry.tool != "phony") {
            entry.output = ("$%s/" % config.output_variable) + entry.output;
        }
    }

    //
    // This target had better be unique.
    //

    if (targets.get(entry.label)) {
        _raiseModuleError(moduleName, entry, "Target label must be unique");
    }

    //
    // If no specific targets are requested, then all targets are active.
    //

    entry.active = false;
    if (config.targets.length() == 0) {
        entry.active = true;
    }

    if (!entry.get("config")) {
        entry.config = {};
    }

    if (!entry.get("inputs")) {
        entry.inputs = [];
    }

    if (!entry.get("implicit")) {
        entry.implicit = [];
    }

    if (!entry.get("orderonly")) {
        entry.orderonly = [];
    }

    targets[entry.label] = entry;
    targetsList.append(entry);
    return;
}

function
_validateToolEntry (
    moduleName,
    entry
    )

/*++

Routine Description:

    This routine validates a new tool entry.

Arguments:

    moduleName - Supplies the name of the module this entry is associated with.

    entry - Supplies the new tool entry.

Return Value:

    None.

--*/

{

    if ((!entry.get("name")) || (!entry.get("command"))) {
        _raiseModuleError(moduleName,
                          entry,
                          "'name' and 'command' are required for tools");
    }

    if (tools.get(entry.name)) {
        _raiseModuleError(moduleName, entry, "Tool name must be unique");
    }

    //
    // If no specific targets are requested, then all tools are active.
    //

    entry.active = false;
    if (config.targets.length() == 0) {
        entry.active = true;
    }

    tools[entry.name] = entry;
    return;
}

function
_validatePoolEntry (
    moduleName,
    entry
    )

/*++

Routine Description:

    This routine validates a new pool entry.

Arguments:

    moduleName - Supplies the name of the module this entry is associated with.

    entry - Supplies the new pool entry.

Return Value:

    None.

--*/

{

    if ((!entry.get("name")) || (!entry.get("depth"))) {
        _raiseModuleError(moduleName,
                          entry,
                          "'name' and 'depth' are required for pools");
    }

    if (pools.get(entry.name)) {
        _raiseModuleError(moduleName, entry, "Pool name must be unique");
    }

    //
    // If no specific targets are requested, then all tools are active.
    //

    entry.active = false;
    if (config.targets.length() == 0) {
        entry.active = true;
    }

    pools[entry.name] = entry;
    return;
}

function
_processTarget (
    target
    )

/*++

Routine Description:

    This routine processes a target entry.

Arguments:

    target - Supplies the target to process.

Return Value:

    None.

--*/

{

    var directory;

    if (config.debug) {
        Core.print("Processing %s" % target.label);
    }

    //
    // Add the target as a build directory if no selective targets are
    // requested.
    //

    if (config.targets.length() == 0) {
        if ((!target.get("tool")) || (target.tool != "phony")) {
            directory = target.output.rsplit("/", 1)[0];
            buildDirectories[directory] = true;
        }
    }

    //
    // If this is the default entry, set it as such.
    //

    if (target.get("default")) {
        config.default_target = target.label;
    }

    //
    // Convert the inputs to an array of either sources or other targets.
    //

    target.inputs = _createInputsList(target, target.inputs);
    target.implicit = _createInputsList(target, target.implicit);
    target.orderonly = _createInputsList(target, target.orderonly);
    return;
}

function
_createInputsList (
    target,
    inputs
    )

/*++

Routine Description:

    This routine creates a list that is a combination of sources and other
    targets.

Arguments:

    target - Supplies the target being processed.

    inputs - Supplies the raw list of input strings.

Return Value:

    Returns the resulting list, where references to other targets will be
    replaced with those targets.

--*/

{

    var element;
    var result = [];

    for (input in inputs) {
        element = _processInput(target, input);
        if (element is List) {
            result += element;

        } else {
            result.append(element);
        }
    }

    return result;
}

function
_processInput (
    target,
    input
    )

/*++

Routine Description:

    This routine processes an input for a target.

Arguments:

    target - Supplies the target being processed.

    input - Supplies the input string, which is either a direct source file or
        a reference to another target by label

Return Value:

    Returns the input string if it is a raw source.

    Returns a target or a list of targets if the given input refers to another
    output.

--*/

{

    var inputTarget;
    var label;
    var module;

    //
    // If there's no colon, it's a direct file path.
    //

    if (input.indexOf(":") < 0) {
        if (_isPathAbsolute(input)) {
            return input;
        }

        return ("$%s/" % config.input_variable) + target.module + "/" + input;
    }

    input = _canonicalizeLabel(target.module, input);
    inputTarget = targets.get(input);
    if (!inputTarget) {
        module = input.rsplit(":", 1);
        label = module[1];
        module = module[0];
        try {
            _loadModule(module);

        } except ImportError as e {
            _raiseModuleError(target.module,
                              target,
                              "Failed to import '%s'" % module);
        }

        if (label == "") {
            inputTarget = _findAllTargets(module);

        } else {
            inputTarget = targets.get(input);
            if (!inputTarget) {
                _raiseModuleError(target.module,
                                  target,
                                  "Failed to find input '%s'" % input);
            }
        }
    }

    return inputTarget;
}

function
_raiseModuleError (
    moduleName,
    entry,
    description
    )

/*++

Routine Description:

    This routine raises an error with one of the module values.

Arguments:

    moduleName - Supplies the name of the module this entry is associated with.

    entry - Supplies the new target entry.

Return Value:

    None.

--*/

{

    var name = entry.get("label");

    if (moduleName == "") {
        moduleName = "<root>";
    }

    if (!name) {
        name = entry.get("output");
        if (!name) {
            name = "<unknown>";
        }
    }

    description = "%s: %s: %s" % [moduleName, name, description];
    Core.raise(ValueError(description));
    return;
}

function
_canonicalizeLabel (
    moduleName,
    label
    )

/*++

Routine Description:

    This routine canonicalizes a label into its full form: <path>:<label>.

Arguments:

    moduleName - Supplies the name of the module the label is defined in.

    label - Supplies the initial label string.

Return Value:

    Returns the canonical label form.

--*/

{

    if (label.indexOf(":") < 0) {
        label = ":" + label;
    }

    if (label[0] == ":") {
        return moduleName + label;
    }

    return label;
}

function
_isPathAbsolute (
    path
    )

/*++

Routine Description:

    This routine determines if the given path is fully specified or relative to
    a directory.

Arguments:

    path - Supplies the path to test.

Return Value:

    Returns true if the path is absolute.

    Returns false if the path is relative.

--*/

{

    if ((path.startsWith("/")) ||
        (path.startsWith("$%s" % config.input_variable)) ||
        (path.startsWith("${%s}" % config.input_variable)) ||
        (path.startsWith("$%s" % config.output_variable)) ||
        (path.startsWith("${%s}" % config.output_variable))) {

        return true;
    }

    return false;
}

function
_findAllTargets (
    module
    )

/*++

Routine Description:

    This routine finds all targets in the given module.

Arguments:

    module - Supplies the module name.

Return Value:

    Returns a list of all labels in the given module.

--*/

{

    var result = [];

    module = module + ":";
    for (key in targets) {
        if (key.startsWith(module)) {
            result.append(targets[key]);
        }
    }

    return result;
}

function
_markTargetActive (
    target
    )

/*++

Routine Description:

    This routine marks the given target and all of its dependencies as active.

Arguments:

    target - Supplies the target being marked active.

Return Value:

    None.

--*/

{

    var directory;
    var inputs;
    var pool;
    var tool;

    if (target.active) {
        return;
    }

    target.active = true;
    tool = target.get("tool");
    if ((tool) && (tool != "phony")) {
        tools[tool].active = true;
    }

    pool = target.get("pool");
    if (pool) {
        pools[pool].active = true;
    }

    //
    // Add the target to the list of build directories.
    //

    if ((!target.get("tool")) || (target.tool != "phony")) {
        directory = target.output.rsplit("/", 1)[0];
        buildDirectories[directory] = true;
    }

    //
    // Recursively mark all the input targets as active as well.
    //

    inputs = target.inputs;
    for (input in inputs) {
        if (input is Dict) {
            _markTargetActive(input);
        }
    }

    inputs = target.implicit;
    for (input in inputs) {
        if (input is Dict) {
            _markTargetActive(input);
        }
    }

    inputs = target.orderonly;
    for (input in inputs) {
        if (input is Dict) {
            _markTargetActive(input);
        }
    }

    return;
}

function
_printAllEntries (
    )

/*++

Routine Description:

    This routine prints out all targets, tools, and pools for verbose mode.

Arguments:

    None.

Return Value:

    None.

--*/

{

    for (key in tools) {
        _printTool(tools[key]);
    }

    for (key in pools) {
        _printPool(pools[key]);
    }

    for (key in targets) {
        _printTarget(targets[key]);
    }

    Core.print("Final Config:");
    for (key in config) {
        Core.print("  %s: %s" % [key, config[key].__str()]);
    }

    Core.print("");
    return;
}

function
_printTool (
    tool
    )

/*++

Routine Description:

    This routine prints out the given tool.

Arguments:

    tool - Supplies the tool to print.

Return Value:

    None.

--*/

{

    var depFile = tool.get("depfile");
    var depsFormat = tool.get("depsformat");
    var description = tool.get("description");

    description ?= "";
    Core.print("Tool '%s': '%s'" % [tool.name, description]);
    Core.print("\t%s" % tool.command);
    if (depFile || depsFormat) {
        Core.print("\tDepfile: %s DepsFormat: %s" % [depFile, depsFormat]);
    }

    Core.print("");
    return;
}

function
_printPool (
    pool
    )

/*++

Routine Description:

    This routine prints out the given pool.

Arguments:

    pool - Supplies the pool to print.

Return Value:

    None.

--*/

{

    Core.print("Pool %s Depth: %d" % [pool.name, pool.depth]);
    return;
}

function
_printTarget (
    target
    )

/*++

Routine Description:

    This routine prints out the given target.

Arguments:

    target - Supplies the target to print.

Return Value:

    None.

--*/

{

    var config;
    var inputs;
    var label;

    label = target.label.rsplit(":", 1)[1];
    Core.print("%s (%s): %s" % [target.output, label, target.tool]);
    inputs = target.inputs;
    for (input in inputs) {
        if (input is Dict) {
            input = input.label;
        }

        Core.print("\t" + input);
    }

    inputs = target.implicit;
    for (input in inputs) {
        if (input is Dict) {
            input = input.label;
        }

        Core.print("\t| " + input);
    }

    inputs = target.orderonly;
    for (input in inputs) {
        if (input is Dict) {
            input = input.label;
        }

        Core.print("\t|| " + input);
    }

    config = target.config;
    if (config.length()) {
        for (key in config) {
            Core.print("\t\t%s: %s" % [key, config[key]]);
        }
    }

    Core.print("");
    return;
}

