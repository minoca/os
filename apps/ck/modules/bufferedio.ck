/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bufferedio.ck

Abstract:

    This module implements support for performing buffered file I/O. It's
    inspired by Python's buffered I/O classes.

Author:

    Evan Green 10-Jan-2016

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from iobase import BufferedIoBase, IoError, DEFAULT_BUFFER_SIZE, IO_SEEK_CUR;

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
// BufferedRwSeekable is the fully functional read/write implementation of the
// Buffered I/O base for a seekable file.
//

class BufferedIo is BufferedIoBase {
    var _raw;
    var _readable;
    var _writable;
    var _position;
    var _bufferSize;
    var _readBuffer;
    var _readOffset;
    var _writeBuffers;
    var _writeSize;

    function
    __init (
        raw,
        bufferSize
        )

    /*++

    Routine Description:

        This routine instantiates a new BufferdIo object.

    Arguments:

        raw - Supplies the raw I/O object, an instance of a RawIoBase subclass.

        bufferSize - Supplies the buffer size to use. If -1,
            DEFAULT_BUFFER_SIZE will be used.

    Return Value:

        Returns the object.

    --*/

    {

        _raw = raw;
        this.name = _raw.name;
        this.mode = _raw.mode;
        this.closed = false;
        this.raw = _raw;
        if (bufferSize <= 0) {
            bufferSize = DEFAULT_BUFFER_SIZE;
        }

        _bufferSize = bufferSize;
        _readable = _raw.isReadable();
        _writable = _raw.isWritable();
        _position = 0;
        _readBuffer = "";
        _readOffset = 0;
        _writeBuffers = [];
        _writeSize = 0;
        return this;
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

        if (_raw.isClosed()) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        if (_writable) {
            this._flushWrite();
        }

        this._flushRead();
        return;
    }

    function
    peek (
        size
        )

    /*++

    Routine Description:

        This routine returns data from the stream without advancing the
        position. At most one single read on the raw stream is done to
        satisfy the call. The number of bytes returned may be more or less
        than requested.

    Arguments:

        size - Supplies the size to peek at.

    Return Value:

        Returns the peeked data.

    --*/

    {

        var data;
        var existingLength;
        var readData;
        var readSize;
        var result;

        if (_writable) {
            this._flushWrite();
        }

        existingLength = _readBuffer.length() - _readOffset;
        if (size <= 0) {
            if (existingLength > 0) {
                size = existingLength;

            } else {
                size = _bufferSize;
            }
        }

        //
        // See if the request can be satisfied completely from the buffer.
        //

        if (existingLength >= size) {
            data = _readBuffer[_readOffset..(_readOffset + size)];
            return data;
        }

        //
        // Do one read to try and get the rest of the data requested. Round up
        // to try and read the whole buffer.
        //

        readSize = size - existingLength;
        if (readSize < _bufferSize) {
            readSize = _bufferSize;
        }

        readData = _raw.read(readSize);
        _position += readData.length();
        _readBuffer = _readBuffer[_readOffset...-1] + readData;
        _readOffset = 0;
        return _readBuffer[0..size];
    }

    function
    read (
        size
        )

    /*++

    Routine Description:

        This routine performs a read of the requested size.

    Arguments:

        size - Supplies the amount to read. If -1 is supplied, then readall is
            called.

    Return Value:

        Returns the bytes read. If the empty string is returned and more than
        zero bytes were requested, then this is end-of-file.

    --*/

    {

        var data;
        var existingLength;
        var length = 0;
        var result;

        if (size < -1) {
            Core.raise(ValueError("Size must be positive or -1"));
        }

        if (size == -1) {
            return this.readall();
        }

        if (_writable) {
            this._flushWrite();
        }

        //
        // Grab as much as possible from the existing buffer. Maybe it's all
        // that's needed.
        //

        existingLength = _readBuffer.length() - _readOffset;
        if (existingLength > size) {
            result = _readBuffer[_readOffset..(_readOffset + size)];
            _readOffset += size;
            return result;
        }

        //
        // Wipe out the remaining buffer, and start a list of buffers that
        // represents the result.
        //

        data = _readBuffer[_readOffset...-1];
        length = data.length();
        if (length) {
            size -= length;
            result = [data];

        } else {
            result = [];
        }

        _readBuffer = "";
        _readOffset = 0;

        //
        // Read directly into the result if huge amounts are requested.
        //

        length = 1;
        while (size >= _bufferSize) {
            data = _raw.read(size);
            length = data.length();
            if (!length) {
                break;
            }

            _position += length;
            result.append(data);
            size -= length;
        }

        //
        // Now refill the buffer and do the partial read out of it.
        //

        if (length != 0) {
            while (size != 0) {
                _readBuffer = _raw.read(_bufferSize);
                length = _readBuffer.length();
                if (!length) {
                    break;
                }

                _position += length;
                if (size <= length) {
                    result.append(_readBuffer[0..size]);
                    _readOffset = size;
                    break;

                } else {
                    result.append(_readBuffer);
                    size -= length;
                }
            }
        }

        return "".joinList(result);
    }

    function
    read1 (
        size
        )

    /*++

    Routine Description:

        This routine performs a single read.

    Arguments:

        size - Supplies the amount to read. If -1 is supplied, then data is
            read until the end of file is encountered.

    Return Value:

        Returns the bytes read. If the empty string is returned and more than
        zero bytes were requested, then this is end-of-file.

    --*/

    {

        var data;
        var existingLength;
        var length = 0;
        var result;

        if (size < 0) {
            Core.raise(ValueError("Size must be positive"));
        }

        if (_writable) {
            this._flushWrite();
        }

        //
        // Grab as much as possible from the existing buffer. Maybe it's all
        // that's needed.
        //

        existingLength = _readBuffer.length() - _readOffset;
        if (existingLength > size) {
            result = _readBuffer[_readOffset..(_readOffset + size)];
            _readOffset += size;
            return result;
        }

        //
        // Wipe out the remaining buffer, and start a list of buffers that
        // represents the result.
        //

        data = _readBuffer[_readOffset...-1];
        if (length) {
            size -= existingLength;
            result = [data];

        } else {
            result = [];
        }

        _readBuffer = "";
        _readOffset = 0;

        //
        // If the caller just wants a little, refill the buffer. Otherwise,
        // do all of what the caller wants.
        //

        if (size < _bufferSize) {
            _readBuffer = _raw.read(_bufferSize);
            length = _readBuffer.length();
            if (length) {
                _position += length;
                if (size > length) {
                    size = length;
                }

                _readOffset = size;
                result.append(_readBuffer[0..size]);
            }

        } else {
            data = _raw.read(size);
            result.append(data);
            _position += data.length();
        }

        return "".joinList(result);
    }

    function
    readline (
        limit
        )

    /*++

    Routine Description:

        This routine reads a single line from the stream.

    Arguments:

        limit - Supplies the maximum number of bytes to read. Supply -1 for no
            limit.

    Return Value:

        Returns the line read from the file.

    --*/

    {

        var data;
        var index;
        var length;
        var result;

        if (_writable) {
            this._flushWrite();
        }

        //
        // Clip the buffer so that indexOf doesn't find something already found.
        //

        if (_readOffset) {
            _readBuffer = _readBuffer[_readOffset...-1];
            _readOffset = 0;
        }

        //
        // See if the buffer already contains a complete line.
        //

        index = _readBuffer.indexOf("\n");
        if (index != -1) {
            result = _readBuffer[_readOffset...index];
            _readOffset = index + 1;
            return result;
        }

        //
        // Pull the whole buffer into the result.
        //

        if (_readBuffer.length()) {
            data = _readBuffer[_readOffset...-1];
            result = [data];
            _readBuffer = "";
            _readOffset = 0;

        } else {
            result = [];
        }

        //
        // Loop reading blocks until a newline or EOF is found.
        //

        while (true) {
            _readBuffer = _raw.read(_bufferSize);
            length = _readBuffer.length();
            if (!length) {
                break;
            }

            _position += length;
            index = _readBuffer.indexOf("\n");
            if (index != -1) {
                result.append(_readBuffer[0...index]);
                _readOffset = index + 1;
                break;
            }

            result.append(_readBuffer);
        }

        return "".join(result);
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

        return _position + _writeSize - (_readBuffer.length() - _readOffset);
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

        this.flush();
        _position = _raw.seek(offset, whence);
        return _position;
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

        var result;

        this.flush();
        result = _raw.truncate(size);
        _position = _raw.tell();
        return result;
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

        var data;
        var existingLength;
        var result;

        if (_writable) {
            this._flushWrite();
        }

        existingLength = _readBuffer.length() - _readOffset;
        if (!existingLength) {
            result = _raw.readall();
            _position += result.length();
            return result;
        }

        result = [_readBuffer[_readOffset...-1]];
        data = _raw.readall();
        _position += data.length();
        result.append(data);
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

        var length = data.length();
        var result = 0;

        if (_raw.isClosed()) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        if (!_writable) {
            Core.raise(ValueError("File not open for writing"));
        }

        if (_readable) {
            this._flushRead();
        }

        if (length > _bufferSize) {
            this._flushWrite();
            result = _raw.write(data);
            _position += result;
            return result;
        }

        if (!(data is String)) {
            Core.raise(TypeError("Expected a string"));
        }

        _writeBuffers.append(data);
        _writeSize += length;
        if (_writeSize > _bufferSize) {
            this._flushWrite();
        }

        return length;
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
            _raw.close();
            this.closed = true;
        }

        return 0;
    }

    function
    detach (
        )

    /*++

    Routine Description:

        This routine returns the raw file I/O object this object is buffering.
        Afterwards, this object is left in an unusable state.

    Arguments:

        None.

    Return Value:

        Returns the raw object.

    --*/

    {

        var raw = _raw;

        this.flush();
        _raw = null;
        return raw;
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

        return _raw.fileno();
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

        return _raw.isatty();
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

        return _raw.isReadable();
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

        return _raw.isSeekable();
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

        return _raw.isWritable();
    }

    //
    // Internal functions
    //

    function
    _flushRead (
        )

    /*++

    Routine Description:

        This routine closes and flushes a stream and seeks back over the
        buffered contents.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var readLength;

        if (_readable) {
            readLength = _readBuffer.length() - _readOffset;
            if (readLength) {
                _position = _raw.seek(-readLength, IO_SEEK_CUR);
                _readBuffer = "";
                _readOffset = 0;
            }
        }

        return;
    }

    function
    _flushWrite (
        )

    /*++

    Routine Description:

        This routine flushes the dirty write buffer.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var data = "".join(_writeBuffers);
        var length = data.length();
        var written;

        _writeBuffers = [];
        _writeSize = 0;
        while (length) {
            written = _raw.write(data);
            if (written <= 0) {
                Core.raise(IoError("Unable to write"));
            }

            _position += written;
            data = data[written...-1];
            length = data.length();
        }

        return;
    }
}

//
// --------------------------------------------------------- Internal Functions
//

