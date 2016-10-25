/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mmap.c

Abstract:

    This module implements the performance benchmark tests for the mmap() and
    munmap() C library routines.

Author:

    Chris Stevens 7-May-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#include "perftest.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PT_MMAP_TEST_FILE_NAME_LENGTH 48
#define PT_MMAP_TEST_REGION_SIZE (2 * 1024 * 1024)
#define PT_MMAP_TEST_BLOCK_SIZE 4096

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
MmapMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the memory map performance benchmark tests.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    void *Address;
    char *Buffer;
    ssize_t BytesWritten;
    int CreateFile;
    char *CurrentAddress;
    char *EndAddress;
    int FileCreated;
    int FileDescriptor;
    char FileName[PT_MMAP_TEST_FILE_NAME_LENGTH];
    int Index;
    unsigned long long Iterations;
    int MmapFlags;
    int PerformIo;
    pid_t ProcessId;
    int ProtectionFlags;
    int Status;
    int Write;

    Buffer = NULL;
    CreateFile = 0;
    FileCreated = 0;
    Iterations = 0;
    PerformIo = 0;
    Result->Type = PtResultIterations;
    Result->Status = 0;
    Write = 0;

    //
    // Determine which protection flags, mmap flags, and file to use based on
    // the test type.
    //

    FileDescriptor = -1;
    ProtectionFlags = PROT_READ | PROT_WRITE;
    switch (Test->TestType) {
    case PtTestMmapIoPrivate:
        PerformIo = 1;

    case PtTestMmapPrivate:
        MmapFlags = MAP_PRIVATE;
        CreateFile = 1;
        break;

    case PtTestMmapIoShared:
        PerformIo = 1;

    case PtTestMmapShared:
        MmapFlags = MAP_SHARED;
        CreateFile = 1;
        break;

    case PtTestMmapIoAnon:
        PerformIo = 1;

    case PtTestMmapAnon:
        MmapFlags = MAP_ANON | MAP_PRIVATE;
        break;

    default:

        assert(0);

        Result->Status = EINVAL;
        return;
    }

    if (CreateFile != 0) {
        ProcessId = getpid();
        Status = snprintf(FileName,
                          PT_MMAP_TEST_FILE_NAME_LENGTH,
                          "mmap_%d.txt",
                          ProcessId);

        if (Status < 0) {
            Result->Status = errno;
            goto MainEnd;
        }

        //
        // Create and open the file with read/write permission so the size can
        // be extended.
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
        // If a file is going to be used for I/O, then prime the page cache.
        //

        if (PerformIo != 0) {
            Buffer = malloc(PT_MMAP_TEST_BLOCK_SIZE);
            if (Buffer == NULL) {
                Result->Status = ENOMEM;
                goto MainEnd;
            }

            memset(Buffer, 0, PT_MMAP_TEST_BLOCK_SIZE);
            for (Index = 0;
                 Index < (PT_MMAP_TEST_REGION_SIZE / PT_MMAP_TEST_BLOCK_SIZE);
                 Index += 1) {

                do {
                    BytesWritten = write(FileDescriptor,
                                         Buffer,
                                         PT_MMAP_TEST_BLOCK_SIZE);

                } while ((BytesWritten < 0) && (errno == EINTR));

                if (BytesWritten < 0) {
                    Result->Status = errno;
                    goto MainEnd;
                }

                if (BytesWritten != PT_MMAP_TEST_BLOCK_SIZE) {
                    Result->Status = EIO;
                    goto MainEnd;
                }
            }

            free(Buffer);
            Buffer = NULL;

            //
            // Let the system process these writes before starting the I/O.
            //

            Status = fsync(FileDescriptor);
            if (Status != 0) {
                Result->Status = errno;
                goto MainEnd;
            }
        }
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
    // Measure the performance of the mmap() and munmap() C library routines
    // and, for some tests, the speed of page faulting mapped regions.
    //

    while (PtIsTimedTestRunning() != 0) {
        Address = mmap(NULL,
                       PT_MMAP_TEST_REGION_SIZE,
                       ProtectionFlags,
                       MmapFlags,
                       FileDescriptor,
                       0);

        if (Address == MAP_FAILED) {
            Result->Status = errno;
            break;
        }

        //
        // If this is an I/O test, then alternating reading and writing each
        // block in the mapped region.
        //

        if (PerformIo != 0) {
            CurrentAddress = (char *)Address;
            EndAddress = CurrentAddress + PT_MMAP_TEST_REGION_SIZE;
            while (CurrentAddress < EndAddress) {
                if (Write == 0) {
                    if (*CurrentAddress != 0) {
                        Result->Status = EIO;
                    }

                    Write = 1;

                } else {
                    *CurrentAddress = 0x1;
                    Write = 0;
                }

                CurrentAddress += PT_MMAP_TEST_BLOCK_SIZE;
            }
        }

        Status = munmap(Address, PT_MMAP_TEST_REGION_SIZE);
        if (Status != 0) {
            Result->Status = errno;
            break;
        }

        Iterations += 1;
    }

    Status = PtFinishTimedTest(Result);
    if ((Status != 0) && (Result->Status == 0)) {
        Result->Status = errno;
    }

MainEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    if (FileCreated != 0) {
        close(FileDescriptor);
        remove(FileName);
    }

    Result->Data.Iterations = Iterations;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

