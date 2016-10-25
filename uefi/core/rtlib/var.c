/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    var.c

Abstract:

    This module implements UEFI runtime core variable services.

Author:

    Evan Green 18-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlib.h"
#include "varback.h"

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_VARIABLE_HEADER_MAGIC 0x73726156
#define EFI_VARIABLE_HEADER_VERSION 0x00010000

//
// Define the default allocation size for EFI variable storages.
//

#define EFI_DEFAULT_VARIABLE_SPACE_PAGE_COUNT 0x10

//
// Define the size of the header to CRC.
//

#define EFI_VARIABLE_HEADER_CRC_SIZE OFFSET_OF(EFI_VARIABLE_HEADER, HeaderCrc32)

//
// This flag is set if the variable storage area has been written to but not
// flushed to non-volatile storage.
//

#define EFI_VARIABLE_FLAG_DIRTY 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure sits at the head of the variable storage area.

Members:

    Magic - Stores the constant value EFI_VARIABLE_HEADER_MAGIC.

    Version - Stores the version of the storage format. Set to
        EFI_VARIABLE_HEADER_VERSION.

    Flags - Stores a bitfield of flags describing the variable state. See
        EFI_VARIABLE_FLAG_* definitions.

    DataSize - Stores the size of the region of valid data following this
        header, including the header itself.

    FreeSize - Stores the amount of space that's free data. This may not be
        contiguous.

    HeaderCrc32 - Stores the CRC32 of the header, up to this field. Use the
        EFI_VARIABLE_HEADER_CRC_SIZE define.

    DataCrc32 - Stores the CRC32 of the data portion, not including this header.

--*/

typedef struct _EFI_VARIABLE_HEADER {
    UINT32 Magic;
    UINT32 Version;
    UINT32 Flags;
    UINT32 DataSize;
    UINT32 FreeSize;
    UINT32 HeaderCrc32;
    UINT32 DataCrc32;
} EFI_VARIABLE_HEADER, *PEFI_VARIABLE_HEADER;

/*++

Structure Description:

    This structure defines the layout of an EFI variable. This structure
    is laid out in an array, but there's variable length data off the end of
    each structure.

Members:

    VendorGuid - Stores the vendor GUID of the variable.

    Attributes - Stores the variable attributes.

    NameSize - Stores the size of the name that immediately follows this
        structure, in bytes, including the null terminator.

    DataSize - Stores the size of the data that immediately follows the name
        data, in bytes.

--*/

typedef struct _EFI_VARIABLE_ENTRY {
    EFI_GUID VendorGuid;
    UINT32 Attributes;
    UINT32 NameSize;
    UINT32 DataSize;
} EFI_VARIABLE_ENTRY, *PEFI_VARIABLE_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfiVariableBackendSetData (
    EFI_VARIABLE_BACKEND_PROTOCOL *This,
    VOID *Data,
    UINTN DataSize,
    BOOLEAN Replace
    );

EFIAPI
EFI_STATUS
EfiVariableBackendGetData (
    EFI_VARIABLE_BACKEND_PROTOCOL *This,
    VOID **Data,
    UINTN *DataSize
    );

BOOLEAN
EfipValidateVariableSpace (
    PEFI_VARIABLE_HEADER Header,
    UINTN TotalSize
    );

EFI_STATUS
EfipWriteVariableData (
    PEFI_VARIABLE_HEADER Header
    );

EFI_STATUS
EfipSetVariableDataCrc (
    PEFI_VARIABLE_HEADER Header
    );

PEFI_VARIABLE_ENTRY
EfipCoreGetVariableEntry (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    VOID **Data
    );

VOID
EfipCoreDeleteVariableEntry (
    PEFI_VARIABLE_ENTRY Entry
    );

PEFI_VARIABLE_ENTRY
EfipCoreAddVariableEntry (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 Attributes,
    UINTN DataSize,
    VOID *Data
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the space reserved for EFI variables. This can be overridden by the
// platform-specific portion of the binary.
//

UINTN EfiVariableAllocationPageCount = EFI_DEFAULT_VARIABLE_SPACE_PAGE_COUNT;

//
// Define the physical address for variable storage. If zero, any pages are
// allocated. The platform-specific code can set this to reserve a region of
// memory just for variables.
//

EFI_PHYSICAL_ADDRESS EfiVariableAllocationAddress = 0;

//
// Define the pointer to the variable region.
//

PEFI_VARIABLE_HEADER EfiVariableHeader;
PEFI_VARIABLE_ENTRY EfiVariableEnd;
PEFI_VARIABLE_ENTRY EfiVariableNextFree;

//
// Remember if the variables have changed.
//

BOOLEAN EfiVariablesChanged = FALSE;

//
// Store the single instance of the variable backend protocol.
//

EFI_GUID EfiVariableBackendProtocolGuid = EFI_VARIABLE_BACKEND_PROTOCOL_GUID;

EFI_VARIABLE_BACKEND_PROTOCOL EfiVariableBackendProtocol = {
    EfiVariableBackendSetData,
    EfiVariableBackendGetData
};

EFI_HANDLE EfiVariableBackendHandle;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiCoreSetVariable (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 Attributes,
    UINTN DataSize,
    VOID *Data
    )

/*++

Routine Description:

    This routine sets the value of a variable.

Arguments:

    VariableName - Supplies a pointer to a null-terminated string containing
        the name of the vendor's variable. Each variable name is unique for a
        particular vendor GUID. A variable name must be at least one character
        in length.

    VendorGuid - Supplies a pointer to the unique GUID for the vendor.

    Attributes - Supplies the attributes for this variable. See EFI_VARIABLE_*
        definitions.

    DataSize - Supplies the size of the data buffer. Unless the
        EFI_VARIABLE_APPEND_WRITE, EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS, or
        EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS attribute is set, a
        size of zero causes the variable to be deleted. When the
        EFI_VARIABLE_APPEND_WRITE attribute is set, then a set variable call
        with a data size of zero will not cause any change to the variable
        value (the timestamp associated with the variable may be updated
        however even if no new data value is provided,see the description of
        the EFI_VARIABLE_AUTHENTICATION_2 descriptor below. In this case the
        data size will not be zero since the EFI_VARIABLE_AUTHENTICATION_2
        descriptor will be populated).

    Data - Supplies the contents of the variable.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the variable being updated or deleted was not found.

    EFI_INVALID_PARAMETER if an invalid combination of attribute bits, name,
    and GUID was suplied, data size exceeds the maximum, or the variable name
    is an empty string.

    EFI_DEVICE_ERROR if a hardware error occurred trying to access the variable.

    EFI_WRITE_PROTECTED if the variable is read-only or cannot be deleted.

    EFI_SECURITY_VIOLATION if variable could not be written due to
    EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS or
    EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACESS being set, but the
    authorization information does NOT pass the validation check carried out by
    the firmware.

--*/

{

    PEFI_VARIABLE_ENTRY Entry;
    VOID *InternalData;

    if ((EfiIsAtRuntime() != FALSE) &&
        ((Attributes & EFI_VARIABLE_RUNTIME_ACCESS) == 0)) {

        return EFI_NOT_FOUND;
    }

    Entry = EfipCoreGetVariableEntry(VariableName, VendorGuid, &InternalData);
    if (Entry == NULL) {
        if (DataSize == 0) {
            return EFI_NOT_FOUND;
        }

        Entry = EfipCoreAddVariableEntry(VariableName,
                                         VendorGuid,
                                         Attributes,
                                         DataSize,
                                         Data);

        if (Entry == NULL) {
            return EFI_OUT_OF_RESOURCES;
        }

    //
    // The variable is there and has a zero size, so delete it.
    //

    } else if (DataSize == 0) {
        EfipCoreDeleteVariableEntry(Entry);

    //
    // The variable data size hasn't changed, so just smash over the data.
    //

    } else if (DataSize == Entry->DataSize) {
        Entry->Attributes |= Attributes;
        EfiCoreCopyMemory(InternalData, Data, DataSize);
        EfiVariableHeader->Flags |= EFI_VARIABLE_FLAG_DIRTY;
        EfiVariablesChanged = TRUE;

    //
    // Delete the entry and add it back. Don't lose the old attributes.
    //

    } else {
        Attributes |= Entry->Attributes;
        EfipCoreDeleteVariableEntry(Entry);
        Entry = EfipCoreAddVariableEntry(VariableName,
                                         VendorGuid,
                                         Attributes,
                                         DataSize,
                                         Data);

        if (Entry == NULL) {
            return EFI_OUT_OF_RESOURCES;
        }
    }

    //
    // If at runtime, try to write this out to non-volatile storage immediately.
    //

    if (EfiIsAtRuntime() != FALSE) {
        EfipWriteVariableData(EfiVariableHeader);
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiCoreGetNextVariableName (
    UINTN *VariableNameSize,
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid
    )

/*++

Routine Description:

    This routine enumerates the current variable names.

Arguments:

    VariableNameSize - Supplies a pointer that on input contains the size of
        the variable name buffer. On output, will contain the size of the
        variable name.

    VariableName - Supplies a pointer that on input contains the last variable
        name that was returned. On output, returns the null terminated string
        of the current variable.

    VendorGuid - Supplies a pointer that on input contains the last vendor GUID
        returned by this routine. On output, returns the vendor GUID of the
        current variable.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the next variable was not found.

    EFI_BUFFER_TOO_SMALL if the supplied buffer is not big enough.

    EFI_INVALID_PARAMETER if the variable name, vendor GUID, or data size is
    NULL.

    EFI_DEVICE_ERROR if a hardware error occurred trying to read the variable.

--*/

{

    BOOLEAN Done;
    PEFI_VARIABLE_ENTRY Entry;
    VOID *InternalData;
    UINTN StringSize;

    Done = FALSE;
    while (Done == FALSE) {
        Entry = EfipCoreGetVariableEntry(VariableName,
                                         VendorGuid,
                                         &InternalData);

        if (Entry == NULL) {
            return EFI_NOT_FOUND;
        }

        //
        // If at runtime and the variable doesn't have runtime access, skip it.
        //

        Done = TRUE;
        if ((EfiIsAtRuntime() != FALSE) &&
            ((Entry->Attributes & EFI_VARIABLE_RUNTIME_ACCESS) == 0)) {

            Done = FALSE;
        }
    }

    StringSize = Entry->NameSize;
    Entry = (PEFI_VARIABLE_ENTRY)((UINT8 *)Entry +
                                  sizeof(EFI_VARIABLE_ENTRY) +
                                  StringSize +
                                  Entry->DataSize);

    if (Entry >= EfiVariableEnd) {
        return EFI_NOT_FOUND;
    }

    if (*VariableNameSize < StringSize) {
        *VariableNameSize = StringSize;
        return EFI_BUFFER_TOO_SMALL;
    }

    *VariableNameSize = StringSize;
    EfiCoreCopyMemory(VariableName, (CHAR16 *)(Entry + 1), StringSize);
    EfiCoreCopyMemory(VendorGuid, &(Entry->VendorGuid), sizeof(EFI_GUID));
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiCoreGetVariable (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 *Attributes,
    UINTN *DataSize,
    VOID *Data
    )

/*++

Routine Description:

    This routine returns the value of a variable.

Arguments:

    VariableName - Supplies a pointer to a null-terminated string containing
        the name of the vendor's variable.

    VendorGuid - Supplies a pointer to the unique GUID for the vendor.

    Attributes - Supplies an optional pointer where the attribute mask for the
        variable will be returned.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer. On output, the actual size of the data will be returned.

    Data - Supplies a pointer where the variable value will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the variable was not found.

    EFI_BUFFER_TOO_SMALL if the supplied buffer is not big enough.

    EFI_INVALID_PARAMETER if the variable name, vendor GUID, or data size is
    NULL.

    EFI_DEVICE_ERROR if a hardware error occurred trying to read the variable.

    EFI_SECURITY_VIOLATION if the variable could not be retrieved due to an
    authentication failure.

--*/

{

    PEFI_VARIABLE_ENTRY Entry;
    VOID *InternalData;

    if ((EfiIsAtRuntime() != FALSE) && (Attributes != NULL)) {
        if ((*Attributes & EFI_VARIABLE_RUNTIME_ACCESS) == 0) {
            return EFI_NOT_FOUND;
        }
    }

    Entry = EfipCoreGetVariableEntry(VariableName, VendorGuid, &InternalData);
    if (Entry == NULL) {
        return EFI_NOT_FOUND;
    }

    if (*DataSize < Entry->DataSize) {
        *DataSize = Entry->DataSize;
        return EFI_BUFFER_TOO_SMALL;
    }

    *DataSize = Entry->DataSize;
    if (Attributes != NULL) {
        *Attributes = Entry->Attributes;
    }

    EfiCoreCopyMemory(Data, InternalData, *DataSize);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiCoreQueryVariableInfo (
    UINT32 Attributes,
    UINT64 *MaximumVariableStorageSize,
    UINT64 *RemainingVariableStorageSize,
    UINT64 *MaximumVariableSize
    )

/*++

Routine Description:

    This routine returns information about EFI variables.

Arguments:

    Attributes - Supplies a bitmask of attributes specifying the type of
        variables on which to return information.

    MaximumVariableStorageSize - Supplies a pointer where the maximum size of
        storage space for EFI variables with the given attributes will be
        returned.

    RemainingVariableStorageSize - Supplies a pointer where the remaining size
        of the storage space available for EFI variables associated with the
        attributes specified will be returned.

    MaximumVariableSize - Supplies a pointer where the maximum size of an
        individual variable will be returned on success.

Return Value:

    EFI_SUCCESS if a valid answer was returned.

    EFI_UNSUPPORTED if the attribute is not supported on this platform.

    EFI_INVALID_PARAMETER if an invalid combination of attributes was supplied.

--*/

{

    if (EfiVariableHeader == NULL) {
        return EFI_UNSUPPORTED;
    }

    *MaximumVariableStorageSize = EfiVariableHeader->DataSize;
    *RemainingVariableStorageSize = EfiVariableHeader->FreeSize;
    *MaximumVariableSize = EfiVariableHeader->FreeSize;
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiCoreFlushVariableData (
    VOID
    )

/*++

Routine Description:

    This routine attempts to write variable data out to non-volatile storage.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    if (EfiVariableHeader == NULL) {
        return EFI_NOT_READY;
    }

    return EfipWriteVariableData(EfiVariableHeader);
}

EFI_STATUS
EfipCoreInitializeVariableServices (
    VOID
    )

/*++

Routine Description:

    This routine initialize core variable services.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

{

    EFI_PHYSICAL_ADDRESS Address;
    UINTN FreeOffset;
    PEFI_VARIABLE_HEADER Header;
    EFI_STATUS Status;
    UINTN TotalSize;
    EFI_ALLOCATE_TYPE Type;

    Address = 0;
    if (EfiVariableAllocationAddress != 0) {
        Type = AllocateAddress;
        Address = EfiVariableAllocationAddress;

    } else {
        Type = AllocateAnyPages;
    }

    if (EfiVariableAllocationPageCount == 0) {
        return EFI_UNSUPPORTED;
    }

    Status = EfiAllocatePages(Type,
                              EfiRuntimeServicesData,
                              EfiVariableAllocationPageCount,
                              &Address);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Header = (PEFI_VARIABLE_HEADER)(UINTN)Address;
    TotalSize = EfiVariableAllocationPageCount << EFI_PAGE_SHIFT;
    EfiVariableHeader = Header;
    EfiVariableEnd = (PEFI_VARIABLE_ENTRY)((UINT8 *)Header + TotalSize);
    EfiVariableNextFree = (PEFI_VARIABLE_ENTRY)(Header + 1);

    //
    // Look to see if there's already valid data in this region. If it's dirty,
    // try to write it out right now, as it may have come from a previous boot.
    //

    if (EfipValidateVariableSpace(Header, TotalSize) != FALSE) {
        if ((Header->Flags & EFI_VARIABLE_FLAG_DIRTY) != 0) {
            EfipWriteVariableData(Header);
        }

        //
        // Find the next free entry.
        //

        FreeOffset = Header->DataSize -
                     sizeof(EFI_VARIABLE_HEADER) - Header->FreeSize;

        EfiVariableNextFree =
                   (PEFI_VARIABLE_ENTRY)(((UINT8 *)(Header + 1)) + FreeOffset);

        Status = EFI_SUCCESS;
        goto InitializeVariableServicesEnd;
    }

    //
    // Try to read from non-volatile storage. If it worked and it's valid, then
    // use it.
    //

    Status = EfiPlatformReadNonVolatileData(Header, TotalSize);
    if (!EFI_ERROR(Status)) {
        if (EfipValidateVariableSpace(Header, TotalSize) != FALSE) {

            //
            // The dirty flag really should already be cleared, but clear it
            // anyway.
            //

            if ((Header->Flags & EFI_VARIABLE_FLAG_DIRTY) != 0) {
                Header->Flags &= ~EFI_VARIABLE_FLAG_DIRTY;
                EfiVariablesChanged = TRUE;
            }

            //
            // Find the next free entry.
            //

            FreeOffset = Header->DataSize -
                     sizeof(EFI_VARIABLE_HEADER) - Header->FreeSize;

            EfiVariableNextFree =
                   (PEFI_VARIABLE_ENTRY)(((UINT8 *)(Header + 1)) + FreeOffset);

            Status = EFI_SUCCESS;
            goto InitializeVariableServicesEnd;
        }
    }

    //
    // Initialize the variable area to be empty.
    //

    EfiSetMem(Header, TotalSize, 0);
    Header->Magic = EFI_VARIABLE_HEADER_MAGIC;
    Header->Version = EFI_VARIABLE_HEADER_VERSION;
    Header->Flags = 0;
    Header->DataSize = TotalSize;
    Header->FreeSize = TotalSize - sizeof(EFI_VARIABLE_HEADER);
    Header->HeaderCrc32 = 0;
    Header->DataCrc32 = 0;
    EfiVariablesChanged = TRUE;
    Status = EFI_SUCCESS;

InitializeVariableServicesEnd:

    //
    // If everything worked, publish the variable backend protocol.
    //

    if (!EFI_ERROR(Status)) {
        Status = EfiInstallMultipleProtocolInterfaces(
                                               &EfiVariableBackendHandle,
                                               &EfiVariableBackendProtocolGuid,
                                               &EfiVariableBackendProtocol,
                                               NULL);
    }

    return Status;
}

VOID
EfipCoreVariableHandleExitBootServices (
    VOID
    )

/*++

Routine Description:

    This routine is called when leaving boot services.

Arguments:

    None.

Return Value:

    None.

--*/

{

    EfipWriteVariableData(EfiVariableHeader);
    return;
}

VOID
EfipCoreVariableHandleVirtualAddressChange (
    VOID
    )

/*++

Routine Description:

    This routine is called to change from physical to virtual mode.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Not so bad, eh?
    //

    EfiConvertPointer(0, (VOID **)&EfiVariableHeader);
    EfiConvertPointer(0, (VOID **)&EfiVariableNextFree);
    EfiConvertPointer(0, (VOID **)&EfiVariableEnd);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfiVariableBackendSetData (
    EFI_VARIABLE_BACKEND_PROTOCOL *This,
    VOID *Data,
    UINTN DataSize,
    BOOLEAN Replace
    )

/*++

Routine Description:

    This routine adds or replaces the current EFI variables with the given
    serialized variable buffer.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Data - Supplies a pointer to the serialized EFI variable data.

    DataSize - Supplies the size of the data buffer in bytes.

    Replace - Supplies a boolean indicating if the contents of this buffer
        should be merged with the current variables (FALSE) or if all current
        variables should be destroyed before adding these (TRUE).

Return Value:

    EFI status code.

--*/

{

    PEFI_VARIABLE_ENTRY End;
    PEFI_VARIABLE_ENTRY Entry;
    PEFI_VARIABLE_HEADER Header;
    BOOLEAN Valid;
    VOID *VariableData;

    Valid = EfipValidateVariableSpace(Data, DataSize);
    if (Valid == FALSE) {
        return EFI_COMPROMISED_DATA;
    }

    if (Replace != FALSE) {
        EfiVariableHeader->FreeSize = EfiVariableHeader->DataSize;
        EfiVariablesChanged = TRUE;
        EfiVariableHeader->Flags |= EFI_VARIABLE_FLAG_DIRTY;
        EfiVariableNextFree = (PEFI_VARIABLE_ENTRY)(EfiVariableHeader + 1);
    }

    Header = Data;
    Entry = (PEFI_VARIABLE_ENTRY)(Header + 1);
    End = (PEFI_VARIABLE_ENTRY)((UINTN)Entry +
                                (Header->DataSize - Header->FreeSize));

    while (Entry + 1 <= End) {
        if ((Entry->NameSize == 0) || (Entry->DataSize == 0)) {
            break;
        }

        VariableData = (UINT8 *)(Entry + 1) + Entry->NameSize;
        if (VariableData + Entry->DataSize > (VOID *)End) {
            break;
        }

        EfiCoreSetVariable((CHAR16 *)(Entry + 1),
                           &(Entry->VendorGuid),
                           Entry->Attributes,
                           Entry->DataSize,
                           VariableData);

        Entry = (PEFI_VARIABLE_ENTRY)(VariableData + Entry->DataSize);
        Entry = ALIGN_POINTER(Entry, 4);
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiVariableBackendGetData (
    EFI_VARIABLE_BACKEND_PROTOCOL *This,
    VOID **Data,
    UINTN *DataSize
    )

/*++

Routine Description:

    This routine returns a serialized form of the given variables. The caller
    must ensure no variable changes are made while using this buffer.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Data - Supplies a pointer where a pointer will be returned to the
        serialized variable data. This data may be live, so the caller may not
        modify it.

    DataSize - Supplies a pointer where the size of the data will be returned
        on success.

Return Value:

    EFI status code.

--*/

{

    if ((Data == NULL) || (DataSize == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    if ((EfiVariableHeader == NULL) || (EfiVariableEnd == NULL)) {
        return EFI_NOT_READY;
    }

    EfipSetVariableDataCrc(EfiVariableHeader);
    *Data = EfiVariableHeader;
    *DataSize = (UINTN)EfiVariableEnd - (UINTN)EfiVariableHeader;
    return EFI_SUCCESS;
}

BOOLEAN
EfipValidateVariableSpace (
    PEFI_VARIABLE_HEADER Header,
    UINTN TotalSize
    )

/*++

Routine Description:

    This routine validates the EFI variable region.

Arguments:

    Header - Supplies a pointer to the header to validate.

    TotalSize - Supplies the total size in bytes of the variable region.

Return Value:

    TRUE if the variable region is valid.

    FALSE if the variable region is not valid.

--*/

{

    UINT32 ComputedCrc;
    EFI_STATUS Status;

    if (TotalSize < sizeof(EFI_VARIABLE_HEADER)) {
        return FALSE;
    }

    if ((Header->Magic != EFI_VARIABLE_HEADER_MAGIC) ||
        (Header->Version != EFI_VARIABLE_HEADER_VERSION)) {

        return FALSE;
    }

    //
    // Compute the header CRC before examining the length.
    //

    Status = EfiCalculateCrc32(Header,
                               EFI_VARIABLE_HEADER_CRC_SIZE,
                               &ComputedCrc);

    if ((EFI_ERROR(Status)) || (ComputedCrc != Header->HeaderCrc32)) {
        return FALSE;
    }

    //
    // Compute the CRC of the data.
    //

    if (Header->DataSize > TotalSize) {
        return FALSE;
    }

    if (Header->FreeSize > Header->DataSize) {
        return FALSE;
    }

    EfiCalculateCrc32(Header + 1,
                      Header->DataSize - sizeof(EFI_VARIABLE_HEADER),
                      &ComputedCrc);

    if (Header->DataCrc32 != ComputedCrc) {
        return FALSE;
    }

    return TRUE;
}

EFI_STATUS
EfipWriteVariableData (
    PEFI_VARIABLE_HEADER Header
    )

/*++

Routine Description:

    This routine attempts to write the variable data to a non-volatile platform
    area.

Arguments:

    Header - Supplies a pointer to the data.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    //
    // Mark the variable region as clean for the flush out to storage.
    //

    if ((Header->Flags & EFI_VARIABLE_FLAG_DIRTY) != 0) {
        Header->Flags &= ~EFI_VARIABLE_FLAG_DIRTY;
        EfiVariablesChanged = TRUE;
    }

    //
    // If nothing has changed then return successfully.
    //

    if (EfiVariablesChanged == FALSE) {
        return EFI_SUCCESS;
    }

    //
    // Recompute the CRCs if anything has changed.
    //

    Status = EfipSetVariableDataCrc(Header);
    if (EFI_ERROR(Status)) {
        Header->Flags |= EFI_VARIABLE_FLAG_DIRTY;
        return Status;
    }

    Status = EfiPlatformWriteNonVolatileData(Header, Header->DataSize);
    if (EFI_ERROR(Status)) {
        Header->Flags |= EFI_VARIABLE_FLAG_DIRTY;

    } else {
        EfiVariablesChanged = FALSE;
    }

    return Status;
}

EFI_STATUS
EfipSetVariableDataCrc (
    PEFI_VARIABLE_HEADER Header
    )

/*++

Routine Description:

    This routine writes the CRC of the current variable data.

Arguments:

    Header - Supplies a pointer to the data.

Return Value:

    EFI status code.

--*/

{

    UINT32 OriginalCrc;
    EFI_STATUS Status;

    //
    // Recompute the CRCs if anything has changed.
    //

    OriginalCrc = Header->HeaderCrc32;
    Header->HeaderCrc32 = 0;
    Status = EfiCoreCalculateCrc32(Header,
                                   EFI_VARIABLE_HEADER_CRC_SIZE,
                                   &(Header->HeaderCrc32));

    if (EFI_ERROR(Status)) {
        Header->HeaderCrc32 = OriginalCrc;
        return Status;
    }

    Header->DataCrc32 = 0;
    Status = EfiCoreCalculateCrc32(
                                Header + 1,
                                Header->DataSize - sizeof(EFI_VARIABLE_HEADER),
                                &(Header->DataCrc32));

    return Status;
}

PEFI_VARIABLE_ENTRY
EfipCoreGetVariableEntry (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    VOID **Data
    )

/*++

Routine Description:

    This routine returns the variable entry corresponding with the given name
    and vendor GUID.

Arguments:

    VariableName - Supplies a pointer to the variable name to find.

    VendorGuid - Supplies a pointer to the vendor GUID to find.

    Data - Supplies a pointer where the variable data pointer will be
        returned.

Return Value:

    Returns a pointer to the variable entry on success.

    NULL if not found.

--*/

{

    INTN CompareResult;
    PEFI_VARIABLE_ENTRY Entry;
    UINTN Size;

    //
    // The first entry is a null terminated string.
    //

    Entry = (PEFI_VARIABLE_ENTRY)(EfiVariableHeader + 1);
    if (*VariableName == L'\0') {
        if (Entry == EfiVariableNextFree) {
            return NULL;
        }

        return Entry;
    }

    while (Entry + 1 <= EfiVariableNextFree) {
        if ((Entry->DataSize != 0) &&
            (EfiCoreCompareGuids(VendorGuid, &(Entry->VendorGuid)) != FALSE)) {

            CompareResult = EfiCoreCompareMemory(VariableName,
                                                 (CHAR16 *)(Entry + 1),
                                                 Entry->NameSize);

            if (CompareResult == 0) {
                *Data = (VOID *)((UINT8 *)Entry +
                                 sizeof(EFI_VARIABLE_ENTRY) +
                                 Entry->NameSize);

                return Entry;
            }
        }

        Size = sizeof(EFI_VARIABLE_ENTRY) + Entry->NameSize + Entry->DataSize;
        Size = ALIGN_VALUE(Size, 4);
        Entry = (PEFI_VARIABLE_ENTRY)((UINT8 *)Entry + Size);
    }

    return NULL;
}

VOID
EfipCoreDeleteVariableEntry (
    PEFI_VARIABLE_ENTRY Entry
    )

/*++

Routine Description:

    This routine deletes the given variable entry.

Arguments:

    Entry - Supplies a pointer to the variable entry to delete.

Return Value:

    None.

--*/

{

    EFI_TPL CurrentTpl;
    UINT8 *Data;
    UINTN Size;

    CurrentTpl = TPL_HIGH_LEVEL;
    Size = sizeof(EFI_VARIABLE_ENTRY) + Entry->NameSize + Entry->DataSize;
    Size = ALIGN_VALUE(Size, 4);
    Data = ((UINT8 *)Entry) + Size;
    EfiCoreCopyMemory(Entry, Data, (UINTN)EfiVariableNextFree - (UINTN)Data);
    if (EfiIsAtRuntime() == FALSE) {
        CurrentTpl = EfiRaiseTPL(TPL_HIGH_LEVEL);
    }

    EfiVariableHeader->FreeSize += Size;
    EfiVariableNextFree =
                  (PEFI_VARIABLE_ENTRY)(((UINT8 *)EfiVariableNextFree) - Size);

    EfiVariablesChanged = TRUE;
    EfiVariableHeader->Flags |= EFI_VARIABLE_FLAG_DIRTY;
    if (EfiIsAtRuntime() == FALSE) {
        EfiRestoreTPL(CurrentTpl);
    }

    return;
}

PEFI_VARIABLE_ENTRY
EfipCoreAddVariableEntry (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 Attributes,
    UINTN DataSize,
    VOID *Data
    )

/*++

Routine Description:

    This routine adds a new variable entry.

Arguments:

    VariableName - Supplies a pointer to a null-terminated string containing
        the name of the vendor's variable. Each variable name is unique for a
        particular vendor GUID. A variable name must be at least one character
        in length.

    VendorGuid - Supplies a pointer to the unique GUID for the vendor.

    Attributes - Supplies the attributes for this variable. See EFI_VARIABLE_*
        definitions.

    DataSize - Supplies the size of the data buffer. The size must not be zero.

    Data - Supplies the contents of the variable.

Return Value:

    Returns a pointer to the new entry on success.

    NULL on failure.

--*/

{

    PEFI_VARIABLE_ENTRY Entry;
    EFI_TPL OldTpl;
    UINTN Size;
    UINTN StringSize;

    StringSize = (EfiCoreStringLength(VariableName) + 1) * sizeof(CHAR16);
    Size = sizeof(EFI_VARIABLE_ENTRY) + StringSize + DataSize;
    Size = ALIGN_VALUE(Size, 4);
    if ((PEFI_VARIABLE_ENTRY)(((UINT8 *)EfiVariableNextFree) + Size) >
        EfiVariableEnd) {

        return NULL;
    }

    OldTpl = TPL_HIGH_LEVEL;
    if (EfiIsAtRuntime() == FALSE) {
        OldTpl = EfiRaiseTPL(TPL_HIGH_LEVEL);
    }

    Entry = EfiVariableNextFree;
    EfiCoreCopyMemory(&(Entry->VendorGuid), VendorGuid, sizeof(EFI_GUID));
    Entry->Attributes = Attributes;
    Entry->DataSize = DataSize;
    Entry->NameSize = StringSize;
    EfiCoreCopyMemory(Entry + 1, VariableName, StringSize);
    EfiCoreCopyMemory(((UINT8 *)(Entry + 1)) + Entry->NameSize, Data, DataSize);
    EfiVariableNextFree = (PEFI_VARIABLE_ENTRY)(((UINT8 *)Entry) + Size);
    EfiVariableHeader->FreeSize -= Size;
    EfiVariableHeader->Flags |= EFI_VARIABLE_FLAG_DIRTY;
    EfiVariablesChanged = TRUE;
    if (EfiIsAtRuntime() == FALSE) {
        EfiRestoreTPL(OldTpl);
    }

    return Entry;
}

