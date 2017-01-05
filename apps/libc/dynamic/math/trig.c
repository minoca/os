/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    trig.c

Abstract:

    This module implements the base trignometric mathematical functions (sine,
    cosine, and tangent).

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Evan Green 23-Jul-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../libcp.h"
#include "mathp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SINE_SMALL_VALUE_HIGH_WORD 0x3E500000
#define COSINE_SMALL_VALUE_HIGH_WORD 0x3E46A09E
#define PI_OVER_2_MEDIUM_HIGH_WORD_LIMIT 0x413921FB
#define TANGENT_THRESHOLD_HIGH_WORD 0x3FE59428
#define TANGENT_LOWER_LIMIT_HIGH_WORD 0x3E400000

#define MATH_3_PI_OVER_4_HIGH_WORD 0x4002D97C
#define MATH_9_PI_OVER_4_HIGH_WORD 0x401C463B
#define MATH_7_PI_OVER_4_HIGH_WORD 0x4015FDBC
#define MATH_3_PI_OVER_2_HIGH_WORD 0x4012D97C
#define MATH_4_PI_OVER_2_HIGH_WORD 0x401921FB
#define PI_OVER_TWO_HIGH_WORD_VALUE 0x000921FB
#define MEDIUM_SIZED_ROUNDING_VALUE 0x1.8p52

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

double
ClpSineDouble (
    double Value,
    double Tail,
    BOOL TailValid
    );

double
ClpCosineDouble (
    double Value,
    double Tail
    );

double
ClpTangentDouble (
    double Value,
    double Tail,
    INT TailAndSign
    );

INT
ClpRemovePiOver2 (
    double Value,
    double Remainder[2]
    );

INT
ClpSubtractPiOver2Multiple (
    double Value,
    BOOL Positive,
    int Multiplier,
    double Remainder[2]
    );

INT
ClpRemovePiOver2Big (
    double *Input,
    double Output[3],
    LONG InputExponent,
    LONG InputCount,
    FLOATING_PRECISION Precision
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the coefficients of the polynomial used to compute sine.
//

const double ClSine1 = -1.66666666666666324348e-01;
const double ClSine2 = 8.33333333332248946124e-03;
const double ClSine3 = -1.98412698298579493134e-04;
const double ClSine4 = 2.75573137070700676789e-06;
const double ClSine5 = -2.50507602534068634195e-08;
const double ClSine6 = 1.58969099521155010221e-10;

//
// Define the coefficients of the polynomial used to compute cosine.
//

const double ClCosine0 = -4.99999997251031003120e-01;
const double ClCosine1 = 4.16666666666666019037e-02;
const double ClCosine2 = -1.38888888888741095749e-03;
const double ClCosine3 = 2.48015872894767294178e-05;
const double ClCosine4 = -2.75573143513906633035e-07;
const double ClCosine5 = 2.08757232129817482790e-09;
const double ClCosine6 = -1.13596475577881948265e-11;

//
// Define the coefficients of the polynomial used to compute the tangent
// function.
//

const double ClTangent[13] = {
    3.33333333333334091986e-01,
    1.33333333333201242699e-01,
    5.39682539762260521377e-02,
    2.18694882948595424599e-02,
    8.86323982359930005737e-03,
    3.59207910759131235356e-03,
    1.45620945432529025516e-03,
    5.88041240820264096874e-04,
    2.46463134818469906812e-04,
    7.81794442939557092300e-05,
    7.14072491382608190305e-05,
    -1.85586374855275456654e-05,
    2.59073051863633712884e-05,
};

const double ClTangentFloat[6] = {
    0x15554d3418c99f.0p-54,
    0x1112fd38999f72.0p-55,
    0x1b54c91d865afe.0p-57,
    0x191df3908c33ce.0p-58,
    0x185dadfcecf44e.0p-61,
    0x1362b9bf971bcd.0p-59,
};

//
// Define some basic constants. The tail is the result of subtracting the
// pi over 2 variable from the real pi/2. The 2 and 3 versions define the next
// least significant bits for pi/2.
//

const double ClPiOverTwo1 = 1.57079632673412561417e+00;
const double ClPiOverTwo1Tail = 6.07710050650619224932e-11;
const double ClPiOverTwo2 = 6.07710050630396597660e-11;
const double ClPiOverTwo2Tail = 2.02226624879595063154e-21;
const double ClPiOverTwo3 = 2.02226624871116645580e-21;
const double ClPiOverTwo3Tail = 8.47842766036889956997e-32;
const double ClInversePiOverTwo = 6.36619772367581382433e-01;
const double ClTwo24 = 1.67772160000000000000e+07;
const double ClTwoNegative24 = 5.96046447753906250000e-08;

//
// Define the continuing bits of pi/2.
//

const double ClPiOver2[8] = {
    1.57079625129699707031e+00,
    7.54978941586159635335e-08,
    5.39030252995776476554e-15,
    3.28200341580791294123e-22,
    1.27065575308067607349e-29,
    1.22933308981111328932e-36,
    2.73370053816464559624e-44,
    2.16741683877804819444e-51,
};

//
// Define the initial number of pi/2 terms to use for a given precision level.
//

INT ClPiOverTwoInitialTermCount[FloatingPrecisionCount] = {3, 4, 4, 6};

//
// Define a table of constants for 2/pi. This is an integer array, each
// element containing 24 bits of 2/pi after the binary point. The corresponding
// floating value is array[i] * 2^(-24 * (i + 1)).
// Note that this table must have at least (Exponent - 3) / 24 + InitialCount
// terms.
//

const ULONG ClTwoOverPiIntegers[] = {
    0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62,
    0x95993C, 0x439041, 0xFE5163, 0xABDEBB, 0xC561B7, 0x246E3A,
    0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129,
    0xA73EE8, 0x8235F5, 0x2EBB44, 0x84E99C, 0x7026B4, 0x5F7E41,
    0x3991D6, 0x398353, 0x39F49C, 0x845F8B, 0xBDF928, 0x3B1FF8,
    0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D, 0x367ECF,
    0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5,
    0xF17B3D, 0x0739F7, 0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08,
    0x560330, 0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3,
    0x91615E, 0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880,
    0x4D7327, 0x310606, 0x1556CA, 0x73A8C9, 0x60E27B, 0xC08C6B,

#if LDBL_MAX_EXP > 1024

#if LDBL_MAX_EXP > 16384

#error "2/Pi integer table needs to be expanded"

#endif

    0x47C419, 0xC367CD, 0xDCE809, 0x2A8359, 0xC4768B, 0x961CA6,
    0xDDAF44, 0xD15719, 0x053EA5, 0xFF0705, 0x3F7E33, 0xE832C2,
    0xDE4F98, 0x327DBB, 0xC33D26, 0xEF6B1E, 0x5EF89F, 0x3A1F35,
    0xCAF27F, 0x1D87F1, 0x21907C, 0x7C246A, 0xFA6ED5, 0x772D30,
    0x433B15, 0xC614B5, 0x9D19C3, 0xC2C4AD, 0x414D2C, 0x5D000C,
    0x467D86, 0x2D71E3, 0x9AC69B, 0x006233, 0x7CD2B4, 0x97A7B4,
    0xD55537, 0xF63ED7, 0x1810A3, 0xFC764D, 0x2A9D64, 0xABD770,
    0xF87C63, 0x57B07A, 0xE71517, 0x5649C0, 0xD9D63B, 0x3884A7,
    0xCB2324, 0x778AD6, 0x23545A, 0xB91F00, 0x1B0AF1, 0xDFCE19,
    0xFF319F, 0x6A1E66, 0x615799, 0x47FBAC, 0xD87F7E, 0xB76522,
    0x89E832, 0x60BFE6, 0xCDC4EF, 0x09366C, 0xD43F5D, 0xD7DE16,
    0xDE3B58, 0x929BDE, 0x2822D2, 0xE88628, 0x4D58E2, 0x32CAC6,
    0x16E308, 0xCB7DE0, 0x50C017, 0xA71DF3, 0x5BE018, 0x34132E,
    0x621283, 0x014883, 0x5B8EF5, 0x7FB0AD, 0xF2E91E, 0x434A48,
    0xD36710, 0xD8DDAA, 0x425FAE, 0xCE616A, 0xA4280A, 0xB499D3,
    0xF2A606, 0x7F775C, 0x83C2A3, 0x883C61, 0x78738A, 0x5A8CAF,
    0xBDD76F, 0x63A62D, 0xCBBFF4, 0xEF818D, 0x67C126, 0x45CA55,
    0x36D9CA, 0xD2A828, 0x8D61C2, 0x77C912, 0x142604, 0x9B4612,
    0xC459C4, 0x44C5C8, 0x91B24D, 0xF31700, 0xAD43D4, 0xE54929,
    0x10D5FD, 0xFCBE00, 0xCC941E, 0xEECE70, 0xF53E13, 0x80F1EC,
    0xC3E7B3, 0x28F8C7, 0x940593, 0x3E71C1, 0xB3092E, 0xF3450B,
    0x9C1288, 0x7B20AB, 0x9FB52E, 0xC29247, 0x2F327B, 0x6D550C,
    0x90A772, 0x1FE76B, 0x96CB31, 0x4A1679, 0xE27941, 0x89DFF4,
    0x9794E8, 0x84E6E2, 0x973199, 0x6BED88, 0x365F5F, 0x0EFDBB,
    0xB49A48, 0x6CA467, 0x427271, 0x325D8D, 0xB8159F, 0x09E5BC,
    0x25318D, 0x3974F7, 0x1C0530, 0x010C0D, 0x68084B, 0x58EE2C,
    0x90AA47, 0x02E774, 0x24D6BD, 0xA67DF7, 0x72486E, 0xEF169F,
    0xA6948E, 0xF691B4, 0x5153D1, 0xF20ACF, 0x339820, 0x7E4BF5,
    0x6863B2, 0x5F3EDD, 0x035D40, 0x7F8985, 0x295255, 0xC06437,
    0x10D86D, 0x324832, 0x754C5B, 0xD4714E, 0x6E5445, 0xC1090B,
    0x69F52A, 0xD56614, 0x9D0727, 0x50045D, 0xDB3BB4, 0xC576EA,
    0x17F987, 0x7D6B49, 0xBA271D, 0x296996, 0xACCCC6, 0x5414AD,
    0x6AE290, 0x89D988, 0x50722C, 0xBEA404, 0x940777, 0x7030F3,
    0x27FC00, 0xA871EA, 0x49C266, 0x3DE064, 0x83DD97, 0x973FA3,
    0xFD9443, 0x8C860D, 0xDE4131, 0x9D3992, 0x8C70DD, 0xE7B717,
    0x3BDF08, 0x2B3715, 0xA0805C, 0x93805A, 0x921110, 0xD8E80F,
    0xAF806C, 0x4BFFDB, 0x0F9038, 0x761859, 0x15A562, 0xBBCB61,
    0xB989C7, 0xBD4010, 0x04F2D2, 0x277549, 0xF6B6EB, 0xBB22DB,
    0xAA140A, 0x2F2689, 0x768364, 0x333B09, 0x1A940E, 0xAA3A51,
    0xC2A31D, 0xAEEDAF, 0x12265C, 0x4DC26D, 0x9C7A2D, 0x9756C0,
    0x833F03, 0xF6F009, 0x8C402B, 0x99316D, 0x07B439, 0x15200C,
    0x5BC3D8, 0xC492F5, 0x4BADC6, 0xA5CA4E, 0xCD37A7, 0x36A9E6,
    0x9492AB, 0x6842DD, 0xDE6319, 0xEF8C76, 0x528B68, 0x37DBFC,
    0xABA1AE, 0x3115DF, 0xA1AE00, 0xDAFB0C, 0x664D64, 0xB705ED,
    0x306529, 0xBF5657, 0x3AFF47, 0xB9F96A, 0xF3BE75, 0xDF9328,
    0x3080AB, 0xF68C66, 0x15CB04, 0x0622FA, 0x1DE4D9, 0xA4B33D,
    0x8F1B57, 0x09CD36, 0xE9424E, 0xA4BE13, 0xB52333, 0x1AAAF0,
    0xA8654F, 0xA5C1D2, 0x0F3F0B, 0xCD785B, 0x76F923, 0x048B7B,
    0x721789, 0x53A6C6, 0xE26E6F, 0x00EBEF, 0x584A9B, 0xB7DAC4,
    0xBA66AA, 0xCFCF76, 0x1D02D1, 0x2DF1B1, 0xC1998C, 0x77ADC3,
    0xDA4886, 0xA05DF7, 0xF480C6, 0x2FF0AC, 0x9AECDD, 0xBC5C3F,
    0x6DDED0, 0x1FC790, 0xB6DB2A, 0x3A25A3, 0x9AAF00, 0x9353AD,
    0x0457B6, 0xB42D29, 0x7E804B, 0xA707DA, 0x0EAA76, 0xA1597B,
    0x2A1216, 0x2DB7DC, 0xFDE5FA, 0xFEDB89, 0xFDBE89, 0x6C76E4,
    0xFCA906, 0x70803E, 0x156E85, 0xFF87FD, 0x073E28, 0x336761,
    0x86182A, 0xEABD4D, 0xAFE7B3, 0x6E6D8F, 0x396795, 0x5BBF31,
    0x48D784, 0x16DF30, 0x432DC7, 0x356125, 0xCE70C9, 0xB8CB30,
    0xFD6CBF, 0xA200A4, 0xE46C05, 0xA0DD5A, 0x476F21, 0xD21262,
    0x845CB9, 0x496170, 0xE0566B, 0x015299, 0x375550, 0xB7D51E,
    0xC4F133, 0x5F6E13, 0xE4305D, 0xA92E85, 0xC3B21D, 0x3632A1,
    0xA4B708, 0xD4B1EA, 0x21F716, 0xE4698F, 0x77FF27, 0x80030C,
    0x2D408D, 0xA0CD4F, 0x99A520, 0xD3A2B3, 0x0A5D2F, 0x42F9B4,
    0xCBDA11, 0xD0BE7D, 0xC1DB9B, 0xBD17AB, 0x81A2CA, 0x5C6A08,
    0x17552E, 0x550027, 0xF0147F, 0x8607E1, 0x640B14, 0x8D4196,
    0xDEBE87, 0x2AFDDA, 0xB6256B, 0x34897B, 0xFEF305, 0x9EBFB9,
    0x4F6A68, 0xA82A4A, 0x5AC44F, 0xBCF82D, 0x985AD7, 0x95C7F4,
    0x8D4D0D, 0xA63A20, 0x5F57A4, 0xB13F14, 0x953880, 0x0120CC,
    0x86DD71, 0xB6DEC9, 0xF560BF, 0x11654D, 0x6B0701, 0xACB08C,
    0xD0C0B2, 0x485551, 0x0EFB1E, 0xC37295, 0x3B06A3, 0x3540C0,
    0x7BDC06, 0xCC45E0, 0xFA294E, 0xC8CAD6, 0x41F3E8, 0xDE647C,
    0xD8649B, 0x31BED9, 0xC397A4, 0xD45877, 0xC5E369, 0x13DAF0,
    0x3C3ABA, 0x461846, 0x5F7555, 0xF5BDD2, 0xC6926E, 0x5D2EAC,
    0xED440E, 0x423E1C, 0x87C461, 0xE9FD29, 0xF3D6E7, 0xCA7C22,
    0x35916F, 0xC5E008, 0x8DD7FF, 0xE26A6E, 0xC6FDB0, 0xC10893,
    0x745D7C, 0xB2AD6B, 0x9D6ECD, 0x7B723E, 0x6A11C6, 0xA9CFF7,
    0xDF7329, 0xBAC9B5, 0x5100B7, 0x0DB2E2, 0x24BA74, 0x607DE5,
    0x8AD874, 0x2C150D, 0x0C1881, 0x94667E, 0x162901, 0x767A9F,
    0xBEFDFD, 0xEF4556, 0x367ED9, 0x13D9EC, 0xB9BA8B, 0xFC97C4,
    0x27A831, 0xC36EF1, 0x36C594, 0x56A8D8, 0xB5A8B4, 0x0ECCCF,
    0x2D8912, 0x34576F, 0x89562C, 0xE3CE99, 0xB920D6, 0xAA5E6B,
    0x9C2A3E, 0xCC5F11, 0x4A0BFD, 0xFBF4E1, 0x6D3B8E, 0x2C86E2,
    0x84D4E9, 0xA9B4FC, 0xD1EEEF, 0xC9352E, 0x61392F, 0x442138,
    0xC8D91B, 0x0AFC81, 0x6A4AFB, 0xD81C2F, 0x84B453, 0x8C994E,
    0xCC2254, 0xDC552A, 0xD6C6C0, 0x96190B, 0xB8701A, 0x649569,
    0x605A26, 0xEE523F, 0x0F117F, 0x11B5F4, 0xF5CBFC, 0x2DBC34,
    0xEEBC34, 0xCC5DE8, 0x605EDD, 0x9B8E67, 0xEF3392, 0xB817C9,
    0x9B5861, 0xBC57E1, 0xC68351, 0x103ED8, 0x4871DD, 0xDD1C2D,
    0xA118AF, 0x462C21, 0xD7F359, 0x987AD9, 0xC0549E, 0xFA864F,
    0xFC0656, 0xAE79E5, 0x362289, 0x22AD38, 0xDC9367, 0xAAE855,
    0x382682, 0x9BE7CA, 0xA40D51, 0xB13399, 0x0ED7A9, 0x480569,
    0xF0B265, 0xA7887F, 0x974C88, 0x36D1F9, 0xB39221, 0x4A827B,
    0x21CF98, 0xDC9F40, 0x5547DC, 0x3A74E1, 0x42EB67, 0xDF9DFE,
    0x5FD45E, 0xA4677B, 0x7AACBA, 0xA2F655, 0x23882B, 0x55BA41,
    0x086E59, 0x862A21, 0x834739, 0xE6E389, 0xD49EE5, 0x40FB49,
    0xE956FF, 0xCA0F1C, 0x8A59C5, 0x2BFA94, 0xC5C1D3, 0xCFC50F,
    0xAE5ADB, 0x86C547, 0x624385, 0x3B8621, 0x94792C, 0x876110,
    0x7B4C2A, 0x1A2C80, 0x12BF43, 0x902688, 0x893C78, 0xE4C4A8,
    0x7BDBE5, 0xC23AC4, 0xEAF426, 0x8A67F7, 0xBF920D, 0x2BA365,
    0xB1933D, 0x0B7CBD, 0xDC51A4, 0x63DD27, 0xDDE169, 0x19949A,
    0x9529A8, 0x28CE68, 0xB4ED09, 0x209F44, 0xCA984E, 0x638270,
    0x237C7E, 0x32B90F, 0x8EF5A7, 0xE75614, 0x08F121, 0x2A9DB5,
    0x4D7E6F, 0x5119A5, 0xABF9B5, 0xD6DF82, 0x61DD96, 0x023616,
    0x9F3AC4, 0xA1A283, 0x6DED72, 0x7A8D39, 0xA9B882, 0x5C326B,
    0x5B2746, 0xED3400, 0x7700D2, 0x55F4FC, 0x4D5901, 0x8071E0,

#endif

};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
sin (
    double Value
    )

/*++

Routine Description:

    This routine returns the sine of the given value.

Arguments:

    Value - Supplies the value to compute the sine of, in radians.

Return Value:

    Returns the sine of the value.

--*/

{

    ULONG HighWord;
    double Output[2];
    INT PiOver2Count;
    DOUBLE_PARTS ValueParts;

    ValueParts.Double = Value;
    HighWord = ValueParts.Ulong.High &
               ((~DOUBLE_SIGN_BIT) >> DOUBLE_HIGH_WORD_SHIFT);

    //
    // See if the absolute value of x is less than pi/4. If it is, the
    // calculation can be done directly. If it's too small, sin approximates to
    // the value itself.
    //

    if (HighWord <= PI_OVER_4_HIGH_WORD) {
        if (HighWord < SINE_SMALL_VALUE_HIGH_WORD) {

            //
            // Generate an inexact exception.
            //

            if ((INT)Value == 0) {
                return Value;
            }
        }

        return ClpSineDouble(Value, 0.0, FALSE);
    }

    //
    // Sine of infinity or NaN is NaN.
    //

    if (HighWord >= NAN_HIGH_WORD) {
        return Value - Value;
    }

    //
    // Range reduction is needed to get the value near pi/4.
    //

    PiOver2Count = ClpRemovePiOver2(Value, Output);
    switch (PiOver2Count & 3) {
    case 0:
        return ClpSineDouble(Output[0], Output[1], TRUE);

    case 1:
        return ClpCosineDouble(Output[0], Output[1]);

    case 2:
        return -ClpSineDouble(Output[0], Output[1], TRUE);

    case 3:
        break;
    }

    return -ClpCosineDouble(Output[0], Output[1]);
}

LIBC_API
double
cos (
    double Value
    )

/*++

Routine Description:

    This routine returns the cosine of the given value.

Arguments:

    Value - Supplies the value to compute the cosine of, in radians.

Return Value:

    Returns the cosine of the value.

--*/

{

    LONG HighWord;
    double Output[2];
    LONG PiOver2Count;
    DOUBLE_PARTS ValueParts;

    ValueParts.Double = Value;
    HighWord = ValueParts.Ulong.High &
               ((~DOUBLE_SIGN_BIT) >> DOUBLE_HIGH_WORD_SHIFT);

    //
    // The computation can be done directly with values less than or equal to
    // pi/4.
    //

    if (HighWord <= PI_OVER_4_HIGH_WORD) {

        //
        // For very very small angles, cosine is pretty much just one.
        //

        if (HighWord < COSINE_SMALL_VALUE_HIGH_WORD) {

            //
            // Generate an inexact condition.
            //

            if (((int)Value) == 0) {
                return 1.0;
            }
        }

        return ClpCosineDouble(Value, 0.0);

    //
    // Cosine of infinity or NaN is NaN.
    //

    } else if (HighWord >= NAN_HIGH_WORD) {
        return Value - Value;
    }

    //
    // Range reduction is needed to get the value near pi/4.
    //

    PiOver2Count = ClpRemovePiOver2(Value, Output);
    switch (PiOver2Count & 3) {
    case 0:
        return ClpCosineDouble(Output[0], Output[1]);

    case 1:
        return -ClpSineDouble(Output[0], Output[1], TRUE);

    case 2:
        return -ClpCosineDouble(Output[0], Output[1]);

    default:
        break;
    }

    return ClpSineDouble(Output[0], Output[1], 1);
}

LIBC_API
double
tan (
    double Value
    )

/*++

Routine Description:

    This routine returns the tangent of the given value.

Arguments:

    Value - Supplies the value to compute the tangent of, in radians.

Return Value:

    Returns the cosine of the value.

--*/

{

    LONG HighWord;
    double Output[2];
    LONG PiOver2Count;
    INT TailAndSign;
    DOUBLE_PARTS ValueParts;

    ValueParts.Double = Value;
    HighWord = ValueParts.Ulong.High &
               (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    //
    // If the absolute value is near pi/4, no fuss is needed.
    //

    if (HighWord <= PI_OVER_4_HIGH_WORD) {

        //
        // If the value if too small, generate an inexact condition.
        //

        if (HighWord < TANGENT_LOWER_LIMIT_HIGH_WORD) {
            if ((int)Value == 0) {
                return Value;
            }
        }

        return ClpTangentDouble(Value, 0.0, 1);

    //
    // Tangent of infinity or NaN is NaN.
    //

    } else if (HighWord >= NAN_HIGH_WORD) {
        return Value - Value;
    }

    //
    // Range reduction is needed here.
    //

    PiOver2Count = ClpRemovePiOver2(Value, Output);

    //
    // Compute regular tangent on even quadrants, and -1/tangent on odd
    // quadrants.
    //

    TailAndSign = 1;
    if ((PiOver2Count & 0x1) != 0) {
        TailAndSign = -1;
    }

    return ClpTangentDouble(Output[0], Output[1], TailAndSign);
}

//
// --------------------------------------------------------- Internal Functions
//

double
ClpSineDouble (
    double Value,
    double Tail,
    BOOL TailValid
    )

/*++

Routine Description:

    This routine implements the sine function along the range of
    [-pi/4 to pi/4].

Arguments:

    Value - Supplies the value between -pi/4 and pi/4.

    Tail - Supplies the tail of the value.

    TailValid - Supplies a boolean indicating if the tail is valid (TRUE) or
        assumed to be zero (FALSE).

Return Value:

    Returns the sine of the given value.

--*/

{

    double Sine;
    double UpperDegrees;
    double Value2;
    double Value3;
    double Value4;

    //
    // Approximate sine with a polynomial of degree 13 between 0 to pi/4:
    // x + A * x^3 + ... + F * x^13
    //
    // where A through F are the "sine double" constants and x is Value. This
    // can be rearranged in a way that works better for rounding errors:
    // r = x^3 * (B + x^2 * (C + x^2 * (D + x^2 * (E + x^2 * F))))
    //
    // Then add in the tailings.
    // sin(x+y) ~ sin(x) + (1 - (x^2)/2) * y
    //  ~ x + (A* x^3 + (x^2 * (r - (y / 2)) + y))
    //

    Value2 = Value * Value;
    Value4 = Value2 * Value2;
    UpperDegrees = ClSine2 +
                   Value2 * (ClSine3 + Value2 * ClSine4) +
                   (Value2 * Value4 *
                    (ClSine5 + Value2 * ClSine6));

    Value3 = Value2 * Value;
    if (TailValid == FALSE) {
        Sine = Value + Value3 * (ClSine1 + Value2 * UpperDegrees);

    } else {
        Sine = Value -
               ((Value2 * (ClDoubleOneHalf * Tail - Value3 * UpperDegrees) -
                 Tail) -
                Value3 * ClSine1);
    }

    return Sine;
}

double
ClpCosineDouble (
    double Value,
    double Tail
    )

/*++

Routine Description:

    This routine implements the cosine function along the range of
    [-pi/4 to pi/4].

Arguments:

    Value - Supplies the value between -pi/4 and pi/4.

    Tail - Supplies the tail of the value.

Return Value:

    Returns the sine of the given value.

--*/

{

    double Flipped;
    double Result;
    double UpperDegrees;
    double Value2;
    double Value2Over2;
    double Value4;

    Value2 = Value * Value;
    Value4 = Value2 * Value2;
    UpperDegrees = Value2 *
                   (ClCosine1 + Value2 * (ClCosine2 + Value2 * ClCosine3)) +
                   Value4 * Value4 * (ClCosine4 + Value2 *
                                      (ClCosine5 + Value2 * ClCosine6));

    Value2Over2 = ClDoubleOneHalf * Value2;
    Flipped = ClDoubleOne - Value2Over2;
    Result = Flipped + (((ClDoubleOne - Flipped) - Value2Over2) +
             (Value2 * UpperDegrees - Value * Tail));

    return Result;
}

double
ClpTangentDouble (
    double Value,
    double Tail,
    INT TailAndSign
    )

/*++

Routine Description:

    This routine implements the tangent function along the range of
    [-pi/4 to pi/4].

Arguments:

    Value - Supplies the value between -pi/4 and pi/4.

    Tail - Supplies the tail of the value.

    TailAndSign - Supplies zero if the tail is assumed to be zero, 1 if the
        positive tangent is to be computed, and -1 if -1/tangent is to be
        computed.

Return Value:

    Returns the tangent of the given value.

--*/

{

    LONG AbsoluteHighWord;
    double Evens;
    LONG HighWord;
    double InverseTangent;
    double InverseTangentHigh;
    double Odds;
    double Sign;
    double Tangent;
    double TangentHigh;
    double TangentTerms;
    double Value2;
    double Value3;
    double Value4;
    DOUBLE_PARTS ValueParts;
    double ValueSign;

    ValueParts.Double = Value;
    HighWord = ValueParts.Ulong.High;
    AbsoluteHighWord = HighWord & (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    //
    // For values above a certain threshold near pi/4, let y = pi/4 - Value.
    // Then:
    // tan(x) = tan(pi/4-y) = (1-tan(y)) / (1+tan(y))) =
    // 1 + 2*tan(y) - (tan(y)^2) / (1+tan(y)))
    //

    if (AbsoluteHighWord >= TANGENT_THRESHOLD_HIGH_WORD) {
        if (HighWord < 0) {
            Value = -Value;
            Tail = -Tail;
        }

        Value2 = ClPiOver4 - Value;
        Value4 = ClPiOver4Tail - Tail;
        Value = Value2 + Value4;
        Tail = 0.0;
    }

    Value2 = Value * Value;
    Value4 = Value2 * Value2;

    //
    // Break Value5 * (T[1] + Value2 * T[2] + ...) into
    // Value5 * (T[1] + Value4 * T[3] + ... + Value20 * T[11]) +
    // Value5 * (Value2 * (T[2] + Value4 * T[4] + ... + Value22 * T[12])).
    // Otherwise the parentheses just get maddening.
    //

    Odds = ClTangent[1] + Value4 *
           (ClTangent[3] + Value4 *
            (ClTangent[5] + Value4 *
             (ClTangent[7] + Value4 *
              (ClTangent[9] + Value4 * ClTangent[11]))));

    Evens = Value2 *
            (ClTangent[2] + Value4 *
             (ClTangent[4] + Value4 *
              (ClTangent[6] + Value4 *
               (ClTangent[8] + Value4 *
                (ClTangent[10] + Value4 * ClTangent[12])))));

    Value3 = Value2 * Value;
    TangentTerms = Tail + Value2 * (Value3 * (Odds + Evens) + Tail);
    TangentTerms += ClTangent[0] * Value3;
    Tangent = Value + TangentTerms;
    if (AbsoluteHighWord >= TANGENT_THRESHOLD_HIGH_WORD) {
        Sign = (double)TailAndSign;
        ValueSign = 1.0;
        if ((HighWord & (DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT)) != 0) {
            ValueSign = -1.0;
        }

        Tangent = ValueSign *
                  (Sign - 2.0 *
                   (Value - (Tangent * Tangent / (Tangent + Sign) -
                             TangentTerms)));

        return Tangent;
    }

    if (TailAndSign == 1) {
        return Tangent;
    }

    //
    // Compute -1.0 / (Value + Odds) accurately.
    //

    ValueParts.Double = Tangent;
    ValueParts.Ulong.Low = 0;
    TangentHigh = ValueParts.Double;

    //
    // TangentHigh + Events = Odds + Value.
    //

    Evens = TangentTerms - (TangentHigh - Value);
    InverseTangent = -1.0 / Tangent;
    ValueParts.Double = InverseTangent;
    ValueParts.Ulong.Low = 0;
    InverseTangentHigh = ValueParts.Double;
    Value3 = 1.0 + InverseTangentHigh * TangentHigh;
    Tangent = InverseTangentHigh + InverseTangent *
              (Value3 + InverseTangentHigh * Evens);

    return Tangent;
}

INT
ClpRemovePiOver2 (
    double Value,
    double Remainder[2]
    )

/*++

Routine Description:

    This routine removes multiples of pi/2 from the given value.

Arguments:

    Value - Supplies the value to reduce.

    Remainder - Supplies a pointer where the remainder of the number after
        reducing pi/2 will be returned (sum the two values).

Return Value:

    Returns the value divided by pi/2.

--*/

{

    LONG Exponent;
    LONG ExponentDifference;
    double Extra;
    LONG HighWord;
    double Input[3];
    ULONG InputCount;
    ULONG InputIndex;
    LONG LowWord;
    double Output[3];
    LONG PiOver2Count;
    BOOL Positive;
    double PreviousExtra;
    DOUBLE_PARTS RaisedValue;
    ULONG RemainderHighWord;
    DOUBLE_PARTS RemainderParts;
    double RoundedValue;
    double Tail;
    DOUBLE_PARTS ValueParts;
    volatile double VolatileValue;

    ValueParts.Double = Value;
    HighWord = ValueParts.Ulong.High &
               (~(DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT));

    Positive = TRUE;
    if ((ValueParts.Ulonglong & DOUBLE_SIGN_BIT) != 0) {
        Positive = FALSE;
    }

    //
    // Use the small version if the value is less than 5pi/4 and is not close
    // to pi or pi/2.
    //

    if ((HighWord <= MATH_5_PI_OVER_4_HIGH_WORD) &&
        ((HighWord & DOUBLE_HIGH_VALUE_MASK) != PI_OVER_TWO_HIGH_WORD_VALUE)) {

        //
        // If the absolute value is less than 3pi/4, just subtract.
        //

        if (HighWord <= MATH_3_PI_OVER_4_HIGH_WORD) {
            return ClpSubtractPiOver2Multiple(Value, Positive, 1, Remainder);

        //
        // The absolute value is between 3pi/4 and 5pi/4.
        //

        } else {
            return ClpSubtractPiOver2Multiple(Value, Positive, 2, Remainder);
        }

    //
    // Also special case things less than 9pi/4. 3pi/2 has cancellation
    // problems, so use the medium method there.
    //

    } else if ((HighWord <= MATH_3_PI_OVER_4_HIGH_WORD) &&
               (HighWord != MATH_3_PI_OVER_2_HIGH_WORD) &&
               (HighWord != MATH_4_PI_OVER_2_HIGH_WORD)) {

        if (HighWord <= MATH_7_PI_OVER_4_HIGH_WORD) {
            return ClpSubtractPiOver2Multiple(Value, Positive, 3, Remainder);

        } else {
            return ClpSubtractPiOver2Multiple(Value, Positive, 4, Remainder);
        }
    }

    //
    // Use the medium size for value less than 2^20 * (pi/2), or the bad
    // cancellation cases before.
    //

    if (HighWord < PI_OVER_2_MEDIUM_HIGH_WORD_LIMIT) {
        RoundedValue = Value * ClInversePiOverTwo + MEDIUM_SIZED_ROUNDING_VALUE;
        VolatileValue = RoundedValue;
        RoundedValue = VolatileValue;
        RoundedValue = RoundedValue - MEDIUM_SIZED_ROUNDING_VALUE;
        PiOver2Count = (LONG)RoundedValue;
        Extra = Value - RoundedValue * ClPiOverTwo1;
        Tail = RoundedValue * ClPiOverTwo1Tail;
        Exponent = HighWord >>
                   (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);

        Remainder[0] = Extra - Tail;
        RemainderParts.Double = Remainder[0];
        RemainderHighWord = RemainderParts.Ulong.High &
                            (~(DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT));

        ExponentDifference =
                 Exponent - (RemainderHighWord >>
                             (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT));

        //
        // For a really high exponent, another iteration is needed. This is good
        // to 118 bits.
        //

        if (ExponentDifference > 16) {
            PreviousExtra = Extra;
            Tail = RoundedValue * ClPiOverTwo2;
            Extra = PreviousExtra - Tail;
            Tail = RoundedValue * ClPiOverTwo2Tail -
                   ((PreviousExtra - Extra) - Tail);

            Remainder[0] = Extra - Tail;
            RemainderParts.Double = Remainder[0];
            RemainderHighWord = RemainderParts.Ulong.High &
                                (~(DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT));

            ExponentDifference =
                 Exponent - (RemainderHighWord >>
                             (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT));

            //
            // Do one final iteration, good for 151 bits of accuracy, which
            // should cover all possible cases.
            //

            if (ExponentDifference > 49) {
                PreviousExtra = Extra;
                Tail = RoundedValue * ClPiOverTwo3;
                Extra = PreviousExtra - Tail;
                Tail = RoundedValue * ClPiOverTwo3Tail -
                       ((PreviousExtra - Extra) - Tail);

                Remainder[0] = Extra - Tail;
                RemainderParts.Double = Remainder[0];
                RemainderHighWord = RemainderParts.Ulong.High &
                                    (~(DOUBLE_SIGN_BIT >>
                                       DOUBLE_HIGH_WORD_SHIFT));
            }
        }

        Remainder[1] = (Extra - Remainder[0]) - Tail;
        return PiOver2Count;
    }

    //
    // Making it this far means this is a very large argument. Deal with the
    // value being infinity or NaN real quick.
    //

    if (HighWord >= NAN_HIGH_WORD) {
        Remainder[0] = Value - Value;
        Remainder[1] = Value - Value;
        return 0;
    }

    //
    // Break the value into three 24-bit integers in double precision format.
    // It looks like this:
    // Exponent = ilogb(Value) - 23
    // RaisedValue = scalbn(Value, -Exponent)
    // for i = 0, 1, 2
    //     Input[i] = floor(RaisedValue)
    //     RaisedValue = (RaisedValue - Input[i]) * 2^24.
    //

    LowWord = ValueParts.Ulong.Low;

    //
    // The exponent is equal to ilogb(Value) - 23.
    //

    Exponent = (HighWord >> (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT)) -
               1046;

    //
    // Set the raised low part to the scalbn(|Value|, ilogb(Value)-23).
    //

    RaisedValue.Ulong.High = HighWord -
                             (Exponent <<
                              (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT));

    RaisedValue.Ulong.Low = LowWord;
    for (InputIndex = 0; InputIndex < 2; InputIndex += 1) {
        Input[InputIndex] = (double)(LONG)RaisedValue.Double;
        RaisedValue.Double = (RaisedValue.Double - Input[InputIndex]) * ClTwo24;
    }

    Input[2] = RaisedValue.Double;

    //
    // Skip zero terms.
    //

    InputCount = 3;
    while ((InputCount >= 1) && (Input[InputCount - 1] == ClDoubleZero)) {
        InputCount -= 1;
    }

    PiOver2Count = ClpRemovePiOver2Big(Input,
                                       Output,
                                       Exponent,
                                       InputCount,
                                       FloatingPrecisionDouble);

    Remainder[0] = Output[0];
    Remainder[1] = Output[1];
    if (Positive == FALSE) {
        Remainder[0] = -Remainder[0];
        Remainder[1] = -Remainder[1];
        return -PiOver2Count;
    }

    return PiOver2Count;
}

INT
ClpSubtractPiOver2Multiple (
    double Value,
    BOOL Positive,
    int Multiplier,
    double Remainder[2]
    )

/*++

Routine Description:

    This routine removes multiples of pi/2 from the given value.

Arguments:

    Value - Supplies the value to reduce.

    Positive - Supplies a boolean indicating if the multiplier should be
        negated.

    Multiplier - Supplies the multiplier of pi/2 to use.

    Remainder - Supplies a pointer where the remainder of the number after
        reducing pi/2 will be returned (sum the two values).

Return Value:

    Returns the multiplier (or the negative multiplier if positive is false).

--*/

{

    double Subtraction;

    if (Positive != FALSE) {
        Subtraction = Value - Multiplier * ClPiOverTwo1;
        Remainder[0] = Subtraction - Multiplier * ClPiOverTwo1Tail;
        Remainder[1] = (Subtraction - Remainder[0]) -
                       Multiplier * ClPiOverTwo1Tail;

        return Multiplier;
    }

    Subtraction = Value + Multiplier * ClPiOverTwo1;
    Remainder[0] = Subtraction + Multiplier * ClPiOverTwo1Tail;
    Remainder[1] = (Subtraction - Remainder[0]) +
                   Multiplier * ClPiOverTwo1Tail;

    return -Multiplier;
}

INT
ClpRemovePiOver2Big (
    double *Input,
    double Output[3],
    LONG InputExponent,
    LONG InputCount,
    FLOATING_PRECISION Precision
    )

/*++

Routine Description:

    This routine removes multiples of pi/2 from the given large value.

Arguments:

    Input - Supplies a pointer to an array of input values, broken up into
        24-bit chunks of floating point values.

    Output - Supplies a pointer where the output remainder will be returned.
        The result comes from adding these values together.

    InputExponent - Supplies the exponent of the first input double.

    InputCount - Supplies the number of elements in the input array.

    Precision - Supplies the precision of the floating point type.

Return Value:

    Returns the number of pi/2s (mod 8) that went into this input.

--*/

{

    LONG Carry;
    LONG EndIndex;
    double Final;
    double FinalProduct[20];
    LONG HighWord;
    LONG Index;
    LONG Index2;
    LONG InitialTermCount;
    double Integral[20];
    LONG IntegralExponent;
    LONG IntegralInteger[20];
    LONG LastInput;
    LONG NeededTerms;
    double PiOver2[20];
    LONG PiOver2Count;
    LONG TableIndex;
    LONG TermCount;
    double Value;
    volatile double VolatileValue;

    //
    // Get the number of terms to start with based on the supplied precision.
    //

    InitialTermCount = ClPiOverTwoInitialTermCount[Precision];
    LastInput = InputCount - 1;
    TableIndex = (InputExponent - 3) / 24;
    if (TableIndex < 0) {
        TableIndex = 0;
    }

    IntegralExponent =  InputExponent - (24 * (TableIndex + 1));

    //
    // Initialize the pi/2 variable up to LastInput + InitialTermCount, where
    // the value at LastInput + InitialTermCount is the integer array at
    // TableIndex + InitialTermCount.
    //

    Index2 = TableIndex - LastInput;
    EndIndex = LastInput + InitialTermCount;
    for (Index = 0; Index <= EndIndex; Index += 1) {
        if (Index2 < 0) {
            PiOver2[Index] = 0.0;

        } else {
            PiOver2[Index] = ClTwoOverPiIntegers[Index2];
        }

        Index2 += 1;
    }

    //
    // Compute the initial integral values up to the initial term count.
    //

    for (Index = 0; Index <= InitialTermCount; Index += 1) {
        Final = 0.0;
        for (Index2 = 0; Index2 <= LastInput; Index2 += 1) {
            Final += Input[Index2] * PiOver2[LastInput + Index - Index2];
        }

        Integral[Index] = Final;
    }

    TermCount = InitialTermCount;

    //
    // Loop recomputing while the number of terms needed is changing.
    //

    while (TRUE) {

        //
        // Distill the integral array into the integer array in reverse.
        //

        Index = 0;
        Value = Integral[TermCount];
        for (Index2 = TermCount; Index2 > 0; Index2 -= 1) {
            Final = (double)(LONG)(ClTwoNegative24 * Value);
            IntegralInteger[Index] = (LONG)(Value - (ClTwo24 * Final));
            Value = Integral[Index2 - 1] + Final;
            Index += 1;
        }

        //
        // Compute the integer count of dividing by pi/2.
        //

        Value  = scalbn(Value, IntegralExponent);

        //
        // Trim off anything above 8.
        //

        Value -= 8.0 * floor(Value * 0.125);
        PiOver2Count = (LONG)Value;
        Value -= (double)PiOver2Count;
        HighWord = 0;

        //
        // The last integral integer is needed to determine the count of pi/2s.
        //

        if (IntegralExponent > 0) {
            Index = (IntegralInteger[TermCount - 1] >> (24 - IntegralExponent));
            PiOver2Count += Index;
            IntegralInteger[TermCount - 1] -= Index << (24 - IntegralExponent);
            HighWord = IntegralInteger[TermCount - 1] >>
                       (23 - IntegralExponent);

        } else if (IntegralExponent == 0) {
            HighWord = IntegralInteger[TermCount - 1] >> 23;

        } else if (Value >= 0.5) {
            HighWord = 2;
        }

        //
        // Check to see if the integral is greater than 0.5.
        //

        if (HighWord > 0) {
            PiOver2Count += 1;
            Carry = 0;
            for (Index = 0; Index < TermCount; Index += 1) {
                Index2 = IntegralInteger[Index];
                if (Carry == 0) {
                    if (Index2 != 0) {
                        Carry = 1;
                        IntegralInteger[Index] = 0x1000000 - Index2;
                    }

                } else {
                    IntegralInteger[Index] = 0xFFFFFF - Index2;
                }
            }

            //
            // 1 in 12 times this will occur.
            //

            if (IntegralExponent > 0) {
                switch (IntegralExponent) {
                case 1:
                    IntegralInteger[TermCount - 1] &= 0x7FFFFF;
                    break;

                case 2:
                    IntegralInteger[TermCount - 1] &= 0x3FFFFF;
                    break;

                default:
                    break;
                }
            }

            if (HighWord == 2) {
                Value = 1.0 - Value;
                if (Carry != 0) {
                    Value -= scalbn(1.0, IntegralExponent);
                }
            }
        }

        //
        // Decide whether or not recomputation is necesary.
        //

        if (Value == 0.0) {
            Index2 = 0;
            for (Index = TermCount - 1; Index >= InitialTermCount; Index -= 1) {
                Index2 |= IntegralInteger[Index];
            }

            if (Index2 == 0) {
                NeededTerms = 1;
                while (IntegralInteger[InitialTermCount - NeededTerms] == 0) {
                    NeededTerms += 1;
                }

                //
                // Add Integral[TermCount + 1] to
                // Integral[TermCount + NeededTerms].
                //

                for (Index = TermCount + 1;
                     Index <= TermCount + NeededTerms;
                     Index += 1) {

                    PiOver2[LastInput + Index] =
                               (double)ClTwoOverPiIntegers[TableIndex + Index];

                    Final = 0.0;
                    for (Index2 = 0; Index2 <= LastInput; Index2 += 1) {
                        Final += Input[Index2] *
                                 PiOver2[LastInput + Index - Index2];
                    }

                    Integral[Index] = Final;
                }

                TermCount += NeededTerms;

                //
                // Go back up and recompute.
                //

                continue;
            }
        }

        break;
    }

    //
    // Chop off any zero terms.
    //

    if (Value == 0.0) {
        TermCount -= 1;
        IntegralExponent -= 24;
        while (IntegralInteger[TermCount] == 0) {
            TermCount -= 1;
            IntegralExponent -= 24;
        }

    //
    // Break the value into 24 bit chunks if necessary.
    //

    } else {
        Value = scalbn(Value, -IntegralExponent);
        if (Value >= ClTwo24) {
            Final = (double)((LONG)(ClTwoNegative24 * Value));
            IntegralInteger[TermCount] = (LONG)(Value - ClTwo24 * Final);
            TermCount += 1;
            IntegralExponent += 24;
            IntegralInteger[TermCount] = (LONG)Final;

        } else {
            IntegralInteger[TermCount] = (LONG)Value;
        }
    }

    //
    // Convert the integer bit chunk into a floating point value.
    //

    Final = scalbn(1.0, IntegralExponent);
    for (Index = TermCount; Index >= 0; Index -= 1) {
        Integral[Index] = Final * (double)IntegralInteger[Index];
        Final *= ClTwoNegative24;
    }

    //
    // Compute PiOver2[0 ... InitialTermCount] * Integral[TermCount ... 0]
    //

    for (Index = TermCount; Index >= 0; Index -= 1) {
        Final = 0.0;
        for (Index2 = 0;
             (Index2 <= InitialTermCount) && (Index2 <= TermCount - Index);
             Index2 += 1) {

            Final += ClPiOver2[Index2] * Integral[Index + Index2];
        }

        FinalProduct[TermCount - Index] = Final;
    }

    //
    // Compress the final product into the output.
    //

    switch (Precision) {
    case FloatingPrecisionSingle:
        Final = 0.0;
        for (Index = TermCount; Index >= 0; Index -= 1) {
            Final += FinalProduct[Index];
        }

        if (HighWord == 0) {
            Output[0] = Final;

        } else {
            Output[0] = -Final;
        }

        break;

    case FloatingPrecisionDouble:
    case FloatingPrecisionExtended:
        Final = 0.0;
        for (Index = TermCount; Index >= 0; Index -= 1) {
            Final += FinalProduct[Index];
        }

        VolatileValue = Final;
        Final = VolatileValue;
        if (HighWord == 0) {
            Output[0] = Final;

        } else {
            Output[0] = -Final;
        }

        Final = FinalProduct[0] - Final;
        for (Index = 1; Index <= TermCount; Index += 1) {
            Final += FinalProduct[Index];
        }

        if (HighWord == 0) {
            Output[1] = Final;

        } else {
            Output[1] = -Final;
        }

        break;

    case FloatingPrecisionQuad:
        for (Index = TermCount; Index > 0; Index -= 1) {
            Final = FinalProduct[Index - 1] + FinalProduct[Index];
            FinalProduct[Index] += FinalProduct[Index - 1] - Final;
            FinalProduct[Index - 1] = Final;
        }

        for (Index = TermCount; Index > 1; Index -= 1) {
            Final = FinalProduct[Index - 1] + FinalProduct[Index];
            FinalProduct[Index] += FinalProduct[Index - 1] - Final;
            FinalProduct[Index - 1] = Final;
        }

        Final = 0.0;
        for (Index = TermCount; Index >= 2; Index -= 1) {
            Final += FinalProduct[Index];
        }

        if (HighWord == 0) {
            Output[0] = FinalProduct[0];
            Output[1] = FinalProduct[1];
            Output[2] = Final;

        } else {
            Output[0] = -FinalProduct[0];
            Output[1] = -FinalProduct[1];
            Output[2] = -Final;
        }

        break;

    default:
        break;
    }

    return PiOver2Count & 7;
}

