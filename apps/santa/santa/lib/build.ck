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
import santa.lib.defaultbuild;

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

class Build {
    var _startDirectory;
    var _module;
    var _subpackageDirectories;

    function
    __init (
        buildFilePath
        )

    /*++

    Routine Description:

        This routine initializes a new build from a path to a build file.

    Arguments:

        buildFilePath - Supplies the path to the build file.

    Return Value:

        Returns the instance, initialized.

    --*/

    {

        this._loadModule(buildFilePath);
        this.filePath = buildFilePath;
        this.module = _module;
        this.step = "init";
        this.vars = {
            "os": os.system,
            "buildos": os.system,
            "targetos": os.system,
            "buildarch": os.machine,
            "targetarch": os.machine,
            "verbose": config.getKey("core.verbose"),
            "debug": false,
        };

        _subpackageDirectories = [];
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

        if (step == "complete") {
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
            if (this.vars.verbose) {
                if (stepFunction == definition.default) {
                    Core.print("Executing default step %s" % step);

                } else {
                    Core.print("Executing step %s" % step);
                }
            }

            stepFunction(this);
        }

        //
        // Process any results.
        //

        if (step == "init") {
            this._postInitStep();

        } else if (step == "clean") {
            this._postCleanupStep();
        }

        next = definition.next;

        //
        // Advance the step.
        //

        if (next) {
            this.step = next;
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

        var finalPath;
        var vars = this.vars;
        var verbose = vars.verbose;
        var value;

        this._initVars();

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
        }

        finalPath = path(vars.pkgdir);
        if (!(os.isdir)(finalPath)) {
            if (verbose) {
                Core.print("Creating pkgdir: %s" % finalPath);
            }

            mkdir(vars.pkgdir);
            vars.pkgdir = finalPath;
        }

        finalPath = path(vars.builddir);
        if (!(os.isdir)(finalPath)) {
            if (verbose) {
                Core.print("Creating builddir: %s" % finalPath);
            }

            mkdir(vars.builddir);
            vars.builddir = finalPath;
        }

        //
        // Set the variables in the environment as well.
        //

        for (key in vars) {
            value = vars[key];
            if (value is List) {
                value = " ".join(value);

            } else {
                value = value.__str();
            }

            (os.setenv)(key, value);
        }

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
                                  "end in .ck." % buildFilePath));
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
        var pid = (os.getpid)();
        var vars = this.vars;

        buildRoot = path("%s/%d" % [buildRoot, pid]);
        if (vars.verbose) {
            Core.print("Removing %s" % buildRoot);
        }

        rmtree(buildRoot);
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
        var pid = (os.getpid)();
        var vars = this.vars;

        if (!buildRoot) {
            Core.raise(ValueError("core.builddir not set"));
        }

        vars.startdir = _startDirectory;
        vars.srcdir = "%s/%d/src" % [buildRoot, pid];
        vars.pkgdir = "%s/%d/pkg" % [buildRoot, pid];
        vars.builddir = "%s/%d/bld" % [buildRoot, pid];
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

                } except NameError {}

                Core.raise(e);
            }

            //
            // Call out to the subpackage function.
            //

            subpackageFunction(this);

            //
            // Remove the subpackage directory.
            //

            rmtree(pkgdir);
            _subpackageDirectories.remove(_subpackageDirectories.length() - 1);
        }

        return;
    }
}

//
// --------------------------------------------------------- Internal Functions
//

