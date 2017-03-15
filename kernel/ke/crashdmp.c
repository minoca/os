/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    crashdmp.c

Abstract:

    This module implements support for collecting and writing out crash dump
    data in the unfortunate event of a fatal system error.

Author:

    Chris Stevens 26-Aug-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/crashdmp.h>
#include <minoca/intrface/disk.h>
#include "kep.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the saved context for a crash dump file.

Members:

    ListEntry - Stores pointers to the next and previous crash dump files.

    FileHandle - Stores a handle to the open file.

    FileSize - Stores the size of the file, in bytes.

    BlockIoContext - Stores the I/O context necessary to perform block-level
        writes to the crash dump file.

    Device - Stores a pointer to the device the crash dump file ultimately
        writes to.

    DiskInterface - Stores a pointer to the disk interface.

--*/

typedef struct _CRASH_DUMP_FILE {
    LIST_ENTRY ListEntry;
    PIO_HANDLE FileHandle;
    ULONGLONG FileSize;
    FILE_BLOCK_IO_CONTEXT BlockIoContext;
    PDEVICE Device;
    PDISK_INTERFACE DiskInterface;
} CRASH_DUMP_FILE, *PCRASH_DUMP_FILE;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KepDestroyCrashDumpFile (
    PCRASH_DUMP_FILE CrashDumpFile
    );

USHORT
KepCalculateChecksum (
    PVOID Data,
    ULONG DataSize
    );

VOID
KepCrashDumpDiskInterfaceNotificationCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a list of available crash dump files and a lock to protect them.
//

KSPIN_LOCK KeCrashDumpListLock;
LIST_ENTRY KeCrashDumpListHead;

//
// Store the UUID of the disk interface.
//

UUID KeDiskInterfaceUuid = UUID_DISK_INTERFACE;

//
// Store a scratch I/O buffer used to write portions of the crash dump file.
//

PIO_BUFFER KeCrashDumpScratchBuffer;

//
// Store a boolean indicating whether to write all crash dump files or just
// stop at the first successfully written one.
//

BOOL KeWriteAllCrashDumpFiles;

//
// ------------------------------------------------------------------ Functions
//

VOID
KeRegisterCrashDumpFile (
    HANDLE Handle,
    BOOL Register
    )

/*++

Routine Description:

    This routine registers a file for use as a crash dump file.

Arguments:

    Handle - Supplies a handle to the page file to register.

    Register - Supplies a boolean indicating if the page file is registering
        (TRUE) or de-registering (FALSE).

Return Value:

    None.

--*/

{

    PFILE_BLOCK_INFORMATION BlockInformation;
    PFILE_BLOCK_IO_CONTEXT BlockIoContext;
    PCRASH_DUMP_FILE CrashDumpFile;
    PLIST_ENTRY CurrentEntry;
    PDEVICE DiskDevice;
    PDISK_INTERFACE DiskInterface;
    PVOID DiskToken;
    ULONGLONG FileSize;
    PIO_HANDLE IoHandle;
    PCRASH_DUMP_FILE NewCrashDumpFile;
    PCRASH_DUMP_FILE OldCrashDumpFile;
    KSTATUS Status;
    PDEVICE Volume;

    BlockInformation = NULL;
    IoHandle = (PIO_HANDLE)Handle;
    NewCrashDumpFile = NULL;
    OldCrashDumpFile = NULL;

    //
    // If registering the page file, then look up necessary information and
    // create the crash dump file.
    //

    if (Register != FALSE) {
        Status = IoGetDevice(IoHandle, &Volume);
        if (!KSUCCESS(Status)) {
            goto RegisterCrashDumpFileEnd;
        }

        DiskDevice = IoGetDiskDevice(Volume);

        ASSERT(DiskDevice != NULL);

        //
        // Query the file system to get a list of device offsets and sizes for
        // the location of the page file.
        //

        Status = IoGetFileBlockInformation(IoHandle, &BlockInformation);
        if (!KSUCCESS(Status)) {
            goto RegisterCrashDumpFileEnd;
        }

        //
        // Create the crash dump file structure.
        //

        NewCrashDumpFile = MmAllocateNonPagedPool(sizeof(CRASH_DUMP_FILE),
                                                  KE_ALLOCATION_TAG);

        if (NewCrashDumpFile == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto RegisterCrashDumpFileEnd;
        }

        RtlZeroMemory(NewCrashDumpFile, sizeof(CRASH_DUMP_FILE));
        NewCrashDumpFile->FileHandle = IoHandle;
        Status = IoGetFileSize(IoHandle, &FileSize);
        if (!KSUCCESS(Status)) {
            goto RegisterCrashDumpFileEnd;
        }

        NewCrashDumpFile->FileSize = FileSize;
        NewCrashDumpFile->Device = DiskDevice;
        BlockIoContext = &(NewCrashDumpFile->BlockIoContext);
        BlockIoContext->FileBlockInformation = BlockInformation;
        BlockInformation = NULL;
        Status = IoRegisterForInterfaceNotifications(
                                 &KeDiskInterfaceUuid,
                                 KepCrashDumpDiskInterfaceNotificationCallback,
                                 NewCrashDumpFile->Device,
                                 NewCrashDumpFile,
                                 TRUE);

        if (!KSUCCESS(Status)) {
            goto RegisterCrashDumpFileEnd;
        }

        //
        // If the interface wasn't immediately filled in, then fail.
        //

        DiskInterface = NewCrashDumpFile->DiskInterface;
        if (DiskInterface == NULL) {
            Status = STATUS_NOT_SUPPORTED;
            goto RegisterCrashDumpFileEnd;
        }

        //
        // The crash dump file is ready to go. Give the disk a heads up that
        // it's block I/O routines may be called into action. This gives it a
        // chance to allocate any memory it may need later.
        //

        if (DiskInterface->BlockIoInitialize != NULL) {
            DiskToken = DiskInterface->DiskToken;
            Status = DiskInterface->BlockIoInitialize(DiskToken);
            if (!KSUCCESS(Status)) {
                goto RegisterCrashDumpFileEnd;
            }
        }
    }

    //
    // Search for a crash file with the same handle.
    //

    KeAcquireSpinLock(&KeCrashDumpListLock);
    CurrentEntry = KeCrashDumpListHead.Next;
    while (CurrentEntry != &KeCrashDumpListHead) {
        CrashDumpFile = LIST_VALUE(CurrentEntry, CRASH_DUMP_FILE, ListEntry);
        if (CrashDumpFile->FileHandle == IoHandle) {
            OldCrashDumpFile = CrashDumpFile;
            LIST_REMOVE(CurrentEntry);
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (NewCrashDumpFile != NULL) {
        INSERT_BEFORE(&(NewCrashDumpFile->ListEntry), &KeCrashDumpListHead);
    }

    KeReleaseSpinLock(&KeCrashDumpListLock);
    if (OldCrashDumpFile != NULL) {
        KepDestroyCrashDumpFile(CrashDumpFile);
    }

    Status = STATUS_SUCCESS;

RegisterCrashDumpFileEnd:
    if (!KSUCCESS(Status)) {
        if (NewCrashDumpFile != NULL) {
            KepDestroyCrashDumpFile(NewCrashDumpFile);
        }

        if (BlockInformation != NULL) {
            IoDestroyFileBlockInformation(BlockInformation);
        }
    }

    return;
}

KSTATUS
KepInitializeCrashDumpSupport (
    VOID
    )

/*++

Routine Description:

    This routine initializes system crash dump support.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONG IoBufferFlags;
    ULONG PageSize;

    INITIALIZE_LIST_HEAD(&KeCrashDumpListHead);
    KeInitializeSpinLock(&KeCrashDumpListLock);
    PageSize = MmPageSize();

    ASSERT(PageSize >= sizeof(CRASH_DUMP_HEADER));

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    KeCrashDumpScratchBuffer = MmAllocateNonPagedIoBuffer(0,
                                                          MAX_ULONGLONG,
                                                          0,
                                                          PageSize,
                                                          IoBufferFlags);

    if (KeCrashDumpScratchBuffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return STATUS_SUCCESS;
}

KSTATUS
KepWriteCrashDump (
    ULONG CrashCode,
    ULONGLONG Parameter1,
    ULONGLONG Parameter2,
    ULONGLONG Parameter3,
    ULONGLONG Parameter4
    )

/*++

Routine Description:

    This routine writes crash dump data to disk.

Arguments:

    CrashCode - Supplies the reason for the system crash.

    Parameter1 - Supplies an optional parameter regarding the crash.

    Parameter2 - Supplies an optional parameter regarding the crash.

    Parameter3 - Supplies an optional parameter regarding the crash.

    Parameter4 - Supplies an optional parameter regarding the crash.

Return Value:

    Status code.

--*/

{

    PFILE_BLOCK_IO_CONTEXT BlockIoContext;
    PDISK_BLOCK_IO_RESET BlockIoReset;
    PVOID Buffer;
    ULONG BufferSize;
    UINTN BytesCompleted;
    USHORT Checksum;
    PCRASH_DUMP_FILE CrashFile;
    PLIST_ENTRY CurrentEntry;
    PCRASH_DUMP_HEADER Header;
    KSTATUS Status;
    SYSTEM_VERSION_INFORMATION VersionInformation;

    if (LIST_EMPTY(&KeCrashDumpListHead)) {
        RtlDebugPrint("No registered crash dump files.\n");
    }

    Status = STATUS_SUCCESS;
    CurrentEntry = KeCrashDumpListHead.Next;
    while (CurrentEntry != &KeCrashDumpListHead) {
        CrashFile = LIST_VALUE(CurrentEntry, CRASH_DUMP_FILE, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // If there's no interface for this one, skip it.
        //

        if (CrashFile->DiskInterface == NULL) {
            RtlDebugPrint("Skipping dump file 0x%x without interface.\n",
                          CrashFile);

            continue;
        }

        //
        // The file should be big enough for the minimal dump.
        //

        ASSERT(CrashFile->FileSize >= sizeof(CRASH_DUMP_HEADER));

        //
        // Reset the disk for the crash dump, giving the device a heads up that
        // writes are about to come in.
        //

        BlockIoContext = &(CrashFile->BlockIoContext);
        BlockIoReset = BlockIoContext->BlockIoReset;
        if (BlockIoReset != NULL) {
            Status = BlockIoReset(CrashFile->BlockIoContext.DiskToken);
            if (!KSUCCESS(Status)) {
                goto WriteCrashDumpEnd;
            }
        }

        //
        // Copy the crash dump header to the scratch buffer.
        //

        ASSERT(KeCrashDumpScratchBuffer->FragmentCount == 1);
        ASSERT(KeCrashDumpScratchBuffer->Fragment[0].VirtualAddress != NULL);

        Header = KeCrashDumpScratchBuffer->Fragment[0].VirtualAddress;
        RtlZeroMemory(Header, sizeof(CRASH_DUMP_HEADER));
        Header->Signature = CRASH_DUMP_SIGNATURE;
        Header->Type = CrashDumpMinimal;
        Header->DumpSize = sizeof(CRASH_DUMP_HEADER);
        Header->CrashCode = CrashCode;
        Header->Parameter1 = Parameter1;
        Header->Parameter2 = Parameter2;
        Header->Parameter3 = Parameter3;
        Header->Parameter4 = Parameter4;

        //
        // Copy the system version information to the header if available.
        //

        Buffer = (PVOID)(Header + 1);
        BufferSize = KeCrashDumpScratchBuffer->Fragment[0].Size -
                     sizeof(CRASH_DUMP_HEADER);

        Status = KeGetSystemVersion(&VersionInformation, Buffer, &BufferSize);
        if (KSUCCESS(Status)) {
            Header->MajorVersion = VersionInformation.MajorVersion;
            Header->MinorVersion = VersionInformation.MinorVersion;
            Header->Revision = VersionInformation.Revision;
            Header->SerialVersion = VersionInformation.SerialVersion;
            Header->ReleaseLevel = VersionInformation.ReleaseLevel;
            Header->DebugLevel = VersionInformation.DebugLevel;
            if (VersionInformation.ProductName != NULL) {
                Header->ProductNameOffset =
                         (PVOID)VersionInformation.ProductName - (PVOID)Header;
            }

            if (VersionInformation.BuildString != NULL) {
                Header->BuildStringOffset =
                         (PVOID)VersionInformation.BuildString - (PVOID)Header;
            }

            RtlCopyMemory(&(Header->BuildTime),
                          &(VersionInformation.BuildTime),
                          sizeof(SYSTEM_TIME));

            Header->DumpSize += BufferSize;
        }

        //
        // Calculate the header's checksum. Do not include the product and
        // build strings as they are outside the header.
        //

        Checksum = KepCalculateChecksum(Header, sizeof(CRASH_DUMP_HEADER));
        Header->HeaderChecksum = Checksum;

        //
        // Write the header out to the file.
        //

        Status = IoWriteFileBlocks(&(CrashFile->BlockIoContext),
                                   KeCrashDumpScratchBuffer,
                                   0,
                                   (UINTN)Header->DumpSize,
                                   &BytesCompleted);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Failed to write crash dump to file 0x%x: %d\n",
                          CrashFile,
                          Status);

            continue;
        }

        //
        // One crash dump file was successfully written. If that's all that's
        // requested, stop now.
        //

        if (KeWriteAllCrashDumpFiles == FALSE) {
            break;
        }
    }

WriteCrashDumpEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KepCrashDumpDiskInterfaceNotificationCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    )

/*++

Routine Description:

    This routine is called when the disk interface associated with a crash
    dump file appears or disappears.

Arguments:

    Context - Supplies the caller's context pointer, supplied when the caller
        requested interface notifications.

    Device - Supplies a pointer to the device exposing or deleting the
        interface.

    InterfaceBuffer - Supplies a pointer to the interface buffer of the
        interface.

    InterfaceBufferSize - Supplies the buffer size.

    Arrival - Supplies TRUE if a new interface is arriving, or FALSE if an
        interface is departing.

Return Value:

    None.

--*/

{

    PFILE_BLOCK_IO_CONTEXT BlockIoContext;
    PDISK_INTERFACE DiskInterface;
    PCRASH_DUMP_FILE DumpFile;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    DumpFile = Context;
    BlockIoContext = &(DumpFile->BlockIoContext);
    if (Arrival != FALSE) {
        DiskInterface = InterfaceBuffer;
        if ((InterfaceBufferSize < sizeof(DISK_INTERFACE)) ||
            (DiskInterface->Version < DISK_INTERFACE_VERSION)) {

            return;
        }

        //
        // It's not expected that the device would expose multiple disk
        // interfaces.
        //

        ASSERT(DumpFile->DiskInterface == NULL);

        DumpFile->DiskInterface = InterfaceBuffer;
        BlockIoContext->DiskToken = DiskInterface->DiskToken;
        BlockIoContext->BlockSize = DiskInterface->BlockSize;
        BlockIoContext->BlockCount = DiskInterface->BlockCount;
        BlockIoContext->BlockIoReset = DiskInterface->BlockIoReset;
        BlockIoContext->BlockIoRead = DiskInterface->BlockIoRead;
        BlockIoContext->BlockIoWrite = DiskInterface->BlockIoWrite;

    //
    // The interface is disappearing.
    //

    } else {
        DumpFile->DiskInterface = NULL;
        BlockIoContext->BlockIoRead = NULL;
        BlockIoContext->BlockIoWrite = NULL;
        BlockIoContext->BlockIoReset = NULL;
    }

    return;
}

VOID
KepDestroyCrashDumpFile (
    PCRASH_DUMP_FILE CrashDumpFile
    )

/*++

Routine Description:

    This routine destroys a crash dump file.

Arguments:

    CrashDumpFile - Supplies a pointer to the crash dump file to destroy.

Return Value:

    None.

--*/

{

    PFILE_BLOCK_IO_CONTEXT BlockIoContext;
    KSTATUS Status;

    BlockIoContext = &(CrashDumpFile->BlockIoContext);
    if (BlockIoContext->FileBlockInformation != NULL) {
        IoDestroyFileBlockInformation(BlockIoContext->FileBlockInformation);
    }

    Status = IoUnregisterForInterfaceNotifications(
                                 &KeDiskInterfaceUuid,
                                 KepCrashDumpDiskInterfaceNotificationCallback,
                                 CrashDumpFile->Device,
                                 CrashDumpFile);

    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

        return;
    }

    MmFreeNonPagedPool(CrashDumpFile);
    return;
}

USHORT
KepCalculateChecksum (
    PVOID Data,
    ULONG DataSize
    )

/*++

Routine Description:

    This routine calculates the one's compliment checksum of a data buffer.

Arguments:

    Data - Supplies a pointer to the data on which to calculate the checksum.

    DataSize - Supplies the size of the data, in bytes.

Return Value:

    Returns thee one's compliment checksum of the data.

--*/

{

    ULONG ShortIndex;
    PUSHORT ShortPointer;
    ULONG Sum;

    Sum = 0;

    //
    // Now checksum the actual header and data.
    //

    ShortPointer = (PUSHORT)Data;
    for (ShortIndex = 0;
         ShortIndex < DataSize / sizeof(USHORT);
         ShortIndex += 1) {

        Sum += ShortPointer[ShortIndex];
    }

    //
    // If the data size is odd, then grab the last byte.
    //

    if ((DataSize & 0x1) != 0) {
        Sum += *((PUCHAR)&(ShortPointer[ShortIndex]));
    }

    //
    // With one's complement arithmetic, every time a wraparound occurs the
    // carry must be added back in on the right (to skip over "negative zero").
    // Perform all these carries at once by adding in the high word. That
    // addition itself can also cause a wraparound, which is why the while loop
    // is there.
    //

    while ((Sum >> (sizeof(USHORT) * BITS_PER_BYTE)) != 0) {
        Sum = (Sum & 0xFFFF) + (Sum >> (sizeof(USHORT) * BITS_PER_BYTE));
    }

    //
    // The checksum is the one's complement of the sum.
    //

    return (USHORT)~Sum;
}

