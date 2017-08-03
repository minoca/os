/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    diff.ck

Abstract:

    This module implements the DiffFile class, which can create a diff between
    two regular files.

Author:

    Evan Green 5-Jul-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import io;
import os;
import time;

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

class DiffFile {
    var _downVector;
    var _upVector;
    var _modifiedA;
    var _modifiedB;

    function
    __init (
        leftFilePath,
        rightFilePath
        )

    /*++

    Routine Description:

        This routine initializes a new diff file instance based on two file
        paths.

    Arguments:

        leftFilePath - Supplies the path to the left file.

        rightFilePath - Supplies the path to the right file.

    Return Value:

        Returns an initialized class instance.

    --*/

    {

        this.left = this._loadFile(leftFilePath);
        this.right = this._loadFile(rightFilePath);
        this.contextLines = 3;
        this.comparison = -1;
        this._computeDiff();
        return this;
    }

    function
    iterate (
        iterator
        )

    /*++

    Routine Description:

        This routine iterates over the hunks in a diff.

    Arguments:

        iterator - Supplies null initially, or the previous iterator.

    Return Value:

        Returns the new hunk.

    --*/

    {

        return this._findNextHunk(iterator);
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

        return iterator;
    }

    function
    unifiedDiff (
        )

    /*++

    Routine Description:

        This routine returns the unified diff for the two files.

    Arguments:

        None.

    Return Value:

        Returns a string of the unified diff between the files.

    --*/

    {

        var result;

        if (this.comparison == 0) {
            return "";
        }

        result = [this.unifiedHeader()];
        if (this.left.fileType != this.right.fileType) {
            result.append("Left file is a %s, and right file is a %s" % [
                          this.left.fileType,
                          this.right.fileType]);

        } else if (this.left.fileType == "file") {
            for (hunk in this) {
                result.append(this.unifiedHunk(hunk));
            }
        }

        return "".join(result);
    }

    function
    unifiedHeader (
        )

    /*++

    Routine Description:

        This routine returns the unified diff format header for the given
        diff instance.

    Arguments:

        None.

    Return Value:

        Returns a string of the unified diff between the files.

    --*/

    {

        var leftTime;
        var result;
        var rightTime;

        leftTime = (time.Time).fromUtcTimestamp(this.left.date, 0);
        rightTime = (time.Time).fromUtcTimestamp(this.right.date, 0);
        result = "--- %s\t%s\n"
                 "+++ %s\t%s\n" % [
                 this.left.name,
                 leftTime.strftime("%a %b %d %H:%M:%S %Y"),
                 this.right.name,
                 rightTime.strftime("%a %b %d %H:%M:%S %Y")];

        return result;
    }

    function
    unifiedHunk (
        hunk
        )

    /*++

    Routine Description:

        This routine returns the unified diff format output for the given hunk.

    Arguments:

        hunk - Supplies the hunk to convert to a string.

    Return Value:

        Returns a string of the hunk.

    --*/

    {

        var header;
        var endA;
        var endB;
        var indexA;
        var indexB;
        var lineA;
        var lineB;
        var result;
        var sizeA;
        var sizeB;

        lineA = hunk.left.line;
        lineB = hunk.right.line;
        sizeA = hunk.left.size;
        sizeB = hunk.right.size;
        endA = lineA + sizeA;
        endB = lineB + sizeB;

        //
        // Print the hunk marker. For size zero hunks, use line zero if the
        // file was empty, or the correct 1-based line if not.
        //

        if (sizeA == 0) {
            header = "@@ -%d,0 " % (lineA + (lineA != 0));

        } else if (sizeA == 1) {
            header = "@@ -%d " % (lineA + 1);

        } else {
            header = "@@ -%d,%d " % [lineA + 1, sizeA];
        }

        if (sizeB == 0) {
            header += "+%d,0 @@" % (lineB + (lineB != 0));

        } else if (sizeB == 1) {
            header += "+%d @@" % (lineB + 1);

        } else {
            header += "+%d,%d @@" % [lineB + 1, sizeB];
        }

        result = [header];
        indexA = lineA;
        indexB = lineB;
        while ((indexA < endA) || (indexB < endB)) {

            //
            // Print any context lines.
            //

            while ((indexA < endA) && (!_modifiedA.get(indexA)) &&
                   (indexB < endB) && (!_modifiedB.get(indexB))) {

                result.append("\n ");
                result.append(this.left.lines[indexA]);
                indexA += 1;
                indexB += 1;
            }

            //
            // Print all deletions lines together.
            //

            while ((indexA < endA) && (_modifiedA.get(indexA))) {
                result.append("\n-");
                result.append(this.left.lines[indexA]);
                indexA += 1;
            }

            if ((sizeA != 0) && (indexA == this.left.lines.length()) &&
                (this.left.nonewline)) {

                result.append("\n\\ No newline at end of file");
            }

            //
            // Print all insertion lines together.
            //

            while ((indexB < endB) && (_modifiedB.get(indexB))) {
                result.append("\n+");
                result.append(this.right.lines[indexB]);
                indexB += 1;
            }

            if ((sizeB != 0) && (indexB == this.right.lines.length()) &&
                (this.right.nonewline)) {

                result.append("\n\\ No newline at end of file");
            }
        }

        result.append("\n");
        return "".join(result);
    }

    function
    _computeDiff (
        )

    /*++

    Routine Description:

        This routine computes the diff between the two files mentioned here.

    Arguments:

        None.

    Return Value:

        None. The edit script will be saved into this instance.

    --*/

    {

        var leftData;
        var leftLength;
        var originalLeft;
        var rightData;
        var rightLength;
        var originalRight;

        //
        // If the diff was already computed, don't do it again.
        //

        if (this.comparison != -1) {
            return;
        }

        leftData = this.left.data;
        rightData = this.right.data;
        this.left.data = null;
        this.right.data = null;

        //
        // If the types don't agree, then the files are "different" in the same
        // way that apples and sportscars are different.
        //

        if (this.left.fileType != this.right.fileType) {
            this.comparison = 2;
            return;
        }

        //
        // First just compare the entire data set. If it matches, there's no
        // need to do the tedious line splitting, etc.
        //

        if (leftData == rightData) {
            this.comparison = 0;
            return;
        }

        //
        // Split the left and right files into lines.
        //

        this.left.lines = leftData.split("\n", -1);
        if (this.left.lines[-1] == "") {
            this.left.lines.removeAt(-1);
            this.left.nonewline = false;
        }

        this.right.lines = rightData.split("\n", -1);
        if (this.right.lines[-1] == "") {
            this.right.lines.removeAt(-1);
            this.right.nonewline = false;
        }

        leftLength = this.left.lines.length();
        rightLength = this.right.lines.length();
        _downVector = {};
        _upVector = {};
        _modifiedA = {};
        _modifiedB = {};

        //
        // If the left and right disagree on newlines, then temporarily
        // substitute last lines that will compare false.
        //

        if (this.left.nonewline != this.right.nonewline) {
            if (leftLength) {
                originalLeft = this.left.lines[leftLength - 1];
                if (this.left.nonewline) {
                    this.left.lines[leftLength - 1] =
                                                 originalLeft + "\\no newline";
                }
            }

            if (rightLength) {
                originalRight = this.right.lines[rightLength - 1];
                if (this.right.nonewline) {
                    this.right.lines[rightLength - 1] = originalRight +
                                                        "\\no newline";
                }
            }
        }

        this.comparison =
            this._computeLongestCommonSubsequence(0,
                                                  leftLength,
                                                  0,
                                                  rightLength);

        //
        // Put the originals back.
        //

        if (originalLeft != null) {
            this.left.lines[leftLength - 1] = originalLeft;
        }

        if (originalRight != null) {
            this.right.lines[rightLength - 1] = originalRight;
        }

        _downVector = null;
        _upVector = null;

        //
        // If the files are identical, release the lines as well to free up
        // memory.
        //

        if (this.comparison == 0) {
            this.left.lines = null;
            this.right.lines = null;
        }

        return;
    }

    function
    _computeLongestCommonSubsequence (
        lowerA,
        upperA,
        lowerB,
        upperB
        )

    /*++

    Routine Description:

        This routine implements the Myers' algorithm for computing the longest
        common subsequence in linear space (but with recursion). The algorithm
        is a divide-and-conquer algorithm, finding an element of the correct
        path in the middle and then recursing on each of the slightly smaller
        split pieces.

    Arguments:

        LowerA - Supplies the starting index within file A to work on.

        UpperA - Supplies the ending index within file A to work on, exclusive.

        LowerB - Supplies the starting index within file B to work on.

        UpperB - Supplies the ending index within file B to work on, exclusive.

    Return Value:

        0 if the files are equal in the compared region.

        1 if the files differ somewhere.

    --*/

    {

        var linesA = this.left.lines;
        var linesB = this.right.lines;
        var snake;
        var status = 0;
        var substatus;

        //
        // As a basic no-brainer, skip any lines at the beginning and end that
        // match.
        //

        while ((lowerA < upperA) && (lowerB < upperB)) {
            if (linesA[lowerA] != linesB[lowerB]) {
                status = 1;
                break;
            }

            lowerA += 1;
            lowerB += 1;
        }

        while ((lowerA < upperA) && (lowerB < upperB)) {
            if (linesA[upperA - 1] != linesB[upperB - 1]) {
                status = 1;
                break;
            }

            upperA -= 1;
            upperB -= 1;
        }

        //
        // If file A ended, then mark everything in file B as an insertion.
        //

        if (lowerA == upperA) {
            if (lowerB < upperB) {
                status = 1;
            }

            while (lowerB < upperB) {
                _modifiedB[lowerB] = true;
                lowerB += 1;
            }

        //
        // If file B ended, then mark everything in file A as a deletion.
        //

        } else if (lowerB == upperB) {
            if (lowerA < upperA) {
                status = 1;
            }

            while (lowerA < upperA) {
                _modifiedA[lowerA] = true;
                lowerA += 1;
            }

        //
        // Run the real crux of the diff algorithm.
        //

        } else {
            snake = this._computeShortestMiddleSnake(lowerA,
                                                     upperA,
                                                     lowerB,
                                                     upperB);

            substatus = this._computeLongestCommonSubsequence(lowerA,
                                                              snake.x,
                                                              lowerB,
                                                              snake.y);

            if (substatus != 0) {
                status = substatus;
            }

            substatus = this._computeLongestCommonSubsequence(snake.x,
                                                              upperA,
                                                              snake.y,
                                                              upperB);

            if (substatus != 0) {
                status = substatus;
            }
        }

        return status;
    }

    function
    _computeShortestMiddleSnake (
        lowerA,
        upperA,
        lowerB,
        upperB
        )

    /*++

    Routine Description:

        This routine implements the crux of the Myers' algorithm for computing
        the longest common subsequence in linear space.

    Arguments:

        LowerA - Supplies the starting index within file A to work on.

        UpperA - Supplies the ending index within file A to work on, exclusive.

        LowerB - Supplies the starting index within file B to work on.

        UpperB - Supplies the ending index within file B to work on, exclusive.

    Return Value:

        Returns a dictionary with x and y elements of the middle snake with the
        lowest D value.

    --*/

    {

        var delta;
        var deltaIsOdd = false;
        var dIndex;
        var downK;
        var downOffset;
        var kIndex;
        var linesA = this.left.lines;
        var linesB = this.right.lines;
        var maximum;
        var maximumD;
        var snakeX;
        var snakeY;
        var upK;
        var upOffset;

        //
        // The maximum D value would be going all the way right and all the way
        // down (meaning the files are entirely different).
        //

        maximum = this.left.lines.length() + this.right.lines.length() + 1;

        //
        // Compute the K lines to start the forward (down) and reverse (up)
        // searches.
        //

        downK = lowerA - lowerB;
        upK = upperA - upperB;

        //
        // Delta is the difference in k between the start point and the end
        // point. This is needed to know which indices of the vector to check
        // for overlap.
        //

        delta = (upperA - lowerA) - (upperB - lowerB);
        if (delta & 0x1) {
            deltaIsOdd = true;
        }

        //
        // In the paper, k values can go from -D to D. Use offsets to avoid
        // actually accessing negative array values. Though the underlying
        // structure is a dictionary (which would be fine with negative keys),
        // keep the offset so that a List could be used with minimal fuss here.
        //

        downOffset = maximum - downK;
        upOffset = maximum - upK;

        //
        // Running the algorithm forward and reverse is guaranteed to cross
        // somewhere before D / 2.
        //

        maximumD = (((upperA - lowerA) + (upperB - lowerB)) / 2) + 1;

        //
        // Initialize the vectors.
        //

        _downVector[downOffset + downK + 1] = lowerA;
        _upVector[upOffset + upK - 1] = upperA;

        //
        // Iterate through successive D values until an overlap is found. This
        // is guaranteed to be the shortest path because it has the lowest D
        // value.
        //

        for (dIndex = 0; dIndex <= maximumD; dIndex += 1) {

            //
            // Run the algorithm forward. Compute all the coordinates for each
            // k line between -D and D in steps of two.
            //

            for (kIndex = downK - dIndex;
                 kIndex <= downK + dIndex;
                 kIndex += 2) {

                //
                // Use the better of the two x coordinates of adjacent k lines,
                // being careful at the edges to avoid comparing against
                // impossible (unreachable) k lines.
                //

                if (kIndex == downK - dIndex) {

                    //
                    // Take the same x coordinate as the k line above (meaning
                    // go down).
                    //

                    snakeX = _downVector[downOffset + kIndex + 1];

                } else {

                    //
                    // Take the 1 + the x coordinate below, meaning to right.
                    // Switch to going down if it's possible and better (starts
                    // further). In a tie, go down.
                    //

                    snakeX = _downVector[downOffset + kIndex - 1] + 1;
                    if ((kIndex < downK + dIndex) &&
                        (_downVector[downOffset + kIndex + 1] >= snakeX)) {

                        snakeX = _downVector[downOffset + kIndex + 1];
                    }
                }

                snakeY = snakeX - kIndex;

                //
                // Advance as many diagonals as possible.
                //

                while ((snakeX < upperA) && (snakeY < upperB)) {
                    if (linesA[snakeX] != linesB[snakeY]) {
                        break;
                    }

                    snakeX += 1;
                    snakeY += 1;
                }

                _downVector[downOffset + kIndex] = snakeX;

                //
                // Check for overlap.
                //

                if (deltaIsOdd &&
                    (kIndex > upK - dIndex) && (kIndex < upK + dIndex)) {

                    if (_upVector[upOffset + kIndex] <=
                        _downVector[downOffset + kIndex]) {

                        return {
                            "x": _downVector[downOffset + kIndex],
                            "y": _downVector[downOffset + kIndex] - kIndex
                        };
                    }
                }
            }

            //
            // Run the algorithm in reverse. Compute all the coordinates for
            // each k line between -D and D in steps of two.
            //

            for (kIndex = upK - dIndex;
                 kIndex <= upK + dIndex;
                 kIndex += 2) {

                //
                // Decide whether to take the path from the bottom or right.
                //

                if (kIndex == upK + dIndex) {

                    //
                    // Take the x position from the lower k line, meaning go up.
                    //

                    snakeX = _upVector[upOffset + kIndex - 1];

                } else {

                    //
                    // Go right, unless going up is better.
                    //

                    snakeX = _upVector[upOffset + kIndex + 1] - 1;
                    if ((kIndex > upK - dIndex) &&
                        (_upVector[upOffset + kIndex - 1] < snakeX)) {

                        snakeX = _upVector[upOffset + kIndex - 1];
                    }
                }

                snakeY = snakeX - kIndex;

                //
                // Take as many diagonals as possible.
                //

                while ((snakeX > lowerA) && (snakeY > lowerB)) {
                    if (linesA[snakeX - 1] != linesB[snakeY - 1]) {
                        break;
                    }

                    snakeX -= 1;
                    snakeY -= 1;
                }

                _upVector[upOffset + kIndex] = snakeX;

                //
                // Check for overlap.
                //

                if ((!deltaIsOdd) &&
                    (kIndex >= downK - dIndex) &&
                    (kIndex <= downK + dIndex)) {

                    if (_upVector[upOffset + kIndex] <=
                        _downVector[downOffset + kIndex]) {

                        return {
                            "x": _downVector[downOffset + kIndex],
                            "y": _downVector[downOffset + kIndex] - kIndex
                        };
                    }
                }
            }
        }

        //
        // A middle snake should always be found.
        //

        Core.raise(ValueError("Internal error: A middle snake was not found"));
    }

    function
    _loadFile (
        path
        )

    /*++

    Routine Description:

        This routine initializes a new file from a path.

    Arguments:

        path - Supplies the path to load.

    Return Value:

        Returns a dictionary of file information.

    --*/

    {

        var file;
        var fileType;
        var result = {};
        var stat;

        result.name = path;
        try {
            stat = (os.lstat)(path);
            fileType = stat.st_mode & os.S_IFMT;
            if (fileType == os.S_IFREG) {
                fileType = "file";

            } else if (fileType == os.S_IFDIR) {
                fileType == "directory";

            } else if (fileType == os.S_IFBLK) {
                fileType == "block device";

            } else if (fileType == os.S_IFCHR) {
                fileType == "character device";

            } else if (fileType == os.S_IFIFO) {
                fileType = "FIFO";

            } else if (fileType == os.S_IFSOCK) {
                fileType = "socket";

            } else if (fileType == os.S_IFLNK) {
                fileType = "symbolic link";

            } else {
                Core.raise(ValueError("Unknown file type %d" % fileType));
            }

            result.fileType = fileType;
            result.date = stat.st_mtime;
            result.nonewline = true;
            result.lines = [];
            result.data = null;
            if (fileType == "file") {
                file = (io.open)(path, "r");
                result.data = file.readall();
                file.close();
            }

        } except os.OsError as e {
            if (e.errno == os.ENOENT) {
                result.fileType = "file";
                result.date = 0;
                result.lines = [];
                result.nonewline = false;
                result.data = "";

            } else {
                Core.raise(e);
            }
        }

        return result;
    }

    function
    _findNextHunk (
        hunk
        )

    /*++

    Routine Description:

        This routine finds the next hunk in a diff between two files.

    Arguments:

        hunk - Supplies the previous hunk, or null to find the first hunk.

    Return Value:

        Returns the next hunk, or null if there are no more hunks. The lines
        in the hunks are zero-based.

    --*/

    {

        var contextA;
        var contextB;
        var contextLines;
        var originalLineA = 0;
        var originalLineB = 0;
        var lineA = 0;
        var lineB = 0;
        var lineCountA = this.left.lines.length();
        var lineCountB = this.right.lines.length();
        var sizeA = 0;
        var sizeB = 0;

        if (hunk == null) {
            hunk = {
                "left": {
                    "line": 0,
                },

                "right": {
                    "line": 0,
                }
            };

        } else {
            originalLineA = hunk.left.line;
            originalLineB = hunk.right.line;
            lineA = originalLineA + hunk.left.size;
            lineB = originalLineB + hunk.right.size;
        }

        hunk.left.size = 0;
        hunk.right.size = 0;
        while ((lineA < lineCountA) && (lineB < lineCountB)) {

            //
            // If either line is modified, then a hunk has been found.
            //

            if ((_modifiedA.get(lineA)) || (_modifiedB.get(lineB))) {
                break;
            }

            lineA += 1;
            lineB += 1;
            hunk.left.line = lineA;
            hunk.right.line = lineB;
        }

        //
        // Loop advancing past the diff and lines of context.
        //

        while (1) {

            //
            // Advance through modified lines in A, and as well in B.
            //

            while (_modifiedA.get(lineA + sizeA)) {
                sizeA += 1;
            }

            while (_modifiedB.get(lineB + sizeB)) {
                sizeB += 1;
            }

            //
            // Now try to advance through the lines of context as well. If a
            // modification is found in either line, then the next hunk blurs
            // into this one. Look past two sets of context lines because any
            // fewer and the next hunk will bleed back up into this one.
            //

            contextLines = 0;
            contextA = 0;
            contextB = 0;
            while (contextLines < (this.contextLines * 2) + 1) {
                if (_modifiedA.get(lineA + sizeA + contextA)) {
                    break;
                }

                if (_modifiedB.get(lineB + sizeB + contextB)) {
                    break;
                }

                if (lineA + sizeA + contextA < lineCountA) {
                    contextA += 1;
                }

                if (lineB + sizeB + contextB < lineCountB) {
                    contextB += 1;
                }

                contextLines += 1;
            }

            if ((contextLines == (this.contextLines * 2) + 1) ||
                ((contextA == 0) && (contextB == 0))) {

                //
                // Add up to the requested amount of context lines to the size.
                //

                if (contextA > this.contextLines) {
                    contextA = this.contextLines;
                }

                if (contextB > this.contextLines) {
                    contextB = this.contextLines;
                }

                sizeA += contextA;
                sizeB += contextB;
                break;

            //
            // Otherwise, add all the context lines consumed to the hunk.
            //

            } else {
                sizeA += contextA;
                sizeB += contextB;
            }
        }

        //
        // If both hunks are of size zero, there are no more diffs.
        //

        if (sizeA | sizeB == 0) {
            return null;
        }

        //
        // Also back up to provide context lines at the beginning.
        //

        contextLines = this.contextLines;
        if (contextLines > lineA) {
            contextLines = lineA;
        }

        if (lineA - contextLines < originalLineA) {
            contextLines = lineA - originalLineA;
        }

        lineA -= contextLines;
        sizeA += contextLines;
        contextLines = this.contextLines;
        if (contextLines > lineB) {
            contextLines = lineB;
        }

        if (lineB - contextLines < originalLineB) {
            contextLines = lineB - originalLineB;
        }

        lineB -= contextLines;
        sizeB += contextLines;
        hunk.left.line = lineA;
        hunk.left.size = sizeA;
        hunk.right.line = lineB;
        hunk.right.size = sizeB;
        return hunk;
    }
}

class DiffSet {
    function
    __init (
        )

    /*++

    Routine Description:

        This routine initializes a new diff set.

    Arguments:

        leftFilePath - Supplies the path to the left file.

        rightFilePath - Supplies the path to the right file.

    Return Value:

        Returns an initialized class instance.

    --*/

    {

        this.files = [];
        return this;
    }

    function
    add (
        leftPath,
        rightPath
        )

    /*++

    Routine Description:

        This routine adds a diff to the diff set. If these are both directories,
        then all files under the directories are diffed, recursively.

    Arguments:

        leftPath - Supplies the path to the left file.

        rightPath - Supplies the path to the right file.

    Return Value:

        None.

    --*/

    {

        var file;
        var files = {};
        var leftDir = (os.isdir)(leftPath);
        var leftExists = (os.exists)(leftPath);
        var rightDir = (os.isdir)(rightPath);
        var rightExists = (os.exists)(rightPath);

        //
        // If both paths are directories, or one path is and the other doesn't
        // exist, then recurse.
        //

        if ((leftDir && rightDir) ||
            (leftDir && !rightExists) || (rightDir && !leftExists)) {

            //
            // Create a union of both directories.
            //

            if (leftDir) {
                for (file in (os.listdir)(leftPath)) {
                    files[file] = 1;
                }
            }

            if (rightDir) {
                for (file in (os.listdir)(rightPath)) {
                    files[file] = 1;
                }
            }

            //
            // Create a diff for each file in either directory.
            //

            for (file in files) {
                this.add("%s/%s" % [leftPath, file],
                         "%s/%s" % [rightPath, file]);
            }

            return;
        }

        //
        // Either the file types don't match up, or they're both regular files.
        //

        file = DiffFile(leftPath, rightPath);
        if (file.comparison != 0) {
            this.files.append(file);
        }

        return;
    }

    function
    unifiedDiff (
        )

    /*++

    Routine Description:

        This routine returns the unified diff for all files in the set.

    Arguments:

        None.

    Return Value:

        Returns a string of the unified diff between all files.

    --*/

    {

        var diff;
        var result = [];

        for (file in this) {
            diff = file.unifiedDiff();
            if (diff != "") {
                result.append(diff);
            }
        }

        return "".join(result);
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
}

//
// --------------------------------------------------------- Internal Functions
//

