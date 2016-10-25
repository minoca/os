/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    socktest.c

Abstract:

    This module implements an application that tests out the system's socket
    functionality.

Author:

    Evan Green 6-May-2013

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
TestTransmitThroughput (
    ULONG ChunkSize,
    ULONG ChunkCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine implements the socket test program.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    return TestTransmitThroughput(64 * 1024, 16);
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
TestTransmitThroughput (
    ULONG ChunkSize,
    ULONG ChunkCount
    )

/*++

Routine Description:

    This routine tests transmitting a large amount of data out of a socket.

Arguments:

    ChunkSize - Supplies the size of each buffer passed to the send() function.

    ChunkCount - Supplies the number of chunks that will be sent.

Return Value:

    Returns the number of failures that occurred in the test.

--*/

{

    ULONG ByteIndex;
    int BytesSent;
    struct sockaddr_in DestinationHost;
    ULONG Errors;
    ULONG LoopIndex;
    int Result;
    PCHAR TestSendBuffer;
    int TestSocket;

    Errors = 0;
    TestSendBuffer = NULL;
    TestSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (TestSocket == -1) {
        printf("socket() failed. Errno = %d.\n", errno);
        Errors += 1;
        goto TestTransmitThroughputEnd;
    }

    //
    // TODO: Replace this with a real function (and not just a hard-coded
    // network order IP address.
    //

    DestinationHost.sin_family = AF_INET;
    DestinationHost.sin_port = htons(7653);
    DestinationHost.sin_addr.s_addr = (192 << 0) | (168 << 8) | (1 << 16) |
                                      (19 << 24);

    //
    // Connect to the remote host.
    //

    printf("Connecting to host...");
    Result = connect(TestSocket,
                     (struct sockaddr *)&DestinationHost,
                     sizeof(struct sockaddr_in));

    if (Result == 0) {
        printf("Connected.\n");

    } else {
        printf("Failed: Return value %d, errno = %d.\n", Result, errno);
        Errors += 1;
        goto TestTransmitThroughputEnd;
    }

    //
    // Allocate and initialize a big old test buffer.
    //

    TestSendBuffer = malloc(ChunkSize);
    if (TestSendBuffer == NULL) {
        printf("Failed to allocate %d bytes.\n", ChunkSize);
        Errors += 1;
        goto TestTransmitThroughputEnd;
    }

    for (ByteIndex = 0; ByteIndex < ChunkSize; ByteIndex += 1) {
        if ((ByteIndex & 0x1) == 0) {
            TestSendBuffer[ByteIndex] = (UCHAR)ByteIndex;

        } else {
            TestSendBuffer[ByteIndex] = (UCHAR)(ByteIndex >> 8);
        }
    }

    //
    // Loop sending data hardcore.
    //

    for (LoopIndex = 0; LoopIndex < ChunkCount; LoopIndex += 1) {
        BytesSent = send(TestSocket, TestSendBuffer, ChunkSize, 0);
        if (BytesSent == -1) {
            printf("Error: Failed to send chunk. errno = %d.\n", errno);
            Errors += 1;
        }

        if (BytesSent != ChunkSize) {
            printf("Error: send() sent only %d of %d bytes.\n",
                   BytesSent,
                   ChunkSize);

            Errors += 1;
        }

        //
        // At some point stop just stupidly printing out failures and give
        // up.
        //

        if (Errors > 10) {
            goto TestTransmitThroughputEnd;
        }
    }

TestTransmitThroughputEnd:
    if (TestSendBuffer != NULL) {
        free(TestSendBuffer);
    }

    close(TestSocket);
    printf("TestTransmitThroughput done. %d errors found.\n", Errors);
    return Errors;
}

