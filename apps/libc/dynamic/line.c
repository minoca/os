/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    line.c

Abstract:

    This module implements line processing functions.

Author:

    Evan Green 9-Mar-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define GETLINE_INITIAL_BUFFER_SIZE 64

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

void
ClpGetpassSignalHandler (
    int Signal
    );

//
// -------------------------------------------------------------------- Globals
//

char *ClGetpassBuffer = NULL;
size_t ClGetpassBufferSize = 0;
int *ClGetpassSignals = NULL;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
char *
getpass (
    const char *Prompt
    )

/*++

Routine Description:

    This routine reads outputs the given prompt, and reads in a line of input
    without echoing it. This routine attempts to use the process' controlling
    terminal, or stdin/stderr otherwise. This routine is neither thread-safe
    nor reentrant.

Arguments:

    Prompt - Supplies a pointer to the prompt to print.

Return Value:

    Returns a pointer to the entered input on success. If this is a password,
    the caller should be sure to clear this buffer out as soon as possible.

    NULL on failure.

--*/

{

    ssize_t BytesRead;
    char Character;
    int DescriptorIn;
    FILE *FileIn;
    size_t LineSize;
    struct sigaction NewAction;
    void *NewBuffer;
    size_t NewBufferSize;
    struct termios NewSettings;
    struct termios OriginalSettings;
    PSTR ResultBuffer;
    struct sigaction SaveAlarm;
    struct sigaction SaveHup;
    struct sigaction SaveInt;
    struct sigaction SavePipe;
    struct sigaction SaveQuit;
    struct sigaction SaveTerm;
    struct sigaction SaveTstop;
    struct sigaction SaveTtin;
    struct sigaction SaveTtou;
    int Signal;
    int Signals[NSIG];

    ResultBuffer = NULL;
    memset(Signals, 0, sizeof(Signals));
    ClGetpassSignals = Signals;
    FileIn = fopen(_PATH_TTY, "w+");
    if (FileIn == NULL) {
        return NULL;
    }

    DescriptorIn = fileno(FileIn);

    //
    // Turn off echoing.
    //

    if ((DescriptorIn < 0) ||
        (tcgetattr(DescriptorIn, &OriginalSettings) != 0)) {

        fclose(FileIn);
        return NULL;
    }

    memcpy(&NewSettings, &OriginalSettings, sizeof(struct termios));
    NewSettings.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    if (tcsetattr(DescriptorIn, TCSAFLUSH, &NewSettings) != 0) {
        goto getpassEnd;
    }

    //
    // Handle all signals so that the terminal settings can be put back.
    //

    sigemptyset(&(NewAction.sa_mask));
    NewAction.sa_flags = 0;
    NewAction.sa_handler = ClpGetpassSignalHandler;
    sigaction(SIGALRM, &NewAction, &SaveAlarm);
    sigaction(SIGHUP, &NewAction, &SaveHup);
    sigaction(SIGINT, &NewAction, &SaveInt);
    sigaction(SIGPIPE, &NewAction, &SavePipe);
    sigaction(SIGQUIT, &NewAction, &SaveQuit);
    sigaction(SIGTERM, &NewAction, &SaveTerm);
    sigaction(SIGTSTP, &NewAction, &SaveTstop);
    sigaction(SIGTTIN, &NewAction, &SaveTtin);
    sigaction(SIGTTOU, &NewAction, &SaveTtou);

    //
    // Print the prompt.
    //

    fprintf(stderr, Prompt);
    fflush(stderr);

    //
    // Loop reading characters from the input.
    //

    LineSize = 0;
    BytesRead = 0;
    while (TRUE) {
        for (Signal = 0; Signal < NSIG; Signal += 1) {
            if (ClGetpassSignals[Signal] != 0) {
                break;
            }
        }

        if (Signal != NSIG) {
            break;
        }

        BytesRead = read(DescriptorIn, &Character, 1);
        if ((BytesRead < 0) && (errno == EINTR)) {
            continue;
        }

        if (BytesRead <= 0) {
            break;
        }

        //
        // Reallocate the buffer if needed.
        //

        if (LineSize + 1 >= ClGetpassBufferSize) {
            if (ClGetpassBufferSize == 0) {
                NewBufferSize = GETLINE_INITIAL_BUFFER_SIZE;

            } else {
                NewBufferSize = ClGetpassBufferSize * 2;
            }

            NewBuffer = malloc(NewBufferSize);

            //
            // Whether or not the allocation succeeded, zero out the previous
            // buffer to avoid leaking potential passwords.
            //

            if (ClGetpassBufferSize != 0) {
                if (NewBuffer != NULL) {
                    memcpy(NewBuffer, ClGetpassBuffer, ClGetpassBufferSize);
                }

                SECURITY_ZERO(ClGetpassBuffer, ClGetpassBufferSize);
                free(ClGetpassBuffer);
                ClGetpassBuffer = NULL;
                ClGetpassBufferSize = 0;
            }

            if (NewBuffer == NULL) {
                LineSize = 0;
                break;

            } else {
                ClGetpassBuffer = NewBuffer;
                ClGetpassBufferSize = NewBufferSize;
            }
        }

        if ((Character == '\r') || (Character == '\n')) {
            break;
        }

        //
        // Control-C cancels early.
        //

        if (Character == 3) {
            goto getpassEnd;
        }

        //
        // Add the character to the buffer.
        //

        ClGetpassBuffer[LineSize] = Character;
        LineSize += 1;
    }

    if (BytesRead >= 0) {
        if ((BytesRead > 0) || (LineSize > 0)) {

            assert(LineSize + 1 < ClGetpassBufferSize);

            ClGetpassBuffer[LineSize] = '\0';
            LineSize += 1;

        } else {
            BytesRead = -1;
        }

        fputc('\n', stderr);
    }

    ResultBuffer = ClGetpassBuffer;

getpassEnd:

    //
    // If the result was not successful but there's a partial buffer, zero it
    // out.
    //

    if ((ResultBuffer == NULL) && (ClGetpassBufferSize != 0)) {
        SECURITY_ZERO(ClGetpassBuffer, ClGetpassBufferSize);
    }

    //
    // Restore the original terminal settings.
    //

    tcsetattr(DescriptorIn, TCSAFLUSH, &OriginalSettings);
    fclose(FileIn);

    //
    // Restore the original signal handlers.
    //

    sigaction(SIGALRM, &SaveAlarm, NULL);
    sigaction(SIGHUP, &SaveHup, NULL);
    sigaction(SIGINT, &SaveInt, NULL);
    sigaction(SIGPIPE, &SavePipe, NULL);
    sigaction(SIGQUIT, &SaveQuit, NULL);
    sigaction(SIGTERM, &SaveTerm, NULL);
    sigaction(SIGTSTP, &SaveTstop, NULL);
    sigaction(SIGTTIN, &SaveTtin, NULL);
    sigaction(SIGTTOU, &SaveTtou, NULL);

    //
    // Replay any signals that were sent during the read.
    //

    for (Signal = 0; Signal < NSIG; Signal += 1) {
        while (ClGetpassSignals[Signal] != 0) {
            kill(getpid(), Signal);
            ClGetpassSignals[Signal] -= 1;
        }
    }

    ClGetpassSignals = NULL;
    return ResultBuffer;
}

LIBC_API
ssize_t
getline (
    char **LinePointer,
    size_t *Size,
    FILE *Stream
    )

/*++

Routine Description:

    This routine reads an entire line from the given stream. This routine will
    allocate or reallocate the given buffer so that the buffer is big enough.

Arguments:

    LinePointer - Supplies a pointer that on input contains an optional pointer
        to a buffer to use to read the line. If the buffer ends up being not
        big enough, it will be reallocated. If no buffer is supplied, one will
        be allocated. On output, contains a pointer to the buffer containing
        the read line on success.

    Size - Supplies a pointer that on input contains the size in bytes of the
        supplied line pointer. On output, this value will be updated to contain
        the size of the buffer returned in the line buffer parameter.

    Stream - Supplies the stream to read the line from.

Return Value:

    On success, returns the number of characters read, including the delimiter
    character, but not including the null terminator.

    Returns -1 on failure (including an end of file condition), and errno will
    be set to contain more information.

--*/

{

    return getdelim(LinePointer, Size, '\n', Stream);
}

LIBC_API
ssize_t
getdelim (
    char **LinePointer,
    size_t *Size,
    int Delimiter,
    FILE *Stream
    )

/*++

Routine Description:

    This routine reads an entire line from the given stream, delimited by the
    given delimiter character. This routine will allocate or reallocate the
    given buffer so that the buffer is big enough.

Arguments:

    LinePointer - Supplies a pointer that on input contains an optional pointer
        to a buffer to use to read the line. If the buffer ends up being not
        big enough, it will be reallocated. If no buffer is supplied, one will
        be allocated. On output, contains a pointer to the buffer containing
        the read line on success.

    Size - Supplies a pointer that on input contains the size in bytes of the
        supplied line pointer. On output, this value will be updated to contain
        the size of the buffer returned in the line buffer parameter.

    Delimiter - Supplies the delimiter to split the line on.

    Stream - Supplies the stream to read the line from.

Return Value:

    On success, returns the number of characters read, including the delimiter
    character, but not including the null terminator.

    Returns -1 on failure (including an end of file condition), and errno will
    be set to contain more information.

--*/

{

    int Character;
    size_t LineSize;
    char *NewBuffer;
    size_t NewSize;

    LineSize = 0;
    if ((LinePointer == NULL) || (Size == NULL)) {
        errno = EINVAL;
        return -1;
    }

    if (Stream == NULL) {
        errno = EBADF;
        return -1;
    }

    //
    // Allocate an initial buffer if the caller passed one that is NULL or
    // too small.
    //

    if ((*LinePointer == NULL) || (*Size < GETLINE_INITIAL_BUFFER_SIZE)) {
        NewBuffer = realloc(*LinePointer, GETLINE_INITIAL_BUFFER_SIZE);
        if (NewBuffer == NULL) {
            return -1;
        }

        *LinePointer = NewBuffer;
        *Size = GETLINE_INITIAL_BUFFER_SIZE;
    }

    while (TRUE) {
        Character = fgetc(Stream);
        if (Character == EOF) {
            if (LineSize != 0) {
                break;
            }

            return -1;
        }

        if (LineSize + 2 > *Size) {

            assert(*Size != 0);

            NewSize = *Size;
            while (NewSize < LineSize + 2) {
                NewSize *= 2;
            }

            NewBuffer = realloc(*LinePointer, NewSize);
            if (NewBuffer == NULL) {
                return -1;
            }

            *LinePointer = NewBuffer;
            *Size = NewSize;
        }

        (*LinePointer)[LineSize] = Character;
        LineSize += 1;
        if (Character == Delimiter) {
            break;
        }
    }

    assert(*Size > LineSize);

    (*LinePointer)[LineSize] = '\0';
    return LineSize;
}

//
// --------------------------------------------------------- Internal Functions
//

void
ClpGetpassSignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine is the signal handler installed during the getpass function.
    It simply tracks signals for replay later.

Arguments:

    Signal - Supplies the signal number that fired.

Return Value:

    None.

--*/

{

    assert((ClGetpassSignals != NULL) && (Signal < NSIG));

    ClGetpassSignals[Signal] += 1;
    return;
}

