/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    copy.c

Abstract:

    This module implements generic copy functionality for the Swiss common
    library.

Author:

    Evan Green 3-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the buffer size for copy blocks.
//

#define COPY_BLOCK_SIZE (1024 * 512)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SwpCopy (
    BOOL IsOperand,
    ULONG Options,
    PSTR Source,
    PSTR Destination
    );

INT
SwpCopyRegularFile (
    ULONG Options,
    PSTR Source,
    struct stat *SourceStat,
    PSTR Destination,
    struct stat *DestinationStat
    );

INT
SwpCopyNonRegularFile (
    ULONG Options,
    PSTR Source,
    struct stat *SourceStat,
    PSTR Destination,
    struct stat *DestinationStat
    );

INT
SwpMatchFileProperties (
    PSTR Destination,
    struct stat *Stat
    );

BOOL
SwpTestForFileInPathTraversal (
    PSTR Path,
    dev_t Device,
    ino_t File
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
SwCopy (
    ULONG Options,
    PSTR Source,
    PSTR Destination
    )

/*++

Routine Description:

    This routine performs a copy of the source file or directory to the
    destination.

Arguments:

    Options - Supplies a bitfield of options governing the behavior of the
        copy.

    Source - Supplies a pointer to the string describing the source path to
        copy.

    Destination - Supplies a pointer to the string describing the destination of
        the copy.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    mode_t OriginalMask;
    INT Result;

    OriginalMask = 0;
    if ((Options & COPY_OPTION_PRESERVE_PERMISSIONS) != 0) {
        OriginalMask = umask(0);
    }

    Result = SwpCopy(TRUE, Options, Source, Destination);
    if ((Options & COPY_OPTION_PRESERVE_PERMISSIONS) != 0) {
        umask(OriginalMask);
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SwpCopy (
    BOOL IsOperand,
    ULONG Options,
    PSTR Source,
    PSTR Destination
    )

/*++

Routine Description:

    This routine performs a copy of the source file or directory to the
    destination.

Arguments:

    IsOperand - Supplies a boolean indicating if this is a direct call from
        someone or a recursed call.

    Options - Supplies a bitfield of options governing the behavior of the
        copy.

    Source - Supplies a pointer to the string describing the source path to
        copy.

    Destination - Supplies a pointer to the string describing the destination of
        the copy.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSTR AppendedDestination;
    ULONG AppendedDestinationSize;
    PSTR AppendedSource;
    ULONG AppendedSourceSize;
    BOOL DestinationExists;
    struct stat DestinationStat;
    DIR *Directory;
    struct dirent *DirectoryEntry;
    BOOL FollowLinks;
    PSTR QuotedDestination;
    PSTR QuotedSource;
    BOOL RecursiveCopy;
    size_t SourceLength;
    struct stat SourceStat;
    INT Status;

    AppendedDestination = NULL;
    AppendedSource = NULL;
    Directory = NULL;
    QuotedDestination = Destination;
    QuotedSource = Source;
    FollowLinks = FALSE;
    if (((Options & COPY_OPTION_FOLLOW_LINKS) != 0) ||
        ((IsOperand != FALSE) &&
         ((Options & COPY_OPTION_FOLLOW_OPERAND_LINKS) != 0))) {

        FollowLinks = TRUE;
    }

    //
    // If verbose, print out the operation.
    //

    if ((Options & COPY_OPTION_VERBOSE) != 0) {
        QuotedSource = SwQuoteArgument(Source);
        QuotedDestination = SwQuoteArgument(Destination);
        printf("'%s' -> '%s'\n", QuotedSource, QuotedDestination);
        if (QuotedSource != Source) {
            free(QuotedSource);
            QuotedSource = Source;
        }

        if (QuotedDestination != Destination) {
            free(QuotedDestination);
            QuotedDestination = Destination;
        }
    }

    //
    // Stat the destination and the source.
    //

    DestinationExists = FALSE;
    Status = SwStat(Destination, TRUE, &DestinationStat);
    if (Status == 0) {
        DestinationExists = TRUE;

    } else if (Status != ENOENT) {
        SwPrintError(Status, Destination, "Cannot stat");
        goto CopyEnd;
    }

    Status = SwStat(Source, FollowLinks, &SourceStat);
    if (Status != 0) {
        SwPrintError(Status, Source, "Cannot stat");
        goto CopyEnd;
    }

    //
    // If the source and destination are the same, then print a message and do
    // nothing else.
    //

    if ((DestinationExists != FALSE) &&
        (SourceStat.st_ino == DestinationStat.st_ino) &&
        (SourceStat.st_dev == DestinationStat.st_dev) &&
        (SourceStat.st_ino != 0)) {

        QuotedSource = SwQuoteArgument(Source);
        QuotedDestination = SwQuoteArgument(Destination);
        SwPrintError(0,
                     NULL,
                     "'%s' and '%s' are the same file",
                     QuotedSource,
                     QuotedDestination);

        goto CopyEnd;
    }

    //
    // Copy a directory.
    //

    if (S_ISDIR(SourceStat.st_mode)) {

        //
        // If it's a directory and recursive mode is not enabled, fail.
        //

        if ((Options & COPY_OPTION_RECURSIVE) == 0) {
            SwPrintError(0, Source, "Skipping directory");
            goto CopyEnd;
        }

        //
        // Avoid copying a directory into itself.
        //

        SourceLength = strlen(Source);
        RecursiveCopy = SwpTestForFileInPathTraversal(Destination,
                                                      SourceStat.st_dev,
                                                      SourceStat.st_ino);

        if (RecursiveCopy != FALSE) {
            QuotedSource = SwQuoteArgument(Source);
            QuotedDestination = SwQuoteArgument(Destination);
            SwPrintError(0,
                         NULL,
                         "Cannot copy a directory '%s' into itself '%s'",
                         QuotedSource,
                         QuotedDestination);

            goto CopyEnd;
        }

        //
        // If not specified directly as an operand and it's a dot or a dot dot,
        // skip it.
        //

        if (SourceLength == 0) {
            SwPrintError(0, NULL, "Invalid empty source");
            Status = EINVAL;
            goto CopyEnd;
        }

        if (Source[SourceLength - 1] == '.') {
            if ((SourceLength == 1) || (Source[SourceLength - 2] == '/')) {
                goto CopyEnd;

            } else if ((Source[SourceLength - 2] == '.') &&
                       ((SourceLength == 2) ||
                        (Source[SourceLength - 3] == '/'))) {

                goto CopyEnd;
            }
        }

        //
        // If the destination exists and is not a directory, print a message
        // and skip it.
        //

        if ((DestinationExists != FALSE) &&
            (!S_ISDIR(DestinationStat.st_mode))) {

            QuotedSource = SwQuoteArgument(Source);
            QuotedDestination = SwQuoteArgument(Destination);
            SwPrintError(0,
                         NULL,
                         "Cannot overwrite non-directory '%s' with directory "
                         "'%s'",
                         QuotedDestination,
                         QuotedSource);

            Status = EINVAL;
            goto CopyEnd;
        }

        //
        // Create the destination directory with the same permissions as the
        // source, but allow user access while the contents of the directory
        // are being copied.
        //

        if (DestinationExists == FALSE) {
            Status = SwMakeDirectory(Destination, SourceStat.st_mode | S_IRWXU);
            if (Status != 0) {
                Status = errno;
                SwPrintError(Status, Destination, "Failed to create directory");
                goto CopyEnd;
            }
        }

        //
        // Recursively copy files inside of the source directory.
        //

        Directory = opendir(Source);
        if (Directory == NULL) {
            Status = errno;
            SwPrintError(Status, Source, "Failed to open directory");
            goto CopyEnd;
        }

        while (TRUE) {
            errno = 0;
            DirectoryEntry = readdir(Directory);
            if (DirectoryEntry == NULL) {
                Status = errno;
                if (Status != 0) {
                    SwPrintError(Status, Source, "Failed to read directory");
                    goto CopyEnd;
                }

                break;
            }

            //
            // Though there's also a check above, do a quick check here to
            // avoid unnecessary recursion.
            //

            if ((strcmp(DirectoryEntry->d_name, ".") == 0) ||
                (strcmp(DirectoryEntry->d_name, "..") == 0)) {

                continue;
            }

            //
            // Create appended versions of the source and destination paths.
            //

            Status = SwAppendPath(Destination,
                                  strlen(Destination) + 1,
                                  DirectoryEntry->d_name,
                                  strlen(DirectoryEntry->d_name) + 1,
                                  &AppendedDestination,
                                  &AppendedDestinationSize);

            if (Status == FALSE) {
                Status = ENOMEM;
                SwPrintError(Status, NULL, "Failed to allocate");
                goto CopyEnd;
            }

            Status = SwAppendPath(Source,
                                  strlen(Source) + 1,
                                  DirectoryEntry->d_name,
                                  strlen(DirectoryEntry->d_name) + 1,
                                  &AppendedSource,
                                  &AppendedSourceSize);

            if (Status == FALSE) {
                Status = ENOMEM;
                SwPrintError(Status, NULL, "Failed to allocate");
                goto CopyEnd;
            }

            Status = SwpCopy(FALSE,
                             Options,
                             AppendedSource,
                             AppendedDestination);

            if (Status != 0) {
                SwPrintError(Status, Source, "Bailing out of");
                goto CopyEnd;
            }

            free(AppendedDestination);
            AppendedDestination = NULL;
            free(AppendedSource);
            AppendedSource = NULL;
        }

        closedir(Directory);
        Directory = NULL;

        //
        // If the preserve option is set, set all of the file attributes.
        //

        if ((Options & COPY_OPTION_PRESERVE_PERMISSIONS) != 0) {
            Status = SwpMatchFileProperties(Destination, &SourceStat);
            if (Status != 0) {
                goto CopyEnd;
            }

        //
        // Set the file permission bits to that of the source.
        //

        } else if (SourceStat.st_mode != (SourceStat.st_mode | S_IRWXU)) {
            Status = chmod(Destination, SourceStat.st_mode);
            if (Status != 0) {
                Status = errno;
                SwPrintError(Status,
                             Destination,
                             "Failed to set permissions on directory");

                goto CopyEnd;
            }
        }

    //
    // Copy a regular file.
    //

    } else if (S_ISREG(SourceStat.st_mode)) {
        if (DestinationExists != FALSE) {
            Status = SwpCopyRegularFile(Options,
                                        Source,
                                        &SourceStat,
                                        Destination,
                                        &DestinationStat);

        } else {
            Status = SwpCopyRegularFile(Options,
                                        Source,
                                        &SourceStat,
                                        Destination,
                                        NULL);
        }

        if (Status != 0) {
            goto CopyEnd;
        }

    //
    // Copy something else.
    //

    } else {
        if (DestinationExists != FALSE) {
            Status = SwpCopyNonRegularFile(Options,
                                           Source,
                                           &SourceStat,
                                           Destination,
                                           &DestinationStat);

        } else {
            Status = SwpCopyNonRegularFile(Options,
                                           Source,
                                           &SourceStat,
                                           Destination,
                                           NULL);
        }

        if (Status != 0) {
            goto CopyEnd;
        }
    }

CopyEnd:
    if (Directory != NULL) {
        closedir(Directory);
    }

    if (AppendedDestination != NULL) {
        free(AppendedDestination);
    }

    if (AppendedSource != NULL) {
        free(AppendedSource);
    }

    if (QuotedSource != Source) {
        free(QuotedSource);
    }

    if (QuotedDestination != Destination) {
        free(Destination);
    }

    return Status;
}

INT
SwpCopyRegularFile (
    ULONG Options,
    PSTR Source,
    struct stat *SourceStat,
    PSTR Destination,
    struct stat *DestinationStat
    )

/*++

Routine Description:

    This routine copies a regular file.

Arguments:

    Options - Supplies a bitfield of options governing the behavior of the
        copy.

    Source - Supplies a pointer to the string describing the source path to
        copy.

    SourceStat - Supplies a pointer to the source file information (or that of
        the link destination if appropriate).

    Destination - Supplies a pointer to the string describing the destination of
        the copy.

    DestinationStat - Supplies a pointer to the destination file information, or
        NULL if the destination does not exist.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    BOOL Answer;
    PVOID Buffer;
    ssize_t BytesRead;
    ssize_t BytesWritten;
    int CloseStatus;
    mode_t CreatePermissions;
    int DestinationFile;
    PSTR QuotedDestination;
    INT Result;
    int SourceFile;
    ssize_t TotalBytesWritten;

    DestinationFile = -1;
    Result = 0;
    SourceFile = -1;

    //
    // Allocate a buffer to store the file information.
    //

    Buffer = malloc(COPY_BLOCK_SIZE);
    if (Buffer == NULL) {
        goto CopyRegularFileEnd;
    }

    //
    // If the destination file exists and the interactive option is on,
    // prompt.
    //

    if (DestinationStat != NULL) {
        if ((Options & COPY_OPTION_INTERACTIVE)) {
            QuotedDestination = SwQuoteArgument(Destination);
            fprintf(stderr,
                    "%s: Overwrite file '%s'? ",
                    SwGetCurrentApplicationName(),
                    QuotedDestination);

            Result = SwGetYesNoAnswer(&Answer);
            if ((Result != 0) || (Answer == FALSE)) {
                Result = 1;
                goto CopyRegularFileEnd;
            }
        }

        //
        // Attempt to open and truncate the file.
        //

        DestinationFile = SwOpen(Destination, O_WRONLY | O_TRUNC | O_BINARY, 0);
        if (DestinationFile < 0) {
            if ((Options & COPY_OPTION_UNLINK) != 0) {
                Result = SwUnlink(Destination);
                if (Result != 0) {
                    Result = errno;
                    SwPrintError(Result, Destination, "Cannot remove");
                    goto CopyRegularFileEnd;
                }

            } else {
                Result = errno;
                SwPrintError(Result, Destination, "Cannot open");
                goto CopyRegularFileEnd;
            }
        }
    }

    //
    // If the file isn't already opened, it must not exist or have been
    // unlinked. Create it now. If other properties are going to be changed,
    // then create it with the appropriate permissions to do that. Otherwise,
    // create it with the final permissions.
    //

    if (DestinationFile < 0) {
        CreatePermissions = SourceStat->st_mode;
        if ((Options & COPY_OPTION_PRESERVE_PERMISSIONS) != 0) {
            CreatePermissions |= S_IRWXU;
        }

        DestinationFile = SwOpen(Destination,
                                 O_WRONLY | O_CREAT | O_BINARY,
                                 CreatePermissions);

        if (DestinationFile < 0) {
            Result = errno;
            SwPrintError(Result, Destination, "Cannot open");
            goto CopyRegularFileEnd;
        }
    }

    //
    // Open up the source as well.
    //

    SourceFile = SwOpen(Source, O_RDONLY | O_BINARY, 0);
    if (SourceFile < 0) {
        Result = errno;
        SwPrintError(Result, Destination, "Cannot open");
        goto CopyRegularFileEnd;
    }

    //
    // Repeatedly copy blocks from the source file to the destination file.
    //

    while (TRUE) {
        do {
            BytesRead = read(SourceFile, Buffer, COPY_BLOCK_SIZE);

        } while ((BytesRead < 0) && (errno == EINTR));

        //
        // Stop if the read failed.
        //

        if (BytesRead < 0) {
            Result = errno;
            break;
        }

        //
        // Stop at the end of the file.
        //

        if (BytesRead == 0) {
            break;
        }

        //
        // Write this bunch in.
        //

        TotalBytesWritten = 0;
        while (TotalBytesWritten < BytesRead) {
            do {
                BytesWritten = write(DestinationFile,
                                     Buffer + TotalBytesWritten,
                                     BytesRead - TotalBytesWritten);

            } while ((BytesWritten <= 0) && (errno == EINTR));

            //
            // Stop if the write failed.
            //

            if (BytesWritten <= 0) {
                Result = errno;
                SwPrintError(Result, Destination, "Failed to write to");
                goto CopyRegularFileEnd;
            }

            TotalBytesWritten += BytesWritten;
        }
    }

    //
    // Fix up the permissions if requested. Make sure to close the destination
    // file first.
    //

    close(DestinationFile);
    DestinationFile = -1;
    if ((Options & COPY_OPTION_PRESERVE_PERMISSIONS) != 0) {
        Result = SwpMatchFileProperties(Destination, SourceStat);
        if (Result != 0) {
            goto CopyRegularFileEnd;
        }
    }

CopyRegularFileEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    if (SourceFile >= 0) {
        close(SourceFile);
    }

    if (DestinationFile >= 0) {
        CloseStatus = close(DestinationFile);
        if (CloseStatus != 0) {
            CloseStatus = errno;
            SwPrintError(CloseStatus, Destination, "Failed to close");
            if (Result == 0) {
                Result = CloseStatus;
            }
        }
    }

    return Result;
}

INT
SwpCopyNonRegularFile (
    ULONG Options,
    PSTR Source,
    struct stat *SourceStat,
    PSTR Destination,
    struct stat *DestinationStat
    )

/*++

Routine Description:

    This routine copies a symbolic link or FIFO object.

Arguments:

    Options - Supplies a bitfield of options governing the behavior of the
        copy.

    Source - Supplies a pointer to the string describing the source path to
        copy.

    SourceStat - Supplies a pointer to the source file information (or that of
        the link destination if appropriate).

    Destination - Supplies a pointer to the string describing the destination of
        the copy.

    DestinationStat - Supplies a pointer to the destination file information, or
        NULL if the destination does not exist.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    BOOL Answer;
    PSTR LinkDestination;
    PSTR QuotedDestination;
    INT Result;

    LinkDestination = NULL;
    Result = 0;

    //
    // Unless the recursive option is on, don't do anything.
    //

    if (((Options & COPY_OPTION_RECURSIVE) == 0) ||
        ((!S_ISLNK(SourceStat->st_mode)) && (!S_ISFIFO(SourceStat->st_mode)))) {

        SwPrintError(0, Source, "Skipping non-regular file");
        goto CopyNonRegularFileEnd;
    }

    //
    // If the destination file exists and the interactive option is on,
    // prompt.
    //

    if (DestinationStat != NULL) {
        if ((Options & COPY_OPTION_INTERACTIVE)) {
            QuotedDestination = SwQuoteArgument(Destination);
            fprintf(stderr,
                    "%s: Overwrite non-regular file '%s'? ",
                    SwGetCurrentApplicationName(),
                    QuotedDestination);

            Result = SwGetYesNoAnswer(&Answer);
            if ((Result != 0) || (Answer == FALSE)) {
                Result = 1;
                goto CopyNonRegularFileEnd;
            }
        }
    }

    //
    // Attempt to create the thing.
    //

    if (S_ISFIFO(SourceStat->st_mode)) {
        Result = SwMakeFifo(Destination, SourceStat->st_mode);
        if (Result != 0) {
            SwPrintError(Result, Source, "Failed to create FIFO");
            goto CopyNonRegularFileEnd;
        }

    } else if (S_ISLNK(SourceStat->st_mode)) {
        Result = SwReadLink(Source, &LinkDestination);
        if (Result != 0) {
            SwPrintError(Result, Source, "Failed to read link");
            goto CopyNonRegularFileEnd;
        }

        Result = SwCreateSymbolicLink(LinkDestination, Destination);
        if (Result != 0) {
            SwPrintError(Result, Source, "Failed to create symlink");
            goto CopyNonRegularFileEnd;
        }
    }

CopyNonRegularFileEnd:
    if (LinkDestination != NULL) {
        free(LinkDestination);
    }

    return Result;
}

INT
SwpMatchFileProperties (
    PSTR Destination,
    struct stat *Stat
    )

/*++

Routine Description:

    This routine sets the owner, group, modification time, and access time of
    the given file based on the supplied stat structure. If the owner and
    group could not be set, the ISUID and ISGID bits will be cleared.

Arguments:

    Destination - Supplies a pointer to the string describing the path whose
        permissions should be changed.

    Stat - Supplies a pointer to the file information to set.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    mode_t Mode;
    int Result;
    struct utimbuf Times;

    //
    // Set the file times before potentially revoking write access.
    //

    Times.actime = Stat->st_atime;
    Times.modtime = Stat->st_mtime;
    Result = utime(Destination, &Times);
    if ((Result != 0) && (!S_ISDIR(Stat->st_mode))) {

        //
        // If the times are set to -1 and the status is EINVAL, try setting
        // the times to now. This works around an issue on Windows with file
        // times that are not set. Other OSes should not return EINVAL for
        // -1 times.
        //

        Result = errno;
        if ((Result == EINVAL) &&
            ((Times.actime == -1) ||
             (Times.modtime == -1))) {

            if (Times.actime == -1) {
                Times.actime = time(NULL);
            }

            if (Times.modtime == -1) {
                Times.modtime = time(NULL);
            }

            Result = utime(Destination, &Times);
            if (Result != 0) {
                Result = errno;
            }
        }

        if (Result != 0) {
            SwPrintError(Result, Destination, "Failed to set times of ");
            goto MatchFilePropertiesEnd;
        }
    }

    Result = SwChangeFileOwner(Destination, FALSE, Stat->st_uid, Stat->st_gid);
    if (Result != 0) {

        //
        // Change the permissions to clear the ISGID and ISUID bits.
        //

        Mode = Stat->st_mode & (~(S_ISGID | S_ISUID));
        chmod(Destination, Mode);
        goto MatchFilePropertiesEnd;
    }

    Result = chmod(Destination, Stat->st_mode);
    if (Result != 0) {
        Result = errno;
        SwPrintError(Result, Destination, "Failed to set permissions for");
        goto MatchFilePropertiesEnd;
    }

    Result = 0;

MatchFilePropertiesEnd:
    return Result;
}

BOOL
SwpTestForFileInPathTraversal (
    PSTR Path,
    dev_t Device,
    ino_t File
    )

/*++

Routine Description:

    This routine tests to see if the given file number is in the path
    traversal.

Arguments:

    Path - Supplies a pointer to the path to check.

    Device - Supplies the device number to check for.

    File - Supplies the file number to check for.

Return Value:

    TRUE if the file number is in the path traversal.

    FALSE on error or if the file number is not in the path traversal.

--*/

{

    BOOL Answer;
    PSTR CurrentSeparator;
    PSTR NextSeparator;
    CHAR OriginalCharacter;
    PSTR PathCopy;
    ULONG PathLength;
    INT Result;
    struct stat Stat;

    Answer = FALSE;
    PathCopy = NULL;

    //
    // Skip this if all the file numbers are zero.
    //

    if (File == 0) {
        goto TestForFileInPathTraversalEnd;
    }

    PathLength = strlen(Path);
    PathCopy = malloc(PathLength + 1);
    if (PathCopy == NULL) {
        goto TestForFileInPathTraversalEnd;
    }

    memcpy(PathCopy, Path, PathLength + 1);
    if (*PathCopy == '/') {
        CurrentSeparator = PathCopy + 1;

    } else {
        CurrentSeparator = strchr(PathCopy, '/');
    }

    while (CurrentSeparator != NULL) {
        NextSeparator = strchr(CurrentSeparator + 1, '/');

        //
        // Temporarily terminate the string and stat that partial path.
        //

        OriginalCharacter = *CurrentSeparator;
        *CurrentSeparator = '\0';
        Result = SwStat(PathCopy, TRUE, &Stat);
        *CurrentSeparator = OriginalCharacter;
        if (Result != 0) {
            break;
        }

        if ((Stat.st_dev == Device) && (Stat.st_ino == File)) {
            Answer = TRUE;
            goto TestForFileInPathTraversalEnd;
        }

        CurrentSeparator = NextSeparator;
    }

    //
    // Check the path as a whole.
    //

    Result = SwStat(Path, TRUE, &Stat);
    if (Result != 0) {
        goto TestForFileInPathTraversalEnd;
    }

    if (Stat.st_ino == File) {
        Answer = TRUE;
        goto TestForFileInPathTraversalEnd;
    }

TestForFileInPathTraversalEnd:
    if (PathCopy != NULL) {
        free(PathCopy);
    }

    return Answer;
}

