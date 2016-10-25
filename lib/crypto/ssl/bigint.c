/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bigint.c

Abstract:

    This module implements support for large integer arithmetic.

Author:

    Evan Green 14-Jul-2015

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../cryptop.h"

//
// --------------------------------------------------------------------- Macros
//

#define CypBiResidue(_Context, _Value) \
    CypBiPerformBarrettReduction((_Context), (_Value))

#define CypBiModulo(_Context, _Value)                       \
    CypBiDivide((_Context),                                 \
                (_Value),                                   \
                (_Context)->Modulus[(_Context)->ModOffset], \
                TRUE)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the special reference count that signifies reference counting is
// not in use for this value.
//

#define BIG_INTEGER_PERMANENT_REFERENCE 0x7FFFFFF0

//
// Define the number of bits in a component.
//

#define BIG_INTEGER_COMPONENT_BITS \
    (sizeof(BIG_INTEGER_COMPONENT) * BITS_PER_BYTE)

#define BIG_INTEGER_LONG_COMPONENT_MAX 0xFFFFFFFFFFFFFFFFULL

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PBIG_INTEGER
CypBiAdd (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Left,
    PBIG_INTEGER Right
    );

PBIG_INTEGER
CypBiSubtract (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Left,
    PBIG_INTEGER Right,
    PBOOL NegativeResult
    );

PBIG_INTEGER
CypBiMultiply (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Left,
    PBIG_INTEGER Right
    );

PBIG_INTEGER
CypBiDivide (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Numerator,
    PBIG_INTEGER Denominator,
    BOOL ModuloOperation
    );

PBIG_INTEGER
CypBiSquare (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value
    );

INT
CypBiCompare (
    PBIG_INTEGER Left,
    PBIG_INTEGER Right
    );

PBIG_INTEGER
CypBiMultiplyComponent (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Left,
    BIG_INTEGER_COMPONENT RightComponent
    );

PBIG_INTEGER
CypBiMultiplyStandard (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Left,
    PBIG_INTEGER Right,
    INTN InnerPartial,
    INTN OuterPartial
    );

PBIG_INTEGER
CypBiDivideComponent (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Numerator,
    BIG_INTEGER_COMPONENT Denominator
    );

PBIG_INTEGER
CypBiRightShiftComponent (
    PBIG_INTEGER Value,
    UINTN ComponentCount
    );

PBIG_INTEGER
CypBiLeftShiftComponent (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value,
    UINTN ComponentCount
    );

INTN
CypBiFindLeadingBit (
    PBIG_INTEGER Value
    );

BOOL
CypBiTestBit (
    PBIG_INTEGER Value,
    UINTN BitIndex
    );

PBIG_INTEGER
CypBiPerformBarrettReduction (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value
    );

KSTATUS
CypBiComputeExponentTable (
    PBIG_INTEGER_CONTEXT Context,
    INTN CountExponent,
    PBIG_INTEGER Value
    );

PBIG_INTEGER
CypBiClone (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Integer
    );

PBIG_INTEGER
CypBiCreateFromInteger (
    PBIG_INTEGER_CONTEXT Context,
    BIG_INTEGER_COMPONENT Value
    );

PBIG_INTEGER
CypBiCreate (
    PBIG_INTEGER_CONTEXT Context,
    UINTN ComponentCount
    );

KSTATUS
CypBiResize (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Integer,
    UINTN ComponentCount
    );

VOID
CypBiTrim (
    PBIG_INTEGER Integer
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
CypBiInitializeContext (
    PBIG_INTEGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes a big integer context.

Arguments:

    Context - Supplies a pointer to the context to initialize.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the context was not partially filled
        in correctly.

    STATUS_INSUFFICIENT_RESOURCES if an allocation failed.

--*/

{

    UINTN DataSize;

    if ((Context->AllocateMemory == NULL) ||
        (Context->ReallocateMemory == NULL) ||
        (Context->FreeMemory == NULL)) {

        ASSERT(FALSE);

        return STATUS_INVALID_PARAMETER;
    }

    //
    // Zero everything but the functions.
    //

    DataSize = sizeof(BIG_INTEGER_CONTEXT) -
               FIELD_OFFSET(BIG_INTEGER_CONTEXT, ActiveList);

    RtlZeroMemory(&(Context->ActiveList), DataSize);
    Context->Radix = CypBiCreate(Context, 2);
    if (Context->Radix == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Context->Radix->Components[0] = 0;
    Context->Radix->Components[1] = 1;
    CypBiMakePermanent(Context->Radix);
    return STATUS_SUCCESS;
}

VOID
CypBiDestroyContext (
    PBIG_INTEGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys a big integer context.

Arguments:

    Context - Supplies a pointer to the context to tear down.

Return Value:

    None.

--*/

{

    ASSERT((Context->ExponentTable == NULL) &&
           (Context->WindowSize == 0));

    CypBiMakeNonPermanent(Context->Radix);
    CypBiReleaseReference(Context, Context->Radix);

    ASSERT(Context->ActiveCount == 0);

    CypBiClearCache(Context);
    return;
}

VOID
CypBiClearCache (
    PBIG_INTEGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys all big integers on the free list for the given
    context.

Arguments:

    Context - Supplies a pointer to the context to clear.
        for.

Return Value:

    None.

--*/

{

    PBIG_INTEGER Integer;
    PBIG_INTEGER Next;

    Integer = Context->FreeList;
    while (Integer != NULL) {
        Next = Integer->Next;

        //
        // Zero out the value itself to avoid leaking state.
        //

        RtlZeroMemory(Integer->Components,
                      Integer->Capacity * sizeof(BIG_INTEGER_COMPONENT));

        Context->FreeMemory(Integer->Components);
        Context->FreeMemory(Integer);
        Integer = Next;
    }

    return;
}

KSTATUS
CypBiCalculateModuli (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value,
    INTN ModOffset
    )

/*++

Routine Description:

    This routine performs some pre-calculations used in modulo reduction
    optimizations.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Value - Supplies a pointer to the modulus that will be used. This value
        will be made permanent.

    ModOffset - Supplies an offset to the moduli that can be used: the standard
        moduli, or the primes p and q.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if an allocation failed.

--*/

{

    BIG_INTEGER_COMPONENT DValue;
    PBIG_INTEGER RadixCopy;
    PBIG_INTEGER ShiftedRadix;
    UINTN Size;
    KSTATUS Status;

    RadixCopy = NULL;
    Size = Value->Size;
    Status = STATUS_INSUFFICIENT_RESOURCES;
    DValue = BIG_INTEGER_RADIX / (Value->Components[Size - 1] + 1);

    ASSERT(Context->Modulus[ModOffset] == NULL);

    Context->Modulus[ModOffset] = Value;
    CypBiMakePermanent(Value);

    ASSERT(Context->NormalizedMod[ModOffset] == NULL);

    Context->NormalizedMod[ModOffset] = CypBiMultiplyComponent(Context,
                                                               Value,
                                                               DValue);

    if (Context->NormalizedMod[ModOffset] == NULL) {
        goto BiCalculateModuliEnd;
    }

    CypBiMakePermanent(Context->NormalizedMod[ModOffset]);

    //
    // Compute Mu for Barrett reduction.
    //

    RadixCopy = CypBiClone(Context, Context->Radix);
    if (RadixCopy == NULL) {
        goto BiCalculateModuliEnd;
    }

    ShiftedRadix = CypBiLeftShiftComponent(Context, RadixCopy, (Size * 2) - 1);
    if (ShiftedRadix == NULL) {
        goto BiCalculateModuliEnd;
    }

    ASSERT(Context->Mu[ModOffset] == NULL);

    Context->Mu[ModOffset] = CypBiDivide(Context,
                                         ShiftedRadix,
                                         Context->Modulus[ModOffset],
                                         FALSE);

    if (Context->Mu[ModOffset] == NULL) {
        goto BiCalculateModuliEnd;
    }

    CypBiMakePermanent(Context->Mu[ModOffset]);
    RadixCopy = NULL;
    Status = STATUS_SUCCESS;

BiCalculateModuliEnd:
    if (RadixCopy != NULL) {
        CypBiReleaseReference(Context, RadixCopy);
    }

    return Status;
}

VOID
CypBiReleaseModuli (
    PBIG_INTEGER_CONTEXT Context,
    INTN ModOffset
    )

/*++

Routine Description:

    This routine frees memory associated with moduli for the given offset.

Arguments:

    Context - Supplies a pointer to the big integer context.

    ModOffset - Supplies the index of the moduli to free: the standard
        moduli, or the primes p and q.

Return Value:

    None.

--*/

{

    PBIG_INTEGER *Pointer;

    Pointer = &(Context->Modulus[ModOffset]);
    if (*Pointer != NULL) {
        CypBiMakeNonPermanent(*Pointer);
        CypBiReleaseReference(Context, *Pointer);
        *Pointer = NULL;
    }

    Pointer = &(Context->Mu[ModOffset]);
    if (*Pointer != NULL) {
        CypBiMakeNonPermanent(*Pointer);
        CypBiReleaseReference(Context, *Pointer);
        *Pointer = NULL;
    }

    Pointer = &(Context->NormalizedMod[ModOffset]);
    if (*Pointer != NULL) {
        CypBiMakeNonPermanent(*Pointer);
        CypBiReleaseReference(Context, *Pointer);
        *Pointer = NULL;
    }

    return;
}

PBIG_INTEGER
CypBiExponentiateModulo (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value,
    PBIG_INTEGER Exponent
    )

/*++

Routine Description:

    This routine performs exponentiation, modulo a value.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Value - Supplies a pointer to the value to reduce. A reference on this
        value will be released on success.

    Exponent - Supplies the exponent to raise the value to. A reference on this
        value will be released on success.

Return Value:

    Returns a pointer to the exponentiated value on success.

    NULL on allocation failure.

--*/

{

    INTN BitIndex;
    UINTN Index;
    INTN LeadingBit;
    PBIG_INTEGER NewValue;
    INTN NextBit;
    INTN PartialExponent;
    PBIG_INTEGER Result;
    KSTATUS Status;
    INTN WindowSize;

    WindowSize = 1;
    LeadingBit = CypBiFindLeadingBit(Exponent);

    ASSERT(LeadingBit > 0);

    Result = CypBiCreateFromInteger(Context, 1);
    if (Result == NULL) {
        return NULL;
    }

    //
    // Work out a reasonable window size.
    //

    for (BitIndex = LeadingBit; BitIndex > 32; BitIndex /= 5) {
        WindowSize += 1;
    }

    Status = CypBiComputeExponentTable(Context, WindowSize, Value);
    if (!KSUCCESS(Status)) {
        goto BiExponentiateModuloEnd;
    }

    //
    // Compute the exponentiated value, a bit at a time if the window size is
    // one, or a few bits at a time for a greater window.
    //

    Status = STATUS_INSUFFICIENT_RESOURCES;
    do {
        if (CypBiTestBit(Exponent, LeadingBit) != FALSE) {
            NextBit = LeadingBit - WindowSize + 1;

            //
            // The least significant bit of the exponent is always set.
            //

            if (NextBit < 0) {
                NextBit = 0;

            } else {
                while (CypBiTestBit(Exponent, NextBit) == FALSE) {
                    NextBit += 1;
                }
            }

            PartialExponent = 0;

            //
            // Compute the portion of the exponent.
            //

            for (BitIndex = LeadingBit; BitIndex >= NextBit; BitIndex -= 1) {
                NewValue = CypBiSquare(Context, Result);
                if (NewValue == NULL) {
                    goto BiExponentiateModuloEnd;
                }

                Result = NewValue;
                NewValue = CypBiResidue(Context, Result);
                if (NewValue == NULL) {
                    goto BiExponentiateModuloEnd;
                }

                ASSERT(NewValue == Result);

                if (CypBiTestBit(Exponent, BitIndex) != FALSE) {
                    PartialExponent += 1;
                }

                if (BitIndex != NextBit) {
                    PartialExponent <<= 1;
                }
            }

            //
            // Adjust to the array indices.
            //

            PartialExponent = (PartialExponent - 1) / 2;

            ASSERT(PartialExponent < Context->WindowSize);

            NewValue = CypBiMultiply(Context,
                                     Result,
                                     Context->ExponentTable[PartialExponent]);

            if (NewValue == NULL) {
                goto BiExponentiateModuloEnd;
            }

            Result = NewValue;
            NewValue = CypBiResidue(Context, Result);
            if (NewValue == NULL) {
                goto BiExponentiateModuloEnd;
            }

            ASSERT(NewValue == Result);

            LeadingBit = NextBit - 1;

        //
        // Square the value.
        //

        } else {
            NewValue = CypBiSquare(Context, Result);
            if (NewValue == NULL) {
                goto BiExponentiateModuloEnd;
            }

            Result = NewValue;
            NewValue = CypBiResidue(Context, Result);
            if (NewValue == NULL) {
                goto BiExponentiateModuloEnd;
            }

            ASSERT(NewValue == Result);

            LeadingBit -= 1;
        }

    } while (LeadingBit >= 0);

    CypBiReleaseReference(Context, Value);
    CypBiReleaseReference(Context, Exponent);
    Status = STATUS_SUCCESS;

BiExponentiateModuloEnd:

    //
    // Destroy the exponent table.
    //

    if (Context->ExponentTable != NULL) {
        for (Index = 0; Index < Context->WindowSize; Index += 1) {
            CypBiMakeNonPermanent(Context->ExponentTable[Index]);
            CypBiReleaseReference(Context, Context->ExponentTable[Index]);
        }

        Context->FreeMemory(Context->ExponentTable);
        Context->ExponentTable = NULL;
        Context->WindowSize = 0;
    }

    if (!KSUCCESS(Status)) {
        if (Result != NULL) {
            CypBiReleaseReference(Context, Result);
            Result = NULL;
        }
    }

    return Result;
}

PBIG_INTEGER
CypBiChineseRemainderTheorem (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value,
    PBIG_INTEGER DpValue,
    PBIG_INTEGER DqValue,
    PBIG_INTEGER PValue,
    PBIG_INTEGER QValue,
    PBIG_INTEGER QInverse
    )

/*++

Routine Description:

    This routine uses the Chinese Remainder Theorem as an aide to quickly
    decrypting RSA values.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Value - Supplies a pointer to the value to perform the exponentiation on. A
        reference on this value will be released on success.

    DpValue - Supplies a pointer to the dP value. A reference on this will be
        released on success.

    DqValue - Supplies a pointer to the dQ value. A reference on this will be
        released on success.

    PValue - Supplies a pointer to the p prime. A reference on this value will
        be released on success.

    QValue - Supplies a pointer to the q prime. A reference on this value will
        be released on success.

    QInverse - Supplies a pointer to the Q inverse. A reference on this will be
        released on success.

Return Value:

    Returns a pointer to the result of the Chinese Remainder Theorem on success.

    NULL on allocation failure.

--*/

{

    PBIG_INTEGER HValue;
    PBIG_INTEGER M1;
    PBIG_INTEGER M2;
    PBIG_INTEGER NewValue;
    UCHAR OriginalModOffset;
    PBIG_INTEGER Result;

    HValue = NULL;
    M1 = NULL;
    M2 = NULL;
    Result = NULL;
    OriginalModOffset = Context->ModOffset;
    Context->ModOffset = BIG_INTEGER_P_OFFSET;
    CypBiAddReference(Value);
    CypBiAddReference(DpValue);
    M1 = CypBiExponentiateModulo(Context, Value, DpValue);
    if (M1 == NULL) {
        CypBiReleaseReference(Context, Value);
        CypBiReleaseReference(Context, DpValue);
        goto BiChineseRemainderTheoremEnd;
    }

    Context->ModOffset = BIG_INTEGER_Q_OFFSET;
    CypBiAddReference(Value);
    CypBiAddReference(DqValue);
    M2 = CypBiExponentiateModulo(Context, Value, DqValue);
    if (M2 == NULL) {
        CypBiReleaseReference(Context, Value);
        CypBiReleaseReference(Context, DqValue);
        goto BiChineseRemainderTheoremEnd;
    }

    CypBiAddReference(PValue);
    HValue = CypBiAdd(Context, M1, PValue);
    if (HValue == NULL) {
        CypBiReleaseReference(Context, PValue);
        goto BiChineseRemainderTheoremEnd;
    }

    M1 = NULL;
    CypBiAddReference(M2);
    NewValue = CypBiSubtract(Context, HValue, M2, NULL);
    if (NewValue == NULL) {
        CypBiReleaseReference(Context, M2);
        goto BiChineseRemainderTheoremEnd;
    }

    ASSERT(HValue == NewValue);

    CypBiAddReference(QInverse);
    NewValue = CypBiMultiply(Context, HValue, QInverse);
    if (NewValue == NULL) {
        CypBiReleaseReference(Context, QInverse);
        goto BiChineseRemainderTheoremEnd;
    }

    HValue = NewValue;
    Context->ModOffset = BIG_INTEGER_P_OFFSET;
    NewValue = CypBiResidue(Context, HValue);
    if (NewValue == NULL) {
        goto BiChineseRemainderTheoremEnd;
    }

    ASSERT(NewValue == HValue);

    CypBiAddReference(QValue);
    NewValue = CypBiMultiply(Context, QValue, HValue);
    if (NewValue == NULL) {
        CypBiReleaseReference(Context, QValue);
        goto BiChineseRemainderTheoremEnd;
    }

    HValue = NULL;
    Result = CypBiAdd(Context, M2, NewValue);
    if (Result == NULL) {
        goto BiChineseRemainderTheoremEnd;
    }

    M2 = NULL;
    CypBiReleaseReference(Context, PValue);
    CypBiReleaseReference(Context, QValue);
    CypBiReleaseReference(Context, DpValue);
    CypBiReleaseReference(Context, DqValue);
    CypBiReleaseReference(Context, QInverse);
    CypBiReleaseReference(Context, Value);

BiChineseRemainderTheoremEnd:
    Context->ModOffset = OriginalModOffset;
    if (M1 != NULL) {
        CypBiReleaseReference(Context, M1);
    }

    if (M2 != NULL) {
        CypBiReleaseReference(Context, M2);
    }

    if (HValue != NULL) {
        CypBiReleaseReference(Context, HValue);
    }

    return Result;
}

PBIG_INTEGER
CypBiImport (
    PBIG_INTEGER_CONTEXT Context,
    PVOID Data,
    UINTN Size
    )

/*++

Routine Description:

    This routine creates a big integer from a set of raw binary bytes.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Data - Supplies a pointer to the data to import.

    Size - Supplies the number of bytes in the data.

Return Value:

    Returns a pointer to the newly created value on success.

    NULL on allocation failure.

--*/

{

    UINTN ByteIndex;
    PUCHAR Bytes;
    UINTN ComponentCount;
    INTN Index;
    UINTN Offset;
    PBIG_INTEGER Value;

    Bytes = Data;
    ComponentCount = ALIGN_RANGE_UP(Size, sizeof(BIG_INTEGER_COMPONENT));
    Value = CypBiCreate(Context, ComponentCount);
    if (Value == NULL) {
        return NULL;
    }

    RtlZeroMemory(Value->Components,
                  Value->Size * sizeof(BIG_INTEGER_COMPONENT));

    //
    // The data comes in as a sequence of bytes, most significant first.
    // Convert that to a series of components, least significant component
    // first.
    //

    ByteIndex = 0;
    Offset = 0;
    for (Index = Size - 1; Index >= 0; Index -= 1) {
        Value->Components[Offset] += Bytes[Index] <<
                                     (ByteIndex * BITS_PER_BYTE);

        ByteIndex += 1;
        if (ByteIndex == sizeof(BIG_INTEGER_COMPONENT)) {
            ByteIndex = 0;
            Offset += 1;
        }
    }

    CypBiTrim(Value);
    return Value;
}

KSTATUS
CypBiExport (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value,
    PVOID Data,
    UINTN Size
    )

/*++

Routine Description:

    This routine exports a big integer to a byte stream.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Value - Supplies a pointer to the big integer to export. A reference is
        released on this value by this function on success.

    Data - Supplies a pointer to the data buffer.

    Size - Supplies the number of bytes in the data buffer.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the given buffer was not big enough to hold the
    entire integer.

--*/

{

    ULONG Byte;
    UINTN ByteIndex;
    PUCHAR DataBytes;
    UINTN DataIndex;
    INTN Index;
    UINTN IntegerSize;
    BIG_INTEGER_COMPONENT Mask;

    DataBytes = Data;
    IntegerSize = Value->Size * sizeof(BIG_INTEGER_COMPONENT);
    if (IntegerSize > Size) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    DataIndex = IntegerSize - 1;
    for (Index = 0; Index < Value->Size; Index += 1) {
        for (ByteIndex = 0;
             ByteIndex < sizeof(BIG_INTEGER_COMPONENT);
             ByteIndex += 1) {

            Mask = 0xFF << (ByteIndex * BITS_PER_BYTE);
            Byte = (Value->Components[Index] & Mask) >>
                   (ByteIndex * BITS_PER_BYTE);

            DataBytes[DataIndex] = Byte;
            DataIndex -= 1;
        }
    }

    CypBiReleaseReference(Context, Value);
    return STATUS_SUCCESS;
}

VOID
CypBiDebugPrint (
    PBIG_INTEGER Value
    )

/*++

Routine Description:

    This routine debug prints the contents of a big integer.

Arguments:

    Value - Supplies a pointer to the value to print.

Return Value:

    None.

--*/

{

    PBIG_INTEGER_COMPONENT Components;
    ULONG FieldSize;
    INTN Index;

    //
    // Each hex digit prints 4 bits, hence the divide by 4.
    //

    FieldSize = BIG_INTEGER_COMPONENT_BITS / 4;
    Index = Value->Size - 1;
    Components = Value->Components;
    RtlDebugPrint("%x", Components[Index]);
    while (Index > 0) {
        RtlDebugPrint("%0*llx", FieldSize, (ULONGLONG)(Components[Index]));
        Index -= 1;
    }

    return;
}

VOID
CypBiAddReference (
    PBIG_INTEGER Integer
    )

/*++

Routine Description:

    This routine adds a reference to the given big integer.

Arguments:

    Integer - Supplies a pointer to the big integer.

Return Value:

    None.

--*/

{

    if (Integer->ReferenceCount == BIG_INTEGER_PERMANENT_REFERENCE) {
        return;
    }

    ASSERT((Integer->ReferenceCount != 0) &&
           (Integer->ReferenceCount < 0x10000000));

    Integer->ReferenceCount += 1;
    return;
}

VOID
CypBiReleaseReference (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Integer
    )

/*++

Routine Description:

    This routine releases resources associated with a big integer.

Arguments:

    Context - Supplies a pointer to the context that owns the integer.

    Integer - Supplies a pointer to the big integer.

    ComponentCount - Supplies the number of components to allocate
        for.

Return Value:

    None.

--*/

{

    if (Integer->ReferenceCount == BIG_INTEGER_PERMANENT_REFERENCE) {
        return;
    }

    ASSERT((Integer->ReferenceCount != 0) &&
           (Integer->ReferenceCount < 0x10000000));

    Integer->ReferenceCount -= 1;
    if (Integer->ReferenceCount > 0) {
        return;
    }

    //
    // Move the integer to the free list.
    //

    Integer->Next = Context->FreeList;
    Context->FreeList = Integer;
    Context->FreeCount += 1;

    ASSERT(Context->ActiveCount > 0);

    Context->ActiveCount -= 1;
    return;
}

VOID
CypBiMakePermanent (
    PBIG_INTEGER Integer
    )

/*++

Routine Description:

    This routine makes a big integer "permanent", causing add and release
    references to be ignored.

Arguments:

    Integer - Supplies a pointer to the big integer.

Return Value:

    None.

--*/

{

    ASSERT(Integer->ReferenceCount == 1);

    Integer->ReferenceCount = BIG_INTEGER_PERMANENT_REFERENCE;
    return;
}

VOID
CypBiMakeNonPermanent (
    PBIG_INTEGER Integer
    )

/*++

Routine Description:

    This routine undoes the effects of making a big integer permanent,
    instead giving it a reference count of 1.

Arguments:

    Integer - Supplies a pointer to the big integer.

Return Value:

    None.

--*/

{

    ASSERT(Integer->ReferenceCount == BIG_INTEGER_PERMANENT_REFERENCE);

    Integer->ReferenceCount = 1;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PBIG_INTEGER
CypBiAdd (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Left,
    PBIG_INTEGER Right
    )

/*++

Routine Description:

    This routine adds two big integers together.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Left - Supplies the first value to add, as well as the destination.

    Right - Supplies the second value to add. A reference on this value will be
        released on success.

Return Value:

    Returns a pointer to the left value, which will have accumulated
    the sum of the two.

    NULL on allocation failure.

--*/

{

    BIG_INTEGER_COMPONENT Carry;
    PBIG_INTEGER_COMPONENT LeftComponent;
    PBIG_INTEGER_COMPONENT RightComponent;
    KSTATUS Status;
    BIG_INTEGER_COMPONENT Sum;
    UINTN SumSize;
    BIG_INTEGER_COMPONENT SumWithCarry;

    //
    // The number of components in the sum is the maximum number of components
    // between then two, plus one for a potential carry.
    //

    SumSize = Left->Size;
    if (SumSize < Right->Size) {
        SumSize = Right->Size;
    }

    //
    // Resize the left for the final result, and resize the right to be the max
    // of the two to avoid annoying checks within the computation loop.
    //

    Status = CypBiResize(Context, Left, SumSize + 1);
    if (!KSUCCESS(Status)) {
        return NULL;
    }

    Status = CypBiResize(Context, Right, SumSize);
    if (!KSUCCESS(Status)) {
        return NULL;
    }

    LeftComponent = Left->Components;
    RightComponent = Right->Components;
    Carry = 0;
    do {
        Sum = *LeftComponent + *RightComponent;
        SumWithCarry = Sum + Carry;
        Carry = 0;
        if ((Sum < *LeftComponent) || (SumWithCarry < Sum)) {
            Carry = 1;
        }

        *LeftComponent = SumWithCarry;
        LeftComponent += 1;
        RightComponent += 1;
        SumSize -= 1;

    } while (SumSize != 0);

    *LeftComponent = Carry;
    CypBiReleaseReference(Context, Right);
    CypBiTrim(Left);
    return Left;
}

PBIG_INTEGER
CypBiSubtract (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Left,
    PBIG_INTEGER Right,
    PBOOL NegativeResult
    )

/*++

Routine Description:

    This routine subtracts two big integers from each other.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Left - Supplies the value to subtract from, as well as the destination.

    Right - Supplies the value to subtract. A reference on this value will be
        released on success.

    NegativeResult - Supplies an optional pointer where a boolean will be
        returned indicating whether the result was negative or not.

Return Value:

    Returns a pointer to the left value, which will have subtracted
    value.

    NULL on allocation failure.

--*/

{

    BIG_INTEGER_COMPONENT Carry;
    PBIG_INTEGER_COMPONENT LeftComponent;
    PBIG_INTEGER_COMPONENT RightComponent;
    INTN Size;
    KSTATUS Status;
    BIG_INTEGER_COMPONENT Sum;
    BIG_INTEGER_COMPONENT SumWithCarry;

    Size = Left->Size;
    Status = CypBiResize(Context, Right, Size);
    if (!KSUCCESS(Status)) {
        return NULL;
    }

    LeftComponent = Left->Components;
    RightComponent = Right->Components;
    Carry = 0;
    do {
        Sum = *LeftComponent - *RightComponent;
        SumWithCarry = Sum - Carry;
        Carry = 0;
        if ((Sum > *LeftComponent) || (SumWithCarry > Sum)) {
            Carry = 1;
        }

        *LeftComponent = SumWithCarry;
        LeftComponent += 1;
        RightComponent += 1;
        Size -= 1;

    } while (Size != 0);

    if (NegativeResult != NULL) {
        *NegativeResult = Carry;
    }

    //
    // Put the right side back to what it was.
    //

    CypBiTrim(Right);
    CypBiReleaseReference(Context, Right);
    CypBiTrim(Left);
    return Left;
}

PBIG_INTEGER
CypBiMultiply (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Left,
    PBIG_INTEGER Right
    )

/*++

Routine Description:

    This routine multiplies two big integers together.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Left - Supplies the value to multiply. A reference on this value will be
        released on success.

    Right - Supplies the value to multiply by. A reference on this value will be
        released on success.

Return Value:

    Returns a pointer to the new product.

    NULL on allocation failure.

--*/

{

    return CypBiMultiplyStandard(Context, Left, Right, 0, 0);
}

PBIG_INTEGER
CypBiDivide (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Numerator,
    PBIG_INTEGER Denominator,
    BOOL ModuloOperation
    )

/*++

Routine Description:

    This routine divides two big integers.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Numerator - Supplies a pointer to the numerator. This is also the
        destination.

    Denominator - Supplies a pointer to the denominator. A reference will be
        released on this value on success.

    ModuloOperation - Supplies a boolean indicating whether to return the
        quotient (FALSE) or the modulo (TRUE).

Return Value:

    Returns a pointer to the numerator (now quotient or modulo) on success.

    NULL on allocation failure.

--*/

{

    PBIG_INTEGER DenominatorTimesQDash;
    UINTN Index;
    BIG_INTEGER_COMPONENT Inner;
    BOOL IsNegative;
    BIG_INTEGER_COMPONENT Last;
    BIG_INTEGER_COMPONENT LastDenominator;
    BIG_INTEGER_COMPONENT LastWorking;
    INTN ModOffset;
    PBIG_INTEGER NewWorking;
    INTN NumeratorIndex;
    INTN OriginalNumeratorSize;
    BIG_INTEGER_COMPONENT QPrime;
    PBIG_INTEGER Quotient;
    INTN QuotientSize;
    BIG_INTEGER_COMPONENT SecondLastDenominator;
    BIG_INTEGER_COMPONENT SecondLastWorking;
    INTN Size;
    KSTATUS Status;
    PBIG_INTEGER Working;

    Status = STATUS_INSUFFICIENT_RESOURCES;
    DenominatorTimesQDash = NULL;
    ModOffset = Context->ModOffset;
    OriginalNumeratorSize = Numerator->Size;
    Size = Denominator->Size;
    QuotientSize = Numerator->Size - Size;
    Working = NULL;

    //
    // Perform a quick exit: if the value is less than the modulo, then just
    // return the value.
    //

    if ((ModuloOperation != FALSE) &&
        (CypBiCompare(Denominator, Numerator) > 0)) {

        CypBiReleaseReference(Context, Denominator);
        return Numerator;
    }

    Quotient = CypBiCreate(Context, QuotientSize + 1);
    if (Quotient == NULL) {
        goto BiDivideEnd;
    }

    RtlZeroMemory(Quotient->Components,
                  Quotient->Size * sizeof(BIG_INTEGER_COMPONENT));

    Working = CypBiCreate(Context, Size + 1);
    if (Working == NULL) {
        goto BiDivideEnd;
    }

    CypBiTrim(Denominator);
    Last = BIG_INTEGER_RADIX /
           (Denominator->Components[Denominator->Size - 1] + 1);

    if (Last > 1) {
        Numerator = CypBiMultiplyComponent(Context, Numerator, Last);
        if (Numerator == NULL) {
            goto BiDivideEnd;
        }

        if (ModuloOperation != FALSE) {
            Denominator = Context->NormalizedMod[ModOffset];

        } else {
            Denominator = CypBiMultiplyComponent(Context, Denominator, Last);
        }
    }

    if (OriginalNumeratorSize == Numerator->Size) {
        CypBiResize(Context, Numerator, OriginalNumeratorSize + 1);
    }

    Index = 0;
    do {

        //
        // Create a shorter version of the numerator.
        //

        NumeratorIndex = Numerator->Size - Size - 1 - Index;
        RtlCopyMemory(Working->Components,
                      &(Numerator->Components[NumeratorIndex]),
                      (Size + 1) * sizeof(BIG_INTEGER_COMPONENT));

        //
        // Calculate q'.
        //

        LastWorking = Working->Components[Working->Size - 1];
        LastDenominator = Denominator->Components[Denominator->Size - 1];
        if (LastWorking == LastDenominator) {
            QPrime = BIG_INTEGER_RADIX - 1;

        } else {
            SecondLastWorking = Working->Components[Working->Size - 2];
            QPrime = (((BIG_INTEGER_LONG_COMPONENT)LastWorking *
                       BIG_INTEGER_RADIX) +
                      SecondLastWorking) / LastDenominator;

            if (Denominator->Size > 1) {
                SecondLastDenominator =
                                Denominator->Components[Denominator->Size - 2];

                if (SecondLastDenominator != 0) {
                    Inner = (BIG_INTEGER_RADIX * LastWorking) +
                            SecondLastWorking -
                            ((BIG_INTEGER_LONG_COMPONENT)QPrime *
                             LastDenominator);

                    if (((BIG_INTEGER_LONG_COMPONENT)SecondLastDenominator *
                         QPrime) >
                        (((BIG_INTEGER_LONG_COMPONENT)Inner *
                          BIG_INTEGER_RADIX) + SecondLastWorking)) {

                        QPrime -= 1;
                    }
                }
            }
        }

        //
        // Multiply and subtract the working value.
        //

        if (QPrime != 0) {
            CypBiAddReference(Denominator);
            DenominatorTimesQDash = CypBiMultiplyComponent(Context,
                                                           Denominator,
                                                           QPrime);

            if (DenominatorTimesQDash == NULL) {
                goto BiDivideEnd;
            }

            NewWorking = CypBiSubtract(Context,
                                       Working,
                                       DenominatorTimesQDash,
                                       &IsNegative);

            if (NewWorking == NULL) {
                goto BiDivideEnd;
            }

            DenominatorTimesQDash = NULL;
            Working = NewWorking;
            Status = CypBiResize(Context, Working, Size + 1);
            if (!KSUCCESS(Status)) {
                goto BiDivideEnd;
            }

            if (IsNegative != FALSE) {
                QPrime -= 1;
                CypBiAddReference(Denominator);
                NewWorking = CypBiAdd(Context, Working, Denominator);
                if (NewWorking == NULL) {
                    goto BiDivideEnd;
                }

                Working = NewWorking;
                Working->Size -= 1;
                Denominator->Size -= 1;
            }
        }

        Quotient->Components[Quotient->Size - Index - 1] = QPrime;

        //
        // Copy the result back.
        //

        NumeratorIndex = Numerator->Size - Size - 1 - Index;
        RtlCopyMemory(&(Numerator->Components[NumeratorIndex]),
                      Working->Components,
                      (Size + 1) * sizeof(BIG_INTEGER_COMPONENT));

        Index += 1;

    } while (Index <= QuotientSize);

    CypBiReleaseReference(Context, Working);
    Working = NULL;
    CypBiReleaseReference(Context, Denominator);
    Denominator = NULL;

    //
    // If it's a modulo operation, get the remainder.
    //

    if (ModuloOperation != FALSE) {
        CypBiReleaseReference(Context, Quotient);
        CypBiTrim(Numerator);
        Quotient = CypBiDivideComponent(Context, Numerator, Last);
        if (Quotient == NULL) {
            goto BiDivideEnd;
        }

    } else {
        CypBiReleaseReference(Context, Numerator);
        CypBiTrim(Quotient);
    }

    Status = STATUS_SUCCESS;

BiDivideEnd:
    if (Working != NULL) {
        CypBiReleaseReference(Context, Working);
    }

    if (DenominatorTimesQDash != NULL) {
        CypBiReleaseReference(Context, DenominatorTimesQDash);
    }

    if (!KSUCCESS(Status)) {
        if (Quotient != NULL) {
            CypBiReleaseReference(Context, Quotient);
            Quotient = NULL;
        }
    }

    return Quotient;
}

PBIG_INTEGER
CypBiSquare (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value
    )

/*++

Routine Description:

    This routine squares a value, using half the multiplies of the standard
    multiply method.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Value - Supplies a pointer to the big integer to square. On success a
        reference on this value is released.

Return Value:

    Returns a pointer to the new square.

    NULL on allocation failure.

--*/

{

    BIG_INTEGER_LONG_COMPONENT Carry;
    INTN Index;
    INTN InnerCarry;
    INTN InnerIndex;
    BIG_INTEGER_LONG_COMPONENT InnerProduct;
    BIG_INTEGER_LONG_COMPONENT Product;
    PBIG_INTEGER Result;
    PBIG_INTEGER_COMPONENT ResultComponents;
    INTN Size;
    PBIG_INTEGER_COMPONENT ValueComponents;

    Index = 0;
    Size = Value->Size;
    Result = CypBiCreate(Context, (Size * 2) + 1);
    if (Result == NULL) {
        return NULL;
    }

    ResultComponents = Result->Components;
    ValueComponents = Value->Components;
    RtlZeroMemory(ResultComponents,
                  Result->Size * sizeof(BIG_INTEGER_COMPONENT));

    do {
        Product = ResultComponents[Index * 2] +
                  (BIG_INTEGER_LONG_COMPONENT)ValueComponents[Index] *
                  ValueComponents[Index];

        ResultComponents[Index * 2] = (BIG_INTEGER_COMPONENT)Product;
        Carry = Product >> BIG_INTEGER_COMPONENT_BITS;
        for (InnerIndex = Index + 1; InnerIndex < Size; InnerIndex += 1) {
            InnerCarry = 0;
            InnerProduct = (BIG_INTEGER_LONG_COMPONENT)ValueComponents[Index] *
                           ValueComponents[InnerIndex];

            if ((BIG_INTEGER_LONG_COMPONENT_MAX - InnerProduct) <
                InnerProduct) {

                InnerCarry = 1;
            }

            Product = InnerProduct << 1;
            if ((BIG_INTEGER_LONG_COMPONENT_MAX - Product) <
                ResultComponents[Index + InnerIndex]) {

                InnerCarry = 1;
            }

            Product += ResultComponents[Index + InnerIndex];
            if ((BIG_INTEGER_LONG_COMPONENT_MAX - Product) < Carry) {
                InnerCarry = 1;
            }

            Product += Carry;
            ResultComponents[Index + InnerIndex] =
                                                (BIG_INTEGER_COMPONENT)Product;

            Carry = Product >> BIG_INTEGER_COMPONENT_BITS;
            if (InnerCarry != 0) {
                Carry += BIG_INTEGER_RADIX;
            }
        }

        Product = ResultComponents[Index + Size] + Carry;
        ResultComponents[Index + Size] = Product;
        ResultComponents[Index + Size + 1] =
                                         Product >> BIG_INTEGER_COMPONENT_BITS;

        Index += 1;

    } while (Index < Size);

    CypBiReleaseReference(Context, Value);
    CypBiTrim(Result);
    return Result;
}

INT
CypBiCompare (
    PBIG_INTEGER Left,
    PBIG_INTEGER Right
    )

/*++

Routine Description:

    This routine compares two big integers.

Arguments:

    Left - Supplies a pointer to the left value to compare.

    Right - Supplies a pointer to the right value to compare.

Return Value:

    < 0 if Left < Right.

    0 if Left == Right.

    > 0 if Left > Right.

--*/

{

    INTN Index;
    PBIG_INTEGER_COMPONENT LeftComponents;
    INT Result;
    PBIG_INTEGER_COMPONENT RightComponents;

    if (Left->Size > Right->Size) {
        Result = 1;

    } else if (Left->Size < Right->Size) {
        Result = -1;

    } else {
        LeftComponents = Left->Components;
        RightComponents = Right->Components;
        Result = 0;
        Index = Left->Size - 1;
        do {
            if (LeftComponents[Index] > RightComponents[Index]) {
                Result = 1;
                break;

            } else if (LeftComponents[Index] < RightComponents[Index]) {
                Result = -1;
                break;
            }

            Index -= 1;

        } while (Index >= 0);
    }

    return Result;
}

PBIG_INTEGER
CypBiMultiplyComponent (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Left,
    BIG_INTEGER_COMPONENT RightComponent
    )

/*++

Routine Description:

    This routine multiplies a big integer by a big integer component.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Left - Supplies the value to multiply. A reference on this value will be
        released on success.

    RightComponent - Supplies the value to multiply by.

Return Value:

    Returns a pointer to the new result.

    NULL on allocation failure.

--*/

{

    BIG_INTEGER_COMPONENT Carry;
    UINTN Index;
    PBIG_INTEGER_COMPONENT LeftComponents;
    PBIG_INTEGER Result;
    BIG_INTEGER_LONG_COMPONENT ResultComponent;
    PBIG_INTEGER_COMPONENT ResultComponents;
    UINTN Size;

    Size = Left->Size;
    Result = CypBiCreate(Context, Size + 1);
    if (Result == NULL) {
        return NULL;
    }

    LeftComponents = Left->Components;
    ResultComponents = Result->Components;
    RtlZeroMemory(ResultComponents, (Size * 1) * sizeof(BIG_INTEGER_COMPONENT));
    Carry = 0;
    for (Index = 0; Index < Size; Index += 1) {
        ResultComponent = ((BIG_INTEGER_LONG_COMPONENT)*LeftComponents *
                           RightComponent) + Carry;

        *ResultComponents = ResultComponent;
        Carry = ResultComponent >> BIG_INTEGER_COMPONENT_BITS;
        ResultComponents += 1;
        LeftComponents += 1;
    }

    *ResultComponents = Carry;
    CypBiReleaseReference(Context, Left);
    CypBiTrim(Result);
    return Result;
}

PBIG_INTEGER
CypBiMultiplyStandard (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Left,
    PBIG_INTEGER Right,
    INTN InnerPartial,
    INTN OuterPartial
    )

/*++

Routine Description:

    This routine multiplies two big integers together.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Left - Supplies the value to multiply. A reference on this value will be
        released on success.

    Right - Supplies the value to multiply by. A reference on this value will be
        released on success.

    InnerPartial - Supplies the inner partial product.

    OuterPartial - Supplies the outer partial product.

Return Value:

    Returns a pointer to the new product.

    NULL on allocation failure.

--*/

{

    BIG_INTEGER_COMPONENT Carry;
    PBIG_INTEGER_COMPONENT LeftComponents;
    INTN LeftIndex;
    INTN LeftSize;
    BIG_INTEGER_LONG_COMPONENT Product;
    PBIG_INTEGER Result;
    PBIG_INTEGER_COMPONENT ResultComponents;
    INTN ResultIndex;
    PBIG_INTEGER_COMPONENT RightComponents;
    INTN RightIndex;
    INTN RightSize;

    LeftSize = Left->Size;
    RightSize = Right->Size;
    Result = CypBiCreate(Context, LeftSize + RightSize);
    if (Result == NULL) {
        return NULL;
    }

    LeftComponents = Left->Components;
    RightComponents = Right->Components;
    ResultComponents = Result->Components;
    RtlZeroMemory(ResultComponents,
                  (LeftSize + RightSize) * sizeof(BIG_INTEGER_COMPONENT));

    RightIndex = 0;
    do {
        Carry = 0;
        ResultIndex = RightIndex;
        LeftIndex = 0;
        if ((OuterPartial != 0) && (OuterPartial - RightIndex > 0) &&
            (OuterPartial < LeftSize)) {

            ResultIndex = OuterPartial - 1;
            LeftIndex = OuterPartial - RightIndex - 1;
        }

        do {
            if ((InnerPartial != 0) && (ResultIndex >= InnerPartial)) {
                break;
            }

            Product = ResultComponents[ResultIndex] +
                    (((BIG_INTEGER_LONG_COMPONENT)LeftComponents[LeftIndex]) *
                      RightComponents[RightIndex]) + Carry;

            ResultComponents[ResultIndex] = Product;
            ResultIndex += 1;
            LeftIndex += 1;
            Carry = Product >> BIG_INTEGER_COMPONENT_BITS;

        } while (LeftIndex < LeftSize);

        ResultComponents[ResultIndex] = Carry;
        RightIndex += 1;

    } while (RightIndex < RightSize);

    CypBiReleaseReference(Context, Left);
    CypBiReleaseReference(Context, Right);
    CypBiTrim(Result);
    return Result;
}

PBIG_INTEGER
CypBiDivideComponent (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Numerator,
    BIG_INTEGER_COMPONENT Denominator
    )

/*++

Routine Description:

    This routine divides a big integer by a big integer component.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Numerator - Supplies a pointer to the numerator. This is also the
        destination.

    Denominator - Supplies the denominator value to divide by.

Return Value:

    Returns a pointer to the numerator (now quotient) on success.

    NULL on allocation failure.

--*/

{

    INTN Index;
    PBIG_INTEGER Result;
    BIG_INTEGER_LONG_COMPONENT ResultComponent;

    ASSERT(Numerator->Size != 0);

    Result = Numerator;
    ResultComponent = 0;
    Index = Numerator->Size - 1;
    do {
        ResultComponent = (ResultComponent << BIG_INTEGER_COMPONENT_BITS) +
                          Numerator->Components[Index];

        Result->Components[Index] =
                        (BIG_INTEGER_COMPONENT)(ResultComponent / Denominator);

        ResultComponent = ResultComponent % Denominator;
        Index -= 1;

    } while (Index >= 0);

    CypBiTrim(Result);
    return Result;
}

PBIG_INTEGER
CypBiRightShiftComponent (
    PBIG_INTEGER Value,
    UINTN ComponentCount
    )

/*++

Routine Description:

    This routine performs a right shift of a given big integer.

Arguments:

    Value - Supplies a pointer to the value to shift. This is also the
        destination.

    ComponentCount - Supplies the amount to shift, in components.

Return Value:

    Returns a pointer to the value on success.

    NULL on allocation failure.

--*/

{

    PBIG_INTEGER_COMPONENT High;
    INTN Index;
    PBIG_INTEGER_COMPONENT Low;

    Index = Value->Size - ComponentCount;
    Low = Value->Components;
    High = &(Value->Components[ComponentCount]);
    if (Index <= 0) {
        Value->Components[0] = 0;
        Value->Size = 1;
        return Value;
    }

    do {
        *Low = *High;
        Low += 1;
        High += 1;
        Index -= 1;

    } while (Index > 0);

    Value->Size -= ComponentCount;
    return Value;
}

PBIG_INTEGER
CypBiLeftShiftComponent (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value,
    UINTN ComponentCount
    )

/*++

Routine Description:

    This routine performs a left shift of a given big integer.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Value - Supplies a pointer to the value to shift. This is also the
        destination.

    ComponentCount - Supplies the amount to shift, in components.

Return Value:

    Returns a pointer to the value on success.

    NULL on allocation failure.

--*/

{

    PBIG_INTEGER_COMPONENT High;
    INTN Index;
    PBIG_INTEGER_COMPONENT Low;
    KSTATUS Status;

    Index = Value->Size - 1;
    if (ComponentCount <= 0) {
        return Value;
    }

    Status = CypBiResize(Context, Value, Value->Size + ComponentCount);
    if (!KSUCCESS(Status)) {
        return NULL;
    }

    High = &(Value->Components[Index + ComponentCount]);
    Low = &(Value->Components[Index]);
    do {
        *High = *Low;
        High += 1;
        Low += 1;
        Index -= 1;

    } while (Index > 0);

    RtlZeroMemory(Value->Components,
                  ComponentCount * sizeof(BIG_INTEGER_COMPONENT));

    return Value;
}

INTN
CypBiFindLeadingBit (
    PBIG_INTEGER Value
    )

/*++

Routine Description:

    This routine returns the bit index of the highest bit set in the given
    value.

Arguments:

    Value - Supplies a pointer to the value to find the leading bit index of.

Return Value:

    Returns the index of the highest bit set in the value.

    -1 if the value is zero.

--*/

{

    BIG_INTEGER_COMPONENT Component;
    BIG_INTEGER_COMPONENT Index;
    BIG_INTEGER_COMPONENT Mask;

    Component = Value->Components[Value->Size - 1];
    Mask = BIG_INTEGER_RADIX / 2;
    Index = BIG_INTEGER_COMPONENT_BITS - 1;
    do {
        if ((Component & Mask) != 0) {
            return Index + ((Value->Size - 1) * BIG_INTEGER_COMPONENT_BITS);
        }

        Mask >>= 1;
        Index -= 1;

    } while (Index != 0);

    return -1;
}

BOOL
CypBiTestBit (
    PBIG_INTEGER Value,
    UINTN BitIndex
    )

/*++

Routine Description:

    This routine tests to see if the bit at the given bit index is set in the
    given value.

Arguments:

    Value - Supplies a pointer to the value to test.

    BitIndex - Supplies the zero-based bit index of the bit to test.

Return Value:

    TRUE if the bit is set in the value.

    FALSE if the bit is not set in the value.

--*/

{

    BIG_INTEGER_COMPONENT Component;
    UINTN ComponentIndex;
    BIG_INTEGER_COMPONENT Mask;

    ComponentIndex = BitIndex / BIG_INTEGER_COMPONENT_BITS;

    ASSERT(ComponentIndex < Value->Size);

    Component = Value->Components[ComponentIndex];
    Mask = 1 << (BitIndex % BIG_INTEGER_COMPONENT_BITS);
    if ((Component & Mask) != 0) {
        return TRUE;
    }

    return FALSE;
}

PBIG_INTEGER
CypBiPerformBarrettReduction (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Value
    )

/*++

Routine Description:

    This routine performs a single Barrett reduction.

Arguments:

    Context - Supplies a pointer to the big integer context.

    Value - Supplies a pointer to the value to reduce. This is also the
        destination.

Return Value:

    Returns a pointer to the reduced value on success.

    NULL on allocation failure.

--*/

{

    UINTN ModOffset;
    PBIG_INTEGER Modulus;
    UINTN ModulusSize;
    PBIG_INTEGER NewValue;
    PBIG_INTEGER QValue;
    PBIG_INTEGER RValue;

    ModOffset = Context->ModOffset;
    Modulus = Context->Modulus[ModOffset];
    ModulusSize = Modulus->Size;
    RValue = NULL;

    //
    // Use the original method if Barrett cannot help.
    //

    if (Value->Size > ModulusSize * 2) {
        return CypBiModulo(Context, Value);
    }

    QValue = CypBiClone(Context, Value);
    if (QValue == NULL) {
        return NULL;
    }

    NewValue = CypBiRightShiftComponent(QValue, ModulusSize - 1);

    ASSERT(NewValue == QValue);

    NewValue = CypBiMultiplyStandard(Context,
                                     QValue,
                                     Context->Mu[ModOffset],
                                     0,
                                     ModulusSize - 1);

    if (NewValue == NULL) {
        CypBiReleaseReference(Context, QValue);
        return NULL;
    }

    QValue = NewValue;
    NewValue = CypBiRightShiftComponent(QValue, ModulusSize + 1);

    ASSERT(NewValue == QValue);

    //
    // Perform an optimized modulo operation via truncation.
    //

    if (Value->Size > ModulusSize + 1) {
        Value->Size = ModulusSize + 1;
    }

    RValue = CypBiMultiplyStandard(Context,
                                   QValue,
                                   Modulus,
                                   ModulusSize + 1,
                                   0);

    if (RValue == NULL) {
        CypBiReleaseReference(Context, QValue);
        return NULL;
    }

    QValue = NULL;

    //
    // Do another modulo truncation.
    //

    if (RValue->Size > ModulusSize + 1) {
        RValue->Size = ModulusSize + 1;
    }

    NewValue = CypBiSubtract(Context, Value, RValue, NULL);
    if (NewValue == NULL) {
        CypBiReleaseReference(Context, RValue);
        return NULL;
    }

    ASSERT(NewValue == Value);

    if (CypBiCompare(Value, Modulus) >= 0) {
        NewValue = CypBiSubtract(Context, Value, Modulus, NULL);
        if (NewValue == NULL) {
            return NULL;
        }

        ASSERT(NewValue == Value);
    }

    return Value;
}

KSTATUS
CypBiComputeExponentTable (
    PBIG_INTEGER_CONTEXT Context,
    INTN CountExponent,
    PBIG_INTEGER Value
    )

/*++

Routine Description:

    This routine computes common exponents for a given value, used to reduce
    the number of multiplies during exponentiation.

Arguments:

    Context - Supplies a pointer to the context that owns the integer.

    CountExponent - Supplies one greater than the power of two number of
        elements to compute in the table.

    Value - Supplies a pointer to the value to compute exponents for.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    UINTN AllocationSize;
    UINTN Count;
    UINTN Index;
    PBIG_INTEGER Next;
    KSTATUS Status;
    PBIG_INTEGER Value2;

    Count = 1 << (CountExponent - 1);
    Status = STATUS_INSUFFICIENT_RESOURCES;
    Value2 = NULL;

    ASSERT(Context->ExponentTable == NULL);

    AllocationSize = Count * sizeof(PBIG_INTEGER);
    Context->ExponentTable = Context->AllocateMemory(AllocationSize);
    if (Context->ExponentTable == NULL) {
        goto BiComputeExponentTableEnd;
    }

    RtlZeroMemory(Context->ExponentTable, AllocationSize);
    Context->ExponentTable[0] = CypBiClone(Context, Value);
    if (Context->ExponentTable[0] == NULL) {
        goto BiComputeExponentTableEnd;
    }

    CypBiMakePermanent(Context->ExponentTable[0]);
    Value2 = CypBiSquare(Context, Context->ExponentTable[0]);
    if (Value2 == NULL) {
        goto BiComputeExponentTableEnd;
    }

    Next = CypBiResidue(Context, Value2);
    if (Next == NULL) {
        goto BiComputeExponentTableEnd;
    }

    ASSERT(Next == Value2);

    for (Index = 1; Index < Count; Index += 1) {
        CypBiAddReference(Value2);
        Next = CypBiMultiply(Context,
                             Context->ExponentTable[Index - 1],
                             Value2);

        if (Next == NULL) {
            CypBiReleaseReference(Context, Value2);
            goto BiComputeExponentTableEnd;
        }

        Context->ExponentTable[Index] = CypBiResidue(Context, Next);
        if (Context->ExponentTable[Index] == NULL) {
            goto BiComputeExponentTableEnd;
        }

        ASSERT(Context->ExponentTable[Index] == Next);

        CypBiMakePermanent(Context->ExponentTable[Index]);
    }

    Context->WindowSize = Count;
    Status = STATUS_SUCCESS;

BiComputeExponentTableEnd:
    if (Value2 != NULL) {
        CypBiReleaseReference(Context, Value2);
    }

    if (!KSUCCESS(Status)) {
        if (Context->ExponentTable != NULL) {
            for (Index = 0; Index < Count; Index += 1) {
                Next = Context->ExponentTable[Index];
                if (Next != NULL) {
                    CypBiMakeNonPermanent(Next);
                    CypBiReleaseReference(Context, Next);
                }
            }

            Context->FreeMemory(Context->ExponentTable);
            Context->ExponentTable = NULL;
            Context->WindowSize = 0;
        }
    }

    return Status;
}

PBIG_INTEGER
CypBiClone (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Integer
    )

/*++

Routine Description:

    This routine creates a new big integer and initializes it with the given
    big integer value.

Arguments:

    Context - Supplies a pointer to the context that owns the integer.

    Integer - Supplies a pointer to an existing big integer to copy.

Return Value:

    Returns a pointer to the integer on success.

    NULL on allocation failure.

--*/

{

    PBIG_INTEGER NewInteger;

    NewInteger = CypBiCreate(Context, Integer->Size);
    if (NewInteger == NULL) {
        return NULL;
    }

    RtlCopyMemory(NewInteger->Components,
                  Integer->Components,
                  Integer->Size * sizeof(BIG_INTEGER_COMPONENT));

    return NewInteger;
}

PBIG_INTEGER
CypBiCreateFromInteger (
    PBIG_INTEGER_CONTEXT Context,
    BIG_INTEGER_COMPONENT Value
    )

/*++

Routine Description:

    This routine creates a new big integer and initializes it with the given
    value.

Arguments:

    Context - Supplies a pointer to the context that owns the integer.

    Value - Supplies the value to initialize the integer to.

Return Value:

    Returns a pointer to the integer on success.

    NULL on allocation failure.

--*/

{

    PBIG_INTEGER Integer;

    Integer = CypBiCreate(Context, 1);
    if (Integer == NULL) {
        return NULL;
    }

    Integer->Components[0] = Value;
    return Integer;
}

PBIG_INTEGER
CypBiCreate (
    PBIG_INTEGER_CONTEXT Context,
    UINTN ComponentCount
    )

/*++

Routine Description:

    This routine creates a new big integer.

Arguments:

    Context - Supplies a pointer to the context that owns the integer.

    ComponentCount - Supplies the number of components to allocate
        for.

Return Value:

    Returns a pointer to the integer on success.

    NULL on allocation failure.

--*/

{

    PBIG_INTEGER Integer;

    if (Context->FreeList != NULL) {
        Integer = Context->FreeList;
        Context->FreeList = Integer->Next;
        Context->FreeCount -= 1;

        ASSERT(Integer->ReferenceCount == 0);

        CypBiResize(Context, Integer, ComponentCount);

    } else {
        Integer = Context->AllocateMemory(sizeof(BIG_INTEGER));
        if (Integer == NULL) {
            return NULL;
        }

        Integer->Components = Context->AllocateMemory(
                               ComponentCount * sizeof(BIG_INTEGER_COMPONENT));

        if (Integer->Components == NULL) {
            Context->FreeMemory(Integer);
            return NULL;
        }

        Integer->Capacity = ComponentCount;
    }

    Integer->Size = ComponentCount;
    Integer->ReferenceCount = 1;
    Integer->Next = NULL;
    Context->ActiveCount += 1;
    return Integer;
}

KSTATUS
CypBiResize (
    PBIG_INTEGER_CONTEXT Context,
    PBIG_INTEGER Integer,
    UINTN ComponentCount
    )

/*++

Routine Description:

    This routine resizes a big integer to ensure it has at least the given
    number of components.

Arguments:

    Context - Supplies a pointer to the context that owns the integer.

    Integer - Supplies a pointer to the big integer.

    ComponentCount - Supplies the number of components to allocate
        for.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    UINTN ExtraSize;
    PVOID NewBuffer;
    UINTN NewCapacity;

    if (Integer->Capacity < ComponentCount) {
        NewCapacity = Integer->Capacity * 2;
        if (NewCapacity < ComponentCount) {
            NewCapacity = ComponentCount;
        }

        NewBuffer = Context->ReallocateMemory(
                                  Integer->Components,
                                  NewCapacity * sizeof(BIG_INTEGER_COMPONENT));

        if (NewBuffer == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Integer->Components = NewBuffer;
        Integer->Capacity = NewCapacity;
    }

    if (ComponentCount > Integer->Size) {
        ExtraSize = (ComponentCount - Integer->Size) *
                    sizeof(BIG_INTEGER_COMPONENT);

        RtlZeroMemory(&(Integer->Components[Integer->Size]), ExtraSize);
    }

    Integer->Size = ComponentCount;
    return STATUS_SUCCESS;
}

VOID
CypBiTrim (
    PBIG_INTEGER Integer
    )

/*++

Routine Description:

    This routine removes leading zero components from an integer.

Arguments:

    Integer - Supplies a pointer to the big integer.

Return Value:

    None.

--*/

{

    while ((Integer->Size > 1) &&
           (Integer->Components[Integer->Size - 1] == 0)) {

        Integer->Size -= 1;
    }

    return;
}

