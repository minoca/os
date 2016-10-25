/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    aiotest.c

Abstract:

    This module implements the asynchronous I/O test suite.

Author:

    Evan Green 27-Jun-2016

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <minoca/lib/types.h>

//
// --------------------------------------------------------------------- Macros
//

#define ERROR(...) fprintf(stderr, __VA_ARGS__)

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
TestAioRun (
    VOID
    );

ULONG
TestAioExecute (
    int Pipe[2]
    );

void
TestAioSigioHandler (
    int Signal,
    siginfo_t *Information,
    void *Context
    );

//
// -------------------------------------------------------------------- Globals
//

ULONG TestAioSignalCount;

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

    This routine implements the signal test program.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONG Failures;

    Failures = TestAioRun();
    if (Failures == 0) {
        return 0;
    }

    ERROR("*** %u failures in async I/O test. ***\n", Failures);
    return 1;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
TestAioRun (
    VOID
    )

/*++

Routine Description:

    This routine runs all asynchronous I/O tests.

Arguments:

    None.

Return Value:

    0 on success.

    Returns the number of errors on failure.

--*/

{

    struct sigaction Action;
    ULONG Failures;
    struct sigaction OldAction;
    int Pipe[2];
    int Status;

    Failures = 0;
    memset(&Action, 0, sizeof(Action));
    Action.sa_sigaction = TestAioSigioHandler;
    Action.sa_flags = SA_SIGINFO;
    TestAioSignalCount = 0;
    sigaction(SIGIO, &Action, &OldAction);
    Status = pipe(Pipe);
    if (Status != 0) {
        ERROR("Failed to create pipe.\n");
        Failures += 1;
        goto TestAioRunEnd;
    }

    Failures += TestAioExecute(Pipe);
    Status = socketpair(AF_UNIX, SOCK_STREAM, 0, Pipe);
    if (Status != 0) {
        ERROR("Failed to create socketpair.\n");
        Failures += 1;
        goto TestAioRunEnd;
    }

    Failures += TestAioExecute(Pipe);

TestAioRunEnd:
    sigaction(SIGIO, &OldAction, NULL);
    return Failures;
}

ULONG
TestAioExecute (
    int Pipe[2]
    )

/*++

Routine Description:

    This routine executes the asynchronous tests.

Arguments:

    Pipe - Supplies the pair of descriptors to use for testing. This routine
        will close these descriptors.

Return Value:

    0 on success.

    Returns the number of errors on failure.

--*/

{

    int Count;
    char Buf[3];
    ULONG Failures;
    int Flags;
    pid_t Pid;

    if (TestAioSignalCount != 0) {
        Failures += TestAioSignalCount;
        ERROR("Unexpected signals before test\n");
        Failures += TestAioSignalCount;
        TestAioSignalCount = 0;
    }

    //
    // Enable SIGIO for both the read and write side. Use the fcntl method
    // for one and the ioctl for the other, and verify both.
    //

    Failures = 0;
    Pid = getpid();
    if ((fcntl(Pipe[0], F_SETOWN, Pid) != 0) ||
        (fcntl(Pipe[1], F_SETOWN, Pid) != 0) ||
        (fcntl(Pipe[0], F_GETOWN) != Pid) ||
        (fcntl(Pipe[1], F_GETOWN) != Pid)) {

        ERROR("Failed to F_SETOWN.\n");
        Failures += 1;
        goto TestAioExecuteEnd;
    }

    //
    // Reading a writing now should still not generate a signal.
    //

    write(Pipe[1], "o", 1);
    read(Pipe[0], Buf, 1);
    if (TestAioSignalCount != 0) {
        Failures += TestAioSignalCount;
        ERROR("Signals generated before O_ASYNC is set.\n");
        Failures += TestAioSignalCount;
        TestAioSignalCount = 0;
    }

    Flags = fcntl(Pipe[1], F_GETFL);
    if (Flags < 0) {
        ERROR("Failed to F_GETFL.\n");
        Failures += 1;
        goto TestAioExecuteEnd;
    }

    Flags |= O_ASYNC | O_NONBLOCK;
    if (fcntl(Pipe[1], F_SETFL, Flags) != 0) {
        ERROR("Failed to F_SETFL.\n");
        Failures += 1;
        goto TestAioExecuteEnd;
    }

    Flags = 1;
    if (ioctl(Pipe[0], FIOASYNC, &Flags) != 0) {
        ERROR("Failed to ioctl.\n");
        Failures += 1;
        goto TestAioExecuteEnd;
    }

    Flags = fcntl(Pipe[0], F_GETFL);
    if ((Flags < 0) || ((Flags & O_ASYNC) == 0)) {
        ERROR("Failed to get flags: %x\n", Flags);
        Failures += 1;
        goto TestAioExecuteEnd;
    }

    Flags = fcntl(Pipe[1], F_GETFL);
    if ((Flags < 0) || ((Flags & O_ASYNC) == 0)) {
        ERROR("Failed to get flags 2: %x\n", Flags);
        Failures += 1;
        goto TestAioExecuteEnd;
    }

    //
    // Simply turning async I/O on should not trigger an edge.
    //

    if (TestAioSignalCount != 0) {
        Failures += TestAioSignalCount;
        ERROR("Signals sent while turing AIO on.\n");
        Failures += TestAioSignalCount;
        TestAioSignalCount = 0;
    }

    //
    // Write something to generate a read edge.
    //

    write(Pipe[1], "123", 3);
    read(Pipe[0], Buf, 2);
    read(Pipe[0], Buf, 1);
    if (TestAioSignalCount != 1) {
        ERROR("Failed basic read AIO signal.\n");
        Failures += 1;
    }

    TestAioSignalCount = 0;

    //
    // Fill the buffer.
    //

    Count = 0;
    while (write(Pipe[1], "x", 1) == 1) {
        Count += 1;
    }

    //
    // This should generate a read edge.
    //

    if (TestAioSignalCount != 1) {
        ERROR("Failed basic read AIO signal 2.\n");
        Failures += 1;
    }

    TestAioSignalCount = 0;

    //
    // Reading a character should generate a write edge.
    //

    read(Pipe[0], Buf, 1);
    Count -= 1;
    if (TestAioSignalCount != 1) {
        ERROR("Failed basic write AIO signal.\n");
        Failures += 1;
    }

    TestAioSignalCount = 0;

    //
    // Read the rest of the characters.
    //

    while (Count != 0) {
        if (read(Pipe[0], Buf, 1) != 1) {
            ERROR("Failed read.\n");
            Failures += 1;
        }

        Count -= 1;
    }

    //
    // There should be no more signals just from draining buffer.
    //

    if (TestAioSignalCount != 0) {
        ERROR("Got extra AIO signals.\n");
        Failures += 1;
        TestAioSignalCount = 0;
    }

TestAioExecuteEnd:
    Flags = 0;
    if ((ioctl(Pipe[0], FIOASYNC, &Flags) != 0) ||
        (ioctl(Pipe[1], FIOASYNC, &Flags) != 0)) {

        ERROR("Failed to clear async.\n");
        Failures += 1;
    }

    if (TestAioSignalCount != 0) {
        ERROR("Got extra AIO while disabling async.\n");
        Failures += 1;
        TestAioSignalCount = 0;
    }

    close(Pipe[0]);
    close(Pipe[1]);
    if (TestAioSignalCount != 0) {
        ERROR("Got extra AIO while closing.\n");
        Failures += 1;
        TestAioSignalCount = 0;
    }

    return Failures;
}

void
TestAioSigioHandler (
    int Signal,
    siginfo_t *Information,
    void *Context
    )

/*++

Routine Description:

    This routine is called when an I/O signal comes in.

Arguments:

    Signal - Supplies the signal that occurred. This should always be SIGIO.

    Information - Supplies a pointer to the signal information.

    Context - Supplies an unused context pointer.

Return Value:

    None.

--*/

{

    assert(Signal == SIGIO);

    TestAioSignalCount += 1;
    return;
}

