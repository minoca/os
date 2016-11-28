/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stubs.c

Abstract:

    This module implements stub functions called by various libraries included
    in the firmware.

Author:

    Evan Green 7-Aug-2013

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/kdebug.h>
#include <minoca/uefi/uefi.h>
#include "shortcut.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

ULONG KeActiveProcessorCount = 1;

//
// ------------------------------------------------------------------ Functions
//

VOID
RtlDebugPrint (
    PCSTR Format,
    ...
    )

/*++

Routine Description:

    This routine prints a printf-style string to the debugger.

Arguments:

    Format - Supplies the printf-style format string to print. The contents of
        this string determine the rest of the arguments passed.

    ... - Supplies any arguments needed to convert the Format string.

Return Value:

    None.

--*/

{

    va_list ArgumentList;
    CHAR Ascii[128];
    ULONG Index;
    USHORT Wide[128];

    //
    // Simply pass the data on to the debugger's print function.
    //

    va_start(ArgumentList, Format);
    KdPrintWithArgumentList(Format, ArgumentList);
    va_end(ArgumentList);
    if (EfiSystemTable->StdErr != NULL) {
        va_start(ArgumentList, Format);
        RtlFormatString(Ascii,
                        sizeof(Ascii) - 1,
                        CharacterEncodingAscii,
                        Format,
                        ArgumentList);

        Index = 0;
        while (Ascii[Index] != '\0') {
            Wide[Index] = Ascii[Index];
            Index += 1;
        }

        Wide[Index] = L'\0';
        va_end(ArgumentList);
        EfiSystemTable->StdErr->OutputString(EfiSystemTable->StdErr, Wide);
    }

    return;
}

VOID
RtlRaiseAssertion (
    PCSTR Expression,
    PCSTR SourceFile,
    ULONG SourceLine
    )

/*++

Routine Description:

    This routine raises an assertion failure exception. If a debugger is
    connected, it will attempt to connect to the debugger.

Arguments:

    Expression - Supplies the string containing the expression that failed.

    SourceFile - Supplies the string describing the source file of the failure.

    SourceLine - Supplies the source line number of the failure.

Return Value:

    None.

--*/

{

    RtlDebugPrint("\n\n *** Assertion Failure: %s\n *** File: %s, Line %d\n\n",
                  Expression,
                  SourceFile,
                  SourceLine);

    RtlDebugService(EXCEPTION_ASSERTION_FAILURE, NULL);
    return;
}

ULONG
MmValidateMemoryAccessForDebugger (
    PVOID Address,
    ULONG Length,
    PBOOL Writable
    )

/*++

Routine Description:

    This routine validates that access to a specified location in memory will
    not cause a page fault.

Arguments:

    Address - Supplies the virtual address of the memory that will be read or
        written.

    Length - Supplies how many bytes at that location the caller would like to
        read or write.

    Writable - Supplies an optional pointer that receives a boolean indicating
        whether or not the memory range is mapped writable.

Return Value:

    Returns the number of bytes from the beginning of the address that are
    accessible. If the memory is completely available, the return value will be
    equal to the Length parameter. If the memory is completely paged out, 0
    will be returned.

--*/

{

    if (Writable != NULL) {
        *Writable = TRUE;
    }

    return Length;
}

VOID
MmModifyAddressMappingForDebugger (
    PVOID Address,
    BOOL Writable,
    PBOOL WasWritable
    )

/*++

Routine Description:

    This routine modifies the mapping properties for the page that contains the
    given address.

Arguments:

    Address - Supplies the virtual address of the memory whose mapping
        properties are to be changed.

    Writable - Supplies a boolean indicating whether or not to make the page
        containing the address writable (TRUE) or read-only (FALSE).

    WasWritable - Supplies a pointer that receives a boolean indicating whether
        or not the page was writable (TRUE) or read-only (FALSE) before any
        modifications.

Return Value:

    None.

--*/

{

    *WasWritable = TRUE;
    return;
}

PPROCESSOR_BLOCK
KeGetCurrentProcessorBlockForDebugger (
    VOID
    )

/*++

Routine Description:

    This routine gets the processor block for the currently executing
    processor. It is intended to be called only by the debugger.

Arguments:

    None.

Return Value:

    Returns the current processor block.

--*/

{

    return NULL;
}

VOID
KeCrashSystemEx (
    ULONG CrashCode,
    PCSTR CrashCodeString,
    ULONGLONG Parameter1,
    ULONGLONG Parameter2,
    ULONGLONG Parameter3,
    ULONGLONG Parameter4
    )

/*++

Routine Description:

    This routine officially takes the system down after a fatal system error
    has occurred. This function does not return.

Arguments:

    CrashCode - Supplies the reason for the system crash.

    CrashCodeString - Supplies the string corresponding to the given crash
        code. This parameter is generated by the macro, and should not be
        filled in directly.

    Parameter1 - Supplies an optional parameter regarding the crash.

    Parameter2 - Supplies an optional parameter regarding the crash.

    Parameter3 - Supplies an optional parameter regarding the crash.

    Parameter4 - Supplies an optional parameter regarding the crash.

Return Value:

    None. This function does not return.

--*/

{

    RtlDebugPrint("\n\n *** Fatal System Error ***\n\n"
                  "Error Code: %s (0x%x)\n"
                  "Parameter1: 0x%08I64x\n"
                  "Parameter2: 0x%08I64x\n"
                  "Parameter3: 0x%08I64x\n"
                  "Parameter4: 0x%08I64x\n\n",
                  CrashCodeString,
                  CrashCode,
                  Parameter1,
                  Parameter2,
                  Parameter3,
                  Parameter4);

    //
    // Spin forever.
    //

    while (TRUE) {
        RtlDebugBreak();
    }
}

KSTATUS
HlSendIpi (
    IPI_TYPE IpiType,
    PPROCESSOR_SET Processors
    )

/*++

Routine Description:

    This routine sends an Inter-Processor Interrupt (IPI) to the given set of
    processors.

Arguments:

    IpiType - Supplies the type of IPI to deliver.

    Processors - Supplies the set of processors to deliver the IPI to.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
HlResetSystem (
    SYSTEM_RESET_TYPE ResetType
    )

/*++

Routine Description:

    This routine resets the system.

Arguments:

    ResetType - Supplies the desired reset type. If the desired reset type is
        not supported, a cold reset will be attempted.

Return Value:

    Does not return on success, the system is reset.

    STATUS_INVALID_PARAMETER if an invalid reset type was supplied.

    STATUS_NOT_SUPPORTED if the system cannot be reset.

    STATUS_UNSUCCESSFUL if the system did not reset.

--*/

{

    EFI_RESET_TYPE EfiResetType;

    switch (ResetType) {
    case SystemResetShutdown:
        EfiResetType = EfiResetShutdown;
        break;

    case SystemResetCold:
        EfiResetType = EfiResetCold;
        break;

    case SystemResetWarm:
    default:
        EfiResetType = EfiResetWarm;
        break;
    }

    if ((EfiRuntimeServices != NULL) &&
        (EfiRuntimeServices->ResetSystem != NULL)) {

        EfiResetSystem(EfiResetType, 0, 0, NULL);
    }

    return STATUS_UNSUCCESSFUL;
}

ULONGLONG
HlQueryTimeCounter (
    VOID
    )

/*++

Routine Description:

    This routine queries the time counter hardware and returns a 64-bit
    monotonically non-decreasing value that represents the number of timer ticks
    since the system was started. This value will continue to count through all
    idle and sleep states.

    This routine can be called at any runlevel.

Arguments:

    None.

Return Value:

    Returns the number of timer ticks that have elapsed since the system was
    booted. The absolute time between successive ticks can be retrieved from the
    Query Time Counter Frequency function.

--*/

{

    return 0;
}

ULONGLONG
HlQueryTimeCounterFrequency (
    VOID
    )

/*++

Routine Description:

    This routine returns the frequency of the time counter. This frequency will
    never change after it is set on boot.

    This routine can be called at any runlevel.

Arguments:

    None.

Return Value:

    Returns the frequency of the time counter, in Hertz.

--*/

{

    ASSERT(FALSE);

    return 1;
}

VOID
HlBusySpin (
    ULONG Microseconds
    )

/*++

Routine Description:

    This routine spins for at least the given number of microseconds by
    repeatedly reading a hardware timer. This routine should be avoided if at
    all possible, as it simply burns CPU cycles.

    This routine can be called at any runlevel.

Arguments:

    Microseconds - Supplies the number of microseconds to spin for.

Return Value:

    Returns the frequency of the time counter, in Hertz.

--*/

{

    if ((EfiBootServices != NULL) && (EfiBootServices->Stall != NULL)) {
        EfiStall(Microseconds);
    }

    return;
}

KSTATUS
SpGetProfilerData (
    PPROFILER_NOTIFICATION ProfilerNotification,
    PULONG Flags
    )

/*++

Routine Description:

    This routine fills the provided profiler notification with profiling data.
    A profiler consumer should call this routine to obtain data to send over
    the wire. It is assumed here that consumers will serialize consumption.

Arguments:

    ProfilerNotification - Supplies a pointer to the profiler notification that
        is to be filled in with profiling data.

    Flags - Supplies a pointer to the types of profiling data the caller wants
        to collect. Upon return, the flags for the returned data will be
        returned.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

ULONG
SpGetProfilerDataStatus (
    VOID
    )

/*++

Routine Description:

    This routine determines if there is profiling data for the current
    processor that needs to be sent to a consumer.

Arguments:

    None.

Return Value:

    Returns a set of flags representing which types of profiling data are
    available. Returns zero if nothing is available.

--*/

{

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

