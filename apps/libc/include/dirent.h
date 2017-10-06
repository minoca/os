/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    dirent.h

Abstract:

    This header contains definitions for enumerating the contents of file
    system directories.

Author:

    Evan Green 11-Mar-2013

--*/

#ifndef _DIRENT_H
#define _DIRENT_H

//
// ------------------------------------------------------------------- Includes
//

#include <limits.h>
#include <sys/types.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define file types found in the directory entry structure.
//

//
// Unknown file type. Use stat to inquire.
//

#define DT_UNKNOWN 0

//
// FIFO pipe object
//

#define DT_FIFO 1

//
// Character device
//

#define DT_CHR 2

//
// Regular directory
//

#define DT_DIR 4

//
// Block device
//

#define DT_BLK 6

//
// Regular file
//

#define DT_REG 8

//
// Symbolic link
//

#define DT_LNK 10

//
// Socket
//

#define DT_SOCK 12

//
// Whiteout entry. The definition is provided for historical reasons. This
// value is never returned by the kernel.
//

#define DT_WHT 14

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define a type used to represent an open directory stream.
//

typedef struct _DIR DIR;

/*++

Structure Description:

    This structure stores information about about a directory entry.

Members:

    d_ino - Stores the file serial number for the entry.

    d_off - Stores the opaque offset of the next directory entry structure.
        This value should not be inspected, as it is unpredictable. It should
        only be used to save and restore a location within a directory.

    d_reclen - Stores the size in bytes of the entire entry, including this
        structure, the name string, and the null terminator on the name.

    d_type - Stores the file type of the entry. See DT_* definitions.

    d_name - Stores the null terminated name of the directory.

--*/

struct dirent {
    ino_t d_ino;
    off_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[NAME_MAX];
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
DIR *
opendir (
    const char *DirectoryName
    );

/*++

Routine Description:

    This routine opens a directory for reading.

Arguments:

    DirectoryName - Supplies a pointer to a null terminated string containing
        the name of the directory to open.

Return Value:

    Returns a pointer to the directory on success.

    NULL on failure.

--*/

LIBC_API
DIR *
fdopendir (
    int FileDescriptor
    );

/*++

Routine Description:

    This routine opens a directory based on an already open file descriptor to
    a directory.

Arguments:

    FileDescriptor - Supplies a pointer to the open handle to the directory.

Return Value:

    0 on success.

    -1 on failure, and more details will be provided in errno.

--*/

LIBC_API
int
closedir (
    DIR *Directory
    );

/*++

Routine Description:

    This routine closes an open directory.

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

Return Value:

    0 on success.

    -1 on failure, and more details will be provided in errno.

--*/

LIBC_API
int
readdir_r (
    DIR *Directory,
    struct dirent *Buffer,
    struct dirent **Result
    );

/*++

Routine Description:

    This routine reads from a directly in a reentrant manner.

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

    Buffer - Supplies the buffer where the next directory entry will be
        returned.

    Result - Supplies a pointer that will either be set to the Buffer pointer
        if there are more entries, or NULL if there are no more entries in the
        directory.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

LIBC_API
struct dirent *
readdir (
    DIR *Directory
    );

/*++

Routine Description:

    This routine reads the next directory entry from the open directory
    stream.

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

Return Value:

    Returns a pointer to the next directory entry on success.

    NULL on failure or when the end of the directory is reached. On failure,
        errno is set. If the end of the directory is reached, errno is not
        changed.

--*/

LIBC_API
void
seekdir (
    DIR *Directory,
    long Location
    );

/*++

Routine Description:

    This routine seeks directory to the given location. The location must have
    been returned from a previous call to telldir, otherwise the results are
    undefined.

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

    Location - Supplies the location within the directory to seek to as given
        by the telldir function.

Return Value:

    None. No errors are defined.

--*/

LIBC_API
long
telldir (
    DIR *Directory
    );

/*++

Routine Description:

    This routine returns the current position within a directory. This position
    can be seeked to later (in fact, the return value from this function is the
    only valid parameter to pass to seekdir).

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

Return Value:

    Returns the current location of the specified directory stream.

--*/

LIBC_API
void
rewinddir (
    DIR *Directory
    );

/*++

Routine Description:

    This routine rewinds a directory back to the beginning.

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

Return Value:

    None.

--*/

LIBC_API
int
dirfd (
    DIR *Directory
    );

/*++

Routine Description:

    This routine returns the file descriptor backing the given directory.

Arguments:

    Directory - Supplies a pointer to the structure returned by the open
        directory function.

Return Value:

    None.

--*/

LIBC_API
int
alphasort (
    const struct dirent **Left,
    const struct dirent **Right
    );

/*++

Routine Description:

    This routine can be used as the comparison function for the scandir
    function. It will compare directory names in alphabetical order.

Arguments:

    Left - Supplies a pointer to a pointer to the left directory entry to
        compare.

    Right - Supplies a pointer to a pointer to the right directory entry to
        compare.

Return Value:

    < 0 if Left < Right.

    0 if Left == Right.

    > 0 if Left > Right.

--*/

LIBC_API
int
scandir (
    const char *DirectoryPath,
    struct dirent ***NameList,
    int (*SelectFunction)(const struct dirent *),
    int (*CompareFunction)(const struct dirent **, const struct dirent **)
    );

/*++

Routine Description:

    This routine scans the given directory, and calls the select function for
    each entry. If the select function returns non-zero, then the entry is
    added to the list of entries to return. If the compare function is
    non-zero the resulting entries are sorted via qsort.

Arguments:

    DirectoryPath - Supplies a pointer to a null-terminated string containing
        the directory path to scan.

    NameList - Supplies a pointer where an array of pointers to selected
        directory entries will be returned on success. The caller is
        responsible for freeing both the returned array and each individual
        element in the array.

    SelectFunction - Supplies an optional pointer to a function used to
        determine whether or not a particular entry should end up in the
        final list. If the select function returns non-zero, then the entry
        is added to the final list. If this parameter is NULL, then all
        entries are selected.

    CompareFunction - Supplies an optional pointer to a function used to
        compare two directory entries during the final sorting process. If this
        is NULL, then the order of the names in the name list is unspecified.

Return Value:

    Returns the number of entries returned in the array on success.

    -1 on failure.

--*/

#ifdef __cplusplus

}

#endif
#endif

