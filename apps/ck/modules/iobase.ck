/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    iobase.ck

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
// Define the default buffer size.
//

var DEFAULT_BUFFER_SIZE = 4096;

//
// Define the seek dispositions.
//

var IO_SEEK_SET = 0;
var IO_SEEK_CUR = 1;
var IO_SEEK_END = 2;

//
// ------------------------------------------------------------------ Functions
//

class IoUnsupportedException is Exception {}
class IoError is Exception {}

//
// IoBase is the abstract base class for all I/O classes.
//

class IoBase {

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

        this.close();
        return;
    }

    function
    iterate (
        context
        )

    /*++

    Routine Description:

        This routine implements the iterator protocol.

    Arguments:

        context - Supplies the iterator context. For the first iteration, null
            will be passed. On subsequent iterations, the previous return value
            will be passed.

    Return Value:

        Returns the next iterator context. Returns null when the iteration is
        complete.

    --*/

    {

        var value = this.readline(-1);

        if (value == "") {
            return null;
        }

        return value;
    }

    function
    iteratorValue (
        context
        )

    /*++

    Routine Description:

        This routine is part of the iterator protocol. It returns the value for
        the current iteration.

    Arguments:

        context - Supplies the iterator context returned during the iterate
            function.

    Return Value:

        Returns the next iterator value. In this case, that is just the context.

    --*/

    {

        return context;
    }

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

        Core.raise(IoUnsupportedException("Function not supported"));
    }

    function
    isClosed (
        )

    /*++

    Routine Description:

        This routine determines whether or not the given stream is closed.

    Arguments:

        None.

    Return Value:

        true if the stream is already closed.

        false if the stream is still open.

    --*/

    {

        return this.closed;
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

        Core.raise(IoUnsupportedException("Function not supported"));
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

        if (this.isClosed()) {
            Core.raise(ValueError("I/O operation on closed file"));
        }

        return null;
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

        Core.raise(IoUnsupportedException("Function not supported"));
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

        return false;
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
        var index = 0;
        var result = [];

        while (true) {
            data = this.read(1);
            if (data.length() == 0) {
                break;
            }

            result.append(data);
            index++;
            if (data == "\n") {
                break;
            }

            //
            // Occasionally coalesce the list into a bigger string with a
            // smaller list.
            //

            if (index >= 1024) {
                result = ["".joinList(result)];
                index = 0;
            }
        }

        return "".joinList(result);
    }

    function
    readlines (
        limit
        )

    /*++

    Routine Description:

        This routine reads multiple lines from a stream.

    Arguments:

        limit - Supplies the maximum number of bytes to read. Supply -1 for no
            limit.

    Return Value:

        Returns a list of lines read from the file.

    --*/

    {

        var line;
        var result = [];
        var size = 0;

        line = this.readline();
        if (limit <= 0) {
            while (line != "") {
                result.append(line);
                line = this.readline();
            }

        } else {
            while ((line != "") && (size < limit)) {
                result.append(line);
                size += line.length();
                line = this.readline();
            }
        }

        return result;
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

        Core.raise(IoUnsupportedException("Function not supported"));
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

        return false;
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

        Core.raise(IoUnsupportedException("Function not supported"));
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

        return false;
    }

    function
    writelines (
        lines
        )

    /*++

    Routine Description:

        This routine writes a list of lines to the stream. Line separators are
        not added between the lines.

    Arguments:

        lines - Supplies a list of lines to write.

    Return Value:

        None.

    --*/

    {

        for (line in lines) {
            this.write(line);
        }

        return;
    }
}

//
// Raw I/O base is the base class for a stream that can perform unbuffered I/O.
//

class RawIoBase is IoBase {

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

        Core.raise(IoUnsupportedException("Function not supported"));
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
        var result = [];

        while (true) {
            data = this.read(DEFAULT_BUFFER_SIZE);
            if (data == "") {
                break;
            }

            result.append(data);
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

        Core.raise(IoUnsupportedException("Function not supported"));
    }
}

//
// Buffered I/O base is the base class for a buffered I/O stream.
//

class BufferedIoBase is IoBase {

    function
    detach (
        )

    /*++

    Routine Description:

        This routine returns the underlying raw stream. After this call, the
        buffered IO object is left in an undefined state.

    Arguments:

        None.

    Return Value:

        Returns the raw IO base object, or raises an exception.

    --*/

    {

        Core.raise(IoUnsupportedException("Function not supported"));
    }

    function
    read (
        size
        )

    /*++

    Routine Description:

        This routine performs a read, attempting to satisfy all requested bytes.
        For interactive raw streams, at most one raw read will be issued, and
        a short read result does not necessarily imply end-of-file.

    Arguments:

        size - Supplies the amount to read. If -1 is supplied, then data is
            read until the end of file is encountered.

    Return Value:

        Returns the bytes read. If the empty string is returned and more than
        zero bytes were requested, then this is end-of-file.

    --*/

    {

        Core.raise(IoUnsupportedException("Function not supported"));
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

        Core.raise(IoUnsupportedException("Function not supported"));
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

        Returns the number of bytes written, which is always equal to the
        length of data (or an exception will be raised).

    --*/

    {

        Core.raise(IoUnsupportedException("Function not supported"));
    }
}

//
// --------------------------------------------------------- Internal Functions
//

