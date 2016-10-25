/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    util.c

Abstract:

    This module implements utility functions for the UEFI boot firmware support.

Author:

    Evan Green 12-Feb-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/fw/smbios.h>
#include <minoca/uefi/uefi.h>
#include <minoca/uefi/guid/acpi.h>
#include <minoca/uefi/protocol/blockio.h>
#include <minoca/uefi/protocol/graphout.h>
#include <minoca/uefi/protocol/loadimg.h>
#include <minoca/uefi/protocol/ramdisk.h>
#include "firmware.h"
#include "efisup.h"

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

//
// Define some needed protocol GUIDs.
//

EFI_GUID BoEfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID BoEfiBlockIoProtocolGuid = EFI_BLOCK_IO_PROTOCOL_GUID;
EFI_GUID BoEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
EFI_GUID BoEfiDevicePathProtocolGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;
EFI_GUID BoEfiRamDiskProtocolGuid = EFI_RAM_DISK_PROTOCOL_GUID;

EFI_GUID BoEfiAcpiTableGuid = EFI_ACPI_20_TABLE_GUID;
EFI_GUID BoEfiAcpi1TableGuid = EFI_ACPI_10_TABLE_GUID;
EFI_GUID BoEfiSmbiosTableGuid = EFI_SMBIOS_TABLE_GUID;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
BopEfiLocateHandle (
    EFI_LOCATE_SEARCH_TYPE SearchType,
    EFI_GUID *Protocol,
    VOID *SearchKey,
    UINTN *BufferSize,
    EFI_HANDLE *Buffer
    )

/*++

Routine Description:

    This routine returns an array of handles that support a specified protocol.

Arguments:

    SearchType - Supplies which handle(s) are to be returned.

    Protocol - Supplies an optional pointer to the protocols to search by.

    SearchKey - Supplies an optional pointer to the search key.

    BufferSize - Supplies a pointer that on input contains the size of the
        result buffer in bytes. On output, the size of the result array will be
        returned (even if the buffer was too small).

    Buffer - Supplies a pointer where the results will be returned.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = BoEfiBootServices->LocateHandle(SearchType,
                                             Protocol,
                                             SearchKey,
                                             BufferSize,
                                             Buffer);

    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiLocateHandleBuffer (
    EFI_LOCATE_SEARCH_TYPE SearchType,
    EFI_GUID *Protocol,
    VOID *SearchKey,
    UINTN *HandleCount,
    EFI_HANDLE **Buffer
    )

/*++

Routine Description:

    This routine returns an array of handles that support the requested
    protocol in a buffer allocated from pool.

Arguments:

    SearchType - Supplies the search behavior.

    Protocol - Supplies a pointer to the protocol to search by.

    SearchKey - Supplies a pointer to the search key.

    HandleCount - Supplies a pointer where the number of handles will be
        returned.

    Buffer - Supplies a pointer where an array will be returned containing the
        requested handles.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if no handles match the search.

    EFI_INVALID_PARAMETER if the handle count or buffer is NULL.

    EFI_OUT_OF_RESOURCES if an allocation failed.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = BoEfiBootServices->LocateHandleBuffer(SearchType,
                                                   Protocol,
                                                   SearchKey,
                                                   HandleCount,
                                                   Buffer);

    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiOpenProtocol (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID **Interface,
    EFI_HANDLE AgentHandle,
    EFI_HANDLE ControllerHandle,
    UINT32 Attributes
    )

/*++

Routine Description:

    This routine queries a handle to determine if it supports a specified
    protocol. If the protocol is supported by the handle, it opens the protocol
    on behalf of the calling agent.

Arguments:

    Handle - Supplies the handle for the protocol interface that is being
        opened.

    Protocol - Supplies the published unique identifier of the protocol.

    Interface - Supplies the address where a pointer to the corresponding
        protocol interface is returned.

    AgentHandle - Supplies the handle of the agent that is opening the protocol
        interface specified by the protocol and interface.

    ControllerHandle - Supplies the controller handle that requires the
        protocl interface if the caller is a driver that follows the UEFI
        driver model. If the caller does not follow the UEFI Driver Model, then
        this parameter is optional.

    Attributes - Supplies the open mode of the protocol interface specified by
        the given handle and protocol.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = BoEfiBootServices->OpenProtocol(Handle,
                                             Protocol,
                                             Interface,
                                             AgentHandle,
                                             ControllerHandle,
                                             Attributes);

    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiCloseProtocol (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    EFI_HANDLE AgentHandle,
    EFI_HANDLE ControllerHandle
    )

/*++

Routine Description:

    This routine closes a protocol on a handle that was previously opened.

Arguments:

    Handle - Supplies the handle for the protocol interface was previously
        opened.

    Protocol - Supplies the published unique identifier of the protocol.

    AgentHandle - Supplies the handle of the agent that is closing the
        protocol interface.

    ControllerHandle - Supplies the controller handle that required the
        protocl interface if the caller is a driver that follows the UEFI
        driver model. If the caller does not follow the UEFI Driver Model, then
        this parameter is optional.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = BoEfiBootServices->CloseProtocol(Handle,
                                              Protocol,
                                              AgentHandle,
                                              ControllerHandle);

    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiHandleProtocol (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID **Interface
    )

/*++

Routine Description:

    This routine queries a handle to determine if it supports a specified
    protocol.

Arguments:

    Handle - Supplies the handle being queried.

    Protocol - Supplies the published unique identifier of the protocol.

    Interface - Supplies the address where a pointer to the corresponding
        protocol interface is returned.

Return Value:

    EFI_SUCCESS if the interface information was returned.

    EFI_UNSUPPORTED if the device not support the specified protocol.

    EFI_INVALID_PARAMETER if the handle, protocol, or interface is NULL.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = BoEfiBootServices->HandleProtocol(Handle, Protocol, Interface);
    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiFreePool (
    VOID *Buffer
    )

/*++

Routine Description:

    This routine frees memory allocated from the EFI firmware heap (not the
    boot environment heap).

Arguments:

    Buffer - Supplies a pointer to the buffer to free.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the buffer was invalid.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = BoEfiBootServices->FreePool(Buffer);
    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiExitBootServices (
    EFI_HANDLE ImageHandle,
    UINTN MapKey
    )

/*++

Routine Description:

    This routine terminates all boot services.

Arguments:

    ImageHandle - Supplies the handle that identifies the exiting image.

    MapKey - Supplies the latest memory map key.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the map key is incorrect.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = BoEfiBootServices->ExitBootServices(ImageHandle, MapKey);
    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiGetTime (
    EFI_TIME *Time,
    EFI_TIME_CAPABILITIES *Capabilities
    )

/*++

Routine Description:

    This routine returns the current time and date information, and
    timekeeping capabilities of the hardware platform.

Arguments:

    Time - Supplies a pointer where the current time will be returned.

    Capabilities - Supplies an optional pointer where the capabilities will be
        returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the time parameter was NULL.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = BoEfiRuntimeServices->GetTime(Time, Capabilities);
    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiStall (
    UINTN Microseconds
    )

/*++

Routine Description:

    This routine induces a fine-grained delay.

Arguments:

    Microseconds - Supplies the number of microseconds to stall execution for.

Return Value:

    EFI_SUCCESS on success.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = BoEfiBootServices->Stall(Microseconds);
    BopEfiRestoreApplicationContext();
    return Status;
}

VOID
BopEfiResetSystem (
    EFI_RESET_TYPE ResetType,
    EFI_STATUS ResetStatus,
    UINTN DataSize,
    VOID *ResetData
    )

/*++

Routine Description:

    This routine resets the entire platform.

Arguments:

    ResetType - Supplies the type of reset to perform.

    ResetStatus - Supplies the status code for this reset.

    DataSize - Supplies the size of the reset data.

    ResetData - Supplies an optional pointer for reset types of cold, warm, or
        shutdown to a null-terminated string, optionally followed by additional
        binary data.

Return Value:

    None. This routine does not return.

--*/

{

    BopEfiRestoreFirmwareContext();
    BoEfiRuntimeServices->ResetSystem(ResetType,
                                      ResetStatus,
                                      DataSize,
                                      ResetData);

    BoEfiBootServices->Stall(1000000);
    BopEfiRestoreApplicationContext();
    return;
}

VOID
BopEfiPrintString (
    PWSTR WideString
    )

/*++

Routine Description:

    This routine prints a string to the EFI standard out console.

Arguments:

    WideString - Supplies a pointer to the wide string to print. A wide
        character is two bytes in EFI.

Return Value:

    None.

--*/

{

    if (BoEfiSystemTable->ConOut != NULL) {
        BopEfiRestoreFirmwareContext();
        BoEfiSystemTable->ConOut->OutputString(BoEfiSystemTable->ConOut,
                                               WideString);

        BopEfiRestoreApplicationContext();
    }

    return;
}

KSTATUS
BopEfiGetSystemConfigurationTable (
    EFI_GUID *Guid,
    VOID **Table
    )

/*++

Routine Description:

    This routine attempts to find a configuration table with the given GUID.

Arguments:

    Guid - Supplies a pointer to the GUID to search for.

    Table - Supplies a pointer where a pointer to the table will be returned on
        success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if no table with the given GUID could be found.

    STATUS_NOT_INITIALIZED if the EFI subsystem has not yet started.

--*/

{

    EFI_CONFIGURATION_TABLE *EfiTable;
    UINTN TableIndex;

    if (BoEfiSystemTable == NULL) {
        return STATUS_NOT_INITIALIZED;
    }

    for (TableIndex = 0;
         TableIndex < BoEfiSystemTable->NumberOfTableEntries;
         TableIndex += 1) {

        EfiTable = &(BoEfiSystemTable->ConfigurationTable[TableIndex]);
        if (BopEfiAreGuidsEqual(Guid, &(EfiTable->VendorGuid)) != FALSE) {
            *Table = EfiTable->VendorTable;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_NOT_FOUND;
}

KSTATUS
BopEfiStatusToKStatus (
    EFI_STATUS Status
    )

/*++

Routine Description:

    This routine returns a kernel status code similar to the given EFI status
    code.

Arguments:

    Status - Supplies the EFI status code.

Return Value:

    Status code.

--*/

{

    switch (Status) {
    case EFI_SUCCESS:
        return STATUS_SUCCESS;

    case EFI_LOAD_ERROR:
        return STATUS_UNSUCCESSFUL;

    case EFI_INVALID_PARAMETER:
        return STATUS_INVALID_PARAMETER;

    case EFI_UNSUPPORTED:
        return STATUS_NOT_SUPPORTED;

    case EFI_BAD_BUFFER_SIZE:
        return STATUS_DATA_LENGTH_MISMATCH;

    case EFI_BUFFER_TOO_SMALL:
        return STATUS_BUFFER_TOO_SMALL;

    case EFI_NOT_READY:
        return STATUS_NOT_READY;

    case EFI_DEVICE_ERROR:
        return STATUS_DEVICE_IO_ERROR;

    case EFI_WRITE_PROTECTED:
        return STATUS_ACCESS_DENIED;

    case EFI_OUT_OF_RESOURCES:
        return STATUS_INSUFFICIENT_RESOURCES;

    case EFI_VOLUME_CORRUPTED:
        return STATUS_VOLUME_CORRUPT;

    case EFI_VOLUME_FULL:
        return STATUS_VOLUME_FULL;

    case EFI_NO_MEDIA:
        return STATUS_NO_MEDIA;

    case EFI_MEDIA_CHANGED:
        return STATUS_INVALID_HANDLE;

    case EFI_NOT_FOUND:
        return STATUS_NOT_FOUND;

    case EFI_ACCESS_DENIED:
        return STATUS_ACCESS_DENIED;

    case EFI_NO_RESPONSE:
        return STATUS_NO_DATA_AVAILABLE;

    case EFI_NO_MAPPING:
        return STATUS_INVALID_ADDRESS;

    case EFI_TIMEOUT:
        return STATUS_TIMEOUT;

    case EFI_NOT_STARTED:
        return STATUS_NOT_STARTED;

    case EFI_ALREADY_STARTED:
        return STATUS_ALREADY_INITIALIZED;

    case EFI_ABORTED:
        return STATUS_INTERRUPTED;

    case EFI_ICMP_ERROR:
        return STATUS_INVALID_SEQUENCE;

    case EFI_TFTP_ERROR:
        return STATUS_INVALID_SEQUENCE;

    case EFI_PROTOCOL_ERROR:
        return STATUS_INVALID_SEQUENCE;

    case EFI_INCOMPATIBLE_VERSION:
        return STATUS_VERSION_MISMATCH;

    case EFI_SECURITY_VIOLATION:
        return STATUS_ACCESS_DENIED;

    case EFI_CRC_ERROR:
        return STATUS_FILE_CORRUPT;

    case EFI_END_OF_MEDIA:
        return STATUS_END_OF_FILE;

    case EFI_END_OF_FILE:
        return STATUS_END_OF_FILE;

    case EFI_INVALID_LANGUAGE:
        return STATUS_NOT_SUPPORTED;

    case EFI_COMPROMISED_DATA:
        return STATUS_UNSUCCESSFUL;

    case EFI_WARN_UNKNOWN_GLYPH:
        return STATUS_UNEXPECTED_TYPE;

    case EFI_WARN_DELETE_FAILURE:
        return STATUS_SUCCESS;

    case EFI_WARN_WRITE_FAILURE:
        return STATUS_SUCCESS;

    case EFI_WARN_BUFFER_TOO_SMALL:
        return STATUS_BUFFER_TOO_SMALL;

    case EFI_WARN_STALE_DATA:
        return STATUS_SUCCESS;

    case EFI_NETWORK_UNREACHABLE:
        return STATUS_DESTINATION_UNREACHABLE;

    case EFI_HOST_UNREACHABLE:
        return STATUS_DESTINATION_UNREACHABLE;

    case EFI_PROTOCOL_UNREACHABLE:
        return STATUS_DESTINATION_UNREACHABLE;

    case EFI_PORT_UNREACHABLE:
        return STATUS_DESTINATION_UNREACHABLE;

    case EFI_CONNECTION_FIN:
        return STATUS_CONNECTION_CLOSED;

    case EFI_CONNECTION_RESET:
        return STATUS_CONNECTION_RESET;

    case EFI_CONNECTION_REFUSED:
        return STATUS_CONNECTION_RESET;

    default:
        break;
    }

    return STATUS_UNSUCCESSFUL;
}

BOOL
BopEfiAreGuidsEqual (
    EFI_GUID *FirstGuid,
    EFI_GUID *SecondGuid
    )

/*++

Routine Description:

    This routine determines if two GUIDs are equal.

Arguments:

    FirstGuid - Supplies a pointer to the first GUID to compare.

    SecondGuid - Supplies a pointer to the second GUId to compare.

Return Value:

    TRUE if the two GUIDs have equal values.

    FALSE if the two GUIDs are not the same.

--*/

{

    PULONG FirstPointer;
    PULONG SecondPointer;

    //
    // Compare GUIDs 32 bits at a time.
    //

    FirstPointer = (PULONG)FirstGuid;
    SecondPointer = (PULONG)SecondGuid;
    if ((FirstPointer[0] == SecondPointer[0]) &&
        (FirstPointer[1] == SecondPointer[1]) &&
        (FirstPointer[2] == SecondPointer[2]) &&
        (FirstPointer[3] == SecondPointer[3])) {

        return TRUE;
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

