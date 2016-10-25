/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bootlib.h

Abstract:

    This header contains definitions for the Boot Library.

Author:

    Evan Green 19-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines an open volume in the boot environment.

Members:

    Parameters - Stores the block device's settings.

    DiskHandle - Stores a handle to device provided by the firmware.

    FileSystemHandle - Stores the handle returned when the file system mounted
        the device.

--*/

typedef struct _BOOT_VOLUME {
    BLOCK_DEVICE_PARAMETERS Parameters;
    HANDLE DiskHandle;
    PVOID FileSystemHandle;
} BOOT_VOLUME, *PBOOT_VOLUME;

//
// -------------------------------------------------------------------- Globals
//

extern MEMORY_DESCRIPTOR_LIST BoMemoryMap;

extern ULONGLONG BoEncodedVersion;
extern ULONGLONG BoVersionSerial;
extern ULONGLONG BoBuildTime;
extern PSTR BoBuildString;
extern PSTR BoProductName;

//
// Store a pointer to an enumerated firmware debug device.
//

extern PDEBUG_DEVICE_DESCRIPTION BoFirmwareDebugDevice;

//
// -------------------------------------------------------- Function Prototypes
//

//
// Functions implemented by the application called by the boot library
//

PVOID
BoExpandHeap (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    );

/*++

Routine Description:

    This routine is called when the heap wants to expand and get more space.

Arguments:

    Heap - Supplies a pointer to the heap to allocate from.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies a 32-bit tag to associate with this allocation for debugging
        purposes. These are usually four ASCII characters so as to stand out
        when a poor developer is looking at a raw memory dump. It could also be
        a return address.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

//
// Initialization functions
//

VOID
BoInitializeProcessor (
    VOID
    );

/*++

Routine Description:

    This routine performs very early architecture specific initialization. It
    runs before the debugger is online.

Arguments:

    None.

Return Value:

    None.

--*/

KSTATUS
BoHlBootInitialize (
    PDEBUG_DEVICE_DESCRIPTION *DebugDevice,
    PHARDWARE_MODULE_GET_ACPI_TABLE GetAcpiTableFunction
    );

/*++

Routine Description:

    This routine initializes the boot hardware library.

Arguments:

    DebugDevice - Supplies a pointer where a pointer to the debug device
        description will be returned on success.

    GetAcpiTableFunction - Supplies an optional pointer to a function used to
        get ACPI tables. If not supplied a default hardware module service
        will be used that always returns NULL.

Return Value:

    Status code.

--*/

VOID
BoHlTestUsbDebugInterface (
    VOID
    );

/*++

Routine Description:

    This routine runs the interface test on a USB debug interface if debugging
    the USB transport itself.

Arguments:

    None.

Return Value:

    None.

--*/

//
// Memory Functions
//

PVOID
BoAllocateMemory (
    UINTN Size
    );

/*++

Routine Description:

    This routine allocates memory in the loader. This memory is marked as
    loader temporary, meaning it will get unmapped and reclaimed during kernel
    initialization.

Arguments:

    Size - Supplies the size of the desired allocation, in bytes.

Return Value:

    Returns a physical pointer to the allocation on success, or NULL on failure.

--*/

VOID
BoFreeMemory (
    PVOID Allocation
    );

/*++

Routine Description:

    This routine frees memory allocated in the boot environment.
Arguments:

    Allocation - Supplies a pointer to the memory allocation being freed.

Return Value:

    None.

--*/

//
// File I/O functions
//

KSTATUS
BoOpenBootVolume (
    ULONG BootDriveNumber,
    ULONGLONG PartitionOffset,
    PBOOT_ENTRY BootEntry,
    PBOOT_VOLUME *VolumeHandle
    );

/*++

Routine Description:

    This routine opens a handle to the boot volume device, which is the device
    this boot application was loaded from.

Arguments:

    BootDriveNumber - Supplies the drive number of the boot device, for PC/AT
        systems.

    PartitionOffset - Supplies the offset in sectors to the start of the boot
        partition, for PC/AT systems.

    BootEntry - Supplies an optional pointer to the boot entry, for EFI systems.

    VolumeHandle - Supplies a pointer where a handle to the open volume will
        be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
BoCloseVolume (
    PBOOT_VOLUME VolumeHandle
    );

/*++

Routine Description:

    This routine closes a disk handle.

Arguments:

    VolumeHandle - Supplies the volume handle returned when the volume was
        opened.

Return Value:

    Status code.

--*/

KSTATUS
BoOpenVolume (
    UCHAR PartitionId[FIRMWARE_PARTITION_ID_SIZE],
    PBOOT_VOLUME *Volume
    );

/*++

Routine Description:

    This routine closes a disk handle.

Arguments:

    PartitionId - Supplies the ID of the partition to open.

    Volume - Supplies a pointer where a handle to the open volume will
        be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
BoLookupPath (
    PBOOT_VOLUME Volume,
    PFILE_ID StartingDirectory,
    PCSTR Path,
    PFILE_PROPERTIES FileProperties
    );

/*++

Routine Description:

    This routine attempts to look up the given file path.

Arguments:

    Volume - Supplies a pointer to the volume token.

    StartingDirectory - Supplies an optional pointer to a file ID containing
        the directory to start path traversal from. If NULL, path lookup will
        start with the root of the volume.

    Path - Supplies a pointer to the path string to look up.

    FileProperties - Supplies a pointer where the properties for the file will
        be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_PATH_NOT_FOUND if the given file path does not exist.

    Other error codes on other failures.

--*/

KSTATUS
BoLoadFile (
    PBOOT_VOLUME Volume,
    PFILE_ID Directory,
    PSTR FileName,
    PVOID *FilePhysical,
    PUINTN FileSize,
    PULONGLONG ModificationDate
    );

/*++

Routine Description:

    This routine loads a file from disk into memory.

Arguments:

    Volume - Supplies a pointer to the mounted volume to read the file from.

    Directory - Supplies an optional pointer to the ID of the directory to
        start path traversal from. If NULL, the root of the volume will be used.

    FileName - Supplies the name of the file to load.

    FilePhysical - Supplies a pointer where the file buffer's physical address
        will be returned. This routine will allocate the buffer to hold the
        file. If this parameter is NULL, the status code will reflect whether
        or not the file could be opened, but the file contents will not be
        loaded.

    FileSize - Supplies a pointer where the size of the file in bytes will be
        returned.

    ModificationDate - Supplies an optional pointer where the modification
        date of the file will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
BoStoreFile (
    PBOOT_VOLUME Volume,
    FILE_ID Directory,
    PSTR FileName,
    ULONG FileNameLength,
    PVOID FilePhysical,
    UINTN FileSize,
    ULONGLONG ModificationDate
    );

/*++

Routine Description:

    This routine stores a file buffer to disk.

Arguments:

    Volume - Supplies a pointer to the mounted volume to read the file from.

    Directory - Supplies the file ID of the directory the file resides in.

    FileName - Supplies the name of the file to store.

    FileNameLength - Supplies the length of the file name buffer in bytes,
        including the null terminator.

    FilePhysical - Supplies a pointer to the buffer containing the file
        contents.

    FileSize - Supplies the size of the file buffer in bytes. The file will be
        truncated to this size if it previously existed and was larger.

    ModificationDate - Supplies the modification date to set.

Return Value:

    Status code.

--*/

