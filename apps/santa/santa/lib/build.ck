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
from santa.build import shell;
from santa.config import config;
from santa.file import isdir, mkdir, path, rmtree;
from santa.lib.config import ConfigFile;
import santa.lib.defaultbuild;
from santa.lib.pkgdepot import PackageDepot;
from santa.lib.pkgman import PackageManager, PackageNotFoundError;
from santa.lib.realmmanager import RealmManager;

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
        "contained": false,
    },

    "fetch": {
        "default": defaultbuild.defaultFetch,
        "next": "unpack",
        "contained": false,
    },

    "unpack": {
        "default": defaultbuild.defaultUnpack,
        "next": "prepare",
        "contained": false,
    },

    "prepare": {
        "default": defaultbuild.defaultPrepare,
        "next": "configure",
        "contained": true,
    },

    "configure": {
        "default": defaultbuild.defaultConfigure,
        "next": "build",
        "contained": true,
    },

    "build": {
        "default": defaultbuild.defaultBuild,
        "next": "check",
        "contained": true,
    },

    "check": {
        "default": defaultbuild.defaultCheck,
        "next": "package",
        "contained": true,
    },

    "package": {
        "default": defaultbuild.defaultPackage,
        "next": "clean",
        "contained": false,
    },

    "clean": {
        "default": defaultbuild.defaultClean,
        "next": "complete",
        "contained": false,
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
    var _packageDepot;
    var _realm;
    var _subpackageDirectories;

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
            this.filePath = pathOrNumber;
            this._loadModule(pathOrNumber);
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
            this.ignoreMissingDeps = false;
            this._getRealm(this.number, true);
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

        var child = null;
        var definition = buildSteps[step];
        var next;
        var returnValue;
        var stepFunction;
        var verbose;

        if (step == "complete") {
            this.complete = true;
            return;

        }

        //
        // Enter the containment environment if desired. Do it from a child
        // process so the outer environment can still be accessed at the end.
        // Windows does not support fork, but also does not support proper
        // containment.
        //

        if (definition.contained != false) {
            try {
                child = (os.fork)();

            } except os.OsError as e {
                if (e.errno != os.ENOSYS) {
                    Core.raise(e);
                }
            }

        }

        if ((child == null) || (child == 0)) {
            if (definition.contained != false) {
                _realm.containment.enter(null);
                this._translatePaths();
            }

            this._setupEnvironment();
            if (step == "package") {
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

            //
            // Return back to the parent process now that the step is complete.
            //

            if (child == 0) {
                (os.exit)(0);
            }

        //
        // In the parent, just wait for the child to exit. In the case fork
        // is not supported, this doesn't run.
        //

        } else {
            child = (os.waitpid)(child, 0)[1];
            if (child != 0) {
                if (verbose) {
                    Core.print("Child worker exited with: %d" % child);
                }

                (os.exit)(child);
            }
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
        var depends;
        var env = this.env;
        var envPath;
        var finalPath;
        var flags;
        var flagsDict;
        var pathSeparator = ":";
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
                if (flag[0] == "!") {
                    flagsDict[flag[1...-1]] = false;

                } else {
                    flagsDict[flag] = true;
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
        for (key in ["depends", "makedepends_build", "makedepends_host",
                     "basedepends_build", "basedepends_host"]) {

            depends = vars.get(key);
            if (depends == null) {
                depends = [];

            } else if (depends is String) {
                depends = depends.split(null, -1);

            } else if (!(depends is List)) {
                Core.raise(PackageConfigError("Invalid %s type" % key));
            }

            vars[key] = depends;
        }

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
        }

        vars.srcdir = finalPath;
        env.srcdir = finalPath;
        finalPath = path(vars.pkgdir);
        if (!(os.isdir)(finalPath)) {
            if (verbose) {
                Core.print("Creating pkgdir: %s" % finalPath);
            }

            mkdir(vars.pkgdir);
        }

        vars.pkgdir = finalPath;
        env.pkgdir = finalPath;
        finalPath = path(vars.builddir);
        if (!(os.isdir)(finalPath)) {
            if (verbose) {
                Core.print("Creating builddir: %s" % finalPath);
            }

            mkdir(vars.builddir);
        }

        vars.builddir = finalPath;
        env.builddir = finalPath;
        vars.buildsysroot = path(vars.buildsysroot);
        env.buildsysroot = vars.buildsysroot;

        //
        // Add the build sysroot to the path.
        //

        try {
            envPath = (os.getenv)("PATH");
            if (envPath.contains(";")) {
                pathSeparator = ";";
            }

            envPath = "%s/bin%s%s/usr/bin%s%s" %
                      [vars.buildsysroot, pathSeparator,
                       vars.buildsysroot, pathSeparator,
                       envPath];

            env.PATH = envPath;

        } except KeyError {}

        vars.sysroot = path(vars.sysroot);
        env.sysroot = vars.sysroot;
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

        //
        // Don't bother saving a build that's already been cleaned up.
        //

        if (this.complete) {
            return;
        }

        if (_config == null) {
            configPath = _realm.containment.outerPath("status.json");
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

        this._getRealm(number, false);
        configPath = _realm.containment.outerPath("status.json");
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
        _startDirectory = (os.getcwd)();
        this.filePath = _startDirectory + "/" + basename;
        Core.setModulePath([_startDirectory] + modulePath);
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

        var deps;
        var missing = [];
        var packages = PackageManager(_realm);
        var params = {};
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

        //
        // Install base packages.
        //

        deps = vars.get("basedepends_build");
        if (deps != null) {
            for (dep in deps) {
                try {
                    packages.install(dep, null);

                } except PackageNotFoundError as e {
                    if (vars.verbose) {
                        Core.print("Ignoring missing base build "
                                   "dependency for %s: %s" %
                                   [dep, e.args.__str()]);
                    }
                }
            }
        }

        deps = vars.get("basedepends_host");
        if (deps != null) {
            if (((vars.arch != os.machine) &&
                 (vars.arch != "none")) || (vars.os != os.system)) {

                params.arch = vars.arch;
                params.os = vars.os;
                if (!vars.sysroot.startsWith(vars.buildsysroot)) {
                    Core.print("Sysroot should be inside buildsysroot");
                }

                params.root = vars.sysroot[vars.buildsysroot.length()...-1];
            }

            for (dep in deps) {
                try {
                    packages.install(dep, params);

                } except PackageNotFoundError as e {
                    if (vars.verbose) {
                        Core.print("Ignoring missing base host "
                                   "dependency for %s: %s" %
                                   [dep, e.args.__str()]);
                    }
                }
            }

            params = {};
        }

        //
        // Install required build dependencies.
        //

        deps = vars.get("makedepends_build");
        if (deps != null) {
            for (dep in deps) {
                try {
                    packages.install(dep, null);

                } except PackageNotFoundError as e {
                    if (!this.ignoreMissingDeps) {
                        Core.raise(e);

                    } else {
                        Core.print("Ignoring missing build dependency: %s" %
                                   dep);
                    }
                }
            }
        }

        deps = vars.get("makedepends_host");
        if (deps != null) {
            if (((vars.arch != os.machine) &&
                 (vars.arch != "none")) || (vars.os != os.system)) {

                params.arch = vars.arch;
                params.os = vars.os;
                params.root = vars.sysroot[vars.buildsysroot.length()...-1];
            }

            for (dep in deps) {
                try {
                    packages.install(dep, params);

                } except PackageNotFoundError as e {
                    if (!this.ignoreMissingDeps) {
                        Core.raise(e);

                    } else {
                        Core.print("Ignoring missing host dependency: %s" %
                                   dep);
                    }
                }
            }
        }

        packages.commit();
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

        this._createPackage(this.vars, path(this.vars.pkgdir));
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

        var buildRoot = _realm.containment.outerPath("santa");
        var realms = RealmManager();
        var vars = this.vars;

        buildRoot = path("%s/%d" % [buildRoot, this.number]);
        if (vars.verbose) {
            Core.print("Cleaning %s" % buildRoot);
        }

        (os.chdir)(vars.startdir);
        rmtree(buildRoot);

        //
        // Destroy the realm used to build this package.
        //

        realms.destroyRealm("build%d" % this.number);
        _realm = null;
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

        var buildRoot = _realm.containment.outerPath("/");
        var vars = this.vars;

        vars.startdir = _startDirectory;
        vars.srcdir = buildRoot + "/src";
        vars.pkgdir = buildRoot + "/pkg";
        vars.builddir = buildRoot + "/bld";
        vars.subpkgdir = null;
        vars.basedepends_host = "base-dev-host";
        vars.buildsysroot = buildRoot;
        vars.sysroot = vars.buildsysroot;
        if ((vars.os != vars.buildos) || (vars.arch != vars.buildarch)) {
            vars.sysroot = "%s/%s-%s" % [buildRoot, vars.arch, vars.os];
            vars.basedepends_build = "base-dev-%s-%s" %
                                      [vars.arch.lower(), vars.os.lower()];

        } else {
            vars.basedepends_build = "base-dev";
        }

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
            // Create a copy of the variables that can be changed by
            // the subpackage.
            //

            this.vars = vars.copy();

            //
            // Concoct a modifier portion from the subpackage name to get the
            // name of the function to call.
            //

            modifier = subpackage.rsplit("-", -1);
            if ((modifier.length() == 2) && (modifier[0] == vars.name)) {
                modifier = modifier[1];
                this.vars.name = "%s-%s" % [vars.name, modifier];

            } else if ((subpackage.startsWith(vars.name)) &&
                       (subpackage.length() > vars.name.length())) {

                modifier = subpackage[(vars.name.length() + 1)...-1];
                this.vars.name = "%s-%s" % [vars.name, modifier];

            } else {
                modifier = subpackage;
                this.vars.name = subpackage;
            }

            for (character in ["-", "+"]) {
                modifier = modifier.replace(character, "_", -1);
            }

            if (modifier.length() == 0) {
                Core.raise(ValueError("Couldn't extract subpackage portion "
                                      "from '%s', package '%s'" %
                                      [subpackage, vars.name]));
            }

            pkgdir = "%s/subpkg-%s" % [vars.pkgdir, modifier];
            mkdir(pkgdir);
            _subpackageDirectories.append(pkgdir);
            this.vars.subpkgdir = pkgdir;
            this.env.subpkgdir = pkgdir;
            (os.setenv)("subpkgdir", pkgdir);

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
            shell("ls -laR " + path(dataDir));
        }

        _packageDepot.createPackage(vars, dataDir, false);
        if (verbose) {
            Core.print("Completed assembling package %s" % vars.name);
        }

        return;
    }

    function
    _getRealm (
        number,
        create
        )

    /*++

    Routine Description:

        This routine creates a package from the current variables.

    Arguments:

        number - Supplies the build number to get the realm for.

        create - Supplies a boolean indicating if the realm should be created
            if it does not exist.

    Return Value:

        None. On failure, an exception is raised.

    --*/

    {

        var realmName = "build%d" % number;
        var realms = RealmManager();

        try {
            _realm = realms.getRealm(realmName);

        } except KeyError {
            if (create) {
                _realm = realms.createRealm(realmName, null);

            } else {
                Core.raise(KeyError("Build %d does not exist" % number));
            }
        }

        return;
    }

    function
    _translatePaths (
        )

    /*++

    Routine Description:

        This routine translates paths that were anchored outside the container
        into the container, now that the containment environment has been
        entered. This routine assumes the environment has not yet been set up.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var env = this.env;
        var value;
        var vars = this.vars;

        for (key in ["srcdir", "builddir", "pkgdir", "sysroot",
                     "buildsysroot"]) {

            value = _realm.containment.innerPath(vars[key]);
            vars[key] = value;
            env[key] = value;
        }

        return;
    }
}

//
// --------------------------------------------------------- Internal Functions
//

