/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fixedreg.h

Abstract:

    This header contains definitions for accessing fixed ACPI registers.

Author:

    Evan Green 21-Nov-2013

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

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
AcpipReadPm1ControlRegister (
    PULONG Value
    );

/*++

Routine Description:

    This routine reads the PM1 control register.

Arguments:

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
AcpipWritePm1ControlRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine writes to the PM1 control register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    Status code.

--*/

KSTATUS
AcpipReadPm2ControlRegister (
    PULONG Value
    );

/*++

Routine Description:

    This routine reads the PM2 control register.

Arguments:

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
AcpipWritePm2ControlRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine writes to the PM2 control register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    Status code.

--*/

KSTATUS
AcpipReadPm1EventRegister (
    PULONG Value
    );

/*++

Routine Description:

    This routine reads the PM1 event/status register.

Arguments:

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
AcpipWritePm1EventRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine writes to the PM1 event register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    Status code.

--*/

KSTATUS
AcpipReadPm1EnableRegister (
    PULONG Value
    );

/*++

Routine Description:

    This routine reads the PM1 enable register.

Arguments:

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
AcpipWritePm1EnableRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine writes to the PM1 enable register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    Status code.

--*/

VOID
AcpipAcquireGlobalLock (
    VOID
    );

/*++

Routine Description:

    This routine acquires the ACPI global lock that coordinates between the
    OSPM and firmware in SMI-land (or in some external controller).

Arguments:

    None.

Return Value:

    None.

--*/

VOID
AcpipReleaseGlobalLock (
    VOID
    );

/*++

Routine Description:

    This routine releases the ACPI global lock.

Arguments:

    None.

Return Value:

    None.

--*/

KSTATUS
AcpipInitializeFixedRegisterSupport (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for accessing fixed registers.

Arguments:

    None.

Return Value:

    Status code.

--*/

VOID
AcpipUnmapFixedRegisters (
    VOID
    );

/*++

Routine Description:

    This routine is called before a driver is about to be unloaded from memory.
    It unmaps any mappings created to access the fixed ACPI registers.

Arguments:

    None.

Return Value:

    None.

--*/
