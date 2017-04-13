/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    info.c

Abstract:

    This module implements support for the get and set system information
    calls.

Author:

    Evan Green 10-Apr-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/fw/smbios.h>
#include "kep.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
KepGetSetSystemInformation (
    BOOL FromKernelMode,
    SYSTEM_INFORMATION_SUBSYSTEM Subsystem,
    UINTN InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
KepGetSetKeSystemInformation (
    BOOL FromKernelMode,
    KE_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
KepGetSystemVersion (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
KepGetFirmwareTable (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
KepGetFirmwareType (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
KepGetProcessorUsage (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
KepGetProcessorCount (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
KepGetKernelCommandLine (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

//
// -------------------------------------------------------------------- Globals
//

SYSTEM_FIRMWARE_TYPE KeSystemFirmwareType = SystemFirmwareUnknown;
PKERNEL_COMMAND_LINE KeCommandLine = NULL;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
KeGetSetSystemInformation (
    SYSTEM_INFORMATION_SUBSYSTEM Subsystem,
    UINTN InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets system information.

Arguments:

    Subsystem - Supplies the subsystem to query or set information of.

    InformationType - Supplies the information type, which is specific to
        the subsystem. The type of this value is generally
        <subsystem>_INFORMATION_TYPE (eg IO_INFORMATION_TYPE).

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    STATUS_SUCCESS if the information was successfully queried or set.

    STATUS_BUFFER_TOO_SMALL if the buffer size specified was too small. The
    required buffer size will be returned in the data size parameter.

    STATUS_DATA_LENGTH_MISMATCH if the buffer size was not correct. The
    correct buffer size will be returned in the data size parameter.

    STATUS_INVALID_PARAMETER if the given subsystem or information type is
    not known.

    Other status codes on other failures.

--*/

{

    KSTATUS Status;

    Status = KepGetSetSystemInformation(TRUE,
                                        Subsystem,
                                        InformationType,
                                        Data,
                                        DataSize,
                                        Set);

    return Status;
}

KERNEL_API
PKERNEL_ARGUMENT
KeGetKernelArgument (
    PKERNEL_ARGUMENT Start,
    PCSTR Component,
    PCSTR Name
    )

/*++

Routine Description:

    This routine looks up a kernel command line argument.

Arguments:

    Start - Supplies an optional pointer to the previous command line argument
        to start from. Supply NULL here initially.

    Component - Supplies a pointer to the component string to look up.

    Name - Supplies a pointer to the argument name to look up.

Return Value:

    Returns a pointer to a matching kernel argument on success.

    NULL if no argument could be found.

--*/

{

    PKERNEL_ARGUMENT Argument;
    UINTN ArgumentIndex;
    PKERNEL_COMMAND_LINE Line;
    BOOL Match;

    Line = KeCommandLine;
    if (Line == NULL) {
        return NULL;
    }

    ArgumentIndex = 0;
    Argument = &(Line->Arguments[ArgumentIndex]);
    if (Start != NULL) {
        while ((ArgumentIndex < Line->ArgumentCount) &&
               (Argument != Start)) {

            Argument += 1;
            ArgumentIndex += 1;
        }

        //
        // If the argument was never found or is the last argument, nothing
        // new will be found.
        //

        if (ArgumentIndex >= Line->ArgumentCount - 1) {
            return NULL;
        }

        Argument += 1;
        ArgumentIndex += 1;
    }

    while (ArgumentIndex < Line->ArgumentCount) {
        Match = RtlAreStringsEqual(Component,
                                   Argument->Component,
                                   KERNEL_MAX_COMMAND_LINE);

        if (Match != FALSE) {
            Match = RtlAreStringsEqual(Name,
                                       Argument->Name,
                                       KERNEL_MAX_COMMAND_LINE);

            if (Match != FALSE) {
                return Argument;
            }
        }

        ArgumentIndex += 1;
        Argument += 1;
    }

    return NULL;
}

INTN
KeSysGetSetSystemInformation (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the user mode system call for getting and setting
    system information.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PVOID Buffer;
    UINTN CopySize;
    KSTATUS CopyStatus;
    PSYSTEM_CALL_GET_SET_SYSTEM_INFORMATION Request;
    KSTATUS Status;

    Buffer = NULL;
    Request = SystemCallParameter;

    //
    // Create a paged pool buffer to hold the data.
    //

    CopySize = 0;
    if (Request->DataSize != 0) {
        Buffer = MmAllocatePagedPool(Request->DataSize,
                                     KE_INFORMATION_ALLOCATION_TAG);

        if (Buffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SysGetSetSystemInformationEnd;
        }

        CopySize = Request->DataSize;

        //
        // Copy the data into the kernel mode buffer.
        //

        Status = MmCopyFromUserMode(Buffer,
                                    Request->Data,
                                    Request->DataSize);

        if (!KSUCCESS(Status)) {
            goto SysGetSetSystemInformationEnd;
        }
    }

    Status = KepGetSetSystemInformation(FALSE,
                                        Request->Subsystem,
                                        Request->InformationType,
                                        Buffer,
                                        &(Request->DataSize),
                                        Request->Set);

    //
    // Copy the data back into user mode, even on set operations.
    //

    if (CopySize > Request->DataSize) {
        CopySize = Request->DataSize;
    }

    if (CopySize != 0) {
        CopyStatus = MmCopyToUserMode(Request->Data, Buffer, CopySize);
        if ((KSUCCESS(Status)) && (!KSUCCESS(CopyStatus))) {
            Status = CopyStatus;
        }
    }

SysGetSetSystemInformationEnd:
    if (Buffer != NULL) {
        MmFreePagedPool(Buffer);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
KepGetSetSystemInformation (
    BOOL FromKernelMode,
    SYSTEM_INFORMATION_SUBSYSTEM Subsystem,
    UINTN InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets system information.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    Subsystem - Supplies the subsystem to query or set information of.

    InformationType - Supplies the information type, which is specific to
        the subsystem. The type of this value is generally
        <subsystem>_INFORMATION_TYPE (eg IO_INFORMATION_TYPE).

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    STATUS_SUCCESS if the information was successfully queried or set.

    STATUS_BUFFER_TOO_SMALL if the buffer size specified was too small. The
    required buffer size will be returned in the data size parameter.

    STATUS_DATA_LENGTH_MISMATCH if the buffer size was not correct. The
    correct buffer size will be returned in the data size parameter.

    STATUS_INVALID_PARAMETER if the given subsystem or information type is
    not known.

    Other status codes on other failures.

--*/

{

    KSTATUS Status;

    switch (Subsystem) {
    case SystemInformationKe:
        Status = KepGetSetKeSystemInformation(FromKernelMode,
                                              InformationType,
                                              Data,
                                              DataSize,
                                              Set);

        break;

    case SystemInformationIo:
        Status = IoGetSetSystemInformation(FromKernelMode,
                                           InformationType,
                                           Data,
                                           DataSize,
                                           Set);

        break;

    case SystemInformationMm:
        Status = MmGetSetSystemInformation(FromKernelMode,
                                           InformationType,
                                           Data,
                                           DataSize,
                                           Set);

        break;

    case SystemInformationPs:
        Status = PsGetSetSystemInformation(FromKernelMode,
                                           InformationType,
                                           Data,
                                           DataSize,
                                           Set);

        break;

    case SystemInformationHl:
        Status = HlGetSetSystemInformation(FromKernelMode,
                                           InformationType,
                                           Data,
                                           DataSize,
                                           Set);

        break;

    case SystemInformationSp:
        Status = SpGetSetSystemInformation(FromKernelMode,
                                           InformationType,
                                           Data,
                                           DataSize,
                                           Set);

        break;

    case SystemInformationPm:
        Status = PmGetSetSystemInformation(FromKernelMode,
                                           InformationType,
                                           Data,
                                           DataSize,
                                           Set);

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        *DataSize = 0;
        break;
    }

    return Status;
}

KSTATUS
KepGetSetKeSystemInformation (
    BOOL FromKernelMode,
    KE_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets system information.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    InformationType - Supplies the information type.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    switch (InformationType) {
    case KeInformationSystemVersion:
        Status = KepGetSystemVersion(Data, DataSize, Set);
        break;

    case KeInformationFirmwareTable:
        Status = KepGetFirmwareTable(Data, DataSize, Set);
        break;

    case KeInformationFirmwareType:
        Status = KepGetFirmwareType(Data, DataSize, Set);
        break;

    case KeInformationProcessorUsage:
        Status = KepGetProcessorUsage(Data, DataSize, Set);
        break;

    case KeInformationProcessorCount:
        Status = KepGetProcessorCount(Data, DataSize, Set);
        break;

    case KeInformationKernelCommandLine:
        Status = KepGetKernelCommandLine(Data, DataSize, Set);
        break;

    case KeInformationBannerThread:
        Status = KepSetBannerThread(Data, DataSize, Set);
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        *DataSize = 0;
        break;
    }

    return Status;
}

KSTATUS
KepGetSystemVersion (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets OS version information.

Arguments:

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    ULONG BufferSize;
    KSTATUS Status;
    SYSTEM_VERSION_INFORMATION VersionInformation;
    PSYSTEM_VERSION_INFORMATION VersionInformationPointer;

    if (Set != FALSE) {
        Status = STATUS_ACCESS_DENIED;
        return Status;
    }

    //
    // If the data is at least big enough to hold the version information
    // structure, then try to get everything.
    //

    if (*DataSize >= sizeof(SYSTEM_VERSION_INFORMATION)) {
        BufferSize = *DataSize - sizeof(SYSTEM_VERSION_INFORMATION);
        VersionInformationPointer = Data;
        Status = KeGetSystemVersion(VersionInformationPointer,
                                    Data + sizeof(SYSTEM_VERSION_INFORMATION),
                                    &BufferSize);

        if (KSUCCESS(Status)) {

            //
            // Make the string pointers into offsets for user mode.
            //

            if (VersionInformationPointer->ProductName != NULL) {
                VersionInformationPointer->ProductName =
                    (PVOID)((UINTN)(VersionInformationPointer->ProductName) -
                            (UINTN)Data);
            }

            if (VersionInformationPointer->BuildString != NULL) {
                VersionInformationPointer->BuildString =
                    (PVOID)((UINTN)(VersionInformationPointer->BuildString) -
                            (UINTN)Data);
            }
        }

    //
    // The data isn't even big enough for the version information structure.
    // Call out to get the true size.
    //

    } else {
        VersionInformationPointer = &VersionInformation;
        BufferSize = 0;
        KeGetSystemVersion(VersionInformationPointer,
                           NULL,
                           &BufferSize);

        Status = STATUS_BUFFER_TOO_SMALL;
    }

    BufferSize += sizeof(SYSTEM_VERSION_INFORMATION);
    *DataSize = BufferSize;
    return Status;
}

KSTATUS
KepGetFirmwareTable (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets a system firmware table.

Arguments:

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    PDESCRIPTION_HEADER AcpiTable;
    ULONG Length;
    PSMBIOS_ENTRY_POINT SmbiosTable;
    KSTATUS Status;

    if (Set != FALSE) {
        return STATUS_ACCESS_DENIED;
    }

    Status = PsCheckPermission(PERMISSION_SYSTEM_ADMINISTRATOR);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (*DataSize < sizeof(DESCRIPTION_HEADER)) {
        *DataSize = sizeof(DESCRIPTION_HEADER);
        return STATUS_BUFFER_TOO_SMALL;
    }

    AcpiTable = AcpiFindTable(*((PULONG)Data), NULL);
    if (AcpiTable == NULL) {
        *DataSize = 0;
        return STATUS_NOT_FOUND;
    }

    if (*((PULONG)Data) == SMBIOS_ANCHOR_STRING_VALUE) {
        SmbiosTable = (PSMBIOS_ENTRY_POINT)AcpiTable;
        Length = sizeof(SMBIOS_ENTRY_POINT) + SmbiosTable->StructureTableLength;

    } else {
        Length = AcpiTable->Length;
    }

    if (*DataSize < Length) {
        *DataSize = Length;
        return STATUS_BUFFER_TOO_SMALL;
    }

    RtlCopyMemory(Data, AcpiTable, Length);
    *DataSize = Length;
    return STATUS_SUCCESS;
}

KSTATUS
KepGetFirmwareType (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets the platform firmware type.

Arguments:

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    if (Set != FALSE) {
        return STATUS_ACCESS_DENIED;
    }

    if (*DataSize < sizeof(ULONG)) {
        *DataSize = sizeof(ULONG);
        return STATUS_BUFFER_TOO_SMALL;
    }

    *((PULONG)Data) = KeSystemFirmwareType;
    *DataSize = sizeof(ULONG);
    return STATUS_SUCCESS;
}

KSTATUS
KepGetProcessorUsage (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets processor usage information.

Arguments:

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    PPROCESSOR_USAGE_INFORMATION Information;
    UINTN ProcessorCount;
    KSTATUS Status;

    if (Set != FALSE) {
        return STATUS_ACCESS_DENIED;
    }

    Status = PsCheckPermission(PERMISSION_RESOURCES);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (*DataSize != sizeof(PROCESSOR_USAGE_INFORMATION)) {
        *DataSize = sizeof(PROCESSOR_USAGE_INFORMATION);
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    Information = Data;
    Information->CycleCounterFrequency = HlQueryProcessorCounterFrequency();
    if (Information->ProcessorNumber == (UINTN)-1) {
        KeGetTotalProcessorCycleAccounting(&(Information->Usage));

    } else {
        ProcessorCount = KeGetActiveProcessorCount();
        if (Information->ProcessorNumber > ProcessorCount) {
            Information->ProcessorNumber = ProcessorCount;
            return STATUS_OUT_OF_BOUNDS;
        }

        Status = KeGetProcessorCycleAccounting(Information->ProcessorNumber,
                                               &(Information->Usage));

        return Status;
    }

    return STATUS_SUCCESS;
}

KSTATUS
KepGetProcessorCount (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets processor count information.

Arguments:

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    PPROCESSOR_COUNT_INFORMATION Information;
    KSTATUS Status;

    if (Set != FALSE) {
        return STATUS_ACCESS_DENIED;
    }

    Status = PsCheckPermission(PERMISSION_RESOURCES);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (*DataSize != sizeof(PROCESSOR_COUNT_INFORMATION)) {
        *DataSize = sizeof(PROCESSOR_COUNT_INFORMATION);
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    Information = Data;
    Information->MaxProcessorCount = HlGetMaximumProcessorCount();
    Information->ActiveProcessorCount = KeGetActiveProcessorCount();
    return STATUS_SUCCESS;
}

KSTATUS
KepGetKernelCommandLine (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets the kernel command line information.

Arguments:

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    if (Set != FALSE) {
        return STATUS_ACCESS_DENIED;
    }

    if (KeCommandLine == NULL) {
        return STATUS_NOT_FOUND;
    }

    if (*DataSize < KeCommandLine->LineSize) {
        *DataSize = KeCommandLine->LineSize;
        return STATUS_BUFFER_TOO_SMALL;
    }

    RtlCopyMemory(Data, KeCommandLine->Line, KeCommandLine->LineSize);
    return STATUS_SUCCESS;
}

