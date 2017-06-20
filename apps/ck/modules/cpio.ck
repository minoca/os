/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cpio.ck

Abstract:

    This module implements support for working with CPIO archives.

Author:

    Evan Green 7-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from iobase import IoError;
from io import IO_SEEK_SET, IO_SEEK_CUR, IO_SEEK_END, open;
import os;
from time import Time;

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the CPIO mode bits, in octal.
//

var CPIO_MODE_TYPE_MASK = 0170000;
var CPIO_MODE_TYPE_SOCKET = 0140000;
var CPIO_MODE_TYPE_SYMLINK = 0120000;
var CPIO_MODE_TYPE_FILE = 0100000;
var CPIO_MODE_TYPE_BLOCKDEV = 0060000;
var CPIO_MODE_TYPE_DIRECTORY = 0040000;
var CPIO_MODE_TYPE_CHARDEV = 0020000;
var CPIO_MODE_TYPE_FIFO = 0010000;
var CPIO_MODE_SUID = 0004000;
var CPIO_MODE_SGID = 0002000;
var CPIO_MODE_STICKY = 0001000;
var CPIO_MODE_PERMISSIONS = 00000777;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define a table that helps convert a mode integer into a ls-style mode string.
//

var _fileModeTable = [
    [
        [CPIO_MODE_TYPE_SOCKET, "s"],
        [CPIO_MODE_TYPE_SYMLINK, "l"],
        [CPIO_MODE_TYPE_FILE, "-"],
        [CPIO_MODE_TYPE_BLOCKDEV, "b"],
        [CPIO_MODE_TYPE_DIRECTORY, "d"],
        [CPIO_MODE_TYPE_CHARDEV, "c"],
        [CPIO_MODE_TYPE_FIFO, "p"]
    ],

    [[0400, "r"],],
    [[0200, "w"],],

    [
        [0100 | CPIO_MODE_SUID, "s"],
        [CPIO_MODE_SUID, "S"],
        [0100, "x"]
    ],

    [[0040, "r"],],
    [[0020, "w"],],

    [
        [0010 | CPIO_MODE_SGID, "s"],
        [CPIO_MODE_SGID, "S"],
        [0010, "x"]
    ],

    [[0004, "r"],],
    [[0002, "w"],],

    [
        [0001 | CPIO_MODE_STICKY, "t"],
        [CPIO_MODE_STICKY, "T"],
        [0001, "x"]
    ]
];

//
// Define a table that helps get between OS file types and CPIO file types.
//

var _osFileTypes = {
    os.S_IFREG: CPIO_MODE_TYPE_FILE,
    os.S_IFDIR: CPIO_MODE_TYPE_DIRECTORY,
    os.S_IFBLK: CPIO_MODE_TYPE_BLOCKDEV,
    os.S_IFCHR: CPIO_MODE_TYPE_CHARDEV,
    os.S_IFLNK: CPIO_MODE_TYPE_SYMLINK,
    os.S_IFIFO: CPIO_MODE_TYPE_FIFO,
    os.S_IFSOCK: CPIO_MODE_TYPE_SOCKET
};

//
// ------------------------------------------------------------------ Functions
//

class CpioFormatError is Exception {}
class CpioEofError is Exception {}

//
// Define the CPIO member class, which describes a single member of a CPIO
// archive.
//

class CpioMember {
    static
    function
    fromFile (
        file
        )

    /*++

    Routine Description:

        This routine creates a new CpioMember objects from the input file.

    Arguments:

        file - Supplies a file-like object to read from.

    Return Value:

        Returns the initialized object.

    --*/

    {

        var format;
        var magic = file.read(2);
        var member = CpioMember();

        if (magic.length() == 0) {
            Core.raise(CpioEofError());
        }

        //
        // Look at the magic field to determine the format.
        //

        if (magic == "\xC7\x71") {
            format = "bin";

        } else if (magic == "07") {
            magic += file.read(4);
            if (magic.length() < 6) {
                Core.raise(CpioFormatError("Magic truncated"));
            }

            if (magic == "070707") {
                format = "odc";

            } else if (magic == "070701") {
                format = "newc";

            } else if (magic == "070702") {
                format = "crc";
            }
        }

        if (format == "bin") {
            member._readBinaryHeader(file, format);

        } else if ((format == "odc") || (format == "newc") ||
                   (format == "crc")) {

            member._readAsciiHeader(file, format);

        } else {
            Core.raise(CpioFormatError("Invalid CPIO header magic: \"%s\"" %
                                       magic));
        }

        //
        // If it's a symbolic link, read the link target now. Attribute that
        // read data to the header so the caller knows where the current file
        // position is.
        //

        if (member.isLink()) {
            member.link = file.read(member.size);
            if (member.link.length() != member.size) {
                Core.raise(CpioFormatError("Data truncated"));
            }

            member.headerSize += member.size;
        }

        return member;
    }

    function
    __init (
        )

    /*++

    Routine Description:

        This routine instantiates a new CpioMember class with an empty file
        name.

    Arguments:

        None.

    Return Value:

        Returns the initialized object.

    --*/

    {

        return this.__init("");
    }

    function
    __init (
        name
        )

    /*++

    Routine Description:

        This routine instantiates a new CpioMember class with the given
        archive member name.

    Arguments:

        name - Supplies the path of the member within the archive.

    Return Value:

        Returns the initialized object.

    --*/

    {

        this.name = name;
        this.devMajor = 0;
        this.devMinor = 0;
        this.mode = CPIO_MODE_TYPE_FILE;
        this.uid = 0;
        this.gid = 0;
        this.nlink = 1;
        this.rdevMajor = 0;
        this.rdevMinor = 0;
        this.mtime = Time.now().timestamp();
        this.size = 0;
        this.check = -1;
        return this;
    }

    function
    ls (
        verbose
        )

    /*++

    Routine Description:

        This routine returns a string containing a listing of the archive
        member.

    Arguments:

        verbose - Supplies a boolean indicating whether to print just the
            name, or a more ls -l type listing.

    Return Value:

        Returns a string describing the object.

    --*/

    {

        var bit;
        var found;
        var mode = this.mode;
        var modeString = "";
        var result = "";
        var modificationTime = Time.fromTimestamp(this.mtime, 0, null);

        if (!verbose) {
            return this.name;
        }

        if (verbose > 1) {
            result = "%8d" % [this.inode];
        }

        for (table in _fileModeTable) {
            found = false;
            for (element in table) {
                bit = element[0];
                if ((bit & mode) == bit) {
                    modeString += element[1];
                    found = true;
                    break;
                }
            }

            if (!found) {
                modeString += "-";
            }
        }

        result += modeString +
                  " %2d %5d %5d " % [this.nlink, this.uid, this.gid];

        if (this.isChr() || this.isBlk()) {
            result += "%8s" % ("%d,%d" % [this.devMajor, this.devMinor]);

        } else {
            result += "%8d" % this.size;
        }

        result += modificationTime.strftime(" %F %T ");
        result += this.name;
        return result;
    }

    function
    isFile (
        )

    /*++

    Routine Description:

        This routine returns true if the given info object is a regular file.

    Arguments:

        None.

    Return Value:

        true if the object is a regular file.

        false if the object is not a regular file.

    --*/

    {

        if ((this.mode & CPIO_MODE_TYPE_MASK) == CPIO_MODE_TYPE_FILE) {
            return true;
        }

        return false;
    }

    function
    isDir (
        )

    /*++

    Routine Description:

        This routine returns true if the given info object is a directory.

    Arguments:

        None.

    Return Value:

        true if the object is a directory.

        false if the object is not a directory.

    --*/

    {

        if ((this.mode & CPIO_MODE_TYPE_MASK) == CPIO_MODE_TYPE_DIRECTORY) {
            return true;
        }

        return false;
    }

    function
    isLink (
        )

    /*++

    Routine Description:

        This routine returns true if the given info object is a symbolic link.

    Arguments:

        None.

    Return Value:

        true if the object is a symbolic link.

        false if the object is not a symbolic link.

    --*/

    {

        if ((this.mode & CPIO_MODE_TYPE_MASK) == CPIO_MODE_TYPE_SYMLINK) {
            return true;
        }

        return false;
    }

    function
    isChr (
        )

    /*++

    Routine Description:

        This routine returns true if the given info object is a character
        device.

    Arguments:

        None.

    Return Value:

        true if the object is a character device.

        false if the object is not a character device.

    --*/

    {

        if ((this.mode & CPIO_MODE_TYPE_MASK) == CPIO_MODE_TYPE_CHARDEV) {
            return true;
        }

        return false;
    }

    function
    isBlk (
        )

    /*++

    Routine Description:

        This routine returns true if the given info object is a block
        device.

    Arguments:

        None.

    Return Value:

        true if the object is a block device.

        false if the object is not a block device.

    --*/

    {

        if ((this.mode & CPIO_MODE_TYPE_MASK) == CPIO_MODE_TYPE_BLOCKDEV) {
            return true;
        }

        return false;
    }

    function
    isFifo (
        )

    /*++

    Routine Description:

        This routine returns true if the given info object is a FIFO.

    Arguments:

        None.

    Return Value:

        true if the object is a FIFO.

        false if the object is not a FIFO.

    --*/

    {

        if ((this.mode & CPIO_MODE_TYPE_MASK) == CPIO_MODE_TYPE_FIFO) {
            return true;
        }

        return false;
    }

    function
    isDev (
        )

    /*++

    Routine Description:

        This routine returns true if the given info object is a FIFO, block
        device, or character device.

    Arguments:

        None.

    Return Value:

        true if the object is a special device of some kind.

        false if the object is a regular file, directory, or symbolic link.

    --*/

    {

        var type = this.mode & CPIO_MODE_TYPE_MASK;

        if ((type == CPIO_MODE_TYPE_FIFO) ||
            (type == CPIO_MODE_TYPE_BLOCKDEV) ||
            (type == CPIO_MODE_TYPE_CHARDEV)) {

            return true;
        }

        return false;
    }

    function
    write (
        inFile,
        outFile
        )

    /*++

    Routine Description:

        This routine writes the given member out to an archive.

    Arguments:

        inFile - Supplies an optional file object to write out as the data
            contents. This only applies to regular files.

        outFile - Supplies the file object to write the data out to.

    Return Value:

        None on success.

        An exception is raised on error.

    --*/

    {

        var format = this.format;

        if (format == "bin") {
            this._writeBinaryMember(inFile, outFile);

        } else if (format == "odc") {
            this._writeOldAsciiMember(inFile, outFile);

        } else {
            this._writeNewAsciiMember(format, inFile, outFile);
        }

        return;
    }

    function
    _readBinaryHeader (
        file,
        format
        )

    /*++

    Routine Description:

        This routine instantiates the member with the values from a binary
        header.

    Arguments:

        file - Supplies the file object, positioned just after the magic field.

        format - Supplies the format, which should just be "bin".

    Return Value:

        None. The fields are set in the object.

    --*/

    {

        var bytes = [];
        var data = file.read(24);
        var nameSize;
        var totalSize;

        if (data.length() != 24) {
            Core.raise(CpioFormatError("Header truncated"));
        }

        //
        // Convert all the bytes to numbers.
        //

        for (index in 0..24) {
            bytes.append(data.byteAt(index));
        }

        this.format = format;
        this.devMajor = 0;
        this.devMinor = bytes[0] + (bytes[1] << 8);
        this.inode = bytes[2] + (bytes[3] << 8);
        this.mode = bytes[4] + (bytes[5] << 8);
        this.uid = bytes[6] + (bytes[7] << 8);
        this.gid = bytes[8] + (bytes[9] << 8);
        this.nlink = bytes[10] + (bytes[11] << 8);
        this.rdevMajor = 0;
        this.rdevMinor = bytes[12] + (bytes[13] << 8);

        //
        // For the modification time, the most significant 16 bits are stored
        // first. Each 16-bit word is stored in native endian order.
        //

        this.mtime = ((bytes[14] + (bytes[15] << 8)) << 16) +
                     bytes[16] + (bytes[17] << 8);

        nameSize = bytes[18] + (bytes[19] << 8);
        this.size = ((bytes[20] + (bytes[21] << 8)) << 16) +
                    bytes[22] + (bytes[23] << 8);

        //
        // The name size includes a null terminating byte. If the name size is
        // odd, an extra byte of padding is added.
        //

        this.headerSize = 26 + nameSize;
        this.name = file.read(nameSize)[0..-1];
        if ((nameSize & 0x1) != 0) {
            file.read(1);
            this.headerSize += 1;
        }

        totalSize = this.headerSize + this.size;
        if ((totalSize & 0x1) != 0) {
            totalSize += 1;
        }

        this.archiveSize = totalSize;
        this.check = -1;
        return;
    }

    function
    _readAsciiHeader (
        file,
        format
        )

    /*++

    Routine Description:

        This routine instantiates the member with the values from an ASCII
        header.

    Arguments:

        file - Supplies the file object, positioned just after the magic field.

        format - Supplies the format, which should just be either "odc", "newc",
            or "crc".

    Return Value:

        None. The fields are set in the object.

    --*/

    {

        var check;
        var devMajor = "0";
        var devMinor;
        var fileSize;
        var gid;
        var header;
        var headerSize;
        var inode;
        var mode;
        var mtime;
        var nameSize;
        var nlink;
        var prefix;
        var rdevMajor = "0";
        var rdevMinor;
        var remainder;
        var totalSize;
        var uid;

        if (format == "odc") {
            prefix = "0";
            headerSize = 76;
            header = file.read(70);
            if (header.length() != 70) {
                Core.raise(CpioFormatError("Header truncated"));
            }

            devMinor = header[0..6];
            inode = header[6..12];
            mode = header[12..18];
            uid = header[18..24];
            gid = header[24..30];
            nlink = header[30..36];
            rdevMinor = header[36..42];
            mtime = header[42..53];
            nameSize = header[53..59];
            fileSize = header[59..70];
            nameSize = Int.fromString(prefix + nameSize);
            headerSize += nameSize;

            //
            // The old format contains no padding.
            //

            remainder = 0;

        } else {
            prefix = "0x";
            headerSize = 110;
            header = file.read(104);
            if (header.length() != 104) {
                Core.raise(CpioFormatError("Header truncated"));
            }

            inode = header[0..8];
            mode = header[8..16];
            uid = header[16..24];
            gid = header[24..32];
            nlink = header[32..40];
            mtime = header[40..48];
            fileSize = header[48..56];
            devMajor = header[56..64];
            devMinor = header[64..72];
            rdevMajor = header[72..80];
            rdevMinor = header[80..88];
            nameSize = header[88..96];
            check = header[96..104];
            nameSize = Int.fromString(prefix + nameSize);
            headerSize += nameSize;

            //
            // The new ASCII format's name is padded with null bytes to reach
            // a multiple of 4.
            //

            remainder = headerSize % 4;
        }

        this.format = format;
        this.inode = Int.fromString(prefix + inode);
        this.mode = Int.fromString(prefix + mode);
        this.uid = Int.fromString(prefix + uid);
        this.gid = Int.fromString(prefix + gid);
        this.nlink = Int.fromString(prefix + nlink);
        this.mtime = Int.fromString(prefix + mtime);
        this.size = Int.fromString(prefix + fileSize);
        this.devMajor = Int.fromString(prefix + devMajor);
        this.devMinor = Int.fromString(prefix + devMinor);
        this.rdevMajor = Int.fromString(prefix + rdevMajor);
        this.rdevMinor = Int.fromString(prefix + rdevMinor);
        if ((check != null) && (format == "crc")) {
            this.check = Int.fromString(prefix + check);

        } else {
            this.check = -1;
        }

        this.name = file.read(nameSize)[0..-1];
        if (this.name.length() != nameSize - 1) {
            Core.raise(CpioFormatError("File name truncated"));
        }

        //
        // The archive is padded with null bytes such that the fixed header
        // plus the name size plus the padding bytes is a multiple of 4.
        //

        if (remainder != 0) {
            remainder = 4 - remainder;
            file.read(remainder);
            headerSize += remainder;
        }

        totalSize = headerSize + this.size;

        //
        // The new ASCII format's file data is also padded out to a multiple
        // of 4.
        //

        if (format != "odc") {
            if ((totalSize % 4) != 0) {
                totalSize += 4 - (totalSize % 4);
            }
        }

        this.headerSize = headerSize;
        this.archiveSize = totalSize;
        return;
    }

    function
    _writeBinaryMember (
        inFile,
        outFile
        )

    /*++

    Routine Description:

        This routine writes out a member in binary format.

    Arguments:

        inFile - Supplies the file to read from.

        outFile - Supplies the file to write out to.

    Return Value:

        Returns the binary header string corresponding to this instance.

    --*/

    {

        var link;
        var max = 0xFFFF;
        var max2 = 0xFFFFFFFF;
        var nameSize = this.name.length() + 1;
        var result;

        if ((this.devMajor != 0) ||
            (this.devMinor > max) ||
            (this.inode > max) || (this.mode > max) || (this.uid > max) ||
            (this.gid > max) || (this.nlink > max) || (this.rdevMajor != 0) ||
            (this.rdevMinor > max) || (this.mtime > max2) ||
            (nameSize > max) || (this.size > max2)) {

            Core.raise(CpioFormatError("Header value too big for format"));
        }

        result = ["\xC7",
                  "\x71",
                  String.fromByte(this.devMinor & 0xFF),
                  String.fromByte((this.devMinor >> 8) & 0xFF),
                  String.fromByte(this.inode & 0xFF),
                  String.fromByte((this.inode >> 8) & 0xFF),
                  String.fromByte(this.mode & 0xFF),
                  String.fromByte((this.mode >> 8) & 0xFF),
                  String.fromByte(this.uid & 0xFF),
                  String.fromByte((this.uid >> 8) & 0xFF),
                  String.fromByte(this.gid & 0xFF),
                  String.fromByte((this.gid >> 8) & 0xFF),
                  String.fromByte(this.nlink & 0xFF),
                  String.fromByte((this.nlink >> 8) & 0xFF),
                  String.fromByte(this.rdevMajor & 0xFF),
                  String.fromByte((this.rdevMinor >> 8) & 0xFF),
                  String.fromByte((this.mtime >> 16) & 0xFF),
                  String.fromByte((this.mtime >> 24) & 0xFF),
                  String.fromByte(this.mtime & 0xFF),
                  String.fromByte((this.mtime >> 8) & 0xFF),
                  String.fromByte(nameSize & 0xFF),
                  String.fromByte((nameSize >> 8) & 0xFF),
                  String.fromByte((this.size >> 16) & 0xFF),
                  String.fromByte((this.size >> 24) & 0xFF),
                  String.fromByte(this.size & 0xFF),
                  String.fromByte((this.size >> 8) & 0xFF),
                  this.name,
                  "\0"];

        result = "".join(result);
        if ((result.length() & 0x1) != 0) {
            result += "\0";
        }

        this.headerSize = result.length();
        outFile.write(result);
        if ((this.mode & CPIO_MODE_TYPE_MASK) == CPIO_MODE_TYPE_SYMLINK) {
            link = this.link;
            if (link.length() != this.size) {
                Core.raise(CpioFormatError("Member length %d does not match "
                                           "size of link '%s'" %
                                           [this.size, link]));
            }

            outFile.write(link);
            this.headerSize += link.length();
            this.archiveSize = this.headerSize;

        } else if ((this.mode & CPIO_MODE_TYPE_MASK) == CPIO_MODE_TYPE_FILE) {
            this._writeFileContents(inFile, outFile, this.size);
            this.archiveSize = this.headerSize + this.size;

        } else {
            if (this.size != 0) {
                Core.raise(CpioFormatError("Special device has non-zero size"));
            }

            this.archiveSize = this.headerSize;
        }

        if ((this.size & 0x1) != 0) {
            outFile.write("\0");
            this.archiveSize += 1;
        }

        return;
    }

    function
    _writeOldAsciiMember (
        inFile,
        outFile
        )

    /*++

    Routine Description:

        This routine writes out a member in ODC format.

    Arguments:

        inFile - Supplies the file to read from.

        outFile - Supplies the file to write out to.

    Return Value:

        Returns the binary header string corresponding to this instance.

    --*/

    {

        var link;
        var max = 0777777;
        var max2 = 077777777777;
        var nameSize = this.name.length() + 1;
        var result;

        if ((this.devMajor != 0) || (this.devMinor > max) ||
            (this.inode > max) || (this.mode > max) || (this.uid > max) ||
            (this.gid > max) || (this.nlink > max) || (this.rdevMajor != 0) ||
            (this.rdevMinor > max) || (this.mtime > max2) ||
            (nameSize > max) || (this.size > max2)) {

            Core.raise(CpioFormatError("Header value too big for format"));
        }

        result = "%06o%06o%06o%06o%06o%06o%06o%06o%011o%06o%011o" % [
                 070707,
                 this.devMinor,
                 this.inode,
                 this.mode,
                 this.uid,
                 this.gid,
                 this.nlink,
                 this.rdevMinor,
                 this.mtime,
                 nameSize,
                 this.size];

        result += this.name + "\0";
        outFile.write(result);
        this.headerSize = result.length();
        if ((this.mode & CPIO_MODE_TYPE_MASK) == CPIO_MODE_TYPE_SYMLINK) {
            link = this.link;
            if (link.length() != this.size) {
                Core.raise(CpioFormatError("Member length %d does not match "
                                           "size of link '%s'" %
                                           [this.size, link]));
            }

            outFile.write(link);
            this.headerSize += link.length();
            this.archiveSize = this.headerSize;

        } else if ((this.mode & CPIO_MODE_TYPE_MASK) == CPIO_MODE_TYPE_FILE) {
            this._writeFileContents(inFile, outFile, this.size);
            this.archiveSize = this.headerSize + this.size;

        } else {
            if (this.size != 0) {
                Core.raise(CpioFormatError("Special device has non-zero size"));
            }

            this.archiveSize = this.headerSize;
        }

        return;
    }

    function
    _writeNewAsciiMember (
        format,
        inFile,
        outFile
        )

    /*++

    Routine Description:

        This routine writes out a member in newc or crc format.

    Arguments:

        format - Supplies the desired format. Valid values are "newc" and "crc".

        inFile - Supplies the file to read from.

        outFile - Supplies the file to write out to.

    Return Value:

        Returns the binary header string corresponding to this instance.

    --*/

    {

        var check = this.check;
        var link;
        var magic = 070701;
        var max = 0xFFFFFFFF;
        var nameSize = this.name.length() + 1;
        var result;
        var size;

        if (format == "crc") {
            magic = 070702;

            //
            // Compute the sum of the data bytes if needed.
            //

            if (this.check < 0) {
                check = 0;
                if ((this.mode & CPIO_MODE_TYPE_MASK) ==
                    CPIO_MODE_TYPE_SYMLINK) {

                    link = this.link;
                    check = this._sumData(link);
                }

                if ((this.mode & CPIO_MODE_TYPE_MASK) == CPIO_MODE_TYPE_FILE) {
                    check = this._sumFile(inFile, this.size);
                }

                check &= 0xFFFFFFFF;
            }

        } else if (format == "newc") {
            check = 0;

        } else {
            Core.raise(CpioFormatError("Unknown format %s" % format));
        }

        if ((this.devMajor > max) || (this.devMinor > max) ||
            (this.inode > max) || (this.mode > max) || (this.uid > max) ||
            (this.gid > max) || (this.nlink > max) || (this.rdevMajor > max) ||
            (this.rdevMinor > max) || (this.mtime > max) ||
            (nameSize > max) || (this.size > max) || (check > max)) {

            Core.raise(CpioFormatError("Header value too big for format"));
        }

        result = "%06o%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x" % [
                 magic,
                 this.inode,
                 this.mode,
                 this.uid,
                 this.gid,
                 this.nlink,
                 this.mtime,
                 this.size,
                 this.devMajor,
                 this.devMinor,
                 this.rdevMajor,
                 this.rdevMinor,
                 nameSize,
                 check];

        result += this.name + "\0";
        outFile.write(result);

        //
        // Align the header up to 4 bytes.
        //

        size = result.length();
        while ((size & 0x3) != 0) {
            outFile.write("\0");
            size += 1;
        }

        this.headerSize = size;

        //
        // Write out the data contents.
        //

        if ((this.mode & CPIO_MODE_TYPE_MASK) == CPIO_MODE_TYPE_SYMLINK) {
            link = this.link;
            if (link.length() != this.size) {
                Core.raise(CpioFormatError("Member length %d does not match "
                                           "size of link '%s'" %
                                           [this.size, link]));
            }

            outFile.write(link);
            this.headerSize += this.size;
            this.archiveSize = this.headerSize;

        } else if ((this.mode & CPIO_MODE_TYPE_MASK) == CPIO_MODE_TYPE_FILE) {
            this._writeFileContents(inFile, outFile, this.size);
            this.archiveSize = this.headerSize + this.size;

        } else {
            if (this.size != 0) {
                Core.raise(CpioFormatError("Special device has non-zero size"));
            }

            this.archiveSize = this.headerSize;
        }

        //
        // Align the data out to four.
        //

        size = this.size;
        while ((size & 0x3) != 0) {
            outFile.write("\0");
            size += 1;
            this.archiveSize += 1;
        }

        return;
    }

    function
    _writeFileContents (
        inFile,
        outFile,
        size
        )

    /*++

    Routine Description:

        This routine writes the file contents out to the archive.

    Arguments:

        inFile - Supplies the file object to read from..

        outFile - Supplies the file object to write the data out to.

        size - Supplies the number of bytes to write.

    Return Value:

        None on success.

        An exception is raised on error.

    --*/

    {

        var chunk;
        var chunkSize;
        var maxChunkSize = 131072;

        while (size != 0) {
            chunkSize = (size < maxChunkSize) ? size : maxChunkSize;
            chunk = inFile.read(chunkSize);
            if (chunk.length() != chunkSize) {
                Core.raise(CpioEofError("Input file ended early"));
            }

            outFile.write(chunk);
            size -= chunkSize;
        }

        return;
    }

    function
    _sumFile (
        file,
        size
        )

    /*++

    Routine Description:

        This routine returns the sum of all the bytes in the file. It will
        return the file position back to where it was.

    Arguments:

        file - Supplies the file object to sum.

        size - Supplies the number of bytes to read from the file object.

    Return Value:

        Returns the sum of all the bytes in the file.

    --*/

    {

        var chunk;
        var chunkSize;
        var maxChunkSize = 131072;
        var start = file.tell();
        var sum = 0;

        while (size != 0) {
            chunkSize = (size < maxChunkSize) ? size : maxChunkSize;
            chunk = file.read(chunkSize);
            if (chunk.length() != chunkSize) {
                Core.raise(CpioEofError("Input file ended early"));
            }

            sum += this._sumData(chunk);
            size -= chunkSize;
        }

        file.seek(start, IO_SEEK_SET);
        return sum;
    }

    function
    _sumData (
        data
        )

    /*++

    Routine Description:

        This routine returns the sum of all the bytes in the data.

    Arguments:

        data - Supplies the data to sum.

    Return Value:

        Returns the sum of all the bytes in the data.

    --*/

    {

        var sum = 0;

        for (index in 0..data.length()) {
            sum += data.byteAt(index);
        }

        return sum;
    }
}

//
// This class implements a file-like object that comes from the archive.
//

class ArchiveMemberFile {
    var _archive;
    var _file;
    var _member;
    var _startOffset;
    var _endOffset;
    var _currentOffset;

    function
    __init (
        archive,
        file,
        member
        )

    /*++

    Routine Description:

        This routine creates a new archive member file instance.

    Arguments:

        archive - Supplies the archive this member belongs to.

        file - Supplies the archive's underlying file object.

        member - Supplies the member to create a file object for.

    Return Value:

        Returns the initialized object.

    --*/

    {

        _archive = archive;
        _file = file;
        _member = member;
        _startOffset = member.offset + member.headerSize;
        _endOffset = _startOffset + member.size;
        _currentOffset = _startOffset;
        this.mode = "r";
        this.name = member.name;
        this.closed = false;
        return this;
    }

    function
    close (
        )

    /*++

    Routine Description:

        This routine closes the file.

    Arguments:

        None.

    Return Value:

        0 always.

    --*/

    {

        _file = null;
        this.closed = true;
        return 0;
    }

    function
    read (
        size
        )

    /*++

    Routine Description:

        This routine reads from the archive member.

    Arguments:

        size - Supplies the size to read. Supply -1 to read the rest of the
            file.

    Return Value:

        Returns the read data, which may be shorter than requested.

        Returns an empty string on EOF.

    --*/

    {

        var result;

        if (_file == null) {
            Core.raise(IoError("I/O operation on closed file"));
        }

        if ((size < 0) ||
            (size > _endOffset - _currentOffset)) {

            size = _endOffset - _currentOffset;
        }

        if (size == 0) {
            return "";
        }

        this._syncOffset();
        result = _file.read(size);
        if (result.length() < size) {
            Core.raise(IoError("Read error"));
        }

        _currentOffset += result.length();
        _archive._setCurrentOffset(_currentOffset);
        return result;
    }

    function
    readline (
        size
        )

    /*++

    Routine Description:

        This routine reads a line from the archive.

    Arguments:

        size - Supplies the maximum number of bytes to read. Supply -1 to
            read until the new line or end of file.

    Return Value:

        Returns the read data, which may be shorter than requested.

        Returns an empty string on EOF.

    --*/

    {

        var result;

        if (_file == null) {
            Core.raise(IoError("I/O operation on closed file"));
        }

        if ((size < 0) ||
            (size > _endOffset - _currentOffset)) {

            size = _endOffset - _currentOffset;
        }

        if (size == 0) {
            return "";
        }

        this._syncOffset();
        result = _file.readline(size);
        _currentOffset += result.length();
        _archive._setCurrentOffset(_currentOffset);
        return result;
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
    tell (
        )

    /*++

    Routine Description:

        This routine returns the current position within the file.

    Arguments:

        None.

    Return Value:

        Returns the current byte offset within the file.

    --*/

    {

        if (_file == null) {
            Core.raise(IoError("I/O operation on closed file"));
        }

        return _currentOffset - _startOffset;
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

        var newOffset;

        if (_file == null) {
            Core.raise(IoError("I/O operation on closed file"));
        }

        if (whence == IO_SEEK_SET) {
            newOffset = _startOffset + offset;

        } else if (whence == IO_SEEK_CUR) {
            newOffset = _currentOffset + offset;

        } else if (whence == IO_SEEK_END) {
            newOffset = _endOffset + offset;

        } else {
            Core.raise(
                     IoError("Invalid seek disposition: %s" % whence.__str()));
        }

        if (newOffset < _startOffset) {
            newOffset = _startOffset;

        } else if (newOffset > _endOffset) {
            newOffset = _endOffset;
        }

        _currentOffset = newOffset;
        this._syncOffset();
        return _currentOffset - _startOffset;
    }

    function
    iterate (
        iterator
        )

    /*++

    Routine Description:

        This routine iterates over the lines in a file.

    Arguments:

        iterator - Supplies null initially, or the previous iterator

    Return Value:

        Returns the new absolute position within the file.

    --*/

    {

        var value = this.readline();

        if (value == "") {
            return null;
        }

        return value;
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
            Core.raise(IoError("I/O operation on closed file"));
        }

        return true;
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
            Core.raise(IoError("I/O operation on closed file"));
        }

        return false;
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

        if (_file == null) {
            Core.raise(IoError("I/O operation on closed file"));
        }

        return _file.isSeekable();
    }

    function
    _syncOffset (
        )

    /*++

    Routine Description:

        This routine synchronizes the file offset with the archive offset.
        Upon return, ensures that the underlying file object is at the
        correct offset and that the archive knows about any adjustments made.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        if (_archive._getCurrentOffset() == _currentOffset) {
            return;
        }

        _archive._setCurrentOffset(_file.seek(_currentOffset, IO_SEEK_SET));
        return;
    }

}

class CpioArchive {
    var _file;
    var _shouldClose;
    var _isReadable;
    var _isWritable;
    var _isSeekable;
    var _startOffset;
    var _currentOffset;
    var _nextOffset;
    var _members;
    var _memberDict;
    var _fullyEnumerated;
    var _trailerFound;
    var _trailerOffset;
    var _writeTrailer;
    var _nextInode;

    function
    __init (
        )

    /*++

    Routine Description:

        This routine instantiates a new CpioArchive class with null parameters.

    Arguments:

        None.

    Return Value:

        Returns the initialized object.

    --*/

    {

        Core.raise(ValueError("Use the other constructor"));
        return null;
    }

    function
    __init (
        file,
        mode
        )

    /*++

    Routine Description:

        This routine instantiates a new CpioArchiive class with the given file.

    Arguments:

        file - Supplies either a string, indicating the path of an archive to
            open, or a file object to be used directly.

        mode - Supplies the mode to open the archive with. Valid values are
            "r", "w", and "a".

    Return Value:

        Returns the initialized object.

    --*/

    {

        if ((file is String) || (file is Int)) {
            _file = open(file, mode + "b");
            _shouldClose = true;

        } else {
            _file = file;
            _shouldClose = false;
        }

        _isReadable = _file.isReadable();
        _isWritable = _file.isWritable();
        _isSeekable = _file.isSeekable();
        _startOffset = 0;
        if (_isSeekable) {
            _startOffset = _file.tell();
        }

        _currentOffset = _startOffset;
        _nextOffset = _currentOffset;
        _members = [];
        _memberDict = {};
        _fullyEnumerated = false;
        _trailerFound = false;
        _trailerOffset = -1;
        _writeTrailer = false;
        _nextInode = 2;
        return this;
    }

    function
    close (
        )

    /*++

    Routine Description:

        This routine closes the given archive.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var trailer;

        if (_file == null) {
            return;
        }

        //
        // Write out the trailer entry if one needs to be written.
        //

        if (_writeTrailer) {
            if (_currentOffset != _trailerOffset) {
                this._checkAccess("s");
                _currentOffset = _file.seek(_trailerOffset, IO_SEEK_SET);
            }

            trailer = CpioMember("TRAILER!!!");
            trailer.format = _members[-1].format;
            trailer.inode = 0;
            trailer.mode = 0;
            trailer.write(null, _file);
            _writeTrailer = false;
        }

        if (_shouldClose) {
            _file.close();
        }

        _file = null;
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

        var index = context;

        if (context == null) {
            index = -1;
        }

        if (index + 1 < _members.length()) {
            return index + 1;
        }

        if (_fullyEnumerated) {
            return null;
        }

        if (this.next() == null) {
            return null;
        }

        return index + 1;
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

        return _members[context];
    }

    function
    next (
        )

    /*++

    Routine Description:

        This routine returns the next CpioMember in the archive if the archive
        is open for reading.

    Arguments:

        None.

    Return Value:

        Returns the next CpioMember on success.

        null if there are no more members in the archive.

    --*/

    {

        var member;
        var offset;

        if (_trailerFound) {
            return null;
        }

        this._checkAccess("r");

        //
        // Get to the next archive member, as the file may be pointing at the
        // previous archive's data.
        //

        if (_currentOffset != _nextOffset) {
            if (_isSeekable) {
                _currentOffset = _file.seek(_nextOffset, IO_SEEK_SET);

            } else {
                _currentOffset +=
                             _file.read(_nextOffset - _currentOffset).length();
            }

            if (_currentOffset != _nextOffset) {
                Core.raise(IoError("Failed to seek to next archive member"));
            }
        }

        offset = _currentOffset;
        try {
            member = CpioMember.fromFile(_file);

        } except CpioEofError {
            _fullyEnumerated = true;
            return null;
        }

        member.offset = offset;
        _nextOffset = offset + member.archiveSize;
        _currentOffset = offset + member.headerSize;
        if (member.inode >= _nextInode) {
            _nextInode = member.inode + 1;
        }

        //
        // If this is the ending trailer entry, then the end of the archive is
        // found for sure.
        //

        if (((member.name == "TRAILER!!") || (member.name == "TRAILER!!!")) &&
            (member.size == 0)) {

            _trailerFound = true;
            _trailerOffset = offset;
            _fullyEnumerated = true;
            return null;
        }

        _members.append(member);
        _memberDict[member.name] = member;
        return member;
    }

    function
    getMember (
        name
        )

    /*++

    Routine Description:

        This routine returns the archive member with the given name. If the
        member occurs more than once in the archive, the last one is returned.

    Arguments:

        name - Supplies the archive member name.

    Return Value:

        Returns the CpioMember with the given name on success.

        Raises a KeyError if no member with the given name could be found.

    --*/

    {

        if (_isReadable) {

            //
            // Load everything up if it's not all been seen yet.
            //

            while (!_fullyEnumerated) {
                this.next();
            }
        }

        return _memberDict[name];
    }

    function
    getMembers (
        )

    /*++

    Routine Description:

        This routine returns a list of archive members. The order is the same
        as the order in the archive.

    Arguments:

        None.

    Return Value:

        Returns a list of CpioMembers.

    --*/

    {

        if (_isReadable) {
            while (!_fullyEnumerated) {
                this.next();
            }
        }

        return _members;
    }

    function
    getNames (
        )

    /*++

    Routine Description:

        This routine returns a list of archive member names. The list will be
        in the same order as the archive.

    Arguments:

        None.

    Return Value:

        Returns a list of strings containing the archive member names.

    --*/

    {

        var result = [];

        if (_isReadable) {
            while (!_fullyEnumerated) {
                this.next();
            }
        }

        for (member in _members) {
            result.append(member.name);
        }

        return result;
    }

    function
    list (
        members,
        verbose
        )

    /*++

    Routine Description:

        This routine prints a table of contents of the archive.

    Arguments:

        members - Supplies an optional list of CpioMembers to print. If null
            is supplied, then the full listing of the archive is printed.

        verbose - Supplies a boolean indicating whether to print out more
            information about each member (true) or just the names (false).

    Return Value:

        Prints the table of contents using Core.print.

    --*/

    {

        if (members == null) {
            members = this.getMembers();
        }

        for (member in members) {
            Core.print(member.ls(verbose));
        }

        return;
    }

    function
    extractAll (
        members,
        directoryPath,
        uid,
        gid,
        setPermissions
        )

    /*++

    Routine Description:

        This routine extracts all members from the archive. Permissions are
        only set at the very end.

    Arguments:

        members - Supplies an optional list of archive members to extract. The
            list should consist only of members from this archive. If null,
            all archive members will be extracted.

        directoryPath - Supplies an optional directory path to extract to. If
            null, contents will be extracted to the current directory.

        uid - Supplies the user ID to set on the extracted object. Supply -1 to
            use the user ID number in the archive.

        gid - Supplies the group ID to set on the extracted object. Supply -1
            to use the group ID number in the archive.

        setPermissions - Supplies a boolean indicating whether to set the
            permissions in the file. If false and uid/gid are -1, then
            no special attributes will be set on the file (which will make it
            be owned by the caller).

    Return Value:

        None. An exception will be raised on failure.

    --*/

    {

        if (members == null) {
            members = this;
        }

        for (member in members) {
            this._extract(member, directoryPath);
        }

        if (setPermissions) {
            for (member in members) {
                this._setPermissions(member, directoryPath, uid, gid);
            }
        }

        return 0;
    }

    function
    extract (
        member,
        directoryPath,
        uid,
        gid,
        setPermissions
        )

    /*++

    Routine Description:

        This routine extracts a member from the archive as a file object.

    Arguments:

        member - Supplies a pointer to the member to extract.

        directoryPath - Supplies an optional directory path to extract to. If
            null, contents will be extracted to the current directory.

        uid - Supplies the user ID to set on the extracted object. Supply -1 to
            use the user ID number in the archive.

        gid - Supplies the group ID to set on the extracted object. Supply -1
            to use the group ID number in the archive.

        setPermissions - Supplies a boolean indicating whether to set the
            permissions in the file. If false and uid/gid are -1, then
            no special attributes will be set on the file (which will make it
            be owned by the caller).

    Return Value:

        None. An exception will be raised on failure.

    --*/

    {

        this._extract(member, directoryPath, uid, gid, setPermissions);
        if (setPermissions) {
            this._setPermissions(member, directoryPath, uid, gid);
        }

        return 0;
    }

    function
    extractFile (
        member
        )

    /*++

    Routine Description:

        This routine extracts a member from the archive as a file object.

    Arguments:

        member - Supplies a pointer to the member to extract.

    Return Value:

        Returns a readable file like object.

        null if the archive member is not a regular file.

    --*/

    {

        this._checkAccess("r");
        if (!member.isFile()) {
            return null;
        }

        return ArchiveMemberFile(this, _file, member);
    }

    function
    add (
        name,
        archiveName,
        recursive,
        filter
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

        var contents;
        var member;
        var file;

        if (archiveName == null) {
            archiveName = name;
        }

        member = this.createMember(name, archiveName, null);

        //
        // See if the caller wants to filter this element.
        //

        if (filter) {
            member = filter(name, member);
            if (member == null) {
                return;
            }
        }

        if ((os.isfile)(name)) {
            file = open(name, "rb");
        }

        this.addMember(member, file);
        if (file) {
            file.close();
        }

        if (recursive && (os.isdir)(name)) {
            contents = (os.listdir)(name);
            for (element in contents) {
                this.add(name + "/" + element,
                         archiveName + "/" + element,
                         recursive,
                         filter);
            }
        }

        return;
    }

    function
    createMember (
        file,
        archivePath,
        format
        )

    /*++

    Routine Description:

        This routine creates a new CpioMember structure based on the given
        file.

    Arguments:

        file - Supplies either a string or a file object containing the file to
            create a member for.

        archivePath - Supplies an optional within the archive where the file
            should be put. If null it will match the name of the file.

        format - Supplies the format to create the member in.

    Return Value:

        Returns a new CpioMember instance. This member has not yet been added
        to the archive, it can be modified by the caller before callind add.

    --*/

    {

        var member;
        var mode;
        var size;
        var sourcePath = file;
        var stat;
        var statMode;

        if (file is String) {
            stat = (os.stat)(file);
            if (!archivePath) {
                archivePath = file;
            }

        } else {
            stat = (os.fstat)(file.fileno());
            if (!archivePath) {
                archivePath = file.name;
            }

            sourcePath = file.name;
        }

        member = CpioMember(archivePath);
        size = stat.st_size;
        statMode = stat.st_mode;
        mode = statMode & 0777;
        if ((statMode & os.S_ISUID) != 0) {
            mode |= CPIO_MODE_SUID;
        }

        if ((statMode & os.S_ISGID) != 0) {
            mode |= CPIO_MODE_SGID;
        }

        if ((statMode & os.S_ISVTX) != 0) {
            mode |= CPIO_MODE_STICKY;
        }

        if ((statMode & os.S_IFMT) == os.S_IFLNK) {
            member.link = (os.readlink)(sourcePath);
            size = member.link.length();
        }

        mode |= _osFileTypes[statMode & os.S_IFMT];
        if (format != null) {
            member.format = format;

        } else if (_members.length() != 0) {
            member.format = _members[-1].format;

        } else {
            member.format = "newc";
        }

        member.mode = mode;
        member.inode = stat.st_ino;
        if (member.inode == 0) {
            member.inode = _nextInode;
            _nextInode += 1;
        }

        member.uid = stat.st_uid;
        member.gid = stat.st_gid;
        member.nlink = stat.st_nlink;
        member.mtime = stat.st_mtime;
        member.size = stat.st_size;
        member.devMinor = stat.st_dev;
        member.devMajor = 0;
        member.rdevMinor = stat.st_rdev;
        member.rdevMajor = 0;
        member.check = -1;
        return member;
    }

    function
    addMember (
        member,
        file
        )

    /*++

    Routine Description:

        This routine appends a member to the archive.

    Arguments:

        member - Supplies the member to add.

        file - Supplies the file contents to add. The size should correspond to
            the member size.

    Return Value:

        None. The member will be appended to the archive on success.

        An exception will be raised on failure.

    --*/

    {

        this._checkAccess("w");

        //
        // Load the rest of the archive if it's readable.
        //

        if (_isReadable) {
            while (!_fullyEnumerated) {
                this.next();
            }
        }

        //
        // If a trailer was found, back up over it.
        //

        if (_trailerFound) {
            this._checkAccess("s");
            _currentOffset = _file.seek(_trailerOffset, IO_SEEK_SET);
        }

        //
        // Write out the member.
        //

        member.offset = _currentOffset;
        member.write(file, _file);
        _currentOffset += member.archiveSize;
        _trailerOffset = _currentOffset;
        _writeTrailer = true;
        _members.append(member);
        _memberDict[member.name] = member;
        return 0;

    }

    function
    _checkAccess (
        mode
        )

    /*++

    Routine Description:

        This routine validates that the archive has the given access, or raises
        an exception if it doesn't.

    Arguments:

        mode - Supplies the mode to check for.

    Return Value:

        None.

    --*/

    {

        if (_file == null) {
            Core.raise(IoError("File is closed"));
        }

        for (character in mode) {
            if (character == "r") {
                if (!_isReadable) {
                    Core.raise(IoError("File is not open for reading"));
                }

            } else if (character == "w") {
                if (!_isWritable) {
                    Core.raise(IoError("File is not open for writing"));
                }

            } else if (character == "s") {
                if (!_isSeekable) {
                    Core.raise(IoError("File is not seekable"));
                }
            }
        }

        return;
    }

    function
    _getCurrentOffset (
        )

    /*++

    Routine Description:

        This routine returns the internal offset from within the archive. This
        should not be called externally.

    Arguments:

        None.

    Return Value:

        Returns the current offset.

    --*/

    {

        return _currentOffset;
    }

    function
    _setCurrentOffset (
        offset
        )

    /*++

    Routine Description:

        This routine sets the internal offset within the archive. This should
        not be called externally.

    Arguments:

        offset - Supplies the new offset to set.

    Return Value:

        None.

    --*/

    {

        _currentOffset = offset;
        return;
    }

    function
    _extract (
        member,
        directoryPath
        )

    /*++

    Routine Description:

        This routine extracts data from a member to the host file system. This
        routine does not set final permissions on the file.

    Arguments:

        member - Supplies a pointer to the member to extract.

        directoryPath - Supplies an optional directory path to extract to. If
            null, contents will be extracted to the current directory.

    Return Value:

        None. An exception will be raised on failure.

    --*/

    {

        var chunk;
        var chunkSize = 131072;
        var destination;
        var memberMode = member.mode;
        var mode;
        var path;
        var source;

        if (!directoryPath) {
            directoryPath = ".";
        }

        path = directoryPath + "/" + member.name;

        //
        // Special devices are currently not supported.
        //

        if (member.isDev()) {
            return;
        }

        if (member.isDir()) {
            try {
                (os.mkdir)(path, 0777);

            } except os.OsError as e {
                if (e.errno != os.EEXIST) {
                    Core.raise(e);
                }
            }

        } else if (member.isLink()) {
            (os.symlink)(member.link, path);

        } else {
            source = this.extractFile(member);
            destination = open(path, "wb");
            try {
                while (true) {
                    chunk = source.read(chunkSize);
                    if (chunk.length() == 0) {
                        break;
                    }

                    destination.write(chunk);
                }

            } except Exception as e {
                Core.raise(e);

            } finally {
                source.close();
                destination.close();
            }
        }

        return;
    }

    function
    _setPermissions (
        member,
        directoryPath,
        uid,
        gid
        )

    /*++

    Routine Description:

        This routine sets the final permissions for an extracted member.

    Arguments:

        member - Supplies a pointer to the member to extract.

        directoryPath - Supplies an optional directory path to extract to. If
            null, contents will be extracted to the current directory.

        uid - Supplies the user ID to set on the extracted object. Supply -1 to
            use the user ID number in the archive.

        gid - Supplies the group ID to set on the extracted object. Supply -1
            to use the group ID number in the archive.

    Return Value:

        None. An exception will be raised on failure.

    --*/

    {

        var memberMode = member.mode;
        var mode;
        var path;

        if (!directoryPath) {
            directoryPath = ".";
        }

        path = directoryPath + "/" + member.name;
        mode = memberMode & 0777;
        if (memberMode & CPIO_MODE_SUID) {
            mode |= os.S_ISUID;
        }

        if (memberMode & CPIO_MODE_SGID) {
            mode |= os.S_ISGID;
        }

        if (memberMode & CPIO_MODE_STICKY) {
            mode |= os.S_ISVTX;
        }

        (os.chmod)(path, mode);
        if (uid < 0) {
            uid = member.uid;
        }

        if (gid < 0) {
            gid = member.gid;
        }

        (os.utimes)(path, member.mtime, 0, member.mtime, 0);
        try {
            (os.chown)(path, uid, gid);

        } except os.OsError as e {
            if (e.errno != os.ENOSYS) {
                Core.raise(e);
            }
        }

        return;
    }
}

//
// --------------------------------------------------------- Internal Functions
//

