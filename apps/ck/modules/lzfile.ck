/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lzfile.ck

Abstract:

    This module implements support for performing file I/O on Minoca LZMA
    compressed files.

Author:

    Evan Green 23-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from io import open;
from lzma import LzmaEncoder, LzmaDecoder;
from iobase import RawIoBase, IoError;

//
// ---------------------------------------------------------------- Definitions
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

class LzFile is RawIoBase {
    var _readable;
    var _writable;
    var _closefd;
    var _level;
    var _file;
    var _lz;
    var _remainder;
    var _finished;

    function
    __init (
        name,
        mode,
        level
        )

    /*++

    Routine Description:

        This routine instantiates a new LzFile object.

    Arguments:

        name - Supplies the thing to open. If this is a string, this represents
            the path to open. If this is an integer, it represents the os file
            descriptor to open. Otherwise, it's treated as a file object.

        mode - Supplies the access mode. Valid values are 'r' for read, 'w' for
            write, or 'a' for append. Add a plus to get simultaneous reading
            and writing.

        level - Supplies the compression level to use. Valid values are from 0
            to 9.

    Return Value:

        Returns the initialized object.

    --*/

    {

        var accessCount = 0;
        var character;
        var index = 0;
        var modeLength = mode.length();

        if (_closefd) {
            this._close();
        }

        _file = null;
        _readable = false;
        _writable = false;
        _closefd = false;
        _finished = false;
        _remainder = "";
        _level = level;
        this.mode = mode;
        this.name = null;
        this.closed = false;

        //
        // Figure out the mode from the string.
        //

        for (index = 0; index < modeLength; index += 1) {
            character = mode[index];
            if (character == "r") {
                accessCount += 1;
                _readable = true;

            } else if (character == "w") {
                accessCount += 1;
                _writable = true;

            } else if (character == "+") {
                _readable = true;
                _writable = true;

            } else if ((character != "b") && (character != "t")) {
                Core.raise(ValueError("Invalid mode: %s" % mode));
            }
        }

        if (_readable && _writable) {
            Core.raise(ValueError("LZ file cannot be opened r/w"));
        }

        if (accessCount != 1) {
            Core.raise(ValueError("Mode must have exactly one r/w"));
        }

        //
        // If the name is an integer, use it as the file descriptor.
        //

        if ((name is Int) || (name is String)) {
            _file = open(name, mode + "b");

        } else {
            _file = name;
        }

        //
        // If this module opened the file, be responsible for closing it.
        //

        if (name is String) {
            _closefd = true;
        }

        this.name = _file.name;
        if (_readable) {
            _lz = LzmaDecoder(level, true);

        } else {
            _lz = LzmaEncoder(level, true);
        }

        return this;
    }

    //
    // TODO: Add __del method support to Chalk.
    //

    function
    __del (
        )

    /*++

    Routine Description:

        This routine is called when the object is going to be deleted.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        this._close();
        return;
    }

    function
    __str (
        )

    /*++

    Routine Description:

        This routine converts the file IO object into a string.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        return "<LzFile file=%s>" % [_file];
    }

    //
    // RawIoBase functions.
    //

    function
    read (
        size
        )

    /*++

    Routine Description:

        This routine performs a single read.

    Arguments:

        size - Supplies the amount to read. If -1 is supplied, then readall is
            called.

    Return Value:

        Returns the bytes read. If the empty string is returned and more than
        zero bytes were requested, then this is end-of-file.

    --*/

    {

        var compressed;
        var data = _remainder;
        var readSize;

        if (_file == null) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        if (!_readable) {
            this._raiseModeError("reading");
        }

        if (size < 0) {
            return this.readall();
        }

        //
        // See if there's enough in the remainder.
        //

        if (data.length() >= size) {
            _remainder = data[size...-1];
            return data[0..size];
        }

        size -= data.length();
        _remainder = "";

        //
        // Read from the underlying file. Read at least enough to decode one
        // symbol always, so that EOF isn't accidentally returned.
        //

        readSize = size;
        if (readSize < 65535) {
            readSize = 65535;
        }

        if (_finished) {
            compressed = "";

        } else {
            compressed = _file.read(readSize);
        }

        if (compressed.length() == 0) {
            if (!_finished) {
                _remainder = _lz.finish();
                _finished = true;
            }

        } else {
            _remainder = _lz.decompress(compressed);
        }

        if (_remainder.length() >= size) {
            data += _remainder[0..size];
            _remainder = _remainder[size...-1];

        } else {
            data += _remainder;
            _remainder = "";
        }

        return data;
    }

    function
    readall (
        )

    /*++

    Routine Description:

        This routine reads the entire file.

    Arguments:

        None.

    Return Value:

        Returns the contents of the entire file.

    --*/

    {

        var compressed;
        var data = _remainder;

        if (_file == null) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        if (!_readable) {
            this._raiseModeError("reading");
        }

        _remainder = "";
        if (!_finished) {
            compressed = _file.readall();
            data += _lz.decompress(compressed);
            data += _lz.finish();
            _finished = true;
        }

        return data;
    }

    function
    write (
        data
        )

    /*++

    Routine Description:

        This routine performs a single write.

    Arguments:

        size - Supplies the string of data to write.

    Return Value:

        Returns the number of bytes written, which may be less than the size of
        the data.

    --*/

    {

        var compressed;
        var size;

        if (_file == null) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        if (!_writable) {
            this._raiseModeError("writing");
        }

        if (_finished) {
            Core.raise(ValueError("Lz stream is already complete"));
        }

        compressed = _lz.compress(data);
        if (compressed.length()) {
            _remainder += compressed;
            size = _file.write(_remainder);
            if (size < _remainder.length()) {
                _remainder = _remainder[size...-1];

            } else {
                _remainder = "";
            }
        }

        return data.length();
    }

    function
    flush (
        )

    /*++

    Routine Description:

        This routine flushes the stream data. This does nothing for read-only
        or non-blocking streams.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var size;

        if (this.isClosed()) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        if (_writable) {
            while (_remainder.length()) {
                size = _file.write(_remainder);
                if (size < _remainder.length()) {
                    _remainder = _remainder[size...-1];

                } else {
                    _remainder = "";
                }
            }
        }

        return null;
    }

    //
    // IoBase functions.
    //

    function
    close (
        )

    /*++

    Routine Description:

        This routine closes and flushes a stream. If the stream is already
        closed, this does nothing.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var compressed;

        if (!this.closed) {
            if (_writable && !_finished) {
                compressed = _lz.finish();
                _finished = true;
                _remainder += compressed;
            }

            this.flush();
            this._close();
        }

        return 0;
    }

    function
    fileno (
        )

    /*++

    Routine Description:

        This routine returns the OS file descriptor number associated with the
        stream.

    Arguments:

        None.

    Return Value:

        Returns the file descriptor number associated with the stream.

        -1 if no file descriptor is associated with the stream or the file has
        been closed.

    --*/

    {

        return _file.fileno();
    }

    function
    isatty (
        )

    /*++

    Routine Description:

        This routine determines if the file descriptor backing this stream is
        a terminal device.

    Arguments:

        None.

    Return Value:

        true if the file descriptor is a terminal device.

        false if the file descriptor is not a terminal.

    --*/

    {

        return _file.isatty();
    }

    function
    isReadable (
        )

    /*++

    Routine Description:

        This routine determines if the given stream can be read from.

    Arguments:

        None.

    Return Value:

        true if the stream was opened with read permissions.

        false if the stream cannot be read from.

    --*/

    {

        if (_file == null) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        return _readable;
    }

    function
    tell (
        )

    /*++

    Routine Description:

        This routine returns the current file position.

    Arguments:

        None.

    Return Value:

        Returns the current file position.

    --*/

    {

        return _file.tell();
    }

    function
    isWritable (
        )

    /*++

    Routine Description:

        This routine determines if the given file can be written to.

    Arguments:

        None.

    Return Value:

        true if the file can be written to.

        false if the file cannot be written to.

    --*/

    {

        if (_file == null) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        return _writable;
    }

    //
    // Internal functions
    //

    function
    _close (
        )

    /*++

    Routine Description:

        This routine closes and flushes a stream.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        if (_file != null) {
            if (_closefd) {
                _file.close();
            }

            _file = null;
        }

        this.closed = true;
        return 0;
    }

    function
    _raiseModeError (
        mode
        )

    /*++

    Routine Description:

        This routine raises an error for an operation the file is not open for.

    Arguments:

        mode - Supplies the mode string to print in the error.

    Return Value:

        None.

    --*/

    {

        Core.raise(ValueError("File not open for " + mode));
    }
}

function
lzopen (
    name,
    mode,
    level
    )

/*++

Routine Description:

    This routine opens an LZ file for reading or writing.

Arguments:

    name - Supplies the thing to open. If this is a string, this represents
        the path to open. If this is an integer, it represents the os file
        descriptor to open. Otherwise, it's treated as a file object.

    mode - Supplies the access mode. Valid values are 'r' for read, 'w' for
        write, or 'a' for append. Add a plus to get simultaneous reading
        and writing.

    level - Supplies the compression level to use. Valid values are from 0
        to 9.

Return Value:

    Returns an instance of an LzFile, which acts much like a regular FileIo
    object.

--*/

{

    return LzFile(name, mode, level);
}

//
// --------------------------------------------------------- Internal Functions
//

