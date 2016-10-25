/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    read.c

Abstract:

    This module implements the performance benchmark tests for the read() C
    library routine.

Author:

    Chris Stevens 6-May-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "perftest.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PT_READ_TEST_FILE_NAME_LENGTH 48
#define PT_READ_TEST_FILE_SIZE (2 * 1024 * 1024)
#define PT_READ_TEST_BUFFER_SIZE 4096

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
ReadMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the read performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    char *Buffer;
    ssize_t BytesRead;
    ssize_t BytesWritten;
    int FileCreated;
    int FileDescriptor;
    char FileName[PT_READ_TEST_FILE_NAME_LENGTH];
    int Index;
    pid_t ProcessId;
    int Status;
    unsigned long long TotalBytes;

    FileCreated = 0;
    FileDescriptor = -1;
    Result->Type = PtResultBytes;
    Result->Status = 0;
    TotalBytes = 0;

    //
    // Allocate a buffer for the reads.
    //

    Buffer = malloc(PT_READ_TEST_BUFFER_SIZE);
    if (Buffer == NULL) {
        Result->Status = ENOMEM;
        goto MainEnd;
    }

    //
    // Get the process ID and create a process safe file path.
    //

    ProcessId = getpid();
    Status = snprintf(FileName,
                      PT_READ_TEST_FILE_NAME_LENGTH,
                      "read_%d.txt",
                      ProcessId);

    if (Status < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    //
    // Create and open the file with read/write permission so the size can be
    // extended.
    //

    FileDescriptor = open(FileName,
                          O_RDWR | O_CREAT | O_TRUNC,
                          S_IRUSR | S_IWUSR);

    if (FileDescriptor < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    FileCreated = 1;

    //
    // As this is really a test of reading from the system's cache, prime the
    // cache with junk data. ftruncate() could be used here but POSIX does not
    // require systems to support file extension and the goal is not to test
    // various implementations of fruncate() file extension.
    //

    for (Index = 0;
         Index < (PT_READ_TEST_FILE_SIZE / PT_READ_TEST_BUFFER_SIZE);
         Index += 1) {

        do {
            BytesWritten = write(FileDescriptor,
                                 Buffer,
                                 PT_READ_TEST_BUFFER_SIZE);

        } while ((BytesWritten < 0) && (errno == EINTR));

        if (BytesWritten < 0) {
            Result->Status = errno;
            goto MainEnd;
        }

        if (BytesWritten != PT_READ_TEST_BUFFER_SIZE) {
            Result->Status = EIO;
            goto MainEnd;
        }
    }

    //
    // Let the system process these writes before starting the reads.
    //

    Status = fsync(FileDescriptor);
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
    // Measure the performance of the read() C library routine by counting the
    // number of bytes that can be read in.
    //

    while (PtIsTimedTestRunning() != 0) {
        do {
            BytesRead = read(FileDescriptor, Buffer, PT_READ_TEST_BUFFER_SIZE);

        } while ((BytesRead < 0) && (errno == EINTR));

        if (BytesRead < 0) {
            Result->Status = errno;
            break;
        }

        //
        // If the bytes read did not fill the entire buffer, then the end of
        // the file was likely reached. Seek back to the beginning.
        //

        if (BytesRead != PT_READ_TEST_BUFFER_SIZE) {
            Status = lseek(FileDescriptor, 0, SEEK_SET);
            if (Status != 0) {
                if (Status < 0) {
                    Result->Status = errno;

                } else {
                    Result->Status = EIO;
                }

                break;
            }
        }

        TotalBytes += (unsigned long long)BytesRead;
    }

    Status = PtFinishTimedTest(Result);
    if ((Status != 0) && (Result->Status == 0)) {
        Result->Status = errno;
    }

MainEnd:
    if (FileCreated != 0) {
        close(FileDescriptor);
        remove(FileName);
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

