/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    locate.c

Abstract:

    This module implements support for locating handles.

Author:

    Evan Green 5-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_LOCATE_POSITION {
    EFI_GUID *Protocol;
    VOID *SearchKey;
    PLIST_ENTRY Position;
    PEFI_PROTOCOL_ENTRY ProtocolEntry;
} EFI_LOCATE_POSITION, *PEFI_LOCATE_POSITION;

typedef
PEFI_HANDLE_DATA
(*PEFI_CORE_GET_NEXT_HANDLE) (
    PEFI_LOCATE_POSITION Position,
    VOID **Interface
    );

/*++

Routine Description:

    This routine gets the next handle when searching for handles.

Arguments:

    Position - Supplies a pointer to the current position. This will be updated.

    Interface - Supplies a pointer where the interface structure for the
        matching protocol will be returned.

Return Value:

    EFI_SUCCESS if a handle was returned.

    EFI_NOT_FOUND if no handles match the search.

    EFI_INVALID_PARAMETER if the protocol is NULL, device path is NULL, or a
    handle matched and the device is NULL.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

PEFI_HANDLE_DATA
EfipCoreGetNextHandle (
    PEFI_LOCATE_POSITION Position,
    VOID **Interface
    );

PEFI_HANDLE_DATA
EfipCoreGetNextHandleByRegisterNotify (
    PEFI_LOCATE_POSITION Position,
    VOID **Interface
    );

PEFI_HANDLE_DATA
EfipCoreGetNextHandleByProtocol (
    PEFI_LOCATE_POSITION Position,
    VOID **Interface
    );

//
// -------------------------------------------------------------------- Globals
//

UINTN EfiLocateHandleRequest;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiCoreLocateDevicePath (
    EFI_GUID *Protocol,
    EFI_DEVICE_PATH_PROTOCOL **DevicePath,
    EFI_HANDLE *Device
    )

/*++

Routine Description:

    This routine attempts to locate the handle to a device on the device path
    that supports the specified protocol.

Arguments:

    Protocol - Supplies a pointer to the protocol to search for.

    DevicePath - Supplies a pointer that on input contains a pointer to the
        device path. On output, the path pointer is modified to point to the
        remaining part of the device path.

    Device - Supplies a pointer where the handle of the device will be
        returned.

Return Value:

    EFI_SUCCESS if a handle was returned.

    EFI_NOT_FOUND if no handles match the search.

    EFI_INVALID_PARAMETER if the protocol is NULL, device path is NULL, or a
    handle matched and the device is NULL.

--*/

{

    EFI_HANDLE BestDevice;
    INTN BestMatch;
    EFI_HANDLE Handle;
    UINTN HandleCount;
    EFI_HANDLE *Handles;
    UINTN Index;
    EFI_DEVICE_PATH_PROTOCOL *SearchPath;
    INTN Size;
    EFI_DEVICE_PATH_PROTOCOL *SourcePath;
    INTN SourceSize;
    EFI_STATUS Status;

    if ((Protocol == NULL) || (DevicePath == NULL) || (*DevicePath == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    BestDevice = NULL;
    SourcePath = *DevicePath;
    SearchPath = SourcePath;
    while (EfiCoreIsDevicePathEnd(SearchPath) == FALSE) {

        //
        // If the device path is a multi-instance device path, this function
        // will operate on the first instance.
        //

        if (EfiCoreIsDevicePathEndInstance(SearchPath) != FALSE) {
            break;
        }

        SearchPath = EfiCoreGetNextDevicePathNode(SearchPath);
    }

    SourceSize = (UINTN)SearchPath - (UINTN)SourcePath;

    //
    // Get a list of all handles that support the given protocol.
    //

    Status = EfiCoreLocateHandleBuffer(ByProtocol,
                                       Protocol,
                                       NULL,
                                       &HandleCount,
                                       &Handles);

    if ((EFI_ERROR(Status)) || (HandleCount == 0)) {
        return EFI_NOT_FOUND;
    }

    BestMatch = -1;
    for (Index = 0; Index < HandleCount; Index += 1) {
        Handle = Handles[Index];
        Status = EfiCoreHandleProtocol(Handle,
                                       &EfiDevicePathProtocolGuid,
                                       (VOID **)&SearchPath);

        if (EFI_ERROR(Status)) {
            continue;
        }

        //
        // Check if the device path is the first part of the source path.
        //

        Size = EfiCoreGetDevicePathSize(SearchPath) -
               sizeof(EFI_DEVICE_PATH_PROTOCOL);

        ASSERT(Size >= 0);

        if ((Size <= SourceSize) &&
            (EfiCoreCompareMemory(SourcePath, SearchPath, (UINTN)Size) == 0)) {

            //
            // If the size is equal to the best match, then there is a
            // duplicate device path for two different device handles.
            //

            ASSERT(Size != BestMatch);

            if (Size > BestMatch) {
                BestMatch = Size;
                BestDevice = Handle;
            }
        }
    }

    EfiCoreFreePool(Handles);

    //
    // If there wasn't any match, then no parts of the device path where found.
    // This is unexpected, since there should be a "root level" device path
    // in the system.
    //

    if (BestMatch == -1) {
        return EFI_NOT_FOUND;
    }

    if (Device == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    *Device = BestDevice;

    //
    // Return the remaining part of the device path.
    //

    *DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)(((UINT8 *)SourcePath) +
                                               BestMatch);

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiCoreLocateHandleBuffer (
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

    UINTN BufferSize;
    EFI_STATUS Status;

    if ((HandleCount == NULL) || (Buffer == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    BufferSize = 0;
    *HandleCount = 0;
    *Buffer = NULL;
    Status = EfiCoreLocateHandle(SearchType,
                                 Protocol,
                                 SearchKey,
                                 &BufferSize,
                                 *Buffer);

    if ((EFI_ERROR(Status)) && (Status != EFI_BUFFER_TOO_SMALL)) {
        if (Status != EFI_INVALID_PARAMETER) {
            Status = EFI_NOT_FOUND;
        }

        return Status;
    }

    *Buffer = EfiCoreAllocateBootPool(BufferSize);
    if (*Buffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    Status = EfiCoreLocateHandle(SearchType,
                                 Protocol,
                                 SearchKey,
                                 &BufferSize,
                                 *Buffer);

    *HandleCount = BufferSize / sizeof(EFI_HANDLE);
    if (EFI_ERROR(Status)) {
        *HandleCount = 0;
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreLocateHandle (
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

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if no handles match the search.

    EFI_BUFFER_TOO_SMALL if the given buffer wasn't big enough to hold all the
    results.

    EFI_INVALID_PARAMETER if the serach type is invalid, one of the parameters
    required by the given search type was NULL, one or more matches are found
    and the buffer size is NULL, or the buffer size is large enough and the
    buffer is NULL.

--*/

{

    PEFI_CORE_GET_NEXT_HANDLE GetNextHandleFunction;
    PEFI_HANDLE_DATA Handle;
    VOID *Interface;
    EFI_LOCATE_POSITION Position;
    PEFI_PROTOCOL_NOTIFY ProtocolNotify;
    PEFI_HANDLE_DATA *ResultBuffer;
    UINTN ResultSize;
    EFI_STATUS Status;

    if ((BufferSize == NULL) || ((*BufferSize > 0) && (Buffer == NULL))) {
        return EFI_INVALID_PARAMETER;
    }

    GetNextHandleFunction = NULL;
    Position.Protocol = Protocol;
    Position.SearchKey = SearchKey;
    Position.Position = &EfiHandleList;
    ResultSize = 0;
    ResultBuffer = (PEFI_HANDLE_DATA *)Buffer;
    Status = EFI_SUCCESS;
    EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
    switch (SearchType) {
    case AllHandles:
        GetNextHandleFunction = EfipCoreGetNextHandle;
        break;

    case ByRegisterNotify:
        GetNextHandleFunction = EfipCoreGetNextHandleByRegisterNotify;
        if (SearchKey == NULL) {
            Status = EFI_INVALID_PARAMETER;
            break;
        }

        break;

    case ByProtocol:
        GetNextHandleFunction = EfipCoreGetNextHandleByProtocol;
        if (Protocol == NULL) {
            Status = EFI_INVALID_PARAMETER;
            break;
        }

        Position.ProtocolEntry = EfipCoreFindProtocolEntry(Protocol, FALSE);
        if (Position.ProtocolEntry == NULL) {
            Status = EFI_NOT_FOUND;
            break;
        }

        Position.Position = &(Position.ProtocolEntry->ProtocolList);
        break;

    default:
        Status = EFI_INVALID_PARAMETER;
        break;
    }

    if (EFI_ERROR(Status)) {
        EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
        return Status;
    }

    ASSERT(GetNextHandleFunction != NULL);

    EfiLocateHandleRequest += 1;
    while (TRUE) {

        //
        // Get the next handle.
        //

        Handle = GetNextHandleFunction(&Position, &Interface);
        if (Handle == NULL) {
            break;
        }

        //
        // Increase the resulting buffer size, and if this handle fits, return
        // it.
        //

        ResultSize += sizeof(Handle);
        if (ResultSize <= *BufferSize) {
            *ResultBuffer = Handle;
            ResultBuffer += 1;
        }
    }

    //
    // If the result is a zero length buffer, then there were no matching
    // handles.
    //

    if (ResultSize == 0) {
        Status = EFI_NOT_FOUND;

    } else {

        //
        // Return the resulting buffer size. If it's larger than what was
        // passed in, then set the error code.
        //

        if (ResultSize > *BufferSize) {
            Status = EFI_BUFFER_TOO_SMALL;
        }

        *BufferSize = ResultSize;

        //
        // If this is a search by register notify and a handle was returned,
        // update the register notification position.
        //

        if ((SearchType == ByRegisterNotify) && (!EFI_ERROR(Status))) {

            ASSERT(SearchKey != NULL);

            ProtocolNotify = SearchKey;
            ProtocolNotify->Position = ProtocolNotify->Position->Next;
        }
    }

    EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreLocateProtocol (
    EFI_GUID *Protocol,
    VOID *Registration,
    VOID **Interface
    )

/*++

Routine Description:

    This routine returns the first protocol instance that matches the given
    protocol.

Arguments:

    Protocol - Supplies a pointer to the protocol to search by.

    Registration - Supplies a pointer to an optional registration key
        returned from RegisterProtocolNotify.

    Interface - Supplies a pointer where a pointer to the first interface that
        matches will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if no protocol instances matched the search.

    EFI_INVALID_PARAMETER if the interface is NULL.

--*/

{

    PEFI_HANDLE_DATA Handle;
    EFI_LOCATE_POSITION Position;
    PEFI_PROTOCOL_NOTIFY ProtocolNotify;
    EFI_STATUS Status;

    if (Interface == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    if (Protocol == NULL) {
        return EFI_NOT_FOUND;
    }

    *Interface = NULL;
    Status = EFI_SUCCESS;
    Position.Protocol = Protocol;
    Position.SearchKey = Registration;
    Position.Position = &EfiHandleList;
    EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
    EfiLocateHandleRequest += 1;
    if (Registration != NULL) {
        Position.ProtocolEntry = EfipCoreFindProtocolEntry(Protocol, FALSE);
        if (Position.ProtocolEntry == NULL) {
            Status = EFI_NOT_FOUND;
            goto CoreLocateProtocolEnd;
        }

        Position.Position = &(Position.ProtocolEntry->ProtocolList);
        Handle = EfipCoreGetNextHandleByProtocol(&Position, Interface);

    } else {
        Handle = EfipCoreGetNextHandleByRegisterNotify(&Position, Interface);
    }

    if (Handle == NULL) {
        Status = EFI_NOT_FOUND;

    //
    // If this is a search by register notify and a handle was returned,
    // update the register notify position.
    //

    } else if (Registration != NULL) {
        ProtocolNotify = Registration;
        ProtocolNotify->Position = ProtocolNotify->Position->Next;
    }

CoreLocateProtocolEnd:
    EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

PEFI_HANDLE_DATA
EfipCoreGetNextHandle (
    PEFI_LOCATE_POSITION Position,
    VOID **Interface
    )

/*++

Routine Description:

    This routine gets the next handle when searching for all handles.

Arguments:

    Position - Supplies a pointer to the current position. This will be updated.

    Interface - Supplies a pointer where the interface structure for the
        matching protocol will be returned.

Return Value:

    EFI_SUCCESS if a handle was returned.

    EFI_NOT_FOUND if no handles match the search.

    EFI_INVALID_PARAMETER if the protocol is NULL, device path is NULL, or a
    handle matched and the device is NULL.

--*/

{

    PEFI_HANDLE_DATA Handle;

    Position->Position = Position->Position->Next;
    Handle = NULL;
    *Interface = NULL;
    if (Position->Position != &EfiHandleList) {
        Handle = LIST_VALUE(Position->Position, EFI_HANDLE_DATA, ListEntry);

        ASSERT(Handle->Magic == EFI_HANDLE_MAGIC);
    }

    return Handle;
}

PEFI_HANDLE_DATA
EfipCoreGetNextHandleByRegisterNotify (
    PEFI_LOCATE_POSITION Position,
    VOID **Interface
    )

/*++

Routine Description:

    This routine gets the next handle when searching for register protocol
    notifies.

Arguments:

    Position - Supplies a pointer to the current position. This will be updated.

    Interface - Supplies a pointer where the interface structure for the
        matching protocol will be returned.

Return Value:

    EFI_SUCCESS if a handle was returned.

    EFI_NOT_FOUND if no handles match the search.

    EFI_INVALID_PARAMETER if the protocol is NULL, device path is NULL, or a
    handle matched and the device is NULL.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_HANDLE_DATA Handle;
    PEFI_PROTOCOL_INTERFACE ProtocolInterface;
    PEFI_PROTOCOL_NOTIFY ProtocolNotify;

    Handle = NULL;
    *Interface = NULL;
    ProtocolNotify = Position->SearchKey;
    if (ProtocolNotify != NULL) {

        ASSERT(ProtocolNotify->Magic == EFI_PROTOCOL_NOTIFY_MAGIC);

        Position->SearchKey = NULL;

        //
        // If not at the end of the list, get the next handle.
        //

        CurrentEntry = ProtocolNotify->Position->Next;
        if (CurrentEntry != &(ProtocolNotify->Protocol->ProtocolList)) {
            ProtocolInterface = LIST_VALUE(CurrentEntry,
                                           EFI_PROTOCOL_INTERFACE,
                                           ProtocolListEntry);

            ASSERT(ProtocolInterface->Magic == EFI_PROTOCOL_INTERFACE_MAGIC);

            Handle = ProtocolInterface->Handle;
            *Interface = ProtocolInterface->Interface;
        }
    }

    return Handle;
}

PEFI_HANDLE_DATA
EfipCoreGetNextHandleByProtocol (
    PEFI_LOCATE_POSITION Position,
    VOID **Interface
    )

/*++

Routine Description:

    This routine gets the next handle when searching for a given protocol.

Arguments:

    Position - Supplies a pointer to the current position. This will be updated.

    Interface - Supplies a pointer where the interface structure for the
        matching protocol will be returned.

Return Value:

    EFI_SUCCESS if a handle was returned.

    EFI_NOT_FOUND if no handles match the search.

    EFI_INVALID_PARAMETER if the protocol is NULL, device path is NULL, or a
    handle matched and the device is NULL.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_HANDLE_DATA Handle;
    PEFI_PROTOCOL_INTERFACE ProtocolInterface;

    Handle = NULL;
    *Interface = NULL;
    while (TRUE) {
        CurrentEntry = Position->Position->Next;
        Position->Position = CurrentEntry;

        //
        // If at the end, stop.
        //

        if (CurrentEntry == &(Position->ProtocolEntry->ProtocolList)) {
            Handle = NULL;
            break;
        }

        //
        // Get the handle.
        //

        ProtocolInterface = LIST_VALUE(CurrentEntry,
                                       EFI_PROTOCOL_INTERFACE,
                                       ProtocolListEntry);

        ASSERT(ProtocolInterface->Magic == EFI_PROTOCOL_INTERFACE_MAGIC);

        Handle = ProtocolInterface->Handle;
        *Interface = ProtocolInterface->Interface;

        //
        // If this handle has not been returned this request, then return it
        // now.
        //

        if (Handle->LocateRequest != EfiLocateHandleRequest) {
            Handle->LocateRequest = EfiLocateHandleRequest;
            break;
        }
    }

    return Handle;
}

