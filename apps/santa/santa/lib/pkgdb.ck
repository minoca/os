/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pkgdb.ck

Abstract:

    This module implements the PackageDatabase class, which manages the set of
    available packages in various databases.

Author:

    Evan Green 17-Jul-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from santa.config import config;
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
// ------------------------------------------------------------------ Functions
//

class PackageDatabase {

    function
    __init (
        )

    /*++

    Routine Description:

        This routine instantiates a new Package Manager.

    Arguments:

        None.

    Return Value:

        Returns the initialized instance.

    --*/

    {

        var depot;
        var depotList = [];
        var repoList = config.getKey("repositories");
        var verbose = config.getKey("core.verbose");

        if (!(repoList is List)) {
            Core.raise(ValueError("config: repositories should be a list"));
        }

        for (repository in repoList) {
            if (verbose) {
                if (repository == null) {
                    Core.print("Loading local package index: " +
                               config.getKey("core.pkgdir"));

                } else {
                    Core.print("Loading package index: %s" % repository);
                }
            }

            depot = PackageDepot(repository);
            depot.load(false);
            depotList.append(depot);
        }

        this.repositories = depotList;
        return this;
    }

    function
    filter (
        parameters,
        single
        )

    /*++

    Routine Description:

        This routine gets all packages that match the given criteria.

    Arguments:

        parameters - Supplies the dictionary of parameters to match against.

        single - Supplies a boolean indicating whether to return just a single
            package or as many as fit the criteria.

    Return Value:

        Returns a list of eligible packages if single is false.

        Returns the package or null if single is true.

    --*/

    {

        var found;
        var result = [];

        for (repo in this.repositories) {
            for (pkg in repo.pkgs) {
                found = true;

                //
                // Make sure each element specified in the parameters matches.
                //

                for (key in parameters) {

                    //
                    // Allow arch and os to match for "none".
                    //

                    if ((key == "arch") || (key == "os")) {
                        if (pkg[key] == "none") {
                            continue;
                        }
                    }

                    if (pkg[key] != parameters[key]) {
                        found = false;
                        break;
                    }
                }

                if (found) {
                    pkg.repository = repo.location;
                    if (single) {
                        return pkg;
                    }

                    result.append(pkg);
                }
            }
        }

        return result;
    }

    function
    getLatest (
        name
        )

    /*++

    Routine Description:

        This routine gets the newest version of a package by name.

    Arguments:

        name - Supplies the name of the package to get.

    Return Value:

        Returns the/an eligible package.

        null if no such package exists.

    --*/

    {

        var results = this.filter({"name": name}, false);
        var winner;

        if (results.length() == 0) {
            return null;
        }

        winner = results[0];
        for (result in results) {
            if (this.compareVersions(winner, result) < 0) {
                winner = result;
            }
        }

        return winner;
    }

    function
    compareVersionStrings (
        left,
        right
        )

    /*++

    Routine Description:

        This routine compares two package version strings.

    Arguments:

        left - Supplies the left string to compare.

        right - Supplies the right string to compare.

    Return Value:

        -1 if the left < right.

        0 if left == right.

        1 if left > right.

    --*/

    {

        var leftElement;
        var leftNumber;
        var max;
        var rightElement;
        var rightNumber;

        //
        // Split at dots, dashes, underscores, and whitespace.
        //

        left = left.replace(".", " ", -1).replace("-", " ", -1)
                   .replace("_", " ", -1);

        right = right.replace(".", " ", -1).replace("-", " ", -1)
                     .replace("_", " ", -1);

        left = left.split(null, -1);
        right = right.split(null, -1);
        max = left.length();
        if (max < right.length()) {
            max = right.length();
        }

        for (index in 0..max) {
            try {
                leftElement = left[index];

            } except IndexError {
                leftElement = "0";
            }

            try {
                rightElement = right[index];

            } except IndexError {
                rightElement = "0";
            }

            //
            // Try to compare them as numbers. If either don't work, compare as
            // strings.
            //

            try {
                leftNumber = Int.fromString(leftElement);
                rightNumber = Int.fromString(rightElement);

            } except ValueError {
                if (leftElement < rightElement) {
                    return -1;
                }

                if (leftElement > rightElement) {
                    return 1;
                }
            }
        }

        return 0;
    }

    function
    compareVersions (
        left,
        right
        )

    /*++

    Routine Description:

        This routine compares two packages.

    Arguments:

        left - Supplies the left package info to compare.

        right - Supplies the right package info to compare.

    Return Value:

        -1 if the left < right.

        0 if left == right.

        1 if left > right.

    --*/

    {

        var result = this.compareVersionStrings(left.version, right.version);

        if (result != 0) {
            return result;
        }

        if (left.release < right.release) {
            return -1;
        }

        if (left.release > right.release) {
            return 1;
        }

        return 0;
    }
}

//
// --------------------------------------------------------- Internal Functions
//

