/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    system.c

Abstract:

    This module implements support for the system C library functions. This
    function is provided for legacy code. New applications should use the
    fork and exec mechanisms.

Author:

    Evan Green 28-Jul-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the parameters used when running the command interpreter.
//

#define SHELL_ARGUMENT0 _PATH_BSHELL
#define SHELL_ARGUMENT1 "-c"
#define SHELL_NOT_FOUND_STATUS 127

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

LIBC_API
int
system (
    const char *Command
    )

/*++

Routine Description:

    This routine passes the given command to the command line interpreter. If
    the command is null, this routine determines if the host environment has
    a command processor. The environment of the executed command shall be as if
    the child process were created using the fork function, and then the
    child process invoked execl(<shell path>, "sh", "-c", <command>, NULL).

    This routine will ignore the SIGINT and SIGQUIT signals, and shall block
    the SIGCHLD signal while waiting for the command to terminate.

Arguments:

    Command - Supplies an optional pointer to the command to execute.

Return Value:

    Returns a non-zero value if the command is NULL and a command processor is
    available.

    0 if no command processor is available.

    127 if the command processor could not be executed.

    Otherwise, returns the termination status of the command language
    interpreter.

--*/

{

    struct sigaction Action;
    char *Arguments[4];
    pid_t Pid;
    sigset_t SaveBlock;
    struct sigaction SavedInterrupt;
    struct sigaction SavedQuit;
    int Status;

    //
    // Ignore interrupt and quit signals, and block child signals.
    //

    Status = 0;
    Action.sa_handler = SIG_IGN;
    sigemptyset(&Action.sa_mask);
    Action.sa_flags = 0;
    sigemptyset(&(SavedInterrupt.sa_mask));
    sigemptyset(&(SavedQuit.sa_mask));
    sigaction(SIGINT, &Action, &SavedInterrupt);
    sigaction(SIGQUIT, &Action, &SavedQuit);
    sigaddset(&(Action.sa_mask), SIGCHLD);
    sigprocmask(SIG_BLOCK, &(Action.sa_mask), &SaveBlock);

    //
    // Fork off the child process.
    //

    Pid = fork();
    if (Pid < 0) {
        return -1;
    }

    //
    // If this is the child, run the command.
    //

    if (Pid == 0) {
        sigaction(SIGINT, &SavedInterrupt, NULL);
        sigaction(SIGQUIT, &SavedQuit, NULL);
        sigprocmask(SIG_SETMASK, &SaveBlock, NULL);
        Arguments[0] = SHELL_ARGUMENT0;
        Arguments[1] = SHELL_ARGUMENT1;
        Arguments[2] = (char *)Command;
        Arguments[3] = NULL;
        execvp(_PATH_BSHELL, Arguments);
        exit(SHELL_NOT_FOUND_STATUS);

    //
    // This is the parent, wait for the command to finish.
    //

    } else {
        while (waitpid(Pid, &Status, 0) == -1) {
            if (errno != EINTR) {
                Status = -1;
                break;
            }
        }
    }

    //
    // Restore the signal mask and dispositions.
    //

    sigaction(SIGINT, &SavedInterrupt, NULL);
    sigaction(SIGQUIT, &SavedInterrupt, NULL);
    sigprocmask(SIG_SETMASK, &SaveBlock, NULL);
    return Status;
}

LIBC_API
FILE *
popen (
    const char *Command,
    const char *Mode
    )

/*++

Routine Description:

    This routine executes the command specified by the given string. It shall
    create a pipe between the calling program and the executed command, and
    shall return a pointer to a stream that can be used to either read from or
    write to the pipe. Streams returned by this function should be closed with
    the pclose function.

Arguments:

    Command - Supplies a pointer to a null terminated string containing the
        command to execute.

    Mode - Supplies a pointer to a null terminated string containing the mode
        information of the returned string. If the first character of the
        given string is 'r', then the file stream returned can be read to
        retrieve the standard output of the executed process. Otherwise, the
        file stream returned can be written to to send data to the standard
        input of the executed process.

Return Value:

    Returns a pointer to a stream wired up to the standard in or standard out
    of the executed process.

    NULL on failure, and errno will be set to contain more information.

--*/

{

    char *Arguments[4];
    pid_t Pid;
    int Pipe[2];
    FILE *Stream;

    Stream = NULL;
    if (pipe(Pipe) < 0) {
        return NULL;
    }

    if (*Mode == 'r') {
        Stream = fdopen(Pipe[0], "rb");

    } else {
        Stream = fdopen(Pipe[1], "wb");
    }

    if (Stream == NULL) {
        return NULL;
    }

    Pid = fork();
    if (Pid < 0) {
        fclose(Stream);
        close(Pipe[0]);
        close(Pipe[1]);
        return NULL;
    }

    //
    // If this is the child, launch the process.
    //

    if (Pid == 0) {
        Arguments[0] = SHELL_ARGUMENT0;
        Arguments[1] = SHELL_ARGUMENT1;
        Arguments[2] = (char *)Command;
        Arguments[3] = NULL;

        //
        // If the mode is to read, then copy the write end of the pipe to
        // standard out.
        //

        if (*Mode == 'r') {
            close(Pipe[0]);
            close(STDOUT_FILENO);
            dup2(Pipe[1], STDOUT_FILENO);
            close(Pipe[1]);

        //
        // If the mode is to write, then copy the read end of the pipe to
        // standard in.
        //

        } else {
            close(Pipe[1]);
            close(STDIN_FILENO);
            dup2(Pipe[0], STDIN_FILENO);
            close(Pipe[0]);
        }

        execvpe(_PATH_BSHELL, Arguments, (char *const *)environ);
        exit(SHELL_NOT_FOUND_STATUS);
    }

    //
    // This is the parent, so close the end of the pipe the child has. Also
    // mark the descriptors as "close on exec" so that future popens won't hold
    // these pipes open in the child processes they create.
    //

    if (*Mode == 'r') {
        close(Pipe[1]);
        fcntl(Pipe[0], F_SETFD, FD_CLOEXEC);

    } else {
        close(Pipe[0]);
        fcntl(Pipe[1], F_SETFD, FD_CLOEXEC);
    }

    Stream->Pid = Pid;
    return Stream;
}

LIBC_API
int
pclose (
    FILE *Stream
    )

/*++

Routine Description:

    This routine closes a stream opened by the popen function, wait for the
    command to terminate, and return the termination status of the process
    that was running the command language interpreter.

Arguments:

    Stream - Supplies a pointer to the stream opened with popen.

Return Value:

    Returns the execution status of the opened process.

    127 if the command language intepreter cannot be executed.

    -1 if an intervening call to wait or waitpid caused the termination status
    to be unavailable. In this case, errno will be set to ECHLD.

--*/

{

    pid_t Pid;
    int Status;

    Pid = Stream->Pid;
    fclose(Stream);
    if (waitpid(Pid, &Status, 0) < 0) {
        return -1;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

