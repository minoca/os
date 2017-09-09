/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pkgman.ck

Abstract:

    This module implements the PackageManager class, which manages a set of
    package installed within a realm.

Author:

    Evan Green 14-Jul-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import io;
import os;
from santa.config import config;
from santa.file import path, mkdir;
from santa.lib.config import ConfigFile;
from santa.lib.pkg import Package;
from santa.lib.pkgdb import PackageDatabase;
from santa.lib.realmmanager import RealmManager;
from santa.lib.santaconfig import SANTA_PACKAGE_STATE_PATH, SANTA_STORAGE_PATH;

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

var defaultPackageManagerState = {
    "pkgs": []
};

//
// ------------------------------------------------------------------ Functions
//

class PackageDependencyError is Exception {}
class PackageNotFoundError is Exception {}
class MultiplePackagesError is Exception {}
class PackageVersionConflict is Exception {}

class PackageManager {
    var _realm;
    var _state;
    var _addPlan;
    var _removePlan;
    var _nextReferenceCount;

    function
    __init (
        realm
        )

    /*++

    Routine Description:

        This routine instantiates a new Package Manager.

    Arguments:

        realm - Supplies an optional realm to manage packages in. Supply null
            to use the root realm.

    Return Value:

        Returns the initialized instance.

    --*/

    {

        var statePath;

        if (realm == null) {
            realm = RealmManager().getRootRealm();
        }

        _realm = realm;
        statePath = path(SANTA_PACKAGE_STATE_PATH);
        if (realm.name != "root") {
            statePath = realm.containment.outerPath(statePath);
        }

        mkdir((os.dirname)(statePath));
        _state = ConfigFile(statePath, defaultPackageManagerState);
        this.db = PackageDatabase();
        _addPlan = [];
        _removePlan = [];
        _nextReferenceCount = {};
        return this;
    }

    function
    install (
        name,
        parameters
        )

    /*++

    Routine Description:

        This routine installs a package.

    Arguments:

        name - Supplies either the name of the package to install or the path
            to the package to install.

        parameters - Supplies additional parameters about the package to
            install.

    Return Value:

        None. An exception is raised on error.

    --*/

    {

        var dependencyParameters = {"trail": ""};
        var filter;
        var package;
        var packages;

        if (parameters == null) {
            parameters = {};
        }

        name = this._splitVersionRequirements(name, parameters);

        //
        // Set a root if there isn't one so it's always identified as a filter
        // parameter.
        //

        if (parameters.get("root") == null) {
            parameters.root = "";
        }

        //
        // Set the arch and OS if not set as well so a native package is
        // always installed.
        //

        if (parameters.get("arch") == null) {
            parameters.arch = os.machine;
        }

        if (parameters.get("os") == null) {
            parameters.os = os.system;
        }

        //
        // If there's a period, it must be a package file path.
        //

        if (name.indexOf(".") >= 0) {
            package = Package(name);
            package = package.info;
            try {
                package = this._getInstalledPackage(package.name, package);
                Core.raise(ValueError("Package %s is already installed" %
                           package.name));

            } except PackageNotFoundError {}

        } else {
            package = this.getPackage(name, parameters);

            //
            // If the package is already installed, don't add it to the plan.
            //

            if (package.get("referenceCount") != null) {
                if (config.getKey("core.verbose")) {
                    Core.print("Package %s already installed" % name);
                }

                return;
            }
        }

        try {
            package.userCount += 1;

        } except KeyError {
            package.userCount = 1;
            package.referenceCount = 0;
        }

        for (key in ["arch", "os", "root"]) {
            if (parameters.get(key) != null) {
                dependencyParameters[key] = parameters[key];
            }
        }

        this._addPackage(package, dependencyParameters);
        return;
    }

    function
    uninstall (
        name,
        parameters
        )

    /*++

    Routine Description:

        This routine uninstalls a package.

    Arguments:

        name - Supplies either the name of the package to uninstall.

        parameters - Supplies additional parameters about the package to
            uninstall.

    Return Value:

        None. An exception is raised on error.

    --*/

    {

        var dependencyParameters = {"trail": ""};
        var filter;
        var package;
        var packages;
        var verbose = config.getKey("core.verbose");

        //
        // Set a root, arch, and OS if they're not set so a native build gets
        // removed.
        //

        if (parameters.get("root") == null) {
            parameters.root = "";
        }

        if (parameters.get("arch") == null) {
            parameters.arch = os.machine;
        }

        if (parameters.get("os") == null) {
            parameters.os = os.system;
        }

        package = this._getInstalledPackage(name, parameters);
        try {
            package.root = parameters.root;

        } except KeyError {}

        if (package.get("userCount") == null) {
            package.userCount = 0;
        }

        if (package.userCount > 0) {
            package.userCount -= 1;

        } else {
            if (verbose) {
                Core.print("%s: Only installed as a dependency: "
                           "%d references" %
                           [package.name, package.referenceCount]);
            }

            return;
        }


        if (package.userCount != 0) {
            if (verbose) {
                Core.print("%s: Still %d user references on the package "
                           "(%d dependency references as well)" %
                           [package.name,
                            package.userCount,
                            package.referenceCount]);
            }

            return;
        }

        //
        // Add an extra reference to account for the one subtracted by the
        // remove function.
        //

        package.referenceCount += 1;
        for (key in ["arch", "os", "root"]) {
            if (parameters.get(key) != null) {
                dependencyParameters[key] = parameters[key];
            }
        }

        this._removePackage(package, dependencyParameters);
        return;
    }

    function
    getPackage (
        name,
        parameters
        )

    /*++

    Routine Description:

        This routine returns a package dictionary matching the given parameters.

    Arguments:

        name - Supplies either the name of the package to get.

        parameters - Supplies optional additional parameters about the package
            to install.

    Return Value:

        Returns a package dictionary, or raises an exception if the package
        was not found or included multiple results.

    --*/

    {

        var package;

        try {
            package = this._getInstalledPackage(name, parameters);

        } except PackageNotFoundError {
            package = this._getAvailablePackage(name, parameters);
        }

        return package;
    }

    function
    commit (
        )

    /*++

    Routine Description:

        This routine commits the current plan to action, executing the
        specified removals and then additions.

    Arguments:

        name - Supplies either the name of the package to get.

        parameters - Supplies optional additional parameters about the package
            to install.

    Return Value:

        Returns a package dictionary, or raises an exception if the package
        was not found or included multiple results.

    --*/

    {

        var controlDirectory;
        var end = _removePlan.length() - 1;
        var installedPackages = _state.pkgs;
        var package;
        var packageInfo;
        var packagePath;
        var root;

        //
        // Process removals backwards.
        //

        if (end >= 0) {
            for (index in end...0) {
                packageInfo = _removePlan[index];
                controlDirectory = this._packagePath(packageInfo);
                package = this._openInstalledPackage(packageInfo);
                package.uninstall(_realm, packageInfo.get("root"));
                for (pkgIndex in 0..installedPackages.length()) {
                    if (installedPackages[pkgIndex] == packageInfo) {
                        installedPackages.removeAt(pkgIndex);
                        break;
                    }
                }

                //
                // Consider an option to keep extracted packages around. There
                // would need to be something at the other end that reconciled
                // the control directory with a package file.
                //

                package.remove();
            }
        }

        //
        // Process addition forwards.
        //

        end = _addPlan.length();
        for (index in 0..end) {
            packageInfo = _addPlan[index];
            controlDirectory = this._packagePath(packageInfo);
            packagePath = "/".join([packageInfo.repository, packageInfo.file]);
            mkdir(controlDirectory);
            packagePath = path(packagePath);
            package = Package.fromArchive(controlDirectory, packagePath);
            root = packageInfo.get("root");
            if (root) {
                mkdir(root);
            }

            package.install(_realm, root);
            package.save();
            installedPackages.append(packageInfo);
        }

        //
        // Set the reference counts where they should be.
        //

        for (key in _nextReferenceCount) {
            key.referenceCount = _nextReferenceCount[key];
        }

        _nextReferenceCount = {};

        //
        // Save the config, which makes it official.
        //

        this._save();
        return;
    }

    function
    _getInstalledPackage (
        name,
        parameters
        )

    /*++

    Routine Description:

        This routine returns an installed package dictionary matching the given
        parameters.

    Arguments:

        name - Supplies either the name of the package to get.

        parameters - Supplies optional additional parameters about the package
            to install.

    Return Value:

        Returns a package dictionary, or raises an exception if the package
        was not found or included multiple results.

    --*/

    {

        var error;
        var filter = {};
        var found;
        var package = null;
        var results = [];
        var root = parameters.get("root");
        var trail = "";

        for (parameter in ["arch", "os", "release", "version", "root"]) {
            try {
                filter[parameter] = parameters[parameter];
            } except KeyError {}
        }

        //
        // Look for the package in the list of installed packages.
        //

        filter.name = name;
        for (pkg in _state.pkgs) {
            found = true;
            for (key in filter) {
                if (key == "version") {
                    continue;
                }

                if ((key == "arch") || (key == "os")) {
                    if (pkg[key] == "none") {
                        continue;
                    }
                }

                if (pkg[key] != filter[key]) {
                    found = false;
                    break;
                }
            }

            if (!found) {
                continue;
            }

            try {
                if (pkg.root != root) {
                    continue;
                }

            } except KeyError {}

            //
            // Fail if there's already an installed package with the
            // wrong version. At some point consider adding support for
            // upgrading installed packages if that would satisfy the
            // constraints.
            //

            if (!this.db.checkVersionConstraints(pkg, parameters)) {
                if (parameters.get("trail")) {
                    trail = " needed by %s" % parameters.trail;
                }

                error = "Package %s %s is already installed and does not "
                        "satisfy constraints %s%s. "
                        "Please uninstall incompatible package and try "
                        "again" % [pkg.name, pkg.version, parameters.version,
                                   trail];

                Core.raise(PackageVersionConflict(error));
            }

            results.append(pkg);
        }

        if (results.length() == 0) {
            Core.raise(PackageNotFoundError(
                      "Package %s with required parameters not found" %
                      name));

        } else if (results.length() != 1) {
            Core.raise(MultiplePackagesError(
                "Multiple packages match package %s with required "
                "parameters" % name));
        }

        package = results[0];
        return package;
    }

    function
    _getAvailablePackage (
        name,
        parameters
        )

    /*++

    Routine Description:

        This routine returns a package dictionary from the package database.

    Arguments:

        name - Supplies either the name of the package to get.

        parameters - Supplies optional additional parameters about the package
            to install.

    Return Value:

        Returns a Package instance.

    --*/

    {

        var filter;
        var package;
        var packagePath;
        var packages;

        filter = {"name": name};
        for (parameter in ["arch", "os", "release", "version"]) {
            try {
                filter[parameter] = parameters[parameter];

            } except KeyError {}
        }

        if (filter.length()) {
            packages = this.db.filter(filter, false);
            if (packages.length() > 1) {
                if (filter.get("version") == null) {
                    package = this._latest(packages);

                } else {
                    Core.raise(MultiplePackagesError(
                        "Multiple packages match package %s with required "
                        "parameters" % name));
                }

            } else if (packages.length() == 1) {
                package = packages[0];
            }

        } else {
            package = this.db.getLatest(name);
        }

        //
        // If no package was found, re-raise the not found exception.
        //

        if (package == null) {
            Core.raise(PackageNotFoundError("Package %s not found" % name));
        }

        return package;
    }

    function
    _save (
        )

    /*++

    Routine Description:

        This routine saves the package manager state.

    Arguments:

        None.

    Return Value:

        None. An exception is raised on error.

    --*/

    {

        _state.save();
        return;
    }

    function
    _addPackage (
        package,
        parameters
        )

    /*++

    Routine Description:

        This routine adds the given package to the plan of packages to add.
        This routine is recursive.

    Arguments:

        package - Supplies the package information dictionary.

        parameters - Supplies additional parameters for the dependencies.

    Return Value:

        None. The package is added to the action plan.

    --*/

    {

        var added = false;
        var conflict;
        var depends;
        var existing;
        var referenceCount;
        var trail;
        var verbose = config.getKey("core.verbose");
        var versionCompare;

        if (_removePlan.contains(package)) {
            Core.raise(PackageDependencyError(
                "Cannot add %s: needed by %s: Package is slated for removal" %
                [package.name, parameters.trail]));
        }

        //
        // See if the package is already in the plan to be added.
        //

        if (_addPlan.contains(package)) {
            if (verbose) {
                Core.print("Package %s already going to be added" %
                           package.name);
            }

            return;
        }

        //
        // See if there's already a different version of the package in the
        // add plan.
        //

        for (index in 0.._addPlan.length()) {
            existing = _addPlan[index];
            if (existing.name != package.name) {
                continue;
            }

            versionCompare = this.db.compareVersionStrings(existing.version,
                                                           package.version);

            if (versionCompare > 0) {
                Core.raise(PackageVersionConflict(
                    "Package %s version %s is needed by %s, but newer version "
                    "%s is already in the add plan. Please make sure this "
                    "trail of packages is added before whichever package "
                    "included the newer version" %
                    [package.name,
                     package.version,
                     parameters.trail,
                     existing.version]));

            } else if (versionCompare == 0) {
                if (package.release > existing.release) {
                    _addPlan[index] = package;
                    added = true;
                    break;
                }

            //
            // If the existing package is older than the one being added, it
            // might be able to be replaced with this newer version. Re-check
            // all the dependencies.
            //

            } else {
                conflict = this._recheckDependencies(index, package);
                if (conflict == null) {
                    _addPlan[index] = package;
                    added = true;
                    break;

                } else {
                    Core.raise(PackageVersionConflict(
                        "Package %s version %s is needed by %s, but an older "
                        "version %s is needed by %s. Please resolve this "
                        "conflict and try again." %
                        [package.name,
                         package.version,
                         parameters.trail,
                         existing.version,
                         conflict.name]));
                }
            }
        }

        if (verbose) {
            Core.print("Adding %s to plan" % package.name);
        }

        try {
            package.root = parameters.root;

        } except KeyError {}

        if (!added) {
            _addPlan.append(package);
        }

        if (parameters.trail == "") {
            trail = package.name;

        } else {
            trail = "%s <- %s" % [package.name, parameters.trail];
        }

        depends = package.depends;
        if (depends is String) {
            depends = depends.split(null, -1);
        }

        for (dep in depends) {

            //
            // Restore the trail since it may have been appended to by the
            // last round.
            //

            parameters.trail = trail;
            dep = this._splitVersionRequirements(dep, parameters);
            try {
                dep = this.getPackage(dep, parameters);

            } except PackageNotFoundError as e {
                Core.raise(PackageNotFoundError(
                           "Cannot add dependency %s: needed by %s: "
                           "Package not found" % [dep, parameters.trail]));
            }

            //
            // Increment the reference count on this dependency. This is done
            // elsewhere so that the state can be saved after a partial update.
            //

            referenceCount = dep.get("referenceCount");
            try {
                _nextReferenceCount[dep] += 1;

            } except KeyError {
                if (referenceCount != null) {
                    _nextReferenceCount[dep] = referenceCount + 1;

                } else {
                    _nextReferenceCount[dep] = 1;
                }
            }

            //
            // Skip actually adding the dependency to the plan if it's already
            // installed.
            //

            if (referenceCount != null) {
                if (verbose) {
                    Core.print("Dependency %s: needed by %s: "
                               "Already installed" %
                               [dep.name, parameters.trail]);
                }

                continue;
            }

            this._addPackage(dep, parameters);
        }

        return;
    }

    function
    _removePackage (
        package,
        parameters
        )

    /*++

    Routine Description:

        This routine adds the given package to the plan of packages to remove.
        This routine is recursive.

    Arguments:

        package - Supplies the package information dictionary.

        parameters - Supplies additional parameters for the dependencies.

    Return Value:

        None. The package is added to the action plan.

    --*/

    {

        var depends;
        var userCount = package.get("userCount");
        var verbose = config.getKey("core.verbose");

        if (_addPlan.contains(package)) {
            Core.raise(PackageDependencyError(
                "Cannot remove %s: from %s: Package is slated for addition" %
                [package.name, parameters.trail]));
        }

        //
        // See if the package is already in the plan to be added.
        //

        if (_removePlan.contains(package)) {
            Core.print("Warning: Package %s already slated for removal: "
                       "reference counting error" % package.name);

            return;
        }

        package.referenceCount -= 1;
        if ((package.referenceCount != 0) || (userCount)) {
            if (verbose) {
                if (userCount == null) {
                    Core.print("Package %s still has %d references" %
                               [package.name, package.referenceCount]);

                } else {
                    Core.print("Package %s still has %d references "
                               "(and %d users)" %
                               [package.name, package.referenceCount,
                                userCount]);
                }

            }

            return;
        }

        if (verbose) {
            Core.print("Planning to remove %s" % package.name);
        }

        _removePlan.append(package);
        if (parameters.trail == "") {
            parameters.trail = package.name;

        } else {
            parameters.trail = "%s <- %s" % [package.name, parameters.trail];
        }

        depends = package.depends;
        if (depends is String) {
            depends = depends.split(null, -1);
        }

        for (dep in depends) {
            try {
                dep = this._getInstalledPackage(dep, parameters);

            } except PackageNotFoundError as e {
                Core.raise(PackageNotFoundError(
                           "Cannot remove dependency %s: from %s: "
                           "Package not found" % [dep, package.trail]));
            }

            //
            // Release the reference on the dependency.
            //

            this._removePackage(dep, parameters);
        }

        return;
    }

    function
    _openInstalledPackage (
        package
        )

    /*++

    Routine Description:

        This routine returns a new Package instance for the given installed
        package.

    Arguments:

        package - Supplies the package information dictionary.

    Return Value:

        Returns the new Package instance.

    --*/

    {

        var directory = this._packagePath(package);

        return Package.fromControlDirectory(directory);
    }

    function
    _packagePath (
        package
        )

    /*++

    Routine Description:

        This routine returns the path for the given package's storage

    Arguments:

        package - Supplies the package information dictionary.

    Return Value:

        Returns the path to the package storage directory.

    --*/

    {

        var directory;

        directory = "%s/%s-%s/%s-%sr%d" %
                    [SANTA_STORAGE_PATH,
                     package.os,
                     package.arch,
                     package.name,
                     package.version,
                     package.release];

        return path(directory);
    }

    function
    _latest (
        packageList
        )

    /*++

    Routine Description:

        This routine returns the package with the latest version number in the
        given list.

    Arguments:

        packageList - Supplies the list of packages.

    Return Value:

        Returns the element with the latest version number.

    --*/

    {

        var winner;

        if (packageList.length() == 0) {
            return null;
        }

        winner = packageList[0];
        for (element in packageList) {
            if (this.db.compareVersionStrings(element.version, winner.version) >
                0) {

                winner = element;
            }
        }

        return winner;
    }

    function
    _splitVersionRequirements (
        name,
        parameters
        )

    /*++

    Routine Description:

        This routine splits a string like mypkg>=1.2.3 into a name and a
        version parameter. If there is no version constraint, it is cleared
        from the parameters.

    Arguments:

        name - Supplies the name string.

        parameters - Supplies the parameters to set.

    Return Value:

        Returns the actual package name, versioning suffixes removed.

    --*/

    {

        var character;

        parameters.remove("version");
        for (index in 0..name.length()) {
            character = name[index];
            if ((character == ">") || (character == "<") ||
                (character == "=")) {

                parameters["version"] = name[index...-1];
                return name[0..index];
            }
        }

        //
        // No versioning info found.
        //

        return name;
    }

    function
    _recheckDependencies (
        addIndex,
        newPackage
        )

    /*++

    Routine Description:

        This routine determines if all dependencies are still satisfied by
        substituting the package in the add plan at the given index with the
        new package.

    Arguments:

        addIndex - Supplies the index into the add plan to substitute.

        newPackage - Supplies the new package to check.

    Return Value:

        Returns the first package with the incompatible constraint if the
        new package does not work.

        null if the new package works as a substitute.

    --*/

    {

        var deps;
        var existing;
        var name = newPackage.name;
        var parameters = {};

        //
        // Only check one level deep for all packages in the add plan. The
        // add plan may not be a complete closure at this point, but future
        // packages that might conflict will go through this check again.
        // Installed packages are in theory already a complete closure, so none
        // of them depend on this package in question.
        //

        for (index in 0.._addPlan.length()) {
            if (index == addIndex) {
                continue;
            }

            existing = _addPlan[index];
            deps = existing.depends;
            if (deps is String) {
                if (!deps.contains(name)) {
                    continue;
                }

                deps = deps.split(null, -1);
            }

            for (dep in deps) {
                dep = this._splitVersionRequirements(dep, parameters);
                if (dep != name) {
                    continue;
                }

                if (!this.db.checkVersionConstraints(newPackage, parameters)) {
                    return existing;
                }
            }
        }

        return null;
    }
}

//
// --------------------------------------------------------- Internal Functions
//

