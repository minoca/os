/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    write.c

Abstract:

    This module implements the performance benchmark tests for the write()
    C library routine.

Author:

    Chris Stevens 6-May-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "perftest.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PT_WRITE_TEST_FILE_NAME_LENGTH 48
#define PT_WRITE_TEST_FILE_SIZE (2 * 1024 * 1024)
#define PT_WRITE_TEST_BUFFER_SIZE 4096

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
WriteMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the write performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    char *Buffer;
    ssize_t BytesWritten;
    int FileCreated;
    int FileDescriptor;
    char FileName[PT_WRITE_TEST_FILE_NAME_LENGTH];
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
    // Allocate a buffer for the writes.
    //

    Buffer = malloc(PT_WRITE_TEST_BUFFER_SIZE);
    if (Buffer == NULL) {
        Result->Status = ENOMEM;
        goto MainEnd;
    }

    //
    // Get the process ID and create a process safe file path.
    //

    ProcessId = getpid();
    Status = snprintf(FileName,
                      PT_WRITE_TEST_FILE_NAME_LENGTH,
                      "write_%d.txt",
                      ProcessId);

    if (Status < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    //
    // Create the new file, which will open it write-only.
    //

    FileDescriptor = creat(FileName, S_IRUSR | S_IWUSR);
    if (FileDescriptor < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    FileCreated = 1;

    //
    // As this is really a test of writing to the system's cache, prime the
    // cache with junk data. An alternate test could test always writing to new
    // files or new areas of the same file to measure cache entry creation.
    //

    for (Index = 0;
         Index < (PT_WRITE_TEST_FILE_SIZE / PT_WRITE_TEST_BUFFER_SIZE);
         Index += 1) {

        do {
            BytesWritten = write(FileDescriptor,
                                 Buffer,
                                 PT_WRITE_TEST_BUFFER_SIZE);

        } while ((BytesWritten < 0) && (errno == EINTR));

        if (BytesWritten < 0) {
            Result->Status = errno;
            goto MainEnd;
        }

        if (BytesWritten != PT_WRITE_TEST_BUFFER_SIZE) {
            Result->Status = EIO;
            goto MainEnd;
        }
    }

    //
    // Let the system process these writes before starting the real writes.
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
    // Measure the performance of the write() C library routine by counting the
    // number of bytes that can be written.
    //

    Index = 0;
    while (PtIsTimedTestRunning() != 0) {
        do {
            BytesWritten = write(FileDescriptor,
                                 Buffer,
                                 PT_WRITE_TEST_BUFFER_SIZE);

        } while ((BytesWritten < 0) && (errno == EINTR));

        if (BytesWritten < 0) {
            Result->Status = errno;
            break;
        }

        if (BytesWritten != PT_WRITE_TEST_BUFFER_SIZE) {
            Result->Status = EIO;
            break;
        }

        TotalBytes += (unsigned long long)BytesWritten;
        Index += 1;
        if (Index >= (PT_WRITE_TEST_FILE_SIZE / PT_WRITE_TEST_BUFFER_SIZE)) {
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

