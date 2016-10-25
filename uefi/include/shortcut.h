/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    shortcut.h

Abstract:

    This header contains aliases for the EFI boot services and runtime service
    functions. It assumes that global variables for the boot and runtime
    services exist somewhere.

Author:

    Evan Green

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define boot service aliases.
//

#define EfiRaiseTPL EfiBootServices->RaiseTPL
#define EfiRestoreTPL EfiBootServices->RestoreTPL
#define EfiAllocatePages EfiBootServices->AllocatePages
#define EfiFreePages EfiBootServices->FreePages
#define EfiGetMemoryMap EfiBootServices->GetMemoryMap
#define EfiAllocatePool EfiBootServices->AllocatePool
#define EfiFreePool EfiBootServices->FreePool
#define EfiCreateEvent EfiBootServices->CreateEvent
#define EfiSetTimer EfiBootServices->SetTimer
#define EfiWaitForEvent EfiBootServices->WaitForEvent
#define EfiSignalEvent EfiBootServices->SignalEvent
#define EfiCloseEvent EfiBootServices->CloseEvent
#define EfiCheckEvent EfiBootServices->CheckEvent
#define EfiInstallProtocolInterface EfiBootServices->InstallProtocolInterface
#define EfiReinstallProtocolInterface \
    EfiBootServices->ReinstallProtocolInterface

#define EfiUninstallProtocolInterface \
    EfiBootServices->UninstallProtocolInterface

#define EfiHandleProtocol EfiBootServices->HandleProtocol
#define EfiRegisterProtocolNotify EfiBootServices->RegisterProtocolNotify
#define EfiLocateHandle EfiBootServices->LocateHandle
#define EfiLocateDevicePath EfiBootServices->LocateDevicePath
#define EfiInstallConfigurationTable \
    EfiBootServices->InstallConfigurationTable

#define EfiLoadImage EfiBootServices->LoadImage
#define EfiStartImage EfiBootServices->StartImage
#define EfiExit EfiBootServices->Exit
#define EfiUnloadImage EfiBootServices->UnloadImage
#define EfiExitBootServices EfiBootServices->ExitBootServices
#define EfiGetNextMonotonicCount EfiBootServices->GetNextMonotonicCount
#define EfiStall EfiBootServices->Stall
#define EfiSetWatchdogTimer EfiBootServices->SetWatchdogTimer
#define EfiConnectController EfiBootServices->ConnectController
#define EfiDisconnectController EfiBootServices->DisconnectController
#define EfiOpenProtocol EfiBootServices->OpenProtocol
#define EfiCloseProtocol EfiBootServices->CloseProtocol
#define EfiOpenProtocolInformation EfiBootServices->OpenProtocolInformation
#define EfiProtocolsPerHandle EfiBootServices->ProtocolsPerHandle
#define EfiLocateHandleBuffer EfiBootServices->LocateHandleBuffer
#define EfiLocateProtocol EfiBootServices->LocateProtocol
#define EfiInstallMultipleProtocolInterfaces \
    EfiBootServices->InstallMultipleProtocolInterfaces

#define EfiUninstallMultipleProtocolInterfaces \
    EfiBootServices->UninstallMultipleProtocolInterfaces

#define EfiCalculateCrc32 EfiBootServices->CalculateCrc32
#define EfiCopyMem EfiBootServices->CopyMem
#define EfiSetMem EfiBootServices->SetMem
#define EfiCreateEventEx EfiBootServices->CreateEventEx

//
// Define runtime service aliases.
//

#define EfiGetTime EfiRuntimeServices->GetTime
#define EfiSetTime EfiRuntimeServices->SetTime
#define EfiGetWakeupTime EfiRuntimeServices->GetWakeupTime
#define EfiSetWakeupTime EfiRuntimeServices->SetWakeupTime
#define EfiSetVirtualAddressMap EfiRuntimeServices->SetVirtualAddressMap
#define EfiConvertPointer EfiRuntimeServices->ConvertPointer
#define EfiGetVariable EfiRuntimeServices->GetVariable
#define EfiGetNextVariableName EfiRuntimeServices->GetNextVariableName
#define EfiSetVariable EfiRuntimeServices->SetVariable
#define EfiGetNextHighMonotonicCount \
    EfiRuntimeServices->GetNextHighMonotonicCount

#define EfiResetSystem EfiRuntimeServices->ResetSystem
#define EfiUpdateCapsule EfiRuntimeServices->UpdateCapsule
#define EfiQueryCapsuleCapabilities \
    EfiRuntimeServices->QueryCapsuleCapabilities

#define EfiQueryVariableInfo EfiRuntimeServices->QueryVariableInfo

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the globals used by the shortcuts.
//

extern EFI_BOOT_SERVICES *EfiBootServices;
extern EFI_RUNTIME_SERVICES *EfiRuntimeServices;
extern EFI_SYSTEM_TABLE *EfiSystemTable;

//
// -------------------------------------------------------- Function Prototypes
//
