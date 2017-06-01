/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    io.ck

Abstract:

    This module implements support for the base IO classes and functions. It's
    inspired by Python's I/O classes.

Author:

    Evan Green 10-Jan-2016

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from iobase import DEFAULT_BUFFER_SIZE, IO_SEEK_SET, IO_SEEK_CUR, IO_SEEK_END;
from fileio import FileIo;
from bufferedio import BufferedIo;

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

function
open2 (
    file,
    mode,
    permissions,
    buffering,
    closefd
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

function
open (
    file,
    mode
    )

/*++

Routine Description:

    This routine opens a file.

Arguments:

    file - Supplies either a string containing the path name to open, or an
        OS file descriptor number.

    mode - Supplies the access mode of the open file. Use one of 'r' for read,
        'w' for write, or 'a' for append. Add a '+' to open the file for both
        reading and writing.

Return Value:

    Returns an open file object.

--*/

{

    return open2(file, mode, 0664, -1, true);
}

function
open2 (
    file,
    mode,
    permissions,
    buffering,
    closefd
    )

/*++

Routine Description:

    This routine opens a file with specific options.

Arguments:

    file - Supplies either a string containing the path name to open, or an
        OS file descriptor number.

    mode - Supplies the access mode of the open file. Use one of 'r' for read,
        'w' for write, or 'a' for append. Add a '+' to open the file for both
        reading and writing.

    permissions - Supplies the permissions to create the file with. This is
        ignored if this file is not created.

    buffering - Supplies the buffer size to use. Supply 0 to use no buffering,
        -1 for default buffering, or a positive value to specify the buffer
        size. With default buffering, the resulting file object will be
        buffered unless it is an interactive terminal device, in which case it
        will not be.

    closefd - Supplies a boolean indicating whether or not to close the file
        descriptor when the file is closed.

Return Value:

    Returns an open file object.

--*/

{

    var buffered;
    var raw;

    raw = FileIo(file, mode, permissions, closefd);
    if (buffering < 0) {
        if (raw.isatty()) {
            return raw;
        }

    } else if (buffering == 0) {
        return raw;
    }

    buffered = BufferedIo(raw, buffering);
    return buffered;
}

//
// --------------------------------------------------------- Internal Functions
//

