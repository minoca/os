/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uos.c

Abstract:

    This module implements POSIX-specific Chalk spawn functionality.

Author:

    Evan Green 23-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <minoca/lib/types.h>

#include "spawnos.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _CK_SIGCHLD_CONTEXT {
    struct sigaction OriginalAction;
    struct sigaction OriginalPipeAction;
    int Pipe[2];
} CK_SIGCHLD_CONTEXT, *PCK_SIGCHLD_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
CkpInstallChildSignalHandler (
    PCK_SIGCHLD_CONTEXT Context
    );

VOID
CkpRemoveChildSignalHandler (
    PCK_SIGCHLD_CONTEXT Context
    );

VOID
CkpChildSignalHandler (
    int Signal
    );

int
CkpOsWaitPid (
    PSPAWN_ATTRIBUTES Attributes,
    INT Options
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a global pointer to the child signal context. This unfortunately makes
// these functions single threaded only, but they probably were anyway due
// to the fact that they fork.
//

PCK_SIGCHLD_CONTEXT CkSigchldContext;

//
// ------------------------------------------------------------------ Functions
//

INT
CkpOsSpawn (
    PSPAWN_ATTRIBUTES Attributes
    )

/*++

Routine Description:

    This routine spawns a subprocess.

Arguments:

    Attributes - Supplies the attributes of the process to launch.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    UINTN ArgumentCount;
    PSTR *Arguments;
    PCSTR Executable;
    int ExitCode;
    pid_t Pid;
    int ReadStatus;
    PSTR Shell;
    PSTR *ShellArguments;
    int Status;
    int StatusPipe[2];
    pid_t WaitPid;

    Arguments = Attributes->Arguments;
    Shell = NULL;
    Executable = Attributes->Executable;
    if (Executable == NULL) {
        Executable = Attributes->Arguments[0];
    }

    if ((Attributes->Options & SPAWN_OPTION_SHELL) != 0) {
        Shell = getenv("SHELL");
        if (Shell == NULL) {
            Shell = "/bin/sh";
        }

        Executable = Shell;
        ArgumentCount = 0;
        while (Arguments[ArgumentCount] != NULL) {
            ArgumentCount += 1;
        }

        ShellArguments = alloca(sizeof(PSTR) * ArgumentCount + 3);
        ShellArguments[0] = Shell;
        ShellArguments[1] = "-c";
        memcpy(&(ShellArguments[2]),
               Arguments,
               (ArgumentCount + 1) * sizeof(PSTR));

        Arguments = ShellArguments;
    }

    if (pipe(StatusPipe) != 0) {
        return -1;
    }

    Pid = fork();
    if (Pid < 0) {
        close(StatusPipe[0]);
        close(StatusPipe[1]);
        return -1;
    }

    //
    // If this is the child, fix up stdin/out/err and launch the process.
    //

    if (Pid == 0) {
        close(StatusPipe[0]);
        Status = 0;
        errno = 0;

        //
        // Set the write end of the pipe to close on execute.
        //

        fcntl(StatusPipe[1], F_SETFD, FD_CLOEXEC);
        if (Attributes->Stdin.Fd >= 0) {
            dup2(Attributes->Stdin.Fd, STDIN_FILENO);
        }

        if (Attributes->Stdout.Fd >= 0) {
            dup2(Attributes->Stdout.Fd, STDOUT_FILENO);
        }

        if (Attributes->Stderr.Fd >= 0) {
            dup2(Attributes->Stderr.Fd, STDERR_FILENO);
        }

        if (Attributes->Stdin.ParentPipe >= 0) {
            close(Attributes->Stdin.ParentPipe);
        }

        if (Attributes->Stdout.ParentPipe >= 0) {
            close(Attributes->Stdout.ParentPipe);
        }

        if (Attributes->Stderr.ParentPipe >= 0) {
            close(Attributes->Stderr.ParentPipe);
        }

        if ((Attributes->Options & SPAWN_OPTION_NEW_SESSION) != 0) {
            setsid();
        }

        if (errno != 0) {
            Status = errno;
            write(StatusPipe[1], &Status, sizeof(Status));
            _exit(126);
        }

        if ((Attributes->Cwd != NULL) && (chdir(Attributes->Cwd) != 0)) {
            Status = errno;
            write(StatusPipe[1], &Status, sizeof(Status));
            _exit(125);
        }

        if (Attributes->Environment != NULL) {
            execve(Executable, Arguments, Attributes->Environment);

        } else {
            execv(Executable, Arguments);
        }

        //
        // Exec didn't work. Report back on the status pipe and die.
        //

        Status = errno;
        write(StatusPipe[1], &Status, sizeof(Status));
        _exit(127);
    }

    //
    // In the parent, read from the status pipe, which will either come back
    // with EOF on a successful exec, or return a status on failure.
    //

    close(StatusPipe[1]);
    Status = 0;
    do {
        ReadStatus = read(StatusPipe[0], &Status, sizeof(Status));

    } while ((ReadStatus < 0) && (errno == EINTR));

    close(StatusPipe[0]);
    if ((Attributes->Debug &
         (SPAWN_DEBUG_BASIC_LAUNCH | SPAWN_DEBUG_DETAILED_LAUNCH)) != 0) {

        fprintf(stderr,
                "Launch%s %s\n",
                (ReadStatus == 0) ? "ed" : " failed",
                Executable);
    }

    if ((Attributes->Debug & SPAWN_DEBUG_DETAILED_LAUNCH) != 0) {
        fprintf(stderr, "CommandLine: ");
        ArgumentCount = 0;
        while (Arguments[ArgumentCount] != NULL) {
            fprintf(stderr, "%s ", Arguments[ArgumentCount]);
            ArgumentCount += 1;
        }

        fprintf(stderr,
                "\nCwd: %s\n"
                "Stdin/out/err: %d/%d/%d\n"
                "Pid: %d\n",
                Attributes->Cwd,
                Attributes->Stdin.Fd,
                Attributes->Stdout.Fd,
                Attributes->Stderr.Fd,
                (int)Pid);
    }

    //
    // Close the child sides of the file descriptors so that the parent
    // sides will close up when the child dies.
    //

    if (Attributes->Stdin.CloseFd >= 0) {
        close(Attributes->Stdin.CloseFd);
        Attributes->Stdin.CloseFd = -1;
    }

    if (Attributes->Stdout.CloseFd >= 0) {
        close(Attributes->Stdout.CloseFd);
        Attributes->Stdout.CloseFd = -1;
    }

    if (Attributes->Stderr.CloseFd >= 0) {
        close(Attributes->Stderr.CloseFd);
        Attributes->Stderr.CloseFd = -1;
    }

    if (ReadStatus != 0) {
        ExitCode = -1;

        //
        // Failed to read or the read returned a value. Reap the child.
        //

        do {
            WaitPid = waitpid(Pid, &ExitCode, 0);

        } while ((WaitPid < 0) && (errno == EINTR));

        if (ReadStatus == sizeof(Status)) {
            errno = Status;
        }

        if ((Attributes->Debug & SPAWN_DEBUG_DETAILED_LAUNCH) != 0) {
            fprintf(stderr, "Child exited with status %d", ExitCode);
        }

        return -1;
    }

    Attributes->Pid = Pid;
    return 0;
}

INT
CkpOsWait (
    PSPAWN_ATTRIBUTES Attributes,
    int Milliseconds
    )

/*++

Routine Description:

    This routine waits for the process to exit. It sets the return code if
    the process exited, and sets the return value.

Arguments:

    Attributes - Supplies a pointer to the attributes.

    Milliseconds - Supplies the number of milliseconds to wait.

Return Value:

    0 on success.

    1 on timeout.

    -1 on failure.

--*/

{

    struct pollfd Pollfd;
    CK_SIGCHLD_CONTEXT SigchldContext;
    INT Status;

    if (Attributes->Pid < 0) {
        return 0;
    }

    //
    // Install a child signal handler to avoid sigtimedwait, which isn't
    // present on Mac OS X.
    //

    if (CkpInstallChildSignalHandler(&SigchldContext)) {
        return -1;
    }

    //
    // Check to see if the process exited while the handler was being installed.
    //

    if (CkpOsWaitPid(Attributes, WNOHANG) == 0) {
        Status = 0;
        goto OsWaitEnd;
    }

    //
    // Loop going down for a poll on the pipe.
    //

    while (TRUE) {
        memset(&Pollfd, 0, sizeof(Pollfd));
        Pollfd.fd = SigchldContext.Pipe[0];
        Pollfd.events = POLLIN;
        if (Milliseconds < 0) {
            Milliseconds = -1;
        }

        Status = poll(&Pollfd, 1, Milliseconds);

        //
        // Check to see if the process ended no matter what.
        //

        if (CkpOsWaitPid(Attributes, WNOHANG) == 0) {
            Status = 0;
            break;
        }

        //
        // Check for a timeout or error.
        //

        if (Status <= 0) {
            if (Status == 0) {
                Status = 1;
            }

            goto OsWaitEnd;
        }
    }

OsWaitEnd:
    CkpRemoveChildSignalHandler(&SigchldContext);
    return Status;
}

INT
CkpOsCommunicate (
    PSPAWN_ATTRIBUTES Attributes,
    const char *Input,
    size_t InputSize,
    int Milliseconds,
    char **OutData,
    size_t *OutDataSize,
    char **ErrorData,
    size_t *ErrorDataSize
    )

/*++

Routine Description:

    This routine communicates with the subprocess, and waits for it to
    terminate.

Arguments:

    Attributes - Supplies a pointer to the attributes.

    Input - Supplies an optional pointer to the input to send into the
        process.

    InputSize - Supplies the size of the input data in bytes.

    Milliseconds - Supplies the number of milliseconds to wait.

    OutData - Supplies a pointer where the data from stdout will be returned.
        The caller is responsible for freeing this buffer.

    OutDataSize - Supplies the number of bytes in the output data buffer.

    ErrorData - Supplies a pointer where the data from stderr will be returned.
        The caller is responsible for freeing this buffer.

    ErrorDataSize - Supplies a pointer where the size of the stderr data will
        be returned.

Return Value:

    0 on success.

    1 on timeout.

    -1 on failure.

--*/

{

    ssize_t BytesDone;
    PVOID Error;
    size_t ErrorCapacity;
    size_t ErrorSize;
    PVOID NewBuffer;
    int OriginalInFlags;
    PVOID Out;
    size_t OutCapacity;
    size_t OutSize;
    pid_t Pid;
    INT PollCount;
    struct pollfd Pollfd[4];
    INT PollfdCount;
    INT PollIndex;
    BOOL ProcessExited;
    CK_SIGCHLD_CONTEXT SigchldContext;
    INT Status;

    Error = NULL;
    ErrorCapacity = 0;
    ErrorSize = 0;
    OriginalInFlags = -1;
    Out = NULL;
    OutCapacity = 0;
    OutSize = 0;
    Pid = Attributes->Pid;
    ProcessExited = FALSE;
    PollfdCount = 0;

    //
    // Install a child signal handler to avoid sigtimedwait, which isn't
    // present on Mac OS X.
    //

    if (CkpInstallChildSignalHandler(&SigchldContext)) {
        return -1;
    }

    //
    // Add the sigchld pipe if the process hasn't been collected yet. Do an
    // explicit check now to see if the process died while the signal handler
    // was being installed.
    //

    if (Pid > 0) {
        if (CkpOsWaitPid(Attributes, WNOHANG) == 0) {
            ProcessExited = TRUE;
            Milliseconds = 0;

        } else {
            Pollfd[PollfdCount].fd = SigchldContext.Pipe[0];
            Pollfd[PollfdCount].events = POLLIN;
            PollfdCount += 1;
        }

    } else {
        ProcessExited = TRUE;
    }

    //
    // Allocate initial buffers, and add the pipes to the poll fds.
    //

    if (Attributes->Stdout.ParentPipe >= 0) {
        OutCapacity = 2048;
        Out = malloc(OutCapacity);
        if (Out == NULL) {
            return -1;
        }

        Pollfd[PollfdCount].fd = Attributes->Stdout.ParentPipe;
        Pollfd[PollfdCount].events = POLLIN;
        PollfdCount += 1;
    }

    if (Attributes->Stderr.ParentPipe >= 0) {
        ErrorCapacity = 2048;
        Error = malloc(ErrorCapacity);
        if (Error == NULL) {
            return -1;
        }

        Pollfd[PollfdCount].fd = Attributes->Stderr.ParentPipe;
        Pollfd[PollfdCount].events = POLLIN;
        PollfdCount += 1;
    }

    if ((InputSize != 0) && (Attributes->Stdin.ParentPipe >= 0)) {
        Pollfd[PollfdCount].fd = Attributes->Stdin.ParentPipe;
        Pollfd[PollfdCount].events = POLLOUT;
        PollfdCount += 1;

        //
        // Make the input non-blocking.
        //

        OriginalInFlags = fcntl(Attributes->Stdin.ParentPipe, F_GETFL, 0);
        if (OriginalInFlags < 0) {
            Status = -1;
            goto OsCommunicateEnd;
        }

        Status = fcntl(Attributes->Stdin.ParentPipe,
                       F_SETFL,
                       OriginalInFlags | O_NONBLOCK);

        if (Status != 0) {
            goto OsCommunicateEnd;
        }
    }

    if (Milliseconds < 0) {
        Milliseconds = -1;
    }

    //
    // Loop polling.
    //

    Status = 0;
    while (PollfdCount != 0) {
        do {
            PollCount = poll(Pollfd, PollfdCount, Milliseconds);

        } while ((PollCount < 0) && (errno == EINTR));

        if ((Attributes->Debug & SPAWN_DEBUG_IO) != 0) {
            fprintf(stderr,
                    "Communicate %d: Polled %d of %d descriptors.\n",
                    Pid,
                    PollCount,
                    PollfdCount);
        }

        //
        // Handle failure, then timeout.
        //

        if (PollCount < 0) {
            Status = -1;
            goto OsCommunicateEnd;
        }

        if (PollCount == 0) {
            if ((ProcessExited != FALSE) || (OutSize + ErrorSize != 0)) {
                Status = 0;

            } else {
                Status = 1;
            }

            goto OsCommunicateEnd;
        }

        //
        // Loop over all the poll descriptors.
        //

        PollIndex = 0;
        while (PollCount != 0) {
            if (Pollfd[PollIndex].revents == 0) {
                PollIndex += 1;
                continue;
            }

            assert(PollIndex < PollfdCount);

            PollCount -= 1;

            //
            // Write to stdin. SIGPIPE is blocked, though writing input to
            // a broken pipe is still considered a failure.
            //

            if (Pollfd[PollIndex].fd == Attributes->Stdin.ParentPipe) {
                do {
                    BytesDone = write(Attributes->Stdin.ParentPipe,
                                      Input,
                                      InputSize);

                } while ((BytesDone < 0) && (errno == EINTR));

                if ((Attributes->Debug & SPAWN_DEBUG_IO) != 0) {
                    fprintf(stderr,
                            "Communicate %d: Wrote %d of %d to stdin.\n",
                            Pid,
                            (int)BytesDone,
                            (int)InputSize);
                }

                if (BytesDone <= 0) {
                    goto OsCommunicateEnd;
                }

                Input += BytesDone;
                InputSize -= BytesDone;

                //
                // If the input is finished, remove the input descriptor from
                // the poll set.
                //

                if (InputSize == 0) {
                    Pollfd[PollIndex] = Pollfd[PollfdCount - 1];
                    PollfdCount -= 1;
                    continue;
                }

            //
            // Read from the stdout pipe.
            //

            } else if (Pollfd[PollIndex].fd == Attributes->Stdout.ParentPipe) {
                if (OutSize >= OutCapacity) {
                    if (OutCapacity >= CK_SPAWN_MAX_OUTPUT) {
                        errno = ENOMEM;
                        Status = -1;
                        goto OsCommunicateEnd;
                    }

                    OutCapacity *= 2;
                    NewBuffer = realloc(Out, OutCapacity);
                    if (NewBuffer == NULL) {
                        Status = -1;
                        goto OsCommunicateEnd;
                    }

                    Out = NewBuffer;
                }

                do {
                    BytesDone = read(Attributes->Stdout.ParentPipe,
                                     Out + OutSize,
                                     OutCapacity - OutSize);

                } while ((BytesDone < 0) && (errno == EINTR));

                if ((Attributes->Debug & SPAWN_DEBUG_IO) != 0) {
                    fprintf(stderr,
                            "Communicate %d: Read %d from stdout.\n",
                            Pid,
                            (int)BytesDone);
                }

                if (BytesDone < 0) {
                    Status = -1;
                    goto OsCommunicateEnd;
                }

                //
                // On EOF, remove this descriptor from the running.
                //

                if (BytesDone == 0) {
                    Pollfd[PollIndex] = Pollfd[PollfdCount - 1];
                    PollfdCount -= 1;
                    continue;
                }

                OutSize += BytesDone;

            //
            // Read from the stderr pipe.
            //

            } else if (Pollfd[PollIndex].fd == Attributes->Stderr.ParentPipe) {
                if (ErrorSize >= ErrorCapacity) {
                    if (ErrorCapacity >= CK_SPAWN_MAX_OUTPUT) {
                        errno = ENOMEM;
                        Status = -1;
                        goto OsCommunicateEnd;
                    }

                    ErrorCapacity *= 2;
                    NewBuffer = realloc(Error, ErrorCapacity);
                    if (NewBuffer == NULL) {
                        Status = -1;
                        goto OsCommunicateEnd;
                    }

                    Error = NewBuffer;
                }

                do {
                    BytesDone = read(Attributes->Stderr.ParentPipe,
                                     Error + ErrorSize,
                                     ErrorCapacity - ErrorSize);

                } while ((BytesDone < 0) && (errno == EINTR));

                if ((Attributes->Debug & SPAWN_DEBUG_IO) != 0) {
                    fprintf(stderr,
                            "Communicate %d: Read %d from stderr.\n",
                            Pid,
                            (int)BytesDone);
                }

                if (BytesDone < 0) {
                    Status = -1;
                    goto OsCommunicateEnd;
                }

                //
                // On EOF, remove this descriptor from the running.
                //

                if (BytesDone == 0) {
                    Pollfd[PollIndex] = Pollfd[PollfdCount - 1];
                    PollfdCount -= 1;
                    continue;
                }

                ErrorSize += BytesDone;

            //
            // See if the process ended.
            //

            } else {

                assert(Pollfd[PollIndex].fd == SigchldContext.Pipe[0]);

                //
                // Clean out the pipe byte that triggered this poll.
                //

                read(SigchldContext.Pipe[0], &Status, 1);
                if (CkpOsWaitPid(Attributes, WNOHANG) == 0) {
                    Pollfd[PollIndex] = Pollfd[PollfdCount - 1];
                    PollfdCount -= 1;
                    ProcessExited = TRUE;
                    Milliseconds = 0;
                    continue;
                }
            }

            PollIndex += 1;
        }
    }

OsCommunicateEnd:
    if (Status < 0) {
        OutSize = 0;
        ErrorSize = 0;
    }

    if ((Out != NULL) && (OutSize == 0)) {
        free(Out);
        Out = NULL;
    }

    if ((Error != NULL) && (ErrorSize == 0)) {
        free(Error);
        Error = NULL;
    }

    *OutData = Out;
    *OutDataSize = OutSize;
    *ErrorData = Error;
    *ErrorDataSize = ErrorSize;

    //
    // Restore the mode of the input descriptor.
    //

    if (OriginalInFlags >= 0) {
        fcntl(Attributes->Stdin.ParentPipe, F_SETFL, OriginalInFlags);
    }

    CkpRemoveChildSignalHandler(&SigchldContext);
    return Status;
}

INT
CkpOsSendSignal (
    PSPAWN_ATTRIBUTES Attributes,
    INT Signal
    )

/*++

Routine Description:

    This routine sends a signal to the process. On Windows, it calls
    TerminateProcess for SIGTERM and SIGKILL.

Arguments:

    Attributes - Supplies a pointer to the attributes.

    Signal - Supplies the signal to send to the process.

Return Value:

    0 on success.

    -1 on failure.

--*/

{

    INT Status;

    Status = 0;
    if (Attributes->Pid > 0) {
        Status = kill(Attributes->Pid, Signal);
    }

    return Status;
}

VOID
CkpOsTearDownSpawnAttributes (
    PSPAWN_ATTRIBUTES Attributes
    )

/*++

Routine Description:

    This routine closes all OS-specific resources associated with a spawn
    attributes structure.

Arguments:

    Attributes - Supplies a pointer to the attributes to tear down.

Return Value:

    None.

--*/

{

    //
    // Make a cheap last ditch effort to reap the pid. This may not always work.
    //

    if (Attributes->Pid > 0) {
        waitpid(Attributes->Pid, NULL, WNOHANG);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
CkpInstallChildSignalHandler (
    PCK_SIGCHLD_CONTEXT Context
    )

/*++

Routine Description:

    This routine installs a child signal handler with a pipe in it. This
    routine is single threaded only.

Arguments:

    Context - Supplies a pointer where the filled out context will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    struct sigaction Action;

    memset(Context, 0, sizeof(CK_SIGCHLD_CONTEXT));
    memset(&Action, 0, sizeof(Action));
    Action.sa_handler = CkpChildSignalHandler;
    if (sigaction(SIGCHLD, &Action, &(Context->OriginalAction)) != 0) {
        return -1;
    }

    if (pipe(Context->Pipe) != 0) {
        sigaction(SIGCHLD, &(Context->OriginalAction), NULL);
        return -1;
    }

    //
    // Also block SIGPIPE since it's a convenient place to do it. This is
    // really only needed by the communicate mechanism, not the child signal
    // handler.
    //

    Action.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &Action, &(Context->OriginalPipeAction));

    assert(CkSigchldContext == NULL);

    CkSigchldContext = Context;
    return 0;
}

VOID
CkpRemoveChildSignalHandler (
    PCK_SIGCHLD_CONTEXT Context
    )

/*++

Routine Description:

    This routine uninstalls a child signal handler. This routine is single-
    threaded only.

Arguments:

    Context - Supplies a pointer where the filled out context will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    assert(CkSigchldContext == Context);

    sigaction(SIGCHLD, &(Context->OriginalAction), NULL);
    sigaction(SIGPIPE, &(Context->OriginalPipeAction), NULL);
    close(Context->Pipe[0]);
    close(Context->Pipe[1]);
    CkSigchldContext = NULL;
    return;
}

VOID
CkpChildSignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine implements the child signal handler, which simply writes to
    the status pipe.

Arguments:

    Signal - Supplies the signal number, which should be SIGCHLD here.

Return Value:

    None.

--*/

{

    ssize_t Status;
    int Value;

    assert((Signal == SIGCHLD) && (CkSigchldContext != NULL));

    Value = 'y';
    do {
        Status = write(CkSigchldContext->Pipe[1], &Value, 1);

    } while ((Status < 0) && (errno == EINTR));

    return;
}

int
CkpOsWaitPid (
    PSPAWN_ATTRIBUTES Attributes,
    INT Options
    )

/*++

Routine Description:

    This routine executes a waitpid call, and sets the return code in the
    attributes if successful.

Arguments:

    Attributes - Supplies the child process attribues. The return code will be
        set in here if the process exits.

    Options - Supplies the options to supply to waitpid (usually either 0 or
        WNOHANG).

Return Value:

    0 if the wait succeeded.

    Non-zero if the wait timed out or failed.

--*/

{

    int ExitCode;
    pid_t Result;

    assert(Attributes->Pid > 0);

    Result = waitpid(Attributes->Pid, &ExitCode, Options);
    if (Result == Attributes->Pid) {
        if (WIFEXITED(ExitCode)) {
            Attributes->ReturnCode = WEXITSTATUS(ExitCode);

        } else if (WIFSIGNALED(ExitCode)) {
            Attributes->ReturnCode = -WTERMSIG(ExitCode);

        } else {
            Attributes->ReturnCode = ExitCode;
        }

        if ((Attributes->Debug & SPAWN_DEBUG_IO) != 0) {
            fprintf(stderr,
                    "Process %d exited with status %d (0x%x).\n",
                    Attributes->Pid,
                    Attributes->ReturnCode,
                    ExitCode);
        }

        Attributes->Pid = -1;
        return 0;
    }

    return -1;
}

