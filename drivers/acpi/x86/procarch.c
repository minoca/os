/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    procarch.c

Abstract:

    This module contains architecture-specific support for ACPI processor
    management.

Author:

    Evan Green 29-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>

#if __SIZEOF_LONG__ == 8

#include <minoca/kernel/x64.h>

#else

#include <minoca/kernel/x86.h>

#endif

#include "../acpip.h"
#include "../proc.h"
#include "../namespce.h"
#include "../fixedreg.h"

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

UUID AcpiIntelOscUuid = ACPI_OSC_INTEL_UUID;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
AcpipArchInitializeProcessorManagement (
    PACPI_OBJECT NamespaceObject
    )

/*++

Routine Description:

    This routine is called to perform architecture-specific initialization for
    ACPI-based processor power management.

Arguments:

    NamespaceObject - Supplies the namespace object of this processor.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT Array[4];
    ULONG Capabilities[3];
    ULONG Index;
    PACPI_OBJECT OscMethod;
    PACPI_OBJECT OscReturn;
    PACPI_OBJECT PdcMethod;
    ULONG PmStatus;
    KSTATUS Status;
    ULONGLONG Value;

    RtlZeroMemory(&Array, sizeof(Array));
    OscReturn = NULL;
    Capabilities[0] = 0;
    Capabilities[1] = ACPI_OSC_INTEL_SMP_C1_IO_HALT |
                      ACPI_OSC_INTEL_SMP_INDEPENDENT |
                      ACPI_OSC_INTEL_C2_C3_SMP_INDEPENDENT |
                      ACPI_OSC_INTEL_SMP_C1_NATIVE |
                      ACPI_OSC_INTEL_SMP_C2_C3_NATIVE;

    //
    // Read the PM status register for the first time in a mellow state so
    // that the OS isn't trying to do mappings while it's supposed to be going
    // idle.
    //

    AcpipReadPm1EventRegister(&PmStatus);
    AcpipReadPm2ControlRegister(&PmStatus);
    OscMethod = AcpipFindNamedObject(NamespaceObject, ACPI_METHOD__OSC);
    if (OscMethod != NULL) {

        //
        // The argument is a package consisting of the following:
        // UUID buffer
        // Integer revision
        // Integer word count.
        // Buffer of capabilities. The first word is always reserved for status.
        //

        Array[0] = AcpipCreateNamespaceObject(NULL,
                                              AcpiObjectBuffer,
                                              NULL,
                                              &AcpiIntelOscUuid,
                                              sizeof(AcpiIntelOscUuid));

        Value = 1;
        Array[1] = AcpipCreateNamespaceObject(NULL,
                                              AcpiObjectInteger,
                                              NULL,
                                              &Value,
                                              sizeof(Value));

        //
        // The word count is confusing because the documents seem to say it's
        // the number of DWORDs passed in (which would be two including the
        // status word), but the example _PDC -> _OSC function passes the SIZE
        // argument from _PDC directly, which would be one.
        //

        Value = 1;
        Array[2] = AcpipCreateNamespaceObject(NULL,
                                              AcpiObjectInteger,
                                              NULL,
                                              &Value,
                                              sizeof(Value));

        Array[3] = AcpipCreateNamespaceObject(NULL,
                                              AcpiObjectBuffer,
                                              NULL,
                                              Capabilities,
                                              sizeof(Capabilities));

        if ((Array[0] == NULL) || (Array[1] == NULL) || (Array[2] == NULL) ||
            (Array[3] == NULL)) {

            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ArchInitializeProcessorManagementEnd;
        }

        Status = AcpiExecuteMethod(OscMethod,
                                   Array,
                                   4,
                                   AcpiObjectBuffer,
                                   &OscReturn);

        if (!KSUCCESS(Status)) {
            goto ArchInitializeProcessorManagementEnd;
        }

        ASSERT(OscReturn->Type == AcpiObjectBuffer);

        if (OscReturn->U.Buffer.Length > sizeof(ULONG)) {
            RtlCopyMemory(Capabilities,
                          OscReturn->U.Buffer.Buffer,
                          sizeof(ULONG));

            if (Capabilities[0] != 0) {
                RtlDebugPrint("ACPI: _OSC returned %x\n", Capabilities[0]);
            }
        }

    //
    // No OSC method, try for the _PDC method (deprecated in ACPI 3.0).
    //

    } else {
        PdcMethod = AcpipFindNamedObject(NamespaceObject, ACPI_METHOD__PDC);
        if (PdcMethod != NULL) {

            //
            // The now deprecated _PDC method takes one argument, a buffer.
            // The buffer contains:
            // A DWORD for revision ID (1).
            // A DWORD for the count of capability words.
            // The capability words.
            //

            Capabilities[0] = 1;
            Capabilities[2] = Capabilities[1];
            Capabilities[1] = 1;
            Array[0] = AcpipCreateNamespaceObject(NULL,
                                                  AcpiObjectBuffer,
                                                  NULL,
                                                  Capabilities,
                                                  sizeof(Capabilities));

            if (Array[0] == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto ArchInitializeProcessorManagementEnd;
            }

            Status = AcpiExecuteMethod(PdcMethod,
                                       Array,
                                       1,
                                       0,
                                       &OscReturn);

            if (!KSUCCESS(Status)) {
                goto ArchInitializeProcessorManagementEnd;
            }
        }
    }

    Status = STATUS_SUCCESS;

ArchInitializeProcessorManagementEnd:
    for (Index = 0; Index < 4; Index += 1) {
        if (Array[Index] != NULL) {
            AcpipObjectReleaseReference(Array[Index]);
        }
    }

    if (OscReturn != NULL) {
        AcpipObjectReleaseReference(OscReturn);
    }

    return Status;
}

VOID
AcpipEnterCState (
    PPM_IDLE_PROCESSOR_STATE Processor,
    ULONG State
    )

/*++

Routine Description:

    This routine prototype represents a function that is called to go into a
    given idle state on the current processor. This routine is called with
    interrupts disabled, and should return with interrupts disabled.

Arguments:

    Processor - Supplies a pointer to the information for the current processor.

    State - Supplies the new state index to change to.

Return Value:

    None. It is assumed when this function returns that the idle state was
    entered and then exited.

--*/

{

    PACPI_PROCESSOR_CONTEXT Context;
    PACPI_CSTATE CState;
    ULONG PmControl;
    ULONG PmStatus;
    KSTATUS Status;

    Context = Processor->Context;

    //
    // Check for bus master activity if this is C3. If there has been bus
    // master activity, clear it and then go to something that's not C3.
    //

    CState = &(Context->AcpiCStates[State]);
    if (CState->Type == AcpiC3) {
        Status = AcpipReadPm1EventRegister(&PmStatus);
        if ((KSUCCESS(Status)) &&
            ((PmStatus & FADT_PM1_EVENT_BUS_MASTER_STATUS) != 0)) {

            AcpipWritePm1EventRegister(FADT_PM1_EVENT_BUS_MASTER_STATUS);
            State = Context->HighestNonC3;
            Processor->CurrentState = State;
            CState = &(Context->AcpiCStates[State]);
        }
    }

    //
    // If going to C3 or doing bus master avoidance, write the arbiter disable
    // and bus master reload bits.
    //

    if ((CState->Type == AcpiC3) ||
        ((CState->Flags & ACPI_CSTATE_BUS_MASTER_AVOIDANCE) != 0)) {

        Status = AcpipReadPm2ControlRegister(&PmControl);
        if (KSUCCESS(Status)) {
            PmControl |= FADT_PM2_ARBITER_DISABLE;
            AcpipWritePm2ControlRegister(PmControl);
        }

        Status = AcpipReadPm1ControlRegister(&PmControl);
        if (KSUCCESS(Status)) {
            PmControl |= FADT_PM1_CONTROL_BUS_MASTER_WAKE;
            AcpipWritePm1ControlRegister(PmControl);
        }
    }

    //
    // Perform the sleep action.
    //

    if ((CState->Flags & ACPI_CSTATE_HALT) != 0) {
        ArWaitForInterrupt();
        ArDisableInterrupts();

    } else if ((CState->Flags & ACPI_CSTATE_IO_HALT) != 0) {
        ArIoReadAndHalt(CState->Register.Address);
        ArDisableInterrupts();

    } else if ((CState->Flags & ACPI_CSTATE_MWAIT) != 0) {
        ArMonitor(&CState, 0, 0);
        ArMwait(CState->Register.Address, 1);

    //
    // Perform an I/O port read.
    //

    } else {
        HlIoPortInByte(CState->Register.Address);
    }

    //
    // Re-enable bus master arbitration and disable bus master wakeup.
    //

    if ((CState->Type == AcpiC3) ||
        ((CState->Flags & ACPI_CSTATE_BUS_MASTER_AVOIDANCE) != 0)) {

        PmControl = 0;
        AcpipReadPm2ControlRegister(&PmControl);
        PmControl &= ~FADT_PM2_ARBITER_DISABLE;
        AcpipWritePm2ControlRegister(PmControl);
        AcpipReadPm1ControlRegister(&PmControl);
        PmControl &= ~FADT_PM1_CONTROL_BUS_MASTER_WAKE;
        AcpipWritePm1ControlRegister(PmControl);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

