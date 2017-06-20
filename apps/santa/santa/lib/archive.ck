/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archive.ck

Abstract:

    This module implements support for Santa archive files.

Author:

    Evan Green 20-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import cpio;
import io;
import lzfile;

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

class ArchiveMemberError is Exception {}

class Archive {

    static
    function
    open (
        path,
        mode
        )

    /*++

    Routine Description:

        This routine opens a pre-existing archive.

    Arguments:

        path - Supplies the path to the archive to open.

        mode - Supplies the mode to open the archive in. Valid values are "r"
            and "w".

    Return Value:

        Returns a new Archive instance on success.

        Raises an exception on error.

    --*/

    {

        var archive;
        var file;

        if (path is String) {
            file = (io.open)(path, mode + "b");
        }

        try {
            archive = Archive(file, mode);

        } except Exception as e {
            if (path is String) {
                file.close();
                Core.raise(e);
            }
        }

        return archive;
    }

    function
    __init (
        file,
        mode
        )

    /*++

    Routine Description:

        This routine initializes an archive.

    Arguments:

        file - Supplies the open file pointer.

        mode - Supplies the mode to open the archive in. Valid values are "r"
            and "w".

    Return Value:

        Returns this instance on success.

        Raises an exception on error.

    --*/

    {

        this.file = file;
        this.mode = mode;
        this.reset();
        return this;
    }

    function
    close (
        )

    /*++

    Routine Description:

        This routine closes an archive.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var cpio = this.cpio;
        var file = this.file;
        var lzma = this.lzma;

        if (cpio) {
            cpio.close();
            this.cpio = null;
        }

        if (lzma) {
            lzma.close();
            this.lzma = null;
        }

        if (file) {
            file.close();
            this.file = null;
        }

        this.mode = null;
        return;
    }

    function
    iterate (
        context
        )

    /*++

    Routine Description:

        This routine iterates over the archive.

    Arguments:

        context - Supplies the iteration context. null initially.

    Return Value:

        Returns the new iteration context, or null open reaching the end.

    --*/

    {

        return this.cpio.iterate(context);
    }

    function
    iteratorValue (
        context
        )

    /*++

    Routine Description:

        This routine returns the value for the given iteration context.

    Arguments:

        context - Supplies the iteration context. In this case just an index
            into the members.

    Return Value:

        Returns the member at this iteration.

    --*/

    {

        return this.cpio.iteratorValue(context);
    }

    function
    add (
        name,
        archiveName
        )

    /*++

    Routine Description:

        This routine adds one or more files to the archive.

    Arguments:

        name - Supplies the path name of the file to add.

        archiveName - Supplies the name that should go in the archive. Supply
            null to just use the same name as the source.

        recursive - Supplies a boolean indicating whether to include files
            within a specified directory or not.

        filter - Supplies an optional function that may modify or remove each
            member being added to the archive. If the routine returns the
            member, it is added (potentially modified). If it returns null,
            the member is abandoned and not added to the archive.

    Return Value:

        None. The member will be appended to the archive on success.

        An exception will be raised on failure.

    --*/

    {

        return this.cpio.add(name, archiveName, true, null);
    }

    function
    extract (
        archivePath,
        destinationPath
        )

    /*++

    Routine Description:

        This routine extracts a single member from the archive and saves it on
        disk.

    Arguments:

        archivePath - Supplies the path within the archive to extract.

        destinationPath - Supplies the path to save the member to.

    Return Value:

        Null on success. The member is extracted to its location.

        Raises an exception on error.

    --*/

    {

        var chunk;
        var chunkSize = 131072;
        var inFile;
        var outFile;

        for (member in this.cpio) {
            if (member.name == archivePath) {
                if (!member.isFile()) {
                    Core.raise(ValueError("Archive member %s is not a "
                                          "regular file" % archivePath));
                }

                try {
                    inFile = this.cpio.extractFile(member);
                    outFile = (io.open)(destinationPath, "wb");
                    while (true) {
                        chunk = inFile.read(chunkSize);
                        if (chunk.length() == 0) {
                            break;
                        }

                        outFile.write(chunk);
                    }

                } except Exception as e {
                    if (inFile) {
                        inFile.close();
                    }

                    if (outFile) {
                        outFile.close();
                    }

                    Core.raise(e);
                }

                inFile.close();
                outFile.close();
                return null;
            }
        }

        Core.raise(ArchiveMemberError("%s not found" % archivePath));
    }

    function
    extractData (
        archivePath
        )

    /*++

    Routine Description:

        This routine extracts a single member from the archive and returns its
        contents in memory.

    Arguments:

        archivePath - Supplies the path within the archive to extract.

    Return Value:

        Null on success. The member is extracted to its location.

        Raises an exception on error.

    --*/

    {

        for (member in this.cpio) {
            if (member.name == archivePath) {
                return this.cpio.extractFile(member).read(-1);
            }
        }

        Core.raise(ArchiveMemberError("%s not found" % archivePath));
    }

    function
    reset (
        )

    /*++

    Routine Description:

        This routine resets the archive file back to the beginning.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var cpio = this.cpio;
        var file = this.file;
        var lzma = this.lzma;
        var mode = this.mode;

        if (cpio) {
            cpio.close();
            this.cpio = null;
        }

        if (lzma) {
            lzma.close();
            this.lzma = null;
        }

        file.seek(0, io.IO_SEEK_SET);
        lzma = (lzfile.LzFile)(file, mode, 9);
        this.lzma = lzma;
        this.cpio = (cpio.CpioArchive)(lzma, mode);
        return;
    }
}

//
// --------------------------------------------------------- Internal Functions
//

