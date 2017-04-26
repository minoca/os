/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    encopt.c

Abstract:

    This module implements the encoder optimization functions that decide
    which sequence to emit. This is a port of Igor Pavlov's 7z encoder.

Author:

    Evan Green 13-Dec-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/lzma.h>
#include "lzmap.h"
#include "lzmaenc.h"

//
// --------------------------------------------------------------------- Macros
//

#define LzpOptimalMakeAsCharacter(_Optimal) \
    (_Optimal)->BackPrevious = (ULONG)-1L; \
    (_Optimal)->PreviousIsCharacter = FALSE \

#define LzpOptimalMakeAsShortRep(_Optimal) \
    (_Optimal)->BackPrevious = 0; \
    (_Optimal)->PreviousIsCharacter = FALSE

#define LzpOptimalIsShortRep(_Optimal) \
    ((_Optimal)->BackPrevious == 0)

//
// This macro determines whether or not two equal length matches are worth
// swapping given their respective differences.
//

#define LzpLzmaChangePair(_SmallDistance, _BigDistance) \
    (((_BigDistance) >> 7) > (_SmallDistance))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
LzpLzmaReturnBackReference (
    PLZMA_ENCODER Encoder,
    PULONG PositionResult,
    ULONG OptimalIndex
    );

VOID
LzpLzmaMovePosition (
    PLZMA_ENCODER Encoder,
    ULONG Count
    );

ULONG
LzpLiteralEncoderGetPrice (
    PLZ_PROB Probabilities,
    ULONG Symbol,
    PULONG ProbabilityPrices
    );

ULONG
LzpLiteralEncoderGetPriceMatched (
    PLZ_PROB Probabilities,
    ULONG Symbol,
    ULONG MatchByte,
    PULONG ProbabilityPrices
    );

ULONG
LzpLzmaGetRepPrice (
    PLZMA_ENCODER Encoder,
    ULONG RepIndex,
    ULONG Length,
    ULONG State,
    ULONG PositionState
    );

ULONG
LzpLzmaGetPureRepPrice (
    PLZMA_ENCODER Encoder,
    ULONG RepIndex,
    ULONG State,
    ULONG PositionState
    );

ULONG
LzpLzmaGetRepLen1Price (
    PLZMA_ENCODER Encoder,
    ULONG State,
    ULONG PositionState
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

ULONG
LzpLzmaGetOptimumFast (
    PLZMA_ENCODER Encoder,
    ULONG Position,
    PULONG BackResult
    )

/*++

Routine Description:

    This routine determines the optimum repeat code of previous data to emit.

Arguments:

    Encoder - Supplies a pointer to the LZMA encoder.

    Position - Supplies the current input position.

    BackResult - Supplies a pointer where either a distance or the index into
        the previously used distances array will be returned for repeats. If
        this is less than LZMA_REP_COUNT, then it is an index into a previously
        used distance array. If it is greater than or equal to LZMA_REP_COUNT,
        then LZMA_REP_COUNT should be subtracted and it is a distance back
        into the data.

Return Value:

    Returns the length of the best match.

--*/

{

    ULONG AvailableCount;
    PCUCHAR CheckData;
    PCUCHAR Data;
    ULONG Index;
    ULONG Length;
    ULONG Limit;
    ULONG LongestMatch;
    ULONG MainDistance;
    ULONG MainLength;
    PULONG Matches;
    ULONG NewDistance;
    ULONG PairCount;
    ULONG RepIndex;
    ULONG RepLength;

    if (Encoder->AdditionalOffset == 0) {
        MainLength = LzpLzmaReadMatchDistances(Encoder, &PairCount);

    } else {
        MainLength = Encoder->LongestMatchLength;
        PairCount = Encoder->PairCount;
    }

    AvailableCount = Encoder->AvailableCount;
    *BackResult = -1;
    if (AvailableCount < 2) {
        return 1;
    }

    if (AvailableCount > LZMA_MAX_MATCH_LENGTH) {
        AvailableCount = LZMA_MAX_MATCH_LENGTH;
    }

    Data = Encoder->MatchFinder.GetPosition(Encoder->MatchFinderContext) - 1;
    RepLength = 0;
    RepIndex = 0;

    //
    // See if any of the previous reps just happen to work out.
    //

    for (Index = 0; Index < LZMA_REP_COUNT; Index += 1) {
        CheckData = Data - Encoder->Reps[Index] - 1;
        if ((Data[0] != CheckData[0]) || (Data[1] != CheckData[1])) {
            continue;
        }

        Length = 2;
        while ((Length < AvailableCount) &&
               (Data[Length] == CheckData[Length])) {

            Length += 1;
        }

        //
        // If it's long enough, just return it without further fuss.
        //

        if (Length >= Encoder->FastByteCount) {
            *BackResult = Index;
            LzpLzmaMovePosition(Encoder, Length - 1);
            return Length;
        }

        //
        // Save the winner.
        //

        if (Length > RepLength) {
            RepIndex = Index;
            RepLength = Length;
        }
    }

    Matches = Encoder->Matches;
    if (MainLength >= Encoder->FastByteCount) {
        *BackResult = Matches[PairCount - 1] + LZMA_REP_COUNT;
        LzpLzmaMovePosition(Encoder, MainLength - 1);
        return MainLength;
    }

    MainDistance = 0;
    if (MainLength >= 2) {
        MainDistance = Matches[PairCount - 1];

        //
        // If a bunch of the best pairs have the same length, see which one
        // might be best (ie have the smallest distance).
        //

        while ((PairCount > 2) && (MainLength == Matches[PairCount - 4] + 1)) {
            if (!LzpLzmaChangePair(Matches[PairCount - 3], MainDistance)) {
                break;
            }

            PairCount -= 2;
            MainLength = Matches[PairCount - 2];
            MainDistance = Matches[PairCount - 1];
        }

        if ((MainLength == 2) && (MainDistance >= 0x80)) {
            MainLength = 1;
        }
    }

    //
    // Depending on which one takes up more bits to spit out, potentially use
    // the previous rep.
    //

    if ((RepLength >= 2) &&
        ((RepLength + 1 >= MainLength) ||
         ((RepLength + 2 >= MainLength) && (MainDistance >= (1 << 9))) ||
         ((RepLength + 3 >= MainLength) && (MainDistance >= (1 << 15))))) {

        *BackResult = RepIndex;
        LzpLzmaMovePosition(Encoder, RepLength - 1);
        return RepLength;
    }

    if ((MainLength < 2) || (AvailableCount <= 2)) {
        return 1;
    }

    //
    // Take a peek at the next set of matches. If the next set is super good,
    // potentially just output a single literal here.
    //

    LongestMatch = LzpLzmaReadMatchDistances(Encoder, &(Encoder->PairCount));
    Encoder->LongestMatchLength = LongestMatch;
    if (LongestMatch >= 2) {
        NewDistance = Matches[Encoder->PairCount - 1];
        if (((LongestMatch >= MainLength) && (NewDistance < MainDistance)) ||
            ((LongestMatch == MainLength + 1) &&
             (!LzpLzmaChangePair(MainDistance, NewDistance))) ||
            (LongestMatch > MainLength + 1) ||
            ((LongestMatch + 1 >= MainLength) &&
             (MainLength >= 3) &&
             (LzpLzmaChangePair(NewDistance, MainDistance)))) {

            return 1;
        }
    }

    Data = Encoder->MatchFinder.GetPosition(Encoder->MatchFinderContext) - 1;
    for (Index = 0; Index < LZMA_REP_COUNT; Index += 1) {
        CheckData = Data - Encoder->Reps[Index] - 1;
        if ((Data[0] != CheckData[0]) || (Data[1] != CheckData[1])) {
            continue;
        }

        Limit = MainLength - 1;
        Length = 2;
        while ((Length < Limit) && (Data[Length] == CheckData[Length])) {
            Length += 1;
        }

        if (Length >= Limit) {
            return 1;
        }
    }

    *BackResult = MainDistance + LZMA_REP_COUNT;
    LzpLzmaMovePosition(Encoder, MainLength - 2);
    return MainLength;
}

ULONG
LzpLzmaGetOptimum (
    PLZMA_ENCODER Encoder,
    ULONG Position,
    PULONG BackResult
    )

/*++

Routine Description:

    This routine determines the optimum repeat code of previous data to emit.

Arguments:

    Encoder - Supplies a pointer to the LZMA encoder.

    Position - Supplies the current input position.

    BackResult - Supplies a pointer where either a distance or the index into
        the previously used distances array will be returned for repeats. If
        this is less than LZMA_REP_COUNT, then it is an index into a previously
        used distance array. If it is greater than or equal to LZMA_REP_COUNT,
        then LZMA_REP_COUNT should be subtracted and it is a distance back
        into the data.

Return Value:

    Returns the length of the best match.

--*/

{

    ULONG AdjustedLength;
    ULONG AvailableCount;
    ULONG AvailableCountFull;
    ULONG BackPrevious;
    UCHAR Byte;
    PCUCHAR CheckData;
    ULONG Current;
    ULONG CurrentAnd1Price;
    ULONG CurrentAndLengthPrice;
    ULONG CurrentBack;
    ULONG CurrentPrice;
    PCUCHAR Data;
    ULONG Distance;
    ULONG EndOffset;
    ULONG ExtraLength;
    ULONG Index;
    ULONG Length;
    ULONG LengthEnd;
    ULONG LengthResult;
    ULONG LengthTest;
    ULONG LengthToPositionState;
    ULONG Limit;
    PLZ_PROB LiteralProbabilities;
    ULONG MainLength;
    UCHAR MatchByte;
    PULONG Matches;
    ULONG MatchPrice;
    ULONG NewLength;
    BOOL NextIsCharacter;
    PLZMA_OPTIMAL NextOptimal;
    ULONG NextRepMatchPrice;
    ULONG NormalMatchPrice;
    ULONG Offset;
    PLZMA_OPTIMAL Optimal;
    ULONG PairCount;
    ULONG PositionPrevious;
    ULONG PositionState;
    ULONG PositionStateNext;
    PLZMA_OPTIMAL PreviousOptimal;
    ULONG Price;
    PULONG Prices;
    PLZ_PROB Probabilities;
    ULONG RepIndex;
    ULONG RepLength;
    PLZMA_LENGTH_PRICE_ENCODER RepLengthEncoder;
    ULONG RepLengths[LZMA_REP_COUNT];
    ULONG RepMatchPrice;
    ULONG RepMaxIndex;
    ULONG Reps[LZMA_REP_COUNT];
    ULONG SavedLengthTest;
    ULONG ShortRepPrice;
    ULONG Slot;
    ULONG StartLength;
    ULONG State;
    ULONG SymbolIndex;
    ULONG TestState;

    //
    // If there are still steps left over from a previous computation, send
    // those along now.
    //

    if (Encoder->OptimumEndIndex != Encoder->OptimumCurrentIndex) {
        Optimal = &(Encoder->Optimal[Encoder->OptimumCurrentIndex]);
        LengthResult = Optimal->PositionPrevious - Encoder->OptimumCurrentIndex;
        *BackResult = Optimal->BackPrevious;
        Encoder->OptimumCurrentIndex = Optimal->PositionPrevious;
        return LengthResult;
    }

    Encoder->OptimumCurrentIndex = 0;
    Encoder->OptimumEndIndex = 0;
    if (Encoder->AdditionalOffset == 0) {
        MainLength = LzpLzmaReadMatchDistances(Encoder, &PairCount);

    } else {
        MainLength = Encoder->LongestMatchLength;
        PairCount = Encoder->PairCount;
    }

    AvailableCount = Encoder->AvailableCount;
    if (AvailableCount < 2) {
        *BackResult = (ULONG)-1L;
        return 1;
    }

    if (AvailableCount > LZMA_MAX_MATCH_LENGTH) {
        AvailableCount = LZMA_MAX_MATCH_LENGTH;
    }

    //
    // See what kind of repeat length can be gotten out of the last few reps.
    //

    Data = Encoder->MatchFinder.GetPosition(Encoder->MatchFinderContext) - 1;
    RepMaxIndex = 0;
    for (Index = 0; Index < LZMA_REP_COUNT; Index += 1) {
        Reps[Index] = Encoder->Reps[Index];
        CheckData = Data - Reps[Index] - 1;
        if ((Data[0] != CheckData[0]) || (Data[1] != CheckData[1])) {
            RepLengths[Index] = 0;
            continue;
        }

        LengthTest = 2;
        while ((LengthTest < AvailableCount) &&
               (Data[LengthTest] == CheckData[LengthTest])) {

            LengthTest += 1;
        }

        RepLengths[Index] = LengthTest;
        if (LengthTest > RepLengths[RepMaxIndex]) {
            RepMaxIndex = Index;
        }
    }

    //
    // Just return a previous rep if it got a lot.
    //

    if (RepLengths[RepMaxIndex] >= Encoder->FastByteCount) {
        *BackResult = RepMaxIndex;
        LengthResult = RepLengths[RepMaxIndex];
        LzpLzmaMovePosition(Encoder, LengthResult - 1);
        return LengthResult;
    }

    //
    // If one of the matches found was big enough, just return it.
    //

    Matches = Encoder->Matches;
    if (MainLength >= Encoder->FastByteCount) {
        *BackResult = Matches[PairCount - 1] + LZMA_REP_COUNT;
        LzpLzmaMovePosition(Encoder, MainLength - 1);
        return MainLength;
    }

    //
    // If there were no matches found anywhere, just give up.
    //

    Byte = *Data;
    MatchByte = *(Data - (Reps[0] + 1));
    if ((MainLength < 2) && (Byte != MatchByte) &&
        (RepLengths[RepMaxIndex] < 2)) {

        *BackResult = -1;
        return 1;
    }

    Encoder->Optimal[0].State = Encoder->State;
    PositionState = Position & Encoder->PbMask;
    Probabilities = LZP_LITERAL_PROBABILITIES(Encoder, Position, *(Data - 1));
    Encoder->Optimal[1].Price =
        LZP_GET_PRICE(Encoder,
                      Encoder->IsMatch[Encoder->State][PositionState],
                      0);

    if (!LZP_IS_CHARACTER_STATE(Encoder->State)) {
        Encoder->Optimal[1].Price +=
                  LzpLiteralEncoderGetPriceMatched(Probabilities,
                                                   Byte,
                                                   MatchByte,
                                                   Encoder->ProbabilityPrices);

    } else {
        Encoder->Optimal[1].Price +=
                         LzpLiteralEncoderGetPrice(Probabilities,
                                                   Byte,
                                                   Encoder->ProbabilityPrices);
    }

    LzpOptimalMakeAsCharacter(&(Encoder->Optimal[1]));
    MatchPrice = LZP_GET_PRICE(Encoder,
                               Encoder->IsMatch[Encoder->State][PositionState],
                               1);

    RepMatchPrice = MatchPrice +
                    LZP_GET_PRICE(Encoder, Encoder->IsRep[Encoder->State], 1);

    if (MatchByte == Byte) {
        ShortRepPrice = RepMatchPrice +
                        LzpLzmaGetRepLen1Price(Encoder,
                                               Encoder->State,
                                               PositionState);

        if (ShortRepPrice < Encoder->Optimal[1].Price) {
            Encoder->Optimal[1].Price = ShortRepPrice;
            LzpOptimalMakeAsShortRep(&(Encoder->Optimal[1]));
        }
    }

    LengthEnd = MainLength;
    if (LengthEnd < RepLengths[RepMaxIndex]) {
        LengthEnd = RepLengths[RepMaxIndex];
    }

    if (LengthEnd < 2) {
        *BackResult = Encoder->Optimal[1].BackPrevious;
        return 1;
    }

    Encoder->Optimal[1].PositionPrevious = 0;
    for (Index = 0; Index < LZMA_REP_COUNT; Index += 1) {
        Encoder->Optimal[0].Backs[Index] = Reps[Index];
    }

    //
    // Set all the prices initially (except 0 and 1) to infinity.
    //

    Length = LengthEnd;
    do {
        Encoder->Optimal[Length].Price = LZMA_INFINITY_PRICE;
        Length -= 1;

    } while (Length >= 2);

    //
    // Set each of the optimals price to the best of the reps.
    //

    for (Index = 0; Index < LZMA_REP_COUNT; Index += 1) {
        RepLength = RepLengths[Index];
        if (RepLength < 2) {
            continue;
        }

        Price = RepMatchPrice +
                LzpLzmaGetPureRepPrice(Encoder,
                                       Index,
                                       Encoder->State,
                                       PositionState);

        do {
            CurrentAndLengthPrice =
                Price +
                Encoder->RepLengthEncoder.Prices[PositionState][RepLength - 2];

            Optimal = &(Encoder->Optimal[RepLength]);
            if (CurrentAndLengthPrice < Optimal->Price) {
                Optimal->Price = CurrentAndLengthPrice;
                Optimal->PositionPrevious = 0;
                Optimal->BackPrevious = Index;
                Optimal->PreviousIsCharacter = FALSE;
            }

            RepLength -= 1;

        } while (RepLength >= 2);
    }

    NormalMatchPrice = MatchPrice +
                       LZP_GET_PRICE(Encoder,
                                     Encoder->IsRep[Encoder->State],
                                     0);

    Length = 2;
    if (RepLengths[0] >= 2) {
        Length = RepLengths[0] + 1;
    }

    if (Length <= MainLength) {
        Offset = 0;
        while (Length > Matches[Offset]) {
            Offset += 2;
        }

        while (TRUE) {
            Distance = Matches[Offset + 1];
            AdjustedLength = Length - LZMA_MIN_MATCH_LENGTH;
            CurrentAndLengthPrice =
                  NormalMatchPrice +
                  Encoder->LengthEncoder.Prices[PositionState][AdjustedLength];

            LengthToPositionState = LZP_GET_LENGTH_TO_POSITION_STATE(Length);
            if (Distance < LZMA_FULL_DISTANCES) {
                CurrentAndLengthPrice +=
                    Encoder->DistancesPrices[LengthToPositionState][Distance];

            } else {
                Slot = LzpLzmaGetPositionSlot2(Encoder, Distance);
                CurrentAndLengthPrice +=
                             Encoder->AlignPrices[Distance & LZMA_ALIGN_MASK] +
                             Encoder->SlotPrices[LengthToPositionState][Slot];
            }

            Optimal = &(Encoder->Optimal[Length]);
            if (CurrentAndLengthPrice < Optimal->Price) {
                Optimal->Price = CurrentAndLengthPrice;
                Optimal->PositionPrevious = 0;
                Optimal->BackPrevious = Distance + LZMA_REP_COUNT;
                Optimal->PreviousIsCharacter = FALSE;
            }

            if (Length == Matches[Offset]) {
                Offset += 2;
                if (Offset == PairCount) {
                    break;
                }
            }

            Length += 1;
        }
    }

    //
    // Loop getting the best prices for various distances.
    //

    Current = 0;
    while (TRUE) {
        Current += 1;
        if (Current == LengthEnd) {
            return LzpLzmaReturnBackReference(Encoder, BackResult, Current);
        }

        NewLength = LzpLzmaReadMatchDistances(Encoder, &PairCount);
        if (NewLength >= Encoder->FastByteCount) {
            Encoder->PairCount = PairCount;
            Encoder->LongestMatchLength = NewLength;
            return LzpLzmaReturnBackReference(Encoder, BackResult, Current);
        }

        Position += 1;
        Optimal = &(Encoder->Optimal[Current]);
        PositionPrevious = Optimal->PositionPrevious;
        if (Optimal->PreviousIsCharacter != FALSE) {
            PositionPrevious -= 1;
            if (Optimal->Previous2 != FALSE) {
                State = Encoder->Optimal[Optimal->PositionPrevious2].State;
                if (Optimal->BackPrevious2 < LZMA_REP_COUNT) {
                    State = LzLzmaRepNextStates[State];

                } else {
                    State = LzLzmaMatchNextStates[State];
                }

            } else {
                State = Encoder->Optimal[PositionPrevious].State;
            }

            State = LzLzmaLiteralNextStates[State];

        } else {
            State = Encoder->Optimal[PositionPrevious].State;
        }

        if (PositionPrevious == Current - 1) {
            if (LzpOptimalIsShortRep(Optimal)) {
                State = LzLzmaShortRepNextStates[State];

            } else {
                State = LzLzmaLiteralNextStates[State];
            }

        } else {
            if ((Optimal->PreviousIsCharacter != FALSE) &&
                (Optimal->Previous2 != FALSE)) {

                PositionPrevious = Optimal->PositionPrevious2;
                BackPrevious = Optimal->BackPrevious2;
                State = LzLzmaRepNextStates[State];

            } else {
                BackPrevious = Optimal->BackPrevious;
                if (BackPrevious < LZMA_REP_COUNT) {
                    State = LzLzmaRepNextStates[State];

                } else {
                    State = LzLzmaMatchNextStates[State];
                }
            }

            PreviousOptimal = &(Encoder->Optimal[PositionPrevious]);
            if (BackPrevious < LZMA_REP_COUNT) {
                Reps[0] = PreviousOptimal->Backs[BackPrevious];
                for (Index = 1; Index <= BackPrevious; Index += 1) {
                    Reps[Index] = PreviousOptimal->Backs[Index - 1];
                }

                while (Index < LZMA_REP_COUNT) {
                    Reps[Index] = PreviousOptimal->Backs[Index];
                    Index += 1;
                }

            } else {
                Reps[0] = BackPrevious - LZMA_REP_COUNT;
                for (Index = 1; Index < LZMA_REP_COUNT; Index += 1) {
                    Reps[Index] = PreviousOptimal->Backs[Index - 1];
                }
            }
        }

        Optimal->State = State;
        for (Index = 0; Index < LZMA_REP_COUNT; Index += 1) {
            Optimal->Backs[Index] = Reps[Index];
        }

        CurrentPrice = Optimal->Price;
        NextIsCharacter = FALSE;
        Data = Encoder->MatchFinder.GetPosition(Encoder->MatchFinderContext) -
               1;

        Byte = *Data;
        MatchByte = *(Data - (Reps[0] + 1));
        PositionState = Position & Encoder->PbMask;
        CurrentAnd1Price = CurrentPrice +
                           LZP_GET_PRICE(Encoder,
                                         Encoder->IsMatch[State][PositionState],
                                         0);

        Probabilities = LZP_LITERAL_PROBABILITIES(Encoder,
                                                  Position,
                                                  *(Data - 1));

        if (!LZP_IS_CHARACTER_STATE(State)) {
            CurrentAnd1Price += LzpLiteralEncoderGetPriceMatched(
                                                   Probabilities,
                                                   Byte,
                                                   MatchByte,
                                                   Encoder->ProbabilityPrices);

        } else {
            CurrentAnd1Price += LzpLiteralEncoderGetPrice(
                                                   Probabilities,
                                                   Byte,
                                                   Encoder->ProbabilityPrices);
        }

        NextOptimal = &(Encoder->Optimal[Current + 1]);
        if (CurrentAnd1Price < NextOptimal->Price) {
            NextOptimal->Price = CurrentAnd1Price;
            NextOptimal->PositionPrevious = Current;
            LzpOptimalMakeAsCharacter(NextOptimal);
            NextIsCharacter = TRUE;
        }

        MatchPrice = CurrentPrice +
             LZP_GET_PRICE(Encoder, Encoder->IsMatch[State][PositionState], 1);

        RepMatchPrice = MatchPrice +
                        LZP_GET_PRICE(Encoder, Encoder->IsRep[State], 1);

        if ((MatchByte == Byte) &&
            (!((NextOptimal->PositionPrevious < Current) &&
               (NextOptimal->BackPrevious == 0)))) {

            ShortRepPrice =
                         RepMatchPrice +
                         LzpLzmaGetRepLen1Price(Encoder, State, PositionState);

            if (ShortRepPrice <= NextOptimal->Price) {
                NextOptimal->Price = ShortRepPrice;
                NextOptimal->PositionPrevious = Current;
                LzpOptimalMakeAsShortRep(NextOptimal);
                NextIsCharacter = TRUE;
            }
        }

        AvailableCountFull = Encoder->AvailableCount;
        if (AvailableCountFull > LZMA_OPTIMAL_COUNT - 1 - Current) {
            AvailableCountFull = LZMA_OPTIMAL_COUNT - 1 - Current;
        }

        if (AvailableCountFull < 2) {
            continue;
        }

        AvailableCount = Encoder->FastByteCount;
        if (AvailableCountFull <= Encoder->FastByteCount) {
            AvailableCount = AvailableCountFull;
        }

        if ((NextIsCharacter == FALSE) && (MatchByte != Byte)) {

            //
            // Try a literal + rep0.
            //

            CheckData = Data - Reps[0] - 1;
            Limit = Encoder->FastByteCount + 1;
            if (Limit > AvailableCountFull) {
                Limit = AvailableCountFull;
            }

            Index = 1;
            while ((Index < Limit) && (Data[Index] == CheckData[Index])) {
                Index += 1;
            }

            LengthTest = Index - 1;
            if (LengthTest >= 2) {
                TestState = LzLzmaLiteralNextStates[State];
                PositionStateNext = (Position + 1) & Encoder->PbMask;

                //
                // Get the rep price.
                //

                NextRepMatchPrice = CurrentAnd1Price;
                NextRepMatchPrice += LZP_GET_PRICE(
                                Encoder,
                                Encoder->IsMatch[TestState][PositionStateNext],
                                1);

                NextRepMatchPrice += LZP_GET_PRICE(Encoder,
                                                   Encoder->IsRep[TestState],
                                                   1);

                //
                // Get the length price.
                //

                Offset = Current + 1 + LengthTest;
                while (LengthEnd < Offset) {
                    LengthEnd += 1;
                    Encoder->Optimal[LengthEnd].Price = LZMA_INFINITY_PRICE;
                }

                CurrentAndLengthPrice = NextRepMatchPrice +
                                        LzpLzmaGetRepPrice(Encoder,
                                                           0,
                                                           LengthTest,
                                                           TestState,
                                                           PositionStateNext);

                Optimal = &(Encoder->Optimal[Offset]);
                if (CurrentAndLengthPrice < Optimal->Price) {
                    Optimal->Price = CurrentAndLengthPrice;
                    Optimal->PositionPrevious = Current + 1;
                    Optimal->BackPrevious = 0;
                    Optimal->PreviousIsCharacter = TRUE;
                    Optimal->Previous2 = FALSE;
                }
            }
        }

        StartLength = 2;
        for (RepIndex = 0; RepIndex < LZMA_REP_COUNT; RepIndex += 1) {
            CheckData = Data - Reps[RepIndex] - 1;
            if ((Data[0] != CheckData[0]) || (Data[1] != CheckData[1])) {
                continue;
            }

            LengthTest = 2;
            while ((LengthTest < AvailableCount) &&
                   (Data[LengthTest] == CheckData[LengthTest])) {

                LengthTest += 1;
            }

            while (LengthEnd < Current + LengthTest) {
                LengthEnd += 1;
                Encoder->Optimal[LengthEnd].Price = LZMA_INFINITY_PRICE;
            }

            SavedLengthTest = LengthTest;
            Price = RepMatchPrice +
                    LzpLzmaGetPureRepPrice(Encoder,
                                           RepIndex,
                                           State,
                                           PositionState);

            RepLengthEncoder = &(Encoder->RepLengthEncoder);
            do {
                CurrentAndLengthPrice =
                       Price +
                       RepLengthEncoder->Prices[PositionState][LengthTest - 2];

                Optimal = &(Encoder->Optimal[Current + LengthTest]);
                if (CurrentAndLengthPrice < Optimal->Price) {
                    Optimal->Price = CurrentAndLengthPrice;
                    Optimal->PositionPrevious = Current;
                    Optimal->BackPrevious = RepIndex;
                    Optimal->PreviousIsCharacter = FALSE;
                }

                LengthTest -= 1;

            } while (LengthTest >= 2);

            LengthTest = SavedLengthTest;
            if (RepIndex == 0) {
                StartLength = LengthTest + 1;
            }

            ExtraLength = LengthTest + 1;
            Limit = ExtraLength + Encoder->FastByteCount;
            if (Limit > AvailableCountFull) {
                Limit = AvailableCountFull;
            }

            while ((ExtraLength < Limit) &&
                   (Data[ExtraLength] == CheckData[ExtraLength])) {

                ExtraLength += 1;
            }

            ExtraLength -= LengthTest + 1;
            if (ExtraLength >= 2) {
                TestState = LzLzmaRepNextStates[State];
                PositionStateNext = (Position + LengthTest) & Encoder->PbMask;
                CurrentAndLengthPrice =
                       Price +
                       RepLengthEncoder->Prices[PositionState][LengthTest - 2];

                CurrentAndLengthPrice += LZP_GET_PRICE(
                                Encoder,
                                Encoder->IsMatch[TestState][PositionStateNext],
                                0);

                LiteralProbabilities =
                    LZP_LITERAL_PROBABILITIES(Encoder,
                                              Position + LengthTest,
                                              Data[LengthTest - 1]);

                CurrentAndLengthPrice += LzpLiteralEncoderGetPriceMatched(
                                                   LiteralProbabilities,
                                                   Data[LengthTest],
                                                   CheckData[LengthTest],
                                                   Encoder->ProbabilityPrices);

                TestState = LzLzmaLiteralNextStates[TestState];
                PositionStateNext = (Position + LengthTest + 1) &
                                    Encoder->PbMask;

                NextRepMatchPrice = CurrentAndLengthPrice;
                NextRepMatchPrice += LZP_GET_PRICE(
                                Encoder,
                                Encoder->IsMatch[TestState][PositionStateNext],
                                1);

                NextRepMatchPrice += LZP_GET_PRICE(
                                Encoder,
                                Encoder->IsRep[TestState],
                                1);

                //
                // Update prices.
                //

                Offset = Current + LengthTest + 1 + ExtraLength;
                while (LengthEnd < Offset) {
                    LengthEnd += 1;
                    Encoder->Optimal[LengthEnd].Price = LZMA_INFINITY_PRICE;
                }

                CurrentAndLengthPrice = NextRepMatchPrice +
                                        LzpLzmaGetRepPrice(Encoder,
                                                           0,
                                                           ExtraLength,
                                                           TestState,
                                                           PositionStateNext);

                Optimal = &(Encoder->Optimal[Offset]);
                if (CurrentAndLengthPrice < Optimal->Price) {
                    Optimal->Price = CurrentAndLengthPrice;
                    Optimal->PositionPrevious = Current + LengthTest + 1;
                    Optimal->BackPrevious = 0;
                    Optimal->PreviousIsCharacter = TRUE;
                    Optimal->Previous2 = TRUE;
                    Optimal->PositionPrevious2 = Current;
                    Optimal->BackPrevious2 = RepIndex;
                }
            }
        }

        if (NewLength > AvailableCount) {
            NewLength = AvailableCount;
            PairCount = 0;
            while (NewLength > Matches[PairCount]) {
                PairCount += 2;
            }

            Matches[PairCount] = NewLength;
            PairCount += 2;
        }

        if (NewLength >= StartLength) {
            NormalMatchPrice =
                 MatchPrice + LZP_GET_PRICE(Encoder, Encoder->IsRep[State], 0);

            while (LengthEnd < Current + NewLength) {
                LengthEnd += 1;
                Encoder->Optimal[LengthEnd].Price = LZMA_INFINITY_PRICE;
            }

            Offset = 0;
            while (StartLength > Matches[Offset]) {
                Offset += 2;
            }

            CurrentBack = Matches[Offset + 1];
            Slot = LzpLzmaGetPositionSlot2(Encoder, CurrentBack);
            LengthTest = StartLength;
            while (TRUE) {
                SymbolIndex = LengthTest - LZMA_MIN_MATCH_LENGTH;
                CurrentAndLengthPrice =
                    NormalMatchPrice +
                    Encoder->LengthEncoder.Prices[PositionState][SymbolIndex];

                LengthToPositionState =
                                  LZP_GET_LENGTH_TO_POSITION_STATE(LengthTest);

                if (CurrentBack < LZMA_FULL_DISTANCES) {
                    Prices = Encoder->DistancesPrices[LengthToPositionState];
                    CurrentAndLengthPrice += Prices[CurrentBack];

                } else {
                    Prices = Encoder->SlotPrices[LengthToPositionState];
                    CurrentAndLengthPrice +=
                           Prices[Slot] +
                           Encoder->AlignPrices[CurrentBack & LZMA_ALIGN_MASK];
                }

                Optimal = &(Encoder->Optimal[Current + LengthTest]);
                if (CurrentAndLengthPrice < Optimal->Price) {
                    Optimal->Price = CurrentAndLengthPrice;
                    Optimal->PositionPrevious = Current;
                    Optimal->BackPrevious = CurrentBack + LZMA_REP_COUNT;
                    Optimal->PreviousIsCharacter = FALSE;
                }

                //
                // Try for Match + Literal + Rep0.
                //

                if (LengthTest == Matches[Offset]) {
                    CheckData = Data - CurrentBack - 1;
                    ExtraLength = LengthTest + 1;
                    Limit = ExtraLength + Encoder->FastByteCount;
                    if (Limit > AvailableCountFull) {
                        Limit = AvailableCountFull;
                    }

                    while ((ExtraLength < Limit) &&
                           (Data[ExtraLength] == CheckData[ExtraLength])) {

                        ExtraLength += 1;
                    }

                    ExtraLength -= LengthTest + 1;
                    if (ExtraLength >= 2) {
                        TestState = LzLzmaMatchNextStates[State];
                        PositionStateNext = (Position + LengthTest) &
                                            Encoder->PbMask;

                        CurrentAndLengthPrice += LZP_GET_PRICE(
                                 Encoder,
                                 Encoder->IsMatch[TestState][PositionStateNext],
                                 0);

                        LiteralProbabilities =
                            LZP_LITERAL_PROBABILITIES(Encoder,
                                                      Position + LengthTest,
                                                      Data[LengthTest - 1]);

                        CurrentAndLengthPrice +=
                            LzpLiteralEncoderGetPriceMatched(
                                                   LiteralProbabilities,
                                                   Data[LengthTest],
                                                   CheckData[LengthTest],
                                                   Encoder->ProbabilityPrices);

                        TestState = LzLzmaLiteralNextStates[TestState];
                        PositionStateNext = (PositionStateNext + 1) &
                                            Encoder->PbMask;

                        NextRepMatchPrice = CurrentAndLengthPrice;
                        NextRepMatchPrice += LZP_GET_PRICE(
                                Encoder,
                                Encoder->IsMatch[TestState][PositionStateNext],
                                1);

                        NextRepMatchPrice += LZP_GET_PRICE(
                                Encoder,
                                Encoder->IsRep[TestState],
                                1);

                        EndOffset = Current + LengthTest + 1 + ExtraLength;
                        while (LengthEnd < EndOffset) {
                            LengthEnd += 1;
                            Encoder->Optimal[LengthEnd].Price =
                                                           LZMA_INFINITY_PRICE;
                        }

                        CurrentAndLengthPrice =
                                        NextRepMatchPrice +
                                        LzpLzmaGetRepPrice(Encoder,
                                                           0,
                                                           ExtraLength,
                                                           TestState,
                                                           PositionStateNext);

                        Optimal = &(Encoder->Optimal[EndOffset]);
                        if (CurrentAndLengthPrice < Optimal->Price) {
                            Optimal->Price = CurrentAndLengthPrice;
                            Optimal->PositionPrevious =
                                                      Current + LengthTest + 1;

                            Optimal->BackPrevious = 0;
                            Optimal->PreviousIsCharacter = TRUE;
                            Optimal->Previous2 = TRUE;
                            Optimal->PositionPrevious2 = Current;
                            Optimal->BackPrevious2 =
                                                  CurrentBack + LZMA_REP_COUNT;
                        }
                    }

                    Offset += 2;
                    if (Offset == PairCount) {
                        break;
                    }

                    CurrentBack = Matches[Offset + 1];
                    if (CurrentBack >= LZMA_FULL_DISTANCES) {
                        Slot = LzpLzmaGetPositionSlot2(Encoder, CurrentBack);
                    }
                }

                LengthTest += 1;
            }
        }
    }

    //
    // Execution never gets here.
    //

    return 1;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
LzpLzmaReturnBackReference (
    PLZMA_ENCODER Encoder,
    PULONG PositionResult,
    ULONG OptimalIndex
    )

/*++

Routine Description:

    This routine returns a backwards reference, the spoils of compression.

Arguments:

    Encoder - Supplies a pointer to the LZMA encoder.

    PositionResult - Supplies a pointer where the backwards distance will be
        returned.

    OptimalIndex - Supplies the index into the optimal array of the option
        to choose.

Return Value:

    Returns the length of the back reference.

--*/

{

    PLZMA_OPTIMAL Optimal;
    ULONG PreviousBack;
    ULONG PreviousPosition;
    ULONG SavedBack;
    ULONG SavedPosition;

    Optimal = Encoder->Optimal;
    PreviousPosition = Optimal[OptimalIndex].PositionPrevious;
    PreviousBack = Optimal[OptimalIndex].BackPrevious;
    Encoder->OptimumEndIndex = OptimalIndex;
    do {
        if (Optimal[OptimalIndex].PreviousIsCharacter != FALSE) {
            LzpOptimalMakeAsCharacter(&(Optimal[PreviousPosition]));
            Optimal[PreviousPosition].PositionPrevious = PreviousPosition - 1;
            if (Optimal[OptimalIndex].Previous2 != FALSE) {
                Optimal[PreviousPosition - 1].PreviousIsCharacter = FALSE;
                Optimal[PreviousPosition - 1].PositionPrevious =
                                       Optimal[OptimalIndex].PositionPrevious2;

                Optimal[PreviousPosition - 1].BackPrevious =
                                           Optimal[OptimalIndex].BackPrevious2;
            }
        }

        SavedBack = PreviousBack;
        SavedPosition = PreviousPosition;
        PreviousBack = Optimal[SavedPosition].BackPrevious;
        PreviousPosition = Optimal[SavedPosition].PositionPrevious;
        Optimal[SavedPosition].BackPrevious = SavedBack;
        Optimal[SavedPosition].PositionPrevious = OptimalIndex;
        OptimalIndex = SavedPosition;

    } while (OptimalIndex != 0);

    *PositionResult = Optimal[0].BackPrevious;
    Encoder->OptimumCurrentIndex = Optimal[0].PositionPrevious;
    return Encoder->OptimumCurrentIndex;
}

VOID
LzpLzmaMovePosition (
    PLZMA_ENCODER Encoder,
    ULONG Count
    )

/*++

Routine Description:

    This routine skips a given number of bytes in the input.

Arguments:

    Encoder - Supplies a pointer to the LZMA encoder.

    Count - Supplies the number of bytes to skip.

Return Value:

    None.

--*/

{

    if (Count != 0) {
        Encoder->AdditionalOffset += Count;
        Encoder->MatchFinder.Skip(Encoder->MatchFinderContext, Count);
    }

    return;
}

ULONG
LzpLiteralEncoderGetPrice (
    PLZ_PROB Probabilities,
    ULONG Symbol,
    PULONG ProbabilityPrices
    )

/*++

Routine Description:

    This routine returns the price for a given literal.

Arguments:

    Probabilities - Supplies a pointer to the symbol probabilities.

    Symbol - Supplies the symbol to output.

    ProbabilityPrices - Supplies a pointer to the probability prices array.

Return Value:

    Returns the price for the given literal.

--*/

{

    ULONG Price;
    ULONG PriceIndex;

    Price = 0;
    Symbol |= 0x100;

    //
    // Mark the end bit, and then march through all 8 lower bits.
    //

    do {
        PriceIndex = LZP_GET_PRICE_INDEX(Probabilities[Symbol >> 8],
                                         (Symbol >> 7) & 0x1);

        Price += ProbabilityPrices[PriceIndex];
        Symbol <<= 1;

    } while (Symbol < 0x10000);

    return Price;
}

ULONG
LzpLiteralEncoderGetPriceMatched (
    PLZ_PROB Probabilities,
    ULONG Symbol,
    ULONG MatchByte,
    PULONG ProbabilityPrices
    )

/*++

Routine Description:

    This routine returns the price for a given matched literal.

Arguments:

    Probabilities - Supplies a pointer to the symbol probabilities.

    Symbol - Supplies the symbol to output.

    MatchByte - Supplies the byte to match against.

    ProbabilityPrices - Supplies a pointer to the probability prices array.

Return Value:

    Returns the price for the given literal.

--*/

{

    ULONG Offset;
    ULONG Price;
    ULONG PriceIndex;

    Price = 0;
    Offset = 0x100;
    Symbol |= 0x100;

    //
    // Mark the end bit, and then march through all 8 lower bits.
    //

    do {
        MatchByte <<= 1;
        PriceIndex = LZP_GET_PRICE_INDEX(
            Probabilities[Offset + (MatchByte & Offset) + (Symbol >> 8)],
            (Symbol >> 7) & 0x1);

        Price += ProbabilityPrices[PriceIndex];
        Symbol <<= 1;
        Offset &= ~(MatchByte ^ Symbol);

    } while (Symbol < 0x10000);

    return Price;
}

ULONG
LzpLzmaGetRepPrice (
    PLZMA_ENCODER Encoder,
    ULONG RepIndex,
    ULONG Length,
    ULONG State,
    ULONG PositionState
    )

/*++

Routine Description:

    This routine returns the price for outputting a rep.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    RepIndex - Supplies the rep number this is.

    Length - Supplies the rep length.

    State - Supplies the current state.

    PositionState - Supplies the current position state.

Return Value:

    Returns the price for the described rep.

--*/

{

    ULONG Price;
    PLZMA_LENGTH_PRICE_ENCODER RepLengthEncoder;

    RepLengthEncoder = &(Encoder->RepLengthEncoder);
    Price =
        RepLengthEncoder->Prices[PositionState][Length - LZMA_MIN_MATCH_LENGTH];

    Price += LzpLzmaGetPureRepPrice(Encoder, RepIndex, State, PositionState);
    return Price;
}

ULONG
LzpLzmaGetPureRepPrice (
    PLZMA_ENCODER Encoder,
    ULONG RepIndex,
    ULONG State,
    ULONG PositionState
    )

/*++

Routine Description:

    This routine returns the price for outputting a rep without worrying
    about the length.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    RepIndex - Supplies the rep number this is.

    State - Supplies the current state.

    PositionState - Supplies the current position state.

Return Value:

    Returns the price for the described rep.

--*/

{

    ULONG Price;

    Price = 0;
    if (RepIndex == 0) {
        Price = LZP_GET_PRICE(Encoder, Encoder->IsRepG0[State], 0);
        Price += LZP_GET_PRICE(Encoder,
                               Encoder->IsRep0Long[State][PositionState],
                               1);

    } else {
        Price = LZP_GET_PRICE(Encoder, Encoder->IsRepG0[State], 1);
        if (RepIndex == 1) {
            Price += LZP_GET_PRICE(Encoder, Encoder->IsRepG1[State], 0);

        } else {
            Price += LZP_GET_PRICE(Encoder, Encoder->IsRepG1[State], 1);
            Price += LZP_GET_PRICE(Encoder,
                                   Encoder->IsRepG2[State],
                                   RepIndex - 2);
        }
    }

    return Price;
}

ULONG
LzpLzmaGetRepLen1Price (
    PLZMA_ENCODER Encoder,
    ULONG State,
    ULONG PositionState
    )

/*++

Routine Description:

    This routine returns the price for outputting a short rep.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    State - Supplies the current state.

    PositionState - Supplies the current position state.

Return Value:

    Returns the price for the described short rep.

--*/

{

    ULONG Price;

    Price = LZP_GET_PRICE(Encoder, Encoder->IsRepG0[State], 0);
    Price += LZP_GET_PRICE(Encoder,
                           Encoder->IsRep0Long[State][PositionState],
                           0);

    return Price;
}

