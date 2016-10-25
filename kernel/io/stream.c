/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stream.c

Abstract:

    This module implements support for I/O streams.

Author:

    Evan Green 15-Feb-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "iop.h"

//
// ---------------------------------------------------------------- Definitions
//

#define DEFAULT_STREAM_BUFFER_SIZE 8192

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes characteristics about a data stream buffer.

Members:

    Flags - Stores a bitfield of flags governing the state of the stream buffer.
        See STREAM_BUFFER_FLAG_* definitions.

    Size - Stores the size of the buffer, in bytes.

    Buffer - Stores a pointer to the actual stream buffer.

    NextReadOffset - Stores the offset from the beginning of the buffer where
        the next read should occur (points to the first unread byte).

    NextWriteOffset - Stores the offset from the beginning of the buffer where
        the next write should occur (points to the first unused offset).

    AtomicWriteSize - Stores the number of bytes that can always be written
        to the stream atomically (without interleaving).

    Lock - Stores a pointer to a lock ensuring only one party is accessing the
        buffer at once.

    IoState - Stores a pointer to the I/O object state.

--*/

struct _STREAM_BUFFER {
    ULONG Flags;
    ULONG Size;
    PVOID Buffer;
    ULONG NextReadOffset;
    ULONG NextWriteOffset;
    ULONG AtomicWriteSize;
    PQUEUED_LOCK Lock;
    PIO_OBJECT_STATE IoState;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PSTREAM_BUFFER
IoCreateStreamBuffer (
    PIO_OBJECT_STATE IoState,
    ULONG Flags,
    ULONG BufferSize,
    ULONG AtomicWriteSize
    )

/*++

Routine Description:

    This routine allocates and initializes a new stream buffer.

Arguments:

    IoState - Supplies an optional pointer to the I/O state structure to use
        for this stream buffer.

    Flags - Supplies a bitfield of flags governing the behavior of the stream
        buffer. See STREAM_BUFFER_FLAG_* definitions.

    BufferSize - Supplies the size of the buffer. Supply zero to use a default
        system value.

    AtomicWriteSize - Supplies the number of bytes that can always be written
        to the stream atomically (without interleaving).

Return Value:

    Returns a pointer to the buffer on success.

    NULL on invalid parameter or allocation failure.

--*/

{

    KSTATUS Status;
    PSTREAM_BUFFER StreamBuffer;

    if (AtomicWriteSize == 0) {
        AtomicWriteSize = 1;
    }

    if (BufferSize == 0) {
        BufferSize = DEFAULT_STREAM_BUFFER_SIZE;

    //
    // Bump up the internal buffer size since one byte of the buffer is always
    // wasted.
    //

    } else {
        BufferSize += 1;
    }

    if (BufferSize < AtomicWriteSize) {
        BufferSize = AtomicWriteSize + 1;
    }

    //
    // Create the stream buffer structure.
    //

    StreamBuffer = MmAllocatePagedPool(sizeof(STREAM_BUFFER),
                                       FI_ALLOCATION_TAG);

    if (StreamBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateStreamBufferEnd;
    }

    RtlZeroMemory(StreamBuffer, sizeof(STREAM_BUFFER));
    StreamBuffer->Size = BufferSize;
    StreamBuffer->AtomicWriteSize = AtomicWriteSize;
    StreamBuffer->Lock = KeCreateQueuedLock();
    if (StreamBuffer->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateStreamBufferEnd;
    }

    //
    // Create the buffer itself.
    //

    StreamBuffer->Buffer = MmAllocatePagedPool(BufferSize,
                                               FI_ALLOCATION_TAG);

    if (StreamBuffer->Buffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateStreamBufferEnd;
    }

    //
    // Use the given I/O object state or create one.
    //

    ASSERT(IoState != NULL);

    StreamBuffer->IoState = IoState;
    IoSetIoObjectState(StreamBuffer->IoState, POLL_EVENT_OUT, TRUE);
    StreamBuffer->Flags = Flags;
    Status = STATUS_SUCCESS;

CreateStreamBufferEnd:
    if (!KSUCCESS(Status)) {
        if (StreamBuffer != NULL) {
            if (StreamBuffer->Lock != NULL) {
                KeDestroyQueuedLock(StreamBuffer->Lock);
            }

            if (StreamBuffer->Buffer != NULL) {
                MmFreePagedPool(StreamBuffer->Buffer);
            }

            MmFreePagedPool(StreamBuffer);
            StreamBuffer = NULL;
        }
    }

    return StreamBuffer;
}

VOID
IoDestroyStreamBuffer (
    PSTREAM_BUFFER StreamBuffer
    )

/*++

Routine Description:

    This routine destroys an allocated stream buffer. It assumes there are no
    waiters on the events.

Arguments:

    StreamBuffer - Supplies a pointer to the stream buffer to tear down.

Return Value:

    None.

--*/

{

    if (StreamBuffer->Lock != NULL) {
        KeDestroyQueuedLock(StreamBuffer->Lock);
    }

    StreamBuffer->IoState = NULL;
    if (StreamBuffer->Buffer != NULL) {
        MmFreePagedPool(StreamBuffer->Buffer);
        StreamBuffer->Buffer = NULL;
    }

    MmFreePagedPool(StreamBuffer);
    return;
}

KSTATUS
IoReadStreamBuffer (
    PSTREAM_BUFFER StreamBuffer,
    PIO_BUFFER IoBuffer,
    UINTN ByteCount,
    ULONG TimeoutInMilliseconds,
    BOOL NonBlocking,
    PUINTN BytesRead
    )

/*++

Routine Description:

    This routine reads from a stream buffer. This routine must be called at low
    level, unless the stream was set up to be read at dispatch.

Arguments:

    StreamBuffer - Supplies a pointer to the stream buffer to read from.

    IoBuffer - Supplies a pointer to the I/O buffer where the read data will be
        returned on success.

    ByteCount - Supplies the number of bytes to read.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    NonBlocking - Supplies a boolean indicating if this read should avoid
        blocking.

    BytesRead - Supplies a pointer where the number of bytes actually read will
        be returned.

Return Value:

    Status code. If a failing status code is returned, then check the number of
    bytes read to see if any valid data was returned.

--*/

{

    ULONG BytesAvailable;
    ULONG BytesReadHere;
    ULONG BytesToRead;
    ULONG EventsMask;
    ULONG NextWriteOffset;
    ULONG ReturnedEvents;
    KSTATUS Status;

    *BytesRead = 0;
    BytesReadHere = 0;
    EventsMask = POLL_EVENT_IN | POLL_ERROR_EVENTS;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Loop until at least a byte was read.
    //

    Status = STATUS_SUCCESS;
    while (BytesReadHere == 0) {

        //
        // Unless in non-blocking mode, wait for either the read or error
        // events to be set.
        //

        if (NonBlocking == FALSE) {
            Status = IoWaitForIoObjectState(StreamBuffer->IoState,
                                            EventsMask,
                                            TRUE,
                                            TimeoutInMilliseconds,
                                            &ReturnedEvents);

            if (!KSUCCESS(Status)) {
                break;
            }

        } else {
            ReturnedEvents = StreamBuffer->IoState->Events & EventsMask;
        }

        //
        // Multiple threads might have come out of waiting. Acquire the lock.
        //

        KeAcquireQueuedLock(StreamBuffer->Lock);

        //
        // Start over if there's nothing to read.
        //

        if (StreamBuffer->NextReadOffset == StreamBuffer->NextWriteOffset) {

            //
            // If the IN flag is set, then that would mean this routine is
            // busy spinning. Poor form.
            //

            ASSERT((NonBlocking != FALSE) ||
                   ((StreamBuffer->IoState->Events & POLL_ERROR_EVENTS) != 0) ||
                   ((StreamBuffer->IoState->Events & POLL_EVENT_IN) == 0));

            KeReleaseQueuedLock(StreamBuffer->Lock);

            //
            // If the error event is set, error out.
            //

            if ((ReturnedEvents & POLL_ERROR_EVENTS) != 0) {
                Status = STATUS_END_OF_FILE;
                break;
            }

            //
            // Blocking reads loop back to wait on the event, non-blocking
            // reads exit now.
            //

            if (NonBlocking == FALSE) {
                continue;

            } else {
                if (*BytesRead == 0) {
                    Status = STATUS_TRY_AGAIN;
                }

                break;
            }
        }

        //
        // Now read the buffer, at least going to the end of the buffer.
        // Wraparounds will be handled later on.
        //

        NextWriteOffset = StreamBuffer->NextWriteOffset;

        ASSERT(NextWriteOffset < StreamBuffer->Size);

        if (NextWriteOffset > StreamBuffer->NextReadOffset) {
            BytesAvailable = NextWriteOffset - StreamBuffer->NextReadOffset;

        } else {
            BytesAvailable = StreamBuffer->Size - StreamBuffer->NextReadOffset;
        }

        BytesToRead = BytesAvailable;
        if (ByteCount < BytesToRead) {
            BytesToRead = ByteCount;
        }

        Status = MmCopyIoBufferData(
                           IoBuffer,
                           StreamBuffer->Buffer + StreamBuffer->NextReadOffset,
                           *BytesRead,
                           BytesToRead,
                           TRUE);

        if (!KSUCCESS(Status)) {
            KeReleaseQueuedLock(StreamBuffer->Lock);
            return Status;
        }

        //
        // Update the read offset so that it always contains a valid value.
        //

        if (StreamBuffer->NextReadOffset + BytesToRead == StreamBuffer->Size) {
            StreamBuffer->NextReadOffset = 0;

        } else {
            StreamBuffer->NextReadOffset += BytesToRead;
        }

        ASSERT(StreamBuffer->NextReadOffset < StreamBuffer->Size);

        *BytesRead += BytesToRead;
        BytesReadHere += BytesToRead;
        ByteCount -= BytesToRead;

        //
        // The first copy is done, but it's possible that the eligible read
        // content wraps around. Grab the rest of that data if so.
        //

        if ((ByteCount != 0) &&
            (StreamBuffer->NextReadOffset != NextWriteOffset)) {

            ASSERT(StreamBuffer->NextReadOffset == 0);
            ASSERT(NextWriteOffset > StreamBuffer->NextReadOffset);

            BytesAvailable = NextWriteOffset - StreamBuffer->NextReadOffset;
            BytesToRead = BytesAvailable;
            if (ByteCount < BytesToRead) {
                BytesToRead = ByteCount;
            }

            //
            // Don't break out of the loop on failure right away, as the
            // I/O state events need to be adjusted for the successful first
            // copy that happened.
            //

            Status = MmCopyIoBufferData(
                           IoBuffer,
                           StreamBuffer->Buffer + StreamBuffer->NextReadOffset,
                           *BytesRead,
                           BytesToRead,
                           TRUE);

            if (KSUCCESS(Status)) {
                StreamBuffer->NextReadOffset += BytesToRead;

                ASSERT(StreamBuffer->NextReadOffset < StreamBuffer->Size);

                *BytesRead += BytesToRead;
                BytesReadHere += BytesToRead;
                ByteCount -= BytesToRead;
            }
        }

        //
        // Signal the write event (since more space was just made), and signal
        // the read event if there is still data left to be read. Don't do
        // this if the error events are set, as this is probably a disconnected
        // pipe with some data left in it.
        //

        if ((ReturnedEvents & POLL_ERROR_EVENTS) == 0) {
            IoSetIoObjectState(StreamBuffer->IoState, POLL_EVENT_OUT, TRUE);
            if (StreamBuffer->NextReadOffset != StreamBuffer->NextWriteOffset) {
                IoSetIoObjectState(StreamBuffer->IoState, POLL_EVENT_IN, TRUE);

            } else {
                IoSetIoObjectState(StreamBuffer->IoState, POLL_EVENT_IN, FALSE);
            }
        }

        KeReleaseQueuedLock(StreamBuffer->Lock);

        //
        // If that second copy failed, now's the time to break out.
        //

        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    return Status;
}

KSTATUS
IoWriteStreamBuffer (
    PSTREAM_BUFFER StreamBuffer,
    PIO_BUFFER IoBuffer,
    UINTN ByteCount,
    ULONG TimeoutInMilliseconds,
    BOOL NonBlocking,
    PUINTN BytesWritten
    )

/*++

Routine Description:

    This routine writes to a stream buffer. This routine must be called at low
    level, unless the stream was set up to be written at dispatch.

Arguments:

    StreamBuffer - Supplies a pointer to the stream buffer to write to.

    IoBuffer - Supplies a pointer to the I/O buffer containing the data to
        write to the stream buffer.

    ByteCount - Supplies the number of bytes to writes.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    NonBlocking - Supplies a boolean indicating if this write should avoid
        blocking.

    BytesWritten - Supplies a pointer where the number of bytes actually written
        will be returned.

Return Value:

    Status code. If a failing status code is returned, then check the number of
    bytes read to see if any valid data was written.

--*/

{

    ULONG BytesAvailable;
    ULONG BytesToWrite;
    ULONG EventsMask;
    ULONG NextReadOffset;
    ULONG ReturnedEvents;
    KSTATUS Status;
    ULONG TotalBytesAvailable;

    *BytesWritten = 0;
    EventsMask = POLL_EVENT_OUT | POLL_ERROR_EVENTS;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Status = STATUS_SUCCESS;
    while (ByteCount != 0) {
        if (NonBlocking == FALSE) {
            Status = IoWaitForIoObjectState(StreamBuffer->IoState,
                                            EventsMask,
                                            TRUE,
                                            TimeoutInMilliseconds,
                                            &ReturnedEvents);

            if (!KSUCCESS(Status)) {
                break;
            }

            if (ReturnedEvents != POLL_EVENT_OUT) {
                Status = STATUS_BROKEN_PIPE;
                break;
            }
        }

        //
        // Multiple threads might have come out of waiting since read and
        // write aren't synchronized.
        //

        KeAcquireQueuedLock(StreamBuffer->Lock);

        //
        // Figure out how much room there is.
        //

        NextReadOffset = StreamBuffer->NextReadOffset;

        ASSERT(NextReadOffset < StreamBuffer->Size);

        if (NextReadOffset <= StreamBuffer->NextWriteOffset) {

            //
            // The total available is the entire buffer (minus one) minus the
            // distance between the read and write pointers.
            //

            TotalBytesAvailable = (StreamBuffer->Size - 1) -
                                  (StreamBuffer->NextWriteOffset -
                                   NextReadOffset);

            //
            // The first copy goes from the next write offset to the end, but
            // if the read offset is right at zero then the padding byte is at
            // the end there.
            //

            BytesAvailable = StreamBuffer->Size - StreamBuffer->NextWriteOffset;
            if (NextReadOffset == 0) {
                BytesAvailable -= 1;
            }

        } else {

            //
            // The total available space is the distance between the write
            // catching up to the read, minus the one buffer byte.
            //

            BytesAvailable = NextReadOffset - StreamBuffer->NextWriteOffset - 1;
            TotalBytesAvailable = BytesAvailable;
        }

        //
        // Start over if the buffer is full. The stream stipulates that it will
        // always be able to write at least the atomic size without
        // interleaving.
        //

        if ((TotalBytesAvailable < ByteCount) &&
            (TotalBytesAvailable < StreamBuffer->AtomicWriteSize)) {

            IoSetIoObjectState(StreamBuffer->IoState, POLL_EVENT_OUT, FALSE);
            KeReleaseQueuedLock(StreamBuffer->Lock);
            if (NonBlocking == FALSE) {
                continue;

            } else {
                if (*BytesWritten == 0) {
                    Status = STATUS_TRY_AGAIN;
                }

                break;
            }
        }

        //
        // Now write to the buffer, at least going to the end of the buffer.
        // Wraparounds will be handled later on.
        //

        ASSERT(BytesAvailable != 0);

        BytesToWrite = BytesAvailable;
        if (ByteCount < BytesToWrite) {
            BytesToWrite = ByteCount;
        }

        Status = MmCopyIoBufferData(
                          IoBuffer,
                          StreamBuffer->Buffer + StreamBuffer->NextWriteOffset,
                          *BytesWritten,
                          BytesToWrite,
                          FALSE);

        if (!KSUCCESS(Status)) {
            KeReleaseQueuedLock(StreamBuffer->Lock);
            return Status;
        }

        //
        // Update the next write pointer in a manner that ensures its value is
        // always valid.
        //

        if (StreamBuffer->NextWriteOffset + BytesToWrite ==
            StreamBuffer->Size) {

            StreamBuffer->NextWriteOffset = 0;

        } else {
            StreamBuffer->NextWriteOffset += BytesToWrite;
        }

        *BytesWritten += BytesToWrite;
        ByteCount -= BytesToWrite;
        TotalBytesAvailable -= BytesToWrite;

        //
        // The first copy is done, but it's possible that the eligible space
        // wraps around. Write the remainder if so.
        //

        if ((ByteCount != 0) &&
            (((StreamBuffer->NextWriteOffset + 1) % StreamBuffer->Size) !=
                                                             NextReadOffset)) {

            ASSERT(StreamBuffer->NextWriteOffset == 0);
            ASSERT(NextReadOffset > StreamBuffer->NextWriteOffset + 1);

            BytesAvailable = NextReadOffset - StreamBuffer->NextWriteOffset - 1;
            BytesToWrite = BytesAvailable;
            if (ByteCount < BytesToWrite) {
                BytesToWrite = ByteCount;
            }

            //
            // Don't break out of the loop on failure right away, as the
            // I/O state events need to be adjusted for the successful first
            // copy that happened.
            //

            Status = MmCopyIoBufferData(
                          IoBuffer,
                          StreamBuffer->Buffer + StreamBuffer->NextWriteOffset,
                          *BytesWritten,
                          BytesToWrite,
                          FALSE);

            if (KSUCCESS(Status)) {
                StreamBuffer->NextWriteOffset += BytesToWrite;

                ASSERT(StreamBuffer->NextWriteOffset < StreamBuffer->Size);

                *BytesWritten += BytesToWrite;
                ByteCount -= BytesToWrite;
                TotalBytesAvailable -= BytesToWrite;
            }
        }

        //
        // Signal the read event (since there's now stuff to read), and signal
        // the write event if there is still space left.
        //

        IoSetIoObjectState(StreamBuffer->IoState, POLL_EVENT_IN, TRUE);

        ASSERT(TotalBytesAvailable < StreamBuffer->Size);

        if (TotalBytesAvailable >= StreamBuffer->AtomicWriteSize) {
            IoSetIoObjectState(StreamBuffer->IoState, POLL_EVENT_OUT, TRUE);

        } else {
            IoSetIoObjectState(StreamBuffer->IoState, POLL_EVENT_OUT, FALSE);
        }

        KeReleaseQueuedLock(StreamBuffer->Lock);

        //
        // If that second copy failed, now is the time to exit.
        //

        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    return Status;
}

KSTATUS
IoStreamBufferConnect (
    PSTREAM_BUFFER StreamBuffer
    )

/*++

Routine Description:

    This routine resets the I/O object state when someone connects to a stream
    buffer.

Arguments:

    StreamBuffer - Supplies a pointer to the stream buffer.

Return Value:

    Status code.

--*/

{

    ULONG TotalBytesAvailable;

    KeAcquireQueuedLock(StreamBuffer->Lock);

    //
    // Figure out how much space there is.
    //

    if (StreamBuffer->NextReadOffset <= StreamBuffer->NextWriteOffset) {

        //
        // The total available is the entire buffer (minus one) minus the
        // distance between the read and write pointers.
        //

        TotalBytesAvailable = (StreamBuffer->Size - 1) -
                              (StreamBuffer->NextWriteOffset -
                               StreamBuffer->NextReadOffset);

    } else {

        //
        // The total available space is the distance between the write
        // catching up to the read, minus the one buffer byte.
        //

        TotalBytesAvailable = StreamBuffer->NextReadOffset -
                              StreamBuffer->NextWriteOffset - 1;
    }

    //
    // Signal the write event if there's space to be written.
    //

    if (TotalBytesAvailable >= StreamBuffer->AtomicWriteSize) {
        IoSetIoObjectState(StreamBuffer->IoState, POLL_EVENT_OUT, TRUE);

    } else {
        IoSetIoObjectState(StreamBuffer->IoState, POLL_EVENT_OUT, FALSE);
    }

    //
    // Signal the read event if there's data in there.
    //

    if (TotalBytesAvailable != StreamBuffer->Size - 1) {
        IoSetIoObjectState(StreamBuffer->IoState, POLL_EVENT_IN, TRUE);

    } else {
        IoSetIoObjectState(StreamBuffer->IoState, POLL_EVENT_IN, FALSE);
    }

    KeReleaseQueuedLock(StreamBuffer->Lock);
    return STATUS_SUCCESS;
}

PIO_OBJECT_STATE
IoStreamBufferGetIoObjectState (
    PSTREAM_BUFFER StreamBuffer
    )

/*++

Routine Description:

    This routine returns the I/O state for a stream buffer.

Arguments:

    StreamBuffer - Supplies a pointer to the stream buffer.

Return Value:

    Returns a pointer to the stream buffer's I/O object state.

--*/

{

    return StreamBuffer->IoState;
}

//
// --------------------------------------------------------- Internal Functions
//

