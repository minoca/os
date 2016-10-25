/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    copy.c

Abstract:

    This module implements the performance benchmark tests for file copy
    throughput using both read() and write().

Author:

    Chris Stevens 6-May-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "perftest.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PT_COPY_TEST_FILE_NAME_LENGTH 48
#define PT_COPY_TEST_FILE_SIZE (2 * 1024 * 1024)
#define PT_COPY_TEST_BUFFER_SIZE 4096

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

void
CopyMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the file copy performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    char *Buffer;
    ssize_t BytesCompleted;
    int DestinationDescriptor;
    int DestinationFileCreated;
    char DestinationFileName[PT_COPY_TEST_FILE_NAME_LENGTH];
    int Index;
    pid_t ProcessId;
    int SourceDescriptor;
    int SourceFileCreated;
    char SourceFileName[PT_COPY_TEST_FILE_NAME_LENGTH];
    int Status;
    unsigned long long TotalBytes;

    DestinationDescriptor = -1;
    DestinationFileCreated = 0;
    Result->Type = PtResultBytes;
    Result->Status = 0;
    SourceDescriptor = -1;
    SourceFileCreated = 0;
    TotalBytes = 0;

    //
    // Allocate a buffer for the copies.
    //

    Buffer = malloc(PT_COPY_TEST_BUFFER_SIZE);
    if (Buffer == NULL) {
        Result->Status = ENOMEM;
        goto MainEnd;
    }

    //
    // Get the process ID and create process safe file path for the source file.
    //

    ProcessId = getpid();
    Status = snprintf(SourceFileName,
                      PT_COPY_TEST_FILE_NAME_LENGTH,
                      "copy_src_%d.txt",
                      ProcessId);

    if (Status < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    //
    // Create the source file with read/write access, as it needs to be primed.
    //

    SourceDescriptor = open(SourceFileName,
                            O_RDWR | O_CREAT | O_TRUNC,
                            S_IRUSR | S_IWUSR);

    if (SourceDescriptor < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    SourceFileCreated = 1;

    //
    // Create the destination file.
    //

    Status = snprintf(DestinationFileName,
                      PT_COPY_TEST_FILE_NAME_LENGTH,
                      "copy_dst_%d.txt",
                      ProcessId);

    if (Status < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    DestinationDescriptor = open(DestinationFileName,
                                 O_WRONLY | O_CREAT, O_TRUNC,
                                 S_IRUSR | S_IWUSR);

    if (DestinationDescriptor < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    DestinationFileCreated = 1;

    //
    // As this is really a test of copy between files on the system's cache,
    // prime the cache with junk data for both files.
    //

    for (Index = 0;
         Index < (PT_COPY_TEST_FILE_SIZE / PT_COPY_TEST_BUFFER_SIZE);
         Index += 1) {

        do {
            BytesCompleted = write(SourceDescriptor,
                                   Buffer,
                                   PT_COPY_TEST_BUFFER_SIZE);

        } while ((BytesCompleted < 0) && (errno == EINTR));

        if (BytesCompleted < 0) {
            Result->Status = errno;
            goto MainEnd;
        }

        if (BytesCompleted != PT_COPY_TEST_BUFFER_SIZE) {
            Result->Status = EIO;
            goto MainEnd;
        }
    }

    for (Index = 0;
         Index < (PT_COPY_TEST_FILE_SIZE / PT_COPY_TEST_BUFFER_SIZE);
         Index += 1) {

        do {
            BytesCompleted = write(DestinationDescriptor,
                                   Buffer,
                                   PT_COPY_TEST_BUFFER_SIZE);

        } while ((BytesCompleted < 0) && (errno == EINTR));

        if (BytesCompleted < 0) {
            Result->Status = errno;
            goto MainEnd;
        }

        if (BytesCompleted != PT_COPY_TEST_BUFFER_SIZE) {
            Result->Status = EIO;
            goto MainEnd;
        }
    }

    //
    // Let the system process these writes before starting the copies.
    //

    Status = fsync(SourceDescriptor);
    if (Status != 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    Status = fsync(DestinationDescriptor);
    if (Status != 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    //
    // Start the test. This snaps resource usage and starts the clock ticking.
    //

    Status = PtStartTimedTest(Test->Duration);
    if (Status != 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    //
    // Measure the performance of the write() C library routine by counting the
    // number of bytes that can be written.
    //

    while (PtIsTimedTestRunning() != 0) {
        do {
            BytesCompleted = read(SourceDescriptor,
                                  Buffer,
                                  PT_COPY_TEST_BUFFER_SIZE);

        } while ((BytesCompleted < 0) && (errno == EINTR));

        if (BytesCompleted < 0) {
            Result->Status = errno;
            break;
        }

        //
        // If the bytes read did not use the entire buffer, then the end of
        // the file was likely reached. Seek back to the beginning.
        //

        if (BytesCompleted != PT_COPY_TEST_BUFFER_SIZE) {
            Status = lseek(SourceDescriptor, 0, SEEK_SET);
            if (Status != 0) {
                if (Status < 0) {
                    Result->Status = errno;

                } else {
                    Result->Status = EIO;
                }

                break;
            }
        }

        do {
            BytesCompleted = write(DestinationDescriptor,
                                   Buffer,
                                   BytesCompleted);

        } while ((BytesCompleted < 0) && (errno == EINTR));

        if (BytesCompleted < 0) {
            Result->Status = errno;
            break;
        }

        //
        // If a full buffer was not written, then the read likely reached the
        // end of the file above. Seek back to the beginning.
        //

        if (BytesCompleted != PT_COPY_TEST_BUFFER_SIZE) {
            Status = lseek(DestinationDescriptor, 0, SEEK_SET);
            if (Status != 0) {
                if (Status < 0) {
                    Result->Status = errno;

                } else {
                    Result->Status = EIO;
                }

                break;
            }
        }

        TotalBytes += (unsigned long long)BytesCompleted;
    }

    Status = PtFinishTimedTest(Result);
    if ((Status != 0) && (Result->Status == 0)) {
        Result->Status = errno;
    }

MainEnd:
    if (SourceFileCreated != 0) {
        close(SourceDescriptor);
        remove(SourceFileName);
    }

    if (DestinationFileCreated != 0) {
        close(DestinationDescriptor);
        remove(DestinationFileName);
    }

    if (Buffer != NULL) {
        free(Buffer);
    }

    Result->Data.Bytes = TotalBytes;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

