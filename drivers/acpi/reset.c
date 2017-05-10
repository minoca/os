/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    reset.c

Abstract:

    This module handles system reset and sleep transitions via ACPI.

Author:

    Evan Green 9-May-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "acpip.h"
#include "namespce.h"
#include "fixedreg.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro determines whether or not the ACPI reset register can be used.
// If the revision is 3 or above, the flag can give a definitive yes. Otherwise,
// try to use it if the reset register and value are non-zero.
//

#define ACPI_RESET_REGISTER_SUPPORTED(_Fadt) \
    ((((_Fadt)->Header.Revision >= 3) && \
      (((_Fadt)->Flags & FADT_FLAG_RESET_REGISTER_SUPPORTED) != 0)) || \
     (((_Fadt)->Header.Length >= FIELD_OFFSET(FADT, ResetValue)) && \
      ((_Fadt)->ResetValue != 0) && \
      ((_Fadt)->ResetRegister.Address != 0)))

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
AcpiPrepareForSystemStateTransition (
    PVOID Context,
    SYSTEM_RESET_TYPE ResetType
    );

KSTATUS
AcpiPerformSystemStateTransition (
    PVOID Context,
    SYSTEM_RESET_TYPE ResetType,
    PVOID Data,
    UINTN Size
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the \_TTS function, which should be called before any
// device drivers are notified of a sleep transition.
//

PACPI_OBJECT AcpiTransitionToSleepMethod;

//
// Store a pointer to the \_PTS function, which should be called just before
// the transition occurs.
//

PACPI_OBJECT AcpiPrepareToSleepMethod;

//
// Store the sleep package values to write to PM1a_CNT.SLP_TYP. They're
// initialized with the package names for convenience
//

ULONG AcpiSleepValues[6] = {
    ACPI_OBJECT__S0,
    ACPI_OBJECT__S1,
    ACPI_OBJECT__S2,
    ACPI_OBJECT__S3,
    ACPI_OBJECT__S4,
    ACPI_OBJECT__S5
};

//
// Define the reboot module description.
//

REBOOT_MODULE_DESCRIPTION AcpiRebootModuleDescription = {
    REBOOT_MODULE_DESCRIPTION_VERSION,
    {
        AcpiPrepareForSystemStateTransition,
        AcpiPerformSystemStateTransition
    },

    NULL,
    ACPI_ALLOCATION_TAG,
    0
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
AcpipInitializeSystemStateTransitions (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for reboot and system power state
    transitions.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    BOOL CanDoSomething;
    ULONG Index;
    PACPI_OBJECT Integer;
    PACPI_OBJECT Package;
    BOOL Release;
    PACPI_OBJECT Root;
    KSTATUS Status;
    ULONG Value;

    if (AcpiFadtTable == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    if ((AcpiFadtTable->Flags & FADT_FLAG_HARDWARE_REDUCED_ACPI) != 0) {
        return STATUS_SUCCESS;
    }

    CanDoSomething = ACPI_RESET_REGISTER_SUPPORTED(AcpiFadtTable);
    Root = AcpipGetNamespaceRoot();
    AcpiTransitionToSleepMethod = AcpipFindNamedObject(Root, ACPI_METHOD__TTS);
    AcpiPrepareToSleepMethod = AcpipFindNamedObject(Root, ACPI_METHOD__PTS);

    //
    // Loop through and get the Sx sleep values. Don't bother if running in
    // reduced hardware mode.
    //

    if ((AcpiFadtTable->Flags & FADT_FLAG_HARDWARE_REDUCED_ACPI) == 0) {
        for (Index = 0; Index <= 5; Index += 1) {

            //
            // The object name is stored in the value initially for easy
            // one-time lookup.
            //

            Package = AcpipFindNamedObject(Root, AcpiSleepValues[Index]);
            AcpiSleepValues[Index] = 0;
            Release = FALSE;

            //
            // If the package is actually a method, go execute the method.
            //

            if ((Package != NULL) && (Package->Type == AcpiObjectMethod)) {
                Status = AcpiExecuteMethod(Package,
                                           NULL,
                                           0,
                                           AcpiObjectPackage,
                                           &Package);

                if (!KSUCCESS(Status)) {
                    RtlDebugPrint("ACPI: Failed to execute _S%d package: %d\n",
                                  Index,
                                  Status);
                }

                Release = TRUE;
            }

            //
            // The first value in the package is the value to write to
            // PM1a_CNT.SLP_TYPE. The second value is the value to write to
            // PM1B_CNT.SLP_TYE. This implementation currently smashes them
            // together and writes the same value to both.
            //

            if ((Package != NULL) && (Package->Type == AcpiObjectPackage) &&
                (Package->U.Package.ElementCount > 0)) {

                Value = 0;
                Integer = Package->U.Package.Array[0];
                if (Integer->Type == AcpiObjectInteger) {
                    Value |= Integer->U.Integer.Value;
                    CanDoSomething = TRUE;
                }

                if (Package->U.Package.ElementCount > 1) {
                    Integer = Package->U.Package.Array[1];
                    if (Integer->Type == AcpiObjectInteger) {
                        Value |= Integer->U.Integer.Value;
                    }
                }

                AcpiSleepValues[Index] = Value;
            }

            if (Release != FALSE) {
                AcpipObjectReleaseReference(Package);
            }
        }
    }

    if (CanDoSomething == FALSE) {
        return STATUS_SUCCESS;
    }

    Status = HlRegisterHardware(HardwareModuleReboot,
                                &AcpiRebootModuleDescription);

    return Status;
}

KSTATUS
AcpiPrepareForSystemStateTransition (
    PVOID Context,
    SYSTEM_RESET_TYPE ResetType
    )

/*++

Routine Description:

    This routine prepares the system for a reboot or system power transition.
    This function is called at low level when possible. During emergency reboot
    situations, this function may not be called.

Arguments:

    Context - Supplies the pointer to the reboot controller's context, provided
        by the hardware module upon initialization.

    ResetType - Supplies the reset type that is going to occur.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

{

    PACPI_OBJECT Argument;
    ULONGLONG StateInteger;
    KSTATUS Status;

    if ((AcpiTransitionToSleepMethod == NULL) &&
        (AcpiPrepareToSleepMethod == NULL)) {

        return STATUS_SUCCESS;
    }

    ASSERT(KeGetRunLevel() == RunLevelLow);

    StateInteger = 5;
    Argument = AcpipCreateNamespaceObject(NULL,
                                          AcpiObjectInteger,
                                          NULL,
                                          &StateInteger,
                                          sizeof(ULONGLONG));

    if (Argument == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PrepareForSystemStateTransitionEnd;
    }

    //
    // Call the _TTS method. This is really supposed to happen before drivers
    // know, but as of today drivers aren't really told anything.
    //

    if (AcpiTransitionToSleepMethod != NULL) {
        Status = AcpiExecuteMethod(AcpiTransitionToSleepMethod,
                                   &Argument,
                                   1,
                                   AcpiObjectUninitialized,
                                   NULL);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("ACPI: _TTS failed: %d\n", Status);
            goto PrepareForSystemStateTransitionEnd;
        }
    }

    //
    // Call the _PTS method.
    //

    if (AcpiPrepareToSleepMethod != NULL) {
        Status = AcpiExecuteMethod(AcpiPrepareToSleepMethod,
                                   &Argument,
                                   1,
                                   AcpiObjectUninitialized,
                                   NULL);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("ACPI: _PTS failed: %d\n", Status);
            goto PrepareForSystemStateTransitionEnd;
        }
    }

    Status = STATUS_SUCCESS;

PrepareForSystemStateTransitionEnd:
    if (Argument != NULL) {
        AcpipObjectReleaseReference(Argument);
    }

    return Status;
}

KSTATUS
AcpiPerformSystemStateTransition (
    PVOID Context,
    SYSTEM_RESET_TYPE ResetType,
    PVOID Data,
    UINTN Size
    )

/*++

Routine Description:

    This routine shuts down or reboots the entire system.

Arguments:

    Context - Supplies the pointer to the reboot controller's context, provided
        by the hardware module upon initialization.

    ResetType - Supplies the reset type.

    Data - Supplies an optional pointer's worth of platform-specific data.

    Size - Supplies the size of the platform-specific data.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

{

    PFADT Fadt;
    KSTATUS Status;
    ULONG Value;

    //
    // Make sure WAK_STS is clear.
    //

    AcpipWritePm1EventRegister(FADT_PM1_EVENT_WAKE_STATUS);
    if (ResetType == SystemResetShutdown) {
        Value = 0;
        AcpipReadPm1ControlRegister(&Value);
        Value &= ~FADT_PM1_CONTROL_SLEEP_TYPE;
        Value |= AcpiSleepValues[5] << FADT_PM1_CONTROL_SLEEP_TYPE_SHIFT;
        Value |= FADT_PM1_CONTROL_SLEEP;
        Status = AcpipWritePm1ControlRegister(Value);
        return Status;
    }

    //
    // This is a reset transition. There really should be an FADT since the
    // reset module got registered.
    //

    Fadt = AcpiFadtTable;
    if (Fadt == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    if (ACPI_RESET_REGISTER_SUPPORTED(Fadt)) {
        if (Fadt->ResetRegister.AddressSpaceId == AddressSpaceIo) {
            HlIoPortOutByte((USHORT)(Fadt->ResetRegister.Address),
                            Fadt->ResetValue);

            return STATUS_SUCCESS;
        }
    }

    return STATUS_NOT_SUPPORTED;
}

//
// --------------------------------------------------------- Internal Functions
//

