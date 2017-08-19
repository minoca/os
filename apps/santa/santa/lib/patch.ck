/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    patch.ck

Abstract:

    This module implements the PatchFile class, which can apply a patch to a
    file.

Author:

    Evan Green 6-Jul-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import io;
from iobase import IoError;
import os;

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

var PATCH_OPTION_REVERSE = 0x00000001;
var PATCH_OPTION_IGNORE_EOL = 0x00000002;
var PATCH_OPTION_IGNORE_BLANKS = 0x00000004;
var PATCH_OPTION_NO_FUZZ = 0x00000008;
var PATCH_OPTION_TEST = 0x00000010;
var PATCH_OPTION_QUIET = 0x00000020;
var PATCH_OPTION_VERBOSE = 0x00000040;

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

class PatchError is Exception {}
class PatchMissingError is Exception {}
class PatchFormatError is Exception {}

class PatchFile {
    var _targetPath;
    var _verbose;

    function
    __init (
        patchLines,
        offset
        )

    /*++

    Routine Description:

        This routine initializes a new patch for an individual file based on a
        set of patch lines.

    Arguments:

        patchLines - Supplies the lines of the patch file as a list, or a
            string containing the patch itself.

        offset - Supplies an optional offset into the patch lines list to
            start at.

    Return Value:

        Returns an initialized class instance.

    --*/

    {

        if (offset == null) {
            offset = 0;
        }

        if (patchLines is String) {
            patchLines = patchLines.split("\n", -1);
        }

        if (!(patchLines is List)) {
            Core.raise(TypeError("Expected a string or a list"));
        }

        this._readPatch(patchLines, offset);
        return this;
    }

    function
    apply (
        options,
        pathSplit
        )

    /*++

    Routine Description:

        This routine applies a patch.

    Arguments:

        options - Supplies the patch options.

        pathSplit - Supplies the number of leading slashes to trim off the
            path.

    Return Value:

        None. The file specified in the patch is modified according to the
        patche's edit script. An exception is raised on error.

    --*/

    {

        var destination;
        var file;
        var hunk;
        var lines;
        var path;
        var reverse = options & PATCH_OPTION_REVERSE;
        var source;

        if (this.hunks.length() == 1) {
            hunk = this.hunks[0];
            if (reverse) {
                destination = hunk.source;
                source = hunk.destination;

            } else {
                destination = hunk.destination;
                source = hunk.source;
            }

            //
            // Handle removing a file.
            //

            if ((destination.line == 0) && (destination.length == 0)) {
                path = reverse ? this.sourceFile : this.destinationFile;
                if (pathSplit != 0) {
                    path = path.split("/", pathSplit)[-1];
                }

                if (!(os.exists)(path)) {
                    path = reverse ? this.destinationFile: this.sourceFile;
                    if (pathSplit != 0) {
                        path = path.split("/", pathSplit)[-1];
                    }
                }

                if ((os.exists)(path)) {
                    if (options & PATCH_OPTION_VERBOSE) {
                        Core.print("Removing file: %s" % path);
                    }

                    if (options & PATCH_OPTION_TEST) {
                        return;
                    }

                    (os.unlink)(path);
                }

                return;

            //
            // Handle creating a file.
            //

            } else if ((source.line == 0) && (source.length == 0)) {
                lines = [];
            }
        }

        if (lines == null) {
            file = this._openTarget("r", pathSplit);
            lines = file.readall().split("\n", -1);
            file.close();
        }

        //
        // Remove the trailing newline, it gets added explicitly at the end.
        //

        if ((lines.length()) && (lines[-1] == "")) {
            lines.removeAt(-1);
        }

        _verbose = 1;
        if (options & PATCH_OPTION_QUIET) {
            _verbose = 0;

        } else if (options & PATCH_OPTION_VERBOSE) {
            _verbose = 2;
            Core.print("Patching file: %s" % _targetPath);
        }

        for (hunk in this.hunks) {
            this._applyUnifiedHunk(lines, options, hunk);
        }

        if (options & PATCH_OPTION_TEST) {
            return;
        }

        file = this._openTarget("w", pathSplit);
        file.write("\n".join(lines));

        //
        // Write the terminating newline.
        //

        if ((!reverse && this.destinationNewline) ||
            (reverse && this.sourceNewline)) {

            file.write("\n");
        }

        file.close();
        return;
    }

    function
    _readPatch (
        lines,
        offset
        )

    /*++

    Routine Description:

        This routine reads in a patch.

    Arguments:

        lines - Supplies a list of lines, the first patch in there will be
            returned.

        offset - Supplies the offset within the lines to start from.

    Return Value:

        None.

    --*/

    {

        var line;

        //
        // Search for a patch header.
        //

        for (index in offset..(lines.length())) {
            line = lines[index];
            if (line.startsWith("--- ")) {
                if (this._readUnifiedPatch(lines, index)) {
                    this.startLine = offset;
                    this.format = "unified";
                    return;
                }
            }
        }

        if (offset != 0) {
            Core.raise(PatchMissingError("No patch found after line %d" %
                                         offset));
        }

        Core.raise(PatchMissingError("No patch found"));
        return;
    }

    function
    _readUnifiedPatch (
        lines,
        offset
        )

    /*++

    Routine Description:

        This routine reads in a single unified format patch.

    Arguments:

        lines - Supplies a list of lines.

        offset - Supplies the offset within the lines where the unified patch
            exists.

    Return Value:

        Returns an initialized class instance.

    --*/

    {

        var destinationCount;
        var destinationFile;
        var destinationNewline = true;
        var fields;
        var hunk;
        var hunks = [];
        var line = lines[offset];
        var location;
        var previous;
        var sourceCount;
        var sourceFile;
        var sourceNewline = true;
        var startOffset;

        offset += 1;
        if (!line.startsWith("--- ")) {
            return false;
        }

        fields = line[4...-1].split("\t", 1);
        sourceFile = fields[0];
        if (offset >= lines.length()) {
            return false;
        }

        line = lines[offset];
        offset += 1;
        if (!line.startsWith("+++ ")) {
            return false;
        }

        fields = line[4...-1].split("\t", 1);
        destinationFile = fields[0];
        try {

            //
            // Loop reading hunks.
            //

            while (lines[offset].startsWith("@@ ")) {
                line = lines[offset];
                offset += 1;
                startOffset = offset;
                fields = line.split(null, -1);
                if ((fields.length() != 4) ||
                    (fields[0] != "@@") || (fields[3] != "@@") ||
                    (fields[1][0] != "-") || (fields[2][0] != "+")) {

                    Core.raise(PatchFormatError("Invalid hunk header: %s" %
                                                line));
                }

                hunk = {
                    "source": {},
                    "destination": {}
                };

                try {
                    location = fields[1][1...-1].split(",", 1);
                    hunk.source.line = Int.fromString(location[0]);
                    hunk.source.length = 1;
                    if (location.length() > 1) {
                        hunk.source.length = Int.fromString(location[1]);
                    }

                    location = fields[2][1...-1].split(",", 1);
                    hunk.destination.line = Int.fromString(location[0]);
                    hunk.destination.length = 1;
                    if (location.length() > 1) {
                        hunk.destination.length = Int.fromString(location[1]);
                    }

                } except ValueError {
                    Core.raise(PatchFormatError("Invalid hunk bounds: %s" %
                                                line));
                }

                //
                // Loop over and check out the lines in the hunk.
                //

                sourceCount = hunk.source.length;
                destinationCount = hunk.destination.length;
                while (sourceCount + destinationCount != 0) {
                    line = lines[offset];
                    offset += 1;
                    if (line.startsWith(" ")) {
                        sourceCount -= 1;
                        destinationCount -= 1;
                        previous = " ";

                    } else if (line.startsWith("-")) {
                        sourceCount -= 1;
                        previous = "-";

                    } else if (line.startsWith("+")) {
                        destinationCount -= 1;
                        previous = "+";

                    //
                    // Remember if there's a no-newline at end of file marker.
                    // This doesn't count in the diff line numbers.
                    //

                    } else if (line.startsWith("\\ ")) {
                        if (previous == " ") {
                            sourceNewline = false;
                            destinationNewline = false;

                        } else if (previous == "+") {
                            destinationNewline = false;

                        } else if (previous == "-") {
                            sourceNewline = false;
                        }

                    } else {
                        Core.raise(PatchFormatError("Invalid line %d: %s" %
                                                    [offset - 1, line]));
                    }
                }

                if ((sourceCount != 0) || (destinationCount != 0)) {
                    Core.raise(PatchFormatError("Hunk at line %d does not "
                                                "match header" % startOffset));
                }

                hunk.lines = lines[startOffset..offset];
                hunks.append(hunk);
            }

            //
            // See if there's an extra line complaining about no newline.
            //

            if (offset < lines.length()) {
                line = lines[offset];
                if (line.startsWith("\\ ")) {
                    if (previous == " ") {
                        sourceNewline = false;
                        destinationNewline = false;

                    } else if (previous == "+") {
                        destinationNewline = false;

                    } else if (previous == "-") {
                        sourceNewline = false;
                    }

                    offset += 1;
                }
            }

        } except IndexError {
            Core.raise(PatchFormatError("Unexpected end of file"));
        }

        this.sourceFile = sourceFile;
        this.destinationFile = destinationFile;
        this.sourceNewline = sourceNewline;
        this.destinationNewline = destinationNewline;
        this.hunks = hunks;
        this.endLine = offset;
        return true;
    }

    function
    _openTarget (
        mode,
        pathSplit
        )

    /*++

    Routine Description:

        This routine opens the patch file target.

    Arguments:

        mode - Supplies the mode to open with.

        pathSplit - Supplies the number of leading slashes to trim off the
            path.

    Return Value:

        Returns an open file.

    --*/

    {

        var file;
        var path;

        if (_targetPath) {
            return this._openPath(_targetPath, mode);
        }

        path = this.sourceFile;
        if (pathSplit != 0) {
            path = path.split("/", pathSplit)[-1];
        }

        try {
            file = this._openPath(path, mode);
            _targetPath = path;
            return file;

        } except IoError as e {
            if (e.errno == os.ENOENT) {
                path = this.destinationFile;
                if (pathSplit != 0) {
                    path = path.split("/", pathSplit)[-1];
                }

                file = this._openPath(path, mode);
                _targetPath = path;
                return file;
            }

            Core.raise(e);
        }

        Core.raise(RuntimeError("Execution should never get here"));
    }

    function
    _openPath (
        targetPath,
        mode
        )

    /*++

    Routine Description:

        This routine opens a target file path. If the open comes back with
        permission denied, this routine tries to gain permissions.

    Arguments:

        targetPath - Supplies the path to open.

        mode - Supplies the mode to open with.

    Return Value:

        Returns an open file.

    --*/

    {

        var perms;

        try {
            return (io.open)(targetPath, mode);

        } except IoError as e {
            if ((e.errno != os.EPERM) && (e.errno != os.EACCES)) {
                Core.raise(e);
            }
        }

        perms = (os.stat)(_targetPath).st_mode & 03777;
        (os.chmod)(_targetPath, perms | 0220);
        return (io.open)(_targetPath, mode);
    }

    function
    _applyUnifiedHunk (
        lines,
        options,
        hunk
        )

    /*++

    Routine Description:

        This routine applies a hunk in unified form to the given set of lines.

    Arguments:

        lines - Supplies the set of lines in the file.

        options - Suppiles the patch options. See PATCH_OPTION_* definitions.

        hunk - Supplies the hunk to apply.

    Return Value:

        None. The lines are edited inline.

    --*/

    {

        var add = "+";
        var check;
        var destination = "destination";
        var distance = 0;
        var lineCount = lines.length();
        var startLine;
        var subtract = "-";

        if (options & PATCH_OPTION_REVERSE) {
            add = "-";
            subtract = "+";
            destination = "source";
        }

        startLine = hunk[destination].line;
        if (!this._checkUnifiedHunk(lines, hunk, options, startLine)) {

            //
            // See if the patch is already applied.
            //

            check = this._checkUnifiedHunk(lines,
                                           hunk,
                                           options ^ PATCH_OPTION_REVERSE,
                                           startLine);

            if (check) {
                Core.raise(PatchError("%s: Hunk at line %d is already applied" %
                                      [_targetPath, startLine]));
            }

            if ((options & PATCH_OPTION_NO_FUZZ) == 0) {
                while ((startLine - distance > 0) ||
                       (startLine + distance <= lineCount)) {

                    if (startLine - distance >= 0) {
                        check = this._checkUnifiedHunk(lines,
                                                       hunk,
                                                       options,
                                                       startLine - distance);

                        if (check) {
                            break;
                        }
                    }

                    if (startLine + distance < lineCount) {
                        check = this._checkUnifiedHunk(lines,
                                                       hunk,
                                                       options,
                                                       startLine + distance);

                        if (check) {
                            break;
                        }
                    }

                    distance += 1;
                }
            }

            if (!check) {
                Core.raise(PatchError("%s: Failed to apply hunk at line "
                                      "%d" %
                                      [_targetPath, startLine]));
            }
        }

        //
        // Apply the hunk to the lines. Subtract one to take the line from
        // being one-based to zero-based.
        //

        if (_verbose > 0) {
            if (distance != 0) {
                Core.print("%s: Hunk at %d applied %+d lines away" %
                           [_targetPath, startLine, distance]);

            } else if (_verbose > 1) {
                Core.print("%s: Applied hunk at %d" % [_targetPath, startLine]);
            }
        }

        startLine += distance;
        if (startLine != 0) {
            startLine -= 1;
        }

        for (line in hunk.lines) {
            if (line.startsWith(" ")) {
                startLine += 1;
                continue;

            } else if (line.startsWith(add)) {
                lines.insert(startLine, line[1...-1]);
                startLine += 1;

            } else if (line.startsWith(subtract)) {
                lines.removeAt(startLine);

            } else if (!line.startsWith("\\ ")) {
                Core.raise(PatchFormatError("Bad format: %s" % line));
            }
        }

        return;
    }

    function
    _checkUnifiedHunk (
        lines,
        hunk,
        options,
        lineNumber
        )

    /*++

    Routine Description:

        This routine checks to see if a unified hunk will apply at the given
        line.

    Arguments:

        lines - Supplies the set of lines in the file.

        hunk - Supplies the hunk to apply.

        options - Suppiles the patch options. See PATCH_OPTION_* definitions.

        lineNumber - Supplies the line number to attempt to apply it at.

    Return Value:

        true if the patch applies successfully here.

        false if the patch does not apply here.

    --*/

    {

        var subtract = "-";

        if (options & PATCH_OPTION_REVERSE) {
            subtract = "+";
        }

        //
        // Make the line number zero-based.
        //

        if (lineNumber != 0) {
            lineNumber -= 1;
        }

        try {

            //
            // Iterate over each line in the hunk.
            //

            for (line in hunk.lines) {
                if ((line.startsWith(" ")) || (line.startsWith(subtract))) {
                    line = line[1...-1];
                    if (!this._compareLines(lines[lineNumber], line, options)) {
                        return false;
                    }

                    lineNumber += 1;
                }
            }

        } except IndexError {
            return false;
        }

        return true;
    }

    function
    _compareLines (
        left,
        right,
        options
        )

    /*++

    Routine Description:

        This routine compares two lines for equality.

    Arguments:

        left - Supplies the left line to compare.

        right - Supplies the right line to compare.

        options - Suppiles the patch options. See PATCH_OPTION_* definitions.

    Return Value:

        true if the patch applies successfully here.

        false if the patch does not apply here.

    --*/

    {

        if (left == right) {
            return true;
        }

        if (options & (PATCH_OPTION_IGNORE_EOL | PATCH_OPTION_IGNORE_BLANKS)) {
            left = left.replace("\r", "", -1);
            right = right.replace("\r", "", -1);
        }

        if (options & PATCH_OPTION_IGNORE_BLANKS) {
            left = left.replace(" ", "", -1).replace("\t", "", -1);
            left = left.replace("\v", "", -1).replace("\f", "", -1);
            right = right.replace(" ", "", -1).replace("\t", "", -1);
            right = right.replace("\v", "", -1).replace("\f", "", -1);
        }

        return left == right;
    }

}

class PatchSet {
    var _patchFilePath;

    function
    __init (
        patchFilePath
        )

    /*++

    Routine Description:

        This routine initializes a new patch set instance based on the path
        to a patch file.

    Arguments:

        patchFilePath - Supplies the path to the patch file.

    Return Value:

        Returns an initialized class instance.

    --*/

    {

        var file;
        var lines;

        _patchFilePath = patchFilePath;
        file = (io.open)(patchFilePath, "r");
        lines = file.readall().split("\n", -1);
        file.close();
        this._createPatchFiles(lines);
        return this;
    }

    function
    iterate (
        iterator
        )

    /*++

    Routine Description:

        This routine iterates over the files in a diff set.

    Arguments:

        iterator - Supplies null initially, or the previous iterator.

    Return Value:

        Returns the new hunk.

    --*/

    {

        return this.files.iterate(iterator);
    }

    function
    iteratorValue (
        iterator
        )

    /*++

    Routine Description:

        This routine returns the value for an iterator.

    Arguments:

        iterator - Supplies null initially, or the previous iterator

    Return Value:

        Returns the value corresponding with the iterator.

    --*/

    {

        return this.files.iteratorValue(iterator);
    }

    function
    apply (
        directory,
        options,
        pathSplit
        )

    /*++

    Routine Description:

        This routine applies a set of patches to the files in the given
        directory.

    Arguments:

        directory - Supplies the directory to apply patches in.

        options - Supplies a mask of PATCH_OPTION_* bits governing how the
            patch should be applied.

        pathSplit - Supplies the number of leading slashes to strip from the
            patch path.

    Return Value:

        None. An exception is raised if any of the patches fail to apply.

    --*/

    {

        var oldCwd = (os.getcwd)();
        var test;

        if (directory) {
            (os.chdir)(directory);
        }

        //
        // First test everything to avoid leaving some files patched and others
        // not.
        //

        if ((options & PATCH_OPTION_TEST) == 0) {
            test = (options | PATCH_OPTION_TEST | PATCH_OPTION_QUIET) &
                   ~PATCH_OPTION_VERBOSE;

            try {
                for (file in this) {
                    file.apply(test, pathSplit);
                }

            } except Exception as e {
                if (directory) {
                    (os.chdir)(oldCwd);
                }

                Core.raise(e);
            }
        }

        //
        // Okay now do it for real.
        //

        for (file in this) {
            file.apply(options, pathSplit);
        }

        return;
    }

    function
    _createPatchFiles (
        lines
        )

    /*++

    Routine Description:

        This routine creates patch files for each patch in the given file.

    Arguments:

        iterator - Supplies null initially, or the previous iterator.

    Return Value:

        Returns the new hunk.

    --*/

    {

        var file;
        var files = [];
        var line = 0;

        //
        // Loop grabbing patches until an exception is thrown that there are
        // no more patches.
        //

        try {
            while (1) {
                file = PatchFile(lines, line);
                files.append(file);
                line = file.endLine;
            }

        } except PatchMissingError as e {
            if (line == 0) {
                Core.raise(e);
            }
        }

        this.files = files;
        return;
    }
}

//
// --------------------------------------------------------- Internal Functions
//

