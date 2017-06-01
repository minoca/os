/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fileio.ck

Abstract:

    This module implements support for performing file I/O. It requires the os
    module. This module is inspired by Python's FileIO module.

Author:

    Evan Green 10-Jan-2016

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from iobase import RawIoBase, IoError, IO_SEEK_SET, IO_SEEK_CUR, IO_SEEK_END,
    DEFAULT_BUFFER_SIZE;

from os import O_RDONLY, O_WRONLY, O_APPEND, O_RDWR, O_TRUNC, O_CREAT, O_TEXT,
    O_BINARY, OS_SEEK_SET, OS_SEEK_CUR, OS_SEEK_END;

from os import EINTR, EAGAIN;
from os import OsError, open, close, read, write, lseek, isatty, ftruncate,
    fstat;

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

//
// FileIo is the RawIoBase implementation for the os layer.
//

class FileIo is RawIoBase {
    var _fd;
    var _readable;
    var _writable;
    var _appending;
    var _seekable;
    var _closefd;

    function
    __init (
        name,
        mode,
        permissions,
        closefd
        )

    /*++

    Routine Description:

        This routine instantiates a new FileIo object.

    Arguments:

        name - Supplies the thing to open. If this is a string, this represents
            the path to open. If this is an integer, it represents the os file
            descriptor to open.

        mode - Supplies the access mode. Valid values are 'r' for read, 'w' for
            write, or 'a' for append. Add a plus to get simultaneous reading
            and writing.

        permissions - Supplies the permissions to create the file with for
            creations. Ignored if the file is not created.

        closefd - Supplies a boolean indicating whether the file descriptor (if
            passed in via the name parameter) should be closed when the stream
            is closed.

    Return Value:

        Returns the initialized object.

    --*/

    {

        var access = -1;
        var accessCount = 0;
        var character;
        var flags = 0;
        var index = 0;
        var modeLength = mode.length();
        var plus = 0;

        if (_closefd) {
            this._close();
        }

        _fd = -1;
        _readable = false;
        _writable = false;
        _appending = false;
        _seekable = -1;
        _closefd = true;
        this.mode = mode;
        this.name = name;
        this.closed = false;

        //
        // Figure out the mode from the string.
        //

        for (index = 0; index < modeLength; index += 1) {
            character = mode[index];
            if (character == "r") {
                accessCount += 1;
                _readable = true;
                access = O_RDONLY;

            } else if (character == "w") {
                accessCount += 1;
                _writable = true;
                access = O_WRONLY;
                flags |= O_CREAT | O_TRUNC;

            } else if (character == "a") {
                accessCount += 1;
                _writable = true;
                _appending = true;
                access = O_WRONLY;
                flags |= O_CREAT | O_APPEND;

            } else if (character == "+") {
                _readable = true;
                _writable = true;
                plus += 1;
                access = O_RDWR;

            } else if (character == "b") {
                flags &= ~O_TEXT;
                flags |= O_BINARY;

            } else if (character == "t") {
                flags |= O_TEXT;
                flags &= ~O_BINARY;

            } else {
                Core.raise(ValueError("Invalid mode: %s" % mode));
            }
        }

        if ((accessCount != 1) || (plus > 1)) {
            Core.raise(ValueError("Mode must have exactly one r/w/a and "
                                  "at most one plus"));
        }

        //
        // If the name is an integer, use it as the file descriptor.
        //

        if (name is Int) {
            if (name < 0) {
                Core.raise(ValueError("Negative file descriptor passed"));
            }

            _fd = name;
            _closefd = closefd;

        } else if (name is String) {
            if (!closefd) {
                Core.raise(ValueError("Specify true for closefd when opening a "
                                      "file path"));
            }

            try {
                _fd = open(name, access | flags, permissions);

            } except OsError as e {
                this._raiseIoError(e);
            }

        } else {
            Core.raise(TypeError("Expected an integer or a string"));
        }

        if (_appending) {
            try {
                lseek(_fd, 0, OS_SEEK_END);

            } except OsError {}
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

        return "<FileIo name=%s fd=%d mode=%s>" % [this.name, _fd, this.mode];
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

        var data = "";

        if (_fd < 0) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        if (!_readable) {
            this._raiseModeError("reading");
        }

        if (size < 0) {
            return this.readall();
        }

        while (true) {
            try {
                data = read(_fd, size);

            } except OsError as e {
                if (e.errno == EINTR) {
                    continue;

                } else if (e.errno == EAGAIN) {
                    break;
                }

                this._raiseIoError(e);
            }

            break;
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

        var bufferSize;
        var data;
        var result = [];
        var total = 0;

        if (_fd < 0) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        while (true) {
            bufferSize = this._estimateBufferSize(total);
            try {
                data = read(_fd, bufferSize);
                if (data.length() == 0) {
                    break;
                }

                result.append(data);
                total += data.length();

            } except OsError as e {
                if (e.errno == EINTR) {
                    continue;

                } else if (e.errno == EAGAIN) {
                    if (total) {
                        break;
                    }
                }

                this._raiseIoError(e);
            }
        }

        return "".join(result);
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

        var size = 0;

        if (_fd < 0) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        if (!_writable) {
            this._raiseModeError("writing");
        }

        while (true) {
            try {
                size = write(_fd, data);

            } except OsError as e {
                if (e.errno == EINTR) {
                    continue;

                } else if (e.errno == EAGAIN) {
                    break;
                }

                this._raiseIoError(e);
            }

            break;
        }

        return size;
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

        if (!this.closed) {
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

        if (_fd < 0) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        return _fd;
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

        var result = false;

        if (_fd < 0) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        try {
            if (isatty(_fd)) {
                result = true;
            }

        } except OsError {}

        return result;
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

        if (_fd < 0) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        return _readable;
    }

    function
    seek (
        offset,
        whence
        )

    /*++

    Routine Description:

        This routine seeks into the file.

    Arguments:

        offset - Supplies the desired offset to seek from or to.

        whence - Supplies the anchor point for the seek. Supply IO_SEEK_SET to
            seek from the beginning of the file, IO_SEEK_CUR to seek from the
            current position, or IO_SEEK_END to seek from the end of the file.

    Return Value:

        Returns the new absolute position within the file.

    --*/

    {

        var osWhence;

        if (_fd < 0) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        if (whence == IO_SEEK_SET) {
            osWhence = OS_SEEK_SET;

        } else if (whence == IO_SEEK_CUR) {
            osWhence = OS_SEEK_CUR;

        } else if (whence == IO_SEEK_END) {
            osWhence = OS_SEEK_END;
        }

        return lseek(_fd, offset, osWhence);
    }

    function
    isSeekable (
        )

    /*++

    Routine Description:

        This routine determines if the file backing the stream can be seeked on.

    Arguments:

        None.

    Return Value:

        true if the underlying file is seekable.

        false if the underlying file is not seekable.

    --*/

    {

        if (_fd < 0) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        if (_seekable < 0) {
            try {
                lseek(_fd, 0, OS_SEEK_CUR);
                _seekable = true;

            } except OsError {
                _seekable = false;
            }
        }

        return _seekable;
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

        if (_fd < 0) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        return this.seek(0, IO_SEEK_CUR);
    }

    function
    truncate (
        size
        )

    /*++

    Routine Description:

        This routine truncates the file to the given size. The stream position
        is not changed.

    Arguments:

        size - Supplies the new size of the file.

    Return Value:

        Returns the new file size.

    --*/

    {

        if (_fd < 0) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        if (!_writable) {
            this._raiseModeError("writing");
        }

        try {
            ftruncate(_fd, size);

        } except OsError as e {
            this._raiseIoError(e);
        }

        return size;
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

        if (_fd < 0) {
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

        if (_fd >= 0) {
            try {
                close(_fd);
                _fd = -1;

            } except OsError as e {
                this._raiseIoError(e);
            }
        }

        this.closed = true;
        return 0;
    }

    function
    _estimateBufferSize (
        currentSize
        )

    /*++

    Routine Description:

        This routine returns a buffer to hold the file.

    Arguments:

        currentSize - Supplies the current size of the buffer.

    Return Value:

        None.

    --*/

    {

        var end = 0;
        var fileInfo;
        var position;

        try {
            fileInfo = fstat(_fd);
            end = fileInfo.st_size;
            position = lseek(_fd, 0, OS_SEEK_CUR);
            if ((end > DEFAULT_BUFFER_SIZE) && (end >= position) &&
                (position >= 0)) {

                return currentSize + (end - position) + 1;
            }

        } except OsError {}

        if (currentSize <= 0) {
            return DEFAULT_BUFFER_SIZE;
        }

        return currentSize + (currentSize >> 2) + 32;
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

    function
    _raiseIoError (
        oserror
        )

    /*++

    Routine Description:

        This routine raises an I/O error from an OS error.

    Arguments:

        oserror - Supplies the OS error.

    Return Value:

        None.

    --*/

    {

        var ioerror = IoError(oserror.args);

        ioerror.errno = oserror.errno;
        Core.raise(ioerror);
        return;
    }
}

//
// --------------------------------------------------------- Internal Functions
//

