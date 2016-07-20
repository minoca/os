/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    core.h

Abstract:

    This header contains definitions for the Chalk core classes.

Author:

    Evan Green 28-May-2016

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

    This structure contains the state for the Chalk interpreter.

Members:

    Name - Stores the name string of the function to attach to the class.

    Primitive - Stores a pointer to the primitive function to call for the
        function

--*/

typedef struct _CK_PRIMITIVE_DESCRIPTION {
    PSTR Name;
    PCK_PRIMITIVE_METHOD Primitive;
} CK_PRIMITIVE_DESCRIPTION, *PCK_PRIMITIVE_DESCRIPTION;

//
// -------------------------------------------------------------------- Globals
//

extern CK_PRIMITIVE_DESCRIPTION CkIntPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkIntStaticPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkRangePrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkStringPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkStringStaticPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkListPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkDictPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkFiberPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkFiberStaticPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkModulePrimitives[];

//
// -------------------------------------------------------- Function Prototypes
//

CK_ERROR_TYPE
CkpInitializeCore (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine initialize the Chalk VM, creating and wiring up the root
    classes.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Chalk status.

--*/

VOID
CkpRuntimeError (
    PCK_VM Vm,
    PSTR MessageFormat,
    ...
    );

/*++

Routine Description:

    This routine reports a runtime error in the current fiber.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    MessageFormat - Supplies the printf message format string.

    ... - Supplies the remaining arguments.

Return Value:

    None.

--*/
