/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timezone.c

Abstract:

    This module implements support for loading time zone data.

Author:

    Evan Green 5-Aug-2013

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"
#include "time.h"
#include <minoca/lib/tzfmt.h>

//
// -------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define TIME_ZONE_NAME_MAX (TIME_ZONE_ABBREVIATION_SIZE + 1)

#define LOCAL_TIME_TO_SYSTEM_TIME_RETRY_MAX 4

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PCSTR
RtlpTimeZoneGetString (
    PTIME_ZONE_HEADER Header,
    ULONG Offset
    );

KSTATUS
RtlpValidateTimeZoneData (
    PTIME_ZONE_HEADER Header,
    ULONG DataSize
    );

KSTATUS
RtlpTimeZoneAddString (
    PSTR StringsBase,
    ULONG StringsSize,
    PULONG CurrentStringsSize,
    PCSTR String,
    PULONG Offset
    );

VOID
RtlpPrintTimeZoneRule (
    PTIME_ZONE_HEADER Header,
    PTIME_ZONE_RULE Rule
    );

VOID
RtlpPrintTimeZone (
    PTIME_ZONE_HEADER Header,
    ULONG DataSize,
    PTIME_ZONE Zone
    );

VOID
RtlpPrintTimeZoneLeap (
    PTIME_ZONE_LEAP_SECOND Leap
    );

VOID
RtlpPrintTimeZoneEntry (
    PTIME_ZONE_HEADER Header,
    PTIME_ZONE_ENTRY ZoneEntry
    );

VOID
RtlpPrintTimeZoneDate (
    LONGLONG Date
    );

VOID
RtlpPrintTimeZoneTime (
    LONG Time,
    TIME_ZONE_LENS Lens
    );

KSTATUS
RtlpSelectTimeZone (
    PCSTR ZoneName
    );

KSTATUS
RtlpGetCurrentTimeZone (
    PSTR Buffer,
    PULONG BufferSize
    );

VOID
RtlpFindTimeZoneRules (
    PTIME_ZONE_HEADER Header,
    PTIME_ZONE_ENTRY StartingEntry,
    LONG CurrentEntry,
    LONG Year,
    LONG Month,
    PTIME_ZONE_RULE RecentRules[2]
    );

VOID
RtlpSetTimeZoneNames (
    VOID
    );

VOID
RtlpTimeZonePerformSubstitution (
    PSTR Destination,
    ULONG DestinationSize,
    PCSTR Format,
    PTIME_ZONE_RULE Rule
    );

PCSTR
RtlpTimeZoneCacheString (
    PCSTR String
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global pointer to the time zone data.
//

PVOID RtlTimeZoneData;
ULONG RtlTimeZoneDataSize;
ULONG RtlTimeZoneIndex;
PTIME_ZONE_LOCK_FUNCTION RtlAcquireTimeZoneLock;
PTIME_ZONE_LOCK_FUNCTION RtlReleaseTimeZoneLock;
PTIME_ZONE_REALLOCATE_FUNCTION RtlTimeZoneReallocate;

//
// Store arrays of the names of the months and weeks.
//

PSTR RtlMonthStrings[MONTHS_PER_YEAR] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

PSTR RtlAbbreviatedMonthStrings[MONTHS_PER_YEAR] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

PSTR RtlWeekdayStrings[DAYS_PER_WEEK] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday"
};

PSTR RtlAbbreviatedWeekdayStrings[DAYS_PER_WEEK] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat"
};

PSTR RtlAmPmStrings[2][2] = {
    {"AM", "PM"},
    {"am", "pm"}
};

CHAR RtlDaysPerMonth[2][MONTHS_PER_YEAR] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

SHORT RtlMonthDays[2][MONTHS_PER_YEAR] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335},
};

//
// Store the current time zone for standard and daylight time.
//

PCSTR RtlStandardTimeZoneName;
PCSTR RtlDaylightTimeZoneName;
LONG RtlStandardTimeZoneOffset;
LONG RtlDaylightTimeZoneOffset;

//
// Store the cache of time zone names handed out in calendar time structs.
//

PSTR *RtlTimeZoneNameCache;
ULONG RtlTimeZoneNameCacheSize;

//
// ------------------------------------------------------------------ Functions
//

RTL_API
VOID
RtlInitializeTimeZoneSupport (
    PTIME_ZONE_LOCK_FUNCTION AcquireTimeZoneLockFunction,
    PTIME_ZONE_LOCK_FUNCTION ReleaseTimeZoneLockFunction,
    PTIME_ZONE_REALLOCATE_FUNCTION ReallocateFunction
    )

/*++

Routine Description:

    This routine initializes library support functions needed by the time zone
    code.

Arguments:

    AcquireTimeZoneLockFunction - Supplies a pointer to the function called
        to acquire access to the time zone data.

    ReleaseTimeZoneLockFunction - Supplies a pointer to the function called to
        relinquish access to the time zone data.

    ReallocateFunction - Supplies a pointer to a function used to dynamically
        allocate and free memory.

Return Value:

    None.

--*/

{

    ASSERT((RtlAcquireTimeZoneLock == NULL) &&
           (RtlReleaseTimeZoneLock == NULL) &&
           (RtlTimeZoneReallocate == NULL));

    RtlAcquireTimeZoneLock = AcquireTimeZoneLockFunction;
    RtlReleaseTimeZoneLock = ReleaseTimeZoneLockFunction;
    RtlTimeZoneReallocate = ReallocateFunction;
    return;
}

RTL_API
KSTATUS
RtlFilterTimeZoneData (
    PVOID TimeZoneData,
    ULONG TimeZoneDataSize,
    PCSTR TimeZoneName,
    PVOID FilteredData,
    PULONG FilteredDataSize
    )

/*++

Routine Description:

    This routine filters the given time zone data for one specific time zone.

Arguments:

    TimeZoneData - Supplies a pointer to the buffer containing the unfiltered
        time zone data.

    TimeZoneDataSize - Supplies the size in bytes of the unfiltered time zone
        data.

    TimeZoneName - Supplies a pointer to the null terminated string containing
        the name of the time zone to retrieve.

    FilteredData - Supplies an optional pointer to the buffer where the
        filtered data will be returned. If this pointer is NULL, then only the
        size of the required data will be returned.

    FilteredDataSize - Supplies a pointer that on input contains the size of
        the filtered data buffer. On output, will return the required size of
        the output buffer to contain the filtered data.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the provided buffer was valid but too small.

    STATUS_FILE_CORRUPT if the data is invalid.

--*/

{

    ULONG CurrentRuleCount;
    ULONG CurrentStringsSize;
    ULONG EntryIndex;
    PTIME_ZONE_HEADER Header;
    BOOL Match;
    ULONG NeededSize;
    PTIME_ZONE_HEADER NewHeader;
    PTIME_ZONE_RULE NewRule;
    PTIME_ZONE_RULE NewRules;
    PTIME_ZONE NewZone;
    PTIME_ZONE_ENTRY NewZoneEntries;
    PTIME_ZONE_ENTRY NewZoneEntry;
    PTIME_ZONE_RULE Rule;
    ULONG RuleCount;
    ULONG RuleIndex;
    PTIME_ZONE_RULE Rules;
    ULONG SearchIndex;
    KSTATUS Status;
    PCSTR String;
    PSTR StringsBase;
    ULONG StringsSize;
    PTIME_ZONE Zone;
    PTIME_ZONE_ENTRY ZoneEntries;
    PTIME_ZONE_ENTRY ZoneEntry;
    ULONG ZoneEntryCount;
    ULONG ZoneIndex;
    PCSTR ZoneName;
    PTIME_ZONE Zones;

    NeededSize = 0;
    Status = RtlpValidateTimeZoneData(TimeZoneData, TimeZoneDataSize);
    if (!KSUCCESS(Status)) {
        goto FilterTimeZoneDataEnd;
    }

    Header = TimeZoneData;
    RuleCount = 0;
    StringsSize = 0;
    ZoneEntryCount = 0;

    //
    // Find the zone in question.
    //

    Zones = (PTIME_ZONE)((PVOID)Header + Header->ZoneOffset);
    for (ZoneIndex = 0; ZoneIndex < Header->ZoneCount; ZoneIndex += 1) {
        ZoneName = RtlpTimeZoneGetString(Header, Zones[ZoneIndex].Name);
        if (ZoneName == NULL) {
            Status = STATUS_FILE_CORRUPT;
            goto FilterTimeZoneDataEnd;
        }

        Match = RtlAreStringsEqualIgnoringCase(ZoneName,
                                               TimeZoneName,
                                               Header->StringsSize);

        if (Match != FALSE) {
            break;
        }
    }

    if (ZoneIndex >= Header->ZoneCount) {
        Status = STATUS_NOT_FOUND;
        goto FilterTimeZoneDataEnd;
    }

    StringsSize += RtlStringLength(ZoneName) + 1;

    //
    // Loop through the zone entries to figure out how many rule structures
    // apply.
    //

    Zone = &(Zones[ZoneIndex]);
    ZoneEntries = (PTIME_ZONE_ENTRY)((PVOID)Header + Header->ZoneEntryOffset);
    Rules = (PTIME_ZONE_RULE)((PVOID)Header + Header->RuleOffset);
    ZoneEntryCount = Zone->EntryCount;
    if (ZoneEntryCount > Header->ZoneEntryCount) {
        Status = STATUS_FILE_CORRUPT;
        goto FilterTimeZoneDataEnd;
    }

    for (EntryIndex = 0; EntryIndex < ZoneEntryCount; EntryIndex += 1) {
        ZoneEntry = &(ZoneEntries[Zone->EntryIndex + EntryIndex]);
        String = RtlpTimeZoneGetString(Header, ZoneEntry->Format);
        if (String == NULL) {
            Status = STATUS_FILE_CORRUPT;
            goto FilterTimeZoneDataEnd;
        }

        StringsSize += RtlStringLength(String) + 1;
        if (ZoneEntry->Rules == -1) {
            continue;
        }

        //
        // Look to see if this rule index was already picked up by a previous
        // zone entry.
        //

        for (SearchIndex = 0; SearchIndex < EntryIndex; SearchIndex += 1) {
            if (ZoneEntries[Zone->EntryIndex + SearchIndex].Rules ==
                ZoneEntry->Rules) {

                break;
            }
        }

        if (SearchIndex != EntryIndex) {
            continue;
        }

        //
        // Loop through all the rules to find any that apply.
        //

        for (RuleIndex = 0; RuleIndex < Header->RuleCount; RuleIndex += 1) {
            Rule = &(Rules[RuleIndex]);
            if (Rule->Number == ZoneEntry->Rules) {
                String = RtlpTimeZoneGetString(Header, Rule->Letters);
                if (String == NULL) {
                    Status = STATUS_FILE_CORRUPT;
                    goto FilterTimeZoneDataEnd;
                }

                StringsSize += RtlStringLength(String) + 1;
                RuleCount += 1;
            }
        }
    }

    //
    // Calculate the amount of space needed for the filtered data. If no buffer
    // or too small of a buffer was provided, end now. Note that this
    // estimation is not perfect as the same strings may be accounted for
    // multiple times.
    //

    NeededSize = sizeof(TIME_ZONE_HEADER) +
                 (RuleCount * sizeof(TIME_ZONE_RULE)) + sizeof(TIME_ZONE) +
                 (ZoneEntryCount * sizeof(TIME_ZONE_ENTRY)) +
                 (Header->LeapCount * sizeof(TIME_ZONE_LEAP_SECOND)) +
                 StringsSize;

    if (FilteredData == NULL) {
        Status = STATUS_SUCCESS;
        goto FilterTimeZoneDataEnd;
    }

    if (*FilteredDataSize < NeededSize) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto FilterTimeZoneDataEnd;
    }

    //
    // The buffer's big enough, write out the filtered header.
    //

    NewHeader = FilteredData;
    NewHeader->Magic = TIME_ZONE_HEADER_MAGIC;
    NewHeader->RuleOffset = sizeof(TIME_ZONE_HEADER);
    NewHeader->RuleCount = RuleCount;
    NewHeader->ZoneOffset = NewHeader->RuleOffset +
                            (RuleCount * sizeof(TIME_ZONE_RULE));

    NewHeader->ZoneCount = 1;
    NewHeader->ZoneEntryOffset = NewHeader->ZoneOffset +
                                 (1 * sizeof(TIME_ZONE));

    NewHeader->ZoneEntryCount = Zone->EntryCount;
    NewHeader->LeapOffset =
                         NewHeader->ZoneEntryOffset +
                         (NewHeader->ZoneEntryCount * sizeof(TIME_ZONE_ENTRY));

    NewHeader->LeapCount = Header->LeapCount;
    NewHeader->StringsOffset =
                        NewHeader->LeapOffset +
                        (NewHeader->LeapCount * sizeof(TIME_ZONE_LEAP_SECOND));

    NewHeader->StringsSize = StringsSize;

    //
    // Set up the new pointers and indices.
    //

    StringsBase = (PVOID)NewHeader + NewHeader->StringsOffset;
    CurrentStringsSize = 0;
    CurrentRuleCount = 0;
    NewRules = (PVOID)NewHeader + NewHeader->RuleOffset;
    NewZoneEntries = (PVOID)NewHeader + NewHeader->ZoneEntryOffset;

    //
    // Copy the zone in, with its name.
    //

    NewZone = (PVOID)NewHeader + NewHeader->ZoneOffset;
    Status = RtlpTimeZoneAddString(StringsBase,
                                   StringsSize,
                                   &CurrentStringsSize,
                                   ZoneName,
                                   &(NewZone->Name));

    if (!KSUCCESS(Status)) {
        goto FilterTimeZoneDataEnd;
    }

    NewZone->EntryIndex = 0;
    NewZone->EntryCount = ZoneEntryCount;

    //
    // Loop copying the zone entries in, and the rules along the way.
    //

    for (EntryIndex = 0; EntryIndex < ZoneEntryCount; EntryIndex += 1) {
        ZoneEntry = &(ZoneEntries[Zone->EntryIndex + EntryIndex]);
        NewZoneEntry = &(NewZoneEntries[EntryIndex]);
        String = RtlpTimeZoneGetString(Header, ZoneEntry->Format);
        if (String == NULL) {
            Status = STATUS_FILE_CORRUPT;
            goto FilterTimeZoneDataEnd;
        }

        RtlCopyMemory(NewZoneEntry, ZoneEntry, sizeof(TIME_ZONE_ENTRY));
        Status = RtlpTimeZoneAddString(StringsBase,
                                       StringsSize,
                                       &CurrentStringsSize,
                                       String,
                                       &(NewZoneEntry->Format));

        if (!KSUCCESS(Status)) {
            goto FilterTimeZoneDataEnd;
        }

        if (ZoneEntry->Rules == -1) {
            continue;
        }

        //
        // Look to see if this rule index was already picked up by a previous
        // zone entry.
        //

        for (SearchIndex = 0; SearchIndex < EntryIndex; SearchIndex += 1) {
            if (ZoneEntries[Zone->EntryIndex + SearchIndex].Rules ==
                ZoneEntry->Rules) {

                break;
            }
        }

        if (SearchIndex != EntryIndex) {
            continue;
        }

        //
        // Loop through all the rules and copy any that apply.
        //

        for (RuleIndex = 0; RuleIndex < Header->RuleCount; RuleIndex += 1) {
            Rule = &(Rules[RuleIndex]);
            if (Rule->Number == ZoneEntry->Rules) {
                NewRule = &(NewRules[CurrentRuleCount]);
                RtlCopyMemory(NewRule, Rule, sizeof(TIME_ZONE_RULE));
                String = RtlpTimeZoneGetString(Header, Rule->Letters);
                if (String == NULL) {
                    Status = STATUS_FILE_CORRUPT;
                    goto FilterTimeZoneDataEnd;
                }

                Status = RtlpTimeZoneAddString(StringsBase,
                                               StringsSize,
                                               &CurrentStringsSize,
                                               String,
                                               &(NewRule->Letters));

                if (!KSUCCESS(Status)) {
                    goto FilterTimeZoneDataEnd;
                }

                CurrentRuleCount += 1;
            }
        }
    }

    ASSERT(CurrentRuleCount == RuleCount);
    ASSERT(CurrentStringsSize <= StringsSize);

    //
    // Copy the leap seconds.
    //

    RtlCopyMemory((PVOID)NewHeader + NewHeader->LeapOffset,
                  (PVOID)Header + Header->LeapOffset,
                  NewHeader->LeapCount * sizeof(TIME_ZONE_LEAP_SECOND));

    NewHeader->StringsSize = CurrentStringsSize;
    Status = STATUS_SUCCESS;

FilterTimeZoneDataEnd:
    *FilteredDataSize = NeededSize;
    return Status;
}

RTL_API
KSTATUS
RtlGetTimeZoneData (
    PVOID Data,
    PULONG DataSize
    )

/*++

Routine Description:

    This routine copies the current time zone data into the given buffer.

Arguments:

    Data - Supplies a pointer where the current time zone data will be copied
        to.

    DataSize - Supplies a pointer that on input contains the size of the
        supplied data buffer. On output, will contain the size of the
        current data (whether or not a buffer was supplied).

Return Value:

    STATUS_SUCCESS if the time zone data was accepted.

    STATUS_BUFFER_TOO_SMALL if the provided buffer was valid but too small.

    STATUS_NO_DATA_AVAILABLE if there is no data or the data is empty.

--*/

{

    KSTATUS Status;

    Status = STATUS_SUCCESS;
    RtlAcquireTimeZoneLock();
    if (RtlTimeZoneData == NULL) {
        Status = STATUS_NO_DATA_AVAILABLE;

    } else if (Data != NULL) {
        if (*DataSize < RtlTimeZoneDataSize) {
            Status = STATUS_BUFFER_TOO_SMALL;

        } else {
            RtlCopyMemory(Data, RtlTimeZoneData, RtlTimeZoneDataSize);
        }
    }

    *DataSize = RtlTimeZoneDataSize;
    RtlReleaseTimeZoneLock();
    return Status;
}

RTL_API
KSTATUS
RtlSetTimeZoneData (
    PVOID Data,
    ULONG DataSize,
    PCSTR ZoneName,
    PVOID *OldData,
    PULONG OldDataSize,
    PSTR OriginalZoneBuffer,
    PULONG OriginalZoneBufferSize
    )

/*++

Routine Description:

    This routine sets the current time zone data.

Arguments:

    Data - Supplies a pointer to the time zone data to set. No copy will be
        made, the caller must ensure the data is not modified or freed until
        another call to set time zone data completes.

    DataSize - Supplies the size of the data in bytes.

    ZoneName - Supplies an optional pointer to the name of a time zone to
        select within the data. If this pointer is NULL, the first time zone
        in the data will be used.

    OldData - Supplies a pointer where the original (now decommissioned) time
        zone data will be returned.

    OldDataSize - Supplies a pointer where the size of the original
        decommissioned data will be returned.

    OriginalZoneBuffer - Supplies an optional pointer where the original (or
        current if no new time zone was provided) time zone will be returned.

    OriginalZoneBufferSize - Supplies a pointer that on input contains the
        size of the original zone buffer in bytes. On output, this value will
        contain the size of the original zone buffer needed to contain the
        name of the current time zone (even if no buffer was provided).

Return Value:

    STATUS_SUCCESS if the time zone data was accepted.

    STATUS_FILE_CORRUPT if the data is invalid.

    STATUS_NOT_FOUND if the selected time zone could not be found in the new
        data. If this is the case, the new data will not be activated.

--*/

{

    ULONG OriginalIndex;
    KSTATUS Status;

    Status = RtlpValidateTimeZoneData(Data, DataSize);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    RtlAcquireTimeZoneLock();
    if (OriginalZoneBufferSize != NULL) {
        Status = RtlpGetCurrentTimeZone(OriginalZoneBuffer,
                                        OriginalZoneBufferSize);

        if (!KSUCCESS(Status)) {
            goto SetTimeZoneDataEnd;
        }
    }

    *OldData = RtlTimeZoneData;
    *OldDataSize = RtlTimeZoneDataSize;
    OriginalIndex = RtlTimeZoneIndex;
    RtlTimeZoneData = Data;
    RtlTimeZoneDataSize = DataSize;
    RtlTimeZoneIndex = 0;
    if (ZoneName != NULL) {
        Status = RtlpSelectTimeZone(ZoneName);

        //
        // If the select operation failed, roll back to the original data.
        //

        if (!KSUCCESS(Status)) {
            RtlTimeZoneData = *OldData;
            RtlTimeZoneDataSize = *OldDataSize;
            RtlTimeZoneIndex = OriginalIndex;
            *OldData = NULL;
            *OldDataSize = 0;
        }

    } else {
        RtlpSetTimeZoneNames();
    }

SetTimeZoneDataEnd:
    RtlReleaseTimeZoneLock();
    return Status;
}

RTL_API
KSTATUS
RtlListTimeZones (
    PVOID Data,
    ULONG DataSize,
    PSTR ListBuffer,
    PULONG ListBufferSize
    )

/*++

Routine Description:

    This routine creates a list of all time zones available in the given (or
    currently in use) data.

Arguments:

    Data - Supplies a pointer to the time zone data to debug print. If this
        is NULL, the current data will be used.

    DataSize - Supplies the size of the data in bytes.

    ListBuffer - Supplies an optional pointer to a buffer where the null
        terminated strings representing the names of the time zones will be
        returned on success. The buffer will be terminated by an empty string.

    ListBufferSize - Supplies a pointer that on input contains the size of the
        list buffer in bytes. On output this will contain the size needed to
        hold all the strings, regardless of whether a buffer was passed in.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the provided buffer was too small.

    STATUS_FILE_CORRUPT if the time zone data was not valid.

    STATUS_NO_DATA_AVAILABLE if there is no data or the data is empty.

--*/

{

    ULONG BufferSize;
    ULONG CurrentSize;
    PTIME_ZONE_HEADER Header;
    BOOL LockHeld;
    KSTATUS Status;
    PTIME_ZONE Zone;
    ULONG ZoneIndex;
    PCSTR ZoneName;
    ULONG ZoneNameLength;
    PTIME_ZONE Zones;

    BufferSize = *ListBufferSize;
    CurrentSize = 0;
    LockHeld = FALSE;
    if ((Data == NULL) || (DataSize == 0)) {
        RtlAcquireTimeZoneLock();
        LockHeld = TRUE;
        Data = RtlTimeZoneData;
        DataSize = RtlTimeZoneDataSize;
    }

    if ((Data == NULL) || (DataSize == 0)) {
        Status = STATUS_NO_DATA_AVAILABLE;
        goto ListTimeZonesEnd;
    }

    Status = RtlpValidateTimeZoneData(Data, DataSize);
    if (!KSUCCESS(Status)) {
        goto ListTimeZonesEnd;
    }

    Header = Data;
    Zones = (PVOID)Header + Header->ZoneOffset;

    //
    // Loop through all the zones, copying if there's a buffer provided.
    //

    for (ZoneIndex = 0; ZoneIndex < Header->ZoneCount; ZoneIndex += 1) {
        Zone = &(Zones[ZoneIndex]);
        ZoneName = RtlpTimeZoneGetString(Header, Zone->Name);
        if (ZoneName == NULL) {
            Status = STATUS_FILE_CORRUPT;
            goto ListTimeZonesEnd;
        }

        ZoneNameLength = RtlStringLength(ZoneName) + 1;
        if (CurrentSize + ZoneNameLength <= BufferSize) {
            RtlCopyMemory(ListBuffer + CurrentSize, ZoneName, ZoneNameLength);
        }

        CurrentSize += ZoneNameLength;
    }

    //
    // Also copy an empty string.
    //

    if (CurrentSize + sizeof("") <= BufferSize) {
        RtlCopyMemory(ListBuffer + CurrentSize, "", sizeof(""));
    }

    CurrentSize += sizeof("");

    //
    // If there was no buffer, then return happily.
    //

    if (BufferSize == 0) {
        Status = STATUS_SUCCESS;
        goto ListTimeZonesEnd;
    }

    //
    // If the buffer wasn't big enough, return failure.
    //

    if (CurrentSize > BufferSize) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto ListTimeZonesEnd;
    }

    Status = STATUS_SUCCESS;

ListTimeZonesEnd:
    if (LockHeld != FALSE) {
        RtlReleaseTimeZoneLock();
    }

    *ListBufferSize = CurrentSize;
    return Status;
}

RTL_API
VOID
RtlGetTimeZoneNames (
    PCSTR *StandardName,
    PCSTR *DaylightName,
    PLONG StandardGmtOffset,
    PLONG DaylightGmtOffset
    )

/*++

Routine Description:

    This routine returns the names of the currently selected time zone.

Arguments:

    StandardName - Supplies an optional pointer where a pointer to the standard
        time zone name will be returned on success. The caller must not modify
        this memory, and it may change if the time zone is changed.

    DaylightName - Supplies an optional pointer where a pointer to the Daylight
        Saving time zone name will be returned on success. The caller must not
        modify this memory, and it may change if the time zone is changed.

    StandardGmtOffset - Supplies an optional pointer where the offset from GMT
        in seconds will be returned for the time zone.

    DaylightGmtOffset - Supplies an optional pointer where the offset from GMT
        in seconds during Daylight Saving will be returned.

Return Value:

    None.

--*/

{

    if (StandardName != NULL) {
        *StandardName = RtlStandardTimeZoneName;
    }

    if (DaylightName != NULL) {
        *DaylightName = RtlDaylightTimeZoneName;
    }

    if (StandardGmtOffset != NULL) {
        *StandardGmtOffset = RtlStandardTimeZoneOffset;
    }

    if (DaylightGmtOffset != NULL) {
        *DaylightGmtOffset = RtlDaylightTimeZoneOffset;
    }

    return;
}

RTL_API
KSTATUS
RtlSelectTimeZone (
    PSTR ZoneName,
    PSTR OriginalZoneBuffer,
    PULONG OriginalZoneBufferSize
    )

/*++

Routine Description:

    This routine selects a time zone from the current set of data.

Arguments:

    ZoneName - Supplies an optional pointer to a null terminated string
        containing the name of the time zone. If this parameter is NULL then
        the current time zone will simply be returned.

    OriginalZoneBuffer - Supplies an optional pointer where the original (or
        current if no new time zone was provided) time zone will be returned.

    OriginalZoneBufferSize - Supplies a pointer that on input contains the
        size of the original zone buffer in bytes. On output, this value will
        contain the size of the original zone buffer needed to contain the
        name of the current time zone (even if no buffer was provided).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if a time zone with the given name could not be found.

    STATUS_NO_DATA_AVAILABLE if no time zone data has been set yet.

    STATUS_BUFFER_TOO_SMALL if the buffer provided to get the original time
        zone name was too small. If this is the case, the new time zone will
        not have been set.

--*/

{

    KSTATUS Status;

    RtlAcquireTimeZoneLock();

    //
    // Copy the original name in first if conditions are right.
    //

    if (OriginalZoneBufferSize != NULL) {
        Status = RtlpGetCurrentTimeZone(OriginalZoneBuffer,
                                        OriginalZoneBufferSize);

        if (!KSUCCESS(Status)) {
            goto SelectTimeZoneEnd;
        }
    }

    if (ZoneName != NULL) {
        Status = RtlpSelectTimeZone(ZoneName);
        if (!KSUCCESS(Status)) {
            goto SelectTimeZoneEnd;
        }
    }

    Status = STATUS_SUCCESS;

SelectTimeZoneEnd:
    RtlReleaseTimeZoneLock();
    return Status;
}

RTL_API
KSTATUS
RtlSystemTimeToLocalCalendarTime (
    PSYSTEM_TIME SystemTime,
    PCALENDAR_TIME CalendarTime
    )

/*++

Routine Description:

    This routine converts the given system time into calendar time in the
    current local time zone.

Arguments:

    SystemTime - Supplies a pointer to the system time to convert.

    CalendarTime - Supplies a pointer to the calendar time to initialize based
        on the given system time.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OUT_OF_BOUNDS if the given system time is too funky.

--*/

{

    PTIME_ZONE_RULE CurrentRules[2];
    LONG DaysInMonth;
    PTIME_ZONE_RULE EffectiveRule;
    ULONG EntryIndex;
    PCSTR Format;
    CALENDAR_TIME GmtTime;
    PTIME_ZONE_HEADER Header;
    LONG Leap;
    LONG LocalStandardTime;
    PTIME_ZONE_OCCASION Occasion;
    BOOL RuleApplies;
    LONG RuleMonthDay;
    KSTATUS Status;
    LONG Time;
    LONG Weekday;
    PTIME_ZONE Zone;
    PTIME_ZONE_ENTRY ZoneEntries;
    CHAR ZoneNameBuffer[TIME_ZONE_NAME_MAX];

    EffectiveRule = NULL;
    Format = NULL;
    Status = RtlSystemTimeToGmtCalendarTime(SystemTime, CalendarTime);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    RtlCopyMemory(&GmtTime, CalendarTime, sizeof(CALENDAR_TIME));
    RtlAcquireTimeZoneLock();
    Header = RtlTimeZoneData;
    if (Header == NULL) {
        goto SystemTimeToLocalCalendarTimeEnd;
    }

    //
    // Get a pointer to the current time zone and the beginning of its zone
    // entries.
    //

    Zone = (PVOID)Header + Header->ZoneOffset;
    Zone += RtlTimeZoneIndex;
    ZoneEntries = (PVOID)Header + Header->ZoneEntryOffset;
    ZoneEntries += Zone->EntryIndex;

    //
    // Find the current zone entry.
    //

    for (EntryIndex = 0; EntryIndex < Zone->EntryCount; EntryIndex += 1) {
        if (ZoneEntries[EntryIndex].Until > SystemTime->Seconds) {
            break;
        }
    }

    if (EntryIndex == Zone->EntryCount) {
        if (Zone->EntryCount == 0) {
            Status = STATUS_FILE_CORRUPT;
            goto SystemTimeToLocalCalendarTimeEnd;
        }

        EntryIndex = Zone->EntryCount - 1;
    }

    Format = RtlpTimeZoneGetString(Header, ZoneEntries[EntryIndex].Format);

    //
    // Compute the local time with the GMT offset for the current zone entry.
    //

    CalendarTime->GmtOffset = ZoneEntries[EntryIndex].GmtOffset +
                              ZoneEntries[EntryIndex].Save;

    CalendarTime->Second += CalendarTime->GmtOffset;
    RtlpNormalizeCalendarTime(CalendarTime);
    CalendarTime->IsDaylightSaving = FALSE;
    if (ZoneEntries[EntryIndex].Save != 0) {
        CalendarTime->IsDaylightSaving = TRUE;
    }

    //
    // If this timezone has no daylight saving rules, there's no need to go
    // digging through rules.
    //

    if (ZoneEntries[EntryIndex].Rules == -1) {
        RtlpTimeZonePerformSubstitution(ZoneNameBuffer,
                                        sizeof(ZoneNameBuffer),
                                        Format,
                                        NULL);

        CalendarTime->TimeZone = RtlpTimeZoneCacheString(ZoneNameBuffer);
        Status = STATUS_SUCCESS;
        goto SystemTimeToLocalCalendarTimeEnd;
    }

    //
    // Figure out the two rules (or at least one) that apply here.
    //

    RtlpFindTimeZoneRules(Header,
                          ZoneEntries,
                          EntryIndex,
                          CalendarTime->Year,
                          CalendarTime->Month,
                          CurrentRules);

    LocalStandardTime = (CalendarTime->Hour * SECONDS_PER_HOUR) +
                        (CalendarTime->Minute * SECONDS_PER_MINUTE) +
                        CalendarTime->Second;

    //
    // Apply the previous rule if there is one.
    //

    if (CurrentRules[1] != NULL) {
        EffectiveRule = CurrentRules[1];
        if (CurrentRules[1]->Save != 0) {
            CalendarTime->Second += CurrentRules[1]->Save;
            RtlpNormalizeCalendarTime(CalendarTime);
        }
    }

    //
    // If there is no first rule to test, this is done.
    //

    if (CurrentRules[0] == NULL) {

        ASSERT(CurrentRules[1] == NULL);

        Status = STATUS_SUCCESS;
        goto SystemTimeToLocalCalendarTimeEnd;
    }

    //
    // Figure out if the first rule applies, and apply it if so.
    //

    RuleApplies = FALSE;
    RuleMonthDay = 31;
    Occasion = &(CurrentRules[0]->On);

    //
    // If the current rule is not this month, the rule definitely applies,
    // either as a previous month of this year, or a month in last year.
    //

    if (CurrentRules[0]->Month != CalendarTime->Month) {
        RuleApplies = TRUE;

    //
    // Calculating the day of the month this rule applies on is easy if it's
    // spelled out.
    //

    } else if (Occasion->Type == TimeZoneOccasionMonthDate) {
        RuleMonthDay = CurrentRules[0]->On.MonthDay;

    //
    // The day of the month this rule applies on depends on the day of the
    // week. Start by calculating the day of the week for the first of the
    // month.
    //

    } else {
        Status = RtlpCalculateWeekdayForMonth(CalendarTime->Year,
                                              CalendarTime->Month,
                                              &Weekday);

        if (!KSUCCESS(Status)) {
            goto SystemTimeToLocalCalendarTimeEnd;
        }

        Leap = 0;
        if (IS_LEAP_YEAR(CalendarTime->Year)) {
            Leap = 1;
        }

        DaysInMonth = RtlDaysPerMonth[Leap][CalendarTime->Month];
        RuleMonthDay = 1;

        //
        // Make the day of the month line up with the first instance of the
        // weekday in the rule.
        //

        if (Occasion->Weekday >= Weekday) {
            RuleMonthDay += Occasion->Weekday - Weekday;

        } else {
            RuleMonthDay += DAYS_PER_WEEK - (Weekday - Occasion->Weekday);
        }

        switch (Occasion->Type) {

        //
        // Add a week as many times as possible.
        //

        case TimeZoneOccasionLastWeekday:
            while (RuleMonthDay + DAYS_PER_WEEK <= DaysInMonth) {
                RuleMonthDay += DAYS_PER_WEEK;
            }

            break;

        //
        // Add a week as long as it's less than the required minimum month day.
        // If that pushes it over the month, then the occasion doesn't exist.
        //

        case TimeZoneOccasionGreaterOrEqualWeekday:
            while (RuleMonthDay < Occasion->MonthDay) {
                RuleMonthDay += DAYS_PER_WEEK;
            }

            if (RuleMonthDay > DaysInMonth) {
                RuleMonthDay = 31;
            }

            break;

        //
        // If the first instance of that weekday is already too far, then the
        // occasion doesn't exist. Otherwise, keep adding weeks as long as it's
        // still under the limit.
        //

        case TimeZoneOccasionLessOrEqualWeekday:
            if (RuleMonthDay > Occasion->MonthDay) {
                RuleMonthDay = 31;

            } else {
                while (RuleMonthDay + DAYS_PER_WEEK <
                       Occasion->MonthDay) {

                    RuleMonthDay += DAYS_PER_WEEK;
                }
            }

            break;

        default:

            ASSERT(FALSE);

            Status = STATUS_FILE_CORRUPT;
            goto SystemTimeToLocalCalendarTimeEnd;
        }
    }

    //
    // If the day of the month is after the rule occasion, the rule definitely
    // applies. If the day of the month is equal to the day the rule applies,
    // check the time of day.
    //

    if (RuleApplies == FALSE) {
        if (CalendarTime->Day > RuleMonthDay) {
            RuleApplies = TRUE;

        } else if (CalendarTime->Day == RuleMonthDay) {
            switch (CurrentRules[0]->AtLens) {
            case TimeZoneLensLocalTime:
                Time = (CalendarTime->Hour * SECONDS_PER_HOUR) +
                       (CalendarTime->Minute * SECONDS_PER_MINUTE) +
                       CalendarTime->Second;

                break;

            case TimeZoneLensLocalStandardTime:
                Time = LocalStandardTime;
                break;

            case TimeZoneLensUtc:
                Time = (GmtTime.Hour * SECONDS_PER_HOUR) +
                       (GmtTime.Minute * SECONDS_PER_MINUTE) +
                       GmtTime.Second;

                break;

            default:
                Time = SECONDS_PER_DAY;
                break;
            }

            if (Time >= CurrentRules[0]->At) {
                RuleApplies = TRUE;
            }
        }
    }

    //
    // If after all that this rule applies, apply it and unapply the previous
    // rule.
    //

    if (RuleApplies != FALSE) {
        EffectiveRule = CurrentRules[0];
        CalendarTime->Second += CurrentRules[0]->Save;
        if (CurrentRules[1] != NULL) {
            CalendarTime->Second -= CurrentRules[1]->Save;
        }

        RtlpNormalizeCalendarTime(CalendarTime);
    }

SystemTimeToLocalCalendarTimeEnd:
    if (EffectiveRule != NULL) {
        if (EffectiveRule->Save != 0) {
            CalendarTime->IsDaylightSaving = TRUE;
        }

        CalendarTime->GmtOffset += EffectiveRule->Save;
        RtlpTimeZonePerformSubstitution(ZoneNameBuffer,
                                        sizeof(ZoneNameBuffer),
                                        Format,
                                        EffectiveRule);

        CalendarTime->TimeZone = RtlpTimeZoneCacheString(ZoneNameBuffer);
    }

    RtlReleaseTimeZoneLock();
    return Status;
}

RTL_API
KSTATUS
RtlLocalCalendarTimeToSystemTime (
    PCALENDAR_TIME CalendarTime,
    PSYSTEM_TIME SystemTime
    )

/*++

Routine Description:

    This routine converts the given calendar time, assumed to be a local date
    and time, into its corresponding system time. On success, this routine will
    update the supplied calendar time to fill out all fields. The GMT offset
    of the supplied calendar time will be ignored in favor or the local time
    zone's GMT offset.

Arguments:

    CalendarTime - Supplies a pointer to the local calendar time to convert.

    SystemTime - Supplies a pointer where the system time will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OUT_OF_BOUNDS if the given local calendar time is too funky.

--*/

{

    PTIME_ZONE_RULE CurrentRules[2];
    LONG Delta;
    LONG EntryIndex;
    PCALENDAR_TIME FirstLocalTime;
    CALENDAR_TIME GmtCalendarTime;
    PTIME_ZONE_HEADER Header;
    CALENDAR_TIME LocalTimeBuffer[2];
    BOOL LockHeld;
    ULONG RetryCount;
    LONG Save;
    PCALENDAR_TIME SecondLocalTime;
    KSTATUS Status;
    PCALENDAR_TIME TemporaryLocalTime;
    PTIME_ZONE Zone;
    PTIME_ZONE_ENTRY ZoneEntries;

    LockHeld = FALSE;

    //
    // Standardize the given calendar time's daylight saving value by setting
    // all postive values to TRUE.
    //

    if (CalendarTime->IsDaylightSaving > 0) {
        CalendarTime->IsDaylightSaving = TRUE;
    }

    //
    // First make a copy of the calendar time, treat the copy as GMT time and
    // convert it to a GMT system time.
    //

    RtlCopyMemory(&GmtCalendarTime, CalendarTime, sizeof(CALENDAR_TIME));
    Status = RtlGmtCalendarTimeToSystemTime(&GmtCalendarTime, SystemTime);
    if (!KSUCCESS(Status)) {
        goto LocalCalendarTimeToSystemTimeEnd;
    }

    //
    // Now convert the UTC system time into a local time. This will get a local
    // GMT offset for the UTC month, date, and time.
    //

    FirstLocalTime = &(LocalTimeBuffer[0]);
    Status = RtlSystemTimeToLocalCalendarTime(SystemTime, FirstLocalTime);
    if (!KSUCCESS(Status)) {
        goto LocalCalendarTimeToSystemTimeEnd;
    }

    ASSERT(FirstLocalTime->IsDaylightSaving >= 0);

    //
    // Get the system time back into the correct time zone by subtracting the
    // GMT offset.
    //

    SystemTime->Seconds -= FirstLocalTime->GmtOffset;

    //
    // Loop attempting to land in the correct time zone.
    //

    SecondLocalTime = &(LocalTimeBuffer[1]);
    RetryCount = 0;
    while (RetryCount < LOCAL_TIME_TO_SYSTEM_TIME_RETRY_MAX) {
        Status = RtlSystemTimeToLocalCalendarTime(SystemTime, SecondLocalTime);
        if (!KSUCCESS(Status)) {
            goto LocalCalendarTimeToSystemTimeEnd;
        }

        ASSERT(SecondLocalTime->IsDaylightSaving >= 0);

        //
        // Look to see if the GMT offsets are different. A difference indicates
        // that a time zone change is being straddled.
        //

        if (FirstLocalTime->GmtOffset != SecondLocalTime->GmtOffset) {

            //
            // If the given calendar time differs in savings from the second
            // local time, then either the calendar time didn't apply the
            // savings and the GMT offset jump just did or the calendar time
            // had applied the savings and the GMT offset jump just corretly
            // reversed that. Use the UTC system time that has been calculated.
            //

            if ((CalendarTime->IsDaylightSaving >= 0) &&
                (CalendarTime->IsDaylightSaving !=
                 SecondLocalTime->IsDaylightSaving)) {

                Status = STATUS_SUCCESS;
                goto LocalCalendarTimeToSystemTimeEnd;

            //
            // If the given calendar time's savings is specified and matches
            // with the second local time, then adjust the time by the
            // difference in GMT offsets. The assumption is that the caller
            // already handled the savings and this needs to correct back in
            // order to honor that. Do the same thing is the savings was
            // unknown. The caller wasn't sure and nothing can be certain yet.
            //

            } else {
                Delta = FirstLocalTime->GmtOffset - SecondLocalTime->GmtOffset;
                SystemTime->Seconds += Delta;
            }

        //
        // If the GMT offsets are equal there is some good news and some bad
        // news. Handle it below.
        //

        } else {

            ASSERT(FirstLocalTime->IsDaylightSaving ==
                   SecondLocalTime->IsDaylightSaving);

            //
            // Good news. If the supplied time has an unknown daylight savings
            // or it agrees with the daylight savings of the two times, then
            // everything is all set.
            //

            if ((CalendarTime->IsDaylightSaving < 0) ||
                (CalendarTime->IsDaylightSaving ==
                 SecondLocalTime->IsDaylightSaving)) {

                Status = STATUS_SUCCESS;
                goto LocalCalendarTimeToSystemTimeEnd;

            //
            // The bad news is that the user applied savings, one way or
            // another, and the time needs to be adjusted. It gets worse. There
            // is no way to figure out how much the savings should be for the
            // given calendar without parsing the rules. Break out and do that.
            //

            } else {
                break;
            }
        }

        //
        // Swap the local time buffers and retry.
        //

        TemporaryLocalTime = FirstLocalTime;
        FirstLocalTime = SecondLocalTime;
        SecondLocalTime = TemporaryLocalTime;
        RetryCount += 1;
    }

    //
    // If the maximum retries have occurred, then bail out now. The system time
    // is as accurate as it's going to get.
    //

    if (RetryCount == LOCAL_TIME_TO_SYSTEM_TIME_RETRY_MAX) {
        Status = STATUS_SUCCESS;
        goto LocalCalendarTimeToSystemTimeEnd;
    }

    //
    // Now it's time to do it the hard way. The caller supplied a date that has
    // no time savings, but a time that already incorporated savings for the
    // year (or vice versa). As such, the current UTC system time is wrong! The
    // savings rule for the year either needs to be added or subtracted
    // depending on the disagreement. Go hunting for the rules that apply to
    // the date and figure out how to modify the system time. If no rules
    // apply, then consider savings and non-savings to be the same.
    //

    RtlAcquireTimeZoneLock();
    LockHeld = TRUE;

    //
    // If there is no time zone data, then everything must be in GMT. The
    // system time is currently correct for that.
    //

    Header = RtlTimeZoneData;
    if (Header == NULL) {
        Status = STATUS_SUCCESS;
        goto LocalCalendarTimeToSystemTimeEnd;
    }

    //
    // Get a pointer to the current time zone and the beginning of its zone
    // entries.
    //

    Zone = (PVOID)Header + Header->ZoneOffset;
    Zone += RtlTimeZoneIndex;
    ZoneEntries = (PVOID)Header + Header->ZoneEntryOffset;
    ZoneEntries += Zone->EntryIndex;

    //
    // Find the current zone entry.
    //

    for (EntryIndex = 0; EntryIndex < Zone->EntryCount; EntryIndex += 1) {
        if (ZoneEntries[EntryIndex].Until > SystemTime->Seconds) {
            break;
        }
    }

    if (EntryIndex == Zone->EntryCount) {
        if (Zone->EntryCount == 0) {
            Status = STATUS_FILE_CORRUPT;
            goto LocalCalendarTimeToSystemTimeEnd;
        }

        EntryIndex = Zone->EntryCount - 1;
    }

    //
    // If this timezone has no daylight saving rules, there's no need to go
    // digging through rules. Savings and non-saving times are equal, just exit
    // now. The system time is correct.
    //

    if (ZoneEntries[EntryIndex].Rules == -1) {
        Status = STATUS_SUCCESS;
        goto LocalCalendarTimeToSystemTimeEnd;
    }

    //
    // Figure out the two rules (or at least one) that apply here.
    //

    RtlpFindTimeZoneRules(Header,
                          ZoneEntries,
                          EntryIndex,
                          CalendarTime->Year,
                          CalendarTime->Month,
                          CurrentRules);

    //
    // Dig into the rules to see if there were any recent savings. The goal is
    // to just find a neighboring rule and not a rule that 100% applies to the
    // date.
    //

    Save = 0;
    if ((CurrentRules[1] != NULL) && (CurrentRules[1]->Save != 0)) {
        Save = CurrentRules[1]->Save;

    } else if ((CurrentRules[0] != NULL) && (CurrentRules[0]->Save != 0)) {
        Save = CurrentRules[0]->Save;
    }

    //
    // If there are no rules for saving around this time, then exit now. The
    // time is correct.
    //

    if (Save == 0) {
        goto LocalCalendarTimeToSystemTimeEnd;
    }

    //
    // Otherwise the savings need to be applied. Whether it is addition or
    // subtraction depends on the disagreement in daylight savings.
    //

    ASSERT(CalendarTime->IsDaylightSaving >= 0);
    ASSERT(SecondLocalTime->IsDaylightSaving >= 0);

    if ((CalendarTime->IsDaylightSaving > 0) &&
        (SecondLocalTime->IsDaylightSaving == 0)) {

        SystemTime->Seconds -= Save;

    } else {

        ASSERT((CalendarTime->IsDaylightSaving == 0) &&
               (SecondLocalTime->IsDaylightSaving > 0));

        SystemTime->Seconds += Save;
    }

LocalCalendarTimeToSystemTimeEnd:
    if (LockHeld != FALSE) {
        RtlReleaseTimeZoneLock();
    }

    //
    // If successful, then the current system time is correct, but the provided
    // calendar time needs to be handed back fully qualified as the
    // corresponding local time.
    //

    if (KSUCCESS(Status)) {
        Status = RtlSystemTimeToLocalCalendarTime(SystemTime, CalendarTime);
    }

    return Status;
}

RTL_API
VOID
RtlDebugPrintTimeZoneData (
    PVOID Data,
    ULONG DataSize
    )

/*++

Routine Description:

    This routine debug prints the given time zone data.

Arguments:

    Data - Supplies a pointer to the time zone data to debug print. If this is
        NULL, the current data will be used.

    DataSize - Supplies the size of the data in bytes.

Return Value:

    None.

--*/

{

    PTIME_ZONE_HEADER Header;
    ULONG LeapIndex;
    PTIME_ZONE_LEAP_SECOND Leaps;
    BOOL LockHeld;
    ULONG RuleIndex;
    PTIME_ZONE_RULE Rules;
    KSTATUS Status;
    ULONG ZoneIndex;
    PTIME_ZONE Zones;

    LockHeld = FALSE;
    if (Data == NULL) {
        RtlAcquireTimeZoneLock();
        LockHeld = TRUE;
        Data = RtlTimeZoneData;
        DataSize = RtlTimeZoneDataSize;
    }

    if (Data == NULL) {
        RtlDebugPrint("No time zone data set.\n");
        goto DebugPrintTimeZoneDataEnd;
    }

    Header = Data;
    Status = RtlpValidateTimeZoneData(Header, DataSize);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Time zone data failed validation.\n");
        goto DebugPrintTimeZoneDataEnd;
    }

    Rules = (PVOID)Header + Header->RuleOffset;
    Zones = (PVOID)Header + Header->ZoneOffset;
    Leaps = (PVOID)Header + Header->LeapOffset;
    if (Header->RuleCount != 0) {
        RtlDebugPrint("Rules:\n");
    }

    for (RuleIndex = 0; RuleIndex < Header->RuleCount; RuleIndex += 1) {
        RtlpPrintTimeZoneRule(Header, &(Rules[RuleIndex]));
    }

    RtlDebugPrint("\nZones:\n");
    for (ZoneIndex = 0; ZoneIndex < Header->ZoneCount; ZoneIndex += 1) {
        RtlpPrintTimeZone(Header, DataSize, &(Zones[ZoneIndex]));
    }

    RtlDebugPrint("\nLeap Seconds:\n");
    for (LeapIndex = 0; LeapIndex < Header->LeapCount; LeapIndex += 1) {
        RtlpPrintTimeZoneLeap(&(Leaps[LeapIndex]));
    }

DebugPrintTimeZoneDataEnd:
    if (LockHeld != FALSE) {
        RtlReleaseTimeZoneLock();
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PCSTR
RtlpTimeZoneGetString (
    PTIME_ZONE_HEADER Header,
    ULONG Offset
    )

/*++

Routine Description:

    This routine returns a time zone data string.

Arguments:

    Header - Supplies a pointer to the time zone data header.

    Offset - Supplies the string table offset.

Return Value:

    Returns a pointer to the string on success.

    NULL if the offset is out of bounds.

--*/

{

    if (Offset >= Header->StringsSize) {
        return NULL;
    }

    return (PCSTR)Header + Header->StringsOffset + Offset;
}

KSTATUS
RtlpValidateTimeZoneData (
    PTIME_ZONE_HEADER Header,
    ULONG DataSize
    )

/*++

Routine Description:

    This routine validates that the fields in the time zone data header are
    valid.

Arguments:

    Header - Supplies a pointer to the time zone data header.

    DataSize - Supplies the size of the time zone data buffer.

Return Value:

    STATUS_SUCCESS if the data is valid.

    STATUS_FILE_CORRUPT if the data is invalid.

--*/

{

    PSTR LastString;
    ULONG LeapEnd;
    ULONG RulesEnd;
    ULONG StringsEnd;
    ULONG ZoneEntriesEnd;
    ULONG ZonesEnd;

    if (DataSize < sizeof(TIME_ZONE_HEADER)) {
        return STATUS_FILE_CORRUPT;
    }

    //
    // Compute the end offsets for every array.
    //

    RulesEnd = Header->RuleOffset +
               (Header->RuleCount * sizeof(TIME_ZONE_RULE));

    ZonesEnd = Header->ZoneOffset + (Header->ZoneCount * sizeof(TIME_ZONE));
    ZoneEntriesEnd = Header->ZoneEntryOffset +
                     (Header->ZoneEntryCount * sizeof(TIME_ZONE_ENTRY));

    LeapEnd = Header->LeapOffset +
              (Header->LeapCount * sizeof(TIME_ZONE_LEAP_SECOND));

    StringsEnd = Header->StringsOffset + Header->StringsSize;
    LastString = (PSTR)Header + StringsEnd - 1;

    //
    // Ensure that the start offsets are within range, the end offsets are
    // within range, and the end offsets do not overflow. Also ensure that
    // the end of the string table is null terminated.
    //

    if ((Header->RuleOffset >= DataSize) ||
        (RulesEnd > DataSize) ||
        (RulesEnd < Header->RuleOffset) ||
        (Header->ZoneOffset >= DataSize) ||
        (ZonesEnd > DataSize) ||
        (ZonesEnd < Header->ZoneOffset) ||
        (Header->ZoneEntryOffset >= DataSize) ||
        (ZoneEntriesEnd > DataSize) ||
        (ZoneEntriesEnd < Header->ZoneEntryOffset) ||
        (Header->LeapOffset >= DataSize) ||
        (LeapEnd > DataSize) ||
        (LeapEnd < Header->LeapOffset) ||
        (Header->StringsOffset >= DataSize) ||
        (StringsEnd > DataSize) ||
        (StringsEnd < Header->StringsOffset) ||
        (*LastString != '\0')) {

        return STATUS_FILE_CORRUPT;
    }

    return STATUS_SUCCESS;
}

KSTATUS
RtlpTimeZoneAddString (
    PSTR StringsBase,
    ULONG StringsSize,
    PULONG CurrentStringsSize,
    PCSTR String,
    PULONG Offset
    )

/*++

Routine Description:

    This routine adds a string to a preallocated and in-progress string table.

Arguments:

    StringsBase - Supplies a pointer to the base of the string table.

    StringsSize - Supplies the total size of the string table.

    CurrentStringsSize - Supplies a pointer to the current size of the string
        table. This value will be updated if the string is not a duplicate and
        must be inserted.

    String - Supplies a pointer to the string to add.

    Offset - Supplies a pointer where the offset of the string within the table
        will be returned.

Return Value:

    STATUS_SUCCESS if the data is valid.

    STATUS_DATA_LENGTH_MISMATCH if the string is too long to be added to the
    table.

--*/

{

    PSTR CurrentString;
    ULONG CurrentStringLength;
    ULONG Index;
    ULONG Length;

    Length = RtlStringLength(String) + 1;

    ASSERT(*CurrentStringsSize <= StringsSize);

    //
    // Search for the string in the table, it may be there already.
    //

    CurrentString = StringsBase;
    Index = 0;
    while (Index < *CurrentStringsSize) {
        if (RtlAreStringsEqual(CurrentString, String, Length) != FALSE) {
            *Offset = Index;
            return STATUS_SUCCESS;
        }

        CurrentStringLength = RtlStringLength(CurrentString) + 1;
        Index += CurrentStringLength;
        CurrentString += CurrentStringLength;
    }

    ASSERT(Index == *CurrentStringsSize);

    if (Index + Length > StringsSize) {
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    //
    // The string needs to be added to the end of the table here.
    //

    RtlStringCopy(CurrentString, String, Length);
    *Offset = Index;
    *CurrentStringsSize += Length;
    return STATUS_SUCCESS;
}

VOID
RtlpPrintTimeZoneRule (
    PTIME_ZONE_HEADER Header,
    PTIME_ZONE_RULE Rule
    )

/*++

Routine Description:

    This routine prints a time zone rule.

Arguments:

    Header - Supplies a pointer to the time zone data header.

    Rule - Supplies a pointer to the rule to print.

Return Value:

    None.

--*/

{

    INT Month;
    PCSTR String;
    INT Weekday;

    if ((Rule->Number == 0) && (Rule->On.Type == TimeZoneOccasionInvalid)) {
        return;
    }

    Month = Rule->Month;
    RtlDebugPrint("    %-3d %04d-%04d %-9s ",
                  Rule->Number,
                  Rule->From,
                  Rule->To,
                  RtlMonthStrings[Month]);

    Weekday = Rule->On.Weekday;
    switch (Rule->On.Type) {
    case TimeZoneOccasionMonthDate:
        RtlDebugPrint("%-7d ", Rule->On.MonthDay);
        break;

    case TimeZoneOccasionLastWeekday:
        RtlDebugPrint("Last%s ", RtlAbbreviatedWeekdayStrings[Weekday]);
        break;

    case TimeZoneOccasionGreaterOrEqualWeekday:
        RtlDebugPrint("%s>=%-2d ",
                      RtlAbbreviatedWeekdayStrings[Weekday],
                      Rule->On.MonthDay);

        break;

    case TimeZoneOccasionLessOrEqualWeekday:
        RtlDebugPrint("%s<=%-2d ",
                      RtlAbbreviatedWeekdayStrings[Weekday],
                      Rule->On.MonthDay);

        break;

    default:

        ASSERT(FALSE);

        break;
    }

    RtlpPrintTimeZoneTime(Rule->At, Rule->AtLens);
    RtlDebugPrint(" ");
    RtlpPrintTimeZoneTime(Rule->Save, TimeZoneLensLocalTime);
    String = RtlpTimeZoneGetString(Header, Rule->Letters);
    RtlDebugPrint(" %s\n", String);
    return;
}

VOID
RtlpPrintTimeZone (
    PTIME_ZONE_HEADER Header,
    ULONG DataSize,
    PTIME_ZONE Zone
    )

/*++

Routine Description:

    This routine prints a time zone and any associated time zone entries.

Arguments:

    Header - Supplies a pointer to the data header.

    DataSize - Supplies the size of the data buffer in bytes.

    Zone - Supplies a pointer to the zone to print.

Return Value:

    None.

--*/

{

    ULONG EntryIndex;
    PTIME_ZONE_ENTRY ZoneEntries;
    PTIME_ZONE_ENTRY ZoneEntry;
    PCSTR ZoneName;

    ZoneName = RtlpTimeZoneGetString(Header, Zone->Name);
    RtlDebugPrint("    %s\n", ZoneName);
    ZoneEntries = (PVOID)Header + Header->ZoneEntryOffset;
    ZoneEntries += Zone->EntryIndex;
    for (EntryIndex = 0; EntryIndex < Zone->EntryCount; EntryIndex += 1) {
        ZoneEntry = &(ZoneEntries[EntryIndex]);
        RtlDebugPrint("        ");
        RtlpPrintTimeZoneEntry(Header, ZoneEntry);
    }

    RtlDebugPrint("\n");
    return;
}

VOID
RtlpPrintTimeZoneLeap (
    PTIME_ZONE_LEAP_SECOND Leap
    )

/*++

Routine Description:

    This routine prints a time zone leap second.

Arguments:

    Leap - Supplies a pointer to the leap second to print.

Return Value:

    None.

--*/

{

    CHAR Correction;
    CHAR RollingOrStationary;

    RtlDebugPrint("    ");
    RtlpPrintTimeZoneDate(Leap->Date);
    Correction = '-';
    if (Leap->Positive != FALSE) {
        Correction = '+';
    }

    RollingOrStationary = 'S';
    if (Leap->LocalTime != FALSE) {
        RollingOrStationary = 'R';
    }

    RtlDebugPrint(" %c %c\n", Correction, RollingOrStationary);
    return;
}

VOID
RtlpPrintTimeZoneEntry (
    PTIME_ZONE_HEADER Header,
    PTIME_ZONE_ENTRY ZoneEntry
    )

/*++

Routine Description:

    This routine prints a time zone entry.

Arguments:

    Header - Supplies a pointer to the time zone data header.

    ZoneEntry - Supplies a pointer to the zone entry to print.

Return Value:

    None.

--*/

{

    PCSTR String;

    RtlpPrintTimeZoneTime(ZoneEntry->GmtOffset, TimeZoneLensLocalTime);
    RtlDebugPrint(" ");
    if (ZoneEntry->Rules != -1) {
        RtlDebugPrint("%-10d ", ZoneEntry->Rules);

    } else {
        RtlpPrintTimeZoneTime(ZoneEntry->Save, TimeZoneLensLocalTime);
        RtlDebugPrint(" ");
    }

    String = RtlpTimeZoneGetString(Header, ZoneEntry->Format);
    RtlDebugPrint("%-7s ", String);
    if (ZoneEntry->Until < MAX_TIME_ZONE_DATE) {
        RtlpPrintTimeZoneDate(ZoneEntry->Until);
    }

    RtlDebugPrint("\n");
    return;
}

VOID
RtlpPrintTimeZoneDate (
    LONGLONG Date
    )

/*++

Routine Description:

    This routine prints a time zone date.

Arguments:

    Date - Supplies the date in seconds since the epoch.

Return Value:

    None.

--*/

{

    LONG Day;
    LONG Days;
    LONG Leap;
    LONG Month;
    LONG Year;

    //
    // Figure out and subtract off the year. Make the remainder positive (so
    // that something like -1 becomes December 31, 2000.
    //

    Days = Date / SECONDS_PER_DAY;
    Date -= (LONGLONG)Days * SECONDS_PER_DAY;
    if (Date < 0) {
        Date += SECONDS_PER_DAY;
        Days -= 1;
    }

    Year = RtlpComputeYearForDays(&Days);
    Leap = 0;
    if (IS_LEAP_YEAR(Year)) {
        Leap = 1;
    }

    //
    // Subtract off the months.
    //

    Month = 0;
    Day = Days;
    while (Day >= RtlDaysPerMonth[Leap][Month]) {
        Day -= RtlDaysPerMonth[Leap][Month];
        Month += 1;

        ASSERT(Month < TimeZoneMonthCount);

    }

    //
    // Days of the month start with 1.
    //

    Day += 1;

    ASSERT(Date < SECONDS_PER_DAY);

    RtlDebugPrint("%04d", Year);
    if ((Month != TimeZoneMonthJanuary) || (Day != 1) || (Date != 0)) {
        RtlDebugPrint(" %s %2d ",
                      RtlAbbreviatedMonthStrings[Month],
                      Day);

        RtlpPrintTimeZoneTime((LONG)Date, TimeZoneLensLocalTime);

    } else {
        RtlDebugPrint("%8s", "");
    }

    return;
}

VOID
RtlpPrintTimeZoneTime (
    LONG Time,
    TIME_ZONE_LENS Lens
    )

/*++

Routine Description:

    This routine prints a time zone time.

Arguments:

    Time - Supplies the time to print (in seconds).

    Lens - Supplies a lens to print as well.

Return Value:

    None.

--*/

{

    LONG Hours;
    LONG Length;
    CHAR LensCharacter;
    LONG Minutes;
    BOOL Negative;
    LONG Seconds;

    Length = 0;
    Negative = FALSE;
    if (Time < 0) {
        Negative = TRUE;
        Time = -Time;
    }

    Hours = Time / SECONDS_PER_HOUR;
    Time -= Hours * SECONDS_PER_HOUR;
    Minutes = Time / SECONDS_PER_MINUTE;
    Time -= Minutes * SECONDS_PER_MINUTE;
    Seconds = Time;
    if (Negative != FALSE) {
        RtlDebugPrint("-");
        Length += 1;
    }

    RtlDebugPrint("%d:%02d", Hours, Minutes);
    Length += 4;
    if (Hours >= 10) {
        Length += 1;
    }

    if (Seconds != 0) {
        RtlDebugPrint(":%02d", Seconds);
        Length += 3;
    }

    LensCharacter = ' ';
    switch (Lens) {
    case TimeZoneLensLocalTime:
        break;

    case TimeZoneLensLocalStandardTime:
        LensCharacter = 's';
        break;

    case TimeZoneLensUtc:
        LensCharacter = 'u';
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    RtlDebugPrint("%-*c", 10 - Length, LensCharacter);
    return;
}

KSTATUS
RtlpSelectTimeZone (
    PCSTR ZoneName
    )

/*++

Routine Description:

    This routine selects a time zone from the current set of data. This routine
    assumes the global lock is already held.

Arguments:

    ZoneName - Supplies a pointer to a null terminated string containing the
        name of the time zone.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if a time zone with the given name could not be found.

    STATUS_NO_DATA_AVAILABLE if no time zone data has been set yet.

    STATUS_INVALID_PARAMETER if no zone name was supplied.

--*/

{

    PCSTR CurrentZoneName;
    PTIME_ZONE_HEADER Header;
    ULONG Length;
    BOOL Match;
    KSTATUS Status;
    PTIME_ZONE Zone;
    ULONG ZoneIndex;
    PTIME_ZONE Zones;

    Header = RtlTimeZoneData;
    if (Header == NULL) {
        Status = STATUS_NO_DATA_AVAILABLE;
        goto SelectTimeZoneEnd;
    }

    if (ZoneName == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto SelectTimeZoneEnd;
    }

    Length = RtlStringLength(ZoneName) + 1;
    Zones = (PVOID)Header + Header->ZoneOffset;

    //
    // Loop through all the zones searching for the name.
    //

    for (ZoneIndex = 0; ZoneIndex < Header->ZoneCount; ZoneIndex += 1) {
        Zone = &(Zones[ZoneIndex]);
        CurrentZoneName = RtlpTimeZoneGetString(Header, Zone->Name);
        if (CurrentZoneName == NULL) {
            Status = STATUS_FILE_CORRUPT;
            goto SelectTimeZoneEnd;
        }

        Match = RtlAreStringsEqualIgnoringCase(ZoneName,
                                               CurrentZoneName,
                                               Length);

        if (Match != FALSE) {
            RtlTimeZoneIndex = ZoneIndex;
            break;
        }
    }

    if (ZoneIndex == Header->ZoneCount) {
        Status = STATUS_NOT_FOUND;
        goto SelectTimeZoneEnd;
    }

    RtlpSetTimeZoneNames();
    Status = STATUS_SUCCESS;

SelectTimeZoneEnd:
    return Status;
}

KSTATUS
RtlpGetCurrentTimeZone (
    PSTR Buffer,
    PULONG BufferSize
    )

/*++

Routine Description:

    This routine returns the name of the currently selected time zone. This
    routine assumes the global lock is already held.

Arguments:

    Buffer - Supplies an pointer where the name of the time zone will be
        returned on success.

    BufferSize - Supplies a pointer that on input contains the size of the
        buffer in bytes. On output, this value will contain the size of the
        buffer needed to contain the name of the current time zone (even if no
        buffer was provided).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_DATA_AVAILABLE if no time zone data has been set yet.

    STATUS_BUFFER_TOO_SMALL if the buffer provided to get the original time
        zone name was too small. If this is the case, the new time zone will
        not have been set.

--*/

{

    PTIME_ZONE_HEADER Header;
    ULONG Length;
    PCSTR Name;
    PTIME_ZONE Zone;

    Header = RtlTimeZoneData;
    if (Header == NULL) {
        return STATUS_NO_DATA_AVAILABLE;
    }

    Zone = (PVOID)Header + Header->ZoneOffset;

    ASSERT(RtlTimeZoneIndex < Header->ZoneCount);

    Zone += RtlTimeZoneIndex;
    Name = RtlpTimeZoneGetString(Header, Zone->Name);
    if (Name == NULL) {
        Name = "";
    }

    Length = RtlStringLength(Name) + 1;
    if (Buffer != NULL) {
        if ((*BufferSize != 0) && (*BufferSize < Length)) {
            *BufferSize = Length;
            return STATUS_BUFFER_TOO_SMALL;
        }

        RtlCopyMemory(Buffer, Name, Length);
    }

    *BufferSize = Length;
    return STATUS_SUCCESS;
}

VOID
RtlpFindTimeZoneRules (
    PTIME_ZONE_HEADER Header,
    PTIME_ZONE_ENTRY StartingEntry,
    LONG CurrentEntry,
    LONG Year,
    LONG Month,
    PTIME_ZONE_RULE RecentRules[2]
    )

/*++

Routine Description:

    This routine determines the two most recent rules that might apply for
    the given year and month. This routine assumes the global time zone lock
    is already held.

Arguments:

    Header - Supplies a pointer to the time zone data header.

    StartingEntry - Supplies a pointer to the first time zone entry for the
        zone.

    CurrentEntry - Supplies the index from the starting entry where the
        current applicable entry resides.

    Year - Supplies the year to get the rules for.

    Month - Supplies the month to get the rules for.

    RecentRules - Supplies a pointer where the two most recent rules will be
        returned.

Return Value:

    None.

--*/

{

    LONG EntryIndex;
    BOOL FoundInRoundZero;
    PTIME_ZONE_RULE LastRuleLastYear;
    LONGLONG LastYearSeconds;
    LONG OriginalYear;
    PTIME_ZONE_RULE Rule;
    ULONG RuleIndex;
    PTIME_ZONE_RULE Rules;
    PTIME_ZONE_ENTRY ZoneEntries;

    FoundInRoundZero = FALSE;
    LastRuleLastYear = NULL;
    LastYearSeconds = 0;
    OriginalYear = Year;
    RecentRules[0] = NULL;
    RecentRules[1] = NULL;
    Rules = (PVOID)Header + Header->RuleOffset;
    EntryIndex = CurrentEntry;
    ZoneEntries = StartingEntry;

    //
    // Loop until all the current rules slots are filled.
    //

    while (EntryIndex >= 0) {
        if (ZoneEntries[EntryIndex].Rules == -1) {
            break;
        }

        //
        // Loop through the rules looking for applicable ones.
        //

        for (RuleIndex = 0; RuleIndex < Header->RuleCount; RuleIndex += 1) {
            Rule = Rules + RuleIndex;
            if (Rule->Number != ZoneEntries[EntryIndex].Rules) {
                continue;
            }

            //
            // Keep track of the final rule in the previous year, assuming this
            // same set of rules applies (which if it doesn't will get corrected
            // later).
            //

            if ((Year - 1 >= Rule->From) &&
                (Year - 1 <= Rule->To) &&
                ((LastRuleLastYear == NULL) ||
                 (Rule->Month > LastRuleLastYear->Month))) {

                LastRuleLastYear = Rule;
            }

            //
            // If the rule starts later than the current year or ends before the
            // current year then it doesn't apply.
            //

            if ((Rule->From > Year) || (Rule->To < Year)) {
                continue;
            }

            //
            // This rule is in the current year. Skip it if it hasn't started
            // yet.
            //

            if (Rule->Month > Month) {
                continue;
            }

            //
            // Place this rule in the recent list. Anything found in round
            // zero trumps anything found in a subsequent round (more in the
            // past).
            //

            if ((RecentRules[0] == NULL) ||
                (((Year == OriginalYear) || (FoundInRoundZero == FALSE)) &&
                 (RecentRules[0]->Month < Rule->Month))) {

                RecentRules[1] = RecentRules[0];
                RecentRules[0] = Rule;
                if (Year == OriginalYear) {
                    FoundInRoundZero = TRUE;
                }

            } else if ((RecentRules[1] == NULL) ||
                       (RecentRules[1]->Month < Rule->Month)) {

                RecentRules[1] = Rule;
            }
        }

        //
        // Break out if there's a first rule that clearly already applies.
        // No need for rule one if this is the case.
        //

        if ((RecentRules[0] != NULL) &&
            ((RecentRules[0]->Month < Month) || (FoundInRoundZero == FALSE))) {

            RecentRules[1] = NULL;
            break;
        }

        //
        // If both recent rules were found, break out.
        //

        if (RecentRules[1] != NULL) {
            break;
        }

        //
        // Set the time back to the very end of last year and then find the
        // applicable rules (which may mean going back an entry).
        //

        if (Year == OriginalYear) {
            LastYearSeconds = ((LONGLONG)RtlpComputeDaysForYear(Year) *
                               SECONDS_PER_DAY) - 1;

            Year = OriginalYear - 1;
            Month = MONTHS_PER_YEAR - 1;
        }

        //
        // If last year was still in this zone entry, then use the last entry
        // in this set of rules.
        //

        if ((EntryIndex == 0) ||
            (LastYearSeconds > ZoneEntries[EntryIndex - 1].Until)) {

            if (RecentRules[0] == NULL) {
                RecentRules[0] = LastRuleLastYear;

            } else if (RecentRules[1] == NULL) {

                //
                // It doesn't make sense to have only one rule apply in a year,
                // as rules are supposed to turn daylight saving time on and
                // back off.
                //

                ASSERT(RecentRules[0] != LastRuleLastYear);

                RecentRules[1] = LastRuleLastYear;
            }

            break;
        }

        //
        // Move to the previous zone entry, with a date set to the very end
        // of last year.
        //

        EntryIndex -= 1;
    }

    return;
}

VOID
RtlpSetTimeZoneNames (
    VOID
    )

/*++

Routine Description:

    This routine sets up the time zone name strings for the current zone data.
    This routine assumes the global time zone lock is already held.

Arguments:

    None.

Return Value:

    None.

--*/

{

    CHAR Buffer[TIME_ZONE_NAME_MAX];
    PTIME_ZONE_RULE DaylightRule;
    ULONG FirstLength;
    PCSTR Format;
    PTIME_ZONE_HEADER Header;
    ULONG Length;
    PTIME_ZONE_RULE Rule;
    ULONG RuleIndex;
    PTIME_ZONE_RULE Rules;
    PSTR Slash;
    PTIME_ZONE_RULE StandardRule;
    PTIME_ZONE Zone;
    PTIME_ZONE_ENTRY ZoneEntry;

    RtlStandardTimeZoneName = "";
    RtlDaylightTimeZoneName = "";
    Header = RtlTimeZoneData;
    Zone = (PVOID)Header + Header->ZoneOffset;
    if (RtlTimeZoneIndex >= Header->ZoneCount) {
        goto SetTimeZoneNamesEnd;
    }

    Zone += RtlTimeZoneIndex;
    if (Zone->EntryCount == 0) {
        goto SetTimeZoneNamesEnd;
    }

    ZoneEntry = (PVOID)Header + Header->ZoneEntryOffset;
    ZoneEntry += Zone->EntryIndex + Zone->EntryCount - 1;
    RtlStandardTimeZoneOffset = ZoneEntry->GmtOffset + ZoneEntry->Save;
    RtlDaylightTimeZoneOffset = RtlStandardTimeZoneOffset;
    Format = RtlpTimeZoneGetString(Header, ZoneEntry->Format);
    if (Format == NULL) {
        goto SetTimeZoneNamesEnd;
    }

    //
    // If there's a slash in the format, then the slash separates the standard
    // from the daylight times.
    //

    Length = RtlStringLength(Format) + 1;
    Slash = RtlStringFindCharacter(Format, '/', Length);
    if (Slash != NULL) {
        FirstLength = (UINTN)Slash - (UINTN)Format + 1;
        if (FirstLength > TIME_ZONE_NAME_MAX) {
            FirstLength = TIME_ZONE_NAME_MAX;
        }

        RtlStringCopy(Buffer, Format, FirstLength);
        RtlStandardTimeZoneName = RtlpTimeZoneCacheString(Buffer);
        Slash += 1;
        RtlDaylightTimeZoneName = RtlpTimeZoneCacheString(Slash);
        goto SetTimeZoneNamesEnd;
    }

    //
    // If there are no rules, then just copy the name in.
    //

    if (ZoneEntry->Rules == -1) {
        RtlStandardTimeZoneName = RtlpTimeZoneCacheString(Format);
        RtlDaylightTimeZoneName = RtlStandardTimeZoneName;
        goto SetTimeZoneNamesEnd;
    }

    //
    // Root through all the rules to find the standard and daylight letters.
    //

    DaylightRule = NULL;
    StandardRule = NULL;
    Rules = (PVOID)Header + Header->RuleOffset;
    for (RuleIndex = 0; RuleIndex < Header->RuleCount; RuleIndex += 1) {
        Rule = Rules + RuleIndex;
        if (Rule->Number != ZoneEntry->Rules) {
            continue;
        }

        if (Rule->Save == 0) {
            if ((StandardRule == NULL) || (StandardRule->To < Rule->To)) {
                StandardRule = Rule;
            }

        } else {
            if ((DaylightRule == NULL) || (DaylightRule->To < Rule->To)) {
                DaylightRule = Rule;
            }
        }
    }

    if (DaylightRule != NULL) {
        RtlDaylightTimeZoneOffset += DaylightRule->Save;
    }

    RtlpTimeZonePerformSubstitution(Buffer,
                                    TIME_ZONE_NAME_MAX,
                                    Format,
                                    StandardRule);

    Buffer[TIME_ZONE_NAME_MAX - 1] = '\0';
    RtlStandardTimeZoneName = RtlpTimeZoneCacheString(Buffer);
    RtlpTimeZonePerformSubstitution(Buffer,
                                    TIME_ZONE_NAME_MAX,
                                    Format,
                                    DaylightRule);

    Buffer[TIME_ZONE_NAME_MAX - 1] = '\0';
    RtlDaylightTimeZoneName = RtlpTimeZoneCacheString(Buffer);

SetTimeZoneNamesEnd:
    return;
}

VOID
RtlpTimeZonePerformSubstitution (
    PSTR Destination,
    ULONG DestinationSize,
    PCSTR Format,
    PTIME_ZONE_RULE Rule
    )

/*++

Routine Description:

    This routine writes the given time zone format and letters into the
    destination buffer. It's like a super limited version of printf. This
    routine assumes the time zone lock is already held and that the given rule
    is from the current time zone data.

Arguments:

    Destination - Supplies a pointer to the destination buffer.

    DestinationSize - Supplies the size of the destination buffer in bytes.

    Format - Supplies the format string.

    Rule - Supplies the substitution rule.

Return Value:

    None.

--*/

{

    ULONG LetterIndex;
    PCSTR Letters;
    ULONG RemainingSize;
    PSTR Result;

    if (DestinationSize == 0) {
        return;
    }

    Letters = NULL;
    if (Rule != NULL) {
        Letters = RtlpTimeZoneGetString(RtlTimeZoneData, Rule->Letters);
    }

    Result = Destination;
    RemainingSize = DestinationSize - 1;
    while (RemainingSize != 0) {

        //
        // If a %s is found, copy the letters string in.
        //

        if ((*Format == '%') && (*(Format + 1) == 's')) {
            if (Letters != NULL) {
                LetterIndex = 0;
                while (RemainingSize != 0) {
                    if (Letters[LetterIndex] == '\0') {
                        break;
                    }

                    *Result = Letters[LetterIndex];
                    LetterIndex += 1;
                    Result += 1;
                    RemainingSize -= 1;
                }
            }

            Format += 2;

        } else {
            *Result = *Format;
            if (*Format == '\0') {
                break;
            }

            Format += 1;
            Result += 1;
            RemainingSize -= 1;
        }
    }

    *Result = '\0';
    return;
}

PCSTR
RtlpTimeZoneCacheString (
    PCSTR String
    )

/*++

Routine Description:

    This routine returns a cached string for the given string. This routine
    assumes the time zone lock is already held.

Arguments:

    String - Supplies a pointer to the string to find the cached version of.

Return Value:

    Returns a pointer to the cached string on success.

    NULL on allocation failure.

--*/

{

    ULONG Index;
    BOOL Match;
    PSTR *NewBuffer;
    ULONG NewCapacity;
    PSTR NewString;
    UINTN StringSize;

    StringSize = RtlStringLength(String) + 1;

    //
    // There should never really be that many time zone names floating around
    // (probably less than 6 at any time), so use a simple linear search of an
    // array of strings.
    //

    for (Index = 0; Index < RtlTimeZoneNameCacheSize; Index += 1) {
        if (RtlTimeZoneNameCache[Index] == NULL) {
            break;
        }

        Match = RtlAreStringsEqual(RtlTimeZoneNameCache[Index],
                                   String,
                                   StringSize);

        if (Match != FALSE) {
            return RtlTimeZoneNameCache[Index];
        }
    }

    //
    // The string was not found. If the array is full, reallocate it.
    //

    if (Index == RtlTimeZoneNameCacheSize) {
        if (Index == 0) {
            NewCapacity = 8;

        } else {
            NewCapacity = Index * 2;
        }

        NewBuffer = RtlTimeZoneReallocate(RtlTimeZoneNameCache,
                                          NewCapacity * sizeof(PSTR));

        if (NewBuffer == NULL) {
            return NULL;
        }

        RtlTimeZoneNameCache = NewBuffer;
        RtlTimeZoneNameCacheSize = NewCapacity;
        RtlZeroMemory(NewBuffer + Index, (NewCapacity - Index) * sizeof(PSTR));
    }

    NewString = RtlTimeZoneReallocate(NULL, StringSize);
    if (NewString == NULL) {
        return NULL;
    }

    RtlCopyMemory(NewString, String, StringSize);
    RtlTimeZoneNameCache[Index] = NewString;
    return NewString;
}

