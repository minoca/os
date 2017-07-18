/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    build.ck

Abstract:

    This module supports building packages for Santa.

Author:

    Evan Green 30-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import os;
from santa.config import config;
from santa.file import mkdir, path, rmtree;
from santa.lib.config import ConfigFile;
import santa.lib.defaultbuild;
from santa.lib.pkgdepot import PackageDepot;

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the build steps.
//

var buildSteps = {
    "init": {
        "default": defaultbuild.defaultInit,
        "next": "fetch",
    },

    "fetch": {
        "default": defaultbuild.defaultFetch,
        "next": "unpack",
    },

    "unpack": {
        "default": defaultbuild.defaultUnpack,
        "next": "prepare",
    },

    "prepare": {
        "default": defaultbuild.defaultPrepare,
        "next": "configure",
    },

    "configure": {
        "default": defaultbuild.defaultConfigure,
        "next": "build",
    },

    "build": {
        "default": defaultbuild.defaultBuild,
        "next": "check",
    },

    "check": {
        "default": defaultbuild.defaultCheck,
        "next": "package",
    },

    "package": {
        "default": defaultbuild.defaultPackage,
        "next": "clean",
    },

    "clean": {
        "default": defaultbuild.defaultClean,
        "next": "complete",
    },
};

//
// Define the required variables.
//

var requiredVars = [
    "arch",
    "description",
    "license",
    "name",
    "url",
    "version",
];

//
// ------------------------------------------------------------------ Functions
//

class PackageConfigError is Exception {}

class Build {
    var _config;
    var _startDirectory;
    var _module;
    var _subpackageDirectories;
    var _packageDepot;

    function
    __init (
        pathOrNumber
        )

    /*++

    Routine Description:

        This routine initializes a new build from a path to a build file.

    Arguments:

        pathOrNumber - Supplies either a path to the build file, or a build
            number to resume.

    Return Value:

        Returns the instance, initialized.

    --*/

    {

        if (pathOrNumber is Int) {
            this._load(pathOrNumber);

        } else {
            this._loadModule(pathOrNumber);
            this.filePath = pathOrNumber;
            this.number = (os.getpid)();
            this.module = _module;
            this.outdir = null;
            this.step = "init";
            this.vars = {
                "os": os.system,
                "buildos": os.system,
                "buildarch": os.machine,
                "arch": os.machine,
                "verbose": config.getKey("core.verbose"),
                "debug": false,
            };

            this.env = this.vars.copy();
            _subpackageDirectories = [];
        }

        this.complete = false;
        return this;
    }

    function
    run (
        )

    /*++

    Routine Description:

        This routine executes a build.

    Arguments:

        None.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        var current;
        var previous;

        previous = this.step;
        while (this.step != "complete") {
            current = this.step;
            if (!current) {
                Core.raise(ValueError("Invalid step"));
            }

            this.runStep(current);
            previous = current;
            current = this.step;
            if (previous == current) {
                Core.raise(ValueError("Build step did not advance"));
            }
        }

        this.complete = true;
        return;
    }

    function
    runStep (
        step
        )

    /*++

    Routine Description:

        This routine executes a step of the build, and advances the step.

    Arguments:

        step - Supplies the step to execute.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        var definition = buildSteps[step];
        var next;
        var returnValue;
        var stepFunction;
        var verbose;

        this._setupEnvironment();
        if (step == "complete") {
            this.complete = true;
            return;

        } else if (step == "package") {
            this._packageSubmodules();
        }

        //
        // Try to get the function associated with this step.
        //

        try {
            stepFunction = this.module.__get(step);

        } except NameError {
            stepFunction = definition.default;
        }

        if (stepFunction != null) {
            verbose = this.vars.verbose;
            if (verbose) {
                if (stepFunction == definition.default) {
                    Core.print("Executing default step %s" % step);

                } else {
                    Core.print("Executing step %s" % step);
                }
            }

            stepFunction(this);
            if (verbose) {
                Core.print("Completed step: %s" % step);
            }
        }

        //
        // Process any results.
        //

        if (step == "init") {
            this._postInitStep();

        } else if (step == "package") {
            this._postPackageStep();

        } else if (step == "clean") {
            this._postCleanupStep();
        }

        next = definition.next;

        //
        // Advance the step. Save progress.
        //

        if (next) {
            this.step = next;

            //
            // Unless the build is completely cleaned up and deleted, save all
            // progress.
            //

            if (next == "complete") {
                this.complete = true;

            } else {
                this.save();
            }
        }

        return;
    }

    function
    setVariables (
        newVariables
        )

    /*++

    Routine Description:

        This routine sets the package parameters. This function should be
        called once during the init step.

    Arguments:

        newVariables - Supplies the new variables to set. These variables can
            contain $substitutions to other pre-existing variables, including
            keys in this array that have no substitutions. Nested expansion is
            not supported.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        var arch;
        var conffiles;
        var env = this.env;
        var finalPath;
        var flags;
        var flagsDict;
        var supportedOses;
        var targetOs;
        var vars = this.vars;
        var verbose = vars.verbose;
        var value;

        this._initVars();
        arch = vars.arch;
        targetOs = vars.os;

        //
        // Iterate once and add everything without any substitutions.
        //

        for (key in newVariables) {
            if (!key.contains("$")) {
                vars[key] = newVariables[key];
            }
        }

        //
        // Now iterate again and add everything that does have a substitution.
        //

        for (key in newVariables) {
            if (newVariables[key].contains("$")) {
                vars[key] = newVariables[key].template(vars, false);
            }
        }

        if (vars.get("subpackages") == null) {
            vars.subpackages = "$name-dev $name-doc".template(vars, false);
        }

        if (vars.get("section") == null) {
            vars.section = "bin";
        }

        if (vars.get("release") == null) {
            vars.release = 0;
        }

        //
        // Parse out the flags into a dictionary.
        //

        if (vars.get("flags") == null) {
            vars.flags = {};

        } else if (vars.flags is String) {
            flags = vars.flags.split(null, -1);
            flagsDict = {};
            for (flag in flags) {
                if (flags[0] == "!") {
                    flagsDict[flags[1...-1]] = false;

                } else {
                    flagsDict[flags] = true;
                }
            }

            vars.flags = flagsDict;
        }

        conffiles = vars.get("conffiles");
        if (conffiles == null) {
            conffiles = [];

        } else if (conffiles is String) {
            conffiles = conffiles.split(null, -1);
        }

        vars.conffiles = conffiles;

        //
        // Reconcile the architecture. The selected architecture must be
        // supported.
        //

        if (vars.arch is String) {

            //
            // If building for all, build for the current one.
            //

            if (vars.arch == "all") {
                vars.arch = arch;

            } else if (vars.arch != "none") {
                vars.arch = vars.arch.split(null, -1);
            }
        }

        if (vars.arch is List) {
            if (!vars.arch.contains(arch)) {
                Core.raise(PackageConfigError(
                    "Package %s cannot be built for architecture %s" %
                    [vars.name, arch]));
            }

            vars.arch = arch;
        }

        //
        // Reconcile the selected OS. If not supplied, assume all OSes work.
        //

        if (vars.get("os") == null) {
            vars.os = "all";
        }

        if (vars.os is String) {
            if (vars.os != "none") {
                vars.os = vars.os.split(null, -1);
            }
        }

        if (vars.os is List) {
            if (!vars.os.contains(targetOs)) {
                Core.raise(PackageConfigError(
                    "Package %s cannot be built for OS %s" %
                    [vars.name, targetOs]));
            }

            vars.os = targetOs;
        }

        //
        // Complain if the package cannot be cross-compiled.
        //

        if ((vars.os != vars.buildos) || (vars.arch != vars.buildarch)) {
            if (vars.flags.get("cross") == false) {
                Core.raise(PackageConfigError(
                    "Package %s cannot be cross compiled: "
                    "Build is %s-%s, target is %s-%s" %
                    [vars.buildarch, vars.buildos, vars.arch, vars.os]));
            }
        }

        //
        // Create the build directories. The Santa version of mkdir calls path
        // internally, so be careful not to double-apply path().
        //

        finalPath = path(vars.srcdir);
        if (!(os.isdir)(finalPath)) {
            if (verbose) {
                Core.print("Creating srcdir: %s" % finalPath);
            }

            mkdir(vars.srcdir);
            vars.srcdir = finalPath;
            env.srcdir = finalPath;
        }

        finalPath = path(vars.pkgdir);
        if (!(os.isdir)(finalPath)) {
            if (verbose) {
                Core.print("Creating pkgdir: %s" % finalPath);
            }

            mkdir(vars.pkgdir);
            vars.pkgdir = finalPath;
            env.pkgdir = finalPath;
        }

        finalPath = path(vars.builddir);
        if (!(os.isdir)(finalPath)) {
            if (verbose) {
                Core.print("Creating builddir: %s" % finalPath);
            }

            mkdir(vars.builddir);
            vars.builddir = finalPath;
            env.builddir = finalPath;
        }

        return;
    }

    function
    save (
        )

    /*++

    Routine Description:

        This routine saves the current state of the build to permanent storage.

    Arguments:

        None.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        var configPath;
        var buildRoot;

        //
        // Don't bother saving a build that's already been cleaned up.
        //

        if (this.complete) {
            return;
        }

        if (_config == null) {
            buildRoot = config.getKey("core.builddir");
            configPath = path("%s/%d/status.json" % [buildRoot, this.number]);
            _config = ConfigFile(configPath, {});
        }

        _config.clear();
        _config.startDirectory = _startDirectory;
        _config.subpackageDirectories = _subpackageDirectories;
        _config.vars = this.vars;
        _config.env = this.env;
        _config.filePath = this.filePath;
        _config.outdir = this.outdir;
        _config.step = this.step;
        _config.number = this.number;
        _config.save();
        return;
    }

    function
    _load (
        number
        )

    /*++

    Routine Description:

        This routine loads a build from a previously saved configuration file.

    Arguments:

        None.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        var configPath;
        var buildRoot = config.getKey("core.builddir");

        configPath = path("%s/%d/status.json" % [buildRoot, number]);
        if (!(os.exists)(configPath)) {
            Core.raise(ValueError("Build %d does not exist" % number));
        }

        _config = ConfigFile(configPath, {});
        if (_config.number != number) {
            Core.raise(ValueError("Error: build number requested was %d, "
                                  "but loaded %d" % [number, this.number]));
        }

        this.number = number;
        _startDirectory = _config.startDirectory;
        _subpackageDirectories = _config.subpackageDirectories;
        this.vars = _config.vars;
        this.env = _config.env;
        this.filePath = _config.filePath;
        this.outdir = _config.outdir;
        this.step = _config.step;
        this._loadModule(this.filePath);
        this.module = _module;
        return;
    }

    function
    _loadModule (
        buildFilePath
        )

    /*++

    Routine Description:

        This routine loads the module associated with the build file at the
        given path.

    Arguments:

        buildFilePath - Supplies the path to the build file.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        var dirname = (os.dirname)(buildFilePath);
        var basename = (os.basename)(buildFilePath);
        var modulePath = Core.modulePath();
        var oldDirectory = (os.getcwd)();

        if (!basename.endsWith(".ck")) {
            Core.raise(ValueError("Expected build file path '%s' to "
                                  "end in .ck" % buildFilePath));
        }

        //
        // Change to the directory where the file resides, and import the
        // module from there.
        //

        (os.chdir)(dirname);
        Core.setModulePath(modulePath + [dirname]);
        _startDirectory = (os.getcwd)();
        _module = Core.importModule(basename[0..-3]);
        _module.run();

        //
        // Restore the original module path and working directory.
        //

        Core.setModulePath(modulePath);
        (os.chdir)(oldDirectory);
        return;
    }

    function
    _postInitStep (
        )

    /*++

    Routine Description:

        This routine is called after the module's init function finishes. It
        validates that the required parameters have been supplied.

    Arguments:

        None.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        var missing = [];
        var value;
        var vars = this.vars;

        //
        // Make sure the required variables are present.
        //

        for (name in requiredVars) {
            value = vars.get(name);
            if ((value == null) || (value == "")) {
                missing.append(value);
            }
        }

        if (missing.length() != 0) {
            Core.raise(ValueError("Required parameter%s '%s' missing" %
                                  [missing.length() == 1 ? "" : "s",
                                   ", ".join(missing)]));
        }

        return;
    }

    function
    _postPackageStep (
        )

    /*++

    Routine Description:

        This routine is called after the module's package function finishes. It
        creates the main package and updates the index.

    Arguments:

        None.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        this._createPackage(this.vars, this.vars.pkgdir);
        _packageDepot.rebuildIndex();
        return;
    }

    function
    _postCleanupStep (
        )

    /*++

    Routine Description:

        This routine is called after the module's cleanup function finishes. It
        removes the build root directory created for the build.

    Arguments:

        None.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        var buildRoot = config.getKey("core.builddir");
        var vars = this.vars;

        buildRoot = path("%s/%d" % [buildRoot, this.number]);
        if (vars.verbose) {
            Core.print("Cleaning %s" % buildRoot);
        }

        (os.chdir)(vars.startdir);
        rmtree(buildRoot);
        _config = null;
        return;
    }

    function
    _initVars (
        )

    /*++

    Routine Description:

        This routine initializes the variables provided by Santa for a build.

    Arguments:

        None.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        var buildRoot = config.getKey("core.builddir");
        var number = this.number;
        var vars = this.vars;

        if (!buildRoot) {
            Core.raise(ValueError("core.builddir not set"));
        }

        vars.startdir = _startDirectory;
        vars.srcdir = "%s/%d/src" % [buildRoot, number];
        vars.pkgdir = "%s/%d/pkg" % [buildRoot, number];
        vars.builddir = "%s/%d/bld" % [buildRoot, number];
        vars.subpkgdir = null;
        return;
    }

    function
    _packageSubmodules (
        )

    /*++

    Routine Description:

        This routine packages any submodules for the package. This runs before
        the main package function is called.

    Arguments:

        None.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        var buildRoot = config.getKey("core.builddir");
        var functionName;
        var functionResult;
        var modifier;
        var pkgdir;
        var subpackageFunction;
        var subpackages = this.vars.subpackages;
        var vars = this.vars;

        if (!(subpackages is List)) {
            subpackages = subpackages.split(null, -1);
        }

        //
        // Loop through all the listed submodules.
        //

        for (subpackage in subpackages) {

            //
            // Concoct a modifier portion from the subpackage name to get the
            // name of the function to call.
            //

            modifier = subpackage.rsplit("-", 1);
            if (modifier.length() == 2) {
                modifier = modifier[1];

            } else if ((subpackage.startsWith(vars.name)) &&
                       (subpackage.length() > vars.name.length())) {

                modifier = subpackage[(vars.name.length())...-1];

            } else {
                modifier = subpackage;
            }

            if (modifier.length() == 0) {
                Core.raise(ValueError("Couldn't extract subpackage portion "
                                      "from '%s', package '%s'" %
                                      [subpackage, vars.name]));
            }

            pkgdir = "%s/subpkg-%s" % [vars.pkgdir, modifier];
            mkdir(pkgdir);
            _subpackageDirectories.append(pkgdir);
            vars.subpkgdir = pkgdir;
            this.env.subpkgdir = pkgdir;
            (os.setenv)("subpkgdir", pkgdir);

            //
            // Create a copy of the variables that can be changed by
            // the subpackage.
            //

            this.vars = vars.copy();
            this.vars.name = "%s-%s" % [vars.name, modifier];

            //
            // Get the package function associated with the submodule.
            //

            functionName = "package" + modifier;
            try {
                subpackageFunction = _module.__get(functionName);

            } except NameError as e {
                try {
                    functionName = "defaultPackage" + modifier;
                    subpackageFunction = defaultbuild.__get(functionName);

                } except NameError {
                    Core.raise(e);
                }
            }

            //
            // Call out to the subpackage function.
            //

            functionResult = subpackageFunction(this);

            //
            // Create the package if the subpackage function returned non-zero.
            //

            if (functionResult) {
                this._createPackage(this.vars, pkgdir);
            }

            //
            // Remove the subpackage directory.
            //

            this.env.subpkgdir = null;
            (os.setenv)("subpkgdir", null);
            (os.chdir)(vars.startdir);
            rmtree(pkgdir);
            _subpackageDirectories.removeAt(-1);
        }

        //
        // Restore the original variable.
        //

        this.vars = vars;
        return;
    }

    function
    _setupEnvironment (
        )

    /*++

    Routine Description:

        This routine sets all the environment variables within this.env.

    Arguments:

        None.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        var env = this.env;
        var value;

        for (key in env) {
            value = env[key];
            if (value is List) {
                value = " ".join(value);

            } else {
                value = value.__str();
            }

            (os.setenv)(key, env[key]);
        }

        return;
    }

    function
    _createPackage (
        vars,
        dataDir
        )

    /*++

    Routine Description:

        This routine creates a package from the current variables.

    Arguments:

        vars - Supplies the current set of variables to use.

        dataDir - Supplies the pacakge contents directory.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        var outdir = this.outdir;
        var verbose = this.vars.verbose;

        if (_packageDepot == null) {
            _packageDepot = PackageDepot(this.outdir);
        }

        //
        // Create the package. Don't bother rebuilding the index until the end.
        //

        if (verbose) {
            Core.print("Assembling package: %s" % vars.name);
        }

        _packageDepot.createPackage(vars, dataDir, false);
        if (verbose) {
            Core.print("Completed assembling package %s" % vars.name);
        }

        return;
    }
}

//
// --------------------------------------------------------- Internal Functions
//

