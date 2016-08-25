/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    timezone.c

Abstract:

    This module implements support for managing the current time zone within
    the kernel.

Author:

    Evan Green 5-Aug-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "kep.h"

//
// ---------------------------------------------------------------- Definitions
//

#define TIME_ZONE_ALLOCATION_TAG 0x7A54654B // 'zTeK'

#define TIME_ZONE_ALMANAC_FILE_PATH "/Volume/Volume0/tzdata"
#define TIME_ZONE_DEFAULT_FILE_PATH "/Volume/Volume0/tzdflt"

#define MAX_TIME_ZONE_FILE_SIZE (10 * _1MB)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KepAcquireTimeZoneLock (
    VOID
    );

VOID
KepReleaseTimeZoneLock (
    VOID
    );

PVOID
KepTimeZoneReallocate (
    PVOID Memory,
    UINTN NewSize
    );

KSTATUS
KepReadTimeZoneAlmanac (
    PVOID *Buffer,
    PULONG BufferSize
    );

KSTATUS
KepGetCurrentTimeZone (
    PSTR *TimeZone
    );

KSTATUS
KepGetCurrentTimeZoneData (
    PVOID *Data,
    PULONG DataSize
    );

//
// -------------------------------------------------------------------- Globals
//

KSPIN_LOCK KeTimeZoneLock;
volatile RUNLEVEL KeTimeZoneLockOldRunLevel;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
KeSetSystemTimeZone (
    PSTR ZoneName,
    PSTR OriginalZoneBuffer,
    PULONG OriginalZoneBufferSize
    )

/*++

Routine Description:

    This routine attempts to set the system's time zone.

Arguments:

    ZoneName - Supplies an optional pointer to the null terminated string
        containing the name of the time zone to set. If this parameter is NULL,
        then the current time zone will be returned an no other changes will
        be made.

    OriginalZoneBuffer - Supplies an optional pointer where the original (or
        current if no new time zone was provided) time zone will be returned.
        This must be allocated in non-paged pool.

    OriginalZoneBufferSize - Supplies a pointer that on input contains the
        size of the original zone buffer in bytes. On output, this value will
        contain the size of the original zone buffer needed to contain the
        name of the current time zone (even if no buffer was provided).

Return Value:

    Status code.

--*/

{

    UINTN BytesCompleted;
    PVOID DataBuffer;
    ULONG DataSize;
    PVOID FilteredData;
    ULONG FilteredDataSize;
    PIO_HANDLE Handle;
    IO_BUFFER IoBuffer;
    ULONG NameSize;
    PSTR NonPagedName;
    PVOID OldData;
    ULONG OldDataSize;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    DataBuffer = NULL;
    FilteredData = NULL;
    Handle = INVALID_HANDLE;

    //
    // Create a non-paged pool copy of the name as required for running at
    // dispatch level.
    //

    NonPagedName = NULL;
    if (ZoneName != NULL) {
        NameSize = RtlStringLength(ZoneName) + 1;
        NonPagedName = MmAllocateNonPagedPool(NameSize,
                                              TIME_ZONE_ALLOCATION_TAG);

        if (NonPagedName == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetSystemTimeZoneEnd;
        }

        RtlCopyMemory(NonPagedName, ZoneName, NameSize);
    }

    //
    // First try to set the zone based on the data that's already there.
    //

    Status = RtlSelectTimeZone(NonPagedName,
                               OriginalZoneBuffer,
                               OriginalZoneBufferSize);

    if ((KSUCCESS(Status)) || (ZoneName == NULL)) {
        return Status;
    }

    Status = KepReadTimeZoneAlmanac(&DataBuffer, &DataSize);
    if (!KSUCCESS(Status)) {
        goto SetSystemTimeZoneEnd;
    }

    //
    // Filter the given data for the requested time zone. This first call just
    // determines the size of the filtered data.
    //

    Status = RtlFilterTimeZoneData(DataBuffer,
                                   DataSize,
                                   ZoneName,
                                   NULL,
                                   &FilteredDataSize);

    if (!KSUCCESS(Status)) {
        goto SetSystemTimeZoneEnd;
    }

    //
    // Allocate the real data buffer, then filter in the real time zone info.
    //

    FilteredData = MmAllocateNonPagedPool(FilteredDataSize,
                                          TIME_ZONE_ALLOCATION_TAG);

    if (FilteredData == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SetSystemTimeZoneEnd;
    }

    Status = RtlFilterTimeZoneData(DataBuffer,
                                   DataSize,
                                   ZoneName,
                                   FilteredData,
                                   &FilteredDataSize);

    if (!KSUCCESS(Status)) {
        goto SetSystemTimeZoneEnd;
    }

    //
    // Open up the cached information file and write that filtered data in.
    // This will truncate an existing file but will not create a new one.
    // Failing to open the file is not fatal, but failing to write to it is.
    // TODO: This file should also use the configuration directory rather than
    // a hardcoded path.
    //

    Status = IoOpen(TRUE,
                    NULL,
                    TIME_ZONE_DEFAULT_FILE_PATH,
                    sizeof(TIME_ZONE_DEFAULT_FILE_PATH),
                    IO_ACCESS_WRITE,
                    OPEN_FLAG_TRUNCATE,
                    FILE_PERMISSION_NONE,
                    &Handle);

    if (KSUCCESS(Status)) {
        Status = MmInitializeIoBuffer(&IoBuffer,
                                      FilteredData,
                                      INVALID_PHYSICAL_ADDRESS,
                                      FilteredDataSize,
                                      IO_BUFFER_FLAG_KERNEL_MODE_DATA);

        if (!KSUCCESS(Status)) {
            goto SetSystemTimeZoneEnd;
        }

        Status = IoWrite(Handle,
                         &IoBuffer,
                         FilteredDataSize,
                         0,
                         WAIT_TIME_INDEFINITE,
                         &BytesCompleted);

        if (!KSUCCESS(Status)) {
            goto SetSystemTimeZoneEnd;
        }
    }

    //
    // Make this new data active.
    //

    Status = RtlSetTimeZoneData(FilteredData,
                                FilteredDataSize,
                                NULL,
                                &OldData,
                                &OldDataSize,
                                OriginalZoneBuffer,
                                OriginalZoneBufferSize);

    if (!KSUCCESS(Status)) {
        goto SetSystemTimeZoneEnd;
    }

    FilteredData = NULL;
    if (OldData != NULL) {
        MmFreeNonPagedPool(OldData);
    }

SetSystemTimeZoneEnd:
    if (NonPagedName != NULL) {
        MmFreeNonPagedPool(NonPagedName);
    }

    if (Handle != INVALID_HANDLE) {
        IoClose(Handle);
    }

    if (DataBuffer != NULL) {
        MmFreePagedPool(DataBuffer);
    }

    if (FilteredData != NULL) {
        MmFreeNonPagedPool(FilteredData);
    }

    return Status;
}

KERNEL_API
KSTATUS
KeGetCurrentTimeZoneOffset (
    PLONG TimeZoneOffset
    )

/*++

Routine Description:

    This routine returns the current time zone offset. Note that this data is
    stale as soon as it is returned.

Arguments:

    TimeZoneOffset - Supplies a pointer where the current (or really
        immediately previous) time zone offset in seconds to be added to GMT
        will be returned.

Return Value:

    Status code.

--*/

{

    CALENDAR_TIME LocalCalendarTime;
    SYSTEM_TIME LocalSystemTime;
    KSTATUS Status;
    SYSTEM_TIME SystemTime;

    KeGetSystemTime(&SystemTime);
    Status = RtlSystemTimeToLocalCalendarTime(&SystemTime, &LocalCalendarTime);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = RtlCalendarTimeToSystemTime(&LocalCalendarTime, &LocalSystemTime);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    *TimeZoneOffset = LocalSystemTime.Seconds - SystemTime.Seconds;
    return STATUS_SUCCESS;
}

INTN
KeSysTimeZoneControl (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine performs system time zone control operations.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PVOID Almanac;
    ULONG AlmanacSize;
    PSTR CurrentTimeZone;
    ULONG DataSize;
    PVOID FilteredData;
    BOOL FoundCurrentTimeZone;
    BOOL Match;
    PVOID NonPagedBuffer;
    PVOID OriginalZone;
    ULONG OriginalZoneSize;
    PSYSTEM_CALL_TIME_ZONE_CONTROL Parameters;
    KSTATUS Status;
    PSTR ZoneName;

    Almanac = NULL;
    CurrentTimeZone = NULL;
    FilteredData = NULL;
    NonPagedBuffer = NULL;
    OriginalZone = NULL;
    ZoneName = NULL;
    Parameters = SystemCallParameter;
    switch (Parameters->Operation) {

    //
    // Get the currently active time zone data.
    //

    case TimeZoneOperationGetCurrentZoneData:

        //
        // If there's no data buffer, just return the size.
        //

        if ((Parameters->DataBuffer == NULL) ||
            (Parameters->DataBufferSize == 0)) {

            Status = RtlGetTimeZoneData(NULL, &DataSize);
            if (!KSUCCESS(Status)) {
                goto TimeZoneControlEnd;
            }

            Parameters->DataBufferSize = DataSize;
            goto TimeZoneControlEnd;
        }

        Status = KepGetCurrentTimeZoneData(&NonPagedBuffer, &DataSize);
        if (!KSUCCESS(Status)) {
            goto TimeZoneControlEnd;
        }

        if (Parameters->DataBufferSize < DataSize) {
            Parameters->DataBufferSize = DataSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            goto TimeZoneControlEnd;
        }

        Status = MmCopyToUserMode(Parameters->DataBuffer,
                                  NonPagedBuffer,
                                  DataSize);

        if (!KSUCCESS(Status)) {
            goto TimeZoneControlEnd;
        }

        Parameters->DataBufferSize = DataSize;
        break;

    //
    // Get the time zone data for some time zone.
    //

    case TimeZoneOperationGetZoneData:
        Status = MmCreateCopyOfUserModeString(Parameters->ZoneName,
                                              Parameters->ZoneNameSize,
                                              TIME_ZONE_ALLOCATION_TAG,
                                              &ZoneName);

        if (!KSUCCESS(Status)) {
            goto TimeZoneControlEnd;
        }

        //
        // Check to see if the user is asking for the current time zone, and
        // save some file I/O if so.
        //

        FoundCurrentTimeZone = FALSE;
        Status = KepGetCurrentTimeZone(&CurrentTimeZone);
        if (KSUCCESS(Status)) {
            Match = RtlAreStringsEqualIgnoringCase(ZoneName,
                                                   CurrentTimeZone,
                                                   Parameters->ZoneNameSize);

            if (Match != FALSE) {
                FoundCurrentTimeZone = TRUE;
                Status = KepGetCurrentTimeZoneData(&NonPagedBuffer, &DataSize);
                if (!KSUCCESS(Status)) {
                    goto TimeZoneControlEnd;
                }
            }
        }

        //
        // If the time zone requested is not the current time zone, read it
        // out of the almanac.
        //

        if (FoundCurrentTimeZone == FALSE) {
            Status = KepReadTimeZoneAlmanac(&Almanac, &AlmanacSize);
            if (!KSUCCESS(Status)) {
                goto TimeZoneControlEnd;
            }

            Status = RtlFilterTimeZoneData(Almanac,
                                           AlmanacSize,
                                           ZoneName,
                                           NULL,
                                           &DataSize);

            if (!KSUCCESS(Status)) {
                goto TimeZoneControlEnd;
            }
        }

        //
        // Do some buffer boundary checks. Maybe they only want the size
        // anyway.
        //

        if ((Parameters->DataBuffer == NULL) ||
            (Parameters->DataBufferSize == 0)) {

            Parameters->DataBufferSize = DataSize;
            goto TimeZoneControlEnd;
        }

        if (Parameters->DataBufferSize < DataSize) {
            Parameters->DataBufferSize = DataSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            goto TimeZoneControlEnd;
        }

        //
        // Ok, time to copy data. Copy from either the current time zone, or
        // filter the right zone out of the almanac.
        //

        if (FoundCurrentTimeZone != FALSE) {
            Status = MmCopyToUserMode(Parameters->DataBuffer,
                                      NonPagedBuffer,
                                      DataSize);

            if (!KSUCCESS(Status)) {
                goto TimeZoneControlEnd;
            }

        } else {

            //
            // Allocate a buffer, get the filtered data, then copy it into the
            // user mode buffer.
            //

            FilteredData = MmAllocatePagedPool(DataSize,
                                               TIME_ZONE_ALLOCATION_TAG);

            if (FilteredData == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto TimeZoneControlEnd;
            }

            Status = RtlFilterTimeZoneData(Almanac,
                                           AlmanacSize,
                                           ZoneName,
                                           FilteredData,
                                           &DataSize);

            if (!KSUCCESS(Status)) {
                goto TimeZoneControlEnd;
            }

            Status = MmCopyToUserMode(Parameters->DataBuffer,
                                      FilteredData,
                                      DataSize);

            if (!KSUCCESS(Status)) {
                goto TimeZoneControlEnd;
            }
        }

        Parameters->DataBufferSize = DataSize;
        break;

    //
    // Read in and return the entire almanac.
    //

    case TimeZoneOperationGetAllData:
        Status = KepReadTimeZoneAlmanac(&Almanac, &AlmanacSize);
        if (!KSUCCESS(Status)) {
            goto TimeZoneControlEnd;
        }

        DataSize = AlmanacSize;
        if ((Parameters->DataBuffer == NULL) ||
            (Parameters->DataBufferSize == 0)) {

            Parameters->DataBufferSize = DataSize;
            goto TimeZoneControlEnd;
        }

        if (Parameters->DataBufferSize < DataSize) {
            Parameters->DataBufferSize = DataSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            goto TimeZoneControlEnd;
        }

        Status = MmCopyToUserMode(Parameters->DataBuffer, Almanac, DataSize);
        if (!KSUCCESS(Status)) {
            goto TimeZoneControlEnd;
        }

        Parameters->DataBufferSize = DataSize;
        break;

    //
    // Set or get the current time zone.
    //

    case TimeZoneOperationSetZone:
        Status = PsCheckPermission(PERMISSION_TIME);
        if (!KSUCCESS(Status)) {
            goto TimeZoneControlEnd;
        }

        if (Parameters->ZoneName != NULL) {
            Status = MmCreateCopyOfUserModeString(Parameters->ZoneName,
                                                  Parameters->ZoneNameSize,
                                                  TIME_ZONE_ALLOCATION_TAG,
                                                  &ZoneName);

            if (!KSUCCESS(Status)) {
                goto TimeZoneControlEnd;
            }
        }

        //
        // Allocate an original zone buffer if the caller wanted one.
        //

        OriginalZoneSize = Parameters->OriginalZoneNameSize;
        if ((Parameters->OriginalZoneName != NULL) && (OriginalZoneSize != 0)) {
            OriginalZone = MmAllocateNonPagedPool(OriginalZoneSize,
                                                  TIME_ZONE_ALLOCATION_TAG);

            if (OriginalZone == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto TimeZoneControlEnd;
            }
        }

        Status = KeSetSystemTimeZone(ZoneName, OriginalZone, &OriginalZoneSize);
        if (!KSUCCESS(Status)) {
            goto TimeZoneControlEnd;
        }

        Parameters->OriginalZoneNameSize = OriginalZoneSize;
        if (OriginalZone != NULL) {
            Status = MmCopyToUserMode(Parameters->OriginalZoneName,
                                      OriginalZone,
                                      OriginalZoneSize);

            if (!KSUCCESS(Status)) {
                goto TimeZoneControlEnd;
            }
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

TimeZoneControlEnd:
    Parameters->Status = Status;
    if (Almanac != NULL) {
        MmFreePagedPool(Almanac);
    }

    if (CurrentTimeZone != NULL) {
        MmFreeNonPagedPool(CurrentTimeZone);
    }

    if (FilteredData != NULL) {
        MmFreePagedPool(FilteredData);
    }

    if (NonPagedBuffer != NULL) {
        MmFreeNonPagedPool(NonPagedBuffer);
    }

    if (OriginalZone != NULL) {
        MmFreeNonPagedPool(OriginalZone);
    }

    if (ZoneName != NULL) {
        MmFreePagedPool(ZoneName);
    }

    return Status;
}

KSTATUS
KepInitializeTimeZoneSupport (
    PVOID TimeZoneData,
    ULONG TimeZoneDataSize
    )

/*++

Routine Description:

    This routine initializes time zone support in the kernel.

Arguments:

    TimeZoneData - Supplies a pointer to the initial time zone data from the
        loader. A copy of this data will be made.

    TimeZoneDataSize - Supplies the size of the data in bytes.

Return Value:

    Status code.

--*/

{

    PVOID NewData;
    PVOID OldData;
    ULONG OldDataSize;
    KSTATUS Status;

    NewData = NULL;
    KeInitializeSpinLock(&KeTimeZoneLock);
    RtlInitializeTimeZoneSupport(KepAcquireTimeZoneLock,
                                 KepReleaseTimeZoneLock,
                                 KepTimeZoneReallocate);

    if ((TimeZoneData == NULL) || (TimeZoneDataSize == 0)) {
        Status = STATUS_SUCCESS;
        goto InitializeTimeZoneSupportEnd;
    }

    //
    // Create a non-paged copy of the time zone data. An allocation failure for
    // time zone data is not fatal.
    //

    NewData = MmAllocateNonPagedPool(TimeZoneDataSize,
                                     TIME_ZONE_ALLOCATION_TAG);

    if (NewData == NULL) {
        Status = STATUS_SUCCESS;
        goto InitializeTimeZoneSupportEnd;
    }

    RtlCopyMemory(NewData, TimeZoneData, TimeZoneDataSize);

    //
    // Set the time zone data in the runtime library. Failure here is lame,
    // but not fatal.
    //

    Status = RtlSetTimeZoneData(NewData,
                                TimeZoneDataSize,
                                NULL,
                                &OldData,
                                &OldDataSize,
                                NULL,
                                NULL);

    if (!KSUCCESS(Status)) {
        Status = STATUS_SUCCESS;
        goto InitializeTimeZoneSupportEnd;
    }

    ASSERT(OldData == NULL);

    NewData = NULL;
    Status = STATUS_SUCCESS;

InitializeTimeZoneSupportEnd:
    if (NewData != NULL) {
        MmFreeNonPagedPool(NewData);
    }

    return Status;
}

VOID
KepAcquireTimeZoneLock (
    VOID
    )

/*++

Routine Description:

    This routine raises to dispatch and acquires the global time zone lock.

Arguments:

    None.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&KeTimeZoneLock);
    KeTimeZoneLockOldRunLevel = OldRunLevel;
    return;
}

VOID
KepReleaseTimeZoneLock (
    VOID
    )

/*++

Routine Description:

    This routine releases the global time zone lock.

Arguments:

    None.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;

    OldRunLevel = KeTimeZoneLockOldRunLevel;
    KeReleaseSpinLock(&KeTimeZoneLock);
    KeLowerRunLevel(OldRunLevel);
    return;
}

PVOID
KepTimeZoneReallocate (
    PVOID Memory,
    UINTN NewSize
    )

/*++

Routine Description:

    This routine allocates, reallocates, or frees memory for the time zone
    library.

Arguments:

    Memory - Supplies the original active allocation. If this parameter is
        NULL, this routine will simply allocate memory.

    NewSize - Supplies the new required size of the allocation. If this is
        0, then the original allocation will simply be freed.

Return Value:

    Returns a pointer to a buffer with the new size (and original contents) on
    success. This may be a new buffer or the same one.

    NULL on failure or if the new size supplied was zero.

--*/

{

    Memory = MmReallocatePool(PoolTypeNonPaged,
                              Memory,
                              NewSize,
                              TIME_ZONE_ALLOCATION_TAG);

    return Memory;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
KepReadTimeZoneAlmanac (
    PVOID *Buffer,
    PULONG BufferSize
    )

/*++

Routine Description:

    This routine attempts to read in the time zone data almanac.

Arguments:

    Buffer - Supplies a pointer where a pointer to the almanac data (allocated
        from paged pool) will be returned.

    BufferSize - Supplies a pointer where the size of the buffer will be
        returned on success.

Return Value:

    Status code. On success, the caller is responsible for freeing the returned
    data from paged pool.

--*/

{

    UINTN BytesCompleted;
    PVOID DataBuffer;
    ULONG DataSize;
    ULONGLONG FileSize;
    PIO_HANDLE Handle;
    IO_BUFFER IoBuffer;
    KSTATUS Status;

    DataBuffer = NULL;
    DataSize = 0;
    Handle = INVALID_HANDLE;

    //
    // Load the master time zone file.
    // TODO: The volume should not be hardcoded, use an API to get the system
    // configuration directory path.
    //

    Status = IoOpen(TRUE,
                    NULL,
                    TIME_ZONE_ALMANAC_FILE_PATH,
                    sizeof(TIME_ZONE_ALMANAC_FILE_PATH),
                    IO_ACCESS_READ,
                    0,
                    FILE_PERMISSION_NONE,
                    &Handle);

    if (!KSUCCESS(Status)) {
        goto ReadTimeZoneAlmanacEnd;
    }

    //
    // Allocate a buffer to store the entire time zone data file.
    //

    Status = IoGetFileSize(Handle, &FileSize);
    if (!KSUCCESS(Status)) {
        goto ReadTimeZoneAlmanacEnd;
    }

    if (FileSize > MAX_TIME_ZONE_FILE_SIZE) {
        Status = STATUS_BUFFER_OVERRUN;
        goto ReadTimeZoneAlmanacEnd;
    }

    DataSize = (ULONG)FileSize;
    DataBuffer = MmAllocatePagedPool(DataSize, TIME_ZONE_ALLOCATION_TAG);
    if (DataBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReadTimeZoneAlmanacEnd;
    }

    Status = MmInitializeIoBuffer(&IoBuffer,
                                  DataBuffer,
                                  INVALID_PHYSICAL_ADDRESS,
                                  DataSize,
                                  IO_BUFFER_FLAG_KERNEL_MODE_DATA);

    if (!KSUCCESS(Status)) {
        goto ReadTimeZoneAlmanacEnd;
    }

    //
    // Read in the file.
    //

    Status = IoRead(Handle,
                    &IoBuffer,
                    DataSize,
                    0,
                    WAIT_TIME_INDEFINITE,
                    &BytesCompleted);

    if (!KSUCCESS(Status)) {
        goto ReadTimeZoneAlmanacEnd;
    }

    if (BytesCompleted != DataSize) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto ReadTimeZoneAlmanacEnd;
    }

ReadTimeZoneAlmanacEnd:
    if (Handle != INVALID_HANDLE) {
        IoClose(Handle);
    }

    *Buffer = DataBuffer;
    *BufferSize = DataSize;
    return Status;
}

KSTATUS
KepGetCurrentTimeZone (
    PSTR *TimeZone
    )

/*++

Routine Description:

    This routine returns the current time zone.

Arguments:

    TimeZone - Supplies a pointer where a newly allocated null terminated
        string will be returned containing the current time zone. The caller
        is responsible for freeing this buffer from non-paged pool when
        finished.

Return Value:

    Status code. On success, the caller is responsible for freeing the returned
    data from non-paged pool.

--*/

{

    PSTR Allocation;
    ULONG AllocationSize;
    KSTATUS Status;

    Allocation = NULL;
    Status = RtlSelectTimeZone(NULL, NULL, &AllocationSize);
    if (!KSUCCESS(Status)) {
        goto GetCurrentTimeZoneEnd;
    }

    Allocation = MmAllocateNonPagedPool(AllocationSize,
                                        TIME_ZONE_ALLOCATION_TAG);

    if (Allocation == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetCurrentTimeZoneEnd;
    }

    Status = RtlSelectTimeZone(NULL, Allocation, &AllocationSize);
    if (!KSUCCESS(Status)) {
        goto GetCurrentTimeZoneEnd;
    }

GetCurrentTimeZoneEnd:
    if (!KSUCCESS(Status)) {
        if (Allocation != NULL) {
            MmFreeNonPagedPool(Allocation);
            Allocation = NULL;
        }
    }

    *TimeZone = Allocation;
    return Status;
}

KSTATUS
KepGetCurrentTimeZoneData (
    PVOID *Data,
    PULONG DataSize
    )

/*++

Routine Description:

    This routine returns the current time zone data.

Arguments:

    Data - Supplies a pointer where the newly allocated time zone data will be
        returned on success. The caller is responsible for freeing this
        memory from non-paged pool.

    DataSize - Supplies a pointer where the size of the data will be returned
        on success.

Return Value:

    Status code. On success, the caller is responsible for freeing the returned
    data from non-paged pool.

--*/

{

    PVOID Buffer;
    KSTATUS Status;

    Buffer = NULL;
    Status = RtlGetTimeZoneData(NULL, DataSize);
    if (!KSUCCESS(Status)) {
        goto GetCurrentTimeZoneDataEnd;
    }

    Buffer = MmAllocateNonPagedPool(*DataSize, TIME_ZONE_ALLOCATION_TAG);
    if (Buffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetCurrentTimeZoneDataEnd;
    }

    Status = RtlGetTimeZoneData(Buffer, DataSize);
    if (!KSUCCESS(Status)) {
        goto GetCurrentTimeZoneDataEnd;
    }

GetCurrentTimeZoneDataEnd:
    if (!KSUCCESS(Status)) {
        if (Buffer != NULL) {
            MmFreeNonPagedPool(Buffer);
            Buffer = NULL;
        }

        *DataSize = 0;
    }

    *Data = Buffer;
    return Status;
}

